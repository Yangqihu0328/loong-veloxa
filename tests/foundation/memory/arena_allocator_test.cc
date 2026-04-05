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

}  // namespace
}  // namespace vx
