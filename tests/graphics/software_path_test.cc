#include "veloxa/graphics/software/software_path.h"

#include <gtest/gtest.h>

namespace vx::gfx::sw {
namespace {

TEST(SoftwarePathTest, IsEmptyOnNewPath) {
  SoftwarePath path;
  EXPECT_TRUE(path.IsEmpty());
  EXPECT_EQ(path.commands().size(), 0u);
}

TEST(SoftwarePathTest, MoveToLineToTriangleBounds) {
  SoftwarePath path;
  path.MoveTo({0.0f, 0.0f});
  path.LineTo({10.0f, 0.0f});
  path.LineTo({5.0f, 10.0f});
  path.Close();

  EXPECT_FALSE(path.IsEmpty());

  auto bounds = path.Bounds();
  EXPECT_FLOAT_EQ(bounds.x, 0.0f);
  EXPECT_FLOAT_EQ(bounds.y, 0.0f);
  EXPECT_FLOAT_EQ(bounds.w, 10.0f);
  EXPECT_FLOAT_EQ(bounds.h, 10.0f);
}

TEST(SoftwarePathTest, CloseConnectsBackToMoveTo) {
  SoftwarePath path;
  path.MoveTo({0.0f, 0.0f});
  path.LineTo({10.0f, 0.0f});
  path.LineTo({10.0f, 10.0f});
  path.Close();

  EXPECT_EQ(path.commands().size(), 4u);
  EXPECT_EQ(path.commands()[3].type, SoftwarePath::CommandType::kClose);
}

TEST(SoftwarePathTest, ContainsPointInsideTriangle) {
  SoftwarePath path;
  path.MoveTo({0.0f, 0.0f});
  path.LineTo({20.0f, 0.0f});
  path.LineTo({10.0f, 20.0f});
  path.Close();

  EXPECT_TRUE(path.Contains({10.0f, 5.0f}));
}

TEST(SoftwarePathTest, ContainsPointOutsideTriangle) {
  SoftwarePath path;
  path.MoveTo({0.0f, 0.0f});
  path.LineTo({20.0f, 0.0f});
  path.LineTo({10.0f, 20.0f});
  path.Close();

  EXPECT_FALSE(path.Contains({-5.0f, -5.0f}));
  EXPECT_FALSE(path.Contains({25.0f, 10.0f}));
}

TEST(SoftwarePathTest, ResetMakesPathEmpty) {
  SoftwarePath path;
  path.MoveTo({0.0f, 0.0f});
  path.LineTo({10.0f, 10.0f});
  EXPECT_FALSE(path.IsEmpty());

  path.Reset();
  EXPECT_TRUE(path.IsEmpty());
  EXPECT_EQ(path.commands().size(), 0u);
}

TEST(SoftwarePathTest, QuadToBoundsIncludesControlPoints) {
  SoftwarePath path;
  path.MoveTo({0.0f, 0.0f});
  path.QuadTo({5.0f, 20.0f}, {10.0f, 0.0f});

  auto bounds = path.Bounds();
  EXPECT_FLOAT_EQ(bounds.x, 0.0f);
  EXPECT_FLOAT_EQ(bounds.y, 0.0f);
  EXPECT_FLOAT_EQ(bounds.w, 10.0f);
  EXPECT_FLOAT_EQ(bounds.h, 20.0f);
}

TEST(SoftwarePathTest, CubicToBoundsIncludesControlPoints) {
  SoftwarePath path;
  path.MoveTo({0.0f, 0.0f});
  path.CubicTo({0.0f, 30.0f}, {20.0f, 30.0f}, {20.0f, 0.0f});

  auto bounds = path.Bounds();
  EXPECT_FLOAT_EQ(bounds.x, 0.0f);
  EXPECT_FLOAT_EQ(bounds.y, 0.0f);
  EXPECT_FLOAT_EQ(bounds.w, 20.0f);
  EXPECT_FLOAT_EQ(bounds.h, 30.0f);
}

TEST(SoftwarePathTest, ArcToBoundsCoversCenterPlusRadius) {
  SoftwarePath path;
  path.MoveTo({10.0f, 10.0f});
  path.ArcTo({10.0f, 10.0f}, 5.0f, 0.0f, 3.14159f);

  auto bounds = path.Bounds();
  EXPECT_LE(bounds.x, 5.0f);
  EXPECT_LE(bounds.y, 5.0f);
  EXPECT_GE(bounds.right(), 15.0f);
  EXPECT_GE(bounds.bottom(), 15.0f);
}

TEST(SoftwarePathTest, MultipleSubpaths) {
  SoftwarePath path;
  path.MoveTo({0.0f, 0.0f});
  path.LineTo({10.0f, 0.0f});
  path.MoveTo({20.0f, 20.0f});
  path.LineTo({30.0f, 30.0f});

  auto bounds = path.Bounds();
  EXPECT_FLOAT_EQ(bounds.x, 0.0f);
  EXPECT_FLOAT_EQ(bounds.y, 0.0f);
  EXPECT_FLOAT_EQ(bounds.w, 30.0f);
  EXPECT_FLOAT_EQ(bounds.h, 30.0f);
}

TEST(SoftwarePathTest, EmptyPathBoundsIsZero) {
  SoftwarePath path;
  auto bounds = path.Bounds();
  EXPECT_FLOAT_EQ(bounds.x, 0.0f);
  EXPECT_FLOAT_EQ(bounds.y, 0.0f);
  EXPECT_FLOAT_EQ(bounds.w, 0.0f);
  EXPECT_FLOAT_EQ(bounds.h, 0.0f);
}

TEST(SoftwarePathTest, ContainsOnSquare) {
  SoftwarePath path;
  path.MoveTo({0.0f, 0.0f});
  path.LineTo({10.0f, 0.0f});
  path.LineTo({10.0f, 10.0f});
  path.LineTo({0.0f, 10.0f});
  path.Close();

  EXPECT_TRUE(path.Contains({5.0f, 5.0f}));
  EXPECT_FALSE(path.Contains({-1.0f, 5.0f}));
  EXPECT_FALSE(path.Contains({11.0f, 5.0f}));
}

}  // namespace
}  // namespace vx::gfx::sw
