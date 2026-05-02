# 活跃上下文

## 当前阶段

**构建中·轮次 7 完成（Phase A.1.7 + A.1.8 ✅ → Phase A.1 8/8 全部完成 🎉；下次进入 Phase A.2 安全测 + A.3 examples）** — TASK-20260502-01 DevTool Phase A · Inspector 轮次 7 完成「dogfood 外层闭环组」最后冲刺：A.1.7 vx_view_attach_devtool C API（commit `9a14850`，10 测一次过零迭代，0.52×）+ A.1.8 dogfood headless smoke 集成测（commit `<待合并>`，4 测 GREEN，0.62×）；A.1.8 关键产出 = **dogfood 完整链路打通**（Application 加 devtool_script_engine_ + devtool_dom_bindings_ + devtool_script_status_ + EvalDevtoolScript；LoadDevtoolDocument 编织 5 步 JS 链路 Init→Bind→SetDevtoolTargetDocument→RegisterDevtoolBindings→EvalGlobal kInspectorPanelJs；析构/Unload 安全 tear down 顺序；inspector_panel.js R2 防御性 try/catch 让主链路验证 vx_devtool_get_dom_json 跨 Document JSON envelope 绕过 R2 缺陷）+ **R2 引擎缺陷清单沉淀**（DomBindings 缺 Element.children getter / element.addEventListener / element.innerHTML setter 三连，列入独立 P3 候选）；DEVTOOL=ON ctest 1152→1156 PASS（+4 集成测），DEVTOOL=OFF 1062/1062 维持，libvx_api.a OFF 12646 持平 + libvx_core.a OFF 1771620→1775326 (+3706 = application.cc 新字段实例化，nm 验证零 DevTool 符号引用，A14 链接闭包零严格成立)；A.1.8 实测 ~28 min vs plan 45 min ×0.6 = **0.62×**（plan ×0.6 第 31 数据点入库；3 步迭代 vs 一次过 — R2 防御 + StatusOr DCHECK 两个意外踩坑沉淀教训）；Phase A.1 8/8 已完成（A.1.1-A.1.8 全过 ✅），剩余 Phase A.2 安全测 4 子任务（~63 min plan ×0.6）+ A.3 examples + reflect 2 子任务（~45 min plan ×0.6）共 ~108 min plan ×0.6 完成整个 Phase A；推荐选项 A（续 A.2/A.3）或 C（提前 /reflect 因 Phase A.1 是 Phase A 主体，A.2/A.3 是收尾验证）。 — TASK-20260502-01 DevTool Phase A · Inspector 于 2026-05-02 14:30 完成 plan escalation：用户选 escalation A 升级 + brainstorming 锁定 B-A1.1=b 编译期嵌入 + B-A1.2=a full viewport；Phase A.1 plan 从粗 4 子任务概念 → 8 子任务 detailed 5-步 TDD 模板（plan 文档 +384 行）；Phase A 总子任务 16 → 20；**0 篇新 creative**（架构决策全锁定）；下一步直接 `/build` 续上 A.1.1。

**Brainstorming 锁定决策（B-A1.x 系列）：**

| # | 决策 | 锁定值 | 影响 |
|:-:|---|---|---|
| **B-A1.1** | DevTool resource 加载 | **b 编译期嵌入**（推翻 B4=B → B4=A，CMake `file(READ)` + Python codegen `inspector_resources.cc`）| T2 路径穿越威胁面**完全消除** + 减 1 篇 creative + 加快交付 ~3-5h |
| **B-A1.2** | target Document layout viewport | **a full viewport + 渲染覆盖**（target Document layout viewport = window 全尺寸；DevTool 自渲染时覆盖右侧 270px）| splitter 拖拽零额外开销；与 Chrome DevTools 早期行为一致 |

**架构 grep 验证后 3 项修正（plan 必须反映）：**

