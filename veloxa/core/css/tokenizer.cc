#include "veloxa/core/css/tokenizer.h"

namespace vx::css {

CssTokenizer::CssTokenizer(StringView input) : input_(input) {}

char CssTokenizer::PeekChar() const {
  if (pos_ >= input_.size()) return '\0';
  return input_[pos_];
}

char CssTokenizer::PeekNext() const {
  if (pos_ + 1 >= input_.size()) return '\0';
  return input_[pos_ + 1];
}

char CssTokenizer::Advance() {
  char c = input_[pos_++];
  if (c == '\n') {
    ++line_;
    column_ = 1;
  } else {
    ++column_;
  }
  return c;
}

bool CssTokenizer::TrySkipComment() {
  if (pos_ + 1 < input_.size() && input_[pos_] == '/' &&
      input_[pos_ + 1] == '*') {
    Advance();  // '/'
    Advance();  // '*'
    while (pos_ < input_.size()) {
      if (input_[pos_] == '*' && pos_ + 1 < input_.size() &&
          input_[pos_ + 1] == '/') {
        Advance();  // '*'
        Advance();  // '/'
        return true;
      }
      Advance();
    }
    return true;
  }
  return false;
}

bool CssTokenizer::IsNameStart(char c) const {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool CssTokenizer::IsNameChar(char c) const {
  return IsNameStart(c) || IsDigit(c) || c == '-';
}

bool CssTokenizer::IsDigit(char c) const { return c >= '0' && c <= '9'; }

bool CssTokenizer::IsWhitespace(char c) const {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

StringView CssTokenizer::ScanName() {
  u32 start = pos_;
  while (pos_ < input_.size() && IsNameChar(input_[pos_])) {
    Advance();
  }
  return StringView(input_.data() + start, pos_ - start);
}

f32 CssTokenizer::ScanNumberValue() {
  f32 sign = 1.0f;
  if (PeekChar() == '-') {
    sign = -1.0f;
    Advance();
  } else if (PeekChar() == '+') {
    Advance();
  }

  f32 integer_part = 0.0f;
  while (pos_ < input_.size() && IsDigit(input_[pos_])) {
    integer_part = integer_part * 10.0f + static_cast<f32>(input_[pos_] - '0');
    Advance();
  }

  f32 fraction = 0.0f;
  if (pos_ < input_.size() && input_[pos_] == '.') {
    Advance();
    f32 divisor = 10.0f;
    while (pos_ < input_.size() && IsDigit(input_[pos_])) {
      fraction += static_cast<f32>(input_[pos_] - '0') / divisor;
      divisor *= 10.0f;
      Advance();
    }
  }

  return sign * (integer_part + fraction);
}

CssToken CssTokenizer::ScanWhitespace() {
  u32 start = pos_;
  while (pos_ < input_.size()) {
    if (IsWhitespace(input_[pos_])) {
      Advance();
    } else if (TrySkipComment()) {
      // continue
    } else {
      break;
    }
  }
  CssToken tok;
  tok.type = CssTokenType::kWhitespace;
  tok.value = StringView(input_.data() + start, pos_ - start);
  return tok;
}

CssToken CssTokenizer::ScanString(char quote) {
  Advance();  // opening quote
  u32 start = pos_;
  while (pos_ < input_.size() && input_[pos_] != quote) {
    if (input_[pos_] == '\\' && pos_ + 1 < input_.size()) {
      Advance();  // backslash
    }
    Advance();
  }
  u32 end = pos_;
  if (pos_ < input_.size()) {
    Advance();  // closing quote
  }
  CssToken tok;
  tok.type = CssTokenType::kString;
  tok.value = StringView(input_.data() + start, end - start);
  return tok;
}

CssToken CssTokenizer::ScanHash() {
  Advance();  // '#'
  CssToken tok;
  tok.type = CssTokenType::kHash;
  tok.value = ScanName();
  return tok;
}

CssToken CssTokenizer::ScanNumeric() {
  u32 start = pos_;
  f32 num = ScanNumberValue();

  CssToken tok;
  tok.number = num;
  tok.value = StringView(input_.data() + start, pos_ - start);

  if (pos_ < input_.size() && input_[pos_] == '%') {
    Advance();
    tok.type = CssTokenType::kPercentage;
  } else if (pos_ < input_.size() && IsNameStart(input_[pos_])) {
    tok.type = CssTokenType::kDimension;
    tok.unit = ScanName();
  } else {
    tok.type = CssTokenType::kNumber;
  }
  return tok;
}

CssToken CssTokenizer::ScanIdentLike() {
  StringView name = ScanName();
  CssToken tok;
  if (pos_ < input_.size() && input_[pos_] == '(') {
    Advance();  // '('
    tok.type = CssTokenType::kFunction;
  } else {
    tok.type = CssTokenType::kIdent;
  }
  tok.value = name;
  return tok;
}

CssToken CssTokenizer::ScanAtKeyword() {
  Advance();  // '@'
  CssToken tok;
  tok.type = CssTokenType::kAtKeyword;
  tok.value = ScanName();
  return tok;
}

CssToken CssTokenizer::Peek() {
  if (!has_peek_) {
    peek_token_ = Next();
    has_peek_ = true;
  }
  return peek_token_;
}

CssToken CssTokenizer::Next() {
  if (has_peek_) {
    has_peek_ = false;
    return peek_token_;
  }

  while (TrySkipComment()) {
  }

  if (pos_ >= input_.size()) {
    CssToken tok;
    tok.type = CssTokenType::kEof;
    return tok;
  }

  char c = PeekChar();

  if (IsWhitespace(c)) return ScanWhitespace();

  if (c == '"' || c == '\'') return ScanString(c);

  if (c == '#') {
    if (pos_ + 1 < input_.size() && IsNameChar(input_[pos_ + 1])) {
      return ScanHash();
    }
  }

  if (IsDigit(c) || (c == '.' && IsDigit(PeekNext()))) {
    return ScanNumeric();
  }

  if (c == '-') {
    char next = PeekNext();
    if (IsDigit(next) || next == '.') {
      return ScanNumeric();
    }
    if (IsNameStart(next) || next == '-') {
      return ScanIdentLike();
    }
    if (next == '-' && pos_ + 2 < input_.size() && input_[pos_ + 2] == '>') {
      Advance();  // '-'
      Advance();  // '-'
      Advance();  // '>'
      CssToken tok;
      tok.type = CssTokenType::kCDC;
      tok.value = StringView(input_.data() + pos_ - 3, 3);
      return tok;
    }
  }

  if (IsNameStart(c)) return ScanIdentLike();

  if (c == '@' && pos_ + 1 < input_.size() && IsNameStart(input_[pos_ + 1])) {
    return ScanAtKeyword();
  }

  Advance();
  CssToken tok;

  switch (c) {
    case ':':
      tok.type = CssTokenType::kColon;
      break;
    case ';':
      tok.type = CssTokenType::kSemicolon;
      break;
    case ',':
      tok.type = CssTokenType::kComma;
      break;
    case '{':
      tok.type = CssTokenType::kLeftBrace;
      break;
    case '}':
      tok.type = CssTokenType::kRightBrace;
      break;
    case '(':
      tok.type = CssTokenType::kLeftParen;
      break;
    case ')':
      tok.type = CssTokenType::kRightParen;
      break;
    case '[':
      tok.type = CssTokenType::kLeftBracket;
      break;
    case ']':
      tok.type = CssTokenType::kRightBracket;
      break;
    default:
      tok.type = CssTokenType::kDelim;
      tok.value = StringView(input_.data() + pos_ - 1, 1);
      break;
  }
  return tok;
}

}  // namespace vx::css
