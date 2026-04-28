#include "veloxa/core/css/parser.h"

#include <gtest/gtest.h>

#include "veloxa/core/css/enums.h"

namespace vx::css {
namespace {

// --- Selector parsing ---

TEST(CssParserTest, TypeSelector) {
  auto sheet = CssParser::Parse("div { }");
  ASSERT_EQ(sheet.rules.size(), 1u);
  ASSERT_EQ(sheet.rules[0].selectors.size(), 1u);
  auto& sel = sheet.rules[0].selectors[0];
  ASSERT_EQ(sel.compounds.size(), 1u);
  ASSERT_EQ(sel.compounds[0].simple_selectors.size(), 1u);
  EXPECT_EQ(sel.compounds[0].simple_selectors[0].type, SimpleSelectorType::kTag);
  EXPECT_EQ(sel.compounds[0].simple_selectors[0].value.view(), StringView("div"));
}

TEST(CssParserTest, ClassSelector) {
  auto sheet = CssParser::Parse(".foo { }");
  ASSERT_EQ(sheet.rules.size(), 1u);
  auto& sel = sheet.rules[0].selectors[0];
  ASSERT_EQ(sel.compounds.size(), 1u);
  ASSERT_EQ(sel.compounds[0].simple_selectors.size(), 1u);
  EXPECT_EQ(sel.compounds[0].simple_selectors[0].type, SimpleSelectorType::kClass);
  EXPECT_EQ(sel.compounds[0].simple_selectors[0].value.view(), StringView("foo"));
}

TEST(CssParserTest, IdSelector) {
  auto sheet = CssParser::Parse("#bar { }");
  ASSERT_EQ(sheet.rules.size(), 1u);
  auto& sel = sheet.rules[0].selectors[0];
  ASSERT_EQ(sel.compounds.size(), 1u);
  ASSERT_EQ(sel.compounds[0].simple_selectors.size(), 1u);
  EXPECT_EQ(sel.compounds[0].simple_selectors[0].type, SimpleSelectorType::kId);
  EXPECT_EQ(sel.compounds[0].simple_selectors[0].value.view(), StringView("bar"));
}

TEST(CssParserTest, UniversalSelector) {
  auto sheet = CssParser::Parse("* { }");
  ASSERT_EQ(sheet.rules.size(), 1u);
  auto& sel = sheet.rules[0].selectors[0];
  ASSERT_EQ(sel.compounds.size(), 1u);
  ASSERT_EQ(sel.compounds[0].simple_selectors.size(), 1u);
  EXPECT_EQ(sel.compounds[0].simple_selectors[0].type, SimpleSelectorType::kUniversal);
}

TEST(CssParserTest, CompoundSelector) {
  auto sheet = CssParser::Parse("div.foo#bar { }");
  ASSERT_EQ(sheet.rules.size(), 1u);
  auto& sel = sheet.rules[0].selectors[0];
  ASSERT_EQ(sel.compounds.size(), 1u);
  ASSERT_EQ(sel.compounds[0].simple_selectors.size(), 3u);
  EXPECT_EQ(sel.compounds[0].simple_selectors[0].type, SimpleSelectorType::kTag);
  EXPECT_EQ(sel.compounds[0].simple_selectors[1].type, SimpleSelectorType::kClass);
  EXPECT_EQ(sel.compounds[0].simple_selectors[2].type, SimpleSelectorType::kId);
}

TEST(CssParserTest, DescendantCombinator) {
  auto sheet = CssParser::Parse("div p { }");
  ASSERT_EQ(sheet.rules.size(), 1u);
  auto& sel = sheet.rules[0].selectors[0];
  ASSERT_EQ(sel.compounds.size(), 2u);
  EXPECT_EQ(sel.compounds[0].simple_selectors[0].type, SimpleSelectorType::kTag);
  EXPECT_EQ(sel.compounds[0].simple_selectors[0].value.view(), StringView("p"));
  EXPECT_EQ(sel.compounds[1].combinator, Combinator::kDescendant);
}

TEST(CssParserTest, ChildCombinator) {
  auto sheet = CssParser::Parse("div > p { }");
  ASSERT_EQ(sheet.rules.size(), 1u);
  auto& sel = sheet.rules[0].selectors[0];
  ASSERT_EQ(sel.compounds.size(), 2u);
  EXPECT_EQ(sel.compounds[0].simple_selectors[0].value.view(), StringView("p"));
  EXPECT_EQ(sel.compounds[1].combinator, Combinator::kChild);
}

TEST(CssParserTest, SelectorList) {
  auto sheet = CssParser::Parse("div, .foo { }");
  ASSERT_EQ(sheet.rules.size(), 1u);
  EXPECT_EQ(sheet.rules[0].selectors.size(), 2u);
}

TEST(CssParserTest, AttributeSelector) {
  auto sheet = CssParser::Parse("[disabled] { }");
  ASSERT_EQ(sheet.rules.size(), 1u);
  auto& sel = sheet.rules[0].selectors[0];
  ASSERT_EQ(sel.compounds.size(), 1u);
  ASSERT_EQ(sel.compounds[0].simple_selectors.size(), 1u);
  EXPECT_EQ(sel.compounds[0].simple_selectors[0].type, SimpleSelectorType::kAttribute);
  EXPECT_EQ(sel.compounds[0].simple_selectors[0].value.view(), StringView("disabled"));
}

TEST(CssParserTest, AttributeValueSelector) {
  auto sheet = CssParser::Parse("[type=text] { }");
  ASSERT_EQ(sheet.rules.size(), 1u);
  auto& ss = sheet.rules[0].selectors[0].compounds[0].simple_selectors[0];
  EXPECT_EQ(ss.type, SimpleSelectorType::kAttribute);
  EXPECT_EQ(ss.value.view(), StringView("type"));
  EXPECT_EQ(ss.attr_value.view(), StringView("text"));
}

TEST(CssParserTest, PseudoClass) {
  auto sheet = CssParser::Parse(":hover { }");
  ASSERT_EQ(sheet.rules.size(), 1u);
  auto& ss = sheet.rules[0].selectors[0].compounds[0].simple_selectors[0];
  EXPECT_EQ(ss.type, SimpleSelectorType::kPseudoClass);
  EXPECT_EQ(ss.value.view(), StringView("hover"));
}

// --- Specificity ---

TEST(CssParserTest, Specificity_Tag) {
  auto sheet = CssParser::Parse("div { }");
  EXPECT_EQ(sheet.rules[0].selectors[0].specificity, 0x000001u);
}

TEST(CssParserTest, Specificity_Class) {
  auto sheet = CssParser::Parse(".foo { }");
  EXPECT_EQ(sheet.rules[0].selectors[0].specificity, 0x000100u);
}

TEST(CssParserTest, Specificity_Id) {
  auto sheet = CssParser::Parse("#bar { }");
  EXPECT_EQ(sheet.rules[0].selectors[0].specificity, 0x010000u);
}

TEST(CssParserTest, Specificity_Compound) {
  auto sheet = CssParser::Parse("div.foo#bar { }");
  EXPECT_EQ(sheet.rules[0].selectors[0].specificity, 0x010101u);
}

// --- Declaration parsing ---

TEST(CssParserTest, LengthPx) {
  auto sheet = CssParser::Parse("div { width: 100px; }");
  ASSERT_EQ(sheet.rules[0].declarations.size(), 1u);
  auto& d = sheet.rules[0].declarations[0];
  EXPECT_EQ(d.property, PropertyId::kWidth);
  EXPECT_EQ(d.value.type, ValueType::kLength);
  EXPECT_FLOAT_EQ(d.value.number, 100.0f);
  EXPECT_EQ(d.value.unit, Unit::kPx);
}

TEST(CssParserTest, LengthEm) {
  auto sheet = CssParser::Parse("div { font-size: 1.5em; }");
  ASSERT_EQ(sheet.rules[0].declarations.size(), 1u);
  auto& d = sheet.rules[0].declarations[0];
  EXPECT_EQ(d.property, PropertyId::kFontSize);
  EXPECT_EQ(d.value.type, ValueType::kLength);
  EXPECT_FLOAT_EQ(d.value.number, 1.5f);
  EXPECT_EQ(d.value.unit, Unit::kEm);
}

TEST(CssParserTest, Percentage) {
  auto sheet = CssParser::Parse("div { width: 50%; }");
  ASSERT_EQ(sheet.rules[0].declarations.size(), 1u);
  auto& d = sheet.rules[0].declarations[0];
  EXPECT_EQ(d.property, PropertyId::kWidth);
  EXPECT_EQ(d.value.type, ValueType::kLength);
  EXPECT_FLOAT_EQ(d.value.number, 50.0f);
  EXPECT_EQ(d.value.unit, Unit::kPercent);
}

TEST(CssParserTest, ColorHex6) {
  auto sheet = CssParser::Parse("div { color: #ff0000; }");
  ASSERT_EQ(sheet.rules[0].declarations.size(), 1u);
  auto& d = sheet.rules[0].declarations[0];
  EXPECT_EQ(d.property, PropertyId::kColor);
  EXPECT_EQ(d.value.type, ValueType::kColor);
  EXPECT_EQ(d.value.color, 0xFF0000FFu);
}

TEST(CssParserTest, ColorHex3) {
  auto sheet = CssParser::Parse("div { color: #f00; }");
  ASSERT_EQ(sheet.rules[0].declarations.size(), 1u);
  auto& d = sheet.rules[0].declarations[0];
  EXPECT_EQ(d.property, PropertyId::kColor);
  EXPECT_EQ(d.value.type, ValueType::kColor);
  EXPECT_EQ(d.value.color, 0xFF0000FFu);
}

TEST(CssParserTest, ColorNamed) {
  auto sheet = CssParser::Parse("div { color: red; }");
  ASSERT_EQ(sheet.rules[0].declarations.size(), 1u);
  auto& d = sheet.rules[0].declarations[0];
  EXPECT_EQ(d.property, PropertyId::kColor);
  EXPECT_EQ(d.value.type, ValueType::kColor);
  EXPECT_EQ(d.value.color, 0xFF0000FFu);
}

TEST(CssParserTest, ColorRgb) {
  auto sheet = CssParser::Parse("div { color: rgb(255, 0, 0); }");
  ASSERT_EQ(sheet.rules[0].declarations.size(), 1u);
  auto& d = sheet.rules[0].declarations[0];
  EXPECT_EQ(d.property, PropertyId::kColor);
  EXPECT_EQ(d.value.type, ValueType::kColor);
  EXPECT_EQ(d.value.color, 0xFF0000FFu);
}

TEST(CssParserTest, EnumDisplay) {
  auto sheet = CssParser::Parse("div { display: flex; }");
  ASSERT_EQ(sheet.rules[0].declarations.size(), 1u);
  auto& d = sheet.rules[0].declarations[0];
  EXPECT_EQ(d.property, PropertyId::kDisplay);
  EXPECT_EQ(d.value.type, ValueType::kEnum);
  EXPECT_EQ(d.value.enum_value, static_cast<u16>(Display::kFlex));
}

TEST(CssParserTest, EnumPosition) {
  auto sheet = CssParser::Parse("div { position: absolute; }");
  ASSERT_EQ(sheet.rules[0].declarations.size(), 1u);
  auto& d = sheet.rules[0].declarations[0];
  EXPECT_EQ(d.property, PropertyId::kPosition);
  EXPECT_EQ(d.value.type, ValueType::kEnum);
  EXPECT_EQ(d.value.enum_value, static_cast<u16>(Position::kAbsolute));
}

// R4 #21 vertical-align — TASK-20260426-01
// vertical-align 是混合类型属性（CSS 2.1 §10.8.1）：8 关键字走 enum 路径，
// length/percent 走 length 路径。下列 3 case + 1 RED 反向探针验证解析正确性。

TEST(CssParserTest, VerticalAlignBaseline) {
  auto sheet = CssParser::Parse("span { vertical-align: baseline; }");
  ASSERT_EQ(sheet.rules[0].declarations.size(), 1u);
  auto& d = sheet.rules[0].declarations[0];
  EXPECT_EQ(d.property, PropertyId::kVerticalAlign);
  EXPECT_EQ(d.value.type, ValueType::kEnum);
  EXPECT_EQ(d.value.enum_value,
            static_cast<u16>(VerticalAlign::kBaseline));
}

TEST(CssParserTest, VerticalAlignSubDistinctFromSuper) {
  // RED 反向探针：若 parser 把 sub 错算成 super，下面 EXPECT 会失败。
  auto sheet = CssParser::Parse("span { vertical-align: sub; }");
  ASSERT_EQ(sheet.rules[0].declarations.size(), 1u);
  auto& d = sheet.rules[0].declarations[0];
  EXPECT_EQ(d.property, PropertyId::kVerticalAlign);
  EXPECT_EQ(d.value.type, ValueType::kEnum);
  EXPECT_EQ(d.value.enum_value, static_cast<u16>(VerticalAlign::kSub));
  EXPECT_NE(d.value.enum_value, static_cast<u16>(VerticalAlign::kSuper));
}

TEST(CssParserTest, VerticalAlignLengthPercent) {
  // 50% 走 length 路径（unit = kPercent），enum 路径不应被触发。
  auto sheet = CssParser::Parse("span { vertical-align: 50%; }");
  ASSERT_EQ(sheet.rules[0].declarations.size(), 1u);
  auto& d = sheet.rules[0].declarations[0];
  EXPECT_EQ(d.property, PropertyId::kVerticalAlign);
  EXPECT_EQ(d.value.type, ValueType::kLength);
  EXPECT_EQ(d.value.unit, Unit::kPercent);
  EXPECT_FLOAT_EQ(d.value.number, 50.0f);
}

TEST(CssParserTest, AutoValue) {
  auto sheet = CssParser::Parse("div { width: auto; }");
  ASSERT_EQ(sheet.rules[0].declarations.size(), 1u);
  auto& d = sheet.rules[0].declarations[0];
  EXPECT_EQ(d.property, PropertyId::kWidth);
  EXPECT_EQ(d.value.type, ValueType::kAuto);
}

TEST(CssParserTest, InheritValue) {
  auto sheet = CssParser::Parse("div { color: inherit; }");
  ASSERT_EQ(sheet.rules[0].declarations.size(), 1u);
  auto& d = sheet.rules[0].declarations[0];
  EXPECT_EQ(d.property, PropertyId::kColor);
  EXPECT_EQ(d.value.type, ValueType::kInherit);
}

TEST(CssParserTest, Important) {
  auto sheet = CssParser::Parse("div { color: red !important; }");
  ASSERT_EQ(sheet.rules[0].declarations.size(), 1u);
  EXPECT_TRUE(sheet.rules[0].declarations[0].important);
}

TEST(CssParserTest, ZeroWithoutUnit) {
  auto sheet = CssParser::Parse("div { margin-top: 0; }");
  ASSERT_EQ(sheet.rules[0].declarations.size(), 1u);
  auto& d = sheet.rules[0].declarations[0];
  EXPECT_EQ(d.property, PropertyId::kMarginTop);
  EXPECT_EQ(d.value.type, ValueType::kLength);
  EXPECT_FLOAT_EQ(d.value.number, 0.0f);
  EXPECT_EQ(d.value.unit, Unit::kPx);
}

// --- Shorthand expansion ---

TEST(CssParserTest, MarginShorthand_1) {
  auto sheet = CssParser::Parse("div { margin: 10px; }");
  ASSERT_EQ(sheet.rules[0].declarations.size(), 4u);
  EXPECT_EQ(sheet.rules[0].declarations[0].property, PropertyId::kMarginTop);
  EXPECT_FLOAT_EQ(sheet.rules[0].declarations[0].value.number, 10.0f);
  EXPECT_EQ(sheet.rules[0].declarations[1].property, PropertyId::kMarginRight);
  EXPECT_FLOAT_EQ(sheet.rules[0].declarations[1].value.number, 10.0f);
  EXPECT_EQ(sheet.rules[0].declarations[2].property, PropertyId::kMarginBottom);
  EXPECT_FLOAT_EQ(sheet.rules[0].declarations[2].value.number, 10.0f);
  EXPECT_EQ(sheet.rules[0].declarations[3].property, PropertyId::kMarginLeft);
  EXPECT_FLOAT_EQ(sheet.rules[0].declarations[3].value.number, 10.0f);
}

TEST(CssParserTest, PaddingShorthand_2) {
  auto sheet = CssParser::Parse("div { padding: 5px 10px; }");
  ASSERT_EQ(sheet.rules[0].declarations.size(), 4u);
  EXPECT_EQ(sheet.rules[0].declarations[0].property, PropertyId::kPaddingTop);
  EXPECT_FLOAT_EQ(sheet.rules[0].declarations[0].value.number, 5.0f);
  EXPECT_EQ(sheet.rules[0].declarations[1].property, PropertyId::kPaddingRight);
  EXPECT_FLOAT_EQ(sheet.rules[0].declarations[1].value.number, 10.0f);
  EXPECT_EQ(sheet.rules[0].declarations[2].property, PropertyId::kPaddingBottom);
  EXPECT_FLOAT_EQ(sheet.rules[0].declarations[2].value.number, 5.0f);
  EXPECT_EQ(sheet.rules[0].declarations[3].property, PropertyId::kPaddingLeft);
  EXPECT_FLOAT_EQ(sheet.rules[0].declarations[3].value.number, 10.0f);
}

TEST(CssParserTest, BorderShorthand) {
  auto sheet = CssParser::Parse("div { border: 1px solid red; }");
  ASSERT_EQ(sheet.rules[0].declarations.size(), 12u);
  EXPECT_EQ(sheet.rules[0].declarations[0].property, PropertyId::kBorderTopWidth);
  EXPECT_FLOAT_EQ(sheet.rules[0].declarations[0].value.number, 1.0f);
  EXPECT_EQ(sheet.rules[0].declarations[4].property, PropertyId::kBorderTopStyle);
  EXPECT_EQ(sheet.rules[0].declarations[4].value.enum_value,
            static_cast<u16>(BorderStyle::kSolid));
  EXPECT_EQ(sheet.rules[0].declarations[8].property, PropertyId::kBorderTopColor);
  EXPECT_EQ(sheet.rules[0].declarations[8].value.color, 0xFF0000FFu);
}

// --- Multiple rules / full stylesheet ---

TEST(CssParserTest, MultipleRules) {
  auto sheet = CssParser::Parse("div { color: red; } p { color: blue; }");
  EXPECT_EQ(sheet.rules.size(), 2u);
}

TEST(CssParserTest, MultipleDeclarations) {
  auto sheet = CssParser::Parse("div { color: red; display: flex; width: 100px; }");
  ASSERT_EQ(sheet.rules.size(), 1u);
  EXPECT_EQ(sheet.rules[0].declarations.size(), 3u);
}

TEST(CssParserTest, DeclarationList) {
  auto decls = CssParser::ParseDeclarationList("color: red; display: flex");
  EXPECT_EQ(decls.size(), 2u);
  EXPECT_EQ(decls[0].property, PropertyId::kColor);
  EXPECT_EQ(decls[1].property, PropertyId::kDisplay);
}

// --- Error recovery ---

TEST(CssParserTest, UnknownProperty) {
  auto sheet = CssParser::Parse("div { foo: bar; color: red; }");
  ASSERT_EQ(sheet.rules.size(), 1u);
  ASSERT_EQ(sheet.rules[0].declarations.size(), 1u);
  EXPECT_EQ(sheet.rules[0].declarations[0].property, PropertyId::kColor);
}

}  // namespace
}  // namespace vx::css
