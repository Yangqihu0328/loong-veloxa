# Creative — GLES 资源生命周期与 dirty rect / shader 资源（B3 + B4 + B6）

**任务 ID：** TASK-20260505-03
**日期：** 2026-05-05
**状态：** 蓝图（V2=a）
**关联决策：** B3 = CPU 光栅化 + GPU texture atlas；B4 = ComputeDirtyRect + glScissor；B6 = 静态嵌入 .glsl raw string literal

---

## 0. 决策上下文

| 已锁定决策 | 影响 |
|---|---|
| V5 vx_renderer_flag | SoftwareCanvas 共存 / GlyphCache CPU bitmap 复用 |
| B3 CPU + GPU atlas | 复用 FreeType + GlyphCache / GL_R8 texture atlas |
| B4 glScissor | 复用既有 ComputeDirtyRect / 编译期 `#if VX_RENDERER_GLES` 分支 |
| B6 raw string literal | 编译期绑定 / 与 inspector_panel.html inline_resources 同范式 |
| B8 完整预留 | 资源类需含 `OnContextLost / OnContextRestored` 接口 |

---

## 1. 资源生命周期总览

```
┌────────────────────────────────────────────────────────────────────┐
│                  Application 生命周期                              │
│   Construct ──► LoadHTML ──► Update ×N ──► (ContextLost) ──► ...   │
│                                                                    │
└─────────────┬──────────────────────────────┬───────────────────────┘
              │                              │
              ▼                              ▼
┌─────────────────────────┐    ┌─────────────────────────────────┐
│   GLESDisplay           │    │   GLESCanvas                    │
│   (Sdl2EGLDisplay)      │    │                                 │
│                         │    │   ShaderProgram cache           │
│   Initialize()          │    │     ├── solid_shader_           │
│     ▼                   │    │     ├── rounded_rect_shader_    │
│   eglDisplay            │    │     ├── tess_shader_            │
│   eglContext            │    │     ├── texture_shader_         │
│     ▼                   │    │     └── glyph_shader_           │
│   MakeCurrent()         │    │                                 │
│                         │    │   VAO / VBO 池                  │
│   IsContextLost()       │    │     ├── quad_vao_/vbo_          │
│     ▼                   │    │     └── dynamic_vao_/vbo_       │
│   RestoreContext()      │    │                                 │
│                         │    │   GlyphAtlas (B3)               │
│   Shutdown()            │    │     └── GL_R8 texture           │
└─────────────────────────┘    │                                 │
                                │   ImageTexturePool             │
                                │     └── GL_RGBA8 textures      │
                                │                                 │
                                │   CPU Shadow State              │
                                │     ├── transform_stack_        │
                                │     ├── clip_rect_stack_        │
                                │     └── layer_stack_            │
                                └─────────────────────────────────┘
```

**关键不变量：**

1. 全部 GL handle（program / VAO / VBO / texture / FBO）由 `GLESCanvas` 持有 / RAII（dtor 释放）
2. `OnContextLost()` 触发后所有 GL handle 设为 0 / 不再 glDelete*（driver 已释放）
3. `OnContextRestored()` 重新创建全部 GL 资源 / CPU side cache（GlyphCache CPU bitmap / ImageCache）保留
4. `GLESDisplay` 销毁前必须先销毁 `GLESCanvas`（析构序约束 / 类似 SoftwareCanvas 与 surface_pixels_ 关系）

---

## 2. B6 shader 资源管理 — 静态嵌入

### 2.1 raw string literal 嵌入

