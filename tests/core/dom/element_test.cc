#include "veloxa/core/dom/element.h"

#include <gtest/gtest.h>

#include "veloxa/core/dom/text.h"

namespace vx::dom {
namespace {

TEST(ElementTest, TagId) {
  Element el(TagId::kDiv);
  EXPECT_EQ(el.tag_id(), TagId::kDiv);
}

TEST(ElementTest, TagName) {
  Element el(TagId::kDiv);
  EXPECT_STREQ(el.tag_name(), "div");
}

TEST(ElementTest, TagNameSpan) {
  Element el(TagId::kSpan);
  EXPECT_STREQ(el.tag_name(), "span");
}

TEST(ElementTest, AppendSingleChild) {
  Element parent(TagId::kDiv);
  Text child("hello");
  parent.AppendChild(&child);

  EXPECT_EQ(parent.child_count(), 1u);
  EXPECT_EQ(parent.first_child(), &child);
  EXPECT_EQ(parent.last_child(), &child);
  EXPECT_EQ(child.parent(), &parent);
}

TEST(ElementTest, AppendMultipleChildren) {
  Element parent(TagId::kDiv);
  Element c1(TagId::kP);
  Element c2(TagId::kSpan);
  Element c3(TagId::kA);

  parent.AppendChild(&c1);
  parent.AppendChild(&c2);
  parent.AppendChild(&c3);

  EXPECT_EQ(parent.child_count(), 3u);
  EXPECT_EQ(parent.first_child(), &c1);
  EXPECT_EQ(parent.last_child(), &c3);
}

TEST(ElementTest, RemoveFirstChild) {
  Element parent(TagId::kDiv);
  Element c1(TagId::kP);
  Element c2(TagId::kSpan);
  parent.AppendChild(&c1);
  parent.AppendChild(&c2);

  parent.RemoveChild(&c1);
  EXPECT_EQ(parent.child_count(), 1u);
  EXPECT_EQ(parent.first_child(), &c2);
  EXPECT_EQ(parent.last_child(), &c2);
  EXPECT_EQ(c2.prev_sibling(), nullptr);
  EXPECT_EQ(c1.parent(), nullptr);
}

TEST(ElementTest, RemoveLastChild) {
  Element parent(TagId::kDiv);
  Element c1(TagId::kP);
  Element c2(TagId::kSpan);
  parent.AppendChild(&c1);
  parent.AppendChild(&c2);

  parent.RemoveChild(&c2);
  EXPECT_EQ(parent.child_count(), 1u);
  EXPECT_EQ(parent.first_child(), &c1);
  EXPECT_EQ(parent.last_child(), &c1);
  EXPECT_EQ(c1.next_sibling(), nullptr);
  EXPECT_EQ(c2.parent(), nullptr);
}

TEST(ElementTest, RemoveMiddleChild) {
  Element parent(TagId::kDiv);
  Element c1(TagId::kP);
  Element c2(TagId::kSpan);
  Element c3(TagId::kA);
  parent.AppendChild(&c1);
  parent.AppendChild(&c2);
  parent.AppendChild(&c3);

  parent.RemoveChild(&c2);
  EXPECT_EQ(parent.child_count(), 2u);
  EXPECT_EQ(c1.next_sibling(), &c3);
  EXPECT_EQ(c3.prev_sibling(), &c1);
  EXPECT_EQ(c2.parent(), nullptr);
  EXPECT_EQ(c2.next_sibling(), nullptr);
  EXPECT_EQ(c2.prev_sibling(), nullptr);
}

TEST(ElementTest, ChildCountInitiallyZero) {
  Element el(TagId::kDiv);
  EXPECT_EQ(el.child_count(), 0u);
  EXPECT_EQ(el.first_child(), nullptr);
  EXPECT_EQ(el.last_child(), nullptr);
}

TEST(ElementTest, SetAndGetAttribute) {
  Element el(TagId::kDiv);
  auto name = InternedString::Intern("class");
  el.SetAttribute(name, String("container"));

  const String* val = el.GetAttribute(name);
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(*val, "container");
}

TEST(ElementTest, HasAttribute) {
  Element el(TagId::kDiv);
  auto name = InternedString::Intern("id");
  EXPECT_FALSE(el.HasAttribute(name));

  el.SetAttribute(name, String("main"));
  EXPECT_TRUE(el.HasAttribute(name));
}

TEST(ElementTest, OverwriteAttribute) {
  Element el(TagId::kDiv);
  auto name = InternedString::Intern("class");
  el.SetAttribute(name, String("old"));
  el.SetAttribute(name, String("new"));

  const String* val = el.GetAttribute(name);
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(*val, "new");
  EXPECT_EQ(el.attributes().size(), 1u);
}

TEST(ElementTest, RemoveAttribute) {
  Element el(TagId::kDiv);
  auto name = InternedString::Intern("class");
  el.SetAttribute(name, String("x"));
  EXPECT_TRUE(el.HasAttribute(name));

  el.RemoveAttribute(name);
  EXPECT_FALSE(el.HasAttribute(name));
  EXPECT_EQ(el.attributes().size(), 0u);
}

TEST(ElementTest, RemoveNonexistentAttributeIsNoop) {
  Element el(TagId::kDiv);
  auto name = InternedString::Intern("nope");
  el.RemoveAttribute(name);
  EXPECT_EQ(el.attributes().size(), 0u);
}

}  // namespace
}  // namespace vx::dom
