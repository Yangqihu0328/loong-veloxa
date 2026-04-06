# 实现计划：渲染管线（Render Pipeline）

**任务 ID：** TASK-20260405-07
**创建日期：** 2026-04-05
**设计文档：** `memory-bank/creative/creative-render-pipeline.md`

## 文件清单

### 新增文件

| 文件 | 职责 |
|------|------|
| `veloxa/core/render/paint_command.h` | PaintCommand 结构体 + DisplayList 类型别名 + 工厂函数 |
| `veloxa/core/render/render_utils.h` | CssColorToGfx 颜色转换（header-only） |
| `veloxa/core/render/renderer.h` | Record / Replay / Paint 函数声明 |
| `veloxa/core/render/renderer.cc` | RecordBox 递归遍历 + z-index 排序 + Replay + Paint |
| `tests/core/render/paint_command_test.cc` | PaintCommand 工厂、CssColorToGfx 单元测试 |
| `tests/core/render/renderer_test.cc` | Record + Replay 单元测试 |
| `tests/core/render/integration_test.cc` | HTML→DOM→CSS→Layout→Render→PPM 全管线测试 |

### 修改文件

| 文件 | 修改内容 |
|------|---------|
| `veloxa/graphics/canvas.h` | 新增 `DrawText` 纯虚函数 |
| `veloxa/graphics/software/software_canvas.h` | 新增 `DrawText` override 声明 |
| `veloxa/graphics/software/software_canvas.cc` | 实现 `DrawText`（逐字符 FillRect 位图字体存根） |
| `veloxa/core/CMakeLists.txt` | 新增 `render/renderer.cc`，新增 `vx_graphics` 依赖 |
| `tests/CMakeLists.txt` | 新增 3 个测试目标 |

## 依赖链变更

```
变更前: vx_core → vx_foundation
变更后: vx_core → vx_foundation + vx_graphics
         vx_graphics → vx_foundation + vx_platform (不变)
```

无循环依赖。Core Engine 依赖 Graphics HAL 符合分层架构。

## Phase 分解

### Phase 1: 基础设施（headers + Canvas::DrawText + CMake + stubs）

**产出文件：** paint_command.h, render_utils.h, renderer.h, renderer.cc(stub), canvas.h(修改), software_canvas.h/cc(修改), CMakeLists.txt(两处), paint_command_test.cc

**执行方式：** 直接实现（无子代理）

#### 1.1 paint_command.h

```cpp
#ifndef VELOXA_CORE_RENDER_PAINT_COMMAND_H_
#define VELOXA_CORE_RENDER_PAINT_COMMAND_H_

#include "veloxa/foundation/base/types.h"
#include "veloxa/foundation/containers/vector.h"
#include "veloxa/foundation/strings/string_view.h"
#include "veloxa/graphics/types.h"

namespace vx::render {

struct PaintCommand {
  enum class Type : u8 {
    kFillRect,
    kDrawText,
    kPushClipRect,
    kPopClip,
    kPushLayer,
    kPopLayer,
  };

  Type type;
  gfx::Rect rect;
  gfx::Color color;
  f32 param = 0;
  StringView text;

  static PaintCommand FillRect(const gfx::Rect& r, gfx::Color c) {
    return {Type::kFillRect, r, c, 0, {}};
  }

  static PaintCommand DrawText(StringView t, const gfx::Rect& r,
                               f32 font_size, gfx::Color c) {
    return {Type::kDrawText, r, c, font_size, t};
  }

  static PaintCommand PushClipRect(const gfx::Rect& r) {
    return {Type::kPushClipRect, r, {}, 0, {}};
  }

  static PaintCommand PopClip() {
    return {Type::kPopClip, {}, {}, 0, {}};
  }

  static PaintCommand PushLayer(const gfx::Rect& r, f32 opacity) {
    return {Type::kPushLayer, r, {}, opacity, {}};
  }

  static PaintCommand PopLayer() {
    return {Type::kPopLayer, {}, {}, 0, {}};
  }
};

using DisplayList = Vector<PaintCommand>;

}  // namespace vx::render

#endif  // VELOXA_CORE_RENDER_PAINT_COMMAND_H_
```

#### 1.2 render_utils.h

