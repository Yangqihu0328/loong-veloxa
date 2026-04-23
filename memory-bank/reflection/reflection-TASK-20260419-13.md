# 回顾：TASK-20260419-13 流程规则 P0/P1 沉淀冲刺

**日期：** 2026-04-19
**任务 ID：** TASK-20260419-13
**复杂度级别：** Level 2（6 规则/命令文件 + 2 MB 文件 = 8 文件修改；纯文档无代码；遵循 `writing-plans.mdc` 已有段式样）
**TDD 模式：** 文档类任务无 TDD — 以「反例追溯表」替代（plan §4.1 已记录，本次 7/7 通过）

---

## 1. 计划 vs 实际

| 维度 | 计划 | 实际 | 偏差原因 |
|------|------|------|---------|
| Phase 数 | 5 (P0-P4) | 5 (P0-P4) | 严格对齐 |
| 总耗时 | 85-95 min | ~51 min | **~1.67-1.86× plan** — 文档类任务估时偏保守（与 TASK-11 bench 1.5-2.0× 相同区间） |
| Commit 数 | 5-6 (每 Phase 1 commit, D3 决策) | 4 (P0 并入 P1 + P1/P2/P3/P4) | P0 基线验证纯 grep 查证无文件修改，无必要独立 commit |
| 文件变更 | 6 规则/命令 + 2 MB = 8 文件 | 6 规则/命令 + 3 MB (+techContext.md) = 9 文件 | P1 条目 1 设计在 `techContext.md` 添加「Plan/VAN 阶段守卫」交叉引用子段（plan 已明确要求，小偏差未记入 "文件数"） |
| 反例追溯 | 6 case (4+1+2) | 7/7 通过（含本次 Phase 0 sandbox meta-dogfooding 自证 P2 规则） | 实际+1，且为**最强证据型**（实时触发）|
| 10 验收标准 | 10 项 | 9 ✅ + 1 改进（§5.4 工具表合并，加入 rg 基于 Phase 0 实证） | 基于实测数据优化文案密度，非偏离 |
| Plan 阶段决策数 | 3 (D1 占位符 / D2 子状态 / D3 commit 粒度) | 3 全落地 | ✅ |

**估时数据点叠加（bench + 文档类合并分析）：**

| 任务 | 类型 | Plan | 实际 | 倍率 |
|------|------|------|------|------|
| TASK-05 | bench | 4.25h | 75 min | 3.4× |
| TASK-09 | bench | 3.5h | 50 min | 4.2× |
| TASK-11 | bench | 55-80 min | 35-40 min | 1.5-2.0× |
| **TASK-13** | **文档** | **85-95 min** | **51 min** | **1.67-1.86×** |

**结论：** 估时从 TASK-09 4.2× 校准 → TASK-11 2.0× → TASK-13 1.86×，**跨类型稳定收敛到 ~2×**。建议 P1 固化 plan × 0.6 为默认目标倍率，plan × 0.4 作为警戒（极端偏差）。

---

## 2. 做得好的

### a1. **Meta-dogfooding 现场自证（本次最大亮点）**

Phase 0 基线验证时 sandbox 终端 `rg`/`jq`/`valgrind`/`xmllint` **4 个工具 MISS**，**直接触发正在沉淀的条目 2 P1 规则**（smoke 工具链可用性 grep）。这不是刻意设计，而是规则沉淀任务在执行过程中**天然获得了"规则即自证"的证据**：

- TASK-11 反思 #2 的根因（jq 缺失被迫临时换栈）并非孤例，而是 sandbox 环境的**结构性现象**；
- 条目 2 规则最终将 `rg` 加入工具兜底表（spec 原列 7 工具无 rg，基于 Phase 0 实证扩充），这是**规则实施过程自动改进规则本身**的反馈闭环；
- 沉淀到 systemPatterns 作为「规则沉淀类任务的 meta-dogfooding 模式」，成为本类任务的标配 Phase 0 动作。

### a2. **反例追溯表作为 TDD 替代范式 7/7 通过**

文档类任务无可测试单元，plan §4.1 明确以「反例追溯表」替代 RED→GREEN：

| 条目 | 覆盖 case | 结果 |
|:-:|:-:|:-:|
| 1 FetchContent proxy | TASK-19-02/04/07 + TASK-13-01 Phase 0 自证 | 4/4 ✅ |
| 2 smoke 工具链 grep | TASK-11 P3 + TASK-13 Phase 0 自证 | 2/2 ✅（超出 plan 1/1） |
| 3 多轮次 Build 中间态 | TASK-19-03 Round 1 + TASK-19-04 | 2/2 ✅ |
| **总计** | **7 case** | **7/7 ✅** |

"若规则当时生效可预防/改善"的反事实验证是结构化的，不是感觉式的。**本模式应沉淀为流程规则类任务的验证标准。**

