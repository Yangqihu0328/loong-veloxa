# 设计规格：Layout 正确性消化（#25 + #28 + #20 + #21）

- **任务 ID：** TASK-20260426-01
- **复杂度：** Level 4（多子系统 + 架构决策；#20/#21 单独 Level 3 量级）
- **创建日期：** 2026-04-26
- **拆分策略：** D1 全包 + 多轮次 Build 中间态（4 Round 串行 #25 → #28 → #20 → #21）
- **安全相关：** ✅（#28 接收 HTML `style="..."` 外部输入）

---

## 1. 目的与背景

消化 `memory-bank/techContext.md §技术债务清单` 4 项 Layout 正确性债，使引擎在以下维度达到 CSS 2.1 规范一致性：

1. **#25** LayoutBox 坐标 API 抽象提升（`border_box_origin()` / `padding_box_origin()` / `content_box_origin()` 取代 20+ 处分散计算）
2. **#28** HTML `<div style="...">` 属性解析路径打通（HTML parser 接 `CssParser::ParseDeclarationList`）
3. **#20** Block formatting context margin collapsing 完整实现（CSS 2.1 §8.3.1 全套规则）
4. **#21** Inline formatting context line box 模型（baseline / ascent / descent / vertical-align / line-height 半-leading）

收益：渲染正确性提升至 CSS 2.1 §8.3.1 + §10.8 一致；DevTool 主线（FPS overlay / Inspector）依赖正确的 box geometry 表示；为未来 z-index stacking context、float/clear、bidi 等扩展打下规范基础。

---

## 2. 不做（Boundary）

- **不实现 float / clear**（#20 仅做 collapse-with-clearance 的接口预留，float exclusion 真正生效需 float 引擎本身）
- **不实现 inline-block 内部 IFC 重新分箱递归**（仅在 LineBox 层把 inline-block 当替换元素 atomically 处理）
- **不实现 bidi（unicode-bidi / direction）**
- **不实现 white-space `pre` / `pre-wrap` 真正断行**（保持现有 normal 行为，TextShaper 不改 wrapping 算法）
- **不实现 text-indent / hanging-punctuation**
- **不动 flex 路径**（仅 #25 helper 在 `flex_layout.cc` 替换分散计算；flex 自己的 main/cross axis 算法不动）
- **不重写 layout 性能基线**（仅保证 `bench_layout_*` 不退化超 10%）

---

## 3. 成功标准

| # | 维度 | 验收条件 |
|:-:|---|---|
| A1 | #25 origin API | LayoutBox 提供 `(border\|padding\|content)_box_origin()` 返回 `Point{x, y}`；`(border\|padding\|content)_box_top/right/bottom/left()` 返回 4 边坐标；至少 12 处分散计算（layout_engine 5 + flex_layout 7）替换为 helper |
| A2 | #28 HTML inline style | `<div style="color: red; padding: 10px">` HTML 解析后 `element->inline_declarations()` 非空且与 JS 路径 `el.style.color = 'red'` 行为一致 |
| A3 | #28 安全护栏 | (a) declaration count cap 1000 项；(b) 单 value 长度 cap 8 KB；(c) 关键字黑名单 `expression(` / `behavior:` / `javascript:`（命中静默跳过 + log warn）|
| A4 | #20 sibling adjoining | 同级相邻 block 的 margin-bottom + margin-top → max（CSS 2.1 §8.3.1 case 1） |
| A5 | #20 nested first/last child | 父子相邻 margin-top（无 padding/border 间隔）collapse；同理 margin-bottom（CSS 2.1 §8.3.1 case 2） |
| A6 | #20 collapse-through | 空 box（无 content / 无 padding / 无 border / 无 height）的 margin-top + margin-bottom collapse 后再与外部 sibling/parent collapse（CSS 2.1 §8.3.1 case 3） |
| A7 | #20 negative collapse | 负 margin 与正 margin collapse → `max(positives) + min(negatives)`（CSS 2.1 §8.3.1 数学规则） |
| A8 | #20 clearance | `clear: left/right/both` 元素的 collapse 行为正确（CSS 2.1 §9.5.2 / §8.3.1 末尾）— 本任务范围内 stub clearance 接口 + 测试占位（真正 clear 需 float 引擎，留 P3） |
| A9 | #21 LineBox 抽象 | `LineBox` 数据结构含 `baseline_y / max_ascent / max_descent / line_height / start_x / end_x`；inline 子项按 LineBox iteration 布局 |
| A10 | #21 ascent/descent | TextShaper 改造为返回 `TextMetrics{width, height, ascent, descent}`（FreeType `face->size->metrics.ascender/.descender`）；layout 阶段消费 |
| A11 | #21 vertical-align 全 5 关键字 | `baseline / sub / super / middle / top / bottom` 6 关键字（A_full 含 sub/super 即 6 个）+ length/percent 偏移 |
| A12 | #21 line-height 半-leading | CSS 2.1 §10.8.1：`leading = line_height - (ascent + descent)`；上下半-leading 各 `leading/2`；strut（行首隐 inline 锚定行高）|
| A13 | W3C testcase | 至少 6 个 wpt fixture（margin-collapse 4 + line-box 2）入仓 `tests/fixtures/wpt/`；本地像素/坐标断言通过 |
| A14 | ctest 全量 | 951 (基线) + 预计 +25-40 new = 976-991 ALL PASS（Debug + Release `-O3 -Werror`）|
| A15 | bench 不退化 | `bench_layout_buildtree` / `bench_layout_flex` 同机 ≤ baseline × 1.10（每 Round 末必跑） |
| A16 | techContext 闭环 | `techContext.md §技术债务清单` #20/#21/#25/#28 状态变更为 ✅ 已闭环 + 实证数据表 |
| A17 | 安全威胁建模 | spec §6 完整覆盖 #28 三类威胁（DoS / 历史攻击向量 / 资源耗尽）+ plan §2 安全测试任务覆盖 |

