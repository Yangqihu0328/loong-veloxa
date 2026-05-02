#include "veloxa/devtool/overlay/perf_overlay.h"

#include <cmath>

#include <gtest/gtest.h>

namespace vx::devtool::overlay {

// =============================================================================
// perf_overlay_test — TASK-20260502-02 B.1.1
//
// PerfOverlay FrameStats ring buffer + 60 帧滑动聚合 + FPS 计算。纯算法
// 单元测，零外部依赖（不 attach UpdateManager — 那是 B.1.2 的范围）。
// friend class PerfOverlayTest 白名单访问 ring buffer 内部状态（plan §0.5
// 测试基础设施审计）。
// =============================================================================

class PerfOverlayTest : public ::testing::Test {
 protected:
  PerfOverlay overlay_;
};

TEST_F(PerfOverlayTest, InitialFrameCountIsZero) {
  EXPECT_EQ(overlay_.frame_count(), 0u);
}

TEST_F(PerfOverlayTest, RecordOneFramePopulatesRingBuffer) {
  FrameStats stats{0.5f, 1.0f, 2.0f, 1.5f, 5.0f};  // total = 5ms
  overlay_.RecordFrame(stats);
  EXPECT_EQ(overlay_.frame_count(), 1u);
  EXPECT_FLOAT_EQ(overlay_.aggregated().total_ms, 5.0f);
  EXPECT_FLOAT_EQ(overlay_.aggregated().style_ms, 0.5f);
}

TEST_F(PerfOverlayTest, SixtyFramesAggregatesAverages) {
  for (int i = 0; i < 60; i++) {
    overlay_.RecordFrame({1.0f, 2.0f, 3.0f, 4.0f, 10.0f});  // total = 10ms
  }
  EXPECT_EQ(overlay_.frame_count(), 60u);
  EXPECT_FLOAT_EQ(overlay_.aggregated().total_ms, 10.0f);
  EXPECT_FLOAT_EQ(overlay_.aggregated().style_ms, 1.0f);
  EXPECT_FLOAT_EQ(overlay_.aggregated().layout_ms, 2.0f);
  EXPECT_FLOAT_EQ(overlay_.aggregated().render_ms, 3.0f);
  EXPECT_FLOAT_EQ(overlay_.aggregated().paint_ms, 4.0f);
  EXPECT_NEAR(overlay_.fps(), 100.0f, 0.01f);  // 1000 / 10
}

TEST_F(PerfOverlayTest, RingBufferOverwritesOldestFrameAfter60) {
  // First 60 frames: total = 10ms each
  for (int i = 0; i < 60; i++) {
    overlay_.RecordFrame({0, 0, 0, 0, 10.0f});
  }
  // 61st frame: total = 20ms → overwrites slot 0
  overlay_.RecordFrame({0, 0, 0, 0, 20.0f});
  EXPECT_EQ(overlay_.frame_count(), 60u);  // capped at 60

  // Average: (59 * 10 + 20) / 60 = 610 / 60 = 10.1666...
  EXPECT_NEAR(overlay_.aggregated().total_ms, 10.1667f, 0.01f);
}

TEST_F(PerfOverlayTest, ZeroTotalMsGuardedAgainstDivByZero) {
  overlay_.RecordFrame({0, 0, 0, 0, 0.0f});
  // FPS guard: returns 0.0f when avg total <= 0.001 (avoid +inf)
  EXPECT_FALSE(std::isinf(overlay_.fps()));
  EXPECT_FALSE(std::isnan(overlay_.fps()));
  EXPECT_FLOAT_EQ(overlay_.fps(), 0.0f);
}

TEST_F(PerfOverlayTest, FpsCappedAt999) {
  // 0.5ms frame → raw FPS = 2000 → cap at 999
  overlay_.RecordFrame({0, 0, 0, 0, 0.5f});
  EXPECT_FLOAT_EQ(overlay_.fps(), 999.0f);
}

TEST_F(PerfOverlayTest, ResetClearsRingBuffer) {
  overlay_.RecordFrame({1, 1, 1, 1, 4.0f});
  EXPECT_EQ(overlay_.frame_count(), 1u);
  overlay_.Reset();
  EXPECT_EQ(overlay_.frame_count(), 0u);
  EXPECT_FLOAT_EQ(overlay_.aggregated().total_ms, 0.0f);
}

TEST_F(PerfOverlayTest, AggregatedOnEmptyReturnsZero) {
  FrameStats agg = overlay_.aggregated();
  EXPECT_FLOAT_EQ(agg.style_ms, 0.0f);
  EXPECT_FLOAT_EQ(agg.layout_ms, 0.0f);
  EXPECT_FLOAT_EQ(agg.render_ms, 0.0f);
  EXPECT_FLOAT_EQ(agg.paint_ms, 0.0f);
  EXPECT_FLOAT_EQ(agg.total_ms, 0.0f);
}

}  // namespace vx::devtool::overlay
