#ifndef VELOXA_CORE_HTML_PARSER_H_
#define VELOXA_CORE_HTML_PARSER_H_

#include "veloxa/core/dom/document.h"
#include "veloxa/core/html/tokenizer.h"
#include "veloxa/foundation/base/types.h"
#include "veloxa/foundation/containers/vector.h"

namespace vx::html {

// HTML inline style 安全护栏数值（spec §6 / TASK-20260426-01 R2 / #28）。
// 暴露于此供测试断言常量未被静默修改 — 这些是协议合同的一部分。
inline constexpr usize kInlineStyleMaxDeclarationCount = 1000;
inline constexpr usize kInlineStyleMaxValueLength = 8u * 1024u;  // 8 KB

namespace internal {
// ASCII case-insensitive 子串匹配 — 用于 inline style 黑名单检测。
// 暴露于此供 RED 反向探针直接测试（避免被 CssParser unknown-property 错误恢复
// 路径间接 over-match 而失去 blacklist 实证力）。生产路径仅由
// Parser::ApplyInlineStyleAttribute 调用。
bool ContainsBlacklistKeyword(StringView value);
}  // namespace internal

class Parser {
 public:
  static dom::Document* Parse(StringView html,
                              Vector<ParseError>* errors = nullptr);

 private:
  Parser(StringView html, Vector<ParseError>* errors);

  dom::Document* DoParse();
  void ProcessStartTag(const Token& token);
  void ProcessEndTag(const Token& token);
  void ProcessText(const Token& token);
  void ProcessComment(const Token& token);
  void HandleImplicitClose(dom::TagId new_tag);
  void CloseTopElement();
  dom::Element* CurrentElement();

  // 解析 HTML `style="..."` 属性为 inline declarations。安全护栏（spec §6 T1-T7）：
  //   - kMaxInlineValueLength：value 长度上限（DoS 防御）
  //   - kMaxInlineDeclarationCount：declaration 数上限（DoS 防御）
  //   - 黑名单关键字：拒绝历史攻击向量（IE expression / behavior / javascript:）
  // 命中任一安全护栏时整 attribute 静默丢弃，element 仍正常构建。
  void ApplyInlineStyleAttribute(dom::Element* el, StringView css);

  Tokenizer tokenizer_;
  dom::Document* doc_;
  Vector<dom::Element*> open_elements_;
  Vector<ParseError>* errors_;
};

}  // namespace vx::html

#endif  // VELOXA_CORE_HTML_PARSER_H_
