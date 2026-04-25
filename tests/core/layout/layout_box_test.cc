#include "veloxa/core/layout/layout_box.h"

#include <gtest/gtest.h>

namespace vx::layout {
namespace {

TEST(LayoutBoxTest, DefaultValues) {
  LayoutBox box;
  EXPECT_EQ(box.type, LayoutType::kBlock);
  EXPECT_EQ(box.element, nullptr);
  EXPECT_EQ(box.text_node, nullptr);
  EXPECT_EQ(box.style, nullptr);
  EXPECT_EQ(box.parent, nullptr);
  EXPECT_EQ(box.first_child, nullptr);
  EXPECT_EQ(box.last_child, nullptr);
  EXPECT_EQ(box.next_sibling, nullptr);
  EXPECT_EQ(box.prev_sibling, nullptr);
  EXPECT_FLOAT_EQ(box.x, 0.0f);
  EXPECT_FLOAT_EQ(box.y, 0.0f);
  EXPECT_FLOAT_EQ(box.content_width, 0.0f);
  EXPECT_FLOAT_EQ(box.content_height, 0.0f);
  for (int i = 0; i < 4; ++i) {
    EXPECT_FLOAT_EQ(box.padding[i], 0.0f);
    EXPECT_FLOAT_EQ(box.border[i], 0.0f);
    EXPECT_FLOAT_EQ(box.margin[i], 0.0f);
  }
}

TEST(LayoutBoxTest, AppendSingleChild) {
  LayoutBox parent;
  LayoutBox child;
  parent.AppendChild(&child);

  EXPECT_EQ(parent.first_child, &child);
  EXPECT_EQ(parent.last_child, &child);
  EXPECT_EQ(child.parent, &parent);
  EXPECT_EQ(child.prev_sibling, nullptr);
  EXPECT_EQ(child.next_sibling, nullptr);
  EXPECT_EQ(parent.child_count(), 1u);
}

TEST(LayoutBoxTest, AppendMultipleChildren) {
  LayoutBox parent;
  LayoutBox c1, c2, c3;
  parent.AppendChild(&c1);
  parent.AppendChild(&c2);
  parent.AppendChild(&c3);

  EXPECT_EQ(parent.first_child, &c1);
  EXPECT_EQ(parent.last_child, &c3);
  EXPECT_EQ(c1.next_sibling, &c2);
  EXPECT_EQ(c2.next_sibling, &c3);
  EXPECT_EQ(c3.next_sibling, nullptr);
  EXPECT_EQ(c1.prev_sibling, nullptr);
  EXPECT_EQ(c2.prev_sibling, &c1);
  EXPECT_EQ(c3.prev_sibling, &c2);
  EXPECT_EQ(parent.child_count(), 3u);
}

TEST(LayoutBoxTest, ChildCountEmpty) {
  LayoutBox box;
  EXPECT_EQ(box.child_count(), 0u);
}

TEST(LayoutBoxTest, PaddingBoxDimensions) {
  LayoutBox box;
  box.content_width = 100.0f;
  box.content_height = 50.0f;
  box.padding[LayoutBox::kTop] = 5.0f;
  box.padding[LayoutBox::kRight] = 10.0f;
  box.padding[LayoutBox::kBottom] = 5.0f;
  box.padding[LayoutBox::kLeft] = 10.0f;

  EXPECT_FLOAT_EQ(box.padding_box_width(), 120.0f);
  EXPECT_FLOAT_EQ(box.padding_box_height(), 60.0f);
}

TEST(LayoutBoxTest, BorderBoxDimensions) {
  LayoutBox box;
  box.content_width = 100.0f;
  box.content_height = 50.0f;
  box.padding[LayoutBox::kRight] = 10.0f;
  box.padding[LayoutBox::kLeft] = 10.0f;
  box.border[LayoutBox::kRight] = 2.0f;
  box.border[LayoutBox::kLeft] = 2.0f;
  box.border[LayoutBox::kTop] = 1.0f;
  box.border[LayoutBox::kBottom] = 1.0f;

  EXPECT_FLOAT_EQ(box.border_box_width(), 124.0f);
  EXPECT_FLOAT_EQ(box.border_box_height(), 52.0f);
}

TEST(LayoutBoxTest, MarginBoxDimensions) {
  LayoutBox box;
  box.content_width = 100.0f;
  box.content_height = 50.0f;
  box.padding[LayoutBox::kRight] = 10.0f;
  box.padding[LayoutBox::kLeft] = 10.0f;
  box.border[LayoutBox::kRight] = 2.0f;
  box.border[LayoutBox::kLeft] = 2.0f;
  box.margin[LayoutBox::kRight] = 15.0f;
  box.margin[LayoutBox::kLeft] = 15.0f;
  box.margin[LayoutBox::kTop] = 20.0f;
  box.margin[LayoutBox::kBottom] = 20.0f;

  EXPECT_FLOAT_EQ(box.margin_box_width(), 154.0f);
  EXPECT_FLOAT_EQ(box.margin_box_height(), 90.0f);
}

TEST(LayoutBoxTest, LayoutTypeValues) {
  LayoutBox block;
  block.type = LayoutType::kBlock;
  EXPECT_EQ(block.type, LayoutType::kBlock);

  LayoutBox inline_box;
  inline_box.type = LayoutType::kInline;
  EXPECT_EQ(inline_box.type, LayoutType::kInline);

  LayoutBox flex;
  flex.type = LayoutType::kFlex;
  EXPECT_EQ(flex.type, LayoutType::kFlex);

  LayoutBox text;
  text.type = LayoutType::kText;
  EXPECT_EQ(text.type, LayoutType::kText);
}

TEST(LayoutBoxTest, ZeroDimensionsBoxSizes) {
  LayoutBox box;
  EXPECT_FLOAT_EQ(box.padding_box_width(), 0.0f);
  EXPECT_FLOAT_EQ(box.padding_box_height(), 0.0f);
  EXPECT_FLOAT_EQ(box.border_box_width(), 0.0f);
  EXPECT_FLOAT_EQ(box.border_box_height(), 0.0f);
  EXPECT_FLOAT_EQ(box.margin_box_width(), 0.0f);
  EXPECT_FLOAT_EQ(box.margin_box_height(), 0.0f);
}

// =============================================================================
// Origin helpers (#25, TASK-20260426-01 R1, spec §5.1)
//
// 语义：LayoutBox 的 (x, y) 是 border-box 的左上角坐标（相对 parent 的工作坐标
// 系，由调用方决定绝对/相对）。此组 helper 提供 19 个一致语义的访问器，取代
// layout_engine.cc / flex_layout.cc 中 14+ 处分散的坐标拼接表达式。
//   - 3 origin（border / padding / content_box_origin）返回 Point{x, y}
//   - 16 four-side helper：(border|padding|content|margin)_box_(top|right|bottom|left)
//
// 反向探针（RED 验证 helper 真实有效，避免假阴性）：
//   - OriginHelpersZeroBoxIsTrivial — 全 0 box 所有 origin 必须 (0,0)
//   - OriginHelpersAreInversesByMutation — 改 border[kLeft] 后 padding_box_origin
//     必须右移相应距离（防 helper 内部把 kLeft 拼成 kRight 等枚举笔误）
// =============================================================================

TEST(LayoutBoxTest, BorderBoxOriginIsXY) {
  LayoutBox box;
  box.x = 12.5f;
  box.y = 34.0f;

  Point origin = box.border_box_origin();
  EXPECT_FLOAT_EQ(origin.x, 12.5f);
  EXPECT_FLOAT_EQ(origin.y, 34.0f);
}

TEST(LayoutBoxTest, PaddingBoxOriginShiftsByBorder) {
  LayoutBox box;
  box.x = 100.0f;
  box.y = 200.0f;
  box.border[LayoutBox::kTop] = 3.0f;
  box.border[LayoutBox::kLeft] = 5.0f;

  Point origin = box.padding_box_origin();
  EXPECT_FLOAT_EQ(origin.x, 105.0f);
  EXPECT_FLOAT_EQ(origin.y, 203.0f);
}

TEST(LayoutBoxTest, ContentBoxOriginShiftsByBorderPlusPadding) {
  LayoutBox box;
  box.x = 100.0f;
  box.y = 200.0f;
  box.border[LayoutBox::kTop] = 3.0f;
  box.border[LayoutBox::kLeft] = 5.0f;
  box.padding[LayoutBox::kTop] = 7.0f;
  box.padding[LayoutBox::kLeft] = 11.0f;

  Point origin = box.content_box_origin();
  EXPECT_FLOAT_EQ(origin.x, 116.0f);
  EXPECT_FLOAT_EQ(origin.y, 210.0f);
}

TEST(LayoutBoxTest, BorderBoxFourSidesEnclosesBorderBoxRect) {
  LayoutBox box;
  box.x = 10.0f;
  box.y = 20.0f;
  box.content_width = 100.0f;
  box.content_height = 50.0f;
  box.padding[LayoutBox::kLeft] = 5.0f;
  box.padding[LayoutBox::kRight] = 5.0f;
  box.padding[LayoutBox::kTop] = 3.0f;
  box.padding[LayoutBox::kBottom] = 3.0f;
  box.border[LayoutBox::kLeft] = 2.0f;
  box.border[LayoutBox::kRight] = 2.0f;
  box.border[LayoutBox::kTop] = 1.0f;
  box.border[LayoutBox::kBottom] = 1.0f;

  EXPECT_FLOAT_EQ(box.border_box_top(), 20.0f);
  EXPECT_FLOAT_EQ(box.border_box_left(), 10.0f);
  EXPECT_FLOAT_EQ(box.border_box_right(), 10.0f + 114.0f);
  EXPECT_FLOAT_EQ(box.border_box_bottom(), 20.0f + 58.0f);
}

TEST(LayoutBoxTest, PaddingBoxFourSidesNestedInBorder) {
  LayoutBox box;
  box.x = 10.0f;
  box.y = 20.0f;
  box.content_width = 100.0f;
  box.content_height = 50.0f;
  box.padding[LayoutBox::kLeft] = 5.0f;
  box.padding[LayoutBox::kRight] = 5.0f;
  box.padding[LayoutBox::kTop] = 3.0f;
  box.padding[LayoutBox::kBottom] = 3.0f;
  box.border[LayoutBox::kLeft] = 2.0f;
  box.border[LayoutBox::kRight] = 2.0f;
  box.border[LayoutBox::kTop] = 1.0f;
  box.border[LayoutBox::kBottom] = 1.0f;

  EXPECT_FLOAT_EQ(box.padding_box_top(), 21.0f);
  EXPECT_FLOAT_EQ(box.padding_box_left(), 12.0f);
  EXPECT_FLOAT_EQ(box.padding_box_right(), 12.0f + 110.0f);
  EXPECT_FLOAT_EQ(box.padding_box_bottom(), 21.0f + 56.0f);
}

TEST(LayoutBoxTest, ContentBoxFourSidesEqualsContentRect) {
  LayoutBox box;
  box.x = 10.0f;
  box.y = 20.0f;
  box.content_width = 100.0f;
  box.content_height = 50.0f;
  box.padding[LayoutBox::kLeft] = 5.0f;
  box.padding[LayoutBox::kTop] = 3.0f;
  box.border[LayoutBox::kLeft] = 2.0f;
  box.border[LayoutBox::kTop] = 1.0f;

  EXPECT_FLOAT_EQ(box.content_box_top(), 24.0f);
  EXPECT_FLOAT_EQ(box.content_box_left(), 17.0f);
  EXPECT_FLOAT_EQ(box.content_box_right(), 17.0f + 100.0f);
  EXPECT_FLOAT_EQ(box.content_box_bottom(), 24.0f + 50.0f);
}

TEST(LayoutBoxTest, MarginBoxFourSidesExtendsByMargin) {
  LayoutBox box;
  box.x = 100.0f;
  box.y = 200.0f;
  box.content_width = 50.0f;
  box.content_height = 30.0f;
  box.margin[LayoutBox::kTop] = 8.0f;
  box.margin[LayoutBox::kRight] = 12.0f;
  box.margin[LayoutBox::kBottom] = 8.0f;
  box.margin[LayoutBox::kLeft] = 12.0f;

  EXPECT_FLOAT_EQ(box.margin_box_top(), 192.0f);
  EXPECT_FLOAT_EQ(box.margin_box_left(), 88.0f);
  EXPECT_FLOAT_EQ(box.margin_box_right(), 100.0f + 50.0f + 12.0f);
  EXPECT_FLOAT_EQ(box.margin_box_bottom(), 200.0f + 30.0f + 8.0f);
}

// 反向探针 1：全 0 box 时所有 origin 必须 (0, 0)。
// 防御失败：helper 实现误把任何字段 +1 / 加错枚举常量都会被 catch。
TEST(LayoutBoxTest, OriginHelpersZeroBoxIsTrivial) {
  LayoutBox box;
  EXPECT_FLOAT_EQ(box.border_box_origin().x, 0.0f);
  EXPECT_FLOAT_EQ(box.border_box_origin().y, 0.0f);
  EXPECT_FLOAT_EQ(box.padding_box_origin().x, 0.0f);
  EXPECT_FLOAT_EQ(box.padding_box_origin().y, 0.0f);
  EXPECT_FLOAT_EQ(box.content_box_origin().x, 0.0f);
  EXPECT_FLOAT_EQ(box.content_box_origin().y, 0.0f);
  EXPECT_FLOAT_EQ(box.border_box_top(), 0.0f);
  EXPECT_FLOAT_EQ(box.border_box_left(), 0.0f);
  EXPECT_FLOAT_EQ(box.border_box_right(), 0.0f);
  EXPECT_FLOAT_EQ(box.border_box_bottom(), 0.0f);
  EXPECT_FLOAT_EQ(box.margin_box_top(), 0.0f);
  EXPECT_FLOAT_EQ(box.margin_box_left(), 0.0f);
  EXPECT_FLOAT_EQ(box.margin_box_right(), 0.0f);
  EXPECT_FLOAT_EQ(box.margin_box_bottom(), 0.0f);
}

// 反向探针 2：单字段突变检验（防 kLeft/kRight/kTop/kBottom 枚举笔误）。
// 仅改 border[kLeft] 时，padding_box_origin().x 必须右移 = border[kLeft]，
// 而 padding_box_origin().y 不动；改 border[kTop] 时反之。任何枚举/索引互换的
// helper 笔误都被 catch（如把 kLeft 写成 kRight 会让 x 不变 / y 异常）。
TEST(LayoutBoxTest, OriginHelpersAreInversesByMutation) {
  LayoutBox box;
  box.x = 50.0f;
  box.y = 80.0f;

  Point base_pad = box.padding_box_origin();
  EXPECT_FLOAT_EQ(base_pad.x, 50.0f);
  EXPECT_FLOAT_EQ(base_pad.y, 80.0f);

  box.border[LayoutBox::kLeft] = 4.0f;
  Point left_only = box.padding_box_origin();
  EXPECT_FLOAT_EQ(left_only.x, 54.0f);
  EXPECT_FLOAT_EQ(left_only.y, 80.0f);

  box.border[LayoutBox::kLeft] = 0.0f;
  box.border[LayoutBox::kTop] = 7.0f;
  Point top_only = box.padding_box_origin();
  EXPECT_FLOAT_EQ(top_only.x, 50.0f);
  EXPECT_FLOAT_EQ(top_only.y, 87.0f);

  box.border[LayoutBox::kRight] = 100.0f;
  Point right_unchanged = box.padding_box_origin();
  EXPECT_FLOAT_EQ(right_unchanged.x, 50.0f);
  EXPECT_FLOAT_EQ(right_unchanged.y, 87.0f);

  box.border[LayoutBox::kBottom] = 100.0f;
  Point bottom_unchanged = box.padding_box_origin();
  EXPECT_FLOAT_EQ(bottom_unchanged.x, 50.0f);
  EXPECT_FLOAT_EQ(bottom_unchanged.y, 87.0f);
}

}  // namespace
}  // namespace vx::layout
