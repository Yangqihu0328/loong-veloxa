#include "veloxa/core/layout/layout_engine.h"

#include <gtest/gtest.h>

#include "veloxa/core/css/parser.h"
#include "veloxa/core/dom/document.h"

namespace vx::layout {
namespace {

TEST(BlockLayoutTest, FixedWidthHeight) {
  dom::Document doc;
  auto* body = doc.CreateElement(dom::TagId::kBody);
  doc.AppendChild(body);
  auto* div = doc.CreateElement(dom::TagId::kDiv);
  body->AppendChild(div);

  auto sheet = css::CssParser::Parse("div { width: 200px; height: 100px; }");
  Vector<css::Stylesheet> sheets;
  sheets.push_back(std::move(sheet));

  LayoutContext ctx;
  ctx.viewport_width = 800;
  ctx.viewport_height = 600;
  ctx.stylesheets = &sheets;

  auto* root = LayoutEngine::Layout(&doc, ctx);
  auto* body_box = root->first_child;
  auto* div_box = body_box->first_child;
  ASSERT_NE(div_box, nullptr);
  EXPECT_FLOAT_EQ(div_box->content_width, 200.0f);
  EXPECT_FLOAT_EQ(div_box->content_height, 100.0f);
}

TEST(BlockLayoutTest, AutoWidthFillsContainer) {
  dom::Document doc;
  auto* body = doc.CreateElement(dom::TagId::kBody);
  doc.AppendChild(body);
  auto* div = doc.CreateElement(dom::TagId::kDiv);
  body->AppendChild(div);

  LayoutContext ctx;
  ctx.viewport_width = 800;
  ctx.viewport_height = 600;

  auto* root = LayoutEngine::Layout(&doc, ctx);
  auto* body_box = root->first_child;
  auto* div_box = body_box->first_child;
  ASSERT_NE(div_box, nullptr);
  EXPECT_FLOAT_EQ(div_box->content_width, 800.0f);
}

TEST(BlockLayoutTest, AutoHeightFromChildren) {
  dom::Document doc;
  auto* body = doc.CreateElement(dom::TagId::kBody);
  doc.AppendChild(body);
  auto* parent_div = doc.CreateElement(dom::TagId::kDiv);
  body->AppendChild(parent_div);
  auto* child_el = doc.CreateElement(dom::TagId::kP);
  parent_div->AppendChild(child_el);

  auto sheet = css::CssParser::Parse("p { height: 50px; }");
  Vector<css::Stylesheet> sheets;
  sheets.push_back(std::move(sheet));

  LayoutContext ctx;
  ctx.viewport_width = 800;
  ctx.viewport_height = 600;
  ctx.stylesheets = &sheets;

  auto* root = LayoutEngine::Layout(&doc, ctx);
  auto* body_box = root->first_child;
  auto* parent_box = body_box->first_child;
  ASSERT_NE(parent_box, nullptr);
  EXPECT_FLOAT_EQ(parent_box->content_height, 50.0f);
}

TEST(BlockLayoutTest, MultipleBlocksVerticalStack) {
  dom::Document doc;
  auto* body = doc.CreateElement(dom::TagId::kBody);
  doc.AppendChild(body);

  auto sheet = css::CssParser::Parse(".item { height: 30px; }");
  Vector<css::Stylesheet> sheets;
  sheets.push_back(std::move(sheet));

  for (int i = 0; i < 3; ++i) {
    auto* div = doc.CreateElement(dom::TagId::kDiv);
    div->AddClass(InternedString::Intern("item"));
    body->AppendChild(div);
  }

  LayoutContext ctx;
  ctx.viewport_width = 800;
  ctx.viewport_height = 600;
  ctx.stylesheets = &sheets;

  auto* root = LayoutEngine::Layout(&doc, ctx);
  auto* body_box = root->first_child;
  ASSERT_NE(body_box, nullptr);
  EXPECT_EQ(body_box->child_count(), 3u);

  f32 expected_y = 0;
  for (auto* child = body_box->first_child; child;
       child = child->next_sibling) {
    EXPECT_FLOAT_EQ(child->y, expected_y);
    expected_y += child->border_box_height();
  }
  EXPECT_FLOAT_EQ(body_box->content_height, 90.0f);
}

TEST(BlockLayoutTest, PaddingAffectsLayout) {
  dom::Document doc;
  auto* body = doc.CreateElement(dom::TagId::kBody);
  doc.AppendChild(body);
  auto* div = doc.CreateElement(dom::TagId::kDiv);
  body->AppendChild(div);

  auto sheet = css::CssParser::Parse("div { padding: 10px; }");
  Vector<css::Stylesheet> sheets;
  sheets.push_back(std::move(sheet));

  LayoutContext ctx;
  ctx.viewport_width = 800;
  ctx.viewport_height = 600;
  ctx.stylesheets = &sheets;

  auto* root = LayoutEngine::Layout(&doc, ctx);
  auto* body_box = root->first_child;
  auto* div_box = body_box->first_child;
  ASSERT_NE(div_box, nullptr);
  EXPECT_FLOAT_EQ(div_box->content_width, 780.0f);
  EXPECT_FLOAT_EQ(div_box->padding[LayoutBox::kTop], 10.0f);
  EXPECT_FLOAT_EQ(div_box->padding[LayoutBox::kRight], 10.0f);
  EXPECT_FLOAT_EQ(div_box->padding[LayoutBox::kBottom], 10.0f);
  EXPECT_FLOAT_EQ(div_box->padding[LayoutBox::kLeft], 10.0f);
}

TEST(BlockLayoutTest, BorderAffectsLayout) {
  dom::Document doc;
  auto* body = doc.CreateElement(dom::TagId::kBody);
  doc.AppendChild(body);
  auto* div = doc.CreateElement(dom::TagId::kDiv);
  body->AppendChild(div);

  auto sheet = css::CssParser::Parse("div { border: 2px solid black; }");
  Vector<css::Stylesheet> sheets;
  sheets.push_back(std::move(sheet));

  LayoutContext ctx;
  ctx.viewport_width = 800;
  ctx.viewport_height = 600;
  ctx.stylesheets = &sheets;

  auto* root = LayoutEngine::Layout(&doc, ctx);
  auto* body_box = root->first_child;
  auto* div_box = body_box->first_child;
  ASSERT_NE(div_box, nullptr);
  EXPECT_FLOAT_EQ(div_box->content_width, 796.0f);
  EXPECT_FLOAT_EQ(div_box->border[LayoutBox::kTop], 2.0f);
  EXPECT_FLOAT_EQ(div_box->border[LayoutBox::kLeft], 2.0f);
}

TEST(BlockLayoutTest, MarginAffectsChildPosition) {
  dom::Document doc;
  auto* body = doc.CreateElement(dom::TagId::kBody);
  doc.AppendChild(body);
  auto* div = doc.CreateElement(dom::TagId::kDiv);
  body->AppendChild(div);

  auto sheet =
      css::CssParser::Parse("div { margin: 20px; height: 50px; }");
  Vector<css::Stylesheet> sheets;
  sheets.push_back(std::move(sheet));

  LayoutContext ctx;
  ctx.viewport_width = 800;
  ctx.viewport_height = 600;
  ctx.stylesheets = &sheets;

  auto* root = LayoutEngine::Layout(&doc, ctx);
  auto* body_box = root->first_child;
  auto* div_box = body_box->first_child;
  ASSERT_NE(div_box, nullptr);
  EXPECT_FLOAT_EQ(div_box->x, 20.0f);
  EXPECT_FLOAT_EQ(div_box->y, 20.0f);
}

TEST(BlockLayoutTest, BoxSizingBorderBox) {
  dom::Document doc;
  auto* body = doc.CreateElement(dom::TagId::kBody);
  doc.AppendChild(body);
  auto* div = doc.CreateElement(dom::TagId::kDiv);
  body->AppendChild(div);

  auto sheet = css::CssParser::Parse(
      "div { box-sizing: border-box; width: 200px; padding: 20px; }");
  Vector<css::Stylesheet> sheets;
  sheets.push_back(std::move(sheet));

  LayoutContext ctx;
  ctx.viewport_width = 800;
  ctx.viewport_height = 600;
  ctx.stylesheets = &sheets;

  auto* root = LayoutEngine::Layout(&doc, ctx);
  auto* body_box = root->first_child;
  auto* div_box = body_box->first_child;
  ASSERT_NE(div_box, nullptr);
  EXPECT_FLOAT_EQ(div_box->content_width, 160.0f);
  EXPECT_FLOAT_EQ(div_box->border_box_width(), 200.0f);
}

TEST(BlockLayoutTest, AutoMarginCenter) {
  dom::Document doc;
  auto* body = doc.CreateElement(dom::TagId::kBody);
  doc.AppendChild(body);
  auto* div = doc.CreateElement(dom::TagId::kDiv);
  body->AppendChild(div);

  auto sheet = css::CssParser::Parse(
      "div { width: 200px; margin-left: auto; margin-right: auto; }");
  Vector<css::Stylesheet> sheets;
  sheets.push_back(std::move(sheet));

  LayoutContext ctx;
  ctx.viewport_width = 800;
  ctx.viewport_height = 600;
  ctx.stylesheets = &sheets;

  auto* root = LayoutEngine::Layout(&doc, ctx);
  auto* body_box = root->first_child;
  auto* div_box = body_box->first_child;
  ASSERT_NE(div_box, nullptr);
  EXPECT_FLOAT_EQ(div_box->content_width, 200.0f);
  EXPECT_FLOAT_EQ(div_box->margin[LayoutBox::kLeft], 300.0f);
  EXPECT_FLOAT_EQ(div_box->margin[LayoutBox::kRight], 300.0f);
  EXPECT_FLOAT_EQ(div_box->x, 300.0f);
}

TEST(BlockLayoutTest, MinWidth) {
  dom::Document doc;
  auto* body = doc.CreateElement(dom::TagId::kBody);
  doc.AppendChild(body);
  auto* div = doc.CreateElement(dom::TagId::kDiv);
  body->AppendChild(div);

  auto sheet =
      css::CssParser::Parse("div { width: 50px; min-width: 100px; }");
  Vector<css::Stylesheet> sheets;
  sheets.push_back(std::move(sheet));

  LayoutContext ctx;
  ctx.viewport_width = 800;
  ctx.viewport_height = 600;
  ctx.stylesheets = &sheets;

  auto* root = LayoutEngine::Layout(&doc, ctx);
  auto* body_box = root->first_child;
  auto* div_box = body_box->first_child;
  ASSERT_NE(div_box, nullptr);
  EXPECT_FLOAT_EQ(div_box->content_width, 100.0f);
}

TEST(BlockLayoutTest, MaxWidth) {
  dom::Document doc;
  auto* body = doc.CreateElement(dom::TagId::kBody);
  doc.AppendChild(body);
  auto* div = doc.CreateElement(dom::TagId::kDiv);
  body->AppendChild(div);

  auto sheet =
      css::CssParser::Parse("div { width: 500px; max-width: 200px; }");
  Vector<css::Stylesheet> sheets;
  sheets.push_back(std::move(sheet));

  LayoutContext ctx;
  ctx.viewport_width = 800;
  ctx.viewport_height = 600;
  ctx.stylesheets = &sheets;

  auto* root = LayoutEngine::Layout(&doc, ctx);
  auto* body_box = root->first_child;
  auto* div_box = body_box->first_child;
  ASSERT_NE(div_box, nullptr);
  EXPECT_FLOAT_EQ(div_box->content_width, 200.0f);
}

TEST(BlockLayoutTest, NestedBlocks) {
  dom::Document doc;
  auto* body = doc.CreateElement(dom::TagId::kBody);
  doc.AppendChild(body);
  auto* outer = doc.CreateElement(dom::TagId::kDiv);
  body->AppendChild(outer);
  auto* inner = doc.CreateElement(dom::TagId::kP);
  outer->AppendChild(inner);

  auto sheet =
      css::CssParser::Parse("div { width: 400px; } p { height: 60px; }");
  Vector<css::Stylesheet> sheets;
  sheets.push_back(std::move(sheet));

  LayoutContext ctx;
  ctx.viewport_width = 800;
  ctx.viewport_height = 600;
  ctx.stylesheets = &sheets;

  auto* root = LayoutEngine::Layout(&doc, ctx);
  auto* body_box = root->first_child;
  auto* outer_box = body_box->first_child;
  auto* inner_box = outer_box->first_child;
  ASSERT_NE(inner_box, nullptr);
  EXPECT_FLOAT_EQ(outer_box->content_width, 400.0f);
  EXPECT_FLOAT_EQ(inner_box->content_width, 400.0f);
  EXPECT_FLOAT_EQ(inner_box->content_height, 60.0f);
  EXPECT_FLOAT_EQ(outer_box->content_height, 60.0f);
}

}  // namespace
}  // namespace vx::layout
