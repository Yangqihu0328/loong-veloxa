# 活跃上下文

## 当前阶段

**TASK-20260430-04 VAN 完成（2026-04-30 23:50，决策已锁定，等待路由到 `/plan`）** — 「规划 UI 编辑器与调试器」VAN 阶段全部完成；9 项 grep 实证 F1-F9 已落地（5 ✅ 已就绪 / 4 ⚠️ 需扩展 / 6 🔴 需新建）；用户两次跳过 AskQuestion → 按 VAN 推荐默认锁定 V1-V5（**B 三件套 / a 纯蓝图 / A 自渲染 / Level 4 / ✅ 安全相关**）；触及技术债 4 项映射闭环 ROI 路径明确；前置验证 6/6 全 PASS。

> **上一任务 TASK-20260430-03**（全代码库 Code Review）已于 **2026-05-01 ~00:30 归档并 `--no-ff` 合并到 main `2445990`**（11 commits + 1 merge commit）。改进建议沉淀已落实到 `systemPatterns.md` / `techContext.md` / `git-workflow.mdc` / `systematic-debugging.mdc`；R3+ 13 项 P1 后续候选已入仓 `docs/reports/2026-04-30-codebase-review.md`，待用户决策拆分顺序后立项（不在本 TASK-30-04 范围内）。

## 当前任务

### TASK-20260430-04：规划 UI 编辑器与调试器（DevTool 主线蓝图设计）[安全相关]

- **复杂度：** **Level 4 锁定**（V4 = Level 4 — 多子系统蓝图 + 架构决策矩阵 + Spec 主交付 → 触发 `/plan` + `/creative` 强制路径）
- **来源：** 用户主动发起；TASK-20260425-01 archive 备注「解锁 DevTool 主线（hot reload / Inspector / FPS overlay 前置）」首发触发；候选模块包含 Inspector / DOM 树 / Style 检查 / Layout 检查 / JS 调试器 / Hot Reload / Performance Overlay / 完整可视化 UI 编辑器 等
- **意图判读：** 用户用「**规划**」二字（非「实现」）→ 强烈暗示主交付物是**蓝图级 spec + plan**（含子系统拆分 + 子任务 ID 列表 + 优先级排序），而非完整端到端实现 → V2 = a 纯蓝图锁定一致
- **分支：** `feature/TASK-20260430-04-ui-editor-debugger`（基于 main `9411584`，已与最新 main `2445990` 合并完成同步，merge commit `90c5aeb`）

### V1-V5 决策已锁定（用户两次跳过 AskQuestion → 按 VAN 推荐默认锁定）

| # | 维度 | **锁定值** | 推荐理由 |
|:-:|---|---|---|
| **V1** | 子系统范围 | **B 三件套** = Inspector（DOM/Style/Layout）+ Hot Reload + Performance Overlay | DevTool 主线 ROI 最高 3 项；Console / JS Debugger / 完整 UI Editor 标为「扩展候选」入 spec §扩展段 |
| **V2** | 输出形态 | **a 纯蓝图** = spec + plan + creative ×N，**不强制实施代码** | 与「规划」语义最匹配；Level 4 多子系统蓝图任务在 build 前应有用户审视点；build 由用户基于产出 plan 拆出独立 build 任务 |
| **V3** | DevTool UI 渲染层 | **A Veloxa 自渲染** — DevTool UI 自身用 Veloxa HTML/CSS 引擎 | dogfood 模式 — DevTool 即 Veloxa 自我应用样板 + 反向暴露引擎缺陷；vs ImGui-like 多一套 UI lib / vs CDP 要求外置 browser 不能离线运行 |
| **V4** | 复杂度级别 | **Level 4** | 多子系统蓝图 + 架构决策矩阵 → /plan + /creative 强制路径 |
| **V5** | 安全标注 | ✅ **是 [安全相关]** | JS REPL 任意 eval / Hot Reload 路径穿越 / 远程调试 port 暴露 / Inspector 敏感数据回显 4 个威胁面 |

### 触及技术债映射（与 `techContext.md §技术债务清单` 对照）

| # | 技术债 | DevTool 子系统 | 闭环节奏 |
|:-:|---|---|---|
| #26 | LayoutBox.Dump 调试方法缺失 | Inspector Layout 面板 | 本任务 plan 内规划，build 阶段实施（若推进至 build）|
| #35 | UpdateManager 未暴露 frame hook | Performance Overlay | 同 #26 |
| #40 | C API 缺 DOM/Style/Layout introspection | Inspector 全子系统 | 同 #26 |
| #44 | QuickJS Debug API 未对接 DevTool | 扩展候选 JS Debugger | V1=B 扩展段，不阻塞主交付 |