```cpp
// veloxa/graphics/gles/shaders.h
namespace vx::gfx::gles::shaders {

constexpr const char* kSolidVert = R"GLSL(#version 300 es
precision highp float;
layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_uv;
out vec2 v_uv;
uniform mat4 u_proj;
uniform mat3 u_xform;
void main() {
  v_uv = a_uv;
  vec3 xy = u_xform * vec3(a_pos, 1.0);
  gl_Position = u_proj * vec4(xy.xy, 0.0, 1.0);
}
)GLSL";

constexpr const char* kSolidFrag = R"GLSL(#version 300 es
precision mediump float;
uniform vec4 u_color;
out vec4 frag_color;
void main() {
  frag_color = u_color;
}
)GLSL";

constexpr const char* kRoundedRectFrag = R"GLSL(#version 300 es
precision mediump float;
in vec2 v_uv;
uniform vec2 u_size;
uniform float u_radius;
uniform vec4 u_color;
out vec4 frag_color;
float roundedBoxSDF(vec2 p, vec2 b, float r) {
  vec2 q = abs(p) - b + vec2(r);
  return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}
void main() {
  vec2 p = (v_uv - 0.5) * u_size;
  float d = roundedBoxSDF(p, u_size * 0.5, u_radius);
  float a = smoothstep(1.0, -1.0, d);
  frag_color = vec4(u_color.rgb, u_color.a * a);
}
)GLSL";

constexpr const char* kTextureVert = R"GLSL(#version 300 es
precision highp float;
layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_uv;
out vec2 v_uv;
uniform mat4 u_proj;
uniform mat3 u_xform;
void main() {
  v_uv = a_uv;
  vec3 xy = u_xform * vec3(a_pos, 1.0);
  gl_Position = u_proj * vec4(xy.xy, 0.0, 1.0);
}
)GLSL";

constexpr const char* kTextureFrag = R"GLSL(#version 300 es
precision mediump float;
in vec2 v_uv;
uniform sampler2D u_tex;
out vec4 frag_color;
void main() {
  frag_color = texture(u_tex, v_uv);
}
)GLSL";

// glyph: GL_R8 atlas + tinting
constexpr const char* kGlyphFrag = R"GLSL(#version 300 es
precision mediump float;
in vec2 v_uv;
uniform sampler2D u_atlas;
uniform vec4 u_color;
out vec4 frag_color;
void main() {
  float a = texture(u_atlas, v_uv).r;  // R8 atlas
  frag_color = vec4(u_color.rgb, u_color.a * a);
}
)GLSL";

constexpr const char* kLinearGradientFrag = R"GLSL(#version 300 es
precision mediump float;
in vec2 v_uv;
uniform vec2 u_p0;
uniform vec2 u_p1;
uniform vec4 u_color0;
uniform vec4 u_color1;
out vec4 frag_color;
void main() {
  vec2 d = u_p1 - u_p0;
  float t = clamp(dot(v_uv - u_p0, d) / dot(d, d), 0.0, 1.0);
  frag_color = mix(u_color0, u_color1, t);
}
)GLSL";

constexpr const char* kRadialGradientFrag = R"GLSL(#version 300 es
precision mediump float;
in vec2 v_uv;
uniform vec2 u_center;
uniform float u_radius;
uniform vec4 u_color0;
uniform vec4 u_color1;
out vec4 frag_color;
void main() {
  float t = clamp(distance(v_uv, u_center) / u_radius, 0.0, 1.0);
  frag_color = mix(u_color0, u_color1, t);
}
)GLSL";

}  // namespace vx::gfx::gles::shaders
```

### 2.2 与 inspector_panel.html inline_resources 范式协同

既有 DevTool 模式 `__INLINE_CSS__` / `__INLINE_JS__` 占位符替换是字符串拼接动态替换；本任务 shader 使用 **C++ raw string literal 编译期绑定**（更严格 / 不可注入）。

| 维度 | inspector_panel inline | shaders raw string literal |
|---|---|---|
| 资源类型 | HTML / CSS / JS 字符串 | GLSL shader 源码 |
| 绑定时机 | 运行时（占位符替换）| 编译期（raw string literal）|
| 部署依赖 | 0（资源已嵌入 binary）| 0（raw string literal 嵌入 binary）|
| 篡改风险 | 中（运行时构造）| **低**（编译期固定 / shader 注入防御）|

### 2.3 安全：shader 注入防御（[安全相关] P0）

