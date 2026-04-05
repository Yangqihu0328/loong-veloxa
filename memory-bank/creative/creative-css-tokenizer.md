# 创意设计：CSS Tokenizer 状态机

**日期：** 2026-04-05
**状态：** 已批准
**关联任务：** TASK-20260405-04

## 设计挑战

将 CSS 文本拆分为 ~20 种 token 类型。核心挑战：数字/维度/百分比区分、标识符与函数区分、注释/字符串/转义处理。

约束：零拷贝（StringView）、单遍扫描、与 HTML Tokenizer 风格一致。

## 探索的方案

### 方案 A：主分支 + 子扫描器（选定）
`Next()` 根据首字符分发到子扫描方法（ScanWhitespace/ScanString/ScanHash/ScanNumeric/ScanIdentLike 等）。注释在 `Next()` 内循环跳过，空白 token 保留给 Parser。

### 方案 B：表驱动分发
256 大小跳转表。C++ 不支持范围初始化，多字符前缀判断仍需回退 if 逻辑。不推荐。

### 方案 C：注释处理与空白分离
与方案 A 差异不大，注释分离策略被合并入方案 A。

## 选定方案

方案 A + C 注释分离：主分支子扫描器 + 注释内循环跳过 + 空白 token 保留。

## 实现指导

### Next() 主循环

```cpp
CssToken CssTokenizer::Next() {
  // 1. 跳过注释（循环，因为可能连续多个注释）
  while (TrySkipComment()) {}
  
  if (pos_ >= input_.size()) return {CssTokenType::kEof};
  
  char c = PeekChar();
  
  // 2. 空白保留（选择器中空白 = 后代组合子）
  if (IsWhitespace(c)) return ScanWhitespace();
  
  // 3. 字符串
  if (c == '"' || c == '\'') return ScanString(c);
  
  // 4. Hash（#id 或 #color）
  if (c == '#') return ScanHash();
  
  // 5. 数字（含负号和小数点开头）
  if (IsDigit(c) || (c == '.' && IsDigit(PeekNext())))
    return ScanNumeric();
  if (c == '-' && (IsDigit(PeekNext()) || PeekNext() == '.'))
    return ScanNumeric();
  
  // 6. 标识符/函数/关键字
  if (IsNameStart(c) || (c == '-' && IsNameStart(PeekNext())))
    return ScanIdentLike();
  
  // 7. At-keyword
  if (c == '@' && IsNameStart(PeekAt(pos_ + 1)))
    return ScanAtKeyword();
  
  // 8. 单字符标点
  // : ; , { } ( ) [ ]
  
  // 9. CDO/CDC
  // <!-- -->
  
  // 10. 其余字符 → kDelim
  return ScanDelim();
}
```

### 子扫描器职责

| 方法 | 产出 token | 逻辑 |
|------|-----------|------|
| ScanWhitespace() | kWhitespace | 消耗连续空白 |
| ScanString(quote) | kString | 至匹配引号，处理 `\` 转义 |
| ScanHash() | kHash | `#` + name chars |
| ScanNumeric() | kNumber / kDimension / kPercentage | 数字部分 → 检查后缀 |
| ScanIdentLike() | kIdent / kFunction | name → 后跟 `(` 则 kFunction |
| ScanAtKeyword() | kAtKeyword | `@` + name |

### `-` 歧义处理

`-` 可能是：
1. 负数开头：`-10px` → ScanNumeric
2. 标识符开头：`-webkit-foo` → ScanIdentLike
3. CDC 的一部分：`-->` → ScanCDC
4. 减号：单独的 `-` → kDelim

判断顺序：
1. `-` 后跟数字或 `.` → ScanNumeric
2. `-` 后跟 name start char → ScanIdentLike
3. `-->` → kCDC
4. 否则 → kDelim

### Peek 支持

```cpp
CssToken CssTokenizer::Peek() {
  if (!has_peek_) {
    peek_token_ = Next();
    has_peek_ = true;
  }
  return peek_token_;
}
```

Parser 在解析选择器时需要 Peek 来区分空白后跟 `{`（不是后代组合子）和空白后跟标识符（是后代组合子）。
