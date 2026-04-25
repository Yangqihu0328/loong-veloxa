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

TEST(Sdl2InputTranslate, KeyDownAsciiNoMod) {
  SDL_Event ev{};
  ev.type = SDL_KEYDOWN;
  ev.key.keysym.sym = SDLK_a;
  ev.key.keysym.mod = KMOD_NONE;

  VxInputEvent out{};
  ASSERT_TRUE(TranslateSdlEvent(ev, out));
  EXPECT_EQ(out.type, VX_EVENT_KEY_DOWN);
  EXPECT_EQ(out.key_code, static_cast<uint32_t>('a'));
  EXPECT_EQ(out.modifiers, 0u);
}

TEST(Sdl2InputTranslate, KeyUpAsciiCtrlShift) {
  SDL_Event ev{};
  ev.type = SDL_KEYUP;
  ev.key.keysym.sym = SDLK_z;
  ev.key.keysym.mod =
      static_cast<SDL_Keymod>(KMOD_LCTRL | KMOD_LSHIFT);

  VxInputEvent out{};
  ASSERT_TRUE(TranslateSdlEvent(ev, out));
  EXPECT_EQ(out.type, VX_EVENT_KEY_UP);
  EXPECT_EQ(out.key_code, static_cast<uint32_t>('z'));
  // bit0 = Shift, bit1 = Ctrl
  EXPECT_EQ(out.modifiers & 0x1u, 0x1u);  // Shift
  EXPECT_EQ(out.modifiers & 0x2u, 0x2u);  // Ctrl
}

TEST(Sdl2InputTranslate, KeyDownNonAsciiReturnsZero) {
  SDL_Event ev{};
  ev.type = SDL_KEYDOWN;
  ev.key.keysym.sym = SDLK_F1;  // Non-ASCII
  ev.key.keysym.mod = KMOD_NONE;

  VxInputEvent out{};
  ASSERT_TRUE(TranslateSdlEvent(ev, out));
  EXPECT_EQ(out.type, VX_EVENT_KEY_DOWN);
  EXPECT_EQ(out.key_code, 0u);
}

TEST(Sdl2InputTranslate, QuitReturnsFalse) {
  SDL_Event ev{};
  ev.type = SDL_QUIT;
  VxInputEvent out{};
  EXPECT_FALSE(TranslateSdlEvent(ev, out));
}

TEST(Sdl2InputTranslate, WindowEventReturnsFalse) {
  SDL_Event ev{};
  ev.type = SDL_WINDOWEVENT;
  VxInputEvent out{};
  EXPECT_FALSE(TranslateSdlEvent(ev, out));
}

TEST(Sdl2InputTranslate, ModifierMappingAllBits) {
  EXPECT_EQ(TranslateSdlMod(KMOD_NONE), 0u);
  EXPECT_EQ(TranslateSdlMod(KMOD_LSHIFT), 0x1u);
  EXPECT_EQ(TranslateSdlMod(KMOD_RSHIFT), 0x1u);
  EXPECT_EQ(TranslateSdlMod(KMOD_LCTRL), 0x2u);
  EXPECT_EQ(TranslateSdlMod(KMOD_LALT), 0x4u);
  EXPECT_EQ(TranslateSdlMod(KMOD_LGUI), 0x8u);
  EXPECT_EQ(TranslateSdlMod(static_cast<SDL_Keymod>(
                KMOD_LSHIFT | KMOD_LCTRL | KMOD_LALT | KMOD_LGUI)),
            0xFu);
}

}  // namespace
}  // namespace vx::platform
