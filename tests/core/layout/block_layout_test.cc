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

// =============================================================================
// A8/A9/A10 — first/last child margin collapse with parent
// CSS 2.1 §8.3.1 嵌套规则（TASK-20260430-01）
//
// 测试维度（D4.B 完整档）：
//   #1-#2  主流程：first/last child collapse with parent
//   #3-#7  阻断条件：padding-top / border-bottom / BFC root / explicit-height /
//          min-height
//   #8     deep chain：≥ 3 层嵌套 first child 链式合并
//   #9     collapse-through：跨 parent 边界传播
//   #10    outgoing chain 含 parent.margin-bottom 验证
// =============================================================================

// ----- 主流程 #1-#2 ----------------------------------------------------------

// #1 first child margin-top 与 parent margin-top adjoining → collapse
TEST(MarginCollapseLayoutTest, A8_FirstChildMarginCollapsesWithParent_Basic) {
  dom::Document doc;
  auto* body = doc.CreateElement(dom::TagId::kBody);
  doc.AppendChild(body);
  auto* parent = doc.CreateElement(dom::TagId::kDiv);
  parent->set_id(InternedString::Intern("parent"));
  body->AppendChild(parent);
  auto* child = doc.CreateElement(dom::TagId::kDiv);
  child->set_id(InternedString::Intern("child"));
  parent->AppendChild(child);

  Vector<css::Stylesheet> sheets;
  sheets.push_back(css::CssParser::Parse(
      "#parent { margin-top: 30px; }"
      "#child { margin-top: 20px; height: 50px; }"));

  LayoutContext ctx;
  ctx.viewport_width = 800;
  ctx.viewport_height = 600;
  ctx.stylesheets = &sheets;

  auto* root = LayoutEngine::Layout(&doc, ctx);
  auto* body_box = root->first_child;
  auto* p_box = body_box->first_child;
  auto* c_box = p_box->first_child;

  // parent.margin-top + child.margin-top 合并 → max(30, 20) = 30
  // sibling-level：parent.y 由 body's chain 决定 = 30
  EXPECT_FLOAT_EQ(p_box->y, 30.0f);
  // 关键 RED 信号：实施前 child.y = 20 (chain 未跨函数 propagate)
  //              实施后 child.y = 0  (margin-top 已合入 parent.margin-top chain)
  EXPECT_FLOAT_EQ(c_box->y, 0.0f);
  // 状态字段验证：first-child margin-top 已合入 ancestor
  EXPECT_TRUE(c_box->margin_top_collapsed_into_ancestor);
}

// #2 last child margin-bottom 与 parent margin-bottom adjoining → 渗出
TEST(MarginCollapseLayoutTest, A8_LastChildMarginCollapsesWithParent_Basic) {
  dom::Document doc;
  auto* body = doc.CreateElement(dom::TagId::kBody);
  doc.AppendChild(body);
  auto* parent = doc.CreateElement(dom::TagId::kDiv);
  parent->set_id(InternedString::Intern("parent"));
  body->AppendChild(parent);
  auto* child = doc.CreateElement(dom::TagId::kDiv);
  child->set_id(InternedString::Intern("child"));
  parent->AppendChild(child);
  auto* sibling = doc.CreateElement(dom::TagId::kDiv);
  sibling->set_id(InternedString::Intern("sibling"));
  body->AppendChild(sibling);

  Vector<css::Stylesheet> sheets;
  sheets.push_back(css::CssParser::Parse(
      // parent: height:auto, no padding/border/min-height → blocks_bottom=false
      "#child { margin-bottom: 20px; height: 50px; }"
      "#sibling { margin-top: 30px; height: 30px; }"));

  LayoutContext ctx;
  ctx.viewport_width = 800;
  ctx.viewport_height = 600;
  ctx.stylesheets = &sheets;

  auto* root = LayoutEngine::Layout(&doc, ctx);
  auto* body_box = root->first_child;
  auto* p_box = body_box->first_child;
  auto* c_box = p_box->first_child;
  auto* s_box = p_box->next_sibling;

  EXPECT_FLOAT_EQ(p_box->y, 0.0f);
  EXPECT_FLOAT_EQ(c_box->y, 0.0f);
  // 实施前：trailing 20 计入 parent.content_height = 70；sibling.y = 0+70+30 = 100
  // 实施后：trailing 渗出 → parent.content_height = 50；
  //         sibling 与 parent.margin-bottom (effective=20) collapse → max(20,30) = 30
  //         sibling.y = 0 + 50 + 30 = 80
  EXPECT_FLOAT_EQ(p_box->content_height, 50.0f);
  EXPECT_FLOAT_EQ(s_box->y, 80.0f);
  // 状态字段验证：last-child margin-bottom 已合入 ancestor
  EXPECT_TRUE(c_box->margin_bottom_collapsed_into_ancestor);
}

