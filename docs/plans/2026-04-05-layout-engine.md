# Layout Engine 实现计划 — TASK-20260405-06

**复杂度级别：** Level 4
**技术栈：** C++17, CMake, Google Test, Google C++ Style Guide
**设计规格：** `docs/specs/2026-04-05-layout-engine-design.md`

---

## 文件结构

### 新增文件

| 文件 | 职责 | Phase |
|------|------|-------|
| `veloxa/core/layout/layout_box.h` | LayoutType, LayoutBox, LayoutContext | 1 |
| `veloxa/core/layout/text_shaper.h` | TextShaper 纯虚接口 + SimpleTextShaper | 1 |
| `veloxa/core/layout/text_shaper.cc` | SimpleTextShaper 实现 | 1 |
| `veloxa/core/layout/layout_utils.h` | ResolveLength, ResolveBoxModel 声明 | 1 |
| `veloxa/core/layout/layout_utils.cc` | 长度解析 + 盒模型计算实现 | 1 |
| `veloxa/core/layout/layout_engine.h` | LayoutEngine 类声明 | 2 |
| `veloxa/core/layout/layout_engine.cc` | 树构建 + Block 布局 + Inline 布局 + 定位 | 2,3,5 |
| `veloxa/core/layout/flex_layout.h` | LayoutFlex 声明 | 4 |
| `veloxa/core/layout/flex_layout.cc` | Flex 布局算法 | 4 |
| `tests/core/layout/layout_box_test.cc` | LayoutBox 数据结构测试 | 1 |
| `tests/core/layout/layout_utils_test.cc` | 长度解析 + 盒模型测试 | 1 |
| `tests/core/layout/tree_builder_test.cc` | 布局树构建测试 | 2 |
| `tests/core/layout/block_layout_test.cc` | Block 布局测试 | 3 |
| `tests/core/layout/flex_layout_test.cc` | Flex 布局测试 | 4 |
| `tests/core/layout/inline_position_test.cc` | Inline 布局 + 定位测试 | 5 |
| `tests/core/layout/integration_test.cc` | 全管线集成测试 | 6 |

### 修改文件

| 文件 | 修改内容 | Phase |
|------|---------|-------|
| `veloxa/core/CMakeLists.txt` | [共享文件] 新增 layout/ 源文件 | 1 |
| `tests/CMakeLists.txt` | [共享文件] 新增 layout 测试目标 | 1 |

---

## Phase 1：数据结构 + 工具函数 + CMake

**预估时间：** 15 分钟
**依赖：** 无

### 1.1 layout_box.h

```cpp
#ifndef VELOXA_CORE_LAYOUT_LAYOUT_BOX_H_
#define VELOXA_CORE_LAYOUT_LAYOUT_BOX_H_

#include "veloxa/core/css/computed_style.h"
#include "veloxa/core/dom/element.h"
#include "veloxa/core/dom/text.h"
#include "veloxa/foundation/base/types.h"

namespace vx::layout {

enum class LayoutType : u8 {
  kBlock,
  kInline,
  kFlex,
  kText,
};

struct LayoutBox {
  LayoutType type = LayoutType::kBlock;
  dom::Element* element = nullptr;
  dom::Text* text_node = nullptr;
  const css::ComputedStyle* style = nullptr;

  LayoutBox* parent = nullptr;
  LayoutBox* first_child = nullptr;
  LayoutBox* last_child = nullptr;
  LayoutBox* next_sibling = nullptr;
  LayoutBox* prev_sibling = nullptr;

  f32 x = 0, y = 0;
  f32 content_width = 0, content_height = 0;
  f32 padding[4] = {};    // top, right, bottom, left
  f32 border[4] = {};
  f32 margin[4] = {};

  void AppendChild(LayoutBox* child);

  f32 padding_box_width() const;
  f32 padding_box_height() const;
  f32 border_box_width() const;
  f32 border_box_height() const;
  f32 margin_box_width() const;
  f32 margin_box_height() const;

  u32 child_count() const;
};

}  // namespace vx::layout

#endif  // VELOXA_CORE_LAYOUT_LAYOUT_BOX_H_
```

