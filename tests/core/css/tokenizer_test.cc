#include "veloxa/core/css/tokenizer.h"

#include <cmath>

#include <gtest/gtest.h>

namespace vx::css {
namespace {

TEST(CssTokenizerTest, Empty) {
  CssTokenizer tok(StringView("", 0));
  EXPECT_EQ(tok.Next().type, CssTokenType::kEof);
}

TEST(CssTokenizerTest, Whitespace) {
  CssTokenizer tok(StringView("  \t\n"));
  auto t = tok.Next();
  EXPECT_EQ(t.type, CssTokenType::kWhitespace);
  EXPECT_EQ(tok.Next().type, CssTokenType::kEof);
}

TEST(CssTokenizerTest, Punctuation) {
  CssTokenizer tok(StringView("{}()[]"));
  EXPECT_EQ(tok.Next().type, CssTokenType::kLeftBrace);
  EXPECT_EQ(tok.Next().type, CssTokenType::kRightBrace);
  EXPECT_EQ(tok.Next().type, CssTokenType::kLeftParen);
  EXPECT_EQ(tok.Next().type, CssTokenType::kRightParen);
  EXPECT_EQ(tok.Next().type, CssTokenType::kLeftBracket);
  EXPECT_EQ(tok.Next().type, CssTokenType::kRightBracket);
  EXPECT_EQ(tok.Next().type, CssTokenType::kEof);
}

TEST(CssTokenizerTest, ColonSemicolonComma) {
  CssTokenizer tok(StringView(":;,"));
  EXPECT_EQ(tok.Next().type, CssTokenType::kColon);
  EXPECT_EQ(tok.Next().type, CssTokenType::kSemicolon);
  EXPECT_EQ(tok.Next().type, CssTokenType::kComma);
  EXPECT_EQ(tok.Next().type, CssTokenType::kEof);
}

TEST(CssTokenizerTest, Ident) {
  CssTokenizer tok(StringView("div"));
  auto t = tok.Next();
  EXPECT_EQ(t.type, CssTokenType::kIdent);
  EXPECT_EQ(t.value, StringView("div"));
}

TEST(CssTokenizerTest, IdentWithHyphen) {
  CssTokenizer tok(StringView("font-size"));
  auto t = tok.Next();
  EXPECT_EQ(t.type, CssTokenType::kIdent);
  EXPECT_EQ(t.value, StringView("font-size"));
}

TEST(CssTokenizerTest, IdentVendorPrefix) {
  CssTokenizer tok(StringView("-webkit-foo"));
  auto t = tok.Next();
  EXPECT_EQ(t.type, CssTokenType::kIdent);
  EXPECT_EQ(t.value, StringView("-webkit-foo"));
}

TEST(CssTokenizerTest, Number_Integer) {
  CssTokenizer tok(StringView("42"));
  auto t = tok.Next();
  EXPECT_EQ(t.type, CssTokenType::kNumber);
  EXPECT_FLOAT_EQ(t.number, 42.0f);
}

TEST(CssTokenizerTest, Number_Float) {
  CssTokenizer tok(StringView("3.14"));
  auto t = tok.Next();
  EXPECT_EQ(t.type, CssTokenType::kNumber);
  EXPECT_NEAR(t.number, 3.14f, 0.001f);
}

TEST(CssTokenizerTest, Number_Negative) {
  CssTokenizer tok(StringView("-10"));
  auto t = tok.Next();
  EXPECT_EQ(t.type, CssTokenType::kNumber);
  EXPECT_FLOAT_EQ(t.number, -10.0f);
}

TEST(CssTokenizerTest, Dimension_Px) {
  CssTokenizer tok(StringView("10px"));
  auto t = tok.Next();
  EXPECT_EQ(t.type, CssTokenType::kDimension);
  EXPECT_FLOAT_EQ(t.number, 10.0f);
  EXPECT_EQ(t.unit, StringView("px"));
}

TEST(CssTokenizerTest, Dimension_Em) {
  CssTokenizer tok(StringView("2em"));
  auto t = tok.Next();
  EXPECT_EQ(t.type, CssTokenType::kDimension);
  EXPECT_FLOAT_EQ(t.number, 2.0f);
  EXPECT_EQ(t.unit, StringView("em"));
}

TEST(CssTokenizerTest, Dimension_Vh) {
  CssTokenizer tok(StringView("100vh"));
  auto t = tok.Next();
  EXPECT_EQ(t.type, CssTokenType::kDimension);
  EXPECT_FLOAT_EQ(t.number, 100.0f);
  EXPECT_EQ(t.unit, StringView("vh"));
}

TEST(CssTokenizerTest, Percentage) {
  CssTokenizer tok(StringView("50%"));
  auto t = tok.Next();
  EXPECT_EQ(t.type, CssTokenType::kPercentage);
  EXPECT_FLOAT_EQ(t.number, 50.0f);
}

TEST(CssTokenizerTest, String_Double) {
  CssTokenizer tok(StringView("\"hello\""));
  auto t = tok.Next();
  EXPECT_EQ(t.type, CssTokenType::kString);
  EXPECT_EQ(t.value, StringView("hello"));
}

TEST(CssTokenizerTest, String_Single) {
  CssTokenizer tok(StringView("'world'"));
  auto t = tok.Next();
  EXPECT_EQ(t.type, CssTokenType::kString);
  EXPECT_EQ(t.value, StringView("world"));
}

TEST(CssTokenizerTest, Hash) {
  CssTokenizer tok(StringView("#id"));
  auto t = tok.Next();
  EXPECT_EQ(t.type, CssTokenType::kHash);
  EXPECT_EQ(t.value, StringView("id"));
}

TEST(CssTokenizerTest, Hash_Color) {
  CssTokenizer tok(StringView("#ff0000"));
  auto t = tok.Next();
  EXPECT_EQ(t.type, CssTokenType::kHash);
  EXPECT_EQ(t.value, StringView("ff0000"));
}

TEST(CssTokenizerTest, Function) {
  CssTokenizer tok(StringView("rgb("));
  auto t = tok.Next();
  EXPECT_EQ(t.type, CssTokenType::kFunction);
  EXPECT_EQ(t.value, StringView("rgb"));
}

TEST(CssTokenizerTest, AtKeyword) {
  CssTokenizer tok(StringView("@media"));
  auto t = tok.Next();
  EXPECT_EQ(t.type, CssTokenType::kAtKeyword);
  EXPECT_EQ(t.value, StringView("media"));
}

TEST(CssTokenizerTest, Comment_Skip) {
  CssTokenizer tok(StringView("a /* comment */ b"));
  auto t1 = tok.Next();
  EXPECT_EQ(t1.type, CssTokenType::kIdent);
  EXPECT_EQ(t1.value, StringView("a"));

  auto t2 = tok.Next();
  EXPECT_EQ(t2.type, CssTokenType::kWhitespace);

  auto t3 = tok.Next();
  EXPECT_EQ(t3.type, CssTokenType::kIdent);
  EXPECT_EQ(t3.value, StringView("b"));
}

TEST(CssTokenizerTest, LineTracking) {
  CssTokenizer tok(StringView("a\nb\nc"));
  tok.Next();  // 'a'
  tok.Next();  // whitespace '\n'
  tok.Next();  // 'b'
  tok.Next();  // whitespace '\n'
  EXPECT_EQ(tok.line(), 3u);
}

TEST(CssTokenizerTest, Delim) {
  CssTokenizer tok(StringView("*"));
  auto t = tok.Next();
  EXPECT_EQ(t.type, CssTokenType::kDelim);
  EXPECT_EQ(t.value, StringView("*"));
}

TEST(CssTokenizerTest, MixedInput) {
  CssTokenizer tok(StringView("div { color: red; }"));

  auto t1 = tok.Next();
  EXPECT_EQ(t1.type, CssTokenType::kIdent);
  EXPECT_EQ(t1.value, StringView("div"));

  EXPECT_EQ(tok.Next().type, CssTokenType::kWhitespace);

  EXPECT_EQ(tok.Next().type, CssTokenType::kLeftBrace);

  EXPECT_EQ(tok.Next().type, CssTokenType::kWhitespace);

  auto prop = tok.Next();
  EXPECT_EQ(prop.type, CssTokenType::kIdent);
  EXPECT_EQ(prop.value, StringView("color"));

  EXPECT_EQ(tok.Next().type, CssTokenType::kColon);

  EXPECT_EQ(tok.Next().type, CssTokenType::kWhitespace);

  auto val = tok.Next();
  EXPECT_EQ(val.type, CssTokenType::kIdent);
  EXPECT_EQ(val.value, StringView("red"));

  EXPECT_EQ(tok.Next().type, CssTokenType::kSemicolon);

  EXPECT_EQ(tok.Next().type, CssTokenType::kWhitespace);

  EXPECT_EQ(tok.Next().type, CssTokenType::kRightBrace);

  EXPECT_EQ(tok.Next().type, CssTokenType::kEof);
}

}  // namespace
}  // namespace vx::css
