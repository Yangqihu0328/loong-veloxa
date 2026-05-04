#include "veloxa/core/application.h"

#include <cstring>
#include <string>

#include "veloxa/core/html/parser.h"
#include "veloxa/graphics/software/software_canvas.h"
#include "veloxa/script/dom_bindings.h"
#include "veloxa/script/quickjs_engine.h"
#include "veloxa/text/freetype_shaper.h"

#ifdef VX_BUILD_DEVTOOL
#include "veloxa/devtool/console/console_engine.h"
#include "veloxa/devtool/hot_reload/hot_reload_manager.h"
#include "veloxa/devtool/resources/inspector_resources.h"
#include "veloxa/script/dom_bindings.h"
#include "veloxa/script/quickjs_engine.h"
#endif

// SDLK_F11/F12 keycodes forwarded by sdl2_input_translate (A.1.7 + B.3.1).
// We avoid pulling in veloxa_api.h here (would create a core→api header
// cycle); the values MUST match VX_KEY_F11/F12 there. Keep these in sync
// via tests/api/devtool_attach_api_test.cc.
namespace {
constexpr vx::u32 kVxKeyF11 = 0x40000044u;
constexpr vx::u32 kVxKeyF12 = 0x40000045u;
}  // namespace

namespace vx {

namespace {

#ifdef VX_BUILD_DEVTOOL
// Replace one occurrence of `placeholder` in `host` with `replacement`.
// Returns true if a substitution happened. Used for inspector_panel.html
// __INLINE_CSS__ / __INLINE_JS__ placeholder injection.
bool ReplaceFirst(std::string& host, const char* placeholder,
                  const char* replacement) {
  const std::size_t pos = host.find(placeholder);
  if (pos == std::string::npos) return false;
  host.replace(pos, std::strlen(placeholder), replacement);
  return true;
}
#endif

}  // namespace

Application::Application(const Config& config) : config_(config) {
  text_shaper_ = std::make_unique<layout::SimpleTextShaper>();
  font_manager_.Init();
  if (config_.surface) {
    surface_pixels_ = config_.surface->Lock();
    canvas_ = std::make_unique<gfx::sw::SoftwareCanvas>(
        surface_pixels_, config_.surface->width(), config_.surface->height(),
        config_.surface->stride(), &font_manager_, &glyph_cache_);
    canvas_->Begin();
    canvas_->Clear(config_.background_color);
  }
#ifdef VX_BUILD_DEVTOOL
  // TASK-20260503-01 C.4.1 — Hot Reload manager is per-Application and
  // lives for the view's whole lifetime. Construction is cheap (no
  // syscalls, no thread); the worker thread + inotify fd are deferred
  // to Attach(). Passing `this` is safe — HotReloadManager only stores
  // the pointer; it never dereferences it before Attach() spawns the
  // watcher and DrainEvents() is invoked from Update() on the main
  // thread.
  hot_reload_manager_ =
      std::make_unique<vx::devtool::hot_reload::HotReloadManager>(this);
#endif
}

Application::~Application() {
#ifdef VX_BUILD_DEVTOOL
  // C.4.1 — release HotReloadManager BEFORE bindings/Document slots so
  // its dtor (which Detach()s and joins the watcher thread) cannot race
  // with another thread's pending DrainEvents touching `this`. The watch
  // thread itself never touches Application directly; OnFileChanged
  // only enqueues. Still, ordering it first keeps the teardown invariant
  // simple: by the time we touch any other field, no FileWatcher thread
  // is alive.
  hot_reload_manager_.reset();
#endif
  // A.1.8 — tear down DevTool script bindings/engine BEFORE the DevTool
  // Document slot so JS callbacks cannot fire on a freed Document.
  // TASK-20260503-04 D.1 — Console engine releases first (reverse of its
  // construction in LoadDevtoolDocument), preserving the contract that
  // by the time devtool_dom_bindings_ unbinds, no separate runtime is
  // still holding any cross-engine reference (today there is none — see
  // creative-devtool-console §C3 path 2 — but the ordering keeps future
  // additions safe by default).
#ifdef VX_BUILD_DEVTOOL
  console_script_engine_.reset();
#endif
  if (devtool_dom_bindings_) devtool_dom_bindings_->Unbind();
  devtool_dom_bindings_.reset();
  devtool_script_engine_.reset();
  if (dom_bindings_) dom_bindings_->Unbind();
  dom_bindings_.reset();
  script_engine_.reset();
  if (config_.surface && surface_pixels_) {
    config_.surface->Unlock();
  }
  delete target_document_;
  // External devtool_document_ (set directly without LoadDevtoolDocument)
  // ownership belongs to DevTool subsystem and is NOT freed here.
  // owned_devtool_document_ is auto-released by unique_ptr.
  font_manager_.Shutdown();
}

void Application::LoadHTML(StringView html) {
  delete target_document_;
  target_document_ = html::Parser::Parse(html);
  update_manager_.reset();
}

void Application::LoadCSS(StringView css) {
  stylesheets_.push_back(css::CssParser::Parse(css));
  update_manager_.reset();
}

Status Application::LoadFont(StringView path, StringView family) {
  auto result = font_manager_.LoadFont(path, family);
  if (!result.ok()) return result.status();
  auto* ft_shaper = new text::FreeTypeTextShaper(&font_manager_);
  ft_shaper->set_default_font(result.value());
  text_shaper_.reset(ft_shaper);
  return Status::Ok();
}

Status Application::LoadScript(StringView source) {
  if (!target_document_) {
    return Status(StatusCode::kInvalidArgument, "No document loaded");
  }
  if (!script_engine_) {
    script_engine_ = std::make_unique<script::QuickjsEngine>();
    auto st = script_engine_->Init();
    if (!st.ok()) return st;
  }
  if (!dom_bindings_) {
    dom_bindings_ = std::make_unique<script::DomBindings>();
    dom_bindings_->Bind(script_engine_->context(), target_document_,
                        &event_manager_);
  }
  auto result = script_engine_->EvalGlobal(source, StringView("script"));
  if (!result.ok()) return result.status();
  return Status::Ok();
}

void Application::InjectInput(const event::InputEvent& input) {
  // A.1.7 — F12 hotkey interception happens BEFORE forwarding so the
  // event is fully consumed (target Document never sees the F12 in
  // hotkey-on mode, avoiding double-handling).
  if (MaybeHandleDevtoolHotkey(input)) return;

  EnsureUpdateManager();
  if (update_manager_) {
    event_manager_.HandleInput(input, update_manager_->layout_root());
  }
}

bool Application::MaybeHandleDevtoolHotkey(const event::InputEvent& input) {
#ifdef VX_BUILD_DEVTOOL
  // Only react when (a) hotkey was enabled by an explicit attach and
  // (b) the event is KeyDown(F11/F12). Any other event shape (or a fresh
  // Application that never attached) is forwarded normally.
  if (!devtool_hotkey_enabled_) return false;
  if (input.type != event::EventType::kKeyDown) return false;
  if (input.key_code == kVxKeyF12) {
    if (devtool_loaded()) {
      UnloadDevtoolDocument();
    } else {
      LoadDevtoolDocument(devtool_default_width_);
    }
    return true;
  }
  // B.3.1 — F11 toggles HUD visibility. Only effective when DevTool
  // is attached (otherwise there's no HUD to show/hide).
  if (input.key_code == kVxKeyF11) {
    if (devtool_loaded()) {
      hud_visible_ = !hud_visible_;
    }
    // Consume the key even when not attached so the target Document
    // never sees F11 in hotkey-on mode (parity with F12 when
    // disabled-but-still-pressed scenarios; consistent UX).
    return true;
  }
  return false;
#else
  (void)input;
  return false;
#endif
}

void Application::Run() {
  EnsureUpdateManager();
  Update();

  if (config_.event_loop) {
    u32 interval = config_.target_fps > 0 ? 1000 / config_.target_fps : 16;
    frame_timer_id_ =
        config_.event_loop->SetTimer(interval, [this]() { OnFrame(); }, true);
    config_.event_loop->Run();
  }
}

void Application::Quit() {
  if (config_.event_loop) {
    config_.event_loop->CancelTimer(frame_timer_id_);
    config_.event_loop->Quit();
  }
}

void Application::Update() {
#ifdef VX_BUILD_DEVTOOL
  // C.4.1 — drain queued FileWatcher events on the main thread BEFORE
  // running the per-frame UpdateManager. DrainEvents calls LoadCSS for
  // every surviving event, which resets update_manager_ via the existing
  // LoadCSS path — so EnsureUpdateManager below picks up the new
  // stylesheets cleanly within the same frame. No-op when nothing is
  // attached (cheap mutex-protected queue check).
  if (hot_reload_manager_) {
    hot_reload_manager_->DrainEvents();
  }
#endif
  EnsureUpdateManager();
  if (update_manager_) {
    update_manager_->Update();
  }
  // M1 dual UpdateManager — DevTool Document Update runs after target so
  // its render lands on top of the right-hand splitter dock zone (B-A1.2=a
  // full-viewport overlay model). Canvas Translate is applied around the
  // DevTool Update so its DisplayList Replay paints into the dock region;
  // the un-translate guards subsequent target-frame compositing in the
  // same surface.
  if (devtool_update_manager_) {
    RenderDevtoolWithTranslate();
  }
  // Push the rendered buffer to the display backend at end of every frame
  // (no-op for headless surfaces; SDL2 backends UpdateTexture +
  // RenderPresent). Idempotent: backends may be called multiple times per
  // logical frame without issues.
  if (config_.surface) {
    config_.surface->Present();
  }
}

void Application::RenderDevtoolWithTranslate() {
  if (!devtool_update_manager_ || !canvas_) return;
  const f32 dock_x = config_.surface
                         ? static_cast<f32>(config_.surface->width()) -
                               devtool_width_
                         : 0.0f;
  canvas_->PushState();
  canvas_->SetTransform(gfx::Matrix3x2::Translation(dock_x, 0.0f));
  devtool_update_manager_->Update();
  canvas_->PopState();
}

void Application::OnFrame() { Update(); }

void Application::EnsureUpdateManager() {
  if (update_manager_ || !target_document_ || !canvas_) return;
  UpdateManager::Config cfg;
  cfg.document = target_document_;
  cfg.stylesheets = &stylesheets_;
  cfg.layout_context.text_shaper = text_shaper_.get();
  cfg.layout_context.viewport_width =
      config_.surface ? static_cast<f32>(config_.surface->width()) : 0;
  cfg.layout_context.viewport_height =
      config_.surface ? static_cast<f32>(config_.surface->height()) : 0;
  cfg.layout_context.image_cache = &image_cache_;
  cfg.image_cache = &image_cache_;
  cfg.canvas = canvas_.get();
  cfg.event_manager = &event_manager_;
  update_manager_ = std::make_unique<UpdateManager>(cfg);
  // B.0.1 — lazy attach hooks set before first EnsureUpdateManager.
  if (external_hooks_set_) {
    update_manager_->SetPipelineHooks(&external_hooks_);
  }
}

bool Application::SetPipelineHooks(const PipelineHooks* hooks) {
  if (hooks) {
    external_hooks_ = *hooks;
    external_hooks_set_ = true;
  } else {
    external_hooks_ = PipelineHooks{};
    external_hooks_set_ = false;
  }
  if (!update_manager_) return false;  // lazy applied by EnsureUpdateManager
  update_manager_->SetPipelineHooks(external_hooks_set_ ? &external_hooks_
                                                         : nullptr);
  return true;
}

// =============================================================================
// TASK-20260502-01 A.1.6 — DevTool dogfood UI lifecycle (M1 + M3)
// =============================================================================

#ifdef VX_BUILD_DEVTOOL

bool Application::LoadDevtoolDocument(f32 devtool_width) {
  if (!canvas_) return false;

  // Build inspector_panel.html with __INLINE_CSS__ / __INLINE_JS__
  // placeholders runtime-replaced. Inlining avoids triggering DevTool
  // Document <link> / <script src> sub-resource loading whose chain has
  // not been validated yet (A.1.3 dogfood UI design comment).
  std::string html(devtool::resources::kInspectorPanelHtml);
  ReplaceFirst(html, "__INLINE_CSS__", devtool::resources::kInspectorPanelCss);
  ReplaceFirst(html, "__INLINE_JS__", devtool::resources::kInspectorPanelJs);

  // Replace any previously loaded DevTool Document (idempotent).
  UnloadDevtoolDocument();
  owned_devtool_document_.reset(
      html::Parser::Parse(StringView(html.data(), html.size())));
  devtool_document_ = owned_devtool_document_.get();
  devtool_width_ = devtool_width;

  EnsureDevtoolUpdateManager(devtool_width);

  // A.1.8 — bring up DevTool's own QuickJS engine + DOM bindings and
  // execute the inlined inspector_panel.js. Failures land in
  // devtool_script_status_ for callers (and integration tests) to
  // observe; we don't surface them as a hard Load failure because
  // R2-class engine gaps (innerHTML setter, addEventListener, etc.)
  // can leave the panel partially-initialised but still useful for
  // displaying basic structure. devtool_script_status() is the
  // authoritative success signal for "JS executed".
  devtool_script_status_ = Status::Ok();
  if (owned_devtool_document_) {
    devtool_script_engine_ = std::make_unique<script::QuickjsEngine>();
    auto init = devtool_script_engine_->Init();
    if (!init.ok()) {
      devtool_script_status_ = init;
    } else {
      devtool_dom_bindings_ = std::make_unique<script::DomBindings>();
      devtool_dom_bindings_->Bind(devtool_script_engine_->context(),
                                   owned_devtool_document_.get(),
                                   &event_manager_);
      // T3 cross-Document inspection contract: DevTool JS calls
      // vx_devtool_get_dom_json() and gets the TARGET document
      // (with sensitive attributes redacted by SerializeDocument).
      devtool_dom_bindings_->SetDevtoolTargetDocument(target_document_);
      // A.2.1: keep JS binding in sync with the per-view T3 policy so
      // a flip via vx_inspector_set_redaction_policy that occurred
      // BEFORE attach is honoured by the very first JS call.
      devtool_dom_bindings_->SetRedactionPolicy(redaction_policy_);
      // C.4.1: wire HotReloadManager into the DevTool DomBindings so
      // the inspector_panel.js status indicator (C.3.1) can read
      // tracked_count + last_error via vx_devtool_get_hot_reload_status.
      devtool_dom_bindings_->SetHotReloadManager(hot_reload_manager_.get());
      script::RegisterDevtoolBindings(devtool_script_engine_->context());
      // TASK-20260503-04 D.1 — bring up Console isolated runtime BEFORE
      // evaluating inspector_panel.js so the panel script can probe
      // (typeof vx_console_eval) without race. RegisterConsoleBindings
      // (D.2) and the C-API side (D.4) wire eval/drain endpoints onto
      // this engine. Init() failure is non-fatal for the DevTool UI —
      // the inspector panel still works; only the Console tab degrades
      // (REPL eval will return NOT_ATTACHED at the C ABI layer).
      console_script_engine_ =
          std::make_unique<vx::devtool::console::ConsoleEngine>();
      auto console_init = console_script_engine_->Init();
      if (!console_init.ok()) {
        console_script_engine_.reset();  // explicit teardown on Init failure
      }
      auto eval = devtool_script_engine_->EvalGlobal(
          StringView(devtool::resources::kInspectorPanelJs),
          StringView("inspector_panel.js"));
      // StatusOr<T>::status() DCHECKs on the success path — guard.
      devtool_script_status_ = eval.ok() ? Status::Ok() : eval.status();
    }
  }

  return owned_devtool_document_ != nullptr;
}

void Application::UnloadDevtoolDocument() {
  // Reverse construction order: console engine → devtool bindings → devtool
  // engine → update mgr → Document slot. Console engine MUST go first so
  // its JSRuntime is gone before the rest of the DevTool wires unwind —
  // creative-devtool-console §C2 lifecycle timing constraint.
  console_script_engine_.reset();
  devtool_dom_bindings_.reset();
  devtool_script_engine_.reset();
  devtool_script_status_ = Status::Ok();
  // Tear down UpdateManager first so it stops referencing the Document.
  devtool_update_manager_.reset();
  // Only clear the slot if WE owned the Document. External attach paths
  // (devtool_document_ set without populating owned_) keep their slot
  // intact — caller is responsible for clearing.
  if (owned_devtool_document_) {
    devtool_document_ = nullptr;
    owned_devtool_document_.reset();
  }
  // B.3.1 — reset HUD visibility flag so a subsequent re-attach starts
  // with HUD visible (default state). Otherwise an F11 toggle before
  // detach would leak across attach cycles.
  hud_visible_ = true;
  devtool_width_ = 0.0f;
}

StatusOr<std::string> Application::EvalDevtoolScript(StringView source,
                                                     StringView filename) {
  if (!devtool_script_engine_) {
    return Status(StatusCode::kInvalidArgument,
                  "DevTool script engine not attached");
  }
  return devtool_script_engine_->EvalGlobal(source, filename);
}

void Application::EnsureDevtoolUpdateManager(f32 devtool_width) {
  if (!devtool_document_ || !canvas_) return;
  UpdateManager::Config cfg;
  cfg.document = devtool_document_;
  cfg.stylesheets = &stylesheets_;
  cfg.layout_context.text_shaper = text_shaper_.get();
  // B-A1.2=a full viewport + render overlay: DevTool Document lays out
  // to (devtool_width, surface_height) — the splitter dock dimensions.
  // Target Document keeps full surface viewport; DevTool render uses
  // canvas Translate to land on the right-hand zone (see Update()).
  cfg.layout_context.viewport_width = devtool_width;
  cfg.layout_context.viewport_height =
      config_.surface ? static_cast<f32>(config_.surface->height()) : 0;
  cfg.layout_context.image_cache = &image_cache_;
  cfg.image_cache = &image_cache_;
  cfg.canvas = canvas_.get();
  cfg.event_manager = &event_manager_;
  devtool_update_manager_ = std::make_unique<UpdateManager>(cfg);
}

void Application::set_redaction_policy(dom::RedactionPolicy p) {
  redaction_policy_ = p;
  // Live-sync to an attached DevTool JS binding so a flip mid-session
  // applies on the next vx_devtool_get_dom_json() call. Safe when
  // unattached.
  if (devtool_dom_bindings_) {
    devtool_dom_bindings_->SetRedactionPolicy(p);
  }
}

#else  // VX_BUILD_DEVTOOL OFF — A14 zero-byte stub guards

bool Application::LoadDevtoolDocument(f32 /*devtool_width*/) { return false; }
void Application::UnloadDevtoolDocument() {}
void Application::EnsureDevtoolUpdateManager(f32 /*devtool_width*/) {}
StatusOr<std::string> Application::EvalDevtoolScript(StringView /*source*/,
                                                     StringView /*filename*/) {
  return Status(StatusCode::kInvalidArgument, "DevTool not built in");
}
void Application::set_redaction_policy(dom::RedactionPolicy p) {
  // No DevTool subsystem to sync — just store. set is reachable when
  // DEVTOOL=OFF because the C-API stub returns INVALID_STATE before
  // calling here, but we keep the field assignable for embedder code
  // that builds with DEVTOOL=ON in some configs and OFF in others.
  redaction_policy_ = p;
}

#endif  // VX_BUILD_DEVTOOL

}  // namespace vx
