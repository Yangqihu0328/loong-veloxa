#include "veloxa/foundation/strings/interned_string.h"

#include <gtest/gtest.h>

namespace vx {
namespace {

class InternedStringTest : public ::testing::Test {
 protected:
  void TearDown() override { InternedString::ClearInternedStrings(); }
};

TEST_F(InternedStringTest, DefaultConstruction) {
  InternedString s;
  EXPECT_EQ(s.data(), nullptr);
  EXPECT_EQ(s.size(), 0u);
  EXPECT_TRUE(s.empty());
}

TEST_F(InternedStringTest, InternReturnsValidString) {
  auto s = InternedString::Intern("hello");
  EXPECT_EQ(s.size(), 5u);
  EXPECT_EQ(s.view(), StringView("hello"));
}

TEST_F(InternedStringTest, SameStringReturnsSamePointer) {
  auto a = InternedString::Intern("hello");
  auto b = InternedString::Intern("hello");
  EXPECT_EQ(a.data(), b.data());
  EXPECT_TRUE(a == b);
}

TEST_F(InternedStringTest, DifferentStringsDifferentPointers) {
  auto a = InternedString::Intern("hello");
  auto b = InternedString::Intern("world");
  EXPECT_NE(a.data(), b.data());
  EXPECT_TRUE(a != b);
}

TEST_F(InternedStringTest, ComparisonIsPointerBased) {
  auto a = InternedString::Intern("test");
  auto b = InternedString::Intern("test");
  auto c = InternedString::Intern("other");
  EXPECT_TRUE(a == b);
  EXPECT_FALSE(a == c);
  EXPECT_FALSE(a != b);
  EXPECT_TRUE(a != c);
}

TEST_F(InternedStringTest, ClearResetsTable) {
  auto a = InternedString::Intern("hello");
  EXPECT_EQ(a.view(), StringView("hello"));
  InternedString::ClearInternedStrings();
  // After clear, re-interning may give a different pointer
  auto b = InternedString::Intern("hello");
  EXPECT_EQ(b.view(), StringView("hello"));
  // The pointer may or may not be the same depending on allocator reuse,
  // but the content must be correct
}

TEST_F(InternedStringTest, ReInternAfterClear) {
  auto a = InternedString::Intern("alpha");
  const char* ptr_a = a.data();
  InternedString::ClearInternedStrings();
  auto b = InternedString::Intern("alpha");
  // After clear, it's a fresh intern; pointer may differ
  EXPECT_EQ(b.view(), StringView("alpha"));
  (void)ptr_a;
}

TEST_F(InternedStringTest, MultipleDistinctStrings) {
  auto a = InternedString::Intern("one");
  auto b = InternedString::Intern("two");
  auto c = InternedString::Intern("three");
  EXPECT_TRUE(a != b);
  EXPECT_TRUE(b != c);
  EXPECT_TRUE(a != c);
  EXPECT_EQ(a.view(), StringView("one"));
  EXPECT_EQ(b.view(), StringView("two"));
  EXPECT_EQ(c.view(), StringView("three"));
}

TEST_F(InternedStringTest, InternFromStringView) {
  StringView sv("substring test", 9);  // "substring"
  auto s = InternedString::Intern(sv);
  EXPECT_EQ(s.size(), 9u);
  EXPECT_EQ(s.view(), StringView("substring"));
}

TEST_F(InternedStringTest, EmptyStringIntern) {
  auto a = InternedString::Intern("");
  auto b = InternedString::Intern("");
  EXPECT_EQ(a.size(), 0u);
  EXPECT_TRUE(a.empty());
  EXPECT_EQ(a.data(), b.data());
}

TEST_F(InternedStringTest, InternPreservesContent) {
  char buf[] = "mutable";
  auto s = InternedString::Intern(StringView(buf, 7));
  buf[0] = 'X';  // mutate the source
  EXPECT_EQ(s.view(), StringView("mutable"));
}

}  // namespace
}  // namespace vx
