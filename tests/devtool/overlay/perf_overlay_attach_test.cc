#include "veloxa/devtool/overlay/perf_overlay.h"

#include <cstring>
#include <thread>

#include <gtest/gtest.h>

#include "veloxa/core/css/parser.h"
#include "veloxa/core/html/parser.h"
#include "veloxa/core/layout/text_shaper.h"
#include "veloxa/core/update_manager.h"
#include "veloxa/graphics/software/software_canvas.h"

namespace vx::devtool::overlay {

// =============================================================================
// perf_overlay_attach_test — TASK-20260502-02 B.1.2
//
// PerfOverlay::Attach(UpdateManager*) integration tests — installs 5
// PipelineHooks trampolines on real UpdateManager, validates:
//   1. single-instance attach (T6: reject 2nd attach to same/other UM)
//   2. T6 budget abort (callback total > budget_us → abort_count++)
//   3. auto-fill FrameStats on OnFrameEnd → frame_count grows
//   4. Detach correctly clears PipelineHooks
//   5. ClearDirtyRects called on each OnFrameStart (downstream B.2.3 contract)
// =============================================================================

constexpr u32 kW = 200, kH = 200, kStride = kW * 4;

class PerfOverlayAttachTest : public ::testing::Test {
 protected:
  void SetUp() override {
    doc_ = html::Parser::Parse("<div id='box'></div>");
    sheets_.push_back(css::CssParser::Parse(
        "#box { width: 100px; height: 100px; background-color: #0000ff; }"));
    std::memset(pixels_, 0, sizeof(pixels_));
    canvas_ = std::make_unique<gfx::sw::SoftwareCanvas>(pixels_, kW, kH,
                                                         kStride);
    canvas_->Begin();
    canvas_->Clear(gfx::Color::White());
    UpdateManager::Config cfg;
    cfg.document = doc_;
    cfg.stylesheets = &sheets_;
    cfg.layout_context.text_shaper = &shaper_;
    cfg.layout_context.viewport_width = kW;
    cfg.layout_context.viewport_height = kH;
    cfg.canvas = canvas_.get();
    um_ = std::make_unique<UpdateManager>(cfg);
  }

