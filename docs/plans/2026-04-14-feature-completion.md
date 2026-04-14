# 功能补全实现计划

**目标：** 补全 border-radius 渲染、FreeType+HarfBuzz 字体、图片支持、QuickJS DOM 绑定四项核心功能

**架构：** 4 次迭代，每次贯通一个子系统并独立验证；迭代顺序按依赖和复杂度递增排列

**技术栈：** C++17 / CMake / FreeType 2.11 / HarfBuzz 2.7 / libpng 1.6 / libjpeg-turbo 2.1 / QuickJS-ng 0.14

**复杂度级别：** Level 4

---

## 文件结构总览

### 新建文件

```
veloxa/text/
  font_manager.h / font_manager.cc     — 字体加载/注册/查询
  glyph_cache.h / glyph_cache.cc       — 字形位图缓存
  freetype_shaper.h / freetype_shaper.cc — FreeTypeTextShaper 实现
  CMakeLists.txt                        — vx_text 目标

veloxa/graphics/
  image.h                               — Image 类（解码后 RGBA 像素）

veloxa/core/image/
  image_decoder.h / image_decoder.cc    — 格式分发 + PNG/JPEG 解码
  image_cache.h / image_cache.cc        — 按路径去重缓存

veloxa/script/
  dom_bindings.h / dom_bindings.cc      — JS ↔ DOM 桥接

tests/text/
  font_manager_test.cc
  glyph_cache_test.cc
  freetype_shaper_test.cc

tests/core/image/
  image_decoder_test.cc
  image_cache_test.cc

tests/core/render/
  border_radius_test.cc

tests/script/
  dom_bindings_test.cc

tests/assets/                           — 测试资源（小 PNG/JPEG 文件）
```

### 修改文件

| 文件 | 变更摘要 |
|------|---------|
| `veloxa/core/render/paint_command.h` | +kFillRoundedRect, +kStrokeRoundedRect, +kDrawImage; +font_handle/radius/image_handle 字段 |
| `veloxa/core/render/renderer.cc` | RecordBox 使用圆角命令 + 图片命令; Replay 分发新类型 |
| `veloxa/core/render/renderer.h` | Record 增加 ImageCache* 参数（迭代 3） |
| `veloxa/graphics/canvas.h` | DrawText 签名加 FontHandle; +DrawImage |
| `veloxa/graphics/software/software_canvas.h` | 同上; +FontManager*/ImageCache* 引用 |
| `veloxa/graphics/software/software_canvas.cc` | DrawText 真实字形渲染; +DrawImage 实现 |
| `veloxa/core/layout/layout_box.h` | +kReplaced; +image_handle 字段 |
| `veloxa/core/layout/layout_utils.h` | LayoutContext +ImageCache* +FontManager* |
| `veloxa/core/layout/layout_engine.cc` | BuildTree 处理 <img>; LayoutChild 处理 kReplaced |
| `veloxa/core/layout/text_shaper.h` | Measure 增加 FontHandle 参数 |
| `veloxa/core/dom/element.h` | +inline_declarations_ 字段 |
| `veloxa/core/application.h/.cc` | +FontManager +ImageCache; 替换 SimpleTextShaper→FreeTypeTextShaper; +BindDOM |
| `veloxa/api/veloxa_api.h/.cc` | +vx_view_load_font, +vx_view_load_script |
| `veloxa/script/quickjs_engine.h/.cc` | +BindDOM/UnbindDOM; +JSContext 访问器 |
| `veloxa/script/CMakeLists.txt` | 链接 vx_core |
| `veloxa/graphics/CMakeLists.txt` | 链接 vx_text（迭代 2 后） |
| `veloxa/core/CMakeLists.txt` | +image_decoder.cc, image_cache.cc; 链接 PNG JPEG |
| `tests/CMakeLists.txt` | +新测试目标 |
| `CMakeLists.txt`（根） | +add_subdirectory(veloxa/text) |

---

## Iteration 1：border-radius 渲染

### Phase 1.1：PaintCommand 扩展 [TDD]

**文件：**
- 修改：`veloxa/core/render/paint_command.h`
- 测试：`tests/core/render/border_radius_test.cc`

**步骤 1：编写失败测试**

