#ifndef VELOXA_CORE_DOM_TEXT_H_
#define VELOXA_CORE_DOM_TEXT_H_

#include "veloxa/core/dom/node.h"
#include "veloxa/foundation/strings/string.h"

namespace vx::dom {

class Text : public Node {
 public:
  explicit Text(String data) : Node(NodeType::kText), data_(std::move(data)) {}

  bool is_text() const override { return true; }

  const String& data() const { return data_; }
  void set_data(String data) { data_ = std::move(data); }

 private:
  String data_;
};

}  // namespace vx::dom

#endif  // VELOXA_CORE_DOM_TEXT_H_
