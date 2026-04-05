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

TEST(DocumentTest, ArenaAllocation) {
  Document doc;
  EXPECT_EQ(doc.arena_bytes_allocated(), 0u);
  doc.CreateElement(TagId::kDiv);
  EXPECT_GT(doc.arena_bytes_allocated(), 0u);
}

TEST(DocumentTest, ArenaMultipleNodes) {
  Document doc;
  usize prev = doc.arena_bytes_allocated();
  doc.CreateElement(TagId::kDiv);
  EXPECT_GT(doc.arena_bytes_allocated(), prev);
  prev = doc.arena_bytes_allocated();
  doc.CreateText(String("hello"));
  EXPECT_GT(doc.arena_bytes_allocated(), prev);
  prev = doc.arena_bytes_allocated();
  doc.CreateComment(String("comment"));
  EXPECT_GT(doc.arena_bytes_allocated(), prev);
}

TEST(DocumentTest, ArenaCleanupWithSanitizer) {
  auto* doc = new Document();
  for (int i = 0; i < 100; ++i) {
    doc->CreateElement(TagId::kDiv);
    doc->CreateText(String("text data that might heap allocate"));
  }
  delete doc;
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
