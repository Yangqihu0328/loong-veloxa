#include "veloxa/api/veloxa_api.h"

#include <atomic>

#include <gtest/gtest.h>

namespace {

// =============================================================================
// invalidate_api_test — TASK-20260505-02 A.1 (B-G4 closure)
//
// vx_view_invalidate thin C wrapper around Application::Invalidate →
// UpdateManager::Invalidate (sets dirty_=true). Public ABI for embedders
// driving force-rearm in pipeline-hooks / multi-frame profiling scenarios
// in static-DOM views (the dirty_ short-circuit in UpdateManager::Update
// otherwise no-ops after frame 1; details in techContext.md ≈L1035).
//
// Routing target: target update_manager_ only — DevTool's UpdateManager
// runs an independent state machine (D1-A decision).
// Thread-safety: main thread only (D2-A decision).
// =============================================================================

constexpr uint32_t kW = 200, kH = 200;

class InvalidateApiTest : public ::testing::Test {
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
// 1. NULL guard.
// -----------------------------------------------------------------------------
TEST_F(InvalidateApiTest, NullViewReturnsNullParam) {
  EXPECT_EQ(vx_view_invalidate(nullptr), VX_ERROR_NULL_PARAM);
}

// -----------------------------------------------------------------------------
// 2. Fresh view (no LoadHTML) → update_manager_ is null → INVALID_STATE.
//    Same lazy-attach contract as vx_view_set_pipeline_hooks.
// -----------------------------------------------------------------------------
TEST_F(InvalidateApiTest, FreshViewReturnsInvalidState) {
  EXPECT_EQ(vx_view_invalidate(view_), VX_ERROR_INVALID_STATE);
}

// Static counter for the next two tests (C function pointer constraint —
// no lambda capture).
namespace {
std::atomic<int> g_invalidate_hook_calls{};
void OnFrameEndCounter(void*) { g_invalidate_hook_calls++; }
}  // namespace

// -----------------------------------------------------------------------------
// 3. Normal path. After LoadHTML + Update, dirty_ is cleared. A fresh
//    Update without rearm runs no hooks (dirty_ short-circuit). Then a
//    single Invalidate call rearms dirty_, so the next Update fires hooks
//    exactly once.
// -----------------------------------------------------------------------------
TEST_F(InvalidateApiTest, InvalidateMakesNextUpdateRunHooks) {
  // 1. LoadHTML → first Update — dirty_ cleared inside Update.
  EXPECT_EQ(vx_view_load_html(view_, "<div></div>", 11), VX_OK);
  EXPECT_EQ(vx_view_update(view_), VX_OK);

  // 2. Install hooks; second Update without dirty_ rearm → hooks NOT fire
  //    (UpdateManager::Update short-circuits on dirty_=false).
  g_invalidate_hook_calls.store(0);
  VxPipelineHooks hooks{};
  hooks.on_frame_end = &OnFrameEndCounter;
  EXPECT_EQ(vx_view_set_pipeline_hooks(view_, &hooks, nullptr), VX_OK);
  EXPECT_EQ(vx_view_update(view_), VX_OK);
  EXPECT_EQ(g_invalidate_hook_calls.load(), 0);

  // 3. Invalidate → next Update fires hooks exactly once.
  EXPECT_EQ(vx_view_invalidate(view_), VX_OK);
  EXPECT_EQ(vx_view_update(view_), VX_OK);
  EXPECT_EQ(g_invalidate_hook_calls.load(), 1);

  // 4. Cleanup hooks so the static counter stays inert across tests.
  EXPECT_EQ(vx_view_set_pipeline_hooks(view_, nullptr, nullptr), VX_OK);
}

// -----------------------------------------------------------------------------
// 4. Idempotency — N invalidate calls before the next Update == 1 call.
//    Verifies that dirty_=true is a single-assignment (no over-fire).
// -----------------------------------------------------------------------------
TEST_F(InvalidateApiTest, InvalidateIdempotent) {
  EXPECT_EQ(vx_view_load_html(view_, "<div></div>", 11), VX_OK);
  EXPECT_EQ(vx_view_update(view_), VX_OK);  // dirty_ now false

  g_invalidate_hook_calls.store(0);
  VxPipelineHooks hooks{};
  hooks.on_frame_end = &OnFrameEndCounter;
  EXPECT_EQ(vx_view_set_pipeline_hooks(view_, &hooks, nullptr), VX_OK);

  // 5 invalidates before next update → still 1 frame fires hooks.
  EXPECT_EQ(vx_view_invalidate(view_), VX_OK);
  EXPECT_EQ(vx_view_invalidate(view_), VX_OK);
  EXPECT_EQ(vx_view_invalidate(view_), VX_OK);
  EXPECT_EQ(vx_view_invalidate(view_), VX_OK);
  EXPECT_EQ(vx_view_invalidate(view_), VX_OK);
  EXPECT_EQ(vx_view_update(view_), VX_OK);
  EXPECT_EQ(g_invalidate_hook_calls.load(), 1);

  EXPECT_EQ(vx_view_set_pipeline_hooks(view_, nullptr, nullptr), VX_OK);
}

}  // namespace
