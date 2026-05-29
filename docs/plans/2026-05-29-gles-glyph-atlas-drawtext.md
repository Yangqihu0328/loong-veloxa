# G1.8 `GlyphAtlas` + `GLESCanvas::DrawText` 实现计划

**目标：** 新增 `GlyphAtlas`（GL_R8 atlas + row-pack + FT 栅格化）+ `GLESCanvas::DrawText`（逐字形 quad + glyph shader），复用 `veloxa/text/` 基础设施，0 新依赖。

**复杂度：** Level 4 / **决策：** D1-D8 = A（all_recommended 锁定，2026-05-29）/ **creative：** 复用 `creative-gles-resources.md` §2.3/§3（reconcile 折入，跳过独立 `/creative`）

**设计规格：** [`docs/specs/2026-05-29-gles-glyph-atlas-drawtext-design.md`](../specs/2026-05-29-gles-glyph-atlas-drawtext-design.md)

---

## 0. Phase 0 audit

### §0.1 ctest baseline fingerprint（实测 ✅）

| Matrix | VX_RENDERER | baseline | 本任务后期望 |
|:-:|:-:|:-:|:-:|
| A | software | **1303** | 1303（0 退化）|
| B | no-devtool | **1141** | 1141（0 退化）|
| C | gles | **1399** | **1417–1421**（+18–22：glyph_atlas_test ~8-10 + gles_canvas_text_test ~10-12）|

### §0.2 reconcile audit（spec §2，5 项 R1-R5 全部前置消化）

- R1 GlyphCache 不栅格化 → GlyphAtlas 自栅格化（镜像 `software_canvas.cc:207-230`）
- R2 key = font|glyph_id|pixel_size；源 `bitmap.alpha.data()`
- R3 glyph shader 对齐 `u_xform_px`/`u_viewport_px`（非 creative `u_proj`）
- R4 DrawText 复用 `FindFont→SetFacePixelSize→ShapeOrLookup` 流程
- R5 测试字体 `/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf` 实测存在

### §0.3 依赖 / FreeType audit

- `veloxa/graphics`（software/ 子目录）已 `FT_Load_Glyph`/`FT_Render_Glyph` → graphics target 已链接 + include FreeType ✅；glyph_atlas.cc 同 target 可直接用 FT API。
- 0 新 FetchContent / 0 新链接 → FetchContent 代理守卫 ⊘ 跳过（git 代理空但无拉取）。

### §0.4 HashMap API audit

`HashMap<u64, GlyphAtlasInfo, std::hash<u64>>`：`find`/`end`/`operator[]`/`clear` 均可用（`hash_map.h:224,282`）✅。

### §0.5 LinkProgram a_uv audit

现 `LinkProgram` 仅 `glBindAttribLocation(program,0,"a_pos")`。新增 `glBindAttribLocation(program,1,"a_uv")`（GL 规范：绑定不存在的 attr name 被忽略，对 solid/rounded/path 程序无害）→ 单一 LinkProgram 复用。

### §0.6 add_test config guard

| 新增测试 | guard | OFF | software | gles |
|---|---|:-:|:-:|:-:|
| `glyph_atlas_test` | `if(VX_RENDERER STREQUAL "gles")` | ❌ | ❌ | ✅ |
| `gles_canvas_text_test` | 同上 | ❌ | ❌ | ✅ |

---

## 1. 文件结构

