#ifndef VELOXA_CORE_DOM_NODE_H_
#define VELOXA_CORE_DOM_NODE_H_

#include "veloxa/foundation/base/types.h"

namespace vx::dom {

enum class NodeType : u8 {
  kElement = 1,
  kText = 3,
  kComment = 8,
  kDocument = 9,
};

class Element;

class Node {
 public:
  virtual ~Node() = default;

  NodeType type() const { return type_; }
  Element* parent() const { return parent_; }
  void set_parent(Element* p) { parent_ = p; }
  Node* next_sibling() const { return next_; }
  Node* prev_sibling() const { return prev_; }
  void set_next_sibling(Node* n) { next_ = n; }
  void set_prev_sibling(Node* n) { prev_ = n; }

  virtual bool is_element() const { return false; }
  virtual bool is_text() const { return false; }

 protected:
  explicit Node(NodeType type) : type_(type) {}

 private:
  NodeType type_;
  Element* parent_ = nullptr;
  Node* next_ = nullptr;
  Node* prev_ = nullptr;
};

}  // namespace vx::dom

#endif  // VELOXA_CORE_DOM_NODE_H_
