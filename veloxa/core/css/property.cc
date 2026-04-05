#include "veloxa/core/css/property.h"

#include <cstring>

namespace vx::css {

namespace {

// clang-format off
constexpr PropertyInfo kPropertyTable[] = {
  {PropertyId::kUnknown,           "",                   false},
  {PropertyId::kDisplay,           "display",            false},
  {PropertyId::kPosition,          "position",           false},
  {PropertyId::kTop,               "top",                false},
  {PropertyId::kRight,             "right",              false},
  {PropertyId::kBottom,            "bottom",             false},
  {PropertyId::kLeft,              "left",               false},
  {PropertyId::kWidth,             "width",              false},
  {PropertyId::kHeight,            "height",             false},
  {PropertyId::kMinWidth,          "min-width",          false},
  {PropertyId::kMinHeight,         "min-height",         false},
  {PropertyId::kMaxWidth,          "max-width",          false},
  {PropertyId::kMaxHeight,         "max-height",         false},
  {PropertyId::kMarginTop,         "margin-top",         false},
  {PropertyId::kMarginRight,       "margin-right",       false},
  {PropertyId::kMarginBottom,      "margin-bottom",      false},
  {PropertyId::kMarginLeft,        "margin-left",        false},
  {PropertyId::kPaddingTop,        "padding-top",        false},
  {PropertyId::kPaddingRight,      "padding-right",      false},
  {PropertyId::kPaddingBottom,     "padding-bottom",     false},
  {PropertyId::kPaddingLeft,       "padding-left",       false},
  {PropertyId::kBoxSizing,         "box-sizing",         false},
  {PropertyId::kOverflow,          "overflow",           false},
  {PropertyId::kZIndex,            "z-index",            false},
  {PropertyId::kFlexDirection,     "flex-direction",     false},
  {PropertyId::kFlexWrap,          "flex-wrap",          false},
  {PropertyId::kJustifyContent,    "justify-content",    false},
  {PropertyId::kAlignItems,        "align-items",        false},
  {PropertyId::kAlignSelf,         "align-self",         false},
  {PropertyId::kFlexGrow,          "flex-grow",          false},
  {PropertyId::kFlexShrink,        "flex-shrink",        false},
  {PropertyId::kFlexBasis,         "flex-basis",         false},
  {PropertyId::kGap,               "gap",                false},
  {PropertyId::kBackgroundColor,   "background-color",   false},
  {PropertyId::kColor,             "color",              true},
  {PropertyId::kOpacity,           "opacity",            false},
  {PropertyId::kBorderTopWidth,    "border-top-width",   false},
  {PropertyId::kBorderRightWidth,  "border-right-width", false},
  {PropertyId::kBorderBottomWidth, "border-bottom-width",false},
  {PropertyId::kBorderLeftWidth,   "border-left-width",  false},
  {PropertyId::kBorderTopStyle,    "border-top-style",   false},
  {PropertyId::kBorderRightStyle,  "border-right-style", false},
  {PropertyId::kBorderBottomStyle, "border-bottom-style",false},
  {PropertyId::kBorderLeftStyle,   "border-left-style",  false},
  {PropertyId::kBorderTopColor,    "border-top-color",   false},
  {PropertyId::kBorderRightColor,  "border-right-color", false},
  {PropertyId::kBorderBottomColor, "border-bottom-color",false},
  {PropertyId::kBorderLeftColor,   "border-left-color",  false},
  {PropertyId::kBorderRadius,      "border-radius",      false},
  {PropertyId::kVisibility,        "visibility",         true},
  {PropertyId::kFontFamily,        "font-family",        true},
  {PropertyId::kFontSize,          "font-size",          true},
  {PropertyId::kFontWeight,        "font-weight",        true},
  {PropertyId::kFontStyle,         "font-style",         true},
  {PropertyId::kLineHeight,        "line-height",        true},
  {PropertyId::kTextAlign,         "text-align",         true},
  {PropertyId::kWhiteSpace,        "white-space",        true},
  {PropertyId::kLetterSpacing,     "letter-spacing",     true},
};
// clang-format on

constexpr usize kPropertyTableSize =
    sizeof(kPropertyTable) / sizeof(kPropertyTable[0]);

static_assert(kPropertyTableSize == static_cast<usize>(PropertyId::kMaxProperty),
              "Property table size must match PropertyId::kMaxProperty");

}  // namespace

const PropertyInfo& GetPropertyInfo(PropertyId id) {
  auto idx = static_cast<u8>(id);
  if (idx >= kPropertyTableSize) return kPropertyTable[0];
  return kPropertyTable[idx];
}

PropertyId PropertyIdFromName(StringView name) {
  for (usize i = 1; i < kPropertyTableSize; ++i) {
    if (name.size() == std::strlen(kPropertyTable[i].name) &&
        name.compare(StringView(kPropertyTable[i].name,
                                std::strlen(kPropertyTable[i].name))) == 0) {
      return kPropertyTable[i].id;
    }
  }
  return PropertyId::kUnknown;
}

bool IsInheritedProperty(PropertyId id) {
  return GetPropertyInfo(id).inherited;
}

}  // namespace vx::css
