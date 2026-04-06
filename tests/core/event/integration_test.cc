#include <gtest/gtest.h>

#include "veloxa/core/css/parser.h"
#include "veloxa/core/css/selector_matcher.h"
#include "veloxa/core/css/style_resolver.h"
#include "veloxa/core/event/event_manager.h"
#include "veloxa/core/event/hit_test.h"
#include "veloxa/core/html/parser.h"
#include "veloxa/core/layout/layout_engine.h"
#include "veloxa/core/layout/text_shaper.h"

namespace {

using namespace vx;

class EventIntegrationTest : public ::testing::Test {
 protected:
  void ParseAndLayout(const char* html, const char* css_text) {
    doc_ = html::Parser::Parse(StringView(html));
    auto sheet = css::CssParser::Parse(css_text);
    sheets_.push_back(std::move(sheet));
    ctx_.text_shaper = &shaper_;
    ctx_.stylesheets = &sheets_;
    ctx_.viewport_width = 800;
    ctx_.viewport_height = 600;
    root_ = layout::LayoutEngine::Layout(doc_, ctx_);
  }

  dom::Element* FindElement(dom::Element* parent, dom::TagId tag) {
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

  dom::Document* doc_ = nullptr;
  Vector<css::Stylesheet> sheets_;
  layout::SimpleTextShaper shaper_;
  layout::LayoutContext ctx_{};
  layout::LayoutBox* root_ = nullptr;
};

TEST_F(EventIntegrationTest, HitTestSimpleDiv) {
  ParseAndLayout("<div id='d'></div>",
                 "#d { width: 100px; height: 100px; }");
  ASSERT_NE(root_, nullptr);

  auto* div = FindElement(doc_, dom::TagId::kDiv);
  ASSERT_NE(div, nullptr);

  auto* hit = event::HitTest(root_, 50, 50);
  EXPECT_EQ(hit, div);
}

TEST_F(EventIntegrationTest, HitTestNestedElements) {
  ParseAndLayout(
      "<div id='outer'><p id='inner'>text</p></div>",
      "#outer { width: 200px; height: 200px; } "
      "#inner { width: 50px; height: 50px; }");
  ASSERT_NE(root_, nullptr);

  auto* p = FindElement(doc_, dom::TagId::kP);
  ASSERT_NE(p, nullptr);

  auto* hit = event::HitTest(root_, 25, 25);
  EXPECT_EQ(hit, p);
}

TEST_F(EventIntegrationTest, HitTestMiss) {
  ParseAndLayout("<div id='d'></div>",
                 "#d { width: 100px; height: 100px; }");
  ASSERT_NE(root_, nullptr);

  auto* hit = event::HitTest(root_, 750, 550);
  auto* div = FindElement(doc_, dom::TagId::kDiv);
  EXPECT_NE(hit, div);
}

TEST_F(EventIntegrationTest, HoverPseudoClass) {
  ParseAndLayout("<div id='d'></div>",
                 "#d { width: 100px; height: 100px; }");
  ASSERT_NE(root_, nullptr);

  auto* div = FindElement(doc_, dom::TagId::kDiv);
  ASSERT_NE(div, nullptr);

  auto hover_sheet = css::CssParser::Parse(":hover {}");
  const auto& sel = hover_sheet.rules[0].selectors[0];

  event::EventManager em;
  EXPECT_FALSE(css::SelectorMatcher::Matches(sel, div, &em));

  event::InputEvent move{};
  move.type = event::EventType::kPointerMove;
  move.x = 50;
  move.y = 50;
  em.HandleInput(move, root_);

  EXPECT_TRUE(css::SelectorMatcher::Matches(sel, div, &em));
}

TEST_F(EventIntegrationTest, ActivePseudoClass) {
  ParseAndLayout("<div id='d'></div>",
                 "#d { width: 100px; height: 100px; }");
  ASSERT_NE(root_, nullptr);

  auto* div = FindElement(doc_, dom::TagId::kDiv);
  ASSERT_NE(div, nullptr);

  auto sheet = css::CssParser::Parse(":active {}");
  const auto& sel = sheet.rules[0].selectors[0];

  event::EventManager em;
  EXPECT_FALSE(css::SelectorMatcher::Matches(sel, div, &em));

  event::InputEvent down{};
  down.type = event::EventType::kPointerDown;
  down.x = 50;
  down.y = 50;
  em.HandleInput(down, root_);

  EXPECT_TRUE(css::SelectorMatcher::Matches(sel, div, &em));

  event::InputEvent up{};
  up.type = event::EventType::kPointerUp;
  up.x = 50;
  up.y = 50;
  em.HandleInput(up, root_);

  EXPECT_FALSE(css::SelectorMatcher::Matches(sel, div, &em));
}

TEST_F(EventIntegrationTest, FocusPseudoClass) {
  ParseAndLayout("<input id='inp'>",
                 "#inp { width: 200px; height: 30px; }");
  ASSERT_NE(root_, nullptr);

  auto* input_el = FindElement(doc_, dom::TagId::kInput);
  ASSERT_NE(input_el, nullptr);

  auto sheet = css::CssParser::Parse(":focus {}");
  const auto& sel = sheet.rules[0].selectors[0];

  event::EventManager em;
  EXPECT_FALSE(css::SelectorMatcher::Matches(sel, input_el, &em));

  event::InputEvent down{};
  down.type = event::EventType::kPointerDown;
  down.x = 100;
  down.y = 15;
  em.HandleInput(down, root_);

  EXPECT_TRUE(css::SelectorMatcher::Matches(sel, input_el, &em));
}

TEST_F(EventIntegrationTest, EventDispatchBubble) {
  ParseAndLayout(
      "<div id='outer'><p id='inner'>text</p></div>",
      "#outer { width: 200px; height: 200px; } "
      "#inner { width: 50px; height: 50px; }");
  ASSERT_NE(root_, nullptr);

  auto* div = FindElement(doc_, dom::TagId::kDiv);
  auto* p = FindElement(doc_, dom::TagId::kP);
  ASSERT_NE(div, nullptr);
  ASSERT_NE(p, nullptr);

  bool parent_received = false;
  event::EventManager em;
  em.AddEventListener(div, event::EventType::kPointerDown,
                      [&parent_received](event::DOMEvent&) {
                        parent_received = true;
                      });

  event::InputEvent down{};
  down.type = event::EventType::kPointerDown;
  down.x = 25;
  down.y = 25;
  em.HandleInput(down, root_);

  EXPECT_TRUE(parent_received);
}

TEST_F(EventIntegrationTest, HoverAncestorChain) {
  ParseAndLayout(
      "<div id='outer'><p id='inner'>text</p></div>",
      "#outer { width: 200px; height: 200px; } "
      "#inner { width: 50px; height: 50px; }");
  ASSERT_NE(root_, nullptr);

  auto* div = FindElement(doc_, dom::TagId::kDiv);
  auto* p = FindElement(doc_, dom::TagId::kP);
  ASSERT_NE(div, nullptr);
  ASSERT_NE(p, nullptr);

  event::EventManager em;
  event::InputEvent move{};
  move.type = event::EventType::kPointerMove;
  move.x = 25;
  move.y = 25;
  em.HandleInput(move, root_);

  EXPECT_TRUE(em.IsHovered(p));
  EXPECT_TRUE(em.IsHovered(div));

  auto div_hover = css::CssParser::Parse("div:hover {}");
  EXPECT_TRUE(css::SelectorMatcher::Matches(
      div_hover.rules[0].selectors[0], div, &em));
}

}  // namespace
