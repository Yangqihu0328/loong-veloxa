#ifndef VELOXA_CORE_EVENT_HIT_TEST_H_
#define VELOXA_CORE_EVENT_HIT_TEST_H_

#include "veloxa/core/dom/element.h"
#include "veloxa/core/layout/layout_box.h"
#include "veloxa/foundation/base/types.h"

namespace vx::event {

dom::Element* HitTest(layout::LayoutBox* root, f32 x, f32 y);

}  // namespace vx::event

#endif  // VELOXA_CORE_EVENT_HIT_TEST_H_
