#include <gtest/gtest.h>

#include "veloxa/core/dom/document.h"
#include "veloxa/core/dom/text.h"
#include "veloxa/core/event/event_manager.h"
#include "veloxa/foundation/strings/interned_string.h"
#include "veloxa/script/dom_bindings.h"
#include "veloxa/script/quickjs_engine.h"

#ifdef VX_BUILD_DEVTOOL
#include "veloxa/devtool/hot_reload/hot_reload_manager.h"
#include "veloxa/devtool/overlay/perf_overlay.h"
#endif

namespace vx::script {
namespace {

// =============================================================================
// devtool_bindings_test — TASK-20260502-01 A.1.4
//
// Verifies the JS-side native binding `vx_devtool_get_dom_json()` that
// inspector_panel.js (A.1.3) calls to obtain the target Document tree as
// JSON. The binding is conditionally registered via
// DomBindings::RegisterDevtoolBindings(); when VX_BUILD_DEVTOOL=OFF the
// register call is a no-op (A14 zero-byte guard).
//
// Architecture note: the DevTool DomBindings instance binds to the
// DevTool Document (its own JS context's `document` global) but the
// dogfood UI needs target Document data. We add a separate setter
// SetDevtoolTargetDocument() so VxDevtoolGetDomJson can recover the
// target via JS_GetContextOpaque(ctx) → DomBindings::InstanceData →
// devtool_target_doc, without polluting the existing Document binding.
// =============================================================================

class DevtoolBindingsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(engine_.Init().ok());

    auto* div = target_doc_.CreateElement(dom::TagId::kDiv);
    div->set_id(InternedString::Intern("target-box"));
    auto* text = target_doc_.CreateText(String("Hello Target"));
    div->AppendChild(text);
    target_doc_.AppendChild(div);

    bindings_.Bind(engine_.context(), &devtool_doc_, &em_);
  }

  void TearDown() override {
    bindings_.Unbind();
    engine_.Shutdown();
  }

  QuickjsEngine engine_;
  dom::Document target_doc_;
  dom::Document devtool_doc_;
  event::EventManager em_;
  DomBindings bindings_;
};

#ifdef VX_BUILD_DEVTOOL

TEST_F(DevtoolBindingsTest, RegistersGlobalFunction) {
  bindings_.SetDevtoolTargetDocument(&target_doc_);
  RegisterDevtoolBindings(engine_.context());

  auto result = engine_.EvalGlobal("typeof vx_devtool_get_dom_json",
                                   "test.js");
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value(), "function");
}

TEST_F(DevtoolBindingsTest, ReturnsTargetDocumentJsonEnvelope) {
  bindings_.SetDevtoolTargetDocument(&target_doc_);
  RegisterDevtoolBindings(engine_.context());

  auto result = engine_.EvalGlobal(
      "JSON.parse(vx_devtool_get_dom_json()).document.type", "test.js");
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value(), "document");
}

TEST_F(DevtoolBindingsTest, ReturnsTargetDocumentChildren) {
  bindings_.SetDevtoolTargetDocument(&target_doc_);
  RegisterDevtoolBindings(engine_.context());

  // The target Document has 1 <div> root child whose first text child is
  // "Hello Target". We verify both the element tag and the descendant
  // text payload to lock in the JSON envelope contract end-to-end.
  // (Note: set_id() populates Element::id_ but not the attributes map,
  // so we don't assert on .attributes.id here. SetAttribute("id", ...)
  // would be required for that.)
  auto tag = engine_.EvalGlobal(
      "JSON.parse(vx_devtool_get_dom_json()).document.children[0].tag",
      "test.js");
  ASSERT_TRUE(tag.ok());
  EXPECT_EQ(tag.value(), "div");

  auto text = engine_.EvalGlobal(
      "JSON.parse(vx_devtool_get_dom_json())"
      ".document.children[0].children[0].data",
      "test.js");
  ASSERT_TRUE(text.ok());
  EXPECT_EQ(text.value(), "Hello Target");
}

TEST_F(DevtoolBindingsTest, ReturnsNullEnvelopeWhenTargetUnset) {
  // Skip SetDevtoolTargetDocument — VxDevtoolGetDomJson must return a
  // safe stringified envelope ("null" or empty document) rather than crash.
  RegisterDevtoolBindings(engine_.context());

  auto result = engine_.EvalGlobal(
      "typeof vx_devtool_get_dom_json()", "test.js");
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value(), "string");
}

// =============================================================================
// TASK-20260502-02 B.2.2 — vx_view_get_perf_stats native binding
// =============================================================================

TEST_F(DevtoolBindingsTest, VxViewGetPerfStatsRegistersGlobal) {
  RegisterDevtoolBindings(engine_.context());
  auto result = engine_.EvalGlobal("typeof vx_view_get_perf_stats",
                                   "test.js");
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value(), "function");
}

