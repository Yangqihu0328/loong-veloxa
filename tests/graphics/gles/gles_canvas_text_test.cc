// Tests for vx::gfx::gles::GLESCanvas::DrawText (G1.8 round 2).
//
// Headless: SDL_VIDEODRIVER=offscreen + Mesa swrast EGL. White background,
// red text → covered pixels read as (255, low, low) after alpha blend over
// white. P1#2 (G1.7 reflection): positive checks use the dual-channel
// constraint R>200 && G<60 so a blank white pixel never counts as "covered".
// P1#1: glyph coverage is font-dependent per-pixel, so we scan an
// analytically-bounded region (pen origin + ascender band) rather than
// guessing a single texel.

#include "veloxa/graphics/gles/gles_canvas.h"

#include <GLES3/gl3.h>
#include <SDL2/SDL.h>
#include <gtest/gtest.h>

#include "veloxa/graphics/brush.h"
#include "veloxa/graphics/types.h"
#include "veloxa/text/font_manager.h"
#include "veloxa/text/glyph_cache.h"
#include "veloxa/platform/sdl2/sdl2_egl_display.h"
#include "veloxa/platform/sdl2/sdl2_gl_window_surface.h"

namespace vx::gfx::gles {
namespace {

constexpr int kW = 160;
constexpr int kH = 96;
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

// glReadPixels origin is bottom-left; convert from top-left doc Y.
int GlY(int doc_y) { return kH - 1 - doc_y; }

void ReadPixel(int px, int py_doc, uint8_t out[4]) {
  out[0] = out[1] = out[2] = out[3] = 0;
  glReadPixels(px, GlY(py_doc), 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, out);
}

// True if any pixel in the doc-space rect [x0,x1)×[y0,y1) is strongly covered
// by red text (R>200 && G<60). Also reports the covered-pixel count.
int CountCovered(int x0, int y0, int x1, int y1) {
  int count = 0;
  for (int y = y0; y < y1; ++y) {
    for (int x = x0; x < x1; ++x) {
      uint8_t px[4];
      ReadPixel(x, y, px);
      if (px[0] > 200u && px[1] < 60u) ++count;
    }
  }
  return count;
}

class GlesTextTest : public ::testing::Test {
 protected:
  void SetUp() override {
    surface_ = std::make_unique<vx::platform::Sdl2GLWindowSurface>(kW, kH,
                                                                   "vx_g18_text");
    ASSERT_TRUE(surface_->valid());
    ASSERT_TRUE(surface_->gles_display()->MakeCurrent());
    ASSERT_TRUE(fm_.Init().ok());
    auto r = fm_.LoadFont(vx::StringView(kFontPath), vx::StringView("DejaVu"));
    ASSERT_TRUE(r.ok()) << "Cannot load " << kFontPath;
  }

  Brush RedBrush() {
    Brush b;
    b.type = Brush::Type::kSolid;
    b.solid = Color{255, 0, 0, 255};
    return b;
  }

  std::unique_ptr<vx::platform::Sdl2GLWindowSurface> surface_;
  vx::text::FontManager fm_;
  vx::text::GlyphCache gc_;
};

TEST_F(GlesTextTest, DrawText_RendersCoverage) {
  GLESCanvas canvas(surface_.get(), &fm_, &gc_);
  canvas.Begin();
  canvas.Clear(Color{255, 255, 255, 255});
  canvas.DrawText(vx::StringView("Ag"), Rect{10, 10, 140, 60}, 48.0f,
                  RedBrush());
  canvas.End();
  // Ascender band: text top within ~[10, 70] doc-Y, glyphs start at x=10.
  int covered = CountCovered(8, 8, 90, 75);
  EXPECT_GT(covered, 20) << "expected meaningful red glyph coverage";
}

TEST_F(GlesTextTest, DrawText_EmptyStringNoDraw) {
  GLESCanvas canvas(surface_.get(), &fm_, &gc_);
  canvas.Begin();
  canvas.Clear(Color{255, 255, 255, 255});
  canvas.DrawText(vx::StringView(""), Rect{10, 10, 140, 60}, 48.0f, RedBrush());
  canvas.End();
  EXPECT_EQ(CountCovered(0, 0, kW, kH), 0);
}

TEST_F(GlesTextTest, DrawText_TransparentBrushNoDraw) {
  GLESCanvas canvas(surface_.get(), &fm_, &gc_);
  Brush b = RedBrush();
  b.solid.a = 0;
  canvas.Begin();
  canvas.Clear(Color{255, 255, 255, 255});
  canvas.DrawText(vx::StringView("Ag"), Rect{10, 10, 140, 60}, 48.0f, b);
  canvas.End();
  EXPECT_EQ(CountCovered(0, 0, kW, kH), 0);
}

TEST_F(GlesTextTest, DrawText_NoGLError) {
  GLESCanvas canvas(surface_.get(), &fm_, &gc_);
  canvas.Begin();
  canvas.Clear(Color{255, 255, 255, 255});
  while (glGetError() != GL_NO_ERROR) {}
  canvas.DrawText(vx::StringView("Hello World"), Rect{4, 10, 150, 40}, 20.0f,
                  RedBrush());
  canvas.End();
  EXPECT_EQ(glGetError(), static_cast<GLenum>(GL_NO_ERROR));
}

TEST_F(GlesTextTest, DrawText_AfterSetTransform) {
  GLESCanvas canvas(surface_.get(), &fm_, &gc_);
  canvas.Begin();
  canvas.Clear(Color{255, 255, 255, 255});
  canvas.SetTransform(Matrix3x2::Translation(60.0f, 0.0f));
  canvas.DrawText(vx::StringView("A"), Rect{10, 10, 80, 60}, 48.0f, RedBrush());
  canvas.End();
  // Left region (pre-translate origin) must be empty; translated region has ink.
  EXPECT_EQ(CountCovered(8, 8, 55, 75), 0);
  EXPECT_GT(CountCovered(60, 8, 130, 75), 10);
}

TEST_F(GlesTextTest, DrawText_MultipleDraws_NoGLError) {
  GLESCanvas canvas(surface_.get(), &fm_, &gc_);
  canvas.Begin();
  canvas.Clear(Color{255, 255, 255, 255});
  while (glGetError() != GL_NO_ERROR) {}
  for (int i = 0; i < 5; ++i) {
    canvas.DrawText(vx::StringView("Ag"), Rect{10, 10, 140, 60}, 32.0f,
                    RedBrush());
  }
  canvas.End();
  EXPECT_EQ(glGetError(), static_cast<GLenum>(GL_NO_ERROR));
}

TEST_F(GlesTextTest, ReverseProbe_NoFontManagerNoCrash) {
  GLESCanvas canvas(surface_.get());  // no font_manager / glyph_cache
  canvas.Begin();
  canvas.Clear(Color{255, 255, 255, 255});
  canvas.DrawText(vx::StringView("Ag"), Rect{10, 10, 140, 60}, 48.0f,
                  RedBrush());
  canvas.End();
  EXPECT_EQ(CountCovered(0, 0, kW, kH), 0);
}

}  // namespace
}  // namespace vx::gfx::gles
