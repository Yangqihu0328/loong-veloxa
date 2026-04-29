#include "veloxa/core/layout/layout_engine.h"

#include <gtest/gtest.h>

#include "veloxa/core/css/parser.h"
#include "veloxa/core/dom/document.h"
#include "veloxa/core/layout/text_shaper.h"

namespace vx::layout {
namespace {

TEST(InlinePositionTest, TextMeasurement) {
  dom::Document doc;
  auto* body = doc.CreateElement(dom::TagId::kBody);
  doc.AppendChild(body);
  auto* span = doc.CreateElement(dom::TagId::kSpan);
  body->AppendChild(span);
  auto* text = doc.CreateText("Hello");
  span->AppendChild(text);

  auto sheet = css::CssParser::Parse("span { display: inline; }");
  Vector<css::Stylesheet> sheets;
  sheets.push_back(std::move(sheet));

  SimpleTextShaper shaper;
  LayoutContext ctx;
  ctx.text_shaper = &shaper;
  ctx.viewport_width = 800;
  ctx.viewport_height = 600;
  ctx.stylesheets = &sheets;

  auto* root = LayoutEngine::Layout(&doc, ctx);
  auto* body_box = root->first_child;
  auto* span_box = body_box->first_child;
  ASSERT_NE(span_box, nullptr);
  EXPECT_EQ(span_box->type, LayoutType::kInline);

  auto* text_box = span_box->first_child;
  ASSERT_NE(text_box, nullptr);
  EXPECT_FLOAT_EQ(text_box->content_width, 48.0f);
  // R4 #21（TASK-20260426-01）：TextMetrics.height 现在 = ascent + descent
  // = font_size × 1.0（CSS 2.1 §10.8.1 字体度量），而行高 1.2 由 line-height
  // 在 line-box 层计算（半-leading 撑高）。这里断言字体本身高度而非行高。
  EXPECT_FLOAT_EQ(text_box->content_height, 16.0f);
}

TEST(InlinePositionTest, TextWrapping) {
  dom::Document doc;
  auto* body = doc.CreateElement(dom::TagId::kBody);
  doc.AppendChild(body);
  auto* container = doc.CreateElement(dom::TagId::kDiv);
  container->AddClass(InternedString::Intern("narrow"));
  body->AppendChild(container);
  auto* span = doc.CreateElement(dom::TagId::kSpan);
  span->AddClass(InternedString::Intern("inl"));
  container->AppendChild(span);
  auto* text1 = doc.CreateText("Hello ");
  span->AppendChild(text1);
  auto* text2 = doc.CreateText("World");
  span->AppendChild(text2);

  auto sheet = css::CssParser::Parse(
      ".narrow { width: 100px; } .inl { display: inline; }");
  Vector<css::Stylesheet> sheets;
  sheets.push_back(std::move(sheet));

  SimpleTextShaper shaper;
  LayoutContext ctx;
  ctx.text_shaper = &shaper;
  ctx.viewport_width = 800;
  ctx.viewport_height = 600;
  ctx.stylesheets = &sheets;

  auto* root = LayoutEngine::Layout(&doc, ctx);
  auto* body_box = root->first_child;
  auto* container_box = body_box->first_child;
  auto* span_box = container_box->first_child;
  ASSERT_NE(span_box, nullptr);

  auto* t1 = span_box->first_child;
  auto* t2 = t1->next_sibling;
  ASSERT_NE(t1, nullptr);
  ASSERT_NE(t2, nullptr);

  // "Hello " = 6 chars, width = 6 * 16 * 0.6 = 57.6, fits in 100
  EXPECT_FLOAT_EQ(t1->x, 0.0f);
  // R4 #21：text 的 y = baseline_y - ascent + offset。
  //   font_size 16 → ascent 12.8 / descent 3.2 / line_height 19.2
  //   half_leading_top = (19.2 - 16) / 2 = 1.6
  //   baseline_y = 0 + 12.8 + 1.6 = 14.4
  //   t1->y = 14.4 - 12.8 = 1.6
  EXPECT_FLOAT_EQ(t1->y, 1.6f);
  // "World" = 5 chars, width = 48.0, 57.6+48 = 105.6 > 100 → wraps
  EXPECT_FLOAT_EQ(t2->x, 0.0f);
  // 第 2 行 top = line_height = 19.2，t2->y = 19.2 + 1.6 = 20.8
  EXPECT_FLOAT_EQ(t2->y, 20.8f);
}

TEST(InlinePositionTest, MultipleTextNodes) {
  dom::Document doc;
  auto* body = doc.CreateElement(dom::TagId::kBody);
  doc.AppendChild(body);
  auto* span = doc.CreateElement(dom::TagId::kSpan);
  span->AddClass(InternedString::Intern("inl"));
  body->AppendChild(span);
  auto* t1 = doc.CreateText("AA");
  span->AppendChild(t1);
  auto* t2 = doc.CreateText("BB");
  span->AppendChild(t2);
  auto* t3 = doc.CreateText("CC");
  span->AppendChild(t3);

  auto sheet = css::CssParser::Parse(".inl { display: inline; }");
  Vector<css::Stylesheet> sheets;
  sheets.push_back(std::move(sheet));

  SimpleTextShaper shaper;
  LayoutContext ctx;
  ctx.text_shaper = &shaper;
  ctx.viewport_width = 800;
  ctx.viewport_height = 600;
  ctx.stylesheets = &sheets;

  auto* root = LayoutEngine::Layout(&doc, ctx);
  auto* body_box = root->first_child;
  auto* span_box = body_box->first_child;
  ASSERT_NE(span_box, nullptr);

  f32 char_width = 16.0f * 0.6f;
  auto* b1 = span_box->first_child;
  auto* b2 = b1->next_sibling;
  auto* b3 = b2->next_sibling;
  EXPECT_FLOAT_EQ(b1->x, 0.0f);
  EXPECT_FLOAT_EQ(b2->x, 2.0f * char_width);
  EXPECT_FLOAT_EQ(b3->x, 4.0f * char_width);
  // R4 #21：3 个 text 同一 line-box，y = half_leading_top = 1.6 px。
  EXPECT_FLOAT_EQ(b1->y, 1.6f);
  EXPECT_FLOAT_EQ(b2->y, 1.6f);
  EXPECT_FLOAT_EQ(b3->y, 1.6f);
}

TEST(InlinePositionTest, InlineWidth) {
  dom::Document doc;
  auto* body = doc.CreateElement(dom::TagId::kBody);
  doc.AppendChild(body);
  auto* span = doc.CreateElement(dom::TagId::kSpan);
  span->AddClass(InternedString::Intern("sized"));
  body->AppendChild(span);

  auto sheet = css::CssParser::Parse(
      ".sized { display: inline; width: 200px; }");
  Vector<css::Stylesheet> sheets;
  sheets.push_back(std::move(sheet));

  SimpleTextShaper shaper;
  LayoutContext ctx;
  ctx.text_shaper = &shaper;
  ctx.viewport_width = 800;
  ctx.viewport_height = 600;
  ctx.stylesheets = &sheets;

  auto* root = LayoutEngine::Layout(&doc, ctx);
  auto* body_box = root->first_child;
  auto* span_box = body_box->first_child;
  ASSERT_NE(span_box, nullptr);
  EXPECT_FLOAT_EQ(span_box->content_width, 200.0f);
}

TEST(InlinePositionTest, RelativePositionLeft) {
  dom::Document doc;
  auto* body = doc.CreateElement(dom::TagId::kBody);
  doc.AppendChild(body);
  auto* div = doc.CreateElement(dom::TagId::kDiv);
  div->AddClass(InternedString::Intern("rel"));
  body->AppendChild(div);

  auto sheet = css::CssParser::Parse(
      ".rel { position: relative; left: 10px; }");
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
  EXPECT_FLOAT_EQ(div_box->x, 10.0f);
}

TEST(InlinePositionTest, RelativePositionTop) {
  dom::Document doc;
  auto* body = doc.CreateElement(dom::TagId::kBody);
  doc.AppendChild(body);
  auto* div = doc.CreateElement(dom::TagId::kDiv);
  div->AddClass(InternedString::Intern("rel"));
  body->AppendChild(div);

  auto sheet = css::CssParser::Parse(
      ".rel { position: relative; top: 20px; }");
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
  EXPECT_FLOAT_EQ(div_box->y, 20.0f);
}

TEST(InlinePositionTest, AbsolutePositionLeftTop) {
  dom::Document doc;
  auto* body = doc.CreateElement(dom::TagId::kBody);
  doc.AppendChild(body);
  auto* div = doc.CreateElement(dom::TagId::kDiv);
  div->AddClass(InternedString::Intern("abs"));
  body->AppendChild(div);

  auto sheet = css::CssParser::Parse(
      ".abs { position: absolute; left: 50px; top: 30px; }");
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
  EXPECT_FLOAT_EQ(div_box->x, 50.0f);
  EXPECT_FLOAT_EQ(div_box->y, 30.0f);
}

TEST(InlinePositionTest, AbsolutePositionWidth) {
  dom::Document doc;
  auto* body = doc.CreateElement(dom::TagId::kBody);
  doc.AppendChild(body);
  auto* div = doc.CreateElement(dom::TagId::kDiv);
  div->AddClass(InternedString::Intern("abs"));
  body->AppendChild(div);

  auto sheet = css::CssParser::Parse(
      ".abs { position: absolute; left: 10px; top: 10px;"
      " width: 200px; height: 100px; }");
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
  EXPECT_FLOAT_EQ(div_box->x, 10.0f);
  EXPECT_FLOAT_EQ(div_box->y, 10.0f);
  EXPECT_FLOAT_EQ(div_box->content_width, 200.0f);
  EXPECT_FLOAT_EQ(div_box->content_height, 100.0f);
}

}  // namespace
}  // namespace vx::layout
