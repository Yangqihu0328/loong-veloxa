#include "veloxa/core/application.h"

#include <cstring>
#include <string>

#include "veloxa/core/html/parser.h"
#include "veloxa/graphics/software/software_canvas.h"
#include "veloxa/script/dom_bindings.h"
#include "veloxa/script/quickjs_engine.h"
#include "veloxa/text/freetype_shaper.h"

#ifdef VX_BUILD_DEVTOOL
#include "veloxa/devtool/resources/inspector_resources.h"
#endif

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
}

Application::~Application() {
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
  EnsureUpdateManager();
  if (update_manager_) {
    event_manager_.HandleInput(input, update_manager_->layout_root());
  }
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
  return owned_devtool_document_ != nullptr;
}

void Application::UnloadDevtoolDocument() {
  // Tear down UpdateManager first so it stops referencing the Document.
  devtool_update_manager_.reset();
  // Only clear the slot if WE owned the Document. External attach paths
  // (devtool_document_ set without populating owned_) keep their slot
  // intact — caller is responsible for clearing.
  if (owned_devtool_document_) {
    devtool_document_ = nullptr;
    owned_devtool_document_.reset();
  }
  devtool_width_ = 0.0f;
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

#else  // VX_BUILD_DEVTOOL OFF — A14 zero-byte stub guards

bool Application::LoadDevtoolDocument(f32 /*devtool_width*/) { return false; }
void Application::UnloadDevtoolDocument() {}
void Application::EnsureDevtoolUpdateManager(f32 /*devtool_width*/) {}

#endif  // VX_BUILD_DEVTOOL

}  // namespace vx