### 1.2 text_shaper.h

```cpp
#ifndef VELOXA_CORE_LAYOUT_TEXT_SHAPER_H_
#define VELOXA_CORE_LAYOUT_TEXT_SHAPER_H_

#include "veloxa/foundation/base/types.h"
#include "veloxa/foundation/strings/string_view.h"

namespace vx::layout {

struct TextMetrics {
  f32 width;
  f32 height;
  f32 baseline;
};

class TextShaper {
 public:
  virtual ~TextShaper() = default;
  virtual TextMetrics Measure(StringView text, f32 font_size,
                              u16 font_weight) = 0;
};

class SimpleTextShaper : public TextShaper {
 public:
  TextMetrics Measure(StringView text, f32 font_size,
                      u16 font_weight) override;
};

}  // namespace vx::layout

#endif  // VELOXA_CORE_LAYOUT_TEXT_SHAPER_H_
```

text_shaper.cc 实现：
- width = text.size() × font_size × 0.6f
- height = font_size × 1.2f
- baseline = font_size

### 1.3 layout_utils.h

```cpp
#ifndef VELOXA_CORE_LAYOUT_LAYOUT_UTILS_H_
#define VELOXA_CORE_LAYOUT_LAYOUT_UTILS_H_

#include "veloxa/core/css/computed_style.h"
#include "veloxa/core/css/css_value.h"
#include "veloxa/foundation/base/types.h"

namespace vx::layout {

struct LayoutContext {
  class TextShaper* text_shaper = nullptr;
  const Vector<css::Stylesheet>* stylesheets = nullptr;
  f32 viewport_width = 0;
  f32 viewport_height = 0;
  f32 root_font_size = 16.0f;
};

// 解析 LengthValue 为像素值
// containing_size: % 的参考值
// font_size: em 的参考值
// ctx: viewport/rem 的参考值
f32 ResolveLength(const css::LengthValue& len, f32 containing_size,
                  f32 font_size, const LayoutContext& ctx);

// 解析盒模型：填充 padding[4], border[4], margin[4]
// containing_width: margin/padding 百分比的参考值
void ResolveBoxModel(const css::ComputedStyle& style,
                     f32 containing_width, f32 font_size,
                     const LayoutContext& ctx,
                     f32 padding[4], f32 border_widths[4],
                     f32 margins[4]);

}  // namespace vx::layout

#endif  // VELOXA_CORE_LAYOUT_LAYOUT_UTILS_H_
```

ResolveLength 实现：
- kPx → value 直接返回
- kPercent → value / 100 × containing_size
- kEm → value × font_size
- kRem → value × ctx.root_font_size
- kVw → value / 100 × ctx.viewport_width
- kVh → value / 100 × ctx.viewport_height
- kAuto/kNone → 返回 0（调用者需特殊处理 auto）

ResolveBoxModel 实现：
- 解析 padding_top/right/bottom/left
- 解析 border_top_width 等（仅 border_style != none 时有效）
- 解析 margin_top/right/bottom/left

### 1.4 CMake 更新 + 存根文件预创建

veloxa/core/CMakeLists.txt 新增：
```cmake
layout/text_shaper.cc
layout/layout_utils.cc
layout/layout_engine.cc
layout/flex_layout.cc
```

tests/CMakeLists.txt 新增所有测试目标。

预创建空存根 .cc 文件（layout_engine.cc, flex_layout.cc）避免 CMake 配置失败。

### 1.5 测试

layout_box_test.cc：
- AppendChild、child_count
- padding/border/margin_box_width/height 计算
- 默认值全零

layout_utils_test.cc：
- ResolveLength：px, %, em, rem, vw, vh, auto, none
- ResolveBoxModel：标准 content-box、border-box

---

## Phase 2：Layout Tree Builder

**预估时间：** 10 分钟
**依赖：** Phase 1

### 2.1 layout_engine.h

