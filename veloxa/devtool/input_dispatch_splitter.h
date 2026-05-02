#ifndef VELOXA_DEVTOOL_INPUT_DISPATCH_SPLITTER_H_
#define VELOXA_DEVTOOL_INPUT_DISPATCH_SPLITTER_H_

#include "veloxa/foundation/base/types.h"

namespace vx::devtool {

// Where a pointer event should be dispatched. Returned by
// InputDispatchSplitter::RouteToDocument so the caller (Application's
// input dispatch loop, A.1.6) can forward the event to the correct
// EventManager / handle the splitter drag itself.
enum class DispatchTarget {
  kTarget,          // event belongs to the user-facing target Document
  kDevTool,         // event belongs to the DevTool dogfood UI Document
  kSplitterHandle,  // event belongs to the splitter dock handle widget
};

// =============================================================================
// InputDispatchSplitter — TASK-20260502-01 A.1.5
//
// Pure state-machine that classifies pointer events for the DevTool
// splitter dock layout (creative #1 decision 1: target Document on the
// left, DevTool dogfood UI Document on the right, draggable handle
// between them).
//
// Routing rules:
//   1. Static hit area: |mouse_x - splitter_x| <= hit_handle_half_width
//      → kSplitterHandle. Otherwise mouse_x < splitter_x → kTarget,
//      mouse_x > splitter_x → kDevTool.
//   2. Drag capture: when pointer-down lands on the handle, ALL
//      subsequent events sticky-route to kSplitterHandle until pointer-up
//      (regardless of mouse_x). This prevents fast drags from "losing"
//      the handle when the cursor leaves the ±4px hit area between
//      frames — standard splitter UX contract.
//   3. Pointer-down outside handle never starts drag capture (R1
//      mitigation — would otherwise hijack normal target/devtool clicks).
//
// Stateless / referentially transparent except for the dragging_ +
// capture_target_ pair; no engine I/O, no allocations.
// =============================================================================

class InputDispatchSplitter {
 public:
  void SetSplitterX(f32 x) { splitter_x_ = x; }
  f32 splitter_x() const { return splitter_x_; }

  // Classify a single pointer event. is_pointer_down/up encode the SDL2
  // mouse-button state edges (cf. SDL_MOUSEBUTTONDOWN/UP); pure mouse
  // moves pass false/false.
  DispatchTarget RouteToDocument(f32 mouse_x, f32 mouse_y,
                                 bool is_pointer_down, bool is_pointer_up);

  bool dragging() const { return dragging_; }

 private:
  f32 splitter_x_ = 530.0f;          // window 800 - DevTool dock 270
  f32 hit_handle_half_width_ = 4.0f;  // splitter handle ±4px hit zone
  bool dragging_ = false;
  DispatchTarget capture_target_ = DispatchTarget::kTarget;
};

}  // namespace vx::devtool

#endif  // VELOXA_DEVTOOL_INPUT_DISPATCH_SPLITTER_H_
