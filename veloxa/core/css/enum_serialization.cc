#include "veloxa/core/css/enum_serialization.h"

#include <cstring>

namespace vx::css {

namespace {

// Per-enum lookup table. Each entry is a NUL-terminated string literal whose
// position in the array equals the underlying integer value of the matching
// `enum class` member declared in `veloxa/core/css/enums.h`. A nullptr entry
// signals "out of range" so callers receive an empty StringView.
//
// Keep table order in lock-step with enums.h. When you add an enum value, add
// the canonical CSS spelling here AND update the matching test in
// `tests/core/css/enum_serialization_test.cc`.

constexpr const char* kDisplay[] = {
    "none", "block", "inline", "inline-block", "flex",
};
constexpr const char* kPosition[] = {
    "static", "relative", "absolute", "fixed",
};
constexpr const char* kFlexDirection[] = {
    "row", "row-reverse", "column", "column-reverse",
};
constexpr const char* kFlexWrap[] = {
    "nowrap", "wrap",
};
constexpr const char* kJustifyContent[] = {
    "flex-start", "flex-end", "center", "space-between", "space-around",
};
// Shared by align-items / align-self.
constexpr const char* kAlign[] = {
    "auto", "flex-start", "flex-end", "center", "stretch", "baseline",
};
constexpr const char* kBoxSizing[] = {
    "content-box", "border-box",
};
constexpr const char* kOverflow[] = {
    "visible", "hidden", "scroll", "auto",
};
constexpr const char* kVisibility[] = {
    "visible", "hidden",
};
constexpr const char* kTextAlign[] = {
    "left", "right", "center", "justify",
};
constexpr const char* kWhiteSpace[] = {
    "normal", "nowrap", "pre", "pre-wrap",
};
// Shared by border-{top,right,bottom,left}-style.
constexpr const char* kBorderStyle[] = {
    "none", "solid", "dashed", "dotted",
};
constexpr const char* kFontStyle[] = {
    "normal", "italic",
};

template <usize N>
StringView Lookup(const char* const (&table)[N], u16 v) {
  if (v >= N) return StringView();
  const char* s = table[v];
  if (!s) return StringView();
  return StringView(s, std::strlen(s));
}

}  // namespace

StringView EnumValueToCssString(PropertyId property, u16 enum_value) {
  switch (property) {
    case PropertyId::kDisplay:
      return Lookup(kDisplay, enum_value);
    case PropertyId::kPosition:
      return Lookup(kPosition, enum_value);
    case PropertyId::kFlexDirection:
      return Lookup(kFlexDirection, enum_value);
    case PropertyId::kFlexWrap:
      return Lookup(kFlexWrap, enum_value);
    case PropertyId::kJustifyContent:
      return Lookup(kJustifyContent, enum_value);
    case PropertyId::kAlignItems:
    case PropertyId::kAlignSelf:
      return Lookup(kAlign, enum_value);
    case PropertyId::kBoxSizing:
      return Lookup(kBoxSizing, enum_value);
    case PropertyId::kOverflow:
      return Lookup(kOverflow, enum_value);
    case PropertyId::kVisibility:
      return Lookup(kVisibility, enum_value);
    case PropertyId::kTextAlign:
      return Lookup(kTextAlign, enum_value);
    case PropertyId::kWhiteSpace:
      return Lookup(kWhiteSpace, enum_value);
    case PropertyId::kBorderTopStyle:
    case PropertyId::kBorderRightStyle:
    case PropertyId::kBorderBottomStyle:
    case PropertyId::kBorderLeftStyle:
      return Lookup(kBorderStyle, enum_value);
    case PropertyId::kFontStyle:
      return Lookup(kFontStyle, enum_value);
    default:
      return StringView();
  }
}

}  // namespace vx::css