### VAN 阶段 grep 实证（F1-F9，2026-04-30 23:40 完成）

| # | 命题 | grep 实证 | 影响设计 |
|:-:|---|---|---|
| F1 | 现有代码是否已有 inspector / devtool / debugger 实现 | ❌ **0 处实现代码**（仅历史 spec / archive 提及） | 完全从零设计 |
| F2 | C API 是否已有 introspection 接口 | ❌ 无 `vx_view_get_document` / `vx_view_dump_*` / `vx_view_get_fps`（C API ~125 行 `veloxa_api.h`） | C API 需扩展（与技术债 #40 重叠） |
| F3 | DOM 序列化能力 | ✅ `vx::dom::Serialize(node)` 已可输出 HTML 字符串 | Inspector DOM 树文本化复用基础 |
| F4 | LayoutBox / DisplayList Dump | ❌ 无 `Dump()` 方法（**技术债 #26**） | 需新增 |
| F5 | UpdateManager 帧生命周期钩子 | ❌ 无 `OnBeforeUpdate` / `OnAfterUpdate`（**技术债 #35**） | Performance Overlay 必需 |
| F6 | JS Debug API 集成 | ❌ `JS_SetInterruptHandler` / 执行预算未实施（**技术债 #44**） | JS 调试器需先做基础设施 |
| F7 | SDL2 后端 / 可见窗口 | ✅ TASK-25-01 已就绪（`hello_sdl2` 范本，`Sdl2WindowSurface` + `Sdl2EventLoop`） | DevTool UI 渲染主线起点 |
| F8 | EventManager hover/active/focus | ✅ HitTest + 状态指针（TASK-08/09 沉淀） | Inspector 元素高亮选取复用基础 |
| F9 | Application 暴露内部状态 | ✅ `document() / event_manager() / update_manager() / event_loop()` 全已 expose | Inspector 读取无障碍 |

**初步识别 Veloxa 引擎的 DevTool 基础设施成熟度：**
- 🟢 **已就绪**（无需新做）：可见窗口 / DOM 序列化 / EventManager 状态 / Application getters / SDL2 输入桥接
- 🟡 **需扩展**（小工程）：C API 加 introspection / LayoutBox.Dump / DisplayList.Dump / UpdateManager 钩子（4 项约 ~3-5h plan 量级）
- 🔴 **需新建**（大工程）：Inspector UI 渲染层 / Hot Reload 文件监听 + 增量解析 / FPS overlay / Console JS 桥接 / JS 调试器 backend / 完整 UI 编辑器（每项 Level 3 量级）

## 上次任务（已归档闭环）

### TASK-20260430-03：全代码库 Code Review — ✅ 已归档（2026-05-01 ~00:30）

- **状态：** 已 `--no-ff` 合并到 main `2445990`（11 commits + 1 merge commit）；feature 分支 `feature/TASK-20260430-03-codebase-review` 仍存在（可 `git branch -d` 安全删除）
- **归档文档：** `memory-bank/archive/archive-TASK-20260430-03.md`
- **核心产出：**
  - R0 准备（commit `ba1cf8b`，~22 min）— grep fingerprint + lcov + CVE 审计 + R1 抽样名单
  - R1 全代码库 6 维度 review 报告（commit `802a273`，~85 min）— 934 行 / 55 项 findings（28 P1 / 19 P2 / 8 P3）
  - R2 P0 quick fix 6 项（F-020 / F-033 / F-040 / F-026 / F-053 / F-055，~70 min，6 commits 全过 ctest 1062/1062）
  - Reflect + Archive 全闭环（plan ×0.6 第 16 数据点入库，0.85-1.00× 区间）
- **改进建议落实率 90%**：P0 1/1 + P1 4/4 + P2 4/5（剩 1 项 P2 #9 留作 ad-hoc）
- **R3+ 13 项 P1 候选（待用户决策拆分顺序后独立立项）：** 详见 `docs/reports/2026-04-30-codebase-review.md`，Top 4 推荐：
  - 🔴 #1 image_decoder 安全三件套（F-049 PNG alpha / F-050 width×height 溢出 / F-051 JPEG `error_exit` kill）— P1 安全 / 估 4-6 h / Level 3
  - 🟡 #2 EventDispatcher snapshot iteration 防 listener mutation UAF（F-046）— P1 正确性 / 估 2-3 h / Level 2
  - 🟡 #3 LoadHTML 重置 `dom_bindings_` 防 use-after-free（F-025）— P1 正确性 / 估 1-2 h / Level 2
  - 🔴 #4 CSS 属性元数据表（F-022）— P1 维护性 / Level 4 大件 / 1-2 周（架构性重构）

