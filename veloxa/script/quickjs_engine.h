#ifndef VELOXA_SCRIPT_QUICKJS_ENGINE_H_
#define VELOXA_SCRIPT_QUICKJS_ENGINE_H_

#include <atomic>
#include <string>

#include "veloxa/foundation/base/status.h"
#include "veloxa/foundation/base/types.h"
#include "veloxa/foundation/strings/string_view.h"

struct JSContext;
struct JSRuntime;

namespace vx {
namespace script {

// Minimal QuickJS embed: global eval only. No js_std_add_helpers (see
// memory-bank/creative/creative-quickjs-host.md).
//
// Eval interrupt budget: see creative-quickjs-host.md §组件 1 方案 C Phase 2.
// Default disabled (budget=0). Callers opt-in via SetEvalInterruptBudget(N>0).
// Each EvalGlobal resets the counter; if exhausted, eval aborts with kAborted.
class QuickjsEngine {
 public:
  // Default budget recommendation when caller opts in. 10000 handler
  // invocations correspond to ~10^8 bytecode (~100-500ms infinite loop on
  // modern CPUs), aligned with QuickJS internal JS_INTERRUPT_COUNTER_INIT
  // = 10000 (one handler call per ~10000 bytecode).
  static constexpr usize kDefaultInterruptBudgetCheckpoints = 10000;

  QuickjsEngine();
  ~QuickjsEngine();

  QuickjsEngine(const QuickjsEngine&) = delete;
  QuickjsEngine& operator=(const QuickjsEngine&) = delete;

  Status Init();
  void Shutdown();

  StatusOr<std::string> EvalGlobal(StringView source, StringView filename);

  // Set the per-EvalGlobal interrupt budget in handler-invocation count.
  // 0 disables the budget (default). Must be called after Init() to take
  // effect on subsequent EvalGlobal calls.
  void SetEvalInterruptBudget(usize max_checkpoints);

  // True iff the most recent EvalGlobal was aborted by the interrupt handler.
  // Reset to false at the start of every EvalGlobal call.
  bool WasInterrupted() const { return was_interrupted_; }

  JSContext* context() const { return ctx_; }

 private:
  static int InterruptCallback(JSRuntime* rt, void* opaque);

  JSRuntime* rt_ = nullptr;
  JSContext* ctx_ = nullptr;
  bool inited_ = false;

  usize interrupt_budget_ = 0;  // 0 = disabled.
  std::atomic<int64_t> interrupt_counter_{0};
  bool was_interrupted_ = false;
};

}  // namespace script
}  // namespace vx

#endif  // VELOXA_SCRIPT_QUICKJS_ENGINE_H_