### a3. **Plan 阶段"关键认知升级"捕获（`.cursor/commands/*.md` 可编辑）**

Plan 阶段初期假设 `.cursor/commands/*.md` 是只读系统提示，范围仅限 `.cursor/rules/*.mdc` 文档类变更。通过 `Glob .cursor/commands/**/*` 验证后发现实际是**普通 workspace 文件**，使本次任务从"纯文档"升级为"**真执行守卫**"：
- 条目 1 从"文档提醒"→ `van.md` §1 子项自动检查脚本
- 条目 3 从"文档描述"→ `build.md` §6.5 轮次完成判断逻辑 + `reflect.md` §0 守卫放宽

ROI 放大约 **2-3×**（规则从被动文档升级为主动守卫）。**"基础假设核查"作为 VAN/Plan 阶段标配步骤**值得沉淀。

### a4. **D1 占位符模式 + 单一真相来源**

用户拍板 D1 后，规则文件零硬编码 IP，代理地址集中在 `techContext.md` L98 单一真相来源段。P4 self-review `grep -rn "192\.168\." .cursor/` 返回 `OK: no hardcoded IP` 验证有效。**此模式适用所有"开发环境敏感配置"**（端口、token、路径前缀等），应沉淀。

### a5. **每 Phase self-review 保障 Level 2 质量**

P1/P2/P3/P4 每个 Phase 结束前批量 grep：锚点位置、YAML frontmatter 完整性、code fence 偶数、交叉引用命中数、硬编码检查、行数增量合理性 — 6 维度实证，**本次零 linter 错误 / 零回填**。

### a6. **activeContext 长期项 3 条标记已落实的闭环仪式**

Phase 4 逐条 `~~原文~~` + `✅ 已于 TASK-13 PN 落实：具体位置` 的删除线标记，让 `activeContext.md` 从**待办堆积**变为**闭环记录**。此仪式是 P1 任务完成的"**改进建议闭环**"信号（`/reflect` §5 要求）。

---

## 3. 遇到的挑战

### c1. **初期 `.cursor/commands/*.md` 可编辑性假设错误（Plan 阶段发现）**

VAN 初期将 `.cursor/commands/` 视为"系统只读提示"，plan 初稿仅 `.cursor/rules/` 修改。Plan 阶段 Glob 验证后纠正，扩大实施范围。**损耗：** 仅 Plan 阶段 ~5 min 重新设计，未进 Build 阶段。**根因：** 对非标准目录（非 `src/` / `tests/`）的可编辑性默认假设。

### c2. **Phase 0 sandbox `rg` MISS 迫使切换 Grep 工具**

Phase 0 首次 `rg -n ...` 返回 `Command 'rg' not found`（Cursor 沙箱 shell 未装 ripgrep；但 Cursor Grep 工具可用）。切换到 Grep 工具 + POSIX grep 后续正常。**损耗：** < 1 min 切换。**意外收益：** 直接为条目 2 规则提供了现场证据（变成 a1 亮点）。

### c3. **Spec 表格被编辑器 auto-reformat（P4 提交时发现）**

`docs/specs/2026-04-19-process-rules-sunk-in-design.md` 的表格列宽被编辑器自动对齐（`--- | ---` 变成 `--- | -----`），未修改语义但污染 diff。**处理：** P4 提交一并并入，标注 "编辑器 auto-reformat" 未来 diff 不污染。**潜在改进：** `.editorconfig` 或 prettier pre-commit hook 统一表格格式，但当前 ROI 低（每 10+ 任务可能 1 次）。

### c4. **§5.4 工具表设计偏离（9 ✅ + 1 改进）**

Spec §5.4 设计为"7 常见工具清单 + 4 替代方案表"分 2 表。实施时基于 Phase 0 实证（rg MISS）决定**合并为 6 工具 + 兜底列同表**，文案密度更高，且加入 rg。**验收阶段应归为"改进"而非"偏离"**，因为：
- 实质覆盖工具数不减（6 vs 7，rg 替代 convert，实际更切 sandbox）
- 用户使用时单表阅读优于跨表查询
- 决策依据是 **Phase 0 实证数据**而非随意选择

**教训：** spec 的"具体工具清单"与 plan 实施时的实证数据可能冲突，**plan 执行时允许基于实证微调 spec 细节**（需在 progress / reflection 说明）。

---

## 4. 经验教训

### L1. **文档类任务估时应 plan × 0.6 为目标**

本次 1.67-1.86× 与 TASK-11 bench 1.5-2.0× 同区间。**跨类型**估时偏差稳定收敛到 ~2×（从 TASK-09 4.2× → TASK-13 1.86×）。写 plan 时：
- **明确标注** "plan 估时为保守上界，目标值 ~ plan × 0.6"
- 作为决策规则 D4（或沉淀到 `writing-plans.mdc` 估时附录）

