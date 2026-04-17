#ifndef VELOXA_SCRIPT_DOM_BINDINGS_H_
#define VELOXA_SCRIPT_DOM_BINDINGS_H_

#include <memory>

#include "veloxa/core/dom/document.h"
#include "veloxa/core/event/event_manager.h"

struct JSContext;

namespace vx::script {

class DomBindings {
 public:
  DomBindings();
  ~DomBindings();

  DomBindings(const DomBindings&) = delete;
  DomBindings& operator=(const DomBindings&) = delete;

  void Bind(JSContext* ctx, dom::Document* doc,
            event::EventManager* em = nullptr);
  void Unbind();

  bool bound() const;
  event::EventManager* event_manager() const;
  dom::Document* document() const;
  JSContext* context() const;

  // Forward declaration is public so internal free-function callbacks
  // (in the .cc file's anonymous namespace) can name the type when
  // receiving a pointer from DomBindingsInternal. The definition and
  // the data_ field remain private, preserving pimpl encapsulation.
  struct InstanceData;

 private:
  friend struct DomBindingsInternal;
  std::unique_ptr<InstanceData> data_;
};

}  // namespace vx::script

#endif  // VELOXA_SCRIPT_DOM_BINDINGS_H_
