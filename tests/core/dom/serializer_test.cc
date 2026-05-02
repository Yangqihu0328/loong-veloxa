#include "veloxa/core/dom/serializer.h"

#include <gtest/gtest.h>

#include <string>

#include "veloxa/core/dom/comment.h"
#include "veloxa/core/dom/document.h"
#include "veloxa/core/dom/element.h"
#include "veloxa/core/dom/tag.h"
#include "veloxa/core/dom/text.h"
#include "veloxa/foundation/strings/interned_string.h"

namespace vx::dom {
namespace {

inline std::string AsStd(const String& s) {
  return std::string(s.data(), s.size());
}

// ===========================================================================
// HTML Serialize() — pre-existing behavior; pin coverage so the new
// ToJson() addition does not regress the HTML serializer.
// ===========================================================================

TEST(SerializeHtmlTest, EmptyDocument) {
  Document doc;
  String out = Serialize(&doc);
  EXPECT_EQ(AsStd(out), "");
}

TEST(SerializeHtmlTest, SimpleElement) {
  Document doc;
  Element el(TagId::kDiv);
  doc.AppendChild(&el);
  EXPECT_EQ(AsStd(Serialize(&doc)), "<div></div>");
}

// ===========================================================================
// JSON ToJson() — DevTool Inspector A1/A2 + T3 sensitive-data redaction
// (TASK-20260502-01 A.0.3). JSON schema:
//   { "type":"element", "tag":"<name>",
//     "attributes": { "<name>":"<value>", ... },
//     "children": [ <node>, ... ] }
//   { "type":"text", "data":"<escaped>" }
//   { "type":"comment", "data":"<raw>" }
//   { "type":"document", "children":[ ... ] }
//
// RedactionPolicy:
//   kRedactSensitive (default for DevTool): input[type=password] value →
//     "[REDACTED]" string. Any future sensitive attributes funnel through
//     the same path.
//   kNone: raw values (test-only / unit-test convenience).
// ===========================================================================

TEST(SerializeJsonTest, DocumentEmptyChildren) {
  Document doc;
  String out = ToJson(&doc, RedactionPolicy::kRedactSensitive);
  EXPECT_EQ(AsStd(out), "{\"type\":\"document\",\"children\":[]}");
}

TEST(SerializeJsonTest, ElementWithoutAttributesOrChildren) {
  Element el(TagId::kDiv);
  std::string json = AsStd(ToJson(&el, RedactionPolicy::kRedactSensitive));
  EXPECT_EQ(json,
            "{\"type\":\"element\",\"tag\":\"div\",\"attributes\":{},"
            "\"children\":[]}");
}

TEST(SerializeJsonTest, ElementWithAttributesAndTextChild) {
  Element el(TagId::kDiv);
  el.SetAttribute(InternedString::Intern("id"), String("box"));
  el.SetAttribute(InternedString::Intern("class"), String("hero"));
  Text text("Hello");
  el.AppendChild(&text);

  std::string json = AsStd(ToJson(&el, RedactionPolicy::kRedactSensitive));
  EXPECT_NE(json.find("\"type\":\"element\""), std::string::npos);
  EXPECT_NE(json.find("\"tag\":\"div\""), std::string::npos);
  EXPECT_NE(json.find("\"id\":\"box\""), std::string::npos);
  EXPECT_NE(json.find("\"class\":\"hero\""), std::string::npos);
  EXPECT_NE(json.find("\"type\":\"text\""), std::string::npos);
  EXPECT_NE(json.find("\"data\":\"Hello\""), std::string::npos);
}

TEST(SerializeJsonTest, CommentNode) {
  Comment c("hello world");
  std::string json = AsStd(ToJson(&c, RedactionPolicy::kRedactSensitive));
  EXPECT_EQ(json, "{\"type\":\"comment\",\"data\":\"hello world\"}");
}

TEST(SerializeJsonTest, EscapesQuotesAndBackslashesInText) {
  Text t(R"(say "hi" \back)");
  std::string json = AsStd(ToJson(&t, RedactionPolicy::kRedactSensitive));
  // Backslash and double-quote MUST be escaped per JSON RFC 8259 §7.
  EXPECT_NE(json.find("\\\""), std::string::npos);
  EXPECT_NE(json.find("\\\\"), std::string::npos);
}

TEST(SerializeJsonTest, EscapesControlCharactersInText) {
  Text t("line1\nline2\ttab");
  std::string json = AsStd(ToJson(&t, RedactionPolicy::kRedactSensitive));
  EXPECT_NE(json.find("\\n"), std::string::npos);
  EXPECT_NE(json.find("\\t"), std::string::npos);
}

// T3: Inspector MUST redact password values to prevent shoulder-surfing leak.
TEST(SerializeJsonTest, T3PasswordInputValueRedacted) {
  Element input(TagId::kInput);
  input.SetAttribute(InternedString::Intern("type"), String("password"));
  input.SetAttribute(InternedString::Intern("value"), String("hunter2"));

  std::string json = AsStd(ToJson(&input, RedactionPolicy::kRedactSensitive));
  EXPECT_EQ(json.find("hunter2"), std::string::npos)
      << "Password value MUST NOT appear in serialized output";
  EXPECT_NE(json.find("\"value\":\"[REDACTED]\""), std::string::npos);
  EXPECT_NE(json.find("\"type\":\"password\""), std::string::npos);
}

TEST(SerializeJsonTest, T3RedactionPolicyNoneLeavesPasswordValueRaw) {
  Element input(TagId::kInput);
  input.SetAttribute(InternedString::Intern("type"), String("password"));
  input.SetAttribute(InternedString::Intern("value"), String("hunter2"));

  std::string json = AsStd(ToJson(&input, RedactionPolicy::kNone));
  // RedactionPolicy::kNone is test-only escape hatch; password leaks through
  // only when caller explicitly opts out of redaction.
  EXPECT_NE(json.find("hunter2"), std::string::npos);
  EXPECT_EQ(json.find("[REDACTED]"), std::string::npos);
}

TEST(SerializeJsonTest, T3NonPasswordInputValueNotRedacted) {
  Element input(TagId::kInput);
  input.SetAttribute(InternedString::Intern("type"), String("text"));
  input.SetAttribute(InternedString::Intern("value"), String("public-data"));

  std::string json = AsStd(ToJson(&input, RedactionPolicy::kRedactSensitive));
  EXPECT_NE(json.find("public-data"), std::string::npos);
  EXPECT_EQ(json.find("[REDACTED]"), std::string::npos);
}

TEST(SerializeJsonTest, NestedTreeStructure) {
  Document doc;
  Element root(TagId::kDiv);
  root.SetAttribute(InternedString::Intern("id"), String("root"));
  Element child(TagId::kSpan);
  Text text("inside");
  child.AppendChild(&text);
  root.AppendChild(&child);
  doc.AppendChild(&root);

  std::string json = AsStd(ToJson(&doc, RedactionPolicy::kRedactSensitive));
  EXPECT_NE(json.find("\"tag\":\"div\""), std::string::npos);
  EXPECT_NE(json.find("\"tag\":\"span\""), std::string::npos);
  EXPECT_NE(json.find("\"data\":\"inside\""), std::string::npos);
  EXPECT_NE(json.find("\"id\":\"root\""), std::string::npos);
}

}  // namespace
}  // namespace vx::dom