```cpp
// tests/core/render/border_radius_test.cc
#include <gtest/gtest.h>
#include "veloxa/core/render/paint_command.h"

namespace vx::render {

TEST(PaintCommandTest, FillRoundedRectFactory) {
  gfx::Rect r{10, 20, 100, 50};
  gfx::Color c = gfx::Color::Red();
  auto cmd = PaintCommand::FillRoundedRect(r, 8.0f, c);
  EXPECT_EQ(cmd.type, PaintCommand::Type::kFillRoundedRect);
  EXPECT_EQ(cmd.rect, r);
  EXPECT_EQ(cmd.color, c);
  EXPECT_FLOAT_EQ(cmd.param, 8.0f);
}

TEST(PaintCommandTest, StrokeRoundedRectFactory) {
  gfx::Rect r{0, 0, 200, 100};
  gfx::Color c = gfx::Color::Blue();
  auto cmd = PaintCommand::StrokeRoundedRect(r, 12.0f, c, 2.0f);
  EXPECT_EQ(cmd.type, PaintCommand::Type::kStrokeRoundedRect);
  EXPECT_EQ(cmd.rect, r);
  EXPECT_EQ(cmd.color, c);
  EXPECT_FLOAT_EQ(cmd.param, 12.0f);
  EXPECT_FLOAT_EQ(cmd.param2, 2.0f);
}

TEST(PaintCommandTest, RoundedRectEquality) {
  auto a = PaintCommand::FillRoundedRect({0,0,100,50}, 8.0f, gfx::Color::Red());
  auto b = PaintCommand::FillRoundedRect({0,0,100,50}, 8.0f, gfx::Color::Red());
  auto c = PaintCommand::FillRoundedRect({0,0,100,50}, 4.0f, gfx::Color::Red());
  EXPECT_EQ(a, b);
  EXPECT_NE(a, c);
}

}  // namespace vx::render
```

**步骤 2：运行测试验证失败**
```bash
cmake --build build && ctest --test-dir build -R border_radius
```
预期：编译失败（kFillRoundedRect 不存在）

**步骤 3：实现**

`paint_command.h` 变更：
- Type 枚举增加 `kFillRoundedRect`、`kStrokeRoundedRect`
- 结构体增加 `f32 param2 = 0`（用于 stroke width）
- 新增工厂函数 `FillRoundedRect(rect, radius, color)`、`StrokeRoundedRect(rect, radius, color, stroke_width)`
- `operator==` 增加 `param2` 比较

**步骤 4：运行测试验证通过**

**步骤 5：提交** `feat(render): add FillRoundedRect/StrokeRoundedRect paint commands`

### Phase 1.2：Renderer Record/Replay 圆角支持 [TDD]

**文件：**
- 修改：`veloxa/core/render/renderer.cc`
- 测试：`tests/core/render/border_radius_test.cc`（追加）

**步骤 1：编写失败测试**

```cpp
TEST(BorderRadiusRenderTest, RecordUsesRoundedRectWhenRadiusSet) {
  ArenaAllocator arena(4096);
  layout::LayoutBox* box = /* 构建带 border_radius=10px 的 LayoutBox */;
  auto list = render::Record(box);
  // 验证 DisplayList 包含 kFillRoundedRect 而非 kFillRect
  bool has_rounded = false;
  for (const auto& cmd : list) {
    if (cmd.type == PaintCommand::Type::kFillRoundedRect) has_rounded = true;
  }
  EXPECT_TRUE(has_rounded);
}

TEST(BorderRadiusRenderTest, ReplayCallsFillRoundedRect) {
  // 构建含 kFillRoundedRect 命令的 DisplayList
  // Replay 到 SoftwareCanvas → 验证像素不全为空（间接验证）
}
```

**步骤 2：运行测试验证失败**（Record 仍用 FillRect）

**步骤 3：实现**

`renderer.cc` 变更：
- `RecordBox()`：resolve `border_radius` → `f32 radius`
  ```cpp
  f32 radius = 0;
  if (style->border_radius.unit == css::Unit::kPx)
    radius = style->border_radius.value;
  ```
- 背景：`radius > 0 ? FillRoundedRect(border_box, radius, bg) : FillRect(border_box, bg)`
- 边框：`radius > 0 ? StrokeRoundedRect(inset_rect, radius, top_border_color, top_border_width) : PaintBorders(...)`
- `Replay()`：增加 `kFillRoundedRect` → `canvas->FillRoundedRect()`、`kStrokeRoundedRect` → `canvas->StrokeRoundedRect()`

