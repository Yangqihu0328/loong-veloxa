#include <gtest/gtest.h>
#include "veloxa/text/glyph_cache.h"

namespace vx::text {

TEST(GlyphCacheTest, GetMissReturnsNull) {
  GlyphCache cache;
  EXPECT_EQ(cache.Get(1, 65, 16), nullptr);
}

TEST(GlyphCacheTest, PutAndGet) {
  GlyphCache cache;
  GlyphBitmap bmp;
  bmp.width = 10;
  bmp.height = 12;
  bmp.advance = 8.5f;
  cache.Put(1, 65, 16, static_cast<GlyphBitmap&&>(bmp));
  EXPECT_EQ(cache.size(), 1u);

  auto* result = cache.Get(1, 65, 16);
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(result->width, 10u);
  EXPECT_EQ(result->height, 12u);
  EXPECT_FLOAT_EQ(result->advance, 8.5f);
}

TEST(GlyphCacheTest, DifferentSizesIndependent) {
  GlyphCache cache;
  GlyphBitmap bmp16, bmp24;
  bmp16.width = 8;
  bmp24.width = 12;
  cache.Put(1, 65, 16, static_cast<GlyphBitmap&&>(bmp16));
  cache.Put(1, 65, 24, static_cast<GlyphBitmap&&>(bmp24));
  EXPECT_EQ(cache.size(), 2u);
  EXPECT_EQ(cache.Get(1, 65, 16)->width, 8u);
  EXPECT_EQ(cache.Get(1, 65, 24)->width, 12u);
}

TEST(GlyphCacheTest, Clear) {
  GlyphCache cache;
  GlyphBitmap bmp;
  cache.Put(1, 65, 16, static_cast<GlyphBitmap&&>(bmp));
  EXPECT_EQ(cache.size(), 1u);
  cache.Clear();
  EXPECT_EQ(cache.size(), 0u);
  EXPECT_EQ(cache.Get(1, 65, 16), nullptr);
}

}  // namespace vx::text