// ----- 阻断条件 #3-#7 -------------------------------------------------------
//
// 这些测试是「保护性不变量测试」（W3C §8.3.1 阻断条件清单），R3 现状下已 PASS
// （因 chain 不跨函数边界自然阻断），实施后保持 PASS（blocks_top/bottom 显式
// 阻断）。它们的 D3 类反向探针在 P5（强制 blocks_top/bottom=false 时触发 FAIL）。

// #3 padding-top 阻断 first-child 与 parent margin-top collapse
TEST(MarginCollapseLayoutTest, A8_PaddingTop_BlocksFirstChildCollapse) {
  dom::Document doc;
  auto* body = doc.CreateElement(dom::TagId::kBody);
  doc.AppendChild(body);
  auto* parent = doc.CreateElement(dom::TagId::kDiv);
  parent->set_id(InternedString::Intern("parent"));
  body->AppendChild(parent);
  auto* child = doc.CreateElement(dom::TagId::kDiv);
  child->set_id(InternedString::Intern("child"));
  parent->AppendChild(child);

  Vector<css::Stylesheet> sheets;
  sheets.push_back(css::CssParser::Parse(
      "#parent { margin-top: 30px; padding-top: 5px; }"
      "#child { margin-top: 20px; height: 50px; }"));

  LayoutContext ctx;
  ctx.viewport_width = 800;
  ctx.viewport_height = 600;
  ctx.stylesheets = &sheets;

  auto* root = LayoutEngine::Layout(&doc, ctx);
  auto* body_box = root->first_child;
  auto* p_box = body_box->first_child;
  auto* c_box = p_box->first_child;

  // padding-top != 0 → blocks_top=true → child.margin-top 独立放置（不合入 parent）
  EXPECT_FLOAT_EQ(p_box->y, 30.0f);
  EXPECT_FLOAT_EQ(c_box->y, 20.0f);
  // 关键 D3 不变量：阻断条件下状态字段不被设置
  EXPECT_FALSE(c_box->margin_top_collapsed_into_ancestor);
}

// #4 padding-bottom 阻断 last-child 与 parent margin-bottom collapse
// （§5.2.2 阻断条件：padding-bottom != 0；与 border-bottom 等价。
//  border-bottom shorthand 当前 css parser 未支持，故用 padding-bottom 代表
//  「parent 底部物理 separator 阻断 last-child collapse」语义。）
TEST(MarginCollapseLayoutTest, A8_PaddingBottom_BlocksLastChildCollapse) {
  dom::Document doc;
  auto* body = doc.CreateElement(dom::TagId::kBody);
  doc.AppendChild(body);
  auto* parent = doc.CreateElement(dom::TagId::kDiv);
  parent->set_id(InternedString::Intern("parent"));
  body->AppendChild(parent);
  auto* child = doc.CreateElement(dom::TagId::kDiv);
  child->set_id(InternedString::Intern("child"));
  parent->AppendChild(child);
  auto* sibling = doc.CreateElement(dom::TagId::kDiv);
  sibling->set_id(InternedString::Intern("sibling"));
  body->AppendChild(sibling);

  Vector<css::Stylesheet> sheets;
  sheets.push_back(css::CssParser::Parse(
      "#parent { padding-bottom: 1px; }"
      "#child { margin-bottom: 20px; height: 50px; }"
      "#sibling { margin-top: 30px; height: 30px; }"));

  LayoutContext ctx;
  ctx.viewport_width = 800;
  ctx.viewport_height = 600;
  ctx.stylesheets = &sheets;

  auto* root = LayoutEngine::Layout(&doc, ctx);
  auto* body_box = root->first_child;
  auto* p_box = body_box->first_child;
  auto* c_box = p_box->first_child;
  auto* s_box = p_box->next_sibling;

  // padding-bottom != 0 → blocks_bottom=true → trailing 计入 parent.content_height
  EXPECT_FLOAT_EQ(p_box->content_height, 70.0f);  // 0 + 50 + 20
  EXPECT_FLOAT_EQ(s_box->y, 101.0f);              // 0 + (70+1) + max(0,30)
  EXPECT_FALSE(c_box->margin_bottom_collapsed_into_ancestor);
}

