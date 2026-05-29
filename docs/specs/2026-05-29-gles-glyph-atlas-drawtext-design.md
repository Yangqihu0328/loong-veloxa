# G1.8 `GlyphAtlas` + `GLESCanvas::DrawText` 设计规格

**任务 ID：** TASK-20260529-03
**日期：** 2026-05-29
**复杂度级别：** Level 4（GPU 资源管理 + LRU + 多字号 + emoji edge case）
**上游：** 蓝图 [`docs/plans/2026-05-05-gles-renderer-blueprint.md`](../plans/2026-05-05-gles-renderer-blueprint.md) §3.8 + creative [`memory-bank/creative/creative-gles-resources.md`](../../memory-bank/creative/creative-gles-resources.md) §2.3/§3

---

## 1. 目标

在 G1.5 shader pipeline 之上新增 **`GlyphAtlas`**（CPU FreeType 栅格化 → `GL_R8` GPU texture atlas / row-pack），并将 `GLESCanvas::DrawText` 由 no-op stub 替换为真实实现（逐字形 quad + glyph shader 采样 R8 alpha）。复用 `veloxa/text/` 既有 `FontManager`/`GlyphCache`/`ShapeCache` 基础设施，**0 新第三方依赖**。

---

## 2. Phase 0 reconcile（creative §3 写于 G1.4 前，对齐实际代码）

| # | creative §3 假设 | 实际代码（实证） | reconcile 决策 |
|:-:|---|---|---|
| R1 | `GlyphCache::GetBitmap(font, codepoint, size, &bmp)` 栅格化 | `glyph_cache.h`：仅 `Get(FontHandle, u32 glyph_id, u32 pixel_size)→const GlyphBitmap*` + `Put(...)`；**不栅格化** | GlyphAtlas miss 时自行 `FT_Load_Glyph`+`FT_Render_Glyph`（镜像 `software_canvas.cc:207-230`）→ `Put` |
| R2 | `bitmap.data` / `codepoint` / `size:f32` | `GlyphBitmap{Vector<u8> alpha; u32 width,height; i32 bearing_x,bearing_y; f32 advance}`；索引按 **glyph_id（shaping 后）+ pixel_size:u32** | atlas key = `font \| glyph_id \| pixel_size`；上传源 `bitmap.alpha.data()` |
| R3 | glyph shader `u_proj` mat4 + `u_xform` mat3 | G1.5 实际 `kSolidVert`/`kPathVert`：`uniform mat3 u_xform_px` + `uniform vec2 u_viewport_px` + 顶点内 `ndc=(px3.xy/viewport)*2-1` + `gl_Position=vec4(ndc.x,-ndc.y,...)` | 新 `kGlyphVert` 对齐 `kPathVert` 风格 + 增加 `in vec2 a_uv;out vec2 v_uv` |
| R4 | （未描述 DrawText 流程）| `software_canvas.cc:143-272`：`FindFont("",400)` → `pixel_size=u32(font_size)` → `SetFacePixelSize` → `ShapeOrLookup(font,pixel_size,text)→ShapedRun{glyphs[]{glyph_id,x_offset,y_offset,x_advance}}` → 逐字形 `Get`/miss `FT_Load+Render+Put` → blit | GLES 完全复用，仅把 CPU blit 换成 atlas upload + quad draw |
| R5 | 测试字体可得性 | `/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf`（759KB，实测存在；既有 `drawtext_shape_cache_test.cc` 范式）| pixel 测加载真实字体；缺失 → `ASSERT` fail-fast（沿用既有范式）|

---

## 3. 架构

### 3.1 数据结构（reconciled）