| # | 文件 | 操作 | 估行 | 共享文件 | 职责 |
|:-:|---|:-:|:-:|:-:|---|
| 1 | `veloxa/graphics/gles/glyph_atlas.h` | 🆕 | ~95 | — | GlyphAtlasInfo + GlyphAtlas 接口 |
| 2 | `veloxa/graphics/gles/glyph_atlas.cc` | 🆕 | ~230 | — | GL_R8 atlas / row-pack / FT 栅格化 / Context Lost |
| 3 | `veloxa/graphics/gles/shaders.h` | 🟡 | +~35 | — | kGlyphVert + kGlyphFrag + kAllShaderSources 注册 |
| 4 | `veloxa/graphics/gles/gles_canvas.h` | 🟡 | +~25 | — | glyph_program_/uniforms/vao/vbo + glyph_atlas_ + DrawText 声明 |
| 5 | `veloxa/graphics/gles/gles_canvas.cc` | 🟡 | +~150 | — | glyph program init + glyph VAO + DrawText impl + LinkProgram a_uv |
| 6 | `veloxa/graphics/gles/CMakeLists.txt` 或 `graphics/CMakeLists.txt` | 🟡 | +~2 | **[共享]** | 注册 glyph_atlas.cc 到 vx_graphics（仅 gles）|
| 7 | `tests/graphics/gles/glyph_atlas_test.cc` | 🆕 | ~240 | — | ~8-10 单测 |
| 8 | `tests/graphics/gles/gles_canvas_text_test.cc` | 🆕 | ~260 | — | ~10-12 像素测 |
| 9 | `tests/CMakeLists.txt` | 🟡 | +~16 | **[共享]** | 注册 2 测（gles guard）|
| **合计** | — | — | **~1053** | — | LOC ×[0.85,1.5] = ~895–1580 |

---

## 2. 实现步骤（TDD 严格顺序）

> 本任务为 Level 4，Build 分 **2 轮次**：轮次 1 = GlyphAtlas（独立可测）；轮次 2 = DrawText 集成。每轮 RED→GREEN→REFACTOR→commit。

### 轮次 1 — GlyphAtlas

#### Phase 1A RED

**1A.1** 创建 `tests/graphics/gles/glyph_atlas_test.cc`（fixture：SDL offscreen + EGL MakeCurrent + FontManager 加载 DejaVu）。

测试矩阵（~8-10）：

| ID | 名称 | 断言 |
|:-:|---|---|
| A1 | `Ctor_TextureNonZero` | 构造后 `texture_id()!=0` |
| A2 | `GetOrUpload_AsciiValidInfo` | 'A' 字形 `valid && width>0 && height>0 && advance>0` |
| A3 | `GetOrUpload_CacheHitCounter` | 同字形二次调用 `cache_hits()==1` |
| A4 | `GetOrUpload_CacheMissCounter` | 两不同 glyph `cache_misses()==2` |
| A5 | `GetOrUpload_SpaceZeroSize` | 空格 glyph `valid && width==0 && advance>0` |
| A6 | `GetOrUpload_UVInRange` | `0<=u0<u1<=1 && 0<=v0<v1<=1` |
| A7 | `GetOrUpload_MissingGlyphInvalid` | glyph_id=0xFFFFFF（.notdef 越界）→ `valid==false`（不崩溃）|
| A8 | `PackGlyph_RowWrap` | 上传 > atlas_w 宽度总和的多字形后 cursor_y 前进（间接：uploads 计数 + 无 GL error）|
| A9 | `OnContextLost_TextureZero` | `OnContextLost()` 后 `texture_id()==0` |
| A10 | `OnContextRestored_TextureNonZero` | `OnContextRestored()` 后 `texture_id()!=0` 且 cache 清空（再 GetOrUpload → miss）|

**1A.2** 注册 `glyph_atlas_test`（gles guard）+ `graphics/CMakeLists.txt` 加 glyph_atlas.cc。

**1A.3** `ctest -R GlyphAtlasTest` → 编译失败（类不存在）→ 创建空壳 → FAIL（断言未实现）。

#### Phase 1B GREEN

**1B.1** `glyph_atlas.h`（spec §3.1）。

**1B.2** `glyph_atlas.cc`：

