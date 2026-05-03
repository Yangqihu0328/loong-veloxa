#include "veloxa/script/quickjs_engine.h"

#include <string>

#include "veloxa/foundation/base/status.h"
#include "veloxa/foundation/strings/string_view.h"

#include <gtest/gtest.h>

namespace vx {
namespace script {
namespace {

TEST(QuickjsEngineTest, EvalArithmetic) {
  QuickjsEngine engine;
  ASSERT_TRUE(engine.Init().ok());
  auto r = engine.EvalGlobal("1 + 2", "<test>");
  ASSERT_TRUE(r.ok()) << (!r.ok() ? r.status().message() : "");
  EXPECT_EQ(r.value(), "3");
}

TEST(QuickjsEngineTest, EvalSyntaxError) {
  QuickjsEngine engine;
  ASSERT_TRUE(engine.Init().ok());
  auto r = engine.EvalGlobal("+++", "<test>");
  EXPECT_FALSE(r.ok());
  EXPECT_FALSE(r.status().message().empty());
}

TEST(QuickjsEngineTest, EvalThrownError) {
  QuickjsEngine engine;
  ASSERT_TRUE(engine.Init().ok());
  auto r = engine.EvalGlobal("throw new Error('x')", "<test>");
  EXPECT_FALSE(r.ok());
  EXPECT_NE(r.status().message().find('x'), std::string::npos);
}

TEST(QuickjsEngineSecurityTest, RejectsOversizedSource) {
  QuickjsEngine engine;
  ASSERT_TRUE(engine.Init().ok());
  std::string big(300 * 1024, 'a');
  big += "\n1";
  auto r = engine.EvalGlobal(
      StringView(big.data(), static_cast<usize>(big.size())), "<big>");
  ASSERT_FALSE(r.ok());
  EXPECT_EQ(r.status().code(), StatusCode::kInvalidArgument);
}

// === [TASK-20260503-05] Interrupt budget tests ===
// Source: creative-quickjs-host.md §组件 1 方案 C Phase 2.
// Coverage matrix (D5 A+C combo):
//   (1) D5.C reverse probe: small budget aborts long loop.
//   (2) D5.A forward probe: budget=0 (default) lets short scripts PASS.
//   (3) D8b validation: default budget (10000) does NOT abort legit scripts.
//   (4) per-EvalGlobal counter reset semantics.
//   (5) D1.B WasInterrupted() last-eval flag semantics.
// All tests bound their loops (`if (i < 0) break;`) so even on slowest
// hardware they cannot hang ctest -- the budget exhaustion path triggers
// well before any natural exit.

TEST(QuickjsEngineInterruptTest, AbortsLongLoopWhenBudgetExhausted) {
  QuickjsEngine engine;
  ASSERT_TRUE(engine.Init().ok());
  engine.SetEvalInterruptBudget(100);  // ~10^6 bytecode ceiling.

  // Long-running loop that would not naturally terminate within budget.
  // Body intentionally trivial (no allocation) to keep bytecode/iter low.
  auto r = engine.EvalGlobal(
      "var i = 0; while (true) { i = i + 1; if (i < 0) break; }",
      "<long-loop>");

  ASSERT_FALSE(r.ok());
  EXPECT_EQ(r.status().code(), StatusCode::kAborted);
  EXPECT_NE(r.status().message().find("aborted"), std::string::npos);
  EXPECT_TRUE(engine.WasInterrupted());
}

TEST(QuickjsEngineInterruptTest, BudgetZeroDisablesInterrupt) {
  QuickjsEngine engine;
  ASSERT_TRUE(engine.Init().ok());
  // budget defaults to 0; no SetEvalInterruptBudget call.
  auto r = engine.EvalGlobal("1 + 2 + 3", "<short>");
  ASSERT_TRUE(r.ok()) << (!r.ok() ? r.status().message() : "");
  EXPECT_EQ(r.value(), "6");
  EXPECT_FALSE(engine.WasInterrupted());
}

TEST(QuickjsEngineInterruptTest, DefaultBudgetDoesNotAbortLegitScripts) {
  QuickjsEngine engine;
  ASSERT_TRUE(engine.Init().ok());
  engine.SetEvalInterruptBudget(
      QuickjsEngine::kDefaultInterruptBudgetCheckpoints);

  // Sum 1..1000 = 500500: bounded loop well within ~10^8 bytecode budget.
  auto r = engine.EvalGlobal(
      "var s = 0; for (var i = 1; i <= 1000; i++) s = s + i; s",
      "<bounded>");

  ASSERT_TRUE(r.ok()) << (!r.ok() ? r.status().message() : "");
  EXPECT_EQ(r.value(), "500500");
  EXPECT_FALSE(engine.WasInterrupted());
}

TEST(QuickjsEngineInterruptTest, RepeatedEvalResetsBudget) {
  QuickjsEngine engine;
  ASSERT_TRUE(engine.Init().ok());
  engine.SetEvalInterruptBudget(100);

  // First call: long loop aborts.
  auto r1 = engine.EvalGlobal(
      "var i = 0; while (true) { i = i + 1; if (i < 0) break; }",
      "<long-loop-1>");
  ASSERT_FALSE(r1.ok());
  EXPECT_EQ(r1.status().code(), StatusCode::kAborted);
  EXPECT_TRUE(engine.WasInterrupted());

  // Second call: short script succeeds (counter reset on entry).
  auto r2 = engine.EvalGlobal("40 + 2", "<short>");
  ASSERT_TRUE(r2.ok()) << (!r2.ok() ? r2.status().message() : "");
  EXPECT_EQ(r2.value(), "42");
  EXPECT_FALSE(engine.WasInterrupted());  // Reset on entry.
}

TEST(QuickjsEngineInterruptTest, WasInterruptedTracksLastEval) {
  QuickjsEngine engine;
  ASSERT_TRUE(engine.Init().ok());

  // Initially false.
  EXPECT_FALSE(engine.WasInterrupted());

  // budget=0 short eval: false.
  auto r1 = engine.EvalGlobal("1", "<a>");
  ASSERT_TRUE(r1.ok());
  EXPECT_FALSE(engine.WasInterrupted());

  // budget=100 long loop: true.
  engine.SetEvalInterruptBudget(100);
  auto r2 = engine.EvalGlobal(
      "var i = 0; while (true) { i = i + 1; if (i < 0) break; }",
      "<b>");
  ASSERT_FALSE(r2.ok());
  EXPECT_TRUE(engine.WasInterrupted());

  // Subsequent short eval (budget still 100, but bounded): false (reset).
  auto r3 = engine.EvalGlobal("2", "<c>");
  ASSERT_TRUE(r3.ok());
  EXPECT_FALSE(engine.WasInterrupted());
}

}  // namespace
}  // namespace script
}  // namespace vx
