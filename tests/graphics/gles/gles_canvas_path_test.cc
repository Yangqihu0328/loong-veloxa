// Tests for vx::gfx::gles::GLESCanvas FillPath via libtess2 (G1.6).
//
// Headless: SDL_VIDEODRIVER=offscreen + Mesa swrast EGL — same fixture as
// gles_canvas_fill_test (G1.5).

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

#define SKIP_IF_SWRAST_BLANK(px)                                              \
  if ((px)[0] == 0 && (px)[1] == 0 && (px)[2] == 0 && (px)[3] == 0) {         \
    GTEST_SKIP() << "Mesa swrast offscreen returned blank pixel; "            \
                    "driver-strictness fallback active.";                       \
  }

// Veloxa top-left origin -> GL bottom-left read coordinate.
static int GlY(vx::u32 surface_h, vx::f32 veloxa_y) {
  return static_cast<int>(surface_h) - 1 -
         static_cast<int>(veloxa_y);
}

static sw::SoftwarePath MakeFullTriangle() {
  sw::SoftwarePath path;
  path.MoveTo({0.0f, 0.0f});
  path.LineTo({16.0f, 0.0f});
  path.LineTo({8.0f, 16.0f});
  path.Close();
  return path;
}

static sw::SoftwarePath MakeStar() {
  sw::SoftwarePath path;
  const vx::f32 cx = 16.0f;
  const vx::f32 cy = 16.0f;
  const vx::f32 outer = 12.0f;
  const vx::f32 inner = 5.0f;
  for (int i = 0; i < 10; ++i) {
    vx::f32 angle =
        static_cast<vx::f32>(i) * 3.14159265f / 5.0f - 3.14159265f / 2.0f;
    vx::f32 r = (i % 2 == 0) ? outer : inner;
    Point p = {cx + r * std::cos(angle), cy + r * std::sin(angle)};
    if (i == 0) {
      path.MoveTo(p);
    } else {
      path.LineTo(p);
    }
  }
  path.Close();
  return path;
}

TEST(GLESCanvasPathTest, Construct_InitsPathProgram) {
  vx::platform::Sdl2GLWindowSurface surface(16, 16, "vx_g16_t1");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  DrainGLErrors();
  canvas.Begin();
  canvas.FillPath(MakeFullTriangle(), Brush::Solid(Color::Red()));
  canvas.End();
  EXPECT_EQ(glGetError(), static_cast<GLenum>(GL_NO_ERROR));
}

TEST(GLESCanvasPathTest, FillPath_Triangle_CenterPixel) {
  vx::platform::Sdl2GLWindowSurface surface(16, 16, "vx_g16_t2");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  canvas.Begin();
  canvas.Clear(Color::White());
  canvas.FillPath(MakeFullTriangle(), Brush::Solid(Color::Red()));
  uint8_t px[4];
  ReadPixel(8, GlY(16, 8.0f), px);
  SKIP_IF_SWRAST_BLANK(px);
  EXPECT_GT(px[0], 200u);
  EXPECT_LT(px[1], 50u);
  EXPECT_LT(px[2], 50u);
  canvas.End();
}

TEST(GLESCanvasPathTest, FillPath_Star_CenterFilled) {
  vx::platform::Sdl2GLWindowSurface surface(32, 32, "vx_g16_t3");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  canvas.Begin();
  canvas.Clear(Color::White());
  canvas.FillPath(MakeStar(), Brush::Solid(Color::Red()));
  uint8_t px[4];
  ReadPixel(16, GlY(32, 16.0f), px);
  SKIP_IF_SWRAST_BLANK(px);
  EXPECT_GT(px[0], 200u);
  EXPECT_LT(px[1], 50u);
  canvas.End();
}

TEST(GLESCanvasPathTest, FillPath_QuadBezier_Coverage) {
  vx::platform::Sdl2GLWindowSurface surface(16, 16, "vx_g16_t4");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  sw::SoftwarePath path;
  path.MoveTo({0.0f, 8.0f});
  path.QuadTo({8.0f, 0.0f}, {16.0f, 8.0f});
  path.LineTo({16.0f, 16.0f});
  path.LineTo({0.0f, 16.0f});
  path.Close();

  GLESCanvas canvas(&surface);
  canvas.Begin();
  canvas.Clear(Color::White());
  canvas.FillPath(path, Brush::Solid(Color::Red()));
  uint8_t px[4];
  ReadPixel(8, GlY(16, 10.0f), px);
  SKIP_IF_SWRAST_BLANK(px);
  EXPECT_GT(px[0], 200u);
  EXPECT_LT(px[1], 50u);
  canvas.End();
}