```cpp
#include "veloxa/graphics/gles/glyph_atlas.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include "veloxa/text/font_manager.h"
#include "veloxa/text/glyph_cache.h"

namespace vx::gfx::gles {

GlyphAtlas::GlyphAtlas(vx::text::FontManager* fm, vx::text::GlyphCache* gc,
                       vx::u32 w, vx::u32 h)
    : font_manager_(fm), glyph_cache_(gc), atlas_width_(w), atlas_height_(h) {
  CreateTexture();
}

GlyphAtlas::~GlyphAtlas() {
  if (texture_ != 0) glDeleteTextures(1, &texture_);
}

void GlyphAtlas::CreateTexture() {
  glGenTextures(1, &texture_);
  glBindTexture(GL_TEXTURE_2D, texture_);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, static_cast<GLsizei>(atlas_width_),
               static_cast<GLsizei>(atlas_height_), 0, GL_RED,
               GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

vx::u64 GlyphAtlas::MakeKey(vx::text::FontHandle f, vx::u32 g, vx::u32 px) {
  return (static_cast<vx::u64>(f) << 40) |
         (static_cast<vx::u64>(g & 0xFFFFFF) << 16) |
         static_cast<vx::u64>(px & 0xFFFF);
}

bool GlyphAtlas::PackGlyph(vx::u32 w, vx::u32 h, vx::u32* ox, vx::u32* oy) {
  if (w > atlas_width_ || h > atlas_height_) return false;
  if (cursor_x_ + w > atlas_width_) {
    cursor_x_ = 0;
    cursor_y_ += row_height_;
    row_height_ = 0;
  }
  if (cursor_y_ + h > atlas_height_) return false;
  *ox = cursor_x_;
  *oy = cursor_y_;
  cursor_x_ += w;
  if (h > row_height_) row_height_ = h;
  return true;
}

GlyphAtlasInfo GlyphAtlas::GetOrUpload(vx::text::FontHandle font,
                                       vx::u32 glyph_id, vx::u32 pixel_size) {
  const vx::u64 key = MakeKey(font, glyph_id, pixel_size);
  auto it = cache_.find(key);
  if (it != cache_.end()) {
    ++cache_hits_;
    return it->second;
  }
  ++cache_misses_;

  // 1. GlyphCache 查 / miss 时 FT 栅格化（镜像 software_canvas.cc）
  const vx::text::GlyphBitmap* bmp =
      glyph_cache_->Get(font, glyph_id, pixel_size);
  if (bmp == nullptr) {
    FT_FaceRec_* face = font_manager_->SetFacePixelSize(font, pixel_size);
    if (face == nullptr) return {};
    if (FT_Load_Glyph(face, glyph_id, FT_LOAD_DEFAULT) != 0) return {};
    if (FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL) != 0) return {};
    FT_Bitmap& fb = face->glyph->bitmap;
    vx::text::GlyphBitmap g;
    g.width = fb.width;
    g.height = fb.rows;
    g.bearing_x = face->glyph->bitmap_left;
    g.bearing_y = face->glyph->bitmap_top;
    g.advance = static_cast<vx::f32>(face->glyph->advance.x >> 6);
    g.alpha.reserve(static_cast<vx::usize>(fb.width) * fb.rows);
    for (vx::u32 r = 0; r < fb.rows; ++r)
      for (vx::u32 c = 0; c < fb.width; ++c)
        g.alpha.push_back(fb.buffer[r * static_cast<vx::u32>(fb.pitch) + c]);
    bmp = glyph_cache_->Put(font, glyph_id, pixel_size,
                            static_cast<vx::text::GlyphBitmap&&>(g));
    if (bmp == nullptr) return {};
  }

  GlyphAtlasInfo info;
  info.bearing_x = bmp->bearing_x;
  info.bearing_y = bmp->bearing_y;
  info.width = static_cast<vx::i32>(bmp->width);
  info.height = static_cast<vx::i32>(bmp->height);
  info.advance = bmp->advance;
  info.valid = true;

  // 2. 空字形（空格）→ 无像素，仅 advance
  if (bmp->width == 0 || bmp->height == 0) {
    cache_[key] = info;
    return info;
  }

  // 3. pack（+1 padding 防双线性渗色）
  vx::u32 x = 0, y = 0;
  if (!PackGlyph(bmp->width + 1, bmp->height + 1, &x, &y)) {
    cache_.clear();
    cursor_x_ = cursor_y_ = row_height_ = 0;
    if (!PackGlyph(bmp->width + 1, bmp->height + 1, &x, &y)) return {};
  }

  // 4. upload GL_R8
  glBindTexture(GL_TEXTURE_2D, texture_);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexSubImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(x), static_cast<GLint>(y),
                  static_cast<GLsizei>(bmp->width),
                  static_cast<GLsizei>(bmp->height), GL_RED, GL_UNSIGNED_BYTE,
                  bmp->alpha.data());
  ++uploads_this_frame_;

  // 5. UV
  info.u0 = static_cast<vx::f32>(x) / atlas_width_;
  info.v0 = static_cast<vx::f32>(y) / atlas_height_;
  info.u1 = static_cast<vx::f32>(x + bmp->width) / atlas_width_;
  info.v1 = static_cast<vx::f32>(y + bmp->height) / atlas_height_;
  cache_[key] = info;
  return info;
}

void GlyphAtlas::OnContextLost() {
  texture_ = 0;  // driver 已释放 / 不 glDelete
  cache_.clear();
  cursor_x_ = cursor_y_ = row_height_ = 0;
}

void GlyphAtlas::OnContextRestored() {
  CreateTexture();
  cache_.clear();
  cursor_x_ = cursor_y_ = row_height_ = 0;
}

}  // namespace vx::gfx::gles
```