| # | 原 plan / spec / creative #1 假设 | 实际架构 | 修正 |
|:-:|---|---|---|
| **M1** | `Renderer::Render(target_doc, devtool_doc, canvas)` class method（creative #1 决策 4）| `render::Record/Replay/Paint` 是 free functions；`UpdateManager` 是 single-Document 设计（cfg.document 单引用）| Application 持有**双 UpdateManager**（target_update_manager_ + devtool_update_manager_），共享 canvas + image_cache，序列 Update 自动叠加；不修改 Renderer / UpdateManager |
| **M2** | `InjectHoverHighlight(dom::Document*, ...)`（plan §A.1.1）| Document 不持有 DisplayList | 改签名 `InjectHoverHighlight(render::DisplayList&, const layout::LayoutBox*, ...)` |
| **M3** | runtime 文件读取 `inspector_panel_loader.cc`（B4=B）| B-A1.1=b 嵌入路径 | 替换为 `veloxa/devtool/resources/CMakeLists.txt` + Python codegen 生成 `inspector_resources.cc` |

**A.1 8 子任务（detailed plan 已落盘）：**

| # | 子任务 | plan ×0.6 | 关键 |
|:-:|---|--:|:-:|
| A.1.1 | `InspectorOverlay::InjectHoverHighlight(DisplayList&, ...)` | 18 min | M2 修正签名 |
| A.1.2 | DevTool resource 编译期嵌入（B-A1.1=b）| 27 min | T2 消除 |
| A.1.3 | `inspector_panel.html/css/js` 编写 | 54 min | dogfood R2 mitigation |
| A.1.4 | JS native binding 扩展 | 36 min | DomBindings pattern extension |
| A.1.5 | InputDispatchSplitter（hit-area + drag capture） | 27 min | 新增 |
| A.1.6 | Application 双 UpdateManager + LoadDevtoolDocument | 36 min | M1 + M3 |
| A.1.7 | `vx_view_attach_devtool` C API + F12 hotkey | 27 min | API 简化（移除 resources_dir）|
| A.1.8 | dogfood headless smoke 集成测 | 45 min | 新增 |
| **合计** | — | **270 min** | — |

**Phase A 累计修正后估时：** 473 min（7.88 h）plan ×0.6（A.0 已完成 ~95 min + A.1 270 min + A.2 63 min + A.3 45 min）— 较原 plan +32 min，可控。

**下一步：** `/build` 启动轮次 3（实际推进 A.1.1）。

**轮次 2 commit 链：**
- `3f3bd38` feat(devtool): vx_devtool 静态库 + Inspector 内部 C++ API（A.0.5）
- `(pending)` feat(api): vx_view_serialize_dom_json + T7（A.0.6 闭环 #40）

**轮次 1 commit 链（已完成）：**
- `e43a5be` chore: VAN + plan 阶段产出
- `0e8e40c` refactor(application): I1 双 Document 槽 → R1 callsite 4 处零漏改
- `4131700` feat(layout): LayoutBox::ToJson() → 闭环技术债 #26（A4 验收）
- `30d54ee` feat(dom): Serializer::ToJson() + T3 password redaction（A1 验收）
- `e98f9fa` feat(render): PaintCommand::kOverlayHighlight + T5 ResetOverlayCommands（A5 验收）

## 当前任务

### TASK-20260502-01：DevTool Phase A — Inspector 实施（DOM tree / Style / Layout panel + hover 高亮）[安全相关]

- **复杂度：** Level 3（中等功能 — 跨 4 子系统 core/devtool/api/tests，plan 16 子任务全部 5-步 TDD 模板就绪）
- **来源：** TASK-20260430-04 蓝图主交付独立立项候选 §主线 3 项之 A
- **分支：** `feature/TASK-20260502-01-devtool-inspector`（基于 main `679304e`）
- **关键文档：**
  - 设计 spec ✅ 复用 `docs/specs/2026-04-30-devtool-design.md`（A1-A5, A13, A14 + T3/T5/T7/T8 + I1/I3/I4/I5/I6 + R1/R2/R5）
  - 实现 plan ✅ **本任务专属** `docs/plans/2026-05-02-devtool-inspector.md`（B1-B8 决策表 + 16 子任务 5-步 TDD 模板 + Phase 0 11 子段实测 + plan ×0.6 第 18 数据点假设 + R7/R8 新风险 + 2 Checkpoint）
  - 蓝图 plan 参照 `docs/plans/2026-04-30-devtool.md` §Phase A（不修改）
  - 创意 ✅ 复用 `memory-bank/creative/creative-devtool-screen-layout.md`（无新 creative 需求）
