// Tests for vx::gfx::gles::GLESCanvas::DrawImage (G1.9 round 2).
//
// Headless: SDL_VIDEODRIVER=offscreen + Mesa swrast EGL. White background; an
// opaque RGBA image blits to a dst rect, so covered framebuffer pixels read
// back as the image's color. Positive checks use a dual-channel constraint
// (P1#2 from G1.7 reflection) so a blank white pixel never counts as covered.
// Sample points are derived analytically from the dst rect interior (P1#1).

#include "veloxa/graphics/gles/gles_canvas.h"

#include <GLES3/gl3.h>
#include <SDL2/SDL.h>
#include <gtest/gtest.h>

#include "veloxa/graphics/brush.h"
#include "veloxa/graphics/image.h"
#include "veloxa/graphics/types.h"
#include "veloxa/platform/sdl2/sdl2_egl_display.h"
#include "veloxa/platform/sdl2/sdl2_gl_window_surface.h"

namespace vx::gfx::gles {
namespace {

constexpr int kW = 160;
constexpr int kH = 96;

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

// glReadPixels origin is bottom-left; convert from top-left doc Y.
int GlY(int doc_y) { return kH - 1 - doc_y; }

void ReadPixel(int px, int py_doc, uint8_t out[4]) {
  out[0] = out[1] = out[2] = out[3] = 0;
  glReadPixels(px, GlY(py_doc), 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, out);
}

constexpr vx::u32 Rgba(vx::u8 r, vx::u8 g, vx::u8 b, vx::u8 a) {
  return static_cast<vx::u32>(r) | (static_cast<vx::u32>(g) << 8) |
         (static_cast<vx::u32>(b) << 16) | (static_cast<vx::u32>(a) << 24);
}

// Count pixels in doc-space rect that read strongly red (R>200, G/B<60).
int CountRed(int x0, int y0, int x1, int y1) {
  int n = 0;
  for (int y = y0; y < y1; ++y)
    for (int x = x0; x < x1; ++x) {
      uint8_t p[4];
      ReadPixel(x, y, p);
      if (p[0] > 200u && p[1] < 60u && p[2] < 60u) ++n;
    }
  return n;
}

int CountBlue(int x0, int y0, int x1, int y1) {
  int n = 0;
  for (int y = y0; y < y1; ++y)
    for (int x = x0; x < x1; ++x) {
      uint8_t p[4];
      ReadPixel(x, y, p);
      if (p[2] > 200u && p[0] < 60u && p[1] < 60u) ++n;
    }
  return n;
}

// "Purple" = both red and blue channels at moderate intensity, i.e. evidence
// that a red and a blue texel were blended (only happens under LINEAR
// filtering at the red|blue seam). NEAREST gives a hard edge → zero purple.
int CountPurple(int x0, int y0, int x1, int y1) {
  int n = 0;
  for (int y = y0; y < y1; ++y)
    for (int x = x0; x < x1; ++x) {
      uint8_t p[4];
      ReadPixel(x, y, p);
      if (p[0] >= 60u && p[0] <= 200u && p[2] >= 60u && p[2] <= 200u) ++n;
    }
  return n;
}

class GlesImageTest : public ::testing::Test {
 protected:
  void SetUp() override {
    surface_ = std::make_unique<vx::platform::Sdl2GLWindowSurface>(kW, kH,
                                                                   "vx_g19_img");
    ASSERT_TRUE(surface_->valid());
    ASSERT_TRUE(surface_->gles_display()->MakeCurrent());
  }

  static Image SolidImage(vx::u32 w, vx::u32 h, vx::u32 rgba) {
    Image img(w, h);
    vx::u32* px = img.pixels();
    for (vx::u32 i = 0; i < w * h; ++i) px[i] = rgba;
    return img;
  }

  // 2x1 image: texel0 = red, texel1 = blue. Drawn scaled up, the red|blue seam
  // blends to purple under LINEAR but stays a hard edge under NEAREST.
  static Image SeamImage() {
    Image img(2, 1);
    vx::u32* px = img.pixels();
    px[0] = Rgba(255, 0, 0, 255);
    px[1] = Rgba(0, 0, 255, 255);
    return img;
  }

