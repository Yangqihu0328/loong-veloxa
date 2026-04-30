# 归档：TASK-20260430-01 first/last child margin collapse with parent

**日期：** 2026-04-30
**任务 ID：** TASK-20260430-01
**复杂度级别：** Level 3（单子系统 Layout + API 设计决策 + 跨函数 chain propagate）
**状态：** ✅ 已完成
**分支：** `feature/TASK-20260430-01-margin-collapse-parent`（基于 main `a84d30d`）
**安全相关：** ❌ 否（纯 layout 算法，无外部输入 / 无认证 / 无新依赖）

---

## 1. 任务概述

完成 TASK-20260426-01 R3 #20 留下的 D1.2 LayoutChild API 边界限留 — 让 `MarginChain` 跨 LayoutBlock 函数边界 propagate，实现 W3C CSS 2.1 §8.3.1 中 first/last child 与 parent 的 margin collapse + 阻断条件（parent padding-top / border-top / BFC root / explicit height / min-height），范围限定为 A 子项（不动 clearance / float / clear，CSS layer 零实现，VAN F2 实证；clearance 完整版独立 Level 4 任务承接）。

**直接验证目标：** wpt-005 SKIP → PASS（`Wpt005_NonSiblingAdjoiningMarginsCollapse`），✅ 已达成。

**来源：** TASK-20260426-01 archive §10 P3 触发型「TASK-26-02」占位 + reflection §4.12 / activeContext 待处理事项 / wpt-005 SKIP-w/-rationale 现实直接验证目标。

---

## 2. 技术方案

### 2.1 决策矩阵（VAN + PLAN 阶段锁定）

| # | 维度 | 选择 | 理由 |
|:-:|---|---|---|
| V1 | 范围 | A 子项（first/last child collapse with parent）| clearance 完整版依赖 float（CSS layer 零实现），独立 Level 4 任务承接 |
| V2 | 拆分策略 | 单 Round 一次过 + plan 内部 Phase 划分 | Level 3，无明显多 Round 边界 |
| V3 | Git 分支 | `feature/TASK-20260430-01-margin-collapse-parent` | 基于 main `a84d30d` |
| V4 | 复杂度 | Level 3 | 单子系统 Layout + API 设计决策（LayoutChild 跨函数 chain 透传协议）|
| V5 | 安全标注 | ❌ 不标 | 纯 layout 算法 |
| D1 | API 改造策略 | A1 新增 `LayoutBlockChild` 专用辅助；`LayoutChild` 不动 | LayoutInline/Flex 路径零影响；遵循 §9.1 跨 LayoutType 独立 box-model；API 边界清晰 |
| D2 | 传递语义 | A by-value in / by-value out（POD 12B）→ **build 阶段调整为 in-out by-pointer in / by-value out** | 编译器 SROA 等价无开销，但多级递归边界 by-value 失效（§5 教训 P0）|
| D3 | 阻断条件覆盖 | A 完整规范子集（padding/border + BFC root + height + min-height）| W3C 浏览器对齐；wpt-005 验证；6 边界条件全覆盖 |
| D4 | 测试深度 | B 完整档（10 单测 + 3 反向探针 + wpt-005）| §9.3 强制最小 ≥ 1；本任务 3 个独立 D3 维度（blocks_top / blocks_bottom / deep chain）|
| D5 | Phase 划分 | B 7 Phase 细粒度 | RED 与 GREEN 分离彻底；first/last 拆开独立 phase |

### 2.2 关键实现摘要

**核心算法：** 在 `layout_engine.cc` 内新增 `LayoutBlockChild(box, available_width, ctx, MarginChain* incoming)` 辅助函数，签名采用「in-out by-pointer in / by-value out」语义（多级递归 propagate 必需，build 阶段 D2 实证调整）：

- 入参 `incoming`：caller 的 sibling 累积 chain（read-modify-write）
- 返回值：本 box 的 outgoing chain（含 parent.mb + 阻断条件下的归零语义）