- **触及技术债（本任务一次性闭环 2 项）：** #26 LayoutBox.Dump → A.0.2 / #40 C API introspection → A.0.5 + A.0.6
- **Plan 关键实证：** R1 callsite 二次实证 4 处真实（远低于蓝图估 10-15）→ 风险 🟢；既有测试 fingerprint event/application 略低 ⚠️ → A.0.1/A.1.1 实施前补 grep；ctest 基线发现 build/ 过期需 reconfigure；jq 缺失 → python3 兜底
- **plan ×0.6 第 18 数据点假设：** 沿用蓝图 7.35 h plan ×0.6 baseline；reflect 阶段验证落「大件类 0.8-1.2×」桶 vs 「极窄档 0.46-0.59× 群组延续」
- **下一步：** `/build` 启动 Phase 0.1 reconfigure（关键前置）+ A.0.1 I1 Application 双 Document 槽改造

## 上次任务（已归档闭环）

### TASK-20260430-04：规划 UI 编辑器与调试器（DevTool 三件套蓝图设计）— ✅ 已归档（2026-05-01 ~03:00）

- **状态：** 已 `--no-ff` 合入 main；feature 分支 `feature/TASK-20260430-04-ui-editor-debugger` 仍存在（可 `git branch -d` 安全删除）
- **归档文档：** `memory-bank/archive/archive-TASK-20260430-04.md`
- **核心产出：**
  - 4 篇蓝图文档（spec + plan + 2 creative，合计 ~1879 行）
  - 8 决策矩阵 D1-D8 全部锁定（用户连续 8 次跳过 AskQuestion 隐式批准 VAN 推荐默认）
  - 8 项 T1-T8 安全威胁建模（一期落地 T2/T3/T5/T6/T7/T8 + 扩展段占位 T1/T4）
  - 4 项历史技术债（#26 / #35 / #40 / #4）闭环 ROI 路径映射
  - 7 项独立立项候选（TASK-30-04-A/B/C 主线 + 4 项扩展段）— **A 已立项为本任务 TASK-20260502-01**
  - reflection 10 节全面回顾 + 10 项改进建议（P0×3 / P1×4 / P2×3）
- **改进建议落实率 90%**：P0 3/3 + P1 4/4 + P2 2/3（剩 1 项 P2 #10 留待 TASK-30-04-A/B/C 立项后实测回填 — **本任务 reflect 阶段触发回填**）
- **plan ×0.6 第 17 数据点入库**：核心轮次 0.27-0.35× plan / 0.46-0.59× plan ×0.6（极窄档 + review 类下限交界）
- **方法论沉淀**：首次 V2=a 蓝图任务工作流变体实践 + 「批量决策跳过 + 批量文档产出」3 数据点群组化「极窄档」+ dogfood 路径作为探测性 acceptance test 概念

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
  - 🟡 #2 EventDispatcher snapshot iteration 防 listener mutation UAF（F-046）— P1 正确性 / 估 2-3 h / Level 2 — **本任务 A.0/A.1 间接相关**
  - 🟡 #3 LoadHTML 重置 `dom_bindings_` 防 use-after-free（F-025）— P1 正确性 / 估 1-2 h / Level 2 — **与 TASK-30-04-C HTML hot reload 扩展段强依赖**
  - 🔴 #4 CSS 属性元数据表（F-022）— P1 维护性 / Level 4 大件 / 1-2 周（架构性重构）

## 待处理事项（P0/P1/P2 后续 — 跨任务沉淀）

### 长期沉淀（已写入 systemPatterns / techContext / 规则文件）

