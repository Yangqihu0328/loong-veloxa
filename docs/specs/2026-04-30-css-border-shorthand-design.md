# CSS border shorthand 补全（4 方向 + 3 属性级）设计规格

**任务 ID：** TASK-20260430-02
**复杂度级别：** Level 2（多文件修改 + 模式 100% 复用既有 `border` / `padding` 范本）
**安全相关：** ✅ 是
**日期：** 2026-04-30
**状态：** 设计阶段

---

## 1. 目的

补全 W3C CSS 2.1 §8 / §16 标准的 7 个 border shorthand：

| 类型 | shorthand | 1-4 值规则 | 展开成 |
|---|---|---|---|
| 方向（R1，4 个）| `border-top` / `border-right` / `border-bottom` / `border-left` | 单边 width/style/color 任意顺序（仿 `border`）| 3 longhand × 1 边 |
| 属性级（R2，3 个）| `border-width` / `border-style` / `border-color` | 1-4 值（仿 `padding` / `margin`）| 4 longhand × 1 type |

全部展开为既有 12 longhand，零新 PropertyId / 零 enum 改动。

## 2. 不做（明确范围排除）

- ❌ 不动 PropertyId 枚举（`property.h` / `property.cc`）— F4 实证零扩展需求
- ❌ 不动 longhand 解析路径（`border-top-width` 等通过 `PropertyIdFromName` 直查路径不变）
- ❌ 不动现有 `border` / `padding` / `margin` shorthand 实现
- ❌ 不补 CSS 4 标准的 `border-block` / `border-inline` 逻辑属性 shorthand（独立 P3 候选）
- ❌ 不补 `border-image` / `border-radius` 简写（独立 P3 候选）
- ❌ 不动上游 HTML inline style 三件套护栏（已通过本任务威胁建模验证完整覆盖）

## 3. 成功标准

| # | 判据 | 验证方式 |
|:-:|---|---|
| A1 | 4 方向 shorthand 全部双入口（`Parse` + `ParseDeclarationList`）解析正确 | R1 单测 PASS |
| A2 | 3 属性级 shorthand 1-4 值规则与 padding/margin 同模式 | R2 单测 PASS |
| A3 | 既有 `BorderShorthand` 测试不退化（互斥共存验证）| ctest 1039+ 全 PASS |
| A4 | DoS / 病态输入护栏：T6（per-shorthand 内部 token 上限）+ T8（4-value 上限）每分支独立验证 | 安全 N-cap 测试 PASS |
| A5 | §9.3 反向探针 ≥ 2 处（R1 + R2 各 1）| 临时 mis-route 实施 → 测试 FAIL → 恢复 PASS |
| A6 | Release `-O3 -Werror` 0 err/warn | `cmake --build build-bench -j` |
| A7 | ctest 全量 PASS（基线 1039 → 预计 1060+）| `cd build && ctest --output-on-failure` |
| A8 | TASK-30-01 升级规则 §0「CSS shorthand 能力 grep 表」自我应用首次外部任务 ROI 验证 | reflection §8 评估 |

## 4. 决策矩阵（D1-D5，已锁定）

| # | 维度 | 选择 | 理由 |
|:-:|---|---|---|
| D1 | 4 方向 shorthand 实现模式 | **A 复制粘贴** | 与现有 `border` / `padding` / `flex` / `transition` 风格一致；reviewer 心智成本最低；helper 抽取的 30 LOC 收益不抵风险（template/lambda/function pointer 都引入新模式） |
| D2 | 3 属性级 shorthand 实现模式 | **A 复制粘贴** | 同 D1 理由；3 个 value parser（Length / Enum / Color）类型异构，统一 helper 反而需要类型擦除 |
| D3 | 测试深度 | **C 完整档** | V5 安全 + D3 §9.3 反向探针强制；双入口验证防 ParseDeclarationList 路径退化；与既有 BorderShorthand 互斥不退化测试是 §0 fingerprint 自我应用 |
| D4 | 安全护栏复用策略 | **A 完全复用** | 既有 N-cap (3-iter / 4-iter) 已经过 TASK-26-01 R2 验证；上游 HTML inline style 三件套（count 1000 / value 8KB / 黑名单）覆盖威胁面；零新护栏需求 |
| D5 | R1/R2 Phase 划分粒度 | **A 2 GREEN commits** | R1（方向）4 shorthand 实施完全同模式可并发实施 1 commit；R2（属性级）3 shorthand 实施同模式 1 commit；与 V2 多轮次决策一致 |

