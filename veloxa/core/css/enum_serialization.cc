#include "veloxa/core/css/enum_serialization.h"

namespace vx::css {

// RED stub — to be replaced with the lookup-table implementation in B5.3.
StringView EnumValueToCssString(PropertyId /*property*/, u16 /*enum_value*/) {
  return StringView();
}

}  // namespace vx::css
