#ifndef VELOXA_CORE_CSS_CSS_VALUE_H_
#define VELOXA_CORE_CSS_CSS_VALUE_H_

#include "veloxa/foundation/base/types.h"

namespace vx::css {

enum class Unit : u8 {
  kNone,
  kPx,
  kEm,
  kRem,
  kPercent,
  kVw,
  kVh,
  kAuto,
  kNumber,
};

enum class ValueType : u8 {
  kNone,
  kLength,
  kColor,
  kEnum,
  kNumber,
  kAuto,
  kInherit,
  kInitial,
};

struct CssValue {
  ValueType type = ValueType::kNone;
  Unit unit = Unit::kNone;
  union {
    f32 number;
    u32 color;
    u16 enum_value;
  };

  static CssValue None() { return {}; }

  static CssValue Length(f32 val, Unit u) {
    CssValue v;
    v.type = ValueType::kLength;
    v.unit = u;
    v.number = val;
    return v;
  }

  static CssValue Color(u32 rgba) {
    CssValue v;
    v.type = ValueType::kColor;
    v.color = rgba;
    return v;
  }

  static CssValue Enum(u16 e) {
    CssValue v;
    v.type = ValueType::kEnum;
    v.enum_value = e;
    return v;
  }

  static CssValue Number(f32 n) {
    CssValue v;
    v.type = ValueType::kNumber;
    v.number = n;
    return v;
  }

  static CssValue Auto() {
    CssValue v;
    v.type = ValueType::kAuto;
    v.unit = Unit::kAuto;
    return v;
  }

  static CssValue Inherit() {
    CssValue v;
    v.type = ValueType::kInherit;
    return v;
  }

  static CssValue Initial() {
    CssValue v;
    v.type = ValueType::kInitial;
    return v;
  }
};

struct LengthValue {
  f32 value = 0.0f;
  Unit unit = Unit::kNone;

  bool is_auto() const { return unit == Unit::kAuto; }
  bool is_none() const { return unit == Unit::kNone; }

  static LengthValue Px(f32 v) { return {v, Unit::kPx}; }
  static LengthValue Em(f32 v) { return {v, Unit::kEm}; }
  static LengthValue Percent(f32 v) { return {v, Unit::kPercent}; }
  static LengthValue Auto() { return {0.0f, Unit::kAuto}; }
};

}  // namespace vx::css
#endif  // VELOXA_CORE_CSS_CSS_VALUE_H_