TEST_F(DevtoolBindingsTest, VxViewGetPerfStatsReturnsZeroJsonWhenNoOverlay) {
  RegisterDevtoolBindings(engine_.context());
  // No PerfOverlay attached → must return safe zero-stats JSON envelope.
  auto fps = engine_.EvalGlobal(
      "JSON.parse(vx_view_get_perf_stats()).fps", "test.js");
  ASSERT_TRUE(fps.ok());
  EXPECT_EQ(fps.value(), "0");
}

// =============================================================================
// TASK-20260503-01 C.3.1 — vx_devtool_get_hot_reload_status native binding
//
// JSON envelope contract:
//   {"tracked":N,"last_error":"..."}
//   N           = HotReloadManager::tracked_count (LoadCSS invocation count)
//   last_error  = HotReloadManager::last_error (empty string when healthy)
//
// When no HotReloadManager is bound the binding still exists and returns
// the safe zero envelope {"tracked":0,"last_error":""} so the UI can
// always parse the JSON without a try/catch dance.
// =============================================================================

TEST_F(DevtoolBindingsTest, VxDevtoolGetHotReloadStatusRegistersGlobal) {
  RegisterDevtoolBindings(engine_.context());
  auto result = engine_.EvalGlobal(
      "typeof vx_devtool_get_hot_reload_status", "test.js");
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value(), "function");
}

TEST_F(DevtoolBindingsTest,
       VxDevtoolGetHotReloadStatusReturnsZeroEnvelopeWhenUnattached) {
  RegisterDevtoolBindings(engine_.context());
  auto tracked = engine_.EvalGlobal(
      "JSON.parse(vx_devtool_get_hot_reload_status()).tracked", "test.js");
  ASSERT_TRUE(tracked.ok());
  EXPECT_EQ(tracked.value(), "0");

  auto last_error = engine_.EvalGlobal(
      "JSON.parse(vx_devtool_get_hot_reload_status()).last_error", "test.js");
  ASSERT_TRUE(last_error.ok());
  EXPECT_EQ(last_error.value(), "");
}

TEST_F(DevtoolBindingsTest,
       VxDevtoolGetHotReloadStatusReflectsAttachedManager) {
  // Construct a HotReloadManager bound to no Application (nullptr is
  // safe — Attach/DrainEvents are not invoked here, so the Application
  // pointer is never dereferenced). The binding must surface the
  // manager's defaults via the JSON envelope.
  vx::devtool::hot_reload::HotReloadManager mgr(/*app=*/nullptr);
  bindings_.SetHotReloadManager(&mgr);
  RegisterDevtoolBindings(engine_.context());

  auto tracked = engine_.EvalGlobal(
      "JSON.parse(vx_devtool_get_hot_reload_status()).tracked", "test.js");
  ASSERT_TRUE(tracked.ok());
  EXPECT_EQ(tracked.value(), "0");

  auto last_error = engine_.EvalGlobal(
      "JSON.parse(vx_devtool_get_hot_reload_status()).last_error", "test.js");
  ASSERT_TRUE(last_error.ok());
  EXPECT_EQ(last_error.value(), "");
}

TEST_F(DevtoolBindingsTest, VxViewGetPerfStatsReturnsLiveStatsWhenAttached) {
  vx::devtool::overlay::PerfOverlay perf;
  // Push a single synthetic frame: total=10ms → fps=100, style=1ms.
  perf.RecordFrame({1.0f, 2.0f, 3.0f, 4.0f, 10.0f});

  bindings_.SetPerfOverlay(&perf);
  RegisterDevtoolBindings(engine_.context());

  auto fps = engine_.EvalGlobal(
      "JSON.parse(vx_view_get_perf_stats()).fps", "test.js");
  ASSERT_TRUE(fps.ok());
  EXPECT_EQ(fps.value(), "100");

  auto style = engine_.EvalGlobal(
      "JSON.parse(vx_view_get_perf_stats()).style", "test.js");
  ASSERT_TRUE(style.ok());
  EXPECT_EQ(style.value(), "1");
}

#else  // VX_BUILD_DEVTOOL OFF — A14 zero-byte stub guard

TEST_F(DevtoolBindingsTest, StubDoesNotRegisterFunction) {
  // OFF path: RegisterDevtoolBindings is a no-op stub; the global
  // function must not exist (proves the code didn't sneak in via a
  // weak link).
  RegisterDevtoolBindings(engine_.context());

  auto result = engine_.EvalGlobal("typeof vx_devtool_get_dom_json",
                                   "test.js");
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value(), "undefined");
}

#endif  // VX_BUILD_DEVTOOL

}  // namespace
}  // namespace vx::script
