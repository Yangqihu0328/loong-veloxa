// MarginChain 数据结构单元测试 — TASK-20260426-01 R3 / #20
//
// 覆盖 W3C CSS 2.1 §8.3.1 vertical margin collapsing 数学协议：
//   collapsed = max(positives) + min(negatives)
//
// 关键反例（容易写错的 bug）：
//   - 直接累加（max=20, sum=15 但漏 negative 时返回 20）
//   - 负 margin 取 abs（破坏 negative max collapse 语义）
//   - 单方向 max（漏 positive 或 negative）

#include "veloxa/core/layout/margin_collapse.h"

#include <gtest/gtest.h>

namespace vx::layout {
namespace {

// ----------------------------------------------------------------------------
// 基础协议
// ----------------------------------------------------------------------------

TEST(MarginChainTest, EmptyChainIsEmptyAndZero) {
  MarginChain c;
  EXPECT_TRUE(c.IsEmpty());
  EXPECT_FLOAT_EQ(c.Collapsed(), 0.0f);
}

TEST(MarginChainTest, AllPositivesCollapseToMax) {
  MarginChain c;
  c.Add(10.0f);
  c.Add(20.0f);
  c.Add(15.0f);
  EXPECT_FALSE(c.IsEmpty());
  EXPECT_FLOAT_EQ(c.Collapsed(), 20.0f);
}

TEST(MarginChainTest, AllNegativesCollapseToMin) {
  MarginChain c;
  c.Add(-5.0f);
  c.Add(-10.0f);
  c.Add(-3.0f);
  EXPECT_FALSE(c.IsEmpty());
  EXPECT_FLOAT_EQ(c.Collapsed(), -10.0f);
}

TEST(MarginChainTest, MixedPositiveAndNegativeAddTogether) {
  MarginChain c;
  c.Add(10.0f);
  c.Add(-5.0f);
  c.Add(20.0f);
  EXPECT_FLOAT_EQ(c.Collapsed(), 20.0f + (-5.0f));  // = 15
}

TEST(MarginChainTest, ZeroMargin_ConsideredEmpty) {
  MarginChain c;
  c.Add(0.0f);
  EXPECT_TRUE(c.IsEmpty());
  EXPECT_FLOAT_EQ(c.Collapsed(), 0.0f);
}

// ----------------------------------------------------------------------------
// RED 反向探针 — 协议防回归
//
// 目的：若实现把 Collapsed() 改回 `max_positive` 不含 negative，或把 negative
// 取 abs 后参与 max 比较，下面 2 个测试立即 FAIL。
// ----------------------------------------------------------------------------

TEST(MarginChainTest, NegativeOnlyMustNotReturnZero) {
  // 如果实现错把 Collapsed() 写成 max(0, max_positive)，此处会返回 0。
  // 必须返回真实的 min_negative（< 0）。
  MarginChain c;
  c.Add(-7.0f);
  EXPECT_LT(c.Collapsed(), 0.0f);
  EXPECT_FLOAT_EQ(c.Collapsed(), -7.0f);
}

TEST(MarginChainTest, MixedMustNotIgnoreNegative) {
  // 如果实现把 Add(neg) 简单丢弃或对 neg 取 abs，结果会等于 max_positive 而非
  // max(pos)+min(neg)。
  MarginChain c;
  c.Add(20.0f);
  c.Add(-30.0f);
  EXPECT_FLOAT_EQ(c.Collapsed(), 20.0f + (-30.0f));  // = -10，不是 20
  EXPECT_LT(c.Collapsed(), 0.0f);
}

// ----------------------------------------------------------------------------
// 累加幂等 / 顺序无关 — chain 结构性质
// ----------------------------------------------------------------------------

TEST(MarginChainTest, AddOrderDoesNotMatter) {
  MarginChain a, b;
  a.Add(10.0f);
  a.Add(-5.0f);
  a.Add(20.0f);

  b.Add(20.0f);
  b.Add(10.0f);
  b.Add(-5.0f);

  EXPECT_FLOAT_EQ(a.Collapsed(), b.Collapsed());
  EXPECT_FLOAT_EQ(a.max_positive, b.max_positive);
  EXPECT_FLOAT_EQ(a.min_negative, b.min_negative);
}

TEST(MarginChainTest, ReAddingSameValueIsIdempotent) {
  MarginChain c;
  c.Add(15.0f);
  f32 first = c.Collapsed();
  c.Add(15.0f);
  c.Add(15.0f);
  EXPECT_FLOAT_EQ(c.Collapsed(), first);
}

// ----------------------------------------------------------------------------
// Clearance 接口（P3 占位 — TASK-26-02 完整实施）
// ----------------------------------------------------------------------------

TEST(MarginChainTest, ClearanceFlagDefaultFalse) {
  MarginChain c;
  EXPECT_FALSE(c.has_clearance);
}

TEST(MarginChainTest, ApplyClearanceSetsFlagWithoutAffectingValue) {
  MarginChain c;
  c.Add(10.0f);
  f32 before = c.Collapsed();
  c.ApplyClearance(5.0f);
  EXPECT_TRUE(c.has_clearance);
  // Clearance 数值在 R3 范围内不进入 Collapsed()（留 P3）
  EXPECT_FLOAT_EQ(c.Collapsed(), before);
}

}  // namespace
}  // namespace vx::layout