**步骤 4：运行全量测试验证通过**
```bash
cmake --build build && ctest --test-dir build --output-on-failure
```

**步骤 5：提交** `feat(render): wire border-radius to Record/Replay pipeline`

---

## Iteration 2：FreeType + HarfBuzz 字体渲染

### Phase 2.1：CMake 基础设施 + FontHandle 类型 [TDD]

**文件：**
- 新建：`veloxa/text/CMakeLists.txt`
- 新建：`veloxa/text/font_manager.h`（FontHandle 类型定义 + FontManager 接口声明）
- 修改：`CMakeLists.txt`（根，add_subdirectory）

**步骤 1：创建 CMakeLists.txt + FontHandle 类型**

```cmake
# veloxa/text/CMakeLists.txt
find_package(Freetype REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(HARFBUZZ REQUIRED harfbuzz)

add_library(vx_text STATIC
  font_manager.cc
  glyph_cache.cc
  freetype_shaper.cc
)
target_include_directories(vx_text PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(vx_text
  PUBLIC vx_foundation
  PRIVATE Freetype::Freetype ${HARFBUZZ_LIBRARIES}
)
target_include_directories(vx_text PRIVATE ${HARFBUZZ_INCLUDE_DIRS})
set_target_properties(vx_text PROPERTIES CXX_STANDARD 17)
```

```cpp
// veloxa/text/font_manager.h — 最小定义
#ifndef VELOXA_TEXT_FONT_MANAGER_H_
#define VELOXA_TEXT_FONT_MANAGER_H_

#include "veloxa/foundation/base/types.h"
#include "veloxa/foundation/base/status.h"
#include "veloxa/foundation/strings/string_view.h"

namespace vx::text {

using FontHandle = u32;
static constexpr FontHandle kInvalidFont = 0;

class FontManager {
 public:
  FontManager();
  ~FontManager();

  FontManager(const FontManager&) = delete;
  FontManager& operator=(const FontManager&) = delete;

  Status Init();
  void Shutdown();

  StatusOr<FontHandle> LoadFont(StringView path, StringView family);
  FontHandle FindFont(StringView family, u16 weight = 400) const;

  // 内部：供 GlyphCache / TextShaper / Canvas 使用
  struct FontData;
  const FontData* GetFontData(FontHandle handle) const;

 private:
  struct Impl;
  Impl* impl_ = nullptr;
};

}  // namespace vx::text

#endif
```

**步骤 2：创建空 .cc 存根（让 CMake 配置通过）**

**步骤 3：验证 cmake 配置成功**
```bash
cmake -B build && cmake --build build --target vx_text
```

**步骤 4：提交** `build(text): add vx_text module with FreeType+HarfBuzz CMake setup`

### Phase 2.2：FontManager 实现 [TDD]

**文件：**
- 修改：`veloxa/text/font_manager.cc`
- 测试：`tests/text/font_manager_test.cc`

**步骤 1：编写失败测试**

```cpp
TEST(FontManagerTest, InitShutdown) {
  text::FontManager fm;
  ASSERT_TRUE(fm.Init().ok());
  fm.Shutdown();
}

TEST(FontManagerTest, LoadValidFont) {
  text::FontManager fm;
  ASSERT_TRUE(fm.Init().ok());
  auto result = fm.LoadFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", "DejaVuSans");
  ASSERT_TRUE(result.ok());
  EXPECT_NE(result.value(), text::kInvalidFont);
  fm.Shutdown();
}

TEST(FontManagerTest, LoadInvalidPath) {
  text::FontManager fm;
  ASSERT_TRUE(fm.Init().ok());
  auto result = fm.LoadFont("/nonexistent/font.ttf", "Missing");
  EXPECT_FALSE(result.ok());
  fm.Shutdown();
}

TEST(FontManagerTest, FindFont) {
  text::FontManager fm;
  ASSERT_TRUE(fm.Init().ok());
  auto r = fm.LoadFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", "DejaVuSans");
  ASSERT_TRUE(r.ok());
  FontHandle h = fm.FindFont("DejaVuSans");
  EXPECT_EQ(h, r.value());
  EXPECT_EQ(fm.FindFont("NonExistent"), text::kInvalidFont);
  fm.Shutdown();
}
```

