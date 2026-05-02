#ifndef VELOXA_CORE_DOM_SERIALIZER_H_
#define VELOXA_CORE_DOM_SERIALIZER_H_

#include "veloxa/core/dom/node.h"
#include "veloxa/foundation/strings/string.h"

namespace vx::dom {

// Serialize a DOM tree (or subtree) to an HTML string.
// If the node is a Document, serializes its children.
// If the node is an Element, serializes the element and its subtree.
String Serialize(const Node* node);

// ===========================================================================
// DevTool Inspector JSON serialization (TASK-20260502-01 A.0.3).
// ===========================================================================

// RedactionPolicy controls whether sensitive attribute values are redacted in
// JSON output. T3 mitigation: input[type=password] value MUST be redacted by
// default to prevent shoulder-surfing leaks via DevTool Inspector.
enum class RedactionPolicy {
  kRedactSensitive,  // default for DevTool — replace sensitive values with "[REDACTED]"
  kNone,             // test-only escape hatch — no redaction
};

// Serialize a DOM tree (or subtree) to JSON for DevTool Inspector consumption.
// Schema:
//   {"type":"element", "tag":"<name>",
//    "attributes":{"<name>":"<escaped-value>", ...},
//    "children":[...]}
//   {"type":"text", "data":"<escaped>"}
//   {"type":"comment", "data":"<raw>"}
//   {"type":"document", "children":[...]}
//
// All string values are JSON-escaped per RFC 8259 §7. Sensitive attribute
// values are replaced with "[REDACTED]" when policy=kRedactSensitive.
String ToJson(const Node* node, RedactionPolicy policy);

}  // namespace vx::dom

#endif  // VELOXA_CORE_DOM_SERIALIZER_H_
