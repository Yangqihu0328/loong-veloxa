#include <gtest/gtest.h>

#include <ft2build.h>
#include FT_FREETYPE_H

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

// ----- hb_font_t caching (#48 / TASK-20260418-01) -----

TEST(FontManagerTest, GetHbFontReusesPerHandle) {
  FontManager fm;
  ASSERT_TRUE(fm.Init().ok());
  auto r = fm.LoadFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                       "DejaVuSans");
  ASSERT_TRUE(r.ok());
  FontHandle h = r.value();

  FT_Face face = fm.GetFace(h);
  ASSERT_NE(face, nullptr);
  FT_Set_Pixel_Sizes(face, 0, 16);
  auto* hb1 = fm.GetHbFont(h, 16);
  auto* hb2 = fm.GetHbFont(h, 16);
  EXPECT_NE(hb1, nullptr);
  EXPECT_EQ(hb1, hb2);  // same handle + same size → cached pointer

  fm.Shutdown();
}

TEST(FontManagerTest, GetHbFontHandlesSizeChange) {
  FontManager fm;
  ASSERT_TRUE(fm.Init().ok());
  auto r = fm.LoadFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                       "DejaVuSans");
  ASSERT_TRUE(r.ok());
  FontHandle h = r.value();
  FT_Face face = fm.GetFace(h);
  ASSERT_NE(face, nullptr);

  FT_Set_Pixel_Sizes(face, 0, 12);
  auto* hb1 = fm.GetHbFont(h, 12);

  FT_Set_Pixel_Sizes(face, 0, 24);
  auto* hb2 = fm.GetHbFont(h, 24);

  EXPECT_NE(hb1, nullptr);
  EXPECT_EQ(hb1, hb2);  // pointer stable; internal reconfig via hb_ft_font_changed

  fm.Shutdown();
}

TEST(FontManagerTest, GetHbFontInvalidHandle) {
  FontManager fm;
  ASSERT_TRUE(fm.Init().ok());
  EXPECT_EQ(fm.GetHbFont(kInvalidFont, 16), nullptr);
  EXPECT_EQ(fm.GetHbFont(999, 16), nullptr);
  fm.Shutdown();
}

}  // namespace vx::text
