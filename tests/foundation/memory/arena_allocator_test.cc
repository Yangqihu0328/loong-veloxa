#include "veloxa/foundation/memory/arena_allocator.h"

#include <cstring>

#include <gtest/gtest.h>

namespace vx {
namespace {

TEST(ArenaAllocatorTest, BasicAllocation) {
  ArenaAllocator arena(1024);
  void* ptr = arena.Allocate(64);
  ASSERT_NE(ptr, nullptr);
  std::memset(ptr, 0xAB, 64);
}

TEST(ArenaAllocatorTest, MultipleAllocationsInSameBlock) {
  ArenaAllocator arena(1024);
  void* p1 = arena.Allocate(100);
  void* p2 = arena.Allocate(100);
  ASSERT_NE(p1, nullptr);
  ASSERT_NE(p2, nullptr);
  EXPECT_NE(p1, p2);
  EXPECT_EQ(arena.bytes_allocated(), 200u);
}

TEST(ArenaAllocatorTest, SpansMultipleBlocks) {
  ArenaAllocator arena(128);
  void* p1 = arena.Allocate(100);
  void* p2 = arena.Allocate(100);
  ASSERT_NE(p1, nullptr);
  ASSERT_NE(p2, nullptr);
  EXPECT_EQ(arena.bytes_allocated(), 200u);
}

TEST(ArenaAllocatorTest, OversizedAllocation) {
  ArenaAllocator arena(64);
  void* ptr = arena.Allocate(256);
  ASSERT_NE(ptr, nullptr);
  std::memset(ptr, 0xCD, 256);
}

TEST(ArenaAllocatorTest, Reset) {
  ArenaAllocator arena(1024);
  arena.Allocate(100);
  arena.Allocate(200);
  EXPECT_EQ(arena.bytes_allocated(), 300u);

  arena.Reset();
  EXPECT_EQ(arena.bytes_allocated(), 0u);

  void* ptr = arena.Allocate(50);
  ASSERT_NE(ptr, nullptr);
  EXPECT_EQ(arena.bytes_allocated(), 50u);
}

TEST(ArenaAllocatorTest, DeallocateIsNoop) {
  ArenaAllocator arena(1024);
  void* ptr = arena.Allocate(64);
  arena.Deallocate(ptr, 64);  // should not crash
  EXPECT_EQ(arena.bytes_allocated(), 64u);
}

TEST(ArenaAllocatorTest, Alignment) {
  ArenaAllocator arena(1024);
  arena.Allocate(1);  // offset by 1
  void* ptr = arena.Allocate(64, 64);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) % 64, 0u);
}

TEST(ArenaAllocatorTest, DefaultBlockSizeFitsLargeAllocations) {
  // TASK-20260424-01: default block_size must accommodate two 16 KB chunks
  // in a single block. 4 KB defaults caused malloc/free churn in the layout
  // hot path (BM_LayoutBuildTreeFlat/128→256 = 9.4× super-linear knee), which
  // Phase 2 block-size sweep proved resolvable by bumping the default to
  // 32768 (R256 9.42× → 3.84×, R_flex 16.49× → 7.40×, both ~2.5× improvement).
  //
  // This test locks the default behaviourally: two 16 KB allocations must
  // land contiguously in the initial block. If default drops below 32 KB,
  // the second allocation triggers the oversized-block path (new malloc),
  // breaking pointer contiguity and this assertion.
  //
  // See docs/specs/2026-04-24-layout-knee-root-cause-design.md §5.
  ArenaAllocator arena;
  constexpr usize kChunk = 16384;
  char* p1 = static_cast<char*>(arena.Allocate(kChunk));
  char* p2 = static_cast<char*>(arena.Allocate(kChunk));
  EXPECT_EQ(p2 - p1, static_cast<ptrdiff_t>(kChunk))
      << "Two 16 KB allocations must fit contiguously in the initial block; "
         "default block_size is too small (expected >= 32768 to avoid layout "
         "hot-path malloc churn per TASK-20260424-01).";
}

}  // namespace
}  // namespace vx