---

## 4. 决策矩阵

| # | 维度 | 选择 | 理由 |
|:-:|---|---|---|
| Q1 | Round 3 #20 实施策略 | **A 一次性完整 W3C CSS 2.1 §8.3.1** | 用户选定全量；clearance 部分 stub 化（真 float 留 P3）|
| Q2 | Round 4 #21 范围 | **A 全量** LineBox + ascent/descent + vertical-align 6 关键字 + length 偏移 + 半-leading | 用户选定全量；inline-block atomic（不递归 IFC） |
| Q3 | W3C testcase 来源 | **A wpt 远程拉取** + Plan §0 代理实证 | 已实证 `172.22.32.1:7890` 代理通；114 个 margin-collapse fixture 可选 |
| Q4 | #28 安全护栏 | **A 完整三件套** count cap + value len cap + 历史攻击关键字黑名单 | 用户选定全量；威胁模型见 §6 |
| Q5 | 轮次提交粒度 | **A 每 Round 1 commit + 中间态报告 + 用户确认** | complexity-levels.mdc §多轮次协议标准路径；TASK-19-13 P3 落实 |
| Q6 | Plan × 0.6 估时档 | **0.6× 准确档** 预测 plan ~13.5h → 实测 ~8.1h | Level 4 首数据点；4 子任务复用度高但有 W3C 规范梳理成本 |

---

## 5. 架构与接口签名

### 5.1 Round 1 — `LayoutBox` origin helpers（#25）

```cpp
// veloxa/core/layout/layout_box.h（追加）
struct Point { f32 x; f32 y; };

struct LayoutBox {
  // ... existing fields ...

  // Origin helpers (border / padding / content box, all relative to parent's
  // padding-box origin).
  Point border_box_origin() const { return {x, y}; }
  Point padding_box_origin() const {
    return {x + border[kLeft], y + border[kTop]};
  }
  Point content_box_origin() const {
    return {x + border[kLeft] + padding[kLeft],
            y + border[kTop] + padding[kTop]};
  }

  // 4-side coordinate helpers
  f32 border_box_top()    const { return y; }
  f32 border_box_right()  const { return x + border_box_width(); }
  f32 border_box_bottom() const { return y + border_box_height(); }
  f32 border_box_left()   const { return x; }
  // ... padding_box / content_box top/right/bottom/left ...
};
```

**替换点（共 20 处分散计算）：**
- `layout_engine.cc:209-212` y_offset 累加表达式 → `child->margin_box_bottom()`
- `flex_layout.cc:235-236, 247-248` cross_size 计算 → 对应 `*_box_*` helper
- 余 16 处见 plan §1 详细列表