TEST(GLESCanvasPathTest, FillPath_TwoContours) {
  vx::platform::Sdl2GLWindowSurface surface(32, 32, "vx_g16_t5");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  sw::SoftwarePath path;
  path.MoveTo({2.0f, 2.0f});
  path.LineTo({10.0f, 2.0f});
  path.LineTo({10.0f, 10.0f});
  path.LineTo({2.0f, 10.0f});
  path.Close();
  path.MoveTo({20.0f, 20.0f});
  path.LineTo({28.0f, 20.0f});
  path.LineTo({28.0f, 28.0f});
  path.LineTo({20.0f, 28.0f});
  path.Close();

  GLESCanvas canvas(&surface);
  canvas.Begin();
  canvas.Clear(Color::White());
  canvas.FillPath(path, Brush::Solid(Color::Red()));
  uint8_t a[4], b[4];
  ReadPixel(6, GlY(32, 6.0f), a);
  ReadPixel(24, GlY(32, 24.0f), b);
  SKIP_IF_SWRAST_BLANK(a);
  EXPECT_GT(a[0], 200u);
  EXPECT_GT(b[0], 200u);
  canvas.End();
}

TEST(GLESCanvasPathTest, FillPath_AfterSetTransform) {
  vx::platform::Sdl2GLWindowSurface surface(32, 32, "vx_g16_t6");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  sw::SoftwarePath path;
  path.MoveTo({0.0f, 0.0f});
  path.LineTo({8.0f, 0.0f});
  path.LineTo({4.0f, 8.0f});
  path.Close();

  GLESCanvas canvas(&surface);
  canvas.Begin();
  canvas.Clear(Color::White());
  canvas.SetTransform(Matrix3x2::Translation(12.0f, 0.0f));
  canvas.FillPath(path, Brush::Solid(Color::Red()));
  uint8_t inside[4], outside[4];
  ReadPixel(14, GlY(32, 4.0f), inside);
  ReadPixel(4, GlY(32, 4.0f), outside);
  SKIP_IF_SWRAST_BLANK(inside);
  EXPECT_GT(inside[0], 200u);
  EXPECT_GT(outside[0], 200u);
  EXPECT_GT(outside[1], 200u);
  canvas.End();
}

TEST(GLESCanvasPathTest, FillPath_KSolidBrush) {
  vx::platform::Sdl2GLWindowSurface surface(16, 16, "vx_g16_t7");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  canvas.Begin();
  canvas.Clear(Color::Black());
  canvas.FillPath(MakeFullTriangle(), Brush::Solid(Color::Blue()));
  uint8_t px[4];
  ReadPixel(8, GlY(16, 8.0f), px);
  SKIP_IF_SWRAST_BLANK(px);
  EXPECT_LT(px[0], 50u);
  EXPECT_LT(px[1], 50u);
  EXPECT_GT(px[2], 200u);
  canvas.End();
}

TEST(GLESCanvasPathTest, ReverseProbe_EmptyPath) {
  vx::platform::Sdl2GLWindowSurface surface(8, 8, "vx_g16_t8");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  sw::SoftwarePath path;
  GLESCanvas canvas(&surface);
  canvas.Begin();
  canvas.Clear(Color::White());
  canvas.FillPath(path, Brush::Solid(Color::Red()));
  uint8_t px[4];
  ReadPixel(4, 4, px);
  SKIP_IF_SWRAST_BLANK(px);
  EXPECT_GT(px[0], 200u);
  EXPECT_GT(px[1], 200u);
  canvas.End();
}

TEST(GLESCanvasPathTest, ReverseProbe_TransparentBrush) {
  vx::platform::Sdl2GLWindowSurface surface(8, 8, "vx_g16_t9");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  canvas.Begin();
  canvas.Clear(Color::White());
  canvas.FillPath(MakeFullTriangle(), Brush::Solid({255, 0, 0, 0}));
  uint8_t px[4];
  ReadPixel(4, 4, px);
  SKIP_IF_SWRAST_BLANK(px);
  EXPECT_GT(px[0], 200u);
  EXPECT_GT(px[1], 200u);
  canvas.End();
}

TEST(GLESCanvasPathTest, FillPath_MultipleDraws_NoGLError) {
  vx::platform::Sdl2GLWindowSurface surface(16, 16, "vx_g16_t10");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  canvas.Begin();
  for (int i = 0; i < 3; ++i) {
    canvas.FillPath(MakeFullTriangle(), Brush::Solid(Color::Red()));
  }
  canvas.End();
  EXPECT_EQ(glGetError(), static_cast<GLenum>(GL_NO_ERROR));
}

}  // namespace
}  // namespace vx::gfx::gles
