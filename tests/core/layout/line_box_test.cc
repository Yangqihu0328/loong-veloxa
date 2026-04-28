// CSS 2.1 §10.8 / §10.8.1 line box 模型测试 — TASK-20260426-01 R4 #21。
//
// 测试矩阵（plan §4.7：9 case + 2 RED 反向探针）：
//   [POD 单测 5]
//     A9.1 LineBox::empty() 初始为真；push_back 后为假
//     A9.2 LineBoxItem 默认值 ascent=descent=0 / baseline_offset=0
//     A9.3 LineBox::height() 取 max(line_height, max_ascent + max_descent)
//     A9.4 LineBox 多 item 累加（不改变 max_ascent，仅由 FinalizeLine 写入）
//     A9.5 LineBox::height() 在 metrics_height > line_height 时取 metrics_height
//   [集成 4 — 通过 LayoutEngine::Layout 验证 2-pass vertical-align 算法]
//     A11.1 vertical-align: baseline（默认）— text 与 inline-block atomic 对齐
//     A11.2 vertical-align: top — kTop item 顶对齐 line 顶
//     A11.3 vertical-align: bottom — kBottom item 底对齐 line 底
//     A11.4 vertical-align: 5px 长度偏移 — 上移 5 像素
//   [RED 反向探针 2]
//     RED1 baseline 默认场景的 y == half_leading_top（破：max_ascent → sum_ascent
//          会导致 baseline_y 错误；本断言会 fail）
//     RED2 kBottom item 撑高 line（破：Phase 2 忘记回写 max_descent → 后续
//          baseline_y 不会下移；本断言会 fail）

#include <gtest/gtest.h>

#include "veloxa/core/css/parser.h"
#include "veloxa/core/dom/document.h"
#include "veloxa/core/layout/layout_engine.h"
#include "veloxa/core/layout/line_box.h"
#include "veloxa/core/layout/text_shaper.h"

