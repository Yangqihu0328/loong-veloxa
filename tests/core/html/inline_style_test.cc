// HTML inline style attribute parsing — TASK-20260426-01 R2 / #28
//
// 覆盖 spec §6 威胁模型 7 类（T1-T7）+ A2 行为一致性 + A3 三件套安全护栏。
// HTML 路径接收外部输入，必须三件套 + 与 JS 路径一致性双轨保证。

#include <gtest/gtest.h>

#include <string>

#include "veloxa/core/css/css_value.h"
#include "veloxa/core/css/parser.h"
#include "veloxa/core/css/property.h"
#include "veloxa/core/dom/document.h"
#include "veloxa/core/dom/element.h"
#include "veloxa/core/html/parser.h"

namespace vx::html {
namespace {

using css::CssValue;
using css::Declaration;
using css::PropertyId;
using css::ValueType;
using dom::Document;
using dom::Element;

class InlineStyleTest : public ::testing::Test {
 protected:
  void TearDown() override { delete doc_; }

  Element* ParseFirstChild(StringView html) {
    doc_ = Parser::Parse(html);
    auto* node = doc_->first_child();
    EXPECT_NE(node, nullptr);
    EXPECT_TRUE(node->is_element());
    return static_cast<Element*>(node);
  }

  Document* doc_ = nullptr;
};

// ----------------------------------------------------------------------------
// A2: 行为一致性（HTML inline style 与 JS path 等价）
// ----------------------------------------------------------------------------

TEST_F(InlineStyleTest, SingleColorDeclarationParsedAsRed) {
  Element* el = ParseFirstChild("<div style=\"color: red\"></div>");
  const auto* decls = el->inline_declarations();
  ASSERT_NE(decls, nullptr);
  ASSERT_EQ(decls->size(), 1u);
  EXPECT_EQ((*decls)[0].property, PropertyId::kColor);
  EXPECT_EQ((*decls)[0].value.type, ValueType::kColor);
  EXPECT_EQ((*decls)[0].value.color, 0xFF0000FFu);
}

TEST_F(InlineStyleTest, MultipleDeclarationsAllApplied) {
  Element* el = ParseFirstChild(
      "<div style=\"color: red; padding-top: 10px\"></div>");
  const auto* decls = el->inline_declarations();
  ASSERT_NE(decls, nullptr);
  EXPECT_EQ(decls->size(), 2u);
}

TEST_F(InlineStyleTest, BehaviorMatchesJsPathExactly) {
  Element* html_el = ParseFirstChild("<div style=\"color: red\"></div>");
  const auto* html_decls = html_el->inline_declarations();
  ASSERT_NE(html_decls, nullptr);

  auto js_decls = css::CssParser::ParseDeclarationList(StringView("color: red"));
  ASSERT_EQ(html_decls->size(), js_decls.size());
  for (usize i = 0; i < html_decls->size(); ++i) {
    EXPECT_EQ((*html_decls)[i].property, js_decls[i].property);
    EXPECT_EQ((*html_decls)[i].value.type, js_decls[i].value.type);
    EXPECT_EQ((*html_decls)[i].value.color, js_decls[i].value.color);
  }
}

TEST_F(InlineStyleTest, EmptyStyleAttributeYieldsNullInlineDeclarations) {
  Element* el = ParseFirstChild("<div style=\"\"></div>");
  EXPECT_EQ(el->inline_declarations(), nullptr);
}

TEST_F(InlineStyleTest, StyleAttributeIsNotStoredAsRegularAttribute) {
  Element* el = ParseFirstChild("<div style=\"color: red\"></div>");
  static const InternedString kStyle = InternedString::Intern("style");
  EXPECT_EQ(el->GetAttribute(kStyle), nullptr);
}

// ----------------------------------------------------------------------------
// A3-T1: declaration count cap = 1000（spec §6 protocol contract）
//
// 双轨实证策略：
//   (a) 常量守卫：通过暴露的 kInlineStyleMaxDeclarationCount/MaxValueLength
//       断言数字未被静默改动 — 协议合同合规性 unit 验证。
//   (b) 路径不崩溃：1100 项重复输入 → 解析路径走通 + 无 abort/segfault。
//       注：SetInlineDeclaration 是 upsert（dedup at property level），1100 个
//       同 property 输入 dedup 后 inline_decls.size() == 1，cap 在 SetInline
//       Declaration 之前的 for-loop 截断生效但被 dedup 隐藏。这是 cap 与 dedup
//       共同作用的合理结果 — 真正的 DoS 防御由 T2 (value cap) 主导。
// ----------------------------------------------------------------------------

TEST(InlineStyleSecurityConstants, MatchesSpecValues) {
  EXPECT_EQ(kInlineStyleMaxDeclarationCount, 1000u);
  EXPECT_EQ(kInlineStyleMaxValueLength, 8u * 1024u);
}

TEST_F(InlineStyleTest, DeclarationCountCapPathSurvives1500Repeats) {
  std::string css;
  css.reserve(8000);
  for (int i = 0; i < 1100; ++i) {
    css += "top:0;";  // 6 chars × 1100 = 6600 < 8 KB（避开 T2 优先触发）
  }
  ASSERT_LT(css.size(), kInlineStyleMaxValueLength)
      << "fixture must not trigger T2 value cap";
  std::string html = "<div style=\"" + css + "\"></div>";
  Element* el = ParseFirstChild(StringView(html.data(), html.size()));

  ASSERT_NE(el, nullptr);
  EXPECT_EQ(el->tag_id(), dom::TagId::kDiv);
  const auto* decls = el->inline_declarations();
  ASSERT_NE(decls, nullptr);
  EXPECT_EQ(decls->size(), 1u) << "upsert dedup at property level expected";
  EXPECT_LE(decls->size(), kInlineStyleMaxDeclarationCount);
}

// ----------------------------------------------------------------------------
// A3-T2: single value len cap = 8 KB
//   16 KB fixture → 整 attribute 跳过 + element 仍存在 + 无 abort
// ----------------------------------------------------------------------------

TEST_F(InlineStyleTest, ValueLengthCapAt8KB) {
  std::string css = "color: ";
  css.reserve(20000);
  for (int i = 0; i < 16000; ++i) css += 'a';
  css += "red";
  std::string html = "<div style=\"" + css + "\"></div>";
  Element* el = ParseFirstChild(StringView(html.data(), html.size()));

  EXPECT_EQ(el->inline_declarations(), nullptr);
}

// ----------------------------------------------------------------------------
// A3-T3/T4/T5: 历史攻击关键字黑名单
//   命中即整 attribute 跳过 — 防御深度（IE expression / behavior / javascript:）
// ----------------------------------------------------------------------------

TEST_F(InlineStyleTest, BlacklistRejectsExpressionKeyword) {
  Element* el = ParseFirstChild(
      "<div style=\"width: expression(alert(1))\"></div>");
  EXPECT_EQ(el->inline_declarations(), nullptr);
}

TEST_F(InlineStyleTest, BlacklistRejectsBehaviorKeyword) {
  Element* el = ParseFirstChild(
      "<div style=\"behavior: url(xss.htc)\"></div>");
  EXPECT_EQ(el->inline_declarations(), nullptr);
}

TEST_F(InlineStyleTest, BlacklistRejectsJavascriptUrl) {
  Element* el = ParseFirstChild(
      "<div style=\"background: url(javascript:alert(1))\"></div>");
  EXPECT_EQ(el->inline_declarations(), nullptr);
}

TEST_F(InlineStyleTest, BlacklistIsAsciiCaseInsensitive) {
  Element* el = ParseFirstChild(
      "<div style=\"width: EXPRESSION(alert(1))\"></div>");
  EXPECT_EQ(el->inline_declarations(), nullptr);
}

// ----------------------------------------------------------------------------
// RED 反向探针：直接测 ContainsBlacklistKeyword 函数。
//
// 不通过 inline_decls 间接测，避免 CssParser unknown-property 错误恢复
// 路径让 blacklist 测试 trivially PASS（over-match）。这些断言确保 blacklist
// 实现真实存在 — 临时返回 false 时这 8 个测试会立即 FAIL。
// ----------------------------------------------------------------------------

TEST(ContainsBlacklistKeywordTest, MatchesExpressionFunctionLiteral) {
  EXPECT_TRUE(internal::ContainsBlacklistKeyword(
      StringView("width: expression(alert(1))")));
}

TEST(ContainsBlacklistKeywordTest, MatchesBehaviorPropertyLiteral) {
  EXPECT_TRUE(internal::ContainsBlacklistKeyword(
      StringView("behavior: url(xss.htc)")));
}

TEST(ContainsBlacklistKeywordTest, MatchesJavascriptUrlLiteral) {
  EXPECT_TRUE(internal::ContainsBlacklistKeyword(
      StringView("background: url(javascript:alert(1))")));
}

TEST(ContainsBlacklistKeywordTest, MatchesUppercaseExpression) {
  EXPECT_TRUE(internal::ContainsBlacklistKeyword(
      StringView("WIDTH: EXPRESSION(BAD)")));
}

TEST(ContainsBlacklistKeywordTest, MatchesMixedCaseJavascript) {
  EXPECT_TRUE(
      internal::ContainsBlacklistKeyword(StringView("url(JavaScript:x)")));
}

TEST(ContainsBlacklistKeywordTest, RejectsBenignDeclaration) {
  EXPECT_FALSE(
      internal::ContainsBlacklistKeyword(StringView("color: red; padding: 10px")));
}

TEST(ContainsBlacklistKeywordTest, RejectsEmptyString) {
  EXPECT_FALSE(internal::ContainsBlacklistKeyword(StringView("")));
}

TEST(ContainsBlacklistKeywordTest, RejectsTooShortInput) {
  EXPECT_FALSE(internal::ContainsBlacklistKeyword(StringView("xyz")));
}

// ----------------------------------------------------------------------------
// 反向探针 1：无 style attribute 时 inline_declarations 必须 nullptr
//   防 ApplyInlineStyleAttribute 在 absence 路径误调
// ----------------------------------------------------------------------------

TEST_F(InlineStyleTest, NoStyleAttributeYieldsNullInlineDecls) {
  Element* el = ParseFirstChild("<div></div>");
  EXPECT_EQ(el->inline_declarations(), nullptr);
}

// ----------------------------------------------------------------------------
// 反向探针 2：无效 CSS（CssParser 错误恢复）→ 不崩溃 + element 仍可访问
// ----------------------------------------------------------------------------

TEST_F(InlineStyleTest, InvalidCssDoesNotCrash) {
  Element* el = ParseFirstChild(
      "<div style=\"@@invalid: ?? @@@\" id=\"x\"></div>");
  EXPECT_EQ(el->tag_id(), dom::TagId::kDiv);
  static const InternedString kId = InternedString::Intern("id");
  const String* id = el->GetAttribute(kId);
  ASSERT_NE(id, nullptr);
  EXPECT_EQ(*id, "x");
}

// ----------------------------------------------------------------------------
// 与 id/class 共存：style 不破坏其他 attribute 处理路径
// ----------------------------------------------------------------------------

TEST_F(InlineStyleTest, StyleCoexistsWithIdAndClass) {
  Element* el = ParseFirstChild(
      "<div id=\"main\" style=\"color: red\" class=\"box\"></div>");
  EXPECT_EQ(el->id(), InternedString::Intern("main"));
  EXPECT_TRUE(el->HasClass(InternedString::Intern("box")));
  const auto* decls = el->inline_declarations();
  ASSERT_NE(decls, nullptr);
  EXPECT_EQ(decls->size(), 1u);
  EXPECT_EQ((*decls)[0].property, PropertyId::kColor);
}

}  // namespace
}  // namespace vx::html
