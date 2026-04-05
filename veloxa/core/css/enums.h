#ifndef VELOXA_CORE_CSS_ENUMS_H_
#define VELOXA_CORE_CSS_ENUMS_H_

#include "veloxa/foundation/base/types.h"

namespace vx::css {

enum class Display : u8 {
  kNone,
  kBlock,
  kInline,
  kInlineBlock,
  kFlex,
};

enum class Position : u8 {
  kStatic,
  kRelative,
  kAbsolute,
  kFixed,
};

enum class FlexDirection : u8 {
  kRow,
  kRowReverse,
  kColumn,
  kColumnReverse,
};

enum class FlexWrap : u8 {
  kNowrap,
  kWrap,
};

enum class JustifyContent : u8 {
  kFlexStart,
  kFlexEnd,
  kCenter,
  kSpaceBetween,
  kSpaceAround,
};

enum class AlignItems : u8 {
  kAuto,
  kFlexStart,
  kFlexEnd,
  kCenter,
  kStretch,
  kBaseline,
};

enum class BoxSizing : u8 {
  kContentBox,
  kBorderBox,
};

enum class Overflow : u8 {
  kVisible,
  kHidden,
  kScroll,
  kAuto,
};

enum class Visibility : u8 {
  kVisible,
  kHidden,
};

enum class TextAlign : u8 {
  kLeft,
  kRight,
  kCenter,
  kJustify,
};

enum class WhiteSpace : u8 {
  kNormal,
  kNowrap,
  kPre,
  kPreWrap,
};

enum class BorderStyle : u8 {
  kNone,
  kSolid,
  kDashed,
  kDotted,
};

enum class CssFontStyle : u8 {
  kNormal,
  kItalic,
};

}  // namespace vx::css
#endif  // VELOXA_CORE_CSS_ENUMS_H_
