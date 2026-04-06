#include "veloxa/core/update_manager.h"

namespace vx {

UpdateManager::UpdateManager(const Config& config) : config_(config) {
  if (config_.event_manager) {
    config_.event_manager->SetInvalidationCallback(
        [this]() { Invalidate(); });
  }
  config_.layout_context.event_manager = config_.event_manager;
  config_.layout_context.stylesheets = config_.stylesheets;
}

void UpdateManager::Invalidate() { dirty_ = true; }

void UpdateManager::Update() {
  if (!dirty_ || !config_.document) return;

  arena_.Reset();
  layout_root_ = layout::LayoutEngine::Layout(config_.document,
                                              config_.layout_context, arena_);

  render::DisplayList new_list;
  if (layout_root_) {
    new_list = render::Record(layout_root_);
  }

  last_dirty_rect_ = render::ComputeDirtyRect(
      display_list_, new_list, config_.layout_context.viewport_width,
      config_.layout_context.viewport_height);

  if (config_.canvas && !last_dirty_rect_.IsEmpty()) {
    config_.canvas->PushClipRect(last_dirty_rect_);
    config_.canvas->FillRect(last_dirty_rect_,
                             gfx::Brush::Solid(gfx::Color::White()));
    render::Replay(new_list, config_.canvas);
    config_.canvas->PopClip();
  }

  display_list_ = std::move(new_list);
  dirty_ = false;
}

}  // namespace vx
