#ifndef VELOXA_CORE_APPLICATION_H_
#define VELOXA_CORE_APPLICATION_H_

#include <memory>

#include "veloxa/core/css/parser.h"
#include "veloxa/core/dom/document.h"
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
  platform::EventLoop::TimerId frame_timer_id_ = 0;
};

}  // namespace vx

#endif  // VELOXA_CORE_APPLICATION_H_
