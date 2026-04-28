#include "veloxa/core/css/parser.h"

#include <cctype>
#include <cstring>

#include "veloxa/core/css/enums.h"
#include "veloxa/core/css/transition.h"

namespace vx::css {

namespace {

char ToLower(char c) {
  if (c >= 'A' && c <= 'Z') return static_cast<char>(c + ('a' - 'A'));
  return c;
}

bool EqualsIgnoreCase(StringView a, StringView b) {
  if (a.size() != b.size()) return false;
  for (usize i = 0; i < a.size(); ++i) {
    if (ToLower(a[i]) != ToLower(b[i])) return false;
  }
  return true;
}

u8 HexDigit(char c) {
  if (c >= '0' && c <= '9') return static_cast<u8>(c - '0');
  if (c >= 'a' && c <= 'f') return static_cast<u8>(c - 'a' + 10);
  if (c >= 'A' && c <= 'F') return static_cast<u8>(c - 'A' + 10);
  return 0;
}

struct NamedColor {
  const char* name;
  u32 rgba;
};

constexpr NamedColor kNamedColors[] = {
    {"aqua", 0x00FFFFFF},       {"black", 0x000000FF},
    {"blue", 0x0000FFFF},       {"cyan", 0x00FFFFFF},
    {"gray", 0x808080FF},       {"green", 0x008000FF},
    {"lime", 0x00FF00FF},       {"maroon", 0x800000FF},
    {"navy", 0x000080FF},       {"olive", 0x808000FF},
    {"orange", 0xFFA500FF},     {"purple", 0x800080FF},
    {"red", 0xFF0000FF},        {"silver", 0xC0C0C0FF},
    {"teal", 0x008080FF},       {"transparent", 0x00000000},
    {"white", 0xFFFFFFFF},      {"yellow", 0xFFFF00FF},
};

constexpr usize kNamedColorCount = sizeof(kNamedColors) / sizeof(kNamedColors[0]);

int CompareIgnoreCase(StringView sv, const char* s) {
  usize len = std::strlen(s);
  usize min_len = sv.size() < len ? sv.size() : len;
  for (usize i = 0; i < min_len; ++i) {
    char a = ToLower(sv[i]);
    char b = ToLower(s[i]);
    if (a < b) return -1;
    if (a > b) return 1;
  }
  if (sv.size() < len) return -1;
  if (sv.size() > len) return 1;
  return 0;
}

CssValue LookupNamedColor(StringView name) {
  usize lo = 0;
  usize hi = kNamedColorCount;
  while (lo < hi) {
    usize mid = lo + (hi - lo) / 2;
    int cmp = CompareIgnoreCase(name, kNamedColors[mid].name);
    if (cmp == 0) return CssValue::Color(kNamedColors[mid].rgba);
    if (cmp < 0)
      hi = mid;
    else
      lo = mid + 1;
  }
  return CssValue::None();
}

bool IsLengthProperty(PropertyId prop) {
  switch (prop) {
    case PropertyId::kTop:
    case PropertyId::kRight:
    case PropertyId::kBottom:
    case PropertyId::kLeft:
    case PropertyId::kWidth:
    case PropertyId::kHeight:
    case PropertyId::kMinWidth:
    case PropertyId::kMinHeight:
    case PropertyId::kMaxWidth:
    case PropertyId::kMaxHeight:
    case PropertyId::kMarginTop:
    case PropertyId::kMarginRight:
    case PropertyId::kMarginBottom:
    case PropertyId::kMarginLeft:
    case PropertyId::kPaddingTop:
    case PropertyId::kPaddingRight:
    case PropertyId::kPaddingBottom:
    case PropertyId::kPaddingLeft:
    case PropertyId::kBorderTopWidth:
    case PropertyId::kBorderRightWidth:
    case PropertyId::kBorderBottomWidth:
    case PropertyId::kBorderLeftWidth:
    case PropertyId::kBorderRadius:
    case PropertyId::kFontSize:
    case PropertyId::kLineHeight:
    case PropertyId::kLetterSpacing:
    case PropertyId::kFlexBasis:
    case PropertyId::kGap:
      return true;
    default:
      return false;
  }
}

bool IsColorProperty(PropertyId prop) {
  switch (prop) {
    case PropertyId::kColor:
    case PropertyId::kBackgroundColor:
    case PropertyId::kBorderTopColor:
    case PropertyId::kBorderRightColor:
    case PropertyId::kBorderBottomColor:
    case PropertyId::kBorderLeftColor:
      return true;
    default:
      return false;
  }
}

bool IsEnumProperty(PropertyId prop) {
  switch (prop) {
    case PropertyId::kDisplay:
    case PropertyId::kPosition:
    case PropertyId::kBoxSizing:
    case PropertyId::kOverflow:
    case PropertyId::kVisibility:
    case PropertyId::kFlexDirection:
    case PropertyId::kFlexWrap:
    case PropertyId::kJustifyContent:
    case PropertyId::kAlignItems:
    case PropertyId::kAlignSelf:
    case PropertyId::kTextAlign:
    case PropertyId::kWhiteSpace:
    case PropertyId::kBorderTopStyle:
    case PropertyId::kBorderRightStyle:
    case PropertyId::kBorderBottomStyle:
    case PropertyId::kBorderLeftStyle:
    case PropertyId::kFontStyle:
      return true;
    default:
      return false;
  }
}

bool IsNumberProperty(PropertyId prop) {
  switch (prop) {
    case PropertyId::kOpacity:
    case PropertyId::kFlexGrow:
    case PropertyId::kFlexShrink:
    case PropertyId::kZIndex:
    case PropertyId::kFontWeight:
      return true;
    default:
      return false;
  }
}

}  // namespace

u32 ComputeSpecificity(const Selector& sel) {
  u32 a = 0, b = 0, c = 0;
  for (const auto& compound : sel.compounds) {
    for (const auto& simple : compound.simple_selectors) {
      switch (simple.type) {
        case SimpleSelectorType::kId:
          ++a;
          break;
        case SimpleSelectorType::kClass:
        case SimpleSelectorType::kAttribute:
        case SimpleSelectorType::kPseudoClass:
          ++b;
          break;
        case SimpleSelectorType::kTag:
          ++c;
          break;
        case SimpleSelectorType::kUniversal:
          break;
      }
    }
  }
  return (a << 16) | (b << 8) | c;
}

CssParser::CssParser(StringView css) : tokenizer_(css) {}

Stylesheet CssParser::Parse(StringView css) {
  CssParser parser(css);
  return parser.DoParse();
}

SmallVector<Declaration, 8> CssParser::ParseDeclarationList(StringView css) {
  CssParser parser(css);
  return parser.DoParseDeclarations();
}

void CssParser::SkipWhitespace() {
  while (tokenizer_.Peek().type == CssTokenType::kWhitespace) {
    tokenizer_.Next();
  }
}

CssToken CssParser::NextNonWS() {
  SkipWhitespace();
  return tokenizer_.Next();
}

CssToken CssParser::PeekNonWS() {
  SkipWhitespace();
  return tokenizer_.Peek();
}

Stylesheet CssParser::DoParse() {
  Stylesheet sheet;
  for (;;) {
    SkipWhitespace();
    CssToken t = tokenizer_.Peek();
    if (t.type == CssTokenType::kEof) break;

    if (t.type == CssTokenType::kAtKeyword) {
      tokenizer_.Next();
      int depth = 0;
      for (;;) {
        CssToken at = tokenizer_.Next();
        if (at.type == CssTokenType::kEof) break;
        if (at.type == CssTokenType::kLeftBrace) ++depth;
        if (at.type == CssTokenType::kRightBrace) {
          if (depth <= 1) break;
          --depth;
        }
      }
      continue;
    }

    if (t.type == CssTokenType::kCDO || t.type == CssTokenType::kCDC) {
      tokenizer_.Next();
      continue;
    }

    sheet.rules.push_back(ParseRule());
  }
  return sheet;
}

StyleRule CssParser::ParseRule() {
  StyleRule rule;
  rule.selectors = ParseSelectorList();

  CssToken t = NextNonWS();
  if (t.type != CssTokenType::kLeftBrace) return rule;

  rule.declarations = DoParseDeclarations();

  SkipWhitespace();
  if (tokenizer_.Peek().type == CssTokenType::kRightBrace) {
    tokenizer_.Next();
  }
  return rule;
}

SmallVector<Selector, 1> CssParser::ParseSelectorList() {
  SmallVector<Selector, 1> selectors;
  selectors.push_back(ParseSelector());

  while (PeekNonWS().type == CssTokenType::kComma) {
    tokenizer_.Next();
    selectors.push_back(ParseSelector());
  }
  return selectors;
}

Selector CssParser::ParseSelector() {
  Selector sel;
  sel.compounds.push_back(ParseCompoundSelector());

  for (;;) {
    CssToken t = tokenizer_.Peek();

    if (t.type == CssTokenType::kLeftBrace || t.type == CssTokenType::kComma ||
        t.type == CssTokenType::kEof) {
      break;
    }

    Combinator comb = Combinator::kNone;

    if (t.type == CssTokenType::kWhitespace) {
      tokenizer_.Next();
      CssToken next = tokenizer_.Peek();

      if (next.type == CssTokenType::kLeftBrace ||
          next.type == CssTokenType::kComma ||
          next.type == CssTokenType::kEof) {
        break;
      }

      if (next.type == CssTokenType::kDelim && next.value == ">") {
        tokenizer_.Next();
        comb = Combinator::kChild;
        SkipWhitespace();
      } else {
        comb = Combinator::kDescendant;
      }
    } else if (t.type == CssTokenType::kDelim && t.value == ">") {
      tokenizer_.Next();
      comb = Combinator::kChild;
      SkipWhitespace();
    } else {
      break;
    }

    CompoundSelector cs = ParseCompoundSelector();
    sel.compounds.back().combinator = comb;
    sel.compounds.push_back(std::move(cs));
  }

  usize n = sel.compounds.size();
  for (usize i = 0; i < n / 2; ++i) {
    usize j = n - 1 - i;
    CompoundSelector tmp = std::move(sel.compounds[i]);
    sel.compounds[i] = std::move(sel.compounds[j]);
    sel.compounds[j] = std::move(tmp);
  }

  sel.specificity = ComputeSpecificity(sel);
  return sel;
}

CompoundSelector CssParser::ParseCompoundSelector() {
  CompoundSelector cs;

  for (;;) {
    CssToken t = tokenizer_.Peek();

    if (t.type == CssTokenType::kIdent) {
      SimpleSelector ss;
      ss.type = SimpleSelectorType::kTag;
      tokenizer_.Next();
      ss.value = InternedString::Intern(t.value);
      cs.simple_selectors.push_back(std::move(ss));
      continue;
    }

    if (t.type == CssTokenType::kDelim && t.value == ".") {
      tokenizer_.Next();
      CssToken name = tokenizer_.Next();
      if (name.type == CssTokenType::kIdent) {
        SimpleSelector ss;
        ss.type = SimpleSelectorType::kClass;
        ss.value = InternedString::Intern(name.value);
        cs.simple_selectors.push_back(std::move(ss));
      }
      continue;
    }

    if (t.type == CssTokenType::kHash) {
      tokenizer_.Next();
      SimpleSelector ss;
      ss.type = SimpleSelectorType::kId;
      ss.value = InternedString::Intern(t.value);
      cs.simple_selectors.push_back(std::move(ss));
      continue;
    }

    if (t.type == CssTokenType::kDelim && t.value == "*") {
      tokenizer_.Next();
      SimpleSelector ss;
      ss.type = SimpleSelectorType::kUniversal;
      cs.simple_selectors.push_back(std::move(ss));
      continue;
    }

    if (t.type == CssTokenType::kLeftBracket) {
      tokenizer_.Next();
      SimpleSelector ss;
      ss.type = SimpleSelectorType::kAttribute;
      CssToken attr = NextNonWS();
      if (attr.type == CssTokenType::kIdent) {
        ss.value = InternedString::Intern(attr.value);
      }
      CssToken next = NextNonWS();
      if (next.type == CssTokenType::kDelim && next.value == "=") {
        CssToken val = NextNonWS();
        if (val.type == CssTokenType::kIdent || val.type == CssTokenType::kString) {
          ss.attr_value = InternedString::Intern(val.value);
        }
        NextNonWS();
      }
      cs.simple_selectors.push_back(std::move(ss));
      continue;
    }

    if (t.type == CssTokenType::kColon) {
      tokenizer_.Next();
      CssToken name = tokenizer_.Next();
      if (name.type == CssTokenType::kIdent) {
        SimpleSelector ss;
        ss.type = SimpleSelectorType::kPseudoClass;
        ss.value = InternedString::Intern(name.value);
        cs.simple_selectors.push_back(std::move(ss));
      }
      continue;
    }

    break;
  }

  return cs;
}

SmallVector<Declaration, 8> CssParser::DoParseDeclarations() {
  SmallVector<Declaration, 8> decls;
  for (;;) {
    SkipWhitespace();
    CssToken t = tokenizer_.Peek();
    if (t.type == CssTokenType::kRightBrace || t.type == CssTokenType::kEof) {
      break;
    }
    if (!ParseDeclaration(decls)) {
      for (;;) {
        CssToken skip = tokenizer_.Peek();
        if (skip.type == CssTokenType::kSemicolon) {
          tokenizer_.Next();
          break;
        }
        if (skip.type == CssTokenType::kRightBrace ||
            skip.type == CssTokenType::kEof) {
          break;
        }
        tokenizer_.Next();
      }
    }
  }
  return decls;
}

bool CssParser::ParseDeclaration(SmallVector<Declaration, 8>& out) {
  CssToken name_tok = NextNonWS();
  if (name_tok.type != CssTokenType::kIdent) return false;

  StringView name = name_tok.value;
  PropertyId prop = PropertyIdFromName(name);

  if (prop == PropertyId::kUnknown) {
    if (EqualsIgnoreCase(name, "margin") || EqualsIgnoreCase(name, "padding")) {
      CssToken colon = NextNonWS();
      if (colon.type != CssTokenType::kColon) return false;

      SmallVector<CssValue, 4> values;
      for (usize i = 0; i < 4; ++i) {
        SkipWhitespace();
        CssToken peek = tokenizer_.Peek();
        if (peek.type == CssTokenType::kSemicolon ||
            peek.type == CssTokenType::kRightBrace ||
            peek.type == CssTokenType::kEof) {
          break;
        }
        if (peek.type == CssTokenType::kDelim && peek.value == "!") break;
        CssValue v = ParseLengthOrPercent();
        if (v.type == ValueType::kNone) break;
        values.push_back(v);
      }

      bool important = false;
      SkipWhitespace();
      CssToken bang = tokenizer_.Peek();
      if (bang.type == CssTokenType::kDelim && bang.value == "!") {
        tokenizer_.Next();
        CssToken imp = NextNonWS();
        if (imp.type == CssTokenType::kIdent &&
            EqualsIgnoreCase(imp.value, "important")) {
          important = true;
        }
      }

      SkipWhitespace();
      if (tokenizer_.Peek().type == CssTokenType::kSemicolon) {
        tokenizer_.Next();
      }

      if (values.empty()) return false;

      CssValue top, right, bottom, left;
      if (values.size() == 1) {
        top = right = bottom = left = values[0];
      } else if (values.size() == 2) {
        top = bottom = values[0];
        right = left = values[1];
      } else if (values.size() == 3) {
        top = values[0];
        right = left = values[1];
        bottom = values[2];
      } else {
        top = values[0];
        right = values[1];
        bottom = values[2];
        left = values[3];
      }

      bool is_margin = EqualsIgnoreCase(name, "margin");
      out.push_back({is_margin ? PropertyId::kMarginTop : PropertyId::kPaddingTop, top, important});
      out.push_back({is_margin ? PropertyId::kMarginRight : PropertyId::kPaddingRight, right, important});
      out.push_back({is_margin ? PropertyId::kMarginBottom : PropertyId::kPaddingBottom, bottom, important});
      out.push_back({is_margin ? PropertyId::kMarginLeft : PropertyId::kPaddingLeft, left, important});
      return true;
    }

    if (EqualsIgnoreCase(name, "border")) {
      CssToken colon = NextNonWS();
      if (colon.type != CssTokenType::kColon) return false;

      CssValue width = CssValue::None();
      CssValue style = CssValue::None();
      CssValue color = CssValue::None();

      for (int i = 0; i < 3; ++i) {
        SkipWhitespace();
        CssToken peek = tokenizer_.Peek();
        if (peek.type == CssTokenType::kSemicolon ||
            peek.type == CssTokenType::kRightBrace ||
            peek.type == CssTokenType::kEof) {
          break;
        }
        if (peek.type == CssTokenType::kDelim && peek.value == "!") break;

        if (peek.type == CssTokenType::kDimension ||
            peek.type == CssTokenType::kNumber ||
            peek.type == CssTokenType::kPercentage) {
          width = ParseLengthOrPercent();
        } else if (peek.type == CssTokenType::kHash) {
          color = ParseColor();
        } else if (peek.type == CssTokenType::kFunction) {
          color = ParseColor();
        } else if (peek.type == CssTokenType::kIdent) {
          StringView val = peek.value;
          if (EqualsIgnoreCase(val, "solid") || EqualsIgnoreCase(val, "dashed") ||
              EqualsIgnoreCase(val, "dotted") || EqualsIgnoreCase(val, "none")) {
            tokenizer_.Next();
            BorderStyle bs = BorderStyle::kNone;
            if (EqualsIgnoreCase(val, "solid")) bs = BorderStyle::kSolid;
            else if (EqualsIgnoreCase(val, "dashed")) bs = BorderStyle::kDashed;
            else if (EqualsIgnoreCase(val, "dotted")) bs = BorderStyle::kDotted;
            style = CssValue::Enum(static_cast<u16>(bs));
          } else {
            color = ParseColor();
          }
        } else {
          break;
        }
      }

      bool important = false;
      SkipWhitespace();
      CssToken bang = tokenizer_.Peek();
      if (bang.type == CssTokenType::kDelim && bang.value == "!") {
        tokenizer_.Next();
        CssToken imp = NextNonWS();
        if (imp.type == CssTokenType::kIdent &&
            EqualsIgnoreCase(imp.value, "important")) {
          important = true;
        }
      }

      SkipWhitespace();
      if (tokenizer_.Peek().type == CssTokenType::kSemicolon) {
        tokenizer_.Next();
      }

      if (width.type != ValueType::kNone) {
        out.push_back({PropertyId::kBorderTopWidth, width, important});
        out.push_back({PropertyId::kBorderRightWidth, width, important});
        out.push_back({PropertyId::kBorderBottomWidth, width, important});
        out.push_back({PropertyId::kBorderLeftWidth, width, important});
      }
      if (style.type != ValueType::kNone) {
        out.push_back({PropertyId::kBorderTopStyle, style, important});
        out.push_back({PropertyId::kBorderRightStyle, style, important});
        out.push_back({PropertyId::kBorderBottomStyle, style, important});
        out.push_back({PropertyId::kBorderLeftStyle, style, important});
      }
      if (color.type != ValueType::kNone) {
        out.push_back({PropertyId::kBorderTopColor, color, important});
        out.push_back({PropertyId::kBorderRightColor, color, important});
        out.push_back({PropertyId::kBorderBottomColor, color, important});
        out.push_back({PropertyId::kBorderLeftColor, color, important});
      }
      return true;
    }

    if (EqualsIgnoreCase(name, "flex")) {
      CssToken colon = NextNonWS();
      if (colon.type != CssTokenType::kColon) return false;

      CssToken t1 = NextNonWS();
      if (t1.type != CssTokenType::kNumber) return false;
      CssValue grow = CssValue::Number(t1.number);

      bool important = false;
      CssValue shrink = CssValue::None();
      CssValue basis = CssValue::None();

      SkipWhitespace();
      CssToken peek = tokenizer_.Peek();
      if (peek.type == CssTokenType::kNumber) {
        tokenizer_.Next();
        shrink = CssValue::Number(peek.number);

        SkipWhitespace();
        peek = tokenizer_.Peek();
        if (peek.type == CssTokenType::kDimension ||
            peek.type == CssTokenType::kPercentage ||
            (peek.type == CssTokenType::kNumber && peek.number == 0.0f)) {
          basis = ParseLengthOrPercent();
        }
      }

      SkipWhitespace();
      CssToken bang = tokenizer_.Peek();
      if (bang.type == CssTokenType::kDelim && bang.value == "!") {
        tokenizer_.Next();
        CssToken imp = NextNonWS();
        if (imp.type == CssTokenType::kIdent &&
            EqualsIgnoreCase(imp.value, "important")) {
          important = true;
        }
      }

      SkipWhitespace();
      if (tokenizer_.Peek().type == CssTokenType::kSemicolon) {
        tokenizer_.Next();
      }

      out.push_back({PropertyId::kFlexGrow, grow, important});
      if (shrink.type != ValueType::kNone) {
        out.push_back({PropertyId::kFlexShrink, shrink, important});
      }
      if (basis.type != ValueType::kNone) {
        out.push_back({PropertyId::kFlexBasis, basis, important});
      }
      return true;
    }

    if (EqualsIgnoreCase(name, "transition")) {
      CssToken colon = NextNonWS();
      if (colon.type != CssTokenType::kColon) return false;

      SkipWhitespace();
      CssToken peek = tokenizer_.Peek();
      if (peek.type == CssTokenType::kIdent &&
          EqualsIgnoreCase(peek.value, "none")) {
        tokenizer_.Next();
        SkipWhitespace();
        if (tokenizer_.Peek().type == CssTokenType::kSemicolon)
          tokenizer_.Next();
        return true;
      }

      CssValue prop_val = CssValue::None();
      CssValue dur_val = CssValue::None();
      CssValue timing_val = CssValue::Enum(
          static_cast<u16>(css::TimingFunction::kEase));
      CssValue delay_val = CssValue::Number(0.0f);

      peek = tokenizer_.Peek();
      if (peek.type == CssTokenType::kIdent) {
        StringView pname = peek.value;
        tokenizer_.Next();
        if (EqualsIgnoreCase(pname, "all")) {
          prop_val = CssValue::Enum(static_cast<u16>(PropertyId::kUnknown));
        } else {
          PropertyId pid = PropertyIdFromName(pname);
          prop_val = CssValue::Enum(static_cast<u16>(pid));
        }
      }

      SkipWhitespace();
      peek = tokenizer_.Peek();
      bool first_time = true;
      while (peek.type == CssTokenType::kDimension ||
             peek.type == CssTokenType::kNumber) {
        f32 val = peek.number;
        f32 ms = val;
        if (peek.type == CssTokenType::kDimension) {
          if (peek.unit == "s" || peek.unit == "S")
            ms = val * 1000.0f;
        }
        tokenizer_.Next();
        if (first_time) {
          dur_val = CssValue::Number(ms);
          first_time = false;
        } else {
          delay_val = CssValue::Number(ms);
          break;
        }

        SkipWhitespace();
        peek = tokenizer_.Peek();
        if (peek.type == CssTokenType::kIdent) {
          StringView tf = peek.value;
          if (EqualsIgnoreCase(tf, "linear")) {
            timing_val = CssValue::Enum(
                static_cast<u16>(css::TimingFunction::kLinear));
            tokenizer_.Next();
          } else if (EqualsIgnoreCase(tf, "ease-in-out")) {
            timing_val = CssValue::Enum(
                static_cast<u16>(css::TimingFunction::kEaseInOut));
            tokenizer_.Next();
          } else if (EqualsIgnoreCase(tf, "ease-in")) {
            timing_val = CssValue::Enum(
                static_cast<u16>(css::TimingFunction::kEaseIn));
            tokenizer_.Next();
          } else if (EqualsIgnoreCase(tf, "ease-out")) {
            timing_val = CssValue::Enum(
                static_cast<u16>(css::TimingFunction::kEaseOut));
            tokenizer_.Next();
          } else if (EqualsIgnoreCase(tf, "ease")) {
            timing_val = CssValue::Enum(
                static_cast<u16>(css::TimingFunction::kEase));
            tokenizer_.Next();
          }
          SkipWhitespace();
          peek = tokenizer_.Peek();
        }
      }

      bool important = false;
      SkipWhitespace();
      CssToken bang = tokenizer_.Peek();
      if (bang.type == CssTokenType::kDelim && bang.value == "!") {
        tokenizer_.Next();
        CssToken imp = NextNonWS();
        if (imp.type == CssTokenType::kIdent &&
            EqualsIgnoreCase(imp.value, "important")) {
          important = true;
        }
      }

      SkipWhitespace();
      if (tokenizer_.Peek().type == CssTokenType::kSemicolon)
        tokenizer_.Next();

      if (prop_val.type != ValueType::kNone)
        out.push_back({PropertyId::kTransitionProperty, prop_val, important});
      if (dur_val.type != ValueType::kNone)
        out.push_back({PropertyId::kTransitionDuration, dur_val, important});
      out.push_back({PropertyId::kTransitionTimingFunction, timing_val, important});
      out.push_back({PropertyId::kTransitionDelay, delay_val, important});
      return true;
    }

    return false;
  }

  CssToken colon = NextNonWS();
  if (colon.type != CssTokenType::kColon) return false;

  SkipWhitespace();
  CssToken val_peek = tokenizer_.Peek();
  if (val_peek.type == CssTokenType::kIdent) {
    if (EqualsIgnoreCase(val_peek.value, "auto")) {
      tokenizer_.Next();
      Declaration decl;
      decl.property = prop;
      decl.value = CssValue::Auto();

      SkipWhitespace();
      CssToken bang = tokenizer_.Peek();
      if (bang.type == CssTokenType::kDelim && bang.value == "!") {
        tokenizer_.Next();
        CssToken imp = NextNonWS();
        if (imp.type == CssTokenType::kIdent &&
            EqualsIgnoreCase(imp.value, "important")) {
          decl.important = true;
        }
      }

      SkipWhitespace();
      if (tokenizer_.Peek().type == CssTokenType::kSemicolon) {
        tokenizer_.Next();
      }
      out.push_back(decl);
      return true;
    }
    if (EqualsIgnoreCase(val_peek.value, "inherit")) {
      tokenizer_.Next();
      Declaration decl;
      decl.property = prop;
      decl.value = CssValue::Inherit();

      SkipWhitespace();
      CssToken bang = tokenizer_.Peek();
      if (bang.type == CssTokenType::kDelim && bang.value == "!") {
        tokenizer_.Next();
        CssToken imp = NextNonWS();
        if (imp.type == CssTokenType::kIdent &&
            EqualsIgnoreCase(imp.value, "important")) {
          decl.important = true;
        }
      }

      SkipWhitespace();
      if (tokenizer_.Peek().type == CssTokenType::kSemicolon) {
        tokenizer_.Next();
      }
      out.push_back(decl);
      return true;
    }
    if (EqualsIgnoreCase(val_peek.value, "initial")) {
      tokenizer_.Next();
      Declaration decl;
      decl.property = prop;
      decl.value = CssValue::Initial();

      SkipWhitespace();
      CssToken bang = tokenizer_.Peek();
      if (bang.type == CssTokenType::kDelim && bang.value == "!") {
        tokenizer_.Next();
        CssToken imp = NextNonWS();
        if (imp.type == CssTokenType::kIdent &&
            EqualsIgnoreCase(imp.value, "important")) {
          decl.important = true;
        }
      }

      SkipWhitespace();
      if (tokenizer_.Peek().type == CssTokenType::kSemicolon) {
        tokenizer_.Next();
      }
      out.push_back(decl);
      return true;
    }
  }

  CssValue value = ParseValue(prop);
  if (value.type == ValueType::kNone) return false;

  Declaration decl;
  decl.property = prop;
  decl.value = value;

  SkipWhitespace();
  CssToken bang = tokenizer_.Peek();
  if (bang.type == CssTokenType::kDelim && bang.value == "!") {
    tokenizer_.Next();
    CssToken imp = NextNonWS();
    if (imp.type == CssTokenType::kIdent &&
        EqualsIgnoreCase(imp.value, "important")) {
      decl.important = true;
    }
  }

  SkipWhitespace();
  if (tokenizer_.Peek().type == CssTokenType::kSemicolon) {
    tokenizer_.Next();
  }

  out.push_back(decl);
  return true;
}

CssValue CssParser::ParseValue(PropertyId prop) {
  // vertical-align 是混合类型（CSS 2.1 §10.8.1 6 关键字 + length/percent）。
  // 先 peek 当前 token：ident → ParseEnumValue；dimension/percentage/0 → length。
  if (prop == PropertyId::kVerticalAlign) {
    SkipWhitespace();
    auto t = tokenizer_.Peek();
    if (t.type == CssTokenType::kIdent) return ParseEnumValue(prop);
    return ParseLengthOrPercent();
  }
  if (IsLengthProperty(prop)) return ParseLengthOrPercent();
  if (IsColorProperty(prop)) return ParseColor();
  if (IsEnumProperty(prop)) return ParseEnumValue(prop);

  if (IsNumberProperty(prop)) {
    CssToken t = NextNonWS();
    if (t.type == CssTokenType::kNumber) return CssValue::Number(t.number);
    return CssValue::None();
  }

  if (prop == PropertyId::kFontFamily) {
    CssToken t = NextNonWS();
    if (t.type == CssTokenType::kString || t.type == CssTokenType::kIdent) {
      return CssValue::Enum(0);
    }
    return CssValue::None();
  }

  return CssValue::None();
}

CssValue CssParser::ParseLengthOrPercent() {
  CssToken t = NextNonWS();
  if (t.type == CssTokenType::kDimension) {
    Unit u = Unit::kPx;
    if (t.unit == "px")
      u = Unit::kPx;
    else if (t.unit == "em")
      u = Unit::kEm;
    else if (t.unit == "rem")
      u = Unit::kRem;
    else if (t.unit == "vw")
      u = Unit::kVw;
    else if (t.unit == "vh")
      u = Unit::kVh;
    return CssValue::Length(t.number, u);
  }
  if (t.type == CssTokenType::kPercentage) {
    return CssValue::Length(t.number, Unit::kPercent);
  }
  if (t.type == CssTokenType::kNumber && t.number == 0.0f) {
    return CssValue::Length(0.0f, Unit::kPx);
  }
  return CssValue::None();
}

CssValue CssParser::ParseColor() {
  CssToken t = NextNonWS();

  if (t.type == CssTokenType::kHash) {
    StringView hex = t.value;
    if (hex.size() == 3) {
      u8 r = HexDigit(hex[0]);
      u8 g = HexDigit(hex[1]);
      u8 b = HexDigit(hex[2]);
      r = static_cast<u8>((r << 4) | r);
      g = static_cast<u8>((g << 4) | g);
      b = static_cast<u8>((b << 4) | b);
      u32 rgba = (static_cast<u32>(r) << 24) | (static_cast<u32>(g) << 16) |
                 (static_cast<u32>(b) << 8) | 0xFF;
      return CssValue::Color(rgba);
    }
    if (hex.size() == 6) {
      u8 r = static_cast<u8>((HexDigit(hex[0]) << 4) | HexDigit(hex[1]));
      u8 g = static_cast<u8>((HexDigit(hex[2]) << 4) | HexDigit(hex[3]));
      u8 b = static_cast<u8>((HexDigit(hex[4]) << 4) | HexDigit(hex[5]));
      u32 rgba = (static_cast<u32>(r) << 24) | (static_cast<u32>(g) << 16) |
                 (static_cast<u32>(b) << 8) | 0xFF;
      return CssValue::Color(rgba);
    }
    if (hex.size() == 8) {
      u8 r = static_cast<u8>((HexDigit(hex[0]) << 4) | HexDigit(hex[1]));
      u8 g = static_cast<u8>((HexDigit(hex[2]) << 4) | HexDigit(hex[3]));
      u8 b = static_cast<u8>((HexDigit(hex[4]) << 4) | HexDigit(hex[5]));
      u8 a = static_cast<u8>((HexDigit(hex[6]) << 4) | HexDigit(hex[7]));
      u32 rgba = (static_cast<u32>(r) << 24) | (static_cast<u32>(g) << 16) |
                 (static_cast<u32>(b) << 8) | static_cast<u32>(a);
      return CssValue::Color(rgba);
    }
    return CssValue::None();
  }

  if (t.type == CssTokenType::kFunction) {
    bool has_alpha = EqualsIgnoreCase(t.value, "rgba");

    CssToken r_tok = NextNonWS();
    u8 r = static_cast<u8>(r_tok.number);

    NextNonWS();  // comma
    CssToken g_tok = NextNonWS();
    u8 g = static_cast<u8>(g_tok.number);

    NextNonWS();  // comma
    CssToken b_tok = NextNonWS();
    u8 b = static_cast<u8>(b_tok.number);

    u8 a = 255;
    if (has_alpha) {
      NextNonWS();  // comma
      CssToken a_tok = NextNonWS();
      a = static_cast<u8>(a_tok.number * 255.0f);
    }

    SkipWhitespace();
    if (tokenizer_.Peek().type == CssTokenType::kRightParen) {
      tokenizer_.Next();
    }

    u32 rgba = (static_cast<u32>(r) << 24) | (static_cast<u32>(g) << 16) |
               (static_cast<u32>(b) << 8) | static_cast<u32>(a);
    return CssValue::Color(rgba);
  }

  if (t.type == CssTokenType::kIdent) {
    return LookupNamedColor(t.value);
  }

  return CssValue::None();
}

CssValue CssParser::ParseEnumValue(PropertyId prop) {
  CssToken t = NextNonWS();
  if (t.type != CssTokenType::kIdent) return CssValue::None();

  StringView val = t.value;

  switch (prop) {
    case PropertyId::kDisplay:
      if (EqualsIgnoreCase(val, "none"))
        return CssValue::Enum(static_cast<u16>(Display::kNone));
      if (EqualsIgnoreCase(val, "block"))
        return CssValue::Enum(static_cast<u16>(Display::kBlock));
      if (EqualsIgnoreCase(val, "inline"))
        return CssValue::Enum(static_cast<u16>(Display::kInline));
      if (EqualsIgnoreCase(val, "inline-block"))
        return CssValue::Enum(static_cast<u16>(Display::kInlineBlock));
      if (EqualsIgnoreCase(val, "flex"))
        return CssValue::Enum(static_cast<u16>(Display::kFlex));
      break;

    case PropertyId::kPosition:
      if (EqualsIgnoreCase(val, "static"))
        return CssValue::Enum(static_cast<u16>(Position::kStatic));
      if (EqualsIgnoreCase(val, "relative"))
        return CssValue::Enum(static_cast<u16>(Position::kRelative));
      if (EqualsIgnoreCase(val, "absolute"))
        return CssValue::Enum(static_cast<u16>(Position::kAbsolute));
      if (EqualsIgnoreCase(val, "fixed"))
        return CssValue::Enum(static_cast<u16>(Position::kFixed));
      break;

    case PropertyId::kBoxSizing:
      if (EqualsIgnoreCase(val, "content-box"))
        return CssValue::Enum(static_cast<u16>(BoxSizing::kContentBox));
      if (EqualsIgnoreCase(val, "border-box"))
        return CssValue::Enum(static_cast<u16>(BoxSizing::kBorderBox));
      break;

    case PropertyId::kOverflow:
      if (EqualsIgnoreCase(val, "visible"))
        return CssValue::Enum(static_cast<u16>(Overflow::kVisible));
      if (EqualsIgnoreCase(val, "hidden"))
        return CssValue::Enum(static_cast<u16>(Overflow::kHidden));
      if (EqualsIgnoreCase(val, "scroll"))
        return CssValue::Enum(static_cast<u16>(Overflow::kScroll));
      if (EqualsIgnoreCase(val, "auto"))
        return CssValue::Enum(static_cast<u16>(Overflow::kAuto));
      break;

    case PropertyId::kVisibility:
      if (EqualsIgnoreCase(val, "visible"))
        return CssValue::Enum(static_cast<u16>(Visibility::kVisible));
      if (EqualsIgnoreCase(val, "hidden"))
        return CssValue::Enum(static_cast<u16>(Visibility::kHidden));
      break;

    case PropertyId::kFlexDirection:
      if (EqualsIgnoreCase(val, "row"))
        return CssValue::Enum(static_cast<u16>(FlexDirection::kRow));
      if (EqualsIgnoreCase(val, "row-reverse"))
        return CssValue::Enum(static_cast<u16>(FlexDirection::kRowReverse));
      if (EqualsIgnoreCase(val, "column"))
        return CssValue::Enum(static_cast<u16>(FlexDirection::kColumn));
      if (EqualsIgnoreCase(val, "column-reverse"))
        return CssValue::Enum(static_cast<u16>(FlexDirection::kColumnReverse));
      break;

    case PropertyId::kFlexWrap:
      if (EqualsIgnoreCase(val, "nowrap"))
        return CssValue::Enum(static_cast<u16>(FlexWrap::kNowrap));
      if (EqualsIgnoreCase(val, "wrap"))
        return CssValue::Enum(static_cast<u16>(FlexWrap::kWrap));
      break;

    case PropertyId::kJustifyContent:
      if (EqualsIgnoreCase(val, "flex-start"))
        return CssValue::Enum(static_cast<u16>(JustifyContent::kFlexStart));
      if (EqualsIgnoreCase(val, "flex-end"))
        return CssValue::Enum(static_cast<u16>(JustifyContent::kFlexEnd));
      if (EqualsIgnoreCase(val, "center"))
        return CssValue::Enum(static_cast<u16>(JustifyContent::kCenter));
      if (EqualsIgnoreCase(val, "space-between"))
        return CssValue::Enum(static_cast<u16>(JustifyContent::kSpaceBetween));
      if (EqualsIgnoreCase(val, "space-around"))
        return CssValue::Enum(static_cast<u16>(JustifyContent::kSpaceAround));
      break;

    case PropertyId::kAlignItems:
    case PropertyId::kAlignSelf:
      if (EqualsIgnoreCase(val, "auto"))
        return CssValue::Enum(static_cast<u16>(AlignItems::kAuto));
      if (EqualsIgnoreCase(val, "flex-start"))
        return CssValue::Enum(static_cast<u16>(AlignItems::kFlexStart));
      if (EqualsIgnoreCase(val, "flex-end"))
        return CssValue::Enum(static_cast<u16>(AlignItems::kFlexEnd));
      if (EqualsIgnoreCase(val, "center"))
        return CssValue::Enum(static_cast<u16>(AlignItems::kCenter));
      if (EqualsIgnoreCase(val, "stretch"))
        return CssValue::Enum(static_cast<u16>(AlignItems::kStretch));
      if (EqualsIgnoreCase(val, "baseline"))
        return CssValue::Enum(static_cast<u16>(AlignItems::kBaseline));
      break;

    case PropertyId::kTextAlign:
      if (EqualsIgnoreCase(val, "left"))
        return CssValue::Enum(static_cast<u16>(TextAlign::kLeft));
      if (EqualsIgnoreCase(val, "right"))
        return CssValue::Enum(static_cast<u16>(TextAlign::kRight));
      if (EqualsIgnoreCase(val, "center"))
        return CssValue::Enum(static_cast<u16>(TextAlign::kCenter));
      if (EqualsIgnoreCase(val, "justify"))
        return CssValue::Enum(static_cast<u16>(TextAlign::kJustify));
      break;

    case PropertyId::kWhiteSpace:
      if (EqualsIgnoreCase(val, "normal"))
        return CssValue::Enum(static_cast<u16>(WhiteSpace::kNormal));
      if (EqualsIgnoreCase(val, "nowrap"))
        return CssValue::Enum(static_cast<u16>(WhiteSpace::kNowrap));
      if (EqualsIgnoreCase(val, "pre"))
        return CssValue::Enum(static_cast<u16>(WhiteSpace::kPre));
      if (EqualsIgnoreCase(val, "pre-wrap"))
        return CssValue::Enum(static_cast<u16>(WhiteSpace::kPreWrap));
      break;

    case PropertyId::kBorderTopStyle:
    case PropertyId::kBorderRightStyle:
    case PropertyId::kBorderBottomStyle:
    case PropertyId::kBorderLeftStyle:
      if (EqualsIgnoreCase(val, "none"))
        return CssValue::Enum(static_cast<u16>(BorderStyle::kNone));
      if (EqualsIgnoreCase(val, "solid"))
        return CssValue::Enum(static_cast<u16>(BorderStyle::kSolid));
      if (EqualsIgnoreCase(val, "dashed"))
        return CssValue::Enum(static_cast<u16>(BorderStyle::kDashed));
      if (EqualsIgnoreCase(val, "dotted"))
        return CssValue::Enum(static_cast<u16>(BorderStyle::kDotted));
      break;

    case PropertyId::kFontStyle:
      if (EqualsIgnoreCase(val, "normal"))
        return CssValue::Enum(static_cast<u16>(CssFontStyle::kNormal));
      if (EqualsIgnoreCase(val, "italic"))
        return CssValue::Enum(static_cast<u16>(CssFontStyle::kItalic));
      break;

    case PropertyId::kVerticalAlign:
      // CSS 2.1 §10.8.1 — 8 关键字（kLength sentinel 仅 length 路径使用）。
      if (EqualsIgnoreCase(val, "baseline"))
        return CssValue::Enum(static_cast<u16>(VerticalAlign::kBaseline));
      if (EqualsIgnoreCase(val, "sub"))
        return CssValue::Enum(static_cast<u16>(VerticalAlign::kSub));
      if (EqualsIgnoreCase(val, "super"))
        return CssValue::Enum(static_cast<u16>(VerticalAlign::kSuper));
      if (EqualsIgnoreCase(val, "middle"))
        return CssValue::Enum(static_cast<u16>(VerticalAlign::kMiddle));
      if (EqualsIgnoreCase(val, "text-top"))
        return CssValue::Enum(static_cast<u16>(VerticalAlign::kTextTop));
      if (EqualsIgnoreCase(val, "text-bottom"))
        return CssValue::Enum(static_cast<u16>(VerticalAlign::kTextBottom));
      if (EqualsIgnoreCase(val, "top"))
        return CssValue::Enum(static_cast<u16>(VerticalAlign::kTop));
      if (EqualsIgnoreCase(val, "bottom"))
        return CssValue::Enum(static_cast<u16>(VerticalAlign::kBottom));
      break;

    default:
      break;
  }

  return CssValue::None();
}

}  // namespace vx::css
