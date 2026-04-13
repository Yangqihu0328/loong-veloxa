#ifndef VELOXA_CORE_CSS_PROPERTY_H_
#define VELOXA_CORE_CSS_PROPERTY_H_

#include "veloxa/foundation/base/types.h"
#include "veloxa/foundation/strings/string_view.h"

namespace vx::css {

enum class PropertyId : u8 {
  kUnknown = 0,
  kDisplay,
  kPosition,
  kTop,
  kRight,
  kBottom,
  kLeft,
  kWidth,
  kHeight,
  kMinWidth,
  kMinHeight,
  kMaxWidth,
  kMaxHeight,
  kMarginTop,
  kMarginRight,
  kMarginBottom,
  kMarginLeft,
  kPaddingTop,
  kPaddingRight,
  kPaddingBottom,
  kPaddingLeft,
  kBoxSizing,
  kOverflow,
  kZIndex,
  kFlexDirection,
  kFlexWrap,
  kJustifyContent,
  kAlignItems,
  kAlignSelf,
  kFlexGrow,
  kFlexShrink,
  kFlexBasis,
  kGap,
  kBackgroundColor,
  kColor,
  kOpacity,
  kBorderTopWidth,
  kBorderRightWidth,
  kBorderBottomWidth,
  kBorderLeftWidth,
  kBorderTopStyle,
  kBorderRightStyle,
  kBorderBottomStyle,
  kBorderLeftStyle,
  kBorderTopColor,
  kBorderRightColor,
  kBorderBottomColor,
  kBorderLeftColor,
  kBorderRadius,
  kVisibility,
  kFontFamily,
  kFontSize,
  kFontWeight,
  kFontStyle,
  kLineHeight,
  kTextAlign,
  kWhiteSpace,
  kLetterSpacing,
  kTransitionProperty,
  kTransitionDuration,
  kTransitionTimingFunction,
  kTransitionDelay,
  kMaxProperty,
};

struct PropertyInfo {
  PropertyId id;
  const char* name;
  bool inherited;
};

const PropertyInfo& GetPropertyInfo(PropertyId id);
PropertyId PropertyIdFromName(StringView name);
bool IsInheritedProperty(PropertyId id);

}  // namespace vx::css
#endif  // VELOXA_CORE_CSS_PROPERTY_H_
