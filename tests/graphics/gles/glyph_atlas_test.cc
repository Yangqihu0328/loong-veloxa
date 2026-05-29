// Tests for vx::gfx::gles::GlyphAtlas (G1.8 round 1).
//
// Headless: SDL_VIDEODRIVER=offscreen + Mesa swrast EGL. Requires system
// font DejaVuSans (same as drawtext_shape_cache_test.cc) — ASSERT fail-fast
// if missing rather than silently degrading.

#include "veloxa/graphics/gles/glyph_atlas.h"

#include <GLES3/gl3.h>
#include <SDL2/SDL.h>
#include <gtest/gtest.h>

#include "veloxa/text/font_manager.h"
#include "veloxa/text/glyph_cache.h"
#include "veloxa/text/shape_cache.h"
#include "veloxa/platform/sdl2/sdl2_egl_display.h"
#include "veloxa/platform/sdl2/sdl2_gl_window_surface.h"

namespace vx::gfx::gles {
namespace {

constexpr const char* kFontPath =
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";

class Sdl2GlSurfaceEnvironment : public ::testing::Environment {
 public:
  void SetUp() override {
    SDL_setenv("SDL_VIDEODRIVER", "offscreen", /*overwrite=*/1);
    ASSERT_EQ(SDL_Init(SDL_INIT_VIDEO), 0) << SDL_GetError();
  }
  void TearDown() override { SDL_Quit(); }
};
[[maybe_unused]] auto* g_sdl_env =
    ::testing::AddGlobalTestEnvironment(new Sdl2GlSurfaceEnvironment);

class GlyphAtlasTest : public ::testing::Test {
 protected:
  void SetUp() override {
    surface_ = std::make_unique<vx::platform::Sdl2GLWindowSurface>(64, 64,
                                                                   "vx_g18_atlas");
    ASSERT_TRUE(surface_->valid());
    ASSERT_TRUE(surface_->gles_display()->MakeCurrent());
    ASSERT_TRUE(fm_.Init().ok()) << "FontManager.Init failed";
    auto r = fm_.LoadFont(vx::StringView(kFontPath), vx::StringView("DejaVu"));
    ASSERT_TRUE(r.ok()) << "Cannot load " << kFontPath;
    font_ = r.value();
  }

  // Resolve a real glyph_id for a single character via the shaper.
  vx::u32 GlyphIdFor(const char* ch, vx::u32 pixel_size) {
    fm_.SetFacePixelSize(font_, pixel_size);
    const vx::text::ShapedRun* run =
        fm_.ShapeOrLookup(font_, pixel_size, vx::StringView(ch));
    if (run == nullptr || run->glyphs.empty()) return 0;
    return run->glyphs[0].glyph_id;
  }

