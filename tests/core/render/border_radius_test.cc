#include <cstring>

#include <gtest/gtest.h>

#include "veloxa/core/css/computed_style.h"
#include "veloxa/core/css/enums.h"
#include "veloxa/core/layout/layout_box.h"
#include "veloxa/core/render/paint_command.h"
#include "veloxa/core/render/render_utils.h"
#include "veloxa/core/render/renderer.h"
#include "veloxa/graphics/software/software_canvas.h"
#include "veloxa/platform/headless/memory_surface.h"

namespace vx::render {
namespace {

// --- PaintCommand factory tests ---

TEST(PaintCommandTest, FillRoundedRectFactory) {
  gfx::Rect r{10, 20, 100, 50};
  gfx::Color c = gfx::Color::Red();
  auto cmd = PaintCommand::FillRoundedRect(r, 8.0f, c);
  EXPECT_EQ(cmd.type, PaintCommand::Type::kFillRoundedRect);
  EXPECT_EQ(cmd.rect, r);
  EXPECT_EQ(cmd.color, c);
  EXPECT_FLOAT_EQ(cmd.param, 8.0f);
}

TEST(PaintCommandTest, StrokeRoundedRectFactory) {
  gfx::Rect r{0, 0, 200, 100};
  gfx::Color c = gfx::Color::Blue();
  auto cmd = PaintCommand::StrokeRoundedRect(r, 12.0f, c, 2.0f);
  EXPECT_EQ(cmd.type, PaintCommand::Type::kStrokeRoundedRect);
  EXPECT_EQ(cmd.rect, r);
  EXPECT_EQ(cmd.color, c);
  EXPECT_FLOAT_EQ(cmd.param, 12.0f);
  EXPECT_FLOAT_EQ(cmd.param2, 2.0f);
}

TEST(PaintCommandTest, RoundedRectEquality) {
  auto a =
      PaintCommand::FillRoundedRect({0, 0, 100, 50}, 8.0f, gfx::Color::Red());
  auto b =
      PaintCommand::FillRoundedRect({0, 0, 100, 50}, 8.0f, gfx::Color::Red());
  auto c =
      PaintCommand::FillRoundedRect({0, 0, 100, 50}, 4.0f, gfx::Color::Red());
  EXPECT_EQ(a, b);
  EXPECT_NE(a, c);
}

TEST(PaintCommandTest, StrokeRoundedRectEquality) {
  auto a = PaintCommand::StrokeRoundedRect({0, 0, 100, 50}, 8.0f,
                                           gfx::Color::Red(), 2.0f);
  auto b = PaintCommand::StrokeRoundedRect({0, 0, 100, 50}, 8.0f,
                                           gfx::Color::Red(), 2.0f);
  auto c = PaintCommand::StrokeRoundedRect({0, 0, 100, 50}, 8.0f,
                                           gfx::Color::Red(), 3.0f);
  EXPECT_EQ(a, b);
  EXPECT_NE(a, c);
}

// --- Record tests ---

TEST(BorderRadiusRenderTest, RecordUsesRoundedRectWhenRadiusSet) {
  css::ComputedStyle style;
  style.background_color = 0xFF0000FFu;
  style.border_radius = {10.0f, css::Unit::kPx};

  layout::LayoutBox box;
  box.style = &style;
  box.content_width = 100;
  box.content_height = 50;

  auto list = Record(&box);

  bool has_fill_rounded = false;
  for (const auto& cmd : list) {
    if (cmd.type == PaintCommand::Type::kFillRoundedRect) {
      has_fill_rounded = true;
      EXPECT_FLOAT_EQ(cmd.param, 10.0f);
    }
    if (cmd.type == PaintCommand::Type::kFillRect) {
      FAIL() << "Expected FillRoundedRect, got FillRect for background";
    }
  }
  EXPECT_TRUE(has_fill_rounded);
}

TEST(BorderRadiusRenderTest, RecordUsesFillRectWhenNoRadius) {
  css::ComputedStyle style;
  style.background_color = 0xFF0000FFu;

  layout::LayoutBox box;
  box.style = &style;
  box.content_width = 100;
  box.content_height = 50;

  auto list = Record(&box);

  bool has_fill_rect = false;
  for (const auto& cmd : list) {
    if (cmd.type == PaintCommand::Type::kFillRect) has_fill_rect = true;
    if (cmd.type == PaintCommand::Type::kFillRoundedRect) {
      FAIL() << "Should not use FillRoundedRect when radius is 0";
    }
  }
  EXPECT_TRUE(has_fill_rect);
}

TEST(BorderRadiusRenderTest, RecordStrokeRoundedBorderWhenRadiusSet) {
  css::ComputedStyle style;
  style.background_color = 0x00000000u;
  style.border_radius = {8.0f, css::Unit::kPx};
  for (int i = 0; i < 4; ++i) {
    style.border_style[i] = css::BorderStyle::kSolid;
    style.border_width[i] = {2.0f, css::Unit::kPx};
    style.border_color[i] = 0x000000FFu;
  }

  layout::LayoutBox box;
  box.style = &style;
  box.content_width = 100;
  box.content_height = 50;
  for (int i = 0; i < 4; ++i) box.border[i] = 2;

  auto list = Record(&box);

  bool has_stroke_rounded = false;
  for (const auto& cmd : list) {
    if (cmd.type == PaintCommand::Type::kStrokeRoundedRect) {
      has_stroke_rounded = true;
      EXPECT_FLOAT_EQ(cmd.param, 8.0f);
      EXPECT_FLOAT_EQ(cmd.param2, 2.0f);
    }
  }
  EXPECT_TRUE(has_stroke_rounded);
}

// --- Replay test ---

TEST(BorderRadiusRenderTest, ReplayFillRoundedRect) {
  const u32 W = 100, H = 50;
  platform::MemorySurface surface(W, H);
  auto* pixels = surface.Lock();
  gfx::sw::SoftwareCanvas canvas(pixels, W, H, W * 4);
  canvas.Begin();
  canvas.Clear(gfx::Color::White());

  DisplayList list;
  list.push_back(PaintCommand::FillRoundedRect(
      {0, 0, static_cast<f32>(W), static_cast<f32>(H)}, 10.0f,
      gfx::Color::Red()));

  Replay(list, &canvas);
  canvas.End();
  surface.Unlock();

  const u32* data = surface.data();
  u32 center = data[(H / 2) * W + (W / 2)];
  EXPECT_EQ(center, gfx::Color::Red().ToRGBA32());

  u32 corner = data[0];
  EXPECT_NE(corner, gfx::Color::Red().ToRGBA32());
}

}  // namespace
}  // namespace vx::render
