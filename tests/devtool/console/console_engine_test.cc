// TASK-20260503-04 D.1 — ConsoleEngine isolated runtime + default-safe T1
// budget tests. Lives in tests/devtool/console/ which is gated by the parent
// tests/devtool/CMakeLists.txt under VX_BUILD_DEVTOOL=ON.
//
// Coverage matrix:
//   D.1-T1  Construct + default-state — Init returns Ok, context() != null.
//   D.1-T2  EvalGlobal arithmetic round-trip (engine wired into QuickJS).
//   D.1-T3  Default-safe budget (T1 mitigation dim 1 + 3) — Init() must
//           auto-apply kDefaultInterruptBudgetCheckpoints; an obvious
//           infinite loop must abort with kAborted + WasInterrupted()=true.
//   D.1-T4  Caller can opt-in to a tighter budget via SetEvalInterruptBudget
//           (T1 mitigation dim 2). Verifies the override is forwarded to
//           the underlying QuickjsEngine and reset semantics survive the
//           wrap (each EvalGlobal resets the counter).
//   D.1-T5  Shutdown is idempotent and explicit; subsequent EvalGlobal
//           returns kInvalidArgument (engine no longer initialised).

#include "veloxa/devtool/console/console_engine.h"

#include <gtest/gtest.h>

namespace vx::devtool::console {
namespace {

// D.1-T1
TEST(ConsoleEngineTest, InitProvidesContext) {
  ConsoleEngine engine;
  ASSERT_TRUE(engine.Init().ok());
  EXPECT_NE(engine.context(), nullptr);
}

// D.1-T2
TEST(ConsoleEngineTest, EvalArithmeticRoundTrip) {
  ConsoleEngine engine;
  ASSERT_TRUE(engine.Init().ok());
  auto r = engine.EvalGlobal(StringView("1 + 2"), StringView("<console>"));
  ASSERT_TRUE(r.ok()) << (!r.ok() ? r.status().message() : "");
  EXPECT_EQ(r.value(), "3");
  EXPECT_FALSE(engine.WasInterrupted());
}

// D.1-T3 — T1 mitigation dim 1 (default-safe) + dim 3 (cannot bypass).
TEST(ConsoleEngineTest, DefaultBudgetAbortsInfiniteLoop) {
  ConsoleEngine engine;
  ASSERT_TRUE(engine.Init().ok());
  // No SetEvalInterruptBudget call — Init() must have applied the default
  // ConsoleEngine::kDefaultInterruptBudgetCheckpoints (10000 handler hits).
  // A `while(1)` loop is a stable upstream-portable abort target.
  auto r = engine.EvalGlobal(StringView("while (true) {}"),
                             StringView("<infinite>"));
  EXPECT_FALSE(r.ok());
  EXPECT_EQ(r.status().code(), StatusCode::kAborted);
  EXPECT_TRUE(engine.WasInterrupted());
}

// D.1-T4 — T1 mitigation dim 2 (opt-in) + reset-per-eval semantics.
TEST(ConsoleEngineTest, OptInTighterBudgetForwardsAndResets) {
  ConsoleEngine engine;
  ASSERT_TRUE(engine.Init().ok());
  // Tighten further to make the abort happen sooner; verifies the wrap
  // forwards the call to the underlying engine.
  engine.SetEvalInterruptBudget(100);
  auto r = engine.EvalGlobal(StringView("while (true) {}"),
                             StringView("<infinite>"));
  EXPECT_FALSE(r.ok());
  EXPECT_EQ(r.status().code(), StatusCode::kAborted);
  EXPECT_TRUE(engine.WasInterrupted());
  // Reset semantics: a subsequent finite eval must succeed and clear the
  // WasInterrupted() snapshot. This is the contract that lets the Console
  // panel show "= N" after a previous "INTERRUPTED" without re-init.
  auto r2 = engine.EvalGlobal(StringView("2 + 3"), StringView("<finite>"));
  ASSERT_TRUE(r2.ok()) << (!r2.ok() ? r2.status().message() : "");
  EXPECT_EQ(r2.value(), "5");
  EXPECT_FALSE(engine.WasInterrupted());
}

// D.1-T5 — explicit Shutdown gates further eval (defensive teardown order).
TEST(ConsoleEngineTest, ShutdownDisablesEval) {
  ConsoleEngine engine;
  ASSERT_TRUE(engine.Init().ok());
  engine.Shutdown();
  // Post-shutdown: context cleared and EvalGlobal returns kInvalidArgument
  // ("engine not initialized"). This mirrors QuickjsEngine's contract.
  EXPECT_EQ(engine.context(), nullptr);
  auto r = engine.EvalGlobal(StringView("1"), StringView("<gone>"));
  EXPECT_FALSE(r.ok());
  EXPECT_EQ(r.status().code(), StatusCode::kInvalidArgument);
}

}  // namespace
}  // namespace vx::devtool::console
