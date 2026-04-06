#ifndef VELOXA_CORE_EVENT_EVENT_DISPATCHER_H_
#define VELOXA_CORE_EVENT_EVENT_DISPATCHER_H_

#include "veloxa/core/event/event_types.h"
#include "veloxa/foundation/containers/hash_map.h"
#include "veloxa/foundation/containers/vector.h"

namespace vx::event {

struct EventListener {
  EventType type;
  EventHandler handler;
  bool use_capture = false;
};

class EventDispatcher {
 public:
  void AddEventListener(dom::Element* element, EventType type,
                        EventHandler handler, bool use_capture = false);
  void RemoveEventListeners(dom::Element* element);
  void Dispatch(DOMEvent& event);

 private:
  void FireListeners(dom::Element* element, DOMEvent& event,
                     bool capture_phase);

  HashMap<dom::Element*, Vector<EventListener>> listeners_;
};

}  // namespace vx::event

#endif  // VELOXA_CORE_EVENT_EVENT_DISPATCHER_H_
