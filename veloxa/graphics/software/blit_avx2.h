#ifndef VELOXA_GRAPHICS_SOFTWARE_BLIT_AVX2_H_
#define VELOXA_GRAPHICS_SOFTWARE_BLIT_AVX2_H_

#include <cstring>

#include "veloxa/foundation/base/types.h"
#include "veloxa/graphics/software/blit_sse2.h"
#include "veloxa/graphics/software/glyph_blend.h"

#if defined(__x86_64__) || defined(__i386__)
#include <immintrin.h>
#endif

namespace vx::gfx::sw {

#if defined(__SSE2__)  // AVX2 is a strict superset of SSE2

// Per-lane DivBy255Approx for a 16 x u16 vector (two 128-bit lanes of
// 8 u16 each). Inputs must satisfy n <= 65025 per lane; output in
// [0, 255]. Semantics identical to the scalar DivBy255Approx.
__attribute__((target("avx2"))) static inline __m256i DivBy255ApproxU16_256(
    __m256i n) {
  const __m256i c128 = _mm256_set1_epi16(128);
  __m256i t = _mm256_add_epi16(n, c128);
  t = _mm256_add_epi16(t, _mm256_srli_epi16(t, 8));
  return _mm256_srli_epi16(t, 8);
}

// AVX2 8-pixel-per-iteration glyph blitter. Per-lane-local semantics
// match the SSE2 path; the only trickery is that AVX2's unpack_epi8
// produces interleaved pixel layout (dst_lo holds pixels 0,1,4,5 and
// dst_hi holds 2,3,6,7 after unpack). Alpha is pre-permuted via
// permute4x64_epi64 so that lane_lo's usable u16 lanes 0-3 hold
// alphas 0-3 and lane_hi's lanes 0-3 hold alphas 4-7, which after
// the standard broadcast chain (unpacklo_epi16/epi32) re-aligns to
// the 0,1,4,5 / 2,3,6,7 layout that dst_lo/dst_hi expect.
//
// Tail of 0..7 pixels falls through to the SSE2 path (which further
// tails 0..3 to scalar).
__attribute__((target("avx2"))) static inline void BlendGlyphRowAVX2(
    u32* dst, const u8* alpha, u32 count, u8 sr, u8 sg, u8 sb, u8 sa) {
  const __m256i zero = _mm256_setzero_si256();
  const __m256i c255 = _mm256_set1_epi16(255);
  const __m256i saa_vec = _mm256_set1_epi16(static_cast<i16>(sa));
  __m128i src_128 =
      _mm_set_epi16(255, static_cast<i16>(sb), static_cast<i16>(sg),
                    static_cast<i16>(sr), 255, static_cast<i16>(sb),
                    static_cast<i16>(sg), static_cast<i16>(sr));
  const __m256i src_vec = _mm256_set_m128i(src_128, src_128);

  u32 i = 0;
  for (; i + 8 <= count; i += 8) {
    __m128i a_128 = _mm_loadl_epi64(
        reinterpret_cast<const __m128i*>(alpha + i));
    // Zero-extend 8 u8 alphas to 16 u16 (lanes 0..7 = a0..a7,
    // lanes 8..15 = 0 because the upper 64 bits of a_128 were zeroed
    // by loadl_epi64).
    __m256i alpha_flat = _mm256_cvtepu8_epi16(a_128);
    // Rearrange u64s so that the 256-bit register splits cleanly
    // into two 128-bit lanes where lane_lo = [a0..a3 | zeros] and
    // lane_hi = [a4..a7 | zeros] — required because the subsequent
    // unpacklo/unpackhi chain is lane-local and only consumes the
    // first 4 u16s of each lane.
    __m256i alpha_vec = _mm256_permute4x64_epi64(alpha_flat, 0xD8);

    __m256i ea =
        DivBy255ApproxU16_256(_mm256_mullo_epi16(alpha_vec, saa_vec));
    __m256i inv = _mm256_sub_epi16(c255, ea);

    __m256i ea_01 = _mm256_unpacklo_epi16(ea, ea);
    __m256i ea_lo = _mm256_unpacklo_epi32(ea_01, ea_01);
    __m256i ea_hi = _mm256_unpackhi_epi32(ea_01, ea_01);
    __m256i inv_01 = _mm256_unpacklo_epi16(inv, inv);
    __m256i inv_lo = _mm256_unpacklo_epi32(inv_01, inv_01);
    __m256i inv_hi = _mm256_unpackhi_epi32(inv_01, inv_01);

    __m256i dst8 = _mm256_loadu_si256(
        reinterpret_cast<const __m256i*>(dst + i));
    __m256i dst_lo = _mm256_unpacklo_epi8(dst8, zero);
    __m256i dst_hi = _mm256_unpackhi_epi8(dst8, zero);

    __m256i src_term_lo =
        DivBy255ApproxU16_256(_mm256_mullo_epi16(src_vec, ea_lo));
    __m256i dst_term_lo =
        DivBy255ApproxU16_256(_mm256_mullo_epi16(dst_lo, inv_lo));
    __m256i out_lo = _mm256_add_epi16(src_term_lo, dst_term_lo);

    __m256i src_term_hi =
        DivBy255ApproxU16_256(_mm256_mullo_epi16(src_vec, ea_hi));
    __m256i dst_term_hi =
        DivBy255ApproxU16_256(_mm256_mullo_epi16(dst_hi, inv_hi));
    __m256i out_hi = _mm256_add_epi16(src_term_hi, dst_term_hi);

    __m256i out_u8 = _mm256_packus_epi16(out_lo, out_hi);
    _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i), out_u8);
  }

  if (i < count) {
    BlendGlyphRowSSE2(dst + i, alpha + i, count - i, sr, sg, sb, sa);
  }
}

// Width threshold (in pixels) above which the AVX2 path is dispatched
// when available. Below this threshold, per-iteration AVX2 setup cost
// (cvtepu8_epi16 + permute4x64_epi64) plus the dispatch branch isn't
// amortised by enough 8-px iterations — the SSE2 4 px/iter path wins
// on small glyphs (ASCII 6-12 px and thin CJK radicals). Empirically
// validated against BM_DrawTextReal_Warm_{Short,Medium,Long} on
// Zen4/Rocket Lake class x86_64.
static constexpr u32 kAVX2MinPixelsPerRow = 16;

// Runtime dispatch: picks the widest path the CPU supports AND that
// pays off for the given row width. Dispatch decision is cached in a
// function-local static (initialised exactly once per process).
// Callers should prefer this entry point over the individual
// AVX2/SSE2 helpers.
static inline void BlendGlyphRow(u32* dst, const u8* alpha, u32 count,
                                 u8 sr, u8 sg, u8 sb, u8 sa) {
#if defined(__x86_64__) || defined(__i386__)
  static const bool has_avx2 = __builtin_cpu_supports("avx2") != 0;
  if (has_avx2 && count >= kAVX2MinPixelsPerRow) {
    BlendGlyphRowAVX2(dst, alpha, count, sr, sg, sb, sa);
    return;
  }
#endif
  BlendGlyphRowSSE2(dst, alpha, count, sr, sg, sb, sa);
}

#else  // !__SSE2__

static inline void BlendGlyphRow(u32* dst, const u8* alpha, u32 count, u8 sr,
                                 u8 sg, u8 sb, u8 sa) {
  BlendGlyphRowSSE2(dst, alpha, count, sr, sg, sb, sa);
}

#endif  // __SSE2__

}  // namespace vx::gfx::sw

#endif  // VELOXA_GRAPHICS_SOFTWARE_BLIT_AVX2_H_