**5 阻断条件（W3C §8.3.1）：**

| # | 条件 | 实现位置 |
|:-:|---|---|
| 1 | `padding-top != 0` | `ComputeBlocksTop` |
| 2 | `border-top != 0` | `ComputeBlocksTop` |
| 3 | BFC root（`overflow != visible`）+ 隐式 BFC root（`box->parent == nullptr` / `box->parent->parent == nullptr`）| `ComputeBlocksTop` / `ComputeBlocksBottom` |
| 4 | `height` explicit（非 auto）| `ComputeBlocksBottom` |
| 5 | `min-height != 0` | `ComputeBlocksBottom` |

**新增字段：** `LayoutBox::margin_bottom_collapsed_into_ancestor`（与 R3 既有 `margin_top_collapsed_into_ancestor` 对称命名）。

**新增 method：** `MarginChain::MergeFrom(const MarginChain& other)` — 合并语义（max_positive / min_negative 双向取极值），区别于既有 `Add(f32)` 单值累积。

### 2.3 文件变更

| 操作 | 文件路径 | 说明 |
|------|---------|------|
| 新建 | `docs/specs/2026-04-30-margin-collapse-with-parent-design.md` | 设计 spec：17 验收 + D1-D5 决策矩阵 + §8.3.1 5 adjoining 规则状态表 + 阻断条件 truth table + 算法伪码 + 6 风险登记 |
| 新建 | `docs/plans/2026-04-30-margin-collapse-with-parent.md` | 实现 plan：7 Phase / 14 任务 / ~6.5h plan / plan×0.6 ≈ 3.9h 准确档 |
| 新建 | `memory-bank/reflection/reflection-TASK-20260430-01.md` | 12 段全维度回顾 + 4 改进建议（P0×1 + P1×3 + P2×2）|
| 新建 | `memory-bank/archive/archive-TASK-20260430-01.md` | 本归档 |
| 修改 | `veloxa/core/layout/layout_engine.cc` | +218/-47 — `LayoutBlockChild` 主逻辑 + first/last child propagate + 5 阻断条件 + collapse-through 跨边界 + P6.2 hoist 优化 |
| 修改 | `veloxa/core/layout/layout_engine.h` | +21 行 — `LayoutBlockChild` 函数声明 + `ComputeBlocksTop/Bottom` 内部辅助 |
| 修改 | `veloxa/core/layout/layout_box.h` | +8 行 — `margin_bottom_collapsed_into_ancestor` 字段 |
| 修改 | `veloxa/core/layout/margin_collapse.h` | +8 行 — `MarginChain::MergeFrom` 合并语义 method |
| 修改 | `tests/core/layout/block_layout_test.cc` | +415 行 — 10 `A8_*` 单测（含 5 阻断条件 + 3 递归/collapse-through + 反向探针标注）|
| 修改 | `tests/integration/wpt_layout_test.cc` | +85 行 — `Wpt005_NonSiblingAdjoiningMarginsCollapse` SKIP→PASS |
| 修改 | `memory-bank/{tasks,activeContext,progress,techContext}.md` | 任务追踪 + 状态推进 + 技术上下文 |
| 修改 | `.cursor/rules/skills/writing-plans.mdc` | 升级 §9.4 递归算法 API 决策必检项（P0）+ §0 既有测试隐式契约 fingerprint（P1）+ §0 CSS shorthand 能力 grep 表（P1）|
| 修改 | `.cursor/commands/creative.md` | 升级 §d.2 算法伪码累积语义 explicit method（P1）|

### 2.4 关键决策

