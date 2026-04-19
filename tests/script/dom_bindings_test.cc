#include <gtest/gtest.h>

#include "veloxa/core/css/computed_style.h"
#include "veloxa/core/dom/document.h"
#include "veloxa/core/dom/text.h"
#include "veloxa/core/event/event_manager.h"
#include "veloxa/core/event/event_types.h"
#include "veloxa/core/layout/layout_box.h"
#include "veloxa/foundation/strings/interned_string.h"
#include "veloxa/foundation/strings/string.h"
#include "veloxa/script/dom_bindings.h"
#include "veloxa/script/quickjs_engine.h"

namespace vx::script {

class DomBindingsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(engine_.Init().ok());

    auto* div = doc_.CreateElement(dom::TagId::kDiv);
    div->set_id(InternedString::Intern("box"));
    auto* text1 = doc_.CreateText(String("Hello"));
    div->AppendChild(text1);
    doc_.AppendChild(div);

    auto* span = doc_.CreateElement(dom::TagId::kSpan);
    span->set_id(InternedString::Intern("btn"));
    auto* text2 = doc_.CreateText(String("Click"));
    span->AppendChild(text2);
    doc_.AppendChild(span);

    bindings_.Bind(engine_.context(), &doc_, &em_);
  }

  void TearDown() override {
    bindings_.Unbind();
    engine_.Shutdown();
  }

  QuickjsEngine engine_;
  dom::Document doc_;
  event::EventManager em_;
  DomBindings bindings_;
};

TEST_F(DomBindingsTest, GetElementByIdFound) {
  auto result = engine_.EvalGlobal(
      "document.getElementById('box') !== null ? 'found' : 'null'", "test.js");
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value(), "found");
}

TEST_F(DomBindingsTest, GetElementByIdNotFound) {
  auto result = engine_.EvalGlobal(
      "document.getElementById('missing') === null ? 'null' : 'found'",
      "test.js");
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value(), "null");
}

TEST_F(DomBindingsTest, ElementTagName) {
  auto result =
      engine_.EvalGlobal("document.getElementById('box').tagName", "test.js");
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value(), "div");
}

TEST_F(DomBindingsTest, ElementId) {
  auto result =
      engine_.EvalGlobal("document.getElementById('btn').id", "test.js");
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value(), "btn");
}

TEST_F(DomBindingsTest, TextContentRead) {
  auto result = engine_.EvalGlobal(
      "document.getElementById('box').textContent", "test.js");
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value(), "Hello");
}

TEST_F(DomBindingsTest, TextContentWrite) {
  auto result = engine_.EvalGlobal(
      "var el = document.getElementById('box');"
      "el.textContent = 'World';"
      "el.textContent",
      "test.js");
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value(), "World");
}

TEST_F(DomBindingsTest, GetSetAttribute) {
  auto result = engine_.EvalGlobal(
      "var el = document.getElementById('box');"
      "el.setAttribute('data-value', '42');"
      "el.getAttribute('data-value')",
      "test.js");
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value(), "42");
}

TEST_F(DomBindingsTest, StyleSetBackgroundColor) {
  engine_.EvalGlobal(
      "document.getElementById('box').style.backgroundColor = 'red'",
      "test.js");
  auto* div = static_cast<dom::Element*>(doc_.first_child());
  auto* decls = div->inline_declarations();
  ASSERT_NE(decls, nullptr);
  EXPECT_GE(decls->size(), 1u);
}

// ----- StyleGetProp read path (#46 / TASK-20260418-01) -----

TEST_F(DomBindingsTest, StyleGetBackgroundColor) {
  auto r = engine_.EvalGlobal(
      "var el = document.getElementById('box');"
      "el.style.backgroundColor = 'red';"
      "el.style.backgroundColor",
      "t.js");
  ASSERT_TRUE(r.ok());
  EXPECT_EQ(r.value(), "rgba(255, 0, 0, 255)");
}

TEST_F(DomBindingsTest, StyleGetWidthPx) {
  auto r = engine_.EvalGlobal(
      "var el = document.getElementById('box');"
      "el.style.width = '100px';"
      "el.style.width",
      "t.js");
  ASSERT_TRUE(r.ok());
  EXPECT_EQ(r.value(), "100px");
}

TEST_F(DomBindingsTest, StyleGetAuto) {
  auto r = engine_.EvalGlobal(
      "var el = document.getElementById('box');"
      "el.style.width = 'auto';"
      "el.style.width",
      "t.js");
  ASSERT_TRUE(r.ok());
  EXPECT_EQ(r.value(), "auto");
}

TEST_F(DomBindingsTest, StyleGetPercent) {
  auto r = engine_.EvalGlobal(
      "var el = document.getElementById('box');"
      "el.style.height = '50%';"
      "el.style.height",
      "t.js");
  ASSERT_TRUE(r.ok());
  EXPECT_EQ(r.value(), "50%");
}

