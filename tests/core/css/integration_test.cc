#include <gtest/gtest.h>

#include "veloxa/core/css/computed_style.h"
#include "veloxa/core/css/parser.h"
#include "veloxa/core/css/style_resolver.h"
#include "veloxa/core/dom/document.h"
#include "veloxa/core/dom/element.h"
#include "veloxa/core/html/parser.h"

namespace vx::css {
namespace {

static dom::Element* FindElement(dom::Element* root, dom::TagId tag_id) {
  for (auto* child = root->first_child(); child; child = child->next_sibling()) {
    if (child->is_element()) {
      auto* el = static_cast<dom::Element*>(child);
      if (el->tag_id() == tag_id) return el;
      auto* found = FindElement(el, tag_id);
      if (found) return found;
    }
  }
  return nullptr;
}

static dom::Element* FindElementWithClass(dom::Element* root,
                                          const char* cls) {
  auto target = InternedString::Intern(cls);
  for (auto* child = root->first_child(); child; child = child->next_sibling()) {
    if (child->is_element()) {
      auto* el = static_cast<dom::Element*>(child);
      if (el->HasClass(target)) return el;
      auto* found = FindElementWithClass(el, cls);
      if (found) return found;
    }
  }
  return nullptr;
}

static dom::Element* FindElementWithId(dom::Element* root, const char* id) {
  auto target = InternedString::Intern(id);
  for (auto* child = root->first_child(); child; child = child->next_sibling()) {
    if (child->is_element()) {
      auto* el = static_cast<dom::Element*>(child);
      if (el->id() == target) return el;
      auto* found = FindElementWithId(el, id);
      if (found) return found;
    }
  }
  return nullptr;
}

class CssIntegrationTest : public ::testing::Test {
 protected:
  void TearDown() override { delete doc_; }

  void ParseHtml(const char* html) { doc_ = html::Parser::Parse(html); }

