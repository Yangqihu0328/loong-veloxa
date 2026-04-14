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

/* ── Event Loop ─────────────────────────────────────────────────── */

VxEventLoop* vx_event_loop_create_headless(void);
void vx_event_loop_destroy(VxEventLoop* loop);

/* ── Surface ────────────────────────────────────────────────────── */

VxSurface* vx_surface_create_memory(uint32_t width, uint32_t height);
void vx_surface_destroy(VxSurface* surface);
VxResult vx_surface_save_ppm(const VxSurface* surface, const char* path);

/* ── View ───────────────────────────────────────────────────────── */

VxView* vx_view_create(const VxViewConfig* config);
void vx_view_destroy(VxView* view);

VxResult vx_view_load_html(VxView* view, const char* html, uint32_t len);
VxResult vx_view_load_css(VxView* view, const char* css, uint32_t len);
VxResult vx_view_inject_input(VxView* view, const VxInputEvent* event);

VxResult vx_view_load_font(VxView* view, const char* path, const char* family);

VxResult vx_view_update(VxView* view);
VxResult vx_view_run(VxView* view);
VxResult vx_view_quit(VxView* view);

/* ── Info ───────────────────────────────────────────────────────── */

const char* vx_version(void);

#ifdef __cplusplus
}
#endif

#endif /* VELOXA_API_VELOXA_API_H_ */
