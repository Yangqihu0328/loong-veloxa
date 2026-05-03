#ifndef VELOXA_SCRIPT_DOM_BINDINGS_H_
#define VELOXA_SCRIPT_DOM_BINDINGS_H_

#include <memory>

#include "veloxa/core/dom/document.h"
#include "veloxa/core/dom/serializer.h"
#include "veloxa/core/event/event_manager.h"

struct JSContext;

namespace vx::devtool::overlay {
class PerfOverlay;  // forward declare to avoid pulling vx_devtool into vx_script
                    // header dependency (vx_script → vx_devtool circular).
}

namespace vx::devtool::hot_reload {
class HotReloadManager;  // forward declare for same reason as PerfOverlay
                         // (TASK-20260503-01 C.3.1).
}

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

  // TASK-20260502-01 A.1.4 — set the target Document inspected by the
  // DevTool dogfood UI's vx_devtool_get_dom_json() native binding. This
  // pointer is independent of `document()` (which here is the DevTool's
  // own UI Document). May be nullptr to clear; ownership is not taken.
  // Available regardless of VX_BUILD_DEVTOOL (no-op effect if OFF since
  // RegisterDevtoolBindings is a stub).
  void SetDevtoolTargetDocument(dom::Document* target);
  dom::Document* devtool_target_document() const;

  // TASK-20260502-01 A.2.1 — T3 redaction policy used by
  // RegisterDevtoolBindings's vx_devtool_get_dom_json. Default is
  // kRedactSensitive; embedders flip via vx_inspector_set_redaction_policy
  // (which drives Application::set_redaction_policy → DomBindings sync).
  void SetRedactionPolicy(dom::RedactionPolicy policy);
  dom::RedactionPolicy redaction_policy() const;

  // TASK-20260502-02 B.2.2 — set the PerfOverlay instance whose live
  // FrameStats are exposed to JS via vx_view_get_perf_stats() native
  // binding (registered by RegisterDevtoolBindings). Pointer is borrowed;
  // ownership stays with the caller. nullptr clears (binding will return
  // a zero-stats JSON envelope). DEVTOOL=OFF: no-op (vx_view_get_perf_stats
  // is itself a stub then; setter still callable to keep ABI symmetric).
  void SetPerfOverlay(vx::devtool::overlay::PerfOverlay* perf);
  vx::devtool::overlay::PerfOverlay* perf_overlay() const;

  // TASK-20260503-01 C.3.1 — set the HotReloadManager whose tracked_count
  // and last_error are exposed to JS via vx_devtool_get_hot_reload_status()
  // (registered by RegisterDevtoolBindings). Pointer is borrowed; ownership
  // stays with the caller. nullptr clears (binding will return the safe
  // zero envelope {"tracked":0,"last_error":""}). DEVTOOL=OFF: no-op
  // (vx_devtool_get_hot_reload_status is itself a stub then; setter still
  // callable to keep ABI symmetric with PerfOverlay).
  void SetHotReloadManager(vx::devtool::hot_reload::HotReloadManager* mgr);
  vx::devtool::hot_reload::HotReloadManager* hot_reload_manager() const;

  // Forward declaration is public so internal free-function callbacks
  // (in the .cc file's anonymous namespace) can name the type when
  // receiving a pointer from DomBindingsInternal. The definition and
  // the data_ field remain private, preserving pimpl encapsulation.
  struct InstanceData;

 private:
  friend struct DomBindingsInternal;
  std::unique_ptr<InstanceData> data_;
};

// TASK-20260502-01 A.1.4 — DevTool dogfood UI native binding.
//
// Registers global JS function `vx_devtool_get_dom_json()` on `ctx`'s
// global object. The function recovers the target Document via
// JS_GetContextOpaque(ctx) → DomBindings → InstanceData →
// devtool_target_doc and returns
// `vx::devtool::SerializeDocument(target, kRedactSensitive)` as a JS
// string. If no target was set, returns the JSON string "null".
//
// When VX_BUILD_DEVTOOL=OFF this is a no-op stub (A14 zero-byte guard);
// inspector_panel.js's call to vx_devtool_get_dom_json will see the
// global as undefined, but the OFF path never loads inspector_panel.js
// (DevTool subsystem is not compiled).
//
// Pre-condition: caller must have already invoked DomBindings::Bind()
// on the same ctx (RegisterDevtoolBindings depends on the
// ContextOpaque link to recover the target Document at call time).
void RegisterDevtoolBindings(JSContext* ctx);

}  // namespace vx::script

#endif  // VELOXA_SCRIPT_DOM_BINDINGS_H_
