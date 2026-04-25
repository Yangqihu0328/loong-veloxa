#ifndef VELOXA_PLATFORM_SDL2_SDL2_INPUT_TRANSLATE_H_
#define VELOXA_PLATFORM_SDL2_SDL2_INPUT_TRANSLATE_H_

#include <SDL2/SDL.h>

#include <cstdint>

#include "veloxa/api/veloxa_api.h"

namespace vx::platform {

// Translate a single SDL_Event into a VxInputEvent.
// Returns false if the SDL event does not map to any input event
// (e.g. SDL_QUIT, SDL_WINDOWEVENT) — caller should handle those separately.
bool TranslateSdlEvent(const SDL_Event& sdl, VxInputEvent& out);

// Map an SDL keycode to a Veloxa key_code (currently ASCII subset; non-ASCII
// keys return 0).
uint32_t TranslateSdlKey(SDL_Keycode key);

// Map SDL_Keymod bit flags to Veloxa modifier bit field:
//   bit 0 = Shift, bit 1 = Ctrl, bit 2 = Alt, bit 3 = Super/GUI
uint32_t TranslateSdlMod(SDL_Keymod mod);

}  // namespace vx::platform

#endif  // VELOXA_PLATFORM_SDL2_SDL2_INPUT_TRANSLATE_H_
