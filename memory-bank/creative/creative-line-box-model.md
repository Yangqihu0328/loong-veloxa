# 创意设计：LineBox 模型 + vertical-align + line-height

- **日期：** 2026-04-26
- **状态：** ✅ 已批准（ACCEPT D2，用户接受全部推荐）
- **关联任务：** TASK-20260426-01（Round 4 / 子任务 #21）
- **关联 spec：** [docs/specs/2026-04-26-layout-correctness-design.md §5.4](../../docs/specs/2026-04-26-layout-correctness-design.md)
- **关联 plan：** [docs/plans/2026-04-26-layout-correctness.md §4](../../docs/plans/2026-04-26-layout-correctness.md)

---

## 设计挑战

### 问题陈述

把 LayoutInline 从「`line_height = max(child.height)` 简化路径」升级为 **CSS 2.1 §10.8 完整 line box 模型**，含：
1. **LineBox 抽象**：每行有明确 baseline / max_ascent / max_descent / line-height / start_x / end_x 字段
2. **TextShaper.TextMetrics ABI 扩展**：`{width, height, baseline}` → `{width, height, baseline, ascent, descent}`
3. **vertical-align 完整支持**：6 关键字 + length（baseline / sub / super / middle / text-top / text-bottom / top / bottom + length/percent）
4. **line-height 半-leading**（CSS 2.1 §10.8.1）：`leading = line_height - (ascent + descent)`，上下半各 `leading/2`
5. **2-pass vertical-align 算法**：`top` / `bottom` 关键字依赖 `line.max_ascent / max_descent`，需先排除 top/bottom 算 max_*，再解析 top/bottom

### 约束条件

- ctest 基线 984 → 1000（+16 R4 实施 + 2 wpt fixture）
- bench 不退化 > 10%（hot path：`bench_layout_buildtree` + `bench_text_shaping` 必须实证）
- 与 R3 margin collapsing 完全隔离（不在 inline formatting 内触发）
- W3C wpt `inline-formatting-context-001.xht` + `vertical-align-baseline-001.xht` 2 例必通过
- inline-block 当 atomic（不递归 IFC），spec §2 不做边界
- 不引入 bidi（unicode-bidi / direction），spec §2 不做边界

### 成功标准

- spec A9-A12 全通
- LineBox 抽象**可独立测试**（不依赖 LayoutEngine 整体路径）
- TextMetrics ABI 扩展**向后兼容**（`baseline` 字段保留 + `[[deprecated]]`）
- 2-pass 算法**有形式化文档说明**
- RED 反向探针有效

---

## 探索的子决策点

LineBox + vertical-align 是紧耦合体系，3 个相互独立的子决策点各自探索：

### D2.A：LineBox 数据结构表征

#### A.1 显式 LineBox struct + Vector\<LineBox\>（spec §5.4 初选）

```cpp
struct LineBoxItem {
  LayoutBox* box;
  f32 ascent;
  f32 descent;
  f32 baseline_offset;
};
struct LineBox {
  f32 start_x = 0, end_x = 0;
  f32 baseline_y;
  f32 max_ascent = 0, max_descent = 0;
  f32 line_height;
  Vector<LineBoxItem> items;
};
```

**优点：** 调试直观；可独立单测；DevTool / Inspector 直接读取（未来扩展）；与 spec §5.4 一致。
**缺点：** 32K 节点 ≈ 11.5 MB ArenaAllocator 压力。
**复杂度：** 中

#### A.2 内联式无显式 LineBox

**描述：** 不创建 LineBox struct；维护 stack 局部变量 `cur_line_max_ascent / max_descent / start_x / baseline_y`。

**优点：** 零内存开销；与现有 LayoutInline 风格一致；bench 几乎无退化。
**缺点：** vertical-align kTop/kBottom 的 2-pass 必须用临时 `Vector` 储存，本质复活 LineBox 部分内存压力但失去抽象独立测试性；DevTool 取行信息要重写遍历。
**复杂度：** 中

#### A.3 紧凑 LineBox（POD + items 链表 ArenaAllocator）

**描述：** LineBox 24 byte POD，item 链表节点用 ArenaAllocator 分配。

**优点：** 内存效率高；与 ArenaAllocator 体系一致。
**缺点：** 链表遍历比 vector 慢（cache locality 差）；编码 +30%；调试比 A.1 难。
**复杂度：** 高

#### D2.A 对比

