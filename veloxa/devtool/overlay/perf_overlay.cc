#include "veloxa/devtool/overlay/perf_overlay.h"

namespace vx::devtool::overlay {

void PerfOverlay::RecordFrame(const FrameStats& stats) {
  frames_[head_] = stats;
  head_ = (head_ + 1) % kCapacity;
  if (count_ < kCapacity) count_++;
}

void PerfOverlay::Reset() {
  for (auto& f : frames_) f = FrameStats{};
  head_ = 0;
  count_ = 0;
}

FrameStats PerfOverlay::aggregated() const {
  if (count_ == 0) return FrameStats{};
  FrameStats sum{};
  for (usize i = 0; i < count_; i++) {
    sum.style_ms  += frames_[i].style_ms;
    sum.layout_ms += frames_[i].layout_ms;
    sum.render_ms += frames_[i].render_ms;
    sum.paint_ms  += frames_[i].paint_ms;
    sum.total_ms  += frames_[i].total_ms;
  }
  const f32 n = static_cast<f32>(count_);
  return {sum.style_ms / n, sum.layout_ms / n, sum.render_ms / n,
          sum.paint_ms / n, sum.total_ms / n};
}

f32 PerfOverlay::fps() const {
  const f32 t = aggregated().total_ms;
  if (t <= 0.001f) return 0.0f;     // div-by-zero guard
  const f32 raw = 1000.0f / t;
  return raw > 999.0f ? 999.0f : raw;  // HUD display-width cap
}

}  // namespace vx::devtool::overlay
