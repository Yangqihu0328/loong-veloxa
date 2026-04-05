#include "veloxa/foundation/base/span.h"

#include <gtest/gtest.h>

namespace vx {
namespace {

TEST(SpanTest, DefaultConstruction) {
  Span<int> s;
  EXPECT_EQ(s.data(), nullptr);
  EXPECT_EQ(s.size(), 0u);
  EXPECT_TRUE(s.empty());
}

TEST(SpanTest, PointerSizeConstruction) {
  int arr[] = {1, 2, 3};
  Span<int> s(arr, 3);
  EXPECT_EQ(s.data(), arr);
  EXPECT_EQ(s.size(), 3u);
  EXPECT_FALSE(s.empty());
}

TEST(SpanTest, ArrayConstruction) {
  int arr[] = {10, 20, 30, 40};
  Span<int> s(arr);
  EXPECT_EQ(s.size(), 4u);
  EXPECT_EQ(s[0], 10);
  EXPECT_EQ(s[3], 40);
}

TEST(SpanTest, Indexing) {
  int arr[] = {5, 10, 15};
  Span<int> s(arr);
  EXPECT_EQ(s[0], 5);
  EXPECT_EQ(s[1], 10);
  EXPECT_EQ(s[2], 15);
}

TEST(SpanTest, FrontBack) {
  int arr[] = {1, 2, 3};
  Span<int> s(arr);
  EXPECT_EQ(s.front(), 1);
  EXPECT_EQ(s.back(), 3);
}

TEST(SpanTest, SubspanFull) {
  int arr[] = {1, 2, 3, 4, 5};
  Span<int> s(arr);
  auto sub = s.subspan(0);
  EXPECT_EQ(sub.size(), 5u);
  EXPECT_EQ(sub[0], 1);
}

TEST(SpanTest, SubspanOffset) {
  int arr[] = {1, 2, 3, 4, 5};
  Span<int> s(arr);
  auto sub = s.subspan(2);
  EXPECT_EQ(sub.size(), 3u);
  EXPECT_EQ(sub[0], 3);
  EXPECT_EQ(sub[2], 5);
}

TEST(SpanTest, SubspanOffsetCount) {
  int arr[] = {1, 2, 3, 4, 5};
  Span<int> s(arr);
  auto sub = s.subspan(1, 2);
  EXPECT_EQ(sub.size(), 2u);
  EXPECT_EQ(sub[0], 2);
  EXPECT_EQ(sub[1], 3);
}

TEST(SpanTest, Iterator) {
  int arr[] = {10, 20, 30};
  Span<int> s(arr);
  int sum = 0;
  for (int v : s) {
    sum += v;
  }
  EXPECT_EQ(sum, 60);
}

TEST(SpanTest, ConstSpan) {
  const int arr[] = {1, 2, 3};
  Span<const int> s(arr);
  EXPECT_EQ(s.size(), 3u);
  EXPECT_EQ(s[1], 2);
}

TEST(SpanTest, Mutation) {
  int arr[] = {1, 2, 3};
  Span<int> s(arr);
  s[1] = 42;
  EXPECT_EQ(arr[1], 42);
}

}  // namespace
}  // namespace vx