### 5.2 Round 2 — HTML parser inline style（#28）

#### 接口签名

```cpp
// veloxa/core/html/parser.cc（修改 ProcessAttributes，~L92）

namespace {
constexpr usize kMaxInlineDeclarationCount = 1000;     // 安全护栏 (a)
constexpr usize kMaxInlineValueLength      = 8 * 1024; // 安全护栏 (b)

constexpr StringView kBlacklistKeywords[] = {  // 安全护栏 (c)
    "expression(", "behavior:", "javascript:",
};

bool ContainsBlacklistKeyword(StringView value);  // ASCII 不区分大小写
}  // namespace

void Parser::ProcessAttributes(Element* el, Token& token) {
  while (token.type == TokenType::kAttribute) {
    String name = std::move(token.name);
    String value = std::move(token.value);
    if (name == "style") {
      ApplyInlineStyleAttribute(el, value.view());
    } else {
      el->SetAttribute(InternedString::Intern(name), std::move(value));
    }
    NextToken(token);
  }
}

void Parser::ApplyInlineStyleAttribute(Element* el, StringView css) {
  if (css.size() > kMaxInlineValueLength) return;            // (b)
  if (ContainsBlacklistKeyword(css)) return;                 // (c)
  auto decls = css::CssParser::ParseDeclarationList(css);
  usize count = std::min(decls.size(), kMaxInlineDeclarationCount); // (a)
  for (usize i = 0; i < count; ++i) {
    el->SetInlineDeclaration(decls[i].property, decls[i].value);
  }
}
```

### 5.3 Round 3 — Margin collapsing（#20）

#### 数据结构

```cpp
// veloxa/core/layout/margin_collapse.h（新增）
namespace vx::layout {

// 表示一个 margin chain（adjoining margins 的集合）
struct MarginChain {
  f32 max_positive = 0.0f;
  f32 min_negative = 0.0f;  // 存储为负数

  void Add(f32 m) {
    if (m >= 0) max_positive = std::max(max_positive, m);
    else        min_negative = std::min(min_negative, m);
  }

  // CSS 2.1 §8.3.1: collapsed = max_positive + min_negative
  f32 Collapsed() const { return max_positive + min_negative; }

  bool IsEmpty() const { return max_positive == 0.0f && min_negative == 0.0f; }
};

}  // namespace vx::layout
```

#### Block formatting algorithm（伪码）

```
LayoutBlock(box):
  // 1. 自己 top margin 加入父 chain（条件：自己是 first child + 无 padding/border-top）
  if self.IsFirstChild() && !self.HasTopBorderOrPadding():
    parent_chain.Add(self.margin_top)
    self.margin_top_collapsed = true

  // 2. 子项 sibling adjoining
  prev_chain = parent_chain  // 父链传入第一个子项
  y_offset = 0
  for child in children:
    if child.is_collapse_through():
      prev_chain.Add(child.margin_top + child.margin_bottom)
      // collapse-through 子项位置 = prev_chain.Collapsed()，content_height=0
      child.y = y_offset + prev_chain.Collapsed()
      continue

    prev_chain.Add(child.margin_top)
    child.y = y_offset + prev_chain.Collapsed()
    LayoutChild(child)
    y_offset = child.border_box_bottom()
    prev_chain = MarginChain(); prev_chain.Add(child.margin_bottom)

  // 3. 自己 bottom margin 加入父 chain（条件：自己是 last child + 无 padding/border-bottom + height auto）
  if self.IsLastChild() && !self.HasBottomBorderOrPadding() && self.style->height.is_auto():
    parent_chain = prev_chain  // 把残余链传出（含末子的 margin-bottom）
    parent_chain.Add(self.margin_bottom)
    self.margin_bottom_collapsed = true
  else:
    self.content_height = y_offset + prev_chain.Collapsed()  // 末子的 margin-bottom 在 self 内吸收
```

注：`is_collapse_through()` = 元素 height auto + content_height=0 + 无 padding/border + 无 line-box。

### 5.4 Round 4 — LineBox + vertical-align + line-height（#21）

#### CSS 字段扩展

