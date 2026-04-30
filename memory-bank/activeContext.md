# 活跃上下文

## 当前阶段

**构建完成（2026-04-30 22:25）** — TASK-20260430-02 BUILD 全过程完成；R1（4 方向 shorthand）+ R2（3 属性级 shorthand）共 7 shorthand 全部实施 + 双反向探针完整三态；ctest Debug 1061/1061 + Release 1030/1030 + Release `-O3 -Werror` 0 err/warn；A1-A7 验收全过；A8 待 /reflect 评估；下一步 `/reflect`。

## 当前任务

**TASK-20260430-02：CSS border shorthand 补全（4 方向 + 3 属性级）[安全相关]** — Level 2

- **目标：** 补全 W3C CSS 2.1 §8 / §16 标准的 7 个 border shorthand（`border-top` / `-right` / `-bottom` / `-left` 4 方向 + `border-width` / `border-style` / `border-color` 3 属性级），全部展开为既有 12 longhand，零新 PropertyId / 零 enum 改动；同时验证 TASK-30-01 升级规则 §0「CSS shorthand 能力 grep 表」首次外部任务 ROI（自我应用：本任务自身是该规则的应用样板）
- **复杂度：** Level 2（多文件修改 + 模式 100% 复用既有 `border` / `padding` 范本）
- **来源：** TASK-20260430-01 archive §改进建议 P3 触发型「CSS parser `border-bottom` shorthand 缺失」直接落实
- **安全相关：** ✅ 是（CSS parser declaration 展开 N-cap 护栏 + 与上游 HTML inline style 三件套护栏交互验证）
- **分支：** `feature/TASK-20260430-02-css-border-shorthand`（基于 main `6b36c87`）

### VAN 阶段产出（2026-04-30 21:50）

| # | 维度 | 选择 | 理由 |
|:-:|---|---|---|
| V1 | 范围 | **A** 全量 7 shorthand（4 方向 + 3 属性级）| 高内聚一次性补全；reviewer 分歧最小；下次同类问题不重复 |
| V2 | 拆分策略 | **多轮次 Build**：R1 = 4 方向 / R2 = 3 属性级 | 用户决策；R1 为 TASK-30-01 直接触发受益；R2 与 padding/margin 完全同模式机械补全 |
| V3 | Git 分支 | `feature/TASK-20260430-02-css-border-shorthand` | 基于 main `6b36c87` |
| V4 | 复杂度 | **Level 2** | 多文件修改 + 模式 100% 复用 + 无架构空白；不需要 /creative |
| V5 | 安全标注 | ✅ **是** | CSS parser declaration 展开 N-cap 护栏 + 与上游 HTML inline style 三件套护栏交互验证 |

### VAN 阶段代码实证（落实 P0 + 升级规则 §0「CSS shorthand 能力 grep 表」自我应用）

| # | 假设/命题 | grep 实证 | 影响设计 |
|:-:|---|---|---|
| F1 | `border` 总称 shorthand 现状 | ✅ `parser.cc:517-597` 整段 4-side expansion；`parser_test.cc:319 BorderShorthand` 覆盖 | 范本可复刻 |
| F2 | `border-top/right/bottom/left` 4 方向 shorthand 现状 | ❌ 完全无实现（`property.cc:49-60` 只列 12 longhand）| TASK-30-01 副发现完全确认 |
| F3 | `border-width/style/color` 3 属性级 shorthand 现状 | ❌ 完全无实现（同 F2）| 一并补全 |
| F4 | PropertyId 枚举需扩展吗？ | ❌ 不需要（直接展开成现有 longhand）| 零 ABI 风险 |
| F5 | 测试 fingerprint 是否充足 | ✅ `ShorthandPadding` (4-value) + `BorderShorthand` (12 expansion) 双范本 | 测试范本 100% 复用 |
| F6 | FetchContent 代理 / 离线状态 | ✅ `_deps/` 已离线预置 | 跳过 proxy 守卫 |

### PLAN 阶段产出（2026-04-30 21:55）

#### 决策矩阵（D1-D5 已锁定）

| # | 维度 | 选择 | 理由 |
|:-:|---|---|---|
| D1 | 4 方向 shorthand 实现模式 | **A 复制粘贴** 既有 `border` 模板 4 次 | 与现有 shorthand 风格一致；reviewer 心智成本最低 |
| D2 | 3 属性级 shorthand 实现模式 | **A 复制粘贴** 既有 `padding/margin` 模板 3 次 | 3 个 value parser（Length/Enum/Color）类型异构，统一 helper 反需类型擦除 |
| D3 | 测试深度 | **C 完整档**（25 测试：12 R1 + 13 R2）| V5 安全 + §9.3 反向探针强制；双入口 + fingerprint 互斥不退化 |
| D4 | 安全护栏复用策略 | **A 完全复用** 既有 N-cap (3-iter / 4-iter) | 上游三件套 + 既有 N-cap 完整覆盖 T1-T8；零新护栏 |
| D5 | R1/R2 Phase 划分粒度 | **A 2 GREEN commits** | R1 4 shorthand 同模式 1 commit / R2 3 shorthand 同模式 1 commit |

#### 威胁建模（T1-T8）

- T1-T5：上游 HTML inline style 三件套护栏（count 1000 / value 8KB / 黑名单）覆盖
- T6（per-shorthand 内部 token cap）+ T8（4-value 上限）：复用既有 N-cap，每分支独立专测
- T7（over-match 误绑）：`EqualsIgnoreCase` 严格相等天然防御
- 结论：零新护栏需求

