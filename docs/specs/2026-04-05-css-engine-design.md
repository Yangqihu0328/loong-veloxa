# CSS 引擎设计规格

**任务 ID**：TASK-20260405-04
**日期**：2026-04-05
**复杂度**：Level 4
**代码规范**：Google C++ Style Guide

---

## 1. 概述

为 Veloxa 引擎构建 CSS 引擎，支持车载 HMI 常用 CSS 子集。包括 CSS Tokenizer、Parser、选择器匹配、属性系统、层叠与继承。

命名空间：`vx::css`，位于 `veloxa/core/css/` 目录。

## 2. CSS 属性子集（~45 个属性）

### 2.1 布局属性（20 个）

| 属性 | PropertyId | 可继承 |
|------|-----------|--------|
| display | kDisplay | 否 |
| position | kPosition | 否 |
| top, right, bottom, left | kTop/kRight/kBottom/kLeft | 否 |
| width, height | kWidth/kHeight | 否 |
| min-width, min-height | kMinWidth/kMinHeight | 否 |
| max-width, max-height | kMaxWidth/kMaxHeight | 否 |
| margin-top/right/bottom/left | kMarginTop/Right/Bottom/Left | 否 |
| padding-top/right/bottom/left | kPaddingTop/Right/Bottom/Left | 否 |
| box-sizing | kBoxSizing | 否 |
| overflow | kOverflow | 否 |
| z-index | kZIndex | 否 |

### 2.2 Flex 属性（9 个）

| 属性 | PropertyId | 可继承 |
|------|-----------|--------|
| flex-direction | kFlexDirection | 否 |
| flex-wrap | kFlexWrap | 否 |
| justify-content | kJustifyContent | 否 |
| align-items | kAlignItems | 否 |
| align-self | kAlignSelf | 否 |
| flex-grow | kFlexGrow | 否 |
| flex-shrink | kFlexShrink | 否 |
| flex-basis | kFlexBasis | 否 |
| gap | kGap | 否 |

### 2.3 视觉属性（8 个）

| 属性 | PropertyId | 可继承 |
|------|-----------|--------|
| background-color | kBackgroundColor | 否 |
| color | kColor | 是 |
| opacity | kOpacity | 否 |
| border-*-width (×4) | kBorderTopWidth 等 | 否 |
| border-*-style (×4) | kBorderTopStyle 等 | 否 |
| border-*-color (×4) | kBorderTopColor 等 | 否 |
| border-radius | kBorderRadius | 否 |
| visibility | kVisibility | 是 |

注：border 展开为 12 个子属性（4 方向 × width/style/color），加 border-radius 共 13 个独立 PropertyId。

### 2.4 文本属性（8 个，均可继承）

| 属性 | PropertyId | 可继承 |
|------|-----------|--------|
| font-family | kFontFamily | 是 |
| font-size | kFontSize | 是 |
| font-weight | kFontWeight | 是 |
| font-style | kFontStyle | 是 |
| line-height | kLineHeight | 是 |
| text-align | kTextAlign | 是 |
| white-space | kWhiteSpace | 是 |
| letter-spacing | kLetterSpacing | 是 |

## 3. CSS 值类型系统（A+C 混合）

### 3.1 解析阶段：CssValue（8 字节 tagged union）

```cpp
enum class Unit : u8 {
  kNone, kPx, kEm, kRem, kPercent, kVw, kVh, kAuto, kNumber,
};

enum class ValueType : u8 {
  kNone, kLength, kColor, kEnum, kNumber, kAuto, kInherit, kInitial,
};

struct CssValue {
  ValueType type = ValueType::kNone;
  Unit unit = Unit::kNone;
  union {
    f32 number;
    u32 color;
    u16 enum_value;
  };
};
```

### 3.2 计算阶段：ComputedStyle（直存具体类型）

ComputedStyle 中每个属性用具体类型存储：
- 枚举属性：`Display`, `Position`, `FlexDirection` 等 enum class (u8)
- 长度属性：`LengthValue { f32 value; Unit unit; }`
- 颜色属性：`u32` (RGBA32)
- 数字属性：`f32` (opacity, flex-grow 等)
- 字符串属性：`InternedString` (font-family)

## 4. CSS Tokenizer

### 4.1 Token 类型

遵循 CSS Syntax Module Level 3 的统一 token 模型：

```cpp
enum class CssTokenType : u8 {
  kEof, kError,
  kIdent, kHash, kString, kNumber, kDimension, kPercentage,
  kFunction, kAtKeyword, kDelim, kWhitespace,
  kColon, kSemicolon, kComma,
  kLeftBrace, kRightBrace, kLeftParen, kRightParen,
  kLeftBracket, kRightBracket,
  kCDO, kCDC,
};

struct CssToken {
  CssTokenType type;
  StringView value;
  f32 number;
  StringView unit;
};
```

### 4.2 Tokenizer 接口