```cpp
#ifndef VELOXA_CORE_LAYOUT_LAYOUT_ENGINE_H_
#define VELOXA_CORE_LAYOUT_LAYOUT_ENGINE_H_

#include "veloxa/core/dom/document.h"
#include "veloxa/core/layout/layout_box.h"
#include "veloxa/core/layout/layout_utils.h"
#include "veloxa/foundation/memory/arena_allocator.h"

namespace vx::layout {

class LayoutEngine {
 public:
  static LayoutBox* Layout(dom::Document* doc,
                           const LayoutContext& ctx);

 private:
  static LayoutBox* BuildTree(dom::Element* element,
                              ArenaAllocator& arena,
                              const LayoutContext& ctx);

  static LayoutBox* CreateBox(LayoutType type, ArenaAllocator& arena);

  static void LayoutBlock(LayoutBox* box, f32 containing_width,
                          const LayoutContext& ctx);

  static void LayoutFlex(LayoutBox* box, f32 containing_width,
                         const LayoutContext& ctx);

  static void LayoutInline(LayoutBox* box, f32 containing_width,
                           const LayoutContext& ctx);

  static void ApplyPositioning(LayoutBox* box);

  static void LayoutChildren(LayoutBox* box, f32 containing_width,
                             const LayoutContext& ctx);
};

}  // namespace vx::layout

#endif  // VELOXA_CORE_LAYOUT_LAYOUT_ENGINE_H_
```

### 2.2 BuildTree 算法

```
BuildTree(element, arena, ctx):
  style = element->computed_style (如果 ComputedStyle 已在 Element 上)
         或通过 StyleResolver 计算
  if style.display == kNone → return nullptr

  box = CreateBox(LayoutTypeFromDisplay(style.display), arena)
  box->element = element
  box->style = &style

  for child in element->children:
    if child.is_text():
      text_box = CreateBox(kText, arena)
      text_box->text_node = static_cast<Text*>(child)
      text_box->style = &style  // 继承父样式
      box->AppendChild(text_box)
    elif child.is_element():
      child_box = BuildTree(static_cast<Element*>(child), arena, ctx)
      if child_box != nullptr:
        box->AppendChild(child_box)

  return box
```

**关键设计：ComputedStyle 的计算与存储**

当前 ComputedStyle 不缓存在 Element 上，需在 BuildTree 中即时计算：
- `LayoutContext` 包含 `stylesheets` 指针
- BuildTree 对每个元素调用 `StyleResolver::Resolve(element, *stylesheets, parent_style)`
- 结果通过 `arena.Allocate(sizeof(ComputedStyle))` + placement new 存储
- LayoutBox::style 指向 arena 分配的 ComputedStyle
- 样式计算遵循继承链：parent_style 来自父 LayoutBox 的 style

### 2.3 测试

tree_builder_test.cc：
- 空 Document → 返回根 LayoutBox
- display:none 跳过
- 嵌套元素正确构建树
- Text 节点生成 kText box
- 混合 block/inline/flex 子元素

---

## Phase 3：Block Layout

**预估时间：** 15 分钟
**依赖：** Phase 1 + 2

### 3.1 LayoutBlock 算法

```
LayoutBlock(box, containing_width, ctx):
  style = box->style
  font_size = ResolveLength(style.font_size, ...) 或默认 16px

  // 1. 解析盒模型
  ResolveBoxModel(style, containing_width, font_size, ctx,
                  box->padding, box->border, box->margin)

  // 2. 解析 width
  if style.width.is_auto():
    box->content_width = containing_width
                         - box->padding[1] - box->padding[3]
                         - box->border[1] - box->border[3]
                         - box->margin[1] - box->margin[3]
  else:
    resolved = ResolveLength(style.width, containing_width, font_size, ctx)
    if style.box_sizing == kBorderBox:
      resolved -= (box->padding[1] + box->padding[3]
                   + box->border[1] + box->border[3])
    box->content_width = resolved

  // 3. 应用 min/max
  ApplyMinMaxWidth(box, containing_width, font_size, ctx)

  // 4. Auto margin 水平居中
  if style.margin_left.is_auto() && style.margin_right.is_auto():
    remaining = containing_width - box->border_box_width()
    box->margin[1] = box->margin[3] = max(0, remaining / 2)

  // 5. 递归布局子元素（垂直堆叠）
  f32 y_offset = 0
  for child in box->children:
    LayoutChildren(child, box->content_width, ctx)
    child->x = child->margin[3]
    child->y = y_offset + child->margin[0]
    y_offset = child->y + child->border_box_height() + child->margin[2]

  // 6. 解析 height
  if style.height.is_auto():
    box->content_height = y_offset
  else:
    resolved = ResolveLength(style.height, ?, font_size, ctx)
    if style.box_sizing == kBorderBox:
      resolved -= (box->padding[0] + box->padding[2]
                   + box->border[0] + box->border[2])
    box->content_height = resolved

  // 7. 应用 min/max height
  ApplyMinMaxHeight(box, font_size, ctx)
```