## 5. 架构 + 接口签名

### 5.1 数据流（无改动）

```
CssParser::ParseDeclaration(out)
  ├─ name = NextNonWS()
  ├─ prop = PropertyIdFromName(name)            # longhand 直查 ✅
  ├─ if prop == kUnknown:
  │    ├─ if name == "margin" / "padding"     → Parse4SideValues + 4 longhand
  │    ├─ if name == "border"                 → Parse(width, style, color) + 12 longhand
  │    ├─ if name == "flex"                   → ...
  │    ├─ if name == "transition"             → ...
  │    └─ ★ 新增 7 分支（按字母序，与现有风格一致）：
  │         ├─ if name == "border-bottom"     → Parse(w,s,c) + 3 longhand (Bottom × 3)
  │         ├─ if name == "border-color"      → Parse4SideValues<Color> + 4 longhand
  │         ├─ if name == "border-left"       → Parse(w,s,c) + 3 longhand (Left × 3)
  │         ├─ if name == "border-right"      → Parse(w,s,c) + 3 longhand (Right × 3)
  │         ├─ if name == "border-style"      → Parse4SideValues<Enum> + 4 longhand
  │         ├─ if name == "border-top"        → Parse(w,s,c) + 3 longhand (Top × 3)
  │         └─ if name == "border-width"      → Parse4SideValues<Length> + 4 longhand
  └─ 正常 longhand 路径（无改动）
```

### 5.2 文件清单

| 操作 | 文件路径 | 估行数 | 说明 |
|---|---|---:|---|
| 修改 | `veloxa/core/css/parser.cc` | +~350 / -0 | 7 个新分支：R1 (4 directional × ~50 LOC) + R2 (3 property × ~50 LOC) |
| 修改 | `tests/core/css/parser_test.cc` | +~250 / -0 | ~21 新单测（D3 完整档：happy + edge + dual-entry + 反向探针 + 安全 N-cap） |
| 新建 | `docs/specs/2026-04-30-css-border-shorthand-design.md` | — | 本设计 |
| 新建 | `docs/plans/2026-04-30-css-border-shorthand.md` | — | 实现 plan |
| 修改 | `memory-bank/{tasks,activeContext,progress}.md` | — | 任务追踪 |

### 5.3 R1 方向 shorthand 实施伪码（4 分支同模式）

```cpp
// 在 parser.cc ParseDeclaration 内 if (prop == kUnknown) 块内添加：
if (EqualsIgnoreCase(name, "border-top")) {
  CssToken colon = NextNonWS();
  if (colon.type != CssTokenType::kColon) return false;

  CssValue width = CssValue::None();
  CssValue style = CssValue::None();
  CssValue color = CssValue::None();

  for (int i = 0; i < 3; ++i) {                    // T6 N-cap: 3 token 上限（与既有 border 一致）
    SkipWhitespace();
    CssToken peek = tokenizer_.Peek();
    if (peek.type == CssTokenType::kSemicolon ||
        peek.type == CssTokenType::kRightBrace ||
        peek.type == CssTokenType::kEof) break;
    if (peek.type == CssTokenType::kDelim && peek.value == "!") break;

    if (peek.type == CssTokenType::kDimension ||
        peek.type == CssTokenType::kNumber ||
        peek.type == CssTokenType::kPercentage) {
      width = ParseLengthOrPercent();
    } else if (peek.type == CssTokenType::kHash) {
      color = ParseColor();
    } else if (peek.type == CssTokenType::kFunction) {
      color = ParseColor();
    } else if (peek.type == CssTokenType::kIdent) {
      StringView val = peek.value;
      if (EqualsIgnoreCase(val, "solid") || EqualsIgnoreCase(val, "dashed") ||
          EqualsIgnoreCase(val, "dotted") || EqualsIgnoreCase(val, "none")) {
        tokenizer_.Next();
        BorderStyle bs = BorderStyle::kNone;
        if (EqualsIgnoreCase(val, "solid")) bs = BorderStyle::kSolid;
        else if (EqualsIgnoreCase(val, "dashed")) bs = BorderStyle::kDashed;
        else if (EqualsIgnoreCase(val, "dotted")) bs = BorderStyle::kDotted;
        style = CssValue::Enum(static_cast<u16>(bs));
      } else {
        color = ParseColor();
      }
    } else {
      break;
    }
  }

  bool important = ConsumeBangImportant();           // 复用既有 inline 模式
  ConsumeOptionalSemicolon();

  if (width.type != ValueType::kNone) {
    out.push_back({PropertyId::kBorderTopWidth, width, important});
  }
  if (style.type != ValueType::kNone) {
    out.push_back({PropertyId::kBorderTopStyle, style, important});
  }
  if (color.type != ValueType::kNone) {
    out.push_back({PropertyId::kBorderTopColor, color, important});
  }
  return true;
}
// border-right / border-bottom / border-left 同模式，仅 PropertyId 不同
```

