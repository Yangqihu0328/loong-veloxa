# 创意设计：Margin Collapsing 算法

- **日期：** 2026-04-26
- **状态：** ✅ 已批准（ACCEPT D1，用户接受全部推荐）
- **关联任务：** TASK-20260426-01（Round 3 / 子任务 #20）
- **关联 spec：** [docs/specs/2026-04-26-layout-correctness-design.md §5.3](../../docs/specs/2026-04-26-layout-correctness-design.md)
- **关联 plan：** [docs/plans/2026-04-26-layout-correctness.md §3](../../docs/plans/2026-04-26-layout-correctness.md)

---

## 设计挑战

### 问题陈述

为 LayoutBlock 实施 CSS 2.1 §8.3.1 完整 margin collapsing。要支持 5 类规则：
1. **同级相邻**（adjoining siblings）：`prev.margin-bottom + next.margin-top → max`
2. **父子相邻**（first child margin-top 与 parent margin-top；last child margin-bottom 与 parent margin-bottom）— 需 parent height auto + 无 padding/border 间隔
3. **空 box collapse-through**：自身 margin-top + margin-bottom 后再外传
4. **负 margin 数学**：`max(positives) + min(negatives)`
5. **clearance** 打破 collapse 链 — 本任务范围内 stub 接口 + 测试占位

### 约束条件

- ctest 基线 951 → 984（+19 R3 实施 + 4 wpt fixture）
- bench 不退化 > 10%（hot path：`bench_layout_buildtree` + `bench_layout_flex` 必须实证）
- 算法可被其他 formatting context（flex/grid future）借鉴或彻底隔离
- W3C wpt margin-collapse-{001,002,003,091}.xht 4 例必通过
- 配合 #25 origin helpers（Round 1 已落地）使用，避免再次出现「分散坐标计算」反模式

### 成功标准

- spec A4-A8 全通
- 算法可读性高：单独 header 文件 / 单 entrypoint 函数
- 后续 float / clear（P3 TASK-26-02）可仅在 LayoutBlock 内增量改造，无需重写算法骨架
- RED 反向探针有效

---

## 探索的方案

### 方案 A：MarginChain in-line 累积法

**描述：** 自顶向下遍历子项时维持一条「滚动 chain」，每遇到新 margin 就 `chain.Add()`，直到遇到「打破点」（非空 content / padding / border / clearance）→ flush chain → 新 chain 开始。

**核心数据结构：**

```cpp
struct MarginChain {
  f32 max_positive = 0.0f;
  f32 min_negative = 0.0f;  // 存储负数

  void Add(f32 m) {
    if (m >= 0) max_positive = std::max(max_positive, m);
    else        min_negative = std::min(min_negative, m);
  }
  f32 Collapsed() const { return max_positive + min_negative; }
  bool IsEmpty() const  { return max_positive == 0 && min_negative == 0; }
};
```

**优点：**
- O(N) 单 pass，与现有架构一致
- 内存常数空间（每层递归 1 个 MarginChain ~8 byte）
- collapse-through 自然嵌入主循环
- 易于 RED 反向探针

**缺点：**
- `IsLastFlowChild()` 判断需「枚举展开后回填」配合
- collapse-through 判定依赖 LayoutChild 之后才能确定的 content_height（需「先 layout child → 后回填 child.y」）

**复杂度：** 中（~150 行 C++）

**风险：** collapse-through 嵌套链 lookback 出错（缓解：margin-collapse-091 wpt fixture + 嵌套 collapse-through 自定义测试）

---

### 方案 B：Pre-pass 双遍法

**描述：** Pass-1 自顶向下递归整棵树，仅收集每 box 的 margin/collapse-through 标志和 chain segments，不放置 box。Pass-2 根据 chain map 一次性计算所有 box 的 final y 坐标。

**优点：**
- 算法分离干净（phase 1 = 收集；phase 2 = 解析）
- collapse-through 不需要 lookback
- 易并行化

**缺点：**
- 内存开销 O(N)：32K 节点 ~768 KB
- 重复树遍历：~2× 时间常数
- 与现有 LayoutEngine 风格不兼容，重构面巨大

**复杂度：** 高

**风险：** bench 退化 +20-30%（违反 ≤×1.10 门槛）

---

### 方案 C：递归累积 + outgoing_chain 返回值法

**描述：** 数据结构同 A，但「chain 状态」从「LayoutBlock 内部局部变量」变成函数返回值的一部分。

```cpp
struct LayoutResult {
  f32 used_height;
  MarginChain outgoing_chain;
};
LayoutResult LayoutBox(box, MarginChain incoming_chain);
```

**优点：**
- 函数纯化，易测试 & 推理
- collapse-through 由 outgoing_chain 自然表达

**缺点：**
- 现有 LayoutBox 副作用 mutate `box->x/y/width/height`，改返回值意味大改 API
- 影响 #25 / #21 的同步签名改造，单 Round commit 边界变模糊
- ~5% 拷贝开销（POD 16 byte × hot path 频次）