| 维度 | A.1 显式 LineBox | A.2 内联式 | A.3 紧凑 LineBox |
|---|:-:|:-:|:-:|
| 抽象层级 | ⭐⭐⭐ 高 | ⭐ 低 | ⭐⭐ 中 |
| 独立单测性 | **优** | 差 | 中 |
| 内存压力（32K） | ~11.5 MB | ~6 MB | ~8 MB |
| bench 退化估计 | +5-8% | +2-3% | +4-6% |
| DevTool 复用 | **优** | 差 | 中 |
| 实施量 | 中 | 中 | 高 |

---

### D2.B：vertical-align 算法（pass 数）

#### B.1 严格 2-pass（spec §5.4 初选）

```
Pass 1: 收集所有非 {kTop, kBottom} item，计算 line.max_ascent / max_descent
Pass 2: 解析 kTop/kBottom item baseline_offset，再用其 metric 更新 line.max_ascent/descent
```

**优点：** 算法明确；2 遍即收敛；与 W3C 规范一致。
**缺点：** 需仔细处理 pass 2 中 max_ascent/descent 的二次更新。
**复杂度：** 中

#### B.2 N-pass 迭代收敛

**描述：** 迭代解析 baseline_offset 直到 max_ascent/descent 收敛。

**优点：** 通用；适合未来扩展。
**缺点：** 多次遍历开销大；过度设计（CSS 2.1 ≤2 pass 即收敛）。
**复杂度：** 高（无收益）

#### B.3 单 pass + 延迟解析（kTop/kBottom 用 sentinel）

**描述：** 单 pass 中 kTop/kBottom item baseline_offset 暂设 NaN，行结束时一次性扫描。

**优点：** 单循环；逻辑紧凑。
**缺点：** NaN 检查分支预测不友好；可读性 < B.1；本质 2-pass。
**复杂度：** 中

#### D2.B 对比

| 维度 | B.1 严格 2-pass | B.2 N-pass | B.3 sentinel |
|---|:-:|:-:|:-:|
| 算法清晰度 | **优** | 中 | 中 |
| 性能（5 item） | ~2× | ~5× | ~1.5× |
| 收敛性证明 | 显式 | 隐式 | 显式 |
| 可维护性 | **优** | 差 | 中 |
| W3C 规范契合 | **完全** | 过度 | 完全 |

---

### D2.C：TextShaper TextMetrics ABI 兼容策略

#### C.1 加字段不删字段（spec §5.4 初选）

```cpp
struct TextMetrics {
  f32 width;
  f32 height;
  [[deprecated("use ascent")]] f32 baseline;  // 兼容字段（= ascent）
  f32 ascent;    // 新增（FT face->size->metrics.ascender / 64.0）
  f32 descent;   // 新增（FT face->size->metrics.descender / 64.0，正数）
};
```

**优点：** 老 caller 0 修改；ABI 不破。
**缺点：** baseline == ascent 冗余 4 byte；后续维护可能困惑（缓解：`[[deprecated]]` 注解）。
**复杂度：** 低

#### C.2 重命名 baseline → ascent + 删除 baseline

**优点：** 清爽；无冗余字段。
**缺点：** ABI 破坏；现有 caller 需同步改名；与 R1/R2/R3 同 Round 协调成本上升。
**复杂度：** 中

#### C.3 拆分两个 struct

**描述：** 老 API 返回 `TextDimensions{width, height}`，新 API 返回 `TextBaselineMetrics{...}`。

**优点：** SOLID 单一职责。
**缺点：** 双 API 调用 → 测量两次（性能 -20%）；过度设计。
**复杂度：** 高（无收益）

#### D2.C 对比

| 维度 | C.1 加字段 | C.2 重命名 | C.3 拆分 |
|---|:-:|:-:|:-:|
| ABI 兼容 | **完全** | 破 | 半 |
| 同 Round 协调成本 | **低** | 中 | 高 |
| 字段冗余 | 4 byte | 0 | 8 byte |
| 调用方修改面 | 0 | grep 替换 N 处 | N 处升级双 API |
| 长期清晰度 | 中（deprecated 标记） | **优** | 中 |

---

## 选定方案：A.1 + B.1 + C.1

### 决策矩阵（已 ACCEPT）