**步骤 2：运行验证失败**

**步骤 3：实现 FontManager**
- `Impl` 持有 `FT_Library` + `Vector<FontData>` (FT_Face + family + handle)
- `LoadFont`：`FT_New_Face` → 存入列表 → 返回 handle（1-based index）
- `FindFont`：线性扫描 family name 匹配
- `GetFontData`：index 查找

**步骤 4：验证通过**

**步骤 5：提交** `feat(text): implement FontManager with FreeType font loading`

### Phase 2.3：GlyphCache 实现 [TDD]

**文件：**
- 修改：`veloxa/text/glyph_cache.h/.cc`
- 测试：`tests/text/glyph_cache_test.cc`

GlyphCache 接口：

```cpp
struct GlyphBitmap {
  Vector<u8> alpha;    // 8-bit alpha values
  u32 width, height;
  i32 bearing_x, bearing_y;
  f32 advance;
};

class GlyphCache {
 public:
  const GlyphBitmap* Get(FontHandle font, u32 glyph_id, u32 pixel_size);
  void Put(FontHandle font, u32 glyph_id, u32 pixel_size, GlyphBitmap bitmap);
  usize size() const;
  void Clear();
};
```

测试覆盖：Put/Get 命中、Miss 返回 nullptr、不同 size 独立缓存。

**提交** `feat(text): implement GlyphCache with HashMap lookup`

### Phase 2.4：FreeTypeTextShaper 实现 [TDD]

**文件：**
- 修改：`veloxa/text/freetype_shaper.h/.cc`
- 修改：`veloxa/core/layout/text_shaper.h`（Measure 签名增加 FontHandle）
- 测试：`tests/text/freetype_shaper_test.cc`

TextShaper 接口变更：

```cpp
class TextShaper {
 public:
  virtual ~TextShaper() = default;
  virtual TextMetrics Measure(StringView text, f32 font_size,
                              u16 font_weight,
                              text::FontHandle font = text::kInvalidFont) = 0;
};
```

SimpleTextShaper：忽略新参数，行为不变（向后兼容）。

FreeTypeTextShaper：
- 持有 `FontManager*` 引用
- `Measure`：获取 FT_Face → `hb_font_create(hb_ft_font_create)` → `hb_buffer_add_utf8` → `hb_shape` → 累加 glyph advances
- 返回精确 TextMetrics

测试：加载 DejaVuSans → Measure "Hello" → 验证 width > 0 且合理（如 30-60px at 16px size）。

**提交** `feat(text): implement FreeTypeTextShaper with HarfBuzz shaping`

### Phase 2.5：Canvas::DrawText 签名变更 + SoftwareCanvas 实现 [TDD]

**文件：**
- 修改：`veloxa/graphics/canvas.h`
- 修改：`veloxa/graphics/software/software_canvas.h/.cc`
- 修改：`veloxa/core/render/paint_command.h`（kDrawText 增加 font_handle）
- 修改：`veloxa/core/render/renderer.cc`（Record 传 font_handle，Replay 传 font）
- 测试：`tests/graphics/software_canvas_test.cc`（追加字体渲染测试）

Canvas::DrawText 新签名：

```cpp
virtual void DrawText(StringView text, const Rect& bounds,
                      text::FontHandle font, f32 font_size,
                      const Brush& brush) = 0;
```

SoftwareCanvas 变更：
- 构造函数增加 `text::FontManager*` 参数（可选，nullptr 时退化为旧行为）
- `DrawText`：若 FontManager 可用且 font 有效 → HarfBuzz shape → FreeType render each glyph → alpha blend；否则退化为旧存根

PaintCommand::kDrawText 增加 `u32 font_handle = 0` 字段。

renderer.cc Record 阶段：从 ComputedStyle 解析 font_family → FontManager::FindFont → 写入 PaintCommand。
需在 Record/Replay 中传递 FontManager*（新增参数或通过 context）。

**提交** `feat(text): integrate FreeType glyph rendering into SoftwareCanvas::DrawText`

### Phase 2.6：C API + Application 集成 [TDD]

**文件：**
- 修改：`veloxa/api/veloxa_api.h/.cc`
- 修改：`veloxa/core/application.h/.cc`
- 测试：`tests/api/api_test.cc`（追加）

C API：

```c
VxResult vx_view_load_font(VxView* view, const char* path, const char* family);
```

