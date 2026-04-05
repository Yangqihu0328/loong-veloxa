#include "veloxa/foundation/base/assert.h"

#include <gtest/gtest.h>

namespace vx {
namespace {

TEST(AssertTest, CheckPassesOnTrue) {
  VX_CHECK(true);
  VX_CHECK(1 == 1);
  VX_CHECK(42 > 0);
}

TEST(AssertTest, CheckFailsOnFalse) {
  EXPECT_DEATH(VX_CHECK(false), "CHECK failed");
}

TEST(AssertTest, DcheckPassesOnTrue) {
  VX_DCHECK(true);
  VX_DCHECK(1 == 1);
}

TEST(AssertTest, DcheckInDebug) {
#ifndef NDEBUG
  EXPECT_DEATH(VX_DCHECK(false), "CHECK failed");
#endif
}

TEST(AssertTest, CheckWithMessage) {
  EXPECT_DEATH(VX_CHECK(false) << "custom error info", "custom error info");
}

TEST(AssertTest, CheckIncludesExpression) {
  EXPECT_DEATH(VX_CHECK(1 == 2), "1 == 2");
}

}  // namespace
}  // namespace vx