1. **D1 = A1（LayoutBlockChild 专用辅助）**：避免改动现有 `LayoutChild` API（影响 LayoutInline/Flex 5 处分支），遵循 §9.1 跨 LayoutType 独立 box-model 原则。
2. **D2 = A → 调整为 in-out by-pointer in**：plan 阶段锁 by-value（POD 12B），build P3 三层递归实证 by-value 在 multi-level propagate 边界失效；改 in-out by-pointer，零 ABI 风险（编译器仍可 SROA），见 §5.1 教训。
3. **隐式 BFC root**（spec truth table 缺失补丁）：`box->parent == nullptr || box->parent->parent == nullptr` 视为 root + html/body 顶层 wrapper（与主流浏览器对齐），防止 R3 既有 sibling collapse 测试退化，见 §5.2 教训。
4. **MarginChain::MergeFrom 合并而非赋值覆盖**：spec §5.4 算法 `cur_chain = child_outgoing` 在 collapse-through 场景退化（覆盖丢失累积），改用 explicit method `MergeFrom`，见 §5.4 教训。
5. **副产品优化 P6.2 last_in_flow_block hoisting**：bench `BM_LayoutBuildTreeFlat/8` 边缘超 +15% 触发；O(N) end-of-loop scan → O(1) running pointer；符合「副产品优化 3 标准」（本 Round bench 触发 / ≤ 5 行修改 / 不引入新结构性改动）。
6. **CSS parser `border-bottom` shorthand 缺失**（副发现）：build 阶段发现 css parser 不解析 `border-bottom` shorthand → 改用 `padding-bottom` 等价物理分隔符；shorthand 缺失列入独立 P3 候选 `tasks.md §待立项候选`。

### 2.5 安全决策

本任务不涉及安全变更（VAN V5 锁定，纯 layout 算法 / 无外部输入 / 无认证 / 无新依赖）。

---

## 3. 测试覆盖

### 3.1 单元测试（10 `A8_*` + 3 反向探针）

| # | 测试名 | 覆盖维度 |
|:-:|---|---|
| 1 | `A8_1_FirstChildMarginCollapsesIntoParent` | first child main flow |
| 2 | `A8_2_LastChildMarginCollapsesIntoParent` | last child main flow |
| 3 | `A8_3_PaddingTopBlocksFirstChildCollapse` | 阻断条件 #1 |
| 4 | `A8_4_PaddingBottomBlocksLastChildCollapse` | 阻断条件 #2（CSS parser shorthand 缺失替代）|
| 5 | `A8_5_BFCRootBlocksMarginCollapseAcrossBoundary` | 阻断条件 #3（BFC root + 隐式 root）|
| 6 | `A8_6_ExplicitHeightBlocksLastChildCollapse` | 阻断条件 #4 |
| 7 | `A8_7_MinHeightBlocksLastChildCollapse` | 阻断条件 #5 |
| 8 | `A8_8_DeepChainPropagatesAcrossThreeLevels` | 三层递归 propagate |
| 9 | `A8_9_CollapseThroughEmptyBoxAcrossBoundary` | collapse-through 跨函数边界 |
| 10 | `A8_10_OutgoingChainContainsParentMarginBottom` | outgoing chain 含 parent.mb |

**§9.3 反向探针 3/3 完整循环**（第 6 次实战）：

- 探针 1 weakened `blocks_top` → #3 PaddingTop FAIL
- 探针 2 weakened `blocks_bottom` → #4+#6+#7 三项 FAIL
- 探针 3 weakened BFC check → #5 BFCRoot FAIL

恢复后全 PASS，证实测试设计有效。

### 3.2 集成测试

- `Wpt005_NonSiblingAdjoiningMarginsCollapse` SKIP → **PASS**（直接验证目标达成）；fixture 几何完全按 plan §0.2 手算期望（`gap = 40px = 2em`）匹配。
- 仅剩 `Wpt001_HorizontalMarginNotCollapse` SKIP（已知未实现 horizontal margin，与本任务无关）。

### 3.3 全量回归

- ctest **1039/1039** PASS（基线 1029 + 10 new cases；Debug + Release `-O3 -Werror` 双通路）
- 测试时间稳定 2.22-2.42s，零 flaky

### 3.4 性能验收（同窗口 stash-swap，TASK-26-01 §7.0.1 升级规则首次外部任务实战）

