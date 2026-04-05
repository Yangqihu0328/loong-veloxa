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

}  // namespace
}  // namespace vx::layout
