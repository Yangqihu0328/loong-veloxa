#include "veloxa/devtool/inspector/inspector_data.h"

#include <gtest/gtest.h>

#include <string>

#include "veloxa/core/css/computed_style.h"
#include "veloxa/core/dom/document.h"
#include "veloxa/core/dom/element.h"
#include "veloxa/core/dom/tag.h"
#include "veloxa/core/dom/text.h"
#include "veloxa/core/layout/layout_box.h"
#include "veloxa/foundation/strings/interned_string.h"

namespace vx::devtool {
namespace {

inline std::string AsStd(const String& s) {
  return std::string(s.data(), s.size());
}

// =============================================================================
// SerializeDocument — DOM tree JSON for Inspector DOM panel (A1).
// =============================================================================

TEST(InspectorDataTest, SerializeDocumentNullReturnsLiteralNull) {
  EXPECT_EQ(AsStd(SerializeDocument(nullptr,
                                     dom::RedactionPolicy::kRedactSensitive)),
            "null");
}

TEST(InspectorDataTest, SerializeDocumentEmptyReturnsDocumentEnvelope) {
  dom::Document doc;
  std::string json = AsStd(SerializeDocument(
      &doc, dom::RedactionPolicy::kRedactSensitive));
  // Schema: {"document":{"type":"document","children":[...]}}
  EXPECT_NE(json.find("\"document\":"), std::string::npos);
  EXPECT_NE(json.find("\"type\":\"document\""), std::string::npos);
  EXPECT_NE(json.find("\"children\":[]"), std::string::npos);
}

TEST(InspectorDataTest, SerializeDocumentWithRootElementReturnsTreeJson) {
  dom::Document doc;
  auto* root = doc.CreateElement(dom::TagId::kHtml);
  doc.AppendChild(root);
  auto* body = doc.CreateElement(dom::TagId::kBody);
  root->AppendChild(body);

  std::string json = AsStd(SerializeDocument(
      &doc, dom::RedactionPolicy::kRedactSensitive));
  EXPECT_NE(json.find("\"tag\":\"html\""), std::string::npos);
  EXPECT_NE(json.find("\"tag\":\"body\""), std::string::npos);
}

// T3: SerializeDocument propagates RedactionPolicy to nested ToJson calls.
TEST(InspectorDataTest, SerializeDocumentT3PropagatesRedactionPolicy) {
  dom::Document doc;
  auto* input = doc.CreateElement(dom::TagId::kInput);
  input->SetAttribute(InternedString::Intern("type"), String("password"));
  input->SetAttribute(InternedString::Intern("value"), String("hunter2"));
  doc.AppendChild(input);

  std::string redacted = AsStd(SerializeDocument(
      &doc, dom::RedactionPolicy::kRedactSensitive));
  EXPECT_EQ(redacted.find("hunter2"), std::string::npos)
      << "Password value MUST NOT leak when policy=kRedactSensitive";
  EXPECT_NE(redacted.find("[REDACTED]"), std::string::npos);

  std::string raw = AsStd(SerializeDocument(&doc, dom::RedactionPolicy::kNone));
  EXPECT_NE(raw.find("hunter2"), std::string::npos);
}

// =============================================================================
// SerializeLayoutBox — single LayoutBox JSON for Inspector Layout panel (A4).
// =============================================================================

TEST(InspectorDataTest, SerializeLayoutBoxNullReturnsLiteralNull) {
  EXPECT_EQ(AsStd(SerializeLayoutBox(nullptr)), "null");
}

TEST(InspectorDataTest, SerializeLayoutBoxDelegatesToBoxToJson) {
  layout::LayoutBox box;
  box.x = 10;
  box.y = 20;
  box.content_width = 100;
  box.content_height = 50;

  std::string json = AsStd(SerializeLayoutBox(&box));
  // Should match LayoutBox::ToJson() schema verbatim.
  EXPECT_NE(json.find("\"x\":10"), std::string::npos);
  EXPECT_NE(json.find("\"content_width\":100"), std::string::npos);
}

// =============================================================================
// SerializeComputedStyle — ComputedStyle JSON for Inspector Style panel (A3).
// =============================================================================

TEST(InspectorDataTest, SerializeComputedStyleNullReturnsLiteralNull) {
  EXPECT_EQ(AsStd(SerializeComputedStyle(nullptr)), "null");
}

TEST(InspectorDataTest, SerializeComputedStyleDefaultsContainCoreProperties) {
  css::ComputedStyle style;
  std::string json = AsStd(SerializeComputedStyle(&style));
  EXPECT_NE(json.find("\"display\":\"block\""), std::string::npos);
  EXPECT_NE(json.find("\"position\":\"static\""), std::string::npos);
  EXPECT_NE(json.find("\"opacity\":1"), std::string::npos);
}

TEST(InspectorDataTest, SerializeComputedStyleEmitsLengthAuto) {
  css::ComputedStyle style;
  style.width = css::LengthValue::Auto();
  style.height = css::LengthValue::Px(50);

  std::string json = AsStd(SerializeComputedStyle(&style));
  EXPECT_NE(json.find("\"width\":\"auto\""), std::string::npos);
  EXPECT_NE(json.find("\"height\":\"50px\""), std::string::npos);
}

TEST(InspectorDataTest, SerializeComputedStyleEmitsHexColors) {
  css::ComputedStyle style;
  style.background_color = 0xFF0000FFu;  // RRGGBBAA — red opaque
  std::string json = AsStd(SerializeComputedStyle(&style));
  EXPECT_NE(json.find("\"background_color\":\"#ff0000ff\""), std::string::npos);
}

}  // namespace
}  // namespace vx::devtool
