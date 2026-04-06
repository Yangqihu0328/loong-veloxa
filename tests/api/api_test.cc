#include "veloxa/api/veloxa_api.h"

#include <cstring>
#include <cstdio>

#include <gtest/gtest.h>

#include "veloxa/platform/headless/headless_event_loop.h"

namespace {

TEST(CApiTest, Version) {
  const char* ver = vx_version();
  ASSERT_NE(ver, nullptr);
  EXPECT_STREQ(ver, "0.1.0");
}

TEST(CApiTest, CreateDestroyEventLoop) {
  VxEventLoop* loop = vx_event_loop_create_headless();
  ASSERT_NE(loop, nullptr);
  vx_event_loop_destroy(loop);
}

TEST(CApiTest, CreateDestroySurface) {
  VxSurface* surface = vx_surface_create_memory(200, 200);
  ASSERT_NE(surface, nullptr);
  vx_surface_destroy(surface);
}

TEST(CApiTest, CreateDestroyView) {
  VxEventLoop* loop = vx_event_loop_create_headless();
  VxSurface* surface = vx_surface_create_memory(200, 200);

  VxViewConfig config{};
  config.event_loop = loop;
  config.surface = surface;
  config.target_fps = 60;
  config.background_color = 0xFFFFFFFF;

  VxView* view = vx_view_create(&config);
  ASSERT_NE(view, nullptr);

  vx_view_destroy(view);
  vx_surface_destroy(surface);
  vx_event_loop_destroy(loop);
}

TEST(CApiTest, LoadHTMLAndUpdate) {
  VxEventLoop* loop = vx_event_loop_create_headless();
  VxSurface* surface = vx_surface_create_memory(200, 200);
  VxViewConfig config{};
  config.event_loop = loop;
  config.surface = surface;
  config.target_fps = 60;

  VxView* view = vx_view_create(&config);
  const char* html = "<div id='box'></div>";
  EXPECT_EQ(vx_view_load_html(view, html, static_cast<uint32_t>(strlen(html))),
            VX_OK);
  EXPECT_EQ(vx_view_update(view), VX_OK);

  vx_view_destroy(view);
  vx_surface_destroy(surface);
  vx_event_loop_destroy(loop);
}

TEST(CApiTest, LoadCSSAndUpdate) {
  VxEventLoop* loop = vx_event_loop_create_headless();
  VxSurface* surface = vx_surface_create_memory(200, 200);
  VxViewConfig config{};
  config.event_loop = loop;
  config.surface = surface;
  config.target_fps = 60;

  VxView* view = vx_view_create(&config);
  const char* html = "<div id='box'></div>";
  const char* css = "#box { width: 50px; height: 50px; background-color: red; }";
  EXPECT_EQ(vx_view_load_html(view, html, static_cast<uint32_t>(strlen(html))),
            VX_OK);
  EXPECT_EQ(vx_view_load_css(view, css, static_cast<uint32_t>(strlen(css))),
            VX_OK);
  EXPECT_EQ(vx_view_update(view), VX_OK);

  vx_view_destroy(view);
  vx_surface_destroy(surface);
  vx_event_loop_destroy(loop);
}

TEST(CApiTest, InjectInput) {
  VxEventLoop* loop = vx_event_loop_create_headless();
  VxSurface* surface = vx_surface_create_memory(200, 200);
  VxViewConfig config{};
  config.event_loop = loop;
  config.surface = surface;
  config.target_fps = 60;

  VxView* view = vx_view_create(&config);
  const char* html = "<div id='box'></div>";
  const char* css = "#box { width: 100px; height: 100px; background-color: red; }";
  vx_view_load_html(view, html, static_cast<uint32_t>(strlen(html)));
  vx_view_load_css(view, css, static_cast<uint32_t>(strlen(css)));
  vx_view_update(view);

  VxInputEvent event{};
  event.type = VX_EVENT_POINTER_MOVE;
  event.x = 50;
  event.y = 50;
  EXPECT_EQ(vx_view_inject_input(view, &event), VX_OK);

  vx_view_destroy(view);
  vx_surface_destroy(surface);
  vx_event_loop_destroy(loop);
}

TEST(CApiTest, RunAndQuit) {
  VxEventLoop* loop = vx_event_loop_create_headless();
  VxSurface* surface = vx_surface_create_memory(200, 200);
  VxViewConfig config{};
  config.event_loop = loop;
  config.surface = surface;
  config.target_fps = 60;

  VxView* view = vx_view_create(&config);
  const char* html = "<div></div>";
  vx_view_load_html(view, html, static_cast<uint32_t>(strlen(html)));

  // Post quit task before run
  auto* real_loop = reinterpret_cast<vx::platform::HeadlessEventLoop*>(loop);
  real_loop->PostTask([view]() { vx_view_quit(view); });

  EXPECT_EQ(vx_view_run(view), VX_OK);

  vx_view_destroy(view);
  vx_surface_destroy(surface);
  vx_event_loop_destroy(loop);
}

TEST(CApiTest, NullParamReturnsError) {
  EXPECT_EQ(vx_view_create(nullptr), nullptr);
  EXPECT_EQ(vx_view_load_html(nullptr, "x", 1), VX_ERROR_NULL_PARAM);
  EXPECT_EQ(vx_view_load_css(nullptr, "x", 1), VX_ERROR_NULL_PARAM);
  EXPECT_EQ(vx_view_inject_input(nullptr, nullptr), VX_ERROR_NULL_PARAM);
  EXPECT_EQ(vx_view_update(nullptr), VX_ERROR_NULL_PARAM);
  EXPECT_EQ(vx_view_run(nullptr), VX_ERROR_NULL_PARAM);
  EXPECT_EQ(vx_view_quit(nullptr), VX_ERROR_NULL_PARAM);
  EXPECT_EQ(vx_surface_save_ppm(nullptr, "x"), VX_ERROR_NULL_PARAM);
}

TEST(CApiTest, SurfaceSavePPM) {
  VxEventLoop* loop = vx_event_loop_create_headless();
  VxSurface* surface = vx_surface_create_memory(50, 50);
  VxViewConfig config{};
  config.event_loop = loop;
  config.surface = surface;
  config.target_fps = 60;

  VxView* view = vx_view_create(&config);
  const char* html = "<div id='box'></div>";
  const char* css = "#box { width: 50px; height: 50px; background-color: red; }";
  vx_view_load_html(view, html, static_cast<uint32_t>(strlen(html)));
  vx_view_load_css(view, css, static_cast<uint32_t>(strlen(css)));
  vx_view_update(view);

  vx_view_destroy(view);

  char path[128];
  snprintf(path, sizeof(path), "/tmp/vx_capi_test_%d.ppm", getpid());
  EXPECT_EQ(vx_surface_save_ppm(surface, path), VX_OK);
  remove(path);

  vx_surface_destroy(surface);
  vx_event_loop_destroy(loop);
}

TEST(CApiTest, UpdateBeforeLoadHTML) {
  VxEventLoop* loop = vx_event_loop_create_headless();
  VxSurface* surface = vx_surface_create_memory(100, 100);
  VxViewConfig config{};
  config.event_loop = loop;
  config.surface = surface;
  config.target_fps = 60;

  VxView* view = vx_view_create(&config);
  EXPECT_EQ(vx_view_update(view), VX_OK);

  vx_view_destroy(view);
  vx_surface_destroy(surface);
  vx_event_loop_destroy(loop);
}

}  // namespace
