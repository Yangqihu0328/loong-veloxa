#include "veloxa/core/dom/serializer.h"

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

}  // namespace vx::dom