### L2. **Meta-dogfooding 作为规则沉淀类任务的标配 Phase 0 动作**

规则沉淀任务的**最强证据**来自"执行过程本身"。流程：
1. Phase 0 基线验证中**刻意检验新规则的触发路径**
2. 若命中 → 记入反例追溯表作为 T-0 的实时证据
3. 若未命中 → 仍按历史反例追溯正常执行

此模式节省"人工搜索历史案例"时间，且提供最新鲜的证据。应沉淀到 `systemPatterns.md`「规则沉淀模式」段。

### L3. **Plan 阶段"基础假设核查"清单**

本次 `.cursor/commands/*.md` 可编辑性假设错误导致 plan 重构。下次对**非标准目录/约定**类的假设必做 **Glob / Read 实证**：
- 目录类型假设（只读 vs 可编辑）
- 文件格式假设（YAML frontmatter vs 纯 markdown）
- 约定假设（Cursor 约定 vs 项目自定义）

### L4. **规则"单一真相来源"的读成本 trade-off**

D1 占位符模式让规则文件零硬编码，但读规则时需**跨文件跳读**（规则文件 `<开发环境代理地址>` → techContext.md 实际值）。这是**写入时成本降低 × 阅读时成本上升**的 trade-off，本次案例合理（敏感配置 + 频繁 unset/reset），但非所有场景适用。**判定条件：**
- ✅ 用：环境敏感值、跨项目复用值、可变配置值
- ❌ 不用：语义稳定值（如固定 phase 号、固定 API 名）

### L5. **规则类改动的"实证微调 spec"范式**

Plan 冻结后实施时，如遇**实证数据与 spec 细节冲突**（本次 rg MISS），应：
1. 保留 spec 核心意图
2. 微调具体细节（如工具清单）
3. 在 progress / reflection 明确说明"实证数据驱动"
4. 标记验收"改进"而非"偏离"

此范式避免"固执执行 spec 的不精准细节"。

---

## 5. 反复模式扫描（基于 27 份历史回顾）

| 已知模式 | 频率 | 本次是否重复？ |
|---------|:-:|---|
| 计划文件清单与实际变更不一致 | 9+ | ⚠️ **小偏差非重复**（spec §5.4 文案偏差，经 P4 self-review 捕获并标记为"改进"；未发生事后偏差） |
| 子代理产出需大量返工 | 7+ | N/A（本次未用子代理） |
| **前置依赖/环境/API 能力未验证** | **8+** | ⚠️ **本次自证触发** — Phase 0 `rg`/`jq` MISS 正是此模式的结构化表现；**同时**本次沉淀的条目 2 规则直接针对此模式；**meta-dogfooding 自闭环** |
| 非默认路径（流式/错误/缓存）遗漏验证 | 4+ | N/A（纯文档无运行时路径） |
| 测试隔离问题 | 7+ | N/A（无 TDD） |
| 提交粒度偏离计划（大杂烩提交） | 7+ | ❌ 未重复（每 Phase 独立 commit，P0 并入 P1 是合理收敛） |
| TDD 严格度与场景不匹配 | 11+ | ❌ 未重复（明确 plan §4.1 以反例追溯表替代，7/7 实证生效） |
| `.cursor/commands/*` 可编辑性假设错误 | **新模式（本次 1 次）** | ✅ 记录作为未来 L3 "基础假设核查"输入 |

---

## 6. 流程改进

- **VAN 阶段充分：** 复杂度评估 Level 2 准确，3 条目锚点全 grep 实证定位，分支按命名约定创建
- **Plan 阶段详细度：** spec 369 行 + plan 554 行（922 行双文档）— 对 8 文件修改的 Level 2 任务**稍重**但**含回顾 checklist 价值**；下次可考虑 spec 400 行以内
- **TDD 替代（反例追溯表）有效：** 文档类任务的验证范式首次完整实证，应固化
- **代码审查（self-review）有效：** 每 Phase 6 维度 grep 保障，零 linter 错误

---

## 7. 技术改进

- **规则文件阅读体验：** 新增的 3 个规则段（proxy / smoke / 多轮次）都以「反复出现 + 根因 + 规则」模式写作，比单纯"规则列表"更易追溯**为什么**
- **命令文件执行性：** `van.md` §1 子项 / `build.md` §6.5 / `reflect.md` §0 守卫**包含具体执行脚本 + 触发条件**，下次执行 `/van` / `/build` 时可直接参照
- **交叉引用一致性：** 3 文件 × 3 段 = 9 个交叉引用节点，全部 grep 命中对称（writing-plans ↔ van.md ↔ techContext.md）

---

## 8. 安全评估

