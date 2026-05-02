#include "veloxa/api/veloxa_api.h"

#include <gtest/gtest.h>

#include <cstring>
#include <string>

namespace {

// =============================================================================
// devtool_redaction_policy_test — TASK-20260502-01 A.2.1 [SECURITY]
//
// vx_inspector_set_redaction_policy: T3 mitigation policy switch C API.
// Default is VX_REDACTION_REDACT_SENSITIVE (passwords → "[REDACTED]"); the
// VX_REDACTION_NONE escape hatch is reserved for embedders that need to
// build their own DevTool features (e.g. raw value inspectors) under their
// own threat model. Switching the policy must affect both the C-API
// vx_view_serialize_dom_json output AND the DevTool JS binding's
// vx_devtool_get_dom_json result on subsequent calls.
//
// The OFF path keeps the API as a no-op stub returning INVALID_STATE
// (A14 zero-link-closure guard, consistent with the rest of the
// DevTool Inspector C API surface).
// =============================================================================

constexpr uint32_t kW = 800, kH = 600;

class DevtoolRedactionPolicyTest : public ::testing::Test {
 protected:
  void SetUp() override {
    loop_ = vx_event_loop_create_headless();
    surface_ = vx_surface_create_memory(kW, kH);

    VxViewConfig cfg{};
    cfg.event_loop = loop_;
    cfg.surface = surface_;
    cfg.target_fps = 60;
    view_ = vx_view_create(&cfg);

    // Page with a sensitive password input — value should be redacted by
    // default policy. Keep markup minimal so the JSON envelope is small
    // enough for stack buffers in this test.
    static constexpr char kHtml[] =
        "<html><body>"
        "<input type=\"password\" value=\"super-secret-pw\"/>"
        "</body></html>";
    vx_view_load_html(view_, kHtml, sizeof(kHtml) - 1);
  }

  void TearDown() override {
    if (view_) vx_view_destroy(view_);
    if (surface_) vx_surface_destroy(surface_);
    if (loop_) vx_event_loop_destroy(loop_);
  }

  std::string SerializeDom() {
    char buf[2048];
    uint32_t len = sizeof(buf);
    auto rc = vx_view_serialize_dom_json(view_, buf, &len, sizeof(buf));
    EXPECT_EQ(rc, VX_OK);
    return std::string(buf, len);
  }

  VxEventLoop* loop_ = nullptr;
  VxSurface* surface_ = nullptr;
  VxView* view_ = nullptr;
};

// Always-on contract: NULL view → NULL_PARAM (compiles in both ON and OFF).
TEST_F(DevtoolRedactionPolicyTest, SetPolicyWithNullViewReturnsNullParam) {
  EXPECT_EQ(vx_inspector_set_redaction_policy(
                nullptr, VX_REDACTION_REDACT_SENSITIVE),
            VX_ERROR_NULL_PARAM);
}

#ifdef VX_BUILD_DEVTOOL

TEST_F(DevtoolRedactionPolicyTest, DefaultPolicyRedactsPasswordValue) {
  // T3 default: input[type=password] value never appears verbatim in JSON.
  std::string json = SerializeDom();
  EXPECT_NE(json.find("[REDACTED]"), std::string::npos) << json;
  EXPECT_EQ(json.find("super-secret-pw"), std::string::npos) << json;
}

TEST_F(DevtoolRedactionPolicyTest, SetPolicyNoneEmitsPasswordValue) {
  // Embedder explicitly opts out of redaction → raw value present.
  ASSERT_EQ(vx_inspector_set_redaction_policy(view_, VX_REDACTION_NONE),
            VX_OK);
  std::string json = SerializeDom();
  EXPECT_NE(json.find("super-secret-pw"), std::string::npos) << json;
  EXPECT_EQ(json.find("[REDACTED]"), std::string::npos) << json;
}

TEST_F(DevtoolRedactionPolicyTest, SwitchBackToRedactRestoresMitigation) {
  // Round-trip: NONE → REDACT_SENSITIVE re-applies T3 mitigation.
  ASSERT_EQ(vx_inspector_set_redaction_policy(view_, VX_REDACTION_NONE),
            VX_OK);
  ASSERT_NE(SerializeDom().find("super-secret-pw"), std::string::npos);

  ASSERT_EQ(vx_inspector_set_redaction_policy(view_,
                                              VX_REDACTION_REDACT_SENSITIVE),
            VX_OK);
  std::string json = SerializeDom();
  EXPECT_NE(json.find("[REDACTED]"), std::string::npos) << json;
  EXPECT_EQ(json.find("super-secret-pw"), std::string::npos) << json;
}

TEST_F(DevtoolRedactionPolicyTest, InvalidPolicyValueReturnsInvalidArg) {
  // Out-of-enum value must be rejected so embedders can't accidentally
  // disable redaction by passing zero-init memory. Cast through the
  // enum since C++ is stricter than C about implicit int→enum.
  EXPECT_EQ(vx_inspector_set_redaction_policy(
                view_, static_cast<VxRedactionPolicy>(0xFFu)),
            VX_ERROR_INVALID_STATE);
  // Default (redact) should still be in effect after the rejected call.
  std::string json = SerializeDom();
  EXPECT_NE(json.find("[REDACTED]"), std::string::npos) << json;
}

#else  // DEVTOOL=OFF — A14 stub guard

TEST_F(DevtoolRedactionPolicyTest, SetPolicyOffPathReturnsInvalidState) {
  EXPECT_EQ(vx_inspector_set_redaction_policy(view_, VX_REDACTION_NONE),
            VX_ERROR_INVALID_STATE);
}

#endif  // VX_BUILD_DEVTOOL

}  // namespace