| ID | 决策点 | 选择 | 理由 |
|:-:|---|---|---|
| **D2.A** | LineBox 数据结构 | **A.1 显式 LineBox + Vector\<LineBox\>** | 抽象清晰、独立单测、DevTool 主线复用 |
| **D2.B** | vertical-align 算法 | **B.1 严格 2-pass** | 与 CSS 2.1 §10.8.1 一致、可维护 |
| **D2.C** | TextMetrics ABI | **C.1 加字段 + `[[deprecated]]`** | ABI 兼容、同 Round 协调成本最低 |
| **D2.D** | inline-block 处理 | **atomic（取 border_box_height 做 ascent，0 做 descent）** | spec §2 不做边界 |
| **D2.E** | line-height 单位语义 | **CSS 2.1 §10.8.1 默认**（数字 → font-size 倍数；length → 绝对像素；percent → font-size 百分比） | 完整规范支持 |

### 综合理由

1. **A.1 LineBox 抽象**：(1) 抽象清晰对应 CSS 2.1 §9.4.2 / §10.8 概念；(2) 独立测试性是 RED 反向探针的前提；(3) DevTool 主线（TASK-25-02 占位）解锁 Inspector 显示行框；(4) 内存压力 11.5 MB 在 ArenaAllocator 32K block × ~360 块下可接受（K8 抖动阈值在 65K block 才触发）。
2. **B.1 严格 2-pass**：(1) CSS 2.1 §10.8.1 规则在 2 pass 内可完全解析；(2) 算法清晰度对维护重要；(3) 文档化 pass 2 后 kTop/kBottom 不影响其他 item 位置。
3. **C.1 加字段不删**：(1) ABI 不破，与 R1/R2/R3 同步推进零冲突；(2) `[[deprecated]]` 给未来清理一个明确锚点；(3) 字段冗余 4 byte 在 hot path 不显著。

---

## 实现指导

### 文件清单

#### 新增

- `veloxa/core/layout/line_box.h` — `LineBox` + `LineBoxItem` POD struct
- `tests/core/layout/line_box_test.cc` — 9 unit tests + 2 RED

#### 修改

- `veloxa/core/layout/text_shaper.h` — `TextMetrics` 加 `ascent / descent`，`baseline` 标 `[[deprecated]]`
- `veloxa/core/layout/text_shaper.cc` — FT path 填 `ascent = face->size->metrics.ascender / 64.0f` + `descent = -face->size->metrics.descender / 64.0f`
- `veloxa/core/layout/layout_engine.cc:245-303` — `LayoutInline` 重写（约 +120 行）
- 6 文件 enum 全链路（参见下方 §CSS 全链路改造）

### 2-pass 算法形式化文档

```
Phase 1 (collect non-extreme metrics):
  for item in line.items:
    if item.style.vertical_align in {kBaseline, kSub, kSuper, kMiddle, kTextTop, kTextBottom, kLength}:
      item.baseline_offset = ComputeNonExtremeAlign(item, parent_metrics)
      line.max_ascent  = max(line.max_ascent,  item.ascent  + item.baseline_offset)
      line.max_descent = max(line.max_descent, item.descent - item.baseline_offset)

Phase 2 (resolve top/bottom + update max_*):
  for item in line.items:
    if item.style.vertical_align == kTop:
      item.baseline_offset = -line.max_ascent + item.ascent
    elif item.style.vertical_align == kBottom:
      item.baseline_offset = +line.max_descent - item.descent

  // CSS 2.1 §10.8.1: top/bottom items can grow line height
  // 但不影响其它 item 的位置（其它 item 已在 Phase 1 锁定）
  for item in line.items:
    if item.style.vertical_align in {kTop, kBottom}:
      line.max_ascent  = max(line.max_ascent,  item.ascent  + item.baseline_offset)
      line.max_descent = max(line.max_descent, item.descent - item.baseline_offset)

Phase 3 (resolve baseline_y from line top):
  line.baseline_y = line.top + line.max_ascent + half_leading_top
  for item in line.items:
    item.box.y = line.baseline_y - item.ascent + item.baseline_offset
```

### vertical-align 6 关键字 + length 计算公式

```cpp
ComputeNonExtremeAlign(item, parent_metrics):
  switch (item.style.vertical_align):
    case kBaseline:    return 0
    case kSub:         return +parent_metrics.font_size * 0.2f   // 沉到下方
    case kSuper:       return -parent_metrics.font_size * 0.4f   // 升到上方
    case kMiddle:      return -(parent_metrics.x_height / 2 + (item.ascent - item.descent) / 2)
    case kTextTop:     return -(parent_metrics.ascent  - item.ascent)
    case kTextBottom:  return +(parent_metrics.descent - item.descent)
    case kLength:      return -ResolveLength(item.style.vertical_align_offset, parent_metrics.font_size)
                       // length / percent → 正值上移，CSS 2.1 §10.8.1
```

