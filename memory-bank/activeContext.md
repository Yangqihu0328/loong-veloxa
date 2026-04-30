# 活跃上下文

## 当前阶段

**初始化（2026-04-30 23:40）** — 新任务 TASK-20260430-04「规划 UI 编辑器与调试器」VAN 阶段进行中；上一任务 TASK-20260430-03（全代码库 Code Review）在自己 feature 分支独立演进（已并发完成 R0+R1+R2.1+R2.2，由 background agent 持续推进，未来用户可独立审 + 收尾）。

## 当前任务

### TASK-20260430-04：规划 UI 编辑器与调试器（DevTool 主线蓝图设计）

- **复杂度：** Level 4（待 V1-V5 用户决策锁定后确认；预期多子系统 + 大量架构决策）
- **来源：** 用户主动发起；TASK-20260425-01 archive 备注「解锁 DevTool 主线（hot reload / Inspector / FPS overlay 前置）」首发触发；候选模块包含 Inspector / DOM 树 / Style 检查 / Layout 检查 / JS 调试器 / Hot Reload / Performance Overlay / 完整可视化 UI 编辑器 等
- **意图判读：** 用户用「**规划**」二字（非「实现」）→ 强烈暗示主交付物是**蓝图级 spec + plan**（含子系统拆分 + 子任务 ID 列表 + 优先级排序），而非完整端到端实现
- **分支：** `feature/TASK-20260430-04-ui-editor-debugger`（基于 main `9411584`，已创建）
- **VAN 阶段焦点：**
  1. 范围 V1 — 选哪些 DevTool 子系统？（Inspector / Hot Reload / Overlay / Console / 编辑器 / JS 调试器 6 候选）
  2. 输出形态 V2 — 仅蓝图（spec + plan + 子任务列表）/ 蓝图 + MVP（最小可运行子集）/ 蓝图 + 全实施（含 R3+）
  3. DevTool UI 渲染层 V3 — Veloxa 自渲染 / ImGui-like immediate mode / CDP 兼容外置协议 / 多模式蓝图共存
  4. 复杂度级别 V4 — Level 3（单子系统聚焦）/ Level 4（多子系统蓝图，预期）
  5. 安全标注 V5 — Hot Reload 涉及文件系统访问 / JS 调试器涉及外部协议 → 大概率 [安全相关]

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

## 上次任务（在自己 feature 分支独立演进）

### TASK-20260430-03：全代码库 Code Review — 🔵 在 feature 分支并发推进中

- **状态：** 在 `feature/TASK-20260430-03-codebase-review` 上由 background agent 持续推进；本任务对其无依赖
- **进度（最后观察 2026-04-30 23:40）：**
  - ✅ R0 准备（commit `ba1cf8b`，~22 min）
  - ✅ R1 全代码库 6 维度 review 报告（commit `802a273`，~85 min）— 934 行 / 55 项 findings（28 P1 / 19 P2 / 8 P3）
  - ✅ R2.1 F-020 selector_matcher dead return 修复（commit `3b4b2e7`）
  - ✅ R2.2 F-033 ProcessEndTag isize/usize 重复转换（commit `1467207`）
  - 🔵 R2 剩余 4 项推进中（F-026 / F-040 rasterizer.cc 注释中 / F-053 / F-055）
  - ⏳ Checkpoint 2 / R3+ / reflect / archive — 待该分支自行收尾
- **未来用户审视点：** 该分支自然演进完毕后，用户可 `git checkout feature/TASK-20260430-03-codebase-review` 审 R1 报告 + R3+ 拆分建议 → 决定 13 项 P1 R3+ 任务的优先级 → 合并 main
- **关键 P1 输入（13 项 R3+ 候选预览）：** 详见 `docs/reports/2026-04-30-codebase-review.md`（已入仓 feature 分支），Top 5：F-051 JPEG `error_exit` 杀进程 / F-050 PNG/JPEG 整数溢出 / F-046 EventDispatcher iterator 失效 / F-025 LoadHTML UAF / F-022 CSS 属性元数据 shotgun

## 待处理事项（P0/P1/P2 后续 — 跨任务沉淀）

### 长期沉淀（已写入 systemPatterns / techContext / 规则文件）

- **TASK-30-02 升级 systemPatterns.md 已生效**：
  - 「最窄路径」表「极窄档 0.2-0.25×」分类 — 下次同类任务（既有 same-subsystem 范本 100% 复用 + 0 创意 + 0 新护栏）应直接按 plan × 0.22 预估
  - 「Spec 实施模式描述粒度准则」段 — 下次 spec §5 应聚焦「契约 + 约束」而非具体分支结构
- **TASK-30-01 升级规则 4 条 ROI 已部分验证**
- **TASK-26-01 升级规则 ROI 已部分验证**

### 来自 codebase review R1 草稿的新输入（13 项 R3+ 候选）

存放在 `docs/reports/2026-04-30-codebase-review.md`（在 codebase review feature 分支已入仓）。Top 5 见上文「上次任务 §关键 P1 输入」。本 TASK-30-04 任务对这些 P1 候选**不依赖**。

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

- 用户对 V1-V5 范围决策做出选择 → 进入 `/plan`（如选定 Level 4 蓝图设计）

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
