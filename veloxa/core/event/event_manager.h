#ifndef VELOXA_CORE_EVENT_EVENT_MANAGER_H_
#define VELOXA_CORE_EVENT_EVENT_MANAGER_H_

#include "veloxa/core/event/event_dispatcher.h"
#include "veloxa/core/event/event_types.h"
#include "veloxa/core/layout/layout_box.h"

namespace vx::event {

class EventManager {
 public:
  using InvalidationCallback = std::function<void()>;
  using DestructionObserver = std::function<void()>;
  // Token returned by AddDestructionObserver. Opaque, monotonically increasing,
  // never reused within a single EventManager instance.
  using DestructionObserverToken = u64;

  EventManager() = default;
  // Destructor fires every registered destruction observer in registration
  // order BEFORE releasing internal state, so observers may safely re-enter
  // EventManager (e.g. clear cached pointers).
  ~EventManager();

  EventManager(const EventManager&) = delete;
  EventManager& operator=(const EventManager&) = delete;

  void HandleInput(const InputEvent& input, layout::LayoutBox* layout_root);

  bool IsHovered(const dom::Element* el) const;
  bool IsActive(const dom::Element* el) const;
  bool IsFocused(const dom::Element* el) const;

  // Returns a ListenerToken (forwarded from the underlying EventDispatcher).
  // Pair with RemoveEventListenerByToken for precise removal; otherwise the
  // legacy bulk RemoveEventListeners is still available.
  ListenerToken AddEventListener(dom::Element* element, EventType type,
                                 EventHandler handler,
                                 bool use_capture = false);
  void RemoveEventListeners(dom::Element* element);
  // Removes only the listener identified by `token` on `element`. Unknown
  // tokens are silently ignored.
  void RemoveEventListenerByToken(dom::Element* element, ListenerToken token);

  void SetInvalidationCallback(InvalidationCallback cb);

  // Register `cb` to be invoked when this EventManager is destroyed. Returns
  // a token that may be passed to RemoveDestructionObserver to deregister
  // (e.g. if the observer's owner is destroyed first).
  DestructionObserverToken AddDestructionObserver(DestructionObserver cb);
  // Deregister an observer previously registered via AddDestructionObserver.
  // Unknown tokens are silently ignored.
  void RemoveDestructionObserver(DestructionObserverToken token);

  dom::Element* hovered_element() const { return hovered_; }
  dom::Element* active_element() const { return active_; }
  dom::Element* focused_element() const { return focused_; }

 private:
  struct DestructionObserverEntry {
    DestructionObserverToken token;
    DestructionObserver callback;
  };

  dom::Element* hovered_ = nullptr;
  dom::Element* active_ = nullptr;
  dom::Element* focused_ = nullptr;
  EventDispatcher dispatcher_;
  InvalidationCallback invalidation_callback_;
  Vector<DestructionObserverEntry> destruction_observers_;
  DestructionObserverToken next_destruction_token_ = 1;
};

}  // namespace vx::event

#endif  // VELOXA_CORE_EVENT_EVENT_MANAGER_H_
