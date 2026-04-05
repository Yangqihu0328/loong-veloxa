#include "veloxa/foundation/memory/pool_allocator.h"

#include <cstring>
#include <set>

#include <gtest/gtest.h>

namespace vx {
namespace {

TEST(PoolAllocatorTest, BasicAllocateAndDeallocate) {
  PoolAllocator pool(64);
  void* ptr = pool.Allocate(64);
  ASSERT_NE(ptr, nullptr);
  std::memset(ptr, 0xAB, 64);
  pool.Deallocate(ptr, 64);
}

TEST(PoolAllocatorTest, Reuse) {
  PoolAllocator pool(32, 4);
  void* p1 = pool.Allocate(32);
  pool.Deallocate(p1, 32);
  void* p2 = pool.Allocate(32);
  EXPECT_EQ(p1, p2);
}

TEST(PoolAllocatorTest, MultipleAllocations) {
  PoolAllocator pool(sizeof(int), 8);
  constexpr int kCount = 8;
  std::set<void*> ptrs;
  for (int i = 0; i < kCount; ++i) {
    void* p = pool.Allocate(sizeof(int));
    ASSERT_NE(p, nullptr);
    ptrs.insert(p);
  }
  EXPECT_EQ(ptrs.size(), static_cast<size_t>(kCount));
}

TEST(PoolAllocatorTest, ChunkExpansion) {
  PoolAllocator pool(16, 4);
  // Allocate more than one chunk's worth
  for (int i = 0; i < 10; ++i) {
    void* p = pool.Allocate(16);
    ASSERT_NE(p, nullptr);
  }
}

TEST(PoolAllocatorTest, DeallocateNull) {
  PoolAllocator pool(32);
  pool.Deallocate(nullptr, 32);  // should not crash
}

TEST(PoolAllocatorTest, BlockSizeMinimum) {
  PoolAllocator pool(1);  // should be bumped to sizeof(FreeNode)
  EXPECT_GE(pool.block_size(), sizeof(void*));
  void* p = pool.Allocate(1);
  ASSERT_NE(p, nullptr);
  pool.Deallocate(p, 1);
}

}  // namespace
}  // namespace vx
