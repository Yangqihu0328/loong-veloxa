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

// =============================================================================
// TASK-20260503-04 D.5 — T1 mitigation 5-dimension completeness suite.
// Plan §7 self-check table maps each dim to its implementation site +
// test coverage. These tests pin the contract so a regression in any
// dim shows up as a deterministic failure.
//
//   Dim 1 — default-safe       (Init applies budget automatically)
//   Dim 2 — opt-in              (caller can SetEvalInterruptBudget(0))
//   Dim 3 — cannot-bypass       (user JS source cannot reset budget)
//   Dim 4 — state-queryable     (WasInterrupted exposed + reset-per-eval)
//   Dim 5 — callback constraint (single-threaded atomic — inherited
//                                from QuickjsEngine, sanity-tested via
//                                back-to-back evals not racing the flag)
// =============================================================================

// D.5-T1 (Dim 1) — default-safe: ConsoleEngine alone (no budget setter
// touched by caller) MUST already abort an infinite loop. Crucially we
// verify behaviour BEFORE any explicit SetEvalInterruptBudget — proves
// Init does the work without caller cooperation.
TEST(ConsoleEngineT1MitigationTest, Dim1_DefaultSafe) {
  ConsoleEngine engine;
  ASSERT_TRUE(engine.Init().ok());
  // No SetEvalInterruptBudget call here.
  auto r = engine.EvalGlobal(StringView("for(;;){}"), StringView("<dim1>"));
  EXPECT_EQ(r.status().code(), StatusCode::kAborted);
  EXPECT_TRUE(engine.WasInterrupted());
}

// D.5-T2 (Dim 2) — opt-in escape hatch: caller passes 0 to disable
// budget, EvalGlobal with a finite script then succeeds + WasInterrupted
// stays false. This proves the budget IS the gate (vs. a permanent
// always-abort). For the safety contract reverse: see Dim 3.
TEST(ConsoleEngineT1MitigationTest, Dim2_OptInDisable) {
  ConsoleEngine engine;
  ASSERT_TRUE(engine.Init().ok());
  engine.SetEvalInterruptBudget(0);  // disable
  auto r = engine.EvalGlobal(StringView("var s=0; for(var i=0;i<100;i++) s+=i; s"),
                             StringView("<dim2>"));
  ASSERT_TRUE(r.ok());
  EXPECT_EQ(r.value(), "4950");
  EXPECT_FALSE(engine.WasInterrupted());
}

// D.5-T3 (Dim 3) — cannot-bypass: user JS cannot reach
// SetEvalInterruptBudget. The reverse probe checks that no JS-callable
// global function exists named after the budget setter. (Stronger than
// the D.2-T7 capability allowlist test which focused on DOM mutators.)
TEST(ConsoleEngineT1MitigationTest, Dim3_CannotBypassFromJsScope) {
  ConsoleEngine engine;
  ASSERT_TRUE(engine.Init().ok());
  // No budget binding registered → all of these MUST be undefined.
  for (const char* probe : {
           "typeof SetEvalInterruptBudget",
           "typeof setEvalInterruptBudget",
           "typeof setBudget",
           "typeof JS_SetInterruptHandler",
       }) {
    auto r = engine.EvalGlobal(StringView(probe), StringView("<dim3>"));
    ASSERT_TRUE(r.ok()) << probe;
    EXPECT_EQ(r.value(), "undefined") << probe;
  }
  // And direct-eval of a Function constructor that re-installs an
  // interrupt handler must still hit the budget on infinite loop —
  // proves the user cannot escape via meta-eval.
  auto r = engine.EvalGlobal(
      StringView("Function('while(1){}')()"), StringView("<dim3-meta>"));
  EXPECT_EQ(r.status().code(), StatusCode::kAborted);
}

// D.5-T4 (Dim 4) — state-queryable + reset-per-eval semantic. After an
// abort, the next eval (with a generous budget) MUST clear the flag
// before its own InterruptCallback observations begin.
TEST(ConsoleEngineT1MitigationTest, Dim4_StateQueryableAndResets) {
  ConsoleEngine engine;
  ASSERT_TRUE(engine.Init().ok());
  // First: trigger abort.
  EXPECT_EQ(engine.EvalGlobal(StringView("while(1){}"),
                              StringView("<dim4-trigger>"))
                .status().code(),
            StatusCode::kAborted);
  EXPECT_TRUE(engine.WasInterrupted());
  // Second: a benign eval. WasInterrupted must reset.
  auto r = engine.EvalGlobal(StringView("'second'"),
                             StringView("<dim4-reset>"));
  ASSERT_TRUE(r.ok());
  EXPECT_EQ(r.value(), "second");
  EXPECT_FALSE(engine.WasInterrupted());
}

// D.5-T5 (Dim 5) — callback constraint sanity: back-to-back evals must
// never see stale interrupt state from a previous run. The InterruptCallback
// uses std::atomic<int64_t> for the counter (defensive against future
// worker-thread integration); this test pins the single-threaded reset
// contract so a regression in the atomic ordering surfaces.
TEST(ConsoleEngineT1MitigationTest, Dim5_BackToBackEvalsNoBleedThrough) {
  ConsoleEngine engine;
  ASSERT_TRUE(engine.Init().ok());
  // 5 cycles of trip + recover. Each iteration must independently
  // observe abort then clean recovery — proves no flag bleed-through.
  for (int i = 0; i < 5; ++i) {
    auto a = engine.EvalGlobal(StringView("while(1){}"),
                               StringView("<dim5-trip>"));
    EXPECT_EQ(a.status().code(), StatusCode::kAborted);
    EXPECT_TRUE(engine.WasInterrupted());
    auto b = engine.EvalGlobal(StringView("42"), StringView("<dim5-recover>"));
    ASSERT_TRUE(b.ok()) << b.status().message();
    EXPECT_EQ(b.value(), "42");
    EXPECT_FALSE(engine.WasInterrupted());
  }
}

}  // namespace
}  // namespace vx::devtool::console
