#ifndef VELOXA_CORE_APPLICATION_H_
#define VELOXA_CORE_APPLICATION_H_

#include <memory>

#include "veloxa/core/css/parser.h"
#include "veloxa/core/dom/document.h"
#include "veloxa/core/dom/serializer.h"
#include "veloxa/core/event/event_manager.h"
#include "veloxa/core/image/image_cache.h"
#include "veloxa/core/layout/text_shaper.h"
#include "veloxa/core/update_manager.h"
#include "veloxa/foundation/strings/string_view.h"
#include "veloxa/graphics/canvas.h"
#include "veloxa/graphics/types.h"
#include "veloxa/platform/event_loop.h"
#include "veloxa/platform/surface.h"
#include "veloxa/text/font_manager.h"
#include "veloxa/text/glyph_cache.h"

namespace vx::script {
class QuickjsEngine;
class DomBindings;
}  // namespace vx::script

namespace vx::devtool::hot_reload {
class HotReloadManager;
}  // namespace vx::devtool::hot_reload

namespace vx::devtool::console {
class ConsoleEngine;
class ConsoleLogBuffer;
}  // namespace vx::devtool::console

namespace vx {

class Application {
 public:
  struct Config {
    platform::EventLoop* event_loop = nullptr;
    platform::Surface* surface = nullptr;
    u32 target_fps = 60;
    gfx::Color background_color = gfx::Color::White();
  };

  explicit Application(const Config& config);
  ~Application();

  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;

  void LoadHTML(StringView html);
  void LoadCSS(StringView css);
  Status LoadFont(StringView path, StringView family);
  Status LoadScript(StringView source);
  void InjectInput(const event::InputEvent& input);

  void Run();
  void Quit();
  void Update();

  // Dual Document slot (DevTool 三件套 I1 改造，TASK-20260502-01 A.0.1)：
  // - target_document_：业务页面 DOM（重命名 from document_）
  // - devtool_document_：DevTool UI DOM（nullable；attach DevTool 时设置；
  //   生命周期由 DevTool 子系统拥有，不在 Application 析构中释放）
  // 移除原 document() getter 强制 callsite 显式选 target/devtool 槽（R1
  // mitigation：漏改在编译期暴露）。
  dom::Document* target_document() const { return target_document_; }
  dom::Document* devtool_document() const { return devtool_document_; }
  event::EventManager& event_manager() { return event_manager_; }
  const UpdateManager* update_manager() const { return update_manager_.get(); }
  const UpdateManager* devtool_update_manager() const {
    return devtool_update_manager_.get();
  }
  platform::EventLoop* event_loop() const { return config_.event_loop; }

  // TASK-20260502-01 A.1.6 — DevTool dogfood UI lifecycle.
  //
  // LoadDevtoolDocument parses the embedded inspector_panel.html (A.1.2
  // compile-time embed) with __INLINE_CSS__ / __INLINE_JS__ placeholders
  // runtime-replaced by kInspectorPanelCss / kInspectorPanelJs (A.1.3
  // R2-verified content), creates devtool_document_ + a paired
  // UpdateManager whose layout viewport is (devtool_width, surface_height)
  // (B-A1.2=a full-viewport overlay model — DevTool renders on top of
  // the right-hand splitter dock zone).
  //
  // Returns false when DEVTOOL=OFF (#ifdef stubbed; A14 zero-byte
  // guard) or when the surface is missing. Idempotent: a second call
  // replaces the prior DevTool Document.
  //
  // UnloadDevtoolDocument tears down the DevTool Document + its
  // UpdateManager. Safe to call when no DevTool was loaded.
  bool LoadDevtoolDocument(f32 devtool_width = 270.0f);
  void UnloadDevtoolDocument();
  bool devtool_loaded() const { return devtool_document_ != nullptr; }

  // TASK-20260502-01 A.2.1 — T3 mitigation policy applied by all
  // DevTool serialization paths (vx_view_serialize_dom_json C API and
  // RegisterDevtoolBindings's vx_devtool_get_dom_json). Default is
  // kRedactSensitive (passwords → "[REDACTED]"). Embedders set via
  // vx_inspector_set_redaction_policy.
  dom::RedactionPolicy redaction_policy() const { return redaction_policy_; }
  void set_redaction_policy(dom::RedactionPolicy p);

  // TASK-20260502-01 A.1.8 — DevTool dogfood JS execution status.
  // Captures the StatusOr returned by the DevTool script engine when
  // LoadDevtoolDocument evaluated inspector_panel.js. ok() means the
  // panel script completed; otherwise the message contains the QuickJS
  // diagnostic. Reset to Status::Ok() on Unload.
  const Status& devtool_script_status() const { return devtool_script_status_; }

