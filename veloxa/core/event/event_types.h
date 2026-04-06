#ifndef VELOXA_CORE_EVENT_EVENT_TYPES_H_
#define VELOXA_CORE_EVENT_EVENT_TYPES_H_

#include <functional>

#include "veloxa/foundation/base/types.h"

namespace vx::dom {
class Element;
}

namespace vx::event {

enum class EventType : u8 {
  kPointerDown,
  kPointerUp,
  kPointerMove,
  kKeyDown,
  kKeyUp,
  kTouchStart,
  kTouchEnd,
  kTouchMove,
  kFocusIn,
  kFocusOut,
};

struct InputEvent {
  EventType type;
  f32 x = 0;
  f32 y = 0;
  u8 button = 0;
  u32 key_code = 0;
  u32 modifiers = 0;
  u32 touch_id = 0;
  u64 timestamp_ms = 0;
};

enum class EventPhase : u8 {
  kNone,
  kCapture,
  kTarget,
  kBubble,
};

struct DOMEvent {
  InputEvent input;
  dom::Element* target = nullptr;
  dom::Element* current_target = nullptr;
  EventPhase phase = EventPhase::kNone;
  bool propagation_stopped = false;
  bool default_prevented = false;

  void StopPropagation() { propagation_stopped = true; }
  void PreventDefault() { default_prevented = true; }
};

using EventHandler = std::function<void(DOMEvent&)>;

}  // namespace vx::event

#endif  // VELOXA_CORE_EVENT_EVENT_TYPES_H_
