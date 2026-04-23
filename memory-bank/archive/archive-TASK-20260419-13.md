# 归档：TASK-20260419-13 流程规则 P0/P1 沉淀冲刺

**日期：** 2026-04-19
**任务 ID：** TASK-20260419-13
**复杂度级别：** Level 2（6 规则/命令文件 + 3 MB 文件 = 9 文件修改；纯文档无代码）
**状态：** ✅ 已完成
**分支：** `feature/TASK-20260419-13-process-rules-sunk-in`（7 commits）
**来源：** `activeContext.md` 长期项累积的 3 条 P0/P1 流程规则积压

---

## 1. 任务概述

将 `activeContext.md`「长期项」段积压的 **3 条 P0/P1 流程规则**（反复 9+ 次痛点）从"反思建议"升级为"可执行守卫 + 可追溯文档"，一次性闭环 3 个条目：

| # | 优先级 | 来源 | 核心痛点 |
|:-:|:-:|---|---|
| 1 | 🔴 P0 | 反复 9+ 次（TASK-02/04/07 反思反复识别但只升 P1，TASK-03 Round 2 破例升 P0）| FetchContent 任务未先设 git 全局代理 → 首次 cmake 超时挂死，单次 5-10 min × N = 累计 ≥ 1h |
| 2 | 🟠 P1 | TASK-11 反思 #2 | Plan 阶段假设 smoke 工具链可用（jq / bc / valgrind 等），Build 才发现缺失返工 |
| 3 | 🟠 P1 | TASK-03 Round 1 首发 + TASK-11 反思复确 3+ 次 | Level 2+ phase ≥ 5 任务无「轮次完成」中间态，用户被迫等整个任务完成才能看回顾 |

**目标**：将 3 条从"被动文档"升级为跨 `.cursor/rules/*.mdc` + `.cursor/commands/*.md` 的「规则 + 命令守卫 + 交叉引用」闭环。

**关键认知升级**（Plan 阶段触发，ROI 放大 2-3×）：通过 `Glob` 实证发现 `.cursor/commands/*.md` 是**可编辑 workspace 文件**（非只读系统提示），使范围从纯文档扩展为真执行守卫。

---

## 2. 技术方案

### 2.1 多点联动设计

每条规则采用「规则定义 + 命令守卫 + 交叉引用」三点模式：

| 条目 | 规则文件（WHAT + WHY）| 命令文件（WHEN + HOW）| 交叉引用（WHERE）|
|:-:|---|---|---|
| 1 P0 proxy | `writing-plans.mdc` L96 新段（6 小节）| `van.md` §1 子项自动检查脚本 | `techContext.md` L98 子段（代理地址单一真相来源）|
| 2 P1 smoke | `writing-plans.mdc` §4 末尾 `####` 子块 | （Plan Phase 0 作为执行时机）| `verification.mdc` 协同定位 |
| 3 P1 多轮次 | `complexity-levels.mdc` L68 跨 Level 通用段 | `build.md` §6.5 轮次完成判断 + `reflect.md` §0 守卫放宽 | git-workflow.mdc commit 粒度段 |

### 2.2 D1 占位符模式（用户批准）

代理地址的**单一真相来源**是 `techContext.md`「FetchContent 与代理」段，规则文件零硬编码 IP，统一用占位符 `<开发环境代理地址>`：
- **写入时收益**：规则可安全公开、项目间迁移、避免硬编码泄漏
- **阅读时成本**：读规则时需跨文件跳读确认实际值
- **P4 self-review 验证**：`grep -rn "192\.168\." .cursor/` 返回空 ✅

### 2.3 D2 子状态标签模式

多轮次 Build 中间态**不新增独立阶段**，通过 `activeContext.md` 当前阶段段的**子状态标签**实现：

```markdown
## 当前阶段
构建中·轮次 N 完成（Phase P1-P3 of P5 ✅；下次进入 P4）
```

