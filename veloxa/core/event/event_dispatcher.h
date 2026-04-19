#ifndef VELOXA_CORE_EVENT_EVENT_DISPATCHER_H_
#define VELOXA_CORE_EVENT_EVENT_DISPATCHER_H_

#include "veloxa/core/event/event_types.h"
#include "veloxa/foundation/containers/hash_map.h"
#include "veloxa/foundation/containers/vector.h"

namespace vx::event {

// Token returned by AddEventListener; passed back to RemoveEventListenerByToken
// to remove a single listener (vs the legacy bulk-remove). 0 is reserved as
// "invalid"; tokens are monotonically increasing within one EventDispatcher
// instance and never reused.
using ListenerToken = u64;
inline constexpr ListenerToken kInvalidListenerToken = 0;

struct EventListener {
  EventType type;
  EventHandler handler;
  bool use_capture = false;
  ListenerToken token = kInvalidListenerToken;
};

class EventDispatcher {
 public:
  // Returns a token uniquely identifying this listener within this dispatcher.
  // Callers may discard the token if they do not need precise removal — the
  // legacy RemoveEventListeners(element) bulk path still works.
  ListenerToken AddEventListener(dom::Element* element, EventType type,
                                 EventHandler handler,
                                 bool use_capture = false);
  void RemoveEventListeners(dom::Element* element);
  // Removes the single listener identified by `token` registered on `element`.
  // Unknown tokens (already-removed, never-issued, wrong element) are a no-op.
  void RemoveEventListenerByToken(dom::Element* element, ListenerToken token);
  void Dispatch(DOMEvent& event);

 private:
  void FireListeners(dom::Element* element, DOMEvent& event,
                     bool capture_phase);

  HashMap<dom::Element*, Vector<EventListener>> listeners_;
  ListenerToken next_token_ = 1;
};

}  // namespace vx::event

#endif  // VELOXA_CORE_EVENT_EVENT_DISPATCHER_H_
