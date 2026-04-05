#include "veloxa/graphics/brush.h"
#include "veloxa/graphics/types.h"

#include <cmath>

#include <gtest/gtest.h>

namespace vx::gfx {
namespace {

// --- Color ---

TEST(ColorTest, FromRGBA) {
  constexpr auto c = Color::FromRGBA(10, 20, 30, 40);
  EXPECT_EQ(c.r, 10);
  EXPECT_EQ(c.g, 20);
  EXPECT_EQ(c.b, 30);
  EXPECT_EQ(c.a, 40);
}

TEST(ColorTest, FromRGBADefaultAlpha) {
  constexpr auto c = Color::FromRGBA(10, 20, 30);
  EXPECT_EQ(c.a, 255);
}

TEST(ColorTest, PredefinedColors) {
  constexpr auto t = Color::Transparent();
  EXPECT_EQ(t, (Color{0, 0, 0, 0}));

  constexpr auto b = Color::Black();
  EXPECT_EQ(b, (Color{0, 0, 0, 255}));

  constexpr auto w = Color::White();
  EXPECT_EQ(w, (Color{255, 255, 255, 255}));

  constexpr auto r = Color::Red();
  EXPECT_EQ(r, (Color{255, 0, 0, 255}));

  constexpr auto g = Color::Green();
  EXPECT_EQ(g, (Color{0, 255, 0, 255}));

  constexpr auto bl = Color::Blue();
  EXPECT_EQ(bl, (Color{0, 0, 255, 255}));
}

TEST(ColorTest, Equality) {
  constexpr Color a{1, 2, 3, 4};
  constexpr Color b{1, 2, 3, 4};
  constexpr Color c{1, 2, 3, 5};
  EXPECT_EQ(a, b);
  EXPECT_NE(a, c);
}

TEST(ColorTest, ToRGBA32) {
  constexpr Color c{0xAA, 0xBB, 0xCC, 0xDD};
  constexpr u32 packed = c.ToRGBA32();
  EXPECT_EQ(packed, 0xDDCCBBAA);
}

TEST(ColorTest, FromRGBA32) {
  constexpr u32 packed = 0xDDCCBBAA;
  constexpr auto c = Color::FromRGBA32(packed);
  EXPECT_EQ(c.r, 0xAA);
  EXPECT_EQ(c.g, 0xBB);
  EXPECT_EQ(c.b, 0xCC);
  EXPECT_EQ(c.a, 0xDD);
}

TEST(ColorTest, RGBA32Roundtrip) {
  constexpr Color original{12, 34, 56, 78};
  constexpr auto roundtripped = Color::FromRGBA32(original.ToRGBA32());
  EXPECT_EQ(original, roundtripped);
}

// --- Point ---

TEST(PointTest, Addition) {
  constexpr Point a{1.0f, 2.0f};
  constexpr Point b{3.0f, 4.0f};
  constexpr auto c = a + b;
  EXPECT_FLOAT_EQ(c.x, 4.0f);
  EXPECT_FLOAT_EQ(c.y, 6.0f);
}

TEST(PointTest, Subtraction) {
  constexpr Point a{5.0f, 7.0f};
  constexpr Point b{2.0f, 3.0f};
  constexpr auto c = a - b;
  EXPECT_FLOAT_EQ(c.x, 3.0f);
  EXPECT_FLOAT_EQ(c.y, 4.0f);
}

TEST(PointTest, ScalarMultiply) {
  constexpr Point a{3.0f, 4.0f};
  constexpr auto c = a * 2.0f;
  EXPECT_FLOAT_EQ(c.x, 6.0f);
  EXPECT_FLOAT_EQ(c.y, 8.0f);
}

TEST(PointTest, Equality) {
  constexpr Point a{1.0f, 2.0f};
  constexpr Point b{1.0f, 2.0f};
  constexpr Point c{1.0f, 3.0f};
  EXPECT_EQ(a, b);
  EXPECT_NE(a, c);
}

// --- Rect ---

TEST(RectTest, RightBottom) {
  constexpr Rect r{10.0f, 20.0f, 30.0f, 40.0f};
  EXPECT_FLOAT_EQ(r.right(), 40.0f);
  EXPECT_FLOAT_EQ(r.bottom(), 60.0f);
}

TEST(RectTest, ContainsPoint) {
  constexpr Rect r{0.0f, 0.0f, 10.0f, 10.0f};
  EXPECT_TRUE(r.Contains({5.0f, 5.0f}));
  EXPECT_TRUE(r.Contains({0.0f, 0.0f}));
  EXPECT_FALSE(r.Contains({10.0f, 10.0f}));
  EXPECT_FALSE(r.Contains({-1.0f, 5.0f}));
  EXPECT_FALSE(r.Contains({5.0f, -1.0f}));
}

TEST(RectTest, IsEmpty) {
  EXPECT_TRUE((Rect{0, 0, 0, 10}).IsEmpty());
  EXPECT_TRUE((Rect{0, 0, 10, 0}).IsEmpty());
  EXPECT_TRUE((Rect{0, 0, -1, 10}).IsEmpty());
  EXPECT_FALSE((Rect{0, 0, 1, 1}).IsEmpty());
}

TEST(RectTest, IntersectOverlapping) {
  Rect a{0, 0, 10, 10};
  Rect b{5, 5, 10, 10};
  auto c = a.Intersect(b);
  EXPECT_FLOAT_EQ(c.x, 5.0f);
  EXPECT_FLOAT_EQ(c.y, 5.0f);
  EXPECT_FLOAT_EQ(c.w, 5.0f);
  EXPECT_FLOAT_EQ(c.h, 5.0f);
}

TEST(RectTest, IntersectNonOverlapping) {
  Rect a{0, 0, 5, 5};
  Rect b{10, 10, 5, 5};
  auto c = a.Intersect(b);
  EXPECT_TRUE(c.IsEmpty());
}

TEST(RectTest, IntersectAdjacent) {
  Rect a{0, 0, 5, 5};
  Rect b{5, 0, 5, 5};
  auto c = a.Intersect(b);
  EXPECT_TRUE(c.IsEmpty());
}

TEST(RectTest, Equality) {
  Rect a{1, 2, 3, 4};
  Rect b{1, 2, 3, 4};
  Rect c{1, 2, 3, 5};
  EXPECT_EQ(a, b);
  EXPECT_NE(a, c);
}

// --- Matrix3x2 ---

TEST(Matrix3x2Test, IdentityApply) {
  auto id = Matrix3x2::Identity();
  Point p{3.0f, 7.0f};
  auto result = id.Apply(p);
  EXPECT_FLOAT_EQ(result.x, 3.0f);
  EXPECT_FLOAT_EQ(result.y, 7.0f);
}

TEST(Matrix3x2Test, Translation) {
  auto t = Matrix3x2::Translation(10.0f, 20.0f);
  auto result = t.Apply({1.0f, 2.0f});
  EXPECT_FLOAT_EQ(result.x, 11.0f);
  EXPECT_FLOAT_EQ(result.y, 22.0f);
}

TEST(Matrix3x2Test, Scale) {
  auto s = Matrix3x2::Scale(2.0f, 3.0f);
  auto result = s.Apply({4.0f, 5.0f});
  EXPECT_FLOAT_EQ(result.x, 8.0f);
  EXPECT_FLOAT_EQ(result.y, 15.0f);
}

TEST(Matrix3x2Test, Rotation90) {
  constexpr float kPi = 3.14159265358979323846f;
  auto r = Matrix3x2::Rotation(kPi / 2.0f);
  auto result = r.Apply({1.0f, 0.0f});
  EXPECT_NEAR(result.x, 0.0f, 1e-5f);
  EXPECT_NEAR(result.y, 1.0f, 1e-5f);
}

TEST(Matrix3x2Test, MultiplyComposition) {
  auto t = Matrix3x2::Translation(5.0f, 10.0f);
  auto s = Matrix3x2::Scale(2.0f, 2.0f);
  auto ts = t.Multiply(s);
  auto result = ts.Apply({1.0f, 1.0f});
  EXPECT_FLOAT_EQ(result.x, 7.0f);
  EXPECT_FLOAT_EQ(result.y, 12.0f);
}

// --- Brush ---

TEST(BrushTest, SolidCreation) {
  auto b = Brush::Solid(Color::Red());
  EXPECT_EQ(b.type, Brush::Type::kSolid);
  EXPECT_EQ(b.solid, Color::Red());
}

TEST(BrushTest, SolidSample) {
  auto b = Brush::Solid(Color::Blue());
  EXPECT_EQ(b.Sample({0, 0}), Color::Blue());
  EXPECT_EQ(b.Sample({100, 200}), Color::Blue());
}

TEST(BrushTest, LinearGradientSampleAtStart) {
  auto b = Brush::Linear({0, 0}, {10, 0}, Color::Black(), Color::White());
  auto c = b.Sample({0, 0});
  EXPECT_EQ(c.r, 0);
  EXPECT_EQ(c.g, 0);
  EXPECT_EQ(c.b, 0);
  EXPECT_EQ(c.a, 255);
}

TEST(BrushTest, LinearGradientSampleAtEnd) {
  auto b = Brush::Linear({0, 0}, {10, 0}, Color::Black(), Color::White());
  auto c = b.Sample({10, 0});
  EXPECT_EQ(c.r, 255);
  EXPECT_EQ(c.g, 255);
  EXPECT_EQ(c.b, 255);
  EXPECT_EQ(c.a, 255);
}

TEST(BrushTest, LinearGradientSampleAtMiddle) {
  auto b = Brush::Linear({0, 0}, {10, 0}, Color::Black(), Color::White());
  auto c = b.Sample({5, 0});
  EXPECT_NEAR(c.r, 127, 1);
  EXPECT_NEAR(c.g, 127, 1);
  EXPECT_NEAR(c.b, 127, 1);
}

TEST(BrushTest, LinearGradientClampsBefore) {
  auto b = Brush::Linear({0, 0}, {10, 0}, Color::Black(), Color::White());
  auto c = b.Sample({-5, 0});
  EXPECT_EQ(c, Color::Black());
}

TEST(BrushTest, LinearGradientClampsAfter) {
  auto b = Brush::Linear({0, 0}, {10, 0}, Color::Black(), Color::White());
  auto c = b.Sample({20, 0});
  EXPECT_EQ(c, Color::White());
}

}  // namespace
}  // namespace vx::gfx
