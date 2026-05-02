#ifndef VELOXA_DEVTOOL_OVERLAY_PERF_OVERLAY_H_
#define VELOXA_DEVTOOL_OVERLAY_PERF_OVERLAY_H_

#include <array>
#include <string>

#include "veloxa/core/css/transition.h"  // SteadyTimePoint
#include "veloxa/core/update_manager.h"  // PipelineHooks + UpdateManager
#include "veloxa/foundation/base/types.h"

namespace vx::devtool::overlay {

// TASK-20260502-02 B.1.1 — Performance Overlay FrameStats 5 字段
// (与 spec §5.1 + B.0.1 五钩子时间差对齐).
//
// 字段公式（PerfOverlay::Attach 在 OnFrameStart / OnAfterStyle / OnAfterLayout /
// OnAfterRender / OnFrameEnd 5 钩子内记录 SteadyTimePoint，本帧 OnFrameEnd
// 计算并 RecordFrame）：
//   style_ms  = AfterStyle  - FrameStart
//   layout_ms = AfterLayout - AfterStyle
//   render_ms = AfterRender - AfterLayout
//   paint_ms  = FrameEnd    - AfterRender
//   total_ms  = FrameEnd    - FrameStart
//
// 注意：当前 LayoutEngine::Layout 含 style resolve → AfterStyle 与 AfterLayout
// 同点触发（style_ms ≈ 0；layout_ms = 整 LayoutEngine 总时长）。子阶段拆分
// 留 #35 阶段 2 P3 候选。
struct FrameStats {
  f32 style_ms  = 0.0f;
  f32 layout_ms = 0.0f;
  f32 render_ms = 0.0f;
  f32 paint_ms  = 0.0f;
  f32 total_ms  = 0.0f;
};

// PerfOverlay — 滑动 60 帧聚合 + ring buffer + FPS 计算。纯算法层面；
// 与 UpdateManager::PipelineHooks 协同（B.1.2 PerfOverlay::Attach 模型）。
//
// 容量 60 = 1 秒 @ 60 FPS（spec A6 滑动 60 帧均值）；超出后 ring buffer
// 覆盖最旧 frame（head_ 自增 mod 60）。frame_count() 在前 60 帧线性增长
// 后封顶到 60。
class PerfOverlay {
 public:
  static constexpr usize kCapacity = 60;

  PerfOverlay() = default;
  // Safety net: if still attached at destruction, automatically detach
  // (clear PipelineHooks on UpdateManager). Caller is encouraged to
  // call Detach() explicitly before destroying the bound UpdateManager
  // — destruction order matters; if UpdateManager dies first, the
  // attached_to_ pointer is already dangling.
  ~PerfOverlay() { Detach(); }

  PerfOverlay(const PerfOverlay&) = delete;
  PerfOverlay& operator=(const PerfOverlay&) = delete;

  // 写入 1 帧（ring buffer push）。
  void RecordFrame(const FrameStats& stats);

  // 清零 ring buffer + head_ + count_。Reset 后 frame_count() == 0
  // 且 aggregated() 全字段返 0。
  void Reset();

  usize frame_count() const { return count_; }

  // 滑动均值（仅取已写入的 [0, count_) 项；count_ == 0 时返 zero
  // FrameStats）。
  FrameStats aggregated() const;

  // FPS = 1000 / aggregated().total_ms，含两道守卫：
  //   - total_ms <= 0.001 → 返 0.0f（避免 +inf）
  //   - 计算结果 > 999 → cap 至 999.0f（HUD UI 显示位数限制）
  f32 fps() const;

  // TASK-20260502-02 B.1.2 — Attach to a UpdateManager. Installs 5
  // PipelineHooks trampolines (this 透传 userdata) so PerfOverlay can:
  //   - record SteadyTimePoint at each of the 5 hook points
  //   - compute FrameStats and call RecordFrame() at OnFrameEnd
  //   - clear UpdateManager::dirty_rects_ at OnFrameStart so B.2.3
  //     InjectDirtyRectHighlights consumes a clean per-frame snapshot
  //   - enforce T6 budget guard (total hook overhead per frame <
  //     budget_us; abort_count_ + last_abort_reason_ on overflow)
  //
  // Single-instance contract (T6 mitigation): if PerfOverlay is already
  // attached, returns false + sets last_attach_error_. This prevents
  // ambiguous double-trampoline scenarios.
  //
  // Returns false on null update_manager OR already-attached state.
  bool Attach(UpdateManager* update_manager);
  void Detach();
  bool attached() const { return attached_to_ != nullptr; }

  // T6 budget guard knobs.
  void set_budget_us(u64 budget_us) { budget_us_ = budget_us; }
  u64 budget_us() const { return budget_us_; }
  usize abort_count() const { return abort_count_; }
  const std::string& last_abort_reason() const { return last_abort_reason_; }
  const std::string& last_attach_error() const { return last_attach_error_; }

 private:
  friend class PerfOverlayTest;  // plan §0.5 测试基础设施审计

  // 5 trampolines (C function pointers — this passed as userdata).
  static void TrampolineFrameStart(void* ud);
  static void TrampolineAfterStyle(void* ud);
  static void TrampolineAfterLayout(void* ud);
  static void TrampolineAfterRender(void* ud);
  static void TrampolineFrameEnd(void* ud);

  // Records timestamp at hook index [0..4] and accumulates per-frame
  // hook overhead for budget guard.
  void HandleHook(usize hook_idx);

  std::array<FrameStats, kCapacity> frames_{};
  usize head_ = 0;
  usize count_ = 0;

  // B.1.2 Attach + budget guard state.
  UpdateManager* attached_to_ = nullptr;
  PipelineHooks hooks_storage_{};
  std::array<css::SteadyTimePoint, 5> hook_times_{};
  u64 frame_total_us_ = 0;  // accumulated hook overhead within a frame
  u64 budget_us_ = 1000;    // 1ms default (T6 spec §7)
  usize abort_count_ = 0;
  std::string last_abort_reason_;
  std::string last_attach_error_;
};

}  // namespace vx::devtool::overlay

#endif  // VELOXA_DEVTOOL_OVERLAY_PERF_OVERLAY_H_