**1B.3** `ctest -R GlyphAtlasTest` → 8-10/10 PASS。

#### Phase 1C REFACTOR + commit

- ReadLints；commit `feat(graphics): add GlyphAtlas (GL_R8 + row-pack + FT raster) — G1.8`。

---

### 轮次 2 — GLESCanvas::DrawText

#### Phase 2A RED

**2A.1** 创建 `tests/graphics/gles/gles_canvas_text_test.cc`（fixture：offscreen + EGL + FontManager+GlyphCache + GLESCanvas(surface, fm, gc)）。

测试矩阵（~10-12）：

| ID | 名称 | 断言（P1#1 解析采样 + P1#2 双通道）|
|:-:|---|---|
| T1 | `DrawText_SingleChar_RedPixel` | 黑底画红 'A'，字形实心区采样 `R>200 && green<50` |
| T2 | `DrawText_WhiteBg_DoubleChannel` | 白底画红 'A'，字形区 `R>200 && green<50`；字外白 `green>200` |
| T3 | `DrawText_MultiChar_Advances` | "AB" 第二字形位于第一字形 advance 右侧（解析 pen_x）|
| T4 | `DrawText_AfterSetTransform` | Translate(dx,dy) 后字形整体位移 |
| T5 | `DrawText_KSolidBlueBrush` | 蓝字 `B>200 && R<50` |
| T6 | `DrawText_EmptyString_NoOp` | 空串背景不变 |
| T7 | `DrawText_TransparentBrush_NoOp` | alpha=0 背景不变 |
| T8 | `DrawText_NoFont_NoOp` | 未加载字体 → no-op 不崩溃 |
| T9 | `DrawText_MultipleDraws_NoGLError` | 3× draw `glGetError()==GL_NO_ERROR` |
| T10 | `DrawText_CacheReuse` | 同串二次 draw → atlas `cache_hits()>0` |
| T11 | `DrawText_SpaceHandling` | "A B" 含空格不崩溃 + 第三字形在空格 advance 后 |
| T12 | `DrawText_BaselinePosition` | 字形顶部约在 `bounds.y + ascender - bearing_y`（解析 ±2px）|

**2A.2** 注册 `gles_canvas_text_test`。**2A.3** RED：DrawText stub → 像素测 FAIL。

#### Phase 2B GREEN

**2B.1** `shaders.h` 加 kGlyphVert + kGlyphFrag（spec §3.3）+ `kAllShaderSources[]` 注册。

**2B.2** `gles_canvas.h`：加 glyph_program_ / `enum GlyphUniform{kGlyphUXformPx,kGlyphUViewportPx,kGlyphUColor,kGlyphUAtlas,kGlyphUniformCount}` / glyph_uniforms_ / glyph_vao_ / glyph_vbo_ / `std::unique_ptr<GlyphAtlas> glyph_atlas_` / DrawText override 声明（移除 stub）/ `#include "veloxa/graphics/gles/glyph_atlas.h"`。

**2B.3** `gles_canvas.cc`：
- `LinkProgram` 增 `glBindAttribLocation(program, 1, "a_uv")`（§0.5）。
- ctor：glyph program（kGlyphVert+kGlyphFrag）+ 缓存 4 uniform；glyph VAO/VBO（stride 4f：a_pos loc0 off0 / a_uv loc1 off2f）；`glyph_atlas_ = std::make_unique<GlyphAtlas>(font_manager_, glyph_cache_)`（仅当 fm&&gc 非空）。
- dtor：`glyph_atlas_.reset()`（先于 glDelete）+ 删 glyph VAO/VBO + glyph_program_ 入 DestroyShaderPrograms。
- DrawText 实现（spec §3.4）：

