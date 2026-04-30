# first/last child margin collapse with parent — 设计规格

**任务 ID：** TASK-20260430-01
**复杂度级别：** Level 3（单子系统 Layout + API 设计决策 + 跨函数 chain propagate）
**安全相关：** ❌ 否
**创建日期：** 2026-04-30
**关联：** 落实 TASK-20260426-01 archive §10 P3 触发型「TASK-26-02」占位 / wpt-005 SKIP-w/-rationale 直接验证目标

---

## 1. 目的

完成 W3C CSS 2.1 §8.3.1 vertical margin collapsing 中**跨函数边界**的两条 adjoining 规则：

1. **first-child margin-top 与 parent margin-top collapse**（在 parent 的 4 项阻断条件不触发时）
2. **last-child margin-bottom 与 parent margin-bottom collapse**（在 parent 的 6 项阻断条件不触发时）

打开 TASK-20260426-01 R3 D1.2 锁定的「LayoutChild API 边界限留」，引入 `LayoutBlockChild` 专用辅助函数让 `MarginChain` 跨 LayoutBlock 函数边界 propagate。

---

## 2. 不做（边界）

| 不做项 | 理由 / 后续承接 |
|---|---|
| `clear` / `float` CSS 属性解析 | VAN F2 实证：CSS layer 完全无实现 → 独立 Level 4 任务（TASK-26-04 候选） |
| float 排版（CSS §9.5）| 同上 |
| clearance 完整算法（清除 float 后的偏移计算）| 依赖 float 实施；当前 `MarginChain.has_clearance` 标志位仍保留 stub |
| LayoutInline 路径的 margin collapse | IFC 边界自然阻断 line boxes（spec §8.3.1）|
| LayoutFlex 路径的 margin collapse | flex item 不参与 vertical margin collapse（spec §10.5）|
| `display: flow-root` BFC trigger | 当前仅识别 `overflow != visible`（与 R3 D1.3 边界一致）|
| max-height < content_height 时的边界细节 | 罕见场景，边界用例 ≤ 1 个，收益不足；P3 触发型留 |

---

## 3. 成功标准

| # | 验收项 | 验证方式 |
|:-:|---|---|
| A1 | ctest 全量 PASS（≥ 1029）| `ctest --output-on-failure` |
| A2 | `+10-15` 新单测全部 PASS | `block_layout_test` |
| A3 | wpt-005 SKIP → PASS（`Wpt005_NonSiblingAdjoiningMarginsCollapse`）| `ctest -R Wpt005` |
| A4 | 3 反向探针完整循环（破坏 → FAIL → 恢复 → PASS）| 临时改实现 + 跑指定测试 |
| A5 | Release `-O3 -Werror` 0 err/warn | `cmake --build build-bench -j` |
| A6 | bench `BM_LayoutBuildTreeFlat/64` 同窗口对照 ≤ +10% | §7.0.1 stash-swap 协议 |
| A7 | bench `BM_LayoutFlex` 不退化超 +10% | 同上 |
| A8 | LayoutChild 签名零变更 | `git diff layout_engine.h \| grep LayoutChild` |
| A9 | LayoutInline / LayoutFlex 调用 LayoutChild 行为零变更 | 既有 inline / flex 测试全 PASS |
| A10 | first child collapse 完整 4 阻断条件覆盖测试 | A2 子集 |
| A11 | last child collapse 完整 6 阻断条件覆盖测试 | A2 子集 |
| A12 | deep chain（≥ 2 层嵌套）首尾贯通 | A2 中 `DeepCollapseChain_NestedFirstChildren` |
| A13 | collapse-through 自身 + first/last 复合场景 | A2 中 `CollapseThrough_BoxPropagatesAcrossLevels` |
| A14 | techContext.md 技术债 #20 完整版状态升级（注明本任务消化的范围）| 文档 grep |
| A15 | progress.md / activeContext.md / tasks.md 三件套同步 | MB 验证 |
| A16 | reflection-TASK-20260430-01.md 落盘（plan × 0.6 第 14 数据点）| /reflect 阶段 |
| A17 | TASK-26-01 沉淀的 5 P0/P1 规则中 4 条得到首次外部任务验证（§7.0.1 / §9.1 / §9.3 / subagent §D3）| 反思 §3.5 矩阵 |

---