```cpp
// veloxa/core/css/enums.h（追加）
enum class VerticalAlign : u8 {
  kBaseline, kSub, kSuper, kMiddle, kTextTop, kTextBottom, kTop, kBottom,
  kLength,  // length / percentage 偏移（值在 ComputedStyle.vertical_align_offset）
};

// veloxa/core/css/computed_style.h（追加）
struct ComputedStyle {
  // ... existing ...
  VerticalAlign vertical_align = VerticalAlign::kBaseline;
  LengthValue   vertical_align_offset;  // 仅当 vertical_align == kLength
};

// veloxa/core/css/property.h enum PropertyId（追加）
kVerticalAlign,
```

涉及 5 文件全链路：`enums.h` / `computed_style.h` / `property.{h,cc}` / `style_resolver.cc` / `transition.cc` / `enum_serialization.{h,cc}` 同 display 模式（参见 TASK-19-01 B5 反查表沉淀）。

#### LineBox 数据结构

```cpp
// veloxa/core/layout/line_box.h（新增）
namespace vx::layout {

struct LineBoxItem {
  LayoutBox* box;
  f32 ascent;    // 该 item 的基线以上高度（px）
  f32 descent;   // 该 item 的基线以下高度（px）
  f32 baseline_offset;  // vertical-align 后基线相对 line.baseline_y 的偏移
};

struct LineBox {
  f32 start_x = 0;
  f32 end_x   = 0;          // 容器 content-box 宽度
  f32 baseline_y;           // 行基线 y 坐标（line top + max_ascent + half-leading-top）
  f32 max_ascent  = 0;      // 行内最高 ascent
  f32 max_descent = 0;      // 行内最深 descent
  f32 line_height;          // CSS line-height
  Vector<LineBoxItem> items;

  f32 height() const { return max_ascent + max_descent; }
  f32 top()    const { return baseline_y - max_ascent; }
  f32 bottom() const { return baseline_y + max_descent; }
};

}  // namespace vx::layout
```

#### Inline formatting algorithm（伪码）

```
LayoutInline(box):
  available = box.content_width
  current_line = LineBox{}
  lines = []
  y = 0

  for child in box.children:
    metrics = MeasureItem(child)  // text → TextMetrics{w,h,asc,desc}; replaced/inline-block → atomic box
    if current_line.x + metrics.width > available && current_line.items.size() > 0:
      // 行尾 → flush
      ResolveLineBaselineAndPositions(current_line, y)
      lines.append(current_line); y += current_line.height()
      current_line = LineBox{}

    item.baseline_offset = ComputeVerticalAlign(child.style, current_line.line_height,
                                                metrics.ascent, metrics.descent)
    current_line.items.append(item)
    current_line.max_ascent  = max(current_line.max_ascent, metrics.ascent + item.baseline_offset)
    current_line.max_descent = max(current_line.max_descent, metrics.descent - item.baseline_offset)

  if current_line.items.size() > 0:
    ResolveLineBaselineAndPositions(current_line, y); y += current_line.height()

  box.content_height = y

ComputeVerticalAlign(style, line_height, ascent, descent):
  switch style.vertical_align:
    case kBaseline:    return 0
    case kSub:         return +font_size * 0.2  // 沉到下方
    case kSuper:       return -font_size * 0.4  // 升到上方
    case kMiddle:      return -(x_height/2 + (ascent - descent)/2)
    case kTextTop:     return -(parent_ascent - ascent)
    case kTextBottom:  return +(parent_descent - descent)
    case kTop:         return -line.max_ascent + ascent
    case kBottom:      return +line.max_descent - descent
    case kLength:      return -ResolveLength(style.vertical_align_offset)
```

注：`kTop / kBottom / kMiddle` 因依赖 line.max_ascent/max_descent，需要 **2-pass** 算法（第 1 pass 收集所有非 top/bottom item 的 metrics → 计算 line.max_ascent/max_descent → 第 2 pass 解析 top/bottom）。`ResolveLineBaselineAndPositions` 内部完成。

#### TextShaper 扩展

```cpp
// veloxa/core/layout/text_shaper.h
struct TextMetrics {
  f32 width;
  f32 height;
  f32 baseline;  // 兼容字段（= ascent）
  f32 ascent;    // 新增（FT face->size->metrics.ascender / 64.0）
  f32 descent;   // 新增（FT face->size->metrics.descender / 64.0，正数）
};
```