```cpp
// veloxa/graphics/gles/glyph_atlas.h
namespace vx::gfx::gles {

struct GlyphAtlasInfo {
  vx::f32 u0 = 0, v0 = 0, u1 = 0, v1 = 0;  // atlas UV [0,1]
  vx::i32 bearing_x = 0, bearing_y = 0;
  vx::i32 width = 0, height = 0;            // glyph 像素尺寸
  vx::f32 advance = 0;
  bool valid = false;                       // false = 字形缺失 / 栅格化失败
};

class GlyphAtlas {
 public:
  GlyphAtlas(vx::text::FontManager* fm, vx::text::GlyphCache* gc,
             vx::u32 atlas_w = 1024, vx::u32 atlas_h = 1024);
  ~GlyphAtlas();

  // 主接口（D1=A）：font + glyph_id（shaping 后）+ pixel_size。
  GlyphAtlasInfo GetOrUpload(vx::text::FontHandle font, vx::u32 glyph_id,
                             vx::u32 pixel_size);

  GLuint texture_id() const { return texture_; }
  vx::u32 atlas_width() const { return atlas_width_; }
  vx::u32 atlas_height() const { return atlas_height_; }

  void OnContextLost();      // D7=A：texture_=0 / 不 glDelete / cache 清空
  void OnContextRestored();  // 重建 texture + 清 cache

  vx::u64 cache_hits() const { return cache_hits_; }
  vx::u64 cache_misses() const { return cache_misses_; }
  vx::u64 uploads_this_frame() const { return uploads_this_frame_; }
  void ResetFrameCounters() { uploads_this_frame_ = 0; }

 private:
  bool PackGlyph(vx::u32 w, vx::u32 h, vx::u32* out_x, vx::u32* out_y);  // row-pack
  void CreateTexture();  // glGenTextures + 空 GL_R8 atlas + 参数
  static vx::u64 MakeKey(vx::text::FontHandle f, vx::u32 g, vx::u32 px);

  vx::text::FontManager* font_manager_ = nullptr;  // 不持有
  vx::text::GlyphCache* glyph_cache_ = nullptr;    // 不持有
  GLuint texture_ = 0;
  vx::u32 atlas_width_, atlas_height_;
  vx::HashMap<vx::u64, GlyphAtlasInfo> cache_;
  vx::u32 cursor_x_ = 0, cursor_y_ = 0, row_height_ = 0;  // row-pack 状态
  vx::u64 cache_hits_ = 0, cache_misses_ = 0, uploads_this_frame_ = 0;
};
}  // namespace vx::gfx::gles
```

### 3.2 GetOrUpload 流程（D2=A 自栅格化）

1. `key = MakeKey(font, glyph_id, pixel_size)`；`cache_.find` 命中 → `++cache_hits_` 返回。
2. miss → `++cache_misses_`；`glyph_cache_->Get(font, glyph_id, pixel_size)`。
3. GlyphCache miss → `font_manager_->SetFacePixelSize(font,pixel_size)` → `FT_Load_Glyph`+`FT_Render_Glyph` → 构 `GlyphBitmap`（copy `alpha`，pitch 对齐）→ `glyph_cache_->Put`（镜像 `software_canvas.cc:209-230`）。
4. 空字形（width==0 || height==0，如空格）→ 不上传，`info.valid=true` 仅含 advance/bearing。
5. `PackGlyph(width+1, height+1, &x, &y)`（+1 padding 防双线性渗色）；满 → clear-all 重排（D5=A）。
6. `glBindTexture` + `glPixelStorei(GL_UNPACK_ALIGNMENT,1)` + `glTexSubImage2D(...,GL_RED,GL_UNSIGNED_BYTE, alpha.data())`；`++uploads_this_frame_`。
7. 构 `GlyphAtlasInfo`（UV = x/atlas_w 等）+ `cache_[key]=info` 返回。

### 3.3 glyph shader（D3=A / R3 reconciled）

```glsl
// kGlyphVert — 对齐 kPathVert，增加 a_uv
#version 300 es
precision highp float;
in vec2 a_pos;   // location 0（文档像素坐标）
in vec2 a_uv;    // location 1（atlas UV）
uniform mat3 u_xform_px;
uniform vec2 u_viewport_px;
out vec2 v_uv;
void main() {
  vec3 px3 = u_xform_px * vec3(a_pos, 1.0);
  vec2 ndc = (px3.xy / u_viewport_px) * 2.0 - 1.0;
  gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);
  v_uv = a_uv;
}

// kGlyphFrag — 采样 R8 alpha × u_color
#version 300 es
precision mediump float;
in vec2 v_uv;
uniform sampler2D u_atlas;
uniform vec4 u_color;
out vec4 frag_color;
void main() {
  float a = texture(u_atlas, v_uv).r;
  frag_color = vec4(u_color.rgb, u_color.a * a);
}
```

`LinkProgram` 增加 `glBindAttribLocation(program, 1, "a_uv")`（对无 a_uv 的程序无害，被忽略）。两 shader 注册到 `kAllShaderSources[]`（B8 安全契约自动覆盖）。

