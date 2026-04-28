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

// =============================================================================
// CSS 2.1 §8.3.1 Vertical Margin Collapsing — TASK-20260426-01 R3 / #20
//
// 验证矩阵（与 spec §5.3 / creative D1.1 对齐）：
//   A4 sibling collapse        — adjoining 同级 margin 取 max(pos)+min(neg)
//   A5 collapse-through 嵌套    — 空 box 的 top/bottom margin 串成单个 chain
//   A6 negative margin 数学     — 正负各极值后相加
//   A7 BFC root 阻断            — overflow: hidden|scroll|auto 不与外部 collapse
//   A8 (stub)                  — 完整 first/last child + clearance 留 P3
// =============================================================================

namespace {

// 测试 helper：构造 doc with 多个 div sibling + style，返回 root LayoutBox。
LayoutBox* LayoutTwoDivsUnderBody(const char* css,
                                  Vector<css::Stylesheet>& sheets,
                                  dom::Document& doc, LayoutContext& ctx,
                                  dom::TagId tag1 = dom::TagId::kDiv,
                                  dom::TagId tag2 = dom::TagId::kDiv) {
  auto* body = doc.CreateElement(dom::TagId::kBody);
  doc.AppendChild(body);
  auto* a = doc.CreateElement(tag1);
  a->set_id(InternedString::Intern("a"));
  body->AppendChild(a);
  auto* b = doc.CreateElement(tag2);
  b->set_id(InternedString::Intern("b"));
  body->AppendChild(b);

  sheets.push_back(css::CssParser::Parse(css));

  ctx.viewport_width = 800;
  ctx.viewport_height = 600;
  ctx.stylesheets = &sheets;

  return LayoutEngine::Layout(&doc, ctx);
}

}  // namespace

// ----- A4 sibling collapse ----------------------------------------------------

TEST(MarginCollapseLayoutTest, A4_AdjacentSiblingsCollapseToMax) {
  dom::Document doc;
  Vector<css::Stylesheet> sheets;
  LayoutContext ctx;
  auto* root = LayoutTwoDivsUnderBody(
      "#a { height: 50px; margin-bottom: 30px; }"
      "#b { height: 40px; margin-top: 20px; }",
      sheets, doc, ctx);

  auto* body = root->first_child;
  auto* a = body->first_child;
  auto* b = a->next_sibling;
  ASSERT_NE(b, nullptr);

  EXPECT_FLOAT_EQ(a->y, 0.0f);
  // a.bottom = 50; collapsed margin = max(30, 20) = 30 → b.y = 80
  EXPECT_FLOAT_EQ(b->y, 80.0f);
}

TEST(MarginCollapseLayoutTest, A4_SiblingsBothEqualMarginsCollapseOnce) {
  dom::Document doc;
  Vector<css::Stylesheet> sheets;
  LayoutContext ctx;
  auto* root = LayoutTwoDivsUnderBody(
      "#a { height: 20px; margin-bottom: 25px; }"
      "#b { height: 20px; margin-top: 25px; }",
      sheets, doc, ctx);

  auto* body = root->first_child;
  auto* a = body->first_child;
  auto* b = a->next_sibling;
  // 旧实现（non-collapse）会得到 b.y = 20 + 25 + 25 = 70；
  // W3C collapse 后 b.y = 20 + 25 = 45。
  EXPECT_FLOAT_EQ(b->y, 45.0f);
}

// ----- A5 collapse-through ----------------------------------------------------

TEST(MarginCollapseLayoutTest, A5_EmptyBoxCollapsesThroughBetweenSiblings) {
  dom::Document doc;
  auto* body = doc.CreateElement(dom::TagId::kBody);
  doc.AppendChild(body);
  auto* a = doc.CreateElement(dom::TagId::kDiv);
  a->set_id(InternedString::Intern("a"));
  body->AppendChild(a);
  auto* mid = doc.CreateElement(dom::TagId::kDiv);
  mid->set_id(InternedString::Intern("mid"));
  body->AppendChild(mid);
  auto* b = doc.CreateElement(dom::TagId::kDiv);
  b->set_id(InternedString::Intern("b"));
  body->AppendChild(b);

  Vector<css::Stylesheet> sheets;
  sheets.push_back(css::CssParser::Parse(
      "#a { height: 50px; margin-bottom: 10px; }"
      "#mid { margin-top: 30px; margin-bottom: 15px; }"  // empty + auto height
      "#b { height: 40px; margin-top: 5px; }"));

  LayoutContext ctx;
  ctx.viewport_width = 800;
  ctx.viewport_height = 600;
  ctx.stylesheets = &sheets;

  auto* root = LayoutEngine::Layout(&doc, ctx);
  auto* body_box = root->first_child;
  auto* a_box = body_box->first_child;
  auto* mid_box = a_box->next_sibling;
  auto* b_box = mid_box->next_sibling;

  // mid 是 collapse-through：自身 height=0、无 padding/border。
  // 所有相邻 margin 进同一 chain：max(10, 30, 15, 5) = 30
  // a.bottom = 50 → b.y = 50 + 30 = 80
  EXPECT_TRUE(mid_box->collapsed_through);
  EXPECT_FLOAT_EQ(b_box->y, 80.0f);
}

