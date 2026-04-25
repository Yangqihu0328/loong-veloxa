/*
 * Veloxa — Hello SDL2 Example
 *
 * Renders the same colored-boxes scene as hello.cc but inside a real SDL2
 * window. Demonstrates:
 *   - vx_event_loop_create_sdl2()
 *   - vx_surface_create_window()
 *   - vx_view_run() driving the SDL2 event loop with auto-wired input
 *     forwarding. Closing the window (SDL_QUIT) shuts the loop down.
 *
 * Build:  cmake -B build -DVX_PLATFORM_SDL2=ON
 *         cmake --build build --target hello_sdl2
 * Run:    ./build/examples/hello_sdl2
 */

#include "veloxa/api/veloxa_api.h"

#include <SDL2/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

static const char kHTML[] =
    "<div id='page'>"
    "  <div id='header'>Veloxa SDL2</div>"
    "  <div id='content'>"
    "    <div id='box-red'></div>"
    "    <div id='box-green'></div>"
    "    <div id='box-blue'></div>"
    "  </div>"
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
    /* :hover variants — enable end-to-end input verification (plan A2):
       moving the mouse over a box should flip its color, proving SDL events
       reach the DOM event manager via vx_view_inject_input. */
    "#box-red:hover   { background-color: #FF6B6B; }"
    "#box-green:hover { background-color: #6BCB77; }"
    "#box-blue:hover  { background-color: #4D96FF; }"
    "#footer { height: 24px; background-color: #2C3E50; }";

int main() {
  std::printf("Veloxa %s — Hello SDL2 Example\n", vx_version());

  VxEventLoop* loop = vx_event_loop_create_sdl2();
  if (!loop) {
    std::fprintf(stderr,
                 "ERROR: SDL2 event loop unavailable (rebuild with "
                 "-DVX_PLATFORM_SDL2=ON).\n");
    return 1;
  }

  VxWindowOptions wopts{};
  wopts.width = 480;
  wopts.height = 360;
  wopts.title = "Veloxa Hello SDL2";
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

  VxResult res = vx_view_load_html(view, kHTML, (uint32_t)std::strlen(kHTML));
  if (res != VX_OK) {
    std::fprintf(stderr, "ERROR: load_html failed (%d)\n", res);
    return 1;
  }
  res = vx_view_load_css(view, kCSS, (uint32_t)std::strlen(kCSS));
  if (res != VX_OK) {
    std::fprintf(stderr, "ERROR: load_css failed (%d)\n", res);
    return 1;
  }

  std::printf("Window opened. Close the window to exit.\n");

  // Test hook: VX_HELLO_SDL2_AUTOQUIT_MS=N posts SDL_QUIT after N ms so
  // the smoke test (ctest, SDL_VIDEODRIVER=dummy) can run without a human.
  // Production users never set this.
  if (const char* env = std::getenv("VX_HELLO_SDL2_AUTOQUIT_MS")) {
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

  vx_view_destroy(view);
  vx_surface_destroy(surface);
  vx_event_loop_destroy(loop);
  std::printf("Done.\n");
  return 0;
}