  // Re-enter the DevTool QuickJS engine to evaluate `source` as a global
  // expression and return the toString() of its result. Reserved for
  // tests that want to inspect intermediate state set up by the panel
  // script (e.g. asserting vx_devtool_get_dom_json() round-trips the
  // target Document JSON envelope). Returns InvalidArgument if no
  // DevTool is currently attached.
  StatusOr<std::string> EvalDevtoolScript(StringView source,
                                          StringView filename);

  // TASK-20260502-01 A.1.7 — F12 hotkey toggle for attach/detach.
  // When set, InjectInput intercepts KeyDown(F12) and toggles
  // LoadDevtoolDocument(devtool_default_width_) / UnloadDevtoolDocument
  // (the F12 event itself is NOT forwarded to event_manager_ in that
  // case). The hotkey is only effective AFTER an initial attach (i.e.
  // we do not auto-load DevTool from a fresh Application — avoids
  // surprise activation per A.1.7 plan contract).
  void SetDevtoolHotkey(bool enabled, f32 devtool_default_width = 270.0f) {
    devtool_hotkey_enabled_ = enabled;
    devtool_default_width_ = devtool_default_width;
  }

  // TASK-20260502-02 B.3.1 — Performance Overlay HUD visibility flag.
  // Default true after LoadDevtoolDocument (HUD shows by default when
  // DevTool attaches). F11 hotkey toggles when devtool_hotkey_enabled_.
  // Returns false when DevTool is not attached.
  bool hud_visible() const { return devtool_loaded() && hud_visible_; }
  void set_hud_visible(bool v) { hud_visible_ = v; }

  // TASK-20260502-02 B.0.1 — set the target Application's UpdateManager
  // PipelineHooks (Performance Overlay 5 callbacks). Application stores
  // an internal copy so the caller does not need to keep the hooks
  // structure alive. Passing nullptr clears the hooks. Returns:
  //   - true on success (hooks installed or cleared)
  //   - false if the target update_manager_ has not been initialized yet
  //     (no Application::Update has run + no LoadHTML to trigger lazy
  //     EnsureUpdateManager). In that case the hooks are still cached
  //     internally and applied on the next EnsureUpdateManager.
  bool SetPipelineHooks(const PipelineHooks* hooks);
  const PipelineHooks* pipeline_hooks() const {
    return external_hooks_set_ ? &external_hooks_ : nullptr;
  }

  // TASK-20260503-01 C.4.1 — Hot Reload manager accessor.
  // Always non-null when built with VX_BUILD_DEVTOOL=ON (Application
  // constructs one in its ctor and owns it for the lifetime of the
  // view). Always nullptr when DEVTOOL=OFF (A14 zero-byte stub guard).
  // The C ABI's vx_view_attach_devtool calls Attach(hot_reload_dir) on
  // this manager when the option is set; Update() drains queued events
  // before forwarding to update_manager_->Update().
  vx::devtool::hot_reload::HotReloadManager* hot_reload_manager() const {
#ifdef VX_BUILD_DEVTOOL
    return hot_reload_manager_.get();
#else
    return nullptr;
#endif
  }

  // TASK-20260503-04 D.1 — Console isolated JSRuntime accessor.
  // Non-null only between LoadDevtoolDocument / UnloadDevtoolDocument
  // (Console scope is tied to the DevTool Document lifecycle so the
  // panel JS that drains the buffer is guaranteed to be alive). Always
  // nullptr when DEVTOOL=OFF (A14 zero-byte stub guard, mirrors
  // hot_reload_manager() pattern).
  vx::devtool::console::ConsoleEngine* console_script_engine() const {
#ifdef VX_BUILD_DEVTOOL
    return console_script_engine_.get();
#else
    return nullptr;
#endif
  }

  // TASK-20260503-04 D.2 — Console log buffer accessor.
  // Same lifecycle as console_script_engine() (created/destroyed inside
  // Load/UnloadDevtoolDocument). The C-API drain (D.4) and integration
  // tests (D.5) reach the buffer through this getter rather than touching
  // the unique_ptr directly.
  vx::devtool::console::ConsoleLogBuffer* console_log_buffer() const {
#ifdef VX_BUILD_DEVTOOL
    return console_log_buffer_.get();
#else
    return nullptr;
#endif
  }

 private:
  void OnFrame();
  void EnsureUpdateManager();
  void EnsureDevtoolUpdateManager(f32 devtool_width);
  void RenderDevtoolWithTranslate();
  // Returns true if the input was consumed by the hotkey handler (and
  // must NOT be forwarded). False = forward as usual.
  bool MaybeHandleDevtoolHotkey(const event::InputEvent& input);