TEST_F(DomBindingsTest, StyleGetOpacity) {
  auto r = engine_.EvalGlobal(
      "var el = document.getElementById('box');"
      "el.style.opacity = '0.5';"
      "el.style.opacity",
      "t.js");
  ASSERT_TRUE(r.ok());
  EXPECT_EQ(r.value(), "0.5");
}

TEST_F(DomBindingsTest, StyleGetUnsetReturnsEmpty) {
  auto r = engine_.EvalGlobal(
      "document.getElementById('box').style.color",
      "t.js");
  ASSERT_TRUE(r.ok());
  EXPECT_EQ(r.value(), "");
}

// display uses kEnum and is now serialized via css::EnumValueToCssString
// (TASK-20260419-01 B5). Each value maps round-trip set→get to its
// canonical CSS spelling.

TEST_F(DomBindingsTest, StyleGetDisplayBlock) {
  auto r = engine_.EvalGlobal(
      "var el = document.getElementById('box');"
      "el.style.display = 'block';"
      "el.style.display",
      "t.js");
  ASSERT_TRUE(r.ok());
  EXPECT_EQ(r.value(), "block");
}

TEST_F(DomBindingsTest, StyleGetDisplayFlex) {
  auto r = engine_.EvalGlobal(
      "var el = document.getElementById('box');"
      "el.style.display = 'flex';"
      "el.style.display",
      "t.js");
  ASSERT_TRUE(r.ok());
  EXPECT_EQ(r.value(), "flex");
}

TEST_F(DomBindingsTest, StyleGetDisplayNone) {
  auto r = engine_.EvalGlobal(
      "var el = document.getElementById('box');"
      "el.style.display = 'none';"
      "el.style.display",
      "t.js");
  ASSERT_TRUE(r.ok());
  EXPECT_EQ(r.value(), "none");
}

TEST_F(DomBindingsTest, StyleGetDisplayInlineBlock) {
  auto r = engine_.EvalGlobal(
      "var el = document.getElementById('box');"
      "el.style.display = 'inline-block';"
      "el.style.display",
      "t.js");
  ASSERT_TRUE(r.ok());
  EXPECT_EQ(r.value(), "inline-block");
}

TEST_F(DomBindingsTest, AddEventListenerRegisters) {
  auto result = engine_.EvalGlobal(
      "var btn = document.getElementById('btn');"
      "btn.addEventListener('pointerdown', function(e) {});"
      "'ok'",
      "test.js");
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value(), "ok");
}

TEST_F(DomBindingsTest, AddEventListenerInvalidTypeSilent) {
  auto result = engine_.EvalGlobal(
      "var btn = document.getElementById('btn');"
      "btn.addEventListener('nosuch', function(e) {});"
      "'ok'",
      "test.js");
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value(), "ok");
}

TEST_F(DomBindingsTest, RemoveEventListenerNoThrow) {
  auto result = engine_.EvalGlobal(
      "var btn = document.getElementById('btn');"
      "btn.addEventListener('pointerdown', function(e) {});"
      "btn.removeEventListener('pointerdown');"
      "'ok'",
      "test.js");
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value(), "ok");
}

TEST_F(DomBindingsTest, AddEventListenerMultipleTypes) {
  auto result = engine_.EvalGlobal(
      "var btn = document.getElementById('btn');"
      "btn.addEventListener('pointerdown', function(e) {});"
      "btn.addEventListener('keydown', function(e) {});"
      "'ok'",
      "test.js");
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value(), "ok");
}

// ----- Lifecycle / multi-instance regression tests (TASK-20260418-01) -----

TEST(DomBindingsLifecycleTest, JSClassIdStableAcrossBindings) {
  QuickjsEngine engine;
  ASSERT_TRUE(engine.Init().ok());

  dom::Document doc1;
  event::EventManager em1;
  auto* div1 = doc1.CreateElement(dom::TagId::kDiv);
  div1->set_id(InternedString::Intern("a"));
  doc1.AppendChild(div1);

  DomBindings b1;
  b1.Bind(engine.context(), &doc1, &em1);
  auto r1 = engine.EvalGlobal(
      "document.getElementById('a').tagName", "t1.js");
  ASSERT_TRUE(r1.ok());
  EXPECT_EQ(r1.value(), "div");
  b1.Unbind();

  dom::Document doc2;
  event::EventManager em2;
  auto* span2 = doc2.CreateElement(dom::TagId::kSpan);
  span2->set_id(InternedString::Intern("b"));
  doc2.AppendChild(span2);

  DomBindings b2;
  b2.Bind(engine.context(), &doc2, &em2);
  auto r2 = engine.EvalGlobal(
      "document.getElementById('b').tagName", "t2.js");
  ASSERT_TRUE(r2.ok());
  EXPECT_EQ(r2.value(), "span");
  b2.Unbind();

  engine.Shutdown();
}