### 3.2 测试

block_layout_test.cc：
- 单个 block，固定 width/height → 正确像素值
- Auto width 填满 containing block
- Auto height 等于子元素总高度
- 多个 block 子元素垂直堆叠
- Padding/border/margin 正确计算
- box-sizing: border-box
- Auto margin 水平居中
- min-width / max-width 约束
- 嵌套 block

---

## Phase 4：Flex Layout

**预估时间：** 20 分钟（最复杂）
**依赖：** Phase 1 + 2

### 4.1 flex_layout.h

```cpp
namespace vx::layout {
void LayoutFlex(LayoutBox* box, f32 containing_width,
                const LayoutContext& ctx);
}
```

### 4.2 Flex 算法（遵循 CSS Flexbox Level 1 §9）

```
LayoutFlex(box, containing_width, ctx):
  style = box->style
  is_row = (style.flex_direction == kRow || kRowReverse)
  is_reverse = (style.flex_direction == kRowReverse || kColumnReverse)

  // 1. 解析盒模型 + 自身尺寸
  ResolveBoxModel(...)
  ResolveContainerSize(box, containing_width, ...)

  main_size = is_row ? box->content_width : box->content_height
  cross_size = is_row ? box->content_height : box->content_width

  // 2. 计算每个 item 的 flex base size
  for item in box->children:
    ResolveBoxModel(item, ...)
    if item->style->flex_basis is not auto:
      base = ResolveLength(flex_basis, main_size, ...)
    elif is_row and item->style->width is not auto:
      base = ResolveLength(width, main_size, ...)
    else:
      base = item 的内容固有尺寸
    item->hypothetical_main = clamp(base, min, max)

  // 3. 收集 flex lines
  lines = CollectFlexLines(items, main_size, style.flex_wrap)

  // 4. 每行：解析弹性长度
  for line in lines:
    total_hypothetical = sum(item.hypothetical_main for item in line)
    if total_hypothetical < main_size:
      // 有剩余空间 → flex-grow 分配
      DistributeSpace(line, main_size - total_hypothetical, grow=true)
    else:
      // 空间不足 → flex-shrink 收缩
      DistributeSpace(line, total_hypothetical - main_size, grow=false)

  // 5. 主轴对齐（justify-content + gap）
  for line in lines:
    ApplyJustifyContent(line, main_size, style.justify_content,
                        style.gap, is_reverse)

  // 6. 交叉轴对齐
  for line in lines:
    for item in line:
      ApplyAlignItems(item, line_cross_size,
                      style.align_items, item->style->align_self)

  // 7. 写回 x/y 坐标
  WriteFinalPositions(box, lines, is_row, is_reverse)
```

### 4.3 测试

flex_layout_test.cc：
- Row direction：items 水平排列
- Column direction：items 垂直排列
- flex-grow：剩余空间按比例分配
- flex-shrink：不足空间按比例收缩
- flex-basis：固定/auto/百分比
- justify-content：flex-start/end/center/space-between/space-around
- align-items：flex-start/end/center/stretch
- align-self 覆盖 align-items
- flex-wrap：多行换行
- gap：间距
- Row-reverse / column-reverse
- min/max 约束与 flex 交互

---

## Phase 5：Inline 布局 + 定位

