#include <gtest/gtest.h>

#include "veloxa/core/css/parser.h"
#include "veloxa/core/css/transition.h"
#include "veloxa/core/dom/document.h"
#include "veloxa/core/dom/tag.h"
#include "veloxa/core/event/event_manager.h"
#include "veloxa/core/event/event_types.h"
#include "veloxa/core/html/parser.h"
#include "veloxa/core/render/renderer.h"
#include "veloxa/core/update_manager.h"
#include "veloxa/foundation/memory/arena_allocator.h"
#include "veloxa/graphics/software/software_canvas.h"

namespace vx {
namespace {

static dom::Element* FindElement(dom::Element* parent, dom::TagId tag) {
  for (auto* child = parent->first_child(); child;
       child = child->next_sibling()) {
    if (child->is_element()) {
      auto* el = static_cast<dom::Element*>(child);
      if (el->tag_id() == tag) return el;
      auto* found = FindElement(el, tag);
      if (found) return found;
    }
  }
  return nullptr;
}

static event::InputEvent PointerMove(f32 x, f32 y) {
  event::InputEvent ev{};
  ev.type = event::EventType::kPointerMove;
  ev.x = x;
  ev.y = y;
  return ev;
}

class TransitionIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    canvas_ = std::make_unique<gfx::sw::SoftwareCanvas>(pixels_, kW, kH, kW);
    canvas_->Begin();
    canvas_->Clear(gfx::Color::White());
  }

  void LoadAndSetup(const char* html_str, const char* css_str) {
    doc_ = html::Parser::Parse(html_str);
    auto sheet = css::CssParser::Parse(css_str);
    sheets_.push_back(std::move(sheet));

    event_mgr_ = std::make_unique<event::EventManager>();

    UpdateManager::Config cfg;
    cfg.document = doc_;
    cfg.stylesheets = &sheets_;
    cfg.layout_context.viewport_width = static_cast<f32>(kW);
    cfg.layout_context.viewport_height = static_cast<f32>(kH);
    cfg.canvas = canvas_.get();
    cfg.event_manager = event_mgr_.get();
    update_mgr_ = std::make_unique<UpdateManager>(cfg);
  }

  void TearDown() override {
    update_mgr_.reset();
    event_mgr_.reset();
    delete doc_;
  }

  static constexpr u32 kW = 200, kH = 200;
  u32 pixels_[kW * kH] = {};
  std::unique_ptr<gfx::sw::SoftwareCanvas> canvas_;
  dom::Document* doc_ = nullptr;
  Vector<css::Stylesheet> sheets_;
  std::unique_ptr<event::EventManager> event_mgr_;
  std::unique_ptr<UpdateManager> update_mgr_;
};

TEST_F(TransitionIntegrationTest, TransitionStartsAfterHover) {
  LoadAndSetup(
      "<div id=\"box\">X</div>",
      "#box { background-color: red; width: 100px; height: 100px; "
      "transition: background-color 500ms linear; }"
      "#box:hover { background-color: blue; }");

  update_mgr_->Update();
  EXPECT_FALSE(update_mgr_->transition_manager().HasActive());

  event_mgr_->HandleInput(PointerMove(50, 50), update_mgr_->layout_root());
  update_mgr_->Update();
  EXPECT_TRUE(update_mgr_->transition_manager().HasActive());
}

TEST_F(TransitionIntegrationTest, NoTransitionWithoutDeclaration) {
  LoadAndSetup(
      "<div id=\"box\">X</div>",
      "#box { background-color: red; width: 100px; height: 100px; }"
      "#box:hover { background-color: blue; }");

  update_mgr_->Update();

  event_mgr_->HandleInput(PointerMove(50, 50), update_mgr_->layout_root());
  update_mgr_->Update();
  EXPECT_FALSE(update_mgr_->transition_manager().HasActive());
}

TEST_F(TransitionIntegrationTest, TransitionKeepsDirtyFlag) {
  LoadAndSetup(
      "<div id=\"box\">X</div>",
      "#box { background-color: red; width: 50px; height: 50px; "
      "transition: background-color 1000ms linear; }"
      "#box:hover { background-color: blue; }");

  update_mgr_->Update();
  EXPECT_FALSE(update_mgr_->is_dirty());

  event_mgr_->HandleInput(PointerMove(25, 25), update_mgr_->layout_root());
  update_mgr_->Update();

  EXPECT_TRUE(update_mgr_->is_dirty());
}

}  // namespace
}  // namespace vx
