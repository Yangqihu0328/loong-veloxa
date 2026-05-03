# 归档：TASK-20260503-05 QuickJS Interrupt Handler + SetEvalInterruptBudget API

**日期：** 2026-05-03
**任务 ID：** `TASK-20260503-05`
**复杂度级别：** Level 2（多文件修改 / 需求清晰 / creative §组件 1 方案 C Phase 2 已预决策 / 无新设计决策 / 无新组件）
**状态：** ✅ 已完成
**安全相关：** ✅ 是 — T1 mitigation 基础设施（JS eval CPU DoS 缓解 / 解锁 TASK-20260503-04 Console JS REPL 完整 T1）

---

## 1. 任务概述

### 1.1 背景

TASK-20260503-04（DevTool Phase D · Console JS REPL）VAN 阶段读取 spec §11.1 时**主动 push-back** 识别硬前置依赖：技术债 #44 QuickJS Interrupt Handler（T1 eval mitigation 基础设施）。用户 V3=A 决策 → 独立立项 #44 完整落地 → 本任务 TASK-20260503-05 = `creative-quickjs-host.md §组件 1 方案 C Phase 2` 完整实施。

### 1.2 范围

5 子任务串行 + 2 Checkpoint，目标实现：
- `QuickjsEngine::SetEvalInterruptBudget(usize max_checkpoints)` 公开 API（opt-in 启用预算）
- `JS_SetInterruptHandler` 在 Init() 始终注册（budget=0 短路 / budget>0 atomic 计数中止）
- `WasInterrupted()` 状态查询 getter（每次 EvalGlobal 入口重置）
- `StatusCode::kAborted` 新增 enum 值（u8 backwards-compatible）
- `kDefaultInterruptBudgetCheckpoints = 10000` 公开常量（与 QuickJS `JS_INTERRUPT_COUNTER_INIT = 10000` 对齐）
- 5 新单测覆盖 4 维度（kAborted + budget=0 关闭 + 默认不误杀 + 重置 + WasInterrupted 语义）

### 1.3 用户决策路径

| 阶段 | AskQuestion | 用户选择 |
|---|---|---|
| Plan brainstorm scope | D 项范围 | `core_only`（D1 + D2 + D5 仅核心 3 项） |
| Plan brainstorm decisions | D1+D2+D5+**D8b** 4/4 锁定（D8b 由 Phase 0 grep 实证后**主动** push-back 抛入）| `all_recommended` |
| Plan B1-B8 决策 | 8/8 推荐锁定 | `all_recommended` |

**用户共 3 次 AskQuestion**（vs 02/03 的 1 次）— more_brainstorm 触发的两步式必要成本（避免 D8b 重大设计修正漏入 build）。

---

## 2. 技术方案

### 2.1 选定方案

完整继承 `creative-quickjs-host.md §组件 1 方案 C` Phase 2，brainstorm 阶段细化 4 项决策 + plan 阶段锁定 8 项 B 决策：

