#ifndef VELOXA_SCRIPT_QUICKJS_ENGINE_H_
#define VELOXA_SCRIPT_QUICKJS_ENGINE_H_

#include <string>

#include "veloxa/foundation/base/status.h"
#include "veloxa/foundation/strings/string_view.h"

struct JSContext;
struct JSRuntime;

namespace vx {
namespace script {

// Minimal QuickJS embed: global eval only. No js_std_add_helpers (see
// memory-bank/creative/creative-quickjs-host.md).
class QuickjsEngine {
 public:
  QuickjsEngine();
  ~QuickjsEngine();

  QuickjsEngine(const QuickjsEngine&) = delete;
  QuickjsEngine& operator=(const QuickjsEngine&) = delete;

  Status Init();
  void Shutdown();

  StatusOr<std::string> EvalGlobal(StringView source, StringView filename);

 private:
  JSRuntime* rt_ = nullptr;
  JSContext* ctx_ = nullptr;
  bool inited_ = false;
};

}  // namespace script
}  // namespace vx

#endif  // VELOXA_SCRIPT_QUICKJS_ENGINE_H_
