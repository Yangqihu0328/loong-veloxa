#include "veloxa/core/css/transition.h"

#include <gtest/gtest.h>

#include <chrono>

#include "veloxa/core/css/computed_style.h"

namespace vx::css {
namespace {

// --- CubicBezier ---

TEST(CubicBezierTest, LinearSolve) {
  auto b = CubicBezier::Linear();
  EXPECT_NEAR(b.Solve(0.0f), 0.0f, 0.01f);
  EXPECT_NEAR(b.Solve(0.5f), 0.5f, 0.01f);
  EXPECT_NEAR(b.Solve(1.0f), 1.0f, 0.01f);
}

TEST(CubicBezierTest, EaseSolveBoundaries) {
  auto b = CubicBezier::Ease();
  EXPECT_NEAR(b.Solve(0.0f), 0.0f, 0.01f);
  EXPECT_NEAR(b.Solve(1.0f), 1.0f, 0.01f);
  f32 mid = b.Solve(0.5f);
  EXPECT_GT(mid, 0.0f);
  EXPECT_LT(mid, 1.0f);
}

TEST(CubicBezierTest, EaseInSolveBelowLinear) {
  auto b = CubicBezier::EaseIn();
  EXPECT_LT(b.Solve(0.5f), 0.5f);
}

TEST(CubicBezierTest, EaseOutSolveAboveLinear) {
  auto b = CubicBezier::EaseOut();
  EXPECT_GT(b.Solve(0.5f), 0.5f);
}

TEST(CubicBezierTest, FromTimingRoundtrip) {
  auto b = CubicBezier::FromTiming(TimingFunction::kEaseInOut);
  EXPECT_FLOAT_EQ(b.x1, 0.42f);
  EXPECT_FLOAT_EQ(b.y1, 0.0f);
  EXPECT_FLOAT_EQ(b.x2, 0.58f);
  EXPECT_FLOAT_EQ(b.y2, 1.0f);
}

// --- Interpolation ---

TEST(InterpolationTest, F32Lerp) {
  EXPECT_FLOAT_EQ(InterpolateF32(0.0f, 100.0f, 0.0f), 0.0f);
  EXPECT_FLOAT_EQ(InterpolateF32(0.0f, 100.0f, 0.3f), 30.0f);
  EXPECT_FLOAT_EQ(InterpolateF32(0.0f, 100.0f, 1.0f), 100.0f);
}

TEST(InterpolationTest, ColorRedToBlue) {
  // CSS RRGGBBAA format: red=FF0000FF, blue=0000FFFF
  u32 red = 0xFF0000FF;
  u32 blue = 0x0000FFFF;
  u32 mid = InterpolateColor(red, blue, 0.5f);

  u32 r = (mid >> 24) & 0xFF;
  u32 g = (mid >> 16) & 0xFF;
  u32 b = (mid >> 8) & 0xFF;
  u32 a = mid & 0xFF;

  EXPECT_NEAR(r, 127, 2);
  EXPECT_NEAR(g, 0, 2);
  EXPECT_NEAR(b, 127, 2);
  EXPECT_EQ(a, 255);
}

TEST(InterpolationTest, ColorAtBoundaries) {
  u32 c = 0xAABBCCDD;
  EXPECT_EQ(InterpolateColor(c, 0x11223344, 0.0f), c);
  EXPECT_EQ(InterpolateColor(c, 0x11223344, 1.0f), 0x11223344u);
}

TEST(InterpolationTest, LengthSameUnit) {
  auto from = LengthValue::Px(10.0f);
  auto to = LengthValue::Px(50.0f);
  auto mid = InterpolateLength(from, to, 0.5f);
  EXPECT_FLOAT_EQ(mid.value, 30.0f);
  EXPECT_EQ(mid.unit, Unit::kPx);
}

TEST(InterpolationTest, LengthDifferentUnit) {
  auto from = LengthValue::Px(10.0f);
  auto to = LengthValue::Percent(50.0f);
  auto result = InterpolateLength(from, to, 0.5f);
  EXPECT_FLOAT_EQ(result.value, 50.0f);
  EXPECT_EQ(result.unit, Unit::kPercent);
}

// --- IsAnimatable ---

TEST(IsAnimatableTest, AnimatableProperties) {
  EXPECT_TRUE(IsAnimatable(PropertyId::kBackgroundColor));
  EXPECT_TRUE(IsAnimatable(PropertyId::kOpacity));
  EXPECT_TRUE(IsAnimatable(PropertyId::kWidth));
  EXPECT_TRUE(IsAnimatable(PropertyId::kColor));
  EXPECT_TRUE(IsAnimatable(PropertyId::kBorderTopColor));
}

TEST(IsAnimatableTest, NonAnimatableProperties) {
  EXPECT_FALSE(IsAnimatable(PropertyId::kDisplay));
  EXPECT_FALSE(IsAnimatable(PropertyId::kPosition));
  EXPECT_FALSE(IsAnimatable(PropertyId::kOverflow));
  EXPECT_FALSE(IsAnimatable(PropertyId::kVisibility));
  EXPECT_FALSE(IsAnimatable(PropertyId::kTextAlign));
}

// --- InterpolateStyle ---

TEST(InterpolateStyleTest, ColorProperty) {
  ComputedStyle from, to, out;
  from.background_color = 0xFF0000FF;
  to.background_color = 0x0000FFFF;
  out = to;

  InterpolateStyle(from, to, PropertyId::kBackgroundColor, 0.5f, out);
  u32 r = (out.background_color >> 24) & 0xFF;
  u32 b = (out.background_color >> 8) & 0xFF;
  EXPECT_NEAR(r, 127, 2);
  EXPECT_NEAR(b, 127, 2);
}

TEST(InterpolateStyleTest, FloatProperty) {
  ComputedStyle from, to, out;
  from.opacity = 0.0f;
  to.opacity = 1.0f;
  out = to;

  InterpolateStyle(from, to, PropertyId::kOpacity, 0.5f, out);
  EXPECT_NEAR(out.opacity, 0.5f, 0.01f);
}

TEST(InterpolateStyleTest, LengthProperty) {
  ComputedStyle from, to, out;
  from.width = LengthValue::Px(100.0f);
  to.width = LengthValue::Px(200.0f);
  out = to;

  InterpolateStyle(from, to, PropertyId::kWidth, 0.5f, out);
  EXPECT_NEAR(out.width.value, 150.0f, 0.01f);
  EXPECT_EQ(out.width.unit, Unit::kPx);
}

// --- TransitionManager ---

TEST(TransitionManagerTest, StartOnColorChange) {
  TransitionManager mgr;
  ComputedStyle old_s, new_s;
  old_s.background_color = 0xFF0000FF;
  new_s.background_color = 0x0000FFFF;

  TransitionSpec spec;
  spec.property = PropertyId::kBackgroundColor;
  spec.duration_ms = 300.0f;
  spec.bezier = CubicBezier::Linear();
  new_s.transitions.push_back(spec);

  auto now = SteadyClock::now();
  mgr.OnStyleChange(reinterpret_cast<const dom::Element*>(0x1), old_s, new_s,
                     now);
  EXPECT_TRUE(mgr.HasActive());
}

TEST(TransitionManagerTest, NoStartWhenNoChange) {
  TransitionManager mgr;
  ComputedStyle old_s, new_s;
  old_s.background_color = 0xFF0000FF;
  new_s.background_color = 0xFF0000FF;

  TransitionSpec spec;
  spec.property = PropertyId::kBackgroundColor;
  spec.duration_ms = 300.0f;
  new_s.transitions.push_back(spec);

  mgr.OnStyleChange(reinterpret_cast<const dom::Element*>(0x1), old_s, new_s,
                     SteadyClock::now());
  EXPECT_FALSE(mgr.HasActive());
}

TEST(TransitionManagerTest, TickProgressToCompletion) {
  TransitionManager mgr;
  ComputedStyle old_s, new_s;
  old_s.background_color = 0xFF0000FF;
  new_s.background_color = 0x0000FFFF;

  TransitionSpec spec;
  spec.property = PropertyId::kBackgroundColor;
  spec.duration_ms = 100.0f;
  spec.bezier = CubicBezier::Linear();
  new_s.transitions.push_back(spec);

  auto t0 = SteadyClock::now();
  const auto* el = reinterpret_cast<const dom::Element*>(0x1);
  mgr.OnStyleChange(el, old_s, new_s, t0);
  EXPECT_TRUE(mgr.HasActive());

  auto t1 = t0 + std::chrono::milliseconds(200);
  mgr.Tick(t1);
  EXPECT_FALSE(mgr.HasActive());
}

TEST(TransitionManagerTest, ApplyInterpolatedColor) {
  TransitionManager mgr;
  ComputedStyle old_s, new_s;
  old_s.background_color = 0xFF0000FF;
  new_s.background_color = 0x0000FFFF;

  TransitionSpec spec;
  spec.property = PropertyId::kBackgroundColor;
  spec.duration_ms = 100.0f;
  spec.bezier = CubicBezier::Linear();
  new_s.transitions.push_back(spec);

  auto t0 = SteadyClock::now();
  const auto* el = reinterpret_cast<const dom::Element*>(0x1);
  mgr.OnStyleChange(el, old_s, new_s, t0);

  auto t_mid = t0 + std::chrono::milliseconds(50);
  mgr.Tick(t_mid);

  ComputedStyle applied = new_s;
  mgr.ApplyTo(el, applied);

  u32 r = (applied.background_color >> 24) & 0xFF;
  u32 b = (applied.background_color >> 8) & 0xFF;
  EXPECT_NEAR(r, 127, 15);
  EXPECT_NEAR(b, 127, 15);
}

TEST(TransitionManagerTest, ClearRemovesAll) {
  TransitionManager mgr;
  ComputedStyle old_s, new_s;
  old_s.opacity = 0.0f;
  new_s.opacity = 1.0f;

  TransitionSpec spec;
  spec.property = PropertyId::kOpacity;
  spec.duration_ms = 500.0f;
  spec.bezier = CubicBezier::Linear();
  new_s.transitions.push_back(spec);

  mgr.OnStyleChange(reinterpret_cast<const dom::Element*>(0x2), old_s, new_s,
                     SteadyClock::now());
  EXPECT_TRUE(mgr.HasActive());

  mgr.Clear();
  EXPECT_FALSE(mgr.HasActive());
}

TEST(TransitionManagerTest, AllPropertiesShorthand) {
  TransitionManager mgr;
  ComputedStyle old_s, new_s;
  old_s.background_color = 0xFF0000FF;
  old_s.opacity = 0.0f;
  new_s.background_color = 0x0000FFFF;
  new_s.opacity = 1.0f;

  TransitionSpec spec;
  spec.property = PropertyId::kUnknown;  // "all"
  spec.duration_ms = 100.0f;
  spec.bezier = CubicBezier::Linear();
  new_s.transitions.push_back(spec);

  mgr.OnStyleChange(reinterpret_cast<const dom::Element*>(0x3), old_s, new_s,
                     SteadyClock::now());
  EXPECT_TRUE(mgr.HasActive());
}

}  // namespace
}  // namespace vx::css