Application 变更：
- `text_shaper_` 从 `SimpleTextShaper` 改为 `std::unique_ptr<layout::TextShaper>`
- 新增 `text::FontManager font_manager_`
- 构造时 `font_manager_.Init()`；若无字体加载则退化为 SimpleTextShaper
- `LoadFont()` → `font_manager_.LoadFont()` → 替换 text_shaper_ 为 FreeTypeTextShaper
- Canvas 创建时传入 `&font_manager_`

测试：`vx_view_load_font` 加载字体 → `vx_view_load_html` 含文本 → `vx_view_update` → 验证像素有文字区域非空。

**提交** `feat(api): add vx_view_load_font and wire FreeType into Application`

---

## Iteration 3：图片支持

### Phase 3.1：Image 类 [TDD]

**文件：**
- 新建：`veloxa/graphics/image.h`
- 测试：`tests/core/image/image_decoder_test.cc`（先测 Image 基础）

```cpp
// veloxa/graphics/image.h
#ifndef VELOXA_GRAPHICS_IMAGE_H_
#define VELOXA_GRAPHICS_IMAGE_H_

#include <memory>
#include "veloxa/foundation/base/types.h"

namespace vx::gfx {

class Image {
 public:
  Image(u32 width, u32 height);

  u32 width() const { return width_; }
  u32 height() const { return height_; }
  u32 stride() const { return width_ * 4; }
  const u32* pixels() const { return pixels_.get(); }
  u32* pixels() { return pixels_.get(); }
  bool valid() const { return pixels_ != nullptr && width_ > 0 && height_ > 0; }

 private:
  u32 width_, height_;
  std::unique_ptr<u32[]> pixels_;
};

using ImageHandle = u32;
static constexpr ImageHandle kInvalidImage = 0;

}  // namespace vx::gfx

#endif
```

**提交** `feat(graphics): add Image class for decoded pixel data`

### Phase 3.2：ImageDecoder (PNG + JPEG) [TDD]

**文件：**
- 新建：`veloxa/core/image/image_decoder.h/.cc`
- 修改：`veloxa/core/CMakeLists.txt`（find_package + 新源文件）
- 测试：`tests/core/image/image_decoder_test.cc`
- 测试资源：`tests/assets/test_4x4.png`、`tests/assets/test_4x4.jpg`（小测试图片，代码生成）

ImageDecoder 接口：

```cpp
namespace vx::image {

StatusOr<gfx::Image> DecodeFromFile(StringView path);
StatusOr<gfx::Image> DecodeFromMemory(const u8* data, usize len);

}  // namespace vx::image
```

实现：
- 检查 magic bytes（PNG: `\x89PNG`，JPEG: `\xFF\xD8\xFF`）
- PNG：libpng `png_read_image` → RGBA 行 → Image 像素
- JPEG：libjpeg `jpeg_read_scanlines` → RGB → RGBA（alpha=255）
- 错误：`Status("unsupported format")` / `Status("decode failed")`

测试：
- 代码生成最小 PNG/JPEG 测试文件（4×4 纯色），解码后验证尺寸和像素值
- 无效文件 → 错误
- 空路径 → 错误

CMake：

```cmake
find_package(PNG REQUIRED)
find_package(JPEG REQUIRED)
# 在 vx_core target 追加源文件和链接
target_sources(vx_core PRIVATE image/image_decoder.cc)
target_link_libraries(vx_core PRIVATE PNG::PNG JPEG::JPEG)
```

**提交** `feat(image): implement PNG/JPEG decoder with libpng and libjpeg-turbo`

### Phase 3.3：ImageCache [TDD]

**文件：**
- 新建：`veloxa/core/image/image_cache.h/.cc`
- 测试：`tests/core/image/image_cache_test.cc`

```cpp
class ImageCache {
 public:
  StatusOr<ImageHandle> Load(StringView path);
  const gfx::Image* Get(ImageHandle handle) const;
  void Clear();
  usize size() const;

 private:
  HashMap<String, ImageHandle> path_to_handle_;
  Vector<gfx::Image> images_;  // 1-based indexing
};
```

测试：加载同路径两次返回相同 handle、Get 返回有效 Image、不存在路径返回错误。

**提交** `feat(image): implement ImageCache with path deduplication`

### Phase 3.4：Layout kReplaced + BuildTree 集成 [TDD]