---

## 6. 安全威胁建模（#28 — [安全相关] 必填段）

按 `.cursor/rules/skills/security.mdc` 轻量威胁建模。

### 6.1 威胁列表

| ID | 威胁 | 攻击路径 | 缓解（A 完整三件套） |
|:-:|---|---|---|
| T1 | DoS via 巨 declaration list | 攻击者构造 `style="a:1;a:1;a:1;...×百万"` 让 parser O(N²) 卡死 | declaration count cap 1000；超额静默截断 |
| T2 | DoS via 巨 single value | 单个 `style="background: url('A...×10MB')"` 让 ParseDeclarationList 字符串扫描超时 | 单 value 长度 cap 8 KB；超额整 attribute 跳过 |
| T3 | 历史 IE expression() 注入 | `style="width:expression(alert(1))"` 在老 IE 触发任意 JS | 引擎不支持 expression，但黑名单关键字早期拦截，避免未来任何 lazy eval 路径意外暴露 |
| T4 | CSS behavior 注入 | `style="behavior:url(...)"` 老 IE htc 攻击 | 黑名单关键字 `behavior:` |
| T5 | javascript: URL via background | `style="background:url(javascript:alert(1))"` | 黑名单关键字 `javascript:`；ImageCache::Load 也应拒绝（留 #28 范围外） |
| T6 | CSS property name overflow | 异常长 property name 触发 InternedString hash 退化 | `ParseDeclarationList` 已按 `Declaration` 结构存储，property 是 enum，name 不入散列 |
| T7 | Race condition via concurrent attribute write | 多线程同时调 `SetInlineDeclaration` | DOM 模型本身单线程（HTML parser + JS event loop 串行），不在威胁面 |

### 6.2 测试覆盖（plan §2 P3）

- T1 reg test: `style=` 含 1500 项 declaration → 仅前 1000 应用 + 无 abort
- T2 reg test: 含 16 KB 单 value → 整 style attribute 跳过 + element 仍可正常渲染
- T3-T5 reg test: 三类黑名单关键字各一例 → 静默跳过 + 无 inline_declaration 落地
- T1-T5 RED 反向探针：临时移除护栏 → 观察 oversized fixture 触发的 ASAN/timeout

### 6.3 不在范围

- CSP（Content-Security-Policy）支持 — 引擎层暂不实现
- Hash-based attribute integrity — 静态站点场景不适用

---

## 7. 测试策略

### 7.1 单元测试增量（plan §1-§4 详列）

| Round | 测试文件 | 新增 case | 覆盖 |
|:-:|---|--:|---|
| R1 | `tests/core/layout/layout_box_test.cc` | +6 | origin helpers / 4-side helpers / Point 语义 |
| R2 | `tests/core/html/parser_test.cc` + 新 `inline_style_test.cc` | +8 | A2 行为一致性 + A3 三类安全护栏 + RED |
| R3 | `tests/core/layout/block_layout_test.cc` + 新 `margin_collapse_test.cc` | +12 | A4-A8 五类 collapsing + 两条 RED 反向探针 |
| R4 | `tests/core/layout/inline_position_test.cc` + 新 `line_box_test.cc` | +9 | A9-A12 LineBox 抽象 + vertical-align 6 关键字 + line-height 半-leading + RED |
| **合计** | — | **+35** | — |

### 7.2 集成测试（W3C wpt fixture）

- `tests/fixtures/wpt/css/CSS2/margin-padding-clear/`：6 个 margin-collapse-NNN.xht（plan §0 拉取并 commit 入仓）
- `tests/fixtures/wpt/css/CSS2/linebox/`：2 个 vertical-align / line-height fixture
- 集成方式：`tests/integration/wpt_layout_test.cc` 用 `vx::html::Parser` 加载 fixture，断言 layout box 几何（坐标 / 尺寸 ± 1 px 容差）

### 7.3 Bench 回归网

- 每 Round 末跑 `bench_layout_buildtree` + `bench_layout_flex` 同机 3 reps mean
- 门槛：≤ baseline × 1.10（TASK-24-01 32K block sweet spot 不破）
- 任一 BM 退化 > 10% → 暂停 Round 进 R1 升级路径（perf 排查）

