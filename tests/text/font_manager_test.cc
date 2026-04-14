#include <gtest/gtest.h>
#include "veloxa/text/font_manager.h"

namespace vx::text {

TEST(FontManagerTest, InitShutdown) {
  FontManager fm;
  ASSERT_TRUE(fm.Init().ok());
  EXPECT_TRUE(fm.initialized());
  fm.Shutdown();
  EXPECT_FALSE(fm.initialized());
}

TEST(FontManagerTest, LoadValidFont) {
  FontManager fm;
  ASSERT_TRUE(fm.Init().ok());
  auto result = fm.LoadFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                            "DejaVuSans");
  ASSERT_TRUE(result.ok());
  EXPECT_NE(result.value(), kInvalidFont);
  EXPECT_EQ(fm.font_count(), 1u);
  fm.Shutdown();
}

TEST(FontManagerTest, LoadInvalidPath) {
  FontManager fm;
  ASSERT_TRUE(fm.Init().ok());
  auto result = fm.LoadFont("/nonexistent/font.ttf", "Missing");
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(fm.font_count(), 0u);
  fm.Shutdown();
}

TEST(FontManagerTest, FindFont) {
  FontManager fm;
  ASSERT_TRUE(fm.Init().ok());
  auto r = fm.LoadFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                       "DejaVuSans");
  ASSERT_TRUE(r.ok());
  FontHandle h = fm.FindFont("DejaVuSans");
  EXPECT_EQ(h, r.value());
  EXPECT_EQ(fm.FindFont("NonExistent"), kInvalidFont);
  fm.Shutdown();
}

TEST(FontManagerTest, GetFace) {
  FontManager fm;
  ASSERT_TRUE(fm.Init().ok());
  auto r = fm.LoadFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                       "DejaVuSans");
  ASSERT_TRUE(r.ok());
  EXPECT_NE(fm.GetFace(r.value()), nullptr);
  EXPECT_EQ(fm.GetFace(kInvalidFont), nullptr);
  EXPECT_EQ(fm.GetFace(999), nullptr);
  fm.Shutdown();
}

}  // namespace vx::text