```cpp
#ifndef VELOXA_CORE_RENDER_RENDER_UTILS_H_
#define VELOXA_CORE_RENDER_RENDER_UTILS_H_

#include "veloxa/foundation/base/types.h"
#include "veloxa/graphics/types.h"

namespace vx::render {

// CSS ComputedStyle 使用 RRGGBBAA 格式：R[24:31] G[16:23] B[8:15] A[0:7]
// gfx::Color 使用 r,g,b,a 字节成员
inline gfx::Color CssColorToGfx(u32 css_color) {
  return gfx::Color::FromRGBA(
      static_cast<u8>((css_color >> 24) & 0xFF),
      static_cast<u8>((css_color >> 16) & 0xFF),
      static_cast<u8>((css_color >> 8) & 0xFF),
      static_cast<u8>(css_color & 0xFF));
}

}  // namespace vx::render

#endif  // VELOXA_CORE_RENDER_RENDER_UTILS_H_
```

#### 1.3 renderer.h

```cpp
#ifndef VELOXA_CORE_RENDER_RENDERER_H_
#define VELOXA_CORE_RENDER_RENDERER_H_

#include "veloxa/core/layout/layout_box.h"
#include "veloxa/core/render/paint_command.h"
#include "veloxa/graphics/canvas.h"

namespace vx::render {

DisplayList Record(layout::LayoutBox* root);

void Replay(const DisplayList& list, gfx::Canvas* canvas);

void Paint(layout::LayoutBox* root, gfx::Canvas* canvas);

}  // namespace vx::render

#endif  // VELOXA_CORE_RENDER_RENDERER_H_
```

#### 1.4 renderer.cc（存根）

```cpp
#include "veloxa/core/render/renderer.h"

namespace vx::render {

DisplayList Record(layout::LayoutBox* /*root*/) { return {}; }
void Replay(const DisplayList& /*list*/, gfx::Canvas* /*canvas*/) {}
void Paint(layout::LayoutBox* /*root*/, gfx::Canvas* /*canvas*/) {}

}  // namespace vx::render
```

#### 1.5 Canvas::DrawText

canvas.h 新增（在 `StrokeLine` 之后、`PushClipRect` 之前）：

```cpp
virtual void DrawText(StringView text, const Rect& bounds,
                      f32 font_size, const Brush& brush) = 0;
```

需要新增 include：`#include "veloxa/foundation/strings/string_view.h"`

#### 1.6 SoftwareCanvas::DrawText

software_canvas.h 新增 override 声明。
software_canvas.cc 实现：逐字符绘制 FillRect（字宽 = 0.6 × font_size，字高 = font_size，空格跳过，超出 bounds 停止）。

#### 1.7 CMakeLists.txt

veloxa/core/CMakeLists.txt：
- 新增 `render/renderer.cc` 到 source list
- 新增 `target_link_libraries(vx_core PUBLIC vx_graphics)` — 注意这替换了原有的 `target_link_libraries(vx_core PUBLIC vx_foundation)`，因为 vx_graphics 已传递依赖 vx_foundation

tests/CMakeLists.txt：
- 新增 `render_paint_command_test`（链接 vx_core）
- 新增 `render_test`（链接 vx_core vx_graphics vx_platform）
- 新增 `render_integration_test`（链接 vx_core vx_graphics vx_platform）

#### 1.8 paint_command_test.cc

测试用例（~10 tests）：

| # | 测试名 | 验证 |
|---|--------|------|
| 1 | FillRectFactory | 工厂函数设置 type=kFillRect, rect, color |
| 2 | DrawTextFactory | 工厂函数设置 type=kDrawText, rect, color, param=font_size, text |
| 3 | PushClipRectFactory | type=kPushClipRect, rect |
| 4 | PopClipFactory | type=kPopClip |
| 5 | PushLayerFactory | type=kPushLayer, rect, param=opacity |
| 6 | PopLayerFactory | type=kPopLayer |
| 7 | CssColorToGfx_Red | `0xFF0000FF` → Color{255,0,0,255} |
| 8 | CssColorToGfx_Green | `0x00FF00FF` → Color{0,255,0,255} |
| 9 | CssColorToGfx_Black | `0x000000FF` → Color{0,0,0,255} |
| 10 | CssColorToGfx_Transparent | `0x00000000` → Color{0,0,0,0} |

---

### Phase 2: Record + Replay 完整实现

**产出文件：** renderer.cc（完整实现）, renderer_test.cc

**执行方式：** 子代理（复杂算法 + 全面测试）

#### 2.1 Record 算法

核心函数 `RecordBox(LayoutBox* box, DisplayList& list)`：

