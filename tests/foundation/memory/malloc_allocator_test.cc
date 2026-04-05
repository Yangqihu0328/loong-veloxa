#include "veloxa/foundation/memory/malloc_allocator.h"

#include <cstring>

#include <gtest/gtest.h>

namespace vx {
namespace {

TEST(MallocAllocatorTest, BasicAllocateAndDeallocate) {
  MallocAllocator alloc;
  void* ptr = alloc.Allocate(128);
  ASSERT_NE(ptr, nullptr);
  std::memset(ptr, 0xAB, 128);
  alloc.Deallocate(ptr, 128);
}

TEST(MallocAllocatorTest, AlignmentDefault) {
  MallocAllocator alloc;
  void* ptr = alloc.Allocate(64);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) % alignof(std::max_align_t), 0u);
  alloc.Deallocate(ptr, 64);
}

TEST(MallocAllocatorTest, Alignment64) {
  MallocAllocator alloc;
  void* ptr = alloc.Allocate(256, 64);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) % 64, 0u);
  alloc.Deallocate(ptr, 256);
}

TEST(MallocAllocatorTest, StatsTracking) {
  GlobalMemoryStats().Reset();
  MallocAllocator alloc;

  void* p1 = alloc.Allocate(100);
  EXPECT_EQ(GlobalMemoryStats().current_bytes.load(), 100);
  EXPECT_EQ(GlobalMemoryStats().total_allocations.load(), 1);

  void* p2 = alloc.Allocate(200);
  EXPECT_EQ(GlobalMemoryStats().current_bytes.load(), 300);
  EXPECT_EQ(GlobalMemoryStats().peak_bytes.load(), 300);
  EXPECT_EQ(GlobalMemoryStats().total_allocations.load(), 2);

  alloc.Deallocate(p1, 100);
  EXPECT_EQ(GlobalMemoryStats().current_bytes.load(), 200);
  EXPECT_EQ(GlobalMemoryStats().peak_bytes.load(), 300);

  alloc.Deallocate(p2, 200);
  EXPECT_EQ(GlobalMemoryStats().current_bytes.load(), 0);
}

TEST(MallocAllocatorTest, DeallocateNull) {
  MallocAllocator alloc;
  alloc.Deallocate(nullptr, 0);  // should not crash
}

TEST(MallocAllocatorTest, MultipleSmallAllocations) {
  MallocAllocator alloc;
  constexpr int kCount = 100;
  void* ptrs[kCount];
  for (int i = 0; i < kCount; ++i) {
    ptrs[i] = alloc.Allocate(16);
    ASSERT_NE(ptrs[i], nullptr);
  }
  for (int i = 0; i < kCount; ++i) {
    alloc.Deallocate(ptrs[i], 16);
  }
}

}  // namespace
}  // namespace vx
