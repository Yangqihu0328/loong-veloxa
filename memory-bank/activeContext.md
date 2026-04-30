# 活跃上下文

## 当前阶段

**回顾中（2026-05-01 ~00:08）** — R0+R1+R2 全部完成 + reflection 文档落盘。Checkpoint 2 待用户决策 R3+ 拆分顺序 + 进入 `/archive`。

**焦点：** 用户决定 R3+ 13 项 P1 拆分顺序（推荐 Top 3）+ archive 时机：
- 🔴 #1 image_decoder 安全三件套（F-049 PNG alpha / F-050 width×height 溢出 / F-051 JPEG error_exit kill）— P1 安全 / 估 4-6 h / Level 3
- 🟡 #2 EventDispatcher snapshot iteration 防 listener mutation UAF（F-046）— P1 正确性 / 估 2-3 h / Level 2
- 🟡 #3 LoadHTML 重置 dom_bindings_ 防 use-after-free（F-025）— P1 正确性 / 估 1-2 h / Level 2
- 🔴 #4 CSS 属性元数据表（F-022）— P1 维护性 / Level 4 大件 / 1-2 周（架构性重构）

**Reflection 主要发现：**
- Background agent 双轨模式首次实战 → race condition 应对协议固化（systemPatterns 新模式）
- plan ×0.6 估时按任务类型分桶系数矩阵升级（review 0.4-0.7× / fast-fix 0.7-1.4× / racy 1.4-2.5× / 大件 0.8-1.2×）
- Quick fix 颗粒度校准 12 min/项（plan 9 min/项 + 33%）
- 总耗时 ~177 min（plan ×0.6 区间 0.85-1.00× ✅）— **plan ×0.6 模型有效，第 16 数据点入库**
- P0 改进建议 1 项（git symbolic-ref commit 守门）— archive 阶段写入 git-workflow skill

**R2 实绩（条件触发 ✅）：**
- 6/6 commits 全过 ctest 1062/1062
- F-020 R2.1 `3b4b2e7`（selector_matcher dead return + VX_DCHECK）
- F-033 R2.2 `1467207`（html parser ProcessEndTag isize 净化）
- F-040 R2.3 `ddea78d`（rasterizer 阈值注释统一）
- F-026 R2.4 `95ae814`（layout_engine 单参 arena thread_local）
- F-053 R2.5 `9c6ad5f`（image_decoder DecodeFromFile max_size 守卫 + 新单测）
- F-055 R2.6 `668a9fe`（vx_version() CMake configure_file 生成）
- 实测耗时 ~70 min（plan 55 min ×0.6 = 33 min；plan ×1.27 / ×0.6 ×2.12，超 ×0.6 因 worktree 隔离 + 并发会话冲突修复 ~25 min 额外开销）

**TDD 模式：** R2 quick fix 不强制 RED-GREEN-REFACTOR（注释 / 单行守卫 / 文档化）；R2.5 image_decoder 是唯一行为变化，加新单测验证守卫触发；其余每项变更 ctest 1062 通过验证。

> **TDD 模式：** R0/R1 不适用（review 类任务为数据收集 + 报告产出，无新代码无新测试）；R2 P0 修复阶段对有测试的修复强制 RED 反向探针（systemPatterns §9.3）。

### R0 数据快照（详见 `docs/reports/2026-04-30-codebase-review-r0-data.md`）

- ctest 基线：**1061/1061 PASS in 2.36s**
- fingerprint grep：veloxa/ 全代码库 0 TODO/FIXME/XXX/HACK + 0 危险 C 函数 + 0 throw + 0 sscanf/atoi 不安全转换
- lcov 覆盖率：行 **85.4%** / 函数 **95.4%** / 分支 **57.6%**（薄弱模块 12 项）
- CVE 审计：**0 CRITICAL/HIGH** + 5 Medium/Low（含 libpng 3 个 2026 新公布 Medium，与 image_decoder 57.1% 覆盖薄弱形成威胁链）
- R1 抽样名单：H 20 / M 80 / L 36 三层就绪
- R0 候选 findings 14 项（F-001 ~ F-014），R1 验证 + 分级 + 写方案
- 实测耗时 **~22 min**（plan 90 min ×0.6 = 54 min；实测 0.41× plan / 0.69× ×0.6） → reflect 阶段第 16 数据点

## 当前任务

### TASK-20260430-03：全代码库 Code Review（6 维度 × 7 子系统 + 多轮次 Build + Checkpoint）[安全相关]

- **复杂度：** Level 4
- **分支：** `feature/TASK-20260430-03-codebase-review`（基于 main `9411584`）
- **范围决策：** V1=A 全代码库 / V2=all 6 维度 / V3=C 完整实施（多轮次 + Checkpoint）/ V4=D 混合视角 / V5=✅ 安全相关
- **执行策略 X：** R0+R1 必然 / R2 条件触发 / R3+ 拆出独立后续任务
- **多轮次划分：**
  - R0（prep）：grep fingerprint + 抽样深度策略
  - R1（必然）：6 维度全代码库 review 报告
  - **Checkpoint 1**（用户审报告）
  - R2（条件触发）：P0 quick fix 5-15 个
  - **Checkpoint 2**（用户决定 R3+ 拆分）
  - R3+：拆出独立后续任务，不在本任务内