**关键差异 vs 现有 `border`**：仅推 1 边 3 longhand（而非 4 边 12 longhand）。

### 5.4 R2 属性级 shorthand 实施伪码（3 分支异模式）

#### `border-width`（仿 `padding` / `margin` Length 模式）

```cpp
if (EqualsIgnoreCase(name, "border-width")) {
  CssToken colon = NextNonWS();
  if (colon.type != CssTokenType::kColon) return false;

  SmallVector<CssValue, 4> values;
  for (usize i = 0; i < 4; ++i) {                  // T8 N-cap: 4 value 上限（与既有 padding 一致）
    SkipWhitespace();
    CssToken peek = tokenizer_.Peek();
    if (peek.type == CssTokenType::kSemicolon ||
        peek.type == CssTokenType::kRightBrace ||
        peek.type == CssTokenType::kEof) break;
    if (peek.type == CssTokenType::kDelim && peek.value == "!") break;
    CssValue v = ParseLengthOrPercent();
    if (v.type == ValueType::kNone) break;
    values.push_back(v);
  }

  bool important = ConsumeBangImportant();
  ConsumeOptionalSemicolon();

  if (values.empty()) return false;

  CssValue top, right, bottom, left;
  if (values.size() == 1) { top = right = bottom = left = values[0]; }
  else if (values.size() == 2) { top = bottom = values[0]; right = left = values[1]; }
  else if (values.size() == 3) { top = values[0]; right = left = values[1]; bottom = values[2]; }
  else { top = values[0]; right = values[1]; bottom = values[2]; left = values[3]; }

  out.push_back({PropertyId::kBorderTopWidth, top, important});
  out.push_back({PropertyId::kBorderRightWidth, right, important});
  out.push_back({PropertyId::kBorderBottomWidth, bottom, important});
  out.push_back({PropertyId::kBorderLeftWidth, left, important});
  return true;
}
```

#### `border-style`（Enum 模式 — 仅 solid/dashed/dotted/none）

```cpp
if (EqualsIgnoreCase(name, "border-style")) {
  CssToken colon = NextNonWS();
  if (colon.type != CssTokenType::kColon) return false;

  SmallVector<CssValue, 4> values;
  for (usize i = 0; i < 4; ++i) {
    SkipWhitespace();
    CssToken peek = tokenizer_.Peek();
    if (peek.type == CssTokenType::kSemicolon ||
        peek.type == CssTokenType::kRightBrace ||
        peek.type == CssTokenType::kEof) break;
    if (peek.type == CssTokenType::kDelim && peek.value == "!") break;
    if (peek.type != CssTokenType::kIdent) break;

    StringView val = peek.value;
    BorderStyle bs;
    if (EqualsIgnoreCase(val, "solid")) bs = BorderStyle::kSolid;
    else if (EqualsIgnoreCase(val, "dashed")) bs = BorderStyle::kDashed;
    else if (EqualsIgnoreCase(val, "dotted")) bs = BorderStyle::kDotted;
    else if (EqualsIgnoreCase(val, "none")) bs = BorderStyle::kNone;
    else break;
    tokenizer_.Next();
    values.push_back(CssValue::Enum(static_cast<u16>(bs)));
  }

  bool important = ConsumeBangImportant();
  ConsumeOptionalSemicolon();

  if (values.empty()) return false;

  // 1-4 value 展开同模式，push 4 PropertyId::kBorder{Top,Right,Bottom,Left}Style
  // ...
}
```