  dom::Document* doc_ = nullptr;
  Vector<css::Stylesheet> sheets_;
  layout::SimpleTextShaper shaper_;
  u32 pixels_[kW * kH];
  std::unique_ptr<gfx::sw::SoftwareCanvas> canvas_;
  std::unique_ptr<UpdateManager> um_;
};

TEST_F(PerfOverlayAttachTest, NotAttachedByDefault) {
  PerfOverlay overlay;
  EXPECT_FALSE(overlay.attached());
}

TEST_F(PerfOverlayAttachTest, AttachToValidUpdateManagerSucceeds) {
  PerfOverlay overlay;
  EXPECT_TRUE(overlay.Attach(um_.get()));
  EXPECT_TRUE(overlay.attached());
  // After attach, UpdateManager should hold the PerfOverlay's hooks.
  EXPECT_NE(um_->pipeline_hooks(), nullptr);
}

TEST_F(PerfOverlayAttachTest, AttachWithNullRejects) {
  PerfOverlay overlay;
  EXPECT_FALSE(overlay.Attach(nullptr));
  EXPECT_FALSE(overlay.attached());
  EXPECT_FALSE(overlay.last_attach_error().empty());
}

TEST_F(PerfOverlayAttachTest, AttachTwiceRejectsSecond) {
  PerfOverlay overlay;
  EXPECT_TRUE(overlay.Attach(um_.get()));
  // T6 single-instance contract: 2nd attach must fail (even to same UM).
  EXPECT_FALSE(overlay.Attach(um_.get()));
  EXPECT_TRUE(overlay.attached());  // 1st attach unchanged
  EXPECT_FALSE(overlay.last_attach_error().empty());
}

TEST_F(PerfOverlayAttachTest, DetachClearsHooks) {
  PerfOverlay overlay;
  ASSERT_TRUE(overlay.Attach(um_.get()));
  ASSERT_NE(um_->pipeline_hooks(), nullptr);
  overlay.Detach();
  EXPECT_FALSE(overlay.attached());
  EXPECT_EQ(um_->pipeline_hooks(), nullptr);
}

TEST_F(PerfOverlayAttachTest, AttachAfterDetachSucceeds) {
  PerfOverlay overlay;
  ASSERT_TRUE(overlay.Attach(um_.get()));
  overlay.Detach();
  EXPECT_TRUE(overlay.Attach(um_.get()));  // re-attach OK after detach
}

TEST_F(PerfOverlayAttachTest, OnFrameEndAutoFillsFrameStats) {
  PerfOverlay overlay;
  ASSERT_TRUE(overlay.Attach(um_.get()));
  EXPECT_EQ(overlay.frame_count(), 0u);

  um_->Update();  // 1 frame

  EXPECT_EQ(overlay.frame_count(), 1u);
  // Times should be non-negative (real measurement).
  FrameStats agg = overlay.aggregated();
  EXPECT_GE(agg.total_ms, 0.0f);
  EXPECT_GE(agg.layout_ms, 0.0f);
  EXPECT_GE(agg.render_ms, 0.0f);
}

TEST_F(PerfOverlayAttachTest, MultipleFramesAccumulateInRing) {
  PerfOverlay overlay;
  ASSERT_TRUE(overlay.Attach(um_.get()));
  for (int i = 0; i < 5; i++) {
    um_->Invalidate();
    um_->Update();
  }
  EXPECT_EQ(overlay.frame_count(), 5u);
}

TEST_F(PerfOverlayAttachTest, OnFrameStartClearsDirtyRectsAccumulator) {
  // B.0.2 dirty_rects_ Vector — PerfOverlay should clear it each frame
  // (B.2.3 InjectDirtyRectHighlights consumes per-frame snapshot).
  PerfOverlay overlay;
  ASSERT_TRUE(overlay.Attach(um_.get()));
  um_->Update();  // 1st frame: dirty_rects_ size 1 (inside Update),
                  // then 2nd frame OnFrameStart clears it before recompute.
  // After 1st update: should still have current frame's dirty rect.
  EXPECT_GE(um_->dirty_rects().size(), 1u);

  // 2nd update: PerfOverlay's OnFrameStart hook clears prior; current
  // update has no visual change → empty rect → not pushed → size = 0.
  um_->Invalidate();
  um_->Update();
  EXPECT_EQ(um_->dirty_rects().size(), 0u);
}

TEST_F(PerfOverlayAttachTest, BudgetGuardDefaultIs1MsAllowsNormalFrame) {
  PerfOverlay overlay;
  EXPECT_EQ(overlay.budget_us(), 1000u);  // default 1ms
  ASSERT_TRUE(overlay.Attach(um_.get()));
  um_->Update();  // normal frame << 1ms hook overhead
  EXPECT_EQ(overlay.abort_count(), 0u);
  EXPECT_TRUE(overlay.last_abort_reason().empty());
}

TEST_F(PerfOverlayAttachTest, BudgetGuardZeroAlwaysAborts) {
  // Set budget to 0 → any non-zero hook overhead exceeds → abort.
  // (Even our trampolines themselves take >0 ns to run.)
  PerfOverlay overlay;
  overlay.set_budget_us(0);
  ASSERT_TRUE(overlay.Attach(um_.get()));
  um_->Update();
  EXPECT_GT(overlay.abort_count(), 0u);
  EXPECT_FALSE(overlay.last_abort_reason().empty());
}

TEST_F(PerfOverlayAttachTest, ResetClearsAbortCount) {
  PerfOverlay overlay;
  overlay.set_budget_us(0);
  ASSERT_TRUE(overlay.Attach(um_.get()));
  um_->Update();
  EXPECT_GT(overlay.abort_count(), 0u);
  overlay.Reset();
  EXPECT_EQ(overlay.abort_count(), 0u);
  EXPECT_TRUE(overlay.last_abort_reason().empty());
}

}  // namespace vx::devtool::overlay
