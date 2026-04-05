# 创意设计：HTML Tokenizer 状态机

**日期：** 2026-04-05
**状态：** 已批准
**关联任务：** TASK-20260405-03

## 设计挑战

将 UTF-8 HTML 字符串（StringView）转换为 Token 流。需处理标签、属性、文本、注释、字符实体、自闭合、rawtext（script/style）。面向车载 HMI 场景，不追求完整 HTML5 规范。

## 选定方案：C — 混合方案（主循环 + 辅助方法）

### 核心设计

`Next()` 主循环处理顶层分支（文本 vs 标签 vs rawtext），复杂子逻辑提取为独立方法。

### Token 结构

```cpp
namespace vx::html {

enum class TokenType : u8 {
  kError,
  kDoctype,
  kStartTag,
  kEndTag,
  kAttribute,
  kTagEnd,       // '>' 或 '/>'
  kText,
  kComment,
  kEof,
};

struct Token {
  TokenType type;
  StringView name;
  StringView value;
  bool self_closing = false;
  bool has_entities = false;
};

}  // namespace vx::html
```

### Tokenizer 类

```cpp
class Tokenizer {
 public:
  explicit Tokenizer(StringView input);
  Token Next();
  u32 line() const;
  u32 column() const;

 private:
  // 基础操作
  char Peek() const;
  char PeekAt(u32 offset) const;
  void Advance();
  void AdvanceN(u32 n);
  bool AtEnd() const;
  bool StartsWith(StringView prefix) const;
  StringView ScanName();
  void SkipWhitespace();

  // 扫描方法
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

  // 状态
  StringView input_;
  u32 pos_ = 0;
  u32 text_start_ = 0;
  u32 line_ = 1;
  u32 column_ = 1;
  bool in_tag_ = false;
  bool has_entities_ = false;
  TagId current_tag_ = TagId::kUnknown;
};
```

### 状态转移

```
初始 → kData
kData:
  遇到 '<' → ScanTag()
    '</' → ScanEndTag() → kEndTag token
    '<!' → ScanCommentOrDoctype()
      '<!--' → ScanComment() → kComment token
      '<!DOCTYPE' → ScanDoctype() → kDoctype token
    '<alpha' → ScanStartTag() → kStartTag token, in_tag_ = true
  遇到 '&' → 标记 has_entities_ = true, 继续累积
  其他 → 累积文本, FlushText() 在遇到 '<' 或 EOF 时产出 kText

in_tag_ == true:
  Next() 调用 ScanAttribute() 或检测 '>'/'/>':
    '>' → kTagEnd token, in_tag_ = false
       如果 tag 是 script/style → 进入 rawtext 模式
    '/>' → kTagEnd token (self_closing), in_tag_ = false
    空白后字母 → ScanAttribute() → kAttribute token
    
rawtext 模式:
  ScanRawText() 扫描直到 '</script>' 或 '</style>'
  产出 kText token, 然后产出 kEndTag token
```

### 字符实体处理

Token 的 `has_entities` 标志延迟解码。Parser 在构建 Text/Attribute 时调用 `DecodeEntities(StringView)`:
- `&amp;` → `&`
- `&lt;` → `<`
- `&gt;` → `>`
- `&quot;` → `"`
- `&apos;` → `'`
- `&#NNN;` → UTF-8 码点
- `&#xHHHH;` → UTF-8 码点

### 行号追踪

`Advance()` 中检测 `\n`，递增 `line_`，重置 `column_`。所有字符消费都通过 `Advance()`，确保行号准确。

### 错误处理

- 非致命：遇到意外字符时产出 `kError` token 并跳过，继续扫描
- Tokenizer 不中止，Parser 决定是否收集错误或终止
