#include "veloxa/foundation/containers/intrusive_list.h"

#include <vector>

#include <gtest/gtest.h>

namespace vx {
namespace {

struct Item {
  int value;
  IntrusiveListNode node;

  explicit Item(int v) : value(v) {}
};

using TestList = IntrusiveList<Item, &Item::node>;

TEST(IntrusiveListTest, EmptyList) {
  TestList list;
  EXPECT_TRUE(list.empty());
  EXPECT_EQ(list.size(), 0u);
}

TEST(IntrusiveListTest, PushFront) {
  Item a(1), b(2), c(3);
  TestList list;
  list.push_front(&a);
  list.push_front(&b);
  list.push_front(&c);
  EXPECT_EQ(list.size(), 3u);
  EXPECT_EQ(list.front().value, 3);
  EXPECT_EQ(list.back().value, 1);
}

TEST(IntrusiveListTest, PushBack) {
  Item a(1), b(2), c(3);
  TestList list;
  list.push_back(&a);
  list.push_back(&b);
  list.push_back(&c);
  EXPECT_EQ(list.size(), 3u);
  EXPECT_EQ(list.front().value, 1);
  EXPECT_EQ(list.back().value, 3);
}

TEST(IntrusiveListTest, RemoveHead) {
  Item a(1), b(2), c(3);
  TestList list;
  list.push_back(&a);
  list.push_back(&b);
  list.push_back(&c);

  list.remove(&a);
  EXPECT_EQ(list.size(), 2u);
  EXPECT_EQ(list.front().value, 2);
}

TEST(IntrusiveListTest, RemoveTail) {
  Item a(1), b(2), c(3);
  TestList list;
  list.push_back(&a);
  list.push_back(&b);
  list.push_back(&c);

  list.remove(&c);
  EXPECT_EQ(list.size(), 2u);
  EXPECT_EQ(list.back().value, 2);
}

TEST(IntrusiveListTest, RemoveMiddle) {
  Item a(1), b(2), c(3);
  TestList list;
  list.push_back(&a);
  list.push_back(&b);
  list.push_back(&c);

  list.remove(&b);
  EXPECT_EQ(list.size(), 2u);
  EXPECT_EQ(list.front().value, 1);
  EXPECT_EQ(list.back().value, 3);
}

TEST(IntrusiveListTest, RemoveAllElements) {
  Item a(1), b(2);
  TestList list;
  list.push_back(&a);
  list.push_back(&b);
  list.remove(&a);
  list.remove(&b);
  EXPECT_TRUE(list.empty());
  EXPECT_EQ(list.size(), 0u);
}

TEST(IntrusiveListTest, ForwardIteration) {
  Item a(10), b(20), c(30);
  TestList list;
  list.push_back(&a);
  list.push_back(&b);
  list.push_back(&c);

  std::vector<int> values;
  for (auto& item : list) {
    values.push_back(item.value);
  }
  ASSERT_EQ(values.size(), 3u);
  EXPECT_EQ(values[0], 10);
  EXPECT_EQ(values[1], 20);
  EXPECT_EQ(values[2], 30);
}

TEST(IntrusiveListTest, BackwardIteration) {
  Item a(1), b(2), c(3);
  TestList list;
  list.push_back(&a);
  list.push_back(&b);
  list.push_back(&c);

  std::vector<int> values;
  auto it = list.end();
  while (it != list.begin()) {
    --it;
    values.push_back(it->value);
  }
  ASSERT_EQ(values.size(), 3u);
  EXPECT_EQ(values[0], 3);
  EXPECT_EQ(values[1], 2);
  EXPECT_EQ(values[2], 1);
}

TEST(IntrusiveListTest, SingleElement) {
  Item a(42);
  TestList list;
  list.push_back(&a);
  EXPECT_EQ(list.size(), 1u);
  EXPECT_EQ(list.front().value, 42);
  EXPECT_EQ(list.back().value, 42);
  list.remove(&a);
  EXPECT_TRUE(list.empty());
}

TEST(IntrusiveListTest, MixedPushFrontAndBack) {
  Item a(1), b(2), c(3), d(4);
  TestList list;
  list.push_back(&b);
  list.push_front(&a);
  list.push_back(&c);
  list.push_back(&d);

  std::vector<int> values;
  for (auto& item : list) {
    values.push_back(item.value);
  }
  ASSERT_EQ(values.size(), 4u);
  EXPECT_EQ(values[0], 1);
  EXPECT_EQ(values[1], 2);
  EXPECT_EQ(values[2], 3);
  EXPECT_EQ(values[3], 4);
}

}  // namespace
}  // namespace vx
