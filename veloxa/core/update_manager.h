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

// TASK-20260502-02 B.0.1 — Performance Overlay 五钩子（技术债 #35 阶段 1 闭环）。
// callback 在 UpdateManager::Update() 内严格按下面顺序触发：
//   on_frame_start  → 入口（dirty/config check 之后）
//   on_after_style  → DetectAndApplyTransitions() 之后
//   on_after_layout → 紧跟（同点触发；当前 LayoutEngine::Layout 含 style resolve，
//                     style/layout 子阶段拆分留 #35 阶段 2 P3 候选）
//   on_after_render → render::Record() 之后（PaintCommand 录制完成）
//   on_frame_end    → Update() 末尾（Replay 之后）
//
// userdata 字段在每个 callback 调用时透传（PerfOverlay::Attach 时传 this 指针给
// 5 trampoline 用）。全部 callback 为 nullptr 时，分支预测器优化路径开销 ~0
// （每钩子 1 个 nullptr check 跳过）。UpdateManager 不持 PipelineHooks 所有权 —
// 调用方（PerfOverlay）保证 hooks 生命周期 ≥ UpdateManager 持有期。
struct PipelineHooks {
  using Callback = void(*)(void* userdata);
  Callback on_frame_start  = nullptr;
  Callback on_after_style  = nullptr;
  Callback on_after_layout = nullptr;
  Callback on_after_render = nullptr;
  Callback on_frame_end    = nullptr;
  void* userdata = nullptr;
};

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
  // TASK-20260502-01 A.1.6 — exposed so DevTool overlays (e.g. A.1.1
  // InspectorOverlay::InjectHoverHighlight) can append PaintCommands to
  // the live frame's DisplayList from outside UpdateManager. Caller
  // must respect the kOverlayHighlight reset contract (T5 mitigation:
  // ResetOverlayCommands runs at frame boundary, see Renderer/Replay).
  render::DisplayList& mutable_display_list() { return display_list_; }
  gfx::Rect last_dirty_rect() const { return last_dirty_rect_; }
  css::TransitionManager& transition_manager() { return transition_mgr_; }

  // TASK-20260502-02 B.0.1 — set hooks (nullptr to clear).
  void SetPipelineHooks(const PipelineHooks* hooks) {
    pipeline_hooks_ = hooks;
  }
  const PipelineHooks* pipeline_hooks() const { return pipeline_hooks_; }

  // TASK-20260502-02 B.0.2 — Vector<gfx::Rect> 累积同帧 / 同 PerfOverlay 周期
  // 内多次 Update() 产生的非空 dirty rect（empty rect 不 push，与既有
  // last_dirty_rect_ empty 语义一致）。Performance Overlay 用：
  //   - PerfOverlay::OnFrameStart hook 调 ClearDirtyRects() 每帧清零
  //   - InspectorOverlay::InjectDirtyRectHighlights(display_list, dirty_rects)
  //     按本帧累积的 dirty rects 注入边框 PaintCommand（B.2.3）
  // last_dirty_rect_ 的 empty 测试契约 100% 保持（独立字段 — ClearDirtyRects
  // 不影响 last_dirty_rect_）。
  const Vector<gfx::Rect>& dirty_rects() const { return dirty_rects_; }
  void ClearDirtyRects() { dirty_rects_.clear(); }

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
  Vector<gfx::Rect> dirty_rects_;  // B.0.2 — 累积非空 dirty rect
  css::TransitionManager transition_mgr_;
  const PipelineHooks* pipeline_hooks_ = nullptr;  // B.0.1

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
