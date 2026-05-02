#include "veloxa/api/veloxa_api.h"

#include <atomic>

#include <gtest/gtest.h>

namespace {

// =============================================================================
// perf_hooks_api_test — TASK-20260502-02 B.0.1 (#35 phase 1 closure)
//
// vx_view_set_pipeline_hooks thin C wrapper around
// Application::SetPipelineHooks → UpdateManager::SetPipelineHooks.
// Public ABI is intentionally available regardless of VX_BUILD_DEVTOOL —
// profiler / tracing embedders can install hooks without DevTool.
// =============================================================================

constexpr uint32_t kW = 200, kH = 200;

class PerfHooksApiTest : public ::testing::Test {
 protected:
  void SetUp() override {
    loop_ = vx_event_loop_create_headless();
    surface_ = vx_surface_create_memory(kW, kH);

    VxViewConfig cfg{};
    cfg.event_loop = loop_;
    cfg.surface = surface_;
    cfg.target_fps = 60;
    view_ = vx_view_create(&cfg);
  }

  void TearDown() override {
    if (view_) vx_view_destroy(view_);
    if (surface_) vx_surface_destroy(surface_);
    if (loop_) vx_event_loop_destroy(loop_);
  }

  VxEventLoop* loop_ = nullptr;
  VxSurface* surface_ = nullptr;
  VxView* view_ = nullptr;
};

// -----------------------------------------------------------------------------
// Always-on tests (NULL/INVALID_STATE) — independent of build flag
// -----------------------------------------------------------------------------

TEST_F(PerfHooksApiTest, SetHooksWithNullViewReturnsNullParam) {
  EXPECT_EQ(vx_view_set_pipeline_hooks(nullptr, nullptr, nullptr),
            VX_ERROR_NULL_PARAM);
}

TEST_F(PerfHooksApiTest, ClearHooksOnFreshViewReturnsInvalidState) {
  // No LoadHTML / Update has been called → update_manager_ is null →
  // hooks are cached but lazy attach pending → return INVALID_STATE.
  EXPECT_EQ(vx_view_set_pipeline_hooks(view_, nullptr, nullptr),
            VX_ERROR_INVALID_STATE);
}

// -----------------------------------------------------------------------------
// Static recorder for end-to-end hook firing tests (C function pointer
// constraint — no lambda capture).
// -----------------------------------------------------------------------------

namespace {
std::atomic<int> g_api_hook_calls[5]{};
void OnFrameStartCb(void*) { g_api_hook_calls[0]++; }
void OnAfterStyleCb(void*) { g_api_hook_calls[1]++; }
void OnAfterLayoutCb(void*) { g_api_hook_calls[2]++; }
void OnAfterRenderCb(void*) { g_api_hook_calls[3]++; }
void OnFrameEndCb(void*) { g_api_hook_calls[4]++; }
void ResetCalls() {
  for (auto& c : g_api_hook_calls) c.store(0);
}
}  // namespace

TEST_F(PerfHooksApiTest, AfterUpdateLazyAttachInstallsHooks) {
  // 1. Set hooks BEFORE any update — should return INVALID_STATE but cache.
  ResetCalls();
  VxPipelineHooks hooks{};
  hooks.on_frame_start  = &OnFrameStartCb;
  hooks.on_after_style  = &OnAfterStyleCb;
  hooks.on_after_layout = &OnAfterLayoutCb;
  hooks.on_after_render = &OnAfterRenderCb;
  hooks.on_frame_end    = &OnFrameEndCb;
  EXPECT_EQ(vx_view_set_pipeline_hooks(view_, &hooks, nullptr),
            VX_ERROR_INVALID_STATE);
  EXPECT_EQ(g_api_hook_calls[0].load(), 0);  // not fired yet

  // 2. Trigger first update (LoadHTML → EnsureUpdateManager → lazy attach).
  EXPECT_EQ(vx_view_load_html(view_, "<div></div>", 11), VX_OK);
  EXPECT_EQ(vx_view_update(view_), VX_OK);

  // 3. All 5 callbacks should have fired exactly once.
  EXPECT_EQ(g_api_hook_calls[0].load(), 1);
  EXPECT_EQ(g_api_hook_calls[1].load(), 1);
  EXPECT_EQ(g_api_hook_calls[2].load(), 1);
  EXPECT_EQ(g_api_hook_calls[3].load(), 1);
  EXPECT_EQ(g_api_hook_calls[4].load(), 1);

  // 4. Clear hooks → next update fires nothing.
  EXPECT_EQ(vx_view_set_pipeline_hooks(view_, nullptr, nullptr), VX_OK);
  EXPECT_EQ(vx_view_load_css(view_, "div { width: 50px; }", 20), VX_OK);
  EXPECT_EQ(vx_view_update(view_), VX_OK);
  EXPECT_EQ(g_api_hook_calls[0].load(), 1);  // unchanged
}

TEST_F(PerfHooksApiTest, UserdataPassedThroughToCallbacks) {
  ResetCalls();
  static int captured = 0;
  static int sentinel = 0xDECAFBAD;
  struct Local {
    static void OnEnd(void* ud) { captured = *static_cast<int*>(ud); }
  };
  VxPipelineHooks hooks{};
  hooks.on_frame_end = &Local::OnEnd;

  // Trigger initial update first so update_manager_ is ready.
  EXPECT_EQ(vx_view_load_html(view_, "<div></div>", 11), VX_OK);
  EXPECT_EQ(vx_view_update(view_), VX_OK);

  EXPECT_EQ(vx_view_set_pipeline_hooks(view_, &hooks, &sentinel), VX_OK);

  // Force a re-update by changing CSS.
  EXPECT_EQ(vx_view_load_css(view_, "div { width: 30px; }", 20), VX_OK);
  EXPECT_EQ(vx_view_update(view_), VX_OK);

  EXPECT_EQ(captured, 0xDECAFBAD);
}

}  // namespace
