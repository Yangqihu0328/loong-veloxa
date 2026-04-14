#include <gtest/gtest.h>

#include "veloxa/core/dom/document.h"
#include "veloxa/core/dom/text.h"
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

    bindings_.Bind(engine_.context(), &doc_);
  }

  void TearDown() override {
    bindings_.Unbind();
    engine_.Shutdown();
  }

  QuickjsEngine engine_;
  dom::Document doc_;
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

}  // namespace vx::script
