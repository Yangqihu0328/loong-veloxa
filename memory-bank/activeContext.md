# 活跃上下文

## 当前阶段

规划中（2026-04-30 19:50）— **TASK-20260430-01: first/last child margin collapse with parent**（CSS 2.1 §8.3.1 嵌套规则）PLAN 完成；5 决策矩阵（D1-D5）锁定；spec + plan 文档落盘；7 Phase / 14 任务划分；估时 ~6.5h plan / ~3.9h plan×0.6。不需要 `/creative`。下一步 `/build`。

## 当前任务

**TASK-20260430-01：first/last child margin collapse with parent（CSS 2.1 §8.3.1 嵌套规则）** — Level 3

- **目标：** 完成 TASK-26-01 R3 #20 留下的 D1.2 LayoutChild API 边界限留 — 让 MarginChain 跨 LayoutBlock 函数边界 propagate，实现 W3C CSS 2.1 §8.3.1 中 first/last child 与 parent 的 margin collapse + 阻断条件（parent padding-top/border-top/min-height）
- **复杂度：** Level 3（单子系统 Layout + API 设计决策 + 跨函数 chain propagate）
- **设计 spec：** `docs/specs/2026-04-30-margin-collapse-with-parent-design.md`
- **实现 plan：** `docs/plans/2026-04-30-margin-collapse-with-parent.md`
- **来源：** TASK-20260426-01 archive §10 P3 触发型「TASK-26-02」占位 + reflection §4.12 / activeContext 待处理事项 / wpt-005 SKIP-w/-rationale 现实直接验证目标
- **范围限定：** ❌ 不动 clearance / float / clear（CSS layer 零实现，VAN F2 实证；clearance 完整版独立 Level 4 任务）
- **直接验证目标：** **wpt-005 SKIP → PASS**（`Wpt005_NonSiblingAdjoiningMarginsCollapse`）
- **安全相关：** ❌ 否（纯 layout 算法）
- **分支：** `feature/TASK-20260430-01-margin-collapse-parent`（基于 main `a84d30d`，已创建）
- **下一步：** `/build`（不需要 /creative，5 决策已在 PLAN 头脑风暴中锁定）

### PLAN 阶段产出（2026-04-30 19:50）

| # | 维度 | 选择 |
|:-:|---|---|
| D1 | API 改造策略 | **A1** 新增 `LayoutBlockChild` 专用辅助；`LayoutChild` 不动 |
| D2 | 传递语义 | **A** by-value in / by-value out (POD 12B) |
| D3 | 阻断条件覆盖 | **A** 完整规范子集（padding/border + BFC root + height + min-height）|
| D4 | 测试深度 | **B** 完整档（10 单测 + 3 反向探针 + wpt-005）|
| D5 | Phase 划分 | **B** 7 Phase 细粒度 |

### VAN 阶段产出

| # | 维度 | 选择 | 理由 |
|:-:|---|---|---|
| V1 | 范围 | **A 子项**（first/last child collapse with parent）| clearance 完整版依赖 float（CSS layer 零实现，F2 实证），独立 Level 4 任务承接 |
| V2 | 拆分策略 | **单 Round 一次过 + plan 内部 Phase 划分** | Level 3，无明显多 Round 边界 |
| V3 | Git 分支 | **feature/TASK-20260430-01-margin-collapse-parent** | 基于 main `a84d30d` |
| V4 | 复杂度 | **Level 3** | 单子系统 Layout + API 设计决策（LayoutChild 跨函数 chain 透传协议）|
| V5 | 安全标注 | ❌ 不标 | 纯 layout 算法 |

### VAN 阶段代码实证（落实 P0「方案根因假设未先验证」）

| # | 假设/命题 | grep 实证 | 影响设计 |
|:-:|---|---|---|
| F1 | clearance 当前是否 stub | ✅ 是 | A 子项不动 clearance |
| F2 | CSS clear/float 属性解析状态 | ❌ **完全无实现**（仅 transition.cc 的 kFloat 数值类型，无关）| 范围拆分关键依据 |
| F3 | first/last child collapse 现状 | ❌ 未实施（`layout_engine.cc:446-451` 显式 P3 TASK-26-02 注释）| 本任务从零实施 |
| F4 | LayoutChild API 现状 | ⚠️ 无 chain 参数，需扩展 in/out | 3 处 callers + 5 处 LayoutType 分支影响面 |
| F5 | wpt-005 内容 | ✅ nested margin collapse 场景，本任务直接验证目标 | 第一首选验证 |
| F6 | FetchContent 代理 | ✅ `_deps/` 已离线预置，不引入新依赖 | 跳过 proxy 守卫 |

### 验收要点（待 /plan 精化）

- ctest 全量 PASS（基线 1029/1029；预计 +10-15 new test cases）
- Release `-O3 -Werror` 0 err/warn
- `bench_layout_*` baseline ≤ +10%（**§7.0.1 同窗口 stash-swap 对照强制**，TASK-26-01 升级规则首次外部任务验证）
- **wpt-005 SKIP → PASS**（核心验收信号）
- W3C §8.3.1 嵌套 collapse 至少 5-7 单测 + wpt-005 PASS
- 反向探针 ≥ 1 处（**§9.3 Mixed TDD D3 类强制**）

### 验证 TASK-26-01 P0/P1 升级规则的 ROI（首次外部任务）

本任务作为同类 Layout 正确性任务，将首次验证 TASK-20260426-01 沉淀的 5 条规则在新任务中的实际表现：

