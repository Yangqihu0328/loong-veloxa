// Tests for vx::gfx::gles::GLESCanvas clip stack (G1.10 / glScissor).
//
// Headless: SDL_VIDEODRIVER=offscreen + Mesa swrast EGL. A white background is
// cleared, then a full-surface red FillRect is issued while a clip is active.
// Only framebuffer pixels inside the current clip read back red; pixels outside
// stay white. Positive checks use a dual-channel constraint (P1#2 from G1.7
// reflection) so a blank white pixel never counts as filled. Sample points are
// derived analytically from the clip rect interior / exterior (P1#1).

#include "veloxa/graphics/gles/gles_canvas.h"

#include <GLES3/gl3.h>
#include <SDL2/SDL.h>
#include <gtest/gtest.h>

#include <memory>

#include "veloxa/graphics/brush.h"
#include "veloxa/graphics/software/software_path.h"
#include "veloxa/graphics/types.h"
#include "veloxa/platform/sdl2/sdl2_egl_display.h"
#include "veloxa/platform/sdl2/sdl2_gl_window_surface.h"

namespace vx::gfx::gles {
namespace {

constexpr int kW = 32;
constexpr int kH = 32;

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

// Strong red: R high, G low (P1#2 dual-channel — a white pixel never passes).
bool IsRed(int x, int y_doc) {
  uint8_t p[4];
  ReadPixel(x, y_doc, p);
  return p[0] > 200u && p[1] < 50u;
}

// White background (not filled): all of R/G/B high.
bool IsWhite(int x, int y_doc) {
  uint8_t p[4];
  ReadPixel(x, y_doc, p);
  return p[0] > 200u && p[1] > 200u && p[2] > 200u;
}

class GlesClipTest : public ::testing::Test {
 protected:
  void SetUp() override {
    surface_ = std::make_unique<vx::platform::Sdl2GLWindowSurface>(kW, kH,
                                                                   "vx_g110_clip");
    ASSERT_TRUE(surface_->valid());
    ASSERT_TRUE(surface_->gles_display()->MakeCurrent());
  }

