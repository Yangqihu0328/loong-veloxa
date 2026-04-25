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
