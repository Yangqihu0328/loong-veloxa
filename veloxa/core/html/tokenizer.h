#ifndef VELOXA_CORE_HTML_TOKENIZER_H_
#define VELOXA_CORE_HTML_TOKENIZER_H_

#include "veloxa/core/dom/tag.h"
#include "veloxa/core/html/token.h"
#include "veloxa/foundation/strings/string.h"
#include "veloxa/foundation/strings/string_view.h"

namespace vx::html {

class Tokenizer {
 public:
  explicit Tokenizer(StringView input);

  Token Next();

  u32 line() const { return line_; }
  u32 column() const { return column_; }

 private:
  char Peek() const;
  char PeekAt(u32 offset) const;
  void Advance();
  bool AtEnd() const;
  bool StartsWith(StringView prefix) const;

  StringView ScanName();
  void SkipWhitespace();

  Token ScanTag();
  Token ScanStartTag();
  Token ScanEndTag();
  Token ScanAttribute();
  StringView ScanAttrValue();
  Token ScanCommentOrDoctype();
  Token ScanComment();
  Token ScanDoctype();
  Token ScanRawText();
  Token FlushText();

  StringView input_;
  u32 pos_ = 0;
  u32 text_start_ = 0;
  u32 line_ = 1;
  u32 column_ = 1;
  bool in_tag_ = false;
  bool has_entities_ = false;
  bool raw_text_pending_ = false;
  dom::TagId current_tag_ = dom::TagId::kUnknown;
};

String DecodeEntities(StringView raw);

}  // namespace vx::html

#endif  // VELOXA_CORE_HTML_TOKENIZER_H_