| BM | mean Δ | A6/A7 退出门 | 判定 |
|---|---:|---:|:-:|
| `BM_LayoutBuildTreeFlat/64` | +7.55% | ≤ +10% | ✅ |
| `BM_LayoutBuildTreeNested/16` | +3.42% | ≤ +10% | ✅ |
| `BM_LayoutBuildTreeMixed/16` | -9.84% | ≤ +10% | ✅（反向证明 P6.2 优化未引入 cache 抖动）|
| `BM_LayoutFlex<1,8>` | +5.84% | ≤ +10% | ✅ |
| `BM_LayoutFlex<8,8>` | +4.40% | ≤ +10% | ✅ |
| `BM_LayoutFlex<16,16>` | +4.94% | ≤ +10% | ✅ |
| `BM_LayoutBuildTreeFlat/8` | +6.10%（P6.2 优化后；优化前 +15%）| ≤ +10% | ✅ |

**ROI 验证：** §7.0.1 同窗口 stash-swap 协议 7 BM 一次过，CV 5-8% 区间为可信信号，无跨时间窗误判 → 高 ROI，建议长期保留。

---

## 4. 经验教训

详见 `memory-bank/reflection/reflection-TASK-20260430-01.md` §5。核心 5 条：

1. **多级递归 mental walkthrough 是 API 传递语义决策的硬性前置**（P0 升级 → `writing-plans.mdc` §9.4）：≥3 层递归 + 是否需要 caller 看到 callee 修改
2. **既有测试是「隐式 spec」必须 grep fingerprint**（P1 升级 → `writing-plans.mdc` §0「既有测试隐式契约 fingerprint」）：layout / parser / event 类任务 plan §0 必含
3. **CSS parser 能力 grep 必须按 shorthand 逐项验证**（P1 升级 → `writing-plans.mdc` §0「CSS shorthand 能力 grep 表」）：vx parser 对各 shorthand 支持参差不齐
4. **算法伪码「累积合并」vs「覆盖」语义必须 explicit method**（P1 升级 → `creative.md` §d.2）：`=` 是高歧义符号，必须用 `MergeFrom` / `Replace` / `swap`
5. **性能优化作为副产品 vs 主线任务的判定**（P2 沉淀 → `systemPatterns.md`「副产品优化 3 标准」）：本 Round bench 触发 / ≤ 5 行修改 / 不引入新结构性改动

### 4.1 反复模式识别

- ✅ **「前置依赖/环境/API 能力未验证」第 9+ 次**（CSS parser border-bottom shorthand + R3 测试边界假设）→ 升级 P1 条目 #2/#3
- ✅ **「API 传递语义决策未做多级 mental trace」**（新候选模式首次定型）→ 升级 P0 条目 #1
- ✅ **「算法伪码赋值符歧义」**（新候选模式首次定型）→ 升级 P1 条目 #4

### 4.2 TASK-26-01 升级规则 ROI 首次外部任务验证

| 规则段 | 触发场景 | 实战表现 | ROI 评估 |
|---|---|---|---|
| `writing-plans.mdc` §7.0.1 同窗口 stash-swap | P6.2 bench 验证 | ✅ 7 BM 一次过；CV 5-8% 可信信号；副产物发现 P6.2 优化点 | 🟢 **高 ROI** |
| `writing-plans.mdc` §9.2 默认值边界 | `incoming` 默认 empty / `outgoing` blocks_bottom 默认 empty | ✅ plan 阶段锁定；零反直觉默认值 | 🟢 中 ROI |
| `writing-plans.mdc` §9.3 D3 反向探针强制 | P1 / P5 测试设计 | ✅ 3/3 反向探针完整循环（第 6 次实战） | 🟢 高 ROI |
| `writing-plans.mdc` §9.1 跨 LayoutType 独立 box-model | LayoutBlockChild 改造 | ⊘ 不触发（仅触 kBlock 路径） | 🟡 不可证伪 |
| `subagent-development.mdc` D3 重评估 | plan 阶段未标 D3 子代理 | ⊘ 不触发（单线程递归依赖不适合并行） | 🟡 不可证伪 |

