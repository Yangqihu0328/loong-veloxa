# CSS 引擎实现计划

**目标：** 构建 CSS Tokenizer、Parser、选择器匹配、属性系统、层叠/继承引擎，支持 ~45 个车载 HMI 核心属性

**架构：** 解析管线 (CSS text → CssTokenizer → CssParser → Stylesheet)，匹配管线 (Stylesheet + DOM → SelectorMatcher → StyleResolver → ComputedStyle)

**技术栈：** C++17, CMake, Google Test, Google C++ Style Guide

**复杂度级别：** Level 4

**设计规格：** `docs/specs/2026-04-05-css-engine-design.md`

---

## 文件结构

### 新建文件

| 文件 | 职责 |
|------|------|
| `veloxa/core/css/property.h` | PropertyId 枚举 + PropertyInfo 元数据表 |
| `veloxa/core/css/property.cc` | PropertyInfo 查找实现 |
| `veloxa/core/css/css_value.h` | Unit 枚举、ValueType、CssValue、LengthValue |
| `veloxa/core/css/enums.h` | Display, Position, FlexDirection 等 CSS 枚举 |
| `veloxa/core/css/tokenizer.h` | CssTokenizer 声明 |
| `veloxa/core/css/tokenizer.cc` | CssTokenizer 实现 |
| `veloxa/core/css/selector.h` | SimpleSelector, CompoundSelector, Selector |
| `veloxa/core/css/parser.h` | CssParser 声明 |
| `veloxa/core/css/parser.cc` | CssParser 实现（规则/选择器/声明/值解析） |
| `veloxa/core/css/selector_matcher.h` | SelectorMatcher 声明 |
| `veloxa/core/css/selector_matcher.cc` | SelectorMatcher 实现 |
| `veloxa/core/css/computed_style.h` | ComputedStyle 结构体 |
| `veloxa/core/css/style_resolver.h` | StyleResolver 声明 |
| `veloxa/core/css/style_resolver.cc` | StyleResolver 实现 |
| `tests/core/css/property_test.cc` | PropertyId 测试 |
| `tests/core/css/tokenizer_test.cc` | CSS Tokenizer 测试 |
| `tests/core/css/parser_test.cc` | CSS Parser 测试 |
| `tests/core/css/selector_matcher_test.cc` | 选择器匹配测试 |
| `tests/core/css/style_resolver_test.cc` | 层叠/继承测试 |
| `tests/core/css/integration_test.cc` | HTML→DOM→CSS→ComputedStyle 端到端测试 |

### 修改文件

| 文件 | 修改内容 |
|------|---------|
| `veloxa/core/dom/element.h` | [共享文件] 添加 id_/classes_ 缓存 + 访问方法 |
| `veloxa/core/dom/element.cc`（新建） | [共享文件] id/class 方法实现 |
| `veloxa/core/html/parser.cc` | [共享文件] ProcessStartTag 中填充 id/classes |
| `veloxa/core/CMakeLists.txt` | [共享文件] 添加 css/ 源文件 + element.cc |
| `tests/CMakeLists.txt` | [共享文件] 添加 css 测试目标 |

---

## Phase 1：属性系统与值类型 [TDD]

**预估时间：** 15 分钟
**文件：** `veloxa/core/css/property.h`, `property.cc`, `css_value.h`, `enums.h`
**测试：** `tests/core/css/property_test.cc`

### 1.1 css_value.h — 值类型定义

