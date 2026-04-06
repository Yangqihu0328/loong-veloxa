#include "veloxa/core/application.h"

#include <gtest/gtest.h>

#include "veloxa/core/render/render_utils.h"
#include "veloxa/platform/headless/headless_event_loop.h"
#include "veloxa/platform/headless/memory_surface.h"

namespace vx {
namespace {

constexpr u32 kW = 200, kH = 200;

class ApplicationIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    surface_ = std::make_unique<platform::MemorySurface>(kW, kH);
    event_loop_ = std::make_unique<platform::HeadlessEventLoop>();
  }

  Application::Config MakeConfig() {
    Application::Config cfg;
    cfg.event_loop = event_loop_.get();
    cfg.surface = surface_.get();
    cfg.target_fps = 60;
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

  std::unique_ptr<platform::MemorySurface> surface_;
  std::unique_ptr<platform::HeadlessEventLoop> event_loop_;
};

TEST_F(ApplicationIntegrationTest, RenderSimplePage) {
  Application app(MakeConfig());
  app.LoadHTML("<div id='box'></div>");
  app.LoadCSS("#box { width: 100px; height: 100px; background-color: red; }");
  app.Update();

  ASSERT_NE(app.update_manager(), nullptr);
  EXPECT_TRUE(HasFillRectWithColor(app.update_manager()->display_list(),
                                   0xFF0000FFu));
}

TEST_F(ApplicationIntegrationTest, HoverChangesStyle) {
  Application app(MakeConfig());
  app.LoadHTML("<div id='btn'></div>");
  app.LoadCSS(
      "#btn { width: 100px; height: 100px; background-color: green; }"
      "#btn:hover { background-color: yellow; }");
  app.Update();

  EXPECT_TRUE(HasFillRectWithColor(app.update_manager()->display_list(),
                                   0x008000FFu));

  app.InjectInput(PointerMove(50, 50));
  app.Update();

  EXPECT_TRUE(HasFillRectWithColor(app.update_manager()->display_list(),
                                   0xFFFF00FFu));
}

TEST_F(ApplicationIntegrationTest, ActiveChangesStyle) {
  Application app(MakeConfig());
  app.LoadHTML("<div id='btn'></div>");
  app.LoadCSS(
      "#btn { width: 100px; height: 100px; background-color: blue; }"
      "#btn:active { background-color: red; }");
  app.Update();

  EXPECT_TRUE(HasFillRectWithColor(app.update_manager()->display_list(),
                                   0x0000FFFFu));

  app.InjectInput(PointerDown(50, 50));
  app.Update();

  EXPECT_TRUE(HasFillRectWithColor(app.update_manager()->display_list(),
                                   0xFF0000FFu));
}

TEST_F(ApplicationIntegrationTest, RunAndQuit) {
  Application app(MakeConfig());
  app.LoadHTML("<div id='box'></div>");
  app.LoadCSS("#box { width: 50px; height: 50px; background-color: red; }");

  event_loop_->PostTask([&app]() { app.Quit(); });
  app.Run();

  EXPECT_FALSE(event_loop_->is_running());
  ASSERT_NE(app.update_manager(), nullptr);
  EXPECT_FALSE(app.update_manager()->display_list().empty());
}

TEST_F(ApplicationIntegrationTest, MultipleInputsCoalesce) {
  Application app(MakeConfig());
  app.LoadHTML("<div id='a'></div>");
  app.LoadCSS(
      "#a { width: 100px; height: 100px; background-color: green; }"
      "#a:hover { background-color: yellow; }");
  app.Update();

  app.InjectInput(PointerMove(10, 10));
  app.InjectInput(PointerMove(20, 20));
  app.InjectInput(PointerMove(50, 50));
  app.Update();

  EXPECT_TRUE(HasFillRectWithColor(app.update_manager()->display_list(),
                                   0xFFFF00FFu));
}

TEST_F(ApplicationIntegrationTest, FullInteractionSequence) {
  Application app(MakeConfig());
  app.LoadHTML("<div id='btn'></div>");
  app.LoadCSS(
      "#btn { width: 100px; height: 100px; background-color: gray; }"
      "#btn:hover { background-color: green; }"
      "#btn:active { background-color: red; }");
  app.Update();

  EXPECT_TRUE(HasFillRectWithColor(app.update_manager()->display_list(),
                                   0x808080FFu));

  app.InjectInput(PointerMove(50, 50));
  app.Update();
  EXPECT_TRUE(HasFillRectWithColor(app.update_manager()->display_list(),
                                   0x008000FFu));

  app.InjectInput(PointerDown(50, 50));
  app.Update();
  EXPECT_TRUE(HasFillRectWithColor(app.update_manager()->display_list(),
                                   0xFF0000FFu));

  app.InjectInput(PointerUp(50, 50));
  app.Update();
  EXPECT_TRUE(HasFillRectWithColor(app.update_manager()->display_list(),
                                   0x008000FFu));

  app.InjectInput(PointerMove(999, 999));
  app.Update();
  EXPECT_TRUE(HasFillRectWithColor(app.update_manager()->display_list(),
                                   0x808080FFu));
}

}  // namespace
}  // namespace vx
