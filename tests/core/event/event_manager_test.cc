#include "veloxa/core/event/event_manager.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "veloxa/core/css/computed_style.h"
#include "veloxa/core/css/parser.h"
#include "veloxa/core/css/selector_matcher.h"
#include "veloxa/core/dom/document.h"
#include "veloxa/core/layout/layout_box.h"

namespace {

using namespace vx;
using namespace vx::event;

class EventManagerTest : public ::testing::Test {
 protected:
  dom::Document doc;

  InputEvent MakePointerMove(f32 x, f32 y) {
    InputEvent e{};
    e.type = EventType::kPointerMove;
    e.x = x;
    e.y = y;
    return e;
  }

  InputEvent MakePointerDown(f32 x, f32 y) {
    InputEvent e{};
    e.type = EventType::kPointerDown;
    e.x = x;
    e.y = y;
    return e;
  }

  InputEvent MakePointerUp(f32 x, f32 y) {
    InputEvent e{};
    e.type = EventType::kPointerUp;
    e.x = x;
    e.y = y;
    return e;
  }

  InputEvent MakeKeyDown(u32 key_code) {
    InputEvent e{};
    e.type = EventType::kKeyDown;
    e.key_code = key_code;
    return e;
  }
};

TEST_F(EventManagerTest, InitialStateNull) {
  EventManager em;
  EXPECT_EQ(em.hovered_element(), nullptr);
  EXPECT_EQ(em.active_element(), nullptr);
  EXPECT_EQ(em.focused_element(), nullptr);
}

TEST_F(EventManagerTest, PointerMoveUpdatesHover) {
  auto* div = doc.CreateElement(dom::TagId::kDiv);
  doc.AppendChild(div);

  css::ComputedStyle style;
  layout::LayoutBox box;
  box.element = div;
  box.style = &style;
  box.x = 0;
  box.y = 0;
  box.content_width = 100;
  box.content_height = 100;

  EventManager em;
  em.HandleInput(MakePointerMove(50, 50), &box);
  EXPECT_EQ(em.hovered_element(), div);
}

TEST_F(EventManagerTest, PointerMoveToEmpty) {
  auto* div = doc.CreateElement(dom::TagId::kDiv);
  doc.AppendChild(div);

  css::ComputedStyle style;
  layout::LayoutBox box;
  box.element = div;
  box.style = &style;
  box.x = 0;
  box.y = 0;
  box.content_width = 100;
  box.content_height = 100;

  EventManager em;
  em.HandleInput(MakePointerMove(50, 50), &box);
  EXPECT_EQ(em.hovered_element(), div);

  em.HandleInput(MakePointerMove(200, 200), &box);
  EXPECT_EQ(em.hovered_element(), nullptr);
}

TEST_F(EventManagerTest, PointerDownUpdatesActive) {
  auto* div = doc.CreateElement(dom::TagId::kDiv);
  doc.AppendChild(div);

  css::ComputedStyle style;
  layout::LayoutBox box;
  box.element = div;
  box.style = &style;
  box.x = 0;
  box.y = 0;
  box.content_width = 100;
  box.content_height = 100;

  EventManager em;
  em.HandleInput(MakePointerDown(50, 50), &box);
  EXPECT_EQ(em.active_element(), div);
}

TEST_F(EventManagerTest, PointerDownUpdatesFocus) {
  auto* div = doc.CreateElement(dom::TagId::kDiv);
  doc.AppendChild(div);

  css::ComputedStyle style;
  layout::LayoutBox box;
  box.element = div;
  box.style = &style;
  box.x = 0;
  box.y = 0;
  box.content_width = 100;
  box.content_height = 100;

  EventManager em;
  em.HandleInput(MakePointerDown(50, 50), &box);
  EXPECT_EQ(em.focused_element(), div);
}

TEST_F(EventManagerTest, PointerUpClearsActive) {
  auto* div = doc.CreateElement(dom::TagId::kDiv);
  doc.AppendChild(div);

  css::ComputedStyle style;
  layout::LayoutBox box;
  box.element = div;
  box.style = &style;
  box.x = 0;
  box.y = 0;
  box.content_width = 100;
  box.content_height = 100;

  EventManager em;
  em.HandleInput(MakePointerDown(50, 50), &box);
  EXPECT_EQ(em.active_element(), div);

  em.HandleInput(MakePointerUp(50, 50), &box);
  EXPECT_EQ(em.active_element(), nullptr);
}

TEST_F(EventManagerTest, FocusChanges) {
  auto* div1 = doc.CreateElement(dom::TagId::kDiv);
  auto* div2 = doc.CreateElement(dom::TagId::kDiv);
  doc.AppendChild(div1);
  doc.AppendChild(div2);

  css::ComputedStyle style1, style2;
  layout::LayoutBox root_box, box1, box2;

  css::ComputedStyle root_style;
  root_box.element = nullptr;
  root_box.style = &root_style;
  root_box.x = 0;
  root_box.y = 0;
  root_box.content_width = 300;
  root_box.content_height = 100;

  box1.element = div1;
  box1.style = &style1;
  box1.x = 0;
  box1.y = 0;
  box1.content_width = 100;
  box1.content_height = 100;

  box2.element = div2;
  box2.style = &style2;
  box2.x = 150;
  box2.y = 0;
  box2.content_width = 100;
  box2.content_height = 100;

  root_box.AppendChild(&box1);
  root_box.AppendChild(&box2);

  EventManager em;
  em.HandleInput(MakePointerDown(50, 50), &root_box);
  EXPECT_EQ(em.focused_element(), div1);

  em.HandleInput(MakePointerDown(200, 50), &root_box);
  EXPECT_EQ(em.focused_element(), div2);
}

TEST_F(EventManagerTest, KeyboardToFocused) {
  auto* input_el = doc.CreateElement(dom::TagId::kInput);
  doc.AppendChild(input_el);

  css::ComputedStyle style;
  layout::LayoutBox box;
  box.element = input_el;
  box.style = &style;
  box.x = 0;
  box.y = 0;
  box.content_width = 200;
  box.content_height = 30;

  EventManager em;
  em.HandleInput(MakePointerDown(100, 15), &box);
  EXPECT_EQ(em.focused_element(), input_el);

  bool key_received = false;
  em.AddEventListener(input_el, EventType::kKeyDown,
                      [&key_received](DOMEvent& e) {
                        key_received = true;
                        EXPECT_EQ(e.input.key_code, 0x41u);
                      });

  em.HandleInput(MakeKeyDown(0x41), &box);
  EXPECT_TRUE(key_received);
}

TEST_F(EventManagerTest, KeyboardNoFocused) {
  EventManager em;
  css::ComputedStyle style;
  layout::LayoutBox box;
  box.style = &style;
  box.content_width = 100;
  box.content_height = 100;

  em.HandleInput(MakeKeyDown(0x41), &box);
  EXPECT_EQ(em.focused_element(), nullptr);
}

TEST_F(EventManagerTest, IsHoveredAncestor) {
  auto* parent = doc.CreateElement(dom::TagId::kDiv);
  auto* child = doc.CreateElement(dom::TagId::kP);
  doc.AppendChild(parent);
  parent->AppendChild(child);

  css::ComputedStyle parent_style, child_style;
  layout::LayoutBox parent_box, child_box;

  parent_box.element = parent;
  parent_box.style = &parent_style;
  parent_box.x = 0;
  parent_box.y = 0;
  parent_box.content_width = 200;
  parent_box.content_height = 200;

  child_box.element = child;
  child_box.style = &child_style;
  child_box.x = 10;
  child_box.y = 10;
  child_box.content_width = 50;
  child_box.content_height = 50;

  parent_box.AppendChild(&child_box);

  EventManager em;
  em.HandleInput(MakePointerMove(30, 30), &parent_box);
  EXPECT_EQ(em.hovered_element(), child);
  EXPECT_TRUE(em.IsHovered(child));
  EXPECT_TRUE(em.IsHovered(parent));
}

TEST_F(EventManagerTest, IsHoveredUnrelated) {
  auto* div_a = doc.CreateElement(dom::TagId::kDiv);
  auto* div_b = doc.CreateElement(dom::TagId::kDiv);
  doc.AppendChild(div_a);
  doc.AppendChild(div_b);

  css::ComputedStyle style_a, style_b;
  layout::LayoutBox root_box, box_a, box_b;

  css::ComputedStyle root_style;
  root_box.style = &root_style;
  root_box.content_width = 300;
  root_box.content_height = 100;

  box_a.element = div_a;
  box_a.style = &style_a;
  box_a.x = 0;
  box_a.y = 0;
  box_a.content_width = 100;
  box_a.content_height = 100;

  box_b.element = div_b;
  box_b.style = &style_b;
  box_b.x = 150;
  box_b.y = 0;
  box_b.content_width = 100;
  box_b.content_height = 100;

  root_box.AppendChild(&box_a);
  root_box.AppendChild(&box_b);

  EventManager em;
  em.HandleInput(MakePointerMove(50, 50), &root_box);
  EXPECT_TRUE(em.IsHovered(div_a));
  EXPECT_FALSE(em.IsHovered(div_b));
}

TEST_F(EventManagerTest, IsActiveAncestor) {
  auto* parent = doc.CreateElement(dom::TagId::kDiv);
  auto* child = doc.CreateElement(dom::TagId::kP);
  doc.AppendChild(parent);
  parent->AppendChild(child);

  css::ComputedStyle parent_style, child_style;
  layout::LayoutBox parent_box, child_box;

  parent_box.element = parent;
  parent_box.style = &parent_style;
  parent_box.x = 0;
  parent_box.y = 0;
  parent_box.content_width = 200;
  parent_box.content_height = 200;

  child_box.element = child;
  child_box.style = &child_style;
  child_box.x = 10;
  child_box.y = 10;
  child_box.content_width = 50;
  child_box.content_height = 50;

  parent_box.AppendChild(&child_box);

  EventManager em;
  em.HandleInput(MakePointerDown(30, 30), &parent_box);
  EXPECT_TRUE(em.IsActive(child));
  EXPECT_TRUE(em.IsActive(parent));
}

TEST_F(EventManagerTest, IsFocusedExact) {
  auto* parent = doc.CreateElement(dom::TagId::kDiv);
  auto* child = doc.CreateElement(dom::TagId::kP);
  doc.AppendChild(parent);
  parent->AppendChild(child);

  css::ComputedStyle parent_style, child_style;
  layout::LayoutBox parent_box, child_box;

  parent_box.element = parent;
  parent_box.style = &parent_style;
  parent_box.x = 0;
  parent_box.y = 0;
  parent_box.content_width = 200;
  parent_box.content_height = 200;

  child_box.element = child;
  child_box.style = &child_style;
  child_box.x = 10;
  child_box.y = 10;
  child_box.content_width = 50;
  child_box.content_height = 50;

  parent_box.AppendChild(&child_box);

  EventManager em;
  em.HandleInput(MakePointerDown(30, 30), &parent_box);
  EXPECT_TRUE(em.IsFocused(child));
  EXPECT_FALSE(em.IsFocused(parent));
}

TEST_F(EventManagerTest, SelectorMatcherHoverIntegration) {
  auto* div = doc.CreateElement(dom::TagId::kDiv);
  doc.AppendChild(div);

  css::ComputedStyle style;
  layout::LayoutBox box;
  box.element = div;
  box.style = &style;
  box.x = 0;
  box.y = 0;
  box.content_width = 100;
  box.content_height = 100;

  auto sheet = css::CssParser::Parse(":hover { }");
  ASSERT_EQ(sheet.rules.size(), 1u);
  const auto& sel = sheet.rules[0].selectors[0];

  EventManager em;

  EXPECT_FALSE(css::SelectorMatcher::Matches(sel, div, &em));

  em.HandleInput(MakePointerMove(50, 50), &box);

  EXPECT_TRUE(css::SelectorMatcher::Matches(sel, div, &em));

  em.HandleInput(MakePointerMove(200, 200), &box);

  EXPECT_FALSE(css::SelectorMatcher::Matches(sel, div, &em));
}

TEST_F(EventManagerTest, InvalidationCallbackOnHoverChange) {
  auto* div = doc.CreateElement(dom::TagId::kDiv);
  doc.AppendChild(div);

  css::ComputedStyle style;
  layout::LayoutBox box;
  box.element = div;
  box.style = &style;
  box.x = 0;
  box.y = 0;
  box.content_width = 100;
  box.content_height = 100;

  EventManager em;
  int callback_count = 0;
  em.SetInvalidationCallback([&]() { ++callback_count; });

  em.HandleInput(MakePointerMove(50, 50), &box);
  EXPECT_EQ(callback_count, 1);
}

TEST_F(EventManagerTest, NoCallbackWhenHoverUnchanged) {
  auto* div = doc.CreateElement(dom::TagId::kDiv);
  doc.AppendChild(div);

  css::ComputedStyle style;
  layout::LayoutBox box;
  box.element = div;
  box.style = &style;
  box.x = 0;
  box.y = 0;
  box.content_width = 100;
  box.content_height = 100;

  EventManager em;
  int callback_count = 0;
  em.SetInvalidationCallback([&]() { ++callback_count; });

  em.HandleInput(MakePointerMove(50, 50), &box);
  EXPECT_EQ(callback_count, 1);

  em.HandleInput(MakePointerMove(60, 60), &box);
  EXPECT_EQ(callback_count, 1);
}

TEST_F(EventManagerTest, CallbackOnPointerDown) {
  auto* div = doc.CreateElement(dom::TagId::kDiv);
  doc.AppendChild(div);

  css::ComputedStyle style;
  layout::LayoutBox box;
  box.element = div;
  box.style = &style;
  box.x = 0;
  box.y = 0;
  box.content_width = 100;
  box.content_height = 100;

  EventManager em;
  int callback_count = 0;
  em.SetInvalidationCallback([&]() { ++callback_count; });

  em.HandleInput(MakePointerDown(50, 50), &box);
  EXPECT_EQ(callback_count, 1);
}

TEST_F(EventManagerTest, CallbackOnPointerUp) {
  auto* div = doc.CreateElement(dom::TagId::kDiv);
  doc.AppendChild(div);

  css::ComputedStyle style;
  layout::LayoutBox box;
  box.element = div;
  box.style = &style;
  box.x = 0;
  box.y = 0;
  box.content_width = 100;
  box.content_height = 100;

  EventManager em;
  int callback_count = 0;
  em.SetInvalidationCallback([&]() { ++callback_count; });

  em.HandleInput(MakePointerDown(50, 50), &box);
  EXPECT_EQ(callback_count, 1);

  em.HandleInput(MakePointerUp(50, 50), &box);
  EXPECT_EQ(callback_count, 2);
}

}  // namespace
