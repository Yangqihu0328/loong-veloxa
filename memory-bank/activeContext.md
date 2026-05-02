# 活跃上下文

## 当前阶段

**规划中（Plan 完成 ✅）** — TASK-20260502-02 Phase B 实施 plan 落盘（2026-05-02 ~22:10，~30 min）：

**Phase 0 11 子段实测填写完成：**
- §0.1 ctest baseline 二次验证 ✅ DEVTOOL=ON 1169 / DEVTOOL=OFF 1065（与 main `8b2ead4` 终态一致）
- §0.2-0.3 CMake 链接拓扑 + 静态库循环依赖审计 ✅ 无新静态库循环依赖（vx_devtool → vx_core 既有 B 链 A 模式持续）
- §0.4 FetchContent 守卫 ✅ 跳过（零新依赖）
- §0.5 测试基础设施审计 ✅ B.1.1 `friend class PerfOverlayTest` 引入方案明确（veloxa/devtool 既有 0 friend 命中）
- §0.6 边界输入清单 16 条（每条对应 ≥ 1 测，分布 B.0.1 / B.0.2 / B.1.1 / B.1.2 / B.2.3 / B.3.x）
- §0.7 调用链端到端验证 ✅ 五钩子 mapping 5 注入点（FrameStart entry / AfterStyle DetectAndApplyTransitions 后 / AfterLayout 同点 / AfterRender Record 后 / FrameEnd Replay 后）+ FrameStats 5 字段公式表
- §0.8 既有测试隐式契约 fingerprint ✅（update_manager_test 2 测 + renderer_test 19 测 + event_test 0 测 + application_test 2 测 + inspector_panel_smoke_test 14 测）
- §0.9 CSS shorthand 能力 grep 表 ✅ HUD CSS 可行性 100%（`opacity` / `position: fixed` fallback 为 `absolute` 视觉等价 / `border-radius` 等 R2-verified；`pointer-events: none` 不支持 → `data-passthrough="1"` 兜底）
- §0.10 工具链与子系统关闭守门 ✅
- §0.11 工具链 grep 命中 ✅（`rg` 不可用 → 系统 `grep -rE` + Grep tool；jq 缺失 → python3 兜底）

**B1-B8 build 级精化决策表全部锁定（用户 1 次 AskQuestion 选 all_recommended → 8/8 按推荐）：**
- B1=A 独立 plan / B2=A 严格串行 / B3=A 新建 `tests/devtool/overlay/` 子树 / B4=A 单矩形扩展 dirty_rects_ Vector（不替换 last_dirty_rect_ 既有契约）/ B5=A 复用 kOverlayHighlight + 新工厂 OverlayDirtyRect（不新增 enum）/ B6=A 每子任务 1 commit / B7=A 假设 ~3-4 h plan ×0.6 / B8=A 复用 spec

**plan 落盘 ~600 行：** `docs/plans/2026-05-02-devtool-perf-overlay.md`（10 子任务 5-步 TDD 模板 + 完整代码片段 + 4 个 R 风险登记 R3/R7/R8/R9 + 3 Checkpoint + 9 条 systemPatterns 协同度自我对照）

**plan ×0.6 第 38 数据点假设入库：** 蓝图 4.35 h → archive 校准 ~3.5-5 h → VAN F2 发现 ~3-4 h →最终预测 **~3-4 h plan ×0.6（0.55-0.75× 比值）**

**推荐路径：** `/build` 启动 B.0.1 PipelineHooks 五钩子（最大子任务 plan 90 min ×0.6 = 54 min）

**关键 push-back 决策保持：** Phase B Level 3 锁定不 escalate（vs Phase A 中途 escalate 到 Level 4）— 实际 Phase B 无 dogfood UI 子系统级风险 + 无 JS native binding 设计 + 无 SDL2 真窗口新例子。

---

## 当前任务

### TASK-20260502-02：DevTool Phase B — Performance Overlay 实施（FPS / 4 阶段 bars / dirty rect 边框高亮）[安全相关]