保持 5 主阶段骨架不变，`/reflect` §0 守卫识别子状态立即返回，`/build` §6.5 输出「轮次完成报告」而非「构建完成报告」。

### 2.4 D3 每 Phase 独立 commit

与 TASK-01/11 同模式，4 个独立 commit（P0 并入 P1 避免空 commit）便于按条目 review / 回滚。

### 2.5 反例追溯作为 TDD 替代（文档类任务验证范式）

无可测试单元，以「反例追溯表」结构化验证每条规则效力：

> 假设规则 X 在历史任务 T 时已生效，能否预防/改善 T 遇到的问题 P？

**7/7 通过**（含 1 个 Phase 0 meta-dogfooding 实时自证的 T-0 证据）。

---

## 3. 实现摘要

### 3.1 文件变更（9 文件）

| 操作 | 文件路径 | 说明 |
|:-:|---|---|
| 修改 | `.cursor/rules/skills/writing-plans.mdc` | L96 新段「FetchContent 网络代理守卫」(6 小节) + §4 末尾新增 `#### smoke 工具链可用性检查` 子块（6 工具 + 兜底列 + 执行时机 + verification 协同）|
| 修改 | `.cursor/rules/workflow/complexity-levels.mdc` | L68 新段「多轮次 Build 中间态」(跨 Level 2-4 通用，5 小节：触发条件 / 子状态协议 / 向前兼容 / 恢复路径 / git 关系) |
| 修改 | `.cursor/commands/van.md` | §1 子项「FetchContent 代理状态检查」(grep 触发 + 沙箱条件 + 离线 _deps 跳过 + 用户确认补设) |
| 修改 | `.cursor/commands/build.md` | §6.5「轮次完成判断」(触发条件 / 执行步骤 / 轮次完成报告模板 / 不触发兜底) |
| 修改 | `.cursor/commands/reflect.md` | §0 守卫放宽：识别 `构建中·轮次 N 完成` 子状态即立即返回 |
| 修改 | `memory-bank/techContext.md` | L98 子段「Plan/VAN 阶段守卫」(3 点交叉引用 + 代理地址单一真相来源声明) |
| 修改 | `memory-bank/activeContext.md` | 长期项 3 条标记已落实（`~~原文~~` + ✅ 落实位置 + 占位符策略说明）+ 新增 4 条 P1/P2 待处理 |
| 修改 | `memory-bank/progress.md` | 里程碑 VAN/Plan/P0-P4/Reflect 完整记录 + 估时偏差观察 + Reflect 关键发现 + 6 改进建议闭环 |
| 修改 | `memory-bank/tasks.md` | 状态 🟢 已完成 + 任务历史表追加 |
| 修改 | `memory-bank/systemPatterns.md` | +106 行：5 新模式沉淀 + bench 估时校准扩展为跨类型 |
| 新建 | `docs/specs/2026-04-19-process-rules-sunk-in-design.md` | 369 行设计规格（VAN Plan 阶段产出）|
| 新建 | `docs/plans/2026-04-19-process-rules-sunk-in.md` | 554 行实现计划（5 phase / 85-95 min 估时 / 10 验收）|
| 新建 | `memory-bank/reflection/reflection-TASK-20260419-13.md` | 228 行 Level 2 基础回顾 |

### 3.2 关键决策（3 决策 + 1 认知升级）