```cpp
// veloxa/core/css/css_value.h
#ifndef VELOXA_CORE_CSS_CSS_VALUE_H_
#define VELOXA_CORE_CSS_CSS_VALUE_H_

#include "veloxa/foundation/base/types.h"

namespace vx::css {

enum class Unit : u8 {
  kNone,
  kPx,
  kEm,
  kRem,
  kPercent,
  kVw,
  kVh,
  kAuto,
  kNumber,
};

enum class ValueType : u8 {
  kNone,
  kLength,
  kColor,
  kEnum,
  kNumber,
  kAuto,
  kInherit,
  kInitial,
};

struct CssValue {
  ValueType type = ValueType::kNone;
  Unit unit = Unit::kNone;
  union {
    f32 number;
    u32 color;
    u16 enum_value;
  };

  static CssValue None() { return {}; }

  static CssValue Length(f32 val, Unit u) {
    CssValue v;
    v.type = ValueType::kLength;
    v.unit = u;
    v.number = val;
    return v;
  }

  static CssValue Color(u32 rgba) {
    CssValue v;
    v.type = ValueType::kColor;
    v.color = rgba;
    return v;
  }

  static CssValue Enum(u16 e) {
    CssValue v;
    v.type = ValueType::kEnum;
    v.enum_value = e;
    return v;
  }

  static CssValue Number(f32 n) {
    CssValue v;
    v.type = ValueType::kNumber;
    v.number = n;
    return v;
  }

  static CssValue Auto() {
    CssValue v;
    v.type = ValueType::kAuto;
    v.unit = Unit::kAuto;
    return v;
  }

  static CssValue Inherit() {
    CssValue v;
    v.type = ValueType::kInherit;
    return v;
  }

  static CssValue Initial() {
    CssValue v;
    v.type = ValueType::kInitial;
    return v;
  }
};

struct LengthValue {
  f32 value = 0.0f;
  Unit unit = Unit::kNone;

  bool is_auto() const { return unit == Unit::kAuto; }
  bool is_none() const { return unit == Unit::kNone; }

  static LengthValue Px(f32 v) { return {v, Unit::kPx}; }
  static LengthValue Em(f32 v) { return {v, Unit::kEm}; }
  static LengthValue Percent(f32 v) { return {v, Unit::kPercent}; }
  static LengthValue Auto() { return {0.0f, Unit::kAuto}; }
};

}  // namespace vx::css

#endif  // VELOXA_CORE_CSS_CSS_VALUE_H_
```

### 1.2 enums.h — CSS 属性枚举值

```cpp
// veloxa/core/css/enums.h
#ifndef VELOXA_CORE_CSS_ENUMS_H_
#define VELOXA_CORE_CSS_ENUMS_H_

#include "veloxa/foundation/base/types.h"

namespace vx::css {

enum class Display : u8 {
  kNone, kBlock, kInline, kInlineBlock, kFlex,
};

enum class Position : u8 {
  kStatic, kRelative, kAbsolute, kFixed,
};

enum class FlexDirection : u8 {
  kRow, kRowReverse, kColumn, kColumnReverse,
};

enum class FlexWrap : u8 {
  kNowrap, kWrap,
};

enum class JustifyContent : u8 {
  kFlexStart, kFlexEnd, kCenter, kSpaceBetween, kSpaceAround,
};

enum class AlignItems : u8 {
  kAuto, kFlexStart, kFlexEnd, kCenter, kStretch, kBaseline,
};

enum class BoxSizing : u8 {
  kContentBox, kBorderBox,
};

enum class Overflow : u8 {
  kVisible, kHidden, kScroll, kAuto,
};

enum class Visibility : u8 {
  kVisible, kHidden,
};

enum class TextAlign : u8 {
  kLeft, kRight, kCenter, kJustify,
};

enum class WhiteSpace : u8 {
  kNormal, kNowrap, kPre, kPreWrap,
};

enum class BorderStyle : u8 {
  kNone, kSolid, kDashed, kDotted,
};

enum class CssFontStyle : u8 {
  kNormal, kItalic,
};

}  // namespace vx::css

#endif  // VELOXA_CORE_CSS_ENUMS_H_
```

### 1.3 property.h / property.cc — 属性 ID 和元数据

```cpp
// veloxa/core/css/property.h
#ifndef VELOXA_CORE_CSS_PROPERTY_H_
#define VELOXA_CORE_CSS_PROPERTY_H_

#include "veloxa/foundation/base/types.h"
#include "veloxa/foundation/strings/string_view.h"

namespace vx::css {

enum class PropertyId : u8 {
  kUnknown = 0,
  kDisplay, kPosition, kTop, kRight, kBottom, kLeft,
  kWidth, kHeight, kMinWidth, kMinHeight, kMaxWidth, kMaxHeight,
  kMarginTop, kMarginRight, kMarginBottom, kMarginLeft,
  kPaddingTop, kPaddingRight, kPaddingBottom, kPaddingLeft,
  kBoxSizing, kOverflow, kZIndex,
  kFlexDirection, kFlexWrap, kJustifyContent, kAlignItems, kAlignSelf,
  kFlexGrow, kFlexShrink, kFlexBasis, kGap,
  kBackgroundColor, kColor, kOpacity,
  kBorderTopWidth, kBorderRightWidth, kBorderBottomWidth, kBorderLeftWidth,
  kBorderTopStyle, kBorderRightStyle, kBorderBottomStyle, kBorderLeftStyle,
  kBorderTopColor, kBorderRightColor, kBorderBottomColor, kBorderLeftColor,
  kBorderRadius, kVisibility,
  kFontFamily, kFontSize, kFontWeight, kFontStyle,
  kLineHeight, kTextAlign, kWhiteSpace, kLetterSpacing,
  kMaxProperty,
};

struct PropertyInfo {
  PropertyId id;
  const char* name;
  bool inherited;
};

const PropertyInfo& GetPropertyInfo(PropertyId id);
PropertyId PropertyIdFromName(StringView name);
bool IsInheritedProperty(PropertyId id);

}  // namespace vx::css

#endif  // VELOXA_CORE_CSS_PROPERTY_H_
```

