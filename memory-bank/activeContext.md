# 活跃上下文

## 当前阶段

**TASK-20260430-04 `/reflect` 阶段完成（2026-05-01 ~02:30，反思文档落盘，正在落实 P0/P1 改进建议）** — reflection-TASK-20260430-04.md 已落盘（10 节全面反思，含 plan ×0.6 第 17 数据点入库 + 10 项改进建议 P0/P1/P2 分级）；P0 #1 #3 #7 改进已落实（main.mdc V2=a 工作流变体段 + systemPatterns 极窄档第 17 数据点 + activeContext 7 项独立立项候选迁移）；P1 #4 已落实（systemPatterns Level 4 蓝图任务 V2=a 工作流变体段）；下一步进入 `/archive` 归档收尾。

**D1-D8 决策矩阵（已锁定）：**
- D1 三件套实施优先级：**B Inspector → Overlay → Hot Reload**
- D2 Inspector 数据采集协议：**B 半结构化（JSON tree + DisplayList overlay + C API JSON）**
- D3 DevTool UI 主屏布局：**B 同窗口 splitter dock + Overlay HUD 子模式**（→ creative #1）
- D4 DevTool 隔离边界：**B 单进程共享容器（双 Document + 共享 EventLoop/App/ImageCache）**
- D5 Hot Reload file watcher + 增量策略：**A 嵌入式专注（Linux inotify + CSS-only）**（→ creative #2）
- D6 Performance Overlay 数据采集：**B Chrome DevTools 风格（五钩子 + 滑动 60 帧 + dirty rect 边框高亮）**
- D7 C API 扩展边界：**C 双层 API（内部 C++ 核心 + 公开 C API 薄封装）**
- D8 安全威胁建模：**A T2/T3/T5/T6/T7/T8 完整 + T1/T4 扩展段占位**

**4 篇产出文档：**
- spec：`docs/specs/2026-04-30-devtool-design.md`（12 段 / 三件套验收 A1-A14 / D1-D8 决策矩阵 / 注入点 I1-I8 核对表 / T1-T8 威胁建模 / R1-R6 风险登记 / ≥ 30 systemPatterns 自我对照）
- plan：`docs/plans/2026-04-30-devtool.md`（Phase 0/A/B/C/D 划分 + CMake 链接审计 + 静态库循环审计 + 边界输入清单 16 项 + 子任务清单 ~40 项 + plan ×0.6 估时矩阵）
- creative #1：`memory-bank/creative/creative-devtool-screen-layout.md`（splitter dock + HUD overlay 双层结构 / dock-to-right vs dock-to-bottom 切换协议 / dirty rect ↔ Inspector hover 红框双线宽渲染顺序）
- creative #2：`memory-bank/creative/creative-devtool-hot-reload.md`（FileWatcher 跨平台抽象 + InotifyFileWatcher T2 8 步守卫 / DOM 状态保留协议 / CSS 解析失败错误恢复）

**主线 plan ×0.6 总估时（V2=a 蓝图任务自身）：** 315-375 min plan / 189-225 min plan ×0.6（Level 4 蓝图，预期落 review 类 0.4-0.7× 区间）

**用户后续独立立项候选（基于 plan 拆出）：**
- TASK-30-04-A：DevTool Phase A — Inspector 实施（Level 3，~12.25 h plan / ~7.35 h plan ×0.6）
- TASK-30-04-B：DevTool Phase B — Performance Overlay 实施（Level 2-3，~7.25 h / ~4.35 h）
- TASK-30-04-C：DevTool Phase C — Hot Reload 实施（Level 3，~10 h / ~6 h）
- 4 项扩展段（Console / JS Debugger / CDP 远程调试 / 完整 UI 编辑器）— spec §11 占位，独立立项

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

### 来自 TASK-30-04 蓝图主交付的 7 项独立立项候选（P0 reflect §5 #7）

基于 `docs/plans/2026-04-30-devtool.md` 拆出，由用户后续独立立项（**不在 TASK-30-04 自身范围内**）：

**主线 3 项：**

- **TASK-30-04-A**：DevTool Phase A — Inspector 实施（DOM tree / Style panel / Layout panel + 元素 hover 高亮）
  - 估时：~12.25 h plan / ~7.35 h plan ×0.6（Level 3）
  - 触及技术债闭环：#26 LayoutBox.Dump / #40 C API introspection
  - **强依赖**：I1 Application 双 Document 槽改造（R1 风险，重命名 `document_` → `target_document_` mitigation 已 plan）

