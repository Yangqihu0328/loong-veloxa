#ifndef VELOXA_CORE_DOM_SERIALIZER_H_
#define VELOXA_CORE_DOM_SERIALIZER_H_

#include "veloxa/core/dom/node.h"
#include "veloxa/foundation/strings/string.h"

namespace vx::dom {

// Serialize a DOM tree (or subtree) to an HTML string.
// If the node is a Document, serializes its children.
// If the node is an Element, serializes the element and its subtree.
String Serialize(const Node* node);

}  // namespace vx::dom

#endif  // VELOXA_CORE_DOM_SERIALIZER_H_