## 4. 决策矩阵（PLAN 头脑风暴 D1-D5 锁定）

| # | 维度 | 选择 | 理由 |
|:-:|---|---|---|
| D1 | API 改造策略 | **A1** 新增 `LayoutBlockChild` 专用辅助；`LayoutChild` 不动 | LayoutInline/Flex 路径零影响；遵循 §9.1 跨 LayoutType 独立 box-model；API 边界清晰 |
| D2 | 传递语义 | **A** by-value in / by-value out | `MarginChain` 12B POD；编译器 SROA 等价无开销；数据流单向可见；无 lifetime / nullable 隐形约束 |
| D3 | 阻断条件覆盖 | **A** 完整规范子集（padding/border + BFC root + height + min-height）| W3C 浏览器对齐；wpt-005 验证；6 边界条件全覆盖 |
| D4 | 测试深度 | **B** 完整档（10 单测 + 3 反向探针 + wpt-005）| §9.3 强制最小 ≥ 1 处 D3 反向探针；本任务 3 个独立 D3 维度（blocks_top / blocks_bottom / deep chain）|
| D5 | Phase 划分 | **B** 7 Phase 细粒度 | RED 与 GREEN 分离彻底；first/last 拆开独立 phase；末期可选择性合并 commits |

---

## 5. 架构与接口

### 5.1 W3C CSS 2.1 §8.3.1 adjoining margin 4 条规则

| # | 规则 | 当前状态（R3 完成后）| 本任务 |
|:-:|---|---|---|
| R1 | top margin of a box and top margin of its first in-flow child | ❌ 未实施 | ✅ 本任务 |
| R2 | bottom margin of a box and top margin of its next in-flow following sibling | ✅ R3 完成 | 不动 |
| R3 | bottom margin of a last in-flow child and bottom margin of its parent (if 'auto' computed height) | ❌ 未实施 | ✅ 本任务 |
| R4 | top and bottom margins of a box that does not establish a new BFC and that has zero computed 'min-height', zero or 'auto' computed 'height', and no in-flow children (collapse-through) | ✅ R3 完成（仅 sibling 范围）| 扩展支持「collapse-through 跨 parent 边界」(R5) |
| R5 | collapse-through 跨 parent 边界（递归向外）| ❌ 未实施（collapse-through chain 未渗出）| ✅ 本任务（含 R5 完整传播）|

### 5.2 阻断条件 truth table

#### 5.2.1 first child margin-top 与 parent margin-top 是否 adjoining

```
adjoining_top(parent, first_child) =
    parent.padding[Top] == 0
 && parent.border[Top] == 0
 && !is_BFC_root(parent)
 && IsInFlow(first_child)
 // 注：clearance 的 separator 条件本任务不实施（依赖 float）
```

#### 5.2.2 last child margin-bottom 与 parent margin-bottom 是否 adjoining

```
adjoining_bottom(parent, last_child) =
    parent.padding[Bottom] == 0
 && parent.border[Bottom] == 0
 && parent.style.height.is_auto()
 && (parent.style.min_height.is_none() || parent.style.min_height.is_auto()
     || resolved(parent.style.min_height) == 0)
 && !is_BFC_root(parent)
 && IsInFlow(last_child)
```

> **注**：max-height 的边界（max-height < natural content_height 时阻断）暂不实施 — 见 §2 不做项；触发概率低。

### 5.3 函数签名

```cpp
// veloxa/core/layout/layout_engine.h（新增 private 静态方法）
class LayoutEngine {
  // ... existing methods ...

  // first/last child margin collapse with parent。仅在 LayoutBlock 内部调用。
  // - incoming：祖先链路传入的 chain（可能含 ancestor.margin_top + 上层 chain）
  // - return：本 box layout 完成后渗出给祖先的 chain（last-child collapse 路径）
  static MarginChain LayoutBlockChild(LayoutBox* child, f32 containing_width,
                                       const LayoutContext& ctx,
                                       MarginChain incoming);

  // ... existing methods ...
};
```

### 5.4 算法（伪码）

