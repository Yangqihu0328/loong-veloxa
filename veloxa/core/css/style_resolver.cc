#include "veloxa/core/css/style_resolver.h"

#include <algorithm>

#include "veloxa/core/css/property.h"
#include "veloxa/core/css/transition.h"

namespace vx::css {

ComputedStyle StyleResolver::Resolve(
    const dom::Element* element,
    const Vector<Stylesheet>& stylesheets,
    const ComputedStyle* parent_style,
    const SmallVector<Declaration, 8>* inline_decls,
    const event::EventManager* em) {
  ComputedStyle style;

  if (parent_style != nullptr) {
    InheritProperties(style, parent_style);
  }

  Vector<MatchedRule> matched;
  CollectMatchingRules(element, stylesheets, matched, em);
  SortBySpecificity(matched);

  for (const auto& mr : matched) {
    for (const auto& decl : mr.rule->declarations) {
      if (!decl.important) {
        ApplyDeclaration(style, decl, parent_style);
      }
    }
  }

  if (inline_decls != nullptr) {
    for (const auto& decl : *inline_decls) {
      if (!decl.important) {
        ApplyDeclaration(style, decl, parent_style);
      }
    }
  }

  for (const auto& mr : matched) {
    for (const auto& decl : mr.rule->declarations) {
      if (decl.important) {
        ApplyDeclaration(style, decl, parent_style);
      }
    }
  }

  if (inline_decls != nullptr) {
    for (const auto& decl : *inline_decls) {
      if (decl.important) {
        ApplyDeclaration(style, decl, parent_style);
      }
    }
  }

  return style;
}

void StyleResolver::CollectMatchingRules(
    const dom::Element* element,
    const Vector<Stylesheet>& stylesheets,
    Vector<MatchedRule>& matched,
    const event::EventManager* em) {
  u32 order = 0;
  for (const auto& sheet : stylesheets) {
    for (const auto& rule : sheet.rules) {
      for (const auto& sel : rule.selectors) {
        if (SelectorMatcher::Matches(sel, element, em)) {
          matched.push_back({&rule, sel.specificity, order});
          ++order;
          break;
        }
      }
    }
  }
}

void StyleResolver::SortBySpecificity(Vector<MatchedRule>& rules) {
  std::sort(rules.begin(), rules.end(),
            [](const MatchedRule& a, const MatchedRule& b) {
              if (a.specificity != b.specificity)
                return a.specificity < b.specificity;
              return a.order < b.order;
            });
}

static LengthValue ToLengthValue(const CssValue& v) {
  if (v.type == ValueType::kLength) return {v.number, v.unit};
  if (v.type == ValueType::kAuto) return LengthValue::Auto();
  return {};
}

void StyleResolver::ApplyDeclaration(ComputedStyle& style,
                                      const Declaration& decl,
                                      const ComputedStyle* parent_style) {
  if (decl.value.type == ValueType::kInherit && parent_style != nullptr) {
    switch (decl.property) {
      case PropertyId::kColor: style.color = parent_style->color; return;
      case PropertyId::kVisibility: style.visibility = parent_style->visibility; return;
      case PropertyId::kFontFamily: style.font_family = parent_style->font_family; return;
      case PropertyId::kFontSize: style.font_size = parent_style->font_size; return;
      case PropertyId::kFontWeight: style.font_weight = parent_style->font_weight; return;
      case PropertyId::kFontStyle: style.font_style = parent_style->font_style; return;
      case PropertyId::kLineHeight: style.line_height = parent_style->line_height; return;
      case PropertyId::kTextAlign: style.text_align = parent_style->text_align; return;
      case PropertyId::kWhiteSpace: style.white_space = parent_style->white_space; return;
      case PropertyId::kLetterSpacing: style.letter_spacing = parent_style->letter_spacing; return;
      case PropertyId::kDisplay: style.display = parent_style->display; return;
      case PropertyId::kPosition: style.position = parent_style->position; return;
      case PropertyId::kTop: style.top = parent_style->top; return;
      case PropertyId::kRight: style.right = parent_style->right; return;
      case PropertyId::kBottom: style.bottom = parent_style->bottom; return;
      case PropertyId::kLeft: style.left = parent_style->left; return;
      case PropertyId::kWidth: style.width = parent_style->width; return;
      case PropertyId::kHeight: style.height = parent_style->height; return;
      case PropertyId::kOpacity: style.opacity = parent_style->opacity; return;
      default: return;
    }
  }

  if (decl.value.type == ValueType::kInitial) {
    ComputedStyle defaults;
    switch (decl.property) {
      case PropertyId::kColor: style.color = defaults.color; return;
      case PropertyId::kDisplay: style.display = defaults.display; return;
      case PropertyId::kVisibility: style.visibility = defaults.visibility; return;
      case PropertyId::kFontSize: style.font_size = defaults.font_size; return;
      case PropertyId::kFontWeight: style.font_weight = defaults.font_weight; return;
      default: return;
    }
  }

  switch (decl.property) {
    case PropertyId::kDisplay:
      if (decl.value.type == ValueType::kEnum)
        style.display = static_cast<Display>(decl.value.enum_value);
      break;
    case PropertyId::kPosition:
      if (decl.value.type == ValueType::kEnum)
        style.position = static_cast<Position>(decl.value.enum_value);
      break;
    case PropertyId::kTop:
      style.top = ToLengthValue(decl.value);
      break;
    case PropertyId::kRight:
      style.right = ToLengthValue(decl.value);
      break;
    case PropertyId::kBottom:
      style.bottom = ToLengthValue(decl.value);
      break;
    case PropertyId::kLeft:
      style.left = ToLengthValue(decl.value);
      break;
    case PropertyId::kWidth:
      style.width = ToLengthValue(decl.value);
      break;
    case PropertyId::kHeight:
      style.height = ToLengthValue(decl.value);
      break;
    case PropertyId::kMinWidth:
      style.min_width = ToLengthValue(decl.value);
      break;
    case PropertyId::kMinHeight:
      style.min_height = ToLengthValue(decl.value);
      break;
    case PropertyId::kMaxWidth:
      style.max_width = ToLengthValue(decl.value);
      break;
    case PropertyId::kMaxHeight:
      style.max_height = ToLengthValue(decl.value);
      break;
    case PropertyId::kMarginTop:
      style.margin_top = ToLengthValue(decl.value);
      break;
    case PropertyId::kMarginRight:
      style.margin_right = ToLengthValue(decl.value);
      break;
    case PropertyId::kMarginBottom:
      style.margin_bottom = ToLengthValue(decl.value);
      break;
    case PropertyId::kMarginLeft:
      style.margin_left = ToLengthValue(decl.value);
      break;
    case PropertyId::kPaddingTop:
      style.padding_top = ToLengthValue(decl.value);
      break;
    case PropertyId::kPaddingRight:
      style.padding_right = ToLengthValue(decl.value);
      break;
    case PropertyId::kPaddingBottom:
      style.padding_bottom = ToLengthValue(decl.value);
      break;
    case PropertyId::kPaddingLeft:
      style.padding_left = ToLengthValue(decl.value);
      break;
    case PropertyId::kBoxSizing:
      if (decl.value.type == ValueType::kEnum)
        style.box_sizing = static_cast<BoxSizing>(decl.value.enum_value);
      break;
    case PropertyId::kOverflow:
      if (decl.value.type == ValueType::kEnum)
        style.overflow = static_cast<Overflow>(decl.value.enum_value);
      else if (decl.value.type == ValueType::kAuto)
        // Parser ParseDeclaration 对 "auto" 走全局快路径返回 ValueType::kAuto
        // 而非 enum；overflow 是合法的 enum 值（CSS 2.1 §11.1.1），此处补合规。
        style.overflow = Overflow::kAuto;
      break;
    case PropertyId::kZIndex:
      if (decl.value.type == ValueType::kNumber)
        style.z_index = static_cast<i32>(decl.value.number);
      break;
    case PropertyId::kFlexDirection:
      if (decl.value.type == ValueType::kEnum)
        style.flex_direction = static_cast<FlexDirection>(decl.value.enum_value);
      break;
    case PropertyId::kFlexWrap:
      if (decl.value.type == ValueType::kEnum)
        style.flex_wrap = static_cast<FlexWrap>(decl.value.enum_value);
      break;
    case PropertyId::kJustifyContent:
      if (decl.value.type == ValueType::kEnum)
        style.justify_content = static_cast<JustifyContent>(decl.value.enum_value);
      break;
    case PropertyId::kAlignItems:
      if (decl.value.type == ValueType::kEnum)
        style.align_items = static_cast<AlignItems>(decl.value.enum_value);
      break;
    case PropertyId::kAlignSelf:
      if (decl.value.type == ValueType::kEnum)
        style.align_self = static_cast<AlignItems>(decl.value.enum_value);
      break;
    case PropertyId::kFlexGrow:
      if (decl.value.type == ValueType::kNumber)
        style.flex_grow = decl.value.number;
      break;
    case PropertyId::kFlexShrink:
      if (decl.value.type == ValueType::kNumber)
        style.flex_shrink = decl.value.number;
      break;
    case PropertyId::kFlexBasis:
      style.flex_basis = ToLengthValue(decl.value);
      break;
    case PropertyId::kGap:
      style.gap = ToLengthValue(decl.value);
      break;
    case PropertyId::kBackgroundColor:
      if (decl.value.type == ValueType::kColor)
        style.background_color = decl.value.color;
      break;
    case PropertyId::kColor:
      if (decl.value.type == ValueType::kColor)
        style.color = decl.value.color;
      break;
    case PropertyId::kOpacity:
      if (decl.value.type == ValueType::kNumber)
        style.opacity = decl.value.number;
      break;
    case PropertyId::kBorderTopWidth:
      style.border_width[0] = ToLengthValue(decl.value);
      break;
    case PropertyId::kBorderRightWidth:
      style.border_width[1] = ToLengthValue(decl.value);
      break;
    case PropertyId::kBorderBottomWidth:
      style.border_width[2] = ToLengthValue(decl.value);
      break;
    case PropertyId::kBorderLeftWidth:
      style.border_width[3] = ToLengthValue(decl.value);
      break;
    case PropertyId::kBorderTopStyle:
      if (decl.value.type == ValueType::kEnum)
        style.border_style[0] = static_cast<BorderStyle>(decl.value.enum_value);
      break;
    case PropertyId::kBorderRightStyle:
      if (decl.value.type == ValueType::kEnum)
        style.border_style[1] = static_cast<BorderStyle>(decl.value.enum_value);
      break;
    case PropertyId::kBorderBottomStyle:
      if (decl.value.type == ValueType::kEnum)
        style.border_style[2] = static_cast<BorderStyle>(decl.value.enum_value);
      break;
    case PropertyId::kBorderLeftStyle:
      if (decl.value.type == ValueType::kEnum)
        style.border_style[3] = static_cast<BorderStyle>(decl.value.enum_value);
      break;
    case PropertyId::kBorderTopColor:
      if (decl.value.type == ValueType::kColor)
        style.border_color[0] = decl.value.color;
      break;
    case PropertyId::kBorderRightColor:
      if (decl.value.type == ValueType::kColor)
        style.border_color[1] = decl.value.color;
      break;
    case PropertyId::kBorderBottomColor:
      if (decl.value.type == ValueType::kColor)
        style.border_color[2] = decl.value.color;
      break;
    case PropertyId::kBorderLeftColor:
      if (decl.value.type == ValueType::kColor)
        style.border_color[3] = decl.value.color;
      break;
    case PropertyId::kBorderRadius:
      style.border_radius = ToLengthValue(decl.value);
      break;
    case PropertyId::kVisibility:
      if (decl.value.type == ValueType::kEnum)
        style.visibility = static_cast<Visibility>(decl.value.enum_value);
      break;
    case PropertyId::kFontFamily:
      if (decl.value.type == ValueType::kEnum || decl.value.type == ValueType::kNone) {
        // Font family handled via InternedString in parser; skip if not directly settable
      }
      break;
    case PropertyId::kFontSize:
      style.font_size = ToLengthValue(decl.value);
      break;
    case PropertyId::kFontWeight:
      if (decl.value.type == ValueType::kNumber)
        style.font_weight = static_cast<u16>(decl.value.number);
      break;
    case PropertyId::kFontStyle:
      if (decl.value.type == ValueType::kEnum)
        style.font_style = static_cast<CssFontStyle>(decl.value.enum_value);
      break;
    case PropertyId::kLineHeight:
      if (decl.value.type == ValueType::kNumber)
        style.line_height = {decl.value.number, Unit::kNumber};
      else
        style.line_height = ToLengthValue(decl.value);
      break;
    case PropertyId::kTextAlign:
      if (decl.value.type == ValueType::kEnum)
        style.text_align = static_cast<TextAlign>(decl.value.enum_value);
      break;
    case PropertyId::kWhiteSpace:
      if (decl.value.type == ValueType::kEnum)
        style.white_space = static_cast<WhiteSpace>(decl.value.enum_value);
      break;
    case PropertyId::kLetterSpacing:
      style.letter_spacing = ToLengthValue(decl.value);
      break;
    case PropertyId::kVerticalAlign:
      // 混合类型：parser 路径 ident → kEnum；length/percent → kLength。
      if (decl.value.type == ValueType::kEnum) {
        style.vertical_align = static_cast<VerticalAlign>(decl.value.enum_value);
        style.vertical_align_offset = LengthValue{};
      } else if (decl.value.type == ValueType::kLength) {
        style.vertical_align = VerticalAlign::kLength;
        style.vertical_align_offset = ToLengthValue(decl.value);
      }
      break;
    case PropertyId::kTransitionProperty:
      if (decl.value.type == ValueType::kEnum) {
        if (style.transitions.empty())
          style.transitions.push_back(TransitionSpec{});
        style.transitions[0].property =
            static_cast<PropertyId>(decl.value.enum_value);
      }
      break;
    case PropertyId::kTransitionDuration:
      if (decl.value.type == ValueType::kNumber) {
        if (style.transitions.empty())
          style.transitions.push_back(TransitionSpec{});
        style.transitions[0].duration_ms = decl.value.number;
      }
      break;
    case PropertyId::kTransitionTimingFunction:
      if (decl.value.type == ValueType::kEnum) {
        if (style.transitions.empty())
          style.transitions.push_back(TransitionSpec{});
        auto tf = static_cast<TimingFunction>(decl.value.enum_value);
        style.transitions[0].timing = tf;
        style.transitions[0].bezier = CubicBezier::FromTiming(tf);
      }
      break;
    case PropertyId::kTransitionDelay:
      if (decl.value.type == ValueType::kNumber) {
        if (style.transitions.empty())
          style.transitions.push_back(TransitionSpec{});
        style.transitions[0].delay_ms = decl.value.number;
      }
      break;
    default:
      break;
  }
}

void StyleResolver::InheritProperties(ComputedStyle& style,
                                       const ComputedStyle* parent_style) {
  style.color = parent_style->color;
  style.visibility = parent_style->visibility;
  style.font_family = parent_style->font_family;
  style.font_size = parent_style->font_size;
  style.font_weight = parent_style->font_weight;
  style.font_style = parent_style->font_style;
  style.line_height = parent_style->line_height;
  style.text_align = parent_style->text_align;
  style.white_space = parent_style->white_space;
  style.letter_spacing = parent_style->letter_spacing;
}

}  // namespace vx::css