### 7.4 Mixed TDD RED 反向探针（每 Round 标配）

按 P1 落实规则（TASK-11 反思 #3 / TASK-24-01 / TASK-24-03 三层成熟应用），D3 类回归测试必标配：
- R1 RED：`Point::operator==` 故意改错 → 观察 helper test FAIL → 恢复
- R2 RED：T1 count cap 故意设为 unlimited → 1500 项 fixture FAIL → 恢复
- R3 RED：collapsing 故意改回直加 → margin-collapse-001 wpt fixture FAIL → 恢复
- R4 RED：max_ascent 故意改为 sum 而非 max → 多 inline metrics fixture FAIL → 恢复

---

## 8. 注入点核对表

| 子任务 | 注入点 | 状态 |
|---|---|:-:|
| #25 origin helpers | `layout_box.h` 追加 6 helper | ✅ |
| #25 替换分散计算 | `layout_engine.cc` 5 处 + `flex_layout.cc` 7-15 处 | ✅ grep 已实证 |
| #28 HTML parser 接 ParseDeclarationList | `html/parser.cc:92-95` 内 if 分支 | ✅ |
| #28 安全护栏 | `html/parser.cc` 文件级常量 + 黑名单 helper | ✅ |
| #20 MarginChain | 新增 `core/layout/margin_collapse.h` | ✅ |
| #20 Block 算法 | `layout_engine.cc:150-243 LayoutBlock` 重写 | ✅ |
| #21 VerticalAlign enum | `css/enums.h` + `computed_style.h` + `property.{h,cc}` + `style_resolver.cc` + `transition.cc` + `enum_serialization.{h,cc}` 全链路 | ✅（5 文件参考 line-height 模式）|
| #21 LineBox + algorithm | 新增 `core/layout/line_box.h` + `layout_engine.cc:245-303 LayoutInline` 重写 | ✅ |
| #21 TextShaper extend | `text_shaper.{h,cc}` `TextMetrics` 加 ascent/descent + FT 路径 | ✅ |

---

## 9. CMake 链接拓扑

无新增依赖、无 ABI 破坏。仅新增 header（`margin_collapse.h` / `line_box.h`），均归属 `vx_core/layout` 静态库内（不影响外部链接）。

---

## 10. 文件清单

### 新增

- `veloxa/core/layout/margin_collapse.h` — MarginChain 数据结构
- `veloxa/core/layout/line_box.h` — LineBox 数据结构
- `tests/core/layout/margin_collapse_test.cc`
- `tests/core/layout/line_box_test.cc`
- `tests/core/html/inline_style_test.cc`
- `tests/integration/wpt_layout_test.cc`
- `tests/fixtures/wpt/css/CSS2/margin-padding-clear/*.xht` × 6
- `tests/fixtures/wpt/css/CSS2/linebox/*.xht` × 2
- `tests/fixtures/wpt/README.md` — 来源 + commit hash + 拉取脚本

### 修改

- `veloxa/core/layout/layout_box.h` — origin helpers + Point
- `veloxa/core/layout/layout_engine.cc` — LayoutBlock 重写 / LayoutInline 重写 / 替换分散计算
- `veloxa/core/layout/flex_layout.cc` — 替换分散计算（不动算法）
- `veloxa/core/layout/text_shaper.{h,cc}` — TextMetrics ascent/descent + FT metrics
- `veloxa/core/html/parser.cc` — ProcessAttributes 加 style 分支 + 安全护栏
- `veloxa/core/css/enums.h` — VerticalAlign enum
- `veloxa/core/css/computed_style.h` — vertical_align + offset
- `veloxa/core/css/property.{h,cc}` — kVerticalAlign + 元数据表
- `veloxa/core/css/style_resolver.cc` — vertical-align 解析 + inheritance
- `veloxa/core/css/transition.cc` — kVerticalAlign 切换支持（不可动画 → fallback）
- `veloxa/core/css/enum_serialization.{h,cc}` — VerticalAlign 反查表
- `tests/core/layout/layout_box_test.cc` — +6 helper case
- `tests/core/layout/block_layout_test.cc` — +6 collapsing 集成 case
- `tests/core/layout/inline_position_test.cc` — +5 line-box case
- `tests/core/html/parser_test.cc` — +3 inline style 基础 case
- `memory-bank/techContext.md` — #20/#21/#25/#28 状态闭环 + 代理地址单一真相来源补全

