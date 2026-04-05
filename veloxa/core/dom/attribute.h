#ifndef VELOXA_CORE_DOM_ATTRIBUTE_H_
#define VELOXA_CORE_DOM_ATTRIBUTE_H_

#include "veloxa/foundation/strings/interned_string.h"
#include "veloxa/foundation/strings/string.h"

namespace vx::dom {

struct Attribute {
  InternedString name;
  String value;
};

}  // namespace vx::dom

#endif  // VELOXA_CORE_DOM_ATTRIBUTE_H_