```cpp
void GLESCanvas::DrawText(vx::StringView text, const Rect& bounds,
                          vx::f32 font_size, const Brush& brush) {
  if (glyph_program_ == 0 || font_manager_ == nullptr ||
      glyph_cache_ == nullptr || glyph_atlas_ == nullptr || text.empty())
    return;
  Color c = BrushSolidColor(brush);
  if (c.a == 0) return;

  using namespace vx::text;
  FontHandle font = kInvalidFont;
  if (font_manager_->font_count() > 0) {
    font = font_manager_->FindFont("", 400);
    if (font == kInvalidFont) font = 1;
  }
  if (font == kInvalidFont) return;

  vx::u32 pixel_size = static_cast<vx::u32>(font_size);
  if (pixel_size == 0) pixel_size = 1;
  FT_FaceRec_* face = font_manager_->SetFacePixelSize(font, pixel_size);
  if (face == nullptr) return;
  const ShapedRun* shaped =
      font_manager_->ShapeOrLookup(font, pixel_size, text);
  if (shaped == nullptr) return;

  vx::f32 pen_x = bounds.x;
  vx::f32 pen_y = bounds.y + static_cast<vx::f32>(face->size->metrics.ascender >> 6);

  GLfloat mat[9];
  Matrix3x2ToMat3(transform_, mat);
  glUseProgram(glyph_program_);
  glUniformMatrix3fv(glyph_uniforms_[kGlyphUXformPx], 1, GL_FALSE, mat);
  glUniform2f(glyph_uniforms_[kGlyphUViewportPx],
              static_cast<GLfloat>(width_), static_cast<GLfloat>(height_));
  glUniform4f(glyph_uniforms_[kGlyphUColor], c.r / 255.0f, c.g / 255.0f,
              c.b / 255.0f, c.a / 255.0f);
  glUniform1i(glyph_uniforms_[kGlyphUAtlas], 0);
  glActiveTexture(GL_TEXTURE0);

  glBindVertexArray(glyph_vao_);
  for (vx::usize i = 0; i < shaped->glyphs.size(); ++i) {
    const ShapedGlyph& g = shaped->glyphs[i];
    GlyphAtlasInfo info =
        glyph_atlas_->GetOrUpload(font, g.glyph_id, pixel_size);
    if (info.valid && info.width > 0 && info.height > 0) {
      vx::f32 x0 = pen_x + g.x_offset + static_cast<vx::f32>(info.bearing_x);
      vx::f32 y0 = pen_y - g.y_offset - static_cast<vx::f32>(info.bearing_y);
      vx::f32 x1 = x0 + static_cast<vx::f32>(info.width);
      vx::f32 y1 = y0 + static_cast<vx::f32>(info.height);
      const GLfloat verts[24] = {
          x0, y0, info.u0, info.v0,  x1, y0, info.u1, info.v0,
          x0, y1, info.u0, info.v1,  x0, y1, info.u0, info.v1,
          x1, y0, info.u1, info.v0,  x1, y1, info.u1, info.v1,
      };
      glBindBuffer(GL_ARRAY_BUFFER, glyph_vbo_);
      glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STREAM_DRAW);
      glBindTexture(GL_TEXTURE_2D, glyph_atlas_->texture_id());
      glDrawArrays(GL_TRIANGLES, 0, 6);
    }
    pen_x += g.x_advance;
  }
  glBindVertexArray(0);
}
```

glyph VAO 初始化（InitGlyphGeometry，ctor 调）：
```cpp
glBindVertexArray(glyph_vao_);
glBindBuffer(GL_ARRAY_BUFFER, glyph_vbo_);
glEnableVertexAttribArray(0);
glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(GLfloat), (void*)0);
glEnableVertexAttribArray(1);
glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(GLfloat),
                      (void*)(2*sizeof(GLfloat)));
glBindVertexArray(0);
```

**2B.4** `ctest -R GLESCanvasTextTest` → 10-12/12 PASS。