// ----- A6 negative margin -----------------------------------------------------

TEST(MarginCollapseLayoutTest, A6_NegativeMarginPullsNextSiblingUp) {
  dom::Document doc;
  Vector<css::Stylesheet> sheets;
  LayoutContext ctx;
  auto* root = LayoutTwoDivsUnderBody(
      "#a { height: 50px; margin-bottom: 20px; }"
      "#b { height: 40px; margin-top: -10px; }",
      sheets, doc, ctx);

  auto* body = root->first_child;
  auto* a = body->first_child;
  auto* b = a->next_sibling;
  // collapsed = max(20) + min(-10) = 20 + (-10) = 10
  EXPECT_FLOAT_EQ(b->y, 50.0f + 10.0f);
}

TEST(MarginCollapseLayoutTest, A6_AllNegativeMarginsCollapseToMostNegative) {
  dom::Document doc;
  Vector<css::Stylesheet> sheets;
  LayoutContext ctx;
  auto* root = LayoutTwoDivsUnderBody(
      "#a { height: 50px; margin-bottom: -5px; }"
      "#b { height: 40px; margin-top: -15px; }",
      sheets, doc, ctx);

  auto* body = root->first_child;
  auto* a = body->first_child;
  auto* b = a->next_sibling;
  // collapsed = 0 + min(-5, -15) = -15
  EXPECT_FLOAT_EQ(b->y, 50.0f + (-15.0f));
}

// ----- A7 BFC root 阻断 -------------------------------------------------------

TEST(MarginCollapseLayoutTest, A7_OverflowHiddenBlocksMarginCollapse) {
  dom::Document doc;
  Vector<css::Stylesheet> sheets;
  LayoutContext ctx;
  auto* root = LayoutTwoDivsUnderBody(
      "#a { height: 50px; margin-bottom: 30px; }"
      "#b { height: 40px; margin-top: 20px; overflow: hidden; }",
      sheets, doc, ctx);

  auto* body = root->first_child;
  auto* a = body->first_child;
  auto* b = a->next_sibling;
  // b 是 BFC root：margin 不与外部 collapse。
  // b.y = a.bottom_margin_box = 50 + 30 + 20 = 100
  EXPECT_FLOAT_EQ(b->y, 100.0f);
}

TEST(MarginCollapseLayoutTest, A7_OverflowAutoBlocksMarginCollapse) {
  dom::Document doc;
  Vector<css::Stylesheet> sheets;
  LayoutContext ctx;
  auto* root = LayoutTwoDivsUnderBody(
      "#a { height: 50px; margin-bottom: 30px; }"
      "#b { height: 40px; margin-top: 20px; overflow: auto; }",
      sheets, doc, ctx);

  auto* body = root->first_child;
  auto* a = body->first_child;
  auto* b = a->next_sibling;
  EXPECT_FLOAT_EQ(b->y, 100.0f);
}

TEST(MarginCollapseLayoutTest, A7_OverflowVisibleStillCollapses) {
  dom::Document doc;
  Vector<css::Stylesheet> sheets;
  LayoutContext ctx;
  auto* root = LayoutTwoDivsUnderBody(
      "#a { height: 50px; margin-bottom: 30px; }"
      "#b { height: 40px; margin-top: 20px; overflow: visible; }",
      sheets, doc, ctx);

  auto* body = root->first_child;
  auto* a = body->first_child;
  auto* b = a->next_sibling;
  // 反向探针：overflow:visible 不创建 BFC，必须 collapse。
  EXPECT_FLOAT_EQ(b->y, 80.0f);
}

// ----- 三 sibling 链路 + collapse-through 嵌套（spec §5.3 A6） ---------------

