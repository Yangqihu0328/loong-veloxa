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
  /* TASK-20260503-01 C.4.1: vx_view_attach_devtool's hot_reload_dir
   * attach failed (T2 path-traversal guard rejected the directory, dir
   * does not exist, etc.). Inspector + Performance Overlay attach still
   * succeeded; hot reload is best-effort and the warning is informational
   * — embedders can call vx_devtool_get_hot_reload_status to inspect
   * last_error, or simply ignore the warning. Positive value to remain
   * cleanly distinguishable from VX_OK while not colliding with any
   * existing negative error code. */
  VX_WARNING_HOT_RELOAD_FAILED = 1,
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

/* ── DevTool Attach C API (TASK-20260502-01 A.1.7) ────────────────
 *
 * Public C wrapper around Application::LoadDevtoolDocument /
 * UnloadDevtoolDocument (A.1.6 dual UpdateManager + dogfood UI). When
 * built with -DVX_BUILD_DEVTOOL=OFF, vx_view_attach_devtool returns
 * VX_ERROR_INVALID_STATE (A14 zero-byte stub guard) and devtool_loaded
 * always returns 0.
 *
 * F12 hotkey: when enable_f12_hotkey=1, a VX_EVENT_KEY_DOWN with
 * key_code == VX_KEY_F12 fed via vx_view_inject_input is consumed
 * internally to toggle DevTool attach/detach (the event is NOT
 * forwarded to the target Document's event handlers). Embedders that
 * want to handle F12 themselves should pass enable_f12_hotkey=0.
 *
 * F12 hotkey only triggers AFTER an initial vx_view_attach_devtool —
 * it never auto-loads DevTool from a fresh view (avoids surprise
 * activation in apps that did not opt-in).
 */

/* SDL2 SDLK_F12 keycode (cf. veloxa/platform/sdl2/sdl2_input_translate
 * which now maps SDLK_F12 → VX_KEY_F12). Embedders not using SDL2 can
 * pass this same value to vx_view_inject_input to invoke the hotkey. */
#define VX_KEY_F12 0x40000045u

/* TASK-20260502-02 B.3.1 — F11 toggle Performance Overlay HUD visibility.
 * SDL2 SDLK_F11 = 0x40000044. Same gate as F12 (only effective AFTER
 * vx_view_attach_devtool with enable_f12_hotkey=1; renamed in mind but
 * the option still gates BOTH F11 + F12 since both belong to the
 * DevTool keyboard hotkey set). HUD visual response (DOM display style)
 * is the embedder's job (B.3.2 hello_devtool integration shows one
 * way: read vx_view_is_hud_visible each frame and apply CSS). */
#define VX_KEY_F11 0x40000044u

typedef struct {
  /* DevTool splitter dock width in px. Default 270; clamped to
   * [200, 400] when out of range (no error returned). */
  uint32_t devtool_width;
  /* 0=off, 1=on. When NULL opts is passed, defaults to 1. */
  uint8_t enable_f12_hotkey;
  /* TASK-20260503-01 C.4.1: optional Hot Reload watch directory. NULL
   * or "" disables Hot Reload (Inspector + Performance Overlay attach
   * still succeed). When set, must be an absolute path; the underlying
   * InotifyFileWatcher applies the T2 8-step canonical-path /
   * symlink-rejection / max-file-size guards and only forwards .css
   * file changes to Application::LoadCSS. Attach failure (relative
   * path, missing dir, T2 reject) returns VX_WARNING_HOT_RELOAD_FAILED
   * — Inspector + Overlay attach are NOT rolled back. The warning is
   * informational; embedders can read vx_devtool_get_hot_reload_status
   * for the last_error string. DEVTOOL=OFF: hot_reload_dir is ignored
   * (the whole attach returns VX_ERROR_INVALID_STATE before reaching
   * this field). */
  const char* hot_reload_dir;
} VxDevtoolOptions;

/* Attach DevTool dogfood UI to view. opts may be NULL → uses
 * defaults (devtool_width=270, enable_f12_hotkey=1). */
VxResult vx_view_attach_devtool(VxView* view, const VxDevtoolOptions* opts);