// #5 BFC root（overflow!=visible）阻断 first/last child 与 parent collapse
TEST(MarginCollapseLayoutTest, A8_BFCRoot_BlocksBothCollapse) {
  dom::Document doc;
  auto* body = doc.CreateElement(dom::TagId::kBody);
  doc.AppendChild(body);
  auto* parent = doc.CreateElement(dom::TagId::kDiv);
  parent->set_id(InternedString::Intern("parent"));
  body->AppendChild(parent);
  auto* child = doc.CreateElement(dom::TagId::kDiv);
  child->set_id(InternedString::Intern("child"));
  parent->AppendChild(child);

  Vector<css::Stylesheet> sheets;
  sheets.push_back(css::CssParser::Parse(
      "#parent { margin-top: 30px; overflow: hidden; }"
      "#child { margin-top: 20px; margin-bottom: 15px; height: 50px; }"));

  LayoutContext ctx;
  ctx.viewport_width = 800;
  ctx.viewport_height = 600;
  ctx.stylesheets = &sheets;

  auto* root = LayoutEngine::Layout(&doc, ctx);
  auto* body_box = root->first_child;
  auto* p_box = body_box->first_child;
  auto* c_box = p_box->first_child;

  // BFC root → 上下都阻断；child 完全独立
  EXPECT_FLOAT_EQ(c_box->y, 20.0f);
  EXPECT_FALSE(c_box->margin_top_collapsed_into_ancestor);
  EXPECT_FALSE(c_box->margin_bottom_collapsed_into_ancestor);
}

// #6 显式 height（!=auto）阻断 last-child margin-bottom 渗出
TEST(MarginCollapseLayoutTest, A8_ExplicitHeight_BlocksLastChildCollapse) {
  dom::Document doc;
  auto* body = doc.CreateElement(dom::TagId::kBody);
  doc.AppendChild(body);
  auto* parent = doc.CreateElement(dom::TagId::kDiv);
  parent->set_id(InternedString::Intern("parent"));
  body->AppendChild(parent);
  auto* child = doc.CreateElement(dom::TagId::kDiv);
  child->set_id(InternedString::Intern("child"));
  parent->AppendChild(child);
  auto* sibling = doc.CreateElement(dom::TagId::kDiv);
  sibling->set_id(InternedString::Intern("sibling"));
  body->AppendChild(sibling);

  Vector<css::Stylesheet> sheets;
  sheets.push_back(css::CssParser::Parse(
      "#parent { height: 80px; }"
      "#child { margin-bottom: 20px; height: 30px; }"
      "#sibling { margin-top: 30px; height: 30px; }"));

  LayoutContext ctx;
  ctx.viewport_width = 800;
  ctx.viewport_height = 600;
  ctx.stylesheets = &sheets;

  auto* root = LayoutEngine::Layout(&doc, ctx);
  auto* body_box = root->first_child;
  auto* p_box = body_box->first_child;
  auto* c_box = p_box->first_child;
  auto* s_box = p_box->next_sibling;

  EXPECT_FLOAT_EQ(p_box->content_height, 80.0f);  // height 显式
  EXPECT_FLOAT_EQ(s_box->y, 110.0f);              // 0 + 80 + max(0,30)
  EXPECT_FALSE(c_box->margin_bottom_collapsed_into_ancestor);
}

