#include "veloxa/devtool/console/console_engine.h"

#include "veloxa/script/quickjs_engine.h"

namespace vx::devtool::console {

ConsoleEngine::ConsoleEngine()
    : engine_(std::make_unique<script::QuickjsEngine>()) {}

ConsoleEngine::~ConsoleEngine() = default;

Status ConsoleEngine::Init() {
  auto s = engine_->Init();
  if (!s.ok()) return s;
  // T1 mitigation dim 1 (default-safe) + dim 3 (cannot-bypass): ALWAYS
  // apply the default budget after Init succeeds. Console user input
  // never touches this call site, so a malicious eval cannot reset the
  // budget to 0 even via SetEvalInterruptBudget — that method is on the
  // C++ owner (Application), not exposed as a JS binding (D.2 verifies
  // RegisterConsoleBindings does not install any setter for budget).
  engine_->SetEvalInterruptBudget(kDefaultInterruptBudgetCheckpoints);
  return Status::Ok();
}

void ConsoleEngine::Shutdown() { engine_->Shutdown(); }

StatusOr<std::string> ConsoleEngine::EvalGlobal(StringView source,
                                                StringView filename) {
  return engine_->EvalGlobal(source, filename);
}

void ConsoleEngine::SetEvalInterruptBudget(usize max_checkpoints) {
  engine_->SetEvalInterruptBudget(max_checkpoints);
}

bool ConsoleEngine::WasInterrupted() const {
  return engine_->WasInterrupted();
}

JSContext* ConsoleEngine::context() const { return engine_->context(); }

}  // namespace vx::devtool::console