```
RecordBox(box, list):
  style = box->style (如果 nullptr → 使用默认值)

  // 1. Skip checks
  if style->visibility == kHidden → return
  // display:none 已在 BuildTree 过滤，此处无需检查

  // 2. 计算 border box 坐标
  bx = box->x - box->border[kLeft]
  by = box->y - box->border[kTop]
  bbw = box->border_box_width()
  bbh = box->border_box_height()
  border_box = Rect(bx, by, bbw, bbh)

  // 3. Opacity 层
  if style->opacity < 1.0 → push PushLayer(border_box, opacity)

  // 4. 背景色（alpha > 0 才绘制）
  bg = CssColorToGfx(style->background_color)
  if bg.a > 0 → push FillRect(border_box, bg)

  // 5. 边框（每边独立：style != none && width > 0）
  PaintBorders(box, list)  // 4 条 FillRect

  // 6. Overflow 裁剪（在子元素之前）
  if style->overflow == kHidden:
    content_rect = Rect(box->x, box->y, box->content_width, box->content_height)
    push PushClipRect(content_rect)

  // 7. 文本（kText 节点）
  if box->type == kText && box->text_node && !box->text_node->data().empty():
    text_color = CssColorToGfx(style->color)
    font_size = style->font_size 的 px 值（如果 kPx 取 value，否则默认 16）
    push DrawText(text, content_rect, font_size, text_color)

  // 8. 子元素 — z-index stable_sort
  children = collect to SmallVector<LayoutBox*, 16>
  std::stable_sort(children, [](a, b) { return a.z_index < b.z_index })
  for child in children:
    RecordBox(child, list)

  // 9. 出栈
  if overflow == hidden → push PopClip
  if opacity < 1.0 → push PopLayer
```

#### 2.2 边框矩形计算

```
PaintBorders(box, list):
  bx = box->x - box->border[kLeft]
  by = box->y - box->border[kTop]
  bbw = box->border_box_width()
  bbh = box->border_box_height()
  bw = box->border  // [top, right, bottom, left]

  // Top
  if style->border_style[0] != kNone && bw[0] > 0:
    push FillRect(Rect(bx, by, bbw, bw[0]), CssColorToGfx(border_color[0]))
  // Bottom
  if style->border_style[2] != kNone && bw[2] > 0:
    push FillRect(Rect(bx, by + bbh - bw[2], bbw, bw[2]), CssColorToGfx(border_color[2]))
  // Left (排除 top/bottom 区域，避免角落重叠)
  if style->border_style[3] != kNone && bw[3] > 0:
    push FillRect(Rect(bx, by + bw[0], bw[3], bbh - bw[0] - bw[2]), CssColorToGfx(border_color[3]))
  // Right
  if style->border_style[1] != kNone && bw[1] > 0:
    push FillRect(Rect(bx + bbw - bw[1], by + bw[0], bw[1], bbh - bw[0] - bw[2]), CssColorToGfx(border_color[1]))
```

#### 2.3 Replay 实现

```cpp
void Replay(const DisplayList& list, gfx::Canvas* canvas) {
  for (const auto& cmd : list) {
    switch (cmd.type) {
      case PaintCommand::Type::kFillRect:
        canvas->FillRect(cmd.rect, gfx::Brush::Solid(cmd.color));
        break;
      case PaintCommand::Type::kDrawText:
        canvas->DrawText(cmd.text, cmd.rect, cmd.param,
                         gfx::Brush::Solid(cmd.color));
        break;
      case PaintCommand::Type::kPushClipRect:
        canvas->PushClipRect(cmd.rect);
        break;
      case PaintCommand::Type::kPopClip:
        canvas->PopClip();
        break;
      case PaintCommand::Type::kPushLayer:
        canvas->PushLayer(cmd.rect, cmd.param);
        break;
      case PaintCommand::Type::kPopLayer:
        canvas->PopLayer();
        break;
    }
  }
}
```

#### 2.4 renderer_test.cc 测试用例（~18 tests）

**Record 测试（使用手动构建 LayoutBox + ComputedStyle）：**