#### Phase 2C REFACTOR + 2D 三矩阵

- ReadLints；`shader_injection_test` 自动覆盖 2 新 shader（确认 S1 范围）。
- 三矩阵：gles 1399→~1419 / software 1303 / no-devtool 1141。
- commit `feat(graphics): implement GLESCanvas::DrawText via GlyphAtlas — G1.8` + finalize。

---

## 3. Commit 时间线

| Phase | subject |
|---|---|
| Plan | `chore(plan): land G1.8 GlyphAtlas + DrawText plan + memory bank` |
| 1A RED | `test(gles): GlyphAtlas RED — ~10 unit tests` |
| 1B/C | `feat(graphics): add GlyphAtlas (GL_R8 + row-pack + FT raster) — G1.8` |
| 2A RED | `test(gles): GLESCanvas DrawText RED — ~12 pixel tests` |
| 2B/C/D | `feat(graphics): implement GLESCanvas::DrawText via GlyphAtlas — G1.8` |
| finalize | `chore(build): finalize TASK-20260529-03 ctest matrix — G1.8` |

---

## 4. 风险登记

| ID | 风险 | 级别 | 缓解 |
|---|---|:-:|---|
| R1 | Mesa swrast GL_R8 texture / glTexSubImage2D 不支持 | 🟡 | A1/A2 早测；若整体 blank → SKIP_IF_SWRAST_BLANK 范式 |
| R2 | 双线性采样字形边缘渗色 | 🟢 | +1 padding（PackGlyph）|
| R3 | FT 栅格化重复（GlyphAtlas vs SoftwareCanvas）| 🟢 | MVP 接受 / reflect 评估抽 helper |
| R4 | 逐字形 draw 性能 | 🟢 | MVP / batch 留 perf 债 |
| R5 | ascender>>6 基线在 swrast 字形偏移 | 🟡 | T12 解析采样 ±2px 容差 |
| R6 | glyph_id 越界（A7）FT 行为 | 🟢 | FT_Load_Glyph 返回非 0 → return {} valid=false |

---

## 5. 反复模式预防（8/8）

| # | 模式 | 抑制 |
|:-:|---|---|
| #1 | 前置依赖未验证 | §0.1-0.6 + 字体实测 ✅ |
| #2 | spec 数据回归 | R1-R5 reconcile 写入 spec/plan ✅ |
| #3 | TDD 倒置 | 2 轮次均 RED 先于 GREEN ✅ |
| #4 | 反向探针弱 | T6/T7/T8 + A7 ✅ |
| #5 | 中文 StrReplace | 无中文 doc 改动 ✅ |
| #6 | Source 溯源 | commit body 必填（FT raster 镜像 software_canvas.cc 标注）✅ |
| #7 | 双 config 盲区 | Phase 2D 三矩阵 ✅ |
| #8 | ctest baseline | §0.1 实测 1399 ✅ |
| **P1#1** | 像素采样坐标几何误判 | T1-T5/T12 **解析推导字形落点 + 像素中心 +0.5** ✅ |
| **P1#2** | 白底假绿 | T2 **双通道 `R>200 && green<50`** ✅ |

---

## 6. 创意阶段需求

⊘ **跳过独立 `/creative`** — `creative-gles-resources.md` §2.3/§3 已有 GlyphAtlas + glyph shader 设计，reconcile（spec §2）已折入本 plan，无新增 UI/算法决策。

---

## 7. 估时（plan ×0.6）

| 阶段 | 估时 |
|---|---|
| Plan | ~40-55 min |
| 轮次 1（GlyphAtlas）| ~50-75 min |
| 轮次 2（DrawText）| ~60-90 min |
| C/D finalize | ~15-25 min |
| **总计 plan ×0.6** | **~165-245 min** |
| **预期实测** | **~120-180 min**（GLES hex+ 极速区续延 / 但 Level 4 资源类首见 GlyphAtlas）|

---

**下一步：** `/build` — 轮次 1 RED → GREEN → 轮次 2 RED → GREEN → C/D

**Source:** 蓝图 §3.8 + creative-gles-resources §2.3/§3 + software_canvas.cc:143-272（DrawText 流程镜像）
