#include "veloxa/core/html/parser.h"

#include <gtest/gtest.h>

#include "veloxa/core/dom/comment.h"
#include "veloxa/core/dom/document.h"
#include "veloxa/core/dom/element.h"
#include "veloxa/core/dom/text.h"
#include "veloxa/foundation/strings/interned_string.h"

namespace vx::html {
namespace {

using dom::Comment;
using dom::Document;
using dom::Element;
using dom::NodeType;
using dom::TagId;
using dom::Text;

class ParserTest : public ::testing::Test {
 protected:
  void TearDown() override { delete doc_; }

  void Parse(StringView html) { doc_ = Parser::Parse(html); }

  Element* FirstChildElement() {
    auto* node = doc_->first_child();
    EXPECT_NE(node, nullptr);
    EXPECT_TRUE(node->is_element());
    return static_cast<Element*>(node);
  }

  Document* doc_ = nullptr;
};

TEST_F(ParserTest, EmptyDocument) {
  Parse("");
  ASSERT_NE(doc_, nullptr);
  EXPECT_EQ(doc_->type(), NodeType::kDocument);
  EXPECT_EQ(doc_->first_child(), nullptr);
}

TEST_F(ParserTest, SingleElement) {
  Parse("<div></div>");
  auto* div = FirstChildElement();
  EXPECT_EQ(div->tag_id(), TagId::kDiv);
  EXPECT_EQ(div->first_child(), nullptr);
}

TEST_F(ParserTest, NestedElements) {
  Parse("<div><span></span></div>");
  auto* div = FirstChildElement();
  EXPECT_EQ(div->tag_id(), TagId::kDiv);

  auto* span = static_cast<Element*>(div->first_child());
  ASSERT_NE(span, nullptr);
  EXPECT_EQ(span->tag_id(), TagId::kSpan);
}

TEST_F(ParserTest, TextContent) {
  Parse("<p>hello</p>");
  auto* p = FirstChildElement();
  EXPECT_EQ(p->tag_id(), TagId::kP);

  auto* text = static_cast<Text*>(p->first_child());
  ASSERT_NE(text, nullptr);
  EXPECT_EQ(text->type(), NodeType::kText);
  EXPECT_EQ(text->data(), "hello");
}

TEST_F(ParserTest, MultipleChildren) {
  Parse("<div><p></p><span></span></div>");
  auto* div = FirstChildElement();
  EXPECT_EQ(div->tag_id(), TagId::kDiv);
  EXPECT_EQ(div->child_count(), 2u);

  auto* p = static_cast<Element*>(div->first_child());
  EXPECT_EQ(p->tag_id(), TagId::kP);

  auto* span = static_cast<Element*>(p->next_sibling());
  ASSERT_NE(span, nullptr);
  EXPECT_EQ(span->tag_id(), TagId::kSpan);
}

TEST_F(ParserTest, ElementAttributes) {
  Parse(R"(<div id="main" class="box"></div>)");
  auto* div = FirstChildElement();
  EXPECT_EQ(div->tag_id(), TagId::kDiv);

  auto id_key = InternedString::Intern("id");
  auto class_key = InternedString::Intern("class");

  const String* id_val = div->GetAttribute(id_key);
  ASSERT_NE(id_val, nullptr);
  EXPECT_EQ(*id_val, "main");

  const String* class_val = div->GetAttribute(class_key);
  ASSERT_NE(class_val, nullptr);
  EXPECT_EQ(*class_val, "box");
}

TEST_F(ParserTest, VoidElement) {
  Parse("<br>");
  auto* br = FirstChildElement();
  EXPECT_EQ(br->tag_id(), TagId::kBr);
  EXPECT_EQ(br->first_child(), nullptr);
}

TEST_F(ParserTest, VoidImg) {
  Parse(R"(<img src="logo.png">)");
  auto* img = FirstChildElement();
  EXPECT_EQ(img->tag_id(), TagId::kImg);

  auto src_key = InternedString::Intern("src");
  const String* src_val = img->GetAttribute(src_key);
  ASSERT_NE(src_val, nullptr);
  EXPECT_EQ(*src_val, "logo.png");
  EXPECT_EQ(img->first_child(), nullptr);
}

TEST_F(ParserTest, SelfClosing) {
  Parse("<br/>");
  auto* br = FirstChildElement();
  EXPECT_EQ(br->tag_id(), TagId::kBr);
  EXPECT_EQ(br->first_child(), nullptr);
}

TEST_F(ParserTest, ImplicitCloseP) {
  Parse("<p>a<div>b</div>");
  // <p> should be implicitly closed when <div> (block) is encountered
  // Result: Document > p > Text("a"), div > Text("b")
  auto* p = static_cast<Element*>(doc_->first_child());
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(p->tag_id(), TagId::kP);

  auto* p_text = static_cast<Text*>(p->first_child());
  ASSERT_NE(p_text, nullptr);
  EXPECT_EQ(p_text->data(), "a");

  auto* div = static_cast<Element*>(p->next_sibling());
  ASSERT_NE(div, nullptr);
  EXPECT_EQ(div->tag_id(), TagId::kDiv);

  auto* div_text = static_cast<Text*>(div->first_child());
  ASSERT_NE(div_text, nullptr);
  EXPECT_EQ(div_text->data(), "b");
}

TEST_F(ParserTest, ImplicitCloseLi) {
  Parse("<ul><li>a<li>b</ul>");
  auto* ul = FirstChildElement();
  EXPECT_EQ(ul->tag_id(), TagId::kUl);
  EXPECT_EQ(ul->child_count(), 2u);

  auto* li1 = static_cast<Element*>(ul->first_child());
  EXPECT_EQ(li1->tag_id(), TagId::kLi);
  auto* text1 = static_cast<Text*>(li1->first_child());
  ASSERT_NE(text1, nullptr);
  EXPECT_EQ(text1->data(), "a");

  auto* li2 = static_cast<Element*>(li1->next_sibling());
  ASSERT_NE(li2, nullptr);
  EXPECT_EQ(li2->tag_id(), TagId::kLi);
  auto* text2 = static_cast<Text*>(li2->first_child());
  ASSERT_NE(text2, nullptr);
  EXPECT_EQ(text2->data(), "b");
}

TEST_F(ParserTest, ImplicitCloseTd) {
  Parse("<table><tr><td>a<td>b</tr></table>");
  auto* table = FirstChildElement();
  EXPECT_EQ(table->tag_id(), TagId::kTable);

  auto* tr = static_cast<Element*>(table->first_child());
  ASSERT_NE(tr, nullptr);
  EXPECT_EQ(tr->tag_id(), TagId::kTr);
  EXPECT_EQ(tr->child_count(), 2u);

  auto* td1 = static_cast<Element*>(tr->first_child());
  EXPECT_EQ(td1->tag_id(), TagId::kTd);

  auto* td2 = static_cast<Element*>(td1->next_sibling());
  ASSERT_NE(td2, nullptr);
  EXPECT_EQ(td2->tag_id(), TagId::kTd);
}

TEST_F(ParserTest, ImplicitCloseOption) {
  Parse("<select><option>a<option>b</select>");
  auto* select = FirstChildElement();
  EXPECT_EQ(select->tag_id(), TagId::kSelect);
  EXPECT_EQ(select->child_count(), 2u);

  auto* opt1 = static_cast<Element*>(select->first_child());
  EXPECT_EQ(opt1->tag_id(), TagId::kOption);

  auto* opt2 = static_cast<Element*>(opt1->next_sibling());
  ASSERT_NE(opt2, nullptr);
  EXPECT_EQ(opt2->tag_id(), TagId::kOption);
}

TEST_F(ParserTest, ImplicitCloseHead) {
  Parse("<html><head><title>T</title><body></body></html>");
  auto* html = FirstChildElement();
  EXPECT_EQ(html->tag_id(), TagId::kHtml);

  auto* head = static_cast<Element*>(html->first_child());
  ASSERT_NE(head, nullptr);
  EXPECT_EQ(head->tag_id(), TagId::kHead);

  auto* body = static_cast<Element*>(head->next_sibling());
  ASSERT_NE(body, nullptr);
  EXPECT_EQ(body->tag_id(), TagId::kBody);
}

TEST_F(ParserTest, CommentNode) {
  Parse("<!-- comment -->");
  auto* node = doc_->first_child();
  ASSERT_NE(node, nullptr);
  EXPECT_EQ(node->type(), NodeType::kComment);

  auto* comment = static_cast<Comment*>(node);
  EXPECT_EQ(comment->data(), " comment ");
}

TEST_F(ParserTest, MixedContent) {
  Parse("<p>text<em>bold</em>more</p>");
  auto* p = FirstChildElement();
  EXPECT_EQ(p->tag_id(), TagId::kP);
  EXPECT_EQ(p->child_count(), 3u);

  auto* text1 = static_cast<Text*>(p->first_child());
  EXPECT_EQ(text1->type(), NodeType::kText);
  EXPECT_EQ(text1->data(), "text");

  auto* em = static_cast<Element*>(text1->next_sibling());
  EXPECT_EQ(em->tag_id(), TagId::kEm);
  auto* bold_text = static_cast<Text*>(em->first_child());
  ASSERT_NE(bold_text, nullptr);
  EXPECT_EQ(bold_text->data(), "bold");

  auto* text2 = static_cast<Text*>(em->next_sibling());
  ASSERT_NE(text2, nullptr);
  EXPECT_EQ(text2->type(), NodeType::kText);
  EXPECT_EQ(text2->data(), "more");
}

TEST_F(ParserTest, EntityDecoding) {
  Parse("<p>&amp;</p>");
  auto* p = FirstChildElement();
  auto* text = static_cast<Text*>(p->first_child());
  ASSERT_NE(text, nullptr);
  EXPECT_EQ(text->data(), "&");
}

TEST_F(ParserTest, CompleteDocument) {
  Parse(
      "<html><head><title>Test</title></head>"
      "<body><div id=\"main\"><p>Hello</p></div></body></html>");

  auto* html = FirstChildElement();
  EXPECT_EQ(html->tag_id(), TagId::kHtml);

  auto* head = static_cast<Element*>(html->first_child());
  ASSERT_NE(head, nullptr);
  EXPECT_EQ(head->tag_id(), TagId::kHead);

  auto* title = static_cast<Element*>(head->first_child());
  ASSERT_NE(title, nullptr);
  EXPECT_EQ(title->tag_id(), TagId::kTitle);

  auto* body = static_cast<Element*>(head->next_sibling());
  ASSERT_NE(body, nullptr);
  EXPECT_EQ(body->tag_id(), TagId::kBody);

  auto* div = static_cast<Element*>(body->first_child());
  ASSERT_NE(div, nullptr);
  EXPECT_EQ(div->tag_id(), TagId::kDiv);

  auto id_key = InternedString::Intern("id");
  const String* id_val = div->GetAttribute(id_key);
  ASSERT_NE(id_val, nullptr);
  EXPECT_EQ(*id_val, "main");

  auto* p = static_cast<Element*>(div->first_child());
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(p->tag_id(), TagId::kP);

  auto* hello = static_cast<Text*>(p->first_child());
  ASSERT_NE(hello, nullptr);
  EXPECT_EQ(hello->data(), "Hello");
}

TEST_F(ParserTest, ScriptRawText) {
  Parse("<script>var x = 1 < 2;</script>");
  auto* script = FirstChildElement();
  EXPECT_EQ(script->tag_id(), TagId::kScript);

  auto* text = static_cast<Text*>(script->first_child());
  ASSERT_NE(text, nullptr);
  EXPECT_EQ(text->data(), "var x = 1 < 2;");
}

TEST_F(ParserTest, UnmatchedEndTag) {
  Parse("</div>");
  EXPECT_EQ(doc_->first_child(), nullptr);
}

TEST_F(ParserTest, ErrorCollectionNoErrors) {
  Vector<ParseError> errors;
  doc_ = Parser::Parse("<div><p>hello</p></div>", &errors);
  EXPECT_TRUE(errors.empty());
}

TEST_F(ParserTest, NullErrorsNoSegfault) {
  doc_ = Parser::Parse("<div></div>", nullptr);
  ASSERT_NE(doc_, nullptr);
}

TEST_F(ParserTest, ErrorCollectionDefaultParam) {
  doc_ = Parser::Parse("<div></div>");
  ASSERT_NE(doc_, nullptr);
}

}  // namespace
}  // namespace vx::html