// #7 min-height>0 阻断 last-child margin-bottom 渗出
TEST(MarginCollapseLayoutTest, A8_MinHeightNonZero_BlocksLastChildCollapse) {
  dom::Document doc;
  auto* body = doc.CreateElement(dom::TagId::kBody);
  doc.AppendChild(body);
  auto* parent = doc.CreateElement(dom::TagId::kDiv);
  parent->set_id(InternedString::Intern("parent"));
  body->AppendChild(parent);
  auto* child = doc.CreateElement(dom::TagId::kDiv);
  child->set_id(InternedString::Intern("child"));
  parent->AppendChild(child);
  auto* sibling = doc.CreateElement(dom::TagId::kDiv);
  sibling->set_id(InternedString::Intern("sibling"));
  body->AppendChild(sibling);

  Vector<css::Stylesheet> sheets;
  sheets.push_back(css::CssParser::Parse(
      "#parent { min-height: 50px; }"
      "#child { margin-bottom: 20px; height: 30px; }"
      "#sibling { margin-top: 30px; height: 30px; }"));

  LayoutContext ctx;
  ctx.viewport_width = 800;
  ctx.viewport_height = 600;
  ctx.stylesheets = &sheets;

  auto* root = LayoutEngine::Layout(&doc, ctx);
  auto* body_box = root->first_child;
  auto* p_box = body_box->first_child;
  auto* c_box = p_box->first_child;
  auto* s_box = p_box->next_sibling;

  // min-height > 0 → blocks_bottom=true → trailing 计入 content_height = 50
  EXPECT_FLOAT_EQ(p_box->content_height, 50.0f);
  EXPECT_FLOAT_EQ(s_box->y, 80.0f);  // 0 + 50 + max(0,30)
  EXPECT_FALSE(c_box->margin_bottom_collapsed_into_ancestor);
}

// ----- 递归 + collapse-through #8-#10 ---------------------------------------

// #8 deep chain：≥ 3 层嵌套 first-child margin-top 链式合并到最外层
TEST(MarginCollapseLayoutTest, A8_DeepCollapseChain_NestedFirstChildren) {
  dom::Document doc;
  auto* body = doc.CreateElement(dom::TagId::kBody);
  doc.AppendChild(body);
  auto* gp = doc.CreateElement(dom::TagId::kDiv);
  gp->set_id(InternedString::Intern("gp"));
  body->AppendChild(gp);
  auto* parent = doc.CreateElement(dom::TagId::kDiv);
  parent->set_id(InternedString::Intern("parent"));
  gp->AppendChild(parent);
  auto* child = doc.CreateElement(dom::TagId::kDiv);
  child->set_id(InternedString::Intern("child"));
  parent->AppendChild(child);

  Vector<css::Stylesheet> sheets;
  sheets.push_back(css::CssParser::Parse(
      // 三层无 padding/border/BFC → 全部 first-child collapse with 上层
      "#gp { margin-top: 30px; }"
      "#parent { margin-top: 25px; }"
      "#child { margin-top: 20px; height: 50px; }"));

  LayoutContext ctx;
  ctx.viewport_width = 800;
  ctx.viewport_height = 600;
  ctx.stylesheets = &sheets;

  auto* root = LayoutEngine::Layout(&doc, ctx);
  auto* body_box = root->first_child;
  auto* gp_box = body_box->first_child;
  auto* p_box = gp_box->first_child;
  auto* c_box = p_box->first_child;

  // 全部三个 margin-top 进 body's chain → max(30, 25, 20) = 30
  EXPECT_FLOAT_EQ(gp_box->y, 30.0f);
  // 实施后：parent / child 都合入 gp 的 chain，相对父 y == 0
  // R3 现状：parent.y = 25, child.y = 20
  EXPECT_FLOAT_EQ(p_box->y, 0.0f);
  EXPECT_FLOAT_EQ(c_box->y, 0.0f);
  EXPECT_TRUE(p_box->margin_top_collapsed_into_ancestor);
  EXPECT_TRUE(c_box->margin_top_collapsed_into_ancestor);
}