| # | 测试名 | 验证 |
|---|--------|------|
| 1 | RecordNullRoot | nullptr → 空 DisplayList |
| 2 | RecordSingleBoxNoBg | 透明背景 → 无 FillRect |
| 3 | RecordSingleBoxWithBg | 红色背景 → 1 个 FillRect(red) |
| 4 | RecordBordersAllSides | 4 边 solid border → 4 个 FillRect |
| 5 | RecordBorderNoneSkipped | border_style=none → 0 个 border FillRect |
| 6 | RecordBorderPartialSides | 仅 top/bottom solid → 2 个 border FillRect |
| 7 | RecordTextNode | kText 节点 → 1 个 DrawText 命令 |
| 8 | RecordEmptyText | 空文本 → 无 DrawText |
| 9 | RecordVisibilityHidden | visibility:hidden → 空 DisplayList |
| 10 | RecordOpacityLayer | opacity=0.5 → PushLayer + content + PopLayer |
| 11 | RecordOverflowHidden | overflow:hidden → PushClipRect + children + PopClip |
| 12 | RecordZIndexSorting | 子元素 z-index [2,0,1] → 按 [0,1,2] 排序绘制 |
| 13 | RecordZIndexStableOrder | 相同 z-index → DOM 源序 |
| 14 | RecordNestedBoxes | 嵌套 block → 先父背景、再子背景 |

**Replay 测试（使用 SoftwareCanvas + 像素验证）：**

| # | 测试名 | 验证 |
|---|--------|------|
| 15 | ReplayEmptyList | 空列表 → 画布无变化 |
| 16 | ReplayFillRect | FillRect 命令 → 指定区域像素为目标颜色 |
| 17 | ReplayDrawText | DrawText 命令 → 文本区域有像素 |
| 18 | ReplayClipRect | PushClipRect + FillRect → 只在裁剪区内有像素 |

---

### Phase 3: 全管线集成测试

**产出文件：** tests/core/render/integration_test.cc

**执行方式：** 直接实现

**测试模式：** 使用真实 HTML/CSS 解析器 → DOM → CSS → Layout → Record → Replay → 像素验证

| # | 测试名 | HTML 输入 | 验证 |
|---|--------|-----------|------|
| 1 | SimpleRedDiv | `<div style="width:10px;height:10px;background:red">` | 指定区域像素为红色 |
| 2 | DivWithBorder | `<div style="width:10px;height:10px;border:2px solid blue">` | 边框区域为蓝色 |
| 3 | TextInParagraph | `<p style="color:red;font-size:10px">Hi</p>` | 文本区域有非零像素 |
| 4 | OpacityBlending | `<div style="background:red;opacity:0.5">` | 像素颜色为混合值 |
| 5 | VisibilityHidden | `<div style="visibility:hidden;background:red">` | 区域无红色像素 |
| 6 | NestedLayout | header + content + footer 三层 block | 各区域正确颜色 |
| 7 | ZIndexOrdering | 两个 absolute 元素，后者 z-index 更低 | 高 z-index 元素像素在前 |
| 8 | DisplayList_Inspect | 简单页面 → 检查 DisplayList 内容而非像素 | 命令类型和数量正确 |

## 子代理策略

### 分组依据

| 分组 | Phase | 产出文件 | 是否共享文件 |
|------|-------|---------|------------|
| 直接实现 | Phase 1 | headers + Canvas::DrawText + CMake + stubs + paint_command_test | 修改 canvas.h, software_canvas.*, CMakeLists.txt |
| 子代理 A | Phase 2 | renderer.cc（完整实现）+ renderer_test.cc | 仅修改 renderer.cc（覆盖 Phase 1 的 stub） |
| 直接实现 | Phase 3 | integration_test.cc | 无共享 |

- Phase 1 和 Phase 2 无文件冲突（Phase 1 创建 stub, Phase 2 覆盖 stub）
- Phase 2 的子代理 prompt 需包含 paint_command.h、render_utils.h、renderer.h 的完整内容 + LayoutBox/ComputedStyle API 签名
- Phase 3 在 Phase 1+2 完成后执行

## 边界输入清单

| Phase | 场景 | 预期 |
|-------|------|------|
| Phase 1 | Canvas::DrawText 空文本 | 不绘制任何矩形 |
| Phase 1 | Canvas::DrawText 文本超出 bounds | 在边界处停止 |
| Phase 2 | Record nullptr root | 返回空 DisplayList |
| Phase 2 | LayoutBox 无 style（nullptr） | 使用默认 ComputedStyle 值 |
| Phase 2 | kText 节点无 text_node 指针 | 跳过 DrawText |
| Phase 2 | opacity == 0 | 仍 PushLayer（合法透明） |
| Phase 2 | border_width > 0 但 border_style == none | 跳过该边 |
| Phase 2 | 所有子元素 z-index 相同 | stable_sort 保持 DOM 序 |
| Phase 3 | 全透明页面 | 所有像素为 Canvas::Clear 颜色 |
