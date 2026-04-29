#include "veloxa/core/css/enum_serialization.h"

#include <cstddef>
#include <cstring>
#include <iterator>

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
// vertical-align: 8 关键字 + kLength sentinel（length 走 length 路径，不在此表）。
// 顺序必须与 enums.h `VerticalAlign` 枚举一致。
constexpr const char* kVerticalAlign[] = {
    "baseline", "sub",     "super", "middle", "text-top",
    "text-bottom", "top",  "bottom", nullptr,  // kLength 不返回字符串
};

// Bounds-checked indexed lookup over a string-pointer table.
//
// History: this used to be a `template<usize N> Lookup(const char* const
// (&table)[N], u16 v)` taking the array by reference so callers could omit the
// length. GCC 11+ at -O2 cloned this template across every distinct N (we have
// 5 different sizes here) and then mis-correlated the cross-clone value-range
// analysis: it inferred that an access proven safe in one clone's [5]-typed
// table was "partly outside" another clone's [2]/[4]-typed table, and -Werror
// =array-bounds turned the false positive into a build failure under Release.
// Detemplatising removes the IPA cloning trigger entirely. Callers now pass
// the length explicitly via `std::size(arr)`; we keep this behind the
// VX_LOOKUP() macro below so the array name and its size cannot drift apart.
StringView LookupImpl(const char* const* table, std::size_t n, u16 v) {
  if (v >= n) return StringView();
  const char* s = table[v];
  if (!s) return StringView();
  return StringView(s, std::strlen(s));
}

// Single-source-of-truth wrapper: deriving the size from `arr` itself prevents
// the kind of length/array mismatch the old template variant guarded against.
// Scoped to this TU; #undef'd after EnumValueToCssString below so the macro
// does not leak into other code via header includes.
#define VX_LOOKUP(arr, v) LookupImpl((arr), std::size(arr), (v))

}  // namespace

StringView EnumValueToCssString(PropertyId property, u16 enum_value) {
  switch (property) {
    case PropertyId::kDisplay:
      return VX_LOOKUP(kDisplay, enum_value);
    case PropertyId::kPosition:
      return VX_LOOKUP(kPosition, enum_value);
    case PropertyId::kFlexDirection:
      return VX_LOOKUP(kFlexDirection, enum_value);
    case PropertyId::kFlexWrap:
      return VX_LOOKUP(kFlexWrap, enum_value);
    case PropertyId::kJustifyContent:
      return VX_LOOKUP(kJustifyContent, enum_value);
    case PropertyId::kAlignItems:
    case PropertyId::kAlignSelf:
      return VX_LOOKUP(kAlign, enum_value);
    case PropertyId::kBoxSizing:
      return VX_LOOKUP(kBoxSizing, enum_value);
    case PropertyId::kOverflow:
      return VX_LOOKUP(kOverflow, enum_value);
    case PropertyId::kVisibility:
      return VX_LOOKUP(kVisibility, enum_value);
    case PropertyId::kTextAlign:
      return VX_LOOKUP(kTextAlign, enum_value);
    case PropertyId::kWhiteSpace:
      return VX_LOOKUP(kWhiteSpace, enum_value);
    case PropertyId::kBorderTopStyle:
    case PropertyId::kBorderRightStyle:
    case PropertyId::kBorderBottomStyle:
    case PropertyId::kBorderLeftStyle:
      return VX_LOOKUP(kBorderStyle, enum_value);
    case PropertyId::kFontStyle:
      return VX_LOOKUP(kFontStyle, enum_value);
    case PropertyId::kVerticalAlign:
      return VX_LOOKUP(kVerticalAlign, enum_value);
    default:
      return StringView();
  }
}

#undef VX_LOOKUP

}  // namespace vx::css