`property.cc` 实现 `kPropertyTable[]` 静态表（与 TagInfo 表模式一致），`PropertyIdFromName` 线性扫描（~50 项，与 TagIdFromName 一致）。

### 1.4 property_test.cc

测试覆盖：
- `PropertyIdFromName` 正确查找各属性（"display" → kDisplay 等）
- `PropertyIdFromName` 未知属性返回 kUnknown
- `GetPropertyInfo` O(1) 查找
- `IsInheritedProperty` 对 color/font-size 返回 true，对 display/margin 返回 false
- **预估 ~12 测试用例**

---

## Phase 2：CSS Tokenizer [TDD]

**预估时间：** 20 分钟
**文件：** `veloxa/core/css/tokenizer.h`, `tokenizer.cc`
**测试：** `tests/core/css/tokenizer_test.cc`
**依赖：** Phase 1（Unit 枚举）

### 2.1 tokenizer.h

```cpp
// veloxa/core/css/tokenizer.h
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
  u32 line() const;
  u32 column() const;

 private:
  char PeekChar() const;
  char Advance();
  void SkipWhitespace();
  bool SkipComment();
  CssToken ScanIdent();
  CssToken ScanNumber();
  CssToken ScanString(char quote);
  CssToken ScanHash();
  bool IsNameStart(char c) const;
  bool IsNameChar(char c) const;
  StringView ScanName();

  StringView input_;
  u32 pos_ = 0;
  u32 line_ = 1;
  u32 column_ = 1;
  bool has_peek_ = false;
  CssToken peek_token_;
};

}  // namespace vx::css

#endif  // VELOXA_CORE_CSS_TOKENIZER_H_
```

### 2.2 tokenizer.cc 核心逻辑

状态机主循环（`Next()`）：
1. 跳过注释（`/* ... */`）
2. 空白 → `kWhitespace`
3. `"` 或 `'` → `ScanString`
4. `#` → `ScanHash`（`kHash`）
5. 数字或 `.` 后跟数字 → `ScanNumber`（`kNumber`/`kDimension`/`kPercentage`）
6. 字母/`_`/`-` 开头 → `ScanIdent`（`kIdent`/`kFunction`）
7. `@` + 字母 → `kAtKeyword`
8. 单字符标点 → `kColon`/`kSemicolon`/`kComma`/`kLeftBrace`/`kRightBrace` 等
9. `<!--` → `kCDO`, `-->` → `kCDC`
10. 其余 → `kDelim`

### 2.3 tokenizer_test.cc

测试覆盖：
- 基本标点：`{ } ( ) [ ] : ; ,`
- 标识符：`div`, `font-size`, `-webkit-foo`
- 数字：`42`, `3.14`, `-10`
- 带单位：`10px`, `2em`, `50%`, `100vh`
- 字符串：`"hello"`, `'world'`
- Hash：`#id`, `#ff0000`
- 函数：`rgb(`
- 注释跳过：`/* comment */`
- 空白保留：选择器中空白有语义
- 行号跟踪
- 混合输入完整扫描
- **预估 ~22 测试用例**

---

## Phase 3：CSS Parser（选择器 + 声明 + 值） [TDD]

**预估时间：** 30 分钟
**文件：** `veloxa/core/css/selector.h`, `parser.h`, `parser.cc`
**测试：** `tests/core/css/parser_test.cc`
**依赖：** Phase 1 + Phase 2

### 3.1 selector.h

