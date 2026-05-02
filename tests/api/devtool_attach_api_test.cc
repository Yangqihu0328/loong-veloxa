#include "veloxa/api/veloxa_api.h"

#include <gtest/gtest.h>

namespace {

// =============================================================================
// devtool_attach_api_test — TASK-20260502-01 A.1.7
//
// vx_view_attach_devtool / vx_view_detach_devtool / vx_view_devtool_loaded
// thin C wrappers around Application::LoadDevtoolDocument (A.1.6) + F12
// hotkey toggle. Compiled in BOTH DEVTOOL=ON and DEVTOOL=OFF builds — the
// OFF path returns VX_ERROR_INVALID_STATE (A14 zero-byte stub guard).
// =============================================================================

constexpr uint32_t kW = 800, kH = 600;

class DevtoolAttachApiTest : public ::testing::Test {
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

TEST_F(DevtoolAttachApiTest, AttachWithNullViewReturnsNullParam) {
  EXPECT_EQ(vx_view_attach_devtool(nullptr, nullptr), VX_ERROR_NULL_PARAM);
}

TEST_F(DevtoolAttachApiTest, DetachWithNullViewReturnsNullParam) {
  EXPECT_EQ(vx_view_detach_devtool(nullptr), VX_ERROR_NULL_PARAM);
}

TEST_F(DevtoolAttachApiTest, DevtoolLoadedWithNullViewReturnsNegativeOne) {
  EXPECT_EQ(vx_view_devtool_loaded(nullptr), -1);
}

#ifdef VX_BUILD_DEVTOOL

// -----------------------------------------------------------------------------
// DEVTOOL=ON: full attach/detach/F12 lifecycle
// -----------------------------------------------------------------------------

TEST_F(DevtoolAttachApiTest, AttachLoadsDevtoolWithDefaults) {
  // Passing NULL opts must use defaults (devtool_width=270, hotkey on)
  // per A.1.7 plan contract.
  EXPECT_EQ(vx_view_devtool_loaded(view_), 0);
  ASSERT_EQ(vx_view_attach_devtool(view_, nullptr), VX_OK);
  EXPECT_EQ(vx_view_devtool_loaded(view_), 1);
}

TEST_F(DevtoolAttachApiTest, AttachWithExplicitOptionsSucceeds) {
  VxDevtoolOptions opts{};
  opts.devtool_width = 270;
  opts.enable_f12_hotkey = 1;
  ASSERT_EQ(vx_view_attach_devtool(view_, &opts), VX_OK);
  EXPECT_EQ(vx_view_devtool_loaded(view_), 1);
}

TEST_F(DevtoolAttachApiTest, DetachUnloadsDevtool) {
  ASSERT_EQ(vx_view_attach_devtool(view_, nullptr), VX_OK);
  ASSERT_EQ(vx_view_devtool_loaded(view_), 1);
  ASSERT_EQ(vx_view_detach_devtool(view_), VX_OK);
  EXPECT_EQ(vx_view_devtool_loaded(view_), 0);
}

TEST_F(DevtoolAttachApiTest, AttachClampsDevtoolWidthToValidRange) {
  // Width clamped to [200, 400] per plan §A.1.7.
  // Below range: 100 → 200; above range: 500 → 400. We can't directly
  // observe the clamped value through public C API, but we verify the
  // attach succeeds (no INVALID_ARG returned for out-of-range input —
  // clamping is the documented behavior).
  VxDevtoolOptions opts_low{};
  opts_low.devtool_width = 100;
  opts_low.enable_f12_hotkey = 0;
  EXPECT_EQ(vx_view_attach_devtool(view_, &opts_low), VX_OK);
  EXPECT_EQ(vx_view_devtool_loaded(view_), 1);
  vx_view_detach_devtool(view_);

  VxDevtoolOptions opts_high{};
  opts_high.devtool_width = 500;
  opts_high.enable_f12_hotkey = 0;
  EXPECT_EQ(vx_view_attach_devtool(view_, &opts_high), VX_OK);
  EXPECT_EQ(vx_view_devtool_loaded(view_), 1);
}

TEST_F(DevtoolAttachApiTest, F12HotkeyTogglesAttachWhenEnabled) {
  // Attach with F12 hotkey enabled, then inject F12 KeyDown — should
  // toggle to detached. Inject again — re-attach.
  VxDevtoolOptions opts{};
  opts.devtool_width = 270;
  opts.enable_f12_hotkey = 1;
  ASSERT_EQ(vx_view_attach_devtool(view_, &opts), VX_OK);
  ASSERT_EQ(vx_view_devtool_loaded(view_), 1);

  VxInputEvent f12{};
  f12.type = VX_EVENT_KEY_DOWN;
  f12.key_code = VX_KEY_F12;
  ASSERT_EQ(vx_view_inject_input(view_, &f12), VX_OK);
  EXPECT_EQ(vx_view_devtool_loaded(view_), 0);

  ASSERT_EQ(vx_view_inject_input(view_, &f12), VX_OK);
  EXPECT_EQ(vx_view_devtool_loaded(view_), 1);
}

TEST_F(DevtoolAttachApiTest, F12HotkeyDisabledIgnoresF12) {
  // enable_f12_hotkey=0 means F12 KeyDown is forwarded as a normal key
  // event without toggling DevTool state.
  VxDevtoolOptions opts{};
  opts.devtool_width = 270;
  opts.enable_f12_hotkey = 0;
  ASSERT_EQ(vx_view_attach_devtool(view_, &opts), VX_OK);
  ASSERT_EQ(vx_view_devtool_loaded(view_), 1);

  VxInputEvent f12{};
  f12.type = VX_EVENT_KEY_DOWN;
  f12.key_code = VX_KEY_F12;
  vx_view_inject_input(view_, &f12);
  // Hotkey disabled → state unchanged.
  EXPECT_EQ(vx_view_devtool_loaded(view_), 1);
}

TEST_F(DevtoolAttachApiTest, F12HotkeyOnlyTriggersOnDevtoolLoaded) {
  // Without prior attach, F12 alone should NOT auto-load DevTool —
  // hotkey state is part of the attach contract, not a global toggle.
  // (Avoids surprise activation in apps that never attached DevTool.)
  EXPECT_EQ(vx_view_devtool_loaded(view_), 0);

  VxInputEvent f12{};
  f12.type = VX_EVENT_KEY_DOWN;
  f12.key_code = VX_KEY_F12;
  vx_view_inject_input(view_, &f12);
  EXPECT_EQ(vx_view_devtool_loaded(view_), 0);
}

// =============================================================================
// TASK-20260502-02 B.3.1 — F11 toggle HUD visibility
//
// F11 toggles Application::hud_visible_ flag (default true at attach time).
// Only effective AFTER DevTool is attached AND enable_f12_hotkey=1 (same
// gate as F12 — embedders that disable hotkeys handle F11 themselves).
// HUD visual response (DOM display style) is wired up at integration time
// (B.3.2 hello_devtool perf smoke).
// =============================================================================

TEST_F(DevtoolAttachApiTest, HudVisibleDefaultTrueAfterAttach) {
  ASSERT_EQ(vx_view_attach_devtool(view_, nullptr), VX_OK);
  EXPECT_EQ(vx_view_is_hud_visible(view_), 1);
}

TEST_F(DevtoolAttachApiTest, HudVisibleZeroBeforeAttach) {
  EXPECT_EQ(vx_view_is_hud_visible(view_), 0);
}

TEST_F(DevtoolAttachApiTest, F11HotkeyTogglesHudVisibleWhenEnabled) {
  VxDevtoolOptions opts{};
  opts.devtool_width = 270;
  opts.enable_f12_hotkey = 1;
  ASSERT_EQ(vx_view_attach_devtool(view_, &opts), VX_OK);
  ASSERT_EQ(vx_view_is_hud_visible(view_), 1);

  VxInputEvent f11{};
  f11.type = VX_EVENT_KEY_DOWN;
  f11.key_code = VX_KEY_F11;
  vx_view_inject_input(view_, &f11);
  EXPECT_EQ(vx_view_is_hud_visible(view_), 0);

  vx_view_inject_input(view_, &f11);
  EXPECT_EQ(vx_view_is_hud_visible(view_), 1);
}

TEST_F(DevtoolAttachApiTest, F11HotkeyDisabledIgnoresF11) {
  VxDevtoolOptions opts{};
  opts.devtool_width = 270;
  opts.enable_f12_hotkey = 0;
  ASSERT_EQ(vx_view_attach_devtool(view_, &opts), VX_OK);
  ASSERT_EQ(vx_view_is_hud_visible(view_), 1);

  VxInputEvent f11{};
  f11.type = VX_EVENT_KEY_DOWN;
  f11.key_code = VX_KEY_F11;
  vx_view_inject_input(view_, &f11);
  // Hotkey disabled → state unchanged.
  EXPECT_EQ(vx_view_is_hud_visible(view_), 1);
}

TEST_F(DevtoolAttachApiTest, F11DoesNotToggleDevtoolAttach) {
  // F11 is HUD-only; DevTool attach state must stay unchanged after F11.
  ASSERT_EQ(vx_view_attach_devtool(view_, nullptr), VX_OK);
  ASSERT_EQ(vx_view_devtool_loaded(view_), 1);

  VxInputEvent f11{};
  f11.type = VX_EVENT_KEY_DOWN;
  f11.key_code = VX_KEY_F11;
  vx_view_inject_input(view_, &f11);
  EXPECT_EQ(vx_view_devtool_loaded(view_), 1);  // unchanged
}

#else  // VX_BUILD_DEVTOOL OFF — A14 zero-byte stub guard

TEST_F(DevtoolAttachApiTest, AttachOffPathReturnsInvalidState) {
  ASSERT_NE(view_, nullptr);
  EXPECT_EQ(vx_view_attach_devtool(view_, nullptr), VX_ERROR_INVALID_STATE);
  // OFF stubs must keep loaded = 0 (no DevTool subsystem compiled in).
  EXPECT_EQ(vx_view_devtool_loaded(view_), 0);
}

#endif  // VX_BUILD_DEVTOOL

}  // namespace
