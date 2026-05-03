/*
 * Veloxa — Hello DevTool Example (TASK-20260502-01 A.3.1 +
 *                                  TASK-20260502-02 B.3.2 perf smoke)
 *
 * Same scene as hello_sdl2.cc but with the DevTool Inspector dogfood UI
 * attached on the right-hand splitter dock. Demonstrates:
 *   - vx_view_attach_devtool() with explicit options
 *   - VxDevtoolOptions { devtool_width, enable_f12_hotkey } configuration
 *   - F12 hotkey toggle (live attach/detach without reload)
 *   - F11 hotkey toggle HUD (B.3.1 — vx_view_is_hud_visible C API)
 *   - vx_view_set_pipeline_hooks() Performance Overlay smoke (B.3.2)
 *   - vx_inspector_set_redaction_policy() T3 policy switch
 *   - SDL2 input forwarding to BOTH target Document and DevTool Document
 *     via Application::InjectInput's hotkey interceptor (A.1.7)
 *
 * Build:  cmake -B build -DVX_PLATFORM_SDL2=ON -DVX_BUILD_DEVTOOL=ON
 *         cmake --build build --target hello_devtool
 * Run:    ./build/examples/hello_devtool
 *         (Press F11 to toggle HUD; F12 to toggle DevTool.)
 */

#include "veloxa/api/veloxa_api.h"