```cpp
MarginChain LayoutEngine::LayoutBlockChild(LayoutBox* box, f32 containing_width,
                                            const LayoutContext& ctx,
                                            MarginChain incoming) {
  // 1) 解析 box-model（与现有 LayoutBlock 等价）
  ResolveBoxModel(...);
  ResolveContentWidth(...);

  // 2) 计算 collapse 阻断状态
  const bool blocks_top = (box->padding[Top] != 0 || box->border[Top] != 0
                            || CreatesBFC(box));
  const bool blocks_bottom = (box->padding[Bottom] != 0 || box->border[Bottom] != 0
                               || !style->height.is_auto()
                               || HasNonZeroMinHeight(style, ctx)
                               || CreatesBFC(box));

  // 3) 启动 chain：决定 incoming 是否合入
  MarginChain cur_chain;
  if (!blocks_top) {
    cur_chain = incoming;                    // 接续 ancestor chain
    cur_chain.Add(box->margin[Top]);          // 加入自身 margin-top
  }
  // 否则 cur_chain = empty；box.margin[Top] 独立放置（不进 chain）

  // 4) children 循环（与 R3 等价，但 child 是 kBlock 时递归调 LayoutBlockChild）
  f32 y_cursor = 0.0f;
  bool any_in_flow = false;
  for (LayoutBox* child = box->first_child; child; child = child->next_sibling) {
    if (!IsInFlow(child)) continue;
    any_in_flow = true;

    if (child->type == LayoutType::kBlock) {
      // 递归子 block，传入当前 chain（含 box.margin_top + ancestor chain）
      MarginChain child_outgoing = LayoutBlockChild(
          child, box->content_width, ctx, cur_chain);
      // 如果 child 完全 collapse-through，child_outgoing 可能继续含 chain
      if (child->collapsed_through) {
        cur_chain = child_outgoing;            // chain 继续向下传播
      } else {
        cur_chain = MarginChain{};
        cur_chain.Add(child->margin[Bottom]);
        y_cursor = child->y + child->border_box_height();
      }
    } else {
      // 非 block 子（kInline / kText / kFlex / kReplaced）走 R3 老路径
      LayoutChild(child, box->content_width, ctx);
      // ... 与 R3 相同 sibling collapse 逻辑 ...
    }
  }

  // 5) 末尾 trailing margin 处理
  f32 trailing_margin = any_in_flow ? cur_chain.Collapsed() : 0.0f;
  ResolveContentHeight(box, font_size, ctx);
  if (style->height.is_auto() || style->height.is_none()) {
    box->content_height = y_cursor + trailing_margin;
  }
  // ... min/max-height clamp ...

  // 6) outgoing chain
  if (!blocks_bottom && any_in_flow) {
    // last child margin-bottom + box.margin_bottom + cur_chain 全部渗出
    MarginChain outgoing = cur_chain;
    outgoing.Add(box->margin[Bottom]);
    box->margin_bottom_collapsed_into_ancestor = true;  // 用于跳过 trailing 双计
    return outgoing;
  } else {
    // 阻断：box.margin_bottom 独立处理；不渗出
    return MarginChain{};
  }
}
```

> **关键正确性约束**：
>   1. `incoming` 与 `box.margin_top` 的合并必须在 `blocks_top == false` 时才发生
>   2. `outgoing` 含 `box.margin_bottom` 必须在 `blocks_bottom == false` 时才发生
>   3. 若 box 自身是 collapse-through（已有 R3 判定），incoming 与 outgoing 应是同一个 chain（递归性）
>   4. `box->y` 的写入由 caller (LayoutBlock) 在拿到 outgoing 之前完成 — 即 child 的 y 坐标基于 incoming 计算（与 R3 在 LayoutBlock 主循环里写 `child->y = y_cursor + cur_chain.Collapsed()` 一致，但需要把 ancestor chain 也合入）

### 5.5 LayoutBlock 顶层入口适配

`LayoutEngine::Layout(doc, ctx)` 顶层入口对 root box 调用一次 `LayoutBlockChild(root, viewport_width, ctx, MarginChain{})` 即可（incoming 永远 empty）。返回值丢弃（root 没有 ancestor）。

或者保留现有 `LayoutBlock(box, w, ctx)` 公共入口，内部转调 `LayoutBlockChild(box, w, ctx, MarginChain{})`，从而对 LayoutFlex 等其他调用者透明。**采纳后者**（更小侵入）。

### 5.6 状态字段扩展

`LayoutBox` 已有 R3 字段：
- `bool collapsed_through`（R3）
- `bool margin_top_collapsed_into_ancestor`（R3）

