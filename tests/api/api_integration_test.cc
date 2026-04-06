#include "veloxa/api/veloxa_api.h"

#include <cstring>

#include <gtest/gtest.h>

#include "veloxa/platform/headless/headless_event_loop.h"
#include "veloxa/platform/headless/memory_surface.h"

namespace {

inline vx::u8 PixelR(vx::u32 px) { return static_cast<vx::u8>(px & 0xFF); }
inline vx::u8 PixelG(vx::u32 px) {
  return static_cast<vx::u8>((px >> 8) & 0xFF);
}
inline vx::u8 PixelB(vx::u32 px) {
  return static_cast<vx::u8>((px >> 16) & 0xFF);
}

class CApiIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    loop_ = vx_event_loop_create_headless();
    surface_ = vx_surface_create_memory(200, 200);
    VxViewConfig config{};
    config.event_loop = loop_;
    config.surface = surface_;
    config.target_fps = 60;
    config.background_color = 0xFFFFFFFF;
    view_ = vx_view_create(&config);
  }

  void TearDown() override {
    vx_view_destroy(view_);
    vx_surface_destroy(surface_);
    vx_event_loop_destroy(loop_);
  }

  VxEventLoop* loop_ = nullptr;
  VxSurface* surface_ = nullptr;
  VxView* view_ = nullptr;
};

TEST_F(CApiIntegrationTest, RenderRedBoxAndVerifyPixels) {
  const char* html = "<div id='box'></div>";
  const char* css =
      "#box { width: 50px; height: 50px; background-color: red; }";
  vx_view_load_html(view_, html, static_cast<uint32_t>(strlen(html)));
  vx_view_load_css(view_, css, static_cast<uint32_t>(strlen(css)));
  vx_view_update(view_);

  auto* mem =
      reinterpret_cast<vx::platform::MemorySurface*>(surface_);
  const vx::u32* pixels = mem->data();
  vx::u32 center = pixels[25 * 200 + 25];

  EXPECT_GT(PixelR(center), 200);
  EXPECT_LT(PixelG(center), 50);
  EXPECT_LT(PixelB(center), 50);
}

TEST_F(CApiIntegrationTest, HoverChangesColor) {
  const char* html = "<div id='box'></div>";
  const char* css =
      "#box { width: 100px; height: 100px; background-color: blue; }"
      "#box:hover { background-color: green; }";
  vx_view_load_html(view_, html, static_cast<uint32_t>(strlen(html)));
  vx_view_load_css(view_, css, static_cast<uint32_t>(strlen(css)));
  vx_view_update(view_);

  auto* mem =
      reinterpret_cast<vx::platform::MemorySurface*>(surface_);
  const vx::u32* pixels = mem->data();
  vx::u32 before = pixels[50 * 200 + 50];
  EXPECT_GT(PixelB(before), 200);

  VxInputEvent move{};
  move.type = VX_EVENT_POINTER_MOVE;
  move.x = 50;
  move.y = 50;
  vx_view_inject_input(view_, &move);
  vx_view_update(view_);

  vx::u32 after = pixels[50 * 200 + 50];
  EXPECT_GT(PixelG(after), 100);
}

TEST_F(CApiIntegrationTest, ReloadHTMLResetsRender) {
  const char* html1 = "<div id='a'></div>";
  const char* css1 = "#a { width: 200px; height: 200px; background-color: red; }";
  vx_view_load_html(view_, html1, static_cast<uint32_t>(strlen(html1)));
  vx_view_load_css(view_, css1, static_cast<uint32_t>(strlen(css1)));
  vx_view_update(view_);

  auto* mem =
      reinterpret_cast<vx::platform::MemorySurface*>(surface_);
  const vx::u32* pixels = mem->data();
  vx::u32 red_px = pixels[100 * 200 + 100];

  const char* html2 = "<div id='b'></div>";
  const char* css2 = "#b { width: 200px; height: 200px; background-color: blue; }";
  vx_view_load_html(view_, html2, static_cast<uint32_t>(strlen(html2)));
  vx_view_load_css(view_, css2, static_cast<uint32_t>(strlen(css2)));
  vx_view_update(view_);

  vx::u32 blue_px = pixels[100 * 200 + 100];
  EXPECT_NE(red_px, blue_px);
}

TEST_F(CApiIntegrationTest, RunQuitLifecycle) {
  const char* html = "<div></div>";
  vx_view_load_html(view_, html, static_cast<uint32_t>(strlen(html)));

  auto* real_loop =
      reinterpret_cast<vx::platform::HeadlessEventLoop*>(loop_);
  real_loop->PostTask([this]() { vx_view_quit(view_); });

  EXPECT_EQ(vx_view_run(view_), VX_OK);
}

TEST_F(CApiIntegrationTest, MultipleInputEvents) {
  const char* html = "<div id='box'></div>";
  const char* css =
      "#box { width: 100px; height: 100px; background-color: blue; }"
      "#box:active { background-color: red; }";
  vx_view_load_html(view_, html, static_cast<uint32_t>(strlen(html)));
  vx_view_load_css(view_, css, static_cast<uint32_t>(strlen(css)));
  vx_view_update(view_);

  VxInputEvent move{};
  move.type = VX_EVENT_POINTER_MOVE;
  move.x = 50;
  move.y = 50;
  vx_view_inject_input(view_, &move);

  VxInputEvent down{};
  down.type = VX_EVENT_POINTER_DOWN;
  down.x = 50;
  down.y = 50;
  vx_view_inject_input(view_, &down);
  vx_view_update(view_);

  auto* mem =
      reinterpret_cast<vx::platform::MemorySurface*>(surface_);
  const vx::u32* pixels = mem->data();
  vx::u32 px = pixels[50 * 200 + 50];
  EXPECT_GT(PixelR(px), 200);
}

}  // namespace
