#include "veloxa/foundation/containers/hash_map.h"

#include <string>

#include <gtest/gtest.h>

namespace vx {
namespace {

TEST(HashMapTest, EmptyMap) {
  HashMap<int, int> m;
  EXPECT_TRUE(m.empty());
  EXPECT_EQ(m.size(), 0u);
}

TEST(HashMapTest, InsertAndFind) {
  HashMap<int, int> m;
  EXPECT_TRUE(m.Insert(1, 10));
  EXPECT_TRUE(m.Insert(2, 20));
  EXPECT_TRUE(m.Insert(3, 30));
  EXPECT_EQ(m.size(), 3u);

  auto* v1 = m.Find(1);
  ASSERT_NE(v1, nullptr);
  EXPECT_EQ(*v1, 10);

  auto* v2 = m.Find(2);
  ASSERT_NE(v2, nullptr);
  EXPECT_EQ(*v2, 20);

  EXPECT_EQ(m.Find(999), nullptr);
}

TEST(HashMapTest, InsertDuplicate) {
  HashMap<int, int> m;
  EXPECT_TRUE(m.Insert(1, 10));
  EXPECT_FALSE(m.Insert(1, 99));
  EXPECT_EQ(m.size(), 1u);
  EXPECT_EQ(*m.Find(1), 99);
}

TEST(HashMapTest, Contains) {
  HashMap<std::string, int> m;
  m.Insert("hello", 1);
  m.Insert("world", 2);
  EXPECT_TRUE(m.Contains("hello"));
  EXPECT_TRUE(m.Contains("world"));
  EXPECT_FALSE(m.Contains("missing"));
}

TEST(HashMapTest, Erase) {
  HashMap<int, int> m;
  m.Insert(1, 10);
  m.Insert(2, 20);
  m.Insert(3, 30);

  EXPECT_TRUE(m.Erase(2));
  EXPECT_EQ(m.size(), 2u);
  EXPECT_FALSE(m.Contains(2));
  EXPECT_TRUE(m.Contains(1));
  EXPECT_TRUE(m.Contains(3));

  EXPECT_FALSE(m.Erase(999));
}

TEST(HashMapTest, OperatorBracketAutoInsert) {
  HashMap<std::string, int> m;
  m["key1"] = 100;
  m["key2"] = 200;
  EXPECT_EQ(m.size(), 2u);
  EXPECT_EQ(m["key1"], 100);

  m["key1"] = 999;
  EXPECT_EQ(m["key1"], 999);
  EXPECT_EQ(m.size(), 2u);

  // Auto-insert default value
  int val = m["new_key"];
  EXPECT_EQ(val, 0);
  EXPECT_EQ(m.size(), 3u);
}

TEST(HashMapTest, RehashPreservesData) {
  HashMap<int, int> m;
  constexpr int kCount = 200;
  for (int i = 0; i < kCount; ++i) {
    m.Insert(i, i * 10);
  }
  EXPECT_EQ(m.size(), static_cast<usize>(kCount));

  for (int i = 0; i < kCount; ++i) {
    auto* v = m.Find(i);
    ASSERT_NE(v, nullptr) << "key " << i << " not found after rehash";
    EXPECT_EQ(*v, i * 10);
  }
}

TEST(HashMapTest, Clear) {
  HashMap<int, int> m;
  for (int i = 0; i < 50; ++i) {
    m.Insert(i, i);
  }
  EXPECT_EQ(m.size(), 50u);
  m.clear();
  EXPECT_EQ(m.size(), 0u);
  EXPECT_TRUE(m.empty());
  EXPECT_EQ(m.Find(0), nullptr);
}

TEST(HashMapTest, StringKeys) {
  HashMap<std::string, std::string> m;
  m.Insert("name", "veloxa");
  m.Insert("lang", "cpp");
  m.Insert("version", "17");

  EXPECT_EQ(*m.Find("name"), "veloxa");
  EXPECT_EQ(*m.Find("lang"), "cpp");
  EXPECT_EQ(*m.Find("version"), "17");
}

TEST(HashMapTest, Iteration) {
  HashMap<int, int> m;
  m.Insert(1, 10);
  m.Insert(2, 20);
  m.Insert(3, 30);

  int key_sum = 0;
  int val_sum = 0;
  int count = 0;
  for (auto it = m.begin(); it != m.end(); ++it) {
    key_sum += it->key;
    val_sum += it->value;
    ++count;
  }
  EXPECT_EQ(count, 3);
  EXPECT_EQ(key_sum, 6);
  EXPECT_EQ(val_sum, 60);
}

TEST(HashMapTest, RangeFor) {
  HashMap<int, int> m;
  m.Insert(10, 100);
  m.Insert(20, 200);

  int count = 0;
  for (auto& slot : m) {
    EXPECT_TRUE(slot.key == 10 || slot.key == 20);
    ++count;
  }
  EXPECT_EQ(count, 2);
}

TEST(HashMapTest, EraseAndReinsert) {
  HashMap<int, int> m;
  m.Insert(1, 10);
  m.Insert(2, 20);
  m.Erase(1);
  EXPECT_FALSE(m.Contains(1));

  m.Insert(1, 100);
  EXPECT_TRUE(m.Contains(1));
  EXPECT_EQ(*m.Find(1), 100);
}

TEST(HashMapTest, Reserve) {
  HashMap<int, int> m;
  m.reserve(100);
  for (int i = 0; i < 100; ++i) {
    m.Insert(i, i);
  }
  EXPECT_EQ(m.size(), 100u);
  for (int i = 0; i < 100; ++i) {
    EXPECT_TRUE(m.Contains(i));
  }
}

struct CustomHash {
  usize operator()(int v) const { return static_cast<usize>(v * 31); }
};

struct CustomEq {
  bool operator()(int a, int b) const { return a == b; }
};

TEST(HashMapTest, CustomHashAndEq) {
  HashMap<int, int, CustomHash, CustomEq> m;
  m.Insert(1, 10);
  m.Insert(2, 20);
  EXPECT_EQ(*m.Find(1), 10);
  EXPECT_EQ(*m.Find(2), 20);
  EXPECT_EQ(m.size(), 2u);
}

TEST(HashMapTest, CopyConstructor) {
  HashMap<int, int> m;
  m.Insert(1, 10);
  m.Insert(2, 20);

  HashMap<int, int> m2(m);
  EXPECT_EQ(m2.size(), 2u);
  EXPECT_EQ(*m2.Find(1), 10);
  EXPECT_EQ(*m2.Find(2), 20);

  m2.Insert(3, 30);
  EXPECT_EQ(m.size(), 2u);
}

TEST(HashMapTest, MoveConstructor) {
  HashMap<int, int> m;
  m.Insert(1, 10);
  m.Insert(2, 20);

  HashMap<int, int> m2(std::move(m));
  EXPECT_EQ(m2.size(), 2u);
  EXPECT_EQ(*m2.Find(1), 10);
  EXPECT_EQ(m.size(), 0u);
}

TEST(HashMapTest, ConstIteration) {
  HashMap<int, int> m;
  m.Insert(1, 10);
  m.Insert(2, 20);
  const HashMap<int, int>& cm = m;
  int sum = 0;
  for (const auto& slot : cm) {
    sum += slot.key + slot.value;
  }
  EXPECT_EQ(sum, 33);
  int c = 0;
  for (auto it = m.cbegin(); it != m.cend(); ++it) {
    ++c;
    EXPECT_TRUE(it->key == 1 || it->key == 2);
  }
  EXPECT_EQ(c, 2);
}

}  // namespace
}  // namespace vx
