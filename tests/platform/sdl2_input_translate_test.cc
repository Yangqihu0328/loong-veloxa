#include "veloxa/platform/sdl2/sdl2_input_translate.h"

#include <SDL2/SDL.h>

#include <gtest/gtest.h>

namespace vx::platform {
namespace {

TEST(Sdl2InputTranslate, MouseMoveBasic) {
  SDL_Event ev{};
  ev.type = SDL_MOUSEMOTION;
  ev.motion.x = 123;
  ev.motion.y = 456;

  VxInputEvent out{};
  ASSERT_TRUE(TranslateSdlEvent(ev, out));
  EXPECT_EQ(out.type, VX_EVENT_POINTER_MOVE);
  EXPECT_FLOAT_EQ(out.x, 123.0f);
  EXPECT_FLOAT_EQ(out.y, 456.0f);
}

TEST(Sdl2InputTranslate, MouseButtonDownLeft) {
  SDL_Event ev{};
  ev.type = SDL_MOUSEBUTTONDOWN;
  ev.button.x = 10;
  ev.button.y = 20;
  ev.button.button = SDL_BUTTON_LEFT;

  VxInputEvent out{};
  ASSERT_TRUE(TranslateSdlEvent(ev, out));
  EXPECT_EQ(out.type, VX_EVENT_POINTER_DOWN);
  EXPECT_FLOAT_EQ(out.x, 10.0f);
  EXPECT_FLOAT_EQ(out.y, 20.0f);
  EXPECT_EQ(out.button, 0u);
}

TEST(Sdl2InputTranslate, MouseButtonDownMiddle) {
  SDL_Event ev{};
  ev.type = SDL_MOUSEBUTTONDOWN;
  ev.button.button = SDL_BUTTON_MIDDLE;
  VxInputEvent out{};
  ASSERT_TRUE(TranslateSdlEvent(ev, out));
  EXPECT_EQ(out.button, 1u);
}

TEST(Sdl2InputTranslate, MouseButtonUpRight) {
  SDL_Event ev{};
  ev.type = SDL_MOUSEBUTTONUP;
  ev.button.button = SDL_BUTTON_RIGHT;
  VxInputEvent out{};
  ASSERT_TRUE(TranslateSdlEvent(ev, out));
  EXPECT_EQ(out.type, VX_EVENT_POINTER_UP);
  EXPECT_EQ(out.button, 2u);
}

}  // namespace
}  // namespace vx::platform
