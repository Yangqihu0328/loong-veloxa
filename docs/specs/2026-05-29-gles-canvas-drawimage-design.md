# G1.9 `GLESCanvas::DrawImage` 设计规格

**任务：** TASK-20260529-04 / **复杂度：** Level 4 / **日期：** 2026-05-29
**决策锁定：** D1-D8 = 推荐（D2 = B）/ 跳过独立 `/creative`（复用 `creative-gles-resources.md` §4，reconcile 折入本规格）

---

## 1. 目标

为 `GLESCanvas` 实现 `DrawImage(const Image&, const Rect& src_rect, const Rect& dst_rect)`（当前 `gles_canvas.h:88` no-op stub）：

- RGBA8 图像纹理上传（`Image` RGBA8 → `GL_RGBA8`）。
- src_rect 子区（图像像素）→ UV，dst_rect（画布像素）→ quad，镜像 `software_canvas.cc:282-320`。
- `ImageTexturePool` 纹理缓存（避免每帧重传）+ Context Lost/Restored。
- 新增 `kImageVert/kImageFrag`，复用 `u_xform_px/u_viewport_px` 约定 + `a_uv` attr loc 1。
- **0 新依赖**。

---

## 2. Phase 0 Reconcile（creative-gles-resources §4 vs 实际代码）

| # | creative 假设 | 实际 | reconcile 决策 |
|---|---|---|---|
| **R1** | `ImageTexturePool::GetOrUpload` 键 = `image.handle()` | `Image`（`image.h`）**无 handle**，仅 width/height/pixels(u32*) | **D2=B**：键 = `reinterpret_cast<u64>(image.pixels())` + (w,h) 校验。命中且 w/h 一致 → 复用；w/h 不一致（指针复用到不同图）→ 删旧 texture 重传 |
| **R2** | creative §4.2 `glGenerateMipmap` | Mesa swrast mipmap 风险 + DrawImage 无缩小质量需求 | **D6=A**：LINEAR min/mag + CLAMP_TO_EDGE，**不**生成 mipmap |
| **R3** | creative §4.3 `OnContextRestored(image::ImageCache*)` 参数 | 本任务无 ImageCache 依赖（Canvas API 直传 Image&）| `OnContextRestored()` 无参，仅清缓存（lazy 重传由下次 DrawImage 的 GetOrUpload 触发，调用方持 Image&）|
| **R4** | image shader frag 带 tint/opacity | `DrawImage` API 无 brush/opacity 参数 | **D4=A**：`kImageFrag = texture(u_tex, v_uv)` 直出，alpha 由 `GL_BLEND`（Begin 已启 SRC_ALPHA/ONE_MINUS_SRC_ALPHA）|
| **R5** | 像素通道顺序 | RGBA32 = byte0 R/byte1 G/byte2 B/byte3 A（systemPatterns:135）| `GL_RGBA + GL_UNSIGNED_BYTE` 直传，无重排 |

---

## 3. 架构

### 3.1 `ImageTexturePool`（新文件 `gles/image_texture_pool.{h,cc}`）

```cpp
namespace vx::gfx::gles {

// RGBA8 图像 → GL texture 缓存池。键 = image.pixels() 指针 + (w,h) 校验
// （Image 无 handle，D2=B reconcile）。不持有 Image；调用方保证 Image 生命周期。
// 调用方须持 GL context current。
class ImageTexturePool {
 public:
  ImageTexturePool() = default;
  ~ImageTexturePool();
  ImageTexturePool(const ImageTexturePool&) = delete;
  ImageTexturePool& operator=(const ImageTexturePool&) = delete;

  // cache 命中（同指针 + 同 w/h）→ 返回；否则 glGenTextures + glTexImage2D 上传。
  // image 非法 → 返回 0。
  GLuint GetOrUpload(const Image& image);

  void OnContextLost();       // 清缓存 / GL handle 失效不 glDelete
  void OnContextRestored();   // 清缓存 / lazy 重传

  vx::u64 cache_hits() const { return cache_hits_; }
  vx::u64 cache_misses() const { return cache_misses_; }
  vx::usize size() const { return entries_.size(); }

 private:
  struct Entry { GLuint texture; vx::u32 width; vx::u32 height; };
  vx::HashMap<vx::u64, Entry> entries_;   // 键 = (u64)image.pixels()
  vx::u64 cache_hits_ = 0;
  vx::u64 cache_misses_ = 0;
};

}  // namespace vx::gfx::gles
```

**GetOrUpload 流程：**
1. `image.valid()` 否 → return 0。
2. `key = reinterpret_cast<u64>(image.pixels())`。`entries_.Find(key)`：
   - 命中且 `width==image.width() && height==image.height()` → `++cache_hits_`，return texture。
   - 命中但 w/h 不符（指针复用）→ `glDeleteTextures` 旧 texture，落入上传（视为 miss）。
3. `++cache_misses_`；`glGenTextures` + `glBindTexture` + `glPixelStorei(GL_UNPACK_ALIGNMENT,4)`（RGBA8 行对齐 4）+ `glTexImage2D(GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels)` + LINEAR/CLAMP 参数（无 mipmap）。
4. `entries_.Insert(key, {tex, w, h})`，`glBindTexture(0)`，return tex。

**OnContextLost：** 不 glDelete（context 已失效），`entries_.clear()`。
**OnContextRestored：** `entries_.clear()`（lazy 重传）。
**dtor：** 遍历 `entries_` glDeleteTextures（context 仍有效时）。

### 3.2 image shader（`shaders.h` 追加）