- **TASK-30-04 升级 9 条 ROI 已落实**（reflect §5 + archive §6 闭环）：
  - `.cursor/rules/main.mdc`: § Level 4 蓝图任务 V2=a 工作流变体段
  - `.cursor/rules/skills/brainstorming.mdc`: 跨决策协同度 + 决策跳过率监控
  - `systemPatterns.md`: plan ×0.6 矩阵第 17 数据点（极窄档 0.20-0.35× plan）+ Level 4 蓝图任务 V2=a 工作流变体段 + dogfood 路径段
  - `techContext.md`: TASK-30-04 蓝图主交付摘要段（4 篇文档 + D1-D8 + 7 项独立立项 + 4 技术债闭环）
  - `docs/reports/2026-04-30-codebase-review.md`: F-025 段加 Hot Reload 强依赖交叉记录
- **TASK-30-03 升级 9 条 ROI 已落实**：
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

存放在 `docs/reports/2026-04-30-codebase-review.md`（已合并到 main `2445990`）。Top 4 见上文「上次任务 §R3+ 推荐」。

### 来自 TASK-30-04 蓝图主交付的 7 项独立立项候选（更新 — TASK-30-04-A 已立项）

基于 `docs/plans/2026-04-30-devtool.md` 拆出：

**主线 3 项（A 已立项为本任务）：**

- ✅ **TASK-30-04-A** → **本任务 TASK-20260502-01**（VAN 完成 2026-05-02 12:30）
- **TASK-30-04-B**：DevTool Phase B — Performance Overlay 实施（FPS / 帧时序 / dirty rect 边框高亮）
  - 估时：~7.25 h plan / ~4.35 h plan ×0.6（Level 2-3）
  - 触及技术债闭环：#35 UpdateManager 帧钩子（五钩子）+ #4 ImageCache namespace（DevTool icon 隔离）
  - **立项前置**：TASK-20260502-01 必须先于 B 立项（B 依赖 A 的 Inspector 渲染管线 + UI 框架基础）
- **TASK-30-04-C**：DevTool Phase C — Hot Reload 实施（Linux inotify + CSS-only 增量重载）
  - 估时：~10 h plan / ~6 h plan ×0.6（Level 3）
  - **强依赖**：R3+ #3 F-025 LoadHTML use-after-free 修复（如未来扩展 HTML 增量重载）— 一期 CSS-only 不踩
  - 安全守卫：T2 路径穿越 8 步 + T8 mutation propagation 防御
  - **立项条件**：可与 A 并行（无强依赖）

**扩展段 4 项**（spec §11 占位，按用户优先级排期，无相互强依赖）：

- **TASK-30-04-D**（暂定）：Console JS REPL（V1=B 扩展段，威胁 T1 任意 eval mitigation）
- **TASK-30-04-E**（暂定）：JS Debugger backend（QuickJS Debug API 对接，触及技术债 #44 + 威胁 T6 callback budget）
- **TASK-30-04-F**（暂定）：CDP 远程调试 port（威胁 T4 HMAC token + nonce + loopback only + default off mitigation）
- **TASK-30-04-G**（暂定）：完整 UI 编辑器（dogfood 完整闭环，spec §11 长期愿景）

**估时回填校准**（reflect §5 #10 P2 长期 — **TASK-20260502-01 完成后是首个回填数据点**）

### 输入材料：8 项 P3 触发型候选（codebase review R1 已分析）

- TASK-26-02-full（clearance 完整版）
- TASK-26-03（LayoutInline IFC 递归 + bidi）
- TASK-20260424-02（Layout 残余 super-linear ~40%）
- CSS 4 标准逻辑属性 shorthand（`border-block` / `border-inline`）
- `border-image` / `border-radius` 简写
- TASK-20260419-06（HashMap Hash Mixing）
- TASK-20260419-08（`string.h` 剩余 memcpy noinline 化）
- TASK-20260419-12（DrawText 真路径优化，K7 隐式闭环待评估）

## 收尾清理（可选）

- `git branch -d feature/TASK-20260430-03-codebase-review`（已合并到 main，可安全删除）
- `git branch -d feature/TASK-20260430-04-ui-editor-debugger`（归档后合并到 main，可安全删除）

## 最近归档（速查，详细见 archive 文档）

- `archive-TASK-20260430-04.md`（DevTool 三件套蓝图设计 Level 4 V2=a，2026-05-01）
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
