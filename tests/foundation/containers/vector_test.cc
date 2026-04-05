#include "veloxa/foundation/containers/vector.h"

#include <string>

#include <gtest/gtest.h>

namespace vx {
namespace {

struct LifetimeTracker {
  static int alive;
  static int constructed;

  int value;

  LifetimeTracker() : value(0) { ++alive; ++constructed; }
  explicit LifetimeTracker(int v) : value(v) { ++alive; ++constructed; }
  LifetimeTracker(const LifetimeTracker& o) : value(o.value) { ++alive; ++constructed; }
  LifetimeTracker(LifetimeTracker&& o) noexcept : value(o.value) {
    o.value = -1;
    ++alive;
    ++constructed;
  }
  LifetimeTracker& operator=(const LifetimeTracker& o) {
    value = o.value;
    return *this;
  }
  LifetimeTracker& operator=(LifetimeTracker&& o) noexcept {
    value = o.value;
    o.value = -1;
    return *this;
  }
  ~LifetimeTracker() { --alive; }

  static void Reset() { alive = 0; constructed = 0; }
};

int LifetimeTracker::alive = 0;
int LifetimeTracker::constructed = 0;

TEST(VectorTest, DefaultConstructor) {
  Vector<int> v;
  EXPECT_EQ(v.size(), 0u);
  EXPECT_TRUE(v.empty());
  EXPECT_EQ(v.capacity(), 0u);
}

TEST(VectorTest, InitializerList) {
  Vector<int> v = {1, 2, 3, 4, 5};
  EXPECT_EQ(v.size(), 5u);
  EXPECT_EQ(v[0], 1);
  EXPECT_EQ(v[4], 5);
}

TEST(VectorTest, CountValueConstructor) {
  Vector<int> v(5, 42);
  EXPECT_EQ(v.size(), 5u);
  for (usize i = 0; i < v.size(); ++i) {
    EXPECT_EQ(v[i], 42);
  }
}

TEST(VectorTest, PushBackGrowth) {
  Vector<int> v;
  for (int i = 0; i < 100; ++i) {
    v.push_back(i);
  }
  EXPECT_EQ(v.size(), 100u);
  for (int i = 0; i < 100; ++i) {
    EXPECT_EQ(v[static_cast<usize>(i)], i);
  }
  EXPECT_GE(v.capacity(), 100u);
}

TEST(VectorTest, EmplaceBack) {
  Vector<std::string> v;
  v.emplace_back("hello");
  v.emplace_back(3, 'x');
  EXPECT_EQ(v.size(), 2u);
  EXPECT_EQ(v[0], "hello");
  EXPECT_EQ(v[1], "xxx");
}

TEST(VectorTest, PopBack) {
  Vector<int> v = {1, 2, 3};
  v.pop_back();
  EXPECT_EQ(v.size(), 2u);
  EXPECT_EQ(v.back(), 2);
}

TEST(VectorTest, CopyConstructor) {
  Vector<int> v = {1, 2, 3};
  Vector<int> v2(v);
  EXPECT_EQ(v2.size(), 3u);
  EXPECT_EQ(v2[0], 1);
  EXPECT_EQ(v2[2], 3);
  v2.push_back(4);
  EXPECT_EQ(v.size(), 3u);
}

TEST(VectorTest, MoveConstructor) {
  Vector<int> v = {1, 2, 3};
  Vector<int> v2(std::move(v));
  EXPECT_EQ(v2.size(), 3u);
  EXPECT_EQ(v2[0], 1);
  EXPECT_EQ(v.size(), 0u);
  EXPECT_EQ(v.data(), nullptr);
}

TEST(VectorTest, CopyAssignment) {
  Vector<int> v = {1, 2, 3};
  Vector<int> v2;
  v2 = v;
  EXPECT_EQ(v2.size(), 3u);
  EXPECT_EQ(v.size(), 3u);
}

TEST(VectorTest, MoveAssignment) {
  Vector<int> v = {1, 2, 3};
  Vector<int> v2;
  v2 = std::move(v);
  EXPECT_EQ(v2.size(), 3u);
  EXPECT_EQ(v.size(), 0u);
}

TEST(VectorTest, Reserve) {
  Vector<int> v;
  v.reserve(100);
  EXPECT_GE(v.capacity(), 100u);
  EXPECT_EQ(v.size(), 0u);
  v.reserve(50);
  EXPECT_GE(v.capacity(), 100u);
}

TEST(VectorTest, Resize) {
  Vector<int> v = {1, 2, 3};
  v.resize(5);
  EXPECT_EQ(v.size(), 5u);
  EXPECT_EQ(v[0], 1);
  EXPECT_EQ(v[3], 0);
  v.resize(2);
  EXPECT_EQ(v.size(), 2u);
  EXPECT_EQ(v[1], 2);
}

TEST(VectorTest, ResizeWithValue) {
  Vector<int> v;
  v.resize(3, 42);
  EXPECT_EQ(v.size(), 3u);
  EXPECT_EQ(v[0], 42);
  EXPECT_EQ(v[2], 42);
}

TEST(VectorTest, Clear) {
  Vector<int> v = {1, 2, 3, 4, 5};
  v.clear();
  EXPECT_EQ(v.size(), 0u);
  EXPECT_TRUE(v.empty());
  EXPECT_GE(v.capacity(), 5u);
}

TEST(VectorTest, InsertMiddle) {
  Vector<int> v = {1, 2, 4, 5};
  v.insert(v.begin() + 2, 3);
  EXPECT_EQ(v.size(), 5u);
  for (usize i = 0; i < 5; ++i) {
    EXPECT_EQ(v[i], static_cast<int>(i + 1));
  }
}

TEST(VectorTest, InsertBegin) {
  Vector<int> v = {2, 3};
  v.insert(v.begin(), 1);
  EXPECT_EQ(v[0], 1);
  EXPECT_EQ(v.size(), 3u);
}

TEST(VectorTest, EraseSingle) {
  Vector<int> v = {1, 2, 3, 4};
  v.erase(v.begin() + 1);
  EXPECT_EQ(v.size(), 3u);
  EXPECT_EQ(v[0], 1);
  EXPECT_EQ(v[1], 3);
  EXPECT_EQ(v[2], 4);
}

TEST(VectorTest, EraseRange) {
  Vector<int> v = {1, 2, 3, 4, 5};
  v.erase(v.begin() + 1, v.begin() + 3);
  EXPECT_EQ(v.size(), 3u);
  EXPECT_EQ(v[0], 1);
  EXPECT_EQ(v[1], 4);
  EXPECT_EQ(v[2], 5);
}

TEST(VectorTest, RangeFor) {
  Vector<int> v = {10, 20, 30};
  int sum = 0;
  for (int x : v) {
    sum += x;
  }
  EXPECT_EQ(sum, 60);
}

TEST(VectorTest, FrontBack) {
  Vector<int> v = {1, 2, 3};
  EXPECT_EQ(v.front(), 1);
  EXPECT_EQ(v.back(), 3);
}

TEST(VectorTest, NonTrivialTypeLifetime) {
  LifetimeTracker::Reset();
  {
    Vector<LifetimeTracker> v;
    v.emplace_back(1);
    v.emplace_back(2);
    v.emplace_back(3);
    EXPECT_EQ(LifetimeTracker::alive, 3);
    v.pop_back();
    EXPECT_EQ(LifetimeTracker::alive, 2);
  }
  EXPECT_EQ(LifetimeTracker::alive, 0);
}

TEST(VectorTest, NonTrivialTypeCopy) {
  LifetimeTracker::Reset();
  {
    Vector<LifetimeTracker> v;
    v.emplace_back(10);
    v.emplace_back(20);
    {
      Vector<LifetimeTracker> v2(v);
      EXPECT_EQ(v2.size(), 2u);
      EXPECT_EQ(v2[0].value, 10);
    }
    EXPECT_EQ(LifetimeTracker::alive, 2);
  }
  EXPECT_EQ(LifetimeTracker::alive, 0);
}

TEST(VectorTest, StringVector) {
  Vector<std::string> v;
  v.push_back("hello");
  v.push_back("world");
  v.emplace_back("!");
  EXPECT_EQ(v.size(), 3u);
  EXPECT_EQ(v[0], "hello");
  EXPECT_EQ(v[1], "world");
  EXPECT_EQ(v[2], "!");
}

}  // namespace
}  // namespace vx
