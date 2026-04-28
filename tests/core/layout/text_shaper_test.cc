// R4 #21（TASK-20260426-01）—— SimpleTextShaper TextMetrics ABI 扩展验证。
//
// 验证 R4.3 给 TextMetrics 加 ascent/descent 字段后：
//   1. ascent > 0（baseline 之上）
//   2. descent > 0（baseline 之下；CSS 2.1 §10.8.1 约定为正数）
//   3. baseline 兼容字段 == ascent（同语义；deprecated 但仍写入）

#include <gtest/gtest.h>

#include "veloxa/core/layout/text_shaper.h"

namespace vx::layout {
namespace {

TEST(SimpleTextShaperTest, AscentIsPositive) {
  SimpleTextShaper shaper;
  auto m = shaper.Measure("Hello", 16.0f, 400);
  EXPECT_GT(m.ascent, 0.0f);
  // SimpleTextShaper 用经验比例 ascent = font_size × 0.8。
  EXPECT_FLOAT_EQ(m.ascent, 16.0f * 0.8f);
}

TEST(SimpleTextShaperTest, DescentIsPositive) {
  SimpleTextShaper shaper;
  auto m = shaper.Measure("World", 20.0f, 400);
  EXPECT_GT(m.descent, 0.0f);
  // SimpleTextShaper 用经验比例 descent = font_size × 0.2。
  EXPECT_FLOAT_EQ(m.descent, 20.0f * 0.2f);
  // height 必须 = ascent + descent（CSS 2.1 §10.8.1 line-box 公式）。
  EXPECT_FLOAT_EQ(m.height, m.ascent + m.descent);
}

TEST(SimpleTextShaperTest, BaselineCompatEqualsAscent) {
  SimpleTextShaper shaper;
  auto m = shaper.Measure("Compat", 12.0f, 400);
  // baseline 字段已 deprecated 但仍兼容；语义同 ascent。
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  EXPECT_FLOAT_EQ(m.baseline, m.ascent);
#pragma GCC diagnostic pop
}

}  // namespace
}  // namespace vx::layout