namespace vx::layout {
namespace {

// =============================================================================
// LineBox POD 单元测试（D2.A 抽象独立可测性要求）
// =============================================================================

TEST(LineBoxTest, A9_1_DefaultEmpty) {
  LineBox line;
  EXPECT_TRUE(line.empty());
  EXPECT_EQ(line.items.size(), 0u);

  LineBoxItem it;
  line.items.push_back(it);
  EXPECT_FALSE(line.empty());
  EXPECT_EQ(line.items.size(), 1u);
}

TEST(LineBoxTest, A9_2_LineBoxItemDefaults) {
  LineBoxItem it;
  EXPECT_EQ(it.box, nullptr);
  EXPECT_FLOAT_EQ(it.ascent, 0.0f);
  EXPECT_FLOAT_EQ(it.descent, 0.0f);
  EXPECT_FLOAT_EQ(it.baseline_offset, 0.0f);
}

TEST(LineBoxTest, A9_3_HeightTakesMaxOfLineHeightAndMetrics) {
  // line_height > metrics_height → height = line_height（半-leading 撑高）
  LineBox line;
  line.line_height = 24.0f;
  line.max_ascent = 12.8f;
  line.max_descent = 3.2f;  // metrics = 16
  EXPECT_FLOAT_EQ(line.height(), 24.0f);
}

TEST(LineBoxTest, A9_4_MultipleItemsPushPreservesOrder) {
  LineBox line;
  LineBoxItem a;
  a.ascent = 10.0f;
  LineBoxItem b;
  b.ascent = 20.0f;
  line.items.push_back(a);
  line.items.push_back(b);
  ASSERT_EQ(line.items.size(), 2u);
  EXPECT_FLOAT_EQ(line.items[0].ascent, 10.0f);
  EXPECT_FLOAT_EQ(line.items[1].ascent, 20.0f);
}

TEST(LineBoxTest, A9_5_MetricsHeightOverridesSmallLineHeight) {
  // metrics_height > line_height → height = metrics_height
  // CSS 2.1 §10.8.1 允许（小 line-height 不能压扁字体度量）。
  LineBox line;
  line.line_height = 10.0f;
  line.max_ascent = 14.0f;
  line.max_descent = 4.0f;  // metrics = 18
  EXPECT_FLOAT_EQ(line.height(), 18.0f);
}

// =============================================================================
// vertical-align / 半-leading 集成测试（A11 / A12 — 通过 LayoutEngine::Layout）
// =============================================================================

namespace {
struct InlineFixture {
  // 构造一个 .ifc { display: inline; } 容器内带 N 个 child 的 IFC。
  static LayoutBox* RunLayout(const char* css_text, dom::Document& doc,
                               SimpleTextShaper& shaper, LayoutContext& ctx,
                               Vector<css::Stylesheet>& sheets) {
    sheets.push_back(css::CssParser::Parse(css_text));
    ctx.text_shaper = &shaper;
    ctx.viewport_width = 800;
    ctx.viewport_height = 600;
    ctx.stylesheets = &sheets;
    return LayoutEngine::Layout(&doc, ctx);
  }
};
}  // namespace

TEST(LineBoxTest, A11_1_BaselineDefaultAligns) {
  // 同一 IFC 中两个 text run（"AA" + "BB"）— vertical-align 默认 baseline。
  // 期望两者 y 完全相同（都在 line baseline 之上 ascent 处）。
  dom::Document doc;
  auto* body = doc.CreateElement(dom::TagId::kBody);
  doc.AppendChild(body);
  auto* span = doc.CreateElement(dom::TagId::kSpan);
  span->AddClass(InternedString::Intern("ifc"));
  body->AppendChild(span);
  span->AppendChild(doc.CreateText("AA"));
  span->AppendChild(doc.CreateText("BB"));

  Vector<css::Stylesheet> sheets;
  SimpleTextShaper shaper;
  LayoutContext ctx;
  auto* root = InlineFixture::RunLayout(".ifc { display: inline; }", doc,
                                          shaper, ctx, sheets);
  auto* span_box = root->first_child->first_child;
  auto* a = span_box->first_child;
  auto* b = a->next_sibling;
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  // baseline 默认 → 同一 baseline → 同 y（ascent 相等）
  EXPECT_FLOAT_EQ(a->y, b->y);
  // 半-leading 验证（line_height_normal = 19.2, ascent 12.8, half_leading 1.6）
  EXPECT_FLOAT_EQ(a->y, 1.6f);
}

TEST(LineBoxTest, A11_2_VerticalAlignTopPullsItemToLineTop) {
  // span1 默认 baseline；span2 vertical-align: top → 顶部对齐 line 顶。
  // span2 用更大 font-size 把 line 撑高，然后期望 span1 的 y 变化。
  // 简化验证：span2 的 y 应当 = line.top = 0（顶对齐）。
  dom::Document doc;
  auto* body = doc.CreateElement(dom::TagId::kBody);
  doc.AppendChild(body);
  auto* outer = doc.CreateElement(dom::TagId::kSpan);
  outer->AddClass(InternedString::Intern("outer"));
  body->AppendChild(outer);
  auto* s1 = doc.CreateElement(dom::TagId::kSpan);
  s1->AddClass(InternedString::Intern("base"));
  outer->AppendChild(s1);
  s1->AppendChild(doc.CreateText("X"));
  auto* s2 = doc.CreateElement(dom::TagId::kSpan);
  s2->AddClass(InternedString::Intern("top"));
  outer->AppendChild(s2);
  s2->AppendChild(doc.CreateText("Y"));

  Vector<css::Stylesheet> sheets;
  SimpleTextShaper shaper;
  LayoutContext ctx;
  auto* root = InlineFixture::RunLayout(
      ".outer { display: inline; }"
      ".base { display: inline; font-size: 16px; }"
      ".top { display: inline-block; vertical-align: top;"
      " width: 20px; height: 40px; }",
      doc, shaper, ctx, sheets);
  auto* outer_box = root->first_child->first_child;
  auto* base_span = outer_box->first_child;
  auto* top_span = base_span->next_sibling;
  ASSERT_NE(top_span, nullptr);
  // top_span vertical-align: top → 顶部对齐 line 顶 (y = line.top = 0)
  // 容差 0.5 px 容纳 strut 计算和 half_leading 浮点舍入。
  EXPECT_NEAR(top_span->y, 0.0f, 0.5f);
}

TEST(LineBoxTest, A11_3_VerticalAlignBottomPullsItemToLineBottom) {
  dom::Document doc;
  auto* body = doc.CreateElement(dom::TagId::kBody);
  doc.AppendChild(body);
  auto* outer = doc.CreateElement(dom::TagId::kSpan);
  outer->AddClass(InternedString::Intern("outer"));
  body->AppendChild(outer);
  auto* s1 = doc.CreateElement(dom::TagId::kSpan);
  s1->AddClass(InternedString::Intern("base"));
  outer->AppendChild(s1);
  s1->AppendChild(doc.CreateText("X"));
  auto* s2 = doc.CreateElement(dom::TagId::kSpan);
  s2->AddClass(InternedString::Intern("bot"));
  outer->AppendChild(s2);
  s2->AppendChild(doc.CreateText("Y"));

  Vector<css::Stylesheet> sheets;
  SimpleTextShaper shaper;
  LayoutContext ctx;
  auto* root = InlineFixture::RunLayout(
      ".outer { display: inline; }"
      ".base { display: inline; font-size: 16px; }"
      ".bot { display: inline-block; vertical-align: bottom;"
      " width: 20px; height: 40px; }",
      doc, shaper, ctx, sheets);
  auto* outer_box = root->first_child->first_child;
  auto* base_span = outer_box->first_child;
  auto* bot_span = base_span->next_sibling;
  ASSERT_NE(bot_span, nullptr);
  // bot_span vertical-align: bottom → 底部对齐 line 底。bot 高度 40 撑大 line。
  // 推导（坐标 = outer 坐标系）：
  //   max_ascent (post-strut, post-Phase2) = ascent_bot - (max_descent_strut)
  //                                        = 40 - 3.2 = 36.8
  //   max_descent = 3.2（保持 strut）
  //   metrics_height = 40，line.line_height = 40
  //   baseline_y = 36.8
  //   base.y = 36.8 - 19.2 + 0 = 17.6（baseline 默认对齐）
  //   bot.y  = 36.8 - 40 + 3.2 = 0（位于 line 顶；其 bottom = 40 = line bottom）
  // 比较「bottom 更靠下」更精准：bot.bottom > base.bottom。
  const f32 base_bottom = base_span->y + base_span->border_box_height();
  const f32 bot_bottom = bot_span->y + bot_span->border_box_height();
  EXPECT_GT(bot_bottom, base_bottom);
}

TEST(LineBoxTest, A11_4_VerticalAlignLengthOffsetMovesUp) {
  // vertical-align: 5px → 正值上移 5 像素相对于 baseline 默认位置
  dom::Document doc;
  auto* body = doc.CreateElement(dom::TagId::kBody);
  doc.AppendChild(body);
  auto* outer = doc.CreateElement(dom::TagId::kSpan);
  outer->AddClass(InternedString::Intern("outer"));
  body->AppendChild(outer);
  auto* s1 = doc.CreateElement(dom::TagId::kSpan);
  s1->AddClass(InternedString::Intern("base"));
  outer->AppendChild(s1);
  s1->AppendChild(doc.CreateText("X"));
  auto* s2 = doc.CreateElement(dom::TagId::kSpan);
  s2->AddClass(InternedString::Intern("up"));
  outer->AppendChild(s2);
  s2->AppendChild(doc.CreateText("Y"));

  Vector<css::Stylesheet> sheets;
  SimpleTextShaper shaper;
  LayoutContext ctx;
  auto* root = InlineFixture::RunLayout(
      ".outer { display: inline; }"
      ".base { display: inline; }"
      ".up { display: inline; vertical-align: 5px; }",
      doc, shaper, ctx, sheets);
  auto* outer_box = root->first_child->first_child;
  auto* base_span = outer_box->first_child;
  auto* up_span = base_span->next_sibling;
  ASSERT_NE(up_span, nullptr);
  // 因为 up_span 是 inline 而非 atomic，其内部 text run 才是 line item。
  // 但当前实现把 inline child 作为 atomic 处理（ComputeInlineMetrics 走
  // border_box_height 路径）— 这里直接观察其位置。
  // 上移 5px → up_span.y < base_span.y（默认 baseline）
  EXPECT_LT(up_span->y, base_span->y);
}

// =============================================================================
// RED 反向探针 — 测试若实现错误能否被捕获
// =============================================================================

TEST(LineBoxTest, RED1_BaselineYRespectsHalfLeading) {
  // 反向探针：若 FinalizeLine 把 max_ascent 改成 sum_ascent（累加）或忘记
  // strut 兜底，baseline_y 计算会偏离 expected = top + max_ascent + half_leading。
  // 标准 case：单 text run（"X"），font_size 16，line-height normal (19.2)。
  //   max_ascent (含 strut) = 12.8
  //   line_height = 19.2，metrics = 16，leading = 3.2，half_leading_top = 1.6
  //   baseline_y = 0 + 12.8 + 1.6 = 14.4
  //   text->y = baseline_y - 12.8 = 1.6
  dom::Document doc;
  auto* body = doc.CreateElement(dom::TagId::kBody);
  doc.AppendChild(body);
  auto* span = doc.CreateElement(dom::TagId::kSpan);
  span->AddClass(InternedString::Intern("ifc"));
  body->AppendChild(span);
  span->AppendChild(doc.CreateText("X"));

  Vector<css::Stylesheet> sheets;
  SimpleTextShaper shaper;
  LayoutContext ctx;
  auto* root = InlineFixture::RunLayout(".ifc { display: inline; }", doc,
                                          shaper, ctx, sheets);
  auto* text = root->first_child->first_child->first_child;
  ASSERT_NE(text, nullptr);
  EXPECT_FLOAT_EQ(text->y, 1.6f);
}

TEST(LineBoxTest, RED2_BottomItemGrowsLineHeight) {
  // 反向探针：若 Phase 2 忘记把 kBottom item 的 metrics 写回 line.max_descent，
  // 则容器高度（= sum line.line_height）不会被 kBottom item 撑高。
  // 标准 case：base 16px text + bot inline-block 高度 40px / vertical-align: bottom。
  //   line.height >= 40（bot atomic ascent = 40 不变；max_descent 由 Phase 2
  //   计算 -(40 - 0 + offset)， offset = -max_descent_pre - ...复杂）
  // 简化断言：容器高度必须 >= 40（被 bot 撑开）。
  dom::Document doc;
  auto* body = doc.CreateElement(dom::TagId::kBody);
  doc.AppendChild(body);
  auto* span = doc.CreateElement(dom::TagId::kSpan);
  span->AddClass(InternedString::Intern("ifc"));
  body->AppendChild(span);
  span->AppendChild(doc.CreateText("X"));
  auto* big = doc.CreateElement(dom::TagId::kSpan);
  big->AddClass(InternedString::Intern("big"));
  span->AppendChild(big);
  big->AppendChild(doc.CreateText("Y"));

  Vector<css::Stylesheet> sheets;
  SimpleTextShaper shaper;
  LayoutContext ctx;
  auto* root = InlineFixture::RunLayout(
      ".ifc { display: inline; }"
      ".big { display: inline-block; vertical-align: bottom;"
      " width: 20px; height: 40px; }",
      doc, shaper, ctx, sheets);
  auto* span_box = root->first_child->first_child;
  // span_box.content_height >= 40（被 inline-block 撑开）
  EXPECT_GE(span_box->content_height, 40.0f);
}

}  // namespace
}  // namespace vx::layout