**整体结论：** 5 条规则中 3 条触发，全部高/中 ROI 验证；2 条不触发但留存合理。规则有效，建议长期保留。

---

## 5. plan × 0.6 估时数据点

**第 14 数据点（Level 3 单 Round 单子系统）：**

| 维度 | 值 |
|---|---:|
| plan 估时 | 390 min（~6.5h）|
| plan × 0.6 准确档 | 234 min（~3.9h）|
| 实测 | ~180 min（~3h）|
| 比例 | **0.46×** |

**落点分析：** 介于「准确档 0.5-0.6×」（R1/R2 子任务 0.50/0.56）与「中位档 0.37-0.50×」（R3/R4 子任务 0.37/0.50）之间偏中。Level 3 单子系统 + 决策已 plan 阶段锁定 + VAN grep 实证三联合 → 0.40-0.50× 区间稳定。

---

## 6. 提交清单

| # | SHA | 主题 |
|:-:|-----|------|
| 1 | `b66ec3d` | docs(van): TASK-20260430-01 init first/last child margin collapse with parent |
| 2 | `ea43ed3` | docs(plan): TASK-20260430-01 design + plan (D1-D5 locked) |
| 3 | `13a46c3` | test(layout): TASK-20260430-01 P1.1 RED tests for first/last child main flow |
| 4 | `5824071` | test(layout): TASK-20260430-01 P1.2 RED tests for 5 blocking conditions |
| 5 | `c2f8233` | test(layout): TASK-20260430-01 P1.3 RED tests for deep chain + collapse-through |
| 6 | `3b94941` | feat(layout): TASK-20260430-01 P2-P5.2 GREEN — first/last child margin collapse with parent |
| 7 | `2ab9a63` | test(wpt): TASK-20260430-01 P6.1 wpt-005 SKIP -> PASS (direct verification target) |
| 8 | `0ed39be` | perf(layout): TASK-20260430-01 P6.2 last-in-flow-block pointer hoisting |
| 9 | `4b3a0f3` | chore(build): finalize TASK-20260430-01 memory bank state |
| 10 | （归档）| docs(reflect): add reflection for TASK-20260430-01 |
| 11 | （归档）| docs(archive): add archive for TASK-20260430-01 |

---

## 7. 参考文档

- 设计 spec：`docs/specs/2026-04-30-margin-collapse-with-parent-design.md`
- 实现 plan：`docs/plans/2026-04-30-margin-collapse-with-parent.md`
- 回顾文档：`memory-bank/reflection/reflection-TASK-20260430-01.md`
- 创意设计：⊘ 不需要（5 决策 D1-D5 已在 PLAN 头脑风暴中锁定，无 UI/算法/架构空白）
- 上游归档：`memory-bank/archive/archive-TASK-20260426-01.md`（TASK-26-01 R3 #20 sibling collapse + R4 #21 LineBox）
- 升级规则：`.cursor/rules/skills/writing-plans.mdc` §9.4（P0）/ §0 既有测试 fingerprint（P1）/ §0 CSS shorthand grep（P1）；`.cursor/commands/creative.md` §d.2（P1）

---

## 8. 后续触发型候选（未立项，等待触发条件）

- **CSS parser `border-bottom` shorthand 缺失**（P3 触发型，本任务 build 副发现）：触发条件 — 下次 layout / 视觉测试用到 border shorthand 时
- **TASK-26-02-full**（P3 触发型）：clearance 完整版（依赖 float/clear CSS 属性，需独立 Level 4）
- **TASK-26-03**（P3 触发型）：LayoutInline 内部 IFC 递归 + bidi LTR 假设破除（受 D2.D inline-block atomic 决策限留）

---

**归档人：** AI Agent
**归档日期：** 2026-04-30