**预估时间：** 15 分钟
**依赖：** Phase 1 + 2

### 5.1 Inline Layout

```
LayoutInline(box, containing_width, ctx):
  // 简化：每个 kText 子节点调用 TextShaper 测量
  // 按 containing_width 换行
  f32 x_offset = 0, y_offset = 0
  f32 line_height = 0

  for child in box->children:
    if child->type == kText:
      metrics = ctx.text_shaper->Measure(
          child->text_node->data(), font_size, font_weight)
      child->content_width = metrics.width
      child->content_height = metrics.height

      if x_offset + metrics.width > containing_width && x_offset > 0:
        // 换行
        y_offset += line_height
        x_offset = 0
        line_height = 0

      child->x = x_offset
      child->y = y_offset
      x_offset += metrics.width
      line_height = max(line_height, metrics.height)

  box->content_height = y_offset + line_height

  // text-align 对齐（简化：逐行偏移）
```

### 5.2 Positioning

```
ApplyPositioning(box):
  // 递归处理所有子元素
  for child in box->children:
    if child->style == nullptr: continue
    pos = child->style->position

    if pos == kRelative:
      // 在 flow 位置基础上偏移
      if !child->style->left.is_none():
        child->x += ResolveLength(child->style->left, ...)
      elif !child->style->right.is_none():
        child->x -= ResolveLength(child->style->right, ...)
      // 同理 top/bottom

    elif pos == kAbsolute:
      // 相对最近 positioned 祖先
      containing = FindPositionedAncestor(child)
      child->x = ResolveLength(child->style->left, containing->content_width, ...)
      child->y = ResolveLength(child->style->top, containing->content_height, ...)
      // 解析 width/height，布局子元素

    ApplyPositioning(child)  // 递归
```

### 5.3 测试

inline_position_test.cc：
- Text 节点正确测量宽高
- 文本换行
- text-align（left/center/right）
- Relative positioning 偏移
- Absolute positioning

---

## Phase 6：集成 + CMake 收尾

**预估时间：** 10 分钟
**依赖：** Phase 1-5

### 6.1 LayoutEngine::Layout 入口

```cpp
LayoutBox* LayoutEngine::Layout(dom::Document* doc,
                                const LayoutContext& ctx) {
  LayoutBox* root = BuildTree(doc, doc->arena(), ctx);
  if (!root) return nullptr;

  LayoutBlock(root, ctx.viewport_width, ctx);
  ApplyPositioning(root);
  return root;
}
```

### 6.2 集成测试

integration_test.cc：
- HTML → DOM → CSS → Layout 全管线
- 典型 HMI 布局（header + content + footer）
- Flex 导航栏
- 嵌套 block + flex
- display:none 元素不参与布局
- 定位元素正确偏移

---

## 子代理分组策略

| 组 | 内容 | Phase | 预估 |
|----|------|-------|------|
| **A** | 数据结构 + TextShaper + Utils + CMake | 1 | 15 min |
| **B** | Tree Builder + Block Layout | 2 + 3 | 25 min |
| **C** | Flex Layout | 4 | 20 min |
| **D** | Inline/Text + Positioning | 5 | 15 min |
| **E** | 集成测试 + 收尾 | 6 | 10 min |

依赖关系：A → B/C/D（可并行）→ E

**并行窗口：** A 完成后，B/C/D 三组可并行执行。

---

## 预估汇总

| Phase | 预估时间 | 新增测试 |
|-------|---------|---------|
| Phase 1: 数据结构 + 工具 | 15 min | ~15 |
| Phase 2: 树构建 | 10 min | ~8 |
| Phase 3: Block 布局 | 15 min | ~12 |
| Phase 4: Flex 布局 | 20 min | ~15 |
| Phase 5: Inline + 定位 | 15 min | ~8 |
| Phase 6: 集成 | 10 min | ~8 |
| **合计** | **~85 min** | **~66** |

## 需要创意阶段的组件

**无** — 所有架构决策已在规划阶段确认。Block/Flex/Inline 布局算法由 CSS 规范定义，无需额外创意设计。