**文件：**
- 修改：`veloxa/core/layout/layout_box.h`（+kReplaced, +image_handle）
- 修改：`veloxa/core/layout/layout_utils.h`（LayoutContext +image_cache）
- 修改：`veloxa/core/layout/layout_engine.cc`（BuildTree 处理 img, LayoutChild 处理 kReplaced）
- 测试：`tests/core/layout/tree_builder_test.cc`（追加）

LayoutType 变更：

```cpp
enum class LayoutType : u8 {
  kBlock,
  kInline,
  kFlex,
  kText,
  kReplaced,  // 新增
};
```

LayoutBox 变更：`u32 image_handle = 0;`

LayoutContext 变更：`image::ImageCache* image_cache = nullptr;`

BuildTree 变更（伪码）：
```
if (element->tag_id() == TagId::kImg && ctx.image_cache) {
  auto src = element->GetAttribute("src");
  if (src) {
    auto result = ctx.image_cache->Load(*src);
    if (result.ok()) {
      box->type = LayoutType::kReplaced;
      box->image_handle = result.value();
      auto* img = ctx.image_cache->Get(box->image_handle);
      // intrinsic dimensions
      box->content_width = img->width();
      box->content_height = img->height();
    }
  }
}
```

LayoutChild 变更：`kReplaced` 按 CSS width/height 覆盖 intrinsic 尺寸，保留宽高比。

**提交** `feat(layout): add kReplaced layout type and img element handling in BuildTree`

### Phase 3.5：Canvas::DrawImage + PaintCommand + Renderer [TDD]

**文件：**
- 修改：`veloxa/graphics/canvas.h`（+DrawImage）
- 修改：`veloxa/graphics/software/software_canvas.h/.cc`（+DrawImage 实现）
- 修改：`veloxa/core/render/paint_command.h`（+kDrawImage）
- 修改：`veloxa/core/render/renderer.h/.cc`（Record/Replay 支持图片）
- 测试：`tests/core/render/integration_test.cc`（追加图片渲染测试）

Canvas 新增：

```cpp
virtual void DrawImage(const gfx::Image& image,
                       const Rect& src_rect, const Rect& dst_rect) = 0;
```

SoftwareCanvas::DrawImage：
- 遍历 dst_rect 每个像素
- 映射到 src_rect 对应坐标（最近邻）
- SrcOver alpha 混合

PaintCommand 新增：
- `kDrawImage`：使用 `image_handle` 字段 + `rect`(dst) + `src_rect`(需新增字段或用 param 编码)
- 简化 MVP：src_rect = 整张图片（0,0,w,h），只需 image_handle + dst rect

Renderer：
- `Record`：需要 `ImageCache*` 参数（从 LayoutBox.image_handle 查 Image 尺寸生成 src_rect）
- `Replay`：需要 `ImageCache*` 参数（从 handle 获取 Image 数据传给 Canvas）
- Record/Replay 签名变更为接受 `RenderContext` 聚合参数

**提交** `feat(render): add DrawImage to Canvas and wire image rendering pipeline`

### Phase 3.6：Application 集成 + 端到端测试 [TDD]

**文件：**
- 修改：`veloxa/core/application.h/.cc`（+ImageCache，传入 LayoutContext/RenderContext）
- 测试：`tests/core/application_integration_test.cc`（追加图片端到端测试）

测试：HTML `<img src="tests/assets/test_4x4.png">` → Update → 验证像素包含图片内容。

**提交** `feat(app): integrate ImageCache into Application pipeline`

---

## Iteration 4：QuickJS DOM 绑定

### Phase 4.1：CMake + Element inline_declarations [TDD]

**文件：**
- 修改：`veloxa/script/CMakeLists.txt`（链接 vx_core）
- 修改：`veloxa/core/dom/element.h`（+inline_declarations_）
- 测试：`tests/core/dom/element_test.cc`（追加）

Element 变更：

```cpp
// element.h 新增
const Vector<css::Declaration>* inline_declarations() const {
  return inline_decls_.empty() ? nullptr : &inline_decls_;
}
void SetInlineDeclaration(css::PropertyId prop, css::CssValue value);
void ClearInlineDeclarations();

private:
  Vector<css::Declaration> inline_decls_;
```

CMake：`target_link_libraries(vx_script PRIVATE vx_core)`