```cpp
class CssTokenizer {
 public:
  explicit CssTokenizer(StringView input);
  CssToken Next();
  CssToken Peek();
  u32 line() const;
  u32 column() const;
};
```

单模式状态机，`Next()` 返回下一个 token。`Peek()` 支持一个 token 的预读。

## 5. 选择器系统

### 5.1 支持的选择器

| 选择器 | 语法 | 示例 |
|--------|------|------|
| 类型 | `tag` | `div` |
| 类 | `.class` | `.active` |
| ID | `#id` | `#panel` |
| 通配 | `*` | `*` |
| 后代 | `A B` | `div p` |
| 子代 | `A > B` | `div > p` |
| 属性存在 | `[attr]` | `[disabled]` |
| 属性等于 | `[attr=val]` | `[type=text]` |
| 伪类 | `:pseudo` | `:first-child`, `:last-child`, `:hover`, `:active`, `:focus` |

### 5.2 内存结构

```cpp
enum class SimpleSelectorType : u8 {
  kTag, kClass, kId, kUniversal, kAttribute, kPseudoClass,
};

struct SimpleSelector {
  SimpleSelectorType type;
  InternedString value;
  InternedString attr_value;  // [attr=val] 时的值
};

enum class Combinator : u8 {
  kNone, kDescendant, kChild,
};

struct CompoundSelector {
  SmallVector<SimpleSelector, 3> simple_selectors;
  Combinator combinator = Combinator::kNone;
};

struct Selector {
  SmallVector<CompoundSelector, 2> compounds;  // 从右到左存储
  u32 specificity = 0;  // (a << 16 | b << 8 | c)
};
```

### 5.3 匹配算法

从右到左匹配。SelectorMatcher::Matches() 先匹配最右 CompoundSelector，再沿 DOM 树向上验证组合子关系。

### 5.4 Specificity 计算

标准 CSS3：(a, b, c) 压缩为 u32：
- a = ID 选择器数量 × 65536
- b = class/属性/伪类数量 × 256
- c = 类型选择器数量 × 1

## 6. CSS Parser

### 6.1 职责

- 顶层：解析规则集列表 → `Stylesheet`
- 选择器：解析选择器组 → `Vector<Selector>`
- 声明：解析属性声明 → `Vector<Declaration>`
- 值：根据属性类型解析值 → `CssValue`
- 简写展开：`margin`, `padding`, `border`, `flex` → 多个子属性

### 6.2 数据结构

```cpp
struct Declaration {
  PropertyId property;
  CssValue value;
  bool important = false;
};

struct StyleRule {
  SmallVector<Selector, 1> selectors;
  SmallVector<Declaration, 8> declarations;
};

struct Stylesheet {
  Vector<StyleRule> rules;
};
```

### 6.3 接口

```cpp
class CssParser {
 public:
  static Stylesheet Parse(StringView css);
  static SmallVector<Declaration, 8> ParseDeclarationList(StringView css);
};
```

`ParseDeclarationList` 用于解析内联 style 属性。

## 7. 层叠与继承

### 7.1 StyleResolver

```cpp
class StyleResolver {
 public:
  static ComputedStyle Resolve(
      const dom::Element* element,
      const Stylesheet& stylesheet,
      const ComputedStyle* parent_style);
};
```

### 7.2 层叠优先级（低→高）

1. 内置默认样式
2. 样式表规则（按 specificity + 出现顺序）
3. 内联 style 属性
4. !important 声明

### 7.3 继承

对于标记为 `inherited=true` 的属性（color, font-*, line-height, text-align, white-space, letter-spacing, visibility），如果当前元素未设置，则从 parent_style 继承。

## 8. DOM 集成

### 8.1 Element 扩展

Element 新增 `id_` 和 `classes_` 缓存字段，由 HTML Parser 在解析时自动填充。

```cpp
class Element : public Node {
 public:
  InternedString id() const;
  void set_id(InternedString id);
  const SmallVector<InternedString, 2>& classes() const;
  void AddClass(InternedString cls);
  void RemoveClass(InternedString cls);
  bool HasClass(InternedString cls) const;
};
```

### 8.2 Document 扩展

Document 持有 Stylesheet 列表和 ComputedStyle 映射。

```cpp
class Document : public Element {
 public:
  void AddStylesheet(Stylesheet sheet);
  void ComputeStyles();
  const ComputedStyle* GetComputedStyle(const Element* el) const;
};
```

### 8.3 内联样式

`style="..."` 属性通过 `CssParser::ParseDeclarationList()` 解析，在层叠时作为最高优先级（仅次于 !important）。

## 9. 不在范围内

- transform / transition / animation（后续独立 TASK）
- @media / @import / @keyframes
- ::before / ::after 伪元素
- ~ (兄弟) / + (相邻兄弟) 组合子
- :nth-child() 等复杂伪类
- CSS 变量 (var())
- calc() 表达式
- Grid 布局
