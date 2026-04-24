#ifndef VELOXA_GRAPHICS_SOFTWARE_BLIT_SSE2_H_
#define VELOXA_GRAPHICS_SOFTWARE_BLIT_SSE2_H_

#include <cstring>

#include "veloxa/foundation/base/types.h"
#include "veloxa/graphics/software/glyph_blend.h"

#if defined(__SSE2__)
#include <emmintrin.h>
#endif

namespace vx::gfx::sw {

#if defined(__SSE2__)

// Per-lane DivBy255Approx for an 8 x u16 vector. Input lanes must
// satisfy n <= 65025 (i.e. products of two u8 values). Semantics
// match the scalar DivBy255Approx (rounded-to-nearest) defined in
// glyph_blend.h; produces output in [0, 255].
static inline __m128i DivBy255ApproxU16(__m128i n) {
  const __m128i c128 = _mm_set1_epi16(128);
  __m128i t = _mm_add_epi16(n, c128);
  t = _mm_add_epi16(t, _mm_srli_epi16(t, 8));
  return _mm_srli_epi16(t, 8);
}

// Alpha-blends a horizontal run of `count` RGBA8888 pixels at `dst`
// with a single source colour (sr, sg, sb, sa) masked per-pixel by
// the glyph coverage bytes at `alpha`. Four pixels per SSE2
// iteration; any tail of 0..3 pixels falls through to the scalar
// BlendGlyphPixel helper.
//
// Precision contract: every output pixel channel matches the scalar
// BlendGlyphPixel helper to within ±1 LSB. See pixel_blend_test for
// the enforced boundary (including the SIMD-specific fidelity case).
//
// Note: the source alpha channel is forced to 255 inside the SIMD
// vector so that the unified blend formula `(src * ea + dst * inv) /
// 255` holds on the alpha lane too — this is exact because
// (255 * effective_a) / 255 == effective_a for all effective_a in
// [0, 255], matching the scalar `oa = effective_a + (da * inv)/255`
// formulation.
static inline void BlendGlyphRowSSE2(u32* dst, const u8* alpha, u32 count,
                                     u8 sr, u8 sg, u8 sb, u8 sa) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i c255 = _mm_set1_epi16(255);
  const __m128i src_vec =
      _mm_set_epi16(255, static_cast<i16>(sb), static_cast<i16>(sg),
                    static_cast<i16>(sr), 255, static_cast<i16>(sb),
                    static_cast<i16>(sg), static_cast<i16>(sr));
  const __m128i saa_vec = _mm_set1_epi16(static_cast<i16>(sa));

  u32 i = 0;
  for (; i + 4 <= count; i += 4) {
    u32 alpha_u32;
    std::memcpy(&alpha_u32, alpha + i, 4);
    __m128i a_bytes = _mm_cvtsi32_si128(static_cast<i32>(alpha_u32));
    __m128i alpha_u16 = _mm_unpacklo_epi8(a_bytes, zero);

    __m128i ea = DivBy255ApproxU16(_mm_mullo_epi16(alpha_u16, saa_vec));
    __m128i inv = _mm_sub_epi16(c255, ea);

    __m128i ea_01 = _mm_unpacklo_epi16(ea, ea);
    __m128i ea_lo = _mm_unpacklo_epi32(ea_01, ea_01);
    __m128i ea_hi = _mm_unpackhi_epi32(ea_01, ea_01);
    __m128i inv_01 = _mm_unpacklo_epi16(inv, inv);
    __m128i inv_lo = _mm_unpacklo_epi32(inv_01, inv_01);
    __m128i inv_hi = _mm_unpackhi_epi32(inv_01, inv_01);

    __m128i dst4 =
        _mm_loadu_si128(reinterpret_cast<const __m128i*>(dst + i));
    __m128i dst_lo = _mm_unpacklo_epi8(dst4, zero);
    __m128i dst_hi = _mm_unpackhi_epi8(dst4, zero);

    __m128i src_term_lo =
        DivBy255ApproxU16(_mm_mullo_epi16(src_vec, ea_lo));
    __m128i dst_term_lo =
        DivBy255ApproxU16(_mm_mullo_epi16(dst_lo, inv_lo));
    __m128i out_lo = _mm_add_epi16(src_term_lo, dst_term_lo);

    __m128i src_term_hi =
        DivBy255ApproxU16(_mm_mullo_epi16(src_vec, ea_hi));
    __m128i dst_term_hi =
        DivBy255ApproxU16(_mm_mullo_epi16(dst_hi, inv_hi));
    __m128i out_hi = _mm_add_epi16(src_term_hi, dst_term_hi);

    __m128i out_u8 = _mm_packus_epi16(out_lo, out_hi);
    _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + i), out_u8);
  }

  for (; i < count; ++i) {
    u8 a = alpha[i];
    if (a == 0) continue;
    dst[i] = BlendGlyphPixel(dst[i], sr, sg, sb, sa, a);
  }
}

#else  // !__SSE2__

static inline void BlendGlyphRowSSE2(u32* dst, const u8* alpha, u32 count,
                                     u8 sr, u8 sg, u8 sb, u8 sa) {
  for (u32 i = 0; i < count; ++i) {
    u8 a = alpha[i];
    if (a == 0) continue;
    dst[i] = BlendGlyphPixel(dst[i], sr, sg, sb, sa, a);
  }
}

#endif  // __SSE2__

}  // namespace vx::gfx::sw

#endif  // VELOXA_GRAPHICS_SOFTWARE_BLIT_SSE2_H_
