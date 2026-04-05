#include "veloxa/core/dom/node.h"

#include <gtest/gtest.h>

#include "veloxa/core/dom/comment.h"
#include "veloxa/core/dom/element.h"
#include "veloxa/core/dom/text.h"

namespace vx::dom {
namespace {

TEST(NodeTest, ElementNodeType) {
  Element el(TagId::kDiv);
  EXPECT_EQ(el.type(), NodeType::kElement);
}

TEST(NodeTest, TextNodeType) {
  Text t("hello");
  EXPECT_EQ(t.type(), NodeType::kText);
}

TEST(NodeTest, CommentNodeType) {
  Comment c("a comment");
  EXPECT_EQ(c.type(), NodeType::kComment);
}

TEST(NodeTest, IsElementPredicate) {
  Element el(TagId::kDiv);
  Text t("x");
  Comment c("y");
  EXPECT_TRUE(el.is_element());
  EXPECT_FALSE(el.is_text());
  EXPECT_FALSE(t.is_element());
  EXPECT_FALSE(c.is_element());
}

TEST(NodeTest, IsTextPredicate) {
  Text t("x");
  Element el(TagId::kDiv);
  EXPECT_TRUE(t.is_text());
  EXPECT_FALSE(t.is_element());
  EXPECT_FALSE(el.is_text());
}

TEST(NodeTest, InitialParentIsNull) {
  Element el(TagId::kDiv);
  EXPECT_EQ(el.parent(), nullptr);
}

TEST(NodeTest, InitialSiblingsAreNull) {
  Element el(TagId::kDiv);
  EXPECT_EQ(el.next_sibling(), nullptr);
  EXPECT_EQ(el.prev_sibling(), nullptr);
}

TEST(NodeTest, SetParent) {
  Element parent(TagId::kBody);
  Element child(TagId::kDiv);
  child.set_parent(&parent);
  EXPECT_EQ(child.parent(), &parent);
}

TEST(NodeTest, AppendChildSetsParent) {
  Element parent(TagId::kBody);
  Element child(TagId::kDiv);
  parent.AppendChild(&child);
  EXPECT_EQ(child.parent(), &parent);
}

TEST(NodeTest, AppendChildSetsSiblingLinks) {
  Element parent(TagId::kBody);
  Element c1(TagId::kDiv);
  Element c2(TagId::kSpan);
  Text c3("text");

  parent.AppendChild(&c1);
  parent.AppendChild(&c2);
  parent.AppendChild(&c3);

  EXPECT_EQ(parent.first_child(), &c1);
  EXPECT_EQ(parent.last_child(), &c3);

  EXPECT_EQ(c1.prev_sibling(), nullptr);
  EXPECT_EQ(c1.next_sibling(), &c2);
  EXPECT_EQ(c2.prev_sibling(), &c1);
  EXPECT_EQ(c2.next_sibling(), &c3);
  EXPECT_EQ(c3.prev_sibling(), &c2);
  EXPECT_EQ(c3.next_sibling(), nullptr);
}

}  // namespace
}  // namespace vx::dom
