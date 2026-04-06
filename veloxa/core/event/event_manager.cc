#include "veloxa/core/event/event_manager.h"

#include "veloxa/core/event/hit_test.h"

namespace vx::event {

void EventManager::HandleInput(const InputEvent& input,
                               layout::LayoutBox* layout_root) {
  switch (input.type) {
    case EventType::kPointerMove: {
      dom::Element* new_hover = HitTest(layout_root, input.x, input.y);
      hovered_ = new_hover;
      if (new_hover) {
        DOMEvent event{};
        event.input = input;
        event.target = new_hover;
        dispatcher_.Dispatch(event);
      }
      break;
    }

    case EventType::kPointerDown: {
      dom::Element* target = HitTest(layout_root, input.x, input.y);
      active_ = target;
      if (target != focused_) {
        focused_ = target;
      }
      if (target) {
        DOMEvent event{};
        event.input = input;
        event.target = target;
        dispatcher_.Dispatch(event);
      }
      break;
    }

    case EventType::kPointerUp: {
      dom::Element* target = HitTest(layout_root, input.x, input.y);
      active_ = nullptr;
      if (target) {
        DOMEvent event{};
        event.input = input;
        event.target = target;
        dispatcher_.Dispatch(event);
      }
      break;
    }

    case EventType::kKeyDown:
    case EventType::kKeyUp: {
      if (focused_) {
        DOMEvent event{};
        event.input = input;
        event.target = focused_;
        dispatcher_.Dispatch(event);
      }
      break;
    }

    default:
      break;
  }
}

bool EventManager::IsHovered(const dom::Element* el) const {
  const dom::Element* current = hovered_;
  while (current != nullptr) {
    if (current == el) return true;
    current = current->parent();
  }
  return false;
}

bool EventManager::IsActive(const dom::Element* el) const {
  const dom::Element* current = active_;
  while (current != nullptr) {
    if (current == el) return true;
    current = current->parent();
  }
  return false;
}

bool EventManager::IsFocused(const dom::Element* el) const {
  return el != nullptr && el == focused_;
}

void EventManager::AddEventListener(dom::Element* element, EventType type,
                                    EventHandler handler, bool use_capture) {
  dispatcher_.AddEventListener(element, type, std::move(handler), use_capture);
}

void EventManager::RemoveEventListeners(dom::Element* element) {
  dispatcher_.RemoveEventListeners(element);
}

}  // namespace vx::event
