#include "veloxa/core/html/tokenizer.h"

namespace vx::html {

namespace {

bool IsAlpha(char ch) {
  return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

bool IsNameChar(char ch) {
  return IsAlpha(ch) || (ch >= '0' && ch <= '9') || ch == '-' || ch == '_';
}

bool IsWs(char ch) {
  return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
}

char ToLower(char ch) {
  if (ch >= 'A' && ch <= 'Z') return static_cast<char>(ch + 32);
  return ch;
}

void AppendCodepoint(String& out, u32 cp) {
  if (cp <= 0x7F) {
    out.append(static_cast<char>(cp));
  } else if (cp <= 0x7FF) {
    out.append(static_cast<char>(0xC0 | (cp >> 6)));
    out.append(static_cast<char>(0x80 | (cp & 0x3F)));
  } else if (cp <= 0xFFFF) {
    out.append(static_cast<char>(0xE0 | (cp >> 12)));
    out.append(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out.append(static_cast<char>(0x80 | (cp & 0x3F)));
  } else if (cp <= 0x10FFFF) {
    out.append(static_cast<char>(0xF0 | (cp >> 18)));
    out.append(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
    out.append(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out.append(static_cast<char>(0x80 | (cp & 0x3F)));
  }
}

}  // namespace

Tokenizer::Tokenizer(StringView input) : input_(input) {}

char Tokenizer::Peek() const {
  if (pos_ >= input_.size()) return '\0';
  return input_[pos_];
}

char Tokenizer::PeekAt(u32 offset) const {
  u32 idx = pos_ + offset;
  if (idx >= input_.size()) return '\0';
  return input_[idx];
}

void Tokenizer::Advance() {
  if (pos_ < input_.size()) {
    if (input_[pos_] == '\n') {
      ++line_;
      column_ = 1;
    } else {
      ++column_;
    }
    ++pos_;
  }
}

bool Tokenizer::AtEnd() const {
  return pos_ >= input_.size();
}

bool Tokenizer::StartsWith(StringView prefix) const {
  if (pos_ + prefix.size() > input_.size()) return false;
  for (usize i = 0; i < prefix.size(); ++i) {
    if (input_[pos_ + i] != prefix[i]) return false;
  }
  return true;
}

StringView Tokenizer::ScanName() {
  u32 start = pos_;
  while (!AtEnd() && IsNameChar(Peek())) {
    Advance();
  }
  return StringView(input_.data() + start, pos_ - start);
}

void Tokenizer::SkipWhitespace() {
  while (!AtEnd() && IsWs(Peek())) {
    Advance();
  }
}

Token Tokenizer::Next() {
  if (raw_text_pending_) {
    raw_text_pending_ = false;
    return ScanRawText();
  }

  if (in_tag_) {
    SkipWhitespace();
    if (AtEnd()) return Token(TokenType::kError);

    if (Peek() == '/') {
      if (PeekAt(1) == '>') {
        Advance();
        Advance();
        in_tag_ = false;
        text_start_ = pos_;
        has_entities_ = false;
        Token tok(TokenType::kTagEnd);
        tok.self_closing = true;
        return tok;
      }
    }

    if (Peek() == '>') {
      Advance();
      in_tag_ = false;
      text_start_ = pos_;
      has_entities_ = false;
      Token tok(TokenType::kTagEnd);
      if (dom::IsRawTextElement(current_tag_)) {
        raw_text_pending_ = true;
      }
      return tok;
    }

    return ScanAttribute();
  }

  while (!AtEnd()) {
    if (Peek() == '<') {
      if (pos_ > text_start_) {
        return FlushText();
      }
      return ScanTag();
    }
    if (Peek() == '&') {
      has_entities_ = true;
    }
    Advance();
  }

  if (pos_ > text_start_) {
    return FlushText();
  }
  return Token(TokenType::kEof);
}

Token Tokenizer::ScanTag() {
  Advance();  // past '<'

  if (Peek() == '/') {
    return ScanEndTag();
  }

  if (Peek() == '!') {
    Advance();  // past '!'
    return ScanCommentOrDoctype();
  }

  if (IsAlpha(Peek())) {
    return ScanStartTag();
  }

  return Token(TokenType::kError);
}

Token Tokenizer::ScanStartTag() {
  StringView name = ScanName();
  in_tag_ = true;
  current_tag_ = dom::TagIdFromName(name);
  Token tok(TokenType::kStartTag);
  tok.name = name;
  return tok;
}

Token Tokenizer::ScanEndTag() {
  Advance();  // past '/'
  StringView name = ScanName();
  while (!AtEnd() && Peek() != '>') {
    Advance();
  }
  if (!AtEnd()) Advance();  // past '>'
  in_tag_ = false;
  text_start_ = pos_;
  has_entities_ = false;
  Token tok(TokenType::kEndTag);
  tok.name = name;
  return tok;
}

Token Tokenizer::ScanAttribute() {
  StringView name = ScanName();
  if (name.empty()) {
    Advance();
    return Token(TokenType::kError);
  }

  SkipWhitespace();
  Token tok(TokenType::kAttribute);
  tok.name = name;

  if (!AtEnd() && Peek() == '=') {
    Advance();  // past '='
    SkipWhitespace();
    tok.value = ScanAttrValue();
    for (usize i = 0; i < tok.value.size(); ++i) {
      if (tok.value[i] == '&') {
        tok.has_entities = true;
        break;
      }
    }
  }
  return tok;
}

StringView Tokenizer::ScanAttrValue() {
  if (Peek() == '"' || Peek() == '\'') {
    char quote = Peek();
    Advance();  // past opening quote
    u32 start = pos_;
    while (!AtEnd() && Peek() != quote) {
      Advance();
    }
    StringView val(input_.data() + start, pos_ - start);
    if (!AtEnd()) Advance();  // past closing quote
    return val;
  }

  u32 start = pos_;
  while (!AtEnd() && Peek() != '>' && !IsWs(Peek())) {
    Advance();
  }
  return StringView(input_.data() + start, pos_ - start);
}

Token Tokenizer::ScanCommentOrDoctype() {
  if (Peek() == '-' && PeekAt(1) == '-') {
    return ScanComment();
  }

  StringView remaining(input_.data() + pos_,
                       input_.size() - pos_);
  if (remaining.size() >= 7) {
    bool is_doctype = true;
    const char* kDoctype = "DOCTYPE";
    for (int i = 0; i < 7; ++i) {
      if (ToLower(remaining[i]) != ToLower(kDoctype[i])) {
        is_doctype = false;
        break;
      }
    }
    if (is_doctype) return ScanDoctype();
  }

  return Token(TokenType::kError);
}

Token Tokenizer::ScanComment() {
  Advance();  // past first '-'
  Advance();  // past second '-'
  u32 start = pos_;

  while (!AtEnd()) {
    if (Peek() == '-' && PeekAt(1) == '-' && PeekAt(2) == '>') {
      Token tok(TokenType::kComment);
      tok.value = StringView(input_.data() + start, pos_ - start);
      Advance();  // '-'
      Advance();  // '-'
      Advance();  // '>'
      text_start_ = pos_;
      return tok;
    }
    Advance();
  }

  Token tok(TokenType::kComment);
  tok.value = StringView(input_.data() + start, pos_ - start);
  text_start_ = pos_;
  return tok;
}

Token Tokenizer::ScanDoctype() {
  for (int i = 0; i < 7; ++i) Advance();  // past "DOCTYPE"
  SkipWhitespace();
  u32 start = pos_;

  while (!AtEnd() && Peek() != '>') {
    Advance();
  }
  Token tok(TokenType::kDoctype);
  tok.value = StringView(input_.data() + start, pos_ - start);
  if (!AtEnd()) Advance();  // past '>'
  text_start_ = pos_;
  return tok;
}

Token Tokenizer::ScanRawText() {
  u32 start = pos_;
  const auto& info = dom::GetTagInfo(current_tag_);
  StringView tag_name(info.name);

  while (!AtEnd()) {
    if (Peek() == '<' && PeekAt(1) == '/') {
      bool match = true;
      u32 name_len = static_cast<u32>(tag_name.size());
      for (u32 i = 0; i < name_len; ++i) {
        u32 ci = pos_ + 2 + i;
        if (ci >= input_.size()) { match = false; break; }
        if (ToLower(input_[ci]) != tag_name[i]) { match = false; break; }
      }
      if (match) {
        u32 after = pos_ + 2 + name_len;
        if (after < input_.size() &&
            (input_[after] == '>' || IsWs(input_[after]))) {
          Token tok(TokenType::kText);
          tok.value = StringView(input_.data() + start, pos_ - start);
          text_start_ = pos_;
          return tok;
        }
      }
    }
    Advance();
  }

  Token tok(TokenType::kText);
  tok.value = StringView(input_.data() + start, pos_ - start);
  text_start_ = pos_;
  return tok;
}

Token Tokenizer::FlushText() {
  Token tok(TokenType::kText);
  tok.value = StringView(input_.data() + text_start_, pos_ - text_start_);
  tok.has_entities = has_entities_;
  text_start_ = pos_;
  has_entities_ = false;
  return tok;
}

// ---------------------------------------------------------------------------
// DecodeEntities
// ---------------------------------------------------------------------------

String DecodeEntities(StringView raw) {
  String result;
  result.reserve(raw.size());
  usize i = 0;

  while (i < raw.size()) {
    if (raw[i] == '&') {
      usize semi = raw.find(';', i + 1);
      if (semi != StringView::npos) {
        StringView entity = raw.substr(i + 1, semi - i - 1);

        if (entity == "amp") {
          result.append('&'); i = semi + 1; continue;
        }
        if (entity == "lt") {
          result.append('<'); i = semi + 1; continue;
        }
        if (entity == "gt") {
          result.append('>'); i = semi + 1; continue;
        }
        if (entity == "quot") {
          result.append('"'); i = semi + 1; continue;
        }
        if (entity == "apos") {
          result.append('\''); i = semi + 1; continue;
        }

        if (entity.size() >= 2 && entity[0] == '#') {
          u32 codepoint = 0;
          bool valid = true;
          if (entity[1] == 'x' || entity[1] == 'X') {
            if (entity.size() < 3) { valid = false; }
            for (usize j = 2; valid && j < entity.size(); ++j) {
              char ch = entity[j];
              if (ch >= '0' && ch <= '9') {
                codepoint = codepoint * 16 + static_cast<u32>(ch - '0');
              } else if (ch >= 'a' && ch <= 'f') {
                codepoint = codepoint * 16 + static_cast<u32>(ch - 'a' + 10);
              } else if (ch >= 'A' && ch <= 'F') {
                codepoint = codepoint * 16 + static_cast<u32>(ch - 'A' + 10);
              } else {
                valid = false;
              }
            }
          } else {
            for (usize j = 1; valid && j < entity.size(); ++j) {
              char ch = entity[j];
              if (ch >= '0' && ch <= '9') {
                codepoint = codepoint * 10 + static_cast<u32>(ch - '0');
              } else {
                valid = false;
              }
            }
          }
          if (valid && codepoint <= 0x10FFFF) {
            AppendCodepoint(result, codepoint);
            i = semi + 1;
            continue;
          }
        }
      }
    }
    result.append(raw[i]);
    ++i;
  }

  return result;
}

}  // namespace vx::html
