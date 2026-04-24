#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>

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