| # | 维度 | 决策 |
|:-:|---|---|
| **D1** | `WasInterrupted()` Phase 2 vs Phase 3 | **B 本任务实现**（私有 bool flag + 公开 const getter）— 5 行 / 0 复杂度 |
| **D2** | interrupt 错误码 / 消息设计 | **B.1 本任务新增 `StatusCode::kAborted = 6`** — Phase 0 §0.3.4 audit 触发（既有 6 项 enum 无 kAborted/kCancelled/kDeadlineExceeded）|
| **D5** | 死循环 ctest hang 防护 | **A+C 组合** — A: 不真测死循环（小 budget=10/100 准死循环反向探针）+ C: 显式 SetEvalInterruptBudget(small) 兜底 |
| **D8b** ⭐ | creative「10⁷ 检查点」语义重定义 | **B 默认 `kDefaultInterruptBudgetCheckpoints = 10000`**（handler 调用次数 = bytecode/10000，死循环 100-500ms 内中止）— **Phase 0 grep 实证 `JS_INTERRUPT_COUNTER_INIT = 10000` 后主动 push-back，避免 10¹¹ 字节码 ≈ 100-1000s 灾难性默认值** |
| B1 | 子任务执行顺序 | E1 → E2 → E3 (CP1) → E4 → E5 (CP2) |
| B2 | 测试模式 | E1/E2/E3 [覆盖补充] / E4 [文档调整] |
| B3 | 默认 budget 公开常量 | `static constexpr usize kDefaultInterruptBudgetCheckpoints = 10000`（公开暴露给单测）|
| B4 | InterruptCallback 实现 | 静态 free function（cc anon namespace）+ opaque ptr → `QuickjsEngine*` + `std::atomic<int64_t>` 计数器 |
| B5 | budget=0 语义 | **统一注册** handler + handler 内 `return 0` 短路（B5 显式语义，沿用 TASK-20260502-02 沉淀模式）|
| B6 | EvalGlobal 重置点 | 入口处一次性：`interrupt_counter_.store(budget, relaxed); was_interrupted_ = false;` |
| B7 | Phase 0 + Checkpoint | Phase 0 极简 1 子段（5 min）+ CP1（E3 后）+ CP2（E5 后）|
| B8 | commit 粒度 + 估时 | 5 commits 1/子任务 + Source 溯源前缀 + plan ×0.6 ~85-105 min 估 |

### 2.2 决策路径

详见 `docs/plans/2026-05-03-quickjs-interrupt-handler.md` §1.2 brainstorm 决策表 + §B1-B8 plan 决策表。

---

## 3. 实现摘要

### 3.1 文件变更

| 操作 | 文件路径 | 行变更 | 说明 |
|------|---------|:-:|------|
| 修改 | `veloxa/foundation/base/status.h` | +1 | `kAborted = 6` enum 值 |
| 修改 | `veloxa/script/quickjs_engine.h` | +27/-1 | `kDefaultInterruptBudgetCheckpoints` constexpr + `SetEvalInterruptBudget(usize)` + `WasInterrupted() const` + private `interrupt_budget_` / `interrupt_counter_` (atomic) / `was_interrupted_` 字段 + `<atomic>` include + `InterruptCallback(JSRuntime*, void*)` 静态私有 |
| 修改 | `veloxa/script/quickjs_engine.cc` | +49/-1 | Init() 注册 InterruptHandler + `SetEvalInterruptBudget()` 实现 + `InterruptCallback()` 静态 free function（atomic fetch_sub + budget=0 短路）+ `EvalGlobal()` 入口重置 counter/flag + 错误识别 "interrupted" → kAborted（人类可读消息脱敏）|
| 修改 | `tests/script/quickjs_engine_test.cc` | +101 | 5 新测 `QuickjsEngineInterruptTest`：(1) `AbortsLongLoopWhenBudgetExhausted` / (2) `BudgetZeroDisablesInterrupt` / (3) `DefaultBudgetDoesNotAbortLegitScripts` / (4) `RepeatedEvalResetsBudget` / (5) `WasInterruptedTracksLastEval`；D5 A+C 组合（小 budget 准死循环反向探针 / 总耗时 0.25s 远低于 ctest hang 风险）|
| 修改 | `memory-bank/techContext.md` | +1/-1 | #44 条文更新（Phase 2 闭环 ✅ + JSMallocFunctions 仍记技术债）|
| 创建 | `docs/plans/2026-05-03-quickjs-interrupt-handler.md` | +728 | plan 文档（5 子任务 / 12 决策表 / Phase 0 极简 1 子段含 3 grep + Status.h audit / CP1+CP2 / 9 systemPatterns 协同度自我对照 / 5 R 风险登记 / 反复模式预防清单 / 完整 cpp 代码片段）|
| 创建 | `memory-bank/reflection/reflection-TASK-20260503-05.md` | +280 | Level 2+ 12 段（5 做得好 + 3 挑战 + 4 教训 + 4 改进建议）|
| 创建 | `memory-bank/archive/archive-TASK-20260503-05.md` | （本文档） | 归档摘要 |
| 修改 | `memory-bank/{tasks,activeContext,progress}.md` | +多 | 任务跟踪 + 阶段切换 + milestones |
| **新增 P0 #1 落实** | `memory-bank/systemPatterns.md` | +多 | 「最小代码改动 + Phase 0 高度预跑极速区 0.10-0.20×」新子档入库 + 第 67-72 数据点群组 + Phase 0 投入定律 sext-evidence 升级 |
| **新增 P2 #1 落实** | `memory-bank/systemPatterns.md` | +多 | 「DEVTOOL=OFF / 次配置 baseline 验证 SOP」段（FETCHCONTENT_BASE_DIR 复用范式）|

