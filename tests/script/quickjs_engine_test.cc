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

}  // namespace
}  // namespace script
}  // namespace vx