```cpp
// veloxa/core/css/selector.h
#ifndef VELOXA_CORE_CSS_SELECTOR_H_
#define VELOXA_CORE_CSS_SELECTOR_H_

#include "veloxa/core/css/css_value.h"
#include "veloxa/core/css/property.h"
#include "veloxa/foundation/containers/small_vector.h"
#include "veloxa/foundation/containers/vector.h"
#include "veloxa/foundation/strings/interned_string.h"

namespace vx::css {

enum class SimpleSelectorType : u8 {
  kTag, kClass, kId, kUniversal, kAttribute, kPseudoClass,
};

struct SimpleSelector {
  SimpleSelectorType type;
  InternedString value;
  InternedString attr_value;
};

enum class Combinator : u8 {
  kNone, kDescendant, kChild,
};

struct CompoundSelector {
  SmallVector<SimpleSelector, 3> simple_selectors;
  Combinator combinator = Combinator::kNone;
};

struct Selector {
  SmallVector<CompoundSelector, 2> compounds;
  u32 specificity = 0;
};

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

u32 ComputeSpecificity(const Selector& sel);

}  // namespace vx::css

#endif  // VELOXA_CORE_CSS_SELECTOR_H_
```

### 3.2 parser.h / parser.cc

```cpp
// veloxa/core/css/parser.h
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

  // 选择器解析
  SmallVector<Selector, 1> ParseSelectorList();
  Selector ParseSelector();
  CompoundSelector ParseCompoundSelector();
  SimpleSelector ParseSimpleSelector();

  // 声明解析
  SmallVector<Declaration, 8> ParseDeclarations();
  Declaration ParseDeclaration();
  CssValue ParseValue(PropertyId prop);
  CssValue ParseLengthOrPercent();
  CssValue ParseColor();

  // 简写展开
  void ExpandShorthand(PropertyId prop, const CssToken& token,
                       SmallVector<Declaration, 8>& out);
  void ExpandMarginPadding(PropertyId base,
                           SmallVector<Declaration, 8>& out);
  void ExpandBorder(SmallVector<Declaration, 8>& out);

  // 辅助
  bool Match(CssTokenType type);
  bool MatchIdent(StringView name);
  void SkipUntil(CssTokenType type);

  CssTokenizer tokenizer_;
};

}  // namespace vx::css

#endif  // VELOXA_CORE_CSS_PARSER_H_
```

Parser 核心逻辑：
- `DoParse()`: 循环解析 `StyleRule` 直到 EOF
- `ParseRule()`: 解析选择器组 + `{` + 声明列表 + `}`
- `ParseSelectorList()`: 逗号分隔的多个选择器
- `ParseSelector()`: 解析复合选择器链（从左到右解析，存储时反转为从右到左）
- `ParseCompoundSelector()`: 解析 `div.foo#bar:hover` → 多个 SimpleSelector
- `ParseDeclarations()`: 循环解析 `property: value;`
- `ParseValue()`: 根据属性类型调用 `ParseLengthOrPercent()` / `ParseColor()` / 枚举匹配
- 简写展开：`margin: 10px` → 4 个 margin-* 声明；`border: 1px solid red` → 12 个 border-* 声明

颜色解析支持：
- `#RGB` → 展开为 `#RRGGBB` + FF alpha
- `#RRGGBB` → 直接解析 + FF alpha
- `#RRGGBBAA` → 直接解析
- `rgb(r, g, b)` → 解析三参数
- `rgba(r, g, b, a)` → 解析四参数
- 命名颜色：`red`, `green`, `blue`, `white`, `black`, `transparent` 等 ~17 个 HMI 常用颜色

### 3.3 parser_test.cc

测试覆盖：
- 选择器解析：类型 / class / id / 通配 / 属性 / 伪类
- 复合选择器：`div.foo#bar`
- 组合子：`div > p`, `div p`（后代）
- 选择器列表：`div, p, .foo`
- Specificity 计算
- 声明解析：各属性值（长度、颜色、枚举）
- 颜色解析：#hex, rgb(), rgba(), 命名颜色
- 简写展开：margin, padding, border, flex
- !important 标记
- 完整规则解析
- 多规则样式表
- 内联声明列表（`ParseDeclarationList`）
- 错误恢复（畸形输入跳过）
- **预估 ~25 测试用例**

---

## Phase 4：选择器匹配 [TDD]

