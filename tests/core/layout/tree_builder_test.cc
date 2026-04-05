#include "veloxa/core/layout/layout_engine.h"

#include <gtest/gtest.h>

#include "veloxa/core/css/parser.h"
#include "veloxa/core/dom/document.h"

namespace vx::layout {
namespace {

TEST(TreeBuilderTest, EmptyDocument) {
  dom::Document doc;
  LayoutContext ctx;
  ctx.viewport_width = 800;
  ctx.viewport_height = 600;

  auto* root = LayoutEngine::Layout(&doc, ctx);
  ASSERT_NE(root, nullptr);
  EXPECT_EQ(root->child_count(), 0u);
  EXPECT_EQ(root->type, LayoutType::kBlock);
}

TEST(TreeBuilderTest, SingleElement) {
  dom::Document doc;
  auto* body = doc.CreateElement(dom::TagId::kBody);
  doc.AppendChild(body);

  LayoutContext ctx;
  ctx.viewport_width = 800;
  ctx.viewport_height = 600;

  auto* root = LayoutEngine::Layout(&doc, ctx);
  ASSERT_NE(root, nullptr);
  EXPECT_EQ(root->child_count(), 1u);

  auto* child = root->first_child;
  ASSERT_NE(child, nullptr);
  EXPECT_EQ(child->type, LayoutType::kBlock);
  EXPECT_EQ(child->element, body);
}

TEST(TreeBuilderTest, TextNode) {
  dom::Document doc;
  auto* body = doc.CreateElement(dom::TagId::kBody);
  doc.AppendChild(body);
  auto* text = doc.CreateText("Hello");
  body->AppendChild(text);

  LayoutContext ctx;
  ctx.viewport_width = 800;
  ctx.viewport_height = 600;

  auto* root = LayoutEngine::Layout(&doc, ctx);
  ASSERT_NE(root, nullptr);
  auto* body_box = root->first_child;
  ASSERT_NE(body_box, nullptr);
  EXPECT_EQ(body_box->child_count(), 1u);

  auto* text_box = body_box->first_child;
  ASSERT_NE(text_box, nullptr);
  EXPECT_EQ(text_box->type, LayoutType::kText);
  EXPECT_EQ(text_box->text_node, text);
}

TEST(TreeBuilderTest, DisplayNone) {
  dom::Document doc;
  auto* body = doc.CreateElement(dom::TagId::kBody);
  doc.AppendChild(body);

  auto* div1 = doc.CreateElement(dom::TagId::kDiv);
  body->AppendChild(div1);
  auto* div2 = doc.CreateElement(dom::TagId::kDiv);
  div2->AddClass(InternedString::Intern("hidden"));
  body->AppendChild(div2);
  auto* div3 = doc.CreateElement(dom::TagId::kDiv);
  body->AppendChild(div3);

  auto sheet = css::CssParser::Parse(".hidden { display: none; }");
  Vector<css::Stylesheet> sheets;
  sheets.push_back(std::move(sheet));

  LayoutContext ctx;
  ctx.viewport_width = 800;
  ctx.viewport_height = 600;
  ctx.stylesheets = &sheets;

  auto* root = LayoutEngine::Layout(&doc, ctx);
  ASSERT_NE(root, nullptr);
  auto* body_box = root->first_child;
  ASSERT_NE(body_box, nullptr);
  EXPECT_EQ(body_box->child_count(), 2u);
}

TEST(TreeBuilderTest, NestedElements) {
  dom::Document doc;
  auto* body = doc.CreateElement(dom::TagId::kBody);
  doc.AppendChild(body);
  auto* outer = doc.CreateElement(dom::TagId::kDiv);
  body->AppendChild(outer);
  auto* inner = doc.CreateElement(dom::TagId::kDiv);
  outer->AppendChild(inner);

  LayoutContext ctx;
  ctx.viewport_width = 800;
  ctx.viewport_height = 600;

  auto* root = LayoutEngine::Layout(&doc, ctx);
  ASSERT_NE(root, nullptr);
  auto* body_box = root->first_child;
  ASSERT_NE(body_box, nullptr);
  EXPECT_EQ(body_box->child_count(), 1u);

  auto* outer_box = body_box->first_child;
  ASSERT_NE(outer_box, nullptr);
  EXPECT_EQ(outer_box->child_count(), 1u);

  auto* inner_box = outer_box->first_child;
  ASSERT_NE(inner_box, nullptr);
  EXPECT_EQ(inner_box->type, LayoutType::kBlock);
}

TEST(TreeBuilderTest, MixedContent) {
  dom::Document doc;
  auto* body = doc.CreateElement(dom::TagId::kBody);
  doc.AppendChild(body);
  auto* text1 = doc.CreateText("Before");
  body->AppendChild(text1);
  auto* div = doc.CreateElement(dom::TagId::kDiv);
  body->AppendChild(div);
  auto* text2 = doc.CreateText("After");
  body->AppendChild(text2);

  LayoutContext ctx;
  ctx.viewport_width = 800;
  ctx.viewport_height = 600;

  auto* root = LayoutEngine::Layout(&doc, ctx);
  ASSERT_NE(root, nullptr);
  auto* body_box = root->first_child;
  ASSERT_NE(body_box, nullptr);
  EXPECT_EQ(body_box->child_count(), 3u);

  auto* c1 = body_box->first_child;
  EXPECT_EQ(c1->type, LayoutType::kText);
  auto* c2 = c1->next_sibling;
  EXPECT_EQ(c2->type, LayoutType::kBlock);
  auto* c3 = c2->next_sibling;
  EXPECT_EQ(c3->type, LayoutType::kText);
}

TEST(TreeBuilderTest, InlineDisplay) {
  dom::Document doc;
  auto* body = doc.CreateElement(dom::TagId::kBody);
  doc.AppendChild(body);
  auto* span = doc.CreateElement(dom::TagId::kSpan);
  body->AppendChild(span);

  auto sheet = css::CssParser::Parse("span { display: inline; }");
  Vector<css::Stylesheet> sheets;
  sheets.push_back(std::move(sheet));

  LayoutContext ctx;
  ctx.viewport_width = 800;
  ctx.viewport_height = 600;
  ctx.stylesheets = &sheets;

  auto* root = LayoutEngine::Layout(&doc, ctx);
  ASSERT_NE(root, nullptr);
  auto* body_box = root->first_child;
  ASSERT_NE(body_box, nullptr);
  auto* span_box = body_box->first_child;
  ASSERT_NE(span_box, nullptr);
  EXPECT_EQ(span_box->type, LayoutType::kInline);
}

TEST(TreeBuilderTest, FlexDisplay) {
  dom::Document doc;
  auto* body = doc.CreateElement(dom::TagId::kBody);
  doc.AppendChild(body);
  auto* div = doc.CreateElement(dom::TagId::kDiv);
  body->AppendChild(div);

  auto sheet = css::CssParser::Parse("div { display: flex; }");
  Vector<css::Stylesheet> sheets;
  sheets.push_back(std::move(sheet));

  LayoutContext ctx;
  ctx.viewport_width = 800;
  ctx.viewport_height = 600;
  ctx.stylesheets = &sheets;

  auto* root = LayoutEngine::Layout(&doc, ctx);
  ASSERT_NE(root, nullptr);
  auto* body_box = root->first_child;
  ASSERT_NE(body_box, nullptr);
  auto* div_box = body_box->first_child;
  ASSERT_NE(div_box, nullptr);
  EXPECT_EQ(div_box->type, LayoutType::kFlex);
}

}  // namespace
}  // namespace vx::layout
