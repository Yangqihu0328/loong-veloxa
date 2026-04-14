#include "veloxa/api/veloxa_api.h"

#include "veloxa/core/application.h"
#include "veloxa/platform/headless/headless_event_loop.h"
#include "veloxa/platform/headless/memory_surface.h"

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

void vx_event_loop_destroy(VxEventLoop* loop) {
  delete reinterpret_cast<vx::platform::HeadlessEventLoop*>(loop);
}

/* ── Surface ────────────────────────────────────────────────────── */

VxSurface* vx_surface_create_memory(uint32_t width, uint32_t height) {
  auto* surface = new vx::platform::MemorySurface(width, height);
  return reinterpret_cast<VxSurface*>(surface);
}

void vx_surface_destroy(VxSurface* surface) {
  delete reinterpret_cast<vx::platform::MemorySurface*>(surface);
}

VxResult vx_surface_save_ppm(const VxSurface* surface, const char* path) {
  if (!surface || !path) return VX_ERROR_NULL_PARAM;
  auto* s = reinterpret_cast<const vx::platform::MemorySurface*>(surface);
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

VxResult vx_view_update(VxView* view) {
  if (!view) return VX_ERROR_NULL_PARAM;
  auto* app = reinterpret_cast<vx::Application*>(view);
  app->Update();
  return VX_OK;
}

VxResult vx_view_run(VxView* view) {
  if (!view) return VX_ERROR_NULL_PARAM;
  auto* app = reinterpret_cast<vx::Application*>(view);
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
