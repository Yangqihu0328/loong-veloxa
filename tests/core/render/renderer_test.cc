#include "veloxa/core/render/renderer.h"

#include <cstring>

#include <gtest/gtest.h>

#include "veloxa/core/css/computed_style.h"
#include "veloxa/core/css/css_value.h"
#include "veloxa/core/css/enums.h"
#include "veloxa/core/dom/document.h"
#include "veloxa/core/layout/layout_box.h"
#include "veloxa/core/render/render_utils.h"
#include "veloxa/graphics/software/software_canvas.h"
#include "veloxa/platform/headless/memory_surface.h"

namespace vx::render {
namespace {

TEST(RendererTest, RecordNullRoot) {
  DisplayList list = Record(nullptr);
  EXPECT_TRUE(list.empty());
}

TEST(RendererTest, RecordSingleBoxNoBg) {
  css::ComputedStyle style;
  style.background_color = 0x00000000;

  layout::LayoutBox box;
  box.style = &style;
  box.x = 0;
  box.y = 0;
  box.content_width = 100;
  box.content_height = 50;

  DisplayList list = Record(&box);
  for (const auto& cmd : list) {
    EXPECT_NE(cmd.type, PaintCommand::Type::kFillRect);
  }
}

TEST(RendererTest, RecordSingleBoxWithBg) {
  css::ComputedStyle style;
  style.background_color = 0xFF0000FFu;

  layout::LayoutBox box;
  box.style = &style;
  box.x = 0;
  box.y = 0;
  box.content_width = 100;
  box.content_height = 50;

  DisplayList list = Record(&box);
  ASSERT_FALSE(list.empty());

  bool found = false;
  for (const auto& cmd : list) {
    if (cmd.type == PaintCommand::Type::kFillRect) {
      EXPECT_EQ(cmd.color.r, 255);
      EXPECT_EQ(cmd.color.g, 0);
      EXPECT_EQ(cmd.color.b, 0);
      EXPECT_EQ(cmd.color.a, 255);
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found);
}

TEST(RendererTest, RecordBordersAllSides) {
  css::ComputedStyle style;
  style.background_color = 0x00000000;
  for (int i = 0; i < 4; ++i) {
    style.border_style[i] = css::BorderStyle::kSolid;
    style.border_color[i] = 0x000000FFu;
  }

  layout::LayoutBox box;
  box.style = &style;
  box.x = 10;
  box.y = 10;
  box.content_width = 100;
  box.content_height = 50;
  for (int i = 0; i < 4; ++i) box.border[i] = 2;

  DisplayList list = Record(&box);

  u32 fill_count = 0;
  for (const auto& cmd : list) {
    if (cmd.type == PaintCommand::Type::kFillRect) ++fill_count;
  }
  EXPECT_EQ(fill_count, 4u);
}

TEST(RendererTest, RecordBorderNoneSkipped) {
  css::ComputedStyle style;
  style.background_color = 0x00000000;
  for (int i = 0; i < 4; ++i) {
    style.border_style[i] = css::BorderStyle::kNone;
    style.border_color[i] = 0x000000FFu;
  }

  layout::LayoutBox box;
  box.style = &style;
  box.x = 10;
  box.y = 10;
  box.content_width = 100;
  box.content_height = 50;
  for (int i = 0; i < 4; ++i) box.border[i] = 2;

  DisplayList list = Record(&box);

  for (const auto& cmd : list) {
    EXPECT_NE(cmd.type, PaintCommand::Type::kFillRect);
  }
}

TEST(RendererTest, RecordBorderPartialSides) {
  css::ComputedStyle style;
  style.background_color = 0x00000000;
  style.border_style[0] = css::BorderStyle::kSolid;
  style.border_style[1] = css::BorderStyle::kNone;
  style.border_style[2] = css::BorderStyle::kSolid;
  style.border_style[3] = css::BorderStyle::kNone;
  for (int i = 0; i < 4; ++i) style.border_color[i] = 0x000000FFu;

  layout::LayoutBox box;
  box.style = &style;
  box.x = 10;
  box.y = 10;
  box.content_width = 100;
  box.content_height = 50;
  for (int i = 0; i < 4; ++i) box.border[i] = 2;

  DisplayList list = Record(&box);

  u32 fill_count = 0;
  for (const auto& cmd : list) {
    if (cmd.type == PaintCommand::Type::kFillRect) ++fill_count;
  }
  EXPECT_EQ(fill_count, 2u);
}

TEST(RendererTest, RecordTextNode) {
  dom::Document doc;
  auto* text = doc.CreateText("hello");

  css::ComputedStyle style;
  style.font_size = css::LengthValue::Px(14.0f);
  style.color = 0x000000FFu;

  layout::LayoutBox box;
  box.type = layout::LayoutType::kText;
  box.text_node = text;
  box.style = &style;
  box.x = 0;
  box.y = 0;
  box.content_width = 50;
  box.content_height = 14;

  DisplayList list = Record(&box);

  bool found = false;
  for (const auto& cmd : list) {
    if (cmd.type == PaintCommand::Type::kDrawText) {
      EXPECT_EQ(cmd.text, StringView("hello"));
      EXPECT_FLOAT_EQ(cmd.param, 14.0f);
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found);
}

TEST(RendererTest, RecordEmptyText) {
  dom::Document doc;
  auto* text = doc.CreateText("");

  css::ComputedStyle style;

  layout::LayoutBox box;
  box.type = layout::LayoutType::kText;
  box.text_node = text;
  box.style = &style;
  box.x = 0;
  box.y = 0;
  box.content_width = 50;
  box.content_height = 14;

  DisplayList list = Record(&box);

  for (const auto& cmd : list) {
    EXPECT_NE(cmd.type, PaintCommand::Type::kDrawText);
  }
}

TEST(RendererTest, RecordVisibilityHidden) {
  css::ComputedStyle style;
  style.visibility = css::Visibility::kHidden;
  style.background_color = 0xFF0000FFu;

  layout::LayoutBox box;
  box.style = &style;
  box.x = 0;
  box.y = 0;
  box.content_width = 100;
  box.content_height = 50;

  DisplayList list = Record(&box);
  EXPECT_TRUE(list.empty());
}

TEST(RendererTest, RecordOpacityLayer) {
  css::ComputedStyle style;
  style.opacity = 0.5f;
  style.background_color = 0xFF0000FFu;

  layout::LayoutBox box;
  box.style = &style;
  box.x = 0;
  box.y = 0;
  box.content_width = 100;
  box.content_height = 50;

  DisplayList list = Record(&box);
  ASSERT_EQ(list.size(), 3u);
  EXPECT_EQ(list[0].type, PaintCommand::Type::kPushLayer);
  EXPECT_FLOAT_EQ(list[0].param, 0.5f);
  EXPECT_EQ(list[1].type, PaintCommand::Type::kFillRect);
  EXPECT_EQ(list[2].type, PaintCommand::Type::kPopLayer);
}

TEST(RendererTest, RecordOverflowHidden) {
  css::ComputedStyle parent_style;
  parent_style.overflow = css::Overflow::kHidden;
  parent_style.background_color = 0x00000000;

  css::ComputedStyle child_style;
  child_style.background_color = 0x00FF00FFu;

  layout::LayoutBox parent;
  parent.style = &parent_style;
  parent.x = 0;
  parent.y = 0;
  parent.content_width = 100;
  parent.content_height = 50;

  layout::LayoutBox child;
  child.style = &child_style;
  child.x = 0;
  child.y = 0;
  child.content_width = 50;
  child.content_height = 25;

  parent.AppendChild(&child);

  DisplayList list = Record(&parent);

  bool found_push = false;
  bool found_pop = false;
  for (const auto& cmd : list) {
    if (cmd.type == PaintCommand::Type::kPushClipRect) found_push = true;
    if (cmd.type == PaintCommand::Type::kPopClip) found_pop = true;
  }
  EXPECT_TRUE(found_push);
  EXPECT_TRUE(found_pop);
}

TEST(RendererTest, RecordZIndexSorting) {
  css::ComputedStyle parent_style;
  parent_style.background_color = 0x00000000;

  css::ComputedStyle style_red;
  style_red.background_color = 0xFF0000FFu;
  style_red.z_index = 2;

  css::ComputedStyle style_green;
  style_green.background_color = 0x00FF00FFu;
  style_green.z_index = 0;

  css::ComputedStyle style_blue;
  style_blue.background_color = 0x0000FFFFu;
  style_blue.z_index = 1;

  layout::LayoutBox parent;
  parent.style = &parent_style;
  parent.x = 0;
  parent.y = 0;
  parent.content_width = 300;
  parent.content_height = 100;

  layout::LayoutBox child_red;
  child_red.style = &style_red;
  child_red.x = 0;
  child_red.y = 0;
  child_red.content_width = 100;
  child_red.content_height = 100;

  layout::LayoutBox child_green;
  child_green.style = &style_green;
  child_green.x = 100;
  child_green.y = 0;
  child_green.content_width = 100;
  child_green.content_height = 100;

  layout::LayoutBox child_blue;
  child_blue.style = &style_blue;
  child_blue.x = 200;
  child_blue.y = 0;
  child_blue.content_width = 100;
  child_blue.content_height = 100;

  parent.AppendChild(&child_red);
  parent.AppendChild(&child_green);
  parent.AppendChild(&child_blue);

  DisplayList list = Record(&parent);

  Vector<gfx::Color> fill_colors;
  for (const auto& cmd : list) {
    if (cmd.type == PaintCommand::Type::kFillRect) {
      fill_colors.push_back(cmd.color);
    }
  }

  ASSERT_GE(fill_colors.size(), 3u);
  EXPECT_EQ(fill_colors[0], (gfx::Color{0, 255, 0, 255}));
  EXPECT_EQ(fill_colors[1], (gfx::Color{0, 0, 255, 255}));
  EXPECT_EQ(fill_colors[2], (gfx::Color{255, 0, 0, 255}));
}

TEST(RendererTest, RecordZIndexStableOrder) {
  css::ComputedStyle parent_style;
  parent_style.background_color = 0x00000000;

  css::ComputedStyle style_a;
  style_a.background_color = 0xFF0000FFu;
  style_a.z_index = 0;

  css::ComputedStyle style_b;
  style_b.background_color = 0x00FF00FFu;
  style_b.z_index = 0;

  css::ComputedStyle style_c;
  style_c.background_color = 0x0000FFFFu;
  style_c.z_index = 0;

  layout::LayoutBox parent;
  parent.style = &parent_style;
  parent.x = 0;
  parent.y = 0;
  parent.content_width = 300;
  parent.content_height = 100;

  layout::LayoutBox child_a;
  child_a.style = &style_a;
  child_a.x = 0;
  child_a.y = 0;
  child_a.content_width = 100;
  child_a.content_height = 100;

  layout::LayoutBox child_b;
  child_b.style = &style_b;
  child_b.x = 100;
  child_b.y = 0;
  child_b.content_width = 100;
  child_b.content_height = 100;

  layout::LayoutBox child_c;
  child_c.style = &style_c;
  child_c.x = 200;
  child_c.y = 0;
  child_c.content_width = 100;
  child_c.content_height = 100;

  parent.AppendChild(&child_a);
  parent.AppendChild(&child_b);
  parent.AppendChild(&child_c);

  DisplayList list = Record(&parent);

  Vector<gfx::Color> fill_colors;
  for (const auto& cmd : list) {
    if (cmd.type == PaintCommand::Type::kFillRect) {
      fill_colors.push_back(cmd.color);
    }
  }

  ASSERT_GE(fill_colors.size(), 3u);
  EXPECT_EQ(fill_colors[0], (gfx::Color{255, 0, 0, 255}));
  EXPECT_EQ(fill_colors[1], (gfx::Color{0, 255, 0, 255}));
  EXPECT_EQ(fill_colors[2], (gfx::Color{0, 0, 255, 255}));
}

TEST(RendererTest, RecordNestedBoxes) {
  css::ComputedStyle parent_style;
  parent_style.background_color = 0xFF0000FFu;

  css::ComputedStyle child_style;
  child_style.background_color = 0x0000FFFFu;

  layout::LayoutBox parent;
  parent.style = &parent_style;
  parent.x = 0;
  parent.y = 0;
  parent.content_width = 200;
  parent.content_height = 100;

  layout::LayoutBox child;
  child.style = &child_style;
  child.x = 10;
  child.y = 10;
  child.content_width = 50;
  child.content_height = 30;

  parent.AppendChild(&child);

  DisplayList list = Record(&parent);

  Vector<gfx::Color> fill_colors;
  for (const auto& cmd : list) {
    if (cmd.type == PaintCommand::Type::kFillRect) {
      fill_colors.push_back(cmd.color);
    }
  }

  ASSERT_GE(fill_colors.size(), 2u);
  EXPECT_EQ(fill_colors[0], (gfx::Color{255, 0, 0, 255}));
  EXPECT_EQ(fill_colors[1], (gfx::Color{0, 0, 255, 255}));
}

TEST(RendererTest, ReplayEmptyList) {
  constexpr u32 kW = 20, kH = 20, kStride = kW * 4;
  u32 pixels[kW * kH];
  std::memset(pixels, 0, sizeof(pixels));
  gfx::sw::SoftwareCanvas canvas(pixels, kW, kH, kStride);
  canvas.Begin();
  canvas.Clear(gfx::Color::White());

  u32 white = gfx::Color::White().ToRGBA32();
  for (u32 i = 0; i < kW * kH; ++i) {
    ASSERT_EQ(pixels[i], white);
  }

  DisplayList list;
  Replay(list, &canvas);

  canvas.End();

  for (u32 i = 0; i < kW * kH; ++i) {
    EXPECT_EQ(pixels[i], white);
  }
}

TEST(RendererTest, ReplayFillRect) {
  constexpr u32 kW = 20, kH = 20, kStride = kW * 4;
  u32 pixels[kW * kH];
  std::memset(pixels, 0, sizeof(pixels));
  gfx::sw::SoftwareCanvas canvas(pixels, kW, kH, kStride);
  canvas.Begin();
  canvas.Clear(gfx::Color::White());

  DisplayList list;
  list.push_back(
      PaintCommand::FillRect({2, 2, 5, 5}, gfx::Color::Red()));

  Replay(list, &canvas);
  canvas.End();

  auto PixelAt = [&](u32 x, u32 y) { return pixels[y * kW + x]; };

  u32 red = gfx::Color::Red().ToRGBA32();
  EXPECT_EQ(PixelAt(3, 3), red);
  EXPECT_EQ(PixelAt(2, 2), red);
  EXPECT_EQ(PixelAt(6, 6), red);

  u32 white = gfx::Color::White().ToRGBA32();
  EXPECT_EQ(PixelAt(0, 0), white);
  EXPECT_EQ(PixelAt(19, 19), white);
}

TEST(RendererTest, ReplayDrawText) {
  constexpr u32 kW = 20, kH = 20, kStride = kW * 4;
  u32 pixels[kW * kH];
  std::memset(pixels, 0, sizeof(pixels));
  gfx::sw::SoftwareCanvas canvas(pixels, kW, kH, kStride);
  canvas.Begin();
  canvas.Clear(gfx::Color::White());

  DisplayList list;
  list.push_back(PaintCommand::DrawText("A", {2, 2, 16, 16}, 14.0f,
                                        gfx::Color::Black()));

  Replay(list, &canvas);
  canvas.End();

  u32 white = gfx::Color::White().ToRGBA32();
  bool has_non_white = false;
  for (u32 y = 2; y < 18 && !has_non_white; ++y) {
    for (u32 x = 2; x < 18 && !has_non_white; ++x) {
      if (pixels[y * kW + x] != white) has_non_white = true;
    }
  }
  EXPECT_TRUE(has_non_white);
}

TEST(RendererTest, ReplayClipRect) {
  constexpr u32 kW = 20, kH = 20, kStride = kW * 4;
  u32 pixels[kW * kH];
  std::memset(pixels, 0, sizeof(pixels));
  gfx::sw::SoftwareCanvas canvas(pixels, kW, kH, kStride);
  canvas.Begin();
  canvas.Clear(gfx::Color::White());

  DisplayList list;
  list.push_back(PaintCommand::PushClipRect({5, 5, 10, 10}));
  list.push_back(
      PaintCommand::FillRect({0, 0, 20, 20}, gfx::Color::Red()));
  list.push_back(PaintCommand::PopClip());

  Replay(list, &canvas);
  canvas.End();

  auto PixelAt = [&](u32 x, u32 y) { return pixels[y * kW + x]; };

  u32 red = gfx::Color::Red().ToRGBA32();
  u32 white = gfx::Color::White().ToRGBA32();

  EXPECT_EQ(PixelAt(7, 7), red);
  EXPECT_EQ(PixelAt(0, 0), white);
  EXPECT_EQ(PixelAt(19, 19), white);
}

TEST(ComputeDirtyRectTest, IdenticalListsReturnEmpty) {
  DisplayList a;
  a.push_back(PaintCommand::FillRect({10, 10, 50, 50}, gfx::Color::Red()));
  DisplayList b = a;
  gfx::Rect dirty = ComputeDirtyRect(a, b, 800, 600);
  EXPECT_TRUE(dirty.IsEmpty());
}

TEST(ComputeDirtyRectTest, DifferentColorReturnsCmdRect) {
  DisplayList a;
  a.push_back(PaintCommand::FillRect({10, 20, 100, 50}, gfx::Color::Red()));
  DisplayList b;
  b.push_back(PaintCommand::FillRect({10, 20, 100, 50}, gfx::Color::Blue()));
  gfx::Rect dirty = ComputeDirtyRect(a, b, 800, 600);
  EXPECT_FLOAT_EQ(dirty.x, 10);
  EXPECT_FLOAT_EQ(dirty.y, 20);
  EXPECT_FLOAT_EQ(dirty.w, 100);
  EXPECT_FLOAT_EQ(dirty.h, 50);
}

TEST(ComputeDirtyRectTest, DifferentLengthsReturnFullViewport) {
  DisplayList a;
  a.push_back(PaintCommand::FillRect({0, 0, 50, 50}, gfx::Color::Red()));
  DisplayList b;
  gfx::Rect dirty = ComputeDirtyRect(a, b, 800, 600);
  EXPECT_FLOAT_EQ(dirty.x, 0);
  EXPECT_FLOAT_EQ(dirty.y, 0);
  EXPECT_FLOAT_EQ(dirty.w, 800);
  EXPECT_FLOAT_EQ(dirty.h, 600);
}

TEST(ComputeDirtyRectTest, MultipleDiffsUnionRects) {
  DisplayList a;
  a.push_back(PaintCommand::FillRect({0, 0, 10, 10}, gfx::Color::Red()));
  a.push_back(PaintCommand::FillRect({50, 50, 20, 20}, gfx::Color::Green()));
  a.push_back(PaintCommand::FillRect({100, 100, 5, 5}, gfx::Color::Blue()));
  DisplayList b;
  b.push_back(PaintCommand::FillRect({0, 0, 10, 10}, gfx::Color::Blue()));
  b.push_back(PaintCommand::FillRect({50, 50, 20, 20}, gfx::Color::Green()));
  b.push_back(PaintCommand::FillRect({100, 100, 5, 5}, gfx::Color::Red()));
  gfx::Rect dirty = ComputeDirtyRect(a, b, 800, 600);
  EXPECT_FLOAT_EQ(dirty.x, 0);
  EXPECT_FLOAT_EQ(dirty.y, 0);
  EXPECT_FLOAT_EQ(dirty.w, 105);
  EXPECT_FLOAT_EQ(dirty.h, 105);
}

TEST(ComputeDirtyRectTest, EmptyListsReturnEmpty) {
  DisplayList a;
  DisplayList b;
  gfx::Rect dirty = ComputeDirtyRect(a, b, 800, 600);
  EXPECT_TRUE(dirty.IsEmpty());
}

}  // namespace
}  // namespace vx::render
