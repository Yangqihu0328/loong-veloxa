#ifndef VELOXA_DEVTOOL_CONSOLE_CONSOLE_ENGINE_H_
#define VELOXA_DEVTOOL_CONSOLE_CONSOLE_ENGINE_H_

#include <memory>
#include <string>

#include "veloxa/foundation/base/status.h"
#include "veloxa/foundation/strings/string_view.h"

struct JSContext;

namespace vx::script {
class QuickjsEngine;
}  // namespace vx::script

namespace vx::devtool::console {

// TASK-20260503-04 D.1 — Isolated QuickJS runtime hosting the DevTool
// Console JS REPL. Wraps script::QuickjsEngine and makes T1 mitigation
// the default behaviour (dim 1: default-safe, dim 3: cannot-bypass).
//
// Why a separate engine instead of reusing devtool_script_engine_?
//   Console eval ingests untrusted user input typed at the REPL; running
//   it inside the devtool_script_engine_ (which loads inspector_panel.js)
//   would let user code overwrite or interrogate panel JS globals. C2.B
//   creative decision: third QuickjsEngine instance, owned by Application
//   alongside devtool_script_engine_, with its own JSRuntime/JSContext
//   and its own InterruptHandler counter (single-threaded contract per
//   creative-quickjs-host §组件 1 方案 C Phase 2).
//
// What this class does NOT do:
//   - DomBindings::Bind: Console scope MUST NOT see Element mutators per
//     spec §11.1 capability-allowlist. RegisterConsoleBindings (D.2)
//     installs only console.log/error/warn + drain query.
//   - js_std_add_helpers: inherits creative-quickjs-host §组件 2 方案 B
//     via QuickjsEngine::Init — no `os` / `std` modules → no file/network
//     reach (T1 mitigation auto-satisfied).
//   - JS_SetMemoryLimit override: inherits creative-quickjs-host §组件 3
//     方案 B (~32 MiB per JSRuntime) which QuickjsEngine::Init applies
//     unconditionally (audited: quickjs_engine.cc:54).
class ConsoleEngine {
 public:
  // Default budget Init() auto-applies (T1 mitigation dim 1 + 3). Public
  // so unit tests can assert the "cannot bypass" contract by reading the
  // forwarded value via a tighter override in the same EvalGlobal call.
  // Forwarded straight to QuickjsEngine::SetEvalInterruptBudget.
  static constexpr usize kDefaultInterruptBudgetCheckpoints = 10000;

  ConsoleEngine();
  ~ConsoleEngine();

  ConsoleEngine(const ConsoleEngine&) = delete;
  ConsoleEngine& operator=(const ConsoleEngine&) = delete;

  // Initialize the underlying QuickjsEngine and install the default-safe
  // interrupt budget. Idempotent on a fresh instance (call twice → second
  // Init returns Ok and re-applies the default budget without re-creating
  // the runtime).
  Status Init();

  // Tear down the underlying runtime + context. Subsequent EvalGlobal
  // calls return kInvalidArgument. Idempotent.
  void Shutdown();

  // Evaluate `source` as a global script. Resets the interrupt counter
  // before each call (so a prior abort does not poison subsequent
  // evaluations). Returns the toString()-formatted value on success,
  // or a Status with kAborted (budget exhausted) / kInternal (runtime
  // exception) / kInvalidArgument (uninitialised or oversized source).
  StatusOr<std::string> EvalGlobal(StringView source, StringView filename);

  // Override the interrupt budget for subsequent EvalGlobal calls. Pass
  // 0 to opt out entirely (NOT recommended for Console; provided only to
  // support the audit/test path that proves budget=0 disables the abort).
  void SetEvalInterruptBudget(usize max_checkpoints);

  // Snapshot of whether the most recent EvalGlobal was aborted by the
  // interrupt handler. Reset to false at the start of every EvalGlobal.
  bool WasInterrupted() const;

  // Underlying JSContext, exposed for RegisterConsoleBindings (D.2).
  // Returns nullptr after Shutdown.
  JSContext* context() const;

 private:
  std::unique_ptr<script::QuickjsEngine> engine_;
};

}  // namespace vx::devtool::console

#endif  // VELOXA_DEVTOOL_CONSOLE_CONSOLE_ENGINE_H_