**预估时间：** 15 分钟
**文件：** `veloxa/core/css/selector_matcher.h`, `selector_matcher.cc`
**测试：** `tests/core/css/selector_matcher_test.cc`
**依赖：** Phase 3 + DOM 系统

### 4.1 selector_matcher.h / selector_matcher.cc

```cpp
// veloxa/core/css/selector_matcher.h
#ifndef VELOXA_CORE_CSS_SELECTOR_MATCHER_H_
#define VELOXA_CORE_CSS_SELECTOR_MATCHER_H_

#include "veloxa/core/css/selector.h"
#include "veloxa/core/dom/element.h"

namespace vx::css {

class SelectorMatcher {
 public:
  static bool Matches(const Selector& selector, const dom::Element* element);

 private:
  static bool MatchCompound(const CompoundSelector& compound,
                             const dom::Element* element);
  static bool MatchSimple(const SimpleSelector& simple,
                           const dom::Element* element);
};

}  // namespace vx::css

#endif  // VELOXA_CORE_CSS_SELECTOR_MATCHER_H_
```

匹配逻辑：
- `Matches()`: 从 compounds[0]（最右）开始，匹配当前元素，然后根据 combinator 向上遍历
- `MatchCompound()`: 所有 simple_selectors 必须全部匹配（AND 关系）
- `MatchSimple()`:
  - `kTag`: `element->tag_id() == TagIdFromName(value)`
  - `kClass`: `element->HasClass(value)`
  - `kId`: `element->id() == value`
  - `kUniversal`: 始终 true
  - `kAttribute`: `element->HasAttribute(value)` 或 `GetAttribute(value) == attr_value`
  - `kPseudoClass`: `:first-child` → 检查 `prev_sibling() == nullptr`; `:last-child` → 检查 `next_sibling() == nullptr`; `:hover/:active/:focus` → 检查元素状态标志（本次用 stub，返回 false）

### 4.2 selector_matcher_test.cc

测试覆盖：
- 类型选择器匹配：`div` 对 `<div>` / `<p>`
- Class 选择器匹配：`.foo` 对有/无 class 元素
- ID 选择器匹配：`#bar`
- 通配选择器：`*`
- 属性选择器：`[disabled]`, `[type=text]`
- 伪类：`:first-child`, `:last-child`
- 复合匹配：`div.foo`
- 后代匹配：`div p`（需构建 DOM 树）
- 子代匹配：`div > p`
- 不匹配场景
- **预估 ~15 测试用例**

---

## Phase 5：DOM 扩展（id/classes 缓存） [TDD]

**预估时间：** 10 分钟
**文件：** `veloxa/core/dom/element.h`（修改）, `veloxa/core/dom/element.cc`（新建）, `veloxa/core/html/parser.cc`（修改）
**测试：** `tests/core/dom/element_test.cc`（扩展）
**影响前序测试：** 现有 element_test 需保持通过

### 5.1 Element 新增字段和方法

在 `element.h` 的 `Element` 类中添加：

```cpp
// 新增 public 方法
InternedString id() const { return id_; }
void set_id(InternedString id) { id_ = id; }
const SmallVector<InternedString, 2>& classes() const { return classes_; }
void AddClass(InternedString cls);
void RemoveClass(InternedString cls);
bool HasClass(InternedString cls) const;

// 新增 private 字段
InternedString id_;
SmallVector<InternedString, 2> classes_;
```

### 5.2 element.cc

实现 `AddClass`, `RemoveClass`, `HasClass`（线性扫描 SmallVector，HMI 元素 class 数量少）。将 `AppendChild`/`RemoveChild` 从 header inline 移到 .cc。

### 5.3 HTML Parser 修改

在 `parser.cc` 的 `ProcessStartTag` 中，检查属性 token：
- `id` 属性 → `element->set_id(InternedString::Intern(value))`
- `class` 属性 → 按空格拆分，逐个 `element->AddClass(InternedString::Intern(part))`

### 5.4 element_test.cc 新增测试

- `AddClass` / `HasClass` / `RemoveClass`
- 多 class 操作
- id 设置和查询
- **预估 ~8 新增测试用例**

---

## Phase 6：ComputedStyle + StyleResolver [TDD]

**预估时间：** 20 分钟
**文件：** `veloxa/core/css/computed_style.h`, `style_resolver.h`, `style_resolver.cc`
**测试：** `tests/core/css/style_resolver_test.cc`
**依赖：** Phase 1-5

