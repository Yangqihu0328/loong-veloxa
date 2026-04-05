#ifndef VELOXA_CORE_CSS_PARSER_H_
#define VELOXA_CORE_CSS_PARSER_H_

#include "veloxa/core/css/selector.h"
#include "veloxa/core/css/tokenizer.h"

namespace vx::css {

class CssParser {
 public:
  static Stylesheet Parse(StringView css);
  static SmallVector<Declaration, 8> ParseDeclarationList(StringView css);

 private:
  explicit CssParser(StringView css);

  Stylesheet DoParse();
  StyleRule ParseRule();
  SmallVector<Selector, 1> ParseSelectorList();
  Selector ParseSelector();
  CompoundSelector ParseCompoundSelector();

  SmallVector<Declaration, 8> DoParseDeclarations();
  bool ParseDeclaration(SmallVector<Declaration, 8>& out);
  CssValue ParseValue(PropertyId prop);
  CssValue ParseLengthOrPercent();
  CssValue ParseColor();
  CssValue ParseEnumValue(PropertyId prop);

  void SkipWhitespace();
  CssToken NextNonWS();
  CssToken PeekNonWS();

  CssTokenizer tokenizer_;
};

}  // namespace vx::css
#endif  // VELOXA_CORE_CSS_PARSER_H_
