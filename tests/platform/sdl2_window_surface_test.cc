#include "veloxa/platform/sdl2/sdl2_window_surface.h"

#include <SDL2/SDL.h>

#include <gtest/gtest.h>

namespace vx::platform {
namespace {

TEST(Sdl2WindowSurface, ConstructDestructDummyDriver) {
  Sdl2WindowSurface surface(320, 240, "test-window");
  ASSERT_TRUE(surface.valid()) << "Sdl2WindowSurface failed to construct: "
                               << SDL_GetError();
  EXPECT_EQ(surface.width(), 320u);
  EXPECT_EQ(surface.height(), 240u);
  EXPECT_EQ(surface.stride(), 320u * 4);
  EXPECT_NE(surface.window(), nullptr);
  EXPECT_GT(surface.window_id(), 0u);
}

TEST(Sdl2WindowSurface, RepeatedCreateDestroyNoSdlSubsystemLeak) {
  for (int i = 0; i < 3; ++i) {
    Sdl2WindowSurface surface(100, 100, "repeat");
    ASSERT_TRUE(surface.valid());
  }
  EXPECT_EQ(SDL_WasInit(SDL_INIT_VIDEO), 0u)
      << "VIDEO subsystem still initialized after all Sdl2WindowSurface "
         "instances destroyed (refcount leak)";
}

TEST(Sdl2WindowSurface, LockReturnsNonNullAndPresentDoesNotCrash) {
  Sdl2WindowSurface surface(64, 48, "lock-present");
  ASSERT_TRUE(surface.valid());

  vx::u32* pixels = surface.Lock();
  ASSERT_NE(pixels, nullptr);
  for (vx::u32 i = 0; i < 64u * 48u; ++i) {
    pixels[i] = 0xFF202060;  // ABGR: opaque dark blue
  }
  surface.Unlock();

  surface.Present();
  surface.Present();  // double-present should be safe (idempotent)

  EXPECT_EQ(surface.width(), 64u);
}

TEST(Sdl2WindowSurface, PresentBeforeAnyLockDoesNotCrash) {
  Sdl2WindowSurface surface(32, 32, "no-lock");
  ASSERT_TRUE(surface.valid());
  surface.Present();
  SUCCEED();
}

TEST(Sdl2WindowSurface, PresentUploadsPixelsToTexture) {
  // We can't read back from a window backbuffer reliably under the dummy
  // driver, but SDL_RenderReadPixels reads what was last RenderCopy'd.
  // Verify Present's SDL_UpdateTexture + RenderCopy path runs by reading
  // back at least one pixel from the renderer.
  Sdl2WindowSurface surface(8, 8, "readback");
  ASSERT_TRUE(surface.valid());

  vx::u32* pixels = surface.Lock();
  ASSERT_NE(pixels, nullptr);
  for (vx::u32 i = 0; i < 64; ++i) {
    pixels[i] = 0xFFFFFFFF;  // Solid white (ABGR opaque)
  }
  surface.Unlock();
  surface.Present();

  // Read back via the renderer. SDL renderer pixel reads must use the
  // renderer's pixel format. We just ensure the call succeeds.
  SDL_Renderer* r = SDL_GetRenderer(surface.window());
  ASSERT_NE(r, nullptr);
  vx::u32 readback[64] = {};
  int rc = SDL_RenderReadPixels(r, nullptr, SDL_PIXELFORMAT_ABGR8888, readback,
                                8 * sizeof(vx::u32));
  EXPECT_EQ(rc, 0) << "SDL_RenderReadPixels failed: " << SDL_GetError();
}

TEST(Sdl2WindowSurface, CoexistsWithExternalSdlVideoInit) {
  ASSERT_EQ(SDL_InitSubSystem(SDL_INIT_VIDEO), 0)
      << "Failed to pre-init SDL video: " << SDL_GetError();
  {
    Sdl2WindowSurface surface(100, 100, "coexist");
    ASSERT_TRUE(surface.valid());
  }
  EXPECT_NE(SDL_WasInit(SDL_INIT_VIDEO), 0u)
      << "Sdl2WindowSurface dtor must not tear down externally-owned "
         "VIDEO subsystem";
  SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

}  // namespace
}  // namespace vx::platform