#### `border-color`（Color 模式 — `ParseColor()`）

```cpp
if (EqualsIgnoreCase(name, "border-color")) {
  CssToken colon = NextNonWS();
  if (colon.type != CssTokenType::kColon) return false;

  SmallVector<CssValue, 4> values;
  for (usize i = 0; i < 4; ++i) {
    SkipWhitespace();
    CssToken peek = tokenizer_.Peek();
    if (peek.type == CssTokenType::kSemicolon ||
        peek.type == CssTokenType::kRightBrace ||
        peek.type == CssTokenType::kEof) break;
    if (peek.type == CssTokenType::kDelim && peek.value == "!") break;

    if (peek.type != CssTokenType::kHash &&
        peek.type != CssTokenType::kFunction &&
        peek.type != CssTokenType::kIdent) break;

    CssValue v = ParseColor();                      // ParseColor 处理 #rgb/rgb()/named-color
    if (v.type == ValueType::kNone) break;
    values.push_back(v);
  }

  bool important = ConsumeBangImportant();
  ConsumeOptionalSemicolon();

  if (values.empty()) return false;

  // 1-4 value 展开同模式，push 4 PropertyId::kBorder{Top,Right,Bottom,Left}Color
  // ...
}
```

**注意**：`ConsumeBangImportant()` / `ConsumeOptionalSemicolon()` 是伪码可读性提升 — 实际实施时**不抽 helper**（D1=A 决策），inline 复制 8 行即可。

### 5.5 插入位置（parser.cc ParseDeclaration 内）

按字母序插入到现有 shorthand 块内（保持代码可读性）：

| 既有顺序 | 新顺序（插入后） |
|---|---|
| 1. `margin` / `padding` | 1. `margin` / `padding` |
| 2. `border` | 2. `border` ★（保留首位，避免 4 方向 shorthand 串接 prefix-match 误绑 — 实际无误绑因 EqualsIgnoreCase 严格相等）|
| 3. `flex` | 3. **`border-bottom`** （新）|
| 4. `transition` | 4. **`border-color`** （新）|
| | 5. **`border-left`** （新）|
| | 6. **`border-right`** （新）|
| | 7. **`border-style`** （新）|
| | 8. **`border-top`** （新）|
| | 9. **`border-width`** （新）|
| | 10. `flex` |
| | 11. `transition` |

字母序遵循既有代码可读性约定。

## 6. 安全威胁建模（V5 安全相关，详见 systemPatterns.md「CSS shorthand parsing 威胁建模」段，未来归档时落盘）

### 6.1 威胁清单（T1-T8）

| # | 威胁 | 攻击载体 | 现有/新增护栏 | 验证方式 |
|:-:|---|---|---|---|
| T1 | DoS via 海量 declaration | inline style 含数千个 shorthand | ✅ 上游 `kInlineStyleMaxDeclarationCount = 1000`（`html/parser.h:13`，TASK-26-01 R2） | T1 不本任务测，依赖上游既测 |
| T2 | DoS via 单 value 巨长 | `style="border-width: 1234567...(8KB+)"` | ✅ 上游 `kInlineStyleMaxValueLength = 8KB` | 同 T1 |
| T3-T5 | 历史攻击向量（expression/behavior/javascript:） | 注入到 border-color hex 等 | ✅ 上游 `ContainsBlacklistKeyword`（`html/parser.cc:69`） | 同 T1 |
| T6 | parser 内部 token 循环耗尽 CPU（4 方向 shorthand） | `border-top: 1 1 1 1 1 1 1 1 1 1 1 ...` | ⚠️ **复用** `for (int i = 0; i < 3; ++i)` 上限 | 本任务专测：`BorderTopShorthand_NCapSecurity` |
| T7 | over-match 误绑（`border-top-width` 误进 shorthand 路径） | 实际 `border-top-width` 通过 `PropertyIdFromName` 直查，不进 shorthand 块 | ✅ 既有 `EqualsIgnoreCase` 严格相等天然防御 | 本任务专测：`BorderTopShorthand_LonghandFallthrough` |
| T8 | 4-value 解析下界绕过（`border-width: 1 2 3 4 5 6 ...`） | 跳过 4-iter cap | ⚠️ **复用** `for (usize i = 0; i < 4; ++i)` 上限 | 本任务专测：`BorderWidthShorthand_NCapSecurity` |

