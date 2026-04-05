#include "veloxa/core/layout/flex_layout.h"

#include <gtest/gtest.h>

namespace vx::layout {
namespace {

LayoutContext MakeCtx(f32 vw = 800, f32 vh = 600) {
  LayoutContext ctx;
  ctx.viewport_width = vw;
  ctx.viewport_height = vh;
  ctx.root_font_size = 16.0f;
  return ctx;
}

css::ComputedStyle MakeFlexStyle() {
  css::ComputedStyle s;
  s.display = css::Display::kFlex;
  return s;
}

css::ComputedStyle MakeItemStyle(f32 basis_px = 0, f32 grow = 0,
                                 f32 shrink = 1) {
  css::ComputedStyle s;
  if (basis_px > 0) s.flex_basis = css::LengthValue::Px(basis_px);
  s.flex_grow = grow;
  s.flex_shrink = shrink;
  return s;
}

// ---------------------------------------------------------------
// 1. RowDirection
// ---------------------------------------------------------------
TEST(FlexLayoutTest, RowDirection) {
  auto ctx = MakeCtx();

  css::ComputedStyle container_style = MakeFlexStyle();
  container_style.width = css::LengthValue::Px(400);

  css::ComputedStyle item_style = MakeItemStyle(100);

  LayoutBox container;
  container.type = LayoutType::kFlex;
  container.style = &container_style;

  LayoutBox c1, c2, c3;
  c1.type = LayoutType::kBlock;
  c1.style = &item_style;
  c2.type = LayoutType::kBlock;
  c2.style = &item_style;
  c3.type = LayoutType::kBlock;
  c3.style = &item_style;

  container.AppendChild(&c1);
  container.AppendChild(&c2);
  container.AppendChild(&c3);

  LayoutFlex(&container, 800, ctx);

  EXPECT_FLOAT_EQ(c1.x, 0);
  EXPECT_FLOAT_EQ(c2.x, 100);
  EXPECT_FLOAT_EQ(c3.x, 200);
  EXPECT_FLOAT_EQ(c1.content_width, 100);
  EXPECT_FLOAT_EQ(c2.content_width, 100);
  EXPECT_FLOAT_EQ(c3.content_width, 100);
}

// ---------------------------------------------------------------
// 2. ColumnDirection
// ---------------------------------------------------------------
TEST(FlexLayoutTest, ColumnDirection) {
  auto ctx = MakeCtx();

  css::ComputedStyle container_style = MakeFlexStyle();
  container_style.flex_direction = css::FlexDirection::kColumn;
  container_style.height = css::LengthValue::Px(300);

  css::ComputedStyle item_style = MakeItemStyle(50);

  LayoutBox container;
  container.type = LayoutType::kFlex;
  container.style = &container_style;

  LayoutBox c1, c2, c3;
  c1.type = LayoutType::kBlock;
  c1.style = &item_style;
  c2.type = LayoutType::kBlock;
  c2.style = &item_style;
  c3.type = LayoutType::kBlock;
  c3.style = &item_style;

  container.AppendChild(&c1);
  container.AppendChild(&c2);
  container.AppendChild(&c3);

  LayoutFlex(&container, 800, ctx);

  EXPECT_FLOAT_EQ(c1.y, 0);
  EXPECT_FLOAT_EQ(c2.y, 50);
  EXPECT_FLOAT_EQ(c3.y, 100);
  EXPECT_FLOAT_EQ(c1.content_height, 50);
  EXPECT_FLOAT_EQ(c2.content_height, 50);
  EXPECT_FLOAT_EQ(c3.content_height, 50);
}

// ---------------------------------------------------------------
// 3. FlexGrow — equal
// ---------------------------------------------------------------
TEST(FlexLayoutTest, FlexGrow) {
  auto ctx = MakeCtx();

  css::ComputedStyle container_style = MakeFlexStyle();
  container_style.width = css::LengthValue::Px(400);

  css::ComputedStyle item_style = MakeItemStyle(100, /*grow=*/1);

  LayoutBox container;
  container.type = LayoutType::kFlex;
  container.style = &container_style;

  LayoutBox c1, c2;
  c1.type = LayoutType::kBlock;
  c1.style = &item_style;
  c2.type = LayoutType::kBlock;
  c2.style = &item_style;

  container.AppendChild(&c1);
  container.AppendChild(&c2);

  LayoutFlex(&container, 800, ctx);

  EXPECT_FLOAT_EQ(c1.content_width, 200);
  EXPECT_FLOAT_EQ(c2.content_width, 200);
  EXPECT_FLOAT_EQ(c1.x, 0);
  EXPECT_FLOAT_EQ(c2.x, 200);
}

// ---------------------------------------------------------------
// 4. FlexGrowUnequal — 1:3
// ---------------------------------------------------------------
TEST(FlexLayoutTest, FlexGrowUnequal) {
  auto ctx = MakeCtx();

  css::ComputedStyle container_style = MakeFlexStyle();
  container_style.width = css::LengthValue::Px(500);

  css::ComputedStyle style1 = MakeItemStyle(100, /*grow=*/1);
  css::ComputedStyle style2 = MakeItemStyle(100, /*grow=*/3);

  LayoutBox container;
  container.type = LayoutType::kFlex;
  container.style = &container_style;

  LayoutBox c1, c2;
  c1.type = LayoutType::kBlock;
  c1.style = &style1;
  c2.type = LayoutType::kBlock;
  c2.style = &style2;

  container.AppendChild(&c1);
  container.AppendChild(&c2);

  LayoutFlex(&container, 800, ctx);

  // free = 500 - 200 = 300; c1 gets 75, c2 gets 225
  EXPECT_FLOAT_EQ(c1.content_width, 175);
  EXPECT_FLOAT_EQ(c2.content_width, 325);
  EXPECT_FLOAT_EQ(c1.x, 0);
  EXPECT_FLOAT_EQ(c2.x, 175);
}

// ---------------------------------------------------------------
// 5. FlexShrink
// ---------------------------------------------------------------
TEST(FlexLayoutTest, FlexShrink) {
  auto ctx = MakeCtx();

  css::ComputedStyle container_style = MakeFlexStyle();
  container_style.width = css::LengthValue::Px(400);

  css::ComputedStyle item_style = MakeItemStyle(300, /*grow=*/0, /*shrink=*/1);

  LayoutBox container;
  container.type = LayoutType::kFlex;
  container.style = &container_style;

  LayoutBox c1, c2;
  c1.type = LayoutType::kBlock;
  c1.style = &item_style;
  c2.type = LayoutType::kBlock;
  c2.style = &item_style;

  container.AppendChild(&c1);
  container.AppendChild(&c2);

  LayoutFlex(&container, 800, ctx);

  // total basis = 600, free = -200, each shrinks by 100
  EXPECT_FLOAT_EQ(c1.content_width, 200);
  EXPECT_FLOAT_EQ(c2.content_width, 200);
}

// ---------------------------------------------------------------
// 6. FlexBasisPx
// ---------------------------------------------------------------
TEST(FlexLayoutTest, FlexBasisPx) {
  auto ctx = MakeCtx();

  css::ComputedStyle container_style = MakeFlexStyle();
  container_style.width = css::LengthValue::Px(400);

  css::ComputedStyle item_style;
  item_style.flex_basis = css::LengthValue::Px(150);

  LayoutBox container;
  container.type = LayoutType::kFlex;
  container.style = &container_style;

  LayoutBox c1;
  c1.type = LayoutType::kBlock;
  c1.style = &item_style;

  container.AppendChild(&c1);

  LayoutFlex(&container, 800, ctx);

  EXPECT_FLOAT_EQ(c1.content_width, 150);
  EXPECT_FLOAT_EQ(c1.x, 0);
}

// ---------------------------------------------------------------
// 7. JustifyFlexStart
// ---------------------------------------------------------------
TEST(FlexLayoutTest, JustifyFlexStart) {
  auto ctx = MakeCtx();

  css::ComputedStyle container_style = MakeFlexStyle();
  container_style.width = css::LengthValue::Px(400);
  container_style.justify_content = css::JustifyContent::kFlexStart;

  css::ComputedStyle item_style = MakeItemStyle(100);

  LayoutBox container;
  container.type = LayoutType::kFlex;
  container.style = &container_style;

  LayoutBox c1, c2;
  c1.type = LayoutType::kBlock;
  c1.style = &item_style;
  c2.type = LayoutType::kBlock;
  c2.style = &item_style;

  container.AppendChild(&c1);
  container.AppendChild(&c2);

  LayoutFlex(&container, 800, ctx);

  EXPECT_FLOAT_EQ(c1.x, 0);
  EXPECT_FLOAT_EQ(c2.x, 100);
}

// ---------------------------------------------------------------
// 8. JustifyFlexEnd
// ---------------------------------------------------------------
TEST(FlexLayoutTest, JustifyFlexEnd) {
  auto ctx = MakeCtx();

  css::ComputedStyle container_style = MakeFlexStyle();
  container_style.width = css::LengthValue::Px(400);
  container_style.justify_content = css::JustifyContent::kFlexEnd;

  css::ComputedStyle item_style = MakeItemStyle(100);

  LayoutBox container;
  container.type = LayoutType::kFlex;
  container.style = &container_style;

  LayoutBox c1, c2;
  c1.type = LayoutType::kBlock;
  c1.style = &item_style;
  c2.type = LayoutType::kBlock;
  c2.style = &item_style;

  container.AppendChild(&c1);
  container.AppendChild(&c2);

  LayoutFlex(&container, 800, ctx);

  // remaining = 400 - 200 = 200, items shift right by 200
  EXPECT_FLOAT_EQ(c1.x, 200);
  EXPECT_FLOAT_EQ(c2.x, 300);
}

// ---------------------------------------------------------------
// 9. JustifyCenter
// ---------------------------------------------------------------
TEST(FlexLayoutTest, JustifyCenter) {
  auto ctx = MakeCtx();

  css::ComputedStyle container_style = MakeFlexStyle();
  container_style.width = css::LengthValue::Px(400);
  container_style.justify_content = css::JustifyContent::kCenter;

  css::ComputedStyle item_style = MakeItemStyle(100);

  LayoutBox container;
  container.type = LayoutType::kFlex;
  container.style = &container_style;

  LayoutBox c1, c2;
  c1.type = LayoutType::kBlock;
  c1.style = &item_style;
  c2.type = LayoutType::kBlock;
  c2.style = &item_style;

  container.AppendChild(&c1);
  container.AppendChild(&c2);

  LayoutFlex(&container, 800, ctx);

  // remaining = 200, offset = 100
  EXPECT_FLOAT_EQ(c1.x, 100);
  EXPECT_FLOAT_EQ(c2.x, 200);
}

// ---------------------------------------------------------------
// 10. JustifySpaceBetween
// ---------------------------------------------------------------
TEST(FlexLayoutTest, JustifySpaceBetween) {
  auto ctx = MakeCtx();

  css::ComputedStyle container_style = MakeFlexStyle();
  container_style.width = css::LengthValue::Px(400);
  container_style.justify_content = css::JustifyContent::kSpaceBetween;

  css::ComputedStyle item_style = MakeItemStyle(100);

  LayoutBox container;
  container.type = LayoutType::kFlex;
  container.style = &container_style;

  LayoutBox c1, c2;
  c1.type = LayoutType::kBlock;
  c1.style = &item_style;
  c2.type = LayoutType::kBlock;
  c2.style = &item_style;

  container.AppendChild(&c1);
  container.AppendChild(&c2);

  LayoutFlex(&container, 800, ctx);

  // remaining = 200, gap = 200/(2-1) = 200
  EXPECT_FLOAT_EQ(c1.x, 0);
  EXPECT_FLOAT_EQ(c2.x, 300);
}

// ---------------------------------------------------------------
// 11. JustifySpaceAround
// ---------------------------------------------------------------
TEST(FlexLayoutTest, JustifySpaceAround) {
  auto ctx = MakeCtx();

  css::ComputedStyle container_style = MakeFlexStyle();
  container_style.width = css::LengthValue::Px(400);
  container_style.justify_content = css::JustifyContent::kSpaceAround;

  css::ComputedStyle item_style = MakeItemStyle(100);

  LayoutBox container;
  container.type = LayoutType::kFlex;
  container.style = &container_style;

  LayoutBox c1, c2;
  c1.type = LayoutType::kBlock;
  c1.style = &item_style;
  c2.type = LayoutType::kBlock;
  c2.style = &item_style;

  container.AppendChild(&c1);
  container.AppendChild(&c2);

  LayoutFlex(&container, 800, ctx);

  // remaining=200, space=100, offset=50, gap=100
  EXPECT_FLOAT_EQ(c1.x, 50);
  EXPECT_FLOAT_EQ(c2.x, 250);
}

// ---------------------------------------------------------------
// 12. AlignStretch (default)
// ---------------------------------------------------------------
TEST(FlexLayoutTest, AlignStretch) {
  auto ctx = MakeCtx();

  css::ComputedStyle container_style = MakeFlexStyle();
  container_style.width = css::LengthValue::Px(400);

  css::ComputedStyle style_tall = MakeItemStyle(100);
  style_tall.height = css::LengthValue::Px(80);

  css::ComputedStyle style_auto = MakeItemStyle(100);

  LayoutBox container;
  container.type = LayoutType::kFlex;
  container.style = &container_style;

  LayoutBox c1, c2;
  c1.type = LayoutType::kBlock;
  c1.style = &style_tall;
  c2.type = LayoutType::kBlock;
  c2.style = &style_auto;

  container.AppendChild(&c1);
  container.AppendChild(&c2);

  LayoutFlex(&container, 800, ctx);

  EXPECT_FLOAT_EQ(c1.content_height, 80);
  EXPECT_FLOAT_EQ(c2.content_height, 80);
}

// ---------------------------------------------------------------
// 13. AlignCenter
// ---------------------------------------------------------------
TEST(FlexLayoutTest, AlignCenter) {
  auto ctx = MakeCtx();

  css::ComputedStyle container_style = MakeFlexStyle();
  container_style.width = css::LengthValue::Px(400);
  container_style.align_items = css::AlignItems::kCenter;

  css::ComputedStyle style1 = MakeItemStyle(100);
  style1.height = css::LengthValue::Px(80);

  css::ComputedStyle style2 = MakeItemStyle(100);
  style2.height = css::LengthValue::Px(40);

  LayoutBox container;
  container.type = LayoutType::kFlex;
  container.style = &container_style;

  LayoutBox c1, c2;
  c1.type = LayoutType::kBlock;
  c1.style = &style1;
  c2.type = LayoutType::kBlock;
  c2.style = &style2;

  container.AppendChild(&c1);
  container.AppendChild(&c2);

  LayoutFlex(&container, 800, ctx);

  // line cross = 80; c1 centered at (80-80)/2 = 0; c2 at (80-40)/2 = 20
  EXPECT_FLOAT_EQ(c1.y, 0);
  EXPECT_FLOAT_EQ(c2.y, 20);
}

// ---------------------------------------------------------------
// 14. AlignSelfOverride
// ---------------------------------------------------------------
TEST(FlexLayoutTest, AlignSelfOverride) {
  auto ctx = MakeCtx();

  css::ComputedStyle container_style = MakeFlexStyle();
  container_style.width = css::LengthValue::Px(400);
  container_style.align_items = css::AlignItems::kFlexStart;

  css::ComputedStyle style1 = MakeItemStyle(100);
  style1.height = css::LengthValue::Px(40);

  css::ComputedStyle style2 = MakeItemStyle(100);
  style2.height = css::LengthValue::Px(40);
  style2.align_self = css::AlignItems::kFlexEnd;

  css::ComputedStyle style3 = MakeItemStyle(100);
  style3.height = css::LengthValue::Px(80);

  LayoutBox container;
  container.type = LayoutType::kFlex;
  container.style = &container_style;

  LayoutBox c1, c2, c3;
  c1.type = LayoutType::kBlock;
  c1.style = &style1;
  c2.type = LayoutType::kBlock;
  c2.style = &style2;
  c3.type = LayoutType::kBlock;
  c3.style = &style3;

  container.AppendChild(&c1);
  container.AppendChild(&c2);
  container.AppendChild(&c3);

  LayoutFlex(&container, 800, ctx);

  // line cross = 80
  EXPECT_FLOAT_EQ(c1.y, 0);
  EXPECT_FLOAT_EQ(c2.y, 40);  // flex-end: 80 - 40 = 40
  EXPECT_FLOAT_EQ(c3.y, 0);
}

// ---------------------------------------------------------------
// 15. Gap
// ---------------------------------------------------------------
TEST(FlexLayoutTest, Gap) {
  auto ctx = MakeCtx();

  css::ComputedStyle container_style = MakeFlexStyle();
  container_style.width = css::LengthValue::Px(400);
  container_style.gap = css::LengthValue::Px(10);

  css::ComputedStyle item_style = MakeItemStyle(100);

  LayoutBox container;
  container.type = LayoutType::kFlex;
  container.style = &container_style;

  LayoutBox c1, c2, c3;
  c1.type = LayoutType::kBlock;
  c1.style = &item_style;
  c2.type = LayoutType::kBlock;
  c2.style = &item_style;
  c3.type = LayoutType::kBlock;
  c3.style = &item_style;

  container.AppendChild(&c1);
  container.AppendChild(&c2);
  container.AppendChild(&c3);

  LayoutFlex(&container, 800, ctx);

  EXPECT_FLOAT_EQ(c1.x, 0);
  EXPECT_FLOAT_EQ(c2.x, 110);
  EXPECT_FLOAT_EQ(c3.x, 220);
  EXPECT_FLOAT_EQ(c1.content_width, 100);
}

}  // namespace
}  // namespace vx::layout