- **复杂度：** Level 3（中等功能 — 跨 5 子系统 core/devtool-overlay/devtool-inspector_panel-extension/api/tests，蓝图 plan §Phase B 10 子任务）
- **来源：** TASK-20260430-04 蓝图主交付独立立项候选 §主线 3 项之 B；TASK-20260502-01 archive §10 / §6.3 标注「立项条件就绪」
- **分支：** `feature/TASK-20260502-02-devtool-perf-overlay`（基于 main `8b2ead4`）
- **关键文档：**
  - 设计 spec ✅ 复用 `docs/specs/2026-04-30-devtool-design.md`（A6-A9 + A13-A14 + T5/T6 + I2/I3/I7 + R3）
  - 实现 plan ✅ `docs/plans/2026-05-02-devtool-perf-overlay.md`（~600 行，B1-B8 决策表 + Phase 0 11 子段实测 + 10 子任务 5-步 TDD 模板 + 完整代码片段 + R3/R7/R8/R9 风险登记 + 3 Checkpoint + 9 条 systemPatterns 协同度自我对照）
  - 创意 ✅ 复用 `creative-devtool-screen-layout.md` 决策 3（HUD overlay）+ 决策 5（F11 toggle）
- **触及技术债（本任务一次性闭环 1 项 + 1 项不闭环留 P3）：** ✅ #35 UpdateManager frame hook → B.0.1（阶段 1 闭环；阶段 2 拆 LayoutEngine 内 style/layout 子阶段留 P3）/ ⊘ #4 ImageCache namespace（HUD 不需 icon，留 P3）
- **F1-F9 grep 实证：** 5 ✅ 已就绪 / 1 ⚠️ 部分（#4 不闭环）/ 1 🔴 需新建（#35 五钩子）— 比 Phase A 启动时 5/2/2 更优
- **下一步：** `/build` 启动 B.0.1 PipelineHooks 五钩子（最大子任务 plan 90 min ×0.6 = 54 min）

---

## 最近任务核心成果（速查 — TASK-20260502-01 Phase A）

- 16/16 子任务完成（A.0/A.1/A.2/A.3 跨 4 Phase / 8 轮 build / 28 commits）
- ctest DEVTOOL=ON 1062 → **1169 PASS（+107 测）**；DEVTOOL=OFF 1062 → **1065 PASS（+3 测）**
- A14 链接闭包零自动化守门（`tests/smoke/devtool_a14_link_closure.cmake` 每次 ctest 自动 nm 验证 8 符号黑名单）
- 4 安全威胁 mitigation 全到位（T2 完全消除 / T3 redact / T5 overlay 隔离 / T7 buffer 三层守卫）
- 2 历史技术债闭环（#26 LayoutBox.Dump / #40 C API DOM introspection）
- 3 R2 P3 候选沉淀（DomBindings Element.children / addEventListener / innerHTML setter — 见下文「待处理事项 §R2 P3 候选」）
- 7 公开 C API + JS native binding + Hello example + dogfood 完整链路打通
- plan ×0.6 矩阵第 18-37 数据点入库（20 个新数据点群组），「大件实现」桶系数下调 0.8-1.2× → 0.6-1.1× + 3 子桶
- 3 新 systemPattern 沉淀（plan escalation 中途触发 + 反向探针有效性陷阱清单 + 子系统关闭守门 ctest smoke 范式）
- **5 大可复用架构范式**（Phase B 直接用 4 项）：双 UpdateManager / 双层 API / #ifdef + CMake conditional / canvas Translate / 资源嵌入

---

## 上次任务（已归档闭环）

### TASK-20260502-01：DevTool Phase A — Inspector 实施 [安全相关] — ✅ 已归档（2026-05-02 ~18:10）