- **上限封顶：** R0+R1+R2，plan ~5-8h / plan×0.6 ~3-5h

## 焦点

- R0 / R1 / R2 / Reflect 全部 ✅；6 项 P0 quick fix 入仓 ctest 1062/1062 PASS；reflection 文档落盘
- **当前 Checkpoint 2 等待用户决策（archive 前最后一步）**：
  1. R3+ 拆分顺序（Top 3 推荐 + Level 4 大件单独评估）— 决定哪些拆出独立任务 ID
  2. `/archive` 时机（立即 / 等用户审 reflection / 等 R3+ 决策完）
- 下一轮（pending）：
  - ARCHIVE：archive-TASK-20260430-03.md（含 R3+ task IDs 迁入 activeContext / tasks.md + 改进建议 P0/P1 闭环 + 合并 main 由用户决定）
- 注：本任务在 background agent 模式下运行（用户主线在 04 任务），03 feature 分支独立演进，最终归档时合并 main 由用户审 + 决定时机

## 待处理事项 — TASK-20260430-03 改进建议闭环（reflection §5）

### P0（archive 前必完成）

- [ ] #3 git symbolic-ref --short HEAD commit 前断言守门 → 写入 `.cursor/rules/skills/git-workflow.mdc`「commit 前防御」段（archive 阶段执行）

### P1（下次同类任务前完成，迁入 activeContext 待处理事项）

- [ ] #1 Background agent 双轨模式 + worktree 隔离协议固化 → systemPatterns + workflow rule（已部分写入 systemPatterns，archive 阶段补 rule 文件）
- [ ] #2 plan ×0.6 估时按任务类型分桶系数矩阵 → systemPatterns §最窄路径表升级（已写入 systemPatterns，archive 阶段交叉验证）
- [ ] #4 reflog 作为会话冲突首发诊断工具 → systematic-debugging skill 新段
- [ ] #7 Quick fix 颗粒度 plan 估时 12 min/项 校准 → 已写入 systemPatterns

### P2（长期沉淀，已写入 systemPatterns / techContext）

- ✅ #5 Worktree 删除前 rm -rf cmake FetchContent → 已写入 techContext
- ✅ #6 CMake basic vs full 矩阵 → 已写入 techContext
- ✅ #8 Checkpoint 推荐默认 + 隐式批准 → 已写入 systemPatterns
- ⏳ #9 R3+ 拆分协议「13 候选 → Top N」流程标准化（archive 阶段考虑加入 spec 模板）
- ⏳ #10 review 类任务 spec 模板 → 已写入 systemPatterns，archive 阶段考虑加入 `.cursor/rules/templates/`

### R3+ 13 项拆分任务（待 Checkpoint 2 决策具体拆分顺序）

详见 reflection §6.1 / R1 报告 §8 / 上文「焦点」段 Top 4。

## 待处理事项（P0/P1/P2 后续）

### 长期沉淀（已写入 systemPatterns / techContext / 规则文件）

- **TASK-30-02 升级 systemPatterns.md 已生效**：
  - 「最窄路径」表「极窄档 0.2-0.25×」分类
  - 「Spec 实施模式描述粒度准则」段
- **TASK-30-01 升级规则 4 条 ROI 已部分验证**
- **TASK-26-01 升级规则 ROI 已部分验证**

### 输入材料：8 项 P3 触发型候选（R1 必读）

- TASK-26-02-full（clearance 完整版）
- TASK-26-03（LayoutInline IFC 递归 + bidi）
- TASK-20260424-02（Layout 残余 super-linear ~40%）
- CSS 4 标准逻辑属性 shorthand（`border-block` / `border-inline`）
- `border-image` / `border-radius` 简写
- TASK-20260419-06（HashMap Hash Mixing）
- TASK-20260419-08（`string.h` 剩余 memcpy noinline 化）
- TASK-20260419-12（DrawText 真路径优化，K7 隐式闭环待评估）

## 下一步

- Checkpoint 2 用户决策 R3+ 拆分顺序 → `/archive`（含 P0/P1 改进建议闭环 + R3+ task IDs 迁入）

## 最近归档（速查，详细见 archive 文档）

- `archive-TASK-20260430-02.md`（CSS border shorthand 补全 Level 2，2026-04-30）
- `archive-TASK-20260430-01.md`（first/last child margin collapse with parent Level 3，2026-04-30）
- `archive-TASK-20260426-01.md`（Layout 正确性消化 Level 4，2026-04-30）
- `archive-TASK-20260425-01.md`（SDL2 窗口后端 + 输入事件桥接 Level 3，2026-04-26）
- `archive-TASK-20260424-04.md`（DrawText warm 残余优化 Level 2 D 纯收尾，2026-04-25）
- `archive-TASK-20260424-03.md`（DrawText warm 优化 Level 2-3 K7 Resolved，2026-04-24）
- `archive-TASK-20260424-01.md`（Layout super-linear knee 根因调查，2026-04-24）
- `archive-TASK-20260419-13.md`（流程规则 P0/P1 沉淀冲刺，2026-04-19）
- `archive-TASK-20260419-11.md`（ImageCache::Load HashMap 化，2026-04-19）
- 更早归档见 `memory-bank/archive/` 目录与 `tasks.md §任务历史`
