#include "veloxa/core/css/style_resolver.h"

#include <gtest/gtest.h>

#include "veloxa/core/css/parser.h"
#include "veloxa/core/dom/document.h"

namespace vx::css {
namespace {

TEST(StyleResolverTest, DefaultValues) {
  dom::Document doc;
  auto* el = doc.CreateElement(dom::TagId::kDiv);
  doc.AppendChild(el);
  Vector<Stylesheet> sheets;
  auto style = StyleResolver::Resolve(el, sheets, nullptr);
  EXPECT_EQ(style.display, Display::kBlock);
  EXPECT_EQ(style.position, Position::kStatic);
  EXPECT_EQ(style.color, 0x000000FFu);
  EXPECT_FLOAT_EQ(style.opacity, 1.0f);
  EXPECT_EQ(style.box_sizing, BoxSizing::kContentBox);
}

TEST(StyleResolverTest, SingleRuleApply) {
  dom::Document doc;
  auto* el = doc.CreateElement(dom::TagId::kDiv);
  doc.AppendChild(el);
  Vector<Stylesheet> sheets;
  sheets.push_back(CssParser::Parse(StringView("div { display: flex; }")));
  auto style = StyleResolver::Resolve(el, sheets, nullptr);
  EXPECT_EQ(style.display, Display::kFlex);
}

TEST(StyleResolverTest, ColorApply) {
  dom::Document doc;
  auto* el = doc.CreateElement(dom::TagId::kDiv);
  doc.AppendChild(el);
  Vector<Stylesheet> sheets;
  sheets.push_back(CssParser::Parse(StringView("div { color: #ff0000; }")));
  auto style = StyleResolver::Resolve(el, sheets, nullptr);
  EXPECT_EQ(style.color, 0xFF0000FFu);
}

TEST(StyleResolverTest, LengthApply) {
  dom::Document doc;
  auto* el = doc.CreateElement(dom::TagId::kDiv);
  doc.AppendChild(el);
  Vector<Stylesheet> sheets;
  sheets.push_back(CssParser::Parse(StringView("div { width: 100px; }")));
  auto style = StyleResolver::Resolve(el, sheets, nullptr);
  EXPECT_FLOAT_EQ(style.width.value, 100.0f);
  EXPECT_EQ(style.width.unit, Unit::kPx);
}

TEST(StyleResolverTest, SpecificityOrder) {
  dom::Document doc;
  auto* el = doc.CreateElement(dom::TagId::kDiv);
  el->AddClass(InternedString::Intern(StringView("foo")));
  doc.AppendChild(el);
  Vector<Stylesheet> sheets;
  sheets.push_back(CssParser::Parse(
      StringView("div { color: #ff0000; } .foo { color: #00ff00; }")));
  auto style = StyleResolver::Resolve(el, sheets, nullptr);
  EXPECT_EQ(style.color, 0x00FF00FFu);
}

TEST(StyleResolverTest, LaterRuleWins) {
  dom::Document doc;
  auto* el = doc.CreateElement(dom::TagId::kDiv);
  doc.AppendChild(el);
  Vector<Stylesheet> sheets;
  sheets.push_back(CssParser::Parse(
      StringView("div { color: #ff0000; } div { color: #00ff00; }")));
  auto style = StyleResolver::Resolve(el, sheets, nullptr);
  EXPECT_EQ(style.color, 0x00FF00FFu);
}

TEST(StyleResolverTest, ImportantWins) {
  dom::Document doc;
  auto* el = doc.CreateElement(dom::TagId::kDiv);
  el->AddClass(InternedString::Intern(StringView("foo")));
  doc.AppendChild(el);
  Vector<Stylesheet> sheets;
  sheets.push_back(CssParser::Parse(StringView(
      "div { color: #ff0000 !important; } .foo { color: #00ff00; }")));
  auto style = StyleResolver::Resolve(el, sheets, nullptr);
  EXPECT_EQ(style.color, 0xFF0000FFu);
}

TEST(StyleResolverTest, InheritColor) {
  dom::Document doc;
  auto* el = doc.CreateElement(dom::TagId::kDiv);
  doc.AppendChild(el);
  Vector<Stylesheet> sheets;
  ComputedStyle parent;
  parent.color = 0xFF0000FFu;
  auto style = StyleResolver::Resolve(el, sheets, &parent);
  EXPECT_EQ(style.color, 0xFF0000FFu);
}

TEST(StyleResolverTest, NonInheritedNotInherited) {
  dom::Document doc;
  auto* el = doc.CreateElement(dom::TagId::kDiv);
  doc.AppendChild(el);
  Vector<Stylesheet> sheets;
  ComputedStyle parent;
  parent.display = Display::kFlex;
  auto style = StyleResolver::Resolve(el, sheets, &parent);
  EXPECT_EQ(style.display, Display::kBlock);
}

TEST(StyleResolverTest, InheritKeyword) {
  dom::Document doc;
  auto* el = doc.CreateElement(dom::TagId::kDiv);
  doc.AppendChild(el);
  Vector<Stylesheet> sheets;
  sheets.push_back(CssParser::Parse(StringView("div { color: inherit; }")));
  ComputedStyle parent;
  parent.color = 0x00FF00FFu;
  auto style = StyleResolver::Resolve(el, sheets, &parent);
  EXPECT_EQ(style.color, 0x00FF00FFu);
}

TEST(StyleResolverTest, InitialKeyword) {
  dom::Document doc;
  auto* el = doc.CreateElement(dom::TagId::kDiv);
  doc.AppendChild(el);
  Vector<Stylesheet> sheets;
  sheets.push_back(CssParser::Parse(StringView("div { color: initial; }")));
  ComputedStyle parent;
  parent.color = 0xFF0000FFu;
  auto style = StyleResolver::Resolve(el, sheets, &parent);
  EXPECT_EQ(style.color, 0x000000FFu);
}

TEST(StyleResolverTest, InlineStylePriority) {
  dom::Document doc;
  auto* el = doc.CreateElement(dom::TagId::kDiv);
  doc.AppendChild(el);
  Vector<Stylesheet> sheets;
  sheets.push_back(CssParser::Parse(StringView("div { color: #ff0000; }")));
  auto inline_decls = CssParser::ParseDeclarationList(StringView("color: #00ff00"));
  auto style = StyleResolver::Resolve(el, sheets, nullptr, &inline_decls);
  EXPECT_EQ(style.color, 0x00FF00FFu);
}

TEST(StyleResolverTest, FlexProperties) {
  dom::Document doc;
  auto* el = doc.CreateElement(dom::TagId::kDiv);
  doc.AppendChild(el);
  Vector<Stylesheet> sheets;
  sheets.push_back(CssParser::Parse(StringView(
      "div { display: flex; flex-direction: column; justify-content: center; }")));
  auto style = StyleResolver::Resolve(el, sheets, nullptr);
  EXPECT_EQ(style.display, Display::kFlex);
  EXPECT_EQ(style.flex_direction, FlexDirection::kColumn);
  EXPECT_EQ(style.justify_content, JustifyContent::kCenter);
}

TEST(StyleResolverTest, BorderProperties) {
  dom::Document doc;
  auto* el = doc.CreateElement(dom::TagId::kDiv);
  doc.AppendChild(el);
  Vector<Stylesheet> sheets;
  sheets.push_back(CssParser::Parse(StringView(
      "div { border-top-width: 1px; border-top-style: solid; border-top-color: #ff0000; }")));
  auto style = StyleResolver::Resolve(el, sheets, nullptr);
  EXPECT_FLOAT_EQ(style.border_width[0].value, 1.0f);
  EXPECT_EQ(style.border_width[0].unit, Unit::kPx);
  EXPECT_EQ(style.border_style[0], BorderStyle::kSolid);
  EXPECT_EQ(style.border_color[0], 0xFF0000FFu);
}

TEST(StyleResolverTest, FontInheritance) {
  dom::Document doc;
  auto* el = doc.CreateElement(dom::TagId::kDiv);
  doc.AppendChild(el);
  Vector<Stylesheet> sheets;
  ComputedStyle parent;
  parent.font_size = LengthValue::Px(20.0f);
  auto style = StyleResolver::Resolve(el, sheets, &parent);
  EXPECT_FLOAT_EQ(style.font_size.value, 20.0f);
  EXPECT_EQ(style.font_size.unit, Unit::kPx);
}

}  // namespace
}  // namespace vx::css
