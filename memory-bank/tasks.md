# 任务跟踪

## 当前任务

> **TASK-20260503-04 创意完成（2026-05-04 ~15:30）** — 阶段：**创意中→构建前**（VAN ✅ + Plan ✅ + Creative ✅ → 等待 `/build` 5 子任务串行 + CP1+CP2）。详细见下方任务段。

### TASK-20260503-04：DevTool Phase D — Console JS REPL + console.log 桥接（V1=B 扩展段）[安全相关] — 🟢 创意完成 2026-05-04 ~15:30

- **当前阶段：** **创意中→构建前**（VAN ✅ + Plan ✅ + **Creative ✅** — 12/12 brainstorm+plan 决策锁定 / 4 维度 C1-C4 详细设计完成 / Phase 0 7 子段实证完成 / Plan 文档 ~530 行 + Creative 文档 ~720 行 / 等待 `/build`）
- **创建日期：** 2026-05-03（搁置） / **恢复日期：** 2026-05-04
- **复杂度级别：** **Level 3**（恢复时以原范围重启 — V3=A 锁定）
- **焦点：** `veloxa/devtool/console/` + `console_panel.html/css/js`（第 4 件套 UI）+ isolated JSRuntime + capability allowlist + JS_SetInterruptHandler 集成 + console.log 桥接
- **分支：** `feature/TASK-20260503-04-devtool-console-repl`（基于 main `509fec3` ✅ 已创建）
- **已锁定决策（搁置时锁定，恢复时复用，无需重问）：**
  - **V1=B**：完整 Console Panel（DevTool 第 4 件套 UI — `console_panel.html/css/js` 复用 inspector_panel 双层 API 范式 + REPL input + 滚动 output + console.log 桥接 + C API 完整）
  - **V3=A**：严格按 spec — 05 做 #44 独立前置已完成（commit `63a0bab` 闭环 ✅），04 在 05 完成后以 Level 3 原范围重启
- **触及威胁：** **T1 任意 eval mitigation**（首次完整暴露 — Console 引入是 T1 威胁面落地实施）
- **触及技术债：** 闭环 spec §11.1 Console 占位 / 解锁 TASK-30-04-E JS Debugger backend / 复用 TASK-05 闭环的 SetEvalInterruptBudget API + WasInterrupted（V1=B 一行配置即可）
- **安全相关：** ✅ 是（T1 任意 eval — isolated JSRuntime + capability allowlist 双层防御 + interrupt handler 复用）
- **估时假设：** ~3-4 h plan ×0.6（VAN 校准；落「极窄档延续高效区 0.30-0.45×」候选续延 Phase B/C 同档）

#### 前置验证（4/4 PASS）

| 维度 | 检查内容 | 结果 |
|---|---|:-:|
| 依赖可获取性 | quickjsng 已离线预置（`build/_deps/`）/ 零新依赖 / interrupt handler 已闭环（TASK-05 commit `63a0bab` / `kDefaultInterruptBudgetCheckpoints = 10000` 已就位 / `WasInterrupted()` 已就位 / `kAborted` enum 已就位）/ FetchContent F9 ⊘ 跳过 | ✅ |
| 环境就绪 | main `509fec3` 干净 / cmake 4.2.3 + gcc 15.2.0 + ninja + ctest + ld 2.46 hotfix 全可用 / build/ DEVTOOL=ON 1252 baseline 已知（TASK-05 闭环值）/ rg 仍 MISS → Grep 兜底 | ✅ |
| 已有 artifact | ⚠️ **混合**：可复用 ✅（inspector_panel.{html,css,js} 双层 API 范式 + inspector_resources.h 资源嵌入范式 + overlay HUD 范式 + hot_reload status indicator 范式 + QuickjsEngine class + Application::LoadDevtoolDocument）+ 待新建 ⏳（`veloxa/devtool/console/` 子系统 + console_panel.{html,css,js} + creative-devtool-console.md 设计文档 + 第 4 件套 UI 在 DevTool 主屏布局占位 — creative-devtool-screen-layout.md 未预留） | ✅ |
| 待处理事项 | 闭环 spec §11.1 Console 占位 ✅ + 闭环 TASK-05 reflection §6 P3 #2「kCancelled 进一步语义拆分」可顺带触及 + 解锁下游 TASK-30-04-E JS Debugger backend ✅ + Phase 0 P1×2 待处理事项可在本任务 plan 阶段顺带验证（writing-plans audit 子条 + brainstorming 主动 push-back 模式） | ✅ 极强 |

#### VAN 阶段识别的关键 push-back（待 brainstorm/creative 决策）

| # | 维度 | 关键发现 | 待决策 |
|:-:|---|---|---|
| C1 | DevTool 主屏第 4 件套布局 | creative-devtool-screen-layout.md 仅预留 3 tab（DOM/Style/Layout）+ 1 HUD（FPS+stages+HR），**未预留 Console 位置** | 4 tab 扩展（Console 同 inspector tab 平级）vs 底部 dock（Chrome DevTools 风格）vs splitter 分离上下区域 |
| C2 | isolated JSRuntime 实施 | `QuickjsEngine` 当前是**单 runtime + 单 context**（`rt_` / `ctx_` 单实例 / quickjs_engine.h:56-57）；spec §11.1 强制 isolated JSRuntime | 新建独立 `QuickjsEngine` 实例 vs 新建 `ConsoleQuickjsEngine` 子类 vs capability flag 复用既有 |
| C3 | capability allowlist 实施 | spec §11.1 要求「默认仅 read-only DOM access，禁 file / network / Hot Reload trigger」 — 当前 DomBindings 设计未区分 read-only vs mutable | DomBindings 增加 read-only mode flag vs 新建 ConsoleDomBindings 子集 vs JS-level Object.freeze 包裹 |
| C4 | console.log 桥接 | target Document 的 `console.log(x)` 需输出到 DevTool Document Console panel — 涉及双 Document 跨 binding | console.log → DevTool C API 回调 → DevTool JS 端 dom append 节点 vs 共享 JSON queue + 轮询 drain |
| C5 | Interrupt handler 集成 | TASK-05 已闭环 `SetEvalInterruptBudget(N)` + `WasInterrupted()` API — V1=B 仅需 1 行 `console_engine.SetEvalInterruptBudget(10000)` + 错误识别 | ✅ 无设计决策（直接复用） |

**结论：** C1-C4 共 4 维度需 creative 阶段决策 → 触发 V4=✅ creative 必要 → 产出 `memory-bank/creative/creative-devtool-console.md`

#### Phase 0 极简清单（plan 阶段实证）

- §0.1 ctest baseline 二次验证 DEVTOOL=ON 1252 + DEVTOOL=OFF 1087（TASK-05 闭环值）
- §0.2 grep `JS_NewRuntime` / `JS_NewContext` 当前用法实证 + isolated runtime 多实例可行性
- §0.3 grep `inspector_panel` 双层 API 范式 + `LoadDevtoolDocument` 范式 + `inspector_resources.h` 资源嵌入范式
- §0.4 grep DomBindings 当前公开 API 表 + read-only 隔离切入点
- §0.5 grep `vx_devtool_register_callback` 范式 + JS-side console.log 桥接路径
- §0.6 配置矩阵假设：DEVTOOL=ON 1252→1252+N（含 N 新测）/ DEVTOOL=OFF 1087 不变（A14 黑名单加 ConsoleEngine + ConsoleDomBindings + ConsoleBridge 3-N 项）
- §0.7 反复模式预防：grep + 实证「现有实现 runtime 行为假设未实证」（反复模式 #1 第 10 次抑制）

#### 推荐工作流（V3=A Level 3 标准路径）

`/van` ✅ → **`/plan`（含 brainstorm C1-C5 决策表 + Phase 0 实证 + B1-BN plan 决策表）→ `/creative`（4 维度设计 → creative-devtool-console.md）→ `/build`（4-5 子任务串行 + CP1+CP2）→ `/reflect` → `/archive`**

#### 关键约束（VAN 阶段锁定）

- ABI 严格扩展（新增公开 C API + JS native binding，不改既有签名）— 复用 lazy-attach C ABI 容错模式 + 双层 API 范式（Phase A/B/C 沉淀）
- ctest 双 config 不退化（DEVTOOL=ON 1252→1252+N / DEVTOOL=OFF 1087 不变）+ A14 链接闭包黑名单更新（ConsoleEngine 等 3-N 项）
- Source 溯源 commit body 范式延续（19 commits triple-evidence 已达 git-workflow.mdc 固化阈值）
- 反复模式预防：Phase 0 三层抑制（§0.2 isolated runtime 可行性 grep + §0.4 DomBindings read-only 切入点 grep + §0.6 ctest config 矩阵 audit 含 add_test guard 检查 — 闭环 TASK-05 reflection P1 #1 audit 子条）
- T1 mitigation 5 维度完整性验证（默认 isolated runtime + capability allowlist 不可绕过 + interrupt handler 复用 + console.log 桥接 redact / 双向覆盖单测 / dogfood 路径覆盖）

#### 9 systemPatterns 协同度自我对照（plan 阶段细化）

预期：**5 ✅**（双层 API + #ifdef + CMake conditional + lazy-attach C ABI 容错 + canvas Translate / 资源嵌入 + dogfood 路径）+ **2 🆕**（isolated JSRuntime 隔离模式新沉淀 + capability allowlist 范式新沉淀）+ **1 🎯**（连续第 6 次零反复目标 — Phase 0 三层抑制延续 TASK-05 范式）+ **1 ⊘**（A14 守门 — 沿用既有自动化）

---

#### Plan 阶段产出（2026-05-04 ~14:55）

**12/12 决策锁定（C1-C4 brainstorm + B1-B8 plan / 1 次 AskQuestion + all_recommended 范式）：**

| 维度 | 锁定 |
|:-:|---|
| C1 第 4 件套布局 | A 第 4 tab 平级（DOM/Style/Layout/Console） |
| C2 isolated JSRuntime | B 新建 console_script_engine_（第 3 个 QuickjsEngine 实例） |
| C3 capability allowlist | A 严格隔离 binding（不调 DomBindings::Bind） |
| C4 console.log 桥接 | A drain 模式（vx_devtool_get_console_log_drain query） |
| B1 子任务划分 | A 5 子任务串行（D.1 → D.2 → D.3 [CP1] → D.4 → D.5 [CP2]） |
| B2 测试模式 | A 推荐组合（D.1/D.2/D.4 [覆盖补充] / D.3 [文档调整] / D.5 [覆盖补充] + [集成测]） |
| B3 buffer 上限 | A 双上限（max_count=1000 + max_text=4 KiB） |
| B4 lifecycle | A 与 LoadDevtoolDocument 同步 |
| B5 drain envelope | A 完整格式（messages + truncated + dropped_count） |
| B6 公开 C API | A vx_view_eval_console + vx_view_console_log_drain |
| B7 Checkpoint | A CP1 D.3 + CP2 D.5 |
| B8 commit + 估时 | A 5 commits + Source 溯源 / plan ×0.6 ~180-240 min |

**Plan 文档：** `docs/plans/2026-05-04-devtool-console-repl.md`（~530 行 / 14 段 / 5 子任务详细 + 完整代码骨架 + 5 R 风险 + T1 mitigation 5 维度自检表 + 2 Checkpoint + 9 systemPatterns 协同度自我对照 + 反复模式预防清单）

**估时（VAN+Plan 校准）：** plan ×0.6 ~180-240 min → 实测预期 ~70-105 min（落「极窄档延续高效区 0.30-0.45×」候选续延 Phase B/C 同档；plan ×0.6 第 73-77 数据点群组假设入库）

#### 需要创意阶段的组件（V4=✅ creative 必要）

C1-C4 4 维度需 `/creative` 阶段产出详细设计文档：

| # | 维度 | brainstorm 锁定方向 | creative 阶段详细设计内容 |
|:-:|---|---|---|
| C1 | DevTool 主屏第 4 件套布局 | A 第 4 tab 平级 | tab 切换协议精化 / dock-to-bottom 视觉范式（Chrome 参考）/ Enter keydown 与既有冲突解决 / CSS input element 子集 grep 验证 |
| C2 | isolated JSRuntime 实施 | B 新建 console_script_engine_ | 与 devtool_script_engine_ 协同设计 / lifecycle 时序图 / DEVTOOL=OFF 路径 #ifdef / 与 creative-quickjs-host 兼容性自查 |
| C3 | capability allowlist 实施 | A 严格隔离 binding | RegisterConsoleBindings 全量 API 表 vs RegisterDevtoolBindings 对比表 / 反向探针单测设计 / spec §11.1 安全证明 |
| C4 | console.log 桥接 | A drain 模式 | drain JSON envelope 完整规范 / 双上限 mitigation 算法 / utf8 边界截断算法 / level 颜色 mapping / ts_ms 时间源选择 |

**待产出：** ~~`memory-bank/creative/creative-devtool-console.md`（预期 ~400-500 行）~~ ✅ **已落盘 ~720 行**（详见下方「Creative 阶段产出」段）

#### Creative 阶段产出（2026-05-04 ~15:30）

**创意文档落盘：** `memory-bank/creative/creative-devtool-console.md`（**~720 行** / vs plan 预期 ~400-500 行 = 1.4-1.8× 文档密度延续 plan 文档密度规律）

**4 维度详细设计完成（C1-C4 全决策含探索方案 + 对比表 + 实现指导 + 代码骨架）：**

| 维度 | 决策 | 关键设计输出 | 关键 push-back |
|:-:|---|---|---|
| **C1** DevTool 主屏第 4 件套布局 | A 第 4 tab 平级 | inspector_panel.html 集成范式 + console_panel.{html,css,js} 完整代码骨架 + Enter keydown 冲突 0 风险表 | **input element 子集**：grep 实证 Veloxa 自渲染层 input 无浏览器原生输入行为 → 用 `<span>` + JS 自管理 value state（不用 `<input>`） |
| **C2** isolated JSRuntime 实施 | B 新建 console_script_engine_ | lifecycle 时序图（Load 19 步 / Unload 7 步 / 关键时序约束 3 项）+ application.h #ifdef 包围范式（Phase C R4 范式延续）+ 与 creative-quickjs-host.md 兼容性自查表（4 项 ✅） | **JS_SetMemoryLimit audit 待办**：留 P3（不阻塞本任务） |
| **C3** capability allowlist 实施 | A 严格隔离 binding | RegisterConsoleBindings 全量 API 表（7 函数）vs RegisterDevtoolBindings 对比 + 完整代码骨架（~150 行）+ 反向探针单测设计（5+ mutator + 5 allowlist）+ spec §11.1 安全证明（5 维度全自动满足） | **opaque-ptr channel 冲突**：路径 2 解决（Console 不暴露 dom_json/perf_stats，用户从 console_panel.js 内通过 vx_console_eval 间接访问） |
| **C4** console.log 桥接 | A drain 模式 | drain JSON envelope 完整规范（messages/level/text/ts + truncated + dropped_count 6 字段语义表）+ 双上限 mitigation 算法（max_count=1000 drop_oldest + max_text=4096 truncate_utf8_safe）+ DrainAsJson envelope 构建 + level 颜色 mapping 5 类 + ts_ms 时间源 system_clock | **TruncateAtUtf8Boundary 自实现**：grep 实证 utf.cc 0 boundary 函数 → 自实现 + 5 case 单测 |

**Phase 0 Creative pre-check 实证：**

- **#1 input element 子集**: `tag.cc:67` 已 `kInput, kInlineBlock, kVoid` 注册但**无原生输入行为** → C1 改用 `<span>` + JS keydown
- **#2 utf8 boundary 函数**: `utf.cc` 0 命中 `FindUtf8Boundary` → C4 自实现（参考 EncodeUtf8 字节判定逻辑）
- **#3 RegisterDevtoolBindings 完整 API**: 实证 3 函数全 query-only + opaque-ptr 冲突识别 → C3 路径 2 解决

**§d.1 / §d.2 触发评估**：4 维度均无 ≥ 2 坐标系算法 / 无累积量赋值歧义 → **均不触发**（已显式标注）

**Build 阶段待解决（已在 creative 文档标注）：**

| # | 待解决 | 责任阶段 | 备注 |
|:-:|---|---|---|
| 1 | opaque-ptr channel 冲突 — 默认走路径 2 | D.2 build | creative §C3 已锁定路径 |
| 2 | quickjs_engine.cc audit `JS_SetMemoryLimit` | D.1 build 前 | 如未调用 → 留 P3 |

**4 项待用户审查重点（creative 文档末尾段）：**

1. C1 `<span>` + JS 自管理 value state 方案（input 原生支持留 V2）
2. C2 application.h #ifdef 包围粒度（默认整段包围）
3. C3 不暴露 dom_json/perf_stats 给 Console（用户用 vx_console_eval 间接访问）
4. C4 max_count=1000 + max_text=4 KiB 默认值（可配置上限留 P3）

#### 5 子任务清单（B1.A 锁定）

| # | 子任务 | plan ×0.6 | 实测预期 | 测试模式 | 文件数 |
|:-:|---|:-:|:-:|---|:-:|
| D.1 | ConsoleEngine + Application lifecycle | 30-40 min | 12-18 min | [覆盖补充] | 8 |
| D.2 | RegisterConsoleBindings + drain API | 40-50 min | 15-22 min | [覆盖补充] | 6 |
| D.3 | console_panel 资源 + tab 集成 | 40-50 min | 15-22 min | [文档/资源调整] | 10 |
| — | 🛑 **CP1 自审** | — | ~3 min | — | — |
| D.4 | 公开 C API + JS native binding | 30-40 min | 12-18 min | [覆盖补充] | 5 |
| D.5 | 单测扩充 + dogfood smoke + A14 + finalize | 40-60 min | 16-25 min | [覆盖补充] + [集成测] | 9 |
| — | 🛑 **CP2 自审**（终局） | — | ~5 min | — | — |
| **合计** | — | **180-240 min** | **70-105 min** | — | ~38 / ~800-900 行 |

**下一步：** ~~用户调用 `/creative` 启动创意阶段~~ ✅ 已完成 → 用户调用 `/build` 启动子任务串行（D.1 → D.2 → D.3 → CP1 → D.4 → D.5 → CP2）

## 任务历史（最近完成）

### TASK-20260503-05：QuickJS Interrupt Handler + SetEvalInterruptBudget API（技术债 #44 组件 1 Phase 2 闭环）[安全相关] — ✅ 已归档（2026-05-03 ~23:55）

> **本任务已归档闭环。详细执行记录已迁移到 `memory-bank/archive/archive-TASK-20260503-05.md`（详细 12 段 ~352 行）。**

- **当前阶段：** ✅ **已归档闭环**（VAN ✅ + Plan ✅ + Build ✅ + Reflect ✅ + Archive ✅）
- **复杂度级别：** Level 2
- **回顾文档：** `memory-bank/reflection/reflection-TASK-20260503-05.md`（Level 2+ 12 段 ~280 行）
- **归档文档：** `memory-bank/archive/archive-TASK-20260503-05.md`（Level 2+ 12 段 ~352 行）
- **闭环成果：** 9 commits（VAN + plan + 5 实施 E1-E5 全 Source 溯源 + reflect + archive）/ DEVTOOL=ON 1247→1252 + DEVTOOL=OFF 1082→1087 全 PASS + QuickjsEngine 9/9 全 PASS / T1 mitigation 5 维度完整性 ✅ / 技术债 #44 组件 1 Phase 2 闭环 ✅（JSMallocFunctions 仍记技术债）/ 解锁 TASK-20260503-04 Console JS REPL ✅ / 反复模式 0/7 第 5 次零反复 ✅ / Phase 0 投入定律 sext-evidence 升级（5.2-16× ROI 平均 8.1× / 16× ROI 创历史新高）/ plan ×0.6 实测 0.16× 触发新效率区子档「最小代码改动 + Phase 0 高度预跑极速区 0.10-0.20×」首个数据点入库 / 主动 push-back 模式（D8b）首次实证 / DEVTOOL=OFF baseline 验证 SOP（FETCHCONTENT_BASE_DIR 复用范式）首次实证 / commit body Source 溯源 triple-evidence（19 commits 累计）/ 改进建议 P0 1/1 ✅（systemPatterns plan ×0.6 矩阵 + 数据点 + sext-evidence）+ P1 2/2 ✅ 已迁移 activeContext 待处理事项（writing-plans audit 子条 + brainstorming 主动 push-back 模式）+ P2 1/1 ✅（systemPatterns DEVTOOL=OFF baseline SOP）
- **创建日期：** 2026-05-03
- **分支：** `feature/TASK-20260503-05-quickjs-interrupt-handler`（基于 main `72f011e` ✅ 创建 → 9 commits → fast-forward merge 到 main `63a0bab` ✅ feature 分支已删除）
- **来源：** TASK-20260503-04 VAN 阶段 push-back 识别硬前置依赖技术债 #44（spec §11.1 明示）→ 用户 V3=A 决策独立立项；focus = creative-quickjs-host.md §组件 1 方案 C Phase 2 完整落地
- **安全相关：** ✅ 是（T1 mitigation 基础设施 — JS eval CPU DoS 缓解）

<!-- TASK-20260503-05 详细子任务范围 / V1-V5 决策 / brainstorm 4 决策 / B1-B8 plan 决策 / Phase 0 grep 实证 + Status.h audit / 9 systemPatterns 协同度自我对照 / 反复模式预防清单 / plan ×0.6 数据点已迁移到 archive 文档（见 archive-TASK-20260503-05.md §2-§7）

**原 TASK-20260503-05 详细执行记录摘要（折叠保留供历史检索）：**
- **焦点：** 实现 `QuickjsEngine::SetEvalInterruptBudget(usize max_checkpoints)` + `JS_SetInterruptHandler` 注册 + `WasInterrupted()` API + `StatusCode::kAborted` + 死循环中止单测；作为 TASK-20260503-04 Console JS REPL 的硬前置依赖（spec §11.1 明示）
- **依赖：** `memory-bank/creative/creative-quickjs-host.md` §组件 1 方案 C Phase 2（已批准 2026-04-13）
- **触及威胁：** T1 基础设施（JS eval CPU DoS mitigation — 解锁 TASK-04 Console 完整 T1 mitigation）
- **触及技术债：** #44 QuickJS Interrupt Handler（本任务闭环组件 1 Phase 2 / 组件 3 JSMallocFunctions 仍记技术债 / `JS_SetMemoryLimit` 已落地无需本任务）
- **估时：** plan ×0.6 ~85-105 min / **实测 ~15 min（0.16×）**

#### 任务范围（V1-V5 默认锁定）

| # | 维度 | 选择 | 理由 |
|:-:|---|---|---|
| V1 | 子任务范围 | **5 子任务**（E1 API + E2 实现 + E3 单测 + E4 techContext + E5 finalize）| creative §组件 1 Phase 2 范围明确锁定 |
| V2 | 实施模式 | **b 完整实施**（含 plan / build / verify / commit / reflect / archive） | 与 Level 2 标准路径一致 |
| V3 | 复杂度 | **Level 2** | 5 文件修改 / 需求清晰 / 无新组件 / 无设计决策（creative 已预决策） |
| V4 | 创意需求 | **❌ 否** | creative §组件 1 方案 C Phase 2 已批准 |
| V5 | 安全标注 | **✅ 是** | T1 mitigation 基础设施 |

#### 5 子任务清单（详细子任务定义）

| # | ID | 标题 | 文件 | plan ×0.6 | 测试模式 |
|:-:|---|---|---|:-:|---|
| 1 | E1 | API + StatusCode 扩展声明（Status.h `kAborted` + quickjs_engine.h 公开方法 + 私有字段）| `Status.h` (+1) + `quickjs_engine.h` (+12-15) | ~10 min | [覆盖补充] |
| 2 | E2 | Init 注册 InterruptHandler + EvalGlobal 重置预算 + 静态 InterruptCallback + 错误识别（"interrupted" → kAborted） | `quickjs_engine.cc` (+40-50) | ~30-40 min | [覆盖补充] |
| 3 | E3 | 5 新测（D5 A+C 组合 + 4 维度覆盖 — 中止 / 关闭 / 不误杀 / 重置 / WasInterrupted 语义）| `quickjs_engine_test.cc` (+80-100) | ~30-40 min | [覆盖补充] |
| — | 🛑 CP1 自审 | E3 完成 → DEVTOOL=ON 1247 → 1252 + 既有 4 测无回归 / DEVTOOL=OFF 1082 不退化 / D2/D5/D8b 全实证 | — | — | — |
| 4 | E4 | techContext.md #44 条文更新（JS_SetMemoryLimit 已落地 + interrupt handler 本任务闭环 + JSMallocFunctions 仍记技术债）| `techContext.md` (+5-8) | ~5 min | [文档调整模式] |
| 5 | E5 | finalize（MB 三件套同步 + 分支合并 ff）| `tasks.md` + `activeContext.md` + `progress.md` | ~10 min | — |
| — | 🛑 CP2 自审 | E5 完成 → 5 commits Source 溯源完整 / 反复模式 0/7 自检 / 新 P3 候选迁移 | — | — | — |

**总估时：** plan ×0.6 ~85-105 min / **实测预期 ~25-45 min**（落「极窄档延续高效区 0.30-0.45×」候选）

#### 验收要点（A1-A10 详见 plan §5）

- A1: DEVTOOL=ON ctest 1247 → 1252 PASS（+5 新测）
- A2: DEVTOOL=OFF ctest 1082 PASS（不退化）
- A3: 既有 4 QuickjsEngine 测无回归
- A4-A6: API + 实现 + Status.h 变更范围严格控制
- A7-A8: D2/D5/D8b 实证（kAborted + 双向 budget 覆盖 + 默认值不误杀）
- A9: techContext.md #44 闭环 + JSMallocFunctions 技术债保留
- A10: 5 commits 全 Source 溯源 + 1 commit/子任务

#### 前置验证（4/4 PASS — VAN 阶段）

| 维度 | 检查内容 | 结果 |
|---|---|:-:|
| 依赖可获取性 | quickjsng 已离线预置 / 零新依赖 / F9 ⊘ 跳过 | ✅ |
| 环境就绪 | main `72f011e` 干净 / cmake 4.2.3 + gcc 15.2.0 + ninja + ctest / build/ DEVTOOL=ON 1247 baseline 可用 | ✅ |
| 已有 artifact | `quickjs_engine.{h,cc}` 极简结构待扩展 / `quickjs_engine_test.cc` 已存在含三元守卫范本 / creative-quickjs-host.md §组件 1 方案 C Phase 2 决策已就位 | ✅ |
| 待处理事项 | 闭环 techContext.md #44 + creative-quickjs-host.md §组件 1 Phase 2 占位 + 解锁 TASK-20260503-04 Console JS REPL（搁置） | ✅ 极强 |

#### Phase 0 极简 1 子段（plan §0 — 含 3 grep 实证 + Status.h audit）

- §0.1 工具链快照 ✅（cmake 4.2.3 + gcc 15.2.0 + ninja + ctest + ld 2.46 hotfix）
- §0.2 ctest baseline DEVTOOL=ON 1247/1247 PASS 6.42s（与 main 一致）
- §0.3.1 `JS_SetInterruptHandler` 签名实证 ✅（quickjs.h:1147-1149）
- §0.3.2 `JS_INTERRUPT_COUNTER_INIT = 10000` 实证 ✅（quickjs.c:474-476）→ **触发 D8b push-back**
- §0.3.3 `JS_ThrowInterrupted` 错误信息实证 ✅（quickjs.c:8165-8169）— "interrupted" + uncatchable
- §0.3.4 `Status.h` StatusCode audit ✅（无 kAborted）→ **触发 D2.B.1 决策**
- §0.4 配置矩阵假设：DEVTOOL=ON 1247→1252 / DEVTOOL=OFF 1082 不变 / SDL2=ON 1265→1270
- §0.5 smoke 工具：rg ❌ MISS → Grep 兜底已实证

#### Brainstorm 决策（4 项 — D1+D2+D5+D8b 用户已批准）

| # | 维度 | 决策 |
|:-:|---|---|
| D1 | `WasInterrupted()` Phase 2 vs Phase 3 边界 | **B：Phase 2 实现（私有 bool flag + getter）** |
| D2 | interrupt 错误码 / 消息设计 | **B.1：本任务新增 `StatusCode::kAborted = 6` enum 值**（Phase 0 audit 触发）|
| D5 | budget=0 死循环测避免 ctest hang 策略 | **A+C 组合**（不真测死循环 + 小 budget=10/100 准死循环反向探针）|
| D8b | creative「10⁷ 级检查点」单位语义重定义 | **B：默认 `kDefaultInterruptBudgetCheckpoints = 10000`**（handler 调用次数 / 与 JS_INTERRUPT_COUNTER_INIT 对齐 / 死循环 100-500ms 内中止） |

#### Plan 阶段决策表（B1-B8 — 用户 1 次 AskQuestion 选 all_recommended → 8/8 按推荐锁定）

| # | 维度 | 锁定 |
|:-:|---|---|
| B1 | 子任务执行顺序 | E1 → E2 → E3 (CP1) → E4 → E5 (CP2) |
| B2 | 测试模式 | E1/E2/E3 [覆盖补充] / E4 [文档调整] / E5 — |
| B3 | 默认 budget 值常量 | `static constexpr usize kDefaultInterruptBudgetCheckpoints = 10000`（公开 / 暴露给单测）|
| B4 | InterruptCallback 实现 | 静态 free function（cc anon namespace）+ opaque ptr → `QuickjsEngine*` + `std::atomic<int64_t>` 计数器 |
| B5 | budget=0 语义 | 显式短路：budget=0 时 handler 内 `return 0` 不递减（统一注册 handler 路径减少代码分支）|
| B6 | EvalGlobal 重置点 | 入口处一次性：`interrupt_counter_.store(budget, relaxed); was_interrupted_ = false;` |
| B7 | Phase 0 + Checkpoint | Phase 0 极简 1 子段 + CP1（E3 后）+ CP2（E5 后）|
| B8 | commit 粒度 + 估时 | 5 commits 1/子任务 + Source 溯源前缀 + 1 reflect + 1 archive = 7 commits 总 / plan ×0.6 ~85-105 min → 实测 ~25-45 min |

#### 9 systemPatterns 协同度自我对照

详见 plan §7。摘要：4 ✅（Phase 0 quint-evidence / 反复模式渐进式抑制 / Multi-subtask commit / 显式语义状态）+ 4 ⊘（双层 API / A14 / lazy-attach / dogfood — 本任务不涉及）+ 1 🎯（连续第 5 次零反复目标 — 抵消 03 的 1/7 回升）

#### 推荐工作流

✅ `/van` → ✅ `/plan` → `/build`（5 子任务串行 + CP1 + CP2）→ `/reflect` → `/archive` → 用户决策是否立即恢复 TASK-20260503-04 Console

#### 关键约束

- `quickjs_engine.h` ABI 扩展（新增公开方法 + constexpr，不改既有方法签名）
- `Status.h` 仅追加 enum（不修改既有 6 项 / u8 空间充裕 / backwards-compatible）
- ctest 双 config 不退化（DEVTOOL=ON 1247→1252 / DEVTOOL=OFF 1082 不变）
- 5 commits + Source 溯源前缀「`Source: TASK-20260503-05 creative-quickjs-host.md §组件 1 方案 C Phase 2`」
- 反复模式 0/7 自检（CP2 必检）— Phase 0 三层抑制 + 主动 push-back D8b + audit D2

#### 反复模式预防清单

详见 plan §8。Top 抑制目标：#1 前置依赖/环境/API 能力未验证（历史 9 次） — 本任务三层抑制（Phase 0 §0.3 三 grep + D2 audit + D8b push-back）

#### plan ×0.6 数据点假设入库

第 67-72 数据点群组（5 子任务 + 1 finalize）/ 假设比值 0.30-0.45×（落「极窄档延续高效区」候选续延 02 0.21× 与 03 0.42× 之间）— **实测 0.16× 落新子档「最小代码改动 + Phase 0 高度预跑极速区 0.10-0.20×」**（已入 systemPatterns / 详见 archive §5）

-->

### TASK-20260503-03：DevTool 三件套主线收官 — 4 项 P3 候选批量清零

- **复杂度级别：** **Level 2**（多文件修改 / 需求清晰 / 4 项小型清理 / 无新组件 / 无设计决策）
- **状态：** ✅ **已完成（已归档闭环）**（VAN ✅ + Plan ✅ + Build ✅ + Reflect ✅ + Archive ✅）
- **回顾文档：** `memory-bank/reflection/reflection-TASK-20260503-03.md`
- **归档文档：** `memory-bank/archive/archive-TASK-20260503-03.md`
- **闭环成果：** 5 commits 完成 3/4 P3（P3.1+P3.2 三元守卫 fix + P2 三件套装裱 + P4 README 72 行 + finalize）+ P1 多帧验证 ❌ 取消升级为 P3 候选 #0「Performance Overlay 持续 invalidate 机制」+ 反复模式 #1 第 9 次命中适用范围扩展（runtime 行为假设维度）+ Phase 0 投入定律 quint-evidence（5 任务 → law）+ commit body Source double-evidence + plan ×0.6 0.42× 极窄档延续高效区首次入库 + 改进建议 P0 1/1 ✅（writing-plans）+ P1 2/2 ✅（sysPatterns + techContext）+ P2 1/1 迁移待处理事项段
- **创建日期：** 2026-05-03
- **分支：** `feature/TASK-20260503-03-devtool-trio-finalize`（基于 main `5667c8c` ✅ fast-forward merge 到 main `f93ef07` ✅ feature 分支已删除）
- **来源：** 用户 2026-05-03 通过 `/van DevTool 三件套主线收官` 启动 → AskQuestion intent=B（P3 候选清零）+ items=P6（P1+P2+P3+P4 全选）
- **安全相关：** ❌ 否

<!-- TASK-20260503-03 详细子任务范围 / audit 表 / B1-B9 决策 / 前置验证 / 推荐工作流 / 关键约束已迁移到 archive 文档（见 archive-TASK-20260503-03.md §3）

**原 TASK-20260503-03 详细执行记录摘要：**
- **创建日期：** 2026-05-03
- **分支：** `feature/TASK-20260503-03-devtool-trio-finalize`（基于 main `5667c8c` / +5 commits：P3.1 `ebe5fab` + P3.2 `95a43e7` + P2 `3a8ccb2` + P4 `af3b34e` + finalize 待 commit）
- **Build 阶段调整事件**：P1 实施触发反复模式 #1 命中（VAN/plan 阶段未深读 `update_manager.cc:17` `if (!dirty_) return;` 硬约束 / dirty_ 仅 transition_mgr_.HasActive() 才 rearm / 静态 CSS → 第 1 帧后永久 frames=1）→ 用户 AskQuestion p1_fix=A 回退 P1 + 拆细化为「P3 候选 #0 Performance Overlay 持续 invalidate 机制」入 activeContext 待处理事项段
- **设计 spec：** ❌ 不需（4 项 P3 全部为既有功能小补强，无新组件 / 无 API 设计 / 无安全边界变更 — 直接以 plan 替代 spec）
- **实现 plan：** ✅ `docs/plans/2026-05-03-devtool-trio-finalize.md`（~370 行 / 6 子任务 [覆盖补充] ×2 + [文档调整模式] ×2 + [覆盖补充] + finalize / Phase 0 极简 1 子段含 audit 预跑结论 / B1-B9 9/9 锁定 / CP1+CP2 / 9 systemPatterns 协同度自我对照 / §3.1 ctest config 矩阵 / §3.3 plan ×0.6 第 62-67 数据点假设入库）
- **创意文档：** ❌ 否（无设计空白）
- **需要创意阶段：** ❌ 否
- **来源：** 用户 2026-05-03 通过 `/van DevTool 三件套主线收官` 启动 → AskQuestion intent=B（P3 候选批量清零）→ AskQuestion items=P6（P1+P2+P3+P4 全选）；4 项全部来自 `activeContext.md` 待处理事项段（3 项来自 TASK-20260502-02 reflection §5 + 1 项来自 TASK-20260503-02 reflection §6 P2 #3）
- **安全相关：** ❌ 否（无新外部输入处理 / 无认证 / 无数据存储 / 无部署；P3 #3 GoogleTest 三元守卫属代码 robustness 改进，非安全边界）

#### 任务范围（V1-V5 默认锁定）

| # | 维度 | 选择 | 理由 |
|:-:|---|---|---|
| V1 | 子任务范围 | **4 子任务**（P1 / P2 / P3 / P4） | 用户 AskQuestion items 选 P6 锁定 4 项全清理 |
| V2 | 实施模式 | **b 完整实施**（含 plan / build / verify / commit / reflect / archive） | 与 Level 2 标准路径一致 |
| V3 | 复杂度 | **Level 2** | 5 文件修改 / 需求清晰 / 无新组件 / 无设计决策 |
| V4 | 创意需求 | **❌ 否** | 4 项 P3 全部既有功能补强 |
| V5 | 安全标注 | **❌ 否** | 4 项 P3 与安全边界无关 |

#### 4 项 P3 清单（详细子任务定义）

| # | ID | 标题 | 文件位置 | 估时 plan ×0.6 | 闭环来源 |
|:-:|---|---|---|:-:|---|
| 1 | P1 | hello_devtool_perf_smoke 多帧验证（autoquit ms 调整 + PASS regex 增强 N≥3）| `tests/CMakeLists.txt` | ~30-60 min | TASK-20260502-02 P3 #3 |
| 2 | P2 | 三件套 dogfood 路径装裱（注释一致化 + 示例块标准化）| `tests/CMakeLists.txt` + `examples/hello_devtool.cc` | ~15-30 min | DevTool 三件套主线收官语义需求 |
| 3 | P3 | GoogleTest 三元守卫 audit + fix（**audit 已预跑 5 命中 / 仅 2 处实际反模式**）| `tests/integration/devtool_dogfood_smoke_test.cc:106` + `tests/devtool/hot_reload/file_watcher_test.cc:314` | ~10 audit + ~20-30 fix | TASK-20260503-02 reflection §6 P2 #3 |
| 4 | P4 | DevTool README 章节补强（root README 当前 2 行，需 +5-10 行 DevTool 引入 + 交叉链接）| `README.md` | ~15-20 min | DevTool 三件套主线收官语义需求 |

**总估时**：~70-110 min plan ×0.6（落「极窄档延续高效区 0.30-0.45×」候选 / 因 audit 预跑减负预期可达 0.30× 左右）

#### 验收要点

- 5 项文件 git diff 可见 + 内容符合各 P3 描述
- DEVTOOL=ON ctest 1247 baseline **不退化**（P1 修改 PASS regex 后必须 verify hello_devtool_perf_smoke 多帧实测 PASS）
- DEVTOOL=OFF ctest 1082 baseline **不退化**（README + GoogleTest fix 不影响 OFF 路径）
- P3 反模式 fix 后既有相关测试继续 PASS（dogfood smoke + file_watcher_test）
- README 增加段落 markdown 渲染正确（手验链接有效）
- `activeContext.md` 待处理事项段中 4 项 P3 标 ✅ 并迁移到「长期沉淀」段

#### 前置验证（4/4 PASS）

| 维度 | 检查内容 | 结果 |
|---|---|:-:|
| 依赖可获取性 | 无新依赖 / `build/_deps` 已预置 | ✅ |
| 环境就绪 | main `5667c8c` 干净 / cmake 4.2.3 + gcc 15.2.0 + ninja + ctest / Grep 工具兜底 rg | ✅ |
| 已有 artifact | 5 文件全部已存在并已读取上下文 | ✅ |
| 待处理事项 | 直接清理 4 项 P3 — 闭环 TASK-20260502-02 §P3 + TASK-20260503-02 §P2 #3 | ✅ 极强 |

#### P3 task #3 audit 预跑结论（VAN 阶段已固化）

GoogleTest `ASSERT_TRUE(x.ok()) << x.status().message()` 模式 audit 5 命中：

| # | 文件:行 | 模式 | 评估 |
|:-:|---|---|:-:|
| 1 | `tests/platform/memory_surface_test.cc:96` | `ASSERT_TRUE(status.ok()) << status.message();` | ✅ 安全（status 已先取出）|
| 2 | `tests/integration/devtool_dogfood_smoke_test.cc:106` | `ASSERT_TRUE(json.ok()) << "...: " << json.status().message().data();` | ⚠️ **反模式 — 需 fix** |
| 3 | `tests/graphics/drawtext_shape_cache_test.cc:39,41` | `ASSERT_TRUE(...) << "string literal";` | ✅ 字面量安全 |
| 4 | `tests/devtool/hot_reload/file_watcher_test.cc:314` | `ASSERT_TRUE(resolved.ok()) << "...: " << resolved.status().message();` | ⚠️ **反模式 — 需 fix** |
| 5 | `tests/script/quickjs_engine_test.cc:18` | `ASSERT_TRUE(r.ok()) << (!r.ok() ? r.status().message() : "");` | ✅ **三元守卫范本** |

结论：实际 fix 范围 **2 处**（远低于 activeContext 假设的 8 处，audit 缩减 ~75% 工作量）；fix 范本 = `tests/script/quickjs_engine_test.cc:18` 三元守卫模式直接复制。

#### Plan 阶段决策表（B1-B9 — 用户 1 次 AskQuestion 选 `all_recommended` → 9/9 按推荐锁定）

| # | 维度 | 锁定 |
|:-:|---|---|
| B1 | 子任务执行顺序 | P3 反模式 fix → P1 ctest 配置 → P2 注释装裱 → P4 README → finalize |
| B2 | 测试模式 | P1 [覆盖补充] / P2 [文档调整模式] / P3 [覆盖补充] / P4 [文档调整模式] |
| B3 | P1 PASS regex | N≥3（`frames=([3-9]\|[1-9][0-9]+)`）+ autoquit 600ms |
| B4 | P2 装裱范围 | 仅 ctest 三段注释 + `hello_devtool.cc` 文件头 docstring |
| B5 | P4 README 篇幅 | ~50-80 行（项目简述 + 核心功能表 + 三件套段 + Build & Run + 交叉链接）|
| B6 | Phase 0 检查 | 极简 1 子段（工具链快照 + smoke 工具 + audit 预跑结论引用）|
| B7 | Checkpoint | CP1（P3 fix 完成）+ CP2（P1+P2 完成）|
| B8 | commit 粒度 | 4+1+1 = 6 commits（4 P3 + 1 finalize + 1 reflect）|
| B9 | 估时 | plan ×0.6 ~54 min → 实测 ~22-35 min（落「极窄档延续高效区 0.30-0.45×」候选续延）|

#### 推荐工作流

`/plan` ✅ → `/build`（CP1 = P3 fix 完成 + CP2 = P1+P2 完成）→ `/reflect` → `/archive`

#### 关键约束

- 4 项 P3 零功能变更（无新 API / 无新子系统 / 无新威胁面 mitigation）
- 子任务 1 commit/项保持语义清洁（应用 git-workflow.mdc 「Multi-subtask commit 拆分」推荐范式 — 来自 TASK-20260503-02 任务 4 落实）
- DEVTOOL=ON 1247 + DEVTOOL=OFF 1082 双 config baseline 不退化（P1 触发 PASS regex 增强后必须 hello_devtool_perf_smoke 多帧实测 ≥3）
- README 增加内容须聚焦 DevTool 三件套（不扩散到非主线模块）

-->

---

### TASK-20260503-02：工作流/规则类技术债批量清理（6 项 P1 — 跨任务 reflection 沉淀）

- **复杂度级别：** **Level 2**（多文件修改 / 需求清晰 / 5 项规则文档调整 + 1 项 codebase audit / 无新代码逻辑变更）
- **状态：** ✅ **已完成（已归档闭环）**（VAN ✅ + Plan ✅ + Build ✅ + Reflect ✅ + Archive ✅）
- **回顾文档：** `memory-bank/reflection/reflection-TASK-20260503-02.md`
- **归档文档：** `memory-bank/archive/archive-TASK-20260503-02.md`
- **闭环成果：** 7 项跨任务上游 P1 全部清零 + 4 项本任务改进建议 100% 落实 + 4 大新协议首次实证沉淀到 systemPatterns + 0/7 反复模式（第三次连续零反复）+ 0.21× 总比值创历史新低
- **创建日期：** 2026-05-03
- **分支：** `feature/TASK-20260503-02-techdebt-workflow-cleanup`（基于 main `55dea8f` ✅ 已创建并切换）
- **设计 spec：** ❌ 不需（纯文档 / 规则调整任务，直接以 plan 替代 spec）
- **实现 plan：** ✅ `docs/plans/2026-05-03-techdebt-workflow-cleanup.md`（~480 行 / 6 子任务 [文档调整模式] / Phase 0 极简 1 子段含 audit 预跑 / B1-B8 8/8 锁定 / CP1+CP2 / 9 systemPatterns 协同度自我对照）
- **创意文档：** ❌ 否（纯文档调整无设计空白）
- **需要创意阶段：** ❌ 否
- **来源：** 用户 2026-05-03 通过 `/van 清理技术债` 显式启动；用户 AskQuestion 选 A（工作流/规则类批量清理）— 6 项 P1 全部来自 `activeContext.md` 待处理事项段
- **安全相关：** ❌ 否（纯文档/规则清理，无代码逻辑变更，无新外部输入处理）

#### 任务范围（V1-V5 默认锁定）

| # | 维度 | 选择 | 理由 |
|:-:|---|---|---|
| V1 | 子任务范围 | **6 子任务**（C-#1 / C-#2 / C-#4 / A-P1#4 / A-P1#6 / A-P1#8） | 用户 AskQuestion 选 A 锁定 6 项 P1 全清理 |
| V2 | 实施模式 | **b 完整实施**（含 plan / build / verify / commit / reflect / archive） | 与 Level 2 标准路径一致 |
| V3 | 复杂度 | **Level 2** | 6 文件修改 / 需求清晰 / 无新组件 / 无设计决策 |
| V4 | 创意需求 | **❌ 否** | 纯文档清理无设计空白 |
| V5 | 安全标注 | **❌ 否** | 纯规则文档调整 |

#### 6 项 P1 清单（详细子任务定义）

| # | ID | 标题 | 文件位置 | 估时 plan ×0.6 | 反复模式 |
|:-:|---|---|---|:-:|:-:|
| 1 | C-#1 | writing-plans skill「§API 设计」补 testability 子段 | `.cursor/rules/skills/writing-plans.mdc` | ~10-15 min | — |
| 2 | C-#2 | writing-plans skill「§验收要点」补 ctest config 矩阵明示 | `.cursor/rules/skills/writing-plans.mdc` | ~10 min | ⚠️ 与 502-02 P1 #2 同类反复（升 P1） |
| 3 | C-#4 | writing-plans skill Phase 0 §0.10 补 toolchain 版本激进升级检查 | `.cursor/rules/skills/writing-plans.mdc` | ~10 min | — |
| 4 | A-P1#4 | git-workflow.mdc 补「Multi-subtask commit 拆分」`git add -p` 段 | `.cursor/rules/skills/git-workflow.mdc` | ~10-15 min | — |
| 5 | A-P1#6 | StatusOr<T>::status() 三元守卫 codebase audit | 全 codebase rg 验证 + 必要时 fix | ~30-60 min | — |
| 6 | A-P1#8 | spec template「A14 解读」附录段 | `docs/specs/2026-04-30-devtool-design.md` | ~15-20 min | — |

**总估时**：~85-130 min plan ×0.6（不含 clang-tidy enforce 路径，留 P3）

#### 验收要点

- 5 项规则/spec 文档调整 git diff 可见 + 内容符合各 P1 描述
- A-P1#6 audit 结果文档化（如有 fix 需 ctest 验证 + commit 单独记录）
- DEVTOOL=ON ctest 1247 baseline 不退化（如 A-P1#6 触发 codebase fix 必须 verify）
- `activeContext.md` 待处理事项段中已落实项标 ✅ 并迁移到「长期沉淀」段

#### 前置验证（4/4 PASS）

| 维度 | 检查内容 | 结果 |
|---|---|:-:|
| 依赖可获取性 | 无新依赖 | ✅ |
| 环境就绪 | main 干净 / GTest + ctest / `rg` audit 工具 | ✅ |
| 已有 artifact | 4 skill 文件 + 1 spec + audit 范围 | ✅ |
| 待处理事项 | 直接清理 6 项 P1 | ✅ 极强 |

#### Checkpoint（B6 Plan 阶段确认）

- **CP1**：任务 1-3 完成后（writing-plans 3 段同文件 batch）→ 自审段落语义连贯 / 无过度工程 / 标题层级 / 交叉引用真实
- **CP2**：任务 5 完成后（audit 文档化）→ 自审 audit 范围完整性 / 命令兜底正确 / P3 候选迁移

#### Plan 阶段决策表（B1-B8 — 用户 1 次 AskQuestion 选 all_recommended → 8/8 按推荐锁定）

| # | 维度 | 锁定 |
|:-:|---|---|
| B1 | writing-plans 3 项 commit 粒度 | 3 commits（每项独立，自我吃 A-P1#4 狗粮）|
| B2 | A-P1#6 audit 输出 | audit only + 0 fix（Phase 0 §0.2 预跑 6/6 ✅）|
| B3 | 子任务执行顺序 | 按文件分组（writing-plans 3 → git-workflow → audit → spec）|
| B4 | 测试模式 | [文档调整模式] 新模式（无 TDD，git diff + ctest 不退化 verify）|
| B5 | Phase 0 检查 | 极简 1 子段（含 audit 预跑 + 工具链快照 C-#4 落实首版）|
| B6 | Checkpoint | CP1（任务 1-3 后）+ CP2（任务 5 后）|
| B7 | plan 估时 | ~85-100 min plan ×0.6 → 实测 ~40-65 min 期待（极窄档延续 0.40-0.50×）|
| B8 | 反复模式记录 | C-#2 标注「第二次同类反复」+ reflect 阶段统计反复抑制有效性 |

#### Phase 0 §0.2 audit 预跑结论（plan 阶段已固化）

全 codebase `.status()` 调用 6 处（5 文件 5 上下文）：

| # | 文件:行 | 守卫模式 | 评估 |
|:-:|---|---|:-:|
| 1 | `application.cc:113` | `if (!result.ok()) return result.status();` | ✅ |
| 2 | `application.cc:135` | 同上 | ✅ |
| 3 | `application.cc:348` | `eval.ok() ? Status::Ok() : eval.status();` | ✅ 三元守卫范本 |
| 4 | `image_cache.cc:16` | `if (!result.ok()) { return result.status(); }` | ✅ |
| 5 | `file_watcher_inotify.cc:239+242` | `if (!resolved.ok()) { ... .status().code()/message() ... }` | ✅ 守卫块内合法 |

结论：6/6 ✅ 全部正确，0 fix 必要 → 任务 5 仅文档化 + clang-tidy enforce 留 P3。

#### 推荐工作流

`/plan` → `/build`（含 CP1 + CP2 自审）→ `/reflect` → `/archive`

#### 关键约束

- 纯文档/规则任务零代码逻辑变更（除 A-P1#6 audit 触发的可能 fix）
- 子任务必须 1 commit/项保持语义清洁（应用 A-P1#4 自身的 `git add -p` 推荐范式）
- 反复模式 C-#2 必须在 plan 阶段标注「第二次反复 → 必须落实」
- A-P1#6 如全 codebase audit 零误用 → 直接关闭（无 fix 需求）

---

### TASK-20260503-01：DevTool Phase C — Hot Reload 实施（Linux inotify + CSS-only 增量重载）[安全相关]

- **复杂度级别：** **Level 3**（中等功能 — 蓝图 plan §Phase C 直接锁定 Level 3；新增子系统 `veloxa/devtool/hot_reload/`：FileWatcher 抽象基类 + InotifyFileWatcher Linux 实现（~150-200 LOC + std::thread watch loop）+ HotReloadManager CSS-only 增量重载 + 1 公开 C API + 1 example 扩展 + 8+ 安全单测；vs Phase A escalate 是因为 dogfood UI 子系统级 — 本任务**无 dogfood UI 新搭** + **无 JS native binding 设计** + **无新 example 子系统**（hello_devtool 第三次扩展 = 已成熟范式）+ **无新创意决策**（creative #2 5 决策已锁定）— Level 3 锁定不 escalate）
- **状态：** ✅ **已完成（已归档闭环 2026-05-03 ~17:05）** — main `6deb5a6` fast-forward merge 完成 + feature 分支已删除 — **DevTool 三件套主线收官（Phase A → B → C 完整闭环）**。归档文档 `memory-bank/archive/archive-TASK-20260503-01.md`（Level 3 详细 12 段 ~440 行）+ 回顾文档 `memory-bank/reflection/reflection-TASK-20260503-01.md`（Level 3 详细 11 段 ~720 行）。Build ~104 min（plan ×0.6 333 min → 比值 0.31× 触发「极窄档加速衰减区」候选新子档下沿）；DEVTOOL=ON 1247 / DEVTOOL=OFF 1082 / SDL2=ON 1265 三路径双绿 verify ✅；Phase 0 投入定律 triple-evidence 升级（A 5.3× / B 5.2× / **C 7.6× ROI**）；5 大可复用范式 100% 命中且加深 + lazy-attach C ABI 模式扩展（新增 VX_WARNING_HOT_RELOAD_FAILED warning 语义层）；T2 8 步守卫 dual-probe 16 测全覆盖；2 公开 C API 新增 + 1 新子系统（vx::devtool::hot_reload::{FileWatcher, InotifyFileWatcher, HotReloadManager}）；A14 黑名单 +3 项。**改进建议落实状态**：P0 0 项（创纪录第二次连续）/ P1 4 项（C-#3/C-#4 reflect 阶段已直接落实到 systemPatterns / C-#1/C-#2 待下次任务前由 build-phase 触发）/ P2 4/4 ✅。**反复模式：1/7 部分命中**（前置组件能力假设未实测 — CssParser 严格性 + 工具链 binutils 2.46+ 行为变化）。**额外事件**：build §0.1 baseline 阻塞 binutils 2.46+ hotfix 已 fast-forward main `ddc1e3c` 零业务范围污染（首次实证「baseline 阻塞 hotfix 分离协议」+ R12「工具链版本激进升级」风险登记）。
- **创建日期：** 2026-05-03
- **分支：** `feature/TASK-20260503-01-devtool-hot-reload`（基于 main `ddc1e3c` rebase 后，含 TASK-20260502-01 Phase A + TASK-20260502-02 Phase B 全归档 + 5 大可复用架构范式 + lazy-attach C ABI 容错模式 + binutils 2.46+ link group 兼容修复）
- **设计 spec：** ✅ 复用 `docs/specs/2026-04-30-devtool-design.md` Hot Reload 验收段（A10-A12 + A13-A14 + T2 路径穿越威胁高 + T8 mutation propagation 中威胁 + I7/I8 注入点 + R4/R6 风险 + 强依赖 R3+ #3 F-025 LoadHTML use-after-free）
- **实现 plan：** ⏳ **本任务专属 build 级 plan** `docs/plans/2026-05-03-devtool-hot-reload.md`（待 `/plan` 阶段产出，预估 ~500-600 行 / 10-12 子任务 5-步 TDD 模板 + Phase 0 实测填写 + B1-B8 决策表 + plan ×0.6 第 48+ 数据点假设 + R 风险登记 + Checkpoint）；蓝图 plan `docs/plans/2026-04-30-devtool.md` §Phase C 段（10-12 子任务粗粒度模板）作为参照基线（不修改）
- **创意文档：** ✅ 复用 `memory-bank/creative/creative-devtool-hot-reload.md`（5 决策已锁定 — file watcher 抽象层接口 / CSS-only 增量重载策略 / DOM 状态保留协议 / watcher root 边界 / CSS 解析失败错误恢复，**无新 creative 需求**）
- **需要创意阶段：** ❌ 否（TASK-30-04 蓝图阶段已产出 creative #2 全覆盖 Phase C 5 决策；纯 inotify Linux 实现 + LoadCSS path 单路径，无新架构空白）
- **来源：** TASK-20260430-04 蓝图主交付独立立项候选 §主线 3 项之 C；用户 2026-05-03 通过 `/van` AskQuestion 选 `phase_c_hot_reload` 显式启动；Phase A archive §10 / Phase B archive §11 标注 Phase C「立项条件就绪」+ 估时回填校准下调（Phase A 5 大可复用架构范式 + Phase B 进一步 ~30% 下调因 lazy-attach C ABI 模式 + dogfood 路径成熟）
- **安全相关：** ✅ 是（**T2 路径穿越高威胁** — 8 步守卫：watcher root allowlist + IN_MODIFY/IN_CLOSE_WRITE only（不监听 IN_CREATE 防 atomic+symlink 穿越） + realpath canonicalize + boundary check + 4 MiB max_size 上限 + symlink 跨 root 拒绝 + 安全日志 + reverse probe；**T8 mutation propagation 中威胁** — CSS-only 严格不触发 LoadHTML（不踩 F-025 use-after-free）；双 Document 严格独立 stylesheet）

#### 任务范围（VAN 推荐默认锁定 V1-V5）

| # | 维度 | 选择 | 理由 |
|:-:|---|---|---|
| V1 | 子系统范围 | **Phase C Hot Reload 10-12 子任务**（C.0.1 FileWatcher 抽象 + C.1.1-C.1.3 InotifyFileWatcher 实现/反向探针/8 项测试 + C.2.1-C.2.3 HotReloadManager 基础/CSS 重载/状态保留 + C.4.1-C.4.2 C API + example smoke + C.5.1-C.5.2 T2 安全测/reflect）| TASK-30-04 plan §Phase C 直接复用，无新增子任务 |
| V2 | 实施模式 | **b 完整实施**（含 build / ctest / commit / merge） | 与 Phase A/B 模式一致；Level 3 build 主路径 |
| V3 | 复杂度 | **Level 3** | 跨 4 子系统（devtool-hot_reload/api/examples/tests）但 plan 已明确；不需新架构决策；无 dogfood 子系统级风险 |
| V4 | 创意需求 | **❌ 否** | creative #2 5 决策已覆盖 Hot Reload 设计；无新设计空白 |
| V5 | 安全标注 | ✅ **是** | T2 路径穿越（高，8 步守卫）+ T8 mutation propagation（中，CSS-only 不踩 F-025）2 威胁面全程守门 |

#### VAN 阶段实证（F1-F8 — 基础设施成熟度三色，5 ✅ / 1 ⚠️ / 2 🔴）

| # | 命题 | grep 实证 | 影响蓝图 |
|:-:|---|---|:-:|
| F1 | I7 inotify watcher 基础（platform/ 当前无 file watcher）| 🔴 **完全不存在** — 全代码库无 `inotify_init` / `IN_MODIFY` / `sys/inotify.h` 调用；蓝图 spec §6 I7 标记「需新建 `veloxa/devtool/hot_reload/file_watcher_inotify.{h,cc}`」| 🔴 C.0.1 + C.1.1 必做（~150-200 LOC 自实现 + std::thread watch loop） |
| F2 | I8 Hot Reload restyle 入口（Application::LoadCSS 已存在）| ✅ 已就绪 — `Application::LoadCSS()` 在 application.cc 已存在并经 R3+ codebase review 验证（不像 LoadHTML 有 dom_bindings_ use-after-free 风险）；HotReloadManager::OnFileChanged 仅调 LoadCSS 即可触发全 restyle | 🟢 C.2.1-C.2.2 极简 — 单路径调用 |
| F3 | F-025 LoadHTML use-after-free 是否已修复（强依赖判断）| ⚠️ **仍未修复**（R3+ #3 标 P1 但未独立立项）| 🟢 一期 CSS-only 严格不踩 — 显式契约：HotReloadManager 仅识别 .css 文件 + 仅调 LoadCSS（不调 LoadHTML），plan 必须设计反向探针单测验证此契约 |
| F4 | C API 双层模式（lazy-attach 兼容）| ✅ Phase B B.0.1 实证 lazy-attach C ABI 容错模式 — `vx_view_set_pipeline_hooks` 即使 update_manager_ 未初始化也返 INVALID_STATE 提示 + cache hooks → EnsureUpdateManager 时激活 | 🟢 C.4.1 `vx_view_attach_devtool` 加 hot_reload_dir 参数透传 + 同款 lazy-attach 范式（hot_reload_dir cache → first LoadCSS 时启动 watcher）|
| F5 | std::thread + std::condition_variable + std::atomic 已有用例 | ⚠️ 需 grep 确认 — `Threads::Threads` CMake link 在 vx_devtool 当前未声明（蓝图 plan §3 标 PRIVATE Threads::Threads 需新增）；已有 vx_image / vx_layout 是否用 std::thread 待 plan 阶段实测 | 🟡 C.1.1 需 CMake 加 `target_link_libraries(vx_devtool PRIVATE Threads::Threads)`（一行 patch） |
| F6 | inotify 系统头可用性（Linux libc）| ✅ Linux 平台 100% — `<sys/inotify.h>` / `<unistd.h>` / `<fcntl.h>` 是 libc 系统头，无 CMake link 需求；macOS/Windows 不支持 → 一期 D5=A 锁定 Linux only，跨平台留 P2 候选 | 🟢 C.1.1 直接 #include 即可 |
| F7 | T2 8 步守卫的 realpath / canonicalize 工具 | ⚠️ 全代码库无 `realpath()` / `std::filesystem::canonical()` 调用 — 蓝图 plan §3 推荐用 `realpath(path, nullptr)` (POSIX) 或 `std::filesystem::canonical()` (C++17) | 🟡 C.1.2-C.1.3 + C.5.1 T2 守卫实施需新引入；plan 阶段决策 POSIX vs std::filesystem（lib 体积/异常 trade-off） |
| F8 | examples/hello_devtool.cc 现状（example 第三次扩展）| ✅ Phase A.3.1 + Phase B.3.2 已成熟 example 范式 — 包含 EnsureUpdateManager + auto-quit + PASS_REGULAR_EXPRESSION ctest smoke；C.4.2 仅需追加 hot_reload_dir 参数 + 修改临时 CSS 文件 + 验证 restyle 触发 | 🟢 C.4.2 极简 — example 第三次扩展，dogfood 路径已成熟 |

**汇总：** **5 ✅ 已就绪 / 2 ⚠️ 部分（F5 std::thread / F7 realpath，需 plan 阶段决策）/ 1 🔴 需新建（F1 inotify watcher 子系统）— 比 Phase B 启动前 5/1/1 略多新建因子（inotify 子系统 + std::thread + realpath 三点新引入）**

**关键发现（Phase A/B 后续 ROI 验证）：**

- **F2 LoadCSS 已就绪 + F4 lazy-attach C ABI 模式可复用** = 比 Phase B archive §11 估时进一步下调到 ~2-3 h plan ×0.6 是合理的（实际 plan 阶段需做 Phase 0 实测才能锁定）
- **F8 example 第三次扩展 + dogfood 路径成熟** = C.4.2 是「极窄档延续」候选（plan 30 min ×0.6 = 18 min 可能进一步压缩）
- **F1 inotify 是新子系统但量级有限**（~150-200 LOC 自实现 + 业界范式成熟）= 不构成 escalate 触发条件
- **F3 F-025 不踩契约清晰** = 一期 CSS-only 严格边界 + 反向探针单测验证（C.2.x「不调 LoadHTML」契约）= 安全守门强

#### 触及技术债映射（与 `techContext.md` 对照）

| # | 技术债 | 子任务 | 闭环节奏 |
|:-:|---|---|---|
| #4 | ImageCache 命名空间隔离（DevTool icon vs target image）| ⊘ Phase C 不需 icon → 不闭环 | 留 P3（Phase D 完整 UI Editor 才会用 icon）|
| #35 阶段 2 | LayoutEngine 内 style/layout 子阶段拆分 | ⊘ 不在本任务范围 | 留 P3（独立立项，Phase B 已闭环阶段 1） |
| F-025 | LoadHTML use-after-free | ⊘ **强依赖但本任务不踩**（CSS-only 严格契约）| 留 R3+ #3 独立立项（如未来扩展段加 HTML 增量重载，必须前置闭环）|

#### 验收要点（Phase C 主交付，A10-A12 + A13 + A14）

- A10 修改 watcher root 内 .css 文件 → 目标 View 自动 restyle 不重启（手工 + inotify 事件 → restyle 触发单测）
- A11 watcher root 安全：路径穿越拒绝（`../../etc/passwd`）+ symlink 跨 root 拒绝 + 安全日志（T2 单测 8 步守卫验证）
- A12 4 MiB CSS 文件大小上限 + 超限拒绝 + 错误日志（max_size 守卫单测，沿用 R2.5 image_decoder max_size 模式）
- A13 现有 ctest 全绿（DevTool OFF 时零回归）— ctest 1228 ON / 1082 OFF baseline 维持
- A14 DevTool 关闭时构建产物零变化（链接闭合 + binary size diff = 0）— 复用 TASK-20260502-01 A.2.4 + Phase B B.3.3 自动化 ctest smoke（`tests/smoke/devtool_a14_link_closure.cmake` 加 `FileWatcher` / `InotifyFileWatcher` / `HotReloadManager` 内部符号到黑名单）

#### Phase C 子任务清单（蓝图 plan §Phase C 10 子任务，待 `/plan` 阶段精化）

| 子任务 | 描述 | 估时 plan ×0.6（蓝图）|
|:-:|---|---:|
| C.0.1 | FileWatcher 抽象基类 + Platform factory（pure virtual + nullptr 退化） | 18 min |
| C.1.1 | InotifyFileWatcher 基础实现（init + add_watch + watch loop std::thread）| 54 min |
| C.1.2 | InotifyFileWatcher T2 路径守卫 8 步（allowlist + realpath + boundary + max_size + symlink 拒绝）| 36 min |
| C.1.3 | InotifyFileWatcher 完整测试集（8 项 — modify / atomic write / symlink escape / max_size / extension filter / debounce / restart / kInvalidArgument 反向探针）| 45 min |
| C.2.1 | HotReloadManager 基础（path_to_stylesheet_id_ + OnFileChanged） | 36 min |
| C.2.2 | HotReloadManager LoadCSS 调用 + restyle 触发 + dom_bindings_ 不重建（CSS-only 契约 — F-025 不踩反向探针单测） | 27 min |
| C.2.3 | HotReloadManager DOM 状态保留协议（hover_target / focus_target / scroll_position snapshot + restyle 后恢复 — creative #2 决策 3）| 27 min |
| C.4.1 | `vx_view_attach_devtool` 加 hot_reload_dir 参数透传（lazy-attach C ABI 模式复用 Phase B B.0.1）| 18 min |
| C.4.2 | examples/hello_devtool.cc — Hot Reload smoke（修改 .css 实时观察 restyle 触发，第三次 example 扩展） | 27 min |
| C.5.1 | T2 完整安全单测（路径穿越 / canonicalize / boundary 8 测，独立 security_test.cc）| 27 min |
| C.5.2 | Phase C reflect prep + finalize commit + A14 黑名单更新（FileWatcher / InotifyFileWatcher / HotReloadManager 3 项）| 18 min |
| **合计** | **11 子任务**（plan 阶段可能进一步合并/拆分 — 蓝图原写 10 子任务，本 VAN 拆 C.2 三段更细） | **333 min（5.55 h，蓝图）** |

**估时回填校准（基于 TASK-20260502-01 + TASK-20260502-02 实证）：** 蓝图 5.55 h plan ×0.6 → Phase A archive §10 调整为 ~2.5-4 h（基于 5 大可复用范式）→ Phase B archive §11 进一步下调 ~30%（因 lazy-attach C ABI 模式 + dogfood 路径成熟）→ **plan 阶段假设 ~2-3 h plan ×0.6**（11 子任务 vs Phase B 10 子任务略多 + 5/2/1 已就绪与 Phase B 5/1/1 接近 + 5 大可复用架构范式 100% 命中预期 + lazy-attach C ABI 模式 + dogfood 路径第三次复用，落「极窄档延续高效区 0.30-0.45×」候选新子档 — Phase B 同档实证 0.40×）。

#### 前置验证清单（VAN 阶段产出）

| 维度 | 结果 | 备注 |
|---|:-:|---|
| 依赖可获取性 | ✅ | 零新 FetchContent；Linux inotify 是 libc 系统调用 + std::thread / std::filesystem (C++17) 标准库 |
| 环境就绪 | ⚠️ 需 reconfigure | `build/` + `build-noffi/` 不存在（Phase B 后已清理）→ plan §0.1 必跑全新 cmake configure + ctest 二次验证 |
| 已有 artifact | ✅ | spec + plan §Phase C + creative #2 全部就绪（TASK-30-04 蓝图主交付）；可直接进 build phase 精化 |
| ctest 基线 | ✅ | 1228 ON / 1082 OFF PASS（main `c0c4cbd` 终态继承自 TASK-20260502-02 Phase B 完成快照），plan 阶段 reconfigure 后实证 |
| FetchContent 代理守卫 | ⊘ 跳过 | 零新依赖（inotify 是 libc，std::thread/filesystem 是 stdlib） |
| 待处理事项关联 | ✅ 极强 | DevTool 三件套主线收官（A → B → C 完整闭环）；与 Phase A/B 沉淀的 5 大架构范式 + lazy-attach C ABI 模式直接用；与 R3+ #3 F-025 LoadHTML use-after-free 强依赖（一期 CSS-only 严格不踩，反向探针单测验证）；与 R2 P3 三连 / R9 EventManager HitTest / #35 阶段 2 无相互依赖 |

#### VAN 阶段 push-back 决策（已沉淀）

| 风险 | 应对 |
|---|---|
| 「F-025 LoadHTML use-after-free 一期 CSS-only 边界守不住」陷阱 | C.2.x 实施必须设计**反向探针单测**：HotReloadManager 仅识别 `.css` 后缀文件 + 仅调 LoadCSS（断言不调 LoadHTML）+ 删除该 if-branch → 反向探针测 FAIL；spec §F-025 已记录强依赖契约 |
| 「inotify watch loop 在独立 std::thread 但事件投递到主线程 race condition」陷阱 | C.1.1 plan 阶段必须设计：std::condition_variable + queue 跨线程消息传递 + 主线程 OnFrame 时 drain queue（与 Phase B B.0.1 PipelineHooks 五钩子主线程范式一致）；T6 类比 callback budget 1ms guard |
| 「T2 路径穿越 8 步守卫漏一步导致 CVE 级风险」陷阱 | C.5.1 独立 `security_test.cc` 必须 8 步全测 + 反向探针（删 `realpath` → SymlinkEscape FAIL）；creative #2 决策 4 watcher root 边界 + spec §7 T2 mitigation 8 步逐项对应单测 |
| 「跨平台抽象一期仅 Linux，未来 macOS/Windows 接口不兼容」陷阱（spec §9 R4）| C.0.1 抽象基类设计参考 efsw 库 ≥ 3 平台共有接口（不绑死 inotify 特性，creative #2 决策 1 callback-based + 事件归一化已锁定）；Platform factory nullptr 退化（非 Linux 平台返 nullptr，HotReloadManager 安全 no-op） |
| 「直接进 build 跳过 plan 精化」诱惑 | 拒绝 — Level 3 默认进 `/plan` 做 Phase 0 grep（callsite 全表 + 既有测试 fingerprint + CMake 链接审计 + plan ×0.6 估时校准 + reconfigure 二次验证 ctest baseline + std::thread/realpath 标准库选型决策）+ 沉淀 plan ×0.6 第 48+ 数据点 |
| 「TASK-30-04 plan 已就绪 build phase 全照搬」陷阱 | plan 阶段必须验证：(a) plan 写于 2026-04-30，main 终态后续大变（TASK-20260502-01 Phase A + TASK-20260502-02 Phase B 已合入 + 5 大可复用范式 + lazy-attach C ABI 模式）；(b) F4/F8 已就绪发现可能进一步简化 C.4.1/C.4.2；(c) plan ×0.6 第 48+ 数据点回归「极窄档延续高效区」vs「大件实现下端」可能 |
| 「DOM 状态保留协议（hover/focus/scroll）实施复杂度低估」陷阱（creative #2 决策 3）| C.2.3 单独子任务 27 min 不应低估 — 需 grep 验证 dom::Node 指针稳定性（restyle 不改变 Node tree 是 creative #2 假设，plan §0.x 必须实证）+ ScrollPosition / FocusManager / EventManager hover_target_ 三处状态 snapshot/restore API 设计 |

#### 与并发任务关系

- **TASK-20260502-01**（DevTool Phase A · Inspector）：✅ 已归档（2026-05-02 ~18:10），main `c0c4cbd` 已含 5 大可复用架构范式 + DevTool 子系统骨架 + A14 自动化守门；本任务在新 main 上独立演进，零 git 依赖
- **TASK-20260502-02**（DevTool Phase B · Performance Overlay）：✅ 已归档（2026-05-03 ~00:30），main `c0c4cbd` 已含 lazy-attach C ABI 容错模式 + Phase 0 投入定律 dual-evidence + Level 3 vs L4 子代理决策法则 + A14 黑名单维护演进透明度子段；本任务复用 lazy-attach C ABI 模式 + 5 大架构范式
- **TASK-20260430-04**（DevTool 蓝图）：✅ 已归档，spec §Phase C + plan §Phase C + creative #2 全部就绪可直接复用
- **TASK-20260430-03**（codebase review）：**强依赖 R3+ #3 F-025 LoadHTML use-after-free**（一期 CSS-only 严格不踩，反向探针单测验证；如未来扩展 HTML 增量重载必须前置闭环）；R3+ #2 EventDispatcher snapshot iteration 与本任务**无关**（HotReloadManager 不调 EventDispatcher）

#### 任务历史

| 时间 | 阶段 | 备注 |
|---|---|---|
| 2026-05-03 13:15 | 用户启动 | `/van` AskQuestion 选 `phase_c_hot_reload`（DevTool 三件套主线收官） |
| 2026-05-03 13:20 | VAN 完成 | 工作区干净 ✅；F1-F8 grep 实证（5 ✅ / 2 ⚠️（std::thread + realpath 选型）/ 1 🔴（inotify 子系统必做））— 比 Phase B 启动时 5/1/1 略多新建因子但仍 Level 3 量级；F4 lazy-attach C ABI 模式 + F8 dogfood 路径第三次复用发现 ROI 高于蓝图预期；分支 `feature/TASK-20260503-01-devtool-hot-reload` 基于 main `c0c4cbd` 创建（含 TASK-20260502-01 Phase A + TASK-20260502-02 Phase B 全归档 + 5 大可复用架构范式 + lazy-attach C ABI 模式）；前置验证 6/6 全 PASS（ctest 1228 ON / 1082 OFF 基线隐式继承，plan §0.1 reconfigure 必跑二次验证因 build/ 已清理）；MB 三件套同步；推荐路径 `/plan` 做 build 级精化 |
| 2026-05-03 13:35 | Plan 完成 | Phase 0 13 子段实测填写完成（核心发现 3 项：§0.7 LoadCSS 路径 100% 安全 — `Application::LoadCSS` 仅 `stylesheets_.push_back` + `update_manager_.reset()` 不动 dom_bindings_/target_document_ → CSS-only 严格契约自然成立 + 反向探针单测验证；§0.12 std::thread 全代码库零既有用例 → 本任务首次引入需谨慎 thread-safe queue 设计；§0.13 realpath / std::filesystem 全代码库零既有用例 → POSIX `realpath()` + RAII 一致性优胜）；B1-B8 build 级精化决策表全部锁定（用户 1 次 AskQuestion 选 all_recommended → 8/8 按 VAN 推荐：B1=A 独立 plan / B2=A 严格串行 / B3=A 新建 tests/devtool/hot_reload/ 子树 / B4=A POSIX realpath + unique_ptr RAII / B5=A 合并 C.2.2（YAGNI — §0.7 grep 实证 hover/focus/scroll 当前不持久化 → 节省 ~27 min plan）/ B6=A 每子任务 1 commit / B7=A ~2-3 h plan ×0.6 假设 / B8=A 复用 spec+creative）；plan `docs/plans/2026-05-03-devtool-hot-reload.md` 落盘 ~700 行（11 子任务 5-步 TDD 模板 + 完整代码片段 + 5 个 R 风险登记 R4/R6/R7 race condition 新增/R8 T2 8 步漏 CVE 新增/R9 F-025 不踩契约新增 + R10/R11 + 3 Checkpoint + 12 条 systemPatterns 协同度自我对照 + plan ×0.6 第 48+ 数据点假设入库）；安全分析 STRIDE 6 项 + T2 8 步守卫详细映射表（步骤 1:1 对应 §0.6 边界输入 #2-9 单测 + 反向探针 meta-test）；plan ×0.6 第 48+ 数据点假设入库（蓝图 5.55 h → 11 子任务 plan ~555 min / plan ×0.6 ~333 min → 最终预测 ~100-150 min 实测耗时即 0.30-0.45× 比值，落「极窄档延续高效区」候选新子档延续 Phase B 0.40× 实证）；推荐路径 `/build` 启动 C.0.1 FileWatcher 抽象（最小子任务 plan 30 min ×0.6 = 18 min）|

---

<details>
<summary>TASK-20260502-02：DevTool Phase B — Performance Overlay 实施（Level 3 / 2026-05-02）— ✅ 已归档（点开查看历史）</summary>

### TASK-20260502-02：DevTool Phase B — Performance Overlay 实施（FPS / 4 阶段 bars / dirty rect 边框高亮）[安全相关]

- **复杂度级别：** **Level 3**（中等功能 — Phase B 总 10 子任务跨 5 子系统 core/devtool-overlay/devtool-inspector_panel-extension/api/tests；触及 #35 UpdateManager 五钩子架构性改造但非 dogfood UI 子系统级；vs Phase A escalate 后 Level 4 — 本任务无 dogfood UI 新搭 + 无 JS native binding 设计 + 无 SDL2 真窗口新例子；HUD 是 inspector_panel 扩展非新建）
- **状态：** ✅ **已归档（2026-05-03 ~00:30）** — 归档文档 `memory-bank/archive/archive-TASK-20260502-02.md`（Level 3 详细 11 段 ~352 行，含技术方案 + 实现摘要 + 关键决策 10 项 + 安全决策 + 测试覆盖 + 经验教训 + 改进建议落实状态 11/11 + 架构影响 + 长期维护建议 + commit 链 + 后续任务依赖估时调整）；reflection 产出 `memory-bank/reflection/reflection-TASK-20260502-02.md`（Level 3 详细 11 段 ~370 行）；10/10 子任务 build 全完成（11 commits / ctest ON 1228 + OFF 1082 / A14 链接闭包黑名单 +2 项闭环 / 2 安全威胁 mitigation 全到位 / 1 历史技术债 #35 阶段 1 闭环 / 3 P3 候选沉淀）；同步沉淀 systemPatterns 第 38-47 数据点 + 「极窄档延续高效区 0.30-0.45×」候选新子档 + Phase 0 ROI 定律 dual-evidence + lazy-attach C ABI 模式 + Level 3 vs L4 子代理决策法则 + A14 维护演进透明度子段 + techContext T6 边界场景测设计原则段 + productContext DevTool Phase B 能力段。**改进建议落实率：** P0 0/0（创纪录 — build 全按 plan 执行无重大偏差）+ P1 3/3 ✅（systemPatterns ×0.6 矩阵 + Phase 0 ROI 定律 + lazy-attach C ABI 模式）+ P2 8/8 ✅（含 archive 本身作为 dual-evidence 载体）；3 P3 候选已迁移到 activeContext 待处理事项（#35 阶段 2 / R9 EventManager HitTest / hello_devtool_perf_smoke 多帧）。
- **Plan 完成（2026-05-02 ~22:10，~30 min）** — Phase 0 11 子段实测填写完成（ctest baseline 二次验证 1169 ON / 1065 OFF ✅ + 五钩子 mapping 明确 5 注入点 + CSS HUD 设计可行 100%（`opacity` / `position: fixed` fallback 为 `absolute` 视觉等价 / `border-radius` 等 R2-verified）+ `UpdateManager::Update()` 单一 callsite 改造局部化 + dirty_rect 单矩形事实 → B4 决策扩展为 Vector 累积不替换 + friend pattern 引入方案明确）；B1-B8 build 级精化决策表全部锁定（用户 1 次 AskQuestion 选 all_recommended → 8/8 按推荐）；plan `docs/plans/2026-05-02-devtool-perf-overlay.md` 落盘 ~600 行（10 子任务 5-步 TDD 模板 + 完整代码片段 + 4 个 R 风险登记 + 3 Checkpoint + 9 条 systemPatterns 协同度自我对照）；plan ×0.6 第 38 数据点假设入库（蓝图 4.35 h → archive 校准 ~3.5-5 h → VAN F2 发现 ~3-4 h →最终预测 ~3-4 h plan ×0.6 即 0.55-0.75× 比值）；推荐路径 `/build` 启动 B.0.1 PipelineHooks 五钩子（最大子任务 plan 90 min ×0.6 = 54 min）
- **创建日期：** 2026-05-02
- **分支：** `feature/TASK-20260502-02-devtool-perf-overlay`（基于 main `8b2ead4`，已含 TASK-20260502-01 Phase A 全部归档 + 5 大可复用架构范式）
- **设计 spec：** ✅ 复用 `docs/specs/2026-04-30-devtool-design.md` Performance Overlay 验收段（A6-A9 + A13-A14 + T5/T6 威胁 + I2/I3/I7 注入点 + R3 风险）
- **实现 plan：** ✅ **本任务专属** `docs/plans/2026-05-02-devtool-perf-overlay.md`（~600 行 / B1-B8 决策表 + Phase 0 11 子段实测 + 10 子任务 5-步 TDD 模板 + 完整代码片段 + plan ×0.6 第 38 数据点假设 + R3/R7/R8/R9 风险登记 + 3 Checkpoint + 9 条 systemPatterns 协同度自我对照）；蓝图 plan `docs/plans/2026-04-30-devtool.md` §Phase B 段（10 子任务粗粒度模板）作为参照基线（不修改）
- **创意文档：** ✅ 复用 `memory-bank/creative/creative-devtool-screen-layout.md` 决策 3（HUD overlay 透明合成：DevTool Document 内 absolute positioned `<div>` + CSS opacity 0.85）+ 决策 5（F11 toggle HUD overlay 协议）— **无新 creative 需求**
- **需要创意阶段：** ❌ 否（TASK-30-04 蓝图阶段已产出 creative #1 决策 3/5 全覆盖 Phase B HUD 设计；纯算法层面 — FrameStats ring buffer + 滑动 60 帧聚合 + 1ms/frame budget abort 是常规嵌入式范式无新架构空白）
- **来源：** TASK-20260430-04 蓝图主交付独立立项候选 §主线 3 项之 B；用户主动通过 `/van TASK-30-04-B Performance Overlay` 显式启动；TASK-20260502-01 archive §10 / §6.3 标注 Phase B「立项条件就绪」+ 估时回填校准下调 ~30%（基于 Phase A 实证 0.64× + 5 大可复用架构范式）
- **安全相关：** ✅ 是（T6 UpdateManager 钩子 callback 任意代码执行 — 1ms/frame budget abort + 单 instance 验证；T5 DisplayList overlay 跨帧累积 — dirty rect 边框高亮复用 ResetOverlayCommands 协议）

#### 任务范围（VAN 推荐默认锁定 V1-V5）

| # | 维度 | 选择 | 理由 |
|:-:|---|---|---|
| V1 | 子系统范围 | **Phase B Performance Overlay 10 子任务**（B.0 前置改造 2 = 五钩子 + dirty rect collector + B.1 PerfOverlay 内核 2 = ring buffer + T6 budget + B.2 HUD UI 3 = HTML/CSS + JS + dirty rect 渲染 + B.3 集成 3 = F11 toggle + example smoke + reflect prep）| TASK-30-04 plan §Phase B 直接复用，无新增 |
| V2 | 实施模式 | **b 完整实施**（含 build / ctest / commit / merge） | 与 Phase A 模式一致；Level 3 build 主路径 |
| V3 | 复杂度 | **Level 3** | 跨 5 子系统但 plan 已明确；不需新架构决策；无 dogfood 子系统级风险 |
| V4 | 创意需求 | **❌ 否** | creative #1 决策 3/5 已覆盖 HUD 设计；无新设计空白 |
| V5 | 安全标注 | ✅ **是** | T6 callback budget + T5 dirty rect overlay 隔离 2 威胁面全程守门 |

#### VAN 阶段实证（F1-F9 — 基础设施成熟度三色，5 ✅ / 1 ⚠️ / 1 🔴）

| # | 命题 | grep 实证 | 影响蓝图 |
|:-:|---|---|:-:|
| F1 | I2 UpdateManager 五钩子（OnFrameStart/OnAfterStyle/OnAfterLayout/OnAfterRender/OnFrameEnd）| 🔴 **完全不存在** — `OnFrame*` / `FrameStart` / `FrameEnd` 在 `update_manager.h` 0 命中；`Application::OnFrame()` (application.h:125) 是单一 callback 而非五钩子；技术债 #35 100% 未闭环 | 🔴 B.0.1 必做 |
| F2 | dirty rect collector | ✅ **已就绪 — 比 plan 预期高 ROI** — `Renderer::ComputeDirtyRect(old, new)` (renderer.h:23) + `UpdateManager::last_dirty_rect()` (update_manager.h:39) + 缓存字段 `last_dirty_rect_` (update_manager.h:52) 已可用 | 🟢 B.0.2 极简化 — 无需新建 dirty rect collector，仅需暴露每帧 dirty rect 给 PerfOverlay + 可能扩展为 Vector<Rect> 累积 |
| F3 | UpdateManager::mutable_display_list() getter | ✅ TASK-20260502-01 A.1.6 已暴露（update_manager.h:38）+ 含 T5 reset contract 注释 | 🟢 B.2.3 dirty rect 边框高亮直接 push_back PaintCommand，与 InspectorOverlay 同模式 |
| F4 | PaintCommand kOverlayHighlight + ResetOverlayCommands | ✅ TASK-20260502-01 A.0.4 + A.1.1 落地（paint_command.h:26/82 + renderer.cc:201/236）| 🟢 B.2.3 dirty rect 边框可直接复用 OverlayHighlight 工厂（红框 stroke rect），是否新增 kOverlayDirtyRect enum 是 brainstorming 决策（plan 写新 enum，可能简化为复用）|
| F5 | SteadyTimePoint / SteadyClock 时间抽象 | ✅ `vx::css::SteadyTimePoint = std::chrono::steady_clock::time_point` 在 transition.h:20-21 已导出 | 🟢 B.1.1 FrameStats ring buffer 的 timestamp 字段无需新增 time abstraction |
| F6 | I7 ImageCache namespace 隔离（"devtool:" prefix）| ⚠️ **当前 vx::image::ImageCache 单 namespace 无 prefix 机制，技术债 #4 仍开** — 但 Performance Overlay 实际**不需要 icon**（HUD 是 PaintCommand stroke rect + 文字渲染，零 image 资源）→ **#4 在 Phase B 不闭环，留 P3 候选** | 🟡 蓝图 plan 写「闭环 #4」过于乐观；本任务实际范围排除 #4 |
| F7 | I8 Application::OnFrame() 单一 callback 拓扑 | ✅ application.h:125 单 OnFrame；位置在 EnsureUpdateManager / EnsureDevtoolUpdateManager / RenderDevtoolWithTranslate 之前 → B.0.1 PipelineHooks 在 UpdateManager::Update() 内部拆为五钩子调用点（callback fn pointer + nullptr branch predictor）；Application::OnFrame() 不动 | 🟢 设计选择清晰 |
| F8 | HUD overlay 设计（DevTool Document 内 absolute positioned div + CSS opacity）| ✅ creative #1 决策 3 已锁定（方案 B：`<div>` + `position: fixed; opacity: 0.85`，CSS R2-verified `position: fixed` 待 plan 阶段 grep 验证）| 🟡 plan §0.x 必须 grep 验证 `position: fixed` / `opacity` 是否在 R2-verified CSS 子集（Phase A.1.3 锁的 CSS shorthand 表）|
| F9 | F11 toggle HUD overlay 协议 | ✅ creative #1 决策 5 已锁定（F11 toggle HUD，独立于 F12 toggle splitter）；Phase A 已实现 F12 hotkey 范式可直接扩展 | 🟢 B.3.1 极简 — Application::SetDevtoolHotkey 模式扩展 +F11 hot key（map 化 hotkey table，或并列字段）|

**汇总：** **5 ✅ 已就绪 / 1 ⚠️ 部分（#4 不在本范围）/ 1 🔴 需新建（#35 五钩子）— 比 Phase A 启动前 5/2/2 更优**

**关键发现（Phase A 后续 ROI 验证）：**

- **F2 dirty rect 已就绪** = TASK-20260502-01 archive §10 估时下调 ~30% 偏保守 → 本任务可能进一步下调到 ~3-4 h plan ×0.6（vs 蓝图 4.35 h）
- **F3/F4/F5 三项均为 Phase A 副产品复用** = 5 大可复用架构范式（archive §8.4 表格）首次实证：双 UpdateManager / 双层 API / #ifdef+CMake / canvas Translate / 资源嵌入 → 全部直接用，**0 范式重新发明**

#### 触及技术债映射（与 `techContext.md` 对照）

| # | 技术债 | 子任务 | 闭环节奏 |
|:-:|---|---|---|
| #35 | UpdateManager 未暴露 frame hook | B.0.1 PipelineHooks 五钩子 | 本任务一次性闭环 |
| #4 | ImageCache 命名空间隔离（DevTool icon vs target image）| ⊘ Phase B 不需 icon → 不闭环 | 留 P3（Phase D 完整 UI Editor 才会用 icon）|
| #44 | QuickJS Debug API 未对接 DevTool（callback budget abort 同源机制）| B.1.2 T6 1ms/frame abort（部分相关 — 借鉴 Interrupt Handler 思路）| 部分参照不闭环 |

#### 验收要点（Phase B 主交付，A6-A9 + A13 + A14）

- A6 HUD 显示 FPS 数字（滑动 60 帧均值）— 60 帧累计单测 + dogfood 真窗口手工验证
- A7 HUD 显示 4 阶段时长 bars（style / layout / render / paint）— UpdateManager 五钩子触发顺序单测
- A8 dirty rect 边框高亮（每帧标记重绘区域）— dirty rect collector 单测 + DisplayList 注入单测
- A9 UpdateManager 钩子 callback 1ms/frame 执行预算超时 abort frame — 注入慢 callback → 断言 abort + 错误日志
- A13 现有 ctest 全绿（DevTool OFF 时零回归）— ctest 1169 ON / 1065 OFF baseline 维持
- A14 DevTool 关闭时构建产物零变化（链接闭合 + binary size diff = 0）— 复用 TASK-20260502-01 A.2.4 自动化 ctest smoke（`tests/smoke/devtool_a14_link_closure.cmake` 加 PerfOverlay 内部符号到黑名单）

#### Phase B 子任务清单（蓝图 plan §Phase B 10 子任务，待 `/plan` 阶段精化）

| 子任务 | 描述 | 估时 plan ×0.6（蓝图）|
|:-:|---|---:|
| B.0.1 | I2 UpdateManager PipelineHooks 五钩子 + `vx_view_set_pipeline_hooks` C API（**#35 闭环**） | 54 min |
| B.0.2 | dirty rect collector 扩展（每帧 Vector<Rect> 累积，复用既有 ComputeDirtyRect + last_dirty_rect_）| 36 min |
| B.1.1 | PerfOverlay FrameStats ring buffer + 滑动 60 帧聚合 | 36 min |
| B.1.2 | T6 callback 1ms/frame budget abort + 单 instance 验证 | 27 min |
| B.2.1 | DevTool Document HUD overlay HTML/CSS（absolute positioned + opacity 0.85，creative #1 决策 3）| 27 min |
| B.2.2 | HUD JS 每帧读 perf_stats + 更新 DOM | 18 min |
| B.2.3 | dirty rect 边框高亮渲染（PaintCommand kOverlayDirtyRect 或复用 kOverlayHighlight，brainstorming 决策）| 18 min |
| B.3.1 | F11 toggle HUD（仅 DevTool 已开启时）— 复用 Phase A F12 hotkey 范式 | 9 min |
| B.3.2 | examples/hello_devtool.cc — Overlay smoke 扩展 | 18 min |
| B.3.3 | Phase B reflect + commit | 18 min |
| **合计** | **10 子任务**（plan 阶段可能进一步合并/拆分） | **261 min（4.35 h，蓝图）** |

**估时回填校准（基于 TASK-20260502-01 实证）：** 蓝图 4.35 h plan ×0.6 → archive §10 表格调整为 ~3.5-5 h；本次 VAN F2 dirty rect 已就绪进一步发现 → **plan 阶段假设 ~3-4 h plan ×0.6**（Phase A 总系数 0.64× + Phase B 10 子任务 vs Phase A 16 子任务 + 5 ✅ 已就绪 vs Phase A 5 ✅ + 5 大可复用架构范式直接用）。

#### 前置验证清单（VAN 阶段产出）

| 维度 | 结果 | 备注 |
|---|:-:|---|
| 依赖可获取性 | ✅ | 零新 FetchContent；用 std::chrono / vx::css::SteadyTimePoint + 既有 stdlib |
| 环境就绪 | ✅ | `build/` + `build-noffi/` 已存在但 mtime 5 月 2 日 15:48（Phase A 中段过期）→ plan §0.1 必跑 reconfigure 二次验证 |
| 已有 artifact | ✅ | spec + plan + creative #1 全部就绪（TASK-30-04 蓝图主交付）；可直接进 build phase 精化 |
| ctest 基线 | ✅ | 1169 ON / 1065 OFF PASS（main `8b2ead4` 终态继承自 TASK-20260502-01 Phase A 完成快照），plan 阶段 reconfigure 后实证 |
| FetchContent 代理守卫 | ⊘ 跳过 | 零新依赖 |
| 待处理事项关联 | ✅ 极强 | 闭环 1 项历史技术债（#35 UpdateManager frame hook）；与 TASK-20260502-01 沉淀的 5 大架构范式直接用；与 R2 P3 三连无相互依赖（panel JS 已防御）|

#### VAN 阶段 push-back 决策（已沉淀）

| 风险 | 应对 |
|---|---|
| 「#35 五钩子重构影响目标 View 性能」陷阱（spec §9 R3）| B.0.1 必用 `function pointer + nullptr branch predictor` 优化（编译器优化分支预测，hooks 全 nullptr 时 ~0 开销）+ 单测 verify nullptr 路径快路径性能不退化 |
| 「Phase B 巨大规模」陷阱（看蓝图 10 子任务可能误判 Level 4）| 实际 Phase B 无 dogfood UI 子系统级风险（HUD 是 inspector_panel 扩展非新建）+ 无 JS native binding 设计 + 无 SDL2 真窗口新例子（hello_devtool 已存在仅扩展）→ Level 3 锁定，不需要 escalate |
| 「ImageCache namespace #4 蓝图说要闭环但实际不需要」陷阱 | F6 grep 实证 Phase B HUD 是 PaintCommand 自渲染零 image 资源 → #4 不在本任务范围，留 P3（Phase D 完整 UI Editor 才需要） |
| 「直接进 build 跳过 plan 精化」诱惑 | 拒绝 — Level 3 默认进 `/plan` 做 Phase 0 grep（callsite 全表 + 既有测试 fingerprint + CMake 链接审计 + plan ×0.6 估时校准 + reconfigure 二次验证 ctest baseline）+ 沉淀 plan ×0.6 第 38 数据点 |
| 「TASK-30-04 plan 已就绪 build phase 全照搬」陷阱 | plan 阶段必须验证：(a) plan 写于 2026-04-30，main 终态后续大变（TASK-20260502-01 Phase A 已合入）；(b) F2 dirty rect 已就绪发现可能进一步简化 B.0.2；(c) plan ×0.6 第 38 数据点回归「大件实现」桶 0.6-1.1× vs 「极窄档延续」可能 |
| 「dogfood R2 三连缺陷阻塞 HUD JS」陷阱 | F8 plan §0.x 必 grep 验证 `position: fixed` / `opacity` 是否在 R2-verified CSS 子集；HUD JS 复用 inspector_panel.js panel-side defensive 模式（已沉淀 SOP）|

#### 与并发任务关系

- **TASK-20260502-01**（DevTool Phase A · Inspector）：✅ 已归档（2026-05-02 ~18:10），main `8b2ead4` 已含 5 大可复用架构范式 + DevTool 子系统骨架 + A14 自动化守门；本任务在新 main 上独立演进，零 git 依赖
- **TASK-20260430-04**（DevTool 蓝图）：✅ 已归档，spec §Phase B + plan §Phase B 全部就绪可直接复用
- **TASK-20260430-03**（codebase review）：R3+ #2 EventDispatcher snapshot iteration / #3 LoadHTML use-after-free 与本任务**间接相关**（B.0.1 五钩子如果 callback mutate listener 可能触发 F-046，需在 plan 阶段评估）
- **DomBindings R2 P3 候选**（来自 Phase A.1.8 暴露 3 缺陷）：✅ 不影响 — HUD 不依赖 `Element.children` / `addEventListener` / `innerHTML setter`，HUD JS 可仿照 inspector_panel.js 的 panel-side defensive 模式

#### 任务历史

| 时间 | 阶段 | 备注 |
|---|---|---|
| 2026-05-02 21:30 | 用户启动 | `/van TASK-30-04-B Performance Overlay`（直接显式 Phase B 立项） |
| 2026-05-02 21:35 | VAN 完成 | 工作区干净 ✅；F1-F9 grep 实证（5 ✅ / 1 ⚠️（#4 不闭环）/ 1 🔴（#35 必做））— 比 Phase A 启动时 5/2/2 更优；F2 dirty rect 已就绪发现 ROI 高于蓝图预期；分支 `feature/TASK-20260502-02-devtool-perf-overlay` 基于 main `8b2ead4` 创建（含 TASK-20260502-01 Phase A 全归档 + 5 大可复用架构范式）；前置验证 6/6 全 PASS（ctest 1169 ON / 1065 OFF 基线隐式继承，plan §0.1 reconfigure 必跑二次验证）；MB 三件套同步；推荐路径 `/plan` 做 build 级精化 |
| 2026-05-03 00:10 | Reflect 完成 | reflection 文档 `memory-bank/reflection/reflection-TASK-20260502-02.md` 落盘（Level 3 详细 11 段 ~370 行）；核心发现 5 项：(1) plan ×0.6 0.40× 落「极窄档延续高效区」候选新子档 0.30-0.45×（VAN 预测 0.55-0.75× 偏保守 -50%；10 数据点群组入库 systemPatterns）/ (2) plan-fact reconcile 11→1 处 = -91%（证明蓝图 plan + Phase 0 实测细化双重质量保护）/ (3) 5 大可复用架构范式（Phase A 沉淀）100% 命中且加深 = 设计资产化 / (4) 0/7 反复模式命中（连续两次任务零反复）/ (5) lazy-attach C ABI 容错模式实证（B.0.1 + B.3.2 端到端 — INVALID_STATE 提示 + cache hooks + EnsureUpdateManager 激活）；改进建议 11 项（P1 ×3 + P2 ×8 — 0 P0 因 build 全部按 plan 执行无重大偏差）；Phase B vs Phase A 对照表（实测 -63% / plan-fact -91% / commits 28→11 / 反复模式持平 0/7）；安全评估 T5/T6 双 ✅；3 P3 候选入待处理事项（#35 阶段 2 拆 LayoutEngine / R9 EventManager HitTest / hello_devtool_perf_smoke 多帧）；下一步 `/archive` 归档（systemPatterns/techContext/productContext 同步更新在 archive 阶段执行）|
| 2026-05-02 23:55 | Build B.3.3 完成（**Phase B 10/10 全部完成 ✅**）| Phase B finalize + A14 smoke 黑名单更新（plan 30 min ×0.6 = 18 min / 实测 ~5 min = **0.28× plan ×0.6**，落「极窄档延续」桶高效端 — 纯文本 cmake 黑名单追加 + ctest reverify）；改 1 文件 +20 行（tests/smoke/devtool_a14_link_closure.cmake 黑名单追加 `PerfOverlay` + `InjectDirtyRectHighlights` 2 项 + Phase A/B 区分注释 + NOT in blacklist intentional 注释段说明 vx_view_set_pipeline_hooks/vx_view_get_perf_stats/vx_view_is_hud_visible 公开 ABI 不入名单 + VxViewGetPerfStats 在 vx_script anonymous namespace 不在检查 archive 内）；测 unchanged（DEVTOOL=ON 1228/1228 PASS 终态 / DEVTOOL=OFF 1082/1082 PASS 终态 双绿 ✅）；Phase B 累计测 +59 ON / +17 OFF；A14 守门双绿（ON test #1151 + OFF test #1082 PASS）；**Phase B 总耗时 ~104 min vs plan ×0.6 261 min = 0.40×**（10 子任务平均，落「极窄档延续」桶高效区，验证 VAN 估时假设 0.55-0.75× 偏保守 — 实际是 Phase 0 11 子段实测填写换来的高 ROI）；T5/T6 mitigation 全到位实证（T5 = ResetOverlayCommands 协议复用 / T6 = 单 instance + budget=0 短路 + 1ms guard）；技术债 #35 阶段 1 闭环 ✅ 阶段 2（拆 LayoutEngine style/layout）留 P3；下一步 `/reflect` 回顾 |
| 2026-05-02 23:50 | Build B.3.2 完成 | hello_devtool perf smoke（C API ABI 端到端）（plan 30 min ×0.6 = 18 min / 实测 ~7 min = **0.39× plan ×0.6**，落「极窄档延续」桶 — examples 增量编辑 + 已成熟 hello_devtool 模板复用 + ctest PASS_REGULAR_EXPRESSION 范式）；改 2 文件 +50 行（hello_devtool.cc 加 PerfHooks 安装段 / s_perf_smoke_frames 计数器 / OnFrameEnd capture-less lambda → C 函数指针 / 退出后 print "PERF SMOKE: frames=N hud_visible=M" 稳定行；tests/CMakeLists.txt 加 hello_devtool_perf_smoke ctest with 300ms autoquit + PASS_REGULAR_EXPRESSION `frames=[1-9][0-9]*`）；测 +1（DEVTOOL=ON 1228 / DEVTOOL=OFF 1082 unchanged — `if(VX_BUILD_DEVTOOL)` guard 复用 hello_devtool_smoke 同一 block）；实测输出 `PERF SMOKE: frames=1 hud_visible=1` ✅（lazy-attach 工作 — set_pipeline_hooks 返 INVALID_STATE 因 update_manager_ 未初始化但 hooks 已 cached → vx_view_run timer 触发 EnsureUpdateManager 后激活）；A14 守门隐式继承（`if(VX_BUILD_DEVTOOL)` guard 同 hello_devtool_smoke）；零反向探针（smoke 测的是 print line 存在性 — 删 print → ctest FAIL 是显然的） |
| 2026-05-02 23:42 | Build B.3.1 完成 | F11 toggle HUD 可见性（plan 15 min ×0.6 = 9 min / 实测 ~10 min = **1.11× plan ×0.6**，落「中件实施」桶 — 跨 5 文件更新 + F12 范式精确复用 + 测试 fixture 复用）；改 6 文件 +95 行（veloxa_api.h 加 VX_KEY_F11 + vx_view_is_hud_visible C API；sdl2_input_translate 加 SDLK_F11 → VX_KEY_F11 映射；application.h 加 hud_visible getter/setter + hud_visible_ 字段；application.cc MaybeHandleDevtoolHotkey 扩展 F11 分支 + UnloadDevtoolDocument reset hud_visible_；veloxa_api.cc 实施 vx_view_is_hud_visible；devtool_attach_api_test +5 F11/Hud 测）；测 +5（DEVTOOL=ON 1227 / DEVTOOL=OFF 1082 unchanged）；A14 守门（vx_view_is_hud_visible OFF 路径返 0 stub；libvx_api.a OFF 字节增长仅 ~50 bytes 公开 ABI 表面）；反向探针精准捕获（删 F11 toggle 路径 → F11HotkeyTogglesHudVisibleWhenEnabled FAIL） |
| 2026-05-02 23:32 | Build B.2.3 完成 | dirty rect 边框 OverlayDirtyRect 工厂 + InspectorOverlay::InjectDirtyRectHighlights（plan 30 min ×0.6 = 18 min / 实测 ~7 min = **0.39× plan ×0.6**，落「极窄档延续」桶 — InspectorOverlay 范式复用）；改 4 文件 +60 行（paint_command.h 加 OverlayDirtyRect 工厂 — 复用 kOverlayHighlight type 默认黄绿色 1px stroke；inspector_overlay.h/cc 加 InjectDirtyRectHighlights 静态方法 — 跳空 rect 防御；inspector_overlay_test 加 4 测）；测 +4（DEVTOOL=ON 1222 / DEVTOOL=OFF 1082 unchanged）；T5 mitigation 协议复用（ResetOverlayCommands 同时清 hover + dirty rect overlays — 不扩增 reset 路径）；反向探针精准捕获（跳 IsEmpty filter → SkipsEmpty 测 FAIL） |
| 2026-05-02 23:25 | Build B.2.2 完成 | HUD JS updateHud + vx_view_get_perf_stats native binding（plan 30 min ×0.6 = 18 min / 实测 ~13 min = **0.72× plan ×0.6**，落「中件实施」桶下端 — 跨 vx_script ↔ vx_devtool 边界 + forward declare PerfOverlay 解循环依赖 + JSON envelope 设计）；改 5 文件 +110 行（dom_bindings.h forward decl + SetPerfOverlay/perf_overlay；dom_bindings.cc InstanceData 加字段 + VxViewGetPerfStats C 函数 + 注册；inspector_panel.js 加 updateHud；devtool_bindings_test +3 测；inspector_panel_smoke_test +2 测；CMake link vx_devtool to ON build）；测 +5（DEVTOOL=ON 1218 / DEVTOOL=OFF 1082 unchanged — A14 守门：SetPerfOverlay setter 在 OFF 路径可调用但 vx_view_get_perf_stats 函数本身在 OFF 是 stub，零 vx_devtool 内部符号泄漏）；反向探针精准捕获（fps 设 0 → LiveStats 测 FAIL） |
| 2026-05-02 23:12 | Build B.2.1 完成 | HUD overlay HTML/CSS（absolute + opacity 0.85；creative #1 决策 3 落地）（plan 45 min ×0.6 = 27 min / 实测 ~7 min = **0.26× plan ×0.6**，落「极窄档延续」桶 — HTML/CSS 资源类纯文本编辑）；改 3 文件 +60 行（inspector_panel.html 加 #devtool-hud + 4 stage bars + data-passthrough；inspector_panel.css 加 HUD 样式块；inspector_panel_smoke_test 加 7 HUD smoke 测）；测 +7（DEVTOOL=ON 1213 / DEVTOOL=OFF 1082 unchanged）；R8 mitigation 落地（`position: fixed` fallback 为 `absolute` — plan §0.9 grep 实证视觉等价）；R9 mitigation 占位（data-passthrough="1" attribute — HitTest 改造留 R3+ EventManager 任务） |
| 2026-05-02 23:05 | Build B.1.2 完成 | PerfOverlay::Attach + 5 trampolines + T6 budget abort + 单 instance + 自动填 FrameStats（plan 45 min ×0.6 = 27 min / 实测 ~15 min = **0.56× plan ×0.6**，落「中件实施」桶下端 — 涉及 SteadyTimePoint 时间差计算 + 跨 vx_devtool/vx_core lib 边界 + 5 trampoline + budget guard 边缘场景）；改 2 文件 +130 行（perf_overlay.h/cc + perf_overlay_attach_test.cc 新建）+ tests/devtool/overlay/CMakeLists.txt（vx_add_test 注册）；测 +12（DEVTOOL=ON 1206 / DEVTOOL=OFF 1082 unchanged）；A14 隐式守门；反向探针精准捕获（跳 single-instance check → AttachTwiceRejects FAIL）；T6 mitigation 落地（单 instance + budget=0 显式短路 + budget guard 累加）；plan-fact 校准（plan §B.1.2 budget=0 floating-point 精度问题 → 改为 `budget_us_ == 0` 显式短路） |
| 2026-05-02 22:50 | Build B.1.1 完成 | PerfOverlay FrameStats ring buffer + 60 帧滑动聚合 + FPS（cap 999 / div-by-zero guard）（plan 60 min ×0.6 = 36 min / 实测 ~8 min = **0.22× plan ×0.6**，落「极窄档延续」桶 — 纯算法零外部依赖）；新建 4 文件 +130 行（perf_overlay.h/cc + perf_overlay_test.cc + tests/devtool/overlay/CMakeLists.txt）+ 改 2 文件（vx_devtool target_sources append + tests/devtool/CMakeLists.txt 加 add_subdirectory）；测 +8（DEVTOOL=ON 1194 / DEVTOOL=OFF 1082 unchanged — `if(VX_BUILD_DEVTOOL)` guard）；A14 隐式守门（vx_devtool 内部符号自然不在 OFF build）；反向探针精准捕获（去 modulo → RingBufferOverwritesOldestFrameAfter60 FAIL）；friend class PerfOverlayTest 白名单引入（plan §0.5 测试基础设施审计实施） |
| 2026-05-02 22:42 | Build B.0.2 完成 | dirty_rects_ Vector 累积扩展（plan 60 min ×0.6 = 36 min / 实测 ~7 min = **0.19× plan ×0.6**，落「极窄档延续」桶 — Vector + push_back + clear API 极简）；改 3 文件 +90 行；测 +6（DEVTOOL=ON 1186 / DEVTOOL=OFF 1082 双绿）；plan §B.0.2 RED 测假设按 update_manager_test:131 现实校准（"size=3 假设" → "hover change 累积模型"）；A14 守门隐式继承（dirty_rects_ 字段是 vx_core 内部，零 vx_devtool 泄漏）；反向探针精准捕获（注释 push_back → FirstUpdateAccumulatesOneDirtyRect FAIL + back() on empty CHECK abort） |
| 2026-05-02 22:35 | Build B.0.1 完成 | UpdateManager PipelineHooks 五钩子 + vx_view_set_pipeline_hooks C API（**技术债 #35 阶段 1 闭环 ✅**）；plan 90 min ×0.6 = 54 min / 实测 ~25 min（**0.46× plan ×0.6**，落「极窄档延续」桶）；改 8 文件 +400 行；测 +11（DEVTOOL=ON 1180 / DEVTOOL=OFF 1076 双绿）；A14 ctest smoke PASS（libvx_api.a OFF + ~700 bytes 公开 ABI 表面 / libvx_core.a OFF + ~10 KB 内部增长，零 vx_devtool 泄漏）；反向探针精准捕获（注释 OnAfterLayout → 2 测精准 FAIL）；lazy-attach 设计让 caller 不需要先 LoadHTML（INVALID_STATE 提示） |
| 2026-05-02 22:10 | Plan 完成 | Phase 0 11 子段实测填写完成（ctest baseline 二次验证 1169 ON / 1065 OFF ✅ + 五钩子 mapping 5 注入点明确 + FrameStats 5 字段公式表 + CSS HUD 设计可行 100%（`opacity` / `position: fixed` fallback 为 `absolute` 视觉等价 + R2-verified `border-radius` 等）+ `UpdateManager::Update()` 单一 callsite 改造局部化 + dirty_rect 单矩形事实 → B4 Vector 累积不替换 + friend pattern 引入方案明确）；B1-B8 build 级精化决策表全部锁定（用户 1 次 AskQuestion 选 all_recommended → 8/8 按推荐：B1=A 独立 plan / B2=A 严格串行 / B3=A `tests/devtool/overlay/` 子树 / B4=A 单矩形 Vector 扩展 / B5=A 复用 kOverlayHighlight + 新工厂 / B6=A 每子任务 1 commit / B7=A ~3-4 h plan ×0.6 假设 / B8=A 复用 spec）；plan `docs/plans/2026-05-02-devtool-perf-overlay.md` 落盘 ~600 行（10 子任务 5-步 TDD 模板 + 完整代码片段 + 4 R 风险登记 + 3 Checkpoint + 9 条 systemPatterns 协同度自我对照）；plan ×0.6 第 38 数据点假设入库（蓝图 4.35 h → archive 校准 ~3.5-5 h → VAN F2 发现 ~3-4 h → 最终预测 ~3-4 h plan ×0.6 即 0.55-0.75× 比值）；推荐路径 `/build` 启动 B.0.1 PipelineHooks 五钩子（最大子任务 plan 90 min ×0.6 = 54 min）|

</details>

---

<details>
<summary>TASK-20260502-01：DevTool Phase A — Inspector 实施（Level 4 / 2026-05-02）— ✅ 已归档（点开查看历史）</summary>

### TASK-20260502-01：DevTool Phase A — Inspector 实施（DOM tree / Style panel / Layout panel + 元素 hover 高亮）[安全相关]

- **复杂度级别：** ~~Level 3~~ → **Level 4**（2026-05-02 14:15 用户选 escalation A 升级；Phase A.1 dogfood UI 实质子系统级 + 8 子任务；Phase A 总 20 子任务跨 5 子系统；与 spec §10 ≥30 systemPatterns 自我对照 + V1-V8 决策矩阵 + Phase A.0/A.1/A.2/A.3 多 Phase 结构对齐 Level 4 判定）
- **状态：** ✅ **已归档（2026-05-02 ~18:10）** — 归档文档 `memory-bank/archive/archive-TASK-20260502-01.md`（Level 4 全维度，10 段含技术方案 + 实现摘要 + 11 处 plan-fact reconcile 表 + 4 安全威胁 mitigation 总结 + 改进建议落实状态 + 架构影响 + 长期维护建议 + 后续任务依赖估时调整）；reflection 产出 `memory-bank/reflection/reflection-TASK-20260502-01.md`（13 段全维度）；16/16 子任务 build 全完成（28 commits / ctest ON 1169 + OFF 1065 / A14 链接闭包零自动化守门 / 4 安全威胁 mitigation 全到位 / 2 历史技术债 #26 + #40 闭环 / 3 R2 P3 候选沉淀）；同步沉淀 3 新 systemPattern + plan ×0.6 矩阵第 18-37 数据点入库 + 「大件实现」桶系数下调 + techContext StatusOr 规范段 + productContext DevTool Phase A 能力更新段。**改进建议落实率：** P0 3/3 ✅（沉淀到 systemPatterns）+ P1 5/5 落实/迁移完成（A14 smoke template ✅ + dogfood SOP ✅ + 余 3 项 git-workflow / StatusOr audit / spec A14 附录已迁移到 activeContext 待处理事项）+ P2 2/2（escalation 估时校准 ✅ + R2 P3 立项预案 ✅）。
- **创建日期：** 2026-05-02
- **分支：** `feature/TASK-20260502-01-devtool-inspector`（基于 main `679304e`，已含 TASK-30-04 蓝图全部归档）
- **设计 spec：** ✅ 复用 `docs/specs/2026-04-30-devtool-design.md` Inspector 验收段（A1-A5, A13, A14 + T3/T5/T7/T8 威胁 + I1-I8 注入点中 I1/I3/I4/I5/I6 + R1/R2/R5 风险）
- **实现 plan：** ✅ **本任务专属 build 级 plan** `docs/plans/2026-05-02-devtool-inspector.md`（~700 行 / 16 子任务 RED/GREEN/REFACTOR 5-步模板 + Phase 0 11 子段实测填写 + B1-B8 决策表 + plan ×0.6 第 18 数据点假设 + R7/R8 新增风险登记 + 2 Checkpoint）；蓝图 plan `docs/plans/2026-04-30-devtool.md` §Phase A 段作为参照基线
- **创意文档：** ✅ 复用 `memory-bank/creative/creative-devtool-screen-layout.md`（splitter dock + HUD overlay 双层结构 5 决策已锁定，**无新 creative 需求**）
- **需要创意阶段：** ❌ 否（TASK-30-04 蓝图阶段已产出 creative #1 全覆盖 Phase A UI 设计决策；DevTool UI 主屏布局 / dock 模式切换 / HUD 透明合成 / overlay 渲染顺序双线宽 / F12-F11 toggle 5 决策直接复用）
- **来源：** TASK-20260430-04 蓝图主交付独立立项候选 §主线 3 项之 A；用户主动通过 `/van TASK-20260430-04` + AskQuestion 选 subtask_a 启动
- **安全相关：** ✅ 是（T3 Inspector 敏感数据 redact / T5 DisplayList overlay 隔离 / T7 C API buffer overflow 双调用模式 / T8 共享容器 mutation propagation 4 个威胁面）

#### 任务范围（用户决策路径锁定 — 跳过 AskQuestion 由 VAN 推荐默认）

| # | 维度 | 选择 | 理由 |
|:-:|---|---|---|
| V1 | 子系统范围 | **Phase A Inspector 16 子任务**（A.0 前置改造 6 + A.1 DevTool UI 4 + A.2 安全单测 4 + A.3 example/reflect 2） | TASK-30-04 plan §Phase A 直接复用，无新增 |
| V2 | 实施模式 | **b 完整实施**（含 build / ctest / commit / merge） | 与「实施」语义匹配；Level 3 build 主路径 |
| V3 | 复杂度 | **Level 3** | 跨 4 子系统（core/devtool/api/tests）但 plan 已明确；不需新架构决策 |
| V4 | 创意需求 | **❌ 否** | creative #1 已覆盖；无新设计空白 |
| V5 | 安全标注 | ✅ **是** | 4 威胁面 T3/T5/T7/T8 全程守门 |

#### VAN 阶段实证（R1 callsite 量化 + 基础设施成熟度三色）

| # | 命题 | grep 实证 | 影响蓝图 |
|:-:|---|---|:-:|
| F1 | I1 R1 callsite 范围（spec 估 10-15 处） | ⚠️ 实测 **5-9 处**（src `application.{h,cc}` 9 处字段访问 + tests 4 处 `app.document()` callsite + dom_bindings_test 1 处不相关）→ R1 风险降 🟡→🟢-🟡 | 🟢 比 spec 预估更可控 |
| F2 | I4 LayoutBox `ToJson()` 缺失 | ✅ 仍缺失（技术债 #26 未闭环）| 🔴 A.0.2 任务必做 |
| F3 | I5 DOM Serializer `ToJson(node, RedactionPolicy)` 缺失 | ✅ 仅有 HTML `Serialize()`，JSON 路径缺失 | 🟡 A.0.3 任务必做 |
| F4 | I3 PaintCommand `kOverlayHighlight` tag 缺失 | ✅ 仍缺失（render renderer 无 overlay 注入）| 🔴 A.0.4 任务必做 |
| F5 | C API DevTool introspection 接口 | ⚠️ 仅运行时 `vx_view_*`，缺 `vx_view_serialize_dom_json` / `vx_node_get_computed_style_json` / `vx_node_get_layout_box_json`（技术债 #40） | 🟡 A.0.5 + A.0.6 任务必做 |
| F6 | DOM `Serialize(node)` HTML 路径 | ✅ 已就绪 — `veloxa/core/dom/serializer.h` 含 `Serialize(const Node*)`；A.0.3 在此基础上加 `ToJson` 重载 | 🟢 A.0.3 复用基础 |
| F7 | EventManager hover/HitTest | ✅ 已就绪 — TASK-25-01 后 `HitTest()` + hover_target 状态可用 | 🟢 A.1.1 hover 选取直接复用 |
| F8 | SDL2 后端 + `vx_view_run` | ✅ 已就绪 — TASK-25-01 后可见窗口 + 输入事件三阶段冒泡 + auto-quit env hook | 🟢 A.3.1 example 复用 |
| F9 | FetchContent 状态 | ✅ 三处 `_deps/` 离线齐全；Phase A 零新 FetchContent 依赖 | 🟢 跳过 git proxy 守卫 |

**汇总：** 5 ✅ 已就绪 / 2 ⚠️ 需扩展（小工程） / 2 🔴 需新建（属技术债 #26 + #40）— **R1 风险经实证降级**

#### 触及技术债映射（与 `techContext.md` 对照）

| # | 技术债 | 子任务 | 闭环节奏 |
|:-:|---|---|---|
| #26 | LayoutBox.Dump 调试方法缺失 | A.0.2 LayoutBox::ToJson() | 本任务一次性闭环 |
| #40 | C API 缺 DOM/Style/Layout introspection | A.0.5 + A.0.6 双层 API | 本任务一次性闭环 |
| #4 | ImageCache 命名空间隔离（DevTool icon vs target image） | （Phase B 才用，本任务不闭环） | 留 Phase B（TASK-20260502-NN 后续） |

#### 验收要点（Phase A 主交付，A1-A5 + A13 + A14）

- A1 DevTool 主屏可见 DOM tree 文本 + 折叠树节点交互（手工 + DOM tree JSON 序列化单测）
- A2 鼠标 hover 目标 View 元素 → DevTool 高亮选中节点 + DisplayList overlay 红框（手工 + DisplayList overlay 命令单测）
- A3 选中节点显示 ComputedStyle JSON（手工 + `vx_node_get_computed_style_json()` C API 单测）
- A4 选中节点显示 LayoutBox JSON（手工 + `vx_node_get_layout_box_json()` C API 单测）
- A5 DOM 序列化默认 redact `<input type="password">` value 为 `[REDACTED]`（T3 单测）
- A13 现有 ctest 1062/1062 全绿（DevTool OFF 时零回归 — `VX_BUILD_DEVTOOL=OFF` 默认）
- A14 DevTool 关闭时构建产物零变化（链接闭合 + binary size diff = 0）

#### Phase A 子任务清单（plan 已就绪，build 阶段执行）

| 子任务 | 描述 | 估时 plan ×0.6 |
|:-:|---|---:|
| A.0.1 | I1 Application 双 Document 槽改造 + 全 callsite 重命名 | 54 min |
| A.0.2 | I4 LayoutBox::ToJson() 闭环技术债 #26 | 18 min |
| A.0.3 | I5 DOM Serializer::ToJson(node, RedactionPolicy) | 27 min |
| A.0.4 | I3 PaintCommand kOverlayHighlight + ResetOverlayCommands | 18 min |
| A.0.5 | C API 内部 C++ 层 `inspector_data.h` | 36 min |
| A.0.6 | C API 公开 thin wrapper `vx_view_serialize_*` | 36 min |
| A.1.1 | InspectorOverlay::InjectHoverHighlight (DisplayList&)（M2 修正签名） | 18 min |
| A.1.2 | DevTool resource 编译期嵌入（B-A1.1=b CMake `file(READ)`，T2 消除）| 27 min |
| A.1.3 | inspector_panel.html/css/js 编写 | 54 min |
| A.1.4 | JS native binding 扩展（vx_devtool_get_dom_json 等全局函数） | 36 min |
| A.1.5 | InputDispatchSplitter（hit-area 路由 + drag capture，新增） | 27 min |
| A.1.6 | Application 双 UpdateManager + LoadDevtoolDocument（M1 + M3，新增）| 36 min |
| A.1.7 | vx_view_attach_devtool/detach C API + F12 hotkey | 27 min |
| A.1.8 | dogfood headless smoke 集成测（新增） | 45 min |
| A.2.1 | T3 redaction policy 单测 + API | 18 min |
| A.2.2 | T5 overlay 隔离 + 每帧复位单测 | 18 min |
| A.2.3 | T7 buffer overflow 守卫单测（双调用模式 + max_size） | 18 min |
| A.2.4 | A14 binary size diff 验证（DevTool OFF 编译） | 9 min |
| A.3.1 | examples/hello_devtool.cc — Inspector smoke | 27 min |
| A.3.2 | Phase A reflect + commit + integration test | 18 min |
| **合计** | **20 子任务**（A.1 escalation +4 子任务） | **473 min（7.88 h）** |

#### 前置验证清单（VAN 阶段产出）

| 维度 | 结果 | 备注 |
|---|:-:|---|
| 依赖可获取性 | ✅ | F9 — Phase A 零新 FetchContent；用既有 stdlib + DOM/Layout/Render |
| 环境就绪 | ✅ | `build/` + `build-bench/` 已存在；ctest 1062/1062 隐式继承 |
| 已有 artifact | ✅ | spec + plan + creative #1 全部就绪（TASK-30-04 主交付）；可直接进 build phase 精化 |
| ctest 基线 | ✅ | 1062/1062 PASS（main `679304e` 终态继承 — TASK-30-03 R2.5 守卫单测纳入后基线）|
| FetchContent 代理守卫 | ⊘ 跳过 | F9 — 三处 `_deps/` 离线预置；本任务零新 FetchContent 依赖 |
| 待处理事项关联 | ✅ 极强 | 闭环 2 项历史技术债（#26 LayoutBox.Dump / #40 C API introspection）；R3+ #2 EventDispatcher snapshot iteration / #3 LoadHTML reset dom_bindings_ 间接相关（A.0 改造可能触发 lifecycle 边界）|

#### VAN 阶段 push-back 决策（已沉淀）

| 风险 | 应对 |
|---|---|
| R1 「I1 callsite 漏改」漏改导致编译/测试失败 | A.0.1 必做：`document_` → `target_document_` 重命名让漏改在编译期暴露；实证 callsite ~5-9 处（低于 spec 估）|
| R2 「DevTool 自渲染暴露引擎缺陷阻塞主线」陷阱 | DevTool UI 主屏用「已验证 OK」CSS 子集（avoid flex 边角 + bidi + transform）；缺陷暴露后立即列入 R3+ 任务，不阻塞主线 |
| R5 「ComputedStyle JSON 序列化 hover hot path 性能」陷阱 | hover 选取走内部 C++ API（D7=C 第一层零拷贝）；C API JSON 走第二层薄封装仅外部接入用；ComputedStyle JSON lazy（仅用户点开 Style panel 时序列化）|
| 「直接进 build 跳过 plan 精化」诱惑 | 拒绝 — Level 3 默认进 `/plan` 做 Phase 0 grep（callsite 全表 + 测试 fingerprint + CMake 链接审计） + plan ×0.6 估时校准；本任务的 plan 阶段是 build 级精化（与 TASK-30-04 蓝图级 plan 区分）|
| 「TASK-30-04 plan 已就绪 build phase 全照搬」陷阱 | plan 阶段必须验证：(a) plan 写于 2026-04-30，main 终态可能小改；(b) plan ×0.6 第 18 数据点应实测沉淀（极窄档延续 vs 回归 review 类下限）|

#### 与并发任务关系

- **TASK-20260430-04**（DevTool 蓝图）：✅ 已归档（2026-05-01 ~03:00），main `679304e` 已含 spec + plan + 2 creative；本任务在新 main 上独立演进，零 git 依赖
- **TASK-20260430-03**（codebase review）：✅ 已归档（2026-05-01 ~00:30），R3+ #2 EventDispatcher snapshot iteration / #3 LoadHTML reset dom_bindings_ 与本任务 A.0 lifecycle 改造**间接相关**（在 A.0.1 + A.1.1 实施中需注意 EventDispatcher 在 hover 状态变化时的安全性，必要时联动修复 F-046）
- **TASK-20260425-01 SDL2 后端**：✅ 已归档（2026-04-26）；本任务 A.3.1 example smoke 直接复用 `Sdl2WindowSurface` / `Sdl2EventLoop` / `vx_view_run` auto-quit env hook

#### Plan 阶段产出（B1-B8 决策表 + Phase 0 11 子段实测）

**B1-B8 build 级精化决策（用户 1 次 AskQuestion 选 all_recommended → 8/8 按 VAN 推荐锁定）：**

- B1=A 独立 plan 文档 `docs/plans/2026-05-02-devtool-inspector.md`
- B2=A 严格串行 A.0.1 → 0.6
- B3=B 混合测试组织（A.0 → tests/core/ + A.1+ → 新建 tests/devtool/inspector/）
- B4=B runtime 文件读取（与蓝图 §0.2 Q6 一致）
- B5=OFF（蓝图已锁，A14 守门依赖）
- B6=A 每子任务 1 commit（~16 + reflect/archive ≈ 18 commits）
- B7=A 沿用蓝图估时 7.35 h plan ×0.6（baseline，reflect 实测沉淀第 18 数据点）
- B8=A 复用 TASK-30-04 spec

**Phase 0 11 子段实测关键发现：**

- **0.1 ctest 基线 reconfigure**：当前 `build/` 过期（mtime 2026-04-26），实测 1061，与 R2.5 落地后预期 1062 不符 → Phase A 启动前必跑 reconfigure（plan §0.1）
- **0.7 R1 callsite 二次实证**：`->document()` / `.document()` 实际 4 处（`tests/core/application_test.cc` lines 53/60/72/208）+ `dom_bindings.document()` 1 处不相关 → R1 风险 🟡→🟢 进一步降级（远低于蓝图估 10-15 处）
- **0.8 既有测试 fingerprint**：layout 65 / parser 45 / paint_command 8 / renderer 17 ≥ 期望 ✅；event + application 子系统命中略低 ⚠️ → A.0.1/A.1.1 实施前补 grep 扩展关键字
- **0.10 工具链**：rg/awk/python3 ✅；jq 缺失 → A.2.4 用 python3 兜底
- **0.9 CSS shorthand**：`flex` ✅ 6 处；`background`/`font`/`border-radius` 待 A.1.2 build 时 grep（缺失改 longhand 或 P3）

**plan 文档结构亮点：**

- 16 子任务全部 5-6 步 TDD 模板（RED → GREEN → 反向探针 → A14 守门 → commit）
- 每子任务附完整代码片段（不是只描述）— A.0.2 LayoutBox::ToJson()、A.0.3 DOM Serializer::ToJson() T3 redact、A.0.4 PaintCommand kOverlayHighlight、A.0.5 inspector_data.h、A.0.6 vx_view_serialize_*_json T7 双调用模式
- writing-plans 必填 9 段全部就绪：CMake 链接 / 静态库循环 / FetchContent / 测试基础设施 / 边界输入清单 ≥ 14 条 / 调用链端到端 / 既有测试 fingerprint / CSS shorthand / smoke 工具链
- 安全任务 4 项（A.2.1 T3 / A.2.2 T5 / A.2.3 T7 / A.2.4 A14 守门）独立段
- 2 Checkpoint（A.0 完成 / A.1 完成）+ 默认协议（隐式批准走 A 选项继续）
- R7 新增「蓝图 plan 漂移」+ R8「runtime resource 加载失败」2 风险登记

#### 任务历史

| 时间 | 阶段 | 备注 |
|---|---|---|
| 2026-05-02 12:25 | 用户启动 | `/van TASK-20260430-04`（已归档 ID）+ AskQuestion 选 subtask_a → 实际启动 TASK-30-04 蓝图主线子任务 A |
| 2026-05-02 12:30 | VAN 完成 | 工作区干净 ✅；R1 callsite 量化 5-9 处 ⬇ 风险降级；分支 `feature/TASK-20260502-01-devtool-inspector` 基于 main `679304e` 创建；前置验证 6/6 全 PASS（含 ctest 1062 基线继承 + 2 项技术债 ROI 闭环）；MB 三件套同步；推荐路径 `/plan` 做 build 级精化 |
| 2026-05-02 13:10 | Plan 完成 | brainstorming B1-B8 全部锁定（用户 1 次 AskQuestion 选 all_recommended → 与蓝图 D1-D8 协同度 8/8 ✅）；Phase 0 11 子段实测填写完成；R1 callsite 二次实证 4 处真实（远低于蓝图估 10-15）；ctest 基线漂移发现（build/ 过期需 reconfigure）；plan `docs/plans/2026-05-02-devtool-inspector.md` 落盘（~700 行）；plan ×0.6 第 18 数据点假设入库；推荐路径 `/build` 启动 Phase 0.1 reconfigure |
| 2026-05-02 13:30 | Build 轮次 1 完成 | Phase 0.1 reconfigure ctest 1062 基线确认；A.0.1 双 Document 槽位（10 min vs plan 54 min ×0.6 = 0.18×）；A.0.2 LayoutBox::ToJson + #26 闭环（10 min vs 18 ×0.6 = 0.56×）；A.0.3 DOM Serializer::ToJson + T3 redact（15 min vs 27 ×0.6 = 0.56×）；A.0.4 PaintCommand kOverlayHighlight + T5 ResetOverlayCommands（15 min vs 18 ×0.6 = 0.83×）；ctest 1062→1085 PASS；轮次 1 暂停 commit |
| 2026-05-02 13:55 | Build 轮次 2 完成 | A.0.5 vx_devtool 静态库 + inspector_data 内部 C++ API（25 min vs plan 36 ×0.6 = 0.69×）+ A14 OFF baseline 1054 ✅；A.0.6 vx_view_serialize_dom_json 公共 C API + T7 buffer overflow + #40 闭环（20 min vs 36 ×0.6 = 0.56×）+ A14 OFF baseline 1057 ✅；ctest DEVTOOL=ON 1085→1102 / DEVTOOL=OFF 1054→1057；轮次 2 暂停 commit `d9ec183` |
| 2026-05-02 14:00 | Build 轮次 3 中止 → Plan escalation | 批判审查 plan A.1.x 发现 Phase A.1 dogfood UI 实质 Level 4 子系统级（plan ×0.6 144 min vs 真实 ~6-8 h+，失配 4-5×）；缺陷清单：(1) A.1.2 需新建 JS native binding + runtime 文件 IO（触发未列威胁 T2 路径穿越）+ 双 viewport 渲染（属 creative 域）+ splitter dock 鼠标拖动 + SDL2 真窗口 dogfood；(2) A.1.1 签名缺陷 `target_doc` → `DisplayList&`；(3) A.1.4 依赖 splitter 实现定型；用户决策 D「返回 /plan 修正」；Phase A.0 6 commits 真实产出保留；待用户 `/plan` 进入 escalation 决策（A 升级 / B 拆分 / C 闭环 reflect） |
| 2026-05-02 14:30 | Plan escalation 完成 | 用户选 escalation A 升级当前 task 为 Level 4；brainstorming 锁定 2 决策：B-A1.1=b 编译期嵌入（推翻 B4=B → B4=A，T2 威胁完全消除 + 减 1 篇 creative + 加快交付 ~3-5h）+ B-A1.2=a full viewport + 渲染覆盖（splitter 拖拽零额外开销）；架构 grep 验证发现 3 项修正 M1 (Application 双 UpdateManager，原假设 `Renderer::Render(target,devtool)` 不成立，实际 `render::*` 是 free fn + UpdateManager 是 single-Doc) / M2 (InjectHoverHighlight 签名 `Document*` → `DisplayList&`) / M3 (B-A1.1=b 嵌入路径替代 inspector_panel_loader)；plan 文档扩 +384 行（A.1 段从粗概念替换为 8 子任务 detailed 5-步 TDD 模板，每子任务含完整代码片段 + RED/GREEN/反向探针/A14 守门 + commit msg）；Phase A 总子任务 16 → 20；总估时 7.35 → 7.88 h plan ×0.6（+32 min，可控）；**0 篇新 creative 需要**；推荐路径 `/build` 续上 A.1.1 |
| 2026-05-02 15:15 | Build 轮次 3 完成 | A.1.1 InspectorOverlay::InjectHoverHighlight (DisplayList&)（10 min vs 18 ×0.6 = 0.56×）+ A.1.2 DevTool resource 编译期嵌入 + T2 消除（15 min vs 27 ×0.6 = 0.56×）+ A.1.3 inspector_panel.{html,css,js} + R2-verified CSS（25 min vs 54 ×0.6 = 0.46×）；ctest ON 1102→1124 / OFF 1057 持平；libvx_api.a OFF 12156 持平；轮次 3 暂停 commit `5c76346` |
| 2026-05-02 15:50 | Build 轮次 4 完成 | A.1.4 vx_devtool_get_dom_json JS native binding + DomBindings::SetDevtoolTargetDocument（22 min vs 36 ×0.6 = 0.61×）；ctest ON 1124→1128 / OFF 1057→1058；轮次 4 暂停 commit `a411cc9` |
| 2026-05-02 15:55 | Build 轮次 5 完成 | A.1.5 InputDispatchSplitter（hit-area + drag capture 纯算法状态机 + R1 mitigation 守卫）（12 min vs 27 ×0.6 = 0.44×）；ctest ON 1128→1135 / OFF 1058 持平；轮次 5 暂停 commit `990029e` |
| 2026-05-02 16:00 | Build 轮次 6 完成 | A.1.6 Application 双 UpdateManager + LoadDevtoolDocument + canvas Translate（M1 + M3 修正实施）（22 min vs 36 ×0.6 = 0.61×）；ctest ON 1135→1142 / OFF 1058 持平；轮次 6 暂停 commit `05c5a30` |
| 2026-05-02 16:42 | Build 轮次 7 完成 | A.1.7 vx_view_attach_devtool C API + F12 hotkey 内化到 Core 层（14 min vs 27 ×0.6 = 0.52×）+ A.1.8 dogfood headless smoke 集成测 + JS execution wiring + R2 三连缺陷暴露（28 min vs 45 ×0.6 = 0.62×）；R2 P3 候选 3 项沉淀（DomBindings Element.children / addEventListener / innerHTML setter 三连）；ctest ON 1142→1156 / OFF 1058→1062；libvx_api.a OFF 12156→12646（公开 ABI 表面 + 490 bytes，nm 验证零 DevTool 链接）；libvx_core.a OFF 1771620→1775326（application.cc 内部增长 + 3706 bytes，nm 验证零 DevTool 符号）；Phase A.1 8/8 全部完成 ✅；轮次 7 暂停 commit `3b70ebf` |
| 2026-05-02 17:25 | Build 轮次 8 完成（Phase A 16/16 ✅✅）| A.2.1 T3 redaction policy + vx_inspector_set_redaction_policy C API + DomBindings/Application 三处统一安全策略（13 min vs 18 ×0.6 = 0.72×）+ A.2.2 T5 多帧 overlay 隔离集成测 + 负控制测（6 min vs 18 ×0.6 = 0.33×）+ A.2.3 T7 buffer overflow 4 边界测（plan 4GB 拒绝错误修正）（7 min vs 18 ×0.6 = 0.39×）+ A.2.4 A14 链接闭包 ctest smoke 自动化（pure CMake script + nm + 8 符号黑名单）（8 min vs 9 ×0.6 = 0.89×，唯一近 1× 数据点）+ A.3.1 hello_devtool example + headless smoke（SDL2 真窗口 + DevTool attach + F12 + 运行时 T3 验证）（7 min vs 27 ×0.6 = 0.26×）+ A.3.2 Phase A integration verify + reflect prep（2 min vs 18 ×0.6 = 0.11×）；ctest ON 1156→1169 (+13 测) / OFF 1062→1065 (+3 测)；Phase A 总览 16 子任务 / ~281 min vs 441 min plan ×0.6 = 0.64×；轮次 8 commits `bb729e9 / 1023ddd / 62c959d / 1ec6b80 / d8245c5` |
| 2026-05-02 17:50 | Reflect 完成 | Level 4 全维度 reflection 产出 `memory-bank/reflection/reflection-TASK-20260502-01.md`（13 段 / ~880 行）— Executive Summary + 计划 vs 实际 5 维（数量 / 时间 plan ×0.6 第 18-37 数据点 / 范围 / 文件变更 / 11 处 plan-fact reconcile）+ 检查清单 4 维 + 做得好的 3 类 + 挑战 6 类 + 教训 4 大跨子任务复用 + 改进建议 10 项（P0×3 / P1×5 / P2×2）+ 安全评估 + 反复模式识别（7 已知 + 3 新模式）+ 架构评估 + 长期影响分析 + 元反思；同步沉淀：systemPatterns 3 新段（plan escalation 中途触发 + 反向探针有效性陷阱清单 + 子系统关闭守门 ctest smoke 范式）+ plan ×0.6 矩阵第 18-37 数据点入库 + 「大件实现」桶系数下调 0.8-1.2× → 0.6-1.1× + 3 子桶（极细致 plan 0.40-0.60× / 批量小子任务 0.30-0.50× / escalation 后子任务 ~1.0×）+ techContext StatusOr 使用规范段 + productContext DevTool Phase A 能力更新段；reflect commit `57d46b6` |
| 2026-05-02 18:10 | Archive 完成 | 归档文档 `memory-bank/archive/archive-TASK-20260502-01.md`（10 段 Level 4 全面归档 / ~340 行）— 任务概述 + 技术方案（工作流路径 + 16 子任务实施全景 + 28 commits 时间线 + 11 处 plan-fact reconcile + 8 关键决策 + 4 安全决策）+ 实现摘要（~32 文件变更 + 公开 C API 7 函数 + 内部 C++ API + JS native binding + 编译期嵌入 CMake）+ 测试覆盖（ctest 1062→1169 ON / 1062→1065 OFF + A14 自动化守门）+ 4 大教训 + 改进建议落实状态 + 安全评估总结 + 架构影响（含 R2 P3 3 候选 + 长期 12月/3-6月/1年）+ 长期维护建议；改进建议落实率：P0 3/3 + P1 5/5（部分迁移 activeContext）+ P2 2/2；archive commit `(本次)` |

</details>

---

<details>
<summary>TASK-20260430-04：UI 编辑器 + 调试器规划（DevTool 三件套蓝图）[安全相关] — ✅ 已归档（点开查看历史）</summary>

### TASK-20260430-04：UI 编辑器 + 调试器规划（DevTool 三件套蓝图）[安全相关]

- **复杂度级别：** Level 4（多子系统蓝图 + 8 决策矩阵 + 8 威胁面 + 涉及 6 个 DevTool 子系统中的 3 件套主交付 + 4 件扩展候选）
- **状态：** ✅ 已归档（2026-05-01 ~03:00）— 归档文档 `memory-bank/archive/archive-TASK-20260430-04.md`；reflection 10 项改进建议落实率 90%（P0 3/3 + P1 4/4 + P2 2/3，剩 1 项 P2 #10 待 TASK-30-04-A/B/C 立项后实测回填）；4 篇蓝图文档落盘（spec + plan + 2 creative 合计 ~1879 行）；plan ×0.6 第 17 数据点入库（核心轮次 0.27-0.35× plan / 0.46-0.59× plan ×0.6 — 极窄档 + review 类下限交界）；首次 V2=a 蓝图任务工作流变体实践 + 「批量决策跳过 + 批量文档产出」3 数据点群组化「极窄档」+ dogfood 路径作为探测性 acceptance test 概念沉淀
- **创建日期：** 2026-04-30
- **分支：** `feature/TASK-20260430-04-ui-editor-debugger`（基于 main `2445990` — 已含 TASK-30-03 codebase review 全部归档 + R2 quick fix 6 项落地）
- **设计 spec：** ✅ `docs/specs/2026-04-30-devtool-design.md`（12 段 / 三件套验收 A1-A14 / D1-D8 / 注入点 I1-I8 / T1-T8 / R1-R6 / ≥ 30 systemPatterns 自我对照）
- **实现 plan：** ✅ `docs/plans/2026-04-30-devtool.md`（Phase 0/A/B/C/D 划分 + CMake 链接审计 + 静态库循环审计 + 边界输入清单 16 项 + 子任务 ~40 项 + plan ×0.6 估时矩阵）
- **创意文档：** ✅ 2 篇
  - `memory-bank/creative/creative-devtool-screen-layout.md`（splitter dock + HUD overlay 双层结构 / 5 决策）
  - `memory-bank/creative/creative-devtool-hot-reload.md`（InotifyFileWatcher T2 8 步守卫 / DOM 状态保留协议 / 5 决策）
- **需要创意阶段：** ✅ 是（已产出 2 篇 — DevTool UI 主屏布局 + Hot Reload 协议）
- **来源：** 用户主动发起；`productContext.md §愿景` DevTool 主线对齐；TASK-25-01 SDL2 后端归档时定位为「实时调试 UI 主线第一步」是直接前置依赖
- **安全相关：** ✅ 是（V5=✅；D8=A T2/T3/T5/T6/T7/T8 完整建模 + T1/T4 扩展段占位）

#### 任务语义

「**规划 ui 编辑器与调试器**」用户原指令中「规划」二字是核心锚点 — **主交付 = 蓝图级 spec/plan + creative 设计决策档**，**不是端到端可运行的 DevTool 实现**。本任务进入 `/plan` → `/creative` 后即在 reflect/archive 收尾，是否触发 build 由用户基于产出 plan 拆出独立 build 任务决定。此判断与 V2=a 用户决策一致。

#### 范围（用户跳过 AskQuestion → 按 VAN 推荐默认锁定 V1-V5）

| # | 维度 | 选择 | 理由 |
|:-:|---|---|---|
| V1 | 子系统范围 | **B 三件套**：Inspector（DOM/Style/Layout）+ Hot Reload + Performance Overlay | DevTool 主线 ROI 最高 3 项；Console / JS Debugger / 完整 UI Editor 标为「扩展候选」入 spec §扩展段 |
| V2 | 输出形态 | **a 纯蓝图** — spec + plan + creative ×N，不强制实施代码 | 与「规划」语义最匹配；Level 4 多子系统蓝图任务在 build 前应有用户审视点 |
| V3 | UI 渲染层 | **A Veloxa 自渲染** — DevTool UI 自身用 Veloxa HTML/CSS 引擎 | dogfood 模式：DevTool 即 Veloxa 自我应用样板 + 反向暴露引擎缺陷；vs ImGui-like 多一套 UI lib / vs CDP 要求外置 browser 不能离线运行 |
| V4 | 复杂度 | **Level 4** | 多子系统蓝图 + 架构决策矩阵 + Spec 主交付 → 触发 `/plan` + `/creative` 强制路径 |
| V5 | 安全标注 | ✅ **是** | JS REPL 任意 eval / Hot Reload 路径穿越 / 远程调试 port 暴露 / Inspector 敏感数据回显 4 个威胁面 |

#### `/plan` 阶段决策矩阵 D1-D8（已锁定，brainstorming 完成 2026-05-01 ~01:50）

| # | 维度 | **锁定值** | 关联文档段 |
|:-:|---|---|---|
| **D1** | 三件套实施优先级 | **B** Inspector → Overlay → Hot Reload | spec §6 Phase 划分 + plan Phase A/B/C |
| **D2** | Inspector 数据采集协议 | **B** 半结构化（JSON tree + DisplayList overlay + C API JSON）| spec §5.3 数据流 + plan A.0.3-A.0.6 |
| **D3** | DevTool UI 主屏布局 | **B** 同窗口 splitter dock + Overlay HUD 子模式 | creative #1 §决策 1-5 |
| **D4** | DevTool 隔离边界 | **B** 单进程共享容器（双 Document + 共享 EventLoop / Application / ImageCache）| spec §5.1 模块边界 + plan A.0.1（I1 改造）|
| **D5** | Hot Reload file watcher + 增量策略 | **A** 嵌入式专注（Linux inotify + CSS-only 增量重载）| creative #2 §决策 1-5 + plan Phase C |
| **D6** | Performance Overlay 数据采集点 | **B** Chrome DevTools 风格（五钩子 + 滑动 60 帧 + dirty rect 边框高亮）| spec §5.1 perf_overlay + plan Phase B |
| **D7** | C API 扩展边界 | **C** 双层 API（内部 C++ 核心 + 公开 C API 薄封装）| spec §5.1 vx_devtool / vx_api + plan A.0.5/A.0.6 |
| **D8** | 安全威胁建模 | **A** T2/T3/T5/T6/T7/T8 完整 + T1/T4 扩展段占位 | spec §7 + plan Phase A.2 / B.3 / C.5 |

#### VAN 阶段实证（F1-F9）— 基础设施成熟度评估

| # | 假设/命题 | grep / read 实证 | 影响蓝图 |
|:-:|---|---|:-:|
| F1 | C API 是否已暴露 DevTool introspection 接口 | ⚠️ 部分 — `veloxa/api/veloxa_api.h` 仅运行时接口（vx_view_inject_input / vx_view_run），**无** DOM tree dump / style query / layout box 读取；属技术债 #40 | 🟡 需扩展 — Inspector 需新增 vx_view_serialize_dom / vx_node_get_computed_style / vx_node_get_layout_box 等只读 API |
| F2 | DOM 序列化能力 | ✅ 已就绪 — `veloxa/core/dom/serializer.h::Serialize(const Node*)`，DomBindings outerHTML 已使用 | 🟢 Inspector DOM 面板可直接复用 |
| F3 | UpdateManager frame 钩子 | ❌ 未导出 — TASK-26-01 R3 archive 提到「frame hook 未抽象」，属技术债 #35 | 🟡 需扩展 — Overlay 需 UpdateManager 加 OnFrameStart/OnFrameEnd callback 注册 |
| F4 | LayoutBox::Dump 调试方法 | ❌ 不存在 — `veloxa/core/layout/layout_box.h` 无 dump/print/inspect API；属技术债 #26 | 🔴 需新建 — Inspector Layout 面板第一前置 |
| F5 | QuickJS Interrupt Handler 调试钩子 | ⚠️ 部分 — TASK-13/19-01 提到 JS_SetInterruptHandler 已暴露但未对接 DevTool；属技术债 #44 | 🟡 需扩展（仅 V1=B 扩展候选 JS Debugger 才需，三件套不阻塞）|
| F6 | SDL2 后端是否就绪用于自渲染 | ✅ 已就绪 — TASK-25-01 已归档；Sdl2WindowSurface + Sdl2EventLoop 可见窗口 + 输入事件三阶段冒泡 | 🟢 V3=A 自渲染方案技术前置已满足 |
| F7 | EventManager 事件注入 | ✅ 已就绪 — TASK-25-01 后 vx_view_inject_input 可注入 SDL 事件 | 🟢 Inspector hover-to-select 可复用 |
| F8 | Hot Reload 文件 watch 基础 | ❌ 全代码库无 inotify/kqueue/FindFirstChangeNotification 调用 | 🔴 需新建 — 跨平台 file watcher 抽象（platform/ 子系统）|
| F9 | FetchContent 状态 / 第三方依赖 | ✅ 三处 `_deps/` 离线齐全；DevTool 三件套预计零新 FetchContent 依赖 | 🟢 跳过 git proxy 守卫 |

**汇总：** 5 ✅ 已就绪 / 4 ⚠️ 需扩展 / 6 🔴 需新建 — 其中 4 项触及既有技术债 #26 / #35 / #40 / #44，本任务规划即闭环这些债项的 ROI 路径

#### 触及技术债映射（与 `techContext.md §技术债务清单` 对照）

| # | 技术债 | DevTool 子系统 | 闭环节奏 |
|:-:|---|---|---|
| #26 | LayoutBox.Dump 调试方法缺失 | Inspector Layout 面板 | 本任务 plan 内规划，build 阶段实施（若推进至 build）|
| #35 | UpdateManager 未暴露 frame hook | Performance Overlay | 同 #26 |
| #40 | C API 缺 DOM/Style/Layout introspection | Inspector 全子系统 | 同 #26 |
| #44 | QuickJS Debug API 未对接 DevTool | 扩展候选 JS Debugger | V1=B 扩展段，不阻塞主交付 |

#### 验收要点（待 `/plan` 精化）

- spec 主文档落盘 12 段式样：目的 / 不做 / 三件套验收 A1-A12（每件套 3-4 acceptance）/ D1-D6 决策矩阵 / 架构图 + 注入点核对表 / T1-T8 安全威胁建模 / 扩展候选段（Console / JS Debugger / 完整 UI Editor）/ 与既有 systemPatterns 兼容性段 / R1-R6 风险登记 / 与未来任务关系
- plan 主文档落盘：Phase 划分 + 估时 plan ×0.6（预期 Level 4 蓝图任务落 0.5-0.7×）
- ≥ 2 篇 creative 设计决策档（预期：DevTool UI 主屏布局 / Inspector 数据采集协议层）
- T1-T8 安全威胁建模完整：JS REPL 隔离协议 / Hot Reload 路径 sanitize / 远程调试 port 暴露策略（含 default-off + 仅 loopback）/ Inspector 敏感数据 redact 策略
- 与既有 systemPatterns ≥ 30 模式对照（DevTool 是 Veloxa 自我应用，多个既有规则需自我验证 ROI）
- 与既有技术债 #26 / #35 / #40 / #44 闭环路径明确（每项标本任务规划 vs 后续 build 任务实施）

#### 前置验证清单（VAN 阶段产出）

| 维度 | 结果 | 备注 |
|---|:-:|---|
| 依赖可获取性 | ✅ | F9 — 三处 `_deps/` 离线齐全；DevTool 三件套预计零新 FetchContent 依赖 |
| 环境就绪 | ✅ | `build/` + `build-bench/` + SDL2 后端可见窗口（TASK-25-01 验证）已就绪 |
| 已有 artifact | ✅ | `systemPatterns.md` ≥ 30 模式 + 30+ archive + DOM serializer / EventManager / Sdl2WindowSurface 全部已就绪可复用 |
| ctest 基线 | ✅ | 隐式 1062/1062 PASS（main `2445990` 终态继承）|
| FetchContent 代理守卫 | ⊘ 跳过 | F9 — 三处 `_deps/` 离线预置；本任务零新 FetchContent 依赖 |
| 待处理事项关联 | ✅ 极强 | 触及技术债 4 项（#26 / #35 / #40 / #44）闭环 ROI 路径 + TASK-25-01 SDL2 前置依赖明确 + TASK-30-03 R3+ 13 项部分间接相关 |

#### VAN 阶段 push-back 决策（已沉淀）

| 风险 | 应对 |
|---|---|
| 「6 个 DevTool 子系统全做」陷阱 | V1=B 三件套范围收敛 — Inspector + Hot Reload + Overlay 主交付；Console / JS Debugger / 完整 UI Editor 入 spec §扩展段 |
| 「规划」语义模糊（用户可能期望端到端实施）| V2=a 纯蓝图主交付明确 — 在 reflect/archive 收尾，是否触发 build 由用户决定 |
| UI 渲染层选型分歧（自渲染 vs ImGui-like vs CDP）| V3=A 自渲染锁定 — dogfood 路径 ROI 最高 + 与既有 SDL2 后端无缝衔接；外置协议接口留作扩展段 discussion |
| Spec 主文档过长（违反 TASK-30-02「Spec 描述粒度准则」）| 12 段式样规划 + 扩展候选独立段 + 复杂决策拆出 creative 文档 |
| 与既有 systemPatterns 重叠（DevTool 是 Veloxa 自我应用）| spec 阶段强制对照 ≥ 30 模式 — 自我验证 ROI；凡命中既有规则的标「✅ 自我应用样板」 |

#### 与并发任务关系

- **TASK-20260430-03**（codebase review）：✅ 已归档（2026-05-01 ~00:30），main 已 `--no-ff` 合入（merge commit `2445990`）；本任务在新 main 上独立演进，零 git 依赖
- **TASK-20260430-03 R3+ 13 项任务建议**：用户决策 R3+ 拆分顺序仍待定；与本任务在 spec 阶段做交叉引用（Inspector 需稳定 lifecycle hook → R3+ #2 EventDispatcher snapshot iteration / #3 LoadHTML reset dom_bindings_ 是间接前置）
- **TASK-20260425-01 SDL2 后端**：✅ 已归档（2026-04-26）；本任务 V3=A 自渲染方案的关键技术前置（Sdl2WindowSurface / Sdl2EventLoop 直接复用）

#### 任务历史

| 时间 | 阶段 | 备注 |
|---|---|---|
| 2026-04-30 22:55 | 初始化（首次尝试）| 主线试图在 main `9411584` 创建 04 分支并写 MB 三件套，与 background agent 03 任务并发修改同名 MB 文件冲突；多次 StrReplace 失败 |
| 2026-04-30 23:00 | 重启初始化 | TASK-30-03 background agent 完成 R2 + reflect + archive 并 merge main（`2445990`），04 分支 reset 到新 main 重新基线；放弃旧 VAN commit `44fc062` |
| 2026-04-30 23:05 | VAN 完成 | 用户跳过 AskQuestion → 按 VAN 推荐默认锁定 V1-V5（B/a/A/4/✅）；F1-F9 grep 实证完成（5 ✅ / 4 ⚠️ / 6 🔴）；触及技术债 4 项映射；前置验证全 PASS；分支 `feature/TASK-20260430-04-ui-editor-debugger` 基于 main `2445990` 已建立；下一步 `/plan` |
| 2026-05-01 01:01 | VAN commit | `b33d86f` — VAN 完成 commit 落地（含 MB 三件套同步）|
| 2026-05-01 01:50 | `/plan` 完成 | brainstorming D1-D8 全部锁定（用户连续 8 次跳过 AskQuestion 后按 VAN 推荐默认）；4 篇文档落盘（spec / plan / 2 creative）；下一步路径三选一：A 进入 `/reflect`（推荐）/ B 改 V2 → b 进 `/build` / C 暂停审查 |
| 2026-05-01 02:00 | `/plan` commit | `5b802b5` — `/plan` 阶段一次性 commit 全部 4 篇文档 + 3 MB 同步 |
| 2026-05-01 02:30 | `/reflect` 完成 | reflection 文档落盘（`memory-bank/reflection/reflection-TASK-20260430-04.md` — 10 节 / Level 4 含架构评估 + 长期影响）；plan ×0.6 第 17 数据点入库 0.27-0.35× plan / 0.46-0.59× plan ×0.6（极窄档 + review 类下限交界）；10 项改进建议 P0×3 / P1×4 / P2×3；P0 全部 + P1 第 1 项已落实（main.mdc V2=a 工作流变体段 / systemPatterns 极窄档第 17 数据点 + Level 4 蓝图 V2=a 工作流变体段 / activeContext 7 项独立立项候选迁移）；下一步进入 `/archive` |
| 2026-05-01 03:00 | `/archive` 完成 | archive 文档落盘（`memory-bank/archive/archive-TASK-20260430-04.md`）+ 剩余 P1/P2 改进建议落实（P1 #2 brainstorming 协同度段 / P1 #6 techContext 蓝图主交付摘要 / P1 #8 R3+ 强依赖交叉记录到 codebase-review F-025 / P2 #5 systemPatterns dogfood 路径段 / P2 #9 brainstorming 决策跳过率监控段；P2 #10 留 TASK-30-04-A/B/C 立项后回填）；MB 三件套重置为空闲状态；feature 分支 `--no-ff` 合入 main；任务闭环 ✅ |

---

</details>

<details>
<summary>TASK-20260430-03：全代码库 Code Review（6 维度 × 7 子系统 + 多轮次 Build + Checkpoint）[安全相关] — ✅ 已归档（点开查看历史）</summary>

### TASK-20260430-03：全代码库 Code Review（6 维度 × 7 子系统 + 多轮次 Build + Checkpoint）[安全相关]

- **复杂度级别：** Level 4（多子系统横扫 + 6 维度全覆盖 + 多轮次必然 + 不可估时上限拆 R3+ 后续任务消化）
- **状态：** ✅ 已归档（2026-05-01）— 归档文档 `memory-bank/archive/archive-TASK-20260430-03.md`；reflection 10 项改进建议落实率 90%（P0 1/1 + P1 4/4 + P2 4/5）；R0+R1+R2 三轮次完成 ctest 1062/1062 PASS（基线 1061 + R2.5 新增守卫单测）；55 项 findings（28 P1 + 19 P2 + 8 P3）+ 13 R3+ 拆分任务建议入仓；plan ×0.6 第 16 数据点入库（核心轮次 0.85-1.00× ×0.6 阈内 ✅）；首发 background agent 双轨模式 + worktree 隔离协议沉淀
- **创建日期：** 2026-04-30
- **分支：** `feature/TASK-20260430-03-codebase-review`（基于 main `9411584`，已创建）
- **设计 spec：** `docs/specs/2026-04-30-codebase-review-design.md`（12 段 / D1-D10 决策矩阵 / T7-T10 安全威胁建模 + R0/R1/R2 多轮次 + Checkpoint 协议）
- **实现 plan：** `docs/plans/2026-04-30-codebase-review.md`（10 段 / R0 + R1.1-R1.4 + R2 + Checkpoint 1/2 / plan ~6-10h / plan×0.6 ~3.6-6h / 7-17 commits）
- **需要创意阶段：** ❌ 否（review 报告本身是产出物，无 UI/算法/架构空白）
- **来源：** 用户主动发起；历史候选区累积 8 项 P3 触发型不足项作为 review 输入材料（`activeContext.md §待处理事项` + `tasks.md §待立项候选`）
- **安全相关：** ✅ 是（V2 涵盖 security 维度 — HTML inline style 三件套护栏 / CSS parser DoS 护栏 / `BasicString`-Werror 误报 / 第三方依赖 CVE 周期审计 review）

#### 范围（用户决策 V1-V4 锁定 + 策略 X 多轮次 Checkpoint）

| # | 维度 | 选择 | 理由 |
|:-:|---|---|---|
| V1 | Review 范围 | **A 全代码库**（136 源 + 80 测试，7 子系统：foundation / core / graphics / text / script / platform / api） | 用户决策；与 30+ 已归档任务的子系统级聚焦互补，找规则未覆盖盲点 |
| V2 | Review 维度 | **all 6 维**（性能 / 正确性 / 可维护性 / 安全 / 测试 / 一致性） | 用户决策；6 维度交叉互证可暴露单维 review 不易发现的反模式 |
| V3 | 输出形态 | **C 完整实施**（解读为多轮次 Build + 强制 Checkpoint） | 用户决策；本任务保证 R0+R1（报告必然），R2/R3+ 由用户审报告后按 ROI 决定 |
| V4 | Review 视角 | **D 混合**（外部 reviewer 直觉视角 + 内部 systemPatterns 规则验证视角） | 用户决策；外部视角找命名 / 反直觉 / 直观盲点 + 内部视角找规则未覆盖死角 |
| V5 | 安全标注 | ✅ **是** | V2 含 security 维度 + 触及既有 HTML/CSS 护栏 + 第三方依赖 CVE 审计 |
| 策略 X | 多轮次 Checkpoint | **R0+R1 必然 / R2 条件触发 / R3+ 拆出独立后续任务** | VAN push back 推荐；把不可估的修复成本关进 R3+，避免本任务跨多天误工 |

#### 多轮次 Build 划分（VAN 阶段锁定）

| Round | 内容 | 必然 / 条件 | 类型 | plan 估时（待精化）| 实测 |
|:-:|---|:-:|---|---:|---:|
| R0 | 准备：6 维度 grep fingerprint 反模式预硕 + 抽样深度策略锁定 + ctest 基线 1061/1061 核验 | ✅ 必然 | meta | ~90 min plan / ~54 min ×0.6 | **~22 min ✅ (0.41× plan)** |
| R1 | **6 维度全代码库 review 报告** — spec 主文档列出 N 项不足 + 每项方案 + P0/P1/P2 分类 | ✅ 必然 | report | ~150-200 min plan / ~90-120 min ×0.6 | **~85 min ✅ (0.50× plan / 0.78× ×0.6)** |
| **Checkpoint 1** | 用户审报告 + 决定 R2 范围（选项 A：全 6 项 quick fix）| — | — | — | ✅ A |
| R2 | P0 quick fix（F-020 / F-026 / F-033 / F-040 / F-053 / F-055，全 6 项）| ✅ 触发 | impl | ~55 min plan / ~33 min ×0.6 | **~70 min ✅ (1.27× plan / 2.12× ×0.6, +25 min worktree 隔离修并发会话冲突)** |
| **Checkpoint 2** | 用户决定 R3+ 范围 / 拆出后续任务 ID（每个独立 Level 1-2）| — | — | — | ⏳ |
| R3+ | P1 大件修复 → **不在本任务内完成**；按 ROI 拆出独立子任务 TASK-* | ❌ 拆出 | — | — | — |
| **合计上限** | R0+R1+R2 | — | — | **~5-8h plan / ~3-5h plan×0.6 实测预期** | — |

#### BUILD R0 完成实绩（2026-04-30 23:08）

- **产出文档：** `docs/reports/2026-04-30-codebase-review-r0-data.md`（R0 全部数据综合快照）
- **R0.1 ctest 基线 ✅：** 1061/1061 PASS in 2.36s（1 Skip Wpt001 沉淀）
- **R0.2 fingerprint grep ✅：** 18 项反模式覆盖 30 关键字 / 7 子系统 → veloxa/ 全代码库 0 TODO/FIXME/XXX/HACK + 0 危险 C 函数 + 0 throw + 0 sscanf/atoi 不安全转换 + 0 STL 重容器
- **R0.3 lcov 覆盖率 ✅：** 行 **85.4%** (6152/7206) / 函数 **95.4%** (1620/1699) / 分支 **57.6%** (3905/6775)；薄弱模块 12 项（image_decoder 57.1% / rasterizer 60.4% / canvas 73.5% 是热点）
- **R0.4 CVE 审计 ✅：** **0 CRITICAL/HIGH** 满足 D9 acceptance；5 Medium/Low（**libpng 1.6.37 命中 3 个 2026 新公布 Medium CVE 是关键发现**，与 image_decoder.cc 57.1% 覆盖薄弱形成威胁链路 → 候选 P0 F-010）
- **R0.5 抽样名单 ✅：** R1 三层抽样确认（H 20 / M 80 / L 36）
- **R0 候选 findings：** 14 项（F-001 ~ F-014 跨 6 维度），R1 验证 + 分级 + 写方案
- **实测耗时：** ~22 min（plan 90 min ×0.6=54 min；实测 0.41× plan / 0.69× ×0.6） → reflect 阶段 plan ×0.6 校准协议第 16 数据点
- **工具链补强：** OSV.dev 走 WSL2 → Windows Clash 代理 `172.22.32.1:7890`（沙箱直连 DNS 失败 → 宿主代理 fallback）；候选 systemPattern reflect 阶段沉淀

#### BUILD R2 完成实绩（2026-04-30 ~24:55）

- **R2 commit 链：** 6 项 P0 quick fix 全过 ctest **1062/1062 PASS**（基线 1061 + R2.5 新增 1 测）
  - R2.1 `3b4b2e7` fix(css): F-020 selector_matcher 末尾 dead return + VX_DCHECK 标 unreachable
  - R2.2 `1467207` fix(html): F-033 ProcessEndTag isize/usize 重复转换净化 + early-return 防 underflow
  - R2.3 `ddea78d` docs(rasterizer): F-040 FlattenQuad/Cubic 0.0625 vs 0.25 阈值数学等价注释
  - R2.4 `95ae814` fix(layout): F-026 LayoutEngine 一参 arena static → thread_local 改善线程安全
  - R2.5 `9c6ad5f` fix(image): F-053 DecodeFromFile 加 max_size 守卫（默认 256 MiB） + 单测 DecodeFromFileRejectsOversizedFile（StatusCode::kOutOfMemory）
  - R2.6 `668a9fe` fix(api): F-055 vx_version() 改 CMake configure_file（version.h.in → @PROJECT_VERSION@）消除 hardcode
- **执行环境：** 独立 git worktree `.worktree-03-r2` 隔离主 worktree 的并发会话分支切换冲突（reflog 见 23:41:23-30 / 23:45:08 双切）；worktree 完成后清理
- **新增文件：** `veloxa/api/version.h.in`（CMake 模板）
- **修改文件：** 6 文件（5 src + 1 CMakeLists + 1 test）
- **实测耗时：** ~70 min（plan 55 min ×0.6=33 min；实测 1.27× plan / 2.12× ×0.6） → 超 ×0.6 主因 worktree 隔离 + 并发会话冲突修复 ~25 min 额外开销，扣除后 ~45 min ≈ 0.82× plan / 1.36× ×0.6 (仍超但合理)
- **跨任务沉淀候选**（reflect 阶段决定）：
  - 「并发会话切分支冲突」应对模式 → systemPatterns 新模式
  - background agent 双轨 + worktree 隔离 → workflow rule 新段
  - 6 项 fix 平均 ~12 min/项（plan ~9 min/项），plan ×0.6 fast-fix 颗粒度 ~6 min/项校准点

#### BUILD R1 完成实绩（2026-04-30 ~24:39）

- **产出文档：** `docs/reports/2026-04-30-codebase-review.md`（11 段 / 55 项 findings / 6 维度归集）
- **R1.1 H 层深读 ✅：** 实际深读 25+ 文件（H 计划 20，含附带 H 头文件 application.h / event_manager.h / node.h / element.h / dom_bindings.h / update_manager.h 等）
- **R1.2 M 层 grep 一过 ✅：** static / memcpy / magic numbers / delete 4 模式扫描漏网 P0/P1（实际 grep 命中 5 个 delete 全部合理 — 验证嵌入式 arena 策略实施完美）
- **R1.3 6 维度归集 ✅：** 55 项 findings 跨 7 子系统：
    - CSS（11 项）：F-015 命名颜色 sorted assertion / F-017 sibling combinator / F-018 nth/not pseudo / F-019 attr ops / F-020 dead return / F-021 border 复制粘贴 80 行 / F-022 **CSS 元数据表缺失（最大杠杆点）** / F-023 transition lifecycle / F-024 kInitial 仅 5 / F-025 LoadHTML use-after-free
    - Layout（5 项）：F-026 static arena multi-thread / F-029 root_incoming 边角 / F-030 inline available / F-031 absolute right/bottom / F-032 LOC split
    - DOM/HTML/App（9 项）：F-001 / F-033 / F-034 / F-035 实体表过简 / F-036 / F-037 / F-038 / F-039 RemoveChild lifecycle hook
    - Rendering（5 项）：F-040 / F-041 / F-042 SIMD / F-043 brush hoist / F-044 dynamic_cast
    - Script/Event/Update（4 项）：F-045 / F-046 **dispatch mutation iterator 失效（真 bug）** / F-047 / F-048
    - Foundation/Text/Image/API（11 项）：F-049 PNG alpha 丢失 / F-050 width×height 溢出 / F-051 JPEG default exit / F-052 / F-053 文件大小上限 / F-054 / F-055 / F-056-057 typikum 实现
- **R1.4 P0/P1/P2 分级 ✅：** 28 个 P1 真 bug + 19 P2 + 8 P3 沉淀；**0 P0** （quick fix 标准 < 30 min/项 1 行修复）
- **R1.5 报告落盘 ✅：** `docs/reports/2026-04-30-codebase-review.md` 11 段，含 Top 5 紧急修 + R2 6 项候选（F-020/F-026/F-033/F-040/F-053/F-055，55 min 估）+ R3+ 13 拆分任务建议
- **实测耗时：** ~85 min（plan 150-200 min ×0.6=90-120 min；实测 0.50× plan / 0.78× ×0.6）→ reflect 阶段 plan ×0.6 校准协议第 16+ 数据点

#### Checkpoint 1（用户决策待启动）

- 用户审报告 → 决定 R2 范围（执行/限定/跳过 6 项 quick fix）
- 用户决定 R3+ 拆分顺序（13 个 P1 候选任务 ID）
- 决策后：
    - 批 R2 → `/build` 触发 R2 quick fix 轮次
    - 跳 R2 → `/reflect` 进入回顾阶段

#### 验收要点（待 `/plan` 精化）

- R0 末：6 维度 fingerprint 完整产出（每维度 ≥ 5 反模式 grep 结果）+ 抽样深度策略明确（哪些子系统全扫 / 哪些抽样）
- R1 末：spec 主文档落盘 `docs/specs/2026-04-30-codebase-review-report.md`，含：
  - 不足清单 ≥ N 项（N 待 R0 fingerprint 后估算，预期 20-50 项）
  - 每项必含：定位（文件:行）/ 维度归类 / 反模式描述 / 影响评估 / 解决方案 / 优先级（P0/P1/P2）
  - 分类汇总表 + ROI 矩阵
  - 与既有 systemPatterns 规则对照（哪些违反、哪些规则需新增）
- R2（条件触发）：每 P0 修复独立 commit + 单测覆盖 + RED 反向探针（§9.3）+ ctest 1061+ → 1061+N PASS
- Release `-O3 -Werror` 0 err/warn（继承基线）
- `bench_*` baseline 不退化超 +5%（继承基线 — R2 不应触发 hot path 性能改动）
- 与 TASK-26-01 R2 #28 / TASK-30-02 既有护栏兼容性验证（V2 安全维度强制项）

#### 前置验证清单（VAN 阶段产出）

| 维度 | 结果 | 备注 |
|---|:-:|---|
| 依赖可获取性 | ✅ | 无新依赖（纯 review + 修复，复用既有 vx_* 子系统）|
| 环境就绪 | ✅ | `build/`(Debug 1061/1061) + `build-bench/`(Release 1030/1030) + `build-release/` 三份 `_deps/` 离线均存在可复用 |
| 已有 artifact | ✅ | `systemPatterns.md` ≥ 30 模式 + 30+ archive 文档 + 8 项 P3 候选作为 review 输入材料 |
| ctest 基线 | ✅ | 隐式 1061/1061 PASS（TASK-30-02 终态继承）|
| FetchContent 代理守卫 | ⊘ 跳过 | `_deps/` 已离线预置三处；`git config --global http.proxy` 空（exit=1）|
| 待处理事项关联 | ✅ 极强 | 8 项 P3 候选 + 30+ archive 改进建议未追踪状态全部进入 R1 输入材料 |

#### VAN 阶段 push-back 决策（已沉淀）

| 风险 | 应对 |
|---|---|
| 「样样不深」陷阱（6 × 7 × 136 = 5712 单元格）| R0 抽样深度策略：每维度全扫 grep fingerprint + 每子系统挑 1-2 个最深的代码点深扫 |
| 不可估时（修复数量取决于 review 发现）| 强制 R3+ 拆出独立后续任务，**本任务上限封顶 R0+R1+R2**（plan ~5-8h）|
| Checkpoint 缺失（直接「完整实施」无中途审视）| Checkpoint 1（R1 末用户审报告）+ Checkpoint 2（R2 末用户决定 R3+ 拆分）|
| Spec 主文档过长（违反 TASK-30-02「Spec 描述粒度准则」）| R1 spec 拆分：主文档列「定位 + 优先级 + 简述」/ 复杂项独立附录 |
| 与既有 systemPatterns 规则重叠 | R1 必须对照 systemPatterns ≥ 30 模式 — 已被规则覆盖的不再列入「不足」，避免噪音 |

#### 来源待立项候选清单（R1 输入材料）

| 候选 ID | 优先级 | 描述 |
|---|:-:|---|
| TASK-26-02-full | P3 | clearance 完整版（依赖 float/clear，需独立 Level 4）|
| TASK-26-03 | P3 | LayoutInline 内部 IFC 递归 + bidi LTR 假设破除 |
| TASK-20260424-02 | P3 | Layout 残余 super-linear 调查（R256 4.18× / R_flex 6.40× ~40% 未解）|
| CSS 4 逻辑属性 shorthand | P3 | `border-block` / `border-inline` |
| `border-image` / `border-radius` 简写 | P3 | 富装饰需求触发 |
| TASK-20260419-06 | P3 降级 | HashMap Hash Mixing 优化 |
| TASK-20260419-08 | P3 | `string.h` 剩余 3 处 runtime-size memcpy 防御性 noinline 化 |
| TASK-20260419-12 | P2 | `SoftwareCanvas::DrawText` 真路径优化（K7 已被 TASK-24-04 隐式闭环，待评估）|

#### PLAN 阶段决策（已锁定）

| # | 维度 | 选择 | 理由 |
|:-:|---|---|---|
| D1 | R0 抽样深度策略 | **B 优先深扫 TOP-3** — foundation/{containers,strings} + script/ + core/{dom,html} 全文件深扫；其他 4 子系统 grep 模式 | 30+ archive 任务后多数子系统已经过深度沉淀；TOP-3 选项是「最久未深度审视」的，深扫 ROI 最高 |
| D2 | R1 报告章节组织 | **C 矩阵 + 优先级前置** | 先列 P0/P1/P2 总表方便用户审 Checkpoint 1 决策 R2 范围 |
| D3 | 不足项「方案」深度 | **B 结构化描述**（问题 + 方向 + 工时 + 代码量，不含具体代码示例） | 报告量级可控；R3+ 拆出独立 task 时再写完整 spec |
| D4 | 与既有 systemPatterns 对照 | **B 强制每项标注 3 状态** | 避免与已沉淀规则重叠；显化「需新增规则」候选 |
| D5 | P0/P1/P2 分类标准 | spec §6 显式定义 | 必须显式定义避免歧义 |
| D6 | R2 上限 | **15 个 P0 上限** | Checkpoint 1 用户决定实际启动数量 |
| D7 | 元规则同步 review | **C 仅记长期项** | scope 收紧；元规则修改需用户亲自决策 |
| D8 | CVE 审计 | **C 跑 + 列长期项** | 本任务一次性给出当前状态 + 长期 audit 节奏 |
| D9 | 测试覆盖率深度 | **B lcov 实测**（line / branch / function 三维度） | 真实数据 vs grep 启发；lcov 1.14 已验证可用 |
| D10 | 是否用子代理 | **❌ 不用**（独 agent 跑） | 单 reviewer 任务 prompt 上下文传递成本 > 并行收益 |

#### PLAN 阶段安全威胁建模（V5 安全相关，T1-T10）

| # | 威胁面 | 既有护栏 | 本 review 关注 |
|:-:|---|---|---|
| T1-T5 | HTML inline style DoS / 历史攻击向量 | TASK-26-01 R2 三件套护栏 | 仅复检不动 |
| T6 | CSS parser N-cap | TASK-30-02 已批量 review | 仅复检 |
| **T7** | **QuickJS 集成** | TASK-18-01 + TASK-19-01 部分 | 🔴 D1 TOP-3 含 script/ 重点深扫 |
| **T8** | **FreeType / HarfBuzz 集成** | 部分隐式 | 🟡 抽样深扫候选 |
| **T9** | **图片解码（libpng / libjpeg）** | 当前未审 | 🟡 抽样深扫候选 |
| T10 | 第三方依赖 CVE | 当前未跑 | 🟡 D8=C 本任务跑 |

#### 工具链验证（PLAN 阶段产出）

| 工具 | 版本 | 状态 |
|---|---|---|
| `lcov` | 1.14 | ✅ 已装（用户确认）|
| `gcov` | 11.4 | ✅ 已装（gcc 11.4 自带）|
| `genhtml` | 1.14 | ✅ 已装 |
| `rg`(ripgrep) | — | 待 R0 验证 |
| `gh`(GitHub CLI) | — | 待 R0 验证（需 full_network for CVE 审计）|
| FetchContent 离线 | — | ✅ 三处 `_deps/`（quickjs-ng v0.14.0 + benchmark v1.9.1）|
| ctest 基线 | 1061/1061 | 待 R0.1 核验 |

#### 任务历史

| 时间 | 阶段 | 备注 |
|---|---|---|
| 2026-04-30 22:24 | 初始化 | VAN 完成；用户决策 V1-V5（A / all / C / D / ✅）+ 策略 X（R0+R1 必然 + R2 条件 + R3+ 拆出）锁定；分支创建（基于 main `9411584`）；Memory Bank 同步；下一步 `/plan` |
| 2026-04-30 22:42 | 规划完成 | PLAN 完成；10 决策矩阵 D1-D10 锁定（B/C/B/B/spec/15/C/C/B/❌不用子代理）；spec + plan 文档落盘；T1-T10 安全威胁建模（T7 QuickJS / T8 FreeType / T9 图片解码 + T10 CVE 为重点）；估时 plan ~6-10h / plan×0.6 ~3.6-6h；不需要 `/creative`；下一步 `/build` 进入 R0 准备阶段 |
| 2026-04-30 23:08 | R0 完成 | grep fingerprint 6 维度 × 30 关键字扫描 / lcov 覆盖率 85.4%/95.4%/57.6% / 7 依赖 CVE 审计 0 CRIT-HIGH + 5 Med-Low / 抽样名单 H 25+ + M 80 + L 36 / R0 报告落盘；实测 ~22 min（plan ×0.6 0.69×）⚡ |
| 2026-04-30 24:39 | R1 完成 | 6 维度全代码库 review 报告产出（934 行 / 55 项 findings / 28 P1 + 19 P2 + 8 P3）；Top 5 priority + R2 候选 6 项 + R3+ 13 拆分任务建议；commit `802a273`；实测 ~85 min（plan ×0.6 0.78×）⚡ |
| 2026-04-30 24:55 | R2 完成 | 6 项 P0 quick fix 全过 ctest 1062/1062 PASS（基线 1061 + R2.5 新单测）；commits `3b4b2e7`/`1467207`/`ddea78d`/`95ae814`/`9c6ad5f`/`668a9fe` + MB 收尾 `33c99f4`；首次实战 background agent 双轨模式（worktree 隔离应对 race condition）；实测 ~70 min（含冲突 1.27× plan，扣冲突 0.82×）|
| 2026-05-01 00:08 | 回顾完成 | reflection 文档 10 段 + 2 附录（Level 4 全面回顾）；plan ×0.6 第 16 数据点入库（核心轮次 0.85-1.00× ×0.6 阈内 ✅）；10 改进建议（P0×1 / P1×4 / P2×5）；systemPatterns 新增 5 段 + techContext 新增 3 段；commit `77bec3c` |
| 2026-05-01 00:30 | 归档完成 | archive 文档 10 段；P0 #3 git symbolic-ref commit 守门 → `git-workflow.mdc` 落实；P1 #4 reflog 诊断 → `systematic-debugging.mdc` 落实；R3+ 13 项任务建议入 activeContext 待 Checkpoint 2 用户决策；分支保留未合并（background agent 双轨产物，由用户决定合并时机）|

</details>

---

<details>
<summary>TASK-20260430-02：CSS border shorthand 补全（4 方向 + 3 属性级）[安全相关] — ✅ 已归档（点开查看历史）</summary>

### TASK-20260430-02：CSS border shorthand 补全（4 方向 + 3 属性级）[安全相关]

- **复杂度级别：** Level 2（多文件修改 + 需求清晰 + 模式 100% 复用既有 `border` / `padding` shorthand 范本，无架构/UI/算法空白）
- **状态：** ✅ 已归档（2026-04-30）— 归档文档 `memory-bank/archive/archive-TASK-20260430-02.md`；3 改进建议全部闭环（P1 #1 + P2 #2 systemPatterns.md 落实 / #3 评估）；A8 ROI 验证 ✅；ctest Debug 1061/1061 + Release 1030/1030 + Release `-O3 -Werror` 0 err/warn；plan × 0.6 第 15 数据点 0.22× 与 TASK-30-01 P6（0.21×）一同定型「极窄档 0.2-0.25×」
- **创建日期：** 2026-04-30
- **分支：** `feature/TASK-20260430-02-css-border-shorthand`（基于 main `6b36c87`，已创建）
- **设计 spec：** `docs/specs/2026-04-30-css-border-shorthand-design.md`（11 段：目的 / 不做 / A1-A8 验收 / D1-D5 决策矩阵 / 架构 + 实施伪码 / T1-T8 威胁建模 / 测试策略 D3 完整档 25 测试 / R1-R2 多轮次划分 / 6 风险登记 / 与既有任务关系）
- **实现 plan：** `docs/plans/2026-04-30-css-border-shorthand.md`（8 Phase / 25 测试 / 5 commits / plan ~170 min / plan×0.6 ~102 min 准确档预期）
- **需要创意阶段：** ❌ 否（5 决策 D1-D5 已锁定，无 UI/算法/架构空白；可直接 `/build`）
- **来源：** TASK-20260430-01 archive §改进建议「副发现 P3 触发型候选 — CSS parser `border-bottom` shorthand 缺失」直接落实；同时验证 TASK-30-01 升级规则 `.cursor/rules/skills/writing-plans.mdc` §0「CSS shorthand 能力 grep 表」的首次外部任务 ROI（自我应用：本任务自身是该规则的应用样板）
- **安全相关：** ✅ 是（CSS parser 内部 declaration 展开循环 N-cap + 值长度边界 + 与上游 HTML inline style 三件套护栏交互验证；威胁模型在 spec 阶段细化）

#### 范围（用户决策 V1=A 全量 7 shorthand）

| 类型 | shorthand | 1-4 值规则 | 展开成 |
|---|---|---|---|
| 方向（R1，4 个）| `border-top` / `border-right` / `border-bottom` / `border-left` | 单边 width/style/color 任意顺序（仿 `border`）| 3 longhand × 1 边 = 3 declaration |
| 属性级（R2，3 个）| `border-width` / `border-style` / `border-color` | 1-4 值（仿 `padding` / `margin`）| 4 longhand × 1 type = 4 declaration |

合计 7 个新 shorthand，全部展开为既有 12 longhand，零新 PropertyId / 零 enum 改动。

#### VAN 阶段决策（已锁定）

| # | 维度 | 选择 | 理由 |
|:-:|---|---|---|
| V1 | 范围 | **A** 全量 7 shorthand（4 方向 + 3 属性级）| 高内聚一次性补全；reviewer 分歧最小；下次同类问题不重复 |
| V2 | 拆分策略 | **多轮次 Build**：R1 = 4 方向 / R2 = 3 属性级 | 用户决策；R1 为 TASK-30-01 直接触发受益；R2 是与 padding/margin 完全同模式机械补全 |
| V3 | Git 分支 | `feature/TASK-20260430-02-css-border-shorthand` | 基于 main `6b36c87` |
| V4 | 复杂度 | **Level 2** | 多文件修改 + 模式 100% 复用 + 无架构空白；不需要 /creative |
| V5 | 安全标注 | ✅ **是** | CSS parser declaration 展开 N-cap 护栏 + 与上游 HTML inline style 三件套护栏交互验证 |

#### VAN 阶段代码实证（落实 P0「方案根因假设未先验证」+ TASK-30-01 升级规则「CSS shorthand 能力 grep 表」自我应用）

| # | 假设/命题 | grep 实证 | 影响设计 |
|:-:|---|---|---|
| F1 | `border` 总称 shorthand 当前是否支持？ | ✅ 是。`parser.cc:517-597` 整段 4-side expansion（width/style/color，每边一致）；`tests/core/css/parser_test.cc:319 BorderShorthand` 覆盖 12 declaration 展开 | 已有成熟范本可复刻 |
| F2 | `border-top/right/bottom/left` 4 方向 shorthand 是否支持？ | ❌ **完全无实现**。`property.cc:49-60` 只列 12 longhand；`PropertyIdFromName` 命中率 0；`ParseDeclaration` 无对应分支 | TASK-30-01 build 副发现完全确认 |
| F3 | `border-width/style/color` 3 属性级 shorthand 是否支持？ | ❌ **完全无实现**。同 F2 — 仅 longhand 入口，无 1-4 值展开（与 padding/margin 同模式）| 一并补全可大幅减少 reviewer 分歧 |
| F4 | PropertyId 枚举需扩展吗？ | ❌ **不需要**。所有新 shorthand 直接展开为现有 longhand 写入 declaration list | 零 ABI 风险 / 零跨子系统影响 |
| F5 | 现有测试 fingerprint 是否充足？ | ✅ `parser_test.cc` 含 `ShorthandPadding` (4-value) + `BorderShorthand` (12 expansion) 双范本；`property_test.cc`、`enum_serialization_test.cc:117` | 测试范本 100% 复用 |
| F6 | FetchContent 代理 / 离线状态 | ✅ `build/_deps/` + `build-bench/_deps/` 已含完整离线 | 跳过 proxy 守卫 |

#### 验收要点（待 /plan 精化）

- ctest 全量 PASS（基线 1039/1039；预计 +14-20 new test cases，每 shorthand 1-2 测试 + 2-3 反向探针 + 1-2 安全 N-cap 测试）
- Release `-O3 -Werror` 0 err/warn
- 双入口验证：`CssParser::Parse()`（stylesheet 路径）+ `CssParser::ParseDeclarationList()`（HTML inline style 路径）均覆盖
- DoS / 病态输入护栏：每 shorthand 复用既有 `for (int i = 0; i < 3; ++i)` / `for (usize i = 0; i < 4; ++i)` N-cap
- 与 TASK-26-01 R2 #28 上游 HTML inline style 三件套护栏（count cap 1000 / value cap 8KB / 黑名单）兼容性验证
- §9.3 反向探针 ≥ 1 处（D3 类强制）
- TASK-30-01 升级规则 §0「CSS shorthand 能力 grep 表」首次外部任务 ROI 自我验证

#### 前置验证清单

| 维度 | 结果 | 备注 |
|---|:-:|---|
| 依赖可获取性 | ✅ | 无新依赖（完全在 vx_core CSS 子系统） |
| 环境就绪 | ✅ | `build/`(Debug) + `build-bench/`(Release) 均存在可复用 |
| 已有 artifact | ✅ | parser.cc 现有 `border` shorthand 范本 + parser_test.cc 测试范本 100% 复用 |
| ctest 基线 | ✅ | 隐式 1039/1039 PASS（TASK-30-01 终态继承） |
| FetchContent 代理守卫 | ⊘ 跳过 | F6 离线 |
| 待处理事项关联 | ✅ | TASK-30-01 archive §10 P3 触发型候选直接落实 + 验证 §0 升级规则 ROI 自我应用 + plan × 0.6 第 15 数据点（Level 2 多轮次 + 安全相关）|

#### PLAN 阶段决策（已锁定）

| # | 维度 | 选择 | 理由 |
|:-:|---|---|---|
| D1 | 4 方向 shorthand 实现模式 | **A 复制粘贴** 既有 `border` 模板 4 次 | 与现有 `border` / `padding` / `flex` / `transition` 风格一致；reviewer 心智成本最低；helper 抽取的 30 LOC 收益不抵 template/lambda 引入新模式风险 |
| D2 | 3 属性级 shorthand 实现模式 | **A 复制粘贴** 既有 `padding/margin` 模板 3 次 | 同 D1；3 个 value parser（Length/Enum/Color）类型异构，统一 helper 反而需要类型擦除 |
| D3 | 测试深度 | **C 完整档**（25 测试：12 R1 + 13 R2）| V5 安全 + §9.3 反向探针强制；双入口验证防 ParseDeclarationList 路径退化；与既有 BorderShorthand 互斥不退化测试是 §0 fingerprint 自我应用 |
| D4 | 安全护栏复用策略 | **A 完全复用** 既有 N-cap (3-iter / 4-iter) | 已经过 TASK-26-01 R2 验证；上游 HTML inline style 三件套（count 1000 / value 8KB / 黑名单）覆盖威胁面 T1-T5；零新护栏需求 |
| D5 | R1/R2 Phase 划分粒度 | **A 2 GREEN commits**（R1 = 4 方向同时绿 / R2 = 3 属性级同时绿）| 4 shorthand 同模式可并发实施 1 commit；3 shorthand 同模式 1 commit；与 V2 多轮次决策一致 |

#### PLAN 阶段威胁建模（V5 安全相关，T1-T8）

| # | 威胁 | 攻击载体 | 护栏 |
|:-:|---|---|---|
| T1 | DoS via 海量 declaration | inline style 含数千个 shorthand | ✅ 上游 `kInlineStyleMaxDeclarationCount = 1000` |
| T2 | DoS via 单 value 巨长 | `style="border-width: 1234567...(8KB+)"` | ✅ 上游 `kInlineStyleMaxValueLength = 8KB` |
| T3-T5 | 历史攻击向量（expression/behavior/javascript:）| 注入到 border-color | ✅ 上游 `ContainsBlacklistKeyword` |
| T6 | parser 内部 token 循环耗尽 CPU（4 方向 shorthand）| `border-top: 1 1 1 1 ... ` | ⚠️ 复用既有 3-iter cap；`BorderTopShorthand_NCapSecurity` 专测 |
| T7 | over-match 误绑（`border-top-width` 进 shorthand 路径）| longhand 抢匹配 | ✅ `EqualsIgnoreCase` 严格相等天然防御；`BorderTopShorthand_LonghandFallthrough` 专测 |
| T8 | 4-value 解析下界绕过 | `border-width: 1 2 3 4 5 6 ...` | ⚠️ 复用既有 4-iter cap；`BorderWidthShorthand_NCapSecurity` 专测 |

**结论**：威胁面被既有上游护栏 + 既有 parser 内部 N-cap 完整覆盖；零新护栏需求。

#### 任务历史

| 时间 | 阶段 | 备注 |
|---|---|---|
| 2026-04-30 21:50 | 初始化 | VAN 完成；6 项 grep 实证（F1-F6）；用户决策 V1-V5 锁定（V1=A 全量 7 / V2=多轮次 / V4=Level 2 / V5=✅ 安全）；分支创建（基于 main `6b36c87`）；MB 同步；下一步 `/plan` |
| 2026-04-30 21:55 | 规划完成 | PLAN 完成；5 决策矩阵 D1-D5 锁定（A/A/C/A/A）；spec + plan 文档落盘；威胁建模 T1-T8 完整；R1（4 方向）+ R2（3 属性级）多轮次划分；估时 ~170 min plan / ~102 min plan×0.6；不需要 `/creative`；下一步 `/build` |
| 2026-04-30 22:05 | 构建中·R1 完成 | R1 TDD 闭环：P0 grep G1-G6 ✅ → R1.1 RED 9 FAIL + 1 sentinel PASS → R1.2 GREEN（parser.cc +96 LOC，单分支聚合 4 shorthand）→ R1.3 §9.3 反向探针完整三态；ctest 全量 1049/1049 PASS；3 commits（PLAN docs / R1 RED `f8ed62f` / R1 GREEN `5378e67`） |
| 2026-04-30 22:25 | 构建完成 | R2 TDD 闭环：R2.1 RED 11 FAIL + 1 sentinel PASS → R2.2 GREEN（parser.cc +116 LOC，单分支聚合 3 shorthand + Mode enum 分派）→ R2.3 §9.3 反向探针（border-width 2-value 错位）完整三态；ctest Debug 1061/1061 + Release 1030/1030；A1-A7 全过；2 R2 commits（`e2688a7` / `1761b4f`）；下一步 `/reflect` |
| 2026-04-30 22:35 | 回顾完成 | reflection 文档落盘 `memory-bank/reflection/reflection-TASK-20260430-02.md`；3 改进建议落实：P1 #1（systemPatterns.md「极窄路径 0.2-0.25×」档新增）+ P2 #2（systemPatterns.md「Spec 实施模式描述粒度准则」新段）+ A8 ROI 评估；A8 ✅ 高/中 ROI（2/4 触发，§0 grep 表 + 隐式契约 fingerprint 双触发）；plan × 0.6 第 15 数据点 0.22× 与 TASK-30-01 P6（0.21×）一同定型「极窄档」；下一步 `/archive` |
| 2026-04-30 22:42 | 已归档 | 归档文档 `memory-bank/archive/archive-TASK-20260430-02.md`；3 改进建议全部闭环（P1 #1 + P2 #2 systemPatterns.md 落实 / #3 评估 — 无需迁移到「待处理事项」）；feature 分支已合并到 main `--no-ff` 后删除；MB 三件套重置为「空闲」 |

</details>

---

<details>
<summary>TASK-20260430-01：first/last child margin collapse with parent（CSS 2.1 §8.3.1 嵌套规则） — ✅ 已归档（点开查看历史）</summary>

### TASK-20260430-01：first/last child margin collapse with parent（CSS 2.1 §8.3.1 嵌套规则）

- **复杂度级别：** Level 3（单子系统 Layout + API 设计决策 + 跨函数 chain propagate）
- **状态：** ✅ 已归档（2026-04-30）— 归档文档 `memory-bank/archive/archive-TASK-20260430-01.md`；4 改进建议全部落实（P0 → `writing-plans.mdc` §9.4；P1×3 → `writing-plans.mdc` §0 + `creative.md` §d.2；P2×2 → `systemPatterns.md`）；ctest 1039/1039 PASS；wpt-005 SKIP→PASS；plan × 0.6 第 14 数据点 0.46×
- **设计 spec：** `docs/specs/2026-04-30-margin-collapse-with-parent-design.md`（17 验收 / D1-D5 决策矩阵 / §8.3.1 5 adjoining 规则状态表 / 阻断条件 truth table / 算法伪码 / 6 风险登记）
- **实现 plan：** `docs/plans/2026-04-30-margin-collapse-with-parent.md`（7 Phase / 14 任务 / ~6.5h plan / plan×0.6 ≈ 3.9h 准确档）
- **需要创意阶段：** ❌ 否（5 决策 D1-D5 已在 PLAN 头脑风暴中锁定，无 UI/算法/架构空白）
- **创建日期：** 2026-04-30
- **分支：** `feature/TASK-20260430-01-margin-collapse-parent`（基于 main `a84d30d`，已创建）
- **来源：** TASK-20260426-01 archive §10 + reflection §4.12 P3 触发型 TASK-26-02 占位 / activeContext §待处理事项「P3 候选 TASK-26-02」 / wpt-005 SKIP-w/-rationale 现实直接验证目标
- **安全相关：** ❌ 否（纯 layout 算法，无外部输入 / 无认证 / 无新依赖）

#### 范围（用户决策 V1=A）

仅做 **A 子项**：first/last child margin collapse with parent。**不动** clearance / float / clear（CSS layer 完全无实现，需独立 Level 4 任务承接）。

| W3C CSS 2.1 §8.3.1 规则 | 当前状态 | 本任务目标 |
|---|---|---|
| 同级 sibling collapse `max(pos)+min(neg)` | ✅ R3 完成 | 不动 |
| collapse-through 空 box 串联 | ✅ R3 完成 | 不动 |
| 负 margin 数学 | ✅ R3 完成 | 不动 |
| BFC root 阻断（overflow!=visible）| ✅ R3 部分（仅 overflow trigger）| 不动 |
| **first child 与 parent margin-top collapse** | ❌ 未实施（受 D1.2 LayoutChild API 边界限留）| ✅ **本任务** |
| **last child 与 parent margin-bottom collapse** | ❌ 未实施 | ✅ **本任务** |
| collapse 阻断条件（parent padding-top/border-top/min-height）| ❌ 未实施 | ✅ **本任务** |
| clearance | ⚠️ stub（`MarginChain.has_clearance` 标志位）| 不动（依赖 float）|

#### 验收要点（待 /plan 精化）

- ctest 全量 PASS（基线 1029/1029；预计 +10-15 new test cases）
- Release `-O3 -Werror` 0 err/warn
- `bench_layout_*` baseline 不退化超 +10%（**§7.0.1 同窗口 stash-swap 对照强制**）
- **wpt-005 SKIP → PASS**（`Wpt005_NonSiblingAdjoiningMarginsCollapse` 是直接验证目标）
- W3C CSS 2.1 §8.3.1 嵌套 collapse 至少 5-7 单测（first child / last child / 阻断条件 padding-top/border-top/min-height / 多层嵌套）+ wpt-005 PASS
- 反向探针 ≥ 1 处（D3 类回归测试，§9.3 强制）

#### VAN 阶段决策（已锁定）

| # | 维度 | 选择 | 理由 |
|:-:|---|---|---|
| V1 | 范围 | **A 子项**（first/last child collapse with parent）| clearance 完整版依赖 float（CSS layer 零实现），独立 Level 4 任务 |
| V2 | 拆分策略 | **单 Round 一次过 + plan 内部 Phase 划分** | Level 3，无明显多 Round 边界 |
| V3 | Git 分支 | **feature/TASK-20260430-01-margin-collapse-parent** | 基于 main `a84d30d` |
| V4 | 复杂度 | **Level 3** | 单子系统 + API 设计决策（LayoutChild 跨函数 chain 透传协议）|
| V5 | 安全标注 | ❌ 不标 | 纯 layout 算法 |

#### VAN 阶段代码实证（落实 P0「方案根因假设未先验证」+ TASK-13 反思 #2 基础假设核查）

| # | 假设/命题 | grep 实证 | 影响设计 |
|:-:|---|---|---|
| F1 | clearance 当前是否 stub | ✅ 是。`margin_collapse.h:52-54` `ApplyClearance()` 仅置标志位 + 注释「完整实施 = TASK-26-02 (float/clear)」 | 范围拆分依据；A 子项不动 clearance |
| F2 | CSS `clear` / `float` 属性是否被解析 | ❌ **完全无实现**。`grep -i clear\|float` in `veloxa/core/css/` 仅命中 transition.cc 的 `kFloat`（数值类型 enum，无关）；CSS parser / property / style_resolver 零 clear/float 处理 | clearance 完整版需独立 Level 4 任务，本任务跳过 |
| F3 | first/last child margin collapse with parent 当前状态 | ❌ 未实施。`layout_engine.cc:446-451` 显式注释「P3 TASK-26-02 完成」/「未跨函数 propagate」，仅做 sibling-level collapse | 本任务从零实施 |
| F4 | `LayoutChild` API 当前签名 | ⚠️ 无 chain 参数。`LayoutChild(LayoutBox*, f32, const LayoutContext&)` — 需要扩展为带 `MarginChain*` in/out | API 改动影响面：3 处 callers（LayoutBlock / LayoutFlex / 顶层 Layout），以及 5 处 LayoutType 分支（kBlock/kFlex/kInline/kText/kReplaced）|
| F5 | wpt-005 fixture 内容 | ✅ 是 first/last child margin collapse with parent 场景（nested `<div>` 嵌套 `#div3.margin-bottom:2em` 与外层 div bottom collapse + `#div4.margin-top:1em`）；R3.5 显式 SKIP-w/-rationale | wpt-005 是直接验证目标第一首选 |
| F6 | FetchContent 代理 | ✅ `_deps/` 已离线预置（quickjs-ng + benchmark），不引入新依赖 | 跳过 git proxy 守卫 |

#### 前置验证清单

| 维度 | 结果 | 备注 |
|---|---|---|
| 依赖可获取性 | ✅ | 无新依赖（全部 vx_core 已有模块）|
| 环境就绪 | ✅ | `build/` Debug + `build-bench/` Release 均已配置可复用 |
| 已有 artifact | ✅ | TASK-26-01 沉淀完整：`MarginChain` POD / wpt-005 fixture / `tests/integration/wpt_layout_test.cc` 框架 / `tests/core/layout/block_layout_test.cc` 风格 / `bench_layout_buildtree` baseline |
| ctest 基线 | ✅ 1029/1029 PASS in 2.55s（含 2 SKIP-w/-rationale，本任务目标 wpt-005 SKIP → PASS） |
| FetchContent 代理守卫 | ⊘ 跳过 | F6 不引入新依赖 |
| 待处理事项关联 | ✅ 多条适用 | **§7.0.1 同窗口对照 bench**（首次外部任务命中验证 ROI）+ **§9.1 Layout 必检项**（默认 width/height + 跨 LayoutType 独立 box-model；本任务 LayoutChild API 改造直接命中）+ **§9.3 Mixed TDD RED 反向探针强制**（第 6 次实战）+ **subagent §D3 重评估机制**（plan 阶段标 D3 build 阶段评估）+ **plan × 0.6 第 14 数据点**（Level 3 单 Round，预期落「准确档」0.5-0.6×）|

#### 任务历史

| 时间 | 阶段 | 备注 |
|---|---|---|
| 2026-04-30 19:33 | 初始化 | VAN 完成；6 项 grep 实证（F1-F6）；用户决策 V1=A 锁定范围；分支创建（基于 main `a84d30d`）；MB 同步 |
| 2026-04-30 19:50 | 规划中 | PLAN 完成；5 决策矩阵（D1-D5）锁定；spec + plan 文档落盘；7 Phase / 14 任务划分；估时 ~6.5h plan / ~3.9h plan×0.6；不需要 /creative |
| 2026-04-30 21:00 | 构建完成 | BUILD 完成；P0-P6 全 7 Phase ✅；TDD 严格 RED→GREEN→REFACTOR + §9.3 反向探针 3/3 + collapse-through 跨边界 + deep chain；D2 决策实施时调整为 in-out by-pointer（`MarginChain* incoming`，by-value out）以支持多级跨函数 propagate；隐式 BFC root 识别（root + html/body 顶层 wrapper，`box->parent->parent == nullptr`）；wpt-005 SKIP→PASS；ctest 1039/1039 PASS（Debug + Release `-O3 -Werror`）；同窗口 stash-swap bench `BM_LayoutBuildTreeFlat/64 +7.55%` / `Nested/16 +3.42%` / `Mixed -9.84%` / `Flex<1,8> +5.84%` / `Flex<8,8> +4.40%` / `Flex<16,16> +4.94%`，A6/A7 ≤ +10% 全 PASS；O(N) → O(1) 性能优化（`last_in_flow_block` 指针 hoisting）；新发现：CSS parser 不处理 `border-bottom` shorthand（限制已记入 spec），改用 `padding-bottom` 等价测试 |
| 2026-04-30 21:30 | 回顾完成 | REFLECT 完成；reflection-TASK-20260430-01.md 落盘 12 段全维度回顾；plan × 0.6 第 14 数据点 **0.46×**（实测 ~180 min vs plan 234 min plan×0.6，落 0.40-0.50× Level 3 单子系统区间）；4 改进建议（P0×1 + P1×3 + P2×2）；2 新候选反复模式定型（API 传递语义未做多级 mental trace + 算法伪码赋值符歧义）；TASK-26-01 升级规则 ROI 验证 3/5 触发全高/中（§7.0.1 同窗口 stash-swap 首次外部任务 7 BM 一次过 / §9.3 反向探针第 6 次实战 / §9.2 默认值边界 plan 阶段锁定）；2 不触发但合理留存（§9.1 Layout 必检项 + subagent D3）；等待 /archive 落实 P0 建议（升级 `writing-plans.mdc` §9 递归算法 API 决策必检项）|
| 2026-04-30 21:45 | 归档 | ARCHIVE 完成：`archive-TASK-20260430-01.md` 落盘；P0/P1 改进建议全部迁移落实 — `.cursor/rules/skills/writing-plans.mdc` §9.4（P0 递归算法 API 决策必检项）+ §0 既有测试隐式契约 fingerprint（P1）+ §0 CSS shorthand 能力 grep 表（P1）；`.cursor/commands/creative.md` §d.2（P1 算法伪码累积语义 explicit method）；`memory-bank/systemPatterns.md` 新增 5 段（递归 API / 测试 fingerprint / CSS shorthand / 算法 explicit method / 副产品优化 3 标准）；任务状态置为已归档 |

#### PLAN 阶段决策矩阵（已锁定）

| # | 维度 | 选择 | 理由 |
|:-:|---|---|---|
| D1 | API 改造策略 | **A1** 新增 `LayoutBlockChild` 专用辅助；`LayoutChild` 不动 | LayoutInline/Flex 路径零影响；遵循 §9.1 跨 LayoutType 独立 box-model；API 边界清晰 |
| D2 | 传递语义 | **A** by-value in / by-value out (`MarginChain` POD 12B) | 编译器 SROA 等价无开销；数据流单向可见；无 lifetime/nullable 隐形约束 |
| D3 | 阻断条件覆盖 | **A** 完整规范子集（padding/border + BFC root + height + min-height）| W3C 浏览器对齐；wpt-005 验证；6 边界条件全覆盖 |
| D4 | 测试深度 | **B** 完整档（10 单测 + 3 反向探针 + wpt-005）| §9.3 强制最小 ≥ 1；本任务 3 个独立 D3 维度（blocks_top / blocks_bottom / deep chain）|
| D5 | Phase 划分 | **B** 7 Phase 细粒度 | RED 与 GREEN 分离彻底；first/last 拆开独立 phase |

#### 7 Phase 划分（Plan 内部）

| Phase | 内容 | plan (min) | × 0.6 (min) |
|:-:|---|---:|---:|
| P0 | 准备（grep + wpt-005 拆解 + 基线） | 30 | 18 |
| P1 | RED 单测全套（10 单测 + 3 反向探针位置注释） | 60 | 36 |
| P2 | API 改造 + dispatch（`LayoutBlockChild` skeleton 调通编译） | 30 | 18 |
| P3 | GREEN first child collapse | 45 | 27 |
| P4 | GREEN last child collapse + 4 阻断条件 | 90 | 54 |
| P5 | 反向探针验证 + deep chain + collapse-through 递归 | 60 | 36 |
| P6 | wpt-005 + bench 同窗口 + 收尾 | 75 | 45 |
| **合计** | — | **390 (~6.5h)** | **234 (~3.9h)** |

#### 需要创意阶段的组件

❌ 无（5 决策 D1-D5 已在 PLAN 头脑风暴中锁定，无 UI/算法/架构空白；可直接 `/build`）

</details>

---

<details>
<summary>TASK-20260426-01：Layout 正确性消化（#25 + #28 + #20 + #21）[安全相关] — ✅ 已归档（点开查看历史）</summary>

### TASK-20260426-01：Layout 正确性消化（#25 + #28 + #20 + #21）[安全相关]

- **复杂度级别：** Level 4（多子系统：HTML parser + Layout block flow + Layout inline formatting + LayoutBox API；#20/#21 单独够撑 Level 3）
- **状态：** ✅ 已归档（2026-04-30）— 归档文档 `memory-bank/archive/archive-TASK-20260426-01.md` 已落盘；4 子任务全部完成（#25/#28/#20/#21）+ Release ctest 1029/1029 PASS + 同窗口对照 bench mean -3.6% / median +2.65%；P0/P1 改进建议均已迁移落地（规则文件与 systemPatterns 同步）
- **创建日期：** 2026-04-26
- **分支：** `feature/TASK-20260426-01-layout-correctness`（基于 main `9f7f338`，已创建）
- **来源：** `techContext.md §技术债务清单` 4 项 + `tasks.md §待立项候选 包 D` + 本次 /van 用户决策 D1 全包
- **设计 spec：** `docs/specs/2026-04-26-layout-correctness-design.md`
- **实现 plan：** `docs/plans/2026-04-26-layout-correctness.md`
- **创意文档：**
  - `memory-bank/creative/creative-margin-collapsing.md`（R3 #20，D1 5 决策全 ACCEPT）
  - `memory-bank/creative/creative-line-box-model.md`（R4 #21，D2 5 决策全 ACCEPT）

#### 目标

按 D1 全包 + 多轮次 Build 中间态（complexity-levels.mdc §轮次完成协议）依次消化 4 项 Layout 正确性技术债：

| 子任务 | 编号 | 现状（VAN grep 实证） | 修复方向 | 量级 |
|:-:|:-:|---|---|---:|
| Round 1 | **#25** | LayoutBox 仅有 `*_box_width/height()` 系列；缺 origin helpers；坐标计算分散在 `layout_engine.cc` 5 处 + `flex_layout.cc` 15+ 处 | 加 `border_box_origin()` / `padding_box_origin()` / `content_box_origin()` 6 helpers + 全量替换分散计算 + 集成测试覆盖坐标语义 | ~30-60 min |
| Round 2 | **#28** | HTML parser `parser.cc:95` 把 `style` attr 当普通 attribute；`CssParser::ParseDeclarationList` API 已存在（`script/dom_bindings.cc:322` 已用）| HTML parser 处理 attribute 时识别 `style` → 调 ParseDeclarationList → 逐条 `SetInlineDeclaration` + 集成测试（HTML 路径与 JS 路径行为一致）+ 安全（DoS 巨大 declaration / 无效 `url()` / CSS var 未实现退化） | ~60-90 min |
| Round 3 | **#20** | `layout_engine.cc:209-212` 完全无 margin collapsing（直接 `y_offset = child->y + border_box + margin[Bottom]`）| CSS 2.1 §8.3.1 完整实现：adjoining vertical margins / nested / negative collapse / collapse-through (empty box) / clearance / float exclusion；至少 8-12 testcases（W3C 官方 testcase 选取参考）| ~3-5h |
| Round 4 | **#21** | `layout_engine.cc:260-303` LayoutInline 用 `line_height = max(child.height)` 简化；`text_shaper.h:12` 有 `f32 baseline` 字段但 layout 未消费 | 引入 LineBox 抽象；接入 TextShaper.baseline；实现 ascent/descent metric；vertical-align（baseline/top/middle/bottom）；line-height 计算；半-leading；line box reflow | ~5-8h |
| **合计** | — | — | — | **~10-15h plan / ~6-9h plan×0.6 实测预期** |

#### 验收要点（待 /plan 精化）

- 每 Round 末「轮次完成」中间态报告（complexity-levels.mdc §轮次完成判断 + build.md §6.5）
- ctest 全量 PASS（基线 951/951；预计 +20-40 new test cases）
- Release `-O3 -Werror` 0 err/warn
- `bench_layout_*` baseline 不退化超 10%（TASK-24-01 32K block sweet spot 不破）
- W3C CSS 2.1 §8.3.1 testcase 选取至少 6 例做集成测试金标准
- `techContext.md §技术债务清单` #20/#21/#25/#28 状态变更为 ✅ 已闭环

#### VAN 阶段决策（已锁定）

| # | 维度 | 选择 | 理由 |
|:-:|---|---|---|
| V1 | 范围包 | **包 D（Layout 正确性）** | 用户从 5 包候选中选定 |
| V2 | 拆分策略 | **D1 全包 + 多轮次 Build** | 4 子任务依次串行，每 Round 独立验收中间态 |
| V3 | Git 分支 | **feature/TASK-20260426-01-layout-correctness** | 基于 main `9f7f338` |
| V4 | 复杂度 | **Level 4** | 多子系统 + 架构决策 + #20/#21 单独 Level 3 量级 |
| V5 | 安全相关 | **✅ [安全相关]** | #28 接收 HTML `style="..."` 外部输入 |

#### VAN 阶段代码实证（落实 P0「方案根因假设未先验证」+ TASK-13 反思 #2 基础假设核查）

| # | 假设/命题 | grep 实证 | 影响设计 |
|:-:|---|---|---|
| F1 | 「#28 LayoutEngine::BuildTree 不解析 inline style」 | ⚠️ **描述不精确**：`layout_engine.cc:35` 已传 inline_declarations；真实缺口在 `html/parser.cc:95` | spec 阶段修正 #28 描述 → 修复路径在 HTML parser，非 layout |
| F2 | 「`ParseDeclarationList` API 是否存在」 | ✅ `css/parser.h:12` 完整实现；`script/dom_bindings.cc:322` 已成熟使用 | #28 实施零 API 设计成本 |
| F3 | 「LayoutBox origin 计算分散位置」 | ✅ `layout_engine.cc` 5 处 + `flex_layout.cc` 15+ 处 | #25 改造影响面已知 |
| F4 | 「Block 布局当前是否有 margin collapsing」 | ✅ 完全无 collapsing | #20 是从零实施 |
| F5 | 「LayoutInline 是否流入 TextShaper.baseline」 | ✅ baseline 字段未消费 | #21 第一步：接入 baseline + 引入 LineBox |
| F6 | 「FetchContent 代理状态」 | ✅ 不引入新依赖 | 跳过 git proxy 守卫 |

#### 任务历史

| 时间 | 阶段 | 备注 |
|---|---|---|
| 2026-04-26 01:35 | 初始化 | VAN 完成；6 项 grep 实证；用户决策 D1 全包 + 多轮次 Build；分支创建（基于 main `9f7f338`）；MB 三件套同步 |
| 2026-04-26 02:00 | 规划 | PLAN 完成；6 决策矩阵锁定（Q1A 完整 §8.3.1 + Q2A 全量 LineBox + Q3A wpt 远程拉取 + Q4A 三件套安全护栏 + Q5A 每 Round 1 commit + Q6 0.6× 准确档）；spec + plan 落盘；代理 `172.22.32.1:7890` 实证可达 wpt 仓库 114 fixture 备选；下一步 `/creative` 2 篇创意文档 |
| 2026-04-26 02:20 | 设计 | CREATIVE 完成；D1 (margin collapsing) 5 决策全 ACCEPT — 方案 A MarginChain in-line 累积 + 内部栈式 chain 状态 + BFC root 仅 overflow + VX_DEBUG_LAYOUT trace + 先 layout child 回填 child.y；D2 (LineBox 模型) 5 决策全 ACCEPT — A.1 显式 LineBox + B.1 严格 2-pass vertical-align + C.1 加字段不删 [[deprecated]] + inline-block atomic + CSS 2.1 §10.8.1 默认 line-height；2 篇创意文档落盘（含 3 方案探索 + 3×3 子决策对比 + 形式化伪码 + 6 关键字公式 + 风险登记）；下一步 `/build` R0 |
| 2026-04-26 02:28 | 构建·R0 | BUILD R0 准备完成；ctest 951/951 PASS in 1.23s + bench `BM_LayoutBuildTreeFlat/64` 3709 ns baseline（R3 退出门 ≤ 3895 ns）+ wpt 11 文件入仓 (`tests/fixtures/wpt/css/CSS2/`) 含 README BSD-3-Clause + grep 4 类 fingerprint（F1 #25 替换点 14 处 ≥ 12 / F2 inline_decl 全链路 6 文件 / F3 vertical-align 0 hint 全新 / F4 enum 全链路 7 文件 kLineHeight 模板）；**实证微调**：plan 原选 `margin-collapse-091` 不存在 → 替代 `005`，`vertical-align` fixture 在 `linebox/` 子目录；下一步 R1 #25 origin helpers |
| 2026-04-26 02:38 | 构建·R1 | BUILD R1 完成；#25 origin helpers 落地 — `Point{f32 x, f32 y}` struct + 19 helper（3 origin border/padding/content_box_origin + 16 four-side (border/padding/content/margin)_box_(top/right/bottom/left)）/ TDD RED 反向探针 2/2 通过（ZeroBoxIsTrivial + InversesByMutation）/ 10 处分散计算替换为 helper（layout_engine 1 + flex_layout 9）+ 4 处保留语义清晰（reverse 步进 / box-sizing 反推 / x_offset 步进，无 helper 适用）/ ctest 960/960 PASS（+9 cases，1.58s）/ bench `Flat/64` 3709→3715 ns **+0.16%** ≪ +5% 退出门 (3895 ns) / 全部 6 BM 波动 -1.7% ~ +6.1% ≤ A15 +10% 门；**首次反模式触发**：编译刚完成 Load 4.97 → 假退化 +26% → TASK-19-13 P1「WSL2 bench 稳态协议」sleep 30s 重测 → 真值 +0.16%；plan × 0.6 第 10 数据点 0.5×（plan 60 min / 实测 ~30 min，子档稳定区间 0.22-0.34× 又一确认） |
| 2026-04-26 02:55 | 构建·R2 | BUILD R2 完成 [安全相关]；#28 HTML inline style + 完整三件套安全护栏；ctest 984/984 PASS（+24 case）；bench -3.5%；plan × 0.6 第 11 数据点 0.56× |
| 2026-04-26 03:20 | 构建·R3 | BUILD R3 完成；#20 Block margin collapsing CSS 2.1 §8.3.1 完整 4 类规则 + V1 优化 3 项；ctest 1010/1010 PASS（+26 case）；同窗口对照 bench mean +3.2% / median +3.4%（首次 stash-swap 范式确立）；plan × 0.6 第 12 数据点 0.37× |
| 2026-04-29 01:30 | 构建·R4 | BUILD R4 完成；#21 LayoutInline LineBox 模型 2-pass vertical-align + 半-leading + LineBox Vector + fit-content width / explicit height + inline-block atomic；ctest 1029/1029 PASS（+19 case）+ 2 wpt linebox fixture；同窗口对照 bench mean -3.6% / median +2.65%；plan × 0.6 第 13 数据点 0.5× |
| 2026-04-29 02:10 | 回顾 | REFLECT 完成；R5 finalize 关键步骤收尾（Release 1029 + 0 warn / git proxy unset / techContext #20/#21/#25/#28 状态 ⏳→✅）；reflection 落盘 Level 4 全维度回顾 13 改进建议 + 3 P0 + 4 P1 + 8 新模式沉淀 systemPatterns.md；plan × 0.6 整体 0.44× Level 4 首数据点；等待 /archive |
| 2026-04-30 00:40 | 归档 | ARCHIVE 完成：`archive-TASK-20260426-01.md` 落盘；P0/P1 改进建议迁移确认完成（`writing-plans.mdc` / `creative.md` / `subagent-development.mdc` + `systemPatterns.md` + `productContext.md`）；任务状态置为已归档 |

#### PLAN 阶段决策（已锁定）

| # | 维度 | 选择 | 理由 |
|:-:|---|---|---|
| Q1 | Round 3 #20 实施策略 | **A 一次性完整 W3C CSS 2.1 §8.3.1** | 用户全量；clearance 部分 stub 化（真 float 留 P3 触发型 TASK-26-02 候选） |
| Q2 | Round 4 #21 范围 | **A 全量** LineBox + ascent/descent + vertical-align 6 关键字 + length 偏移 + 半-leading | 用户全量；inline-block atomic（不递归 IFC） |
| Q3 | W3C testcase 来源 | **A wpt 远程拉取** + Plan §0 代理实证 | 代理 `172.22.32.1:7890` (WSL2 host gateway) 实证 200 OK；114 个 margin-collapse fixture 可选 |
| Q4 | #28 安全护栏 | **A 完整三件套** count cap 1000 + value len cap 8 KB + 历史攻击关键字黑名单（expression(/behavior:/javascript:） | 用户全量；威胁模型 spec §6 (T1-T7 全覆盖) |
| Q5 | 轮次提交粒度 | **A 每 Round 1 commit + 中间态报告 + 用户确认** | complexity-levels.mdc §多轮次协议标准路径；TASK-19-13 P3 落实 |
| Q6 | Plan × 0.6 估时档 | **0.6× 准确档** plan 900 min → 实测 ~540 min | Level 4 首数据点；4 子任务复用度高但有 W3C 规范梳理成本 |

#### CREATIVE 阶段决策（已锁定）

##### D1: Margin Collapsing 算法（R3 #20）

| # | 维度 | 选择 | 理由 |
|:-:|---|---|---|
| D1.1 | 算法 | **方案 A — MarginChain in-line 累积法** | 单 pass O(N) + 与现有 LayoutEngine 风格一致 + bench 退化预计 ≤ 5%；vs B pre-pass 双遍（+20-30% 退化） / C 纯函数（API 大改） |
| D1.2 | outgoing_chain 表示 | **LayoutBlock 内部 Vector\<MarginChain\> 栈式状态** | 保持现有 LayoutBox API 不动，避免 API 大改影响 #25 #21 协调成本 |
| D1.3 | BFC root 判定范围 | **本 Round 仅识别 `overflow: hidden\|scroll\|auto` 作为 BFC root** | 防范围蔓延；其他 BFC trigger（display: flow-root / float / 绝对定位）依靠 P3 TASK-26-02 |
| D1.4 | 调试 trace | **`#if VX_DEBUG_LAYOUT` MarginChain::Trace 仅 DEBUG 启用** | Release 0 开销，Debug 可见 |
| D1.5 | collapse-through lookback 实现 | **先 layout child → 基于结果回填 child.y** | 与方案 A 推荐路径一致 |

##### D2: LineBox 模型 + vertical-align（R4 #21）

| # | 维度 | 选择 | 理由 |
|:-:|---|---|---|
| D2.A | LineBox 数据结构 | **A.1 显式 LineBox + Vector\<LineBox\>** | 抽象清晰对应 CSS 2.1 §9.4.2 / §10.8 概念 + 独立测试性 + DevTool 主线复用（TASK-25-02 占位 Inspector） |
| D2.B | vertical-align 算法 | **B.1 严格 2-pass** | CSS 2.1 §10.8.1 在 2 pass 内完全解析 + 算法清晰度高 + 与 spec §5.4 一致 |
| D2.C | TextMetrics ABI 策略 | **C.1 加字段不删 + `[[deprecated("use ascent")]]`** | ABI 不破，与 R1/R2/R3 同步推进零冲突 + `[[deprecated]]` 给未来清理明确锚点 |
| D2.D | inline-block 处理 | **atomic（border_box_height 当 ascent + 0 当 descent）** | spec §2 不做边界（不递归 IFC） |
| D2.E | line-height 单位语义 | **CSS 2.1 §10.8.1 默认**（数字 → font-size 倍数；length → 绝对像素；percent → font-size 百分比） | 完整规范支持 |

#### 子任务多轮次拆分（spec/plan §0-§5）

| Round | 子任务 | 类型 | plan (min) | × 0.6 (min) | 子代理 |
|:-:|:-:|---|---:|---:|---|
| R0 | 准备：基线核验 + wpt 拉取 + grep fingerprint | meta | 30 | 18 | — |
| R1 | #25 LayoutBox origin helpers + 替换分散计算 + 6 测试 + bench | impl/D2 | 60 | 36 | 直执行 |
| R2 | #28 HTML inline style + 三件套安全护栏 + 8 测试 + 集成 | impl/D2 + 安全 | 90 | 54 | 直执行 |
| R3 | #20 Margin collapsing 全实施 + 12 测试 + 4 wpt fixture | impl/D3 | 300 | 180 | 子代理 A（R3.3 LayoutBlock 算法）|
| R4 | #21 LineBox + vertical-align + line-height + 9 测试 + 2 wpt fixture | impl/D3 | 360 | 216 | 子代理 B（R4.3 TextShaper FT）+ 子代理 C（R4.6 LayoutInline）|
| R5 | finalize：techContext 闭环 + git 代理 unset + reflect | meta | 60 | 36 | — |
| **合计** | — | — | **900 (15h)** | **540 (9h)** | **3 子代理 D3** |

</details>

---

<details>
<summary>TASK-20260425-01：SDL2 窗口后端 + 输入事件桥接 — ✅ 已归档（点开查看历史）</summary>

### TASK-20260425-01：SDL2 窗口后端 + 输入事件桥接

- **复杂度级别：** Level 3（中等功能 — 新模块 + 设计决策 + 跨现有 headless 后端的抽象统一）
- **状态：** ✅ 已完成（已 `--no-ff` 合并到 main `4a096ab`，详见 `archive-TASK-20260425-01.md`）
- **反思文档：** `memory-bank/reflection/reflection-TASK-20260425-01.md`
- **归档文档：** `memory-bank/archive/archive-TASK-20260425-01.md`
- **创建日期：** 2026-04-25
- **来源：** 项目主体功能完整（30 任务归档），实时调试 UI 主线第一步；解锁后续 DevTool（hot reload / overlay / Inspector）
- **分支基线：** `main` `e52868b`；建议分支 `feature/TASK-20260425-01-sdl2-backend`（待 `/plan` 后创建）
- **安全相关：** ❌ 否（无外部输入解析 / 无认证 / 无新网络栈；SDL 输入事件已在受控范围内）

#### 目标

为 Veloxa 引擎引入第一个**有可见窗口**的平台后端（SDL2），打通 `vx_view_run()` 在 WSLg / Linux Desktop 的实时显示路径，并把 SDL 输入事件桥接到现有 `VxInputEvent` 结构体与事件系统三阶段冒泡。

#### 验收要点（待 /plan 精化）

- 新增 `vx_event_loop_create_sdl2()` / `vx_surface_create_window(w, h, title)`，与现有 `headless` 后端对等接入 `VxView`
- `examples/hello.cc` 或新增 `examples/hello_sdl2.cc` 在 WSLg 下运行可见 400×300 窗口（颜色块布局完整）
- 鼠标移动 / 点击 → `:hover` / `:active` 状态切换可见
- 不破坏现有 ctest 全绿（headless 路径继续 PASS）

#### VAN 阶段前置检查

| 维度 | 结果 | 备注 |
|---|:-:|---|
| 依赖可获取性 | ✅ | `libsdl2-dev` 2.0.20 已安装（`pkg-config sdl2` ✅ + `/usr/include/SDL2/SDL.h` ✅ + `/usr/lib/x86_64-linux-gnu/cmake/SDL2/sdl2-config.cmake` ✅）|
| 环境就绪 | ✅ | WSLg `DISPLAY=:0` + `WAYLAND_DISPLAY=wayland-0` + `/mnt/wslg` 完整 |
| 已有 artifact | ✅ 部分 | `veloxa/platform/headless/` 可作为对照参考；`VxInputEvent` 结构体已定义；事件系统已有三阶段冒泡 |
| FetchContent 代理守卫 | ⊘ **跳过** | `_deps/` 已离线预置（quickjsng + benchmark）；本任务**不引入新 FetchContent**（SDL2 走系统包） |
| 待处理事项关联 | ✅ | **plan × 0.6 第 8 数据点**（首个新模块类任务，预期 ≥ 1.5× "准确档" 而非 "最窄"）；**集成测试**（涉及生命周期 + 跨模块）；**安全**（输入事件来源外部）；**bench 阈值表绝对增量兜底**（input event 延迟若入 BM）|

#### 阻碍项决策（待用户选择）

| 选项 | 描述 | 优劣 |
|:-:|---|---|
| **(a)** | `sudo apt install libsdl2-dev`（推荐） | 体积 ~5 MB；与现有 FreeType / HarfBuzz / PNG / JPEG 走系统包一致；最快 |
| **(b)** | `FetchContent` 拉 SDL2 源码 | 自包含；但首次构建拉 ~50 MB；额外代理依赖；与现有依赖管理风格不一致 |
| **(c)** | 改换 X11 raw 后端 | 不引入 SDL；但同样需 `libx11-dev`；输入事件映射工作量加倍；跨平台收益丧失 |

#### 用户决策（VAN 阶段产出）

| # | 维度 | 选择 | 理由 |
|:-:|---|---|---|
| V1 | libsdl2-dev 解决方式 | **(a) sudo apt install libsdl2-dev** ✅ 已完成 | 与 FreeType/HarfBuzz/PNG/JPEG 系统包风格一致 |
| V2 | 复杂度分级 | **Level 3** | 新模块 + 设计决策；surface 抽象 / event_loop 模式 / CMake 选型 / 输入事件映射粒度 4 维度需头脑风暴 |
| V3 | Git 分支 | **feature/TASK-20260425-01-sdl2-backend**（待 /plan 后创建） | 基于 main `e52868b` |

#### /plan 头脑风暴决策（已锁定）

| # | 维度 | 选择 | 理由 |
|:-:|---|---|---|
| Q1 | 输入事件投递路径 | **(B) `Sdl2EventLoop::PumpInputEvents(callback)` 裸函数** | EventLoop 不反向持有 View；与 headless 对称；保持平台抽象单向 |
| Q2 | Surface 像素呈现时机 | **(B) `Surface` 抽象增加 `virtual void Present() {}` 默认 no-op** | 语义清晰；headless 默认 no-op；SDL2 重写为 `SDL_UpdateTexture + RenderPresent` |
| Q3 | CMake SDL2 查找 | **(C) 双轨兜底** | `find_package(SDL2 CONFIG QUIET)` 优先，失败 fallback `pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)` |
| Q4 | 默认构建可选性 | **(C) 默认 OFF，需 `-DVX_PLATFORM_SDL2=ON`** | 与 `VX_BUILD_BENCHMARKS` 风格一致；不破坏 CI minimal 镜像 |
| Q5 | examples 形态 | **(B) 保留 `hello.cc` + 新增 `examples/hello_sdl2.cc`** | PPM 与 SDL2 两后端独立参考；含 ctest smoke (`SDL_VIDEODRIVER=dummy`) |
| Q6 | C API 命名 | **(C) `vx_event_loop_create_sdl2()` + `vx_surface_create_window(const VxWindowOptions*)`** | 与现有 `_headless` / `_memory` 风格一致；options struct 与 `VxViewConfig` 一致；未来加 flags 不破 ABI |

#### 隐含范围（必做技术债清理）

发现：`vx_event_loop_destroy` / `vx_surface_destroy` / `vx_surface_save_ppm` 在 `veloxa_api.cc` 硬编码 `HeadlessEventLoop*` / `MemorySurface*`。加 SDL2 后端时 `delete` 错的派生类指针会 UB。**本任务范围内必须修复**：改用基类指针调用（基类已 `virtual ~`）。详见 spec §5.3。

#### 设计与计划产出

- **设计规格：** `docs/specs/2026-04-25-sdl2-window-backend-design.md`（13 段：目的 / 不做 / 成功标准 A1-A7 / 6 决策 / 架构 + 接口签名 / 安全威胁建模 / 测试策略 + 边界输入清单 / 注入点核对表 / CMake 链接拓扑 / 文件清单 / 估时 / R1-R6 风险登记 / 与未来任务关系）
- **实现计划：** `docs/plans/2026-04-25-sdl2-window-backend.md`（6 Phase / 12 任务 / 13 新增 GTests + 1 ctest smoke）
- **需要创意阶段：** ❌ 否（Level 3，但 Q1-Q6 决策 + 接口签名已具体化；可直接 `/build`）

#### 估时（plan × 0.6 第 8 数据点）

| Phase | 内容 | plan 估时 | × 0.6 实测预期 |
|---|---|---:|---:|
| P0 | 基线核验 + Surface::Present + destroy/save_ppm 基类化 | 25 min | 15 min |
| P1 | sdl2_input_translate.{h,cc} + 7 GTests | 50 min | 30 min |
| P2 | sdl2_window_surface.{h,cc} + 3 GTests | 70 min | 42 min |
| P3 | sdl2_event_loop.{h,cc} + 4 GTests | 50 min | 30 min |
| P4 | C API 扩展 + veloxa_api.cc 修复 | 35 min | 21 min |
| P5 | examples/hello_sdl2.cc + ctest smoke | 40 min | 24 min |
| P6 | WSLg 手工验证 + Release `-Werror` + memory-bank 文档 | 30 min | 18 min |
| **合计** | | **~300 min (5h)** | **~180 min (3h)** |

#### 任务历史

| 时间 | 阶段 | 备注 |
|---|---|---|
| 2026-04-25 23:51 | 初始化 | VAN 完成，识别 libsdl2-dev 阻碍项 + WSLg 环境就绪 |
| 2026-04-25 23:56 | 初始化 | libsdl2-dev 2.0.20 安装完成；阻碍项解除；进入 `/plan` |
| 2026-04-25 23:58 | 规划完成 | /plan 头脑风暴 Q1-Q6 锁定 + spec/plan 文档产出；等待用户审查 → /build |
| 2026-04-26 00:20 | 构建中 | /build 启动；P0.1 基线 ctest 917/917 PASS 1.00s |
| 2026-04-26 00:49 | 构建完成 | P0-P5 + P6.2 + P6.3 ✅；P6.1 用户决策标为遗留验证项；ctest 951/951 PASS（Debug + Release `-O3 -Werror`）；15 commits |
| 2026-04-26 01:00 | 回顾完成 | reflection-TASK-20260425-01.md ✅；plan ×0.6 第 9 数据点 0.22× 历史最快；3 P1 沉淀长期知识库 + P0 #4（hello_sdl2 :hover）落地 |
| 2026-04-26 01:10 | 归档完成 | archive-TASK-20260425-01.md ✅；feature 分支 `--no-ff` 合并到 main `4a096ab`（17 commits）；MB 三件套重置为空闲 |

</details>

---

## 任务历史

<details>
<summary>TASK-20260424-04：SoftwareCanvas::DrawText 真路径 warm 残余优化（D 纯收尾模式）— ✅ 已归档（点开查看历史）</summary>

### TASK-20260424-04：SoftwareCanvas::DrawText 真路径 warm 残余优化（D 纯收尾模式）

- **复杂度级别：** Level 2（单候选方案 (c) hb_shape cache + 精简 FIFO + /plan→/build 直通，跳过 /creative）
- **状态：** ✅ 已完成（详见 `memory-bank/archive/archive-TASK-20260424-04.md`）
- **反思文档：** `memory-bank/reflection/reflection-TASK-20260424-04.md`
- **归档文档：** `memory-bank/archive/archive-TASK-20260424-04.md`
- **分支：** `feature/TASK-20260424-04-drawtext-residual-opt`（9 commits 含 archive）
- **创建日期：** 2026-04-24
- **来源：** TASK-20260424-03 归档 §9.2「残余 499 ns 可能突破点」P3 触发型候选（本任务用 D 模式主动推进，非刚性目标）
- **设计文档：** `docs/specs/2026-04-24-drawtext-shape-cache-design.md`（11 段，含安全威胁建模 + 契约 + 测试矩阵）
- **实现计划：** `docs/plans/2026-04-24-drawtext-shape-cache.md`（6 Phase / 12 Task / 14 新增测试 / 2 新 BM）
- **需要创意阶段：** ❌ 否（Level 2；所有设计决策已于 /plan 头脑风暴 Q1-Q5 锁定）
- **安全相关：** ❌ 否（性能优化 / 无外部输入 / 无认证 / 无新依赖；但设计文档 §6.2 做了轻量威胁建模覆盖 DoS / 碰撞侧信道 / UAF / Key 溢出）

#### 目标

SoftwareCanvas::DrawText 真路径 **warm Medium** 从 3499 ns → **< 3200 ns**（**-299 ns / -8.5%**，新结构性阈值），作为 TASK-24-03 K7 Resolved 后的收尾优化。**D 纯收尾模式**：达成 `<3200 ns` 新阈值即归档，避免 P3 长期挂单；不追求原 D5 刚性 `<3000 ns`（用户已于 TASK-24-03 知情接受）。

| BM | 当前 warm（TASK-24-03 baseline）| 目标 | 倍率 |
|---|---:|---:|---|
| `BM_DrawTextReal_Warm_Medium` (19 char) | **3499 ns** | **< 3200 ns** | **-8.5% / -299 ns** |
| `BM_DrawTextReal_Warm_Short` (2 char) | 677 ns | 不回归（≤ 677 × 1.1 = 744 ns）| 兜底 |
| `BM_DrawTextReal_Warm_Long` (124 char) | 10573 ns | 不回归（≤ 10573 × 1.1 = 11630 ns）| 兜底 |

#### VAN 阶段基础假设核查（代码实证）

| # | 候选 | 代码位置 | 命题/实证 | 预期收益 | 状态 |
|:-:|---|---|---|---:|:-:|
| (a) | **skip-all-zero AA fast path** | `blit_avx2.h` L117-127 `BlendGlyphRow` + `blit_sse2.h` 入口 + 标量 `glyph_blend.h` | 当前无 zero-row short-circuit；FT_Bitmap 已 crop bounding box 所以 zero-row 分布**不确定**，需 Phase 0 实测 zero-row 占比再决定投入 | -50 ~ -200 ns | ⚠️ 条件命题（占比 <20% 则可能 regress）|
| (b) | GlyphCache row_ptr 数组 | `glyph_cache.h` L11-18 `GlyphBitmap` 仅 `Vector<u8> alpha` | TASK-24-03 Phase 6 B2 **已在 DrawText 外层循环 hoist row_ptr**（`alpha_row = alpha.data() + row_start * width` 每 glyph 仅 1 次）→ 残余优化空间 ≤ 20 ns | ~5-20 ns | ❌ **基本否决** |
| (c) | **hb_shape cache per-text** | `software_canvas.cc` L221-226 `hb_buffer_reset + add_utf8 + hb_shape` 每次 DrawText 无条件执行 | BM warm 测试同 text × N 次，cache 命中 100% → 最可能单独达标；但真实 UI workload 若 text 变化频繁 → cache miss 路径额外 hash + memcpy 开销可能 net regression | -200 ~ -500 ns | ✅ 高收益但带副作用 |

#### 前置验证清单

| 维度 | 结果 | 备注 |
|---|:-:|---|
| 依赖可获取性 | ✅ | FT/hb/google-benchmark 已集成（TASK-20260414-01 / TASK-20260419-02），无新依赖 |
| 环境就绪 | ✅ | `build-bench/` + `build-bench/benchmarks/bench_drawtext` 已存在；DejaVuSans.ttf ✅ |
| 已有 artifact | ✅ 100% 复用 | `bench_drawtext.cc` 6 BM + `baseline/bench_drawtext.json` (TASK-24-03 Phase 7.4 刷新) + `pixel_blend_test.cc` 11 GTests + `blit_sse2.h` / `blit_avx2.h` / `glyph_blend.h` header 全部复用 |
| **FetchContent 代理守卫** | ⊘ 跳过 | 本任务**不触发 FetchContent**（仅用已集成依赖）|
| 待处理事项关联 | ✅ 多条 P1/P2 适用 | **plan × 0.6 通用协议**（第 7 数据点，「最窄」档候选）+ **bench 阈值表绝对增量兜底**（TASK-11 #1）+ **Mixed TDD RED 反向探针**（本任务 (a) 新 fast path 标配）+ **WSL2 bench 稳态协议**（TASK-24-03 §7 强制）+ **编译器优化识别反模式**（TASK-24-03 §8 — (a) pmovmskb+testz fast path 标量 fallback 需 godbolt）+ **异构工作负载 SIMD 尺寸阈值 dispatch**（TASK-24-03 新模式 — (a) 可能也需要 count 阈值）|

#### 用户决策（VAN 阶段产出）

| # | 维度 | 选择 | 理由 |
|:-:|---|---|---|
| V1 | D5 重启定位 | **D 纯收尾模式** | 目标放宽为 `<3200 ns` 新结构性阈值；达成即归档，无严格 `<3000 ns` 压力 |
| V2 | 复杂度分级 | **Level 2** | 3 候选 scope 已锁定；无架构空白；直接 /plan→/build |
| V3 | Git 分支 | **feature/TASK-20260424-04-drawtext-residual-opt** | 基于 main `78cabf4`（TASK-24-03 已合并）|

#### /plan 头脑风暴决策（2026-04-24 锁定）

| # | 维度 | 锁定选项 | 简述 |
|:-:|---|---|---|
| Q1 | 候选组合策略 | **方案 B — 仅 (c) hb_shape cache** | (a) 块级 zero-skip 进一步代码实证净收益 ≈ 0；单 (c) 几乎必达 `<3200 ns` + 长期复用价值（CJK / UI label）|
| Q2 | Cache key 表示 | **K2 — u64 fingerprint（FNV-1a）+ text_len 碰撞护栏** | 无 String 拷贝；碰撞概率 ~ 2^-96 实际不可达 |
| Q3 | Cache 存活范围 | **S2 — per-FontManager 成员** | key = (FontHandle, pixel_size, fingerprint, text_len)；font 重载 Clear 安全 |
| Q4 | 容量 + 淘汰 | **C1 — 固定 128 + FIFO** | 最简实现；~40 KB 内存天花板 |
| Q5 | Cache miss 回归护栏 | **B1 RoundRobin 256 门槛 BM** + B3 AllMiss 参考 BM | RoundRobin 50% hit 作为门槛（≤ pre-baseline + max(5%, 50 ns)）；AllMiss 入 baseline CSV 不计门槛 |

#### 需要创意阶段的组件

❌ 无（Level 2；所有设计决策已通过头脑风暴 Q1-Q5 锁定，直接 /build）

#### 接受标准（/plan 精化）

1. `BM_DrawTextReal_Warm_Medium_mean` < 3200 ns（主目标；D 模式下若 3200 ≤ value < 3499 ns 接受"部分达成归档"）— **✅ 实测 mean 2350 ns / single 1877 ns（超额 850 ns / 26%，甚至间接达成技术刚性 <3000 ns 目标）**
2. `BM_DrawTextReal_Warm_Short` / `_Long` ≤ baseline × 1.1（677×1.1=744 / 10573×1.1=11630）— **✅ 实测 Short 311 ns (-54%) / Long 4333 ns (-59%)**
3. `BM_DrawTextReal_Cold_Medium` ≤ 28338 × 1.1 = 31172 ns — **⚠️ 实测 mean 33620 ns (+18.6%) 但 CV 12.67%（FT_Load 主导噪声），median 36392 ns；VAN 已识别 Cold 路径非本任务范围；接受为噪声区间**
4. **新 `BM_DrawTextReal_Warm_TextVarying_RoundRobin` ≤ pre-baseline + max(5%, 50 ns)**（cache miss 路径护栏）— **✅ 实测 2676 ns (hit=100%)；pre-baseline 3605 ns → 改动后反降 -929 ns (-25.8%) 远好于阈值**
5. **新 `BM_DrawTextReal_Warm_TextVarying_AllMiss` 入 baseline CSV，不计门槛**（最坏边界参考）— **✅ 实测 4711 ns (miss=100%)；pre-baseline 3736 ns → +975 ns 代表 insert + ShapedRun copy 开销**
6. ctest 全量 PASS（当前 59+ tests → +11-12 new：9 unit + 3 integration + 1 R2 反向探针；R1 手动 checklist）— **✅ 917/917 PASSED（+4：10 shape_cache_test + 4 drawtext_shape_cache_test 含 R2 `DifferentTexts_DifferentOutput`）**
7. Release `-O3 -Werror` 0 err/warn — **✅ build-bench 全量构建干净；shape_cache_test 10/10 + drawtext_shape_cache_test 4/4 在 Release 下亦 PASS**
8. `benchmarks/baseline/bench_drawtext.json` + README 刷新 + 追加 TASK-20260424-04 历史行 — **✅ baseline.json 10 BMs (+2 new); README K9 新发现行 + 历史表 TASK-04 行 + 当前生成环境日期 2026-04-25**

#### 实施骨架（6 Phase / 12 Task）

| Phase | 内容 | 工时 × 0.6 |
|:-:|---|---:|
| P1 | HashBytesU64 FNV-1a header + 3 单元测试（T8, T9, LengthMatters） | 9 min |
| P2 | ShapeCache 核心 + 7 单元测试（T1-T7，含 T6 碰撞降级） | 24 min |
| P3 | FontHandle/HbBufferHolder 提取 + FontManager::ShapeOrLookup + DrawText 接入 + 3 集成测试 + R2 反向探针 | 36 min |
| P4 | 2 新 BM 骨架（RoundRobin + AllMiss）+ env-toggle `VX_SHAPE_CACHE_OFF=1` pre-baseline 采集 | 18 min |
| P5 | 完整 bench（WSL2 稳态协议：sleep 10s + warm-up + 10 reps）+ 门槛判决 | 24 min |
| P6 | baseline.json + README 刷新 | 12 min |
| **合计** | | **~2h 3m** |

#### 关键成果

- **Warm_Medium：** 3499 → **2350 ns mean / 1877 ns single**（-32.8% / -46.4%；超额门槛 <3200 ns 达 850 ns，意外直破技术刚性 <3000 ns）
- **Warm_Short：** 677 → 311 ns（-54%）；**Warm_Long：** 10573 → 4333 ns（-59%）
- **ctest：** 917/917 PASS（+14 new test cases / 4 new targets）
- **Plan × 0.6：** 第 7 数据点 **0.26×**（「最窄路径」子档第 3 次确认）
- **新规则：** `writing-plans.mdc` §7.1 Cache BM 稳态访问模式数学推演清单（P1 改进建议归档阶段落实）
- **新模式：** `systemPatterns.md` × 3（Env Toggle A/B 对照 / 预提取依赖 Header 原则 / 第三方 API 消除型优化估时下限公式）

</details>

---

<details>
<summary>TASK-20260424-03：SoftwareCanvas::DrawText 真路径 warm 优化 — ✅ 已归档（点开查看历史）</summary>

### TASK-20260424-03：SoftwareCanvas::DrawText 真路径 warm 优化

- **复杂度级别：** Level 2-3（优化类；多候选路径 + 5 设计决策；7 Phase + 2 次 R1 升级 = 7 文件 + 3 新 SIMD header + 1 新 test）
- **状态：** ✅ 已完成（详见 `memory-bank/archive/archive-TASK-20260424-03.md`）
- **反思文档：** `memory-bank/reflection/reflection-TASK-20260424-03.md`（13 段 / 7 改进建议 / plan × 0.6 第 6 数据点 0.34× 最窄档确认）
- **归档文档：** `memory-bank/archive/archive-TASK-20260424-03.md`
- **分支：** `feature/TASK-20260424-03-drawtext-warm-opt`（13 commits 含 archive，`--no-ff` 合并到 main）
- **创建日期：** 2026-04-24
- **来源：** `activeContext.md` 后续任务候选 TASK-20260419-12（TASK-09 K7 拆出，P2 触发型）
- **设计文档：** `docs/specs/2026-04-24-drawtext-warm-opt-design.md` ✅
- **实现计划：** `docs/plans/2026-04-24-drawtext-warm-opt.md` ✅（7 Phase 阶梯骨架 / 130 min plan / plan×0.6 预期 ~78 min / 10 commits 含升级分支）
- **需要创意阶段：** ❌ 否（spec §2 5 决策已锁定；所有架构/算法空白均无；注入点核对表 6/6 可行）
- **安全相关：** ❌ 否（性能优化 / 无外部输入 / 无认证 / 无新依赖）

#### 目标

SoftwareCanvas::DrawText 真路径（FreeType+HarfBuzz）**warm** 5807 ns → **< 3000 ns**（小于 fallback 3647 ns），使真路径默认化具备性能前置条件。来源：TASK-20260419-09 K7 发现，`benchmarks/baseline/bench_drawtext.json` L279 baseline。

| BM | 当前 warm | 目标 | 倍率 |
|---|---:|---:|---|
| `BM_DrawTextReal_Warm_Medium` (19 char) | **5807 ns** | **< 3000 ns** | **≥ 1.94×↓** |
| `BM_DrawTextReal_Warm_Short` (2 char) | 968 ns | 显著改善（hb_buffer 固定开销显化） | 待测 |
| `BM_DrawTextReal_Warm_Long` (124 char) | 16852 ns（136 ns/char 最佳摊还） | 线性摊还下应保持 136 ns/char 或更优 | 兜底 |

#### VAN 阶段基础假设核查（代码实证）

| K7 候选 | 代码路径 | 命题状态 |
|---|---|:-:|
| (a) `hb_buffer` 每次分配 | `software_canvas.cc` L183 `hb_buffer_create()` + L274 `hb_buffer_destroy()` 每次 DrawText 必然成对调用 | ✅ 命题成立 |
| (b) "glyph bitmap 两次拷贝" | **需修正**：warm 路径无两次拷贝（L234-268 直接从 `GlyphBitmap::alpha` blit）；cold 路径才有 FT_Bitmap → Vector<u8> 拷贝（L220-224）| ⚠️ 需 plan 重写 |

#### VAN 阶段额外优化候选（代码实证副产品）

| # | 候选 | 代码位置 | 推测收益 |
|:-:|---|---|---|
| **A** | `hb_buffer` 复用（static / per-canvas） | `software_canvas.cc` L183/L274 | hb_buffer_create/destroy 每次 ~百 ns 级，短字符串显化 |
| **B** | Inner blit loop 优化（去 per-pixel bounds / `/255` 替换 / SIMD） | `software_canvas.cc` L234-268（~20 语句 × W×H glyph 像素） | warm 主耗点；医 /255 → `(x*257+128)>>16` 可去整数除 |
| **C** | `FT_Set_Pixel_Sizes` 状态化缓存 | `software_canvas.cc` L172 每次无条件调用 | 若 FontManager 已跟踪 pixel_size，可省 1 次 face state 写 |
| **D** | GlyphCache Get 双重查询 | `software_canvas.cc` L206 Get → L225 Put → L227 再 Get | 可改 Put 返回插入指针 |
| **E** | FindFont("", 400) 每次字符串 lookup | `software_canvas.cc` L154 | 可 cache default_font_handle |

具体优化顺序、组合、阈值由 `/plan` 头脑风暴定。

#### 前置验证清单

| 维度 | 结果 | 备注 |
|---|:-:|---|
| 依赖可获取性 | ✅ | FreeType/HarfBuzz/google-benchmark 均已集成（TASK-20260414-01 / TASK-20260419-02），无新依赖 |
| 环境就绪 | ✅ | DejaVuSans.ttf 系统文件存在；gcc 11.4 / cmake 3.22.1 / WSL2 |
| 已有 artifact | ✅ | `bench_drawtext.cc` 6 BM（含 warm/cold/short/medium/long） + `baseline/bench_drawtext.json` 已入仓 |
| **FetchContent 代理守卫** | ⊘ 跳过 | 本任务**不触发 FetchContent**（仅用已集成依赖）→ `main.mdc` §1 FetchContent 检查子项不适用 |
| 待处理事项关联 | ✅ | **P1 bench 阈值表绝对增量兜底**（TASK-11 #1）+ **P1 Mixed TDD RED 反向探针**（TASK-11 #3 / TASK-24-01）+ **P1 性能基准必检项**（TASK-05）+ **P1 plan × 0.6 估时校准**（跨 5 任务）全部适用 |

#### 用户决策（/plan 头脑风暴产出）

| # | 维度 | 选择 | 理由 |
|:-:|---|---|---|
| D1 | 优化实施策略 | **方案 1 阶梯验证驱动**（7 Phase）| 复用 TASK-24-01 0.29× 样板；便宜先做 A→C→E→D→B1→B2；累计达标即止 |
| D2 | `hb_buffer` 复用 | **thread_local + RAII** | 零线程风险 + 零 header 污染 + thread exit 自动清理 |
| D3 | Inner blit loop 深度 | **B1 + B2 组合** | /255 乘-移近似 + pre-clip；SIMD 留触发型 P2（ARM/NEON 兼容性开销）|
| D4 | `GlyphBitmap` 结构 | **保持 Vector<u8>** | 0 ABI 改动；data() + row ptr 推进即可 |
| D5 | 验收阈值 | **刚性 < 3000 ns**（用户选严格）| 不设中间带；B1+B2 不达标则 AskQuestion 升 B3 SIMD |

#### Phase 划分（7 Phase 骨架 + 阶梯退出条件）

| Phase | 名称 | plan (min) | plan×0.6 (min) | commits |
|:-:|---|:-:|:-:|:-:|
| 0 | 基线核验 + 工具/API grep | 10 | 6 | 1 |
| 1 | 候选 A — hb_buffer 复用 | 15 | 9 | 1 |
| 2 | 候选 C — FT_Set_Pixel_Sizes 缓存 | 20 | 12 | 1 |
| 3 | 候选 E — FindFont 缓存 | 10 | 6 | 1 |
| 4 | 候选 D — GlyphCache::Put 返回 ptr | 15 | 9 | 1 |
| 5 | 候选 B1 — /255 乘-移近似 + RED 反向探针 | 25 | 15 | 2 |
| 6 | 候选 B2 — Pre-clip + row ptr（**条件触发**）| 20 | 12 | 1 |
| 7 | Baseline 刷新 + MB 构建态收尾 | 15 | 9 | 2 |
| **合计** | — | **130** | **~78** | **10** |
| 升级 B3（条件）| SIMD（x86_64 SSE2-only / SSE2+NEON 双版本）| +40-60 | +25-45 | +1-2 |

**阶梯退出：** 任一 Phase 末 `BM_DrawTextReal_Warm_Medium_mean < 3000 ns` 即跳过后续候选 Phase 直接进 Phase 7。

#### 注入点核对表（spec §4，全部 ✅ 可行，无需扩接口透传）

| 候选 | 注入点 | 状态 |
|---|---|:-:|
| A hb_buffer 复用 | `software_canvas.cc` L183/L274 | ✅ |
| C FT_Set_Pixel_Sizes 缓存 | `software_canvas.cc` L172 + `font_manager.{h,cc}` | ✅ |
| E FindFont 缓存 | `software_canvas.cc` L154 | ✅ |
| D GlyphCache::Put 返回 ptr | `glyph_cache.{h,cc}` + `software_canvas.cc` L225-227 | ✅ |
| B1 /255 替换 | `software_canvas.cc` L253-267 | ✅ |
| B2 Pre-clip + row ptr | `software_canvas.cc` L230-269 | ✅ |

#### 验收标准（plan 阶段精化）

初步（待 plan 细化）：
1. `BM_DrawTextReal_Warm_Medium` 达到 D5 阈值
2. `BM_DrawTextReal_Warm_Short` / `_Long` 回归不差于基线（1.1× 内波动可接受）
3. `BM_DrawTextReal_Cold_Medium` 回归不差于基线（冷路径可能随改动略退化）
4. `bench_imagecache` / `bench_render_*` / `bench_layout_*` 4 bench baseline 无显著退化（10% 内）
5. ctest 全量 PASS（预计 892+ tests）
6. Release `-O3 -Werror` 全量 rebuild 0 err/warn
7. 若引入对「warm 路径正确性」的改动，新增至少 1 条像素断言 GTest（Mixed TDD D3 + RED 反向探针验证）
8. `benchmarks/baseline/bench_drawtext.json` 刷新；`baseline/README.md` 更新 K7 → resolved 状态
9. `techContext.md` Replay-Deepbench 段 K7 段补 resolved + 新 warm 数据表

#### /build 阶段成果（2026-04-24）

**7/7 Phase 完成**，阶梯验证策略 + 2 次 R1 AskQuestion 升级（Phase 6→7 用户选 B3 SSE2；Phase 7 AVX2 实验后用户选智能阈值 dispatch）。

**核心改动（7 文件 / 8 commits 覆盖 Phase 0-7）：**

1. `veloxa/graphics/software/software_canvas.{h,cc}` — Phase 1 hb_buffer thread_local 复用（`HbBufferHolder` RAII）+ Phase 3 default FontHandle 缓存（`cached_default_font_`）+ Phase 6 inner blit pre-clip + row ptr 预计算 + Phase 7 接线 `BlendGlyphRow` runtime dispatch
2. `veloxa/text/font_manager.{h,cc}` — Phase 2 `SetFacePixelSize` 状态化缓存（`FontEntry::ft_pixel_size`）避免重复 `FT_Set_Pixel_Sizes`
3. `veloxa/text/glyph_cache.{h,cc}` — Phase 4 `Put` 返回 `GlyphBitmap*` 消除 Put 后冗余 Get
4. `veloxa/graphics/software/glyph_blend.h` — Phase 5 header-only `DivBy255Approx` + `BlendGlyphPixel`（Phase 5 /255 乘移位试验回退后保留作 SIMD 参考）
5. `veloxa/graphics/software/blit_sse2.h` — Phase 7 SSE2 4 px/iter `BlendGlyphRowSSE2`（pmullw + DivBy255ApproxU16 + packus + 标量 tail）
6. `veloxa/graphics/software/blit_avx2.h` — Phase 7 AVX2 8 px/iter `BlendGlyphRowAVX2`（target attribute + permute4x64_epi64 lane 对齐 + cvtepu8_epi16）+ `BlendGlyphRow` 智能 dispatch（`count >= 16 && __builtin_cpu_supports("avx2")` 才走 AVX2）
7. `tests/graphics/pixel_blend_test.cc` — 11 GTests：DivBy255 精度 / BlendGlyphPixel 契约 / SSE2 4 布局 + zero-alpha + full-opaque + 256 stress / Dispatch AVX2 13 布局 + SSE2 parity 对比，全部 RED 反向探针完整循环验证

**关键 Before/After（同机 Release warm-up + 10 reps 测量）：**

| BM | 修前（baseline） | 修后（Phase 7 AVX2 dispatch） | 改善 |
|---|---:|---:|---|
| `BM_DrawTextReal_Warm_Medium` (19 char) | **5905 ns** | **3499 ns** | **-40.7% / -2406 ns** |
| `BM_DrawTextReal_Warm_Short` (2 char) | 975 ns | 677 ns | -30.6% / -298 ns |
| `BM_DrawTextReal_Warm_Long` (124 char) | 17456 ns | 10573 ns | -39.4% / -6883 ns |
| `BM_DrawTextReal_Cold_Medium` | 52873 ns | 28338 ns | -46.4%（副产品：default font + pixel size 缓存对 cold 路径也有效）|
| `BM_DrawTextFallback_Medium` (对照) | 3813 ns | 3608 ns | -5.4%（basically 噪声，未参与优化）|

**K7 命题评估：**
- **业务前置条件** 真路径 < fallback：✅ **达成**（3499 ns < 3608 ns，快 3%；warm-up + 10 reps 稳定 CV ≤ 1% 非噪声）
- **技术刚性目标** D5 `< 3000 ns`：⚠️ 未达（差 499 ns / 14%）— 两次 R1 AskQuestion 升级后仍差；用户知情接受
- **阶梯验证实证**：Phase 1-4 累计微幅改善（-2% 量级）；**Phase 6 blit 内层优化** 单独贡献 -12%；**Phase 7 SSE2 SIMD** 单独贡献 -29% → 印证 spec 对「inner blit loop 是 warm hot path 大头」的假设；**Phase 5 /255 乘移位试验回退** 印证 GCC Granlund-Montgomery 自动降级已是最优，手写近似不能无脑替换
- **AVX2 在当前 glyph 宽度分布下无可见收益**（ASCII 6-12 px 偏小），改为 `count >= 16` 智能阈值只对 CJK / 大字号启用，为未来硬件进化保留 headroom

#### Phase 耗时对比（plan × 0.6 协议校准）

| Phase | plan (min) | 实测 (min) | 比例 |
|:-:|:-:|:-:|:-:|
| 0 | 10 | ~3 | 0.30× |
| 1-3 (A+C+E 批) | 45 | ~18 | 0.40× |
| 4 (D) | 15 | ~4 | 0.27× |
| 5 (B1 试验回退) | 25 | ~12 | 0.48× |
| 6 (B2) | 20 | ~6 | 0.30× |
| 7 (B3 SSE2 + AVX2 dispatch) | 40-60（含升级分支）| ~30 | 0.50-0.75× |
| finalize | 15 | ~5 | 0.33× |
| **合计** | **170-190** | **~78** | **~0.42×** |

**第 6 个数据点**（继 TASK-05 0.34× / TASK-09 0.42× / TASK-11 0.15-0.20× / TASK-13 0.17-0.19× / TASK-24-01 0.29×）：性能优化类任务 plan × 0.6 协议稳定落在 **0.20-0.50×**，本任务 0.42× 中位数，含 2 次 R1 升级分支（Phase 7 SSE2 + AVX2）时间仍可控。

---

</details>

<details>
<summary>TASK-20260424-01：Layout super-linear knee 根因调查 — ✅ 已归档（点开查看历史）</summary>

### TASK-20260424-01：Layout super-linear knee 根因调查

- **复杂度级别：** Level 2-3（研究/调查类；**可能**产出小型性能补丁或 arena 默认配置调整）
- **状态：** ✅ 已完成（详见 `archive-TASK-20260424-01.md`）
- **分支：** `feature/TASK-20260424-01-layout-knee-root-cause`（7 commits 含 archive）
- **回顾文档：** `memory-bank/reflection/reflection-TASK-20260424-01.md`
- **归档文档：** `memory-bank/archive/archive-TASK-20260424-01.md`

#### Build 阶段成果

**根因定位：(d) ArenaAllocator 4KB block malloc/free churn**（Phase 2 block-size 扫描实证 32K 为 sweet spot；65K 在 Flex 回弹暗示 L1D 抖动，为 K8 新发现）。

**核心改动（3 文件 / 5 commits）：**

1. `veloxa/foundation/memory/arena_allocator.h` — 默认 block_size 4096 → 32768（+20 行注释含完整扫描表 + 根因追溯）
2. `tests/foundation/memory/arena_allocator_test.cc` — 新增 `DefaultBlockSizeFitsLargeAllocations` GTest（23 行，RED 反向探针验证有效）
3. `benchmarks/baseline/bench_layout_{buildtree,flex}.json` + `README.md` — 入仓新 baseline（3-reps mean，Release 同机）+ K2/K3/K8 更新

**关键 Before/After（3-reps mean）：**

| BM | 修前 | 修后 | 改善 |
|---|---:|---:|---|
| `BM_LayoutBuildTreeFlat/256` | 70 µs | **42.3 µs** | **1.66×** |
| `BM_LayoutBuildTreeFlat/512` | 196 µs | 140 µs | 1.40× |
| R256（knee 强度） | 9.42× | **4.18×** | **2.25× ↓** |
| `BM_LayoutFlex<16,16>` | 82.5 µs | **44.2 µs** | **1.87×** |
| R_flex | 16.49× | **6.40×** | **2.58× ↓** |

内存成本：`Document::arena_` +28 KB/instance（仅 Document 用默认 block；其它 3 处 `UpdateManager` / `LayoutEngine` / bench 显式传值不受影响）。

#### 验收 10/10

| # | 判据 | 结果 |
|:-:|---|:-:|
| 1 | Phase 1 判据命中（R256 < 6×，(d) 至少部分成立）| ✅ R256=3.61× (65K 探针) |
| 2 | Phase 2 定位 OPT_SIZE | ✅ 32768（Flex sweet spot） |
| 3 | R256 改善达标 | ⚠️ 4.18×（plan 阈值≤2.5 过严，实证调整，用户确认）|
| 4 | R_flex 改善达标 | ⚠️ 6.40×（plan 阈值≤5 过严，同上）|
| 5 | ctest 全量 PASS | ✅ 892/892 |
| 6 | Release 全量 rebuild 0 err/warn | ✅ |
| 7 | 新增 GTest + RED 反向探针 | ✅ revert→FAIL, restore→PASS |
| 8 | 2 baseline JSON 刷新 release | ✅ |
| 9 | baseline README 环境 + 历史更新 | ✅ K2/K3/K8 段 + 变更历史行 |
| 10 | techContext Layout 性能基线段补 resolved | ✅ K2/K3 resolved + K8 新发现 |

#### Phase 耗时对比（plan × 0.6 协议校准）

| Phase | plan (min) | 实测 (min) | 比例 |
|:-:|:-:|:-:|:-:|
| 0 | 10 | ~3 | 0.30× |
| 1 | 20 | ~4 | 0.20× |
| 2 | 25 | ~5 | 0.20× |
| 3 | 25 | ~8 | 0.32× |
| 4 | 20 | ~5 | 0.25× |
| 5 | 15 | ~8 | 0.53× |
| **合计** | **115** | **~33** | **0.29×** |

**第 5 个数据点**（继 TASK-05 3.4× / TASK-09 4.2× / TASK-11 1.5-2.0× / TASK-13 1.67-1.86×）：研究型小补丁 0.29× 属于**最快档**，可作为「脚本化实验 + 单行改动」样板。
- **创建日期：** 2026-04-24
- **来源：** 候选区 TASK-20260419-10（TASK-05 K2/K3 + TASK-09 VAN 拆出，activeContext 标为 P2 触发型）
- **设计文档：** `docs/specs/2026-04-24-layout-knee-root-cause-design.md` ✅
- **实现计划：** `docs/plans/2026-04-24-layout-knee-root-cause.md` ✅（6 Phase 骨架 / 115 min plan / plan × 0.6 预期 ~70 min / 5 commits 含 Phase 1B 升级分支）
- **需要创意阶段：** ❌ 否（用户决策 D1=A 阶梯验证 + D2=A 全局 bump 已锁定；所有架构/算法空白均无）
- **安全相关：** ❌ 否（性能测量/优化任务，无外部输入/无认证/无新依赖）

#### 用户决策（Plan 头脑风暴产出）

| # | 维度 | 选择 | 理由 |
|:-:|---|---|---|
| D1 | 实验策略 | **A — 阶梯验证** | 先测最可能 (d) malloc churn（实验成本 < 5 min），命中即止步；否则升 Phase 1B |
| D2 | 修复交付范围 | **A — 全局 ArenaAllocator 默认 bump** | grep 穷举 4 处使用点：仅 `Document::arena_` 受影响（+12KB/instance 可接受） |

#### Phase 划分（6 Phase 骨架）

| Phase | 名称 | plan (min) | plan × 0.6 (min) | commits |
|:-:|---|:-:|:-:|:-:|
| 0 | 基线核验 + smoke 工具 + 分支 | 10 | 6 | 1 |
| 1 | 假设 (d) 单点探针 65536 | 20 | 12 | 0 |
| 2 | block size 5 档扫描 | 25 | 15 | 0 |
| 3 | 实施 fix + RED 反向探针 | 25 | 15 | 2 |
| 4 | Bench baseline 刷新 | 20 | 12 | 1 |
| 5 | techContext + MB 收尾 | 15 | 9 | 1 |
| **合计** | — | **115** | **~70** | **5** |
| **1B（升级，条件触发）** | per-phase 拆分 BM | +60-90 | +40-55 | +1-2 |

#### 基线实测（VAN Phase，main `e3952dc`）

| BM | Time | items/s |
|---|---:|---:|
| `BM_LayoutBuildTreeFlat/8`   | 534 ns | 15.0M |
| `BM_LayoutBuildTreeFlat/16`  | 982 ns | 16.3M |
| `BM_LayoutBuildTreeFlat/32`  | 1871 ns | 17.1M |
| `BM_LayoutBuildTreeFlat/64`  | 3906 ns | 16.4M |
| `BM_LayoutBuildTreeFlat/128` | 7901 ns | 16.2M |
| **`BM_LayoutBuildTreeFlat/256`** | **76375 ns** | **3.4M** ← knee |
| `BM_LayoutBuildTreeFlat/512` | 208027 ns | 2.5M |

**knee 命题：** N=128→256 时 9.67× for 2×N（预期 2×），throughput 跌 ~4.8×。

#### VAN 已实证（落实 P0「方案根因假设未先验证」规则）

| 候选根因 | 状态 | 实证依据 |
|---|:-:|---|
| (a) `Vector<LayoutBox*> children` 扩容 | ❌ 推翻 | `layout_box.h:26-29` 用侵入式 `first_child/next_sibling` 双向链表 |
| (b) `ArenaAllocator` chunk 2× 增长 | ❌ 推翻 | `arena_allocator.h:13,39` 固定 4096 字节 block，TASK-09 VAN 已否定 |
| (c) L2 cache 溢出 | ❌ 推翻 | N=256 工作集 142KB << L2 1280KB |
| (d) Arena 4KB block malloc churn | 🔬 保留 | N=256 每迭代 ~37 次 malloc+free，需实验验证 |
| (e) L1D 抖动 | 🔬 保留 | 单元素 552B > cache line 跨度，需 perf 验证 |
| (f) layout 算法内部 O(N²) | 🔬 保留（低概率） | grep 未见显式 O(N²)，但需 per-phase 拆分实证 |

#### 前置验证（全部 ✅）

| 维度 | 结果 |
|---|---|
| 依赖可获取性 | ✅ google/benchmark 在 `build-bench/_deps/` 已缓存，FetchContent 不触发 → P0 git proxy 不触发 |
| 环境就绪 | ✅ `build-bench/` Release 构建可用；`bench_layout_buildtree` + `bench_layout_flex` 可直接运行 |
| 已有 artifact | ✅ `benchmarks/baseline/bench_layout_{buildtree,flex}.json` 已入仓（TASK-05） |
| 待处理事项关联 | ✅ 落实候选区 TASK-20260419-10（P2 触发型） |
| Sticky ID 一致性 | ⚠️ 原候选区 ID = TASK-20260419-10；本次按 Memory Bank 约定「当天序号从 01 开始」用 TASK-20260424-01，在任务描述中交叉引用原 ID |

#### 关键数据（VAN Phase 沉淀）

- 环境：Linux 6.6.87.2 WSL2 / 8 CPU @ 2.92 GHz / L1D 48KB(x4) / L2 1280KB(x4) / L3 12288KB / gcc 11.4
- `sizeof(LayoutBox)`：**144 字节**
- `sizeof(ComputedStyle)`：**408 字节**（18 `LengthValue` + transitions `SmallVector<_,2>` 等）
- 每元素内存：**552 字节**

#### 后续工作流

- `/plan`：设计 2-3 个可证伪实验（block-size 扫描 / per-phase 拆分 / perf stat），给出验收判据
- `/creative`：**视情况**（若需要在多个修复方案间做架构决策）
- `/build`：实施 fix 或输出研究报告
- `/reflect` + `/archive`

</details>

<details>
<summary>TASK-20260419-13：流程规则 P0/P1 沉淀冲刺（3 条积压条目一次性闭环） — ✅ 已归档（点开查看历史）</summary>

### TASK-20260419-13：流程规则 P0/P1 沉淀冲刺（3 条积压条目一次性闭环）

- **复杂度级别：** Level 2（6 个规则/命令文件 + 3 MB 文件 = 9 文件修改；遵循 `writing-plans.mdc` 已有段式样；纯文档无代码）
- **状态：** ✅ 已完成（已 `--no-ff` 合并到 main `8a436ed`，详见 `archive-TASK-20260419-13.md`）
- **回顾文档：** `memory-bank/reflection/reflection-TASK-20260419-13.md`
- **归档文档：** `memory-bank/archive/archive-TASK-20260419-13.md`
- **分支：** `feature/TASK-20260419-13-process-rules-sunk-in`（共 8 commits 含 archive；已删除）
- **设计文档：** `docs/specs/2026-04-19-process-rules-sunk-in-design.md` ✅
- **实现计划：** `docs/plans/2026-04-19-process-rules-sunk-in.md` ✅（5 phase / 85-95 min 预估 / 实际 ~51 min = 1.67-1.86× plan）
- **需要创意阶段：** ❌ 否（3 条目均遵循 `writing-plans.mdc` 已有段模式，零架构/UI/算法决策）
- **安全相关：** ❌ 否（纯流程规则文档）

#### 3 条规则落地

| # | 优先级 | 来源 | 目标文件 | 状态 |
|:-:|:-:|---|---|:-:|
| 1 | 🔴 P0 | 反复 9+ 次 | `writing-plans.mdc` L96 + `van.md` §1 + `techContext.md` L98 | ✅ |
| 2 | 🟠 P1 | TASK-11 反思 #2 | `writing-plans.mdc` §4 末尾子块 L255 | ✅ |
| 3 | 🟠 P1 | TASK-03 Round 1 首发 | `complexity-levels.mdc` L68 + `build.md` §6.5 + `reflect.md` §0 | ✅ |

#### 核心成果

- **反例追溯 7/7 通过**（含 meta-dogfooding Phase 0 实时自证：`rg`/`jq`/`valgrind`/`xmllint` MISS 触发条目 2 规则）
- **10 验收标准 9 ✅ + 1 改进**（§5.4 工具表合并同表兜底，Phase 0 实证驱动加入 rg）
- **跨类型估时收敛 plan × 0.6 通用协议**（TASK-05/09/11/13 四数据点）
- **5 新模式沉淀 `systemPatterns.md`**（+106 行）：Meta-dogfooding Phase 0 / 基础假设核查 / 单一真相来源占位符 / 实证微调 spec / bench 估时校准扩展跨类型
- **3 条 activeContext 长期项标记已落实** + 新增 P1×3 + P2×3 待处理（P2×2 已沉淀 systemPatterns，P2 #6 .editorconfig 观察）
- **Plan 阶段关键认知升级**：`.cursor/commands/*.md` 可编辑性假设纠正，ROI 放大 2-3×

#### 4 核心决策

| # | 维度 | 选择 |
|:-:|---|---|
| D1 | 代理地址 | 占位符 `<开发环境代理地址>`（零硬编码 IP，`techContext.md` 单一真相来源）|
| D2 | 子状态实现 | `构建中·轮次 N 完成` 子状态标签（保 5 主阶段骨架不变）|
| D3 | Phase 提交粒度 | 每 Phase 1 commit（实际 4 commits，P0 并入 P1）|
| CU | `.cursor/commands/*` 可编辑 | Plan 阶段 Glob 验证纠正 → ROI 放大 2-3× |

#### 提交清单（8 commits + 1 merge）

| # | SHA | 主题 |
|:-:|-----|------|
| 1 | `ec78f1c` | docs(van): TASK-20260419-13 init process rules sunk-in sprint |
| 2 | `db833ce` | docs(plan): TASK-20260419-13 process rules sunk-in design + plan |
| 3 | `76ed4e0` | docs(rules): P1 — FetchContent proxy guard (P0) |
| 4 | `b8ca9b4` | docs(rules): P2 — smoke toolchain grep (P1) |
| 5 | `60d047c` | docs(rules): P3 — multi-round build interim state (P1) |
| 6 | `020e574` | docs(rules): P4 — finalize rules sunk-in sprint |
| 7 | `ccfce04` | docs(reflect): add reflection for TASK-20260419-13 |
| 8 | `a9fb547` | docs(archive): add archive for TASK-20260419-13 |
| 9 | `8a436ed` | Merge branch 'feature/...' into main (`--no-ff`) |

</details>

<details>
<summary>TASK-20260419-11：ImageCache::Load HashMap 化（K6 高 ROI 优化） — ✅ 已归档（点开查看历史）</summary>

### TASK-20260419-11：ImageCache::Load HashMap 化（K6 高 ROI 优化）

- **复杂度级别：** Level 2（多文件：image_cache.{h,cc} + tests + bench 复跑验证；机械替换 + 数据双索引设计；无 UI/架构空白）
- **状态：** ✅ 已完成（已合并到 main `8515c25`，详见 `archive-TASK-20260419-11.md`）
- **回顾文档：** `memory-bank/reflection/reflection-TASK-20260419-11.md`
- **归档文档：** `memory-bank/archive/archive-TASK-20260419-11.md`
- **分支：** `feature/TASK-20260419-11-imagecache-hashmap`（共 8 commits 含 archive；已 `--no-ff` 合并到 main）
- **回顾文档：** `memory-bank/reflection/reflection-TASK-20260419-11.md`
- **设计文档：** `docs/specs/2026-04-19-imagecache-hashmap-design.md` ✅
- **实现计划：** `docs/plans/2026-04-19-imagecache-hashmap.md` ✅（4 phase / 1 GTest 新增 / ~55-80 min plan 估时 / 5 commits 含 VAN）
- **创建日期：** 2026-04-19
- **来源：** TASK-20260419-09 K6 拆出（`ImageCache::Load` hit 路径 O(N) 字符串扫描；Hit<256> 1151.77 ns > `ReplayImageReal<16>` 595 ns）；候选区已立项 P1 高 ROI
- **预期工作量：** ~1-2h（含 baseline 同机对比 + 单测 + bench 复跑确认 K6 命题已解）

#### 4 决策（D1-D4，plan 阶段头脑风暴用户确认）

| # | 维度 | 选择 | 理由 |
|---|------|------|------|
| D1 | HashMap key 类型 | **owned `String`** + 自定义 `StringHash` + `StringEq` | VAN F2 + plan 补 grep（`string.h:30-56` SSO 内联确认）：StringView key 在 vector resize 时悬挂；`std::equal_to<String>` 类型推导不确定 → 自定义 functor 兜底 |
| D2 | HashMap 初始容量 | 沿用 `kDefaultCapacity = 16`，不 reserve | 生产场景不频繁增长；TASK-09 K6 baseline 极端 256 entry 仅 ~4 次 rehash，分摊代价远低于命中收益 |
| D3 | 边界测试 | 加 1 个 `ClearAndReloadDeduplicates` GTest（~5 行） | 双索引最易出 bug 点 = `Clear()` 漏清 HashMap → 此测试是「双索引同步」回归安全网 |
| D4 | Phase 划分 | **4 phase**（细粒度提交）：P1 仅加字段 / P2 切 Load 路径 + 新测试 / P3 bench + baseline / P4 文档 | Incremental 改造模式；P1 引入字段不影响行为，「数据结构改造与行为切换分离」便于独立 review/回滚 |

#### Phase 划分（4 phase，详见 plan §2）

| Phase | 估时 | 文件 | 提交主题 |
|-------|------|------|---------|
| 1 | 10-15 min | image_cache.h（加字段 + functor） | wip phase-1 |
| 2 | 20-30 min | image_cache.cc（切 Load + Clear）+ test（+1 用例） | feat phase-2 |
| 3 | 15-20 min | bench 同机复跑 + baseline JSON + 2 README | docs phase-3 |
| 4 | 10-15 min | techContext + MB 收尾 | chore phase-4 |

#### 核心目标

将 `vx::image::ImageCache` 的 `Load(path)` hit 路径从 O(N) 字符串线性扫升级为 O(1) HashMap 查找，**同时保持现有 ABI 不变**（`gfx::ImageHandle` 仍是 1-based vector 下标，`Get(handle)` 保持 O(1)）。

| 指标 | 当前（baseline） | 目标 |
|------|----------------|------|
| `BM_ImageCacheLoad_Hit<1>` | 10.35 ns | 保持 ~10-30 ns |
| `BM_ImageCacheLoad_Hit<16>` | 50.87 ns | **降到 ~10-30 ns**（5×↓） |
| `BM_ImageCacheLoad_Hit<256>` | 1151.77 ns | **降到 ~10-30 ns**（38-115×↓） |
| `BM_ImageCacheLoad_Miss` | （不变，由 DecodeFromFile 主导） | 不退化 |
| `BM_ImageCacheGet` | （不变） | 不退化 |

#### VAN 阶段重大发现（5 项 grep 实证，落实「方案根因假设未先验证」P0 #4 完整应用）

| # | 命题 | grep 实证 | 影响设计 |
|---|------|----------|---------|
| F1 | 「`gfx::ImageHandle` 可改为不透明 token」 | ❌ 错。`graphics/image.h:49` `using ImageHandle = u32;` + `image_cache.cc:30` `&images_[handle - 1].image` — handle **必须保持 1-based vector 下标**否则破坏 `Get` O(1) 语义 + ABI | 必须**双索引设计**：保留 `Vector<Entry>` 提供 handle→image O(1) + 新增 `HashMap<String, Handle>` 提供 path→handle O(1) |
| F2 | 「`std::equal_to<String>` 默认可用」 | ⚠️ 风险。`string.h:178/190` 仅有 `operator==(BasicString, StringView)` + 反向，**无** `operator==(BasicString, BasicString)` 直接版本（依赖隐式转换）— `std::equal_to<String>` 实例化是否成功需 plan 阶段试编译验证 | 设计 `StringEq` 自定义 functor（仿 `property.cc:94 StringViewEq`）作为兜底 |
| F3 | 「需要新写 djb2 hash」 | ❌ 错。`property.cc:84-92 StringViewHash` 已是现成 djb2，可机械复刻为 `StringHash`（仅参数类型 `StringView` → `const String&`） | hash functor 工作量 ≈ 5 行复刻 |
| F4 | 「修改可能影响多个调用方」 | ❌ 错。`application.cc:67/118` 是**唯一调用方**（仅作为指针字段 + `&image_cache_` 引用传递）— 无 handle 数值递增/连续性的外部依赖 | 回归风险接近零 |
| F5 | 「需新加 dedup 测试用例」 | ❌ 错。`image_cache_test.cc:55-65 DeduplicateSamePath` 已覆盖 — 改造后必须保持「同 path Load 返回相同 handle」契约即可 | 现有 3 个测试用例（LoadAndGet / DeduplicateSamePath / LoadInvalidPath）已充分覆盖正确性 |

#### 前置验证（全部 ✅）

| 维度 | 结果 |
|---|---|
| 依赖可获取性 | ✅ `HashMap<K,V,Hash,Eq>` API 完整（`hash_map.h` Insert/Find/Erase/clear/reserve）；djb2 hash 模板（`property.cc:84`）已现成 |
| 环境就绪 | ✅ `build/` + `build-bench/` 复用，无 FetchContent → P0 git proxy 不触发 |
| 已有 artifact | ✅ `bench_imagecache.json` baseline 已入仓（TASK-09），改造后可直接同机对比 |
| 调用方影响 | ✅ 仅 `application.cc` 1 处持有指针，无 handle 数值依赖 |
| 测试覆盖 | ✅ 3 个 GTest 已覆盖核心契约（dedup 是 K6 改造关键回归点，已覆盖） |
| 待处理事项关联 | ✅ 落实 candidate TASK-11（候选区 P1 高 ROI）；与 TASK-10/12 不冲突 |
| Sticky ID 一致性 | ✅ 候选区 ID = TASK-20260419-11，本任务沿用 |

#### 验收标准（构建完成，10/10 ✅）

1. ✅ 双索引数据结构（`Vector<Entry>` + `HashMap<String, ImageHandle, StringHash, StringEq>`）实现，handle 保持 1-based vector 下标
2. ✅ `ImageCache::Load` hit 路径全部走 HashMap O(1)，O(N) 字符串扫被消除（Hit<256> 1151.77 → 45.70 ns，**25.2×↓**）
3. ✅ 现有 3 个 GTest 全部通过（含 `DeduplicateSamePath` dedup 契约）
4. ✅ ctest 全量回归 **891/891** 通过（新增 `ClearAndReloadDeduplicates` 1 项，D3 RED probe 验证有效）
5. ✅ Release `-O3 -DNDEBUG` 通路（`build-bench/`）零编译失败 / 零 -Wall 警告
6. ✅ `bench_imagecache` 同机复跑：Hit<16> = 44.05 ns（< 50 ns ✓）；Hit<256> = 45.70 ns（< 100 ns ✓）— K6 量化命题完全解
7. ✅ Miss 3833 → 3344 ns（不退化）；Get 0.94 → 1.16 ns（噪声级，实现未改动）；ReplayImageReal<16/64> 持平
8. ✅ baseline JSON 重生成入仓（带 repetitions=3 / mean+median+stddev/cv，library_build_type=release），baseline/README 加 K6-resolved 行 + 历史项；benchmarks/README ImageCache 量级行已更新
9. ✅ techContext.md「Replay-Deepbench 性能基线」段 K6 状态从"待优化"改为"已解决"，含双索引架构 + 实测对比数字
10. ✅ Memory Bank 更新（tasks/activeContext/progress 推进到「构建完成」；K6 候选标记结案；1-2h 实绩待 `/reflect` 评估，目前实测 P1+P2+P3+P4 ~35-40 min vs plan 估 ~55-80 min）

#### 安全相关

否（性能优化任务，无外部输入新增/无认证路径/无新依赖；`HashMap` 已是 vx_core 内部成熟容器）。

</details>

<details>
<summary>TASK-20260419-09：Replay hot path 深度基准 + 真 ImageCache 通路（A+B 子集） — ✅ 已归档（点开查看历史）</summary>

### TASK-20260419-09：Replay hot path 深度基准 + 真 ImageCache 通路（A+B 子集）

- **复杂度级别：** Level 2-3（2 个新 bench exe + 复用 layout_corpus.h + 2 baseline JSON 入仓 + README 更新）
- **状态：** ✅ 已完成（已合并到 main，详见 `archive-TASK-20260419-09.md`）
- **回顾文档：** `memory-bank/reflection/reflection-TASK-20260419-09.md`
- **归档文档：** `memory-bank/archive/archive-TASK-20260419-09.md`
- **设计文档：** `docs/specs/2026-04-19-replay-deepbench-imagecache-design.md`
- **实现计划：** `docs/plans/2026-04-19-replay-deepbench-imagecache.md`（5 phase / 15 BMs / **plan 估 3.5h / 实际 ~50 min** / 7 commits）

#### 5 决策（D1-D5，plan 阶段头脑风暴用户确认）

| # | 维度 | 选择 |
|---|------|------|
| D1 | DrawText 层次 | **A1+A2**：A1 直接 `SoftwareCanvas::DrawText` 微基准（fallback / Real_Cold / Real_Warm）+ A2 端到端 Replay TextHeavy 真 vs fallback 对比 |
| D2 | 文本维度 | 短(2)/中(19)/长(124) char + glyph cache cold/warm 分组（量化 cache 收益） |
| D3 | ImageCache 维度 | B1 Load hit/miss × cache size {1, 16, 256}（验 O(N) 扫）+ B2 端到端真路径 N={16, 64} + B3 Get 单次 |
| D4 | fixture 策略 | 多张 distinct PNG（setup 写 256 张 1×1 RGBA 到 `/tmp/vx_bench_<pid>_<i>.png`，path 唯一避免 hit 污染） |
| D5 | Phase 划分 | 5 phase（P1 corpus 扩 + 2 smoke / P2 bench_imagecache / P3 bench_drawtext / P4 baseline JSON + README / P5 techContext + MB 收尾） |

#### Phase 划分（5 phase，详见 plan §1-5）

| Phase | 时间 | 文件 | BM 数 | 提交主题 |
|-------|------|------|-------|---------|
| 1 | 30 min | layout_corpus.h 扩展 + 2 smoke .cc + CMakeLists | 2 smoke | wip phase-1 |
| 2 | 50 min | bench_imagecache.cc 全套 | 7 BMs | feat phase-2 |
| 3 | 60 min | bench_drawtext.cc 全套 | 8 BMs | feat phase-3 |
| 4 | 30 min | 2 baseline JSON + 2 README | 0 | docs phase-4 |
| 5 | 30 min | techContext + systemPatterns + MB 收尾 | 0 | docs + chore phase-5 |

#### 验收标准（design §7 完整版，10 项）

1. ✅ 2 bench exe Release build 0 errors（`build-bench/`，phase-1 + phase-2 + phase-3 各确认）
2. ✅ bench_drawtext 8 BMs + bench_imagecache 7 BMs，全 exit 0
3. ✅ smoke 三件套全过（每 BM 数字非零 + `SetItemsProcessed > 0` + JSON `items_per_second > 0`，phase-1/2/3 全验证）
4. ✅ 11 现存 + 2 新 = 13 bench targets 共存零冲突（CMake 重 configure 全过）
5. ✅ 2 baseline JSON 入仓（`benchmarks/baseline/bench_drawtext.json` + `bench_imagecache.json`，default `--benchmark_min_time` ~0.5s，library_build_type=release）
6. ✅ `benchmarks/README.md` exe 表 11→13；新增 K1' / K1'' / K6 / K7 量级行；run-all 11→13；baseline 列表 7→9
7. ✅ `memory-bank/techContext.md` 新增「Replay-Deepbench 性能基线」段 + Render 基线 K1 修正归因；`systemPatterns.md` Render Bench 前置清单加 DrawText fallback gate（隐式契约 2→3）
8. ✅ Debug ctest 不引入回归（仅新增 bench exe，不触动核心源码 / 库 / 测试）
9. ✅ **K1 命题判定**（fallback 192 ns/char / cold real 2777 ns/char / warm real 305 ns/char — 完整三路证据）
10. ✅ **K5 命题判定**（K6 新发现取代：B2 ReplayImageReal 37 ns/cmd 线性 vs B1 Load hit/256 1162 ns 是真瓶颈）

#### 不需要 `/creative` 阶段

理由：所有设计决策已在 plan 阶段头脑风暴 D1-D5 完成；无 UI/算法/架构空白。

#### 元数据

- **分支：** `feature/TASK-20260419-09-replay-deepbench`（基于 main `bfe44ae`）
- **创建日期：** 2026-04-19
- **归档日期：** 2026-04-19
- **来源：** TASK-20260419-05 K1（DrawText 真路径未验证）+ K5（ImageCache 三阶段链断）触发；候选区已立项 P1

#### 范围决策（VAN /van 阶段拆分，2026-04-19）

原候选 TASK-09 包含 3 个独立子目标，VAN 范围检查后**拆为 2 任务**：

| 子目标 | 本次范围 | 原因 |
|---|---|---|
| A. DrawText 真路径微基准（FreeType + HarfBuzz + glyph_cache 拆解；fallback vs 真路径对比） | ✅ 本次做 | bench 类，与 TASK-05 同模式可复用 layout_corpus.h |
| B. 真 ImageCache 通路基准（Load + Get + cache hit/miss + nullptr 对比） | ✅ 本次做 | bench 类，同 A |
| C. Layout super-linear knee 根因（K2 + K3） | ❌ 拆为 TASK-20260419-10 | 异质（研究/调查类，可能产出 layout 算法重构 PR），混做会拉长任务且模式不统一 |

#### VAN 阶段重大发现（推翻 K1/K5 假设，落实「方案根因假设未先验证」P0 #3 提前止损）

| # | 原假设（TASK-05 K1/K5） | VAN grep 实证 | 影响 |
|---|---|---|---|
| F1 | K5：「ImageCache 真路径需 fixture 文件复制（`configure_file()` 拷 PNG）」 | ❌ 错。`tests/core/image/image_decoder_test.cc:12-41` 已有 `CreateTestPng()` 用 libpng 程序化构造 PNG 写 `/tmp/vx_test_<pid>.png` | B 子目标实现简化 ~1h（不用 cmake fixture 工程） |
| F2 | K1：「DrawText 8200 ns/cmd 是 FreeType+HarfBuzz 真路径」 | ⚠️ 可能误判。`SoftwareCanvas::DrawText` line 145 在 `font_manager==nullptr \|\| glyph_cache==nullptr` 时 fallback 到 `DrawTextFallback`（逐字符 FillRect）；当前 bench 不传 font_manager → **测的实际是 fallback 路径** | A 子目标核心命题改为「fallback vs 真路径谁是 820× 根因」，拆解维度更细 |
| F3 | K2/K3：「ArenaAllocator chunk grow 是 super-linear 根因」 | ❌ 错。`ArenaAllocator` 默认 4096 byte/block 不 grow（新 block 仍 4096）；malloc 4096 块 ~µs 级，无法解释 7.7→70 µs 的 10× 跳变 | C 子目标已拆为 TASK-10，候选根因表需重写 |
| F4 | TTF 字体 fixture 来源 | ✅ `/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf` 已被 font_manager_test 验证可用 | A 子目标无需打包字体 |
| F5 | 依赖可获取性 | ✅ FreeType + HarfBuzz + libpng 已在 vx_text/vx_core 链接 | 不触发 FetchContent → P0 git proxy 不触发 |

#### 前置验证（全部 ✅）

| 维度 | 结果 |
|---|---|
| 依赖可获取性 | ✅ FreeType / HarfBuzz / libpng 已在 vx_text / vx_core；DejaVuSans.ttf 系统已装 |
| 环境就绪 | ✅ `build-bench/` 复用，无 FetchContent → P0 git proxy 不触发 |
| 已有 artifact | ✅ TASK-05 4 bench 已在 main；新增 2 bench 文件名（`bench_drawtext.cc` / `bench_imagecache.cc` 或同等命名）不冲突 |
| 待处理事项关联 | ✅ 落实 candidate TASK-09（候选区 P1）；推 TASK-10（候选） |
| Sticky ID 一致性 | ✅ 候选区 ID = TASK-20260419-09，本任务沿用 |

#### 关键发现（4 项 → /reflect 输入）

| # | 发现 | 数值 | 后续 |
|---|------|------|------|
| K1' | TASK-05 「DrawText 8200 ns/cmd 是 FT+HB 真路径」**修正归因** | fallback 192 ns/char ≈ FillRect ×19；"820×"是 per-cmd 工作不可比性 | 已写入 techContext + systemPatterns DrawText fallback gate 隐式契约 |
| K1'' | DrawText 真路径**冷路径**才是 hot | `Real_Cold_Medium` 52763 ns / 19 char（**14× of fallback / 9.1× of warm**） | glyph_cache 已是 ROI 极高存量优化 |
| K6 | `ImageCache::Load` hit 路径 O(N) 字符串扫；size=256 时 1162 ns | hit/1 9.99 ns → hit/256 1162 ns（116×）；**比 ReplayImageReal<16> 595 ns 还慢** | 立 **TASK-20260419-11**（候选 P1 高 ROI）：HashMap 改造 |
| K7 | warm 真路径 5807 ns > fallback 3647 ns（1.6×） | hb_shape 固定开销 + glyph bitmap memcpy > 19 个 FillRect | 立 **TASK-20260419-12**（候选 P2 触发型）：hb_buffer 复用 + 直接 raster |

#### 提交清单（7 commits, on `feature/TASK-20260419-09-replay-deepbench`）

| # | SHA | 主题 |
|---|-----|------|
| 1 | `71e01ab` | docs(plan): TASK-20260419-09 replay deepbench design + plan |
| 2 | `f767231` | wip(TASK-20260419-09): phase-1 register bench_drawtext + bench_imagecache smoke |
| 3 | `c19dc97` | feat(bench): add bench_imagecache full suite (TASK-20260419-09 phase-2) |
| 4 | `8e55337` | feat(bench): add bench_drawtext full suite + K1 verdict (TASK-20260419-09 phase-3) |
| 5 | `913bf01` | docs(bench): TASK-20260419-09 phase-4 baselines + READMEs |
| 6 | `8cdb36c` | chore(build): finalize TASK-20260419-09 memory bank state (phase-5) |
| 7 | `6e0f775` | docs(reflect): add reflection for TASK-20260419-09 |

#### 安全相关

否（性能测量任务，无外部输入/无认证/无新依赖）。

</details>

<details>
<summary>TASK-20260419-05：Layout + Render 性能基准（4 个 bench exe） — ✅ 已归档（点开查看历史）</summary>

### TASK-20260419-05：Layout + Render 性能基准（4 个 bench exe）

- **复杂度级别：** Level 2-3（4 文件新建 + CMakeLists 更新 + README + 4 baseline JSON 入仓）
- **状态：** ✅ 已完成（已合并到 main，详见 `archive-TASK-20260419-05.md`）
- **回顾文档：** `memory-bank/reflection/reflection-TASK-20260419-05.md`
- **归档文档：** `memory-bank/archive/archive-TASK-20260419-05.md`
- **设计文档：** `docs/specs/2026-04-19-layout-render-benchmarks-design.md`
- **实现计划：** `docs/plans/2026-04-19-layout-render-benchmarks.md`（7 phase / ~25 BMs / ~4.25h / 7 commits）
- **分支：** `feature/TASK-20260419-05-layout-render-benchmarks`（基于 main `2985220`）
- **创建日期：** 2026-04-19
- **来源：** TASK-20260419-02 归档 P1 后续任务 + TASK-20260419-03 baseline 模式延伸应用
- **Sticky ID 说明：** 候选区固定 ID TASK-05（自 TASK-02 归档以来），与当天序号 08 不同；按候选区一致性使用 sticky ID（参考 TASK-04 同样反向插入先例）

#### 目标 API（已验证）

| API | bench 用途 |
|---|---|
| `vx::layout::LayoutEngine::Layout(doc, ctx)` | `bench_layout_buildtree` — 构造 BlockBox / InlineBox 树 + 完整 layout（默认 block flow） |
| `vx::layout::LayoutEngine::Layout(doc, ctx)` （带 `display: flex` DOM） | `bench_layout_flex` — 触发 flex code path（行/列、子项数、尺寸约束）|
| `vx::render::Record(root)` → `DisplayList` | `bench_render_record` — 树→ DisplayList 转换 |
| `vx::render::Replay(list, canvas)` | `bench_render_replay` — DisplayList → Canvas 命令重放 |

#### 衔接 TASK-03 模式（可复用）

- ✅ `vx_add_benchmark()` CMake 函数已支持额外链接库（TASK-03 引入）
- ✅ baseline JSON 入仓 + `benchmarks/baseline/README.md` 4-piece 失真兜底协议（TASK-03 沉淀）
- ✅ RangeMultiplier 公式 `ceil(log_m(hi/lo))+1`（TASK-03 实证）
- ✅ 程序化 corpus 构造 + 静态 cache（TASK-03 `StylesheetCorpus` / `InlineStyleCorpus` 模式可复刻为 `LayoutCorpus`）
- ✅ 「带否定判据的发现型 phase」模板（TASK-03 cluster 度量；本任务可用于发现 layout 慢路径）

#### 前置验证（全部 ✅）

| 维度 | 结果 |
|------|------|
| 依赖可获取性 | ✅ 4 API 全在 main，`vx_core` 已链 |
| 环境就绪 | ✅ `build-bench/` 复用，**无需 FetchContent → P0 git proxy 不触发** |
| 已有 artifact | ✅ 7 现存 bench（4 Foundation + 3 CSS）；新增 4 bench 文件名不冲突 |
| 待处理事项关联 | ✅ 落实 `activeContext.md` 长期项 P1 「Layout + Render 基准」 |

#### 4 决策（头脑风暴产出）

| # | 维度 | 选择 |
|---|------|------|
| 1 | Corpus 构造 | **A** 纯程序化 DOM API（`CreateElement` + `AppendChild` + `SetInlineDeclaration`） |
| 2 | Flex 输入维度 | **B** 二维 `BENCHMARK_TEMPLATE(rows, cols)` 5 固定多维点 + 1 嵌套 flex |
| 3 | ImageCache 覆盖 | **B** Record / Replay 各加 1 个 img-only 对比 BM（含 ImgVsNoImg/Cache vs /NoCache 判定 hot path） |
| 4 | baseline 入仓 | **A** 4 个全入仓 + 复用 TASK-03 baseline/README 4-piece 失真兜底协议 |

#### Phase 划分（7 phase，详见 plan §1-7）

| Phase | 时间 | 文件 | BM 数 | 提交主题 |
|-------|------|------|-------|---------|
| 1 | 30min | CMakeLists + 4 smoke .cc | 4 | wip phase-1 |
| 2 | 30min | layout_corpus.h | 0 | wip phase-2 |
| 3 | 45min | bench_layout_buildtree.cc | 9 → 14 行 | feat phase-3 |
| 4 | 45min | bench_layout_flex.cc | 6 | feat phase-4 |
| 5 | 30min | bench_render_record.cc | 5 | feat phase-5 |
| 6 | 45min | bench_render_replay.cc | 5-6 | feat phase-6 |
| 7 | 30min | README + baseline + MB | 0 | docs phase-7 |

#### 验收标准（design §9 完整版 — 全部已验证 ✅）

1. ✅ 4 bench exe Release build 0 errors
2. ✅ 各 bench BM 数量符合设计：buildtree 14 行 / flex 6 / record 5 / replay 5
3. ✅ 4 bench 全 exit 0 + 数字非零（含 Replay 修正后 list 非空，~10 ns/cmd FillRect）
4. ✅ 全 7+4=11 bench targets 共存、零冲突
5. ✅ Debug ctest 890/890 不变（`build/` 重新 build 验证）
6. ✅ 4 baseline JSON 入仓（全 release 体检 ✅）+ baseline/README + benchmarks/README 更新
7. ✅ techContext.md「Layout 性能基线」+「Render 性能基线」段已补（/reflect 阶段落实改进建议 #1，与 CSS 性能基线段并列）
8. ⚠️ ImageCache 对比 BM 给出明确判定 — **回答「不是 hot path」但**真测路径 layout→Record→Replay 三阶段都不传 cache → list 内 0 个 kDrawImage。改用 Replay TextHeavy 通路实测出真正 hot path = `DrawText`（820× FillRect）。ImageCache 真测推 TASK-09。验收意图（量化是否 hot path）已满足。
- 安全相关：否（性能测量任务，无外部输入/无认证/无新依赖）

#### 关键发现（5 项 → /reflect 输入）

| # | 发现 | 数值 | 后续 |
|---|------|------|------|
| K1 | Replay hot path = `DrawText` | 8200 ns/cmd vs FillRect 10 ns/cmd = 820× | 立 TASK-20260419-09（候选）DrawText / shaping micro-benches |
| K2 | Layout buildtree-flat super-linear knee | N=128→256 时 7.7→70 µs（10× for 2× N）| 调查 cache miss / arena grow |
| K3 | Layout flex 同源 super-linear | 8x8→16x16 时 4.9→73 µs（14.9× for 4× cells）| 同 K2 |
| K4 | Record 对 image 元素无额外开销 | image_handle=0 → RecordBox 跳过；ImgVsNoImg 16 ≈ Medium 64/4 | 设计正确 |
| K5 | ImageCache 真测 fixture 缺失 | DecodeFromFile I/O；layout 不传 cache → 三阶段链断 | 推 TASK-20260419-09 |

#### 提交清单（7 commits, on `feature/TASK-20260419-05-...`）

| # | SHA | 主题 |
|---|-----|------|
| 1 | `3eb9070` | docs(plan): TASK-20260419-05 layout/render benchmarks design + plan |
| 2 | (phase 1) | wip phase-1 register 4 smoke benches |
| 3 | (phase 2) | wip phase-2 add layout_corpus.h |
| 4 | (phase 3) | feat phase-3 bench_layout_buildtree full suite |
| 5 | (phase 4) | feat phase-4 bench_layout_flex 2D matrix |
| 6 | (phase 5) | feat phase-5 bench_render_record full suite |
| 7 | (phase 6) | feat phase-6 bench_render_replay + hot-path finding |
| 8 | (phase 7) | docs(bench): 4 layout/render baselines + README updates |
| 9 | `81d85cc` | chore(build): finalize TASK-20260419-05 memory bank state |
| 10 | `01833c6` | docs(reflect): add reflection for TASK-20260419-05 |

#### 安全相关

否（性能测量任务，无外部输入/无认证/无新依赖）。

</details>

<details>
<summary>TASK-20260419-07：修复 main Release `-Werror` 编译失败（2 处） — ✅ 已归档（点开查看历史）</summary>

### TASK-20260419-07：修复 main Release `-Werror` 编译失败（2 处）

- **复杂度级别：** Level 1（2 文件，修复路径明确）
- **状态：** 🔵 回顾完成（待 `/archive`）
- **回顾文档：** `memory-bank/reflection/reflection-TASK-20260419-07.md`
- **分支：** `feature/TASK-20260419-07-release-werror-fixes`（基于 main `b321482`）
- **创建日期：** 2026-04-19
- **来源：** TASK-20260419-03 P6 Release fresh build 副发现 + 长期项 P1 #2
- **关联模式：** 与 TASK-20260419-04 同源（Release 通路验证缺失反复模式，第 2 次因此暴露）

#### 修复目标（已实地复现 ✅）

| # | 文件 | 行 | 错误 | 修复方向 |
|---|------|----|----|---------|
| (a) | `tests/platform/memory_surface_test.cc` | 102 / 105 / 108 | `fgets -Werror=unused-result` | ✅ A1 `ASSERT_NE(std::fgets(...), nullptr)` 包裹（commit `8b57f8d`） |
| (b) | `veloxa/foundation/strings/string.h` 拷贝构造 | 61-74 | `memcpy` `-Werror=array-bounds` GCC IPA 误报 | ✅ B2 `[[gnu::noinline]]` 拷贝构造（GCC-only 守卫），commit `51d6ff1`。**B3 `__builtin_memcpy` 试验失败** — 诊断在 IPA 中端阶段，先于 fortify 展开 |

#### 验收标准（全部 ✅）

- ✅ `cmake --build build-bench -j` 全量构建零 `-Werror` 失败（38.7s）
- ✅ `ctest` Debug 全量回归 890/890 不变（2.46s）
- ✅ bench 7 目标继续 exit 0（sanity 跑 1ms 全 BM 数字正常）

#### 提交清单

| # | SHA | 描述 |
|---|------|------|
| 1 | `8b57f8d` | fix(tests/platform): assert fgets return value |
| 2 | `51d6ff1` | fix(foundation/strings): mark BasicString copy ctor noinline |
| 3 | `6fca7cb` | chore(build): finalize TASK-20260419-07 memory bank state |
| 4 | `0d749c1` | docs(reflect): add reflection for TASK-20260419-07 |
| 5 | `466ba01` | chore: ignore build-*/ directories |

#### 安全相关

否。

</details>

### 待立项候选

- ~~TASK-20260419-05：已立项为当前任务，详见上方「当前任务」段~~
- **TASK-20260419-06（建议，**P3 降级**）：** HashMap Hash Mixing 优化 — 触发条件改为「短字符串 ≠ 主用例 + 容器规模 > 1000 entry」的新场景出现时再立项（来源 TASK-03 P4 实测均匀降级）
- **TASK-20260419-08（候选，P3 触发型）：** `string.h` 剩余 3 处 runtime-size memcpy（line 45 SSO ctor / 150 Append / 230 GrowAndCopy）防御性 noinline 化。**触发条件**：未来 GCC 升级回归同类 `-Warray-bounds` 误报（来源 TASK-07 副发现）
- ~~TASK-20260419-09：已立项为当前任务（A+B 子集），详见上方「当前任务」段。VAN 阶段 grep 推翻 K5「需 fixture 文件复制」假设（复用 `image_decoder_test.cc::CreateTestPng()` 程序化构造写 /tmp）+ K1「DrawText 真路径」假设（实际走 fallback FillRect）~~
- ~~TASK-20260419-10：已立项为 TASK-20260424-01 并完成（见上方「当前任务」段）。根因 (d) 定位 + 32K 落地，K2 R256 9.42→4.18 / K3 R_flex 16.49→6.40；残余 super-linear 拆出 TASK-20260424-02 继续跟进~~
- **TASK-20260424-02（新增，TASK-24-01 Phase 1B 升级路径拆出，建议 P3 触发型）：** Layout 残余 super-linear 调查 — TASK-24-01 已解决 ~60% knee（(d) malloc churn），但 R256 仍 4.18× / R_flex 仍 6.40×，剩余 ~40% 属于 (e) L1D 抖动 或 (f) 隐藏算法因素。**方向**：新建 `benchmarks/bench_layout_phases.cc` 拆分 `LayoutEngine::Layout` 三阶段（BuildTree / LayoutBlock / ApplyPositioning）独立 BM，按分解公式 `阶段 cost = 累计 - 前序累计` 定位 super-linear 所属阶段。**预期产出**：调查报告 +（可能）per-phase 优化 PR；**另有 K8 信号**：Phase 2 扫描中 65K block 让 R_flex 回弹（7.40→8.36）暗示 L1D 抖动（L1D 48KB 边界）— 可结合 per-element cache line 布局分析。**触发条件**：(1) 下次 layout 性能需求（新容器类型如 grid/multi-column）之前补短板；(2) 或当有额外预算主动改进；当前工作集已合理，不立即紧急
- ~~TASK-20260419-11：已完成并归档（合并到 main `8515c25`），详见 `archive-TASK-20260419-11.md`。**核心成果**：双索引方案 (`Vector<Entry>` + `HashMap<String, ImageHandle, StringHash, StringEq>`) 让 `BM_ImageCacheLoad_Hit<256>` 从 1151.77 ns → 45.70 ns（**25.2×↓**），K6 命题完全解；ctest 891/891 PASS；Release `-O3` 0 errors~~
- **TASK-20260419-12（新增，TASK-09 K7 拆出，建议 P2 触发型）：** `SoftwareCanvas::DrawText` 真路径优化（**优化类**）— 当前 warm 真路径 5807 ns > fallback 3647 ns（1.6×），阻碍未来默认开真路径。候选：(a) `hb_buffer` 复用避免每次 alloc/free；(b) glyph bitmap 直接 raster 到 canvas 避免 GlyphCache → 中间 buffer → blit 两次拷贝。**预期产出**：warm 真路径 < 3000 ns 后默认真路径开关。**触发条件**：当真路径默认化提上日程时

## 任务历史

| 任务 ID | 描述 | 状态 | 完成日期 | 归档文档 |
|---------|------|------|---------|---------|
| TASK-20260503-02 | 工作流/规则类技术债批量清理（Level 2 / 工作流元任务首次实施 / 6 项 P1 跨任务 reflection 沉淀清零）— 4 from C + 3 from A 项 P1 闭环（writing-plans.mdc +3 段 / git-workflow.mdc +1 段 / techContext.md StatusOr.status() 14/14 audit 0 fix / spec A14 解读附录段）；测试模式 `[文档调整模式]` 新正式定义；ctest 1247/1247 PASS（DEVTOOL=ON + SDL2=ON + Benchmarks=ON 全配置不退化）；8 commits（6 docs + 1 chore + 1 reflect）；plan ×0.6 第 56-61 数据点入库（**0.21× 总比值创历史新低 → 新效率区子档「纯文档/规则极速区 0.15-0.25×」识别**）；**0/7 反复模式命中**（创纪录第三次连续零反复 — Phase A → B → C → 本任务）；**4 大新协议首次实证**（reflection 沉淀回流模式 / 反复模式渐进式抑制 / Phase 0 audit 预跑子模式 / 纯文档极速区）+ Phase 0 投入定律 quad-evidence 升级（A 5.3× / B 5.2× / C 7.6× / 本任务 6.7× ROI / 平均 6.2×）；C-#2 反复模式渐进式抑制首次实证（plan + commit + 自我吃狗粮三层抑制）；改进建议 P0 0/0 + P1 1/1 + P2 3/3 全部 archive 阶段直接落实；2 新 P3 候选迁移到 activeContext（GoogleTest 短路评估易错模式 + plan §文档段落 LOC 预估系数修正）| ✅ 已完成 | 2026-05-03 | `archive-TASK-20260503-02.md` |
| TASK-20260503-01 | DevTool Phase C — Hot Reload 实施（Linux inotify + CSS-only 增量重载）[安全相关]（Level 3 / 零 escalation）— 11/11 子任务（C.0/C.1/C.2/C.3/C.4/C.5 跨 6 段）；HotReloadManager + Linux inotify watcher（std::thread + epoll fallback）+ CSS-only 增量重载（DOM tree 复用 + StyleResolver 重新解析）+ lazy-attach C ABI 容错模式扩展（VX_WARNING_HOT_RELOAD_FAILED warning 语义层）+ hello_devtool 第三次扩展（dogfood 路径成熟范式）；ctest 1228→**1247 (+19)** / OFF 1082→1082；A14 黑名单 +1 项；plan ×0.6 第 48-58 数据点入库（**0.31× = 极窄档加速衰减区候选新子档 0.20-0.30×**）；2 公开 C API 新增 + 1 新子系统 vx::devtool::hot_reload；改进建议 P0 0/0 + P1 4/4 落实（reflect 阶段 P1 #3 直接落实 systemPatterns + P1 #1/#2/#4 由 TASK-20260503-02 工作流元任务闭环）；**1/7 反复模式命中**（部分 — binutils 2.46+ + CssParser 严格性假设 2 项）；Phase 0 投入定律 triple-evidence 升级（7.6× ROI 历史最高）；hotfix 分支 → fast-forward main → feature rebase 链路首次实证（binutils 2.46+ baseline 阻塞事件）| ✅ 已完成 | 2026-05-03 | `archive-TASK-20260503-01.md` |
| TASK-20260502-02 | DevTool Phase B — Performance Overlay 实施 [安全相关]（Level 3 / 零 escalation）— 10/10 子任务（B.0/B.1/B.2/B.3 跨 4 段 / 11 commits）；PipelineHooks 五钩子（**#35 阶段 1 闭环 ✅**）+ PerfOverlay 60 帧 ring buffer + T6 三层 mitigation + HUD overlay UI + dirty rect 边框可视化 + F11 toggle + lazy-attach C ABI 容错模式（B.0.1+B.3.2 端到端实证）；ctest ON 1169→**1228 (+59)** / OFF 1065→**1082 (+17)**；A14 黑名单 +2 项（PerfOverlay + InjectDirtyRectHighlights）；2 安全威胁全程守门（T5 复用 ResetOverlayCommands / T6 单 instance + budget=0 显式短路 + 1ms guard）；plan ×0.6 第 38-47 数据点入库（0.40× = **极窄档延续高效区候选新子档 0.30-0.45×**）；4 公开 C API 新增 + 1 新子系统 vx::devtool::overlay::PerfOverlay；改进建议落实率 P0 0/0（创纪录）+ P1 3/3 + P2 8/8；3 P3 候选迁移到 activeContext（#35 阶段 2 / R9 EventManager HitTest / hello_devtool_perf_smoke 多帧）；连续两次任务 0/7 反复模式命中 + 5 大可复用架构范式 100% 命中且加深（设计资产化）+ Phase 0 投入定律 dual-evidence 实证（5.2× ROI ≈ Phase A 5.3×）| ✅ 已完成 | 2026-05-03 | `archive-TASK-20260502-02.md` |
| TASK-20260430-01 | first/last child margin collapse with parent（Level 3 单子系统 Layout + API 设计决策）— W3C CSS 2.1 §8.3.1 嵌套规则 R1 + R3 + R5 完整实施；新增 `LayoutBlockChild` 专用辅助 + `MarginChain` in-out by-pointer 跨函数 propagate（D2 build 阶段调整）+ `MarginChain::MergeFrom` 合并语义 method + `margin_bottom_collapsed_into_ancestor` 字段 + 隐式 BFC root 识别（root + html/body 顶层 wrapper）+ 5 阻断条件（padding/border + BFC root + height + min-height）；wpt-005 SKIP→PASS（直接验证目标达成）；ctest 1029→1039 PASS（+10 cases）；同窗口 stash-swap bench A6/A7 全 PASS（mean 5%-7% 区间 ≤ +10% 退出门）；P6.2 副产品优化 O(N) → O(1) `last_in_flow_block` hoisting；plan × 0.6 第 14 数据点 0.46×；4 改进建议全部落实（P0 递归算法 API 决策必检项 + P1×3 既有测试 fingerprint / CSS shorthand grep / 算法伪码 explicit method）；TASK-26-01 升级规则 ROI 3/5 触发全部高/中验证（§7.0.1 同窗口 stash-swap 首次外部任务 7 BM 一次过 / §9.3 反向探针第 6 次实战 / §9.2 默认值边界）；CSS parser `border-bottom` shorthand 缺失列入独立 P3 候选 | ✅ 已完成 | 2026-04-30 | `archive-TASK-20260430-01.md` |
| TASK-20260426-01 | Layout 正确性消化（#25 + #28 + #20 + #21）[安全相关]（Level 4）— 4 子任务多轮次：LayoutBox origin helpers + HTML inline style 三件套护栏 + Block margin collapsing + LineBox 模型 vertical-align；ctest 1029/1029 PASS；同窗口对照 bench mean -3.6% / median +2.65%；P0/P1 5 条规则升级落地（首次由 TASK-30-01 外部验证 ROI） | ✅ 已完成 | 2026-04-30 | `archive-TASK-20260426-01.md` |
| TASK-20260425-01 | SDL2 窗口后端 + 输入事件桥接（Level 3 中等功能）— 第一个有可见窗口的平台后端，解锁 DevTool 主线（hot reload / inspector / FPS overlay 前置）；`Sdl2WindowSurface` + `Sdl2EventLoop`（Composition over Inheritance 内组合 `HeadlessEventLoop`）+ `Surface::Present()` virtual no-op + `vx_view_run()` 自动 wire input callback；C API：`vx_event_loop_create_sdl2 / vx_surface_create_window(VxWindowOptions*) / vx_event_loop_pump_input`；隐含技术债清理：`destroy/save_ppm` 基类化清掉 UB 隐患；CMake 双轨 SDL2 lookup + `VX_PLATFORM_SDL2=OFF` 默认；`examples/hello_sdl2.cc` + `hello_sdl2_smoke` ctest（`SDL_VIDEODRIVER=dummy` + `VX_HELLO_SDL2_AUTOQUIT_MS` env hook 0.22s 自终止）；ctest 951/951 PASS（Debug + Release `-O3 -Werror`）；plan ×0.6 第 9 数据点 0.22× 历史最快「最窄路径」第 4 次确认；5 新模式 + 2 新 plan §0 grep 子段沉淀长期知识库；P6.1 WSLg 真窗口手测标遗留 | ✅ 已完成 | 2026-04-26 | `archive-TASK-20260425-01.md` |
| TASK-20260424-01 | Layout super-linear knee 根因调查（研究类）— 根因定位 (d) ArenaAllocator 4KB block malloc/free churn；默认 block_size 4096 → 32768；K2 R256 9.42×→4.18× / K3 R_flex 16.49×→6.40×；3 文件核心 + 7 commits；新增 `DefaultBlockSizeFitsLargeAllocations` GTest + RED 反向探针；K8 新发现（65K block > L1D 触发抖动）；plan × 0.6 第 5 数据点 0.29×（历史最快，「最窄路径」子档样板）；3 新模式沉淀 systemPatterns；残余 ~40% super-linear 拆出 TASK-20260424-02 | ✅ 已完成 | 2026-04-24 | `archive-TASK-20260424-01.md` |
| TASK-20260419-13 | 流程规则 P0/P1 沉淀冲刺（3 条积压条目一次性闭环）— P0 FetchContent proxy 守卫（反复 9+ 次痛点终结）/ P1 smoke 工具链 grep / P1 多轮次 Build 中间态；9 文件 / 8 commits / 反例追溯 7/7 通过（含 meta-dogfooding 实时自证）/ 10 验收 9 ✅ + 1 改进；跨类型估时收敛 plan × 0.6 通用协议；5 新模式沉淀 systemPatterns | ✅ 已完成 | 2026-04-19 | `archive-TASK-20260419-13.md` |
| TASK-20260419-11 | ImageCache::Load HashMap 化（K6 高 ROI 修复）— 双索引 (`Vector<Entry>` + `HashMap<String, ImageHandle, StringHash, StringEq>`)；保 ABI / Get O(1)；Hit<256> 1151.77 ns → 45.70 ns（25.2×↓）；ctest 891/891 PASS；新增 `ClearAndReloadDeduplicates` D3 回归网（RED 反向探针验证有效）；3 P1 + 3 P2 改进沉淀 | ✅ 已完成 | 2026-04-19 | `archive-TASK-20260419-11.md` |
| TASK-20260419-09 | Replay 深度基准（2 bench exe / 15 BMs / 2 baseline JSON）；修正 K1 归因（fallback 非真路径），定位真冷路径 14× 慢；新发现 K6 ImageCache::Load O(N) + K7 warm 真路径 1.6× 慢；推 TASK-11/12 | ✅ 已完成 | 2026-04-19 | `archive-TASK-20260419-09.md` |
| TASK-20260419-05 | Layout + Render 性能基准（4 bench exe / 30 BMs / 4 baseline JSON）；K1 实测 DrawText 是 Replay hot path（820× FillRect），ImageCache 不是；推 TASK-09 | ✅ 已完成 | 2026-04-19 | `archive-TASK-20260419-05.md` |
| TASK-20260419-07 | 修复 main Release `-Werror` 编译失败（fgets unused-result + BasicString copy ctor IPA array-bounds 误报）— 与 TASK-04 同元模式不同手法 | ✅ 已完成 | 2026-04-19 | `archive-TASK-20260419-07.md` |
| TASK-20260419-03 | CSS 解析性能基准（Tokenizer / Parser / PropertyLookup）— 30 BMs + 3 baseline JSON + Cluster 度量证 PropertyMap 均匀 | ✅ 已完成 | 2026-04-19 | `archive-TASK-20260419-03.md` |
| TASK-20260419-04 | 修复 `enum_serialization.cc` Release `-Warray-bounds` 误报（解锁 TASK-03 Phase 1） | ✅ 已完成 | 2026-04-19 | `archive-TASK-20260419-04.md` |

<details>
<summary>已归档：TASK-20260419-02 — 补充 Google Benchmark 集成与 Foundation 性能基准（点开查看历史细节）</summary>

- **复杂度级别：** Level 2（简单增强）
- **状态：** ✅ 已完成（已合并到 main）
- **分支：** `feature/TASK-20260419-02-benchmarks`（已删除）
- **创建日期：** 2026-04-19
- **归档日期：** 2026-04-19
- **来源：** TASK-20260405-01 Foundation 延期项 P1#1（"Benchmark 延期 — 需 google benchmark，网络恢复后补充"）
- **设计规格：** `docs/specs/2026-04-19-benchmarks-design.md`
- **实现计划：** `docs/plans/2026-04-19-benchmarks.md`
- **回顾文档：** `memory-bank/reflection/reflection-TASK-20260419-02.md`
- **归档文档：** `memory-bank/archive/archive-TASK-20260419-02.md`
- **实际提交：** 10 个（plan + 7 build + reflect + archive）
- **实际新增 BM 用例：** 40（allocators 13 + containers 8 + hash_map 10 + strings 9）；GTest 基线 890/890 不变

#### 关键决策（头脑风暴 4 轮，全部 A 方案）

| 维度 | 决策 |
|------|------|
| 范围 | 仅 Foundation（allocators / containers / strings / hash_map），不含 CSS/Layout/Render |
| Executable 粒度 | 一文件一 exe（4 个） |
| 用例深度 | 中等（每组件核心操作 + 1-2 个尺寸/容量梯度） |
| 结果留存 | 仅控制台 + README 给出 JSON 导出（不提交 baseline 文件） |

#### Phase 概览

| Phase | 内容 | 提交 |
|-------|------|------|
| 0 | 基线验证（890 测试全绿；FetchContent 通路验证） | 0 |
| 1 | 创建分支 + `benchmarks/CMakeLists.txt` + 4 个 smoke .cc | 1 |
| 2 | `bench_allocators.cc`（Malloc/Arena/Pool，13 BM） | 1 |
| 3 | `bench_containers.cc`（Vector/SmallVector/IntrusiveList，8 BM） | 1 |
| 4 | `bench_hash_map.cc`（insert/lookup hit&miss/rehash，10 BM） | 1 |
| 5 | `bench_strings.cc`（BasicString SSO+heap、InternedString，9 BM） | 1 |
| 6 | `README.md` + `techContext.md` 更新（依赖表 + Benchmark 启用段） | 1 |
| 7 | 收尾验证 + 依赖安全审计（google/benchmark v1.9.1 CVE） + MB 收尾 | 1 |

#### 安全相关

否（纯内部性能测量；唯一新依赖 google/benchmark v1.9.1 经 CVE 审计：GitHub Security Advisories 0 条）

#### 副发现

`HashMap::Find` 对小整数 key 在 cap≥1024 时严重 cluster（LookupHit/16384=9µs vs n=64=69ns）。根因 `H1=h>>7` + `std::hash<int>` 恒等映射使所有起始探测位置压在 [0,127]。已记入 `benchmarks/README.md` ⚠️ 量级表 + activeContext 待处理事项 P2，建议立项 TASK-20260419-05。

</details>

<details>
<summary>已归档：TASK-20260419-01 — 流程规则沉淀 + P2 功能技术债收口（点开查看历史细节）</summary>

- **复杂度级别：** Level 3（中等功能；跨「.cursor 规则」+ 「Script/CSS/Event」子系统）
- **状态：** ✅ 已完成（已合并到 main）
- **分支：** `feature/TASK-20260419-01-rules-and-debt`（已删除）
- **创建日期：** 2026-04-19
- **归档日期：** 2026-04-19
- **设计规格：** `docs/specs/2026-04-19-rules-and-debt-design.md`
- **实现计划：** `docs/plans/2026-04-19-rules-and-debt.md`
- **回顾文档：** `memory-bank/reflection/reflection-TASK-20260419-01.md`
- **归档文档：** `memory-bank/archive/archive-TASK-20260419-01.md`
- **预估提交：** 21 个；**实际：** 23 个（含 reflect + archive）
- **预估新增测试：** +22；**实际：** +34（基线 856 → 890）

#### 需要创意阶段的组件

**无。** 头脑风暴阶段已为 Part B 三个设计点（B5 Enum 反查表 / B6 反向析构机制 / B7 精确移除 API）固化方案：

| 子项 | 决策 | 模式来源 |
|------|------|---------|
| B5 | 完整反查表 `enum_serialization.{h,cc}` | 枚举 + 元数据表驱动模式（property.cc 同构） |
| B6 | EventManager `AddDestructionObserver` 回调 | Push 回调失效触发模式（SetInvalidationCallback 同构） |
| B7 | u64 ListenerToken + `RemoveByToken` API | 不透明句柄模式（与现有 `JSValue` 句柄风格一致） |

无需进一步探索 → 跳过 `/creative`，直接 `/build`。

#### Phase 概览

| Phase | 内容 | 预计耗时 |
|-------|------|---------|
| 0 | 基线验证（856 测试全绿） | 5 min |
| A1 | writing-plans.mdc 增 5 段 | 30 min |
| A2 | subagent-development.mdc 增 3 段 | 20 min |
| A3 | 新建 integration-testing.mdc + 注册 | 20 min |
| A4 | techContext.md「FetchContent 与代理」段 | 10 min |
| B5 | enum_serialization.{h,cc} + dom_bindings 接入 + 测试 | 45 min |
| B6 | EventManager DestructionObserver + DomBindings 注册 | 30 min |
| B7 | Listener Token API + 精确 removeEventListener | 60 min |
| 8 | 收尾验证 + Memory Bank 更新 | 20 min |
| **合计** | | **~4 h** |

#### 任务范围

**Part A — 流程规则沉淀（14 条 P1 待办，纯 .mdc 文件修改）**

1. 计划模板（`.cursor/rules/skills/writing-plans.mdc`）增段：
   - FetchContent 引入第三方编译时校验根 `add_compile_options(-Werror...)` 是否仅限 `$<COMPILE_LANGUAGE:CXX>` 或目标级（来源 TASK-20260413-01）
   - 测试基础设施审计——列出测试需访问的内部状态及访问路径（来源 TASK-11）
   - 边界输入清单——每个 Phase 列出非默认路径（来源 TASK-06）
   - 跨模块参数透传修改时调用链端到端验证（来源 TASK-09）
   - 设计文档管线注入点须附代码级可行性验证（来源 TASK-13）
2. 子代理 prompt（`.cursor/rules/skills/subagent-development.mdc`）增段：
   - 跨模块数据格式段（来源 TASK-02）
   - LayoutBox 坐标计算时须包含 x/y 语义定义 content origin vs border box origin（来源 TASK-07）
   - 并行子代理可行条件：无共享 .cc + 共享 .h 已创建 + CMakeLists.txt 已更新（来源 TASK-08）
   - 存根文件预创建策略（来源 TASK-04）
   - 合并 Phase 给子代理的策略（来源 TASK-04）
3. 集成测试规范——新建 `.cursor/rules/skills/integration-testing.mdc`：
   - 集成测试优先验证数据格式一致性（来源 TASK-02）
   - 必须使用真实 HTML/CSS 解析器，禁止仅用手动 DOM 构建（来源 TASK-06）
   - 像素验证优先用 DisplayList 检查和区域扫描，避免硬编码坐标（来源 TASK-07）
   - CSS 颜色测试禁止与 gfx::Color 编程常量直接比较，必须通过 CssColorToGfx 转换（来源 TASK-07）
   - 禁止使用 HTML inline style（BuildTree 不解析），必须用外部 CSS 选择器（来源 TASK-08）
   - 集成测试模板增加 API 备忘清单：html::Parser / FindElement / HandleInput（来源 TASK-13）
4. 文档：含 Git 拉取依赖的文档（`techContext.md` 或 README）写明代理 `http_proxy`/`HTTPS_PROXY` 与首次 `cmake` 注意点（来源 TASK-20260413-01）

**Part B — P2 功能技术债（含代码与测试）**

5. **#46 续作**：`StyleGetProp` Enum 读路径（display 等）——构建 `PropertyId→enum string` 反查表，覆盖 display/position/visibility 等 Enum 类型 CSS 属性
6. **#50 续作**：`DomBindings`/`EventManager` 析构顺序硬约束——目前仅保证 `DomBindings` 先析构；反向场景（`EventManager` 先销毁）需弱引用机制
7. **#47 续作**：`removeEventListener` 按 `(type, handler)` 精确移除——扩展 `EventManager::RemoveEventListenersByHandler` API 并在 `dom_bindings` 调用

#### 安全相关

否（纯内部技术债 + 流程规则；不涉及外部输入/认证/存储/部署）

</details>

## 任务历史

| ID | 描述 | 最终状态 | 归档日期 |
|----|------|---------|---------|
| TASK-20260419-02 | 补充 Google Benchmark 集成与 Foundation 性能基准（4 个 bench exe / 40 BM 用例） | ✅ 已完成 | 2026-04-19 |
| TASK-20260419-01 | 流程规则沉淀 + P2 功能技术债收口（14 条 P1 流程规则 + CSS Enum 序列化 / EventManager 反向析构 / removeEventListener 精确移除） | ✅ 已完成 | 2026-04-19 |
| TASK-20260418-01 | 消化关键技术债务（#45 dom_bindings 全局状态 / #46 StyleGetProp 读路径 / #48 hb_font 缓存 / #50 addEventListener 生命周期） | ✅ 已完成 | 2026-04-18 |
| TASK-20260414-01 | 功能补全（border-radius / 字体渲染 / 图片支持 / JS-DOM 绑定） | ✅ 已完成 | 2026-04-14 |
| TASK-20260413-02 | 消化技术债务子集（#41 `HashMap` const 迭代、#39 测试像素头、`active_count_` 移除） | ✅ 已完成 | 2026-04-13 |
| TASK-20260413-01 | QuickJS 脚本引擎集成（quickjs-ng、`vx_script`、`QuickjsEngine`） | ✅ 已完成 | 2026-04-13 |
| TASK-20260405-01 | 构建 Foundation 基础库（内存管理/容器/字符串/日志） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-02 | 构建 Graphics HAL 图形抽象层与 Platform HAL 平台抽象层 | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-03 | 构建 DOM 树 + HTML 解析器 | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-04 | 构建 CSS 引擎（Tokenizer/Parser/选择器/属性/层叠） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-05 | 消化技术债务（Arena/HashMap/PPM/Parser Error） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-06 | 构建 Layout Engine 布局引擎 | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-07 | 构建渲染管线（Render Pipeline） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-08 | 构建事件系统（Event System） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-09 | 脏区更新与重绘机制 | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-10 | 事件循环与应用壳（EventLoop / Application Shell） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-11 | C API 层（对外嵌入接口） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-12 | 示例应用（examples/hello） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-13 | CSS 动画系统（CSS Transitions） | ✅ 已完成 | 2026-04-05 |
