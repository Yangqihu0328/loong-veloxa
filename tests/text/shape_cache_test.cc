// tests/text/shape_cache_test.cc
//
// Coverage (TASK-20260424-04):
//   Phase 1 (P1) — HashBytesU64 FNV-1a byte hash (3 tests)
//   Phase 2 (P2) — ShapeCache FIFO cache contract (7 tests) — appended in P2.1
//
// See docs/plans/2026-04-24-drawtext-shape-cache.md Phase 1 & 2.

#include <gtest/gtest.h>

#include "veloxa/foundation/base/hash.h"

namespace {

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

}  // namespace
