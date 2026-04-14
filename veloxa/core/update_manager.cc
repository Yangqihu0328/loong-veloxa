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

  DetectAndApplyTransitions();

  render::DisplayList new_list;
  if (layout_root_) {
    new_list = render::Record(layout_root_, config_.image_cache);
  }

  last_dirty_rect_ = render::ComputeDirtyRect(
      display_list_, new_list, config_.layout_context.viewport_width,
      config_.layout_context.viewport_height);

  if (config_.canvas && !last_dirty_rect_.IsEmpty()) {
    config_.canvas->PushClipRect(last_dirty_rect_);
    config_.canvas->FillRect(last_dirty_rect_,
                             gfx::Brush::Solid(gfx::Color::White()));
    render::Replay(new_list, config_.canvas, config_.image_cache);
    config_.canvas->PopClip();
  }

  display_list_ = std::move(new_list);
  dirty_ = false;

  if (transition_mgr_.HasActive()) {
    dirty_ = true;
  }
}

void UpdateManager::DetectAndApplyTransitions() {
  if (!layout_root_) return;
  auto now = css::SteadyClock::now();
  TraverseForTransitions(layout_root_, now);
  transition_mgr_.Tick(now);
}

void UpdateManager::TraverseForTransitions(layout::LayoutBox* box,
                                            css::SteadyTimePoint now) {
  if (box->element && box->style) {
    if (!box->style->transitions.empty()) {
      const void* key = static_cast<const void*>(box->element);
      const css::ComputedStyle* prev = prev_styles_.Find(key);
      if (prev) {
        transition_mgr_.OnStyleChange(box->element, *prev, *box->style, now);
      }
      prev_styles_.Insert(key, *box->style);
    }

    auto* mutable_style =
        const_cast<css::ComputedStyle*>(box->style);
    transition_mgr_.ApplyTo(box->element, *mutable_style);
  }

  for (auto* child = box->first_child; child; child = child->next_sibling) {
    TraverseForTransitions(child, now);
  }
}

}  // namespace vx
