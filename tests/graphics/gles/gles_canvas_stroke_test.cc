// Tests for vx::gfx::gles::GLESCanvas Stroke* via Fill conversion (G1.7).
//
// Headless: SDL_VIDEODRIVER=offscreen + Mesa swrast EGL — same fixture as
// gles_canvas_fill_test (G1.5) / gles_canvas_path_test (G1.6).

#include "veloxa/graphics/gles/gles_canvas.h"

#include <cmath>
#include <GLES3/gl3.h>
#include <SDL2/SDL.h>
#include <gtest/gtest.h>

#include "veloxa/graphics/brush.h"
#include "veloxa/graphics/software/software_path.h"
#include "veloxa/graphics/types.h"
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

static void DrainGLErrors() {
  while (glGetError() != GL_NO_ERROR) {}
}

static void ReadPixel(int px, int py, uint8_t out[4]) {
  out[0] = out[1] = out[2] = out[3] = 0;
  glReadPixels(px, py, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, out);
}

#define SKIP_IF_SWRAST_BLANK(px)                                     \
  if ((px)[0] == 0 && (px)[1] == 0 && (px)[2] == 0 && (px)[3] == 0) {\
    GTEST_SKIP() << "Mesa swrast offscreen returned blank pixel; "   \
                    "driver-strictness fallback active.";            \
  }

// Veloxa top-left origin -> GL bottom-left read coordinate.
static int GlY(vx::u32 surface_h, vx::f32 veloxa_y) {
  return static_cast<int>(surface_h) - 1 - static_cast<int>(veloxa_y);
}

static sw::SoftwarePath MakeTriangle() {
  sw::SoftwarePath path;
  path.MoveTo({4.0f, 4.0f});
  path.LineTo({28.0f, 4.0f});
  path.LineTo({16.0f, 28.0f});
  path.Close();
  return path;
}

static sw::SoftwarePath MakeStar() {
  sw::SoftwarePath path;
  const vx::f32 cx = 16.0f, cy = 16.0f, outer = 12.0f, inner = 5.0f;
  for (int i = 0; i < 10; ++i) {
    vx::f32 a =
        static_cast<vx::f32>(i) * 3.14159265f / 5.0f - 3.14159265f / 2.0f;
    vx::f32 r = (i % 2 == 0) ? outer : inner;
    Point p = {cx + r * std::cos(a), cy + r * std::sin(a)};
    (i == 0) ? path.MoveTo(p) : path.LineTo(p);
  }
  path.Close();
  return path;
}

TEST(GLESCanvasStrokeTest, StrokeRect_BorderPixel) {
  vx::platform::Sdl2GLWindowSurface surface(32, 32, "vx_g17_t1");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  canvas.Begin();
  canvas.Clear(Color::White());
  canvas.StrokeRect({4, 4, 24, 24}, Brush::Solid(Color::Red()), 4.0f);
  uint8_t border[4], inside[4];
  ReadPixel(6, GlY(32, 6.0f), border);   // on top border
  ReadPixel(16, GlY(32, 16.0f), inside); // hollow center
  SKIP_IF_SWRAST_BLANK(border);
  EXPECT_GT(border[0], 200u);
  EXPECT_LT(border[1], 50u);
  EXPECT_GT(inside[1], 200u);  // center stays white
  canvas.End();
}

TEST(GLESCanvasStrokeTest, StrokeLine_Diagonal) {
  vx::platform::Sdl2GLWindowSurface surface(32, 32, "vx_g17_t2");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  canvas.Begin();
  canvas.Clear(Color::White());
  canvas.StrokeLine({4, 4}, {28, 28}, Brush::Solid(Color::Red()), 4.0f);
  uint8_t px[4];
  ReadPixel(16, GlY(32, 16.0f), px);
  SKIP_IF_SWRAST_BLANK(px);
  EXPECT_GT(px[0], 200u);
  EXPECT_LT(px[1], 50u);
  canvas.End();
}

