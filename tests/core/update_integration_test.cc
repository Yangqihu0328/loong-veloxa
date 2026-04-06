#include "veloxa/core/update_manager.h"

#include <cstring>

#include <gtest/gtest.h>

#include "veloxa/core/css/parser.h"
#include "veloxa/core/html/parser.h"
#include "veloxa/core/layout/text_shaper.h"
#include "veloxa/core/render/render_utils.h"
#include "veloxa/graphics/software/software_canvas.h"

namespace vx {
namespace {

constexpr u32 kW = 200, kH = 200, kStride = kW * 4;

class UpdateIntegrationTest : public ::testing::Test {
 protected:
  void SetUpWithHtml(StringView html_str, StringView css_str) {
    doc_ = html::Parser::Parse(html_str);
    sheets_.push_back(css::CssParser::Parse(css_str));
    std::memset(pixels_, 0, sizeof(pixels_));
    canvas_ = std::make_unique<gfx::sw::SoftwareCanvas>(pixels_, kW, kH,
                                                        kStride);
    canvas_->Begin();
    canvas_->Clear(gfx::Color::White());
  }

  UpdateManager::Config MakeConfig(event::EventManager* em = nullptr) {
    UpdateManager::Config cfg;
    cfg.document = doc_;
    cfg.stylesheets = &sheets_;
    cfg.layout_context.text_shaper = &shaper_;
    cfg.layout_context.viewport_width = kW;
    cfg.layout_context.viewport_height = kH;
    cfg.canvas = canvas_.get();
    cfg.event_manager = em;
    return cfg;
  }

  event::InputEvent PointerMove(f32 x, f32 y) {
    event::InputEvent e{};
    e.type = event::EventType::kPointerMove;
    e.x = x;
    e.y = y;
    return e;
  }

  event::InputEvent PointerDown(f32 x, f32 y) {
    event::InputEvent e{};
    e.type = event::EventType::kPointerDown;
    e.x = x;
    e.y = y;
    return e;
  }

  event::InputEvent PointerUp(f32 x, f32 y) {
    event::InputEvent e{};
    e.type = event::EventType::kPointerUp;
    e.x = x;
    e.y = y;
    return e;
  }

  bool HasFillRectWithColor(const render::DisplayList& list, u32 css_color) {
    gfx::Color c = render::CssColorToGfx(css_color);
    for (const auto& cmd : list) {
      if (cmd.type == render::PaintCommand::Type::kFillRect &&
          cmd.color == c) {
        return true;
      }
    }
    return false;
  }

