// =============================================================================
// devtool_dogfood_smoke_test — TASK-20260502-01 A.1.8
//
// End-to-end smoke for the DevTool dogfood UI render path on a headless
// surface (no SDL2). Validates that:
//   1. C API vx_view_attach_devtool wires through Application::LoadDevtool-
//      Document and brings up an owned DevTool Document with bound layout.
//   2. The DevTool's own QuickJS engine evaluates inspector_panel.js
//      successfully (status.ok()), proving:
//        - DomBindings::Bind on the DevTool Document supplied a working
//          document.getElementById global,
//        - RegisterDevtoolBindings exposed vx_devtool_get_dom_json which
//          inspector_panel.js calls during renderDomTree(),
//        - the inlined panel script does not crash on the "minimal DOM"
//          surface area used by setupTabs() / renderDomTree().
//   3. vx_devtool_get_dom_json returns a JSON envelope referencing the
//      target Document — the cross-document inspection contract.
//
// Compiled only when VX_BUILD_DEVTOOL=ON (the entire scenario assumes
// a built DevTool subsystem).
// =============================================================================

#include <gtest/gtest.h>

#include <string>

#include "veloxa/api/veloxa_api.h"
#include "veloxa/core/application.h"
#include "veloxa/foundation/base/status.h"

namespace {

constexpr uint32_t kW = 800, kH = 600;

class DevtoolDogfoodSmokeTest : public ::testing::Test {
 protected:
  void SetUp() override {
    loop_ = vx_event_loop_create_headless();
    surface_ = vx_surface_create_memory(kW, kH);

    VxViewConfig cfg{};
    cfg.event_loop = loop_;
    cfg.surface = surface_;
    cfg.target_fps = 60;
    view_ = vx_view_create(&cfg);
    app_ = reinterpret_cast<vx::Application*>(view_);

    static constexpr char kHtml[] =
        "<html><body><div id=\"hello\">Hi</div></body></html>";
    vx_view_load_html(view_, kHtml, sizeof(kHtml) - 1);
  }

  void TearDown() override {
    if (view_) vx_view_destroy(view_);
    if (surface_) vx_surface_destroy(surface_);
    if (loop_) vx_event_loop_destroy(loop_);
  }

  VxEventLoop* loop_ = nullptr;
  VxSurface* surface_ = nullptr;
  VxView* view_ = nullptr;
  vx::Application* app_ = nullptr;
};

TEST_F(DevtoolDogfoodSmokeTest, AttachBuildsDevtoolLayoutTree) {
  // First baseline: no DevTool yet.
  ASSERT_EQ(vx_view_devtool_loaded(view_), 0);

  ASSERT_EQ(vx_view_attach_devtool(view_, nullptr), VX_OK);
  ASSERT_EQ(vx_view_devtool_loaded(view_), 1);

  // Drive one frame so both UpdateManagers materialise their layout.
  vx_view_update(view_);

  // DevTool Document layout root must be built (proves
  // EnsureDevtoolUpdateManager + Update ran end-to-end).
  ASSERT_NE(app_->devtool_update_manager(), nullptr);
  EXPECT_NE(app_->devtool_update_manager()->layout_root(), nullptr);
}

TEST_F(DevtoolDogfoodSmokeTest, AttachExecutesInspectorPanelJsSuccessfully) {
  // Core dogfood smoke: inspector_panel.js (inlined into the panel HTML
  // via __INLINE_JS__) runs through DevTool's own QuickJS engine without
  // a syntax or runtime error. setupTabs() and renderDomTree() — which
  // both call document.getElementById and vx_devtool_get_dom_json — must
  // resolve, otherwise the script status carries an error.
  ASSERT_EQ(vx_view_attach_devtool(view_, nullptr), VX_OK);
  vx_view_update(view_);

  const auto& status = app_->devtool_script_status();
  EXPECT_TRUE(status.ok())
      << "inspector_panel.js failed: " << status.message().data();
}

TEST_F(DevtoolDogfoodSmokeTest, VxDevtoolGetDomJsonReachesTargetDocument) {
  // After attach, re-evaluate vx_devtool_get_dom_json() in the DevTool
  // engine and confirm the JSON envelope mentions the target document
  // (specifically: the body's <div id="hello"> tag). This proves the
  // cross-document JS-binding contract end-to-end.
  ASSERT_EQ(vx_view_attach_devtool(view_, nullptr), VX_OK);
  vx_view_update(view_);

  auto json = app_->EvalDevtoolScript(
      vx::StringView("vx_devtool_get_dom_json()"),
      vx::StringView("smoke_probe"));
  ASSERT_TRUE(json.ok()) << "EvalDevtoolScript: " << json.status().message().data();
  const std::string& s = json.value();
  EXPECT_NE(s.find("\"document\""), std::string::npos) << s;
  EXPECT_NE(s.find("\"div\""), std::string::npos) << s;
  EXPECT_NE(s.find("\"Hi\""), std::string::npos) << s;
}

TEST_F(DevtoolDogfoodSmokeTest, DetachReleasesDevtoolEngine) {
  // Detach must tear down both the DevTool Document AND its script
  // engine — verified via attach → detach → attach cycle (no leak,
  // no stale-pointer crash; subsequent script status is reset).
  ASSERT_EQ(vx_view_attach_devtool(view_, nullptr), VX_OK);
  vx_view_update(view_);
  ASSERT_TRUE(app_->devtool_script_status().ok());

  ASSERT_EQ(vx_view_detach_devtool(view_), VX_OK);
  EXPECT_EQ(vx_view_devtool_loaded(view_), 0);

  // Re-attach in same view: must succeed and re-execute panel JS.
  ASSERT_EQ(vx_view_attach_devtool(view_, nullptr), VX_OK);
  vx_view_update(view_);
  EXPECT_TRUE(app_->devtool_script_status().ok());
}

}  // namespace
