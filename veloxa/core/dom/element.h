#ifndef VELOXA_CORE_DOM_ELEMENT_H_
#define VELOXA_CORE_DOM_ELEMENT_H_

#include "veloxa/core/dom/attribute.h"
#include "veloxa/core/dom/node.h"
#include "veloxa/core/dom/tag.h"
#include "veloxa/foundation/containers/small_vector.h"
#include "veloxa/foundation/strings/interned_string.h"
#include "veloxa/foundation/strings/string.h"

namespace vx::dom {

class Element : public Node {
 public:
  explicit Element(TagId tag_id)
      : Node(NodeType::kElement), tag_id_(tag_id) {}

  bool is_element() const override { return true; }

  TagId tag_id() const { return tag_id_; }
  const char* tag_name() const { return GetTagInfo(tag_id_).name; }

  void AppendChild(Node* child);
  void RemoveChild(Node* child);

  Node* first_child() const { return first_child_; }
  Node* last_child() const { return last_child_; }
  u32 child_count() const { return child_count_; }

  void SetAttribute(InternedString name, String value);
  const String* GetAttribute(InternedString name) const;
  bool HasAttribute(InternedString name) const;
  void RemoveAttribute(InternedString name);
  const SmallVector<Attribute, 4>& attributes() const { return attributes_; }

 protected:
  Element(TagId tag_id, NodeType node_type)
      : Node(node_type), tag_id_(tag_id) {}

 private:
  TagId tag_id_;
  Node* first_child_ = nullptr;
  Node* last_child_ = nullptr;
  u32 child_count_ = 0;
  SmallVector<Attribute, 4> attributes_;
};

}  // namespace vx::dom

#endif  // VELOXA_CORE_DOM_ELEMENT_H_
