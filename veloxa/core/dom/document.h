#ifndef VELOXA_CORE_DOM_DOCUMENT_H_
#define VELOXA_CORE_DOM_DOCUMENT_H_

#include "veloxa/core/dom/comment.h"
#include "veloxa/core/dom/element.h"
#include "veloxa/core/dom/text.h"
#include "veloxa/foundation/containers/vector.h"
#include "veloxa/foundation/strings/string.h"

namespace vx::dom {

class Document : public Element {
 public:
  Document() : Element(TagId::kUnknown, NodeType::kDocument) {}

  ~Document() override {
    for (auto* node : owned_nodes_) {
      delete node;
    }
  }

  Element* CreateElement(TagId tag_id);
  Text* CreateText(String data);
  Comment* CreateComment(String data);

 private:
  Vector<Node*> owned_nodes_;
};

}  // namespace vx::dom

#endif  // VELOXA_CORE_DOM_DOCUMENT_H_
