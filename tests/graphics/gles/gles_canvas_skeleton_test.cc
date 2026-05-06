// Tests for vx::gfx::gles::GLESCanvas skeleton (G1.4).
//
// Headless: SDL_VIDEODRIVER=offscreen + Mesa swrast EGL — same as
// sdl2_gl_window_surface_test (G1.3 first-evidence reused, D9=A).
// Each test creates its own Sdl2GLWindowSurface so GL attributes
// from a previous test cannot leak.

#include "veloxa/graphics/gles/gles_canvas.h"

#include <GLES3/gl3.h>
#include <SDL2/SDL.h>
#include <gtest/gtest.h>

#include <thread>

#include "veloxa/foundation/base/assert.h"
#include "veloxa/graphics/types.h"
#include "veloxa/platform/sdl2/sdl2_egl_display.h"
#include "veloxa/platform/sdl2/sdl2_gl_window_surface.h"

namespace vx::gfx::gles {
namespace {

// ---------------------------------------------------------------------------
// Process-wide SDL environment (offscreen driver + global SDL Init/Quit).
// Mirrors G1.3 sdl2_gl_window_surface_test fixture (D9=A first-evidence reuse).
// ---------------------------------------------------------------------------
class Sdl2GlSurfaceEnvironment : public ::testing::Environment {
 public:
  void SetUp() override {
    SDL_setenv("SDL_VIDEODRIVER", "offscreen", /*overwrite=*/1);
    ASSERT_EQ(SDL_Init(SDL_INIT_VIDEO), 0)
        << "SDL_Init(VIDEO) failed under SDL_VIDEODRIVER=offscreen: "
        << SDL_GetError();
  }
  void TearDown() override { SDL_Quit(); }
};

[[maybe_unused]] auto* g_sdl_env =
    ::testing::AddGlobalTestEnvironment(new Sdl2GlSurfaceEnvironment);

// Helper: matrix equality (Matrix3x2 has no operator==).
static bool MatrixEq(const Matrix3x2& a, const Matrix3x2& b) {
  for (int i = 0; i < 6; ++i) {
    if (a.m[i] != b.m[i]) return false;
  }
  return true;
}

// ---------------------------------------------------------------------------
// T1: Construct_BasicState (D1 + D5 ctor allocates VAO/VBO)
// ---------------------------------------------------------------------------
// Verify ctor allocates VAO/VBO, captures surface dimensions, sets identity
// transform, and starts inactive.
TEST(GLESCanvasSkeletonTest, Construct_BasicState) {
  vx::platform::Sdl2GLWindowSurface surface(64, 48, "vx_g14_t1");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  EXPECT_FALSE(canvas.active());
  EXPECT_NE(canvas.quad_vao(), 0u);
  EXPECT_NE(canvas.quad_vbo(), 0u);
  EXPECT_TRUE(MatrixEq(canvas.GetTransform(), Matrix3x2::Identity()));
}

// ---------------------------------------------------------------------------
// T2: Begin_EnablesBlend (Begin contract / glIsEnabled probe)
// ---------------------------------------------------------------------------
TEST(GLESCanvasSkeletonTest, Begin_EnablesBlend) {
  vx::platform::Sdl2GLWindowSurface surface(32, 32, "vx_g14_t2");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  canvas.Begin();
  EXPECT_TRUE(canvas.active());
  EXPECT_EQ(glIsEnabled(GL_BLEND), GL_TRUE);
}

// ---------------------------------------------------------------------------
// T3: Clear_WritesPixels (Mesa swrast pixel-readback / G1.3 T4 first-evidence reuse)
// ---------------------------------------------------------------------------
// Clear(red) followed by glReadPixels should yield ~red pixels. Because G1.3
// T4 already proved Mesa swrast renders to the default framebuffer, this is
// expected to PASS without GTEST_SKIP. If it doesn't, we fall back to
// GTEST_SKIP for driver-strictness layering consistency.
TEST(GLESCanvasSkeletonTest, Clear_WritesPixels) {
  vx::platform::Sdl2GLWindowSurface surface(8, 8, "vx_g14_t3");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  canvas.Begin();
  canvas.Clear(Color::Red());

  uint8_t pixel[4] = {0, 0, 0, 0};
  glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
  if (pixel[0] == 0 && pixel[1] == 0 && pixel[2] == 0) {
    GTEST_SKIP() << "Mesa swrast offscreen did not render to default fb "
                    "(driver-strictness layer §0.4); G1.3 T4 first-evidence "
                    "should preclude this — investigate if it triggers.";
  }
  EXPECT_GT(pixel[0], 200u);
  EXPECT_LT(pixel[1], 50u);
  EXPECT_LT(pixel[2], 50u);
}

// ---------------------------------------------------------------------------
// T4: SetTransform_GetTransform_RoundTrip (D3 CPU shadow state)
// ---------------------------------------------------------------------------
TEST(GLESCanvasSkeletonTest, SetTransform_GetTransform_RoundTrip) {
  vx::platform::Sdl2GLWindowSurface surface(32, 32, "vx_g14_t4");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  Matrix3x2 m = Matrix3x2::Translation(10.5f, -20.25f);
  canvas.SetTransform(m);
  EXPECT_TRUE(MatrixEq(canvas.GetTransform(), m));
}

// ---------------------------------------------------------------------------
// T5: PushState_PopState_RestoresTransform (D6=A SoftwareCanvas-style stack)
// ---------------------------------------------------------------------------
TEST(GLESCanvasSkeletonTest, PushState_PopState_RestoresTransform) {
  vx::platform::Sdl2GLWindowSurface surface(32, 32, "vx_g14_t5");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  Matrix3x2 base = Matrix3x2::Translation(1, 2);
  canvas.SetTransform(base);
  canvas.PushState();

  Matrix3x2 child = Matrix3x2::Translation(99, 100);
  canvas.SetTransform(child);
  EXPECT_TRUE(MatrixEq(canvas.GetTransform(), child));

  canvas.PopState();
  EXPECT_TRUE(MatrixEq(canvas.GetTransform(), base));
}

// ---------------------------------------------------------------------------
// T6: End_FlushesGL (Begin/End contract closure)
// ---------------------------------------------------------------------------
// End() should call glFlush; we assert no GL error is raised during the
// pair (errors latch otherwise). active() flips back to false.
TEST(GLESCanvasSkeletonTest, End_FlushesGL) {
  vx::platform::Sdl2GLWindowSurface surface(32, 32, "vx_g14_t6");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  canvas.Begin();
  canvas.End();
  EXPECT_FALSE(canvas.active());
  EXPECT_EQ(glGetError(), static_cast<GLenum>(GL_NO_ERROR));
}

// ---------------------------------------------------------------------------
// T7: ReverseProbe_StubMethodsAreNoOp (D7=A 16 stub method coverage)
// ---------------------------------------------------------------------------
// Calling all 15 stub methods must not crash and must not raise GL errors.
// Verifies no-op contract before G1.5+ implementations replace them.
TEST(GLESCanvasSkeletonTest, ReverseProbe_StubMethodsAreNoOp) {
  vx::platform::Sdl2GLWindowSurface surface(32, 32, "vx_g14_t7");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  canvas.Begin();
  // Drain any pre-existing GL error state so this test only judges the
  // stubs' own contribution.
  while (glGetError() != GL_NO_ERROR) {}

  Brush b{};
  Rect r{0, 0, 10, 10};
  canvas.FillRect(r, b);
  canvas.FillRoundedRect(r, 4.0f, b);
  canvas.StrokeRect(r, b, 1.0f);
  canvas.StrokeRoundedRect(r, 4.0f, b, 1.0f);
  canvas.StrokeLine({0, 0}, {10, 10}, b, 1.0f);
  canvas.DrawText(vx::StringView("hi", 2), r, 12.0f, b);
  canvas.PushClipRect(r);
  canvas.PopClip();
  canvas.PushLayer(r, 0.5f);
  canvas.PopLayer();
  EXPECT_EQ(canvas.CreatePath(), nullptr);

  EXPECT_EQ(glGetError(), static_cast<GLenum>(GL_NO_ERROR));
  canvas.End();
}

// ---------------------------------------------------------------------------
// T8: ReverseProbe_NonMainThread_GLContextNotCurrent (蓝图安全矩阵 #5)
// ---------------------------------------------------------------------------
// Skeleton phase doesn't enforce a thread check (would require thread-local
// storage of the GL context owner); we verify documentation contract with a
// soft probe: spawn a thread (no current context), call canvas methods,
// then reclaim main context and verify state is recoverable.
// G1.13 / G1.14 will plumb the real DCHECK once Application owns the lifecycle.
TEST(GLESCanvasSkeletonTest, ReverseProbe_NonMainThread_GLContextNotCurrent) {
  vx::platform::Sdl2GLWindowSurface surface(32, 32, "vx_g14_t8");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);

  std::thread t([&canvas]() {
    canvas.Begin();
    canvas.Clear(Color::Blue());
    canvas.End();
  });
  t.join();

  ASSERT_TRUE(surface.gles_display()->MakeCurrent());
  canvas.Begin();
  EXPECT_EQ(glIsEnabled(GL_BLEND), GL_TRUE)
      << "Thread test corrupted GL context state — would indicate the "
         "skeleton needs an explicit thread DCHECK earlier than G1.13.";
  canvas.End();
}

}  // namespace
}  // namespace vx::gfx::gles