```
威胁：用户内容（HTML 文本 / CSS 字符串）作为 shader source
缓解：shader source 全部 constexpr static / 永不接受外部输入
验证：CodeQL / static analysis 检查 glShaderSource 的 source 参数
```

---

## 3. B3 GlyphAtlas 设计

### 3.1 数据结构

```cpp
namespace vx::gfx::gles {

struct GlyphAtlasInfo {
  f32 u0, v0, u1, v1;  // atlas UV 0..1
  i32 bearing_x;        // FreeType bearing
  i32 bearing_y;
  i32 width;            // glyph 像素宽度
  i32 height;           // glyph 像素高度
  f32 advance;          // 行进距离
};

class GlyphAtlas {
 public:
  GlyphAtlas(text::FontManager* font_manager, text::GlyphCache* glyph_cache,
             u32 atlas_width = 1024, u32 atlas_height = 1024);
  ~GlyphAtlas();

  // 主接口（B3 核心 API）
  GlyphAtlasInfo GetOrUpload(text::FontHandle font, u32 codepoint, f32 size);

  // GL 资源访问
  GLuint texture_id() const { return texture_; }
  u32 atlas_width() const { return atlas_width_; }
  u32 atlas_height() const { return atlas_height_; }

  // Context Lost 协议
  void OnContextLost();
  void OnContextRestored();

  // 性能监控（B7 验收 / G1.17 BM_GLESReplay* 用）
  u64 uploads_this_frame() const { return uploads_this_frame_; }
  u64 cache_hits() const { return cache_hits_; }
  u64 cache_misses() const { return cache_misses_; }
  void ResetFrameCounters();

 private:
  // Atlas packing — 简单 row-pack（first-fit shelf）
  bool PackGlyph(u32 width, u32 height, u32* out_x, u32* out_y);

  // CPU 光栅化 fallback
  Status RasterizeAndUpload(text::FontHandle font, u32 codepoint, f32 size,
                            GlyphAtlasInfo* out_info);

  text::FontManager* font_manager_ = nullptr;  // 不持有
  text::GlyphCache* glyph_cache_ = nullptr;    // 不持有

  GLuint texture_ = 0;
  u32 atlas_width_, atlas_height_;

  // Cache key 构造：font_id (16b) | codepoint (24b) | size_pt_x4 (16b)
  std::unordered_map<u64, GlyphAtlasInfo> cache_;

  // Row-pack 状态
  u32 cursor_x_ = 0;
  u32 cursor_y_ = 0;
  u32 row_height_ = 0;

  // 性能计数
  u64 uploads_this_frame_ = 0;
  u64 cache_hits_ = 0;
  u64 cache_misses_ = 0;
};

}  // namespace vx::gfx::gles
```

### 3.2 GetOrUpload 核心流程

```cpp
GlyphAtlasInfo GlyphAtlas::GetOrUpload(text::FontHandle font, u32 codepoint, f32 size) {
  // 1. cache 查找
  u32 size_quantized = static_cast<u32>(size * 4);  // 0.25 pt 精度
  u64 key = (static_cast<u64>(font) << 48) |
            (static_cast<u64>(codepoint) << 24) |
            static_cast<u64>(size_quantized);

  auto it = cache_.find(key);
  if (it != cache_.end()) {
    ++cache_hits_;
    return it->second;
  }
  ++cache_misses_;

  // 2. cache miss → 调 GlyphCache 获取 CPU bitmap
  text::GlyphBitmap bitmap;
  Status s = glyph_cache_->GetBitmap(font, codepoint, size, &bitmap);
  if (!s.ok()) {
    // 缺失字形 → 返回空 info（fallback 使用 .notdef glyph）
    return {};
  }

  // 3. atlas 打包
  u32 x, y;
  if (!PackGlyph(bitmap.width + 2, bitmap.height + 2, &x, &y)) {
    // atlas 满 → LRU evict 或简单 wrap
    // 本设计阶段：清空 atlas + 重新 upload 当前 glyph（保守策略）
    cache_.clear();
    cursor_x_ = cursor_y_ = row_height_ = 0;
    if (!PackGlyph(bitmap.width + 2, bitmap.height + 2, &x, &y)) {
      VX_LOG_ERROR("Glyph too large for atlas: %u x %u", bitmap.width, bitmap.height);
      return {};
    }
  }

  // 4. upload to GL_R8 texture
  glBindTexture(GL_TEXTURE_2D, texture_);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);  // R8 单字节对齐
  glTexSubImage2D(GL_TEXTURE_2D, 0, x, y,
                  bitmap.width, bitmap.height,
                  GL_RED, GL_UNSIGNED_BYTE, bitmap.data);
  ++uploads_this_frame_;

  // 5. 构造 GlyphAtlasInfo
  GlyphAtlasInfo info;
  info.u0 = static_cast<f32>(x) / atlas_width_;
  info.v0 = static_cast<f32>(y) / atlas_height_;
  info.u1 = static_cast<f32>(x + bitmap.width) / atlas_width_;
  info.v1 = static_cast<f32>(y + bitmap.height) / atlas_height_;
  info.bearing_x = bitmap.bearing_x;
  info.bearing_y = bitmap.bearing_y;
  info.width = bitmap.width;
  info.height = bitmap.height;
  info.advance = bitmap.advance;

  cache_[key] = info;
  return info;
}
```

