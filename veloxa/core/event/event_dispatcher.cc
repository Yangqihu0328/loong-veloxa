#include "veloxa/core/event/event_dispatcher.h"

#include "veloxa/core/dom/element.h"

#include "veloxa/core/dom/element.h"

namespace vx::event {

ListenerToken EventDispatcher::AddEventListener(dom::Element* element,
                                                EventType type,
                                                EventHandler handler,
                                                bool use_capture) {
  ListenerToken token = next_token_++;
  listeners_[element].push_back(
      EventListener{type, std::move(handler), use_capture, token});
  return token;
}

void EventDispatcher::RemoveEventListeners(dom::Element* element) {
  listeners_.Erase(element);
}

void EventDispatcher::RemoveEventListenerByToken(dom::Element* element,
                                                 ListenerToken token) {
  if (token == kInvalidListenerToken) return;
  auto* vec = listeners_.Find(element);
  if (vec == nullptr) return;
  for (usize i = 0; i < vec->size(); ++i) {
    if ((*vec)[i].token == token) {
      vec->erase(vec->begin() + i);
      if (vec->empty()) listeners_.Erase(element);
      return;
    }
  }
}

void EventDispatcher::Dispatch(DOMEvent& event) {
  if (event.target == nullptr) return;

  // Build path: target → ... → root
  Vector<dom::Element*> path;
  for (auto* el = event.target; el != nullptr; el = el->parent()) {
    path.push_back(el);
  }
  // path[0] = target, path[last] = root

  // Capture phase: root → target's parent
  event.phase = EventPhase::kCapture;
  for (usize i = path.size() - 1; i >= 1; --i) {
    event.current_target = path[i];
    FireListeners(path[i], event, true);
    if (event.propagation_stopped) return;
  }

  // Target phase
  event.phase = EventPhase::kTarget;
  event.current_target = event.target;
  FireListeners(event.target, event, true);
  if (event.propagation_stopped) return;
  FireListeners(event.target, event, false);
  if (event.propagation_stopped) return;

  // Bubble phase: target's parent → root
  event.phase = EventPhase::kBubble;
  for (usize i = 1; i < path.size(); ++i) {
    event.current_target = path[i];
    FireListeners(path[i], event, false);
    if (event.propagation_stopped) return;
  }
}

void EventDispatcher::FireListeners(dom::Element* element, DOMEvent& event,
                                    bool capture_phase) {
  auto* listeners = listeners_.Find(element);
  if (listeners == nullptr) return;
  for (usize i = 0; i < listeners->size(); ++i) {
    auto& listener = (*listeners)[i];
    if (listener.type == event.input.type &&
        listener.use_capture == capture_phase) {
      listener.handler(event);
      if (event.propagation_stopped) return;
    }
  }
}

}  // namespace vx::event
