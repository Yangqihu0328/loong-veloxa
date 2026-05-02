#include <gtest/gtest.h>

#include "veloxa/core/dom/document.h"
#include "veloxa/core/dom/text.h"
#include "veloxa/core/event/event_manager.h"
#include "veloxa/foundation/strings/interned_string.h"
#include "veloxa/script/dom_bindings.h"
#include "veloxa/script/quickjs_engine.h"

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