  Config config_;
  dom::Document* target_document_ = nullptr;
  dom::Document* devtool_document_ = nullptr;
  // A.1.6 — DevTool Document instances created by LoadDevtoolDocument
  // are owned here. External attach paths (future) may instead set
  // devtool_document_ directly without populating this slot, in which
  // case ownership stays external (matches A.0.1 contract).
  std::unique_ptr<dom::Document> owned_devtool_document_;
  f32 devtool_width_ = 0.0f;
  // A.1.7 — F12 hotkey state (set by C API vx_view_attach_devtool /
  // SetDevtoolHotkey). enabled gates whether KeyDown(F12) is consumed
  // and toggles attach/detach; default_width is the width passed back
  // into LoadDevtoolDocument when the hotkey re-attaches.
  bool devtool_hotkey_enabled_ = false;
  f32 devtool_default_width_ = 270.0f;
  // A.2.1 — T3 redaction policy (default = redact). Applied by both
  // C-API serialize_dom_json and the DevTool JS binding's
  // vx_devtool_get_dom_json on each call (so embedder may flip live).
  dom::RedactionPolicy redaction_policy_ =
      dom::RedactionPolicy::kRedactSensitive;
  // B.0.1 — owned PipelineHooks copy + flag (caller does not need to
  // keep the source struct alive). Re-applied to update_manager_ each
  // time EnsureUpdateManager runs (lazy attach when set before first
  // Update).
  PipelineHooks external_hooks_{};
  bool external_hooks_set_ = false;
  // B.3.1 — HUD visibility flag (true after LoadDevtoolDocument; F11 toggles).
  bool hud_visible_ = true;
  Vector<css::Stylesheet> stylesheets_;
  event::EventManager event_manager_;
  std::unique_ptr<layout::TextShaper> text_shaper_;
  text::FontManager font_manager_;
  text::GlyphCache glyph_cache_;
  image::ImageCache image_cache_;

  u32* surface_pixels_ = nullptr;
  std::unique_ptr<gfx::Canvas> canvas_;
  std::unique_ptr<UpdateManager> update_manager_;
  // A.1.6 M1 — second UpdateManager driving the DevTool Document.
  // Shares canvas_ + image_cache_ with the target one; renders with a
  // canvas Translate offset to land on the right-hand dock zone.
  std::unique_ptr<UpdateManager> devtool_update_manager_;
  std::unique_ptr<script::QuickjsEngine> script_engine_;
  std::unique_ptr<script::DomBindings> dom_bindings_;
  // A.1.8 — DevTool's own script engine + DOM bindings (independent
  // QuickJS context so DevTool JS sees its own document.* without
  // colliding with target Document bindings). Lifetime owned by
  // Load/UnloadDevtoolDocument.
  std::unique_ptr<script::QuickjsEngine> devtool_script_engine_;
  std::unique_ptr<script::DomBindings> devtool_dom_bindings_;
  Status devtool_script_status_;
#ifdef VX_BUILD_DEVTOOL
  // TASK-20260503-01 C.4.1 — Hot Reload manager (DEVTOOL=ON only;
  // entirely absent from OFF builds — A14 zero-byte stub guard).
  // Constructed in the Application ctor passing `this` so
  // HotReloadManager::DrainEvents can invoke LoadCSS on the main thread.
  // Detach() runs in dtor; the watcher thread is joined synchronously
  // before our fields disappear. The unique_ptr requires the complete
  // HotReloadManager type at instantiation/destruction, so the field
  // must be guarded by the same #ifdef as application.cc's include of
  // hot_reload_manager.h.
  std::unique_ptr<vx::devtool::hot_reload::HotReloadManager> hot_reload_manager_;
  // TASK-20260503-04 D.1 — Console isolated JSRuntime (DEVTOOL=ON only;
  // same A14 zero-byte stub rationale + same complete-type-at-dtor
  // requirement as hot_reload_manager_ above — Phase C R4 范式 reuse).
  // Constructed/destroyed inside Load/UnloadDevtoolDocument so the
  // panel JS that drains the log buffer always has a live engine to
  // talk to. Field ordering matters at teardown: this must release
  // BEFORE devtool_dom_bindings_ / devtool_script_engine_ so a
  // hypothetical cross-engine binding (none registered today) cannot
  // dangle. UnloadDevtoolDocument enforces the order explicitly.
  std::unique_ptr<vx::devtool::console::ConsoleEngine> console_script_engine_;
  // D.2 — buffer that receives console.log/error/warn pushes from the
  // Console JS scope (via RegisterConsoleBindings on the engine ctx).
  // Drained by vx_devtool_get_console_log_drain (called every ~200 ms
  // by D.3 console_panel.js). MUST release AFTER console_script_engine_
  // since the JS callbacks hold a JS_GetContextOpaque(ctx) pointer to
  // the buffer; the engine reset clears that channel before the buffer
  // memory disappears.
  std::unique_ptr<vx::devtool::console::ConsoleLogBuffer> console_log_buffer_;
#endif
  platform::EventLoop::TimerId frame_timer_id_ = 0;
};

}  // namespace vx

#endif  // VELOXA_CORE_APPLICATION_H_
