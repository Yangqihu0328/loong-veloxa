#include "veloxa/platform/sdl2/sdl2_input_translate.h"

namespace vx::platform {

bool TranslateSdlEvent(const SDL_Event& /*sdl*/, VxInputEvent& /*out*/) {
  return false;
}

uint32_t TranslateSdlKey(SDL_Keycode /*key*/) { return 0; }

uint32_t TranslateSdlMod(SDL_Keymod /*mod*/) { return 0; }

}  // namespace vx::platform