- **TASK-30-04-B**：DevTool Phase B — Performance Overlay 实施（FPS / 帧时序 / dirty rect 边框高亮）
  - 估时：~7.25 h plan / ~4.35 h plan ×0.6（Level 2-3）
  - 触及技术债闭环：#35 UpdateManager 帧钩子（五钩子）

- **TASK-30-04-C**：DevTool Phase C — Hot Reload 实施（Linux inotify + CSS-only 增量重载）
  - 估时：~10 h plan / ~6 h plan ×0.6（Level 3）
  - **强依赖**：R3+ #3 F-025 LoadHTML use-after-free 修复（如未来扩展 HTML 增量重载）— 一期 CSS-only 不踩
  - 安全守卫：T2 路径穿越 8 步 + T8 mutation propagation 防御

**扩展段 4 项**（spec §11 占位，独立立项）：

- **TASK-30-04-D**（暂定）：Console JS REPL（V1=B 扩展段，威胁 T1 任意 eval mitigation）
- **TASK-30-04-E**（暂定）：JS Debugger backend（QuickJS Debug API 对接，触及技术债 #44 + 威胁 T6 callback budget）
- **TASK-30-04-F**（暂定）：CDP 远程调试 port（威胁 T4 HMAC token + nonce + loopback only + default off mitigation）
- **TASK-30-04-G**（暂定）：完整 UI 编辑器（dogfood 完整闭环，spec §11 长期愿景）

**立项前置条件：**
- TASK-30-04-A 必须先于 TASK-30-04-B 立项（B 依赖 A 的 Inspector 渲染管线 + UI 框架基础）
- TASK-30-04-C 可与 A 并行（无强依赖）
- 扩展段 4 项独立可立，按用户优先级排期（无相互强依赖）

**估时回填校准**（reflect §5 #10 P2 长期）：
- TASK-30-04-A/B/C 立项时记录 plan 实测 vs plan 估时偏差
- 偏差数据反哺 systemPatterns plan ×0.6 矩阵（review 类 + 蓝图任务子分桶）

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

- **进入 `/archive`**（V2=a 蓝图任务收尾）：创建 `memory-bank/archive/archive-TASK-20260430-04.md` + 更新 tasks.md / progress.md / techContext.md 任务历史 + 落实剩余 P1/P2 改进建议（#2 brainstorming 协同度段 / #5 dogfood acceptance test 段 / #6 techContext 蓝图主交付摘要 / #8 R3+ 强依赖交叉记录 / #9 决策跳过率监控 / #10 估时回填校准）+ merge feature branch → main（`--no-ff`）+ 任务闭环

**reflect 阶段已落实改进（P0 全部 + P1 第 1 项）：**
- ✅ P0 #1：main.mdc 加 V2=a 蓝图任务工作流变体段（已加 § Level 4 蓝图任务 V2=a 工作流变体）
- ✅ P0 #3：systemPatterns plan ×0.6 矩阵加蓝图极窄档（第 17 数据点入库 0.20-0.35× plan）
- ✅ P0 #7：activeContext 待处理事项段加 7 项独立立项候选（本文件上方）
- ✅ P1 #4：systemPatterns 加 Level 4 蓝图任务 V2=a 工作流变体段（含 spec 12 段式样 + 自我对照 ≥ 30 模式 + 批量文档 batch 协议）

**archive 阶段待落实（P1 #2 #6 #8 + P2 #5 #9 #10）：**
- ⏳ P1 #2：brainstorming.mdc 加「与已锁定决策的协同度」段
- ⏳ P1 #6：techContext 加 TASK-30-04 蓝图主交付摘要段
- ⏳ P1 #8：R3+ 强依赖（Hot Reload C.2 ↔ R3+ #3 F-025）显式交叉记录
- ⏳ P2 #5：systemPatterns 加 dogfood 路径 = 探测性 acceptance test 段
- ⏳ P2 #9：brainstorming.mdc 加决策跳过率监控（≥ 5 决策连续跳过 reflect 阶段重审）
- ⏳ P2 #10：蓝图任务子任务清单估时颗粒度回填校准（TASK-30-04-A/B/C 立项后）

**TASK-30-03 收尾（可选清理）**：`git branch -d feature/TASK-20260430-03-codebase-review`（已合并到 main，可安全删除）

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
