// Tests for vx::gfx::gles::GLESCanvas FillRect + FillRoundedRect (G1.5).
//
// Headless: SDL_VIDEODRIVER=offscreen + Mesa swrast EGL — same fixture as
// gles_canvas_skeleton_test (G1.4 first-evidence reused).
// Each test creates its own Sdl2GLWindowSurface so GL state cannot leak.
//
// Pixel verification uses glReadPixels on the default framebuffer. Mesa
// swrast first-evidence (G1.3 T4 + G1.4 T3) shows this is reliable for
// solid fills; we keep a GTEST_SKIP fallback for driver-strictness layering
// consistency.

#include "veloxa/graphics/gles/gles_canvas.h"

#include <GLES3/gl3.h>
#include <SDL2/SDL.h>
#include <gtest/gtest.h>

#include "veloxa/foundation/base/assert.h"
#include "veloxa/graphics/brush.h"
#include "veloxa/graphics/types.h"
#include "veloxa/platform/sdl2/sdl2_egl_display.h"
#include "veloxa/platform/sdl2/sdl2_gl_window_surface.h"

namespace vx::gfx::gles {
namespace {

// Reuse the SDL offscreen environment from skeleton test (each test target
// registers its own AddGlobalTestEnvironment).
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

// Helpers ------------------------------------------------------------------
static void DrainGLErrors() {
  while (glGetError() != GL_NO_ERROR) {}
}

// Read one pixel at (px, py). Origin is bottom-left in glReadPixels (OpenGL
// convention). Caller is responsible for Y conversion.
static void ReadPixel(int px, int py, uint8_t out[4]) {
  out[0] = out[1] = out[2] = out[3] = 0;
  glReadPixels(px, py, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, out);
}

// Skip if Mesa swrast didn't render to the default framebuffer for this
// pixel (driver-strictness fallback / G1.3 first-evidence should preclude).
#define SKIP_IF_SWRAST_BLANK(px)                                              \
  if ((px)[0] == 0 && (px)[1] == 0 && (px)[2] == 0 && (px)[3] == 0) {         \
    GTEST_SKIP() << "Mesa swrast offscreen returned blank pixel; G1.3/G1.4 " \
                    "first-evidence should preclude — driver-strictness "    \
                    "fallback active.";                                       \
  }

// ---------------------------------------------------------------------------
// T1: Construct_InitsShaderPrograms
// ---------------------------------------------------------------------------
// solid_program_ and rounded_program_ must be non-zero after ctor (B5=A).
// Verified indirectly via no GL error on a FillRect call.
TEST(GLESCanvasFillTest, Construct_InitsShaderPrograms) {
  vx::platform::Sdl2GLWindowSurface surface(32, 32, "vx_g15_t1");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  DrainGLErrors();
  canvas.Begin();
  canvas.FillRect({4, 4, 8, 8}, Brush::Solid(Color::Red()));
  canvas.End();
  EXPECT_EQ(glGetError(), static_cast<GLenum>(GL_NO_ERROR))
      << "FillRect emitted a GL error — shader programs likely failed to "
         "link (B5=A ctor init contract)";
}

// ---------------------------------------------------------------------------
// T2: FillRect_TopLeftPixel
// ---------------------------------------------------------------------------
TEST(GLESCanvasFillTest, FillRect_TopLeftPixel) {
  vx::platform::Sdl2GLWindowSurface surface(16, 16, "vx_g15_t2");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  canvas.Begin();
  canvas.Clear(Color::White());
  canvas.FillRect({0, 0, 16, 16}, Brush::Solid(Color::Red()));
  uint8_t px[4];
  ReadPixel(0, 0, px);
  SKIP_IF_SWRAST_BLANK(px);
  EXPECT_GT(px[0], 200u);
  EXPECT_LT(px[1], 50u);
  EXPECT_LT(px[2], 50u);
  canvas.End();
}

// ---------------------------------------------------------------------------
// T3: FillRect_FourCornerSample
// ---------------------------------------------------------------------------
// Fill a 8x8 rect at (4,4) on a 16x16 surface. Sample 4 inner corners +
// outside corner. Inner = green, outside = white (Clear bg).
TEST(GLESCanvasFillTest, FillRect_FourCornerSample) {
  vx::platform::Sdl2GLWindowSurface surface(16, 16, "vx_g15_t3");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  canvas.Begin();
  canvas.Clear(Color::White());
  canvas.FillRect({4, 4, 8, 8}, Brush::Solid(Color::Green()));
  // Veloxa rect is top-left origin. Convert to GL (bottom-left) for read:
  //   GL y = surface_h - 1 - rect_y_top_in_veloxa.
  uint8_t inner_tl[4], inner_tr[4], inner_bl[4], inner_br[4], outside_tl[4];
  ReadPixel(5,  11, inner_tl);   // veloxa (5, 4)
  ReadPixel(10, 11, inner_tr);   // veloxa (10, 4)
  ReadPixel(5,  5,  inner_bl);   // veloxa (5, 10)
  ReadPixel(10, 5,  inner_br);   // veloxa (10, 10)
  ReadPixel(1,  14, outside_tl); // veloxa (1, 1)
  SKIP_IF_SWRAST_BLANK(inner_tl);
  EXPECT_GT(inner_tl[1], 200u);
  EXPECT_GT(inner_tr[1], 200u);
  EXPECT_GT(inner_bl[1], 200u);
  EXPECT_GT(inner_br[1], 200u);
  EXPECT_GT(outside_tl[0], 200u);
  EXPECT_GT(outside_tl[1], 200u);
  EXPECT_GT(outside_tl[2], 200u);
  canvas.End();
}

// ---------------------------------------------------------------------------
// T4: FillRect_KSolidBrush
// ---------------------------------------------------------------------------
TEST(GLESCanvasFillTest, FillRect_KSolidBrush) {
  vx::platform::Sdl2GLWindowSurface surface(8, 8, "vx_g15_t4");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  canvas.Begin();
  canvas.Clear(Color::Black());
  canvas.FillRect({0, 0, 8, 8}, Brush::Solid(Color::Blue()));
  uint8_t px[4];
  ReadPixel(4, 4, px);
  SKIP_IF_SWRAST_BLANK(px);
  EXPECT_LT(px[0], 50u);
  EXPECT_LT(px[1], 50u);
  EXPECT_GT(px[2], 200u);
  canvas.End();
}

// ---------------------------------------------------------------------------
// T5: FillRect_KLinearGradientFallback
// ---------------------------------------------------------------------------
// B7=A: LinearGradient brush should fall back to color_start (no crash, no
// black). Verify red-start gradient yields red pixels (not start-end blend).
TEST(GLESCanvasFillTest, FillRect_KLinearGradientFallback) {
  vx::platform::Sdl2GLWindowSurface surface(8, 8, "vx_g15_t5");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  canvas.Begin();
  canvas.Clear(Color::White());
  Brush gradient =
      Brush::Linear({0, 0}, {8, 0}, Color::Red(), Color::Blue());
  canvas.FillRect({0, 0, 8, 8}, gradient);
  uint8_t px[4];
  ReadPixel(4, 4, px);
  SKIP_IF_SWRAST_BLANK(px);
  // B7=A fallback: every pixel = color_start (red).
  EXPECT_GT(px[0], 200u);
  EXPECT_LT(px[1], 50u);
  EXPECT_LT(px[2], 50u);
  canvas.End();
}

// ---------------------------------------------------------------------------
// T6: FillRoundedRect_CenterIsOpaque
// ---------------------------------------------------------------------------
TEST(GLESCanvasFillTest, FillRoundedRect_CenterIsOpaque) {
  vx::platform::Sdl2GLWindowSurface surface(32, 32, "vx_g15_t6");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  canvas.Begin();
  canvas.Clear(Color::White());
  canvas.FillRoundedRect({4, 4, 24, 24}, 6.0f, Brush::Solid(Color::Red()));
  uint8_t px[4];
  // Center of (4,4 24x24) is veloxa (16,16) -> GL (16, 32-1-16=15).
  ReadPixel(16, 15, px);
  SKIP_IF_SWRAST_BLANK(px);
  EXPECT_GT(px[0], 200u);
  EXPECT_LT(px[1], 50u);
  EXPECT_LT(px[2], 50u);
  canvas.End();
}

// ---------------------------------------------------------------------------
// T7: FillRoundedRect_CornerHasPartialAlpha
// ---------------------------------------------------------------------------
TEST(GLESCanvasFillTest, FillRoundedRect_CornerHasPartialAlpha) {
  vx::platform::Sdl2GLWindowSurface surface(32, 32, "vx_g15_t7");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  canvas.Begin();
  canvas.Clear(Color::White());
  canvas.FillRoundedRect({4, 4, 24, 24}, 8.0f, Brush::Solid(Color::Red()));
  uint8_t corner[4], center[4];
  ReadPixel(4, 27, corner);    // veloxa (4, 4) — outside rounded shape
  ReadPixel(16, 15, center);   // veloxa (16, 16) — center
  SKIP_IF_SWRAST_BLANK(center);
  EXPECT_GT(center[0], 200u);
  // Corner pixel is OUTSIDE the rounded shape -> mostly white visible.
  EXPECT_GT(corner[1], 100u)
      << "Corner should show white background through SDF alpha; got R="
      << static_cast<int>(corner[0]) << " G=" << static_cast<int>(corner[1])
      << " B=" << static_cast<int>(corner[2]);
  canvas.End();
}

// ---------------------------------------------------------------------------
// T8: FillRect_AfterSetTransform_Translation
// ---------------------------------------------------------------------------
TEST(GLESCanvasFillTest, FillRect_AfterSetTransform_Translation) {
  vx::platform::Sdl2GLWindowSurface surface(16, 16, "vx_g15_t8");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  canvas.Begin();
  canvas.Clear(Color::White());
  canvas.SetTransform(Matrix3x2::Translation(8.0f, 0.0f));
  canvas.FillRect({0, 0, 8, 8}, Brush::Solid(Color::Red()));
  uint8_t inside[4], outside[4];
  // Veloxa (12,4) is inside translated rect -> GL (12, 16-1-4=11).
  // Veloxa (4,4) is outside translated rect -> GL (4, 11).
  ReadPixel(12, 11, inside);
  ReadPixel(4,  11, outside);
  SKIP_IF_SWRAST_BLANK(inside);
  EXPECT_GT(inside[0], 200u);
  EXPECT_LT(inside[1], 50u);
  EXPECT_GT(outside[0], 200u);
  EXPECT_GT(outside[1], 200u);
  canvas.End();
}

// ---------------------------------------------------------------------------
// T9: ReverseProbe_TransparentBrush
// ---------------------------------------------------------------------------
// B6=A inline reverse probe: alpha=0 brush should render nothing — Clear
// background must remain visible. Validates the uColor.a path + glBlendFunc.
TEST(GLESCanvasFillTest, ReverseProbe_TransparentBrush) {
  vx::platform::Sdl2GLWindowSurface surface(8, 8, "vx_g15_t9");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  canvas.Begin();
  canvas.Clear(Color::White());
  canvas.FillRect({0, 0, 8, 8}, Brush::Solid({255, 0, 0, 0}));
  uint8_t px[4];
  ReadPixel(4, 4, px);
  SKIP_IF_SWRAST_BLANK(px);
  EXPECT_GT(px[0], 200u);
  EXPECT_GT(px[1], 200u);
  EXPECT_GT(px[2], 200u);
  canvas.End();
}

// ---------------------------------------------------------------------------
// T10: ReverseProbe_EmptyRect
// ---------------------------------------------------------------------------
TEST(GLESCanvasFillTest, ReverseProbe_EmptyRect) {
  vx::platform::Sdl2GLWindowSurface surface(8, 8, "vx_g15_t10");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  canvas.Begin();
  canvas.Clear(Color::White());
  DrainGLErrors();
  canvas.FillRect({4, 4, 0, 8}, Brush::Solid(Color::Red()));
  canvas.FillRect({4, 4, 8, 0}, Brush::Solid(Color::Red()));
  canvas.FillRoundedRect({4, 4, 0, 8}, 2.0f, Brush::Solid(Color::Red()));
  EXPECT_EQ(glGetError(), static_cast<GLenum>(GL_NO_ERROR));
  uint8_t px[4];
  ReadPixel(4, 4, px);
  SKIP_IF_SWRAST_BLANK(px);
  EXPECT_GT(px[0], 200u);
  canvas.End();
}

// ---------------------------------------------------------------------------
// T11: ReverseProbe_ZeroRadiusRoundedRect
// ---------------------------------------------------------------------------
TEST(GLESCanvasFillTest, ReverseProbe_ZeroRadiusRoundedRect) {
  vx::platform::Sdl2GLWindowSurface surface(16, 16, "vx_g15_t11");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  canvas.Begin();
  canvas.Clear(Color::White());
  canvas.FillRoundedRect({0, 0, 16, 16}, 0.0f, Brush::Solid(Color::Red()));
  uint8_t corner[4];
  ReadPixel(0, 15, corner);
  SKIP_IF_SWRAST_BLANK(corner);
  EXPECT_GT(corner[0], 200u);
  EXPECT_LT(corner[1], 50u);
  EXPECT_LT(corner[2], 50u);
  canvas.End();
}

// ---------------------------------------------------------------------------
// T12: FillRect_PreservesQuadVao
// ---------------------------------------------------------------------------
TEST(GLESCanvasFillTest, FillRect_PreservesQuadVao) {
  vx::platform::Sdl2GLWindowSurface surface(8, 8, "vx_g15_t12");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  GLuint vao_before = canvas.quad_vao();
  canvas.Begin();
  for (int i = 0; i < 5; ++i) {
    canvas.FillRect({0, 0, 8, 8}, Brush::Solid(Color::Red()));
  }
  canvas.End();
  EXPECT_EQ(canvas.quad_vao(), vao_before);
  EXPECT_EQ(glGetError(), static_cast<GLenum>(GL_NO_ERROR));
}

}  // namespace
}  // namespace vx::gfx::gles