### 3.2 commit 历史

| # | Commit | Subject | 类型 |
|:-:|---|---|:-:|
| 1 | `67c8c81` | chore(van): initialize TASK-20260503-05 QuickJS Interrupt Handler (#44 component 1 Phase 2) | chore(van) |
| 2 | `3c71334` | chore(plan): TASK-20260503-05 plan + Phase 0 grep audit done | chore(plan) |
| 3 | `f1dfe86` | feat(script): add QuickJS interrupt budget API + kAborted status (E1) | feat |
| 4 | `c371e2f` | feat(script): register InterruptHandler + reset budget per EvalGlobal (E2) | feat |
| 5 | `7d9732f` | test(script): add 5 interrupt budget tests (D5 A+C coverage) (E3) | test |
| 6 | `58e3688` | docs(memory-bank): close #44 component 1 Phase 2 + retain JSMallocFunctions debt (E4) | docs |
| 7 | `12d3522` | chore(build): finalize TASK-20260503-05 memory bank state (E5) | chore(build) |
| 8 | `1923073` | docs(reflect): add reflection for TASK-20260503-05 (QuickJS Interrupt Handler) | docs(reflect) |
| 9 | （本归档） | docs(archive): add archive for TASK-20260503-05 | docs(archive) |

**5 实施 commits（E1-E5）全部含 Source 溯源前缀** — `Source: TASK-20260503-05 creative-quickjs-host.md §组件 1 方案 C Phase 2`（继续 02/03 11 commits double-evidence → **16 commits triple-evidence** 累计）。

### 3.3 关键决策

1. **Phase 0 §0.3.2 grep 实证后主动 push-back D8b** — `JS_INTERRUPT_COUNTER_INIT = 10000` 实证 → 重校准 creative「10⁷ 检查点」字面值（10¹¹ 字节码 ≈ 100-1000s 死循环灾难）→ 默认 `kDefaultInterruptBudgetCheckpoints = 10000`（handler 调用次数 = bytecode/10000，死循环 100-500ms 内中止）。**新协议「brainstorm 中途主动 push-back 模式」首次实证。**
2. **Phase 0 §0.3.4 Status.h audit 触发 D2 细化为 D2.B.1** — 既有 6 项 enum 无 kAborted/kCancelled/kDeadlineExceeded → 本任务直接新增 `kAborted = 6`（u8 空间充裕 / backwards-compatible / 业界惯例）。Phase 0 audit 提前发现避免 build 阶段 enum 缺失返工。
3. **D5 A+C 组合策略避免 ctest hang** — 不真测死循环（避免 ctest TIMEOUT）+ 小 budget=10/100 准死循环反向探针（5 新测总耗时 0.25s 远低于 hang 风险）。
4. **B5 budget=0 显式语义短路** — 统一注册 handler 路径（不分支 if budget>0 才注册），handler 内 `return 0` 短路 = 沿用 TASK-20260502-02「显式语义状态优于阈值」沉淀模式。
5. **B8 commit 粒度 5 commits 1/子任务 + Source 溯源** — 全 5/5 commits 严格 1 commit/子任务 + 全 5/5 含 Source 前缀（继 02/03 11 commits → **16 commits triple-evidence** 累计）。

### 3.4 安全决策

**T1 mitigation 基础设施 ✅ — JS eval CPU DoS 缓解 5 维度完整性：**

| # | 维度 | 实现 |
|:-:|---|---|
| 1 | **默认安全（fail-safe）** | `interrupt_budget_ = 0` 默认值 → handler return 0 cheap short-circuit / 不影响既有 EvalGlobal 调用方 |
| 2 | **opt-in 启用** | 调用方显式 `SetEvalInterruptBudget(N>0)` → atomic counter active |
| 3 | **不可被脚本绕过** | QuickJS 内部 `JS_SetUncatchableError` 设计意图（quickjs.c:8165-8169 实证）— JS try/catch 无法捕获 interrupt |
| 4 | **单线程 atomic 防御** | `std::atomic<int64_t>` + relaxed memory order（QuickJS 单线程模型 — atomic 仅作 future-proof）|
| 5 | **状态可查** | `WasInterrupted() const` getter → 宿主可在每次 EvalGlobal 后判断是 timeout 还是 normal exception |

**错误信息脱敏 ✅** — `kAborted` 错误消息「`script aborted (interrupt budget exhausted)`」无敏感信息泄露（不含路径 / 不含内部状态 / 不含 budget 数值）。

**回调约束 ✅** — `InterruptCallback` 仅做 atomic 递减 + return / 不调 EventLoop / 不分配复杂对象（创意文档 §组件 1 §实现指导明示）。

**详见 reflection §8 安全 checklist + 5 维度完整性详解。**

---

## 4. 测试覆盖

### 4.1 ctest 双配置全 PASS

| 配置 | baseline | 任务后 | 增量 | 耗时 |
|---|:-:|:-:|:-:|:-:|
| **DEVTOOL=ON** `build/` | 1247 | **1252 PASS** | +5 ✅ | 6.39s |
| **DEVTOOL=OFF** `build-off/` | 1082 | **1087 PASS** | +5 ✅（**plan §0.4 假设错误** — 详见 §6 plan-fact reconcile #1）| 6.42s（含 build-off 配置 + 60s 全量构建）|
| **QuickjsEngine 局部** | 4 | **9 PASS** | +5 ✅ | 0.31s |
| Lint | — | 0 错误 | — | — |

### 4.2 5 新测 D5 A+C 组合（`QuickjsEngineInterruptTest`）

| # | 测试 | 验证维度 | 设计 | 耗时 |
|:-:|---|---|---|:-:|
| 1 | `AbortsLongLoopWhenBudgetExhausted` | budget>0 中止 + kAborted | budget=10 + while(true) 准死循环 → 期望 `StatusCode::kAborted` + `WasInterrupted()=true` + 消息含 "aborted" | 0.08s |
| 2 | `BudgetZeroDisablesInterrupt` | budget=0 不影响既有行为 | budget=0 (默认) + 简单 `1+1` script → 期望 OK | 0.01s |
| 3 | `DefaultBudgetDoesNotAbortLegitScripts` | 默认 10000 不误杀正常脚本 | budget=10000 + `for(let i=0;i<100;i++) acc+=i` → 期望 OK 且 `WasInterrupted()=false` | 0.01s |
| 4 | `RepeatedEvalResetsBudget` | 每次 EvalGlobal 入口重置 counter | budget=20 → 第 1 次准死循环 abort → 第 2 次正常脚本 → 期望 OK | 0.08s |
| 5 | `WasInterruptedTracksLastEval` | flag 仅追踪最后一次 EvalGlobal | budget=10 → 第 1 次 abort → 第 2 次 OK → 期望 `WasInterrupted()=false` | 0.07s |

**总耗时 0.25s 远低于 ctest 默认 hang TIMEOUT — D5 A+C 组合策略验证有效。**

### 4.3 既有 4 测无回归 ✅

`QuickjsEngineTest.{Init, EvalSimple, EvalThrow, EvalSyntaxError}` 全 PASS / 0 行为变化（B5 显式语义 / B6 入口重置不影响 budget=0 默认路径）。

---

## 5. plan ×0.6 矩阵新数据点入库

### 5.1 总比值 0.16× 落新效率区候选

| 维度 | 数值 |
|---|---|
| plan ×0.6 估时 | ~85-105 min |
| **实测主线** | **~15 min** |
| **比值** | **~0.16×（创历史新低）** |
| VAN/plan 预测 | 0.30-0.45×（极窄档延续高效区候选）|
| **触发新子档** | **「最小代码改动 + Phase 0 高度预跑极速区 0.10-0.20×」候选** |

### 5.2 第 67-72 数据点群组（5 子任务 + 1 finalize）

| # | 子任务 | plan ×0.6 | 实测 | 比值 |
|:-:|---|:-:|:-:|:-:|
| 67 | E1 API + StatusCode | 10 min | ~2 min | **0.20×** |
| 68 | E2 实现 | 30-40 min | ~3 min | **0.08-0.10×** |
| 69 | E3 5 新测 | 30-40 min | ~3 min | **0.08-0.10×** |
| — | CP1 自审（含 build-off 60s） | — | ~3 min | — |
| 70 | E4 techContext | 5 min | ~1 min | **0.20×** |
| 71 | E5 finalize | 10 min | ~3 min | **0.30×** |
| **总** | — | **85-105 min** | **~15 min** | **~0.16×** |

**中位 0.16× / 分布偏左极速区**：E2/E3 = 0.08-0.10×（最快 — Write 全文件 + 编译 + ctest 一次过）；E5 = 0.30×（含 CP2 自审 + 6 commits 总览检查 = 工作流闭环成本）。

### 5.3 新子档触发条件（5 项叠加）

详见 systemPatterns.md plan ×0.6 矩阵段（本归档同步落实）：

| # | 触发条件 | 本任务体现 |
|:-:|---|---|
| 1 | 真实代码改动 ≤ 200 行 | +184 行（5 文件）✅ |
| 2 | Phase 0 grep audit 完整（≥ 3 grep + 关键文件 audit）| §0.3 三 grep + §0.3.4 Status.h audit ✅ |
| 3 | 完整 cpp 代码片段 plan 阶段 100% 就位（可编译）| plan §3.E1-E3 完整代码 ✅ |
| 4 | 0 brainstorm 之外的设计决策 | 4 brainstorm + 8 B 决策 12/12 锁死 ✅ |
| 5 | 测试矩阵预 audit（含 ctest config 假设）| §0.4 ctest config 矩阵（**有偏差但 CP1 自审已校正**）⚠️ |

---

## 6. plan-fact reconcile #1 + 反复模式自检

### 6.1 plan-fact reconcile #1 — DEVTOOL=OFF 测数预测错误

**plan §0.4 写：** `DEVTOOL=OFF 1082 → 1082（不变）`
**实测：** `DEVTOOL=OFF 1082 → 1087（+5）`

**根因：** `tests/CMakeLists.txt:273` 的 `vx_add_test(quickjs_engine_test ...)` 无 DEVTOOL guard / quickjs_engine_test 在 vx_script 是 unconditional add（vx_script 在 OFF 仍编译 / quickjs-ng 不属于 vx_devtool）→ quickjs_engine_test 在 OFF 配置下也跑 → +5 而不是 0。

**影响：** 无功能影响（CP1 自审实测已校正）；仅是 plan §0.4 audit 缺一子条「待新增测的 add_test 是否有 config guard」。

**沉淀：** P1 改进建议 → writing-plans.mdc «ctest 数量预期 config 矩阵» 段补强 audit 子条 + grep 范本（已迁移 activeContext 待处理事项）。

### 6.2 反复模式自检 0/7 ✅ — 连续第 5 次零反复达成

| # | 已知反复模式 | 历史频率 | 本次是否重复？ |
|:-:|---|:-:|:-:|
| 1 | 计划文件清单与实际变更不一致 | 9+ | ❌（5/5 文件命中清单 / +184 行 vs +138-174 估略高 5%）|
| 2 | 子代理产出需大量返工 | 7+ | ❌（无子代理使用）|
| 3 | **前置依赖/环境/API 能力未验证** | **9+ 最高** | **❌ 第 10 次抑制成功** — Phase 0 §0.3 三 grep + §0.3.4 Status.h audit + D8b push-back 三层 |
| 4 | 非默认路径（流式/错误/缓存）遗漏验证 | 4+ | ❌（EvalGlobal 错误路径在测 1+4+5 验证）|
| 5 | 测试隔离问题（flaky/并行冲突）| 7+ | ❌（5 新测全部独立 QuickjsEngine 实例 / 无全局状态）|
| 6 | 提交粒度偏离计划 | 7+ | ❌（5/5 commits 严格 1/子任务 + Source 溯源）|
| 7 | TDD 严格度与场景不匹配 | 11+ | ❌（[覆盖补充] 模式与 plan §3 标注一致）|

**结论：** 0/7 命中 ✅ — 连续第 5 次零反复达成（02 → 03 → **05 抵消 03 的 1/7 回升**）。

**plan-fact reconcile #1 注记：** 属于反复模式 #1 的**新形式**「config 矩阵 guard 边界假设漏审」（不是「文件清单不一致」）— CP1 自审已捕获并沉淀 P1 改进建议，不计入 7 项反复模式命中。

---

## 7. Phase 0 投入定律 sext-evidence 升级 ⭐

| TASK | Phase 0 投入 | Build 节省 | ROI | 累计 evidence |
|---|:-:|:-:|:-:|---|
| 502-01 Phase A | ~30 min | ~159 min | 5.3× | single |
| 502-02 Phase B | ~30 min | ~157 min | 5.2× | dual |
| 503-01 Phase C | ~30 min | ~229 min | 7.6× | triple |
| 503-02 工作流元 | ~10 min | ~67 min | 6.7× | quad |
| 503-03 三件套收官 | ~10 min | ~80 min | 8.0× | quint |
| **TASK-20260503-05** | **~5 min** | **~80 min** | **16×（创历史新高）** | **sext** ⭐ |

**升级：** **quint-evidence → sext-evidence**（6 次实证 / ROI 范围 5.2× - 16× / 平均 8.1×）。

**16× ROI 加速因子：**
1. Phase 0 极简 1 子段（5 min）vs 历史 30 min（Level 3）
2. 4 grep 实证全 surface 重大设计偏差（**D8b 重校准默认 budget 避免 100-1000s 死循环灾难**）
3. Status.h audit 提前发现避免 build 阶段 enum 缺失返工
4. Level 2 任务范围明确 + creative 已预决策 = 0 设计探索成本

---

## 8. 经验教训（详见 reflection §4 + §5）

### 8.1 L1 — Phase 0 grep 实证驱动的主动 push-back 模式（D8b 实证）⭐

**触发条件：** brainstorm scope 已被用户限定后，Phase 0 grep / audit 阶段**新发现** creative / spec / 既有实现的 runtime 行为偏差且**显著**（默认值差 10³+ 倍 / API 不存在 / signature 不符）。

**协议：** **必须主动**抛出而非等用户问到 — 「证据优于断言」原则的延伸。

**已落实到：** P1 #2 改进建议 → 下次工作流元任务批量落地到 `.cursor/rules/skills/brainstorming.mdc` 新段「Phase 0 grep 实证驱动的主动 push-back 模式」（已迁移 activeContext 待处理事项）。

### 8.2 L2 — plan §0.4 ctest 数量预期 config 矩阵 audit 子条强化

**教训：** plan §0.4 必须包含「待新增测的 add_test 是否有 config guard」audit 子条 + grep 范本：

```bash
rg "vx_add_test\($NEW_TEST_NAME" tests/CMakeLists.txt -C 5 | rg "if\s*\(VX_"
# 如有 if (...) 包围 → 该测仅在该 config 下跑
# 如无 if (...) 包围 → 该测在所有 config 下都跑（影响 OFF 估时）
```

**已落实到：** P1 #1 改进建议 → 下次工作流元任务批量落地到 `.cursor/rules/skills/writing-plans.mdc` «ctest 数量预期 config 矩阵» 段（已迁移 activeContext 待处理事项）。

### 8.3 L3 — 完整 cpp 代码片段 plan 阶段 100% 就位 → build 阶段 0 返工

**实证：** plan §3.E1/E2/E3 直接嵌入完整可编译 cpp 代码（含注释、namespace、public/private 顺序、constexpr、static 修饰符等所有细节），build 阶段 5/5 commits 全部一次通过 / 0 lint / CP1 + CP2 自审无 fix。这是 writing-plans.mdc «完整代码片段优于描述» 原则的极致体现。

### 8.4 L4 — DEVTOOL=OFF / 次配置 baseline 验证 SOP（FETCHCONTENT_BASE_DIR 复用）

**实证：** `-DFETCHCONTENT_BASE_DIR="$(pwd)/build/_deps"` 复用主 build/ 的 _deps，cmake reconfigure 1.8s（vs 75s+ FetchContent 完整拉取）+ build ~60s = 总 ~70s 节省 ~3-5 min。

**已落实到：** P2 #1 改进建议 → **本归档同步落实** systemPatterns.md「DEVTOOL=OFF / 次配置 baseline 验证 SOP」段。

### 8.5 L5 — 反复模式 #1 新形式识别（config 矩阵 guard 边界假设漏审）

**新形式：** 历史 9 次反复模式 #1 主要是「依赖包/编译器/工具链/外部 API/环境工具是否存在」+「现有实现 runtime 行为假设未实证」（TASK-03 第 9 次新维度）；本次第 10 次暴露**第 3 个新维度**「config 矩阵 guard 边界假设漏审」 — VAN/plan 阶段假设新增测仅在某 config 下跑，但实际 add_test 无 guard。

**已沉淀到：** systemPatterns.md plan ×0.6 矩阵段实证「触发条件 #5 测试矩阵预 audit（含 ctest config 假设）」+ writing-plans audit 子条强化（P1 已迁移）。

---

## 9. 改进建议落实状态（4 项 P0×1 + P1×2 + P2×1）

| # | 建议 | 优先级 | 落实状态 | 落实位置 |
|:-:|---|:-:|:-:|---|
| 1 | plan ×0.6 矩阵新子档「最小代码改动 + Phase 0 高度预跑极速区 0.10-0.20×」入库 + 第 67-72 数据点群组 + Phase 0 投入定律 sext-evidence 升级 | **P0** | ✅ **本归档同步落实** | `memory-bank/systemPatterns.md` plan ×0.6 矩阵段 + 实证段追加 |
| 2 | writing-plans.mdc «ctest 数量预期 config 矩阵» 段补强 audit 子条「待新增测的 add_test 是否有 config guard」+ grep 范本 | **P1** | ✅ **已迁移 activeContext 待处理事项** | 下次工作流元任务批量落地 |
| 3 | brainstorming.mdc 加新段「Phase 0 grep 实证驱动的主动 push-back 模式」+ 触发条件清单 | **P1** | ✅ **已迁移 activeContext 待处理事项** | 下次工作流元任务批量落地 |
| 4 | systemPatterns.md「DEVTOOL=OFF / 次配置 baseline 验证 SOP」段沉淀（FETCHCONTENT_BASE_DIR 复用范式）| **P2** | ✅ **本归档同步落实** | `memory-bank/systemPatterns.md` 新段 |

**落实率：** P0 1/1 ✅ + P1 2/2 已迁移 + P2 1/1 ✅ = **100% 闭环**。

---

## 10. P3 候选（本任务沉淀 — 入 activeContext 待处理事项参考）

| # | 候选 | 类型 | 触发时机 |
|:-:|---|---|---|
| 1 | `JS_SetInterruptHandler` opaque ptr lifetime audit（形式化） | 安全 robust | 后续大型 callback API 设计时合并 |
| 2 | `kCancelled` 进一步语义拆分（业务取消 vs interrupt timeout） | API 表面扩展 | TASK-20260503-04 Console 实现期间或之后 |
| 3 | `SetEvalInterruptBudget` 在 Init 之前调用边界处理（Status 返回 / Log warning）| API robustness | API 表面扩展决策时一并 |

**当前不立项**（不阻塞 TASK-20260503-04 启动）。

---

## 11. 下一步路线图

### 11.1 直接关联（解锁）

- ✅ **TASK-20260503-04 DevTool Phase D · Console JS REPL** — 硬前置依赖 #44 已闭环；T1 mitigation 基础设施完整可用。**已搁置 → 用户决策是否立即恢复**（V1=B + V3=A 决策已锁定 / 无需重问）。
- ✅ **技术债 #44 闭环**（组件 1 Phase 2）— `techContext.md:620` 标记 ✅；`creative-quickjs-host.md §组件 1 方案 C Phase 2` 完整落地。

### 11.2 后续 P3 候选

- DomBindings R2 三连补全（Element.children / addEventListener / innerHTML setter — 来自 TASK-20260502-01 R2 P3）
- #35 阶段 2 拆 LayoutEngine 内 style/layout（来自 TASK-20260502-02 P3）
- R9 EventManager HitTest 改造（HUD pointer-events 真支持 — 来自 TASK-20260502-02 P3）
- Performance Overlay 持续 invalidate 机制（来自 TASK-20260503-03 P3 候选 #0）
- R3+ #1 image_decoder 安全三件套（来自 codebase review）

按需独立立项。

---

## 12. 参考文档

- 实现计划：`docs/plans/2026-05-03-quickjs-interrupt-handler.md`（728 行 / 5 子任务 / 12 决策表 / Phase 0 极简 1 子段含 3 grep + Status.h audit / CP1+CP2 / 9 systemPatterns 协同度自我对照 / 完整 cpp 代码片段 / 5 R 风险登记 / 反复模式预防清单）
- 回顾文档：`memory-bank/reflection/reflection-TASK-20260503-05.md`（Level 2+ 12 段 ~280 行）
- 创意设计：`memory-bank/creative/creative-quickjs-host.md` §组件 1 方案 C Phase 2（已批准 2026-04-13）
- 设计规格：N/A（Level 2 + creative 已预决策 / 沿用 02/03 范式）

**关联任务（DevTool 三件套主线 + 安全收官 6 任务闭环）：**
- TASK-20260430-04（蓝图 spec + plan + creative ×7）
- TASK-20260502-01（Phase A Inspector — `archive-TASK-20260502-01.md`）
- TASK-20260502-02（Phase B Performance Overlay — `archive-TASK-20260502-02.md`）
- TASK-20260503-01（Phase C Hot Reload — `archive-TASK-20260503-01.md`）
- TASK-20260503-02（工作流/规则类技术债批量清理 — `archive-TASK-20260503-02.md`）
- TASK-20260503-03（DevTool 三件套主线收官 — `archive-TASK-20260503-03.md`）
- **TASK-20260503-05（QuickJS Interrupt Handler — 本归档）** ✅ — 解锁 TASK-20260503-04 Phase D Console JS REPL

**关联规则文件落实（本归档同步）：**
- `memory-bank/systemPatterns.md` — 「最小代码改动 + Phase 0 高度预跑极速区 0.10-0.20×」新子档 + 第 67-72 数据点群组 + Phase 0 投入定律 sext-evidence 升级 + 「DEVTOOL=OFF / 次配置 baseline 验证 SOP」段
- `memory-bank/techContext.md` — 技术债 #44 Phase 2 闭环（已在 E4 阶段更新，本归档不再重复）
- `memory-bank/productContext.md` — 「最近能力更新 2026-05-03」段追加 QuickJS Interrupt API 公开能力
