#ifndef VELOXA_CORE_RENDER_RENDER_UTILS_H_
#define VELOXA_CORE_RENDER_RENDER_UTILS_H_

#include "veloxa/foundation/base/types.h"
#include "veloxa/graphics/types.h"

namespace vx::render {

// CSS ComputedStyle stores colors as RRGGBBAA: R[24:31] G[16:23] B[8:15] A[0:7]
// gfx::Color stores r,g,b,a byte members
inline gfx::Color CssColorToGfx(u32 css_color) {
  return gfx::Color::FromRGBA(
      static_cast<u8>((css_color >> 24) & 0xFF),
      static_cast<u8>((css_color >> 16) & 0xFF),
      static_cast<u8>((css_color >> 8) & 0xFF),
      static_cast<u8>(css_color & 0xFF));
}

}  // namespace vx::render

#endif  // VELOXA_CORE_RENDER_RENDER_UTILS_H_