### 3.3 Atlas 打包：简单 row-pack（first-fit shelf）

```cpp
bool GlyphAtlas::PackGlyph(u32 width, u32 height, u32* out_x, u32* out_y) {
  if (width > atlas_width_ || height > atlas_height_) return false;

  // 当前行不够 → 换新行
  if (cursor_x_ + width > atlas_width_) {
    cursor_x_ = 0;
    cursor_y_ += row_height_;
    row_height_ = 0;
  }

  // 当前 atlas 不够 → 触发 evict
  if (cursor_y_ + height > atlas_height_) return false;

  *out_x = cursor_x_;
  *out_y = cursor_y_;
  cursor_x_ += width;
  if (height > row_height_) row_height_ = height;
  return true;
}
```

**优势：** O(1) packing / 简单直接 / 适合首版蓝图。

**未来优化（reflect 阶段决定）：** Skyline / MaxRects / 真 LRU evict（需 timestamp + free-list）。

### 3.4 Context Lost 处理

```cpp
void GlyphAtlas::OnContextLost() {
  texture_ = 0;  // GL handle 失效
  // CPU side cache_ 保留（GlyphAtlasInfo 仅 UV / size 等元数据）
}

void GlyphAtlas::OnContextRestored() {
  // 1. 重建 texture
  glGenTextures(1, &texture_);
  glBindTexture(GL_TEXTURE_2D, texture_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R8,
               atlas_width_, atlas_height_, 0,
               GL_RED, GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  // 2. 清空 cache 强制 next-frame 重 upload（保守策略）
  cache_.clear();
  cursor_x_ = cursor_y_ = row_height_ = 0;
}
```

### 3.5 多字号 / emoji edge case

- **多字号** — cache key 含 `size_quantized` / 不同字号独立 entry
- **emoji** — 当前 SoftwareCanvas 已支持 FreeType color emoji（GL_RGBA atlas 可能需要）/ 本蓝图阶段保留 GL_R8 单色 / emoji 暂用 fallback bitmap（后续 G2 阶段升级 GL_RGBA atlas）

---

## 4. ImageTexturePool 设计

### 4.1 数据结构

```cpp
namespace vx::gfx::gles {

class ImageTexturePool {
 public:
  ImageTexturePool();
  ~ImageTexturePool();

  // 主接口：根据 image handle 获取 GL texture
  // cache miss → upload + 缓存
  GLuint GetOrUpload(const Image& image);

  // Context Lost 协议
  void OnContextLost();
  void OnContextRestored(image::ImageCache* image_cache);

 private:
  std::unordered_map<u32 /*image_handle*/, GLuint /*texture_id*/> texture_cache_;
  // 注：image_handle = ImageCache 内部 handle / Veloxa 既有
};

}  // namespace vx::gfx::gles
```