#include <SDL2/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
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

  /* TASK-20260503-01 C.4.2 — Hot Reload smoke setup. Driven by env var
   * VX_HELLO_DEVTOOL_HOT_RELOAD_TEST=1 so the production embedder use
   * case (no hot reload, no temp dir) stays the default. The smoke
   * scenario:
   *   1. mkdtemp /tmp/vx_hr_smoke_XXXXXX/
   *   2. write style.css with an initial body { background: red }
   *   3. attach DevTool with hot_reload_dir = the temp dir
   *   4. SDL timer at +100ms rewrites style.css to body { background: blue }
   *   5. inotify worker debounces (50ms) → main-thread DrainEvents picks
   *      up next frame → Application::LoadCSS → tracked_count++
   *   6. autoquit (caller-controlled, default 500ms in the smoke test)
   *      then print "HOT RELOAD: triggered count=N" — ctest regex matches.
   * cleanup: filesystem::remove_all on the temp dir before returning.
   */
  static char s_hr_dir[256] = {0};
  static char s_hr_css_path[512] = {0};
  bool hot_reload_test = false;
  if (const char* env = std::getenv("VX_HELLO_DEVTOOL_HOT_RELOAD_TEST")) {
    if (env[0] == '1') hot_reload_test = true;
  }

  if (hot_reload_test) {
    std::strncpy(s_hr_dir, "/tmp/vx_hr_smoke_XXXXXX", sizeof(s_hr_dir) - 1);
    if (::mkdtemp(s_hr_dir) == nullptr) {
      std::fprintf(stderr, "ERROR: mkdtemp failed\n");
      vx_view_destroy(view);
      vx_surface_destroy(surface);
      vx_event_loop_destroy(loop);
      return 1;
    }
    std::snprintf(s_hr_css_path, sizeof(s_hr_css_path), "%s/style.css",
                  s_hr_dir);
    FILE* f = std::fopen(s_hr_css_path, "w");
    if (!f) {
      std::fprintf(stderr, "ERROR: open %s failed\n", s_hr_css_path);
      vx_view_destroy(view);
      vx_surface_destroy(surface);
      vx_event_loop_destroy(loop);
      return 1;
    }
    std::fputs("body { background-color: red; }\n", f);
    std::fclose(f);
    std::printf("Hot Reload smoke: watching %s\n", s_hr_dir);
  }

  /* Attach DevTool dogfood UI on the right-hand 270px dock with F12
   * hotkey enabled. Production embedders typically attach unconditionally
   * during development and ship a build with VX_BUILD_DEVTOOL=OFF. */
  VxDevtoolOptions opts{};
  opts.devtool_width = 270;
  opts.enable_f12_hotkey = 1;
  opts.hot_reload_dir = hot_reload_test ? s_hr_dir : nullptr;
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

  if (hot_reload_test) {
    /* Schedule the CSS rewrite at +100ms so the watcher has settled. The
     * timer body MUST be fast (SDL timer thread) — write the file and
     * return; the main thread picks the inotify event up via
     * Application::Update → HotReloadManager::DrainEvents. */
    SDL_InitSubSystem(SDL_INIT_TIMER);
    SDL_AddTimer(
        100,
        [](Uint32, void* /*ud*/) -> Uint32 {
          FILE* f = std::fopen(s_hr_css_path, "w");
          if (f) {
            std::fputs("body { background-color: blue; }\n", f);
            std::fclose(f);
          }
          return 0;
        },
        nullptr);
  }

  /* TASK-20260502-02 B.3.2 — Performance Overlay smoke.
   * Install pipeline hooks via the C ABI to count frames. The Performance
   * Overlay's own PerfOverlay::Attach (B.1.2) installs richer trampolines
   * internally when DevTool wires it up, but here we install a SECOND set
   * via the public C API to verify the ABI surface works end-to-end —
   * UpdateManager::SetPipelineHooks contract is single-instance, so this
   * smoke uses the C API path independently of any internal PerfOverlay
   * attachment (LoadDevtoolDocument doesn't currently install one — that
   * wiring lands in the next subtask + R3 candidates). */
  static int s_perf_smoke_frames = 0;
  VxPipelineHooks perf_hooks{};
  perf_hooks.on_frame_end = [](void* ud) {
    int* counter = static_cast<int*>(ud);
    if (counter) (*counter)++;
  };
  VxResult set_hooks_rc = vx_view_set_pipeline_hooks(view, &perf_hooks,
                                                      &s_perf_smoke_frames);
  /* Hooks ARE cached even when set_hooks_rc != VX_OK; lazy-attached on
   * the next EnsureUpdateManager. After load_html/css already ran above,
   * EnsureUpdateManager has been triggered → hooks are live. */
  std::printf("Pipeline hooks installed (rc=%d).\n", set_hooks_rc);

  /* B.3.1 sanity: HUD should be visible by default after attach. */
  std::printf("HUD visible after attach: %d (1 expected).\n",
              vx_view_is_hud_visible(view));

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

  /* B.3.2 — verify the perf hooks fired during the run. Auto-quit at 200ms
   * @ 60fps target FPS = ~12 frames; we accept >=1 to keep the smoke
   * resilient on slow CI hardware. Print a stable line that ctest can
   * regex-match (PERF SMOKE: frames=N). */
  std::printf("PERF SMOKE: frames=%d hud_visible=%d\n",
              s_perf_smoke_frames, vx_view_is_hud_visible(view));
  if (s_perf_smoke_frames < 1) {
    std::fprintf(stderr,
                 "ERROR: pipeline hooks did not fire — perf overlay broken\n");
  }

  /* C.4.2 — Hot Reload smoke: print stable line BEFORE detach so the
   * tracked count reflects what actually fired during the run. ctest
   * matches the regex "HOT RELOAD: triggered count=[1-9]" to PASS. */
  if (hot_reload_test) {
    int tracked = vx_view_hot_reload_tracked_count(view);
    std::printf("HOT RELOAD: triggered count=%d\n", tracked);
  }

  vx_view_detach_devtool(view);
  vx_view_destroy(view);
  vx_surface_destroy(surface);
  vx_event_loop_destroy(loop);

  if (hot_reload_test && s_hr_dir[0] != '\0') {
    std::error_code ec;
    std::filesystem::remove_all(s_hr_dir, ec);
  }

  std::printf("Done.\n");
  return 0;
}