  std::unique_ptr<vx::platform::Sdl2GLWindowSurface> surface_;
};

// C1: a single clip rect limits a full-surface fill to its interior.
TEST_F(GlesClipTest, PushClipRect_ClipsFill) {
  GLESCanvas canvas(surface_.get());
  canvas.Begin();
  canvas.Clear(Color{255, 255, 255, 255});
  canvas.PushClipRect({8, 8, 16, 16});  // covers doc x[8,24) y[8,24)
  canvas.FillRect({0, 0, kW, kH}, Brush::Solid(Color::Red()));
  canvas.PopClip();
  canvas.End();
  EXPECT_TRUE(IsRed(16, 16)) << "clip interior should be filled";
  EXPECT_TRUE(IsWhite(4, 4)) << "outside clip (x<8) should stay white";
}

// C2: nested clips intersect; only the overlap is filled.
TEST_F(GlesClipTest, NestedClip_Intersection) {
  GLESCanvas canvas(surface_.get());
  canvas.Begin();
  canvas.Clear(Color{255, 255, 255, 255});
  canvas.PushClipRect({0, 0, 20, 20});
  canvas.PushClipRect({10, 10, 20, 20});  // intersect = (10,10,10,10)
  canvas.FillRect({0, 0, kW, kH}, Brush::Solid(Color::Red()));
  canvas.PopClip();
  canvas.PopClip();
  canvas.End();
  EXPECT_TRUE(IsRed(15, 15)) << "intersection center should be filled";
  EXPECT_TRUE(IsWhite(5, 5)) << "below intersection should stay white";
  EXPECT_TRUE(IsWhite(25, 25)) << "above intersection should stay white";
}

// C3: PopClip restores the full surface.
TEST_F(GlesClipTest, PopClip_RestoresFull) {
  GLESCanvas canvas(surface_.get());
  canvas.Begin();
  canvas.Clear(Color{255, 255, 255, 255});
  canvas.PushClipRect({8, 8, 4, 4});
  canvas.PopClip();
  canvas.FillRect({0, 0, kW, kH}, Brush::Solid(Color::Red()));
  canvas.End();
  EXPECT_TRUE(IsRed(4, 4)) << "clip released — corner should be filled";
  EXPECT_TRUE(IsRed(28, 28)) << "clip released — far corner should be filled";
}

// C4: PushClipPath clips by the path's bounding box (D4 AABB approximation).
TEST_F(GlesClipTest, PushClipPath_BoundsApprox) {
  sw::SoftwarePath path;
  path.MoveTo({8, 8});
  path.LineTo({20, 8});
  path.LineTo({20, 20});
  path.LineTo({8, 20});  // bounds = (8,8,12,12) → x[8,20) y[8,20)
  GLESCanvas canvas(surface_.get());
  canvas.Begin();
  canvas.Clear(Color{255, 255, 255, 255});
  canvas.PushClipPath(path);
  canvas.FillRect({0, 0, kW, kH}, Brush::Solid(Color::Red()));
  canvas.PopClip();
  canvas.End();
  EXPECT_TRUE(IsRed(12, 12)) << "inside path bounds should be filled";
  EXPECT_TRUE(IsWhite(28, 28)) << "outside path bounds should stay white";
}

// C5: PopState restores the clip stack depth captured at PushState.
TEST_F(GlesClipTest, PushState_PopState_RestoresClip) {
  GLESCanvas canvas(surface_.get());
  canvas.Begin();
  canvas.Clear(Color{255, 255, 255, 255});
  canvas.PushState();
  canvas.PushClipRect({8, 8, 4, 4});
  canvas.PopState();  // should pop the clip pushed after PushState
  canvas.FillRect({0, 0, kW, kH}, Brush::Solid(Color::Red()));
  canvas.End();
  EXPECT_TRUE(IsRed(28, 28)) << "PopState should have restored full clip";
}

// C6: scissor Y-flip places the clip in the correct half (top vs bottom).
TEST_F(GlesClipTest, ClipYFlip_Position) {
  GLESCanvas canvas(surface_.get());
  canvas.Begin();
  canvas.Clear(Color{255, 255, 255, 255});
  canvas.PushClipRect({0, 0, kW, 16});  // top half, doc y[0,16)
  canvas.FillRect({0, 0, kW, kH}, Brush::Solid(Color::Red()));
  canvas.PopClip();
  canvas.End();
  EXPECT_TRUE(IsRed(8, 8)) << "top half should be filled";
  EXPECT_TRUE(IsWhite(8, 24)) << "bottom half should stay white (Y-flip)";
}

// C7 (reverse probe): two disjoint clips yield an empty region — no fill at all.
TEST_F(GlesClipTest, EmptyIntersection_NoFill) {
  GLESCanvas canvas(surface_.get());
  canvas.Begin();
  canvas.Clear(Color{255, 255, 255, 255});
  canvas.PushClipRect({0, 0, 10, 10});
  canvas.PushClipRect({20, 20, 10, 10});  // disjoint → empty
  canvas.FillRect({0, 0, kW, kH}, Brush::Solid(Color::Red()));
  canvas.PopClip();
  canvas.PopClip();
  canvas.End();
  EXPECT_TRUE(IsWhite(5, 5));
  EXPECT_TRUE(IsWhite(25, 25));
  EXPECT_TRUE(IsWhite(16, 16));
}

// C8 (reverse probe): Begin() resets the clip stack + disables scissor.
TEST_F(GlesClipTest, BeginResetsClip) {
  GLESCanvas canvas(surface_.get());
  canvas.Begin();
  canvas.Clear(Color{255, 255, 255, 255});
  canvas.PushClipRect({8, 8, 4, 4});
  canvas.Begin();  // must clear clip_stack_ + glDisable(GL_SCISSOR_TEST)
  canvas.Clear(Color{255, 255, 255, 255});
  canvas.FillRect({0, 0, kW, kH}, Brush::Solid(Color::Red()));
  canvas.End();
  EXPECT_TRUE(IsRed(28, 28)) << "Begin should have reset the clip";
}

}  // namespace
}  // namespace vx::gfx::gles
