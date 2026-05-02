#include "veloxa/devtool/overlay/perf_overlay.h"

#include <chrono>

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
  abort_count_ = 0;
  last_abort_reason_.clear();
  frame_total_us_ = 0;
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

// ===========================================================================
// B.1.2 — Attach + 5 trampolines + T6 budget guard
// ===========================================================================

bool PerfOverlay::Attach(UpdateManager* update_manager) {
  if (!update_manager) {
    last_attach_error_ = "Attach called with null UpdateManager";
    return false;
  }
  if (attached_to_) {
    // T6 single-instance contract: refuse second attach (ambiguous
    // double-trampoline scenarios). Caller must Detach() first.
    last_attach_error_ = "PerfOverlay already attached; call Detach() first";
    return false;
  }

  hooks_storage_.on_frame_start  = &PerfOverlay::TrampolineFrameStart;
  hooks_storage_.on_after_style  = &PerfOverlay::TrampolineAfterStyle;
  hooks_storage_.on_after_layout = &PerfOverlay::TrampolineAfterLayout;
  hooks_storage_.on_after_render = &PerfOverlay::TrampolineAfterRender;
  hooks_storage_.on_frame_end    = &PerfOverlay::TrampolineFrameEnd;
  hooks_storage_.userdata        = this;

  update_manager->SetPipelineHooks(&hooks_storage_);
  attached_to_ = update_manager;
  last_attach_error_.clear();
  return true;
}

void PerfOverlay::Detach() {
  if (attached_to_) {
    attached_to_->SetPipelineHooks(nullptr);
    attached_to_ = nullptr;
  }
}

void PerfOverlay::TrampolineFrameStart(void* ud) {
  static_cast<PerfOverlay*>(ud)->HandleHook(0);
}
void PerfOverlay::TrampolineAfterStyle(void* ud) {
  static_cast<PerfOverlay*>(ud)->HandleHook(1);
}
void PerfOverlay::TrampolineAfterLayout(void* ud) {
  static_cast<PerfOverlay*>(ud)->HandleHook(2);
}
void PerfOverlay::TrampolineAfterRender(void* ud) {
  static_cast<PerfOverlay*>(ud)->HandleHook(3);
}
void PerfOverlay::TrampolineFrameEnd(void* ud) {
  static_cast<PerfOverlay*>(ud)->HandleHook(4);
}

void PerfOverlay::HandleHook(usize hook_idx) {
  using clock = css::SteadyClock;
  using std::chrono::duration_cast;
  using std::chrono::microseconds;

  const auto t_start = clock::now();
  hook_times_[hook_idx] = t_start;

  if (hook_idx == 0) {
    // OnFrameStart — clear per-frame state and the dirty rect accumulator.
    frame_total_us_ = 0;
    if (attached_to_) attached_to_->ClearDirtyRects();
  } else if (hook_idx == 4) {
    // OnFrameEnd — compute FrameStats from 5 timestamps and record.
    auto to_ms = [](css::SteadyTimePoint a, css::SteadyTimePoint b) -> f32 {
      const auto dt = duration_cast<std::chrono::nanoseconds>(b - a).count();
      return static_cast<f32>(dt) / 1.0e6f;
    };
    FrameStats stats;
    stats.style_ms  = to_ms(hook_times_[0], hook_times_[1]);
    stats.layout_ms = to_ms(hook_times_[1], hook_times_[2]);
    stats.render_ms = to_ms(hook_times_[2], hook_times_[3]);
    stats.paint_ms  = to_ms(hook_times_[3], hook_times_[4]);
    stats.total_ms  = to_ms(hook_times_[0], hook_times_[4]);
    RecordFrame(stats);
  }

  // T6 budget guard — accumulate THIS callback's own overhead
  // (trampoline + handler work). We sample again here (after handler
  // body) to bound the cost we ourselves added to UpdateManager::Update.
  // budget_us_ == 0 is treated as "hooks disabled / always abort"
  // — explicit semantic guard rather than relying on cb_us > 0
  // (which may round down to 0 under sub-microsecond hardware timer).
  const auto t_end = clock::now();
  const u64 cb_us = static_cast<u64>(
      duration_cast<microseconds>(t_end - t_start).count());
  frame_total_us_ += cb_us;
  if (budget_us_ == 0 || frame_total_us_ > budget_us_) {
    abort_count_++;
    last_abort_reason_ =
        "PerfOverlay hooks exceeded budget (" +
        std::to_string(frame_total_us_) + " us > " +
        std::to_string(budget_us_) + " us budget); T6 guard triggered";
  }
}

}  // namespace vx::devtool::overlay