### 6.1 computed_style.h

```cpp
// veloxa/core/css/computed_style.h
#ifndef VELOXA_CORE_CSS_COMPUTED_STYLE_H_
#define VELOXA_CORE_CSS_COMPUTED_STYLE_H_

#include "veloxa/core/css/css_value.h"
#include "veloxa/core/css/enums.h"
#include "veloxa/foundation/strings/interned_string.h"

namespace vx::css {

struct ComputedStyle {
  // 布局
  Display display = Display::kBlock;
  Position position = Position::kStatic;
  LengthValue top, right, bottom, left;
  LengthValue width, height;
  LengthValue min_width, min_height, max_width, max_height;
  LengthValue margin_top, margin_right, margin_bottom, margin_left;
  LengthValue padding_top, padding_right, padding_bottom, padding_left;
  BoxSizing box_sizing = BoxSizing::kContentBox;
  Overflow overflow = Overflow::kVisible;
  i32 z_index = 0;

  // Flex
  FlexDirection flex_direction = FlexDirection::kRow;
  FlexWrap flex_wrap = FlexWrap::kNowrap;
  JustifyContent justify_content = JustifyContent::kFlexStart;
  AlignItems align_items = AlignItems::kStretch;
  AlignItems align_self = AlignItems::kAuto;
  f32 flex_grow = 0.0f;
  f32 flex_shrink = 1.0f;
  LengthValue flex_basis;
  LengthValue gap;

  // 视觉
  u32 background_color = 0x00000000;
  u32 color = 0x000000FFu;
  f32 opacity = 1.0f;
  LengthValue border_width[4];
  BorderStyle border_style[4] = {};
  u32 border_color[4] = {};
  LengthValue border_radius;
  Visibility visibility = Visibility::kVisible;

  // 文本（可继承）
  InternedString font_family;
  LengthValue font_size;
  u16 font_weight = 400;
  CssFontStyle font_style = CssFontStyle::kNormal;
  LengthValue line_height;
  TextAlign text_align = TextAlign::kLeft;
  WhiteSpace white_space = WhiteSpace::kNormal;
  LengthValue letter_spacing;
};

}  // namespace vx::css

#endif  // VELOXA_CORE_CSS_COMPUTED_STYLE_H_
```

### 6.2 style_resolver.h / style_resolver.cc

```cpp
// veloxa/core/css/style_resolver.h
#ifndef VELOXA_CORE_CSS_STYLE_RESOLVER_H_
#define VELOXA_CORE_CSS_STYLE_RESOLVER_H_

#include "veloxa/core/css/computed_style.h"
#include "veloxa/core/css/selector.h"
#include "veloxa/core/css/selector_matcher.h"
#include "veloxa/core/dom/element.h"

namespace vx::css {

class StyleResolver {
 public:
  static ComputedStyle Resolve(
      const dom::Element* element,
      const Vector<Stylesheet>& stylesheets,
      const ComputedStyle* parent_style,
      const SmallVector<Declaration, 8>* inline_decls = nullptr);

 private:
  struct MatchedRule {
    const StyleRule* rule;
    u32 specificity;
    u32 order;
  };

  static void CollectMatchingRules(
      const dom::Element* element,
      const Vector<Stylesheet>& stylesheets,
      Vector<MatchedRule>& matched);

  static void SortBySpecificity(Vector<MatchedRule>& rules);

  static void ApplyDeclaration(ComputedStyle& style,
                                const Declaration& decl);

  static void InheritProperties(ComputedStyle& style,
                                 const ComputedStyle* parent_style);
};

}  // namespace vx::css

#endif  // VELOXA_CORE_CSS_STYLE_RESOLVER_H_
```

StyleResolver 核心逻辑：
1. `CollectMatchingRules()`: 遍历所有 Stylesheet 的所有 StyleRule，对每个选择器调用 `SelectorMatcher::Matches()`，收集匹配的规则
2. `SortBySpecificity()`: 按 (important, specificity, order) 排序
3. 按排序顺序逐条 `ApplyDeclaration()` 到 ComputedStyle
4. 如果有 inline_decls，在样式表规则之后应用（更高优先级）
5. `InheritProperties()`: 对于未设置的可继承属性，从 parent_style 复制

`ApplyDeclaration()` 使用 switch(decl.property) 将 CssValue 转换为 ComputedStyle 的具体类型字段。