- **复杂度：** Level 4（plan escalation 自 Level 3 升级）
- **归档文档：** `memory-bank/archive/archive-TASK-20260502-01.md`
- **回顾文档：** `memory-bank/reflection/reflection-TASK-20260502-01.md`
- **分支：** `feature/TASK-20260502-01-devtool-inspector`（基于 main `679304e`，归档时由用户决策合并方式）
- **改进建议落实率：** P0 3/3 ✅（沉淀到 systemPatterns）+ P1 5/5（A14 smoke template ✅ + dogfood SOP ✅ + 余 3 项已迁移到下方「待处理事项」）+ P2 2/2 ✅
- **方法论沉淀**：首次 Level 4 实施类大件任务（区别于 TASK-30-04 蓝图任务 V2=a）+ 首次 plan escalation 中途触发实证（Phase A.1 0.99×）+ 首次 dogfood 完整链路验证（target Document ↔ DevTool Document JS native binding 跨 Document inspection）
- **闭环 reflection §6 P2 #10 估时回填校准（前置任务遗留项）：** ✅ 完成 — TASK-20260502-01 是首个 TASK-30-04-A/B/C 拆分任务实测点，Phase A 总系数 0.64× 落「大件实现」桶下沿外，触发桶系数下调 0.8-1.2× → 0.6-1.1×

### TASK-20260430-04：规划 UI 编辑器与调试器（DevTool 三件套蓝图设计）— ✅ 已归档（2026-05-01 ~03:00）

- **状态：** 已 `--no-ff` 合入 main；feature 分支 `feature/TASK-20260430-04-ui-editor-debugger` 仍存在（可 `git branch -d` 安全删除）
- **归档文档：** `memory-bank/archive/archive-TASK-20260430-04.md`
- **核心产出：** 4 篇蓝图文档（spec + plan + 2 creative 合计 ~1879 行）+ D1-D8 决策矩阵 + T1-T8 安全威胁建模 + 7 项独立立项候选（A 已立项为本任务 TASK-20260502-01 ✅）
- **plan ×0.6 第 17 数据点入库**：核心轮次 0.27-0.35× plan / 0.46-0.59× plan ×0.6（极窄档 + review 类下限交界）

### TASK-20260430-03：全代码库 Code Review — ✅ 已归档（2026-05-01 ~00:30）

- **状态：** 已 `--no-ff` 合并到 main `2445990`（11 commits + 1 merge commit）
- **归档文档：** `memory-bank/archive/archive-TASK-20260430-03.md`
- **R3+ 13 项 P1 候选** 待用户决策拆分顺序后独立立项；Top 4 推荐：
  - 🔴 #1 image_decoder 安全三件套（F-049 / F-050 / F-051）— P1 安全 / 估 4-6 h / Level 3
  - 🟡 #2 EventDispatcher snapshot iteration 防 listener mutation UAF（F-046）— P1 正确性 / 估 2-3 h / Level 2
  - 🟡 #3 LoadHTML 重置 `dom_bindings_` 防 use-after-free（F-025）— P1 正确性 / 估 1-2 h / Level 2 — **与 TASK-30-04-C HTML hot reload 扩展段强依赖**
  - 🔴 #4 CSS 属性元数据表（F-022）— P1 维护性 / Level 4 大件 / 1-2 周（架构性重构）

---

## 待处理事项（P0/P1/P2 后续 — 跨任务沉淀）

### P1 来自 TASK-20260502-01 reflection §6（archive 阶段迁移）

> 已沉淀到 systemPatterns/techContext 的项目不重复列；下方仅列**需要后续 audit / skill 文件调整 / 新独立任务**才能完成的项。

- **P1 #4 git-workflow 多子任务连续完成时分 commit 前 `git add -p` 选择性 stage** — 来源：A.2.1/A.2.4/A.3.1 三个子任务在 `tests/CMakeLists.txt` 同一文件累积变更，因 `git add tests/CMakeLists.txt` 而非 `git add -p` 导致 A.2.1 commit 隐含包含 A.2.4 + A.3.1 部分变更（功能正确，语义不洁）；建议在 `.cursor/rules/skills/git-workflow.mdc` 「Multi-subtask commit 拆分」段补充 `git add -p` 推荐范式 + checklist。**预估**：~10-15 min skill 文档调整；下次涉及多子任务连续 commit 时由 build-phase 触发。
- **P1 #6 StatusOr<T>::status() 使用规范统一为三元守卫（codebase audit）** — 来源：A.1.8 踩 `devtool_script_status_ = eval.status()` DCHECK abort 坑，已修复并沉淀到 `techContext.md` 「Status / StatusOr 使用规范」段；audit 行动项：`rg "StatusOr.*\.status\(\)"` 全 codebase 验证无误用 + 必要时改为 `r.ok() ? Status::Ok() : r.status()` 三元守卫；考虑加 clang-tidy custom check 强制规范。**预估**：~30-60 min audit + ~1-2 h clang-tidy check 编写（如选 enforce 路径）；下次 codebase 卫生 / quality 类任务（如 TASK-30-03-style review round）时合并执行。
- **P1 #8 spec template「A14 解读」附录段（C ABI stub 公开表面 vs DevTool 闭包精确区分）** — 来源：A.1.7/A.1.8 累计 +4196 bytes 在 OFF binary 中（490 + 3706）但**链接闭包零严格满足**，spec §6 「A14 链接闭合 + size diff = 0」措辞需明确两层语义；建议在 `docs/specs/2026-04-30-devtool-design.md` §6 加 「附录 A: A14 解读」段（也可在未来类似 conditional 子系统 spec template 中复用）。**预估**：~15-20 min spec 文档调整；下次涉及 A14 类 spec 撰写或 audit 时执行。