TEST(MarginCollapseLayoutTest, A5_TwoCollapseThroughInARow) {
  dom::Document doc;
  auto* body = doc.CreateElement(dom::TagId::kBody);
  doc.AppendChild(body);
  auto* a = doc.CreateElement(dom::TagId::kDiv);
  a->set_id(InternedString::Intern("a"));
  body->AppendChild(a);
  auto* m1 = doc.CreateElement(dom::TagId::kDiv);
  m1->set_id(InternedString::Intern("m1"));
  body->AppendChild(m1);
  auto* m2 = doc.CreateElement(dom::TagId::kDiv);
  m2->set_id(InternedString::Intern("m2"));
  body->AppendChild(m2);
  auto* b = doc.CreateElement(dom::TagId::kDiv);
  b->set_id(InternedString::Intern("b"));
  body->AppendChild(b);

  Vector<css::Stylesheet> sheets;
  sheets.push_back(css::CssParser::Parse(
      "#a { height: 50px; margin-bottom: 10px; }"
      "#m1 { margin-top: 25px; margin-bottom: 15px; }"
      "#m2 { margin-top: 12px; margin-bottom: 18px; }"
      "#b { height: 40px; margin-top: 5px; }"));

  LayoutContext ctx;
  ctx.viewport_width = 800;
  ctx.viewport_height = 600;
  ctx.stylesheets = &sheets;
  auto* root = LayoutEngine::Layout(&doc, ctx);
  auto* body_box = root->first_child;
  auto* a_box = body_box->first_child;
  auto* m1_box = a_box->next_sibling;
  auto* m2_box = m1_box->next_sibling;
  auto* b_box = m2_box->next_sibling;

  // 4 个 margin 都进同一 chain（m1/m2 都 collapse-through）：
  // max(10, 25, 15, 12, 18, 5) = 25
  EXPECT_TRUE(m1_box->collapsed_through);
  EXPECT_TRUE(m2_box->collapsed_through);
  EXPECT_FLOAT_EQ(b_box->y, 50.0f + 25.0f);
}

// ----- collapse-through 不适用：有 padding 阻断 -------------------------------

TEST(MarginCollapseLayoutTest, A5_PaddingBreaksCollapseThrough) {
  dom::Document doc;
  Vector<css::Stylesheet> sheets;
  LayoutContext ctx;
  auto* root = LayoutTwoDivsUnderBody(
      "#a { height: 50px; margin-bottom: 10px; padding-bottom: 5px; }"
      "#b { height: 40px; margin-top: 20px; }",
      sheets, doc, ctx);

  auto* body = root->first_child;
  auto* a = body->first_child;
  auto* b = a->next_sibling;
  // a 有 padding-bottom: 5 → border-box 高 55；之后 sibling 间正常 collapse。
  EXPECT_FALSE(a->collapsed_through);
  EXPECT_FLOAT_EQ(b->y, 55.0f + 20.0f);  // = 75
}

// ----- parent 自动高度受 collapse 影响 ----------------------------------------

TEST(MarginCollapseLayoutTest, AutoHeightParentReflectsCollapsedMargins) {
  dom::Document doc;
  auto* body = doc.CreateElement(dom::TagId::kBody);
  doc.AppendChild(body);
  auto* parent = doc.CreateElement(dom::TagId::kDiv);
  parent->set_id(InternedString::Intern("parent"));
  body->AppendChild(parent);
  auto* c1 = doc.CreateElement(dom::TagId::kDiv);
  parent->AppendChild(c1);
  auto* c2 = doc.CreateElement(dom::TagId::kDiv);
  parent->AppendChild(c2);

  Vector<css::Stylesheet> sheets;
  sheets.push_back(css::CssParser::Parse(
      "#parent { padding: 5px; }"  // padding 阻断 first/last with parent
      "div div { height: 30px; margin: 10px 0; }"));

  LayoutContext ctx;
  ctx.viewport_width = 800;
  ctx.viewport_height = 600;
  ctx.stylesheets = &sheets;

  auto* root = LayoutEngine::Layout(&doc, ctx);
  auto* body_box = root->first_child;
  auto* p_box = body_box->first_child;
  // 内部布局：c1 (margin-top 10 + h 30) + collapse(10,10)=10 + c2 (h 30 + mb 10)
  // 末 chain 含 c2 的 margin-bottom = 10
  // content_height = 10(top of c1, no parent collapse due to padding) + 30
  //                + 10(collapsed sibling) + 30 + 10(trailing) = 90
  EXPECT_FLOAT_EQ(p_box->content_height, 90.0f);
}

}  // namespace vx::layout
