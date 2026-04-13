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

StatusOr<std::string> QuickjsEngine::EvalGlobal(StringView source,
                                                StringView filename) {
  if (!inited_) {
    return Status(StatusCode::kInvalidArgument, "QuickjsEngine not initialized");
  }
  if (source.size() > kMaxSourceBytes) {
    return Status(StatusCode::kInvalidArgument,
                   "script source exceeds max size");
  }
  const std::string fn(filename.data(), filename.size());
  JSValue val =
      JS_Eval(ctx_, source.data(), static_cast<size_t>(source.size()),
              fn.c_str(), JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(val)) {
    JS_FreeValue(ctx_, val);
    JSValue ex = JS_GetException(ctx_);
    std::string err = JsValueToUtf8(ctx_, ex);
    JS_FreeValue(ctx_, ex);
    return Status(StatusCode::kInternal, err);
  }
  std::string out = JsValueToUtf8(ctx_, val);
  JS_FreeValue(ctx_, val);
  return out;
}

}  // namespace script
}  // namespace vx