  dom::Document* doc_ = nullptr;
  Vector<css::Stylesheet> sheets_;
  layout::SimpleTextShaper shaper_;
  u32 pixels_[kW * kH];
  std::unique_ptr<gfx::sw::SoftwareCanvas> canvas_;
};

TEST_F(UpdateIntegrationTest, InitialUpdateRendersContent) {
  SetUpWithHtml("<div id='a'></div>",
                "#a { width: 50px; height: 50px; background-color: #0000ff; }");
  UpdateManager um(MakeConfig());
  um.Update();

  EXPECT_FALSE(um.is_dirty());
  EXPECT_NE(um.layout_root(), nullptr);
  EXPECT_TRUE(HasFillRectWithColor(um.display_list(), 0x0000FFFFu));
}

TEST_F(UpdateIntegrationTest, HoverChangeTriggersDirtyRect) {
  SetUpWithHtml(
      "<div id='btn'></div>",
      "#btn { width: 80px; height: 40px; background-color: #cccccc; } "
      "#btn:hover { background-color: #ff0000; }");

  event::EventManager em;
  UpdateManager um(MakeConfig(&em));
  um.Update();
  EXPECT_FALSE(um.is_dirty());

  em.HandleInput(PointerMove(40, 20), um.layout_root());
  EXPECT_TRUE(um.is_dirty());

  um.Update();
  gfx::Rect dirty = um.last_dirty_rect();
  EXPECT_FALSE(dirty.IsEmpty());
}

TEST_F(UpdateIntegrationTest, HoverChangeUpdatesBackgroundColor) {
  SetUpWithHtml(
      "<div id='btn'></div>",
      "#btn { width: 80px; height: 40px; background-color: #0000ff; } "
      "#btn:hover { background-color: #ff0000; }");

  event::EventManager em;
  UpdateManager um(MakeConfig(&em));
  um.Update();

  EXPECT_TRUE(HasFillRectWithColor(um.display_list(), 0x0000FFFFu));
  EXPECT_FALSE(HasFillRectWithColor(um.display_list(), 0xFF0000FFu));

  em.HandleInput(PointerMove(40, 20), um.layout_root());
  um.Update();

  EXPECT_TRUE(HasFillRectWithColor(um.display_list(), 0xFF0000FFu));
}

TEST_F(UpdateIntegrationTest, ActiveStateChangesOnPointerDown) {
  SetUpWithHtml(
      "<div id='btn'></div>",
      "#btn { width: 80px; height: 40px; background-color: #aaaaaa; } "
      "#btn:active { background-color: #00ff00; }");

  event::EventManager em;
  UpdateManager um(MakeConfig(&em));
  um.Update();

  em.HandleInput(PointerDown(40, 20), um.layout_root());
  um.Update();

  EXPECT_TRUE(HasFillRectWithColor(um.display_list(), 0x00FF00FFu));
}

TEST_F(UpdateIntegrationTest, FocusChangeOnPointerDown) {
  SetUpWithHtml(
      "<div id='inp'></div>",
      "#inp { width: 80px; height: 40px; background-color: #aaaaaa; } "
      "#inp:focus { background-color: #0000ff; }");

  event::EventManager em;
  UpdateManager um(MakeConfig(&em));
  um.Update();

  em.HandleInput(PointerDown(40, 20), um.layout_root());
  um.Update();

  EXPECT_TRUE(HasFillRectWithColor(um.display_list(), 0x0000FFFFu));
}

TEST_F(UpdateIntegrationTest, NoUpdateWhenHoverUnchanged) {
  SetUpWithHtml(
      "<div id='btn'></div>",
      "#btn { width: 80px; height: 40px; background-color: #0000ff; } "
      "#btn:hover { background-color: #ff0000; }");

  event::EventManager em;
  UpdateManager um(MakeConfig(&em));
  um.Update();

  em.HandleInput(PointerMove(40, 20), um.layout_root());
  um.Update();
  EXPECT_FALSE(um.is_dirty());

  em.HandleInput(PointerMove(41, 21), um.layout_root());
  EXPECT_FALSE(um.is_dirty());
}

TEST_F(UpdateIntegrationTest, MultipleInvalidatesCoalesced) {
  SetUpWithHtml(
      "<div id='btn'></div>",
      "#btn { width: 80px; height: 40px; background-color: #0000ff; } "
      "#btn:hover { background-color: #ff0000; } "
      "#btn:active { background-color: #00ff00; }");

  event::EventManager em;
  UpdateManager um(MakeConfig(&em));
  um.Update();

  em.HandleInput(PointerMove(40, 20), um.layout_root());
  em.HandleInput(PointerDown(40, 20), um.layout_root());
  EXPECT_TRUE(um.is_dirty());

  um.Update();
  EXPECT_FALSE(um.is_dirty());
  EXPECT_TRUE(HasFillRectWithColor(um.display_list(), 0x00FF00FFu));
}

TEST_F(UpdateIntegrationTest, DirtyRectMatchesChangedRegion) {
  SetUpWithHtml(
      "<div id='btn'></div>",
      "#btn { width: 80px; height: 40px; background-color: #0000ff; } "
      "#btn:hover { background-color: #ff0000; }");

  event::EventManager em;
  UpdateManager um(MakeConfig(&em));
  um.Update();

  em.HandleInput(PointerMove(40, 20), um.layout_root());
  um.Update();

  gfx::Rect dirty = um.last_dirty_rect();
  EXPECT_GT(dirty.w, 0);
  EXPECT_GT(dirty.h, 0);
  EXPECT_LE(dirty.w, kW);
  EXPECT_LE(dirty.h, kH);
}

}  // namespace
}  // namespace vx