### 4.2 GetOrUpload 流程

```cpp
GLuint ImageTexturePool::GetOrUpload(const Image& image) {
  if (!image.valid() || image.handle() == 0) return 0;

  auto it = texture_cache_.find(image.handle());
  if (it != texture_cache_.end()) return it->second;

  // upload to GL_RGBA8 texture
  GLuint tex = 0;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
               image.width(), image.height(), 0,
               GL_RGBA, GL_UNSIGNED_BYTE, image.pixels());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glGenerateMipmap(GL_TEXTURE_2D);

  texture_cache_[image.handle()] = tex;
  return tex;
}
```

### 4.3 Context Lost 处理

```cpp
void ImageTexturePool::OnContextLost() {
  // 全部 GL texture handle 失效 / cache clear
  texture_cache_.clear();
}

void ImageTexturePool::OnContextRestored(image::ImageCache* image_cache) {
  // image_cache 持有 CPU side image data 不丢
  // 不主动 re-upload / 等下次 GetOrUpload 触发 lazy 上传（与既有 ImageCache 模式一致）
}
```

---

## 5. PushClipRect / PopClip 实现

### 5.1 简单矩形裁剪 — glScissor 栈

```cpp
void GLESCanvas::PushClipRect(const Rect& rect) {
  // 1. 计算与 transform / 父 clip 的交集
  Rect xformed = TopTransform().Apply(rect);
  Rect parent = clip_rect_stack_.empty()
                  ? Rect{0, 0, static_cast<f32>(width_),
                         static_cast<f32>(height_)}
                  : clip_rect_stack_.back();
  Rect intersect = Rect::Intersect(xformed, parent);

  clip_rect_stack_.push_back(intersect);

  // 2. 应用到 glScissor
  glEnable(GL_SCISSOR_TEST);
  glScissor(static_cast<GLint>(intersect.x),
            // GLES 坐标系 Y 向上 / 翻转
            static_cast<GLint>(height_ - intersect.y - intersect.h),
            static_cast<GLsizei>(intersect.w),
            static_cast<GLsizei>(intersect.h));
}

void GLESCanvas::PopClip() {
  if (clip_rect_stack_.empty()) return;
  clip_rect_stack_.pop_back();

  if (clip_rect_stack_.empty()) {
    glDisable(GL_SCISSOR_TEST);
  } else {
    Rect r = clip_rect_stack_.back();
    glScissor(static_cast<GLint>(r.x),
              static_cast<GLint>(height_ - r.y - r.h),
              static_cast<GLsizei>(r.w),
              static_cast<GLsizei>(r.h));
  }
}
```

### 5.2 复杂路径裁剪 — stencil buffer

```cpp
void GLESCanvas::PushClipPath(const Path& path) {
  // 1. 启用 stencil
  glEnable(GL_STENCIL_TEST);
  glStencilMask(0xFF);
  glClear(GL_STENCIL_BUFFER_BIT);  // 第一个 clip 才 clear，后续 push 累加 stencil ref

  // 2. 写 path 区域到 stencil（关闭 color write）
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  glStencilFunc(GL_ALWAYS, current_stencil_ref_ + 1, 0xFF);
  glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

  FillPath(path, gfx::Brush::Solid({0, 0, 0, 0}));  // 颜色无关 / 仅 stencil

  // 3. 启用 color write + stencil = ref 才画
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glStencilFunc(GL_EQUAL, current_stencil_ref_ + 1, 0xFF);
  glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
  ++current_stencil_ref_;
}
```

### 5.3 PushLayer / PopLayer — 临时 FBO