  dom::Document* doc_ = nullptr;
};

TEST_F(CssIntegrationTest, BasicStyleApplication) {
  ParseHtml("<div><p>Hello</p></div>");
  auto sheet = CssParser::Parse("p { color: #ff0000; display: inline; }");
  Vector<Stylesheet> sheets;
  sheets.push_back(std::move(sheet));

  auto* p = FindElement(doc_, dom::TagId::kP);
  ASSERT_NE(p, nullptr);

  auto style = StyleResolver::Resolve(p, sheets, nullptr);
  EXPECT_EQ(style.color, 0xFF0000FFu);
  EXPECT_EQ(style.display, Display::kInline);
}

TEST_F(CssIntegrationTest, ClassSelector_FullPipeline) {
  ParseHtml(R"(<div class="container"><span class="active">text</span></div>)");
  auto sheet = CssParser::Parse(".active { color: #00ff00; }");
  Vector<Stylesheet> sheets;
  sheets.push_back(std::move(sheet));

  auto* span = FindElementWithClass(doc_, "active");
  ASSERT_NE(span, nullptr);

  auto style = StyleResolver::Resolve(span, sheets, nullptr);
  EXPECT_EQ(style.color, 0x00FF00FFu);
}

TEST_F(CssIntegrationTest, IdSelector_FullPipeline) {
  ParseHtml(R"(<div id="main">content</div>)");
  auto sheet = CssParser::Parse("#main { background-color: #0000ff; }");
  Vector<Stylesheet> sheets;
  sheets.push_back(std::move(sheet));

  auto* el = FindElementWithId(doc_, "main");
  ASSERT_NE(el, nullptr);

  auto style = StyleResolver::Resolve(el, sheets, nullptr);
  EXPECT_EQ(style.background_color, 0x0000FFFFu);
}

TEST_F(CssIntegrationTest, InheritanceChain) {
  ParseHtml("<body><div><p>text</p></div></body>");
  auto sheet = CssParser::Parse("body { color: #ff0000; font-size: 20px; }");
  Vector<Stylesheet> sheets;
  sheets.push_back(std::move(sheet));

  auto* body = FindElement(doc_, dom::TagId::kBody);
  ASSERT_NE(body, nullptr);
  auto body_style = StyleResolver::Resolve(body, sheets, nullptr);
  EXPECT_EQ(body_style.color, 0xFF0000FFu);
  EXPECT_FLOAT_EQ(body_style.font_size.value, 20.0f);

  auto* div = FindElement(body, dom::TagId::kDiv);
  ASSERT_NE(div, nullptr);
  auto div_style = StyleResolver::Resolve(div, sheets, &body_style);
  EXPECT_EQ(div_style.color, 0xFF0000FFu);
  EXPECT_FLOAT_EQ(div_style.font_size.value, 20.0f);

  auto* p = FindElement(div, dom::TagId::kP);
  ASSERT_NE(p, nullptr);
  auto p_style = StyleResolver::Resolve(p, sheets, &div_style);
  EXPECT_EQ(p_style.color, 0xFF0000FFu);
  EXPECT_FLOAT_EQ(p_style.font_size.value, 20.0f);
}

TEST_F(CssIntegrationTest, CascadeSpecificity) {
  ParseHtml(R"(<div class="foo" id="bar">text</div>)");
  auto sheet = CssParser::Parse(
      "div { color: #ff0000; } .foo { color: #00ff00; } #bar { color: #0000ff; }");
  Vector<Stylesheet> sheets;
  sheets.push_back(std::move(sheet));

  auto* el = FindElementWithId(doc_, "bar");
  ASSERT_NE(el, nullptr);

  auto style = StyleResolver::Resolve(el, sheets, nullptr);
  EXPECT_EQ(style.color, 0x0000FFFFu);
}

TEST_F(CssIntegrationTest, InlineStyleOverrides) {
  ParseHtml("<div>text</div>");
  auto sheet = CssParser::Parse("div { color: #ff0000; }");
  Vector<Stylesheet> sheets;
  sheets.push_back(std::move(sheet));

  auto inline_decls = CssParser::ParseDeclarationList("color: #00ff00");

  auto* el = FindElement(doc_, dom::TagId::kDiv);
  ASSERT_NE(el, nullptr);

  auto style = StyleResolver::Resolve(el, sheets, nullptr, &inline_decls);
  EXPECT_EQ(style.color, 0x00FF00FFu);
}

TEST_F(CssIntegrationTest, FlexLayout) {
  ParseHtml(R"(<div class="flex-container">content</div>)");
  auto sheet = CssParser::Parse(
      ".flex-container { display: flex; flex-direction: column; "
      "justify-content: center; align-items: stretch; gap: 10px; }");
  Vector<Stylesheet> sheets;
  sheets.push_back(std::move(sheet));

  auto* el = FindElementWithClass(doc_, "flex-container");
  ASSERT_NE(el, nullptr);

  auto style = StyleResolver::Resolve(el, sheets, nullptr);
  EXPECT_EQ(style.display, Display::kFlex);
  EXPECT_EQ(style.flex_direction, FlexDirection::kColumn);
  EXPECT_EQ(style.justify_content, JustifyContent::kCenter);
  EXPECT_EQ(style.align_items, AlignItems::kStretch);
  EXPECT_FLOAT_EQ(style.gap.value, 10.0f);
  EXPECT_EQ(style.gap.unit, Unit::kPx);
}

TEST_F(CssIntegrationTest, MultipleSheetsWithMargin) {
  ParseHtml("<div>text</div>");
  Vector<Stylesheet> sheets;
  sheets.push_back(CssParser::Parse("div { margin: 10px; }"));
  sheets.push_back(CssParser::Parse("div { padding: 5px; }"));

  auto* el = FindElement(doc_, dom::TagId::kDiv);
  ASSERT_NE(el, nullptr);

  auto style = StyleResolver::Resolve(el, sheets, nullptr);
  EXPECT_FLOAT_EQ(style.margin_top.value, 10.0f);
  EXPECT_EQ(style.margin_top.unit, Unit::kPx);
  EXPECT_FLOAT_EQ(style.margin_right.value, 10.0f);
  EXPECT_FLOAT_EQ(style.margin_bottom.value, 10.0f);
  EXPECT_FLOAT_EQ(style.margin_left.value, 10.0f);
  EXPECT_FLOAT_EQ(style.padding_top.value, 5.0f);
  EXPECT_EQ(style.padding_top.unit, Unit::kPx);
  EXPECT_FLOAT_EQ(style.padding_right.value, 5.0f);
  EXPECT_FLOAT_EQ(style.padding_bottom.value, 5.0f);
  EXPECT_FLOAT_EQ(style.padding_left.value, 5.0f);
}

TEST_F(CssIntegrationTest, TypicalHmiDashboard) {
  ParseHtml(
      "<html><body>"
      "<div class=\"dashboard\">"
      "<div class=\"header\">Header</div>"
      "<div class=\"content\">"
      "<div class=\"gauge\">Speed</div>"
      "</div>"
      "</div>"
      "</body></html>");

  auto sheet = CssParser::Parse(
      ".dashboard { display: flex; flex-direction: column; "
      "width: 100%; height: 100%; background-color: #000000; color: #ffffff; } "
      ".header { height: 60px; background-color: #333333; } "
      ".content { display: flex; flex-grow: 1; } "
      ".gauge { width: 200px; height: 200px; }");
  Vector<Stylesheet> sheets;
  sheets.push_back(std::move(sheet));

  auto* dashboard = FindElementWithClass(doc_, "dashboard");
  ASSERT_NE(dashboard, nullptr);
  auto dash_style = StyleResolver::Resolve(dashboard, sheets, nullptr);
  EXPECT_EQ(dash_style.display, Display::kFlex);
  EXPECT_EQ(dash_style.flex_direction, FlexDirection::kColumn);
  EXPECT_FLOAT_EQ(dash_style.width.value, 100.0f);
  EXPECT_EQ(dash_style.width.unit, Unit::kPercent);
  EXPECT_FLOAT_EQ(dash_style.height.value, 100.0f);
  EXPECT_EQ(dash_style.height.unit, Unit::kPercent);
  EXPECT_EQ(dash_style.background_color, 0x000000FFu);
  EXPECT_EQ(dash_style.color, 0xFFFFFFFFu);

  auto* header = FindElementWithClass(doc_, "header");
  ASSERT_NE(header, nullptr);
  auto header_style = StyleResolver::Resolve(header, sheets, &dash_style);
  EXPECT_FLOAT_EQ(header_style.height.value, 60.0f);
  EXPECT_EQ(header_style.height.unit, Unit::kPx);
  EXPECT_EQ(header_style.background_color, 0x333333FFu);
  EXPECT_EQ(header_style.color, 0xFFFFFFFFu);

  auto* content = FindElementWithClass(doc_, "content");
  ASSERT_NE(content, nullptr);
  auto content_style = StyleResolver::Resolve(content, sheets, &dash_style);
  EXPECT_EQ(content_style.display, Display::kFlex);
  EXPECT_FLOAT_EQ(content_style.flex_grow, 1.0f);
  EXPECT_EQ(content_style.color, 0xFFFFFFFFu);

  auto* gauge = FindElementWithClass(doc_, "gauge");
  ASSERT_NE(gauge, nullptr);
  auto gauge_style = StyleResolver::Resolve(gauge, sheets, &content_style);
  EXPECT_FLOAT_EQ(gauge_style.width.value, 200.0f);
  EXPECT_EQ(gauge_style.width.unit, Unit::kPx);
  EXPECT_FLOAT_EQ(gauge_style.height.value, 200.0f);
  EXPECT_EQ(gauge_style.height.unit, Unit::kPx);
  EXPECT_EQ(gauge_style.color, 0xFFFFFFFFu);
}

TEST_F(CssIntegrationTest, NonInheritedStaysDefault) {
  ParseHtml("<div><span>child</span></div>");
  auto sheet = CssParser::Parse(
      "div { display: flex; width: 100px; }");
  Vector<Stylesheet> sheets;
  sheets.push_back(std::move(sheet));

  auto* div = FindElement(doc_, dom::TagId::kDiv);
  ASSERT_NE(div, nullptr);
  auto parent_style = StyleResolver::Resolve(div, sheets, nullptr);
  EXPECT_EQ(parent_style.display, Display::kFlex);

  auto* span = FindElement(div, dom::TagId::kSpan);
  ASSERT_NE(span, nullptr);
  auto child_style = StyleResolver::Resolve(span, sheets, &parent_style);
  EXPECT_EQ(child_style.display, Display::kBlock);
  EXPECT_TRUE(child_style.width.is_none());
}

TEST_F(CssIntegrationTest, ImportantOverridesHigherSpec) {
  ParseHtml(R"(<div id="x" class="y">text</div>)");
  auto sheet = CssParser::Parse(
      "#x { color: #ff0000; } .y { color: #00ff00 !important; }");
  Vector<Stylesheet> sheets;
  sheets.push_back(std::move(sheet));

  auto* el = FindElementWithId(doc_, "x");
  ASSERT_NE(el, nullptr);

  auto style = StyleResolver::Resolve(el, sheets, nullptr);
  EXPECT_EQ(style.color, 0x00FF00FFu);
}

TEST_F(CssIntegrationTest, DocumentOwnership) {
  auto* doc = html::Parser::Parse("<div><p>text</p></div>");
  ASSERT_NE(doc, nullptr);

  auto sheet = CssParser::Parse("p { color: #ff0000; }");
  Vector<Stylesheet> sheets;
  sheets.push_back(std::move(sheet));

  auto* p = FindElement(doc, dom::TagId::kP);
  ASSERT_NE(p, nullptr);
  auto style = StyleResolver::Resolve(p, sheets, nullptr);
  EXPECT_EQ(style.color, 0xFF0000FFu);

  delete doc;
}

}  // namespace
}  // namespace vx::css
