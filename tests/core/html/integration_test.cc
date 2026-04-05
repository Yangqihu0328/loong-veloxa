#include <string>

#include <gtest/gtest.h>

#include "veloxa/core/dom/document.h"
#include "veloxa/core/dom/serializer.h"
#include "veloxa/core/dom/text.h"
#include "veloxa/core/html/parser.h"

namespace vx {
namespace {

using dom::Document;
using dom::Serialize;
using dom::Text;

std::string S(const String& s) { return std::string(s.data(), s.size()); }

class IntegrationTest : public ::testing::Test {
 protected:
  void TearDown() override { delete doc_; }

  void Parse(StringView html) { doc_ = html::Parser::Parse(html); }

  Document* doc_ = nullptr;
};

TEST_F(IntegrationTest, ParseAndSerializeEmpty) {
  Parse("");
  String result = Serialize(doc_);
  EXPECT_EQ(S(result), "");
}

TEST_F(IntegrationTest, ParseAndSerializeSimpleDiv) {
  Parse("<div></div>");
  String result = Serialize(doc_);
  EXPECT_EQ(S(result), "<div></div>");
}

TEST_F(IntegrationTest, ParseAndSerializeWithText) {
  Parse("<p>hello</p>");
  String result = Serialize(doc_);
  EXPECT_EQ(S(result), "<p>hello</p>");
}

TEST_F(IntegrationTest, ParseAndSerializeNested) {
  Parse("<div><p>text</p></div>");
  String result = Serialize(doc_);
  EXPECT_EQ(S(result), "<div><p>text</p></div>");
}

TEST_F(IntegrationTest, ParseAndSerializeWithAttributes) {
  Parse(R"(<div id="main" class="box"></div>)");
  String result = Serialize(doc_);
  std::string s = S(result);
  EXPECT_NE(s.find("id=\"main\""), std::string::npos);
  EXPECT_NE(s.find("class=\"box\""), std::string::npos);
  EXPECT_NE(s.find("<div "), std::string::npos);
  EXPECT_NE(s.find("</div>"), std::string::npos);
}

TEST_F(IntegrationTest, ParseAndSerializeVoidElements) {
  Parse(R"(<br><hr><img src="x.png">)");
  String result = Serialize(doc_);
  std::string s = S(result);
  EXPECT_NE(s.find("<br>"), std::string::npos);
  EXPECT_NE(s.find("<hr>"), std::string::npos);
  EXPECT_NE(s.find("<img src=\"x.png\">"), std::string::npos);
  EXPECT_EQ(s.find("</br>"), std::string::npos);
  EXPECT_EQ(s.find("</hr>"), std::string::npos);
  EXPECT_EQ(s.find("</img>"), std::string::npos);
}

TEST_F(IntegrationTest, ParseAndSerializeComment) {
  Parse("<!-- hello -->");
  String result = Serialize(doc_);
  EXPECT_EQ(S(result), "<!-- hello -->");
}

TEST_F(IntegrationTest, EntityRoundTrip) {
  Parse("<p>&amp;&lt;&gt;</p>");

  auto* p = static_cast<dom::Element*>(doc_->first_child());
  ASSERT_NE(p, nullptr);
  auto* text = static_cast<Text*>(p->first_child());
  ASSERT_NE(text, nullptr);
  EXPECT_EQ(S(text->data()), "&<>");

  String result = Serialize(doc_);
  EXPECT_EQ(S(result), "<p>&amp;&lt;&gt;</p>");
}

TEST_F(IntegrationTest, ParseAndSerializeMixedContent) {
  Parse("<p>text<em>bold</em>more</p>");
  String result = Serialize(doc_);
  EXPECT_EQ(S(result), "<p>text<em>bold</em>more</p>");
}

TEST_F(IntegrationTest, ImplicitCloseInSerialize) {
  Parse("<ul><li>a<li>b</ul>");
  String result = Serialize(doc_);
  EXPECT_EQ(S(result), "<ul><li>a</li><li>b</li></ul>");
}

TEST_F(IntegrationTest, TypicalHmiPage) {
  const char* html =
      "<html><head><title>Dashboard</title></head>"
      "<body><nav><a>Home</a></nav>"
      "<main><div class=\"gauge\"><span>120</span>"
      "<span>km/h</span></div></main></body></html>";

  Parse(html);
  String first = Serialize(doc_);

  Document* doc2 = html::Parser::Parse(first.view());
  String second = Serialize(doc2);

  EXPECT_EQ(S(first), S(second));
  delete doc2;
}

TEST_F(IntegrationTest, DocumentOwnership) {
  Document* doc = html::Parser::Parse("<div><p>text</p></div>");
  ASSERT_NE(doc, nullptr);
  delete doc;
}

}  // namespace
}  // namespace vx
