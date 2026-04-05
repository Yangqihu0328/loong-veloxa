#include "veloxa/core/layout/layout_utils.h"

#include <gtest/gtest.h>

namespace vx::layout {
namespace {

class ResolveLengthTest : public ::testing::Test {
 protected:
  LayoutContext ctx_;
  void SetUp() override {
    ctx_.viewport_width = 1024.0f;
    ctx_.viewport_height = 768.0f;
    ctx_.root_font_size = 16.0f;
  }
};

TEST_F(ResolveLengthTest, Px) {
  auto len = css::LengthValue::Px(42.0f);
  EXPECT_FLOAT_EQ(ResolveLength(len, 800.0f, 16.0f, ctx_), 42.0f);
}

TEST_F(ResolveLengthTest, Percent) {
  css::LengthValue len{50.0f, css::Unit::kPercent};
  EXPECT_FLOAT_EQ(ResolveLength(len, 800.0f, 16.0f, ctx_), 400.0f);
}

TEST_F(ResolveLengthTest, Em) {
  css::LengthValue len{2.0f, css::Unit::kEm};
  EXPECT_FLOAT_EQ(ResolveLength(len, 800.0f, 14.0f, ctx_), 28.0f);
}

TEST_F(ResolveLengthTest, Rem) {
  css::LengthValue len{1.5f, css::Unit::kRem};
  EXPECT_FLOAT_EQ(ResolveLength(len, 800.0f, 14.0f, ctx_), 24.0f);
}

TEST_F(ResolveLengthTest, Vw) {
  css::LengthValue len{10.0f, css::Unit::kVw};
  EXPECT_FLOAT_EQ(ResolveLength(len, 800.0f, 16.0f, ctx_), 102.4f);
}

TEST_F(ResolveLengthTest, Vh) {
  css::LengthValue len{25.0f, css::Unit::kVh};
  EXPECT_FLOAT_EQ(ResolveLength(len, 800.0f, 16.0f, ctx_), 192.0f);
}

TEST_F(ResolveLengthTest, Auto) {
  auto len = css::LengthValue::Auto();
  EXPECT_FLOAT_EQ(ResolveLength(len, 800.0f, 16.0f, ctx_), 0.0f);
}

TEST_F(ResolveLengthTest, None) {
  css::LengthValue len;
  EXPECT_EQ(len.unit, css::Unit::kNone);
  EXPECT_FLOAT_EQ(ResolveLength(len, 800.0f, 16.0f, ctx_), 0.0f);
}

TEST_F(ResolveLengthTest, ZeroPercent) {
  css::LengthValue len{0.0f, css::Unit::kPercent};
  EXPECT_FLOAT_EQ(ResolveLength(len, 800.0f, 16.0f, ctx_), 0.0f);
}

TEST_F(ResolveLengthTest, HundredPercent) {
  css::LengthValue len{100.0f, css::Unit::kPercent};
  EXPECT_FLOAT_EQ(ResolveLength(len, 800.0f, 16.0f, ctx_), 800.0f);
}

class ResolveBoxModelTest : public ::testing::Test {
 protected:
  LayoutContext ctx_;
  f32 pad_[4] = {};
  f32 brd_[4] = {};
  f32 mrg_[4] = {};

  void SetUp() override {
    ctx_.viewport_width = 1024.0f;
    ctx_.viewport_height = 768.0f;
    ctx_.root_font_size = 16.0f;
  }
};

TEST_F(ResolveBoxModelTest, ContentBoxWithPadding) {
  css::ComputedStyle style;
  style.padding_top = css::LengthValue::Px(10.0f);
  style.padding_right = css::LengthValue::Px(20.0f);
  style.padding_bottom = css::LengthValue::Px(10.0f);
  style.padding_left = css::LengthValue::Px(20.0f);

  ResolveBoxModel(style, 800.0f, 16.0f, ctx_, pad_, brd_, mrg_);

  EXPECT_FLOAT_EQ(pad_[0], 10.0f);
  EXPECT_FLOAT_EQ(pad_[1], 20.0f);
  EXPECT_FLOAT_EQ(pad_[2], 10.0f);
  EXPECT_FLOAT_EQ(pad_[3], 20.0f);
}

TEST_F(ResolveBoxModelTest, BorderRequiresStyle) {
  css::ComputedStyle style;
  style.border_width[0] = css::LengthValue::Px(5.0f);
  style.border_width[1] = css::LengthValue::Px(5.0f);
  style.border_width[2] = css::LengthValue::Px(5.0f);
  style.border_width[3] = css::LengthValue::Px(5.0f);
  // border_style defaults to kNone → border widths should be 0

  ResolveBoxModel(style, 800.0f, 16.0f, ctx_, pad_, brd_, mrg_);

  for (int i = 0; i < 4; ++i) {
    EXPECT_FLOAT_EQ(brd_[i], 0.0f);
  }
}

TEST_F(ResolveBoxModelTest, BorderWithSolidStyle) {
  css::ComputedStyle style;
  for (int i = 0; i < 4; ++i) {
    style.border_width[i] = css::LengthValue::Px(3.0f);
    style.border_style[i] = css::BorderStyle::kSolid;
  }

  ResolveBoxModel(style, 800.0f, 16.0f, ctx_, pad_, brd_, mrg_);

  for (int i = 0; i < 4; ++i) {
    EXPECT_FLOAT_EQ(brd_[i], 3.0f);
  }
}

TEST_F(ResolveBoxModelTest, MarginPx) {
  css::ComputedStyle style;
  style.margin_top = css::LengthValue::Px(15.0f);
  style.margin_right = css::LengthValue::Px(25.0f);
  style.margin_bottom = css::LengthValue::Px(15.0f);
  style.margin_left = css::LengthValue::Px(25.0f);

  ResolveBoxModel(style, 800.0f, 16.0f, ctx_, pad_, brd_, mrg_);

  EXPECT_FLOAT_EQ(mrg_[0], 15.0f);
  EXPECT_FLOAT_EQ(mrg_[1], 25.0f);
  EXPECT_FLOAT_EQ(mrg_[2], 15.0f);
  EXPECT_FLOAT_EQ(mrg_[3], 25.0f);
}

TEST_F(ResolveBoxModelTest, PaddingPercent) {
  css::ComputedStyle style;
  style.padding_top = {10.0f, css::Unit::kPercent};
  style.padding_right = {5.0f, css::Unit::kPercent};

  ResolveBoxModel(style, 1000.0f, 16.0f, ctx_, pad_, brd_, mrg_);

  EXPECT_FLOAT_EQ(pad_[0], 100.0f);
  EXPECT_FLOAT_EQ(pad_[1], 50.0f);
}

TEST_F(ResolveBoxModelTest, AllZeroDefaults) {
  css::ComputedStyle style;
  ResolveBoxModel(style, 800.0f, 16.0f, ctx_, pad_, brd_, mrg_);

  for (int i = 0; i < 4; ++i) {
    EXPECT_FLOAT_EQ(pad_[i], 0.0f);
    EXPECT_FLOAT_EQ(brd_[i], 0.0f);
    EXPECT_FLOAT_EQ(mrg_[i], 0.0f);
  }
}

}  // namespace
}  // namespace vx::layout
