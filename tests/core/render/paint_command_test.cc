#include "veloxa/core/render/paint_command.h"
#include "veloxa/core/render/render_utils.h"

#include <gtest/gtest.h>

namespace vx::render {
namespace {

TEST(PaintCommandTest, FillRectFactory) {
  auto cmd = PaintCommand::FillRect({10, 20, 30, 40}, gfx::Color::Red());
  EXPECT_EQ(cmd.type, PaintCommand::Type::kFillRect);
  EXPECT_EQ(cmd.rect.x, 10);
  EXPECT_EQ(cmd.rect.y, 20);
  EXPECT_EQ(cmd.rect.w, 30);
  EXPECT_EQ(cmd.rect.h, 40);
  EXPECT_EQ(cmd.color, gfx::Color::Red());
}

TEST(PaintCommandTest, DrawTextFactory) {
  auto cmd = PaintCommand::DrawText("hello", {0, 0, 100, 20}, 16.0f,
                                     gfx::Color::Black());
  EXPECT_EQ(cmd.type, PaintCommand::Type::kDrawText);
  EXPECT_EQ(cmd.text, StringView("hello"));
  EXPECT_EQ(cmd.rect.w, 100);
  EXPECT_EQ(cmd.param, 16.0f);
  EXPECT_EQ(cmd.color, gfx::Color::Black());
}

TEST(PaintCommandTest, PushClipRectFactory) {
  auto cmd = PaintCommand::PushClipRect({5, 5, 50, 50});
  EXPECT_EQ(cmd.type, PaintCommand::Type::kPushClipRect);
  EXPECT_EQ(cmd.rect.x, 5);
  EXPECT_EQ(cmd.rect.w, 50);
}

TEST(PaintCommandTest, PopClipFactory) {
  auto cmd = PaintCommand::PopClip();
  EXPECT_EQ(cmd.type, PaintCommand::Type::kPopClip);
}

TEST(PaintCommandTest, PushLayerFactory) {
  auto cmd = PaintCommand::PushLayer({0, 0, 200, 100}, 0.5f);
  EXPECT_EQ(cmd.type, PaintCommand::Type::kPushLayer);
  EXPECT_EQ(cmd.rect.w, 200);
  EXPECT_EQ(cmd.param, 0.5f);
}

TEST(PaintCommandTest, PopLayerFactory) {
  auto cmd = PaintCommand::PopLayer();
  EXPECT_EQ(cmd.type, PaintCommand::Type::kPopLayer);
}

TEST(PaintCommandTest, DisplayListPushAndIterate) {
  DisplayList list;
  list.push_back(PaintCommand::FillRect({0, 0, 10, 10}, gfx::Color::Red()));
  list.push_back(PaintCommand::DrawText("hi", {0, 0, 50, 20}, 12.0f,
                                         gfx::Color::Black()));
  EXPECT_EQ(list.size(), 2u);
  EXPECT_EQ(list[0].type, PaintCommand::Type::kFillRect);
  EXPECT_EQ(list[1].type, PaintCommand::Type::kDrawText);
}

TEST(RenderUtilsTest, CssColorToGfx_Red) {
  auto c = CssColorToGfx(0xFF0000FFu);
  EXPECT_EQ(c.r, 255);
  EXPECT_EQ(c.g, 0);
  EXPECT_EQ(c.b, 0);
  EXPECT_EQ(c.a, 255);
}

TEST(RenderUtilsTest, CssColorToGfx_Green) {
  auto c = CssColorToGfx(0x00FF00FFu);
  EXPECT_EQ(c.r, 0);
  EXPECT_EQ(c.g, 255);
  EXPECT_EQ(c.b, 0);
  EXPECT_EQ(c.a, 255);
}

TEST(RenderUtilsTest, CssColorToGfx_Blue) {
  auto c = CssColorToGfx(0x0000FFFFu);
  EXPECT_EQ(c.r, 0);
  EXPECT_EQ(c.g, 0);
  EXPECT_EQ(c.b, 255);
  EXPECT_EQ(c.a, 255);
}

TEST(RenderUtilsTest, CssColorToGfx_Black) {
  auto c = CssColorToGfx(0x000000FFu);
  EXPECT_EQ(c.r, 0);
  EXPECT_EQ(c.g, 0);
  EXPECT_EQ(c.b, 0);
  EXPECT_EQ(c.a, 255);
}

TEST(RenderUtilsTest, CssColorToGfx_Transparent) {
  auto c = CssColorToGfx(0x00000000u);
  EXPECT_EQ(c.r, 0);
  EXPECT_EQ(c.g, 0);
  EXPECT_EQ(c.b, 0);
  EXPECT_EQ(c.a, 0);
}

TEST(RenderUtilsTest, CssColorToGfx_White) {
  auto c = CssColorToGfx(0xFFFFFFFFu);
  EXPECT_EQ(c.r, 255);
  EXPECT_EQ(c.g, 255);
  EXPECT_EQ(c.b, 255);
  EXPECT_EQ(c.a, 255);
}

}  // namespace
}  // namespace vx::render
