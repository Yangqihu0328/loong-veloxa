#ifndef VELOXA_CORE_CSS_COMPUTED_STYLE_H_
#define VELOXA_CORE_CSS_COMPUTED_STYLE_H_

#include "veloxa/core/css/css_value.h"
#include "veloxa/core/css/enums.h"
#include "veloxa/core/css/transition.h"
#include "veloxa/foundation/strings/interned_string.h"

namespace vx::css {

struct ComputedStyle {
  Display display = Display::kBlock;
  Position position = Position::kStatic;
  LengthValue top;
  LengthValue right;
  LengthValue bottom;
  LengthValue left;
  LengthValue width;
  LengthValue height;
  LengthValue min_width;
  LengthValue min_height;
  LengthValue max_width;
  LengthValue max_height;
  LengthValue margin_top;
  LengthValue margin_right;
  LengthValue margin_bottom;
  LengthValue margin_left;
  LengthValue padding_top;
  LengthValue padding_right;
  LengthValue padding_bottom;
  LengthValue padding_left;
  BoxSizing box_sizing = BoxSizing::kContentBox;
  Overflow overflow = Overflow::kVisible;
  i32 z_index = 0;

  FlexDirection flex_direction = FlexDirection::kRow;
  FlexWrap flex_wrap = FlexWrap::kNowrap;
  JustifyContent justify_content = JustifyContent::kFlexStart;
  AlignItems align_items = AlignItems::kStretch;
  AlignItems align_self = AlignItems::kAuto;
  f32 flex_grow = 0.0f;
  f32 flex_shrink = 1.0f;
  LengthValue flex_basis;
  LengthValue gap;

  u32 background_color = 0x00000000;
  u32 color = 0x000000FFu;
  f32 opacity = 1.0f;
  LengthValue border_width[4];
  BorderStyle border_style[4] = {};
  u32 border_color[4] = {};
  LengthValue border_radius;
  Visibility visibility = Visibility::kVisible;

  InternedString font_family;
  LengthValue font_size;
  u16 font_weight = 400;
  CssFontStyle font_style = CssFontStyle::kNormal;
  LengthValue line_height;
  TextAlign text_align = TextAlign::kLeft;
  WhiteSpace white_space = WhiteSpace::kNormal;
  LengthValue letter_spacing;

  SmallVector<TransitionSpec, 2> transitions;
};

}  // namespace vx::css
#endif  // VELOXA_CORE_CSS_COMPUTED_STYLE_H_
