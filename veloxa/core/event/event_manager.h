#ifndef VELOXA_CORE_EVENT_EVENT_MANAGER_H_
#define VELOXA_CORE_EVENT_EVENT_MANAGER_H_

#include "veloxa/core/event/event_dispatcher.h"
#include "veloxa/core/event/event_types.h"
#include "veloxa/core/layout/layout_box.h"

namespace vx::event {

class EventManager {
 public:
  using InvalidationCallback = std::function<void()>;

  EventManager() = default;

  void HandleInput(const InputEvent& input, layout::LayoutBox* layout_root);

  bool IsHovered(const dom::Element* el) const;
  bool IsActive(const dom::Element* el) const;
  bool IsFocused(const dom::Element* el) const;

  void AddEventListener(dom::Element* element, EventType type,
                        EventHandler handler, bool use_capture = false);
  void RemoveEventListeners(dom::Element* element);

  void SetInvalidationCallback(InvalidationCallback cb);

  dom::Element* hovered_element() const { return hovered_; }
  dom::Element* active_element() const { return active_; }
  dom::Element* focused_element() const { return focused_; }

 private:
  dom::Element* hovered_ = nullptr;
  dom::Element* active_ = nullptr;
  dom::Element* focused_ = nullptr;
  EventDispatcher dispatcher_;
  InvalidationCallback invalidation_callback_;
};

}  // namespace vx::event

#endif  // VELOXA_CORE_EVENT_EVENT_MANAGER_H_