  std::unique_ptr<vx::platform::Sdl2GLWindowSurface> surface_;
};

TEST_F(GlesImageTest, DrawImage_RendersOpaqueColor) {
  Image img = SolidImage(8, 8, Rgba(255, 0, 0, 255));
  GLESCanvas canvas(surface_.get());
  canvas.Begin();
  canvas.Clear(Color{255, 255, 255, 255});
  canvas.DrawImage(img, Rect{0, 0, 8, 8}, Rect{20, 20, 40, 40});
  canvas.End();
  // dst interior [20,60)×[20,60); sample a comfortably-inside band.
  EXPECT_GT(CountRed(24, 24, 56, 56), 100);
}

TEST_F(GlesImageTest, DrawImage_InvalidImageNoDraw) {
  Image empty;
  GLESCanvas canvas(surface_.get());
  canvas.Begin();
  canvas.Clear(Color{255, 255, 255, 255});
  canvas.DrawImage(empty, Rect{0, 0, 8, 8}, Rect{20, 20, 40, 40});
  canvas.End();
  EXPECT_EQ(CountRed(0, 0, kW, kH), 0);
}

TEST_F(GlesImageTest, DrawImage_EmptySrcRectNoDraw) {
  Image img = SolidImage(8, 8, Rgba(255, 0, 0, 255));
  GLESCanvas canvas(surface_.get());
  canvas.Begin();
  canvas.Clear(Color{255, 255, 255, 255});
  canvas.DrawImage(img, Rect{0, 0, 0, 0}, Rect{20, 20, 40, 40});
  canvas.End();
  EXPECT_EQ(CountRed(0, 0, kW, kH), 0);
}

TEST_F(GlesImageTest, DrawImage_EmptyDstRectNoDraw) {
  Image img = SolidImage(8, 8, Rgba(255, 0, 0, 255));
  GLESCanvas canvas(surface_.get());
  canvas.Begin();
  canvas.Clear(Color{255, 255, 255, 255});
  canvas.DrawImage(img, Rect{0, 0, 8, 8}, Rect{20, 20, 0, 0});
  canvas.End();
  EXPECT_EQ(CountRed(0, 0, kW, kH), 0);
}

TEST_F(GlesImageTest, DrawImage_NoGLError) {
  Image img = SolidImage(16, 16, Rgba(255, 0, 0, 255));
  GLESCanvas canvas(surface_.get());
  canvas.Begin();
  canvas.Clear(Color{255, 255, 255, 255});
  while (glGetError() != GL_NO_ERROR) {}
  canvas.DrawImage(img, Rect{0, 0, 16, 16}, Rect{10, 10, 50, 50});
  canvas.End();
  EXPECT_EQ(glGetError(), static_cast<GLenum>(GL_NO_ERROR));
}

TEST_F(GlesImageTest, DrawImage_AfterSetTransform) {
  Image img = SolidImage(8, 8, Rgba(255, 0, 0, 255));
  GLESCanvas canvas(surface_.get());
  canvas.Begin();
  canvas.Clear(Color{255, 255, 255, 255});
  canvas.SetTransform(Matrix3x2::Translation(70.0f, 0.0f));
  canvas.DrawImage(img, Rect{0, 0, 8, 8}, Rect{10, 20, 40, 40});
  canvas.End();
  // Pre-translate region empty; translated region (x+70) inked.
  EXPECT_EQ(CountRed(12, 24, 48, 56), 0);
  EXPECT_GT(CountRed(82, 24, 118, 56), 100);
}

TEST_F(GlesImageTest, DrawImage_SubRectSampling) {
  // Left half red (x<4), right half blue. src picks the blue half.
  Image img(8, 8);
  vx::u32* px = img.pixels();
  for (vx::u32 y = 0; y < 8; ++y)
    for (vx::u32 x = 0; x < 8; ++x)
      px[y * 8 + x] = (x < 4) ? Rgba(255, 0, 0, 255) : Rgba(0, 0, 255, 255);
  GLESCanvas canvas(surface_.get());
  canvas.Begin();
  canvas.Clear(Color{255, 255, 255, 255});
  canvas.DrawImage(img, Rect{4, 0, 4, 8}, Rect{20, 20, 40, 40});
  canvas.End();
  EXPECT_GT(CountBlue(24, 24, 56, 56), 100);
  EXPECT_EQ(CountRed(24, 24, 56, 56), 0);
}

TEST_F(GlesImageTest, DrawImage_RepeatDrawCacheReuse_NoGLError) {
  Image img = SolidImage(8, 8, Rgba(255, 0, 0, 255));
  GLESCanvas canvas(surface_.get());
  canvas.Begin();
  canvas.Clear(Color{255, 255, 255, 255});
  while (glGetError() != GL_NO_ERROR) {}
  for (int i = 0; i < 5; ++i) {
    canvas.DrawImage(img, Rect{0, 0, 8, 8}, Rect{20, 20, 40, 40});
  }
  canvas.End();
  EXPECT_EQ(glGetError(), static_cast<GLenum>(GL_NO_ERROR));
  EXPECT_GT(CountRed(24, 24, 56, 56), 100);
}

// ---- TASK-20260602-01: image sampling filter (NEAREST / LINEAR) ----

TEST_F(GlesImageTest, SetGetSamplingFilter) {
  GLESCanvas canvas(surface_.get());
  EXPECT_EQ(canvas.image_sampling_filter(), SamplingFilter::kLinear);  // D4
  canvas.SetImageSamplingFilter(SamplingFilter::kNearest);
  EXPECT_EQ(canvas.image_sampling_filter(), SamplingFilter::kNearest);
}

TEST_F(GlesImageTest, DrawImage_DefaultLinear_BlendsAtSeam) {
  Image img = SeamImage();
  GLESCanvas canvas(surface_.get());
  canvas.Begin();
  canvas.Clear(Color{255, 255, 255, 255});
  canvas.DrawImage(img, Rect{0, 0, 2, 1}, Rect{0, 0, 64, 64});
  canvas.End();
  // Default (no setter) must blend → purple present at the seam row.
  EXPECT_GT(CountPurple(0, 32, 64, 33), 0);
}

TEST_F(GlesImageTest, DrawImage_Nearest_HardSeam) {
  Image img = SeamImage();
  GLESCanvas canvas(surface_.get());
  canvas.SetImageSamplingFilter(SamplingFilter::kNearest);
  canvas.Begin();
  canvas.Clear(Color{255, 255, 255, 255});
  canvas.DrawImage(img, Rect{0, 0, 2, 1}, Rect{0, 0, 64, 64});
  canvas.End();
  EXPECT_EQ(CountPurple(0, 32, 64, 33), 0);   // hard edge, no blend
  EXPECT_GT(CountRed(0, 32, 32, 33), 0);
  EXPECT_GT(CountBlue(32, 32, 64, 33), 0);
}

TEST_F(GlesImageTest, DrawImage_Linear_SoftSeam) {
  Image img = SeamImage();
  GLESCanvas canvas(surface_.get());
  canvas.SetImageSamplingFilter(SamplingFilter::kLinear);
  canvas.Begin();
  canvas.Clear(Color{255, 255, 255, 255});
  canvas.DrawImage(img, Rect{0, 0, 2, 1}, Rect{0, 0, 64, 64});
  canvas.End();
  EXPECT_GT(CountPurple(0, 32, 64, 33), 0);
}

TEST_F(GlesImageTest, DrawImage_FilterSwitch_NoGLError) {
  Image img = SeamImage();
  GLESCanvas canvas(surface_.get());
  canvas.Begin();
  canvas.Clear(Color{255, 255, 255, 255});
  while (glGetError() != GL_NO_ERROR) {}
  for (int i = 0; i < 4; ++i) {
    canvas.SetImageSamplingFilter(i % 2 == 0 ? SamplingFilter::kNearest
                                             : SamplingFilter::kLinear);
    canvas.DrawImage(img, Rect{0, 0, 2, 1}, Rect{0, 0, 64, 64});
  }
  canvas.End();
  EXPECT_EQ(glGetError(), static_cast<GLenum>(GL_NO_ERROR));
}

}  // namespace
}  // namespace vx::gfx::gles
