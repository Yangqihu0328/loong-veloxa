#include "veloxa/devtool/input_dispatch_splitter.h"

#include <cmath>

namespace vx::devtool {

DispatchTarget InputDispatchSplitter::RouteToDocument(
    f32 mouse_x, f32 /*mouse_y*/, bool is_pointer_down, bool is_pointer_up) {
  // Sticky drag-capture wins over geometry. The press that started the
  // drag must have landed on the handle (engaged below); subsequent
  // events route to the captured target until pointer up.
  if (dragging_) {
    DispatchTarget current = capture_target_;
    if (is_pointer_up) {
      // Release: terminate drag, but the up event itself still belongs
      // to the captured target so the handle widget sees the matching
      // up event for its own state machine.
      dragging_ = false;
      capture_target_ = DispatchTarget::kTarget;
    }
    return current;
  }

  // Static geometry classification.
  const f32 dx = std::fabs(mouse_x - splitter_x_);
  DispatchTarget zone;
  if (dx <= hit_handle_half_width_) {
    zone = DispatchTarget::kSplitterHandle;
  } else if (mouse_x < splitter_x_) {
    zone = DispatchTarget::kTarget;
  } else {
    zone = DispatchTarget::kDevTool;
  }

  // Engage drag capture only when a press lands on the handle (R1
  // mitigation: pressing inside target/devtool never sticky-captures,
  // so normal clicks are never hijacked).
  if (is_pointer_down && zone == DispatchTarget::kSplitterHandle) {
    dragging_ = true;
    capture_target_ = DispatchTarget::kSplitterHandle;
  }

  return zone;
}

}  // namespace vx::devtool
