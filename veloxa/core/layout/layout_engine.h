#ifndef VELOXA_CORE_LAYOUT_LAYOUT_ENGINE_H_
#define VELOXA_CORE_LAYOUT_LAYOUT_ENGINE_H_

#include "veloxa/core/dom/document.h"
#include "veloxa/core/layout/layout_box.h"
#include "veloxa/core/layout/layout_utils.h"
#include "veloxa/core/layout/margin_collapse.h"
#include "veloxa/foundation/memory/arena_allocator.h"

namespace vx::layout {

class LayoutEngine {
 public:
  static LayoutBox* Layout(dom::Document* doc,
                           const LayoutContext& ctx);
  static LayoutBox* Layout(dom::Document* doc,
                           const LayoutContext& ctx,
                           ArenaAllocator& arena);

 private:
  static LayoutBox* BuildTree(dom::Element* element,
                              const css::ComputedStyle* parent_style,
                              ArenaAllocator& arena,
                              const LayoutContext& ctx);

  static LayoutBox* CreateBox(LayoutType type, ArenaAllocator& arena);

  static void LayoutBlock(LayoutBox* box, f32 containing_width,
                          const LayoutContext& ctx);

  // first/last child margin collapse with parent — CSS 2.1 §8.3.1
  // (TASK-20260430-01 D1.A1; D2 实施时调整为 in-out by-pointer)。
  // 仅在 LayoutBlock 内部对 kBlock 子调用。
  //
  // - incoming (in-out by-pointer):
  //     入参时含「box 之前的 chain」（祖先链路 first-in-flow propagate 的累积）。
  //     LayoutBlockChild 在 !blocks_top 时会把 box.margin_top 加入 *incoming，
  //     并继续把 box 内部 first-in-flow 链路上的 margin propagate 到 *incoming
  //     （让 ancestor 看到完整 chain，物理化由顶层 LayoutBlock wrapper 完成）。
  //     在 blocks_top 时不修改 *incoming。
  // - return: box 完成后**渗出给 caller** 的 chain，含义：
  //     * blocks_bottom=false 且 collapse-through 或 last-child collapse with parent:
  //         outgoing = box 内部 cur_chain + box.margin_bottom
  //     * blocks_bottom=true: outgoing = empty + box.margin_bottom (mb 独立累积)
  //
  // box.x 在 LayoutBlockChild 内部解析（auto-margin 居中）；box.y 由 caller 写。
  static MarginChain LayoutBlockChild(LayoutBox* box, f32 containing_width,
                                       const LayoutContext& ctx,
                                       MarginChain* incoming);

  static void LayoutInline(LayoutBox* box, f32 containing_width,
                           const LayoutContext& ctx);

  static void ApplyPositioning(LayoutBox* box,
                               const LayoutContext& ctx);

  static void LayoutChild(LayoutBox* child, f32 containing_width,
                          const LayoutContext& ctx);

  static f32 ResolveFontSize(const css::ComputedStyle* style,
                             const LayoutContext& ctx);
};

}  // namespace vx::layout

#endif  // VELOXA_CORE_LAYOUT_LAYOUT_ENGINE_H_
