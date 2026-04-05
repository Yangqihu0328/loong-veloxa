#ifndef VELOXA_CORE_DOM_COMMENT_H_
#define VELOXA_CORE_DOM_COMMENT_H_

#include "veloxa/core/dom/node.h"
#include "veloxa/foundation/strings/string.h"

namespace vx::dom {

class Comment : public Node {
 public:
  explicit Comment(String data)
      : Node(NodeType::kComment), data_(std::move(data)) {}

  const String& data() const { return data_; }

 private:
  String data_;
};

}  // namespace vx::dom

#endif  // VELOXA_CORE_DOM_COMMENT_H_