### 6.2 威胁面覆盖结论

- 上游护栏（HTML inline style 三件套）已完整覆盖 T1-T5
- 本任务新分支必须复用既有 N-cap 模式覆盖 T6 / T8
- T7 由既有 `EqualsIgnoreCase` 严格相等 + `PropertyIdFromName` 优先路径天然防御

**零新护栏需求**（D4=A 完全复用决策的依据）。

## 7. 测试策略（D3=C 完整档）

### 7.1 R1 测试清单（4 方向 × 多维度）

| # | 测试名 | 覆盖维度 | 期望 declaration 数 |
|:-:|---|---|:-:|
| 1 | `BorderTopShorthand_FullValue` | `border-top: 1px solid red` | 3（width+style+color）|
| 2 | `BorderRightShorthand_FullValue` | `border-right: 2px dashed blue` | 3 |
| 3 | `BorderBottomShorthand_FullValue` | `border-bottom: 3px dotted green` | 3 |
| 4 | `BorderLeftShorthand_FullValue` | `border-left: 4px solid #FF0000` | 3 |
| 5 | `BorderTopShorthand_PartialValue` | `border-top: 2px solid` （没 color）| 2 |
| 6 | `BorderTopShorthand_OutOfOrder` | `border-top: red 2px solid` | 3 |
| 7 | `BorderTopShorthand_Important` | `border-top: 1px solid red !important` | 3 with `important=true` |
| 8 | `BorderTopShorthand_DualEntry` | 通过 `ParseDeclarationList` 解析 | 3 |
| 9 | `BorderTopShorthand_NCapSecurity` (T6) | `border-top: 1px 2px 3px 4px solid red` | ≤ 3（cap 生效）|
| 10 | `BorderTopShorthand_LonghandFallthrough` (T7) | `border-top-width: 5px` 不走 shorthand | 1（`kBorderTopWidth`）|
| 11 | `ExistingBorderShorthand_NoDegradation` | `border: 1px solid red` 仍 12 declaration | 12 |
| 12 | `BorderTopShorthand_ReverseProbe` | 反向探针（临时 mis-route）| FAIL → restore PASS |

R1 总计 **12 测试**（含 §9.3 反向探针 1 处 + 安全 T6/T7 各 1）。

### 7.2 R2 测试清单（3 属性级 × 多维度）

| # | 测试名 | 覆盖维度 | 期望 declaration 数 |
|:-:|---|---|:-:|
| 1 | `BorderWidthShorthand_OneValue` | `border-width: 1px` → 4 边同值 | 4 |
| 2 | `BorderWidthShorthand_TwoValues` | `border-width: 1px 2px` → top/bottom=1 right/left=2 | 4 |
| 3 | `BorderWidthShorthand_ThreeValues` | `border-width: 1 2 3` | 4 |
| 4 | `BorderWidthShorthand_FourValues` | `border-width: 1 2 3 4` | 4 |
| 5 | `BorderStyleShorthand_OneValue` | `border-style: solid` → 4 边 | 4 |
| 6 | `BorderStyleShorthand_FourValues` | `border-style: solid dashed dotted none` | 4 |
| 7 | `BorderColorShorthand_OneValue` | `border-color: red` → 4 边 | 4 |
| 8 | `BorderColorShorthand_FourValues` | `border-color: red green blue #FFF` | 4 |
| 9 | `BorderWidthShorthand_DualEntry` | 通过 `ParseDeclarationList` 解析 | 4 |
| 10 | `BorderWidthShorthand_NCapSecurity` (T8) | `border-width: 1 2 3 4 5 6 7` | 4（cap 生效）|
| 11 | `BorderStyleShorthand_InvalidIdentRejected` | `border-style: foobar` → 0 declaration | 0 |
| 12 | `BorderColorShorthand_Important` | `border-color: red !important` | 4 with `important=true` |
| 13 | `BorderWidthShorthand_ReverseProbe` | 反向探针（临时 mis-route）| FAIL → restore PASS |

R2 总计 **13 测试**（含 §9.3 反向探针 1 处 + 安全 T8 + invalid ident 防御）。