/* Detach (no-op if not attached). */
VxResult vx_view_detach_devtool(VxView* view);

/* Returns: 1 if DevTool currently attached, 0 if not, -1 if view is NULL. */
int vx_view_devtool_loaded(VxView* view);

/* TASK-20260502-02 B.3.1 — Performance Overlay HUD visibility flag.
 * Returns:
 *   1 if HUD is currently visible (default after attach when DEVTOOL=ON),
 *   0 if HUD is hidden OR DevTool not attached OR DEVTOOL=OFF,
 *  -1 if view is NULL.
 * Toggled by F11 hotkey when enable_f12_hotkey=1; embedders without
 * hotkey can still poll this via the future vx_view_set_hud_visible
 * (not in B.3.1 scope). */
int vx_view_is_hud_visible(VxView* view);

/* ── DevTool Redaction Policy C API (TASK-20260502-01 A.2.1, T3) ──
 *
 * T3 mitigation policy switch for DevTool DOM serialization. Affects
 * BOTH vx_view_serialize_dom_json (C-API path) and the DevTool JS
 * binding's vx_devtool_get_dom_json on subsequent calls. Default
 * policy is VX_REDACTION_REDACT_SENSITIVE — embedders may opt out
 * with VX_REDACTION_NONE under their own threat model.
 *
 * Returns:
 *   VX_OK on success
 *   VX_ERROR_NULL_PARAM when view is NULL
 *   VX_ERROR_INVALID_STATE when policy is not a recognised enum value,
 *                          OR when built with VX_BUILD_DEVTOOL=OFF.
 */
typedef enum {
  /* Default: input[type=password] etc. → "[REDACTED]" */
  VX_REDACTION_REDACT_SENSITIVE = 0,
  /* Escape hatch for embedder-controlled deep inspection */
  VX_REDACTION_NONE = 1,
} VxRedactionPolicy;

VxResult vx_inspector_set_redaction_policy(VxView* view,
                                           VxRedactionPolicy policy);

/* ── DevTool Performance Pipeline Hooks C API (TASK-20260502-02 B.0.1) ──
 *
 * Public C wrapper around Application::SetPipelineHooks (technical debt
 * #35 phase 1 closure — UpdateManager 5-hook injection points).
 * Available regardless of -DVX_BUILD_DEVTOOL: caller may install custom
 * pipeline observers (e.g. profilers, tracing) without DevTool. The
 * Performance Overlay (B.1.x / B.2.x) installs its own hooks via this
 * API internally when DEVTOOL=ON.
 *
 * Hook firing order in UpdateManager::Update():
 *   on_frame_start  → entry (after dirty/config check)
 *   on_after_style  → after DetectAndApplyTransitions
 *   on_after_layout → same point as after_style (LayoutEngine includes
 *                     style resolve; sub-stage split is #35 phase 2 P3)
 *   on_after_render → after render::Record (PaintCommand recorded)
 *   on_frame_end    → at Update() end (after Replay)
 *
 * The userdata pointer is forwarded as the sole argument to each
 * callback. Application stores its own copy of VxPipelineHooks so the
 * caller does not need to keep the struct alive.
 *
 * Returns:
 *   VX_OK on success (hooks installed or cleared with hooks=NULL)
 *   VX_ERROR_NULL_PARAM when view is NULL
 *   VX_ERROR_INVALID_STATE when the view's UpdateManager is not yet
 *     initialized (no LoadHTML / Update has run). The hooks ARE still
 *     cached internally and applied on the next EnsureUpdateManager.
 */
typedef void (*VxPipelineCallback)(void* userdata);

typedef struct {
  VxPipelineCallback on_frame_start;
  VxPipelineCallback on_after_style;
  VxPipelineCallback on_after_layout;
  VxPipelineCallback on_after_render;
  VxPipelineCallback on_frame_end;
} VxPipelineHooks;

VxResult vx_view_set_pipeline_hooks(VxView* view,
                                    const VxPipelineHooks* hooks,
                                    void* userdata);

/* ── Info ───────────────────────────────────────────────────────── */

const char* vx_version(void);

#ifdef __cplusplus
}
#endif

#endif /* VELOXA_API_VELOXA_API_H_ */