TEST(DomBindingsLifecycleTest, MultipleInstancesIndependentDocuments) {
  QuickjsEngine eng1, eng2;
  ASSERT_TRUE(eng1.Init().ok());
  ASSERT_TRUE(eng2.Init().ok());

  dom::Document doc1, doc2;
  event::EventManager em1, em2;

  auto* e1 = doc1.CreateElement(dom::TagId::kDiv);
  e1->set_id(InternedString::Intern("only-in-1"));
  doc1.AppendChild(e1);

  auto* e2 = doc2.CreateElement(dom::TagId::kSpan);
  e2->set_id(InternedString::Intern("only-in-2"));
  doc2.AppendChild(e2);

  DomBindings b1, b2;
  b1.Bind(eng1.context(), &doc1, &em1);
  // Interleaved: b2 binds while b1 still active. Old impl would overwrite
  // the shared g_bound_doc pointer and break eng1 queries.
  b2.Bind(eng2.context(), &doc2, &em2);

  auto r1 = eng1.EvalGlobal(
      "document.getElementById('only-in-1') !== null ? 'found' : 'null'",
      "cross1.js");
  ASSERT_TRUE(r1.ok());
  EXPECT_EQ(r1.value(), "found");

  auto r2 = eng2.EvalGlobal(
      "document.getElementById('only-in-2') !== null ? 'found' : 'null'",
      "cross2.js");
  ASSERT_TRUE(r2.ok());
  EXPECT_EQ(r2.value(), "found");

  // Cross queries should not leak between engines.
  auto r1_cross = eng1.EvalGlobal(
      "document.getElementById('only-in-2') === null ? 'null' : 'found'",
      "cross3.js");
  ASSERT_TRUE(r1_cross.ok());
  EXPECT_EQ(r1_cross.value(), "null");

  b1.Unbind();
  b2.Unbind();
  eng1.Shutdown();
  eng2.Shutdown();
}

TEST(DomBindingsLifecycleTest, UnbindClearsListenersFromEventManager) {
  // Regression for #50: DomBindings::Unbind must remove each tracked
  // element's listeners from EventManager BEFORE freeing the JS
  // callbacks those lambdas captured. Verified here by inspecting that
  // an em sibling listener on the same element is cleared too —
  // RemoveEventListeners(el) is element-scoped so any listener on it
  // is gone after Unbind. Before this fix, em retained the JS lambda
  // (and the sibling listener) and could invoke it over a freed
  // JSValue.
  QuickjsEngine engine;
  ASSERT_TRUE(engine.Init().ok());

  dom::Document doc;
  event::EventManager em;
  auto* el = doc.CreateElement(dom::TagId::kDiv);
  el->set_id(InternedString::Intern("t"));
  doc.AppendChild(el);

  DomBindings bindings;
  bindings.Bind(engine.context(), &doc, &em);

  auto r = engine.EvalGlobal(
      "document.getElementById('t').addEventListener("
      "'pointerdown', function(){});",
      "t.js");
  ASSERT_TRUE(r.ok());

  // Sibling listener — unrelated to DomBindings. Used as observer.
  int sibling_fired = 0;
  em.AddEventListener(el, event::EventType::kKeyDown,
                      [&sibling_fired](event::DOMEvent&) {
                        ++sibling_fired;
                      });

  css::ComputedStyle style;
  layout::LayoutBox box;
  box.element = el;
  box.style = &style;
  box.x = 0;
  box.y = 0;
  box.content_width = 100;
  box.content_height = 50;

  event::InputEvent down{};
  down.type = event::EventType::kPointerDown;
  down.x = 10;
  down.y = 10;
  em.HandleInput(down, &box);
  ASSERT_EQ(em.focused_element(), el);

  event::InputEvent key{};
  key.type = event::EventType::kKeyDown;
  key.key_code = 0x41;
  em.HandleInput(key, &box);
  EXPECT_EQ(sibling_fired, 1);  // baseline: listener works before Unbind

  bindings.Unbind();

  // After Unbind, em must no longer hold any listener for el.
  em.HandleInput(key, &box);
  EXPECT_EQ(sibling_fired, 1);  // still 1 → all listeners on el cleared

  engine.Shutdown();
}

TEST(DomBindingsLifecycleTest, UnbindThenBoundReflectsState) {
  // Sanity: bound() reflects Bind/Unbind transitions.
  QuickjsEngine engine;
  ASSERT_TRUE(engine.Init().ok());
  dom::Document doc;
  event::EventManager em;

  DomBindings bindings;
  EXPECT_FALSE(bindings.bound());
  bindings.Bind(engine.context(), &doc, &em);
  EXPECT_TRUE(bindings.bound());
  EXPECT_EQ(bindings.context(), engine.context());
  EXPECT_EQ(bindings.document(), &doc);
  EXPECT_EQ(bindings.event_manager(), &em);
  bindings.Unbind();
  EXPECT_FALSE(bindings.bound());
  engine.Shutdown();
}

}  // namespace vx::script