#### 文档落盘

- 设计 spec：`docs/specs/2026-04-30-css-border-shorthand-design.md`（11 段）
- 实现 plan：`docs/plans/2026-04-30-css-border-shorthand.md`（8 Phase / 25 测试 / 5 commits）

#### 验收要点（A1-A8）

- A1 4 方向 shorthand 双入口解析正确（R1 12 单测 PASS）
- A2 3 属性级 shorthand 1-4 值规则同 padding/margin（R2 13 单测 PASS）
- A3 既有 `BorderShorthand` 测试不退化（ctest 1039+ 全 PASS）
- A4 DoS 护栏 T6/T8 每分支独立专测 PASS
- A5 §9.3 反向探针 ≥ 2 处（R1.3 + R2.3）
- A6 Release `-O3 -Werror` 0 err/warn
- A7 ctest 全量 PASS（基线 1039 → 1064）
- A8 TASK-30-01 §0 升级规则首次外部 ROI 验证

### 验证 TASK-30-01 升级规则的 ROI（首次外部任务）

本任务作为同类型 CSS / 解析器任务，将首次验证 TASK-20260430-01 沉淀的 4 条规则在新任务中的实际表现：

| 规则段 | 触发场景 | ROI 验证标的 |
|---|---|---|
| `writing-plans.mdc` §0「CSS shorthand 能力 grep 表」| 自我应用（本任务自身是该规则应用样板）| 验证 grep 表模板对 border 系列 shorthand 是否充分 |
| `writing-plans.mdc` §0「既有测试隐式契约 fingerprint」| 验证 R2 父任务 `BorderShorthand` 12-decl expansion 测试隐含的「未指定值不展开」假设 | 验证规则是否避免 R2 既有测试退化 |
| `writing-plans.mdc` §9.4 递归算法 API 决策必检项 | ⊘ 不触发（CSS parser 非递归，仅循环展开）| 不可证伪 |
| `creative.md` §d.2 算法伪码累积语义 | ⊘ 不触发（本任务无算法伪码）| 不可证伪 |

## 最近完成

- **TASK-20260430-01：first/last child margin collapse with parent（CSS 2.1 §8.3.1）** — Level 3 ✅ 已归档（2026-04-30）
  - 归档：`memory-bank/archive/archive-TASK-20260430-01.md`
  - 4 P0/P1 改进建议全部落实（已生效，本任务首次外部验证）

## 待处理事项（P0/P1/P2 后续）

### 长期沉淀（已写入 systemPatterns / techContext / 规则文件）

- **TASK-30-01 升级规则 4 条 ROI 部分由本 TASK-30-02 验证**（详见上方「验证 TASK-30-01 升级规则的 ROI」）
- **TASK-26-01 升级规则 ROI 已部分验证**（TASK-30-01 首次外部触发：§7.0.1 / §9.2 / §9.3 高/中 ROI）

### P2/P3 触发型候选

- **TASK-26-02-full**（P3 触发型）：clearance 完整版（依赖 float/clear CSS 属性，需独立 Level 4）
- **TASK-26-03**（P3 触发型）：LayoutInline 内部 IFC 递归 + bidi LTR 假设破除
- **TASK-20260424-02**（P3 触发型）：Layout 残余 super-linear 调查

## 下一步

- 执行 `/reflect` 启动回顾阶段
  - 评估 plan × 0.6 第 15 数据点：实测 ~37 min ÷ plan 170 min = 0.22×（接近 TASK-30-01 P6 极速档）
  - 评估 A8：TASK-30-01 §0 升级规则首次外部任务 ROI
  - 记录设计偏差：spec §5.3/5.4 描述「逐方向/逐属性独立分支」实际选择「单分支聚合 + name/Mode 映射」（仿既有 margin/padding 模式），等价决策不悖 D1/D2=A 精神
  - 提取潜在改进建议
  - 准备 /archive

## BUILD 完成证据

- 5 commits（PLAN docs / R1 RED / R1 GREEN / R2 RED / R2 GREEN）+ 1 finalize 待提交
- Debug ctest 1061/1061 PASS（基线 1039 + R1 10 + R2 12）
- Release ctest 1030/1030 PASS + `-O3 -Werror` 0 err/warn
- 双反向探针（R1.3 mis-route PropertyId / R2.3 mis-route 2-value 展开）完整三态切换证据

## 未合并分支

- `feature/TASK-20260430-02-css-border-shorthand` — TASK-20260430-02 BUILD 完成（基于 main `6b36c87`），等待 /reflect

## 最近归档（速查，详细见 archive 文档）

- `archive-TASK-20260430-01.md`（first/last child margin collapse with parent Level 3，2026-04-30）
- `archive-TASK-20260426-01.md`（Layout 正确性消化 Level 4，2026-04-30）
- `archive-TASK-20260425-01.md`（SDL2 窗口后端 + 输入事件桥接 Level 3，2026-04-26）
- `archive-TASK-20260424-04.md`（DrawText warm 残余优化 Level 2 D 纯收尾，2026-04-25）
- `archive-TASK-20260424-03.md`（DrawText warm 优化 Level 2-3 K7 Resolved，2026-04-24）
- `archive-TASK-20260424-01.md`（Layout super-linear knee 根因调查，2026-04-24）
- `archive-TASK-20260419-13.md`（流程规则 P0/P1 沉淀冲刺，2026-04-19）
- `archive-TASK-20260419-11.md`（ImageCache::Load HashMap 化，2026-04-19）
- 更早归档见 `memory-bank/archive/` 目录与 `tasks.md §任务历史`