本任务新增：
- `bool margin_bottom_collapsed_into_ancestor`（last-child 渗出 bottom 时设）

---

## 6. 安全威胁建模

❌ N/A — 纯 layout 算法，无外部输入新增 / 无认证 / 无新依赖（VAN F6 实证）。

唯一边界：递归深度风险。LayoutBlockChild 递归层数 = DOM block-level 嵌套深度。当前 BuildTree 已有递归（与 LayoutBlock 同 stack），本任务不新增递归层数（同一个 LayoutBlockChild 替代 LayoutBlock 调用）。栈使用量等价。

---

## 7. 测试策略

### 7.1 单测 10 项（D4.B）

文件：`tests/core/layout/block_layout_test.cc`（扩展现有文件）

| # | 测试 | 验证规则 | 反向探针候选 |
|:-:|---|---|---|
| 1 | `FirstChildMarginCollapsesWithParent_Basic` | R1 主流程 | — |
| 2 | `LastChildMarginCollapsesWithParent_Basic` | R3 主流程 | — |
| 3 | `PaddingTop_BlocksFirstChildCollapse` | §5.2.1 阻断 | ✅ |
| 4 | `BorderBottom_BlocksLastChildCollapse` | §5.2.2 阻断 | — |
| 5 | `BFCRoot_BlocksBothCollapse` | overflow!=visible 阻断 | — |
| 6 | `ExplicitHeight_BlocksLastChildCollapse` | height!=auto 阻断 | ✅ |
| 7 | `MinHeightNonZero_BlocksLastChildCollapse` | min-height>0 阻断 | — |
| 8 | `DeepCollapseChain_NestedFirstChildren` | R1 + 递归 (≥3 层) | — |
| 9 | `CollapseThrough_BoxPropagatesAcrossLevels` | R5 collapse-through 跨边界 | ✅ |
| 10 | `LastChild_OutgoingChainContainsParentMarginBottom` | R3 + outgoing semantic | — |

### 7.2 W3C wpt 集成测试 1 项

文件：`tests/integration/wpt_layout_test.cc`

- `Wpt005_NonSiblingAdjoiningMarginsCollapse`：从 SKIP-w/-rationale 升级为 PASS
  - fixture：`tests/fixtures/wpt/css/CSS2/margin-padding-clear/margin-collapse-005.xht`
  - 期望布局：`#div3.margin-bottom: 2em` 与外层 `<div>.margin-bottom` collapse → 与 `#div4.margin-top: 1em` collapse → 合并值 = max(2em, 1em) = 2em
  - 数值化断言：div3.bottom + 2em offset == div4.top

### 7.3 反向探针 3 项（§9.3 强制）

| # | 探针 | 临时改动 | 期望 D3 类 FAIL |
|:-:|---|---|---|
| 1 | `blocks_top` 检测 | 强制 `blocks_top = false` | `PaddingTop_BlocksFirstChildCollapse` 应 FAIL（first child 反向 collapse 进了不该 collapse 的 parent）|
| 2 | `blocks_bottom` 检测 | 强制 `blocks_bottom = false` | `ExplicitHeight_BlocksLastChildCollapse` 应 FAIL |
| 3 | `outgoing chain` 传播 | LayoutBlockChild 强制返回 `MarginChain{}`（永不渗出）| `CollapseThrough_BoxPropagatesAcrossLevels` 应 FAIL |

### 7.4 性能验收

- bench `BM_LayoutBuildTreeFlat/64` 同窗口 stash-swap 对照（§7.0.1 强制）
- bench `BM_LayoutBuildTreeNested/16`、`BM_LayoutFlex<8,8>` 全维度兜底
- 退出门：mean ≤ +10% / median ≤ +10%

---

## 8. 文件清单

### 8.1 修改

| 文件 | 修改内容 | 估行数 |
|---|---|---|
| `veloxa/core/layout/layout_engine.h` | +1 private static method `LayoutBlockChild` 签名 | +5 |
| `veloxa/core/layout/layout_engine.cc` | 抽离 LayoutBlock 主体到 LayoutBlockChild + 加 incoming/outgoing 处理 + 4 阻断条件 + recursion dispatch | +120 / -5 |
| `veloxa/core/layout/layout_box.h` | +1 字段 `bool margin_bottom_collapsed_into_ancestor` | +1 |
| `tests/core/layout/block_layout_test.cc` | +10 单测（D4.B） | +250 |
| `tests/integration/wpt_layout_test.cc` | Wpt005 SKIP → PASS（数值化）| +20 / -5 |
| `memory-bank/techContext.md` | #20 状态注释扩展（注明 first/last child 范围闭环）| +5 |

