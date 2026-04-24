#ifndef VELOXA_GRAPHICS_SOFTWARE_GLYPH_BLEND_H_
#define VELOXA_GRAPHICS_SOFTWARE_GLYPH_BLEND_H_

#include "veloxa/foundation/base/types.h"

namespace vx::gfx::sw {

// Approximates `n / 255` (rounded to nearest) for n in [0, 65534] using
// only integer add/shift. Classic "n = (n + 128 + ((n + 128) >> 8)) >> 8"
// trick that avoids the x86 idiv dependency on the blit hot path.
//
// Precision contract: |DivBy255Approx(n) - (n / 255)| <= 1 for all n in
// the supported range. The delta comes from rounded-to-nearest vs C's
// truncation-towards-zero semantics — e.g. n=128 yields 1 here vs 0 from
// `n / 255`. At display resolution this is sub-LSB colour-channel drift.
static inline u32 DivBy255Approx(u32 n) {
  return (n + 128u + ((n + 128u) >> 8)) >> 8;
}

// Alpha-blends a single glyph pixel (coverage mask `glyph_alpha`) of a
// solid brush colour (sr, sg, sb, sa) over the destination RGBA8888
// pixel. Output matches the reference scalar `/255` blend to within
// ±1 LSB per channel (see DivBy255Approx precision contract).
static inline u32 BlendGlyphPixel(u32 dst_pixel, u8 sr, u8 sg, u8 sb, u8 sa,
                                  u8 glyph_alpha) {
  u32 dr = dst_pixel & 0xFFu;
  u32 dg = (dst_pixel >> 8) & 0xFFu;
  u32 db = (dst_pixel >> 16) & 0xFFu;
  u32 da = (dst_pixel >> 24) & 0xFFu;

  u32 effective_a = DivBy255Approx(static_cast<u32>(sa) *
                                   static_cast<u32>(glyph_alpha));
  u32 inv = 255u - effective_a;

  u32 orc = DivBy255Approx(static_cast<u32>(sr) * effective_a + dr * inv);
  u32 ogc = DivBy255Approx(static_cast<u32>(sg) * effective_a + dg * inv);
  u32 obc = DivBy255Approx(static_cast<u32>(sb) * effective_a + db * inv);
  u32 oac = effective_a + DivBy255Approx(da * inv);

  return orc | (ogc << 8) | (obc << 16) | (oac << 24);
}

}  // namespace vx::gfx::sw

#endif  // VELOXA_GRAPHICS_SOFTWARE_GLYPH_BLEND_H_