**提交** `feat(dom): add inline_declarations to Element; link vx_script to vx_core`

### Phase 4.2：BuildTree inline style 集成（修复技术债 #28）[TDD]

**文件：**
- 修改：`veloxa/core/layout/layout_engine.cc`（BuildTree 传 inline_decls）
- 测试：`tests/core/layout/tree_builder_test.cc`（追加 inline style 测试）

变更：BuildTree 中 `StyleResolver::Resolve(el, ..., em)` → `Resolve(el, ..., em, el->inline_declarations())`

测试：手动设置 Element inline_declarations → BuildTree → 验证 ComputedStyle 反映 inline 值。

**提交** `feat(layout): pass inline_declarations to StyleResolver in BuildTree (fixes #28)`

### Phase 4.3：DomBindings — document.getElementById [TDD]

**文件：**
- 新建：`veloxa/script/dom_bindings.h/.cc`
- 测试：`tests/script/dom_bindings_test.cc`

```cpp
// veloxa/script/dom_bindings.h
#ifndef VELOXA_SCRIPT_DOM_BINDINGS_H_
#define VELOXA_SCRIPT_DOM_BINDINGS_H_

#include "veloxa/core/dom/document.h"
#include "veloxa/core/event/event_manager.h"

struct JSContext;

namespace vx::script {

class DomBindings {
 public:
  void Bind(JSContext* ctx, dom::Document* doc,
            event::EventManager* em = nullptr);
  void Unbind();

 private:
  JSContext* ctx_ = nullptr;
  dom::Document* doc_ = nullptr;
  event::EventManager* em_ = nullptr;
};

}  // namespace vx::script
#endif
```

测试：

```cpp
TEST(DomBindingsTest, GetElementById) {
  QuickjsEngine engine;
  ASSERT_TRUE(engine.Init().ok());
  dom::Document doc;
  auto* el = doc.CreateElement(dom::TagId::kDiv);
  el->set_id(InternedString("mybox"));
  doc.AppendChild(el);

  DomBindings bindings;
  bindings.Bind(engine.context(), &doc);

  auto result = engine.EvalGlobal(
      "document.getElementById('mybox') !== null ? 'found' : 'null'",
      "test.js");
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value(), "found");

  bindings.Unbind();
  engine.Shutdown();
}
```

实现：
- `Bind()`：在 JSContext 全局创建 `document` 对象
- `document.getElementById`：JS_NewCFunction，接收 string 参数，遍历 DOM 查找匹配 id 的 Element，返回 JS wrapper 或 null

QuickjsEngine 扩展：暴露 `JSContext* context() const { return ctx_; }`

**提交** `feat(script): implement DomBindings with document.getElementById`

### Phase 4.4：Element 属性访问 [TDD]

**文件：**
- 修改：`veloxa/script/dom_bindings.cc`
- 测试：`tests/script/dom_bindings_test.cc`（追加）

实现 Element JS 原型：
- `tagName`（getter）
- `id`（getter）
- `getAttribute(name)` / `setAttribute(name, value)`
- `textContent`（getter/setter）

测试：

```cpp
TEST(DomBindingsTest, TextContentReadWrite) {
  // 构建 doc + <div id="msg">Hello</div>
  auto result = engine.EvalGlobal(
      "var el = document.getElementById('msg');"
      "var old = el.textContent;"
      "el.textContent = 'World';"
      "old",
      "test.js");
  EXPECT_EQ(result.value(), "Hello");
  // 验证 DOM 实际被修改
  auto* text = static_cast<dom::Text*>(el->first_child());
  EXPECT_EQ(text->data(), "World");
}
```

**提交** `feat(script): implement Element properties (tagName, id, textContent, get/setAttribute)`

### Phase 4.5：style proxy [TDD]

**文件：**
- 修改：`veloxa/script/dom_bindings.cc`
- 测试：`tests/script/dom_bindings_test.cc`（追加）

实现 `el.style` proxy 对象：
- Setter：解析 CSS 属性名（camelCase → kebab-case）→ 解析值 → `element->SetInlineDeclaration()`
- Getter：返回 inline_declarations 中的值（或空字符串）

支持的 style 属性（MVP）：backgroundColor, color, width, height, display, opacity

测试：JS `el.style.backgroundColor = "red"` → 验证 `el->inline_declarations()` 包含 background-color Declaration。

