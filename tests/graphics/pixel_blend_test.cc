#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <vector>

#include "veloxa/graphics/software/blit_avx2.h"
#include "veloxa/graphics/software/blit_sse2.h"
#include "veloxa/graphics/software/glyph_blend.h"

namespace vx::gfx::sw {
namespace {

// ---------- DivBy255Approx precision contract --------------------------------

TEST(PixelBlendTest, DivBy255ApproxWithinOneLsbAcrossFullRange) {
  // Products seen on the blit hot path never exceed 255 * 255 = 65025
  // (two u8 multiplicands). Also check the slightly larger inputs that
  // arise from the sum-of-two-products term (e.g. sr*ea + dr*inv ≤
  // 255 * 255 * 2 = 130050).
  for (uint32_t n = 0; n <= 130050u; ++n) {
    uint32_t approx = DivBy255Approx(n);
    uint32_t exact = n / 255u;
    uint32_t diff = approx >= exact ? approx - exact : exact - approx;
    ASSERT_LE(diff, 1u) << "n=" << n << " approx=" << approx
                        << " exact=" << exact;
  }
}

TEST(PixelBlendTest, DivBy255ApproxMatchesExactOnMultiplesOf255) {
  // On exact multiples of 255 the rounded-to-nearest trick and
  // truncating division always agree.
  for (uint32_t k = 0; k <= 255u; ++k) {
    EXPECT_EQ(DivBy255Approx(k * 255u), k) << "k=" << k;
  }
}

// ---------- Reference scalar blend for ground truth -------------------------

namespace {
uint32_t ReferenceBlendGlyphPixel(uint32_t dst_pixel, uint8_t sr, uint8_t sg,
                                  uint8_t sb, uint8_t sa, uint8_t alpha) {
  uint8_t dr = static_cast<uint8_t>(dst_pixel & 0xFFu);
  uint8_t dg = static_cast<uint8_t>((dst_pixel >> 8) & 0xFFu);
  uint8_t db = static_cast<uint8_t>((dst_pixel >> 16) & 0xFFu);
  uint8_t da = static_cast<uint8_t>((dst_pixel >> 24) & 0xFFu);

  uint8_t effective_a =
      static_cast<uint8_t>((static_cast<uint32_t>(sa) * alpha) / 255u);
  uint8_t inv = static_cast<uint8_t>(255u - effective_a);

  uint8_t orc = static_cast<uint8_t>(
      (static_cast<uint32_t>(sr) * effective_a + dr * inv) / 255u);
  uint8_t ogc = static_cast<uint8_t>(
      (static_cast<uint32_t>(sg) * effective_a + dg * inv) / 255u);
  uint8_t obc = static_cast<uint8_t>(
      (static_cast<uint32_t>(sb) * effective_a + db * inv) / 255u);
  uint8_t oac =
      static_cast<uint8_t>(effective_a + (da * inv) / 255u);

  return static_cast<uint32_t>(orc) | (static_cast<uint32_t>(ogc) << 8) |
         (static_cast<uint32_t>(obc) << 16) |
         (static_cast<uint32_t>(oac) << 24);
}
}  // namespace

// ---------- BlendGlyphPixel fidelity contract --------------------------------

TEST(PixelBlendTest, BlendGlyphPixelMatchesReferenceWithinOneLsb) {
  struct Case {
    uint32_t dst;
    uint8_t sr, sg, sb, sa;
    uint8_t glyph;
    const char* label;
  };
  const Case cases[] = {
      {0x00000000u, 255, 0, 0, 255, 255, "red-on-transparent-black, full"},
      {0xFFFFFFFFu, 0, 0, 0, 255, 255, "black-on-opaque-white, full"},
      {0x80808080u, 255, 255, 255, 255, 128, "white-on-mid-grey, half"},
      {0xFFFFFFFFu, 255, 255, 255, 255, 0, "white-on-white, zero-coverage"},
      {0xC0403020u, 10, 200, 150, 220, 200, "arbitrary AA edge"},
      {0x01020304u, 253, 127, 64, 200, 17, "low-coverage, low-alpha"},
      {0x7F7F7F7Fu, 1, 2, 3, 7, 255, "dark-text-on-mid-grey"},
      {0xDEADBEEFu, 128, 128, 128, 128, 128, "mid-grey-half-over-noise"},
  };

  for (const auto& c : cases) {
    uint32_t got =
        BlendGlyphPixel(c.dst, c.sr, c.sg, c.sb, c.sa, c.glyph);
    uint32_t expected =
        ReferenceBlendGlyphPixel(c.dst, c.sr, c.sg, c.sb, c.sa, c.glyph);

    for (int ch = 0; ch < 4; ++ch) {
      int got_b = static_cast<int>((got >> (ch * 8)) & 0xFFu);
      int exp_b = static_cast<int>((expected >> (ch * 8)) & 0xFFu);
      int diff = std::abs(got_b - exp_b);
      ASSERT_LE(diff, 1) << "case=" << c.label << " channel=" << ch
                         << " got=" << got_b << " expected=" << exp_b;
    }
  }
}

TEST(PixelBlendTest, BlendGlyphPixelZeroCoverageIsIdentity) {
  // alpha=0 must leave the destination pixel bit-identical (alpha blending
  // over a zero-coverage mask touches nothing).
  uint32_t dst = 0xDEADBEEFu;
  uint32_t got = BlendGlyphPixel(dst, 255, 128, 64, 200, 0);
  EXPECT_EQ(got, dst);
}

// ---------- BlendGlyphRowSSE2 fidelity contract ------------------------------

namespace {
// Run the SSE2 row blitter and compare every output pixel channel against
// the scalar BlendGlyphPixel helper, asserting |diff| <= 1 per channel.
void ExpectSSE2MatchesScalar(std::vector<uint32_t> dst_initial,
                             const std::vector<uint8_t>& alpha, uint8_t sr,
                             uint8_t sg, uint8_t sb, uint8_t sa,
                             const char* label) {
  ASSERT_EQ(dst_initial.size(), alpha.size()) << label;
  std::vector<uint32_t> dst_scalar = dst_initial;
  std::vector<uint32_t> dst_sse = dst_initial;
  const uint32_t n = static_cast<uint32_t>(dst_initial.size());

  for (uint32_t i = 0; i < n; ++i) {
    uint8_t a = alpha[i];
    if (a == 0) continue;
    dst_scalar[i] = BlendGlyphPixel(dst_scalar[i], sr, sg, sb, sa, a);
  }
  BlendGlyphRowSSE2(dst_sse.data(), alpha.data(), n, sr, sg, sb, sa);

  for (uint32_t i = 0; i < n; ++i) {
    for (int ch = 0; ch < 4; ++ch) {
      int got = static_cast<int>((dst_sse[i] >> (ch * 8)) & 0xFFu);
      int exp = static_cast<int>((dst_scalar[i] >> (ch * 8)) & 0xFFu);
      int diff = std::abs(got - exp);
      ASSERT_LE(diff, 1) << label << " pixel=" << i << " channel=" << ch
                         << " got=" << got << " expected=" << exp;
    }
  }
}
}  // namespace

TEST(PixelBlendTest, BlendGlyphRowSSE2MatchesScalarForVariousLayouts) {
  // Cover counts that exercise both the 4-lane SIMD path and the
  // scalar tail: 0, 1, 3 (tail only), 4 (exactly one SIMD iter), 7
  // (one SIMD + 3 tail), 16 (four SIMD iters, tail empty), 19 (four
  // SIMD + 3 tail).
  const uint32_t counts[] = {0, 1, 3, 4, 7, 16, 19};
  for (uint32_t n : counts) {
    std::vector<uint32_t> dst(n);
    std::vector<uint8_t> alpha(n);
    for (uint32_t i = 0; i < n; ++i) {
      dst[i] = 0x10203040u + i * 0x01010101u;
      alpha[i] = static_cast<uint8_t>((i * 37u) & 0xFFu);
    }
    ExpectSSE2MatchesScalar(dst, alpha, 200, 100, 50, 220,
                            ("count=" + std::to_string(n)).c_str());
  }
}

TEST(PixelBlendTest, BlendGlyphRowSSE2ZeroAlphaSkipsPixel) {
  std::vector<uint32_t> dst(5, 0xDEADBEEFu);
  std::vector<uint8_t> alpha(5, 0);
  BlendGlyphRowSSE2(dst.data(), alpha.data(), 5, 255, 255, 255, 255);
  // Scalar tail path: alpha==0 literally skips the pixel, so dst must
  // be bit-identical. SIMD path may blend with effective_a = 0, which
  // should also produce bit-identical output (dst * 255 / 255 == dst,
  // plus rounded-approx error of at most 1). Check ≤ 1 LSB.
  for (uint32_t p : dst) {
    for (int ch = 0; ch < 4; ++ch) {
      int got = static_cast<int>((p >> (ch * 8)) & 0xFFu);
      int exp = static_cast<int>((0xDEADBEEFu >> (ch * 8)) & 0xFFu);
      EXPECT_LE(std::abs(got - exp), 1) << "channel=" << ch;
    }
  }
}

TEST(PixelBlendTest, BlendGlyphRowSSE2FullCoverageOverwritesRGB) {
  std::vector<uint32_t> dst(4, 0xFF000000u);
  std::vector<uint8_t> alpha(4, 255);
  BlendGlyphRowSSE2(dst.data(), alpha.data(), 4, 100, 150, 200, 255);
  for (uint32_t p : dst) {
    EXPECT_LE(std::abs(static_cast<int>(p & 0xFFu) - 100), 1);
    EXPECT_LE(std::abs(static_cast<int>((p >> 8) & 0xFFu) - 150), 1);
    EXPECT_LE(std::abs(static_cast<int>((p >> 16) & 0xFFu) - 200), 1);
    EXPECT_LE(std::abs(static_cast<int>((p >> 24) & 0xFFu) - 255), 1);
  }
}

// ---------- BlendGlyphRow dispatch (AVX2 if available) fidelity -------------

namespace {
void ExpectDispatchMatchesScalar(std::vector<uint32_t> dst_initial,
                                 const std::vector<uint8_t>& alpha,
                                 uint8_t sr, uint8_t sg, uint8_t sb,
                                 uint8_t sa, const char* label) {
  ASSERT_EQ(dst_initial.size(), alpha.size()) << label;
  std::vector<uint32_t> dst_scalar = dst_initial;
  std::vector<uint32_t> dst_disp = dst_initial;
  const uint32_t n = static_cast<uint32_t>(dst_initial.size());

  for (uint32_t i = 0; i < n; ++i) {
    uint8_t a = alpha[i];
    if (a == 0) continue;
    dst_scalar[i] = BlendGlyphPixel(dst_scalar[i], sr, sg, sb, sa, a);
  }
  BlendGlyphRow(dst_disp.data(), alpha.data(), n, sr, sg, sb, sa);

  for (uint32_t i = 0; i < n; ++i) {
    for (int ch = 0; ch < 4; ++ch) {
      int got = static_cast<int>((dst_disp[i] >> (ch * 8)) & 0xFFu);
      int exp = static_cast<int>((dst_scalar[i] >> (ch * 8)) & 0xFFu);
      int diff = std::abs(got - exp);
      ASSERT_LE(diff, 1) << label << " pixel=" << i << " channel=" << ch
                         << " got=" << got << " expected=" << exp;
    }
  }
}
}  // namespace

TEST(PixelBlendTest, BlendGlyphRowDispatchCoversAVX2AndSSE2AndTailLayouts) {
  // Counts span both the AVX2 (≥8) and SSE2 (≥4) fast paths plus
  // assorted scalar tails. 17/23/31 force AVX + SSE + scalar tail
  // mixes in a single call.
  const uint32_t counts[] = {0, 1, 3, 4, 7, 8, 9, 15, 16, 17, 23, 31, 64};
  for (uint32_t n : counts) {
    std::vector<uint32_t> dst(n);
    std::vector<uint8_t> alpha(n);
    for (uint32_t i = 0; i < n; ++i) {
      dst[i] = 0x11223344u + i * 0x05030107u;
      alpha[i] = static_cast<uint8_t>(((i + 3) * 53u) & 0xFFu);
    }
    ExpectDispatchMatchesScalar(dst, alpha, 200, 100, 50, 220,
                                ("dispatch count=" + std::to_string(n)).c_str());
  }
}

TEST(PixelBlendTest, BlendGlyphRowDispatchAgreesWithSSE2Path) {
  // Feeds the same input through both the dispatch-selected (possibly
  // AVX2) and the hard-coded SSE2 path; any mismatch would mean the
  // AVX2 lane-layout re-alignment is broken. Tolerance 1 LSB.
  const uint32_t n = 33;
  std::vector<uint32_t> dst_avx(n), dst_sse(n);
  std::vector<uint8_t> alpha(n);
  for (uint32_t i = 0; i < n; ++i) {
    const uint32_t seed = 0xABCDEF01u + i * 0x00010203u;
    dst_avx[i] = seed;
    dst_sse[i] = seed;
    alpha[i] = static_cast<uint8_t>((i * 11u) & 0xFFu);
  }
  BlendGlyphRow(dst_avx.data(), alpha.data(), n, 17, 200, 63, 191);
  BlendGlyphRowSSE2(dst_sse.data(), alpha.data(), n, 17, 200, 63, 191);

  for (uint32_t i = 0; i < n; ++i) {
    for (int ch = 0; ch < 4; ++ch) {
      int a = static_cast<int>((dst_avx[i] >> (ch * 8)) & 0xFFu);
      int b = static_cast<int>((dst_sse[i] >> (ch * 8)) & 0xFFu);
      EXPECT_LE(std::abs(a - b), 1) << "pixel=" << i << " channel=" << ch;
    }
  }
}

TEST(PixelBlendTest, BlendGlyphRowSSE2StressAllAlphasAllChannels) {
  // Hit every (alpha, channel) interaction over a 256-pixel row against
  // a reference scalar blend.
  std::vector<uint32_t> dst(256);
  std::vector<uint8_t> alpha(256);
  for (uint32_t i = 0; i < 256; ++i) {
    dst[i] = static_cast<uint32_t>(i) |
             (static_cast<uint32_t>(255 - i) << 8) |
             (static_cast<uint32_t>((i * 7) & 0xFFu) << 16) |
             (static_cast<uint32_t>((i * 13) & 0xFFu) << 24);
    alpha[i] = static_cast<uint8_t>(i);
  }
  ExpectSSE2MatchesScalar(dst, alpha, 17, 200, 63, 191, "stress-256");
}

TEST(PixelBlendTest, BlendGlyphPixelFullOpaqueOverwritesRGB) {
  // sa = 255, glyph = 255: destination RGB should be (approximately) the
  // source colour since effective_a becomes 255 and inv becomes 0.
  uint32_t dst = 0xFF000000u;
  uint32_t got = BlendGlyphPixel(dst, 100, 150, 200, 255, 255);
  EXPECT_EQ(got & 0xFFu, 100u);
  EXPECT_EQ((got >> 8) & 0xFFu, 150u);
  EXPECT_EQ((got >> 16) & 0xFFu, 200u);
  EXPECT_EQ((got >> 24) & 0xFFu, 255u);
}

}  // namespace
}  // namespace vx::gfx::sw