### 7.3 测试总数：R1（12）+ R2（13）= **25 测试**（D3 完整档落点）

## 8. R1/R2 多轮次划分（V2 用户决策）

| Round | 内容 | 估时（plan）| × 0.6 | commit |
|:-:|---|---:|---:|---|
| P0 | 准备：grep 验证 + ctest 基线 + 反向探针位置注释 | 10 | 6 | 0（无代码改动）|
| R1.1 | RED：12 个 R1 单测落地 + 验证 FAIL | 30 | 18 | 1（`test(css): TASK-...02 R1 RED`）|
| R1.2 | GREEN：4 方向 shorthand 实施（parser.cc +~200 LOC）| 40 | 24 | 1（`feat(css): TASK-...02 R1 GREEN border-{top,right,bottom,left}`）|
| R1.3 | 反向探针验证 + ctest 全量 + 中间态报告 | 10 | 6 | 0（验证记录到 progress.md）|
| R2.1 | RED：13 个 R2 单测落地 + 验证 FAIL | 30 | 18 | 1（`test(css): TASK-...02 R2 RED`）|
| R2.2 | GREEN：3 属性级 shorthand 实施（parser.cc +~150 LOC）| 30 | 18 | 1（`feat(css): TASK-...02 R2 GREEN border-{width,style,color}`）|
| R2.3 | 反向探针验证 + ctest 全量 + 中间态报告 | 10 | 6 | 0 |
| P3 | finalize：Release `-O3 -Werror` + MB 同步 + 收尾 | 10 | 6 | 1（`chore(build): TASK-...02 finalize MB state`）|
| **合计** | — | **170 (~2.83h)** | **~102 (~1.7h)** | **5 commits** |

**plan × 0.6 第 15 数据点预期**：Level 2 单子系统 + 模式 100% 复用 + V5 安全 + 多轮次 → 落「准确档 0.5-0.6×」（~85-100 min 实测预期）。

## 9. 不需要 `/creative` 阶段

5 决策 D1-D5 已锁定（全部 A/A/C/A/A），无 UI/算法/架构空白；可直接 `/build`。

## 10. 风险登记

| # | 风险 | 概率 | 影响 | 缓解 |
|:-:|---|:-:|:-:|---|
| R1 | parser.cc 行号扩展导致 R3-R4 后续任务 grep fingerprint 偏移 | 低 | 低 | spec §5.5 字母序插入位置约束；行号变化可接受 |
| R2 | `EqualsIgnoreCase` 严格相等被绕过（如 trailing whitespace）| 极低 | 中 | 既有调用方 `name_tok.value` 来自 tokenizer 已 trim；不补充防御 |
| R3 | T6/T8 N-cap cap 设置过严遗漏合法用例 | 低 | 中 | 复用既有 `border` (3-iter) / `padding` (4-iter) cap，与 W3C 标准一致 |
| R4 | `BorderColorShorthand_FourValues` 解析 `red green blue #FFF` 混合 named + hex 是否需要 ident 主路径区分 | 中 | 低 | `ParseColor()` 既有实现已正确 dispatch named/hex/rgb()；编写测试时双验证 |
| R5 | R1 RED 阶段反向探针位置选错（测错 BorderTop 测错 BorderBottom mis-route）| 低 | 低 | spec §7.1 显式标注反向探针 mis-route 路径 |
| R6 | TASK-30-01 §0 升级规则首次外部验证 ROI 不及预期 | 低 | 低 | 接受为「不可证伪样本」，记入 reflection §8 |

## 11. 与既有任务关系

- **直接来源**：TASK-20260430-01 archive §改进建议「副发现 P3 触发型候选 — CSS parser `border-bottom` shorthand 缺失」
- **自我应用**：TASK-30-01 升级规则 `.cursor/rules/skills/writing-plans.mdc` §0「CSS shorthand 能力 grep 表」的首次外部任务 ROI 验证样板
- **不冲突**：与 TASK-26-02-full（clearance）/ TASK-26-03（IFC）/ TASK-24-02（layout super-linear）正交；本任务仅触 CSS parser 子系统

---

**设计人：** AI Agent
**设计日期：** 2026-04-30
**待用户审查后进入 `/build`**
