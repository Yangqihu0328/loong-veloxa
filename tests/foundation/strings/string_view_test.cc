#include "veloxa/foundation/strings/string_view.h"

#include <gtest/gtest.h>

namespace vx {
namespace {

TEST(StringViewTest, DefaultConstruction) {
  StringView sv;
  EXPECT_EQ(sv.data(), nullptr);
  EXPECT_EQ(sv.size(), 0u);
  EXPECT_TRUE(sv.empty());
}

TEST(StringViewTest, CStringConstruction) {
  StringView sv("hello");
  EXPECT_STREQ(sv.data(), "hello");
  EXPECT_EQ(sv.size(), 5u);
  EXPECT_FALSE(sv.empty());
}

TEST(StringViewTest, DataSizeConstruction) {
  const char* str = "hello world";
  StringView sv(str, 5);
  EXPECT_EQ(sv.size(), 5u);
  EXPECT_EQ(sv[0], 'h');
  EXPECT_EQ(sv[4], 'o');
}

TEST(StringViewTest, NullptrConstruction) {
  StringView sv(static_cast<const char*>(nullptr));
  EXPECT_EQ(sv.data(), nullptr);
  EXPECT_EQ(sv.size(), 0u);
  EXPECT_TRUE(sv.empty());
}

TEST(StringViewTest, Indexing) {
  StringView sv("abcde");
  EXPECT_EQ(sv[0], 'a');
  EXPECT_EQ(sv[2], 'c');
  EXPECT_EQ(sv[4], 'e');
}

TEST(StringViewTest, FrontBack) {
  StringView sv("xyz");
  EXPECT_EQ(sv.front(), 'x');
  EXPECT_EQ(sv.back(), 'z');
}

TEST(StringViewTest, SubstrFull) {
  StringView sv("hello");
  auto sub = sv.substr(0);
  EXPECT_EQ(sub.size(), 5u);
  EXPECT_EQ(sub, StringView("hello"));
}

TEST(StringViewTest, SubstrOffset) {
  StringView sv("hello world");
  auto sub = sv.substr(6);
  EXPECT_EQ(sub, StringView("world"));
}

TEST(StringViewTest, SubstrOffsetCount) {
  StringView sv("hello world");
  auto sub = sv.substr(0, 5);
  EXPECT_EQ(sub, StringView("hello"));
}

TEST(StringViewTest, SubstrBeyondEnd) {
  StringView sv("abc");
  auto sub = sv.substr(10);
  EXPECT_EQ(sub.size(), 0u);
  EXPECT_TRUE(sub.empty());
}

TEST(StringViewTest, FindChar) {
  StringView sv("hello");
  EXPECT_EQ(sv.find('l'), 2u);
  EXPECT_EQ(sv.find('z'), StringView::npos);
  EXPECT_EQ(sv.find('h'), 0u);
  EXPECT_EQ(sv.find('o'), 4u);
}

TEST(StringViewTest, FindCharFromPos) {
  StringView sv("hello");
  EXPECT_EQ(sv.find('l', 3), 3u);
  EXPECT_EQ(sv.find('l', 4), StringView::npos);
}

TEST(StringViewTest, FindStringView) {
  StringView sv("hello world");
  EXPECT_EQ(sv.find(StringView("world")), 6u);
  EXPECT_EQ(sv.find(StringView("xyz")), StringView::npos);
  EXPECT_EQ(sv.find(StringView("")), 0u);
  EXPECT_EQ(sv.find(StringView("hello")), 0u);
}

TEST(StringViewTest, StartsWith) {
  StringView sv("hello world");
  EXPECT_TRUE(sv.starts_with("hello"));
  EXPECT_TRUE(sv.starts_with(""));
  EXPECT_TRUE(sv.starts_with("hello world"));
  EXPECT_FALSE(sv.starts_with("world"));
  EXPECT_FALSE(sv.starts_with("hello world!!!"));
}

TEST(StringViewTest, EndsWith) {
  StringView sv("hello world");
  EXPECT_TRUE(sv.ends_with("world"));
  EXPECT_TRUE(sv.ends_with(""));
  EXPECT_TRUE(sv.ends_with("hello world"));
  EXPECT_FALSE(sv.ends_with("hello"));
  EXPECT_FALSE(sv.ends_with("!!!hello world"));
}

TEST(StringViewTest, Compare) {
  EXPECT_EQ(StringView("abc").compare(StringView("abc")), 0);
  EXPECT_LT(StringView("abc").compare(StringView("abd")), 0);
  EXPECT_GT(StringView("abd").compare(StringView("abc")), 0);
  EXPECT_LT(StringView("ab").compare(StringView("abc")), 0);
  EXPECT_GT(StringView("abc").compare(StringView("ab")), 0);
}

TEST(StringViewTest, EqualityOperator) {
  EXPECT_TRUE(StringView("hello") == StringView("hello"));
  EXPECT_FALSE(StringView("hello") == StringView("world"));
  EXPECT_FALSE(StringView("hello") == StringView("hell"));
}

TEST(StringViewTest, InequalityOperator) {
  EXPECT_FALSE(StringView("hello") != StringView("hello"));
  EXPECT_TRUE(StringView("hello") != StringView("world"));
}

TEST(StringViewTest, LessThanOperator) {
  EXPECT_TRUE(StringView("abc") < StringView("abd"));
  EXPECT_FALSE(StringView("abd") < StringView("abc"));
  EXPECT_FALSE(StringView("abc") < StringView("abc"));
  EXPECT_TRUE(StringView("ab") < StringView("abc"));
}

TEST(StringViewTest, RangeFor) {
  StringView sv("abc");
  int count = 0;
  char expected[] = {'a', 'b', 'c'};
  for (char c : sv) {
    EXPECT_EQ(c, expected[count]);
    ++count;
  }
  EXPECT_EQ(count, 3);
}

TEST(StringViewTest, EmptyStringView) {
  StringView sv("");
  EXPECT_EQ(sv.size(), 0u);
  EXPECT_TRUE(sv.empty());
  EXPECT_NE(sv.data(), nullptr);
}

}  // namespace
}  // namespace vx