### 3.4 DrawText 流程（D4=A 逐字形 quad）

1. 守卫：`font_manager_ && glyph_cache_ && glyph_program_ && glyph_atlas_`，缺任一 → no-op return（GLES MVP 不画 fallback 矩形）。
2. `FindFont` → `pixel_size=max(1,u32(font_size))` → `SetFacePixelSize` → `ShapeOrLookup`。
3. `pen_x=bounds.x`；`pen_y=bounds.y + (face->size->metrics.ascender>>6)`（与 SW 一致）。
4. `glUseProgram(glyph_program_)`；set `u_xform_px=transform_`、`u_viewport_px`、`u_color=BrushSolidColor`、`u_atlas=0`；`glActiveTexture(GL_TEXTURE0)`。
5. 逐字形：`info=glyph_atlas_->GetOrUpload(font,glyph_id,pixel_size)`；若 `info.valid && width>0`：
   - quad 文档坐标：`x0=pen_x+x_offset+bearing_x`，`y0=pen_y - y_offset - bearing_y`，`w=info.width`，`h=info.height`。
   - 6 顶点 interleaved `[x,y,u,v]`（2 三角）上传 `glyph_vbo_`（STREAM_DRAW）。
   - `glBindTexture(GL_TEXTURE_2D, glyph_atlas_->texture_id())` + `glDrawArrays(GL_TRIANGLES,0,6)`。
   - `pen_x += x_advance`。
6. 空格（width==0）：仅 `pen_x += x_advance`。

### 3.5 GL 资源（gles_canvas）

- 新增成员：`glyph_program_`、`glyph_uniforms_[{u_xform_px,u_viewport_px,u_color,u_atlas}]`、`glyph_vao_`、`glyph_vbo_`、`std::unique_ptr<GlyphAtlas> glyph_atlas_`。
- ctor：`InitShaderPrograms` 增 glyph program；`glGenVertexArrays/Buffers` glyph VAO/VBO；`glyph_atlas_ = make_unique<GlyphAtlas>(font_manager_, glyph_cache_)`（GL 上下文已 current）。
- glyph VAO 布局：stride=4 floats，`a_pos`(loc0 offset0)、`a_uv`(loc1 offset 2*float)。
- dtor：`glyph_atlas_.reset()` 先于 `glDelete*`（析构序 creative §7.2）；删 glyph VAO/VBO/program。

---

## 4. 安全

字形数据来自内部 FreeType 栅格化（FT_Bitmap.alpha），文本内容来自内部 layout 树；shader 全 constexpr raw string literal（0 用户拼接）。**本任务不涉及安全变更**；新 glyph shader 经 `kAllShaderSources[]` 自动纳入 `shader_injection_test` S1 覆盖。

---

## 5. 测试策略（D8=A / 承接 P1#1 + P1#2）

- **GlyphAtlas 单测（~8-10）**：构造/texture 非 0、GetOrUpload cache hit/miss 计数、空格 advance、PackGlyph row 换行、atlas 满 clear-repack、OnContextLost/Restored、UV 范围 [0,1]、缺失字形 valid=false。
- **GLESCanvas DrawText 像素测（~10-12）**：单字符红色像素验证、多字符行进、变换后位移、空文本 no-op、透明 brush no-op、无字体 no-op、glGetError 干净、cache 命中复用。
- **P1#1 解析采样坐标**：每个正向像素点用字形 bearing/advance 解析推导落点（不靠直觉），计像素中心 +0.5。
- **P1#2 双通道**：白底正向测 `R>200 && green<50`（红字）；或黑底 `R>200`。反向探针：空文本/透明/无字体后背景通道保持。

---

## 6. 技术债登记（reflect 评估）

- D2 FT 栅格化在 GlyphAtlas 与 SoftwareCanvas 重复（~15 行）→ 可抽 text 模块共享 helper。
- D4 逐字形 draw（N draw call）→ 整行 batch VBO 单 draw（perf）。
- D5 atlas 满 clear-all → 真 LRU（timestamp + free-list）。
- D6 GL_R8 单色 → emoji GL_RGBA 彩色 atlas（G2）。
- D7 Context Lost 方法就位但 App 未集成（G1.14）。

---

**END OF SPEC**
