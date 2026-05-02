#include "veloxa/platform/sdl2/sdl2_input_translate.h"

namespace vx::platform {

bool TranslateSdlEvent(const SDL_Event& sdl, VxInputEvent& out) {
  out = {};
  switch (sdl.type) {
    case SDL_MOUSEMOTION:
      out.type = VX_EVENT_POINTER_MOVE;
      out.x = static_cast<float>(sdl.motion.x);
      out.y = static_cast<float>(sdl.motion.y);
      return true;
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
      out.type = (sdl.type == SDL_MOUSEBUTTONDOWN) ? VX_EVENT_POINTER_DOWN
                                                   : VX_EVENT_POINTER_UP;
      out.x = static_cast<float>(sdl.button.x);
      out.y = static_cast<float>(sdl.button.y);
      // SDL_BUTTON_LEFT=1, MIDDLE=2, RIGHT=3 → Veloxa convention 0/1/2.
      out.button = static_cast<uint8_t>(sdl.button.button - 1);
      return true;
    case SDL_KEYDOWN:
    case SDL_KEYUP:
      out.type =
          (sdl.type == SDL_KEYDOWN) ? VX_EVENT_KEY_DOWN : VX_EVENT_KEY_UP;
      out.key_code = TranslateSdlKey(sdl.key.keysym.sym);
      out.modifiers = TranslateSdlMod(
          static_cast<SDL_Keymod>(sdl.key.keysym.mod));
      return true;
    default:
      return false;
  }
}

uint32_t TranslateSdlKey(SDL_Keycode key) {
  // ASCII subset: SDLK_<ascii> values equal their ASCII code.
  if (key >= 0 && key < 128) {
    return static_cast<uint32_t>(key);
  }
  // TASK-20260502-01 A.1.7: SDLK_F12 → VX_KEY_F12 so SDL2-driven hosts
  // can deliver the DevTool F12 hotkey naturally via vx_event_loop_pump_input.
  // Other F-keys / arrows still return 0 until embedders need them.
  if (key == SDLK_F12) {
    return 0x40000045u;  // mirrors VX_KEY_F12 in veloxa/api/veloxa_api.h
  }
  return 0;
}

uint32_t TranslateSdlMod(SDL_Keymod mod) {
  uint32_t out = 0;
  if (mod & (KMOD_LSHIFT | KMOD_RSHIFT)) out |= 0x1;  // Shift
  if (mod & (KMOD_LCTRL  | KMOD_RCTRL))  out |= 0x2;  // Ctrl
  if (mod & (KMOD_LALT   | KMOD_RALT))   out |= 0x4;  // Alt
  if (mod & (KMOD_LGUI   | KMOD_RGUI))   out |= 0x8;  // Super/GUI
  return out;
}

}  // namespace vx::platform
