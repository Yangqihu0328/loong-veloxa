#include "veloxa/foundation/containers/small_vector.h"

#include <string>

#include <gtest/gtest.h>

namespace vx {
namespace {

TEST(SmallVectorTest, DefaultConstructorInline) {
  SmallVector<int, 4> v;
  EXPECT_EQ(v.size(), 0u);
  EXPECT_TRUE(v.empty());
  EXPECT_TRUE(v.IsInline());
}

TEST(SmallVectorTest, InlineStorageNoHeap) {
  SmallVector<int, 8> v;
  for (int i = 0; i < 8; ++i) {
    v.push_back(i);
  }
  EXPECT_EQ(v.size(), 8u);
  EXPECT_TRUE(v.IsInline());

  // Verify data pointer is within the object's address range
  auto obj_begin = reinterpret_cast<uintptr_t>(&v);
  auto obj_end = obj_begin + sizeof(v);
  auto data_ptr = reinterpret_cast<uintptr_t>(v.data());
  EXPECT_GE(data_ptr, obj_begin);
  EXPECT_LT(data_ptr, obj_end);
}

TEST(SmallVectorTest, OverflowToHeap) {
  SmallVector<int, 4> v;
  for (int i = 0; i < 5; ++i) {
    v.push_back(i);
  }
  EXPECT_EQ(v.size(), 5u);
  EXPECT_FALSE(v.IsInline());
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(v[static_cast<usize>(i)], i);
  }
}

TEST(SmallVectorTest, InitializerListInline) {
  SmallVector<int, 4> v = {1, 2, 3};
  EXPECT_EQ(v.size(), 3u);
  EXPECT_TRUE(v.IsInline());
  EXPECT_EQ(v[0], 1);
  EXPECT_EQ(v[2], 3);
}

TEST(SmallVectorTest, InitializerListOverflow) {
  SmallVector<int, 2> v = {1, 2, 3, 4};
  EXPECT_EQ(v.size(), 4u);
  EXPECT_FALSE(v.IsInline());
}

TEST(SmallVectorTest, PushBackAndPopBack) {
  SmallVector<int, 4> v;
  v.push_back(10);
  v.push_back(20);
  v.push_back(30);
  EXPECT_EQ(v.size(), 3u);
  EXPECT_EQ(v.back(), 30);
  v.pop_back();
  EXPECT_EQ(v.size(), 2u);
  EXPECT_EQ(v.back(), 20);
}

TEST(SmallVectorTest, EmplaceBack) {
  SmallVector<std::string, 4> v;
  v.emplace_back("hello");
  v.emplace_back(3, 'x');
  EXPECT_EQ(v[0], "hello");
  EXPECT_EQ(v[1], "xxx");
}

TEST(SmallVectorTest, CopyInline) {
  SmallVector<int, 4> v = {1, 2, 3};
  SmallVector<int, 4> v2(v);
  EXPECT_EQ(v2.size(), 3u);
  EXPECT_TRUE(v2.IsInline());
  EXPECT_EQ(v2[0], 1);
  v2.push_back(4);
  EXPECT_EQ(v.size(), 3u);
}

TEST(SmallVectorTest, CopyHeap) {
  SmallVector<int, 2> v = {1, 2, 3, 4};
  SmallVector<int, 2> v2(v);
  EXPECT_EQ(v2.size(), 4u);
  EXPECT_FALSE(v2.IsInline());
  for (usize i = 0; i < 4; ++i) {
    EXPECT_EQ(v2[i], static_cast<int>(i + 1));
  }
}

TEST(SmallVectorTest, MoveInline) {
  SmallVector<int, 4> v = {1, 2, 3};
  SmallVector<int, 4> v2(std::move(v));
  EXPECT_EQ(v2.size(), 3u);
  EXPECT_TRUE(v2.IsInline());
  EXPECT_EQ(v2[0], 1);
  EXPECT_EQ(v.size(), 0u);
}

TEST(SmallVectorTest, MoveHeap) {
  SmallVector<int, 2> v = {1, 2, 3, 4};
  EXPECT_FALSE(v.IsInline());
  SmallVector<int, 2> v2(std::move(v));
  EXPECT_EQ(v2.size(), 4u);
  EXPECT_FALSE(v2.IsInline());
  EXPECT_EQ(v.size(), 0u);
}

TEST(SmallVectorTest, ClearStaysInline) {
  SmallVector<int, 4> v = {1, 2, 3};
  v.clear();
  EXPECT_EQ(v.size(), 0u);
  EXPECT_TRUE(v.empty());
}

TEST(SmallVectorTest, InsertAndErase) {
  SmallVector<int, 8> v = {1, 2, 4, 5};
  v.insert(v.begin() + 2, 3);
  EXPECT_EQ(v.size(), 5u);
  for (usize i = 0; i < 5; ++i) {
    EXPECT_EQ(v[i], static_cast<int>(i + 1));
  }
  v.erase(v.begin() + 2);
  EXPECT_EQ(v.size(), 4u);
  EXPECT_EQ(v[2], 4);
}

TEST(SmallVectorTest, RangeFor) {
  SmallVector<int, 4> v = {10, 20, 30};
  int sum = 0;
  for (int x : v) {
    sum += x;
  }
  EXPECT_EQ(sum, 60);
}

TEST(SmallVectorTest, FrontBack) {
  SmallVector<int, 4> v = {1, 2, 3};
  EXPECT_EQ(v.front(), 1);
  EXPECT_EQ(v.back(), 3);
}

TEST(SmallVectorTest, Resize) {
  SmallVector<int, 4> v = {1, 2};
  v.resize(4);
  EXPECT_EQ(v.size(), 4u);
  EXPECT_EQ(v[0], 1);
  EXPECT_EQ(v[2], 0);
  EXPECT_TRUE(v.IsInline());

  v.resize(1);
  EXPECT_EQ(v.size(), 1u);
}

TEST(SmallVectorTest, StringType) {
  SmallVector<std::string, 2> v;
  v.push_back("hello");
  v.push_back("world");
  EXPECT_TRUE(v.IsInline());
  v.push_back("overflow");
  EXPECT_FALSE(v.IsInline());
  EXPECT_EQ(v[0], "hello");
  EXPECT_EQ(v[2], "overflow");
}

}  // namespace
}  // namespace vx
