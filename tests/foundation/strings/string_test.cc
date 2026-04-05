#include "veloxa/foundation/strings/string.h"

#include <gtest/gtest.h>

#include <cstring>

namespace vx {
namespace {

TEST(StringTest, SizeOf24) {
  EXPECT_EQ(sizeof(String), 32u);
  // 24 bytes storage + 8 bytes pointer to allocator = 32
}

TEST(StringTest, DefaultConstruction) {
  String s;
  EXPECT_EQ(s.size(), 0u);
  EXPECT_TRUE(s.empty());
  EXPECT_STREQ(s.c_str(), "");
}

TEST(StringTest, SSOConstruction) {
  String s("hello");
  EXPECT_EQ(s.size(), 5u);
  EXPECT_STREQ(s.c_str(), "hello");
  EXPECT_FALSE(s.empty());
}

TEST(StringTest, SSOBoundary22) {
  // Exactly 22 bytes should fit in SSO
  String s("1234567890123456789012");
  EXPECT_EQ(s.size(), 22u);
  EXPECT_STREQ(s.c_str(), "1234567890123456789012");
}

TEST(StringTest, HeapBoundary23) {
  // 23 bytes should trigger heap allocation
  String s("12345678901234567890123");
  EXPECT_EQ(s.size(), 23u);
  EXPECT_STREQ(s.c_str(), "12345678901234567890123");
}

TEST(StringTest, HeapConstruction) {
  String s("This is a longer string that exceeds SSO capacity!");
  EXPECT_EQ(s.size(), 50u);
  EXPECT_STREQ(s.c_str(), "This is a longer string that exceeds SSO capacity!");
}

TEST(StringTest, CopyConstructSSO) {
  String a("hello");
  String b(a);
  EXPECT_EQ(b.size(), 5u);
  EXPECT_STREQ(b.c_str(), "hello");
  EXPECT_NE(a.data(), b.data());
}

TEST(StringTest, CopyConstructHeap) {
  String a("A long enough string for heap allocation!!");
  String b(a);
  EXPECT_EQ(b.size(), a.size());
  EXPECT_STREQ(b.c_str(), a.c_str());
  EXPECT_NE(a.data(), b.data());
}

TEST(StringTest, MoveConstructSSO) {
  String a("hello");
  const char* old_data = a.data();
  String b(std::move(a));
  EXPECT_EQ(b.size(), 5u);
  EXPECT_STREQ(b.c_str(), "hello");
  EXPECT_TRUE(a.empty());
  (void)old_data;
}

TEST(StringTest, MoveConstructHeap) {
  String a("A long enough string for heap allocation!!");
  const char* ptr = a.data();
  String b(std::move(a));
  EXPECT_EQ(b.data(), ptr);
  EXPECT_TRUE(a.empty());
}

TEST(StringTest, CopyAssign) {
  String a("hello");
  String b("world");
  b = a;
  EXPECT_STREQ(b.c_str(), "hello");
  EXPECT_STREQ(a.c_str(), "hello");
}

TEST(StringTest, MoveAssign) {
  String a("A long enough string for heap allocation!!");
  const char* ptr = a.data();
  String b;
  b = std::move(a);
  EXPECT_EQ(b.data(), ptr);
  EXPECT_TRUE(a.empty());
}

TEST(StringTest, AppendStringView) {
  String s("hello");
  s.append(" world");
  EXPECT_STREQ(s.c_str(), "hello world");
  EXPECT_EQ(s.size(), 11u);
}

TEST(StringTest, AppendChar) {
  String s("abc");
  s.append('d');
  EXPECT_STREQ(s.c_str(), "abcd");
}

TEST(StringTest, AppendGrowth) {
  String s;
  for (int i = 0; i < 100; ++i) {
    s.append('x');
  }
  EXPECT_EQ(s.size(), 100u);
  for (usize i = 0; i < 100; ++i) {
    EXPECT_EQ(s[i], 'x');
  }
}

TEST(StringTest, CStrNullTerminated) {
  String s("test");
  const char* cs = s.c_str();
  EXPECT_EQ(cs[4], '\0');
}

TEST(StringTest, ViewConversion) {
  String s("hello");
  StringView sv = s.view();
  EXPECT_EQ(sv.size(), 5u);
  EXPECT_EQ(sv, StringView("hello"));
}

TEST(StringTest, ImplicitViewConversion) {
  String s("hello");
  StringView sv = s;
  EXPECT_EQ(sv, StringView("hello"));
}

TEST(StringTest, EqualityOperator) {
  String s("hello");
  EXPECT_TRUE(s == StringView("hello"));
  EXPECT_FALSE(s == StringView("world"));
}

TEST(StringTest, InequalityOperator) {
  String s("hello");
  EXPECT_TRUE(s != StringView("world"));
  EXPECT_FALSE(s != StringView("hello"));
}

TEST(StringTest, LessThanOperator) {
  String s("abc");
  EXPECT_TRUE(s < StringView("abd"));
  EXPECT_FALSE(s < StringView("abb"));
}

TEST(StringTest, Clear) {
  String s("hello");
  s.clear();
  EXPECT_TRUE(s.empty());
  EXPECT_EQ(s.size(), 0u);
  EXPECT_STREQ(s.c_str(), "");
}

TEST(StringTest, Reserve) {
  String s;
  s.reserve(100);
  EXPECT_GE(s.capacity(), 100u);
  EXPECT_TRUE(s.empty());
}

TEST(StringTest, SSOToHeapTransition) {
  String s("short");
  EXPECT_EQ(s.size(), 5u);
  s.append(" and then we make it much much longer to exceed SSO");
  EXPECT_GT(s.size(), 22u);
  EXPECT_STREQ(s.c_str(),
               "short and then we make it much much longer to exceed SSO");
}

TEST(StringTest, PlusEqualsOperator) {
  String s("hello");
  s += " ";
  s += "world";
  EXPECT_STREQ(s.c_str(), "hello world");
}

TEST(StringTest, RangeFor) {
  String s("abc");
  int count = 0;
  for (char c : s) {
    (void)c;
    ++count;
  }
  EXPECT_EQ(count, 3);
}

}  // namespace
}  // namespace vx
