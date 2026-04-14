#ifndef VELOXA_CORE_UPDATE_MANAGER_H_
#define VELOXA_CORE_UPDATE_MANAGER_H_

#include "veloxa/core/css/transition.h"
#include "veloxa/core/event/event_manager.h"
#include "veloxa/core/layout/layout_engine.h"
#include "veloxa/core/render/paint_command.h"
#include "veloxa/core/render/renderer.h"
#include "veloxa/foundation/memory/arena_allocator.h"
#include "veloxa/graphics/canvas.h"

namespace vx {

class UpdateManager {
 public:
  struct Config {
    dom::Document* document = nullptr;
    const Vector<css::Stylesheet>* stylesheets = nullptr;
    layout::LayoutContext layout_context;
    gfx::Canvas* canvas = nullptr;
    event::EventManager* event_manager = nullptr;
    image::ImageCache* image_cache = nullptr;
  };

  explicit UpdateManager(const Config& config);

  void Invalidate();
  void Update();

  bool is_dirty() const { return dirty_; }
  layout::LayoutBox* layout_root() const { return layout_root_; }
  const render::DisplayList& display_list() const { return display_list_; }
  gfx::Rect last_dirty_rect() const { return last_dirty_rect_; }
  css::TransitionManager& transition_manager() { return transition_mgr_; }

 private:
  void DetectAndApplyTransitions();
  void TraverseForTransitions(layout::LayoutBox* box,
                               css::SteadyTimePoint now);

  Config config_;
  bool dirty_ = true;
  ArenaAllocator arena_{8192};
  layout::LayoutBox* layout_root_ = nullptr;
  render::DisplayList display_list_;
  gfx::Rect last_dirty_rect_{};
  css::TransitionManager transition_mgr_;

  struct StylePtrHash {
    usize operator()(const void* p) const {
      return reinterpret_cast<usize>(p);
    }
  };
  struct StylePtrEq {
    bool operator()(const void* a, const void* b) const { return a == b; }
  };
  HashMap<const void*, css::ComputedStyle, StylePtrHash, StylePtrEq>
      prev_styles_;
};

}  // namespace vx

#endif  // VELOXA_CORE_UPDATE_MANAGER_H_
