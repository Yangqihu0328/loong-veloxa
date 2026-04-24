// tests/graphics/drawtext_shape_cache_test.cc
//
// TASK-20260424-04 Phase 3 integration tests: validates that DrawText
// produces pixel-byte-identical output with or without hb_shape caching
// hit, and exercises ClearShapeCache / interleaved text safety.
//
// Requires system font DejaVuSans (same as bench_drawtext.cc). Tests are
// gated via ASSERT on font load: if the system font is missing the suite
// fails fast rather than silently degrading to fallback (repeating the
// TASK-05 K1 false-positive mode).

#include <gtest/gtest.h>

#include <cstring>
#include <vector>

#include "veloxa/foundation/strings/string_view.h"
#include "veloxa/graphics/brush.h"
#include "veloxa/graphics/software/software_canvas.h"
#include "veloxa/graphics/types.h"
#include "veloxa/text/font_manager.h"
#include "veloxa/text/glyph_cache.h"

namespace vx::gfx::sw {
namespace {

constexpr vx::u32 kW = 128;
constexpr vx::u32 kH = 64;
constexpr vx::u32 kStrideBytes = kW * 4;
constexpr const char* kFontPath =
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";

class DrawTextShapeCacheTest : public ::testing::Test {
 protected:
  vx::text::FontManager fm_;
  vx::text::GlyphCache gc_;

  void SetUp() override {
    ASSERT_TRUE(fm_.Init().ok()) << "FontManager.Init failed";
    auto r = fm_.LoadFont(vx::StringView(kFontPath), vx::StringView("DejaVu"));
    ASSERT_TRUE(r.ok()) << "Cannot load " << kFontPath
                        << "; test requires this system font";
  }

  void Draw(std::vector<vx::u32>& target, vx::StringView text) {
    std::fill(target.begin(), target.end(), 0u);
    SoftwareCanvas canvas(target.data(), kW, kH, kStrideBytes, &fm_, &gc_);
    auto brush = Brush::Solid(Color{255, 255, 255, 255});
    Rect bounds{0.0f, 0.0f, static_cast<vx::f32>(kW),
                static_cast<vx::f32>(kH)};
    canvas.Begin();
    canvas.DrawText(text, bounds, 12.0f, brush);
    canvas.End();
  }
};

TEST_F(DrawTextShapeCacheTest, SameTextTwice_PixelByteIdentical) {
  std::vector<vx::u32> a(kW * kH, 0), b(kW * kH, 0);
  Draw(a, vx::StringView("Hello World"));
  // Second call exercises the cache-hit path.
  Draw(b, vx::StringView("Hello World"));
  EXPECT_EQ(std::memcmp(a.data(), b.data(), a.size() * sizeof(vx::u32)), 0);
}

TEST_F(DrawTextShapeCacheTest, AfterClearShapeCache_PixelByteIdentical) {
  std::vector<vx::u32> a(kW * kH, 0), b(kW * kH, 0);
  Draw(a, vx::StringView("Hello World"));
  fm_.ClearShapeCache();
  // After Clear the path is back to miss; output must still be identical.
  Draw(b, vx::StringView("Hello World"));
  EXPECT_EQ(std::memcmp(a.data(), b.data(), a.size() * sizeof(vx::u32)), 0);
}

TEST_F(DrawTextShapeCacheTest, InterleavedTexts_StableHits) {
  std::vector<vx::u32> a1(kW * kH, 0), a2(kW * kH, 0), b(kW * kH, 0);
  Draw(a1, vx::StringView("Alpha"));
  Draw(b, vx::StringView("Beta"));
  // Return to "Alpha" — must hit cache and produce identical bytes.
  Draw(a2, vx::StringView("Alpha"));
  EXPECT_EQ(std::memcmp(a1.data(), a2.data(), a1.size() * sizeof(vx::u32)), 0);
}

// R2 end-to-end: different text content must produce different pixels
// (guards against fingerprint collisions silently reusing another shape).
TEST_F(DrawTextShapeCacheTest, DifferentTexts_DifferentOutput) {
  std::vector<vx::u32> a(kW * kH, 0), b(kW * kH, 0);
  Draw(a, vx::StringView("ABC"));
  Draw(b, vx::StringView("XYZ"));
  EXPECT_NE(std::memcmp(a.data(), b.data(), a.size() * sizeof(vx::u32)), 0);
}

}  // namespace
}  // namespace vx::gfx::sw
