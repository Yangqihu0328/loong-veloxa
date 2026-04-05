#include "veloxa/core/dom/document.h"

#include <gtest/gtest.h>

namespace vx::dom {
namespace {

TEST(DocumentTest, NodeTypeIsDocument) {
  Document doc;
  EXPECT_EQ(doc.type(), NodeType::kDocument);
}

TEST(DocumentTest, IsElement) {
  Document doc;
  EXPECT_TRUE(doc.is_element());
}

TEST(DocumentTest, CreateElement) {
  Document doc;
  Element* el = doc.CreateElement(TagId::kDiv);
  ASSERT_NE(el, nullptr);
  EXPECT_EQ(el->tag_id(), TagId::kDiv);
  EXPECT_EQ(el->type(), NodeType::kElement);
}

TEST(DocumentTest, CreateText) {
  Document doc;
  Text* t = doc.CreateText(String("hello"));
  ASSERT_NE(t, nullptr);
  EXPECT_EQ(t->data(), "hello");
  EXPECT_EQ(t->type(), NodeType::kText);
}

TEST(DocumentTest, CreateComment) {
  Document doc;
  Comment* c = doc.CreateComment(String("comment"));
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->data(), "comment");
  EXPECT_EQ(c->type(), NodeType::kComment);
}

TEST(DocumentTest, OwnershipCleanup) {
  auto* doc = new Document();
  doc->CreateElement(TagId::kDiv);
  doc->CreateElement(TagId::kSpan);
  doc->CreateText(String("text"));
  doc->CreateComment(String("comment"));
  delete doc;
  // If ownership is broken, sanitizers (ASan/LSan) will catch the leak.
}

TEST(DocumentTest, BuildTree) {
  Document doc;
  Element* body = doc.CreateElement(TagId::kBody);
  Element* div = doc.CreateElement(TagId::kDiv);
  Text* text = doc.CreateText(String("Hello, world!"));

  doc.AppendChild(body);
  body->AppendChild(div);
  div->AppendChild(text);

  EXPECT_EQ(doc.first_child(), body);
  EXPECT_EQ(body->first_child(), div);
  EXPECT_EQ(div->first_child(), text);
  EXPECT_EQ(text->parent(), div);
}

}  // namespace
}  // namespace vx::dom
