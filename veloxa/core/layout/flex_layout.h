#ifndef VELOXA_CORE_LAYOUT_FLEX_LAYOUT_H_
#define VELOXA_CORE_LAYOUT_FLEX_LAYOUT_H_

#include "veloxa/core/layout/layout_box.h"
#include "veloxa/core/layout/layout_utils.h"

namespace vx::layout {

void LayoutFlex(LayoutBox* box, f32 containing_width,
                const LayoutContext& ctx);

}  // namespace vx::layout

#endif  // VELOXA_CORE_LAYOUT_FLEX_LAYOUT_H_
