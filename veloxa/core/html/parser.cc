#include "veloxa/core/html/parser.h"

#include <utility>

namespace vx::html {

namespace {

struct ImplicitCloseRule {
  dom::TagId open_tag;
  dom::TagId trigger_tag;
  dom::TagType trigger_type;
  bool use_type;
};

static constexpr ImplicitCloseRule kImplicitCloseRules[] = {
    {dom::TagId::kP, dom::TagId::kUnknown, dom::TagType::kBlock, true},
    {dom::TagId::kLi, dom::TagId::kLi, dom::TagType::kInline, false},
    {dom::TagId::kDt, dom::TagId::kDt, dom::TagType::kInline, false},
    {dom::TagId::kDt, dom::TagId::kDd, dom::TagType::kInline, false},
    {dom::TagId::kDd, dom::TagId::kDd, dom::TagType::kInline, false},
    {dom::TagId::kDd, dom::TagId::kDt, dom::TagType::kInline, false},
    {dom::TagId::kTd, dom::TagId::kTd, dom::TagType::kInline, false},
    {dom::TagId::kTd, dom::TagId::kTh, dom::TagType::kInline, false},
    {dom::TagId::kTd, dom::TagId::kTr, dom::TagType::kInline, false},
    {dom::TagId::kTh, dom::TagId::kTd, dom::TagType::kInline, false},
    {dom::TagId::kTh, dom::TagId::kTh, dom::TagType::kInline, false},
    {dom::TagId::kTh, dom::TagId::kTr, dom::TagType::kInline, false},
    {dom::TagId::kTr, dom::TagId::kTr, dom::TagType::kInline, false},
    {dom::TagId::kThead, dom::TagId::kTbody, dom::TagType::kInline, false},
    {dom::TagId::kThead, dom::TagId::kTfoot, dom::TagType::kInline, false},
    {dom::TagId::kTbody, dom::TagId::kTbody, dom::TagType::kInline, false},
    {dom::TagId::kTbody, dom::TagId::kTfoot, dom::TagType::kInline, false},
    {dom::TagId::kTfoot, dom::TagId::kTbody, dom::TagType::kInline, false},
    {dom::TagId::kHead, dom::TagId::kBody, dom::TagType::kInline, false},
    {dom::TagId::kOption, dom::TagId::kOption, dom::TagType::kInline, false},
};

}  // namespace

dom::Document* Parser::Parse(StringView html, Vector<ParseError>* errors) {
  Parser parser(html, errors);
  return parser.DoParse();
}

Parser::Parser(StringView html, Vector<ParseError>* errors)
    : tokenizer_(html), doc_(nullptr), errors_(errors) {}

dom::Document* Parser::DoParse() {
  doc_ = new dom::Document();
  open_elements_.push_back(doc_);

  for (;;) {
    Token token = tokenizer_.Next();
    switch (token.type) {
      case TokenType::kStartTag:
        ProcessStartTag(token);
        break;
      case TokenType::kEndTag:
        ProcessEndTag(token);
        break;
      case TokenType::kText:
        ProcessText(token);
        break;
      case TokenType::kComment:
        ProcessComment(token);
        break;
      case TokenType::kDoctype:
        break;
      case TokenType::kEof:
        return doc_;
      case TokenType::kError:
        if (errors_) {
          errors_->push_back(
              ParseError{tokenizer_.line(), tokenizer_.column(), token.value});
        }
        break;
      case TokenType::kAttribute:
      case TokenType::kTagEnd:
        break;
    }
  }
}

void Parser::ProcessStartTag(const Token& start_token) {
  dom::TagId tag_id = dom::TagIdFromName(start_token.name);
  HandleImplicitClose(tag_id);

  dom::Element* el = doc_->CreateElement(tag_id);

  Token token = tokenizer_.Next();
  while (token.type == TokenType::kAttribute) {
    String value =
        token.has_entities ? DecodeEntities(token.value) : String(token.value);
    el->SetAttribute(InternedString::Intern(token.name), std::move(value));
    token = tokenizer_.Next();
  }
  // token is now kTagEnd

  static const InternedString kId = InternedString::Intern("id");
  static const InternedString kClass = InternedString::Intern("class");

  const String* id_val = el->GetAttribute(kId);
  if (id_val != nullptr && !id_val->empty()) {
    el->set_id(
        InternedString::Intern(StringView(id_val->data(), id_val->size())));
  }

  const String* class_val = el->GetAttribute(kClass);
  if (class_val != nullptr && !class_val->empty()) {
    StringView sv(class_val->data(), class_val->size());
    usize start = 0;
    while (start < sv.size()) {
      while (start < sv.size() &&
             (sv[start] == ' ' || sv[start] == '\t' || sv[start] == '\n'))
        ++start;
      if (start >= sv.size()) break;
      usize end = start;
      while (end < sv.size() && sv[end] != ' ' && sv[end] != '\t' &&
             sv[end] != '\n')
        ++end;
      el->AddClass(InternedString::Intern(sv.substr(start, end - start)));
      start = end;
    }
  }

  CurrentElement()->AppendChild(el);

  if (!dom::IsVoidElement(tag_id) && !token.self_closing) {
    open_elements_.push_back(el);
  }
}

void Parser::ProcessEndTag(const Token& token) {
  dom::TagId tag_id = dom::TagIdFromName(token.name);
  for (isize i = static_cast<isize>(open_elements_.size()) - 1; i >= 1; --i) {
    if (open_elements_[static_cast<usize>(i)]->tag_id() == tag_id) {
      open_elements_.resize(static_cast<usize>(i));
      break;
    }
  }
}

void Parser::ProcessText(const Token& token) {
  String text =
      token.has_entities ? DecodeEntities(token.value) : String(token.value);
  auto* t = doc_->CreateText(std::move(text));
  CurrentElement()->AppendChild(t);
}

void Parser::ProcessComment(const Token& token) {
  auto* c = doc_->CreateComment(String(token.value));
  CurrentElement()->AppendChild(c);
}

void Parser::HandleImplicitClose(dom::TagId new_tag) {
  const auto& new_info = dom::GetTagInfo(new_tag);

  for (;;) {
    if (open_elements_.size() <= 1) break;

    dom::Element* top = open_elements_.back();
    dom::TagId top_tag = top->tag_id();
    bool should_close = false;

    for (const auto& rule : kImplicitCloseRules) {
      if (rule.open_tag != top_tag) continue;
      if (rule.use_type) {
        if (new_info.type == rule.trigger_type) {
          should_close = true;
          break;
        }
      } else {
        if (new_tag == rule.trigger_tag) {
          should_close = true;
          break;
        }
      }
    }

    if (!should_close) break;
    CloseTopElement();
  }
}

void Parser::CloseTopElement() {
  if (open_elements_.size() > 1) {
    open_elements_.pop_back();
  }
}

dom::Element* Parser::CurrentElement() { return open_elements_.back(); }

}  // namespace vx::html
