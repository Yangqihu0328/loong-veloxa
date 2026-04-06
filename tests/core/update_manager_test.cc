#include "veloxa/core/update_manager.h"

#include <cstring>

#include <gtest/gtest.h>

#include "veloxa/core/css/parser.h"
#include "veloxa/core/html/parser.h"
#include "veloxa/core/layout/text_shaper.h"
#include "veloxa/core/render/render_utils.h"
#include "veloxa/graphics/software/software_canvas.h"
#include "veloxa/platform/headless/memory_surface.h"

namespace vx {
namespace {

constexpr u32 kW = 200, kH = 200, kStride = kW * 4;

class UpdateManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    doc_ = html::Parser::Parse("<div id='box'></div>");
    sheets_.push_back(css::CssParser::Parse(
        "#box { width: 100px; height: 100px; background-color: #0000ff; }"));
    std::memset(pixels_, 0, sizeof(pixels_));
    canvas_ = std::make_unique<gfx::sw::SoftwareCanvas>(pixels_, kW, kH,
                                                        kStride);
    canvas_->Begin();
    canvas_->Clear(gfx::Color::White());
  }

  dom::Document* doc_ = nullptr;
  Vector<css::Stylesheet> sheets_;
  layout::SimpleTextShaper shaper_;
  u32 pixels_[kW * kH];
  std::unique_ptr<gfx::sw::SoftwareCanvas> canvas_;

  UpdateManager::Config MakeConfig(event::EventManager* em = nullptr) {
    UpdateManager::Config cfg;
    cfg.document = doc_;
    cfg.stylesheets = &sheets_;
    cfg.layout_context.text_shaper = &shaper_;
    cfg.layout_context.viewport_width = 200;
    cfg.layout_context.viewport_height = 200;
    cfg.canvas = canvas_.get();
    cfg.event_manager = em;
    return cfg;
  }
};

TEST_F(UpdateManagerTest, InitiallyDirty) {
  UpdateManager um(MakeConfig());
  EXPECT_TRUE(um.is_dirty());
}

TEST_F(UpdateManagerTest, UpdateClearsDirty) {
  UpdateManager um(MakeConfig());
  um.Update();
  EXPECT_FALSE(um.is_dirty());
}

TEST_F(UpdateManagerTest, InvalidateSetsDirty) {
  UpdateManager um(MakeConfig());
  um.Update();
  EXPECT_FALSE(um.is_dirty());
  um.Invalidate();
  EXPECT_TRUE(um.is_dirty());
}

TEST_F(UpdateManagerTest, UpdateGeneratesLayoutRoot) {
  UpdateManager um(MakeConfig());
  um.Update();
  ASSERT_NE(um.layout_root(), nullptr);
}

TEST_F(UpdateManagerTest, UpdateGeneratesDisplayList) {
  UpdateManager um(MakeConfig());
  um.Update();
  EXPECT_FALSE(um.display_list().empty());
}

TEST_F(UpdateManagerTest, NoUpdateWhenClean) {
  UpdateManager um(MakeConfig());
  um.Update();
  auto* root1 = um.layout_root();
  auto list_size1 = um.display_list().size();
  um.Update();
  EXPECT_EQ(um.layout_root(), root1);
  EXPECT_EQ(um.display_list().size(), list_size1);
}

TEST_F(UpdateManagerTest, ReInvalidateRebuilds) {
  UpdateManager um(MakeConfig());
  um.Update();
  auto list_size1 = um.display_list().size();
  um.Invalidate();
  um.Update();
  EXPECT_EQ(um.display_list().size(), list_size1);
  EXPECT_FALSE(um.is_dirty());
}

TEST_F(UpdateManagerTest, EventManagerCallbackIntegration) {
  event::EventManager em;
  UpdateManager um(MakeConfig(&em));
  um.Update();
  EXPECT_FALSE(um.is_dirty());

  auto* root = um.layout_root();
  ASSERT_NE(root, nullptr);

  event::InputEvent move{};
  move.type = event::EventType::kPointerMove;
  move.x = 50;
  move.y = 50;
  em.HandleInput(move, root);

  EXPECT_TRUE(um.is_dirty());
}

TEST_F(UpdateManagerTest, DirtyRectComputedOnUpdate) {
  UpdateManager um(MakeConfig());
  um.Update();

  gfx::Rect first_dirty = um.last_dirty_rect();
  EXPECT_FALSE(first_dirty.IsEmpty());

  um.Invalidate();
  um.Update();

  gfx::Rect second_dirty = um.last_dirty_rect();
  EXPECT_TRUE(second_dirty.IsEmpty());
}

TEST_F(UpdateManagerTest, HoverChangeProducesDirtyRect) {
  sheets_.clear();
  sheets_.push_back(css::CssParser::Parse(
      "#box { width: 100px; height: 100px; background-color: #0000ff; } "
      "#box:hover { background-color: #ff0000; }"));

  event::EventManager em;
  UpdateManager um(MakeConfig(&em));
  um.Update();
  EXPECT_FALSE(um.is_dirty());

  auto* root = um.layout_root();
  ASSERT_NE(root, nullptr);

  event::InputEvent move{};
  move.type = event::EventType::kPointerMove;
  move.x = 50;
  move.y = 50;
  em.HandleInput(move, root);

  EXPECT_TRUE(um.is_dirty());
  um.Update();

  gfx::Rect dirty = um.last_dirty_rect();
  EXPECT_FALSE(dirty.IsEmpty());

  bool found_red = false;
  for (const auto& cmd : um.display_list()) {
    if (cmd.type == render::PaintCommand::Type::kFillRect &&
        cmd.color == render::CssColorToGfx(0xFF0000FFu)) {
      found_red = true;
      break;
    }
  }
  EXPECT_TRUE(found_red);
}

}  // namespace
}  // namespace vx