```glsl
// kImageVert — 同 kGlyphVert 结构（pos+uv → NDC + Y-flip）
#version 300 es
precision highp float;
in vec2 a_pos;
in vec2 a_uv;
uniform mat3 u_xform_px;
uniform vec2 u_viewport_px;
out vec2 v_uv;
void main() {
  vec3 px3 = u_xform_px * vec3(a_pos, 1.0);
  vec2 ndc = (px3.xy / u_viewport_px) * 2.0 - 1.0;
  gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);
  v_uv = a_uv;
}

// kImageFrag — RGBA 纹理直采（D4=A / R4），alpha 由 GL_BLEND
#version 300 es
precision mediump float;
in vec2 v_uv;
uniform sampler2D u_tex;
out vec4 frag_color;
void main() {
  frag_color = texture(u_tex, v_uv);
}
```

注册 `kAllShaderSources[]`（B8=A，受 `shader_injection_test` S1 覆盖）。

### 3.3 `GLESCanvas::DrawImage`

```cpp
void GLESCanvas::DrawImage(const Image& image, const Rect& src_rect,
                           const Rect& dst_rect) {
  if (image_program_ == 0 || image_pool_ == nullptr) return;
  if (!image.valid() || src_rect.IsEmpty() || dst_rect.IsEmpty()) return;

  GLuint tex = image_pool_->GetOrUpload(image);  // 可能 mutate texture binding (P1#A)
  if (tex == 0) return;

  // src_rect 子区 → UV（D5）
  const vx::f32 iw = static_cast<vx::f32>(image.width());
  const vx::f32 ih = static_cast<vx::f32>(image.height());
  const vx::f32 u0 = src_rect.x / iw, u1 = src_rect.right() / iw;
  const vx::f32 v0 = src_rect.y / ih, v1 = src_rect.bottom() / ih;
  const vx::f32 x0 = dst_rect.x, y0 = dst_rect.y;
  const vx::f32 x1 = dst_rect.right(), y1 = dst_rect.bottom();

  GLfloat mat[9]; Matrix3x2ToMat3(transform_, mat);
  glUseProgram(image_program_);
  glUniformMatrix3fv(image_uniforms_[kImageUXformPx], 1, GL_FALSE, mat);
  glUniform2f(image_uniforms_[kImageUViewportPx], (GLfloat)width_, (GLfloat)height_);
  glUniform1i(image_uniforms_[kImageUTex], 0);
  glActiveTexture(GL_TEXTURE0);

  const GLfloat verts[24] = {
      x0,y0,u0,v0,  x1,y0,u1,v0,  x0,y1,u0,v1,
      x0,y1,u0,v1,  x1,y0,u1,v0,  x1,y1,u1,v1,
  };
  glBindVertexArray(image_vao_);
  glBindBuffer(GL_ARRAY_BUFFER, image_vbo_);
  glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STREAM_DRAW);
  glBindTexture(GL_TEXTURE_2D, tex);   // P1#A：GetOrUpload 后、draw 前重绑
  glDrawArrays(GL_TRIANGLES, 0, 6);
  glBindVertexArray(0);
  glBindTexture(GL_TEXTURE_2D, 0);
}
```

### 3.4 GLESCanvas 新成员

- `GLuint image_program_, image_vao_, image_vbo_`；`enum ImageUniform{kImageUXformPx,kImageUViewportPx,kImageUTex,kImageUniformCount}`；`GLint image_uniforms_[]`；`std::unique_ptr<ImageTexturePool> image_pool_`。
- ctor：`InitImageResources()`（program + 交错 VBO pos/uv）+ `image_pool_ = make_unique<ImageTexturePool>()`（无条件，DrawImage 不依赖字体）。
- dtor：`image_pool_.reset()` + `DestroyImageResources()`。

---

## 4. 安全

本任务不涉及外部输入安全面（图像像素来自内部 `Image`，src/dst rect 来自内部 layout）。GLSL 注入：`kImageVert/kImageFrag` 编译期常量 + 注册 `kAllShaderSources` → `shader_injection_test` S1 自动覆盖。0 新依赖。

---

## 5. 测试策略

承接 G1.8 P1#A（GetOrUpload mutate 纹理绑定 → draw 前重绑）+ P1#B（HashMap `Find/Insert`）+ G1.7 P1#2（双通道像素约束）。

**轮次 1 `ImageTexturePoolTest`（~8）：** ctor 空 / GetOrUpload 上传 texture≠0 / cache hit 计数（同 Image 二次）/ cache miss 计数 / invalid image→0 / 指针复用 w/h 不符重传 / OnContextLost 清缓存 / OnContextRestored 清缓存+lazy 重传。
**轮次 2 `GlesCanvasImageTest`（~8）：** 纯色图整绘（RGBA 各通道双约束）/ src 子区采样（左半红右半蓝 → dst 采左半得红）/ dst 缩放放大 / AfterSetTransform 平移 / 多图无 GL error / 反向探针 invalid image no-op / empty src/dst no-op / 多次 draw cache 复用（pool cache_hits>0）。

测试图像构造：内存 `Image`（如 16×16 纯红 / 左红右蓝），白底，区域扫描双通道判定。

---

## 6. 技术债（MVP / G2）

- 指针键缓存无 LRU/容量上限（长期运行多图可能累积 texture）→ G2 加 LRU/容量驱逐。
- 无 mipmap（缩小有锯齿）→ G2 视需求加。
- 逐 DrawImage 一次 draw call（无批量）。
- 像素指针键理论上有 ABA 风险（释放后同址新图同 w/h）→ 文档化约束（调用方 Image 生命周期内键稳定）。

---

**Source:** 蓝图 §3.9 + creative-gles-resources §4（ImageTexturePool / reconcile R1-R3）+ software_canvas.cc:282-320（src/dst 采样镜像）+ G1.8 范式（纹理生命周期 + glyph shader 模板 + P1#A/#B）
