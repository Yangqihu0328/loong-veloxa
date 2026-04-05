#include "veloxa/core/html/tokenizer.h"

#include <gtest/gtest.h>

namespace vx::html {
namespace {

TEST(TokenizerTest, EmptyInput) {
  Tokenizer t("");
  EXPECT_EQ(t.Next().type, TokenType::kEof);
}

TEST(TokenizerTest, PlainText) {
  Tokenizer t("hello");
  Token tok = t.Next();
  EXPECT_EQ(tok.type, TokenType::kText);
  EXPECT_EQ(tok.value, "hello");
  EXPECT_EQ(t.Next().type, TokenType::kEof);
}

TEST(TokenizerTest, SimpleTag) {
  Tokenizer t("<div></div>");
  Token tok = t.Next();
  EXPECT_EQ(tok.type, TokenType::kStartTag);
  EXPECT_EQ(tok.name, "div");

  tok = t.Next();
  EXPECT_EQ(tok.type, TokenType::kTagEnd);
  EXPECT_FALSE(tok.self_closing);

  tok = t.Next();
  EXPECT_EQ(tok.type, TokenType::kEndTag);
  EXPECT_EQ(tok.name, "div");

  EXPECT_EQ(t.Next().type, TokenType::kEof);
}

TEST(TokenizerTest, SelfClosingTag) {
  Tokenizer t("<br/>");
  Token tok = t.Next();
  EXPECT_EQ(tok.type, TokenType::kStartTag);
  EXPECT_EQ(tok.name, "br");

  tok = t.Next();
  EXPECT_EQ(tok.type, TokenType::kTagEnd);
  EXPECT_TRUE(tok.self_closing);

  EXPECT_EQ(t.Next().type, TokenType::kEof);
}

TEST(TokenizerTest, TagWithAttribute) {
  Tokenizer t(R"(<div id="main">)");
  Token tok = t.Next();
  EXPECT_EQ(tok.type, TokenType::kStartTag);
  EXPECT_EQ(tok.name, "div");

  tok = t.Next();
  EXPECT_EQ(tok.type, TokenType::kAttribute);
  EXPECT_EQ(tok.name, "id");
  EXPECT_EQ(tok.value, "main");

  tok = t.Next();
  EXPECT_EQ(tok.type, TokenType::kTagEnd);
  EXPECT_FALSE(tok.self_closing);

  EXPECT_EQ(t.Next().type, TokenType::kEof);
}

TEST(TokenizerTest, MultipleAttributes) {
  Tokenizer t(R"(<div id="main" class="container">)");
  EXPECT_EQ(t.Next().type, TokenType::kStartTag);

  Token tok = t.Next();
  EXPECT_EQ(tok.type, TokenType::kAttribute);
  EXPECT_EQ(tok.name, "id");
  EXPECT_EQ(tok.value, "main");

  tok = t.Next();
  EXPECT_EQ(tok.type, TokenType::kAttribute);
  EXPECT_EQ(tok.name, "class");
  EXPECT_EQ(tok.value, "container");

  EXPECT_EQ(t.Next().type, TokenType::kTagEnd);
  EXPECT_EQ(t.Next().type, TokenType::kEof);
}

TEST(TokenizerTest, UnquotedAttribute) {
  Tokenizer t("<div class=main>");
  EXPECT_EQ(t.Next().type, TokenType::kStartTag);

  Token tok = t.Next();
  EXPECT_EQ(tok.type, TokenType::kAttribute);
  EXPECT_EQ(tok.name, "class");
  EXPECT_EQ(tok.value, "main");

  EXPECT_EQ(t.Next().type, TokenType::kTagEnd);
}

TEST(TokenizerTest, BooleanAttribute) {
  Tokenizer t("<input disabled>");
  EXPECT_EQ(t.Next().type, TokenType::kStartTag);

  Token tok = t.Next();
  EXPECT_EQ(tok.type, TokenType::kAttribute);
  EXPECT_EQ(tok.name, "disabled");
  EXPECT_TRUE(tok.value.empty());

  EXPECT_EQ(t.Next().type, TokenType::kTagEnd);
}

TEST(TokenizerTest, TextBetweenTags) {
  Tokenizer t("<p>hello</p>");

  EXPECT_EQ(t.Next().type, TokenType::kStartTag);
  EXPECT_EQ(t.Next().type, TokenType::kTagEnd);

  Token tok = t.Next();
  EXPECT_EQ(tok.type, TokenType::kText);
  EXPECT_EQ(tok.value, "hello");

  tok = t.Next();
  EXPECT_EQ(tok.type, TokenType::kEndTag);
  EXPECT_EQ(tok.name, "p");

  EXPECT_EQ(t.Next().type, TokenType::kEof);
}

TEST(TokenizerTest, NestedTags) {
  Tokenizer t("<div><span>hi</span></div>");

  Token tok = t.Next();
  EXPECT_EQ(tok.type, TokenType::kStartTag);
  EXPECT_EQ(tok.name, "div");
  EXPECT_EQ(t.Next().type, TokenType::kTagEnd);

  tok = t.Next();
  EXPECT_EQ(tok.type, TokenType::kStartTag);
  EXPECT_EQ(tok.name, "span");
  EXPECT_EQ(t.Next().type, TokenType::kTagEnd);

  tok = t.Next();
  EXPECT_EQ(tok.type, TokenType::kText);
  EXPECT_EQ(tok.value, "hi");

  tok = t.Next();
  EXPECT_EQ(tok.type, TokenType::kEndTag);
  EXPECT_EQ(tok.name, "span");

  tok = t.Next();
  EXPECT_EQ(tok.type, TokenType::kEndTag);
  EXPECT_EQ(tok.name, "div");

  EXPECT_EQ(t.Next().type, TokenType::kEof);
}

TEST(TokenizerTest, Comment) {
  Tokenizer t("<!-- hello -->");
  Token tok = t.Next();
  EXPECT_EQ(tok.type, TokenType::kComment);
  EXPECT_EQ(tok.value, " hello ");
  EXPECT_EQ(t.Next().type, TokenType::kEof);
}

TEST(TokenizerTest, Doctype) {
  Tokenizer t("<!DOCTYPE html>");
  Token tok = t.Next();
  EXPECT_EQ(tok.type, TokenType::kDoctype);
  EXPECT_EQ(tok.value, "html");
  EXPECT_EQ(t.Next().type, TokenType::kEof);
}

TEST(TokenizerTest, VoidTag) {
  Tokenizer t("<img>");
  Token tok = t.Next();
  EXPECT_EQ(tok.type, TokenType::kStartTag);
  EXPECT_EQ(tok.name, "img");

  tok = t.Next();
  EXPECT_EQ(tok.type, TokenType::kTagEnd);
  EXPECT_FALSE(tok.self_closing);

  EXPECT_EQ(t.Next().type, TokenType::kEof);
}

TEST(TokenizerTest, RawTextScript) {
  Tokenizer t("<script>var x = 1;</script>");

  Token tok = t.Next();
  EXPECT_EQ(tok.type, TokenType::kStartTag);
  EXPECT_EQ(tok.name, "script");

  tok = t.Next();
  EXPECT_EQ(tok.type, TokenType::kTagEnd);

  tok = t.Next();
  EXPECT_EQ(tok.type, TokenType::kText);
  EXPECT_EQ(tok.value, "var x = 1;");

  tok = t.Next();
  EXPECT_EQ(tok.type, TokenType::kEndTag);
  EXPECT_EQ(tok.name, "script");

  EXPECT_EQ(t.Next().type, TokenType::kEof);
}

TEST(TokenizerTest, RawTextStyle) {
  Tokenizer t("<style>body { color: red; }</style>");

  EXPECT_EQ(t.Next().type, TokenType::kStartTag);
  EXPECT_EQ(t.Next().type, TokenType::kTagEnd);

  Token tok = t.Next();
  EXPECT_EQ(tok.type, TokenType::kText);
  EXPECT_EQ(tok.value, "body { color: red; }");

  tok = t.Next();
  EXPECT_EQ(tok.type, TokenType::kEndTag);
  EXPECT_EQ(tok.name, "style");

  EXPECT_EQ(t.Next().type, TokenType::kEof);
}

TEST(TokenizerTest, EntityInText) {
  Tokenizer t("a &amp; b");
  Token tok = t.Next();
  EXPECT_EQ(tok.type, TokenType::kText);
  EXPECT_EQ(tok.value, "a &amp; b");
  EXPECT_TRUE(tok.has_entities);
}

TEST(TokenizerTest, EntityInAttribute) {
  Tokenizer t(R"(<a href="x&amp;y">)");
  EXPECT_EQ(t.Next().type, TokenType::kStartTag);

  Token tok = t.Next();
  EXPECT_EQ(tok.type, TokenType::kAttribute);
  EXPECT_EQ(tok.name, "href");
  EXPECT_EQ(tok.value, "x&amp;y");
  EXPECT_TRUE(tok.has_entities);

  EXPECT_EQ(t.Next().type, TokenType::kTagEnd);
}

TEST(TokenizerTest, LineTracking) {
  Tokenizer t("ab\ncd\nef");
  EXPECT_EQ(t.line(), 1u);
  EXPECT_EQ(t.column(), 1u);

  t.Next();  // consumes full text "ab\ncd\nef"
  EXPECT_EQ(t.line(), 3u);
  EXPECT_EQ(t.column(), 3u);
}

TEST(TokenizerTest, SingleQuotedAttribute) {
  Tokenizer t("<div id='main'>");
  EXPECT_EQ(t.Next().type, TokenType::kStartTag);

  Token tok = t.Next();
  EXPECT_EQ(tok.type, TokenType::kAttribute);
  EXPECT_EQ(tok.name, "id");
  EXPECT_EQ(tok.value, "main");

  EXPECT_EQ(t.Next().type, TokenType::kTagEnd);
}

TEST(TokenizerTest, WhitespaceAroundEquals) {
  Tokenizer t(R"(<div id = "main">)");
  EXPECT_EQ(t.Next().type, TokenType::kStartTag);

  Token tok = t.Next();
  EXPECT_EQ(tok.type, TokenType::kAttribute);
  EXPECT_EQ(tok.name, "id");
  EXPECT_EQ(tok.value, "main");

  EXPECT_EQ(t.Next().type, TokenType::kTagEnd);
}

TEST(TokenizerTest, DecodeEntitiesNamed) {
  String r = DecodeEntities("&amp; &lt; &gt; &quot; &apos;");
  EXPECT_EQ(r, "& < > \" '");
}

TEST(TokenizerTest, DecodeEntitiesDecimal) {
  String r = DecodeEntities("&#65;");
  EXPECT_EQ(r, "A");
}

TEST(TokenizerTest, DecodeEntitiesHex) {
  String r = DecodeEntities("&#x41;");
  EXPECT_EQ(r, "A");
}

TEST(TokenizerTest, DecodeEntitiesMultibyte) {
  String r = DecodeEntities("&#x4e2d;");
  EXPECT_EQ(r.view(), StringView("\xe4\xb8\xad", 3));
}

TEST(TokenizerTest, DecodeEntitiesNoEntity) {
  String r = DecodeEntities("hello world");
  EXPECT_EQ(r, "hello world");
}

}  // namespace
}  // namespace vx::html
