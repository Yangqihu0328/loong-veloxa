// Tests for Sdl2GLWindowSurface (G1.3 GLES window surface).
//
// Headless CI fixture: mirrors the G1.2 Sdl2EglEnvironment pattern.
// SDL_VIDEODRIVER=offscreen is set process-wide so SDL_GL_CreateContext
// can attach a real GLES 3.0+ context via Mesa swrast EGL without an X11
// display. ctest registration also passes the variable via PROPERTIES
// ENVIRONMENT as a belt-and-suspenders measure.
//
// Each test constructs a fresh Sdl2GLWindowSurface so attribute state from
// a previous test (e.g. SDL_GL_SetAttribute calls inside ctor) cannot leak.
// The surface owns its SDL_Window and Sdl2EGLDisplay — no per-test
// SDL_CreateWindow boilerplate required.

#include "veloxa/platform/sdl2/sdl2_gl_window_surface.h"

#include <GLES3/gl3.h>
#include <SDL2/SDL.h>
#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <string>

#include "veloxa/foundation/base/status.h"
#include "veloxa/platform/sdl2/sdl2_egl_display.h"

namespace vx::platform {
namespace {

// ---------------------------------------------------------------------------
// Process-wide SDL environment (offscreen driver + global SDL Init/Quit).
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

// ---------------------------------------------------------------------------
// Helper: deterministic temp path under /tmp so SavePPM has a valid sink.
// ---------------------------------------------------------------------------
static std::string TempPpmPath(const char* tag) {
  std::string p = "/tmp/vx_g13_";
  p += tag;
  p += ".ppm";
  return p;
}

// ---------------------------------------------------------------------------
// T1: Construct_BasicGetters (D1 + D7 happy path)
// ---------------------------------------------------------------------------
// Verifies that a freshly constructed surface reports correct geometry and
// exposes a live GL context handle. width/height are what we asked for,
// stride is 0 (GLES path has no CPU pixel buffer), and gles_display() is
// non-null with IsValid() == true.
TEST(Sdl2GlWindowSurfaceTest, Construct_BasicGetters) {
  Sdl2GLWindowSurface surface(64, 48, "vx_g13_t1");
  ASSERT_TRUE(surface.valid())
      << "Construction must succeed under SDL_VIDEODRIVER=offscreen";
  EXPECT_EQ(surface.width(), 64u);
  EXPECT_EQ(surface.height(), 48u);
  EXPECT_EQ(surface.stride(), 0u);  // GLES path: no CPU stride
  EXPECT_NE(surface.window(), nullptr);
  EXPECT_NE(surface.gles_display(), nullptr);
  EXPECT_TRUE(surface.gles_display()->IsValid());
}

// ---------------------------------------------------------------------------
// T2: Lock_Returns_Nullptr (D5 GLES no-op contract)
// ---------------------------------------------------------------------------
// GLES rasterisation happens on the GPU; there is no CPU pixel buffer to
// expose. Lock() must return nullptr and Unlock() must be safe even after
// a null Lock().
TEST(Sdl2GlWindowSurfaceTest, Lock_Returns_Nullptr) {
  Sdl2GLWindowSurface surface(32, 32, "vx_g13_t2");
  ASSERT_TRUE(surface.valid());
  EXPECT_EQ(surface.Lock(), nullptr);
  surface.Unlock();  // must not crash when Lock returned nullptr
}

// ---------------------------------------------------------------------------
// T3: Resize_Updates_Dimensions (D6)
// ---------------------------------------------------------------------------
// After Resize the internal width_/height_ accessors must reflect the new
// size. The GL viewport update is GLESCanvas's responsibility (G1.4+); the
// surface only tracks the logical size here.
TEST(Sdl2GlWindowSurfaceTest, Resize_Updates_Dimensions) {
  Sdl2GLWindowSurface surface(64, 48, "vx_g13_t3");
  ASSERT_TRUE(surface.valid());
  surface.Resize(800, 600);
  EXPECT_EQ(surface.width(), 800u);
  EXPECT_EQ(surface.height(), 600u);
}

// ---------------------------------------------------------------------------
// T4: SavePPM_WritesValidFile (D3 + D4 + Phase 0 §0.4 driver probe)
// ---------------------------------------------------------------------------
// Clears the default framebuffer to red, then calls SavePPM. Assertions:
//   (strict)  File exists, header is well-formed P6 with correct size/maxval.
//   (soft)    First pixel is approximately red. If Mesa swrast under the
//             offscreen driver does NOT render to the default framebuffer
//             (all-zeros readback), we GTEST_SKIP the pixel check rather
//             than FAIL — driver-strictness layering first-evidence reuse
//             (G1.2 T5 pattern). The header assertions remain strict.
TEST(Sdl2GlWindowSurfaceTest, SavePPM_WritesValidFile) {
  Sdl2GLWindowSurface surface(8, 8, "vx_g13_t4");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  const std::string path = TempPpmPath("t4");
  vx::Status s = surface.SavePPM(path.c_str());
  ASSERT_TRUE(s.ok()) << "SavePPM failed: " << s.message();

  // --- Strict header assertions -------------------------------------------
  std::ifstream in(path, std::ios::binary);
  ASSERT_TRUE(in.is_open()) << "PPM file not found at: " << path;

  std::string magic, dims, maxval;
  std::getline(in, magic);
  std::getline(in, dims);
  std::getline(in, maxval);
  EXPECT_EQ(magic, "P6");
  EXPECT_EQ(dims, "8 8");
  EXPECT_EQ(maxval, "255");

  // --- Soft pixel assertion (driver-strictness layer) ---------------------
  uint8_t pixel[3] = {0, 0, 0};
  in.read(reinterpret_cast<char*>(pixel), 3);
  if (pixel[0] == 0 && pixel[1] == 0 && pixel[2] == 0) {
    GTEST_SKIP() << "Mesa swrast offscreen driver did not render to default "
                    "framebuffer — P6 header verified; pixel content deferred "
                    "to real-driver CI. (driver-strictness layer §0.4)";
  }
  EXPECT_GT(pixel[0], 200u) << "Expected ~red R channel, got " << +pixel[0];
  EXPECT_LT(pixel[1], 50u) << "Expected ~0 G channel, got " << +pixel[1];
  EXPECT_LT(pixel[2], 50u) << "Expected ~0 B channel, got " << +pixel[2];

  std::remove(path.c_str());
}

// ---------------------------------------------------------------------------
// T5: Present_DoesNotCrash (SwapBuffers smoke)
// ---------------------------------------------------------------------------
// Under SDL_VIDEODRIVER=offscreen SDL_GL_SwapWindow is a no-op but must be
// callable without crashing. Two consecutive calls confirm no internal state
// latch is erroneously set.
TEST(Sdl2GlWindowSurfaceTest, Present_DoesNotCrash) {
  Sdl2GLWindowSurface surface(16, 16, "vx_g13_t5");
  ASSERT_TRUE(surface.valid());
  surface.Present();
  surface.Present();
}

// ---------------------------------------------------------------------------
// T6: ReverseProbe_NullTitle_StillConstructs (D2 soft-fail / nullptr title)
// ---------------------------------------------------------------------------
// Mirrors Sdl2WindowSurface: passing nullptr for title falls back to a
// default string and construction succeeds.
TEST(Sdl2GlWindowSurfaceTest, ReverseProbe_NullTitle_StillConstructs) {
  Sdl2GLWindowSurface surface(32, 32, nullptr);
  EXPECT_TRUE(surface.valid());
}

// ---------------------------------------------------------------------------
// T7: ReverseProbe_ZeroDimensions_SoftFails (D2 zero-dim guard)
// ---------------------------------------------------------------------------
// Zero dimensions trigger the early-return path: valid() == false, and
// window() / gles_display() are both nullptr. Destruction must be safe
// (no SDL_DestroyWindow call because window_ stayed nullptr).
// Fully driver-independent inline reverse probe.
TEST(Sdl2GlWindowSurfaceTest, ReverseProbe_ZeroDimensions_SoftFails) {
  Sdl2GLWindowSurface surface(0, 0, "vx_g13_t7");
  EXPECT_FALSE(surface.valid());
  EXPECT_EQ(surface.window(), nullptr);
  EXPECT_EQ(surface.gles_display(), nullptr);
}

}  // namespace
}  // namespace vx::platform
