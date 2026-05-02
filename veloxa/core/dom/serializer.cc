#include "veloxa/core/dom/serializer.h"

#include <cstdio>

#include "veloxa/core/dom/comment.h"
#include "veloxa/core/dom/element.h"
#include "veloxa/core/dom/tag.h"
#include "veloxa/core/dom/text.h"
#include "veloxa/foundation/strings/string_view.h"

namespace vx::dom {
namespace {

void EscapeText(StringView text, String& out) {
  for (usize i = 0; i < text.size(); ++i) {
    char ch = text[i];
    switch (ch) {
      case '&':
        out += StringView("&amp;");
        break;
      case '<':
        out += StringView("&lt;");
        break;
      case '>':
        out += StringView("&gt;");
        break;
      default:
        out += ch;
        break;
    }
  }
}

void EscapeAttrValue(StringView value, String& out) {
  for (usize i = 0; i < value.size(); ++i) {
    char ch = value[i];
    switch (ch) {
      case '&':
        out += StringView("&amp;");
        break;
      case '<':
        out += StringView("&lt;");
        break;
      case '>':
        out += StringView("&gt;");
        break;
      case '"':
        out += StringView("&quot;");
        break;
      default:
        out += ch;
        break;
    }
  }
}

void SerializeNode(const Node* node, String& out);

void SerializeElement(const Element* el, String& out) {
  const char* name = el->tag_name();
  TagId tag = el->tag_id();

  out += '<';
  out += StringView(name);

  for (const auto& attr : el->attributes()) {
    out += ' ';
    out += attr.name.view();
    out += StringView("=\"");
    EscapeAttrValue(attr.value.view(), out);
    out += '"';
  }

  out += '>';

  if (IsVoidElement(tag)) {
    return;
  }

  for (const Node* child = el->first_child(); child != nullptr;
       child = child->next_sibling()) {
    SerializeNode(child, out);
  }

  out += StringView("</");
  out += StringView(name);
  out += '>';
}

void SerializeNode(const Node* node, String& out) {
  switch (node->type()) {
    case NodeType::kElement:
      SerializeElement(static_cast<const Element*>(node), out);
      break;
    case NodeType::kText:
      EscapeText(static_cast<const Text*>(node)->data().view(), out);
      break;
    case NodeType::kComment:
      out += StringView("<!--");
      out += static_cast<const Comment*>(node)->data().view();
      out += StringView("-->");
      break;
    case NodeType::kDocument:
      for (const Node* child =
               static_cast<const Element*>(node)->first_child();
           child != nullptr; child = child->next_sibling()) {
        SerializeNode(child, out);
      }
      break;
  }
}

}  // namespace

String Serialize(const Node* node) {
  String out;
  if (node == nullptr) return out;
  SerializeNode(node, out);
  return out;
}

// ===========================================================================
// JSON serialization (DevTool Inspector A0.3, TASK-20260502-01).
// ===========================================================================

namespace {

// JSON string escaping per RFC 8259 §7.
void EscapeJsonString(StringView text, String& out) {
  for (usize i = 0; i < text.size(); ++i) {
    unsigned char ch = static_cast<unsigned char>(text[i]);
    switch (ch) {
      case '"':
        out += StringView("\\\"");
        break;
      case '\\':
        out += StringView("\\\\");
        break;
      case '\b':
        out += StringView("\\b");
        break;
      case '\f':
        out += StringView("\\f");
        break;
      case '\n':
        out += StringView("\\n");
        break;
      case '\r':
        out += StringView("\\r");
        break;
      case '\t':
        out += StringView("\\t");
        break;
      default:
        if (ch < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x",
                        static_cast<unsigned int>(ch));
          out += StringView(buf);
        } else {
          out += static_cast<char>(ch);
        }
        break;
    }
  }
}

// T3: input[type=password] value attribute MUST be redacted under
// kRedactSensitive policy. Determined per-attribute since T3 is currently the
// only sensitive vector (future: any attribute funneled through this helper).
bool IsSensitiveAttribute(const Element* el, StringView attr_name) {
  if (el->tag_id() != TagId::kInput) return false;
  if (attr_name != StringView("value")) return false;
  for (const auto& attr : el->attributes()) {
    if (attr.name.view() == StringView("type") &&
        attr.value.view() == StringView("password")) {
      return true;
    }
  }
  return false;
}

void ToJsonNode(const Node* node, RedactionPolicy policy, String& out);

void ToJsonElement(const Element* el, RedactionPolicy policy, String& out) {
  out += StringView("{\"type\":\"element\",\"tag\":\"");
  EscapeJsonString(StringView(el->tag_name()), out);
  out += StringView("\",\"attributes\":{");

  bool first_attr = true;
  for (const auto& attr : el->attributes()) {
    if (!first_attr) out += ',';
    first_attr = false;
    out += '"';
    EscapeJsonString(attr.name.view(), out);
    out += StringView("\":\"");
    if (policy == RedactionPolicy::kRedactSensitive &&
        IsSensitiveAttribute(el, attr.name.view())) {
      out += StringView("[REDACTED]");
    } else {
      EscapeJsonString(attr.value.view(), out);
    }
    out += '"';
  }
  out += StringView("},\"children\":[");

  bool first_child = true;
  for (const Node* child = el->first_child(); child != nullptr;
       child = child->next_sibling()) {
    if (!first_child) out += ',';
    first_child = false;
    ToJsonNode(child, policy, out);
  }
  out += StringView("]}");
}

void ToJsonDocument(const Element* doc, RedactionPolicy policy, String& out) {
  out += StringView("{\"type\":\"document\",\"children\":[");
  bool first_child = true;
  for (const Node* child = doc->first_child(); child != nullptr;
       child = child->next_sibling()) {
    if (!first_child) out += ',';
    first_child = false;
    ToJsonNode(child, policy, out);
  }
  out += StringView("]}");
}

void ToJsonNode(const Node* node, RedactionPolicy policy, String& out) {
  switch (node->type()) {
    case NodeType::kElement:
      ToJsonElement(static_cast<const Element*>(node), policy, out);
      break;
    case NodeType::kText:
      out += StringView("{\"type\":\"text\",\"data\":\"");
      EscapeJsonString(static_cast<const Text*>(node)->data().view(), out);
      out += StringView("\"}");
      break;
    case NodeType::kComment:
      out += StringView("{\"type\":\"comment\",\"data\":\"");
      EscapeJsonString(static_cast<const Comment*>(node)->data().view(), out);
      out += StringView("\"}");
      break;
    case NodeType::kDocument:
      ToJsonDocument(static_cast<const Element*>(node), policy, out);
      break;
  }
}

}  // namespace

String ToJson(const Node* node, RedactionPolicy policy) {
  String out;
  if (node == nullptr) return out;
  out.reserve(256);  // typical small DOM tree fits SSO+small heap upgrade
  ToJsonNode(node, policy, out);
  return out;
}

}  // namespace vx::dom
