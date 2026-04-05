#ifndef VELOXA_CORE_CSS_TOKENIZER_H_
#define VELOXA_CORE_CSS_TOKENIZER_H_

#include "veloxa/foundation/base/types.h"
#include "veloxa/foundation/strings/string_view.h"

namespace vx::css {

enum class CssTokenType : u8 {
  kEof,
  kError,
  kIdent,
  kHash,
  kString,
  kNumber,
  kDimension,
  kPercentage,
  kFunction,
  kAtKeyword,
  kDelim,
  kWhitespace,
  kColon,
  kSemicolon,
  kComma,
  kLeftBrace,
  kRightBrace,
  kLeftParen,
  kRightParen,
  kLeftBracket,
  kRightBracket,
  kCDO,
  kCDC,
};

struct CssToken {
  CssTokenType type = CssTokenType::kEof;
  StringView value;
  f32 number = 0.0f;
  StringView unit;
};

class CssTokenizer {
 public:
  explicit CssTokenizer(StringView input);
  CssToken Next();
  CssToken Peek();
  u32 line() const { return line_; }
  u32 column() const { return column_; }

 private:
  char PeekChar() const;
  char PeekNext() const;
  char Advance();
  bool TrySkipComment();
  CssToken ScanWhitespace();
  CssToken ScanString(char quote);
  CssToken ScanHash();
  CssToken ScanNumeric();
  CssToken ScanIdentLike();
  CssToken ScanAtKeyword();
  bool IsNameStart(char c) const;
  bool IsNameChar(char c) const;
  bool IsDigit(char c) const;
  bool IsWhitespace(char c) const;
  StringView ScanName();
  f32 ScanNumberValue();

  StringView input_;
  u32 pos_ = 0;
  u32 line_ = 1;
  u32 column_ = 1;
  bool has_peek_ = false;
  CssToken peek_token_;
};

}  // namespace vx::css
#endif  // VELOXA_CORE_CSS_TOKENIZER_H_