**提交** `feat(script): implement el.style proxy with inline CSS injection`

### Phase 4.6：addEventListener / removeEventListener [TDD]

**文件：**
- 修改：`veloxa/script/dom_bindings.cc`
- 测试：`tests/script/dom_bindings_test.cc`（追加）

实现：
- `addEventListener(type, fn)`：包装 JSValue 回调为 C++ lambda → `EventDispatcher::AddEventListener`
- `removeEventListener(type, fn)`：查找匹配回调 → `RemoveEventListener`
- 事件对象：传递 `{type, target}` 给 JS 回调

测试：

```cpp
TEST(DomBindingsTest, AddEventListener) {
  // 构建 doc + element + bindings
  engine.EvalGlobal(
      "var clicked = false;"
      "document.getElementById('btn').addEventListener('click', function() {"
      "  clicked = true;"
      "});",
      "test.js");

  // 模拟 click（通过 EventManager::HandleInput）
  event::InputEvent click{event::EventType::kPointerDown, ...};
  em.HandleInput(click, layout_root);

  auto result = engine.EvalGlobal("clicked ? 'yes' : 'no'", "check.js");
  EXPECT_EQ(result.value(), "yes");
}
```

**提交** `feat(script): implement addEventListener/removeEventListener with JS callback bridge`

### Phase 4.7：QuickjsEngine + Application + C API 集成 [TDD]

**文件：**
- 修改：`veloxa/script/quickjs_engine.h/.cc`（+BindDOM/UnbindDOM/context accessor）
- 修改：`veloxa/core/application.h/.cc`（+QuickjsEngine +DomBindings）
- 修改：`veloxa/api/veloxa_api.h/.cc`（+vx_view_load_script）
- 测试：`tests/api/api_integration_test.cc`（追加）

C API：

```c
VxResult vx_view_load_script(VxView* view, const char* source, uint32_t len);
```

Application 变更：
- 新增 `std::unique_ptr<script::QuickjsEngine> script_engine_`
- 新增 `std::unique_ptr<script::DomBindings> dom_bindings_`
- `LoadHTML` 后：若 script_engine_ 已初始化 → BindDOM
- `LoadScript()`：若引擎未初始化 → Init + BindDOM → EvalGlobal

端到端测试：通过 C API 加载 HTML + CSS + JS → Update → 验证 JS 修改的 DOM 反映在渲染结果中。

**提交** `feat(api): add vx_view_load_script and integrate QuickJS DOM bindings into Application`

---

## 子代理分组策略

| 迭代 | Phase 分组 | 原因 |
|------|-----------|------|
| 1 | 1.1 + 1.2 合并为一个子代理 | 共享 paint_command.h + renderer.cc |
| 2 | 2.1 单独 → 2.2 + 2.3 可并行 → 2.4 依赖 2.2 → 2.5 依赖 2.2+2.3 → 2.6 依赖全部 | 2.2 和 2.3 无共享 .cc |
| 3 | 3.1 + 3.2 合并 → 3.3 依赖 3.2 → 3.4 + 3.5 可尝试并行 → 3.6 依赖全部 | 3.4(ImageCache) 和 3.5(Layout) 无共享 .cc |
| 4 | 4.1 + 4.2 合并 → 4.3 → 4.4 → 4.5 → 4.6 → 4.7 严格串行 | dom_bindings.cc 被多个 Phase 修改 |

## 关键约束备忘

- `activeContext.md` P1 项已融入计划：
  - FetchContent C 编译告警 → 不适用（FreeType/HarfBuzz 用系统库 find_package）
  - 集成测试禁用 inline style → Phase 4.2 修复后集成测试可用 inline style
  - 集成测试优先用 DisplayList 检查 → 所有渲染测试遵循
  - CSS 颜色测试用 CssColorToGfx → 所有颜色断言遵循
  - LayoutBox 坐标语义 → 子代理 prompt 必须包含 x/y = content origin 定义
  - 并行子代理条件 → 已按 Phase 标注可并行性

## 预估

| 迭代 | 预估时间 |
|------|---------|
| 1. border-radius | 30 分钟 |
| 2. FreeType+HarfBuzz | 2-3 小时 |
| 3. 图片支持 | 2-3 小时 |
| 4. DOM 绑定 | 3-4 小时 |
| **总计** | **~8-10 小时** |