### 6.3 style_resolver_test.cc

测试覆盖：
- 单规则匹配并应用
- 多规则层叠（后声明覆盖先声明）
- Specificity 优先级：`div` < `.class` < `#id`
- !important 覆盖高 specificity
- 继承属性（color 从父元素继承）
- 非继承属性不继承
- `inherit` 关键字
- `initial` 关键字
- 内联样式优先级
- 默认值验证
- **预估 ~15 测试用例**

---

## Phase 7：集成测试 + CMake 收尾 [TDD]

**预估时间：** 15 分钟
**文件：** `tests/core/css/integration_test.cc`, `veloxa/core/CMakeLists.txt`, `tests/CMakeLists.txt`
**依赖：** Phase 1-6

### 7.1 CMakeLists.txt 更新

`veloxa/core/CMakeLists.txt` 添加 CSS 源文件：

```cmake
add_library(vx_core STATIC
  dom/tag.cc
  dom/node.cc
  dom/element.cc
  dom/serializer.cc
  html/tokenizer.cc
  html/parser.cc
  css/property.cc
  css/tokenizer.cc
  css/parser.cc
  css/selector_matcher.cc
  css/style_resolver.cc
)
target_include_directories(vx_core PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(vx_core PUBLIC vx_foundation)
```

`tests/CMakeLists.txt` 添加 CSS 测试：

```cmake
# Core: CSS
vx_add_test(css_property_test core/css/property_test.cc)
target_link_libraries(css_property_test PRIVATE vx_core)

vx_add_test(css_tokenizer_test core/css/tokenizer_test.cc)
target_link_libraries(css_tokenizer_test PRIVATE vx_core)

vx_add_test(css_parser_test core/css/parser_test.cc)
target_link_libraries(css_parser_test PRIVATE vx_core)

vx_add_test(css_selector_matcher_test core/css/selector_matcher_test.cc)
target_link_libraries(css_selector_matcher_test PRIVATE vx_core)

vx_add_test(css_style_resolver_test core/css/style_resolver_test.cc)
target_link_libraries(css_style_resolver_test PRIVATE vx_core)

vx_add_test(css_integration_test core/css/integration_test.cc)
target_link_libraries(css_integration_test PRIVATE vx_core)
```

### 7.2 integration_test.cc

端到端测试：
- HTML → Parser → DOM → CSS Parser → Stylesheet → StyleResolver → ComputedStyle
- 含 `<style>` 标签的完整 HMI 页面
- 内联 style 属性与外部样式表混合
- 多规则层叠验证
- 继承链验证（body → div → p）
- Flex 布局属性传递
- 典型车载 HMI 页面（仪表盘布局）
- **预估 ~12 测试用例**

### 7.3 最终验证

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

全部既有测试 + 新增测试通过。

---

## 并行化策略

| Phase | 可并行 | 子代理 |
|-------|--------|--------|
| Phase 1 (属性系统) | 独立 | 子代理 A |
| Phase 2 (Tokenizer) | 依赖 Phase 1 | 子代理 B（Phase 1 完成后） |
| Phase 3 (Parser) | 依赖 Phase 1+2 | 子代理 C |
| Phase 4 (SelectorMatcher) | 依赖 Phase 3+5 | 与 Phase 6 串行 |
| Phase 5 (DOM 扩展) | 独立于 CSS | 可与 Phase 2 并行 |
| Phase 6 (StyleResolver) | 依赖 Phase 4 | 子代理 D |
| Phase 7 (集成) | 依赖全部 | 主代理 |

推荐并行组：
- **组 1**：Phase 1 + Phase 5（属性系统 + DOM 扩展，互不依赖）
- **组 2**：Phase 2 + Phase 3（Tokenizer + Parser，串行但可合并为一个子代理）
- **组 3**：Phase 4 + Phase 6（匹配 + 层叠，串行但可合并）
- **组 4**：Phase 7（主代理整合）

---

## 预估汇总

| Phase | 预估时间 | 测试数 |
|-------|---------|--------|
| Phase 1 | 15 min | ~12 |
| Phase 2 | 20 min | ~22 |
| Phase 3 | 30 min | ~25 |
| Phase 4 | 15 min | ~15 |
| Phase 5 | 10 min | ~8 |
| Phase 6 | 20 min | ~15 |
| Phase 7 | 15 min | ~12 |
| **合计** | **~125 min** | **~109** |