```cpp
struct LayerFrame {
  GLuint fbo;
  GLuint color_tex;
  Rect bounds;
  f32 opacity;
};

void GLESCanvas::PushLayer(const Rect& bounds, f32 opacity) {
  // 1. 创建临时 FBO + color attachment texture
  GLuint fbo, tex;
  glGenFramebuffers(1, &fbo);
  glGenTextures(1, &tex);

  i32 w = static_cast<i32>(std::ceil(bounds.w));
  i32 h = static_cast<i32>(std::ceil(bounds.h));

  glBindTexture(GL_TEXTURE_2D, tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                         GL_TEXTURE_2D, tex, 0);

  glClearColor(0, 0, 0, 0);
  glClear(GL_COLOR_BUFFER_BIT);

  layer_stack_.push_back({fbo, tex, bounds, opacity});
}

void GLESCanvas::PopLayer() {
  if (layer_stack_.empty()) return;
  LayerFrame top = layer_stack_.back();
  layer_stack_.pop_back();

  // 1. 恢复主 FBO
  GLuint dst_fbo = layer_stack_.empty() ? framebuffer_
                                         : layer_stack_.back().fbo;
  glBindFramebuffer(GL_FRAMEBUFFER, dst_fbo);

  // 2. 把 layer texture 合成回去（quad + texture_shader + opacity）
  glUseProgram(texture_shader_.program);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, top.color_tex);
  glUniform1f(texture_shader_.uniforms["u_opacity"], top.opacity);

  // 4 顶点 quad to bounds
  // ... draw ...

  // 3. 释放临时资源
  glDeleteFramebuffers(1, &top.fbo);
  glDeleteTextures(1, &top.color_tex);
}
```

---

## 6. B4 dirty rect GPU 化

### 6.1 既有 ComputeDirtyRect 复用

```cpp
// veloxa/core/render/renderer.cc 既有 API（零修改）
gfx::Rect ComputeDirtyRect(const DisplayList& old_list,
                           const DisplayList& new_list,
                           f32 viewport_width, f32 viewport_height);
```

### 6.2 Application::Update GLES 分支

```cpp
void Application::Update() {
  // ... build tree / layout / record new_list ...

  Rect dirty = render::ComputeDirtyRect(prev_list_, new_list_,
                                         width, height);
  if (dirty.IsEmpty() && !force_full_repaint_) {
    return;  // 0 dirty / skip frame
  }

  if (canvas_) {
    canvas_->Begin();

#if VX_RENDERER_GLES
    // GLES 路径 — glScissor + glClear 限定 dirty region
    // 绕过 canvas_->Clear()（整面清）/ 仅 clear dirty region
    glEnable(GL_SCISSOR_TEST);
    glScissor(static_cast<GLint>(dirty.x),
              static_cast<GLint>(height - dirty.y - dirty.h),
              static_cast<GLsizei>(dirty.w),
              static_cast<GLsizei>(dirty.h));
    glClearColor(config_.background_color.r / 255.0f,
                 config_.background_color.g / 255.0f,
                 config_.background_color.b / 255.0f,
                 config_.background_color.a / 255.0f);
    glClear(GL_COLOR_BUFFER_BIT);
#else
    canvas_->Clear(config_.background_color);
#endif

    render::Replay(new_list_, canvas_.get(), &image_cache_);

    canvas_->End();

#if VX_RENDERER_GLES
    glDisable(GL_SCISSOR_TEST);
#endif
  }

  if (config_.surface) config_.surface->Present();
  prev_list_ = std::move(new_list_);
}
```

### 6.3 与 GLESCanvas::PushClipRect 交互

```
glScissor 与 glStencilTest 是独立的 state machine
  - glScissor 控制：哪些像素允许写入
  - glStencilTest 控制：哪些像素通过 stencil 测试

dirty rect 用 glScissor / clip path 用 glStencilTest
两者不冲突 / 可同时开
```

### 6.4 性能预期（B7 验收）

| 场景 | 全帧重画（candidat C）| dirty rect glScissor（candidate A）|
|---|---|---|
| 静态页面 typical 1080p | 16.6ms / 60fps（满载）| < 2ms（仅 dirty 区）|
| 滚动 page | 16.6ms / 60fps | 5-8ms（部分屏幕）|
| 单 button hover 变色 | 16.6ms / 60fps | < 1ms（小 region）|

---

## 7. 资源生命周期协议总结

