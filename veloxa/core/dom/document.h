#ifndef VELOXA_CORE_DOM_DOCUMENT_H_
#define VELOXA_CORE_DOM_DOCUMENT_H_

#include "veloxa/core/dom/comment.h"
#include "veloxa/core/dom/element.h"
#include "veloxa/core/dom/text.h"
#include "veloxa/foundation/containers/vector.h"
#include "veloxa/foundation/memory/arena_allocator.h"
#include "veloxa/foundation/strings/string.h"

namespace vx::dom {

class Document : public Element {
 public:
  explicit Document(usize arena_block_size = 4096)
      : Element(TagId::kUnknown, NodeType::kDocument),
        arena_(arena_block_size) {}

  ~Document() override {
    for (auto* node : owned_nodes_) {
      node->~Node();
    }
  }

  Element* CreateElement(TagId tag_id);
  Text* CreateText(String data);
  Comment* CreateComment(String data);

  usize arena_bytes_allocated() const { return arena_.bytes_allocated(); }

 private:
  ArenaAllocator arena_;
  Vector<Node*> owned_nodes_;
};

}  // namespace vx::dom

#endif  // VELOXA_CORE_DOM_DOCUMENT_H_
