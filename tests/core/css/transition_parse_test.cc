#include <gtest/gtest.h>

#include "veloxa/core/css/parser.h"
#include "veloxa/core/css/style_resolver.h"
#include "veloxa/core/css/transition.h"
#include "veloxa/core/dom/document.h"
#include "veloxa/core/html/parser.h"

namespace vx::css {
namespace {

TEST(TransitionParseTest, ShorthandBackgroundColor) {
  auto sheet = CssParser::Parse(
      ".box { transition: background-color 300ms ease; }");
  ASSERT_EQ(sheet.rules.size(), 1u);
  const auto& decls = sheet.rules[0].declarations;

  bool has_prop = false, has_dur = false, has_timing = false, has_delay = false;
  for (const auto& d : decls) {
    if (d.property == PropertyId::kTransitionProperty) {
      has_prop = true;
      EXPECT_EQ(d.value.type, ValueType::kEnum);
      EXPECT_EQ(static_cast<PropertyId>(d.value.enum_value),
                PropertyId::kBackgroundColor);
    } else if (d.property == PropertyId::kTransitionDuration) {
      has_dur = true;
      EXPECT_EQ(d.value.type, ValueType::kNumber);
      EXPECT_FLOAT_EQ(d.value.number, 300.0f);
    } else if (d.property == PropertyId::kTransitionTimingFunction) {
      has_timing = true;
      EXPECT_EQ(d.value.type, ValueType::kEnum);
      EXPECT_EQ(static_cast<TimingFunction>(d.value.enum_value),
                TimingFunction::kEase);
    } else if (d.property == PropertyId::kTransitionDelay) {
      has_delay = true;
      EXPECT_EQ(d.value.type, ValueType::kNumber);
      EXPECT_FLOAT_EQ(d.value.number, 0.0f);
    }
  }
  EXPECT_TRUE(has_prop);
  EXPECT_TRUE(has_dur);
  EXPECT_TRUE(has_timing);
  EXPECT_TRUE(has_delay);
}

TEST(TransitionParseTest, ShorthandAllLinear) {
  auto sheet = CssParser::Parse(".box { transition: all 500ms linear; }");
  ASSERT_EQ(sheet.rules.size(), 1u);
  const auto& decls = sheet.rules[0].declarations;

  for (const auto& d : decls) {
    if (d.property == PropertyId::kTransitionProperty) {
      EXPECT_EQ(static_cast<PropertyId>(d.value.enum_value),
                PropertyId::kUnknown);
    } else if (d.property == PropertyId::kTransitionDuration) {
      EXPECT_FLOAT_EQ(d.value.number, 500.0f);
    } else if (d.property == PropertyId::kTransitionTimingFunction) {
      EXPECT_EQ(static_cast<TimingFunction>(d.value.enum_value),
                TimingFunction::kLinear);
    }
  }
}

TEST(TransitionParseTest, ShorthandWithSecondsUnit) {
  auto sheet = CssParser::Parse(".box { transition: opacity 0.3s ease-out; }");
  ASSERT_EQ(sheet.rules.size(), 1u);

  for (const auto& d : sheet.rules[0].declarations) {
    if (d.property == PropertyId::kTransitionDuration) {
      EXPECT_NEAR(d.value.number, 300.0f, 1.0f);
    } else if (d.property == PropertyId::kTransitionTimingFunction) {
      EXPECT_EQ(static_cast<TimingFunction>(d.value.enum_value),
                TimingFunction::kEaseOut);
    }
  }
}

TEST(TransitionParseTest, NoneProducesNoDeclarations) {
  auto sheet = CssParser::Parse(".box { transition: none; }");
  ASSERT_EQ(sheet.rules.size(), 1u);
  EXPECT_TRUE(sheet.rules[0].declarations.empty());
}

static dom::Element* FindElement(dom::Element* parent, dom::TagId tag) {
  for (auto* child = parent->first_child(); child;
       child = child->next_sibling()) {
    if (child->is_element()) {
      auto* el = static_cast<dom::Element*>(child);
      if (el->tag_id() == tag) return el;
      auto* found = FindElement(el, tag);
      if (found) return found;
    }
  }
  return nullptr;
}

TEST(TransitionParseTest, StyleResolverPopulatesTransitions) {
  auto sheet = CssParser::Parse(
      "#target { transition: background-color 200ms linear; "
      "background-color: red; }");

  Vector<Stylesheet> sheets;
  sheets.push_back(std::move(sheet));

  auto* doc = html::Parser::Parse("<div id=\"target\">X</div>");
  auto* el = FindElement(doc, dom::TagId::kDiv);
  ASSERT_NE(el, nullptr);

  auto style = StyleResolver::Resolve(el, sheets, nullptr);
  ASSERT_FALSE(style.transitions.empty());
  EXPECT_EQ(style.transitions[0].property, PropertyId::kBackgroundColor);
  EXPECT_FLOAT_EQ(style.transitions[0].duration_ms, 200.0f);
  EXPECT_EQ(style.transitions[0].timing, TimingFunction::kLinear);
  delete doc;
}

}  // namespace
}  // namespace vx::css
