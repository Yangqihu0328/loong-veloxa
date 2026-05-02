#ifndef VELOXA_DEVTOOL_OVERLAY_PERF_OVERLAY_H_
#define VELOXA_DEVTOOL_OVERLAY_PERF_OVERLAY_H_

#include <array>

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
  ~PerfOverlay() = default;

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

 private:
  friend class PerfOverlayTest;  // plan §0.5 测试基础设施审计

  std::array<FrameStats, kCapacity> frames_{};
  usize head_ = 0;   // 下一个写入位置
  usize count_ = 0;  // [0, kCapacity]，达到 kCapacity 后停止增长
};

}  // namespace vx::devtool::overlay

#endif  // VELOXA_DEVTOOL_OVERLAY_PERF_OVERLAY_H_
