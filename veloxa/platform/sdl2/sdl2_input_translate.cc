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
    default:
      return false;
  }
}

uint32_t TranslateSdlKey(SDL_Keycode /*key*/) { return 0; }

uint32_t TranslateSdlMod(SDL_Keymod /*mod*/) { return 0; }

}  // namespace vx::platform
