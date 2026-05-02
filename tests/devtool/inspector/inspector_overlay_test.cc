#include "veloxa/devtool/inspector/inspector_overlay.h"

#include <gtest/gtest.h>

#include "veloxa/core/layout/layout_box.h"
#include "veloxa/core/render/paint_command.h"
#include "veloxa/graphics/types.h"

namespace vx::devtool {
namespace {

// =============================================================================
// InjectHoverHighlight — DisplayList overlay 注入（A.1.1 / I3 / A5 / T5）。
//
// 关键约束（plan A.1.1 锁定 + creative #1 决策 4 渲染顺序末位 + A.0.4 工厂）：
//   - 仅追加 1 条 PaintCommand::Type::kOverlayHighlight 命令到 list 末尾
//   - 不 reset 既有 commands（T5 mitigation：reset 由 ResetOverlayCommands(list)
//     在帧首端做，A.0.4 已落地）
//   - rect = LayoutBox 的 border-box（border_box_width/height）
//   - null hovered_box → no-op（防御性）
// =============================================================================

TEST(InspectorOverlayTest, InjectHoverHighlightAppendsKOverlayHighlightCommand) {
  render::DisplayList list;
  layout::LayoutBox box;
  box.x = 10.0f;
  box.y = 20.0f;
  box.content_width = 80.0f;
  box.content_height = 30.0f;
  // border_box = content + padding + border：保持简单 → padding/border 都 0
  // 故 border_box_width = 80, border_box_height = 30
  ASSERT_EQ(box.border_box_width(), 80.0f);
  ASSERT_EQ(box.border_box_height(), 30.0f);

  InspectorOverlay::InjectHoverHighlight(list, &box);

  ASSERT_EQ(list.size(), 1u);
  EXPECT_EQ(list[0].type, render::PaintCommand::Type::kOverlayHighlight);
  EXPECT_EQ(list[0].rect, (gfx::Rect{10.0f, 20.0f, 80.0f, 30.0f}));
  // 默认 stroke_width = 2.0f（plan 锁定）
  EXPECT_FLOAT_EQ(list[0].param, 2.0f);
}

TEST(InspectorOverlayTest, InjectHoverHighlightDoesNotResetExistingCommands) {
  render::DisplayList list;
  // 模拟 target 已有 fill 命令
  list.push_back(render::PaintCommand::FillRect(
      gfx::Rect{0, 0, 100, 100}, gfx::Color::FromRGBA(200, 200, 200, 255)));

  layout::LayoutBox box;
  box.x = 5.0f;
  box.y = 5.0f;
  box.content_width = 10.0f;
  box.content_height = 10.0f;

  InspectorOverlay::InjectHoverHighlight(list, &box);

  // T5 verified: 既有 fill 保留 + overlay 追加在末位
  ASSERT_EQ(list.size(), 2u);
  EXPECT_EQ(list[0].type, render::PaintCommand::Type::kFillRect);
  EXPECT_EQ(list[1].type, render::PaintCommand::Type::kOverlayHighlight);
}

TEST(InspectorOverlayTest, InjectHoverHighlightWithNullBoxIsNoOp) {
  render::DisplayList list;
  list.push_back(render::PaintCommand::FillRect(
      gfx::Rect{0, 0, 1, 1}, gfx::Color::Red()));
  const std::size_t size_before = list.size();

  InspectorOverlay::InjectHoverHighlight(list, nullptr);

  EXPECT_EQ(list.size(), size_before);
}

TEST(InspectorOverlayTest, InjectHoverHighlightCustomColorAndStrokeWidth) {
  render::DisplayList list;
  layout::LayoutBox box;
  box.x = 0;
  box.y = 0;
  box.content_width = 50;
  box.content_height = 50;

  const gfx::Color custom_color = gfx::Color::FromRGBA(0, 200, 100, 180);
  const f32 custom_stroke = 4.5f;
  InspectorOverlay::InjectHoverHighlight(list, &box, custom_color,
                                          custom_stroke);

  ASSERT_EQ(list.size(), 1u);
  EXPECT_EQ(list[0].color, custom_color);
  EXPECT_FLOAT_EQ(list[0].param, custom_stroke);
}

TEST(InspectorOverlayTest, InjectHoverHighlightUsesBorderBoxIncludingPadding) {
  render::DisplayList list;
  layout::LayoutBox box;
  box.x = 100.0f;
  box.y = 200.0f;
  box.content_width = 50.0f;
  box.content_height = 30.0f;
  // padding[top, right, bottom, left] = 5, 6, 7, 8
  box.padding[layout::LayoutBox::kTop] = 5.0f;
  box.padding[layout::LayoutBox::kRight] = 6.0f;
  box.padding[layout::LayoutBox::kBottom] = 7.0f;
  box.padding[layout::LayoutBox::kLeft] = 8.0f;
  // border[top, right, bottom, left] = 1, 2, 3, 4
  box.border[layout::LayoutBox::kTop] = 1.0f;
  box.border[layout::LayoutBox::kRight] = 2.0f;
  box.border[layout::LayoutBox::kBottom] = 3.0f;
  box.border[layout::LayoutBox::kLeft] = 4.0f;
  // border_box_width = 50 + 6 + 8 + 2 + 4 = 70
  // border_box_height = 30 + 5 + 7 + 1 + 3 = 46

  InspectorOverlay::InjectHoverHighlight(list, &box);

  ASSERT_EQ(list.size(), 1u);
  EXPECT_EQ(list[0].rect, (gfx::Rect{100.0f, 200.0f, 70.0f, 46.0f}));
}

}  // namespace
}  // namespace vx::devtool