| 规则段 | 触发场景 | ROI 验证标的 |
|---|---|---|
| `writing-plans.mdc` §7.0.1 同窗口 stash-swap | bench 验证（layout 改造必需）| 验证规则是否避免跨时间窗误判 |
| `writing-plans.mdc` §9.1 Layout 必检项 | LayoutChild API 改造（跨 LayoutType）| 验证默认值语义 + 独立 box-model 是否被 plan 阶段强制锁定 |
| `writing-plans.mdc` §9.3 Mixed TDD 反向探针 | 新增回归测试（D3 类）| 验证规则强制条目是否被 plan 阶段引用 |
| `.cursor/commands/creative.md` d.1 坐标约定 | 不触发（本任务无多坐标系算法）| ⊘ 不验证 |
| `subagent-development.mdc` D3 重评估 | plan 阶段标 D3 子代理 → build 阶段重评估 | 验证 build 阶段触发条件是否清晰 |

## 最近完成

- **TASK-20260426-01：Layout 正确性消化（#25 + #28 + #20 + #21）** — Level 4 ✅ 已归档（2026-04-30）
  - 归档：`memory-bank/archive/archive-TASK-20260426-01.md`
  - 关键结果：ctest 1029/1029 PASS；同窗口对照 bench mean -3.6% / median +2.65%
  - 5 条 P0/P1 规则升级落地，本任务（TASK-20260430-01）首次外部验证

## 最近完成

- **TASK-20260426-01：Layout 正确性消化（#25 + #28 + #20 + #21）** — Level 4 ✅ 已归档
  - 归档：`memory-bank/archive/archive-TASK-20260426-01.md`
  - 回顾：`memory-bank/reflection/reflection-TASK-20260426-01.md`
  - 关键结果：ctest `1029/1029 PASS`；同窗口对照 bench Flat/64 `mean -3.6% / median +2.65%`
  - 安全：#28 inline style 路径三件套护栏（count cap / value cap / 黑名单）
  - 技术债闭环：`techContext.md` #20/#21/#25/#28 已全部标记 ✅

## 待处理事项（P1/P2 后续）

- **P1（已落实，等待同类任务验证 ROI）**：TASK-20260426-01 新增规则已全部落地（archive §9 已确认 ✅），下次同类（Layout 正确性）任务命中时验证有效性
  - `writing-plans.mdc` §7.0.1：同窗口 stash-swap 对照（含 ninja 时间戳防护补丁）✅
  - `writing-plans.mdc` §9.1：Layout 必检项（默认 width/height 语义 + 跨 LayoutType 独立 box-model）✅
  - `writing-plans.mdc` §9.3：Mixed TDD D3 类反向探针强制条目 ✅
  - `.cursor/commands/creative.md` d.1：多坐标系算法「单一坐标约定 + 公式表」一图 ✅
  - `subagent-development.mdc`：D3 子代理 build 阶段重评估机制（含触发条件 + 4 R3/R4 实证表）✅
- **P2/P3 候选**：
  - `TASK-26-02`（P3 触发型）：clearance + first/last child margin collapse 完整实施（受 D1.2 LayoutChild API 边界限留）
  - `TASK-26-03`（P3 触发型）：LayoutInline 内部 IFC 递归 + bidi LTR 假设破除 + block 含 inline 匿名 IFC（受 D2.D inline-block atomic 决策限留）
  - 详细历史长期项 + 30+ P1/P2/P3 待办见 `memory-bank/tasks.md` §待立项候选 + 各 archive 文档 §改进建议闭环

## 下一步

- 执行 `/build` 启动 TASK-20260430-01 实施（按 plan §P0-P6 共 7 Phase 推进）
  - **P0：** grep + wpt-005 拆解 + 基线核验 + spec/plan commit（~30 min plan / ~18 min ×0.6）
  - **P1：** RED 单测全套（10 单测 + 3 反向探针位置）（~60 min / ~36 min）
  - **P2：** API 改造 + dispatch（LayoutBlockChild skeleton 调通编译）（~30 min / ~18 min）
  - **P3：** GREEN first child collapse（~45 min / ~27 min）
  - **P4：** GREEN last child collapse + 4 阻断条件（~90 min / ~54 min）
  - **P5：** 反向探针验证 + deep chain + collapse-through 递归（~60 min / ~36 min）
  - **P6：** wpt-005 + bench 同窗口 + 收尾（~75 min / ~45 min）
  - **直执行**（不引入子代理；子代理 D3 评估：单子系统 + 决策已锁定 + 强递归依赖单线程，不适合并行）

## 未合并分支

- `feature/TASK-20260430-01-margin-collapse-parent` — TASK-20260430-01 PLAN 完成（基于 main `a84d30d`），等待 /build

## 最近归档（速查，详细见 archive 文档）

- `archive-TASK-20260426-01.md`（Layout 正确性消化 Level 4，2026-04-30）
- `archive-TASK-20260425-01.md`（SDL2 窗口后端 + 输入事件桥接 Level 3，2026-04-26）
- `archive-TASK-20260424-04.md`（DrawText warm 残余优化 Level 2 D 纯收尾，2026-04-25）
- `archive-TASK-20260424-03.md`（DrawText warm 优化 Level 2-3 K7 Resolved，2026-04-24）
- `archive-TASK-20260424-01.md`（Layout super-linear knee 根因调查，2026-04-24）
- `archive-TASK-20260419-13.md`（流程规则 P0/P1 沉淀冲刺，2026-04-19）
- `archive-TASK-20260419-11.md`（ImageCache::Load HashMap 化，2026-04-19）
- 更早归档见 `memory-bank/archive/` 目录与 `tasks.md §任务历史`