**复杂度：** 中-高（API 改造范围 5-8 函数签名）

**风险：** API 改造 spread out 引发 commit 边界混乱

---

## 方案对比

| 维度 | 方案 A | 方案 B | 方案 C |
|---|:-:|:-:|:-:|
| 算法时间复杂度 | O(N) 单 pass | O(N) ×2 pass | O(N) 单 pass |
| 内存空间 | O(深度) chain stack | O(N) segments map | O(深度) chain stack |
| 与现有架构契合度 | ⭐⭐⭐ 高 | ⭐ 低 | ⭐⭐ 中 |
| collapse-through 处理 | 需 lookback | 自然展开 | 自然展开 |
| bench 退化风险 | 低（≤+5%） | **高（+20-30%）** | 中（+5-10%） |
| 可测试性 | 中 | 高 | **高** |
| RED 反向探针易度 | 高 | 高 | 高 |
| flex/grid 隔离 | ⭐⭐⭐ 完全隔离 | ⭐⭐ 共用抽象 | ⭐⭐⭐ 完全隔离 |
| 实施量 | ~150 行 | ~300+ 行 + 改 LayoutEngine | ~200 行 + 改 5-8 签名 |
| 与 #25 #21 协调成本 | **低** | **高** | 中 |

---

## 选定方案：方案 A — MarginChain in-line 累积法

### 选择理由

1. **架构契合度最高**：与 `LayoutEngine::Layout` 现有「自顶向下单 pass + 副作用 mutate box」风格一致，#25/#20/#21 三者**互不冲突**，单 Round commit 边界清晰。
2. **bench 风险最低**：单 pass + 常数空间 chain，预计退化 ≤ 5%（远低于 10% 门槛）。
3. **collapse-through lookback 风险可控**：「先 layout child → 后回填 child.y」+ margin-collapse-091 wpt fixture 作为算法回归门。
4. **测试与 RED 反向探针清晰**：MarginChain 单元测试 + RED（去掉 negative 累加 → 负 margin 测试 FAIL）。

### 决策矩阵（已 ACCEPT）

| ID | 决策点 | 选择 | 理由 |
|:-:|---|---|---|
| D1.1 | Margin collapsing 算法 | **方案 A — MarginChain in-line 累积** | 见上方 4 点 |
| D1.2 | outgoing_chain 表示 | **LayoutBlock 内部 Vector\<MarginChain\> 栈式状态** | 保持现有 LayoutBox API 不动，避免 API 大改 |
| D1.3 | BFC root 判定范围 | **本 Round 仅识别 `overflow: hidden\|scroll\|auto` 作为 BFC root**，缺其他 BFC trigger 依靠 P3 TASK-26-02 | 防范围蔓延，符合 plan §2 不做边界 |
| D1.4 | 调试 trace | **`#if VX_DEBUG_LAYOUT` MarginChain::Trace 仅 DEBUG 启用** | Release 0 开销，Debug 可见 |
| D1.5 | collapse-through lookback 实现 | **先 layout child → 基于结果回填 child.y** | 与方案 A 推荐路径一致 |

---

## 实现指导

### 文件清单

#### 新增

- `veloxa/core/layout/margin_collapse.h` — `MarginChain` POD struct（header-only inline）
- `tests/core/layout/margin_collapse_test.cc` — 4 unit tests + 1 RED

#### 修改

- `veloxa/core/layout/layout_engine.cc:150-243` — `LayoutBlock` 重写（约 +80 行）
- `veloxa/core/layout/layout_box.h` — 追加成员（debug 状态，可选不持久化）：
  ```cpp
  bool collapsed_through = false;
  bool margin_top_collapsed_into_ancestor = false;
  ```

### 主算法伪码（精化版）

```
LayoutBlockChildren(parent, ancestor_chain):
  // (1) parent 自己 top margin 是否合并入 ancestor chain
  if parent.IsFirstFlowChild() && !parent.HasTopBorderOrPadding():
    ancestor_chain.Add(parent.margin_top)
    parent.margin_top_collapsed_into_ancestor = true

  cur_chain = MarginChain{}
  cur_chain.Add(parent.HasTopBorderOrPadding() ? 0 : ancestor_chain.Collapsed())

  flow_children = collect_flow_children(parent)  // 排除 floated / abs-positioned
  y = 0
  for i in 0..flow_children.size():
    child = flow_children[i]
    is_last = (i == flow_children.size() - 1)

    cur_chain.Add(child.margin_top)
    tentative_y = y + cur_chain.Collapsed()
    child.y = tentative_y

    LayoutChild(child, incoming_chain=cur_chain_or_empty)

    if child.is_collapse_through():
      // 回填：child 的 margin-bottom 也并入当前 chain，child 不占高度
      cur_chain.Add(child.margin_bottom)
      child.collapsed_through = true
      // child.y 保持 tentative_y（用作绝对位置基准）
      continue

    // 非 collapse-through 子项 flush chain
    y = child.y + child.border_box_height()
    cur_chain = MarginChain{}
    cur_chain.Add(child.margin_bottom)

  // (2) parent content_height 与 last child margin-bottom 处理
  if parent.IsLastFlowChild() && !parent.HasBottomBorderOrPadding() && parent.style->height.is_auto():
    // 末子 margin-bottom + parent.margin_bottom 留给 ancestor
    parent.outgoing_chain = cur_chain
    parent.outgoing_chain.Add(parent.margin_bottom)
    parent.content_height = y               // 末子 margin-bottom 不计入 content_height
  else:
    parent.content_height = y + cur_chain.Collapsed()
```