// #9 collapse-through 跨 parent 边界传播：empty box 把自身 mt+mb 渗到 grandparent
TEST(MarginCollapseLayoutTest, A8_CollapseThrough_BoxPropagatesAcrossLevels) {
  dom::Document doc;
  auto* body = doc.CreateElement(dom::TagId::kBody);
  doc.AppendChild(body);
  auto* parent = doc.CreateElement(dom::TagId::kDiv);
  parent->set_id(InternedString::Intern("parent"));
  body->AppendChild(parent);
  auto* empty = doc.CreateElement(dom::TagId::kDiv);
  empty->set_id(InternedString::Intern("empty"));
  parent->AppendChild(empty);
  auto* after = doc.CreateElement(dom::TagId::kDiv);
  after->set_id(InternedString::Intern("after"));
  body->AppendChild(after);

  Vector<css::Stylesheet> sheets;
  sheets.push_back(css::CssParser::Parse(
      // empty 是 collapse-through（无 content/padding/border/auto height）
      "#empty { margin-top: 10px; margin-bottom: 15px; }"
      "#after { margin-top: 20px; height: 30px; }"));

  LayoutContext ctx;
  ctx.viewport_width = 800;
  ctx.viewport_height = 600;
  ctx.stylesheets = &sheets;

  auto* root = LayoutEngine::Layout(&doc, ctx);
  auto* body_box = root->first_child;
  auto* p_box = body_box->first_child;
  auto* e_box = p_box->first_child;
  auto* a_box = p_box->next_sibling;

  // empty 是 collapse-through，且 parent 也变成 collapse-through（因 trailing 渗出）
  EXPECT_TRUE(e_box->collapsed_through);
  // 实施后 parent.collapsed_through=true (chain 全部渗出，content_height=0)
  EXPECT_TRUE(p_box->collapsed_through);
  EXPECT_FLOAT_EQ(p_box->content_height, 0.0f);
  // chain 在 body 内合并 max(10, 15, 20) = 20
  // 实施后：after.y = parent.y(0) + parent.bbh(0) + 20 = 20
  // R3：parent.bbh=15 (trailing 计入)，after.y = 0 + 15 + 20 = 35
  EXPECT_FLOAT_EQ(a_box->y, 20.0f);
}

// #10 outgoing chain 含 parent.margin-bottom 与 next-sibling.margin-top collapse
TEST(MarginCollapseLayoutTest, A8_LastChild_OutgoingChainContainsParentMarginBottom) {
  dom::Document doc;
  auto* body = doc.CreateElement(dom::TagId::kBody);
  doc.AppendChild(body);
  auto* parent = doc.CreateElement(dom::TagId::kDiv);
  parent->set_id(InternedString::Intern("parent"));
  body->AppendChild(parent);
  auto* child = doc.CreateElement(dom::TagId::kDiv);
  child->set_id(InternedString::Intern("child"));
  parent->AppendChild(child);
  auto* after = doc.CreateElement(dom::TagId::kDiv);
  after->set_id(InternedString::Intern("after"));
  body->AppendChild(after);

  Vector<css::Stylesheet> sheets;
  sheets.push_back(css::CssParser::Parse(
      "#parent { margin-bottom: 15px; }"
      "#child { margin-bottom: 20px; height: 50px; }"
      "#after { margin-top: 5px; height: 30px; }"));

  LayoutContext ctx;
  ctx.viewport_width = 800;
  ctx.viewport_height = 600;
  ctx.stylesheets = &sheets;

  auto* root = LayoutEngine::Layout(&doc, ctx);
  auto* body_box = root->first_child;
  auto* p_box = body_box->first_child;
  auto* c_box = p_box->first_child;
  auto* a_box = p_box->next_sibling;

  // 实施后：outgoing chain 含 child.mb(20) + parent.mb(15) → max=20
  //         body's chain 与 after.mt(5) → max(20, 5) = 20
  //         after.y = parent.y(0) + parent.bbh(50) + 20 = 70
  //         parent.content_height = 50 (trailing 渗出)
  // R3：parent.content_height = 70, after.y = 0 + 70 + max(15,5) = 85
  EXPECT_FLOAT_EQ(p_box->content_height, 50.0f);
  EXPECT_FLOAT_EQ(a_box->y, 70.0f);
  EXPECT_TRUE(c_box->margin_bottom_collapsed_into_ancestor);
}

}  // namespace vx::layout
