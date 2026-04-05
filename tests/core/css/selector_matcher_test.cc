#include "veloxa/core/css/selector_matcher.h"

#include <gtest/gtest.h>

#include "veloxa/core/css/parser.h"
#include "veloxa/core/dom/document.h"

namespace vx::css {
namespace {

Selector ParseFirstSelector(const char* css) {
  auto sheet = CssParser::Parse(StringView(css));
  return sheet.rules[0].selectors[0];
}

TEST(SelectorMatcherTest, TagSelector_Match) {
  dom::Document doc;
  auto* el = doc.CreateElement(dom::TagId::kDiv);
  doc.AppendChild(el);
  auto sel = ParseFirstSelector("div {}");
  EXPECT_TRUE(SelectorMatcher::Matches(sel, el));
}

TEST(SelectorMatcherTest, TagSelector_NoMatch) {
  dom::Document doc;
  auto* el = doc.CreateElement(dom::TagId::kP);
  doc.AppendChild(el);
  auto sel = ParseFirstSelector("div {}");
  EXPECT_FALSE(SelectorMatcher::Matches(sel, el));
}

TEST(SelectorMatcherTest, ClassSelector_Match) {
  dom::Document doc;
  auto* el = doc.CreateElement(dom::TagId::kDiv);
  el->AddClass(InternedString::Intern(StringView("foo")));
  doc.AppendChild(el);
  auto sel = ParseFirstSelector(".foo {}");
  EXPECT_TRUE(SelectorMatcher::Matches(sel, el));
}

TEST(SelectorMatcherTest, ClassSelector_NoMatch) {
  dom::Document doc;
  auto* el = doc.CreateElement(dom::TagId::kDiv);
  doc.AppendChild(el);
  auto sel = ParseFirstSelector(".foo {}");
  EXPECT_FALSE(SelectorMatcher::Matches(sel, el));
}

TEST(SelectorMatcherTest, IdSelector_Match) {
  dom::Document doc;
  auto* el = doc.CreateElement(dom::TagId::kDiv);
  el->set_id(InternedString::Intern(StringView("bar")));
  doc.AppendChild(el);
  auto sel = ParseFirstSelector("#bar {}");
  EXPECT_TRUE(SelectorMatcher::Matches(sel, el));
}

TEST(SelectorMatcherTest, IdSelector_NoMatch) {
  dom::Document doc;
  auto* el = doc.CreateElement(dom::TagId::kDiv);
  el->set_id(InternedString::Intern(StringView("other")));
  doc.AppendChild(el);
  auto sel = ParseFirstSelector("#bar {}");
  EXPECT_FALSE(SelectorMatcher::Matches(sel, el));
}

TEST(SelectorMatcherTest, UniversalSelector) {
  dom::Document doc;
  auto* el = doc.CreateElement(dom::TagId::kSpan);
  doc.AppendChild(el);
  auto sel = ParseFirstSelector("* {}");
  EXPECT_TRUE(SelectorMatcher::Matches(sel, el));
}

TEST(SelectorMatcherTest, CompoundSelector_Match) {
  dom::Document doc;
  auto* el = doc.CreateElement(dom::TagId::kDiv);
  el->AddClass(InternedString::Intern(StringView("foo")));
  doc.AppendChild(el);
  auto sel = ParseFirstSelector("div.foo {}");
  EXPECT_TRUE(SelectorMatcher::Matches(sel, el));
}

TEST(SelectorMatcherTest, CompoundSelector_PartialNoMatch) {
  dom::Document doc;
  auto* el = doc.CreateElement(dom::TagId::kDiv);
  doc.AppendChild(el);
  auto sel = ParseFirstSelector("div.foo {}");
  EXPECT_FALSE(SelectorMatcher::Matches(sel, el));
}

TEST(SelectorMatcherTest, DescendantCombinator_Match) {
  dom::Document doc;
  auto* div = doc.CreateElement(dom::TagId::kDiv);
  auto* p = doc.CreateElement(dom::TagId::kP);
  doc.AppendChild(div);
  div->AppendChild(p);
  auto sel = ParseFirstSelector("div p {}");
  EXPECT_TRUE(SelectorMatcher::Matches(sel, p));
}

TEST(SelectorMatcherTest, DescendantCombinator_NoMatch) {
  dom::Document doc;
  auto* span = doc.CreateElement(dom::TagId::kSpan);
  auto* p = doc.CreateElement(dom::TagId::kP);
  doc.AppendChild(span);
  span->AppendChild(p);
  auto sel = ParseFirstSelector("div p {}");
  EXPECT_FALSE(SelectorMatcher::Matches(sel, p));
}

TEST(SelectorMatcherTest, ChildCombinator_Match) {
  dom::Document doc;
  auto* div = doc.CreateElement(dom::TagId::kDiv);
  auto* p = doc.CreateElement(dom::TagId::kP);
  doc.AppendChild(div);
  div->AppendChild(p);
  auto sel = ParseFirstSelector("div > p {}");
  EXPECT_TRUE(SelectorMatcher::Matches(sel, p));
}

TEST(SelectorMatcherTest, ChildCombinator_NoMatch) {
  dom::Document doc;
  auto* div = doc.CreateElement(dom::TagId::kDiv);
  auto* span = doc.CreateElement(dom::TagId::kSpan);
  auto* p = doc.CreateElement(dom::TagId::kP);
  doc.AppendChild(div);
  div->AppendChild(span);
  span->AppendChild(p);
  auto sel = ParseFirstSelector("div > p {}");
  EXPECT_FALSE(SelectorMatcher::Matches(sel, p));
}

TEST(SelectorMatcherTest, PseudoFirstChild) {
  dom::Document doc;
  auto* div = doc.CreateElement(dom::TagId::kDiv);
  auto* first = doc.CreateElement(dom::TagId::kP);
  auto* second = doc.CreateElement(dom::TagId::kSpan);
  doc.AppendChild(div);
  div->AppendChild(first);
  div->AppendChild(second);
  auto sel = ParseFirstSelector(":first-child {}");
  EXPECT_TRUE(SelectorMatcher::Matches(sel, first));
  EXPECT_FALSE(SelectorMatcher::Matches(sel, second));
}

TEST(SelectorMatcherTest, PseudoLastChild) {
  dom::Document doc;
  auto* div = doc.CreateElement(dom::TagId::kDiv);
  auto* first = doc.CreateElement(dom::TagId::kP);
  auto* second = doc.CreateElement(dom::TagId::kSpan);
  doc.AppendChild(div);
  div->AppendChild(first);
  div->AppendChild(second);
  auto sel = ParseFirstSelector(":last-child {}");
  EXPECT_FALSE(SelectorMatcher::Matches(sel, first));
  EXPECT_TRUE(SelectorMatcher::Matches(sel, second));
}

TEST(SelectorMatcherTest, AttributeSelector_HasAttr) {
  dom::Document doc;
  auto* el = doc.CreateElement(dom::TagId::kInput);
  el->SetAttribute(InternedString::Intern(StringView("disabled")),
                   String(StringView("")));
  doc.AppendChild(el);
  auto sel = ParseFirstSelector("[disabled] {}");
  EXPECT_TRUE(SelectorMatcher::Matches(sel, el));
}

TEST(SelectorMatcherTest, AttributeSelector_NoAttr) {
  dom::Document doc;
  auto* el = doc.CreateElement(dom::TagId::kInput);
  doc.AppendChild(el);
  auto sel = ParseFirstSelector("[disabled] {}");
  EXPECT_FALSE(SelectorMatcher::Matches(sel, el));
}

}  // namespace
}  // namespace vx::css