### 半-leading 公式

```cpp
f32 leading = line_height - (max_ascent + max_descent);
f32 half_leading_top    = leading / 2.0f;
f32 half_leading_bottom = leading - half_leading_top;  // 处理奇数像素

f32 line_height_actual = max_ascent + max_descent + leading;
//                     = line_height
```

注：当 `line_height < (max_ascent + max_descent)` 时 `leading < 0`，半-leading 也为负 → 行高被字体 metrics 撑开（CSS 2.1 §10.8.1 允许）。

### CSS 全链路改造（6 文件，参考 line-height 模式）

1. `veloxa/core/css/enums.h` — `enum class VerticalAlign : u8 { kBaseline, kSub, kSuper, kMiddle, kTextTop, kTextBottom, kTop, kBottom, kLength }`
2. `veloxa/core/css/computed_style.h` — `VerticalAlign vertical_align = VerticalAlign::kBaseline; LengthValue vertical_align_offset;`
3. `veloxa/core/css/property.h/.cc` — `kVerticalAlign` PropertyId + 元数据表
4. `veloxa/core/css/style_resolver.cc` — parser + inheritance（vertical-align 不继承）
5. `veloxa/core/css/transition.cc` — kVerticalAlign 切换支持（discrete fallback；length 类型可动画）
6. `veloxa/core/css/enum_serialization.h/.cc` — VerticalAlign 反查表

### 测试矩阵

| 类型 | 文件 | case 数 | RED |
|---|---|--:|:-:|
| 单元（LineBox） | tests/core/layout/line_box_test.cc | 9 | 2（max_ascent 改 sum_ascent / Phase 2 忘记更新 max_*） |
| 单元（CSS parser） | tests/core/css/parser_test.cc | +3 | 1（kSub 解析为 kSuper） |
| 单元（TextShaper） | tests/core/layout/text_shaper_test.cc | +3 | — |
| 集成（LayoutInline） | tests/core/layout/inline_position_test.cc | +5 | — |
| W3C wpt | tests/integration/wpt_layout_test.cc | 2（IFC-001 + vertical-align-baseline-001） | — |

---

## 风险登记

| 风险 | 缓解 |
|---|---|
| 显式 LineBox 内存压力 | bench `bench_layout_buildtree` 跑 32K 节点；如退化 > 10% 触 P1 升级降级到 A.3 紧凑路径 |
| 2-pass max_* 二次更新逻辑出错 | 单测覆盖 (a) kTop 撑大 max_ascent / (b) kBottom 撑大 max_descent / (c) 同时 kTop+kBottom；+ 1 RED（Phase 2 忘记更新 max_* → multi-metrics fixture FAIL） |
| `[[deprecated]]` 触发 `-Werror` 编译失败 | layout_engine.cc 现有 `metrics.baseline` 读取处暂用 `metrics.ascent`（同语义）替换；R4 commit 后 grep `metrics.baseline` 应零命中 |
| inline-block 误递归 IFC | LayoutInline 入口判定 `child.style->display`；inline-block 走 atomic 路径（取 child.border_box_height 当 ascent + 0 当 descent） |
| LineBox bidi 假设破裂 | spec §2 显式不做 bidi；测试 fixture 仅 LTR；未来引入 bidi 留 P3 TASK-26-05 |
| line-height 数字 vs length 单位歧义 | CSS 2.1 §10.8.1 默认实现：数字（无单位）→ 子代继承计算因子（不预乘 font-size），length → 绝对像素直接用 |

---

## 与 R1 (#25) / R3 (#20) 的协调

- **#25 origin helpers（R1 已落）：** Round 4 中 `line.start_x` / `line.top` 用 `parent.padding_box_origin()`（来自 Round 1 helper），避免分散计算
- **#20 margin collapsing（R3 已落）：** LineBox 不参与 collapse；inline formatting context 的 line-box 顶部/底部被外层 block context 视作 content 边界（不 collapse 出 BFC root）

---

## 用户确认记录

```
[2026-04-26 02:15 用户回复] 全部 ACCEPT 推荐
→ D2.A = A.1 显式 LineBox + Vector<LineBox>
→ D2.B = B.1 严格 2-pass
→ D2.C = C.1 加字段不删字段 + [[deprecated("use ascent")]]
→ D2.D = inline-block atomic（border_box_height 当 ascent + 0 当 descent）
→ D2.E = CSS 2.1 §10.8.1 默认 line-height 单位语义（数字 / length / percent）
```