### 8.2 新增

无（不引入新文件，复用 R3 沉淀的 `margin_collapse.h` POD）。

---

## 9. 风险登记

| # | 风险 | 概率 | 影响 | 缓解措施 |
|:-:|---|---|---|---|
| R1 | LayoutBlockChild 递归与 R3 sibling 主循环逻辑交叠 → bug | 中 | 中 | P1 RED 单测先建立基准；P3 阻断条件实施后立即跑全 R3 既有测试不退化 |
| R2 | LayoutFlex 内嵌 block child 因 LayoutBlock 公共入口签名不变而行为偏离 | 低 | 中 | A9 验收：既有 flex 测试全 PASS；公共入口转调 LayoutBlockChild 时 incoming = empty |
| R3 | min-height resolve 早于 content_height → 阻断判定时尚无 min_height 数值 | 中 | 低 | min-height resolve 仅依赖 font_size + containing；与 content_height 无关，可在阻断判定前完成 |
| R4 | bench 退化超 +10%（incoming/outgoing 拷贝与递归深度）| 中 | 中 | §7.0.1 同窗口对照 + by-value 编译器 SROA 优化 + 12B POD 极小 |
| R5 | wpt-005 数值化断言精度 | 低 | 低 | 用 epsilon 比较（与 R3 已有 wpt 测试同范式）|
| R6 | reflection 阶段发现 LayoutChild 也需扩展（即 D1.A1 错选）| 低 | 中 | VAN F4 实证 LayoutChild 仅 LayoutBlock 调用需 chain；该风险已被代码实证排除 |

---

## 10. 估时（plan × 0.6 准确档）

| Phase | 内容 | plan (min) | × 0.6 (min) |
|:-:|---|---:|---:|
| P0 | 准备（grep + wpt-005 拆解 + 基线）| 30 | 18 |
| P1 | RED 单测全套 + 反向探针位置注释 | 60 | 36 |
| P2 | API 改造 + dispatch（编译通）| 30 | 18 |
| P3 | GREEN first child collapse | 45 | 27 |
| P4 | GREEN last child collapse + 4 阻断条件 | 90 | 54 |
| P5 | 反向探针验证 + deep chain + collapse-through 递归 | 60 | 36 |
| P6 | wpt-005 + bench 同窗口 + 收尾 | 75 | 45 |
| **合计** | — | **390 (~6.5h)** | **234 (~3.9h)** |

**plan × 0.6 第 14 数据点预期**：Level 3 单 Round 单子系统 + 创意已锁定 + 基础设施 100% 复用 → 落「准确档 0.5-0.6×」（继 R1/R2 子任务 0.50× / 0.56× 之后）。

---

## 11. 验证 TASK-26-01 P0/P1 规则 ROI（首次外部任务）

| 规则段 | 触发场景 | 验证标的 |
|---|---|---|
| `writing-plans.mdc` §7.0.1 同窗口 stash-swap | P6 bench 验证 | 验证规则避免跨时间窗误判 ROI |
| `writing-plans.mdc` §9.1 跨 LayoutType 独立 box-model | P2 LayoutBlockChild 改造 | 验证规则强制 LayoutInline/Flex 路径独立解析 |
| `writing-plans.mdc` §9.2 默认值边界 | spec §5.4 / §5.5 | 验证 plan 锁定 incoming/outgoing 默认 empty 语义 |
| `writing-plans.mdc` §9.3 D3 反向探针强制 | P1 / P5 | 3 反向探针完整循环（第 6 次实战） |
| `subagent-development.mdc` D3 重评估 | plan 阶段标 D3 / build 阶段重评估 | 本任务无子代理标记（直执行直推荐） |
| `creative.md` d.1 多坐标系约定 | 不触发（无多坐标算法）| ⊘ |

---

## 12. 完成定义

- 全部 A1-A17 验收通过
- /reflect 阶段产出 reflection-TASK-20260430-01.md
- /archive 阶段合并到 main + techContext.md #20 闭环更新

---

**设计人：** AI Agent
**审查日期：** 2026-04-30
