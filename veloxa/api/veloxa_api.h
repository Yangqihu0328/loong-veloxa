/*
 * Veloxa — Embeddable UI Rendering Engine
 * Public C API
 *
 * This header is the sole public interface for embedding Veloxa.
 * All types and functions use C99-compatible declarations.
 */

#ifndef VELOXA_API_VELOXA_API_H_
#define VELOXA_API_VELOXA_API_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VX_API_VERSION_MAJOR 0
#define VX_API_VERSION_MINOR 1
#define VX_API_VERSION_PATCH 0

/* ── Opaque handles ─────────────────────────────────────────────── */

typedef struct VxView VxView;
typedef struct VxEventLoop VxEventLoop;
typedef struct VxSurface VxSurface;

/* ── Result codes ───────────────────────────────────────────────── */

typedef enum {
  VX_OK = 0,
  VX_ERROR_NULL_PARAM = -1,
  VX_ERROR_INVALID_STATE = -2,
  /* TASK-20260502-01 A.0.6: T7 mitigation for DevTool serialization API.
   * Returned when serialized JSON would exceed caller-provided max_size or
   * caller's *out_len buffer is too small to receive the JSON. */
  VX_ERROR_OUT_OF_MEMORY = -3,
  /* TASK-20260502-01 A.0.6: reserved for node_id-keyed APIs (A.1.x).
   * Returned when a node_id handle does not resolve in the current document. */
  VX_ERROR_NOT_FOUND = -4,
} VxResult;

/* ── Event types ────────────────────────────────────────────────── */

typedef enum {
  VX_EVENT_POINTER_MOVE = 0,
  VX_EVENT_POINTER_DOWN = 1,
  VX_EVENT_POINTER_UP = 2,
  VX_EVENT_KEY_DOWN = 3,
  VX_EVENT_KEY_UP = 4,
} VxEventType;

typedef struct {
  VxEventType type;
  float x;
  float y;
  uint8_t button;
  uint32_t key_code;
  uint32_t modifiers;
} VxInputEvent;

/* ── View configuration ─────────────────────────────────────────── */

typedef struct {
  VxEventLoop* event_loop;
  VxSurface* surface;
  uint32_t target_fps;
  uint32_t background_color; /* RRGGBBAA */
} VxViewConfig;

/* ── Window options (SDL2 backend) ──────────────────────────────── */

typedef struct {
  uint32_t width;
  uint32_t height;
  const char* title;  /* may be NULL → defaults to "Veloxa" */
  uint32_t flags;     /* reserved; must be 0 in v0.1 */
} VxWindowOptions;

/* ── Event Loop ─────────────────────────────────────────────────── */

VxEventLoop* vx_event_loop_create_headless(void);

/* SDL2 event loop. Returns NULL when the build was not configured with
 * -DVX_PLATFORM_SDL2=ON, or when SDL2 fails to initialize. */
VxEventLoop* vx_event_loop_create_sdl2(void);

void vx_event_loop_destroy(VxEventLoop* loop);

/* Drain pending platform events from `loop` and inject them as input into
 * `view`. No-op if `loop` is not a windowed/SDL2 event loop. Useful when
 * the embedder owns the per-frame pump (e.g. examples/hello_sdl2.cc). */
VxResult vx_event_loop_pump_input(VxEventLoop* loop, VxView* view);

/* ── Surface ────────────────────────────────────────────────────── */

VxSurface* vx_surface_create_memory(uint32_t width, uint32_t height);

/* SDL2 windowed surface. Returns NULL when SDL2 is not built in or when the
 * underlying SDL_CreateWindow fails. */
VxSurface* vx_surface_create_window(const VxWindowOptions* opts);

void vx_surface_destroy(VxSurface* surface);
VxResult vx_surface_save_ppm(const VxSurface* surface, const char* path);

/* ── View ───────────────────────────────────────────────────────── */

VxView* vx_view_create(const VxViewConfig* config);
void vx_view_destroy(VxView* view);

VxResult vx_view_load_html(VxView* view, const char* html, uint32_t len);
VxResult vx_view_load_css(VxView* view, const char* css, uint32_t len);
VxResult vx_view_inject_input(VxView* view, const VxInputEvent* event);

VxResult vx_view_load_font(VxView* view, const char* path, const char* family);

VxResult vx_view_load_script(VxView* view, const char* source, uint32_t len);

VxResult vx_view_update(VxView* view);
VxResult vx_view_run(VxView* view);
VxResult vx_view_quit(VxView* view);

/* ── DevTool Inspector C API (TASK-20260502-01 A.0.6) ─────────────
 *
 * Public thin wrapper over vx::devtool::SerializeDocument (D7=C 第二层).
 * Available only when built with -DVX_BUILD_DEVTOOL=ON; otherwise returns
 * VX_ERROR_INVALID_STATE.
 *
 * Double-call protocol (T7 mitigation against caller buffer overflow):
 *   1. First call with out_buf=NULL → returns needed size in *out_len
 *      (size excludes trailing NUL — JSON is not NUL-terminated).
 *   2. Caller allocates a buffer of size *out_len.
 *   3. Second call writes JSON into out_buf; *out_len updated to actual
 *      bytes written. If *out_len < required, returns VX_ERROR_OUT_OF_MEMORY
 *      without writing.
 *
 * max_size is a hard upper bound on JSON serialization size — defends
 * against DevTool exhaustion when target document is malicious / extremely
 * deep. Returns VX_ERROR_OUT_OF_MEMORY if needed > max_size (no allocation
 * is performed in that path beyond the inner devtool::SerializeDocument
 * which is bounded by reserve()). Recommended: 16 MiB for DOM, 1 MiB for
 * style/box.
 *
 * Sensitive attribute values (e.g. input[type=password] value) are redacted
 * to "[REDACTED]" (T3 mitigation enforced by devtool::SerializeDocument
 * default policy).
 */
VxResult vx_view_serialize_dom_json(VxView* view, char* out_buf,
                                    uint32_t* out_len, uint32_t max_size);

/* ── Info ───────────────────────────────────────────────────────── */

const char* vx_version(void);

#ifdef __cplusplus
}
#endif

#endif /* VELOXA_API_VELOXA_API_H_ */
