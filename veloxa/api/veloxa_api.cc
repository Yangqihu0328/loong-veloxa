#include "veloxa/api/veloxa_api.h"

#include "veloxa/core/application.h"
#include "veloxa/platform/event_loop.h"
#include "veloxa/platform/headless/headless_event_loop.h"
#include "veloxa/platform/headless/memory_surface.h"
#include "veloxa/platform/surface.h"

#ifdef VX_PLATFORM_SDL2
#include "veloxa/platform/sdl2/sdl2_event_loop.h"
#include "veloxa/platform/sdl2/sdl2_window_surface.h"
#endif

static vx::event::EventType MapEventType(VxEventType type) {
  switch (type) {
    case VX_EVENT_POINTER_MOVE:
      return vx::event::EventType::kPointerMove;
    case VX_EVENT_POINTER_DOWN:
      return vx::event::EventType::kPointerDown;
    case VX_EVENT_POINTER_UP:
      return vx::event::EventType::kPointerUp;
    case VX_EVENT_KEY_DOWN:
      return vx::event::EventType::kKeyDown;
    case VX_EVENT_KEY_UP:
      return vx::event::EventType::kKeyUp;
    default:
      return vx::event::EventType::kPointerMove;
  }
}

static vx::gfx::Color ColorFromRGBA(uint32_t rgba) {
  return vx::gfx::Color::FromRGBA(
      static_cast<vx::u8>((rgba >> 24) & 0xFF),
      static_cast<vx::u8>((rgba >> 16) & 0xFF),
      static_cast<vx::u8>((rgba >> 8) & 0xFF),
      static_cast<vx::u8>(rgba & 0xFF));
}