### 7.1 启动序

```
1. Application 构造
2. font_manager_.Init()
3. Sdl2GLWindowSurface 构造
   3.1 SDL_CreateWindow(SDL_WINDOW_OPENGL)
   3.2 Sdl2EGLDisplay::Initialize()
       3.2.1 SDL_GL_CreateContext
       3.2.2 SDL_GL_MakeCurrent
       3.2.3 glGetString(GL_VERSION) 验证
4. GLESCanvas::GLESCanvas(display, w, h, font_manager, glyph_cache)
   4.1 CompileShaders()  // B6 raw string literal
   4.2 glGenVertexArrays / glGenBuffers
   4.3 glyph_atlas_ = std::make_unique<GlyphAtlas>(...)
       4.3.1 glGenTextures / glTexImage2D（empty atlas）
   4.4 image_pool_ = std::make_unique<ImageTexturePool>()
5. canvas_->Begin()
6. canvas_->Clear(background_color)
```

### 7.2 析构序（反向 / 严格）

```
1. canvas_->End()
2. layer_stack_.clear()  // 释放残留 FBO / texture
3. image_pool_.reset()    // 释放 image textures
4. glyph_atlas_.reset()   // 释放 atlas texture
5. glDelete* (VAO / VBO / shader programs)
6. ~GLESCanvas
7. ~Sdl2GLWindowSurface
   7.1 ~Sdl2EGLDisplay
       7.1.1 SDL_GL_DeleteContext
   7.2 SDL_DestroyWindow
8. font_manager_.Shutdown()
9. ~Application
```

### 7.3 Context Lost 序

```
1. EventLoop / glGetError() 检测 GL_CONTEXT_LOST_KHR
2. Sdl2EGLDisplay::context_lost_ = true
3. Application::Update() 下一次调用前检测
4. GLESCanvas::OnContextLost()
   4.1 全部 GL handle 设为 0（不调 glDelete*）
   4.2 layer_stack_.clear()
   4.3 glyph_atlas_->OnContextLost()
   4.4 image_pool_->OnContextLost()
5. Sdl2EGLDisplay::RestoreContext()
   5.1 SDL_GL_DeleteContext / SDL_GL_CreateContext / SDL_GL_MakeCurrent
6. GLESCanvas::OnContextRestored()
   6.1 CompileShaders()
   6.2 glGenVertexArrays / glGenBuffers 重建
   6.3 glyph_atlas_->OnContextRestored()  // 重建 atlas texture / clear cache
   6.4 image_pool_->OnContextRestored(image_cache)
7. canvas_->Begin() / Clear() / 正常进入下一帧
```

---

## 8. 实施任务关联

- **G1.4** GLESCanvas 骨架 — shader 编译 + VAO/VBO 创建
- **G1.8** GlyphAtlas + DrawText — 本 creative §3 落地
- **G1.9** ImageTexturePool + DrawImage — 本 creative §4 落地
- **G1.10** PushClipRect/PopClip + PushLayer/PopLayer — 本 creative §5 落地
- **G1.11** dirty rect glScissor 集成 — 本 creative §6 落地
- **G1.14** Context Lost / Restore — 本 creative §7.3 落地

---

## 9. 反向探针候选

| 反向探针 | 验证点 | 强度档 |
|---|---|:-:|
| 改 GlyphAtlas `cache_.find(key)` 永不命中 | atlas 上传率 / 性能影响 | 平衡 |
| 改 PackGlyph 跳过 row_height_ 更新 | atlas 错误重叠 | 合适 |
| 改 OnContextLost 不清 cache | restore 后 cache 残留 / 错误 UV | 合适 |
| 改 glScissor 范围为 0,0,0,0 | dirty rect 生效 / 整屏不出像素 | 过高 |
| 改 PushClipRect 后跳过 glScissor 调用 | clip rect 生效 / 超出 clip 的像素仍 render | 合适 |

---

**END OF CREATIVE — GLES Resources / Dirty Rect / Shader (B3 + B4 + B6)**