TEST(GLESCanvasStrokeTest, StrokeLine_Horizontal) {
  vx::platform::Sdl2GLWindowSurface surface(32, 32, "vx_g17_t3");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  canvas.Begin();
  canvas.Clear(Color::White());
  canvas.StrokeLine({4, 16}, {28, 16}, Brush::Solid(Color::Red()), 6.0f);
  uint8_t on[4], off[4];
  ReadPixel(16, GlY(32, 16.0f), on);
  ReadPixel(16, GlY(32, 2.0f), off);
  SKIP_IF_SWRAST_BLANK(on);
  EXPECT_GT(on[0], 200u);
  EXPECT_LT(on[1], 50u);
  EXPECT_GT(off[1], 200u);  // away from line stays white
  canvas.End();
}

TEST(GLESCanvasStrokeTest, StrokeRoundedRect_Ring) {
  vx::platform::Sdl2GLWindowSurface surface(32, 32, "vx_g17_t4");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  canvas.Begin();
  canvas.Clear(Color::White());
  canvas.StrokeRoundedRect({4, 4, 24, 24}, 8.0f, Brush::Solid(Color::Red()),
                           3.0f);
  uint8_t ring[4], center[4];
  ReadPixel(16, GlY(32, 5.0f), ring);     // top edge of ring
  ReadPixel(16, GlY(32, 16.0f), center);  // hollow center
  SKIP_IF_SWRAST_BLANK(ring);
  EXPECT_GT(ring[0], 200u);
  EXPECT_LT(ring[1], 50u);
  EXPECT_GT(center[1], 200u);  // center white
  canvas.End();
}

TEST(GLESCanvasStrokeTest, StrokeRoundedRect_SmallWidth) {
  vx::platform::Sdl2GLWindowSurface surface(32, 32, "vx_g17_t5");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  canvas.Begin();
  canvas.Clear(Color::White());
  canvas.StrokeRoundedRect({4, 4, 24, 24}, 6.0f, Brush::Solid(Color::Red()),
                           1.0f);
  DrainGLErrors();
  canvas.StrokeRoundedRect({4, 4, 24, 24}, 6.0f, Brush::Solid(Color::Red()),
                           1.0f);
  EXPECT_EQ(glGetError(), static_cast<GLenum>(GL_NO_ERROR));
  canvas.End();
}

TEST(GLESCanvasStrokeTest, StrokePath_TriangleOutline) {
  vx::platform::Sdl2GLWindowSurface surface(32, 32, "vx_g17_t6");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  canvas.Begin();
  canvas.Clear(Color::White());
  canvas.StrokePath(MakeTriangle(), Brush::Solid(Color::Red()), 4.0f);
  uint8_t edge[4];
  ReadPixel(16, GlY(32, 4.0f), edge);  // on top edge
  SKIP_IF_SWRAST_BLANK(edge);
  EXPECT_GT(edge[0], 200u);
  EXPECT_LT(edge[1], 50u);
  canvas.End();
}

TEST(GLESCanvasStrokeTest, StrokePath_StarOutline) {
  vx::platform::Sdl2GLWindowSurface surface(32, 32, "vx_g17_t7");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  canvas.Begin();
  canvas.Clear(Color::White());
  canvas.StrokePath(MakeStar(), Brush::Solid(Color::Red()), 2.0f);
  uint8_t tip[4];
  ReadPixel(16, GlY(32, 5.0f), tip);  // near top outer tip
  SKIP_IF_SWRAST_BLANK(tip);
  EXPECT_GT(tip[0], 200u);
  EXPECT_LT(tip[1], 50u);
  canvas.End();
}

TEST(GLESCanvasStrokeTest, StrokePath_KSolidBrush) {
  vx::platform::Sdl2GLWindowSurface surface(32, 32, "vx_g17_t8");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  canvas.Begin();
  canvas.Clear(Color::Black());
  canvas.StrokePath(MakeTriangle(), Brush::Solid(Color::Blue()), 4.0f);
  uint8_t edge[4];
  ReadPixel(16, GlY(32, 4.0f), edge);
  SKIP_IF_SWRAST_BLANK(edge);
  EXPECT_GT(edge[2], 200u);
  EXPECT_LT(edge[0], 50u);
  canvas.End();
}

TEST(GLESCanvasStrokeTest, StrokeRect_AfterSetTransform) {
  vx::platform::Sdl2GLWindowSurface surface(32, 32, "vx_g17_t9");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  canvas.Begin();
  canvas.Clear(Color::White());
  canvas.SetTransform(Matrix3x2::Translation(8.0f, 8.0f));
  canvas.StrokeRect({0, 0, 16, 16}, Brush::Solid(Color::Red()), 4.0f);
  uint8_t shifted[4];
  ReadPixel(10, GlY(32, 9.0f), shifted);  // translated border
  SKIP_IF_SWRAST_BLANK(shifted);
  EXPECT_GT(shifted[0], 200u);
  EXPECT_LT(shifted[1], 50u);
  canvas.End();
}

