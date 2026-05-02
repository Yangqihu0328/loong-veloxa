/*
 * Veloxa — Hello DevTool Example (TASK-20260502-01 A.3.1)
 *
 * Same scene as hello_sdl2.cc but with the DevTool Inspector dogfood UI
 * attached on the right-hand splitter dock. Demonstrates:
 *   - vx_view_attach_devtool() with explicit options
 *   - VxDevtoolOptions { devtool_width, enable_f12_hotkey } configuration
 *   - F12 hotkey toggle (live attach/detach without reload)
 *   - vx_inspector_set_redaction_policy() T3 policy switch
 *   - SDL2 input forwarding to BOTH target Document and DevTool Document
 *     via Application::InjectInput's hotkey interceptor (A.1.7)
 *
 * Build:  cmake -B build -DVX_PLATFORM_SDL2=ON -DVX_BUILD_DEVTOOL=ON
 *         cmake --build build --target hello_devtool
 * Run:    ./build/examples/hello_devtool
 *         (Press F12 to toggle the DevTool panel.)
 */

#include "veloxa/api/veloxa_api.h"

#include <SDL2/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>

static const char kHTML[] =
    "<div id='page'>"
    "  <div id='header'>Veloxa DevTool</div>"
    "  <div id='content'>"
    "    <div id='box-red'></div>"
    "    <div id='box-green'></div>"
    "    <div id='box-blue'></div>"
    "  </div>"
    /* Sensitive sub-tree to demonstrate T3 redaction by default. */
    "  <input id='login' type='password' value='do-not-leak'>"
    "  <div id='footer'></div>"
    "</div>";

static const char kCSS[] =
    "#page { width: 480px; background-color: white; }"
    "#header { height: 50px; background-color: #2C3E50; }"
    "#content {"
    "  display: flex;"
    "  justify-content: center;"
    "  gap: 24px;"
    "  padding: 32px;"
    "  background-color: #ECF0F1;"
    "}"
    "#box-red   { width: 96px; height: 96px; background-color: #E74C3C; }"
    "#box-green { width: 96px; height: 96px; background-color: #2ECC71; }"
    "#box-blue  { width: 96px; height: 96px; background-color: #3498DB; }"
    "#footer { height: 24px; background-color: #2C3E50; }"
    "#login { display: block; }";

int main() {
  std::printf("Veloxa %s — Hello DevTool Example\n", vx_version());

  VxEventLoop* loop = vx_event_loop_create_sdl2();
  if (!loop) {
    std::fprintf(stderr,
                 "ERROR: SDL2 event loop unavailable (rebuild with "
                 "-DVX_PLATFORM_SDL2=ON).\n");
    return 1;
  }

  VxWindowOptions wopts{};
  wopts.width = 800;
  wopts.height = 480;
  wopts.title = "Veloxa Hello DevTool — F12 to toggle";
  VxSurface* surface = vx_surface_create_window(&wopts);
  if (!surface) {
    std::fprintf(stderr, "ERROR: failed to create SDL2 window: %s\n",
                 SDL_GetError());
    vx_event_loop_destroy(loop);
    return 1;
  }

  VxViewConfig cfg{};
  cfg.event_loop = loop;
  cfg.surface = surface;
  cfg.target_fps = 60;
  cfg.background_color = 0xFFFFFFFF;

  VxView* view = vx_view_create(&cfg);
  if (!view) {
    std::fprintf(stderr, "ERROR: failed to create view\n");
    vx_surface_destroy(surface);
    vx_event_loop_destroy(loop);
    return 1;
  }

  if (vx_view_load_html(view, kHTML, (uint32_t)std::strlen(kHTML)) != VX_OK ||
      vx_view_load_css(view, kCSS, (uint32_t)std::strlen(kCSS)) != VX_OK) {
    std::fprintf(stderr, "ERROR: load_html/css failed\n");
    return 1;
  }

  /* Attach DevTool dogfood UI on the right-hand 270px dock with F12
   * hotkey enabled. Production embedders typically attach unconditionally
   * during development and ship a build with VX_BUILD_DEVTOOL=OFF. */
  VxDevtoolOptions opts{};
  opts.devtool_width = 270;
  opts.enable_f12_hotkey = 1;
  if (vx_view_attach_devtool(view, &opts) != VX_OK) {
    std::fprintf(stderr,
                 "ERROR: vx_view_attach_devtool failed (rebuild with "
                 "-DVX_BUILD_DEVTOOL=ON).\n");
    vx_view_destroy(view);
    vx_surface_destroy(surface);
    vx_event_loop_destroy(loop);
    return 1;
  }
  std::printf("DevTool attached on right-hand dock (width=%u, F12 to toggle).\n",
              opts.devtool_width);

  /* T3 default policy is REDACT_SENSITIVE — verify by serializing once. */
  char json_buf[64 * 1024];
  uint32_t json_len = sizeof(json_buf);
  if (vx_view_serialize_dom_json(view, json_buf, &json_len, sizeof(json_buf))
          == VX_OK) {
    std::string_view sv(json_buf, json_len);
    if (sv.find("do-not-leak") == std::string_view::npos &&
        sv.find("[REDACTED]") != std::string_view::npos) {
      std::printf("T3 redaction OK: password value masked in DOM JSON.\n");
    } else {
      std::fprintf(stderr,
                   "WARNING: T3 redaction did not mask password value.\n");
    }
  }

  std::printf("Window opened. Press F12 to toggle DevTool.\n");

  /* Same auto-quit hook as hello_sdl2 for the headless smoke test. */
  if (const char* env = std::getenv("VX_HELLO_DEVTOOL_AUTOQUIT_MS")) {
    int ms = std::atoi(env);
    if (ms > 0) {
      SDL_InitSubSystem(SDL_INIT_TIMER);
      SDL_AddTimer(
          (Uint32)ms,
          [](Uint32, void*) -> Uint32 {
            SDL_Event ev{};
            ev.type = SDL_QUIT;
            SDL_PushEvent(&ev);
            return 0;
          },
          nullptr);
    }
  }

  vx_view_run(view);

  vx_view_detach_devtool(view);
  vx_view_destroy(view);
  vx_surface_destroy(surface);
  vx_event_loop_destroy(loop);
  std::printf("Done.\n");
  return 0;
}
