#include "veloxa/core/application.h"

#include "veloxa/core/html/parser.h"
#include "veloxa/graphics/software/software_canvas.h"
#include "veloxa/script/dom_bindings.h"
#include "veloxa/script/quickjs_engine.h"
#include "veloxa/text/freetype_shaper.h"

namespace vx {

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
  delete document_;
  font_manager_.Shutdown();
}

void Application::LoadHTML(StringView html) {
  delete document_;
  document_ = html::Parser::Parse(html);
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
  if (!document_) {
    return Status(StatusCode::kInvalidArgument, "No document loaded");
  }
  if (!script_engine_) {
    script_engine_ = std::make_unique<script::QuickjsEngine>();
    auto st = script_engine_->Init();
    if (!st.ok()) return st;
  }
  if (!dom_bindings_) {
    dom_bindings_ = std::make_unique<script::DomBindings>();
    dom_bindings_->Bind(script_engine_->context(), document_, &event_manager_);
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
  // Push the rendered buffer to the display backend at end of every frame
  // (no-op for headless surfaces; SDL2 backends UpdateTexture +
  // RenderPresent). Idempotent: backends may be called multiple times per
  // logical frame without issues.
  if (config_.surface) {
    config_.surface->Present();
  }
}

void Application::OnFrame() { Update(); }

void Application::EnsureUpdateManager() {
  if (update_manager_ || !document_ || !canvas_) return;
  UpdateManager::Config cfg;
  cfg.document = document_;
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

}  // namespace vx
