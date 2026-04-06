#include "veloxa/core/application.h"

#include "veloxa/core/html/parser.h"
#include "veloxa/graphics/software/software_canvas.h"

namespace vx {

Application::Application(const Config& config) : config_(config) {
  if (config_.surface) {
    surface_pixels_ = config_.surface->Lock();
    canvas_ = std::make_unique<gfx::sw::SoftwareCanvas>(
        surface_pixels_, config_.surface->width(), config_.surface->height(),
        config_.surface->stride());
    canvas_->Begin();
    canvas_->Clear(config_.background_color);
  }
}

Application::~Application() {
  if (config_.surface && surface_pixels_) {
    config_.surface->Unlock();
  }
  delete document_;
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
}

void Application::OnFrame() { Update(); }

void Application::EnsureUpdateManager() {
  if (update_manager_ || !document_ || !canvas_) return;
  UpdateManager::Config cfg;
  cfg.document = document_;
  cfg.stylesheets = &stylesheets_;
  cfg.layout_context.text_shaper = &text_shaper_;
  cfg.layout_context.viewport_width =
      config_.surface ? static_cast<f32>(config_.surface->width()) : 0;
  cfg.layout_context.viewport_height =
      config_.surface ? static_cast<f32>(config_.surface->height()) : 0;
  cfg.canvas = canvas_.get();
  cfg.event_manager = &event_manager_;
  update_manager_ = std::make_unique<UpdateManager>(cfg);
}

}  // namespace vx
