// Tests for vx::gfx::gles::ImageTexturePool (G1.9 round 1).
//
// Headless: SDL_VIDEODRIVER=offscreen + Mesa swrast EGL. Cache key is the
// Image's pixel pointer + (w,h) validation (D2=B reconcile — Image has no
// handle). Each test owns its surface so GL state cannot leak.

#include "veloxa/graphics/gles/image_texture_pool.h"

#include <GLES3/gl3.h>
#include <SDL2/SDL.h>
#include <gtest/gtest.h>

#include "veloxa/graphics/image.h"
#include "veloxa/platform/sdl2/sdl2_egl_display.h"
#include "veloxa/platform/sdl2/sdl2_gl_window_surface.h"

namespace vx::gfx::gles {
namespace {

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

// RGBA32: byte0 R | byte1 G | byte2 B | byte3 A (systemPatterns §像素格式约定).
constexpr vx::u32 Rgba(vx::u8 r, vx::u8 g, vx::u8 b, vx::u8 a) {
  return static_cast<vx::u32>(r) | (static_cast<vx::u32>(g) << 8) |
         (static_cast<vx::u32>(b) << 16) | (static_cast<vx::u32>(a) << 24);
}

Image MakeImage(vx::u32 w, vx::u32 h, vx::u32 rgba) {
  Image img(w, h);
  vx::u32* px = img.pixels();
  for (vx::u32 i = 0; i < w * h; ++i) px[i] = rgba;
  return img;
}

class ImageTexturePoolTest : public ::testing::Test {
 protected:
  void SetUp() override {
    surface_ = std::make_unique<vx::platform::Sdl2GLWindowSurface>(64, 64,
                                                                   "vx_g19_pool");
    ASSERT_TRUE(surface_->valid());
    ASSERT_TRUE(surface_->gles_display()->MakeCurrent());
  }
  std::unique_ptr<vx::platform::Sdl2GLWindowSurface> surface_;
};

TEST_F(ImageTexturePoolTest, Ctor_Empty) {
  ImageTexturePool pool;
  EXPECT_EQ(pool.size(), 0u);
}

TEST_F(ImageTexturePoolTest, GetOrUpload_ValidTexture) {
  ImageTexturePool pool;
  Image img = MakeImage(8, 8, Rgba(255, 0, 0, 255));
  GLuint tex = pool.GetOrUpload(img);
  EXPECT_NE(tex, 0u);
  EXPECT_EQ(pool.size(), 1u);
}

TEST_F(ImageTexturePoolTest, GetOrUpload_CacheHit) {
  ImageTexturePool pool;
  Image img = MakeImage(8, 8, Rgba(0, 255, 0, 255));
  GLuint t1 = pool.GetOrUpload(img);
  GLuint t2 = pool.GetOrUpload(img);
  EXPECT_EQ(t1, t2);
  EXPECT_EQ(pool.cache_hits(), 1u);
  EXPECT_EQ(pool.size(), 1u);
}

TEST_F(ImageTexturePoolTest, GetOrUpload_CacheMiss) {
  ImageTexturePool pool;
  Image a = MakeImage(8, 8, Rgba(255, 0, 0, 255));
  Image b = MakeImage(8, 8, Rgba(0, 0, 255, 255));
  pool.GetOrUpload(a);
  pool.GetOrUpload(b);
  EXPECT_EQ(pool.cache_misses(), 2u);
  EXPECT_EQ(pool.size(), 2u);
}

TEST_F(ImageTexturePoolTest, GetOrUpload_InvalidZero) {
  ImageTexturePool pool;
  Image empty;  // default ctor: no pixels
  EXPECT_EQ(pool.GetOrUpload(empty), 0u);
  EXPECT_EQ(pool.size(), 0u);
}

TEST_F(ImageTexturePoolTest, GetOrUpload_NoGLError) {
  ImageTexturePool pool;
  while (glGetError() != GL_NO_ERROR) {}
  Image a = MakeImage(16, 16, Rgba(255, 0, 0, 255));
  Image b = MakeImage(32, 8, Rgba(0, 255, 0, 255));
  Image c = MakeImage(4, 4, Rgba(0, 0, 255, 255));
  pool.GetOrUpload(a);
  pool.GetOrUpload(b);
  pool.GetOrUpload(c);
  EXPECT_EQ(glGetError(), static_cast<GLenum>(GL_NO_ERROR));
}

TEST_F(ImageTexturePoolTest, OnContextLost_ClearsCache) {
  ImageTexturePool pool;
  Image img = MakeImage(8, 8, Rgba(255, 0, 0, 255));
  pool.GetOrUpload(img);
  pool.OnContextLost();
  EXPECT_EQ(pool.size(), 0u);
}

TEST_F(ImageTexturePoolTest, OnContextRestored_LazyReupload) {
  ImageTexturePool pool;
  Image img = MakeImage(8, 8, Rgba(255, 0, 0, 255));
  pool.GetOrUpload(img);
  pool.OnContextLost();
  pool.OnContextRestored();
  EXPECT_EQ(pool.size(), 0u);
  vx::u64 misses_before = pool.cache_misses();
  pool.GetOrUpload(img);
  EXPECT_EQ(pool.cache_misses(), misses_before + 1);
  EXPECT_EQ(pool.size(), 1u);
}

}  // namespace
}  // namespace vx::gfx::gles
