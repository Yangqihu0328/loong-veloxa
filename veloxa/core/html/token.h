#ifndef VELOXA_CORE_HTML_TOKEN_H_
#define VELOXA_CORE_HTML_TOKEN_H_

#include "veloxa/foundation/base/types.h"
#include "veloxa/foundation/strings/string_view.h"

namespace vx::html {

enum class TokenType : u8 {
  kError,
  kDoctype,
  kStartTag,
  kEndTag,
  kAttribute,
  kTagEnd,
  kText,
  kComment,
  kEof,
};

struct Token {
  TokenType type = TokenType::kEof;
  StringView name;
  StringView value;
  bool self_closing = false;
  bool has_entities = false;

  Token() = default;
  explicit Token(TokenType t) : type(t) {}
};

}  // namespace vx::html

#endif  // VELOXA_CORE_HTML_TOKEN_H_
