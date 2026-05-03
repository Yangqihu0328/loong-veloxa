#include "veloxa/script/quickjs_engine.h"

extern "C" {
#include "quickjs.h"
}

#include <string>

namespace vx {
namespace script {

namespace {

constexpr usize kMaxSourceBytes = 256 * 1024;
constexpr u64 kRuntimeMemoryLimitBytes = 32ULL * 1024 * 1024;

// QuickJS throws InternalError("interrupted") + JS_SetUncatchableError when
// our InterruptCallback returns non-zero (quickjs.c:8165-8169). The literal
// is upstream-stable and serves as the error-message anchor for kAborted
// translation in EvalGlobal.
constexpr const char* kInterruptedQuickjsMessage = "interrupted";
constexpr const char* kAbortedHumanMessage =
    "script aborted (interrupt budget exhausted)";

std::string JsValueToUtf8(JSContext* ctx, JSValue val) {
  JSValue str = JS_ToString(ctx, val);
  if (JS_IsException(str)) {
    JS_FreeValue(ctx, str);
    return "stringify failed";
  }
  const char* utf8 = JS_ToCString(ctx, str);
  std::string out = utf8 ? utf8 : "";
  if (utf8) {
    JS_FreeCString(ctx, utf8);
  }
  JS_FreeValue(ctx, str);
  return out;
}

}  // namespace

QuickjsEngine::QuickjsEngine() = default;

QuickjsEngine::~QuickjsEngine() { Shutdown(); }

Status QuickjsEngine::Init() {
  if (inited_) {
    return Status::Ok();
  }
  rt_ = JS_NewRuntime();
  if (!rt_) {
    return Status(StatusCode::kOutOfMemory, "JS_NewRuntime failed");
  }
  JS_SetMemoryLimit(rt_, static_cast<size_t>(kRuntimeMemoryLimitBytes));
  // Always register the interrupt handler. Behavior switches by budget:
  //   budget == 0 -> handler returns 0 (no abort) cheaply.
  //   budget >  0 -> handler decrements counter; aborts when exhausted.
  // Per creative §组件 1 §实现指导: callback only does atomic decrement +
  // return; no EventLoop coupling, no allocation.
  JS_SetInterruptHandler(rt_, &QuickjsEngine::InterruptCallback, this);
  ctx_ = JS_NewContext(rt_);
  if (!ctx_) {
    JS_FreeRuntime(rt_);
    rt_ = nullptr;
    return Status(StatusCode::kOutOfMemory, "JS_NewContext failed");
  }
  inited_ = true;
  return Status::Ok();
}

void QuickjsEngine::Shutdown() {
  if (ctx_) {
    JS_FreeContext(ctx_);
    ctx_ = nullptr;
  }
  if (rt_) {
    JS_FreeRuntime(rt_);
    rt_ = nullptr;
  }
  inited_ = false;
}

void QuickjsEngine::SetEvalInterruptBudget(usize max_checkpoints) {
  interrupt_budget_ = max_checkpoints;
}

// Static callback registered via JS_SetInterruptHandler. opaque == this.
// Single-threaded contract per JSRuntime; std::atomic is defensive and keeps
// the read/write hardware-ordered for any future worker-thread integration.
int QuickjsEngine::InterruptCallback(JSRuntime* /*rt*/, void* opaque) {
  auto* self = static_cast<QuickjsEngine*>(opaque);
  if (self == nullptr || self->interrupt_budget_ == 0) {
    return 0;  // Disabled: never abort.
  }
  // Decrement; abort when budget exhausted (counter <= 0 after decrement).
  if (self->interrupt_counter_.fetch_sub(1, std::memory_order_relaxed) <= 1) {
    self->was_interrupted_ = true;
    return 1;
  }
  return 0;
}

StatusOr<std::string> QuickjsEngine::EvalGlobal(StringView source,
                                                StringView filename) {
  if (!inited_) {
    return Status(StatusCode::kInvalidArgument, "QuickjsEngine not initialized");
  }
  if (source.size() > kMaxSourceBytes) {
    return Status(StatusCode::kInvalidArgument,
                  "script source exceeds max size");
  }
  // Reset interrupt state at every EvalGlobal entry. budget=0 still resets
  // (cheap; keeps WasInterrupted() semantics last-eval-only).
  interrupt_counter_.store(static_cast<int64_t>(interrupt_budget_),
                           std::memory_order_relaxed);
  was_interrupted_ = false;

  const std::string fn(filename.data(), filename.size());
  JSValue val =
      JS_Eval(ctx_, source.data(), static_cast<size_t>(source.size()),
              fn.c_str(), JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(val)) {
    JS_FreeValue(ctx_, val);
    JSValue ex = JS_GetException(ctx_);
    std::string err = JsValueToUtf8(ctx_, ex);
    JS_FreeValue(ctx_, ex);
    // Identify QuickJS interrupt: was_interrupted_ flag (set by callback) is
    // the primary signal; the upstream-stable "interrupted" message string is
    // a defensive secondary check.
    if (was_interrupted_ ||
        err.find(kInterruptedQuickjsMessage) != std::string::npos) {
      was_interrupted_ = true;
      return Status(StatusCode::kAborted, kAbortedHumanMessage);
    }
    return Status(StatusCode::kInternal, err);
  }
  std::string out = JsValueToUtf8(ctx_, val);
  JS_FreeValue(ctx_, val);
  return out;
}

}  // namespace script
}  // namespace vx