## 待处理事项（P0/P1/P2 后续 — 跨任务沉淀）

### 长期沉淀（已写入 systemPatterns / techContext / 规则文件）

- **TASK-30-03 升级 9 条 ROI 待外部任务验证**：
  - `systemPatterns.md`: Background agent 双轨模式 + worktree 隔离协议
  - `systemPatterns.md`: plan ×0.6 任务类型分桶系数矩阵（review 0.4-0.7× / fast-fix 0.7-1.4× / racy 1.4-2.5× / 大件 0.8-1.2×）
  - `systemPatterns.md`: Quick fix 颗粒度估时基准（12 min/项）
  - `systemPatterns.md`: Checkpoint 推荐默认 + 隐式批准协议
  - `systemPatterns.md`: Review 类任务 6 维度 × 抽样深度矩阵 spec 模板
  - `techContext.md`: CMake basic vs full 配置矩阵（VX_BUILD_BENCHMARKS + VX_PLATFORM_SDL2 默认值差额导致 ctest count 1031 vs 1061-1062）
  - `techContext.md`: Background agent 双轨 worktree 隔离工程指引（含 FetchContent root-owned hooks 清理）
  - `.cursor/rules/skills/git-workflow.mdc`: commit 前防御断言（multi-session safety）— `git symbolic-ref --short HEAD` 守门
  - `.cursor/rules/skills/systematic-debugging.mdc`: 并发会话 / 多 agent 冲突诊断（首发工具 `git reflog`）
- **TASK-30-02 升级 systemPatterns.md 已生效**：
  - 「最窄路径」表「极窄档 0.2-0.25×」分类
  - 「Spec 实施模式描述粒度准则」段
- **TASK-30-01 升级规则 4 条 ROI 已部分验证**
- **TASK-26-01 升级规则 ROI 已部分验证**

### 来自 codebase review R1 报告的新输入（13 项 R3+ 候选）

存放在 `docs/reports/2026-04-30-codebase-review.md`（已合并到 main `2445990`）。Top 4 见上文「上次任务 §R3+ 推荐」。本 TASK-30-04 任务对这些 P1 候选**不依赖**，可在 04 任务进行中或之后独立立项。

### 输入材料：8 项 P3 触发型候选（codebase review R1 已分析）

- TASK-26-02-full（clearance 完整版）
- TASK-26-03（LayoutInline IFC 递归 + bidi）
- TASK-20260424-02（Layout 残余 super-linear ~40%）
- CSS 4 标准逻辑属性 shorthand（`border-block` / `border-inline`）
- `border-image` / `border-radius` 简写
- TASK-20260419-06（HashMap Hash Mixing）
- TASK-20260419-08（`string.h` 剩余 memcpy noinline 化）
- TASK-20260419-12（DrawText 真路径优化，K7 隐式闭环待评估）

## 下一步

- **TASK-30-04 主线**：V1-V5 已按 VAN 推荐默认锁定（B / a / A / Level 4 / ✅）→ 用户输入 `/plan` 进入头脑风暴（预期 D1-D6 决策矩阵：DevTool UI 主屏布局 / Inspector 数据采集协议层 / Hot Reload 触发粒度 / Overlay 数据流 / 自渲染下外置协议保留与否 / 安全模型）
- **预期 spec 主交付（12 段式样）：** 目的 / 不做 / 三件套验收 A1-A12 / D1-D6 决策矩阵 / 架构图 + 注入点核对表 / T1-T8 安全威胁建模 / 扩展候选段（Console / JS Debugger / 完整 UI Editor）/ 与既有 systemPatterns 兼容性段 / R1-R6 风险登记 / 与未来任务关系
- **预期 ≥ 2 篇 creative：** DevTool UI 主屏布局 / Inspector 数据采集协议层
- **不进入 `/build`** — V2 = a 纯蓝图主交付，build 由用户基于产出 plan 拆出独立 build 任务
- **TASK-30-03 收尾（可选清理）**：删除已合并的 feature 分支 `git branch -d feature/TASK-20260430-03-codebase-review`

## 最近归档（速查，详细见 archive 文档）

- `archive-TASK-20260430-03.md`（全代码库 Code Review Level 4，2026-05-01）
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