| # | 决策 | 选择 | 理由 |
|:-:|---|---|---|
| D1 | 代理地址处理 | **占位符 `<开发环境代理地址>`** | 规则零硬编码 IP；地址集中 `techContext.md` 单一真相来源；项目间迁移友好 |
| D2 | 条目 3 子状态实现 | **子状态标签 `构建中·轮次 N 完成`** | 保持 5 主阶段骨架不变；对未感知命令视作 `构建中` 扩展；向后兼容 |
| D3 | Phase 提交粒度 | **每 Phase 1 commit**（实际 4 commits，P0 并入 P1）| 与 TASK-01/11 同模式；每条目可独立 review/回滚 |
| **CU** | **.cursor/commands/* 可编辑性** | **Plan 阶段 Glob 验证纠正** | 从"纯文档"升级为"真执行守卫"，ROI 放大 2-3× |

### 3.3 安全决策

**本任务不涉及安全变更**（纯流程规则文档，无外部输入 / 认证路径 / 数据存储 / 部署变更）。

**D1 占位符模式本身是正向的敏感配置管理实践**：规则文件公开时不泄漏开发环境内部代理 IP，通过 `grep -rn "192\.168\." .cursor/` 零命中验证。

---

## 4. 测试覆盖（反例追溯表替代 TDD）

| 条目 | 覆盖 case | 详情 | 结果 |
|:-:|:-:|---|:-:|
| 1 FetchContent proxy | 4 case | TASK-19-02 Google Benchmark 首次 FetchContent 超时 / TASK-19-04 不触发（验证跳过条件）/ TASK-19-07 不触发 / TASK-13 Phase 0 自证 | 4/4 ✅ |
| 2 smoke 工具链 grep | 2 case | TASK-11 P3 `jq MISS` 临时切 python3 / **TASK-13 Phase 0 meta-dogfooding 实时自证**（`rg`/`jq`/`valgrind`/`xmllint` 4 工具 MISS）| 2/2 ✅ |
| 3 多轮次 Build 中间态 | 2 case | TASK-19-03 Round 1 合并暂停 ~30 min 状态漂移 / TASK-19-04 多 Phase 停歇期 `/reflect` 误入 | 2/2 ✅ |
| **总计** | **7 case** | 含 1 个 T-0 实时自证（最强证据型） | **7/7 ✅** |

**self-review 覆盖（每 Phase 6 维度 grep + Phase 4 整体）：**

| 维度 | 结果 |
|------|:-:|
| 锚点位置（5 处）| ✅ 全部符合 spec 定位 |
| YAML frontmatter 完整性（5 .md 文件 × 2 markers）| ✅ preserved |
| Code fence 偶数（10/2/2/4/6）| ✅ 全平衡 |
| 交叉引用 grep（3 处锚点：writing-plans.mdc / van.md / techContext.md）| ✅ 全命中 |
| 硬编码 IP 检查（`grep -rn "192\.168\." .cursor/`）| ✅ 零命中 |
| ReadLints 全 5 规则文件 | ✅ No linter errors found |

**10 验收标准：** 9 ✅ + 1 改进（§5.4 工具表合并同表兜底，Phase 0 实证驱动加入 `rg` 工具 — 实证微调 spec 范式）

---

## 5. 估时与进度

| 阶段 | Plan 估时 | 实际 | 比率 |
|------|:-:|:-:|:-:|
| P0 基线 | 5 min | ~5 min | 1.0× |
| P1 proxy 守卫 | 25-30 min | ~15 min | ~1.8× |
| P2 smoke 工具链 | 15 min | ~8 min | ~1.9× |
| P3 多轮次 Build | 30-35 min | ~15 min | ~2.1× |
| P4 收尾 | 10 min | ~8 min | 1.25× |
| **Build 合计** | **85-95 min** | **~51 min** | **1.67-1.86×** |

**跨类型估时收敛数据点：**

| 任务 | 类型 | Plan | 实际 | 倍率 |
|------|------|:-:|:-:|:-:|
| TASK-05 | bench | 4.25h | 75 min | 3.4× |
| TASK-09 | bench | 3.5h | 50 min | 4.2× |
| TASK-11 | bench | 55-80 min | 35-40 min | 1.5-2.0× |
| **TASK-13** | **文档** | **85-95 min** | **~51 min** | **1.67-1.86×** |

**结论：** 跨类型稳定收敛到 **~2×**，**通用目标倍率 plan × 0.6**（已扩写 `systemPatterns.md`「bench 估时校准」段为跨类型协议）。

---

## 6. 经验教训

### 6.1 关键发现

1. **Meta-dogfooding 自证闭环（最大亮点）** — Phase 0 sandbox `rg`/`jq`/`valgrind`/`xmllint` MISS 直接触发正在沉淀的条目 2 P1 规则，形成「规则沉淀过程自我证明」的天然实验。反例追溯从 6 case 扩展到 7/7（含 T-0 实时自证，最强证据型）。已沉淀为「规则沉淀类任务 Phase 0 标配动作」。

2. **跨类型估时收敛到 plan × 0.6 通用目标** — 原 bench 类专属估时协议泛化为跨类型（文档类 1.67-1.86× 与 bench 1.5-2.0× 同区间）。

3. **Plan 阶段"关键认知升级"ROI 放大 2-3×** — `.cursor/commands/*.md` 可编辑性假设纠正让规则从被动文档升级为主动守卫；触发「基础假设核查」清单沉淀。

### 6.2 挑战与教训

- **L1：文档类 plan 估时应 plan × 0.6 为目标**（跨 4 任务数据点实证）
- **L2：Meta-dogfooding 作为规则沉淀类 Phase 0 标配动作**（节省人工搜索历史案例时间 + 最新鲜证据）
- **L3：Plan 阶段"基础假设核查"清单** — 非标准目录/约定必做 Glob/Read 实证
- **L4：单一真相来源占位符模式的 read-write trade-off** — 环境敏感值用，语义稳定值不用
- **L5：实证微调 spec 范式** — plan 执行时允许基于实证数据微调 spec 细节，progress/reflection 标注依据

### 6.3 反复模式扫描

- **「前置依赖/环境/API 能力未验证」** (频率 8+) — 本次 **meta-dogfooding 自闭环**：既命中此模式**又**沉淀直接针对此模式的规则；双重价值
- **新记录模式**：`.cursor/commands/*` 可编辑性假设错误（本次 1 次，纳入 L3 基础假设核查）
- 其他模式（子代理返工 / 测试隔离 / 提交粒度 / TDD 严格度 / 计划清单偏差）：无重复

### 6.4 改进建议闭环（6/6 已处理）

| # | 建议 | 优先级 | 落实状态 |
|:-:|---|:-:|---|
| 1 | Meta-dogfooding 规则沉淀 Phase 0 标配 | 🟠 P1 | ✅ 已沉淀 `systemPatterns.md` |
| 2 | 跨类型 plan × 0.6 估时校准 | 🟠 P1 | ✅ 已扩写 `systemPatterns.md`「bench 估时校准」段 |
| 3 | 基础假设核查 VAN/Plan 前置清单 | 🟠 P1 | ✅ 已沉淀 `systemPatterns.md` |
| 4 | 单一真相来源占位符通用化 | 🟢 P2 | ✅ 已沉淀 `systemPatterns.md` |
| 5 | 实证微调 spec 范式 | 🟢 P2 | ✅ 已沉淀 `systemPatterns.md` |
| 6 | `.editorconfig` / prettier 统一 md 表格 | 🟢 P2 | 📍 观察 3+ 任务（已转入 `activeContext.md` 长期项） |

---

## 7. 提交清单

| # | SHA | 主题 |
|:-:|-----|------|
| 1 | `ec78f1c` | docs(van): TASK-20260419-13 init process rules sunk-in sprint |
| 2 | `db833ce` | docs(plan): TASK-20260419-13 process rules sunk-in design + plan |
| 3 | `76ed4e0` | docs(rules): TASK-20260419-13 P1 — FetchContent proxy guard (P0) |
| 4 | `b8ca9b4` | docs(rules): TASK-20260419-13 P2 — smoke toolchain grep (P1) |
| 5 | `60d047c` | docs(rules): TASK-20260419-13 P3 — multi-round build interim state (P1) |
| 6 | `020e574` | docs(rules): TASK-20260419-13 P4 — finalize rules sunk-in sprint |
| 7 | `ccfce04` | docs(reflect): add reflection for TASK-20260419-13 |
| 8 | (archive) | docs(archive): add archive for TASK-20260419-13 |

---

## 8. 长期影响

### 8.1 对工作流的持久影响

本任务交付的 3 条规则将对后续所有任务产生影响：

| 规则 | 预期频率触发 | 预计节省 |
|---|---|---|
| FetchContent proxy 守卫 | 每个含 FetchContent 且无 `_deps` 的新任务 | 单次 5-10 min × N ≥ 每月 30 min |
| smoke 工具链 grep | 每个 bench / smoke plan 阶段 | 避免 Build 阶段临时切换 ~5 min/次 |
| 多轮次 Build 中间态 | 每个 phase ≥ 5 的 Level 2+ 任务 | 避免 session 恢复时重读 plan ~10-20 min/次 |

### 8.2 对长期知识库的贡献

**`systemPatterns.md` 新增 5 段 / 扩写 1 段（+106 行）：**

1. 规则沉淀类任务的 Meta-dogfooding Phase 0 模式
2. 基础假设核查 — VAN/Plan 前置清单
3. 配置管理 — 单一真相来源占位符模式
4. 计划执行 — 实证微调 spec 范式
5. bench 估时校准 → 跨类型 plan × 0.6 通用协议（扩写）

这 5 段构成规则沉淀类任务的**元方法论**，预计在后续同类任务（如下次 P0/P1 沉淀冲刺）中直接引用。

### 8.3 交叉引用网络

本任务建立了 3 条新的跨文件引用链：

```
writing-plans.mdc「FetchContent 网络代理守卫」
  ↔ van.md §1「FetchContent 代理状态检查」
  ↔ techContext.md L98「Plan/VAN 阶段守卫」

complexity-levels.mdc「多轮次 Build 中间态」
  ↔ build.md §6.5「轮次完成判断」
  ↔ reflect.md §0「守卫放宽」
  ↔ git-workflow.mdc「提交粒度」（已存在）

writing-plans.mdc §4「Bench Smoke 验收三件套」末尾子块
  ↔ verification.mdc（前置条件协同）
```

---

## 9. 参考文档

- **设计规格：** `docs/specs/2026-04-19-process-rules-sunk-in-design.md`（369 行）
- **实现计划：** `docs/plans/2026-04-19-process-rules-sunk-in.md`（554 行）
- **回顾文档：** `memory-bank/reflection/reflection-TASK-20260419-13.md`（228 行）
- **创意设计：** ❌ 不需要（3 条目均遵循 `writing-plans.mdc` 已有段模式，零架构/UI/算法决策）

### 新增规则段索引

| 文件 | 锚点 | 主题 |
|---|---|---|
| `.cursor/rules/skills/writing-plans.mdc` | L96 | FetchContent 网络代理守卫 |
| `.cursor/rules/skills/writing-plans.mdc` | L255 | smoke 工具链可用性检查（§4 子块）|
| `.cursor/rules/workflow/complexity-levels.mdc` | L68 | 多轮次 Build 中间态 |
| `.cursor/commands/van.md` | §1 子项 | FetchContent 代理状态检查 |
| `.cursor/commands/build.md` | §6.5 | 轮次完成判断 |
| `.cursor/commands/reflect.md` | §0 | 子状态守卫放宽 |
| `memory-bank/techContext.md` | L98 | Plan/VAN 阶段守卫（代理地址单一真相来源）|

### systemPatterns.md 新增模式索引

| 段 | 来源 |
|---|---|
| 规则沉淀类任务的 Meta-dogfooding Phase 0 模式 | TASK-13 反思 #1 |
| 基础假设核查 — VAN/Plan 前置清单 | TASK-13 反思 #2 |
| 配置管理 — 单一真相来源占位符模式 | TASK-13 反思 #4 |
| 计划执行 — 实证微调 spec 范式 | TASK-13 反思 #5 |
| bench 估时校准（扩展为跨类型 plan × 0.6 通用协议）| TASK-05/09/11/13 四次数据点 |
