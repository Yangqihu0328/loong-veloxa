#include "veloxa/core/css/parser.h"
#include "veloxa/core/html/parser.h"
#include "veloxa/core/layout/layout_engine.h"
#include "veloxa/core/layout/text_shaper.h"

#include <gtest/gtest.h>

namespace vx::layout {
namespace {

class LayoutIntegrationTest : public ::testing::Test {
 protected:
  SimpleTextShaper shaper_;
  LayoutContext ctx_;

  void SetUp() override {
    ctx_.text_shaper = &shaper_;
    ctx_.viewport_width = 800;
    ctx_.viewport_height = 600;
    ctx_.root_font_size = 16.0f;
  }

  LayoutBox* DoLayout(const char* html, const char* css) {
    doc_ = html::Parser::Parse(html);
    sheet_ = css::CssParser::Parse(css);
    sheets_.clear();
    sheets_.push_back(std::move(sheet_));
    ctx_.stylesheets = &sheets_;
    return LayoutEngine::Layout(doc_, ctx_);
  }

 private:
  dom::Document* doc_ = nullptr;
  css::Stylesheet sheet_;
  Vector<css::Stylesheet> sheets_;
};

TEST_F(LayoutIntegrationTest, SingleDivFixedSize) {
  auto* root = DoLayout(
      "<div id='box'></div>",
      "#box { width: 200px; height: 100px; }");
  ASSERT_NE(root, nullptr);
  auto* child = root->first_child;
  ASSERT_NE(child, nullptr);
  EXPECT_FLOAT_EQ(child->content_width, 200.0f);
  EXPECT_FLOAT_EQ(child->content_height, 100.0f);
}

TEST_F(LayoutIntegrationTest, HeaderContentFooter) {
  auto* root = DoLayout(
      "<div id='header'></div>"
      "<div id='content'></div>"
      "<div id='footer'></div>",
      "#header { height: 60px; }"
      "#content { height: 400px; }"
      "#footer { height: 40px; }");
  ASSERT_NE(root, nullptr);

  auto* header = root->first_child;
  ASSERT_NE(header, nullptr);
  EXPECT_FLOAT_EQ(header->y, 0.0f);
  EXPECT_FLOAT_EQ(header->content_height, 60.0f);

  auto* content = header->next_sibling;
  ASSERT_NE(content, nullptr);
  EXPECT_FLOAT_EQ(content->y, 60.0f);
  EXPECT_FLOAT_EQ(content->content_height, 400.0f);

  auto* footer = content->next_sibling;
  ASSERT_NE(footer, nullptr);
  EXPECT_FLOAT_EQ(footer->y, 460.0f);
  EXPECT_FLOAT_EQ(footer->content_height, 40.0f);
}

TEST_F(LayoutIntegrationTest, FlexNavBar) {
  auto* root = DoLayout(
      "<nav class='navbar'>"
      "  <div class='item'></div>"
      "  <div class='item'></div>"
      "  <div class='item'></div>"
      "</nav>",
      ".navbar { display: flex; width: 600px; }"
      ".item { flex-grow: 1; height: 40px; }");
  ASSERT_NE(root, nullptr);
  auto* nav = root->first_child;
  ASSERT_NE(nav, nullptr);
  EXPECT_EQ(nav->type, LayoutType::kFlex);

  auto* i1 = nav->first_child;
  auto* i2 = i1 ? i1->next_sibling : nullptr;
  auto* i3 = i2 ? i2->next_sibling : nullptr;
  ASSERT_NE(i1, nullptr);
  ASSERT_NE(i2, nullptr);
  ASSERT_NE(i3, nullptr);

  EXPECT_FLOAT_EQ(i1->content_width, 200.0f);
  EXPECT_FLOAT_EQ(i2->content_width, 200.0f);
  EXPECT_FLOAT_EQ(i3->content_width, 200.0f);
  EXPECT_FLOAT_EQ(i1->content_height, 40.0f);
}

TEST_F(LayoutIntegrationTest, NestedBlockFlex) {
  auto* root = DoLayout(
      "<div class='outer'>"
      "  <div class='flex-row'>"
      "    <div class='cell'></div>"
      "    <div class='cell'></div>"
      "  </div>"
      "</div>",
      ".outer { width: 400px; }"
      ".flex-row { display: flex; }"
      ".cell { flex-basis: 200px; height: 50px; }");
  ASSERT_NE(root, nullptr);
  auto* outer = root->first_child;
  ASSERT_NE(outer, nullptr);
  EXPECT_FLOAT_EQ(outer->content_width, 400.0f);

  auto* flex_row = outer->first_child;
  ASSERT_NE(flex_row, nullptr);
  EXPECT_EQ(flex_row->type, LayoutType::kFlex);

  auto* c1 = flex_row->first_child;
  auto* c2 = c1 ? c1->next_sibling : nullptr;
  ASSERT_NE(c1, nullptr);
  ASSERT_NE(c2, nullptr);
  EXPECT_FLOAT_EQ(c1->content_width, 200.0f);
  EXPECT_FLOAT_EQ(c2->content_width, 200.0f);
  EXPECT_FLOAT_EQ(c2->x, 200.0f);
}

TEST_F(LayoutIntegrationTest, DisplayNoneSkipped) {
  auto* root = DoLayout(
      "<div class='visible'></div>"
      "<div class='hidden'></div>"
      "<div class='visible'></div>",
      ".visible { height: 50px; }"
      ".hidden { display: none; }");
  ASSERT_NE(root, nullptr);

  u32 count = root->child_count();
  EXPECT_EQ(count, 2u);

  auto* first = root->first_child;
  ASSERT_NE(first, nullptr);
  auto* second = first->next_sibling;
  ASSERT_NE(second, nullptr);
  EXPECT_FLOAT_EQ(second->y, 50.0f);
}

TEST_F(LayoutIntegrationTest, PaddingAndMargin) {
  auto* root = DoLayout(
      "<div id='box'></div>",
      "#box { width: 200px; height: 100px; padding: 10px; margin: 20px; }");
  ASSERT_NE(root, nullptr);
  auto* box = root->first_child;
  ASSERT_NE(box, nullptr);
  EXPECT_FLOAT_EQ(box->content_width, 200.0f);
  EXPECT_FLOAT_EQ(box->padding[LayoutBox::kTop], 10.0f);
  EXPECT_FLOAT_EQ(box->padding[LayoutBox::kRight], 10.0f);
  EXPECT_FLOAT_EQ(box->margin[LayoutBox::kTop], 20.0f);
  EXPECT_FLOAT_EQ(box->border_box_width(), 220.0f);
  EXPECT_FLOAT_EQ(box->margin_box_width(), 260.0f);
  EXPECT_FLOAT_EQ(box->y, 20.0f);
}

TEST_F(LayoutIntegrationTest, TextInBlock) {
  auto* root = DoLayout(
      "<p>Hello</p>",
      "p { width: 400px; }");
  ASSERT_NE(root, nullptr);
  auto* p = root->first_child;
  ASSERT_NE(p, nullptr);
  EXPECT_FLOAT_EQ(p->content_width, 400.0f);
  EXPECT_GT(p->content_height, 0.0f);
}

TEST_F(LayoutIntegrationTest, AutoWidthFillsViewport) {
  auto* root = DoLayout(
      "<div id='full'></div>",
      "#full { height: 50px; }");
  ASSERT_NE(root, nullptr);
  auto* div = root->first_child;
  ASSERT_NE(div, nullptr);
  EXPECT_FLOAT_EQ(div->content_width, 800.0f);
}

// TASK-20260426-01 R2 / #28 端到端：HTML inline style 经 parser 解析后
// 应在 layout 中产生与外联样式表等价的 padding。
// （A2 行为一致性：HTML 路径 ≡ JS 路径 ≡ stylesheet 路径）
TEST_F(LayoutIntegrationTest, InlineStyleAttributePaddingTakesEffect) {
  auto* root = DoLayout(
      "<div id='inline-pad' style='padding: 10px'></div>",
      "#inline-pad { width: 100px; height: 50px; }");
  ASSERT_NE(root, nullptr);
  auto* div = root->first_child;
  ASSERT_NE(div, nullptr);
  EXPECT_FLOAT_EQ(div->padding[LayoutBox::kTop], 10.0f);
  EXPECT_FLOAT_EQ(div->padding[LayoutBox::kRight], 10.0f);
  EXPECT_FLOAT_EQ(div->padding[LayoutBox::kBottom], 10.0f);
  EXPECT_FLOAT_EQ(div->padding[LayoutBox::kLeft], 10.0f);
  EXPECT_FLOAT_EQ(div->content_width, 100.0f);
  EXPECT_FLOAT_EQ(div->content_height, 50.0f);
}

}  // namespace
}  // namespace vx::layout
