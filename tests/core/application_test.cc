#include "veloxa/core/application.h"

#include <gtest/gtest.h>

#include "veloxa/core/render/render_utils.h"
#include "veloxa/platform/headless/headless_event_loop.h"
#include "veloxa/platform/headless/memory_surface.h"

namespace vx {
namespace {

constexpr u32 kW = 200, kH = 200;

class ApplicationTest : public ::testing::Test {
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

  std::unique_ptr<platform::MemorySurface> surface_;
  std::unique_ptr<platform::HeadlessEventLoop> event_loop_;
};

TEST_F(ApplicationTest, ConstructWithSurface) {
  Application app(MakeConfig());
  EXPECT_EQ(app.document(), nullptr);
  EXPECT_EQ(app.update_manager(), nullptr);
}

TEST_F(ApplicationTest, LoadHTMLCreatesDocument) {
  Application app(MakeConfig());
  app.LoadHTML("<div>hello</div>");
  EXPECT_NE(app.document(), nullptr);
}

TEST_F(ApplicationTest, LoadHTMLReplacesDocument) {
  Application app(MakeConfig());
  app.LoadHTML("<div id='first'></div>");
  app.LoadCSS("#first { width: 50px; height: 50px; background-color: red; }");
  app.Update();
  ASSERT_NE(app.update_manager(), nullptr);
  EXPECT_FALSE(app.update_manager()->display_list().empty());

  app.LoadHTML("<p id='second'></p>");
  EXPECT_NE(app.document(), nullptr);
  EXPECT_EQ(app.update_manager(), nullptr);
  app.Update();
  ASSERT_NE(app.update_manager(), nullptr);
}

TEST_F(ApplicationTest, LoadCSSAndUpdate) {
  Application app(MakeConfig());
  app.LoadHTML("<div id='box'>text</div>");
  app.LoadCSS("#box { width: 50px; height: 50px; background-color: red; }");
  app.Update();
  ASSERT_NE(app.update_manager(), nullptr);
  EXPECT_FALSE(app.update_manager()->display_list().empty());
}

TEST_F(ApplicationTest, UpdateWithoutDocument) {
  Application app(MakeConfig());
  app.Update();
  EXPECT_EQ(app.update_manager(), nullptr);
}

TEST_F(ApplicationTest, UpdateRendersContent) {
  Application app(MakeConfig());
  app.LoadHTML("<div id='box'></div>");
  app.LoadCSS("#box { width: 100px; height: 100px; background-color: blue; }");
  app.Update();

  ASSERT_NE(app.update_manager(), nullptr);
  const auto& list = app.update_manager()->display_list();
  bool found_blue = false;
  gfx::Color blue = render::CssColorToGfx(0x0000FFFFu);
  for (const auto& cmd : list) {
    if (cmd.type == render::PaintCommand::Type::kFillRect &&
        cmd.color == blue) {
      found_blue = true;
      break;
    }
  }
  EXPECT_TRUE(found_blue);
}

TEST_F(ApplicationTest, InjectInputUpdatesHoverState) {
  Application app(MakeConfig());
  app.LoadHTML("<div id='btn'></div>");
  app.LoadCSS("#btn { width: 100px; height: 100px; background-color: green; }");
  app.Update();

  app.InjectInput(PointerMove(50, 50));
  EXPECT_NE(app.event_manager().hovered_element(), nullptr);
}

TEST_F(ApplicationTest, QuitStopsEventLoop) {
  Application app(MakeConfig());
  app.LoadHTML("<div></div>");

  event_loop_->PostTask([&app]() { app.Quit(); });
  app.Run();
  EXPECT_FALSE(event_loop_->is_running());
}

TEST_F(ApplicationTest, FrameTimerDrivesUpdate) {
  Application app(MakeConfig());
  app.LoadHTML("<div id='box'></div>");
  app.LoadCSS("#box { width: 50px; height: 50px; background-color: red; }");

  int frame_count = 0;
  event_loop_->PostTask([&]() {
    frame_count++;
    event_loop_->PollOnce();
    frame_count++;
    app.Quit();
  });
  app.Run();
  EXPECT_GE(frame_count, 2);
  EXPECT_NE(app.update_manager(), nullptr);
}

TEST_F(ApplicationTest, MultipleCSSSheets) {
  Application app(MakeConfig());
  app.LoadHTML("<div id='a'></div><div id='b'></div>");
  app.LoadCSS("#a { width: 50px; height: 50px; background-color: red; }");
  app.LoadCSS("#b { width: 50px; height: 50px; background-color: blue; }");
  app.Update();

  const auto& list = app.update_manager()->display_list();
  gfx::Color red = render::CssColorToGfx(0xFF0000FFu);
  gfx::Color blue = render::CssColorToGfx(0x0000FFFFu);
  bool found_red = false, found_blue = false;
  for (const auto& cmd : list) {
    if (cmd.type == render::PaintCommand::Type::kFillRect) {
      if (cmd.color == red) found_red = true;
      if (cmd.color == blue) found_blue = true;
    }
  }
  EXPECT_TRUE(found_red);
  EXPECT_TRUE(found_blue);
}

TEST_F(ApplicationTest, LoadFontAndRenderText) {
  Application app(MakeConfig());

  auto status = app.LoadFont(
      "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", "DejaVuSans");
  ASSERT_TRUE(status.ok());

  app.LoadHTML("<div id=\"msg\">Hello FreeType</div>");
  app.LoadCSS("#msg { color: black; font-size: 20px; }");
  app.Update();

  const u32* data = surface_->data();
  bool has_non_white = false;
  u32 white = gfx::Color::White().ToRGBA32();
  for (u32 i = 0; i < kW * kH; ++i) {
    if (data[i] != white && data[i] != 0) {
      has_non_white = true;
      break;
    }
  }
  EXPECT_TRUE(has_non_white) << "Expected text pixels after FreeType rendering";
}

}  // namespace
}  // namespace vx