TEST(GLESCanvasStrokeTest, StrokePath_AfterSetTransform) {
  vx::platform::Sdl2GLWindowSurface surface(32, 32, "vx_g17_t10");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  sw::SoftwarePath path;
  path.MoveTo({0, 0});
  path.LineTo({16, 0});
  path.LineTo({8, 16});
  path.Close();

  GLESCanvas canvas(&surface);
  canvas.Begin();
  canvas.Clear(Color::White());
  canvas.SetTransform(Matrix3x2::Translation(8.0f, 4.0f));
  canvas.StrokePath(path, Brush::Solid(Color::Red()), 4.0f);
  uint8_t edge[4];
  ReadPixel(16, GlY(32, 4.0f), edge);  // translated top edge
  SKIP_IF_SWRAST_BLANK(edge);
  EXPECT_GT(edge[0], 200u);
  EXPECT_LT(edge[1], 50u);
  canvas.End();
}

TEST(GLESCanvasStrokeTest, ReverseProbe_ZeroWidth) {
  vx::platform::Sdl2GLWindowSurface surface(16, 16, "vx_g17_t11");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  canvas.Begin();
  canvas.Clear(Color::White());
  canvas.StrokeRect({2, 2, 12, 12}, Brush::Solid(Color::Red()), 0.0f);
  canvas.StrokeLine({2, 2}, {14, 14}, Brush::Solid(Color::Red()), 0.0f);
  canvas.StrokePath(MakeTriangle(), Brush::Solid(Color::Red()), 0.0f);
  uint8_t px[4];
  ReadPixel(8, 8, px);
  SKIP_IF_SWRAST_BLANK(px);
  EXPECT_GT(px[0], 200u);
  EXPECT_GT(px[1], 200u);
  EXPECT_GT(px[2], 200u);
  canvas.End();
}

TEST(GLESCanvasStrokeTest, ReverseProbe_TransparentBrush) {
  vx::platform::Sdl2GLWindowSurface surface(16, 16, "vx_g17_t12");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  canvas.Begin();
  canvas.Clear(Color::White());
  canvas.StrokeRect({2, 2, 12, 12}, Brush::Solid({255, 0, 0, 0}), 3.0f);
  uint8_t px[4];
  ReadPixel(2, 8, px);
  SKIP_IF_SWRAST_BLANK(px);
  EXPECT_GT(px[0], 200u);
  EXPECT_GT(px[1], 200u);
  EXPECT_GT(px[2], 200u);
  canvas.End();
}

TEST(GLESCanvasStrokeTest, ReverseProbe_ZeroLengthLine) {
  vx::platform::Sdl2GLWindowSurface surface(16, 16, "vx_g17_t13");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  canvas.Begin();
  canvas.Clear(Color::White());
  canvas.StrokeLine({8, 8}, {8, 8}, Brush::Solid(Color::Red()), 4.0f);
  uint8_t px[4];
  ReadPixel(8, 8, px);
  SKIP_IF_SWRAST_BLANK(px);
  EXPECT_GT(px[1], 200u);  // stays white
  canvas.End();
}

TEST(GLESCanvasStrokeTest, StrokeRect_MultipleDraws_NoGLError) {
  vx::platform::Sdl2GLWindowSurface surface(32, 32, "vx_g17_t14");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  canvas.Begin();
  DrainGLErrors();
  for (int i = 0; i < 3; ++i) {
    canvas.StrokeRect({4, 4, 24, 24}, Brush::Solid(Color::Red()), 2.0f);
    canvas.StrokeLine({4, 4}, {28, 28}, Brush::Solid(Color::Red()), 2.0f);
    canvas.StrokePath(MakeTriangle(), Brush::Solid(Color::Red()), 2.0f);
  }
  canvas.End();
  EXPECT_EQ(glGetError(), static_cast<GLenum>(GL_NO_ERROR));
}

}  // namespace
}  // namespace vx::gfx::gles
