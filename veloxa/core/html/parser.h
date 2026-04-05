#ifndef VELOXA_CORE_HTML_PARSER_H_
#define VELOXA_CORE_HTML_PARSER_H_

#include "veloxa/core/dom/document.h"
#include "veloxa/core/html/tokenizer.h"
#include "veloxa/foundation/containers/vector.h"

namespace vx::html {

class Parser {
 public:
  static dom::Document* Parse(StringView html);

 private:
  explicit Parser(StringView html);

  dom::Document* DoParse();
  void ProcessStartTag(const Token& token);
  void ProcessEndTag(const Token& token);
  void ProcessText(const Token& token);
  void ProcessComment(const Token& token);
  void HandleImplicitClose(dom::TagId new_tag);
  void CloseTopElement();
  dom::Element* CurrentElement();

  Tokenizer tokenizer_;
  dom::Document* doc_;
  Vector<dom::Element*> open_elements_;
};

}  // namespace vx::html

#endif  // VELOXA_CORE_HTML_PARSER_H_
