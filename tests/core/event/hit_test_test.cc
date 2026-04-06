#include "veloxa/core/event/hit_test.h"

#include "veloxa/core/css/computed_style.h"
#include "veloxa/core/dom/document.h"
#include "veloxa/core/layout/layout_box.h"

#include <gtest/gtest.h>

namespace {

using namespace vx;

TEST(HitTestTest, NullRoot) {
  EXPECT_EQ(event::HitTest(nullptr, 0, 0), nullptr);
}

TEST(HitTestTest, SingleBoxHit) {
  dom::Document doc;
  css::ComputedStyle style;
  layout::LayoutBox box;
  box.element = doc.CreateElement(dom::TagId::kDiv);
  box.style = &style;
  box.x = 0;
  box.y = 0;
  box.content_width = 100;
  box.content_height = 100;

  EXPECT_EQ(event::HitTest(&box, 50, 50), box.element);
}

TEST(HitTestTest, SingleBoxMiss) {
  dom::Document doc;
  css::ComputedStyle style;
  layout::LayoutBox box;
  box.element = doc.CreateElement(dom::TagId::kDiv);
  box.style = &style;
  box.x = 0;
  box.y = 0;
  box.content_width = 100;
  box.content_height = 100;

  EXPECT_EQ(event::HitTest(&box, 200, 200), nullptr);
}

TEST(HitTestTest, SingleBoxBorderEdge) {
  dom::Document doc;
  css::ComputedStyle style;
  layout::LayoutBox box;
  box.element = doc.CreateElement(dom::TagId::kDiv);
  box.style = &style;
  box.x = 5;
  box.y = 5;
  box.content_width = 100;
  box.content_height = 100;
  box.border[layout::LayoutBox::kTop] = 5;
  box.border[layout::LayoutBox::kRight] = 5;
  box.border[layout::LayoutBox::kBottom] = 5;
  box.border[layout::LayoutBox::kLeft] = 5;
  // border box origin = (5 - 0 - 5, 5 - 0 - 5) = (0, 0)
  // border box size = 110 x 110

  EXPECT_EQ(event::HitTest(&box, 0, 0), box.element);
  EXPECT_EQ(event::HitTest(&box, 3, 3), box.element);
  EXPECT_EQ(event::HitTest(&box, 109, 109), box.element);
}

TEST(HitTestTest, SingleBoxMarginMiss) {
  dom::Document doc;
  css::ComputedStyle style;
  layout::LayoutBox box;
  box.element = doc.CreateElement(dom::TagId::kDiv);
  box.style = &style;
  box.x = 10;
  box.y = 10;
  box.content_width = 100;
  box.content_height = 100;
  box.margin[layout::LayoutBox::kTop] = 10;
  box.margin[layout::LayoutBox::kLeft] = 10;
  // border box origin = (10, 10), size = 100x100
  // margin area extends beyond border box

  EXPECT_EQ(event::HitTest(&box, 5, 5), nullptr);
  EXPECT_EQ(event::HitTest(&box, 10, 10), box.element);
}

TEST(HitTestTest, NestedBoxes) {
  dom::Document doc;
  css::ComputedStyle parent_style;
  css::ComputedStyle child_style;

  layout::LayoutBox parent;
  parent.element = doc.CreateElement(dom::TagId::kDiv);
  parent.style = &parent_style;
  parent.x = 0;
  parent.y = 0;
  parent.content_width = 200;
  parent.content_height = 200;

  layout::LayoutBox child;
  child.element = doc.CreateElement(dom::TagId::kDiv);
  child.style = &child_style;
  child.x = 50;
  child.y = 50;
  child.content_width = 50;
  child.content_height = 50;

  parent.AppendChild(&child);

  EXPECT_EQ(event::HitTest(&parent, 60, 60), child.element);
}

TEST(HitTestTest, NestedBoxParentArea) {
  dom::Document doc;
  css::ComputedStyle parent_style;
  css::ComputedStyle child_style;

  layout::LayoutBox parent;
  parent.element = doc.CreateElement(dom::TagId::kDiv);
  parent.style = &parent_style;
  parent.x = 0;
  parent.y = 0;
  parent.content_width = 200;
  parent.content_height = 200;

  layout::LayoutBox child;
  child.element = doc.CreateElement(dom::TagId::kDiv);
  child.style = &child_style;
  child.x = 50;
  child.y = 50;
  child.content_width = 50;
  child.content_height = 50;

  parent.AppendChild(&child);

  EXPECT_EQ(event::HitTest(&parent, 10, 10), parent.element);
}

TEST(HitTestTest, ZIndexOrder) {
  dom::Document doc;
  css::ComputedStyle parent_style;

  layout::LayoutBox parent;
  parent.element = doc.CreateElement(dom::TagId::kDiv);
  parent.style = &parent_style;
  parent.x = 0;
  parent.y = 0;
  parent.content_width = 200;
  parent.content_height = 200;

  css::ComputedStyle low_style;
  low_style.z_index = 1;
  layout::LayoutBox low;
  low.element = doc.CreateElement(dom::TagId::kDiv);
  low.style = &low_style;
  low.x = 0;
  low.y = 0;
  low.content_width = 100;
  low.content_height = 100;

  css::ComputedStyle high_style;
  high_style.z_index = 10;
  layout::LayoutBox high;
  high.element = doc.CreateElement(dom::TagId::kDiv);
  high.style = &high_style;
  high.x = 0;
  high.y = 0;
  high.content_width = 100;
  high.content_height = 100;

  // Append low first, then high — but z_index should make high win
  parent.AppendChild(&low);
  parent.AppendChild(&high);

  EXPECT_EQ(event::HitTest(&parent, 50, 50), high.element);
}

TEST(HitTestTest, OverflowHiddenClip) {
  dom::Document doc;
  css::ComputedStyle parent_style;
  parent_style.overflow = css::Overflow::kHidden;

  layout::LayoutBox parent;
  parent.element = doc.CreateElement(dom::TagId::kDiv);
  parent.style = &parent_style;
  parent.x = 0;
  parent.y = 0;
  parent.content_width = 100;
  parent.content_height = 100;

  css::ComputedStyle child_style;
  layout::LayoutBox child;
  child.element = doc.CreateElement(dom::TagId::kDiv);
  child.style = &child_style;
  child.x = 80;
  child.y = 80;
  child.content_width = 50;
  child.content_height = 50;
  // child border box = (80, 80) to (130, 130)
  // parent padding box = (0, 0) to (100, 100)

  parent.AppendChild(&child);

  // Inside both parent and child
  EXPECT_EQ(event::HitTest(&parent, 90, 90), child.element);
  // Outside parent's padding box — overflow:hidden clips
  EXPECT_EQ(event::HitTest(&parent, 110, 110), nullptr);
}

TEST(HitTestTest, VisibilityHidden) {
  dom::Document doc;
  css::ComputedStyle style;
  style.visibility = css::Visibility::kHidden;

  layout::LayoutBox box;
  box.element = doc.CreateElement(dom::TagId::kDiv);
  box.style = &style;
  box.x = 0;
  box.y = 0;
  box.content_width = 100;
  box.content_height = 100;

  EXPECT_EQ(event::HitTest(&box, 50, 50), nullptr);
}

TEST(HitTestTest, TextNodeReturnsParent) {
  dom::Document doc;
  css::ComputedStyle parent_style;
  css::ComputedStyle text_style;

  layout::LayoutBox parent;
  parent.element = doc.CreateElement(dom::TagId::kP);
  parent.style = &parent_style;
  parent.x = 0;
  parent.y = 0;
  parent.content_width = 200;
  parent.content_height = 50;

  layout::LayoutBox text_box;
  text_box.type = layout::LayoutType::kText;
  text_box.text_node = doc.CreateText(String("hello"));
  text_box.style = &text_style;
  text_box.x = 0;
  text_box.y = 0;
  text_box.content_width = 100;
  text_box.content_height = 20;

  parent.AppendChild(&text_box);

  EXPECT_EQ(event::HitTest(&parent, 50, 10), parent.element);
}

TEST(HitTestTest, DeepNesting) {
  dom::Document doc;
  css::ComputedStyle s1, s2, s3;

  layout::LayoutBox l1;
  l1.element = doc.CreateElement(dom::TagId::kDiv);
  l1.style = &s1;
  l1.x = 0;
  l1.y = 0;
  l1.content_width = 300;
  l1.content_height = 300;

  layout::LayoutBox l2;
  l2.element = doc.CreateElement(dom::TagId::kDiv);
  l2.style = &s2;
  l2.x = 10;
  l2.y = 10;
  l2.content_width = 200;
  l2.content_height = 200;

  layout::LayoutBox l3;
  l3.element = doc.CreateElement(dom::TagId::kDiv);
  l3.style = &s3;
  l3.x = 20;
  l3.y = 20;
  l3.content_width = 100;
  l3.content_height = 100;

  l1.AppendChild(&l2);
  l2.AppendChild(&l3);

  EXPECT_EQ(event::HitTest(&l1, 50, 50), l3.element);
  EXPECT_EQ(event::HitTest(&l1, 15, 15), l2.element);
  EXPECT_EQ(event::HitTest(&l1, 5, 5), l1.element);
}

}  // namespace
