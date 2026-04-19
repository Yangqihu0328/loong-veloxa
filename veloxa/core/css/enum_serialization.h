#ifndef VELOXA_CORE_CSS_ENUM_SERIALIZATION_H_
#define VELOXA_CORE_CSS_ENUM_SERIALIZATION_H_

#include "veloxa/core/css/property.h"
#include "veloxa/foundation/strings/string_view.h"

namespace vx::css {

// Returns the CSS-canonical string for an enum value of the given property,
// or an empty StringView when the property is not enum-valued or the value is
// out of range. The returned StringView points to a string literal with static
// storage duration, so callers may store/copy it freely.
//
// Examples:
//   EnumValueToCssString(PropertyId::kDisplay, 1)       == "block"
//   EnumValueToCssString(PropertyId::kPosition, 2)      == "absolute"
//   EnumValueToCssString(PropertyId::kBackgroundColor,0)== ""  // not enum-valued
//   EnumValueToCssString(PropertyId::kDisplay, 99)      == ""  // out of range
StringView EnumValueToCssString(PropertyId property, u16 enum_value);

}  // namespace vx::css
#endif  // VELOXA_CORE_CSS_ENUM_SERIALIZATION_H_
