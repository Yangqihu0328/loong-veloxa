// tests/text/shape_cache_test.cc
//
// Coverage (TASK-20260424-04):
//   Phase 1 (P1) — HashBytesU64 FNV-1a byte hash (3 tests)
//   Phase 2 (P2) — ShapeCache FIFO cache contract (7 tests)
//
// See docs/plans/2026-04-24-drawtext-shape-cache.md Phase 1 & 2.

#include <gtest/gtest.h>

#include <cstdio>
#include <initializer_list>
#include <string>

#include "veloxa/foundation/base/hash.h"
#include "veloxa/foundation/strings/string_view.h"
#include "veloxa/text/shape_cache.h"

namespace {

// ----------------------------------------------------------------------------
// Phase 1 — HashBytesU64 FNV-1a byte hash
// ----------------------------------------------------------------------------

TEST(HashBytesU64, EmptyInput_ReturnsFnvBasis) {
  constexpr vx::u64 kFnvOffsetBasis = 0xCBF29CE484222325ULL;
  EXPECT_EQ(vx::HashBytesU64(nullptr, 0), kFnvOffsetBasis);
  const vx::u8 empty[1] = {0};
  EXPECT_EQ(vx::HashBytesU64(empty, 0), kFnvOffsetBasis);
}

TEST(HashBytesU64, SingleByteDifference_ChangesHash) {
  const vx::u8 a[] = {'a'};
  const vx::u8 b[] = {'b'};
  EXPECT_NE(vx::HashBytesU64(a, 1), vx::HashBytesU64(b, 1));
}

TEST(HashBytesU64, LengthAffectsHash) {
  // Same byte prefix, different lengths -> different hashes.
  const vx::u8 data[] = {'x', 'y'};
  EXPECT_NE(vx::HashBytesU64(data, 1), vx::HashBytesU64(data, 2));
}

// ----------------------------------------------------------------------------
// Phase 2 — ShapeCache FIFO cache contract
// ----------------------------------------------------------------------------

namespace {

vx::text::ShapeCacheKey MakeKey(vx::text::FontHandle font, vx::u32 px,
                                 vx::StringView text) {
  return vx::text::ShapeCacheKey{
      font, px,
      vx::HashBytesU64(reinterpret_cast<const vx::u8*>(text.data()),
                       text.size()),
      static_cast<vx::u32>(text.size())};
}

vx::text::ShapedRun MakeRun(std::initializer_list<vx::u32> glyph_ids) {
  vx::text::ShapedRun r;
  r.glyphs.reserve(glyph_ids.size());
  for (auto id : glyph_ids) {
    r.glyphs.push_back(vx::text::ShapedGlyph{id, 0.0f, 0.0f, 10.0f});
  }
  return r;
}

}  // namespace

TEST(ShapeCache, LookupEmpty_ReturnsNull) {
  vx::text::ShapeCache c;
  EXPECT_EQ(c.LookupOrNull(MakeKey(1, 12, vx::StringView("hi"))), nullptr);
  EXPECT_EQ(c.size(), 0u);
}

TEST(ShapeCache, InsertThenLookup_ReturnsStored) {
  vx::text::ShapeCache c;
  auto key = MakeKey(1, 12, vx::StringView("hi"));
  vx::text::ShapedRun* inserted = c.Insert(key, MakeRun({42, 43}));
  ASSERT_NE(inserted, nullptr);
  const vx::text::ShapedRun* found = c.LookupOrNull(key);
  ASSERT_NE(found, nullptr);
  EXPECT_EQ(found, inserted);
  ASSERT_EQ(found->glyphs.size(), 2u);
  EXPECT_EQ(found->glyphs[0].glyph_id, 42u);
  EXPECT_EQ(found->glyphs[1].glyph_id, 43u);
  EXPECT_EQ(c.size(), 1u);
}

TEST(ShapeCache, InsertBeyondCapacity_FIFOEvictsOldest) {
  vx::text::ShapeCache c;
  char buf[16];
  // Fill exactly 128 entries.
  for (vx::usize i = 0; i < vx::text::ShapeCache::kCapacity; ++i) {
    std::snprintf(buf, sizeof(buf), "t%03zu", i);
    c.Insert(MakeKey(1, 12, vx::StringView(buf)),
             MakeRun({static_cast<vx::u32>(i)}));
  }
  EXPECT_EQ(c.size(), vx::text::ShapeCache::kCapacity);
  // First entry ("t000") must still be retrievable right before overflow.
  EXPECT_NE(c.LookupOrNull(MakeKey(1, 12, vx::StringView("t000"))), nullptr);
  // Insert the 129th entry -> FIFO evicts entry 0 ("t000").
  std::snprintf(buf, sizeof(buf), "t%03d", 128);
  c.Insert(MakeKey(1, 12, vx::StringView(buf)), MakeRun({999}));
  // Count stays at capacity.
  EXPECT_EQ(c.size(), vx::text::ShapeCache::kCapacity);
  // "t000" should have been evicted.
  EXPECT_EQ(c.LookupOrNull(MakeKey(1, 12, vx::StringView("t000"))), nullptr);
  // "t001" still there.
  EXPECT_NE(c.LookupOrNull(MakeKey(1, 12, vx::StringView("t001"))), nullptr);
  // Newest ("t128") hits.
  EXPECT_NE(c.LookupOrNull(MakeKey(1, 12, vx::StringView("t128"))), nullptr);
}

TEST(ShapeCache, KeyMismatch_Font_Miss) {
  vx::text::ShapeCache c;
  c.Insert(MakeKey(1, 12, vx::StringView("hi")), MakeRun({1}));
  EXPECT_EQ(c.LookupOrNull(MakeKey(2, 12, vx::StringView("hi"))), nullptr);
  // Same font still hits.
  EXPECT_NE(c.LookupOrNull(MakeKey(1, 12, vx::StringView("hi"))), nullptr);
}

TEST(ShapeCache, KeyMismatch_PixelSize_Miss) {
  vx::text::ShapeCache c;
  c.Insert(MakeKey(1, 12, vx::StringView("hi")), MakeRun({1}));
  EXPECT_EQ(c.LookupOrNull(MakeKey(1, 14, vx::StringView("hi"))), nullptr);
  EXPECT_NE(c.LookupOrNull(MakeKey(1, 12, vx::StringView("hi"))), nullptr);
}

TEST(ShapeCache, FingerprintEq_LenMismatch_Miss) {
  // Directly craft two keys sharing fingerprint but differing in text_len
  // to exercise the collision-degradation guard (design doc §6.2).
  vx::text::ShapeCache c;
  vx::text::ShapeCacheKey k1{1, 12, 0xDEADBEEFDEADBEEFULL, 2};
  vx::text::ShapeCacheKey k2{1, 12, 0xDEADBEEFDEADBEEFULL, 3};
  c.Insert(k1, MakeRun({42}));
  // text_len differs -> must miss, even though fingerprint matches.
  EXPECT_EQ(c.LookupOrNull(k2), nullptr);
  // Original key still hits.
  EXPECT_NE(c.LookupOrNull(k1), nullptr);
}

TEST(ShapeCache, Clear_EmptiesButPreservesCapacity) {
  vx::text::ShapeCache c;
  c.Insert(MakeKey(1, 12, vx::StringView("hi")), MakeRun({1}));
  c.Insert(MakeKey(1, 12, vx::StringView("world")), MakeRun({2, 3}));
  EXPECT_EQ(c.size(), 2u);
  c.Clear();
  EXPECT_EQ(c.size(), 0u);
  EXPECT_EQ(c.LookupOrNull(MakeKey(1, 12, vx::StringView("hi"))), nullptr);
  EXPECT_EQ(c.LookupOrNull(MakeKey(1, 12, vx::StringView("world"))), nullptr);
  // Post-Clear insert must work.
  c.Insert(MakeKey(1, 12, vx::StringView("hi")), MakeRun({99}));
  EXPECT_EQ(c.size(), 1u);
  const vx::text::ShapedRun* found =
      c.LookupOrNull(MakeKey(1, 12, vx::StringView("hi")));
  ASSERT_NE(found, nullptr);
  ASSERT_EQ(found->glyphs.size(), 1u);
  EXPECT_EQ(found->glyphs[0].glyph_id, 99u);
}

}  // namespace