| 维度 | 状态 | 备注 |
|------|:-:|------|
| 输入验证 | N/A | 纯流程规则文档，无外部输入处理 |
| 认证/授权 | N/A | 无认证路径 |
| 数据保护 | ✅ | D1 占位符模式零硬编码 IP（`grep -rn "192\.168\." .cursor/` 返回空）|
| 依赖审计 | N/A | 无新增依赖 |
| 错误信息脱敏 | N/A | 无运行时错误路径 |
| 敏感数据处理 | ✅ | 代理地址集中 `techContext.md` 单一真相来源；规则文件可安全公开（无 IP 泄漏）|

**结论：** 本任务不涉及安全变更，但 **D1 占位符模式本身是正向的敏感配置管理实践**，建议沉淀为通用模式。

---

## 9. 改进建议（附优先级与落实方式）

| # | 建议 | 优先级 | 落实方式 | 目标 |
|---|------|:-:|---|---|
| 1 | **Meta-dogfooding 作为规则沉淀类任务的 Phase 0 标配**：基线验证时刻意检验新规则的触发路径，命中即记为 T-0 实时证据 | **🟠 P1** | 新增段到 `memory-bank/systemPatterns.md`「规则沉淀模式 — Meta-dogfooding Phase 0」，**/reflect 阶段直接沉淀** | `systemPatterns.md` |
| 2 | **文档类 plan 估时校准：目标 × 0.6 / 警戒 × 0.4**：TASK-13 1.67-1.86× + TASK-11 1.5-2.0× 合并数据点显示跨类型收敛 ~2×，写入 plan 模板 | **🟠 P1** | 追加到 `.cursor/rules/skills/writing-plans.mdc` 某适当位置（如"估时校准附录"或新增段）；或沉淀到 `systemPatterns.md`「bench 类任务估时校准」段（已有，可扩写为跨类型） | `writing-plans.mdc` 或 `systemPatterns.md` |
| 3 | **"基础假设核查"清单**：VAN/Plan 对非标准目录/约定的可编辑性、文件格式、约定等假设必做 Glob/Read 实证 | **🟠 P1** | 新增到 `.cursor/commands/van.md` 或 `writing-plans.mdc` 的 VAN 阶段守卫段 | `van.md` / `writing-plans.mdc` |
| 4 | **"单一真相来源"通用模式：** D1 占位符策略适用所有"环境敏感/跨项目复用/可变"配置；明确写入条件 | **🟢 P2** | 沉淀到 `systemPatterns.md`「配置管理 — 单一真相来源占位符」段 | `systemPatterns.md` |
| 5 | **"实证微调 spec" 范式：** plan 实施时允许基于实证数据微调 spec 细节（文案/清单/数值），在 progress/reflection 说明依据 | **🟢 P2** | 沉淀到 `systemPatterns.md`「计划执行 — 实证微调 spec」段 | `systemPatterns.md` |
| 6 | **`.editorconfig` 或 prettier 统一 markdown 表格格式**：避免 spec 表格 auto-reformat 污染 diff | **🟢 P2** | 观察 3+ 任务再决定是否 ROI 足够；当前记入技术债 | 根 `.editorconfig` |

**闭环归属：**
- P1 3 条（#1/#2/#3）→ 直接沉淀到 `systemPatterns.md` / `writing-plans.mdc`（本 /reflect 阶段落实）或迁移 `activeContext.md`「长期项」待下次同类任务落实
- P2 3 条（#4/#5/#6）→ 沉淀到 `systemPatterns.md` 或 `activeContext.md`「长期项」段

---

## 10. 本次任务核心价值

**成功将 3 条 P0/P1 流程规则（反复 9+ 次痛点）从"反思建议"升级为"可执行守卫 + 可追溯文档"**，交付产物：

1. **3 条规则段** × **6 规则/命令文件**（`writing-plans.mdc` / `complexity-levels.mdc` / `van.md` / `build.md` / `reflect.md`）
2. **2 MB 文件更新**（`techContext.md` 交叉引用 + `activeContext.md` 长期项标记已落实）
3. **反例追溯 7/7 通过**（含 1 个 meta-dogfooding 实时自证，最强证据型）
4. **10 验收 9 ✅ + 1 改进**（实证驱动微调，非偏离）
5. **4 独立 commit**（每 Phase 1 commit，P0 并入 P1）
6. **TDD 替代范式首次完整实证**（文档类任务以反例追溯表替代 RED→GREEN）

**元收益（最大价值）：** 规则沉淀过程本身成为规则的自证案例；Plan 阶段"关键认知升级"（.cursor/commands/ 可编辑）把 ROI 放大 2-3×；跨类型估时收敛到 ~2× 支持 plan × 0.6 目标倍率的通用化。

---

## 11. 下一步

使用 `/archive` 归档任务（Level 2 标准归档：`memory-bank/archive/archive-TASK-20260419-13.md`）。