### 避坑清单

1. **margin collapsing 仅在 BFC 内生效**：
   - LayoutBlock 入口判定 `parent.style->display ∈ {kBlock, kListItem, kFlowRoot}`
   - 浮动 box 不参与 collapse（接口预留 `child.is_floated()` 早退）
   - 绝对定位 box 不参与 collapse（`position: absolute|fixed` 早退）

2. **BFC root 阻断 collapse**：
   - `overflow: hidden|scroll|auto` 创建 BFC，**自身 margin 不与外部 collapse**（D1.3 范围内识别此 case）
   - 完整 BFC trigger（display: flow-root / float / 绝对定位）依靠 P3 TASK-26-02

3. **clearance 接口预留**：
   ```cpp
   struct MarginChain {
     // ... existing ...
     bool has_clearance = false;
     void ApplyClearance(f32 amount) { /* P3 TASK-26-02 实施 */ }
   };
   ```
   测试占位（A8）：`<div clear="both">` 仅断言「未 collapse 入前序 chain」即可（real clear 需 float 引擎）。

4. **`IsLastFlowChild()` 判定失误防御**：改为「枚举展开后回填」（见伪码：先 collect `flow_children` 数组，循环用 `i == size-1` 判定，避免 iterator 类内部状态污染）。

### 测试矩阵

| 类型 | 文件 | case 数 | RED |
|---|---|--:|:-:|
| 单元（MarginChain） | tests/core/layout/margin_collapse_test.cc | 4 | 1（去掉 negative 累加） |
| 集成（LayoutBlock） | tests/core/layout/block_layout_test.cc | +12 | 1（`Collapsed()` 改回直加） |
| W3C wpt | tests/integration/wpt_layout_test.cc | 4（margin-collapse-001/002/003/091） | — |

### 调试 trace 模板（D1.4）

```cpp
#if VX_DEBUG_LAYOUT
void MarginChain::Trace(StringView name) const {
  fprintf(stderr, "[MC] %s  pos=%.2f neg=%.2f collapsed=%.2f\n",
          name.data(), max_positive, min_negative, Collapsed());
}
#else
void MarginChain::Trace(StringView) const {}  // no-op
#endif
```

Release 编译 `-O3` 内联消除（POD 输入 + 函数空体）→ zero cost。

---

## 风险登记

| 风险 | 缓解 |
|---|---|
| collapse-through lookback 出错 | margin-collapse-091 wpt 入门测试 + 嵌套 collapse-through 自定义单元测试（两个空 div 嵌套） |
| `IsLastFlowChild()` 判定失误 | 改为「枚举展开后回填」（先 collect flow_children 数组）|
| chain 状态调试困难 | `MarginChain::Trace` DEBUG-only 方法 |
| outgoing_chain 协议复杂 | 用 LayoutBlock 内部栈式状态而非函数返回值，保持现有 LayoutBox API 不动 |
| BFC root 边界蔓延 | D1.3 范围明确：仅 overflow 触发；其他 BFC trigger 留 P3 TASK-26-02 |
| margin-collapse 误用于 inline/flex/grid | LayoutBlock 入口 display 判定 + 测试覆盖 flex 子项不进入 collapse 路径 |

---

## 与 #25 / #21 的协调

- **#25 origin helpers（R1 已落）：** Round 3 中 `child.y` 的相对参考点用 `parent.padding_box_origin().y`（来自 Round 1 helper），避免分散计算
- **#21 LineBox（R4 后续）：** margin collapsing 不影响 LineBox 内部布局；LineBox 是 inline formatting 内部抽象，与 block formatting 的 chain 完全隔离

---

## 用户确认记录

```
[2026-04-26 02:10 用户回复] ACCEPT 推荐
→ D1.1 = 方案 A
→ D1.2 = LayoutBlock 内部 Vector<MarginChain> 栈式状态
→ D1.3 = 仅 overflow: hidden|scroll|auto 作为 BFC root
→ D1.4 = #if VX_DEBUG_LAYOUT 仅 DEBUG 启用 trace
→ D1.5 = 先 layout child → 基于结果回填 child.y
```