### P2 来自 TASK-20260502-01 reflection §6（archive 阶段迁移）

- **P2 #10 DomBindings R2 三连缺陷独立立项（dogfood 暴露的 R2 引擎缺陷）** — 见下方 §「R2 P3 候选 — 来自 TASK-20260502-01 dogfood 暴露」段。

### R2 P3 候选 — 来自 TASK-20260502-01 dogfood 暴露（3 项 — 候选独立立项）

A.1.8 dogfood headless smoke 集成测真实暴露 3 个 DomBindings 缺失能力，列入独立 P3 候选（**不在原任务范围修复**，等待用户优先级排期决定立项时机）：

| # | 缺陷 | 文件位置 | 当前 panel 临时 mitigation | 优先级 |
|:-:|---|---|---|:-:|
| 1 | DomBindings 缺 `Element.children` 集合 getter（HTMLCollection 风格）| `veloxa/script/dom_bindings.cc` | inspector_panel.js inline `if (!tabs.children) return` 防御 | P3 |
| 2 | DomBindings 缺 `element.addEventListener` | `veloxa/script/dom_bindings.cc` | inspector_panel.js inline `if (typeof btn.addEventListener !== "function") return` 防御 | P3 |
| 3 | DomBindings 缺 `element.innerHTML` setter | `veloxa/script/dom_bindings.cc` | renderDomTree silent no-op；vx_devtool_get_dom_json JSON 已覆盖核心数据契约 | P3 |

**建议立项形态：** 单一 Level 3 任务 `TASK-2026MMDD-NN: DomBindings R2 三连补全`（plan ×0.6 ~3-5 h），TDD 三连实施 + 单测覆盖 + DevTool dogfood 视觉自动恢复验证。

### 长期沉淀（已写入 systemPatterns / techContext / 规则文件）

- **TASK-20260502-01 升级落实（archive 阶段）：**
  - `memory-bank/systemPatterns.md`: § plan escalation 中途触发段 + § 反向探针有效性陷阱清单段 + § 子系统关闭守门 ctest smoke 范式段（含 cmake template）+ plan ×0.6 矩阵 20 新数据点 + 「大件实现」桶系数下调 + 3 子桶
  - `memory-bank/techContext.md`: § TASK-20260502-01 DevTool Phase A · Inspector 实施摘要段 + § Status / StatusOr 使用规范段
  - `memory-bank/productContext.md`: § 最近能力更新（2026-05-02）段 — DevTool Phase A 主线 user-facing 能力清单
- **TASK-30-04 升级 9 条 ROI 已落实**（reflect §5 + archive §6 闭环）：
  - `.cursor/rules/main.mdc`: § Level 4 蓝图任务 V2=a 工作流变体段
  - `.cursor/rules/skills/brainstorming.mdc`: 跨决策协同度 + 决策跳过率监控
  - `systemPatterns.md`: plan ×0.6 矩阵第 17 数据点（极窄档 0.20-0.35× plan）+ Level 4 蓝图任务 V2=a 工作流变体段 + dogfood 路径段
  - `techContext.md`: TASK-30-04 蓝图主交付摘要段（4 篇文档 + D1-D8 + 7 项独立立项 + 4 技术债闭环）
  - `docs/reports/2026-04-30-codebase-review.md`: F-025 段加 Hot Reload 强依赖交叉记录