extern "C" {

/* ── Event Loop ─────────────────────────────────────────────────── */

VxEventLoop* vx_event_loop_create_headless(void) {
  auto* loop = new vx::platform::HeadlessEventLoop();
  return reinterpret_cast<VxEventLoop*>(loop);
}

VxEventLoop* vx_event_loop_create_sdl2(void) {
#ifdef VX_PLATFORM_SDL2
  auto* loop = new vx::platform::Sdl2EventLoop();
  return reinterpret_cast<VxEventLoop*>(loop);
#else
  return nullptr;
#endif
}

VxResult vx_event_loop_pump_input(VxEventLoop* loop, VxView* view) {
  if (!loop || !view) return VX_ERROR_NULL_PARAM;
#ifdef VX_PLATFORM_SDL2
  auto* base = reinterpret_cast<vx::platform::EventLoop*>(loop);
  auto* sdl = dynamic_cast<vx::platform::Sdl2EventLoop*>(base);
  if (!sdl) return VX_ERROR_INVALID_STATE;
  // Bind callback on every call (cheap; lets caller swap views per frame).
  sdl->SetInputCallback([view](const VxInputEvent& e) {
    vx_view_inject_input(view, &e);
  });
  sdl->PumpInputEvents();
  return VX_OK;
#else
  (void)loop; (void)view;
  return VX_ERROR_INVALID_STATE;
#endif
}

void vx_event_loop_destroy(VxEventLoop* loop) {
  // ABI contract: handle is always a vx::platform::EventLoop* (or derived);
  // base class has virtual ~. Adding multiply-inherited backends would break
  // this reinterpret_cast — see TASK-20260425-01 spec §5.3.
  delete reinterpret_cast<vx::platform::EventLoop*>(loop);
}

/* ── Surface ────────────────────────────────────────────────────── */

VxSurface* vx_surface_create_memory(uint32_t width, uint32_t height) {
  auto* surface = new vx::platform::MemorySurface(width, height);
  return reinterpret_cast<VxSurface*>(surface);
}

VxSurface* vx_surface_create_window(const VxWindowOptions* opts) {
  if (!opts) return nullptr;
#ifdef VX_PLATFORM_SDL2
  auto* surface = new vx::platform::Sdl2WindowSurface(
      opts->width, opts->height, opts->title);
  if (!surface->valid()) {
    delete surface;
    return nullptr;
  }
  return reinterpret_cast<VxSurface*>(surface);
#else
  return nullptr;
#endif
}

void vx_surface_destroy(VxSurface* surface) {
  // ABI contract: handle is always a vx::platform::Surface* (or derived);
  // base class has virtual ~. Adding multiply-inherited backends would break
  // this reinterpret_cast — see TASK-20260425-01 spec §5.3.
  delete reinterpret_cast<vx::platform::Surface*>(surface);
}

VxResult vx_surface_save_ppm(const VxSurface* surface, const char* path) {
  if (!surface || !path) return VX_ERROR_NULL_PARAM;
  auto* s = reinterpret_cast<const vx::platform::Surface*>(surface);
  auto status = s->SavePPM(path);
  return status.ok() ? VX_OK : VX_ERROR_INVALID_STATE;
}

/* ── View ───────────────────────────────────────────────────────── */

VxView* vx_view_create(const VxViewConfig* config) {
  if (!config) return nullptr;

  vx::Application::Config cfg;
  cfg.event_loop =
      reinterpret_cast<vx::platform::EventLoop*>(config->event_loop);
  cfg.surface = reinterpret_cast<vx::platform::Surface*>(config->surface);
  cfg.target_fps = config->target_fps > 0 ? config->target_fps : 60;
  if (config->background_color != 0) {
    cfg.background_color = ColorFromRGBA(config->background_color);
  }

  auto* app = new vx::Application(cfg);
  return reinterpret_cast<VxView*>(app);
}

void vx_view_destroy(VxView* view) {
  delete reinterpret_cast<vx::Application*>(view);
}

VxResult vx_view_load_html(VxView* view, const char* html, uint32_t len) {
  if (!view || !html) return VX_ERROR_NULL_PARAM;
  auto* app = reinterpret_cast<vx::Application*>(view);
  app->LoadHTML(vx::StringView(html, len));
  return VX_OK;
}

VxResult vx_view_load_css(VxView* view, const char* css, uint32_t len) {
  if (!view || !css) return VX_ERROR_NULL_PARAM;
  auto* app = reinterpret_cast<vx::Application*>(view);
  app->LoadCSS(vx::StringView(css, len));
  return VX_OK;
}

VxResult vx_view_inject_input(VxView* view, const VxInputEvent* event) {
  if (!view || !event) return VX_ERROR_NULL_PARAM;
  auto* app = reinterpret_cast<vx::Application*>(view);
  vx::event::InputEvent input{};
  input.type = MapEventType(event->type);
  input.x = event->x;
  input.y = event->y;
  input.button = event->button;
  input.key_code = event->key_code;
  input.modifiers = event->modifiers;
  app->InjectInput(input);
  return VX_OK;
}

VxResult vx_view_load_font(VxView* view, const char* path,
                           const char* family) {
  if (!view || !path || !family) return VX_ERROR_NULL_PARAM;
  auto* app = reinterpret_cast<vx::Application*>(view);
  auto status =
      app->LoadFont(vx::StringView(path), vx::StringView(family));
  return status.ok() ? VX_OK : VX_ERROR_INVALID_STATE;
}

VxResult vx_view_load_script(VxView* view, const char* source, uint32_t len) {
  if (!view || !source) return VX_ERROR_NULL_PARAM;
  auto* app = reinterpret_cast<vx::Application*>(view);
  auto status =
      app->LoadScript(vx::StringView(source, static_cast<vx::usize>(len)));
  return status.ok() ? VX_OK : VX_ERROR_INVALID_STATE;
}

VxResult vx_view_update(VxView* view) {
  if (!view) return VX_ERROR_NULL_PARAM;
  auto* app = reinterpret_cast<vx::Application*>(view);
  app->Update();
  return VX_OK;
}

VxResult vx_view_run(VxView* view) {
  if (!view) return VX_ERROR_NULL_PARAM;
  auto* app = reinterpret_cast<vx::Application*>(view);
#ifdef VX_PLATFORM_SDL2
  // Auto-wire SDL input forwarding when the configured event loop is the
  // SDL2 backend. Headless loops are unaffected.
  if (auto* sdl = dynamic_cast<vx::platform::Sdl2EventLoop*>(app->event_loop())) {
    sdl->SetInputCallback([view](const VxInputEvent& e) {
      vx_view_inject_input(view, &e);
    });
  }
#endif
  app->Run();
  return VX_OK;
}

VxResult vx_view_quit(VxView* view) {
  if (!view) return VX_ERROR_NULL_PARAM;
  auto* app = reinterpret_cast<vx::Application*>(view);
  app->Quit();
  return VX_OK;
}

/* ── Info ───────────────────────────────────────────────────────── */

const char* vx_version(void) { return "0.1.0"; }

}  /* extern "C" */
