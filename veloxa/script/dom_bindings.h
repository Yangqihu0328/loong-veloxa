#ifndef VELOXA_SCRIPT_DOM_BINDINGS_H_
#define VELOXA_SCRIPT_DOM_BINDINGS_H_

#include "veloxa/core/dom/document.h"
#include "veloxa/core/event/event_manager.h"

struct JSContext;

namespace vx::script {

class DomBindings {
 public:
  DomBindings() = default;
  ~DomBindings();

  DomBindings(const DomBindings&) = delete;
  DomBindings& operator=(const DomBindings&) = delete;

  void Bind(JSContext* ctx, dom::Document* doc,
            event::EventManager* em = nullptr);
  void Unbind();

  bool bound() const { return ctx_ != nullptr; }
  event::EventManager* event_manager() const { return em_; }
  dom::Document* document() const { return doc_; }

 private:
  JSContext* ctx_ = nullptr;
  dom::Document* doc_ = nullptr;
  event::EventManager* em_ = nullptr;
};

}  // namespace vx::script

#endif  // VELOXA_SCRIPT_DOM_BINDINGS_H_