---

## 11. 估时（Plan × 0.6 第 10 数据点 / Level 4 首数据点）

| Round | 内容 | plan (min) | × 0.6 (min) |
|:-:|---|---:|---:|
| R0 | 基线核验 + 工具 grep + W3C testcase 拉取 | 30 | 18 |
| R1 | #25 origin helpers + 6 测试 + 替换分散计算 + bench | 60 | 36 |
| R2 | #28 HTML parser 接入 + 安全护栏 + 8 测试 + 集成 + bench | 90 | 54 |
| R3 | #20 margin collapsing 全实施 + 12 测试 + wpt 4 fixture + bench | 300 | 180 |
| R4 | #21 LineBox + vertical-align + TextShaper + 9 测试 + wpt 2 fixture + bench | 360 | 216 |
| R5 | finalize（baseline / techContext / MB / reflection） | 60 | 36 |
| **合计** | — | **900 (15h)** | **540 (9h)** |

预测档位：**0.6× 准确档**（plan 15h → 实测 ~9h，与用户选定 Q6 一致）。

---

## 12. R1-Rn 风险登记

| ID | 风险 | 触发条件 | 缓解 |
|:-:|---|---|---|
| R1 | wpt fixture 网络拉取失败 | 代理失效 / GitHub rate limit | plan §0 实证 + 失败降级到 Q3_B handcraft 6 例 |
| R2 | margin collapsing 残留 case | W3C §8.3.1 case 4-7 高级场景未覆盖 | 至少 4 wpt fixture 覆盖 case 1-3；clearance 接口 stub + P3 留余 |
| R3 | vertical-align kTop/kBottom 2-pass 算法死锁 | 自引用：item 取决于 line.max_ascent，line.max_ascent 取决于 item | 算法明确分两轮：第一轮排除 kTop/kBottom 计算 line.max_*；第二轮用 max_* 解析 kTop/kBottom |
| R4 | TextShaper 改造影响 ReplayTextHeavy bench | ascent/descent 增字段使 TextMetrics struct 变大 | bench 提前 baseline；< 5% 噪声接受；> 10% 触发 P1 升级 |
| R5 | enum_serialization 全链路改动多文件忘补 | 8 文件需同步 | spec §5.4 + plan §4 P0 grep 7 文件 fingerprint 模板 |
| R6 | bench_layout_flex 退化 > 10% | #25 helper 内联失败 → 函数调用开销 | helper 必 inline + Release `-O3` 必跑 + 退化触发 helper 改为 `[[nodiscard]] constexpr` 保证内联 |
| R7 | Round 间 git rebase 冲突 | 4 Round 串行 commit，HEAD 漂移 | 每 Round 完成后用户确认进入下一 Round 前 `git status` 检查 |
| R8 | W3C testcase XHTML 解析失败 | 现 HTML parser 不处理 `<?xml` 头 | fixture 预处理脚本去掉 XHTML 头 + DOCTYPE 替换为 HTML5 |

---

## 13. 与未来任务关系

- **本任务承接：** TASK-19-01（B5 enum_serialization 反查表）/ TASK-25-01（Composition over Inheritance — Round 4 LineBox 不继承 LayoutBox 而组合 / TextShaper 扩展沿用相同模式）
- **本任务推：**
  - TASK-26-02（候选 P3）：float / clear 引擎（#20 clearance stub 转正）
  - TASK-26-03（候选 P3）：z-index stacking context + paint order 重构（依赖 origin helper 抽象）
  - TASK-26-04（候选 P3 触发型）：CSS Containment / `position: sticky`（依赖 BFC 完整性）
  - TASK-26-05（候选 P3 触发型）：bidi（unicode-bidi / direction，依赖 LineBox 抽象）
- **本任务为 DevTool 主线（TASK-25-02）解锁：** Inspector 显示精确 box geometry 需要 origin API；FPS overlay 需要 Layout 不引发不必要 reflow（margin collapsing 减少冗余 layout pass 数 ~10-20%）