- **TASK-30-03 升级 9 条 ROI 已落实**：
  - `systemPatterns.md`: Background agent 双轨模式 + worktree 隔离协议
  - `systemPatterns.md`: plan ×0.6 任务类型分桶系数矩阵（review 0.4-0.7× / fast-fix 0.7-1.4× / racy 1.4-2.5× / 大件 0.6-1.1×（已含 502-01 调整））
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

### 来自 TASK-30-04 蓝图主交付的 7 项独立立项候选（更新 — TASK-30-04-A 已闭环）

基于 `docs/plans/2026-04-30-devtool.md` 拆出：

**主线 3 项：**

- ✅ **TASK-30-04-A** → **TASK-20260502-01 已归档完成（2026-05-02）**
- **TASK-30-04-B**：DevTool Phase B — Performance Overlay 实施（FPS / 帧时序 / dirty rect 边框高亮）
  - 估时（**TASK-20260502-01 估时回填校准后下调**）：~7.25 h plan / **~3.5-5 h plan ×0.6**（原 ~4.35 h，~30% 下调，因可复用本任务 5 大架构范式：双 UpdateManager / 双层 API / #ifdef+CMake / canvas Translate / 资源嵌入）
  - 触及技术债闭环：#35 UpdateManager 帧钩子（五钩子）+ #4 ImageCache namespace（DevTool icon 隔离）
  - **立项条件就绪**：本任务 ✅ 闭环，TASK-30-04-B 可立项
- **TASK-30-04-C**：DevTool Phase C — Hot Reload 实施（Linux inotify + CSS-only 增量重载）
  - 估时（**TASK-20260502-01 估时回填校准后下调**）：~10 h plan / **~2.5-4 h plan ×0.6**（原 ~6 h，~20% 下调，因可复用 F12 hotkey 范式 + DevTool resource embed）
  - **强依赖**：R3+ #3 F-025 LoadHTML use-after-free 修复（如未来扩展 HTML 增量重载）— 一期 CSS-only 不踩
  - 安全守卫：T2 路径穿越 8 步 + T8 mutation propagation 防御
  - **立项条件就绪**：可与 B 并行（无强依赖）

**扩展段 4 项**（spec §11 占位，按用户优先级排期，无相互强依赖）：

- **TASK-30-04-D**（暂定）：Console JS REPL（V1=B 扩展段，威胁 T1 任意 eval mitigation）
- **TASK-30-04-E**（暂定）：JS Debugger backend（QuickJS Debug API 对接，触及技术债 #44 + 威胁 T6 callback budget）
- **TASK-30-04-F**（暂定）：CDP 远程调试 port（威胁 T4 HMAC token + nonce + loopback only + default off mitigation）
- **TASK-30-04-G**（暂定）：完整 UI 编辑器（dogfood 完整闭环，spec §11 长期愿景）

### 输入材料：8 项 P3 触发型候选（codebase review R1 已分析）

- TASK-26-02-full（clearance 完整版）
- TASK-26-03（LayoutInline IFC 递归 + bidi）
- TASK-20260424-02（Layout 残余 super-linear ~40%）
- CSS 4 标准逻辑属性 shorthand（`border-block` / `border-inline`）
- `border-image` / `border-radius` 简写
- TASK-20260419-06（HashMap Hash Mixing）
- TASK-20260419-08（`string.h` 剩余 memcpy noinline 化）
- TASK-20260419-12（DrawText 真路径优化，K7 隐式闭环待评估）

---

## 收尾清理（可选）

- `git branch -d feature/TASK-20260430-03-codebase-review`（已合并到 main，可安全删除）
- `git branch -d feature/TASK-20260430-04-ui-editor-debugger`（归档后合并到 main，可安全删除）
- `feature/TASK-20260502-01-devtool-inspector` 视用户 archive 阶段决策（合并 / 保留 / PR）后可清理

---

## 最近归档（速查，详细见 archive 文档）

- `archive-TASK-20260502-01.md`（DevTool Phase A · Inspector 实施 Level 4，2026-05-02）— **本批最新**
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
