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
                              const css::ComputedStyle* parent_style,
                              ArenaAllocator& arena,
                              const LayoutContext& ctx);

  static LayoutBox* CreateBox(LayoutType type, ArenaAllocator& arena);

  static void LayoutBlock(LayoutBox* box, f32 containing_width,
                          const LayoutContext& ctx);

  static void LayoutFlex(LayoutBox* box, f32 containing_width,
                         const LayoutContext& ctx);

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