  std::unique_ptr<vx::platform::Sdl2GLWindowSurface> surface_;
  vx::text::FontManager fm_;
  vx::text::GlyphCache gc_;
  vx::text::FontHandle font_ = vx::text::kInvalidFont;
};

TEST_F(GlyphAtlasTest, Ctor_TextureNonZero) {
  GlyphAtlas atlas(&fm_, &gc_);
  EXPECT_NE(atlas.texture_id(), 0u);
  EXPECT_EQ(atlas.atlas_width(), 1024u);
  EXPECT_EQ(atlas.atlas_height(), 1024u);
}

TEST_F(GlyphAtlasTest, GetOrUpload_AsciiValidInfo) {
  GlyphAtlas atlas(&fm_, &gc_);
  vx::u32 gid = GlyphIdFor("A", 32);
  ASSERT_NE(gid, 0u);
  GlyphAtlasInfo info = atlas.GetOrUpload(font_, gid, 32);
  EXPECT_TRUE(info.valid);
  EXPECT_GT(info.width, 0);
  EXPECT_GT(info.height, 0);
  EXPECT_GT(info.advance, 0.0f);
}

TEST_F(GlyphAtlasTest, GetOrUpload_CacheHitCounter) {
  GlyphAtlas atlas(&fm_, &gc_);
  vx::u32 gid = GlyphIdFor("A", 32);
  atlas.GetOrUpload(font_, gid, 32);
  atlas.GetOrUpload(font_, gid, 32);
  EXPECT_EQ(atlas.cache_hits(), 1u);
}

TEST_F(GlyphAtlasTest, GetOrUpload_CacheMissCounter) {
  GlyphAtlas atlas(&fm_, &gc_);
  atlas.GetOrUpload(font_, GlyphIdFor("A", 32), 32);
  atlas.GetOrUpload(font_, GlyphIdFor("B", 32), 32);
  EXPECT_EQ(atlas.cache_misses(), 2u);
}

TEST_F(GlyphAtlasTest, GetOrUpload_SpaceZeroSize) {
  GlyphAtlas atlas(&fm_, &gc_);
  vx::u32 gid = GlyphIdFor(" ", 32);
  ASSERT_NE(gid, 0u);
  GlyphAtlasInfo info = atlas.GetOrUpload(font_, gid, 32);
  EXPECT_TRUE(info.valid);
  EXPECT_EQ(info.width, 0);
  EXPECT_GT(info.advance, 0.0f);
}

TEST_F(GlyphAtlasTest, GetOrUpload_UVInRange) {
  GlyphAtlas atlas(&fm_, &gc_);
  GlyphAtlasInfo info = atlas.GetOrUpload(font_, GlyphIdFor("A", 32), 32);
  ASSERT_TRUE(info.valid);
  EXPECT_GE(info.u0, 0.0f);
  EXPECT_LT(info.u0, info.u1);
  EXPECT_LE(info.u1, 1.0f);
  EXPECT_GE(info.v0, 0.0f);
  EXPECT_LT(info.v0, info.v1);
  EXPECT_LE(info.v1, 1.0f);
}

TEST_F(GlyphAtlasTest, GetOrUpload_MissingGlyphInvalid) {
  GlyphAtlas atlas(&fm_, &gc_);
  // glyph_id far beyond DejaVu's glyph count → FT_Load_Glyph fails.
  GlyphAtlasInfo info = atlas.GetOrUpload(font_, 999999u, 32);
  EXPECT_FALSE(info.valid);
}

TEST_F(GlyphAtlasTest, GetOrUpload_ManyGlyphsNoGLError) {
  GlyphAtlas atlas(&fm_, &gc_);
  while (glGetError() != GL_NO_ERROR) {}
  const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  for (const char* p = chars; *p; ++p) {
    char buf[2] = {*p, 0};
    vx::u32 gid = GlyphIdFor(buf, 48);
    if (gid != 0) atlas.GetOrUpload(font_, gid, 48);
  }
  EXPECT_EQ(glGetError(), static_cast<GLenum>(GL_NO_ERROR));
  EXPECT_GT(atlas.cache_misses(), 10u);
}

TEST_F(GlyphAtlasTest, OnContextLost_TextureZero) {
  GlyphAtlas atlas(&fm_, &gc_);
  atlas.GetOrUpload(font_, GlyphIdFor("A", 32), 32);
  atlas.OnContextLost();
  EXPECT_EQ(atlas.texture_id(), 0u);
}

TEST_F(GlyphAtlasTest, OnContextRestored_TextureNonZeroAndCacheCleared) {
  GlyphAtlas atlas(&fm_, &gc_);
  vx::u32 gid = GlyphIdFor("A", 32);
  atlas.GetOrUpload(font_, gid, 32);
  atlas.OnContextLost();
  atlas.OnContextRestored();
  EXPECT_NE(atlas.texture_id(), 0u);
  // Cache cleared → next call is a miss again.
  vx::u64 misses_before = atlas.cache_misses();
  atlas.GetOrUpload(font_, gid, 32);
  EXPECT_EQ(atlas.cache_misses(), misses_before + 1);
}

}  // namespace
}  // namespace vx::gfx::gles
