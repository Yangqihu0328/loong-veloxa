#include <gtest/gtest.h>
#include "veloxa/text/freetype_shaper.h"

namespace vx::text {

class FreeTypeShaperTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(fm_.Init().ok());
    auto r = fm_.LoadFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                          "DejaVuSans");
    ASSERT_TRUE(r.ok());
    font_handle_ = r.value();
  }

  void TearDown() override { fm_.Shutdown(); }

  FontManager fm_;
  FontHandle font_handle_ = kInvalidFont;
};

TEST_F(FreeTypeShaperTest, MeasureReturnsPositiveDimensions) {
  FreeTypeTextShaper shaper(&fm_);
  shaper.set_default_font(font_handle_);
  auto metrics = shaper.Measure("Hello", 16.0f, 400);
  EXPECT_GT(metrics.width, 0.0f);
  EXPECT_GT(metrics.height, 0.0f);
  // R4 #21（TASK-20260426-01）：metrics.baseline 已 deprecated（== ascent），
  // 改用 ascent 直接断言；同时验证扩展字段 descent。
  EXPECT_GT(metrics.ascent, 0.0f);
  EXPECT_GT(metrics.descent, 0.0f);
  // ascent + descent == height（FT path）；浮点容差 0.01 px 容纳舍入。
  EXPECT_NEAR(metrics.ascent + metrics.descent, metrics.height, 0.01f);
}

TEST_F(FreeTypeShaperTest, MeasureWidthScalesWithText) {
  FreeTypeTextShaper shaper(&fm_);
  shaper.set_default_font(font_handle_);
  auto m1 = shaper.Measure("Hi", 16.0f, 400);
  auto m2 = shaper.Measure("Hello World", 16.0f, 400);
  EXPECT_GT(m2.width, m1.width);
}

TEST_F(FreeTypeShaperTest, MeasureEmptyText) {
  FreeTypeTextShaper shaper(&fm_);
  shaper.set_default_font(font_handle_);
  auto metrics = shaper.Measure("", 16.0f, 400);
  EXPECT_FLOAT_EQ(metrics.width, 0.0f);
}

TEST_F(FreeTypeShaperTest, MeasureHeightScalesWithFontSize) {
  FreeTypeTextShaper shaper(&fm_);
  shaper.set_default_font(font_handle_);
  auto m16 = shaper.Measure("A", 16.0f, 400);
  auto m32 = shaper.Measure("A", 32.0f, 400);
  EXPECT_GT(m32.height, m16.height);
}

TEST_F(FreeTypeShaperTest, FallbackWhenNoFont) {
  FreeTypeTextShaper shaper(&fm_);
  auto metrics = shaper.Measure("Hello", 16.0f, 400);
  EXPECT_GT(metrics.width, 0.0f);
  EXPECT_GT(metrics.height, 0.0f);
}

}  // namespace vx::text
