# 回顾：TASK-20260503-04 — DevTool Phase D · Console JS REPL + console.log 桥接（V1=B 扩展段）

| 字段 | 值 |
|---|---|
| **日期** | 2026-05-04 |
| **任务 ID** | `TASK-20260503-04`（搁置任务恢复 / 沿用原 ID） |
| **复杂度级别** | Level 3（V3=A 锁定 — 不 escalate；4 件套 UI + 2 公开 C API + isolated JSRuntime） |
| **回顾深度** | Level 3+（详细回顾 + 主动深化到 ~0.07-0.10× 极速区候选新子档识别 + AI agent 加速因子分析） |
| **任务时长** | 主线 ~37 min（VAN ~14 min + Plan ~5 min + Creative ~5 min + Build ~17 min + Reflect ~预计 ~10 min） |
| **分支** | `feature/TASK-20260503-04-devtool-console-repl`（基于 main `509fec3`） |
| **创意文档** | `memory-bank/creative/creative-devtool-console.md`（~1197 行 / 4 维度 C1-C4 详细设计） |
| **Plan 文档** | `docs/plans/2026-05-04-devtool-console-repl.md`（585 行） |
| **触及威胁** | T1 任意 eval mitigation ✅ 完整闭环（首次完整暴露 — 5 维度全实施）/ T6 buffer 上限保护 ✅（双上限 + UTF-8 boundary safe truncate）|
| **触及技术债** | spec §11.1 Console 占位 ✅ 闭环 / 解锁 TASK-30-04-E JS Debugger backend / 复用 TASK-05 闭环的 SetEvalInterruptBudget API |
| **关键产物** | 4 件套 DevTool UI 完整闭环（Inspector + Performance Overlay + Hot Reload + **Console**）/ +2181 行 src+tests / 6 commits build / 1284 PASS DEVTOOL=ON / 1091 PASS DEVTOOL=OFF |

---

## §1 — 计划 vs 实际

| 维度 | 计划 | 实际 | 偏差原因 |
|---|---|---|---|
| 子任务数 | 5（D.1+D.2+D.3+D.4+D.5）+ CP1+CP2 + finalize | 5/5 + 2/2 CP + finalize | ✅ 100% 命中 |
| 预估时间（plan ×0.6） | 180-240 min（VAN 预测 0.30-0.45×） | **build phase ~17 min（commit 间隔）** | **0.07-0.10× 比值 — 远低于 VAN 预测**；触发新子档候选「creative 全锁死 + 范式 100% 复用 + AI agent 极速区 0.05-0.15×」 |
| 文件变更 | 5 子任务 ~+800-900 行 / 4 OFF guard | 6 commits / **+2181 src+tests / +~300 docs / 30 src+tests 文件** | 行数高 ~2.4× — `console_panel.html/css/js`（~242 行）+ DomBindings host data channel 扩展（+150 行 D.4 wiring fix）+ console_bindings.cc（+238 行含 TruncateAtUtf8Boundary 自实现）+ console_api_test.cc（+204 行覆盖 OFF stub）+ dogfood smoke（+205 行）= 累计超估，但单子任务内未触发 escalate |
| 设计变更 | C1-C4 4 维度 creative 全锁定 | (1) C1 input → `<span>` + JS self-managed value（grep 实证 Veloxa 无原生 input 行为 / creative 阶段已 surface）/ (2) **C2 wiring path 2** — D.2 在 console_script_engine ctx 注册 drain，但 panel JS 跑在 devtool_script_engine ctx → **plan-fact reconcile #1**：D.4 通过 DomBindings host data channel 扩展修复 / (3) C4 自实现 TruncateAtUtf8Boundary（grep 实证 utf.cc 无现成函数）| C1+C4 在 creative 阶段已 surface（无 build 阶段返工）；**C2 plan-fact reconcile 仅在 build D.2 阶段识别**（creative 阶段未深查 panel JS context 归属）|
| commits 数 | 5（D.1-D.5）+ 1 reflect + 1 archive = 7 | 5 实施 + 1 finalize + 1 plan + 1 creative + 1 reflect 待 + 1 archive 待 = 9 | +2 — plan + creative 各独立一个 docs commit（与 TASK-05 同协议） |
| ctest 数字 | DEVTOOL=ON 1252→**1252+N**（N≈21 估）/ OFF 1087 不变 / SDL2=ON 1265→1266 | DEVTOOL=ON 1252→**1284（+32）** / **DEVTOOL=OFF 1087→1091（+4）** / SDL2=ON 未跑（plan §0.6 SDL2 行 P3 留后续）| **plan §0.6 假设错误 ×2**：(1) +21 估实际 +32（+52% 偏差）— D.1 5 + D.2 7 + D.3 5 smoke + D.4 7 + D.5 8 / (2) **OFF +4 而非 0** — console_api_test 在 OFF 也跑（因含 OFF stub 测路径 #ifdef VX_BUILD_DEVTOOL → return INVALID_STATE 验证）= **反复模式 #1 「config 矩阵 guard 边界假设漏审」第 2 次连续命中**（TASK-05 plan-fact reconcile #1 同源） |

**关键偏差总结：**

- **正向偏差（远超预期）**：实测 build phase ~0.07-0.10× vs VAN 预测 0.30-0.45× 偏低 4-6×（**最快历史记录** — 破 TASK-20260503-05 0.16× 极值下限）；触发新子档识别（详 §4.4）
- **plan-fact reconcile #1（C2 wiring）**：D.2 假设 panel JS 跑在 console_script_engine ctx → 实际跑在 devtool_script_engine ctx；D.4 通过 DomBindings.SetConsoleEngine/SetConsoleLogBuffer host data channel 扩展修复（+150 行 dom_bindings.cc）— **在同任务内识别 + 修复**（非用户决策返工 plan/creative）
- **plan-fact reconcile #2（OFF +4 而非 0）**：与 TASK-05 同源「config 矩阵 guard 边界假设漏审」**第 2 次连续命中** — 验证了 TASK-05 reflect §4.1 改进建议的紧迫性（应升级 P1 → **P0 立即**改进 writing-plans skill）
- **代码量超估 ~2.4×**：单子任务内未触发 escalate（每个子任务实测都在 plan 上沿内），但累计超出反映「plan 行数估算需含 host data channel 扩展、host endpoint 双注册、自实现算法、smoke 替代手动验证」类的隐性附加工作

---

## §2 — 做得好的（5 项）

### §2.1 Phase 0 七子段 grep 实证 — 反复模式 #1 第 11 次抑制成功 ⭐⭐⭐

**做法：** Plan 阶段 §0.1-0.7 系统性 grep 7 子段实证：

| 子段 | 实证内容 | 触发的修正 / 验证 |
|---|---|---|
| §0.1 | ctest baseline 二次验证 | DEVTOOL=ON 1252 + DEVTOOL=OFF 1087 与 TASK-05 闭环值一致 ✅ |
| §0.2 | `JS_NewRuntime/Context` 当前用法实证 | QuickjsEngine self-contained → 多实例可行 ✅ |
| §0.3 | `inspector_panel` 双层 API 范式 + `LoadDevtoolDocument` 范式 + `inspector_resources.h` 资源嵌入范式 | 100% 复用范式确认（4 件套 UI 协议一致）✅ |
| §0.4 | DomBindings 当前公开 API 表 | 既有 binding mutator-rich + RegisterDevtoolBindings 3 个函数全 query-only 零 mutator ✅（C3 capability allowlist 路径预判 — Console 不调 DomBindings::Bind） |
| §0.5 | `vx_devtool_register_callback` 范式 + JS-side console.log 桥接路径 | 当前无 push-style callback API → query-style drain 模式与既有范式 100% 一致 ✅（C4.A 路径预判） |
| §0.6 | 配置矩阵假设 | DEVTOOL=ON 1252→1252+N / **DEVTOOL=OFF 1087 不变** ⚠️（含 add_test guard 检查 — TASK-05 P1 #1 闭环假设）→ **plan-fact reconcile #2 触发**（OFF 实际 +4 — 见 §3.2） |
| §0.7 | 反复模式 #1 抑制 | §0.2+§0.4 实证「devtool_script_engine_ 已存在 + RegisterDevtoolBindings 已 query-only」避免假设错误 ✅ |

**收益：**
- C3+C4 路径在 §0.4+§0.5 grep 阶段就锁死 → creative 阶段无设计探索成本
- 反复模式 #1「前置依赖/环境/API 能力未验证」**第 11 次连续抑制成功**（TASK-05 第 10 次 → 本任务第 11 次）— **接近双 sext-evidence**（11 次连续抑制 = 12 次完整证据基）
- C2 wiring 假设虽未在 §0.4 充分深查（仅查 RegisterDevtoolBindings 注册位置，未查「panel JS 实际运行在哪个 ctx」），但这属于「runtime 行为深度查证」延伸 — 是反复模式 #1 第 4 个新形式（详 §4.1 新教训）

**Phase 0 ROI：** ~5 min Phase 0 投入 → 节省 ~30+ min creative + build 阶段返工 → **~6-10× ROI**（落 sept-evidence 升级范围内 — Phase 0 投入定律 6.2× 平均 ROI 持续验证）

### §2.2 Creative 4 维度全锁死 + 推送回 plan 的代码骨架 — build 阶段 ~0 设计探索成本 ⭐⭐⭐

**做法：** Creative 阶段（~5 min）针对 C1-C4 四维度产出 ~1197 行设计文档：
- C1：3 探索方案对比表 + `<span>` + JS self-managed value 详细伪码 + isConsoleActive() 断言保护
- C2：lifecycle 时序图（4 阶段：Load 时 ConsoleEngine Init / RegisterDevtoolBindings / vx_view_attach_devtool 后；Unload 时反序）+ DEVTOOL=OFF 路径
- C3：RegisterConsoleBindings 全量 API 表（4 函数 + 反向探针单测设计 8 项 grep 验证）
- C4：drain JSON envelope 6 字段语义表 + 双上限 mitigation 算法 + TruncateAtUtf8Boundary 完整 cpp 实现 + 5 case 单测

**收益：**
- D.1（ConsoleEngine + lifecycle）一次过 — 5 测全 PASS / 0 返工 / +316 行
- D.2（RegisterConsoleBindings + buffer + drain）一次过 — 7 测全 PASS（含 capability allowlist 反向探针）/ +576 行 / 仅修 2 处编译错（quickjs.h 链接 + hex escape 字面量）
- D.3（资源 + 4 tab 集成）一次过 — 5 smoke 全 PASS / 0 设计返工 / 仅修 1 处反向探针 false positive
- D.4（公开 C API）含 plan-fact reconcile #1 修正 — 仍一次过 / 7 测全 PASS / DomBindings 扩展跟随 SetHotReloadManager 范式
- D.5（5 维度 T1 + 3 dogfood）一次过 — 8 测全 PASS / A14 +6 符号 0 泄漏

**0 设计返工实证：** 5 commits 全部一次通过 / 反向探针 false positive 仅 ~3 min 修复（comment 重写）/ plan-fact reconcile 在 D.4 内完成（无返回 /creative 重做）

### §2.3 plan-fact reconcile #1（C2 wiring）build 阶段同任务内识别 + 修复 — 0 重写代价 ⭐⭐ 新协议沉淀

**做法：** D.2 build 阶段实施时假设按 plan §3.D.2 在 `console_script_engine_` ctx 注册 drain（`RegisterConsoleBindings(console_engine.context(), buffer)`）；进入 D.3 后 console_panel.js 内联到 inspector_panel.html 通过 LoadDevtoolDocument 注入到 `devtool_script_engine_` ctx 运行 → drain 函数对 panel JS 不可见。

**识别时机：** D.4 公开 C API 阶段，写 vx_view_eval_console + vx_view_console_log_drain 时认知到「panel JS 应该能 call drain 拿数据 + call eval 跑用户输入」，回查 D.2 注册位置 → 发现 ctx mismatch。

**修复方案（path 2 — DomBindings host data channel 扩展）：**
- DomBindings 加 `SetConsoleEngine` + `SetConsoleLogBuffer` setter/getter（复用既有 `SetHotReloadManager` / `SetPerfOverlay` 范式）
- RegisterDevtoolBindings 在注册既有 3 个 query-only 函数（dom_json / perf_stats / hot_reload_status）时**额外**注册 `vx_console_eval` + `vx_devtool_get_console_log_drain`，从 DomBindings 拿 ConsoleEngine + ConsoleLogBuffer 指针
- console_script_engine_ 仍**严格隔离**（仅暴露 console.log/error/warn，不暴露 dom_json/perf_stats/eval/drain — 因为 user JS 代码不应能 query 自己的 buffer 或递归 eval）

**收益：**
- 修复在同任务内（D.4 commit 一次包含修复 + 7 测增量）— 无返回 /creative 重做 / 无 ROLLBACK
- 修复**0 重写代价** — 复用既有 SetHotReloadManager / SetPerfOverlay 范式 / +150 行 DomBindings 扩展即完成 / 7 测全过
- 沉淀新方法论候选：「**plan-fact reconcile build 阶段同任务内修复透明化**」— commit message Source 段明确标注「plan-fact reconcile #1 — D.2 wiring 修正」+ techContext.md TASK-04 实施摘要 §5 显式记录（vs 沉默修复后续审计困难）

### §2.4 5 大范式 + 9 systemPatterns 协同度 100% — Phase D 完整落地范式 ⭐⭐

| # | 范式 | Phase A/B/C 沉淀 → Phase D 复用 |
|:-:|---|---|
| 1 | 双层 API 范式（Phase A） | DomBindings 复用（既有 Mutator-rich for DevTool UI）+ ConsoleBindings 隔离（capability allowlist） = **双层 API 子模式扩展** ✅ |
| 2 | #ifdef + CMake conditional 范式（Phase A）| `console/` 子目录全程 #ifdef + R4 unique_ptr 子模式（Phase C 沉淀）防 OFF link 失败 ✅ |
| 3 | lazy-attach C ABI 容错模式（Phase B + Phase C）| vx_view_eval_console + vx_view_console_log_drain 双 API lazy-attach（NULL 时返 INVALID_STATE / DEVTOOL=OFF 时返 INVALID_STATE）✅ |
| 4 | 资源嵌入范式（Phase A）| inspector_resources.h +3 extern + console_panel.{html,css,js} + Application::LoadDevtoolDocument 4 处 ReplaceFirst ✅ |
| 5 | dogfood 路径范式（Phase A → 第 4 次扩展）| devtool_console_dogfood_smoke_test 端到端（vx_view_attach_devtool → vx_view_eval_console + console.log drain → detach/re-attach 循环）✅ |
| 6 | Phase 0 投入定律 sept-evidence 升级 | A 5.3× / B 5.2× / C 7.6× / 02 6.7× / 03 8.0× / 05 16× / **04 ~6-10× 预期**（待最终核算）✅ |
| 7 | A14 守门（Phase A 沉淀）| CMake smoke template 加 6 项（console_engine + log_buffer + RegisterConsoleBindings + 3 资源符号）+ Phase D 区分注释 ✅ |
| 8 | **isolated JSRuntime 隔离模式（C2.B 新沉淀）** | 第 3 个 QuickjsEngine 实例 + console_log_buffer 隔离 buffer + lifecycle 反序释放 ✅ 🆕 |
| 9 | **capability allowlist 范式（C3.A 新沉淀）** | Console 严格隔离 binding（不调 DomBindings::Bind / 仅暴露 console.log/error/warn）+ 反向探针单测验证（grep 8 项 mutator 不可达）✅ 🆕 |

**协同度：5 ✅ 复用 + 1 升级 sept-evidence + 2 🆕 新沉淀 + 1 自动化沿用 = 跨范式协同度 100%**

### §2.5 智能合约式 commit Source 溯源 + 实测数据格式 quad-evidence 升级 ⭐ 接近固化阈值

**做法：** 6 commits 全部含 "Source: TASK-20260503-04 plan §X.Y + creative §CN" 前缀 + 实测数据：
- D.1: `Source: TASK-20260503-04 plan §3.D.1 + creative §C2.B` + 「DEVTOOL=ON 1257 PASS (baseline 1252 +5)」
- D.2: `Source: TASK-20260503-04 plan §3.D.2 + creative §C3+C4` + 「DEVTOOL=ON 1264 PASS (+7)」
- D.3: `Source: TASK-20260503-04 plan §3.D.3 + creative §C1` + 「DEVTOOL=ON 1269 PASS (+5 smoke)」
- D.4: `Source: TASK-20260503-04 plan §3.D.4 + creative §C3 path 2 (DomBindings host data channel) + plan-fact reconcile #1` + 「DEVTOOL=ON 1276 PASS (+7)」
- D.5: `Source: TASK-20260503-04 plan §3.D.5 + creative §C2 + spec §11.1 T1 mitigation 5-dim self-check matrix` + 「DEVTOOL=ON 1284 PASS / DEVTOOL=OFF 1091 PASS / A14 0 leaks」
- finalize: `Source: TASK-20260503-04 build phase finalize (§7.5 collateral commit)` + MB 三件套同步

**累计 evidence：** TASK-20260503-02（6 docs commits）+ TASK-20260503-03（5 commits）+ TASK-20260503-05（5+1+1+1=8 commits）+ **TASK-20260503-04（5+1+1+1=8 commits）** = **27 commits 累计 quad-evidence**（**远破 git-workflow.mdc 固化阈值**，急需在下次工作流元任务批量落地时同步固化）

---

## §3 — 遇到的挑战（4 项）

### §3.1 plan-fact reconcile #1（C2 wiring 假设错位）⚠️ 反复模式 #1 第 4 个新形式

**问题：** plan §3.D.2 + creative §C2.B 假设 `RegisterConsoleBindings(console_engine.context(), buffer)` 注册的 drain 函数能被 panel JS 调用，但实际 panel JS 在 `devtool_script_engine_` ctx 运行（通过 LoadDevtoolDocument 内联到 inspector_panel.html → 走 devtool_script_engine eval），不在 `console_script_engine_` ctx 运行。

**根因：** Phase 0 §0.4 grep 仅查 RegisterDevtoolBindings 注册位置（确认 query-only），但**未查 panel JS（resources/inspector_panel.js + 内联 css/js）实际运行在哪个 ctx**。这属于反复模式 #1「前置依赖/环境/API 能力未验证」的**新形式**「JS context 归属与 host binding 注册 ctx 一致性假设」。

**与 TASK-03 第 9 次「现有实现 runtime 行为假设未实证」的区别：**

| 维度 | TASK-03 第 9 次 | TASK-04 本次 |
|---|---|---|
| 假设源 | `dirty_` 短路机制（runtime 状态机） | panel JS context 归属（架构层面 ctx ownership） |
| 触发时机 | build 阶段实施失败 → 回退 → 拆细化 P3 | build D.4 阶段实施时认知到 wiring 错位 → 同任务内修复 |
| 修复成本 | 12 min 失败回退 + 拆细化 P3 候选 | ~10 min DomBindings 扩展 + 7 测一次过 / 0 重写 |
| 沉淀方式 | techContext「Veloxa 引擎核心机制注意点」段 | reflection §3.1 + creative C2 段后续补强建议 |

**影响：** 0 重写代价（同任务内修复）+ creative C2 设计有 path 1（直接注册到 console ctx 失败）→ path 2（DomBindings host data channel）的 fallback 路径（实际 path 2 才是正确的，但 creative 阶段写 path 1 为主）。

**沉淀路径（P1 改进建议）：** Phase 0 grep 子段补强「panel JS / 用户脚本运行 ctx 归属验证」audit 子条 + 新 systemPattern 段「DevTool dual-ctx wiring SOP」（详 §5）。

### §3.2 plan-fact reconcile #2（OFF +4 而非 0）⚠️ 反复模式 #1「config 矩阵 guard 边界假设漏审」第 2 次连续命中

**问题：** plan §0.6 + plan §3.D.4 假设 console_api_test 在 DEVTOOL=OFF 下不跑（因为 #ifdef VX_BUILD_DEVTOOL guard），实际 +4（4 个 OFF stub 测路径验证 INVALID_STATE 返回）。

**根因：** plan §3.D.4 在文件清单写 `tests/api/CMakeLists.txt [共享文件] (+~3 行 / 新增 console_api_test，if(VX_BUILD_DEVTOOL) guard)`，但实施时为了让 OFF 路径也能验证 stub 行为（lazy-attach 容错 ABI 在 OFF 下应返 NOT_ATTACHED），把 add_test 不加 guard，让 console_api_test 在两个 config 下都跑（OFF 下走 #ifndef VX_BUILD_DEVTOOL 分支返 INVALID_STATE 验证）。

**与 TASK-05 plan-fact reconcile #1 同源**：TASK-05 plan §0.4 假设 OFF 1082 不变 → 实际 +5（quickjs_engine_test 在 vx_script 无 DEVTOOL guard）；本任务同样模式（**第 2 次连续命中**）。

**影响：** **正向影响**（OFF stub 测路径覆盖更完整 — 4 测验证 OFF lazy-attach C ABI 协议正确性）+ **数字偏差**（plan §0.6 OFF 1087 不变假设错）。

**沉淀路径升级（P0 改进建议 — TASK-05 P1 因第 2 次连续命中升级为 P0 立即落实）：** writing-plans.mdc «ctest 数量预期 config 矩阵» 段补强 audit 子条**必须在本任务 archive 阶段直接落实**（不能再等下次工作流元任务）— 详 §5.1。

### §3.3 D.3 资源类反向探针 false positive（HTML 注释 `<input` 字面量）⚠️ 新模式

**问题：** D.3 写完 console_panel.html + smoke test 后，`ConsolePanelResourceSmoke.ConsoleHtmlContainsReplPrimitives` 测失败：
```cpp
EXPECT_EQ(html.find("<input"), std::string::npos);  // 反向探针：确认无原生 input
```
HTML 注释里写「<!-- Why use `<span>` not `<input type="text">`? -->」字面量被 `find("<input")` 命中 → 反向探针 false positive。

**根因：** 反向探针 grep 范围是全文件（含注释），但反向探针目标是「**non-comment** 区域无 `<input>`」。注释里的字面量（用作设计说明）不应触发。

**修复：** 注释重写为「Why no native input element here?」（avoid the literal `<input` string），3 min 内完成。

**影响：** 单次返工 ~3 min；但暴露了「资源类反向探针应限定到非注释区域」类规则。

**沉淀路径（P1 改进建议）：** writing-plans skill「资源类反向探针应限定到非注释区域」新子条（详 §5.3）。

### §3.4 代码量超估 ~2.4×（plan ~800-900 行 vs 实际 +2181 行）

**问题：** plan §5 总变更估「+800-900 行」，实际 src+tests +2181 行（+~140% 偏差）。

**根因分析：**
- **隐性附加工作**：DomBindings host data channel 扩展（+150 行 dom_bindings.cc / +31 行 dom_bindings.h）— 创意阶段 path 2 选定，但 plan §5 行数估时仅算 path 1（在 console ctx 注册），未算 host data channel 扩展
- **测试增量超估**：plan estimate 测试 ~360 行（4 文件 / ~20 测）→ 实际 +796 行 测试代码（console_engine_test 91+111 / console_bindings_test 185 / console_api_test 204 / dogfood smoke 205） / 32 测
- **smoke 替代手动验证的代码增量**：D.3 plan 写「手动 hello_devtool 切 Console tab 视觉验证」→ 实际改为 +50 行 smoke test（4 tab structure + 3 placeholder + 5 level color + console JS 必需 binding + `<input>` 反向探针）— 更可靠 + CI 自动化但增量超估

**影响：** 单子任务内未触发 escalate（每个子任务实测都在 plan 上沿内，且 ~17 min build 总时长仍极速）；但反映「plan 行数估算需含 host data channel 扩展、host endpoint 双注册、自实现算法、smoke 替代手动验证」类的隐性附加工作。

**沉淀路径（P2 改进建议）：** writing-plans skill「LOC 估算附录」加「隐性附加工作类型清单」+ ×1.3-1.5 buffer 范本（详 §5.4）。

---

## §4 — 经验教训（4 项）

### §4.1 Phase 0 grep 应深查「panel JS / 用户脚本运行 ctx 归属」+ 「host binding 注册 ctx 一致性」 — 反复模式 #1 第 4 个新形式

**教训：** Phase 0 §0.4 grep DomBindings + RegisterDevtoolBindings 注册位置 + query-only 性质 ✅，但未查「panel JS（即注入到 LoadDevtoolDocument 的 inline JS）实际运行在哪个 JSContext」。结果：D.2 在 console_script_engine_ ctx 注册 drain，panel JS 在 devtool_script_engine_ ctx 运行 → wiring 错位（D.4 修复）。

**沉淀方式（P1 改进建议）：** `.cursor/rules/skills/writing-plans.mdc` Phase 0 段补强：

```markdown
### Phase 0 — JS context 归属与 host binding 注册 ctx 一致性 audit

如果任务涉及多 JSRuntime 实例 + DevTool/Inspector 类内联 JS：
1. grep 「Register*Bindings」找出所有 host endpoint 注册点 → 列每个 endpoint 注册到哪个 ctx
2. grep 「LoadDevtoolDocument / load_inline_js / etc」找出 inline JS 实际 eval 走哪个 ctx
3. 验证「inline JS call 的 endpoint」⊆「该 ctx 上注册的 endpoint」
4. 如有 mismatch → 必须在 plan 阶段决定 host data channel 路径 vs ctx 切换路径
```

### §4.2 plan-fact reconcile 重复模式触发 P0 立即落实（不能再等下次工作流元任务）

**教训：** TASK-05 reflect §3.1 已识别「config 矩阵 guard 边界假设漏审」为 P1（writing-plans.mdc audit 子条强化），原计划下次工作流元任务批量落地。本任务**第 2 次连续命中** → P1 升级 P0 紧迫性触发条件成立（**反复模式 #1 子维度第 3 次=P0 升级阈值**已达）。

**沉淀方式（P0 改进建议）：** archive 阶段直接落实 — 在 archive 时**立即**改 `.cursor/rules/skills/writing-plans.mdc` «ctest 数量预期 config 矩阵» 段，不再等下次工作流元任务批量落地（详 §5.1）。这是「**反复模式 → 3 次后强制 P0**」原则的实证（systemPatterns 「反复模式渐进式抑制」段已沉淀）。

### §4.3 资源类反向探针应限定到非注释区域 — 新规则候选

**教训：** D.3 反向探针 `EXPECT_EQ(html.find("<input"), npos)` 因 HTML 注释里 `<input` 字面量触发 false positive。资源类反向探针需要「明确范围 = 非注释区域」语义。

**沉淀方式（P1 改进建议）：** writing-plans.mdc「资源类反向探针 SOP」新子段：

```markdown
### 资源类反向探针 SOP（HTML/CSS/JS/template）

如果反向探针目标是「该资源不应包含 X 字面量」：
- 错误范本：EXPECT_EQ(text.find("X"), npos)  // 注释里 X 字面量也触发
- 正确范本（其一）：
  (a) 注释规约：禁止在注释里写 X 字面量（comment policy）
  (b) 反向探针前先 strip 注释（HTML/CSS/JS comment 移除 helper）
  (c) regex 限定 non-comment 区域（复杂，慎用）
- 推荐 (a)：comment 策略加 lint 规则 + 反向探针保持简单
```

### §4.4 「creative 全锁死 + 范式 100% 复用 + AI agent 」 极速区候选 0.05-0.15× — 新子档识别

**教训：** 实测 build phase ~17 min vs plan ×0.6 180-240 min = **0.07-0.10×**（含 plan-fact reconcile #1 修复 + 反向探针 false positive 修 + 双 config ctest 验证）— **创历史新低**（破 TASK-05 0.16×）。触发新子档识别：

| # | 触发条件 | 本任务体现 |
|:-:|---|---|
| 1 | Creative 阶段 4+ 维度全锁死（含完整 cpp 代码骨架）| C1-C4 4 维度 + 1197 行 creative ✅ |
| 2 | 5 大架构范式 + 9 systemPatterns 100% 命中（含 N/A 计入）| §2.4 协同度 100% ✅ |
| 3 | Phase 0 grep ≥ 7 子段实证（前置依赖 / runtime 行为 / config 矩阵 / API 能力）| §2.1 7 子段 ✅ |
| 4 | 0 brainstorm 之外的设计决策 | 12/12 决策（C1-C4 + B1-B8）锁死 ✅ |
| 5 | plan-fact reconcile ≤ 2 项 + 在 build 同任务内修复（无返回 /creative 重做）| 本任务 2 项 plan-fact reconcile（C2 wiring + OFF +4） + 0 重写 ✅ |
| 6 | **AI agent 加速因子**（Edit/Write 工具瞬时编辑 + ctest 27s 全跑 + 工具调用并行） | 这是相对人类开发的 ~10-30× 加速 ⚠️ 应在新子档明确标注「AI agent 实测 vs 人类开发实测」差异 |

**沉淀方式（P0 改进建议 — archive 阶段直接落实）：** systemPatterns.md plan ×0.6 矩阵新子档「**creative 全锁死 + 范式 100% 复用极速区**」入库 + 6 触发条件 + AI agent 加速因子标注 + 第 73-77 数据点群组（5 子任务）。

**⚠️ 重要附注：** 0.07-0.10× 实测包含强 AI agent 加速因子（Edit/Write 接近瞬时编辑 + ctest 27s 跑 + 工具调用并行）— **应在新子档明确区分「AI agent 实测」vs「人类开发实测」**，避免后续任务用此基线给人类开发预算造成误导。建议 plan ×0.6 矩阵段加新「实测条件」列：standalone-AI / human-only / mixed。

---

## §5 — 改进建议（5 项 / P0/P1/P2）

| # | 建议 | 优先级 | 落实方式 | 目标 |
|:-:|---|:-:|---|---|
| **1** | **`.cursor/rules/skills/writing-plans.mdc` «ctest 数量预期 config 矩阵» 段补强 audit 子条**「待新增测的 add_test 是否有 config guard」+ grep 范本（**反复模式 #1 第 2 次连续命中升级 P0**）| **P0 立即** | archive 阶段直接落实（不再等下次工作流元任务批量落地）| `.cursor/rules/skills/writing-plans.mdc` |
| **2** | **systemPatterns.md plan ×0.6 矩阵新子档「creative 全锁死 + 范式 100% 复用极速区」0.05-0.15× 入库** + 6 触发条件 + AI agent 加速因子标注 + 第 73-77 数据点群组 | **P0** | archive 阶段直接落实 | `memory-bank/systemPatterns.md` |
| 3 | **writing-plans.mdc Phase 0 段补强「JS context 归属与 host binding 注册 ctx 一致性 audit」子条** — 反复模式 #1 第 4 个新形式（panel JS / 用户脚本 ctx 归属未实证）| P1 | 下次工作流元任务批量落地 | `.cursor/rules/skills/writing-plans.mdc` |
| 4 | **writing-plans.mdc「资源类反向探针 SOP」新子段** — 资源反向探针应限定到非注释区域 / comment policy 推荐 | P1 | 下次工作流元任务批量落地 | `.cursor/rules/skills/writing-plans.mdc` |
| 5 | **writing-plans.mdc「LOC 估算附录」加「隐性附加工作类型清单」+ ×1.3-1.5 buffer 范本** — DomBindings host data channel 扩展 / smoke 替代手动验证 / 自实现算法 等 | P2 | 下次工作流元任务批量落地（与 TASK-20260503-02 P2 #4 同源 — plan §文档段落 LOC 预估系数 ×1.5-2× 修正合并）| `.cursor/rules/skills/writing-plans.mdc` |

**优先级定义复述：**
- **P0 立即**：影响当前工作流正确性 / 必须本任务归档前落实
- **P1 下次**：下个同类任务前应落实 / 迁移到 `activeContext.md` 待处理事项
- **P2 长期**：记录到 `systemPatterns.md` 作为长期改进方向

---

## §6 — 技术改进建议（3 项）

| # | 建议 | 时机 |
|:-:|---|---|
| 1 | **C2 creative 文档 path 1 → path 2 主次互换** — 当前 creative-devtool-console.md §C2.B 主述 path 1（在 console ctx 注册），path 2 仅作 fallback；实际 path 2 才是正确的（panel JS 在 devtool ctx 跑）。建议 archive 阶段或 P3 候选阶段更新 creative 文档主次（不阻塞 archive）| P3 候选 — 留后续（如下次有 Console 类扩展任务时一并） |
| 2 | **TruncateAtUtf8Boundary 算法提升到 utf.cc 公共函数** — 当前自实现在 console_bindings.cc，可能其他模块（log truncate / network response truncate）也需要；建议 P3 候选阶段提取到 `veloxa/util/utf.cc::FindUtf8BoundaryBeforeNthByte(text, n)` | P3 候选 — 留后续 |
| 3 | **Console 异常情况 detach/re-attach test 增加多次循环验证** — 当前 dogfood smoke `DogfoodDetachReattachIsolation` 仅测 1 次 detach + re-attach；建议未来扩为 5+ 次循环 + memory leak audit（valgrind / asan）| P3 候选 — 留后续 |

---

## §7 — 反复模式识别（1/7 部分命中 — TASK-04 反复模式 #1 第 2 次连续命中升级 P0）

| # | 已知反复模式 | 历史频率 | 本次是否重复？ | 抑制策略 |
|:-:|---|:-:|:-:|---|
| 1 | 计划文件清单与实际变更不一致 | 9+ | ❌ | 5/5 文件清单命中 / +2181 行 vs +800-900 估虽超 ~2.4× 但单子任务内未触发 escalate（隐性附加工作类型已沉淀 P2 改进）|
| 2 | 子代理产出需大量返工 | 7+ | ❌ | 本任务无子代理使用 |
| 3 | **前置依赖/环境/API 能力未验证** | **9+ → 第 11 次抑制成功部分** | **⚠️ 部分命中**（**plan-fact reconcile #1 + #2 双命中 — 第 11 次抑制成功但 ctx 归属未实证 + config 矩阵 guard 假设漏审 2 项 →「config 矩阵 guard 边界假设漏审」第 2 次连续命中升级 P0**）| Phase 0 §0.4+§0.5 grep 已抑制 main runtime 行为假设 / 但 panel JS ctx 归属 + OFF guard 假设漏审 → §5.1+§5.3 P0 改进 |
| 4 | 非默认路径（流式/错误/缓存）遗漏验证 | 4+ | ❌ | EvalGlobal 错误路径在 D.4 + D.5 5 维度测覆盖（kAborted + uncatchable + 重置 + lazy-attach + buffer too small 双调用）|
| 5 | 测试隔离问题（flaky/并行冲突）| 7+ | ❌ | 32 新测全部独立 ConsoleEngine 实例 / 无全局状态 / 无 race / 1284 PASS 27.61s 全绿 |
| 6 | 提交粒度偏离计划 | 7+ | ❌ | 6/6 commits 严格 1/子任务 + Source 溯源 + 实测数据 |
| 7 | TDD 严格度与场景不匹配 | 11+ | ❌ | [覆盖补充] 模式与 plan §3 标注一致 / D.1-D.5 含 RED-GREEN cycle / D.3 [文档/资源调整模式] 5 smoke 替代手动视觉 |

**反复模式自检结论：** **1/7 部分命中**（反复模式 #1「前置依赖/环境/API 能力未验证」第 11 次抑制成功部分 — main runtime 行为已抑制，但 panel JS ctx 归属 + config 矩阵 guard 假设两个新形式漏审）。

**关键升级触发：** 反复模式 #1 子维度「config 矩阵 guard 边界假设漏审」**第 2 次连续命中**（TASK-05 第 1 次 → TASK-04 第 2 次） → **达 P0 升级阈值**（`reflection §3.2 + §4.2`）→ archive 阶段必须立即修 writing-plans.mdc（不再等下次工作流元任务批量落地）。

**vs 历史比较：**

| TASK | 反复模式命中 | 备注 |
|---|:-:|---|
| 502-01 Phase A | 0/7 | — |
| 502-02 Phase B | 0/7 | — |
| 503-01 Phase C | 1/7（部分）| 工具链 binutils 2.46+ 行为变化 |
| 503-02 工作流元 | 0/7 | — |
| 503-03 三件套收官 | 1/7（部分）| dirty_ runtime 行为假设 |
| 503-05 Interrupt | 0/7 | — |
| **503-04 Console** | **1/7（部分）— ctx 归属 + config guard 双新形式** | **反复模式 #1 子维度 #3 第 2 次连续命中 → P0 升级** |

---

## §8 — 安全评估

**任务标 [安全相关] ✅** — T1 任意 eval mitigation 完整闭环（首次完整暴露 — 5 维度全实施）+ T6 buffer 上限保护

| 维度 | 状态 | 备注 |
|---|:-:|---|
| **输入验证** | ✅ | (1) vx_view_eval_console source 长度受 EvalGlobal source.size() ≤ 256 KiB 限制（沿用既有逻辑） / (2) console_log_buffer Push 双上限（max_count=1000 / max_text=4096 byte）+ UTF-8 boundary safe truncate / (3) drain JSON envelope 标量类型严格（u8 level + std::string text + u64 ts）|
| **认证/授权** | ⊘ N/A | DevTool 默认 attach 受用户/host 控制（lazy-attach C ABI / vx_view_attach_devtool）；Console binding 严格 capability allowlist（仅 console.log/error/warn）|
| **数据保护** | ✅ | (1) Console JS 在隔离 console_script_engine_ ctx 跑（**isolated JSRuntime** — spec §11.1 强制要求 ✅ 完整实施）/ (2) 不暴露 dom_json/perf_stats/eval/drain 给 console scope（防 user JS 探测自身 buffer 或递归 eval / Console 严格隔离）/ (3) DEVTOOL=OFF 下 console 子系统**完整零字节存在**（A14 +6 符号 0 泄漏 ✅ 完整实施）|
| **依赖审计** | ⊘ N/A | 零新依赖（quickjs-ng 已离线预置 / FetchContent F9 跳过 / 复用 TASK-05 闭环的 SetEvalInterruptBudget API）|
| **错误信息脱敏** | ✅ | (1) vx_console_eval_status enum 仅返枚举值（无路径 / 无内部状态泄露）/ (2) RUNTIME_ERROR 时 quickjs 异常消息走 toString() 限定到 out_buf_size（不泄露内部 stack trace）/ (3) NOT_ATTACHED 不泄露 attach 状态查询机制 |
| 敏感数据处理 | ⊘ N/A | — |
| **T1 mitigation 5 维度完整性** | ✅ **首次完整暴露** | (1) 默认安全：ConsoleEngine::Init 强制 SetEvalInterruptBudget(10000) / (2) opt-in：caller 显式 SetEvalInterruptBudget(0) 才关闭 / (3) **不可绕过**：budget 在 ConsoleEngine 类持有，user JS 不可访问 + JS_SetUncatchableError 设计意图 / (4) 状态可查：WasInterrupted() 暴露 / (5) 单线程 atomic：QuickjsEngine 既有实现 / **8 单测全 PASS（D.1 5 + D.5 5 + dogfood 3）**|
| **capability allowlist 完整性** | ✅ | RegisterConsoleBindings 全量 API 表 4 函数（console.log/error/warn + drain）+ **反向探针单测**（grep 8 项 mutator dom_json/perf_stats/eval/append/replace/setAttribute/load/fetch 不可达）|
| 安全测设计 | ✅ | T1 5 维度单测 + dogfood 端到端死循环 budget 触发 + lazy-attach OFF 路径 stub + capability allowlist 反向探针 = 全维度覆盖 |

**T1 mitigation 5 维度完整闭环（spec §11.1）：**

```
默认安全：       ConsoleEngine::Init() 强制 SetEvalInterruptBudget(10000)
opt-in 启用：    显式 SetEvalInterruptBudget(N>0/0) 仅 host 可调
无法绕过：       budget 在 ConsoleEngine 类持有，user JS source 无法触达
                JS_SetUncatchableError → JS try/catch 无法捕获 interrupt
状态可查：       WasInterrupted() / 宿主可在每次 EvalGlobal 后判断
回调约束：       继承 QuickjsEngine 既有 atomic 实现（仅 atomic 递减 + return）

capability allowlist（spec §11.1 强制）：
- 仅暴露：console.log / console.error / console.warn / vx_devtool_get_console_log_drain
- 不暴露：dom_json / perf_stats / eval / DOM mutator (setAttribute/replaceChild/etc)
- 不暴露：file (read/write/exists) / network (fetch/XHR) / Hot Reload trigger
- 反向探针单测验证 8 项 mutator 不可达
```

**spec §11.1 闭环：** Console + JS REPL T1 任意 eval 威胁面**完整 mitigation**（5 维度 + capability allowlist + double cap buffer + lazy-attach C ABI）✅；T6 buffer 上限保护 ✅；**DevTool 第 4 件套 Console 完整闭环** — Inspector + Performance Overlay + Hot Reload + Console = 4 件套全 ✅。

---

## §9 — Phase 0 投入定律 sept-evidence 升级（候选）

**历史数据点：**

| TASK | Phase 0 投入 | Build 节省（估）| ROI |
|---|:-:|:-:|:-:|
| 502-01 Phase A | ~30 min | ~159 min | 5.3× |
| 502-02 Phase B | ~30 min | ~157 min | 5.2× |
| 503-01 Phase C | ~30 min | ~229 min | 7.6× |
| 503-02 工作流元 | ~10 min | ~67 min | 6.7× |
| 503-03 三件套收官 | ~10 min | ~80 min | 8.0× |
| 503-05 Interrupt | ~5 min | ~80 min | 16×（历史新高）|
| **503-04 Console** | **~5 min（极简 7 子段）** | **~30+ min（避免 creative 阶段返工 + build 阶段 wiring 错位返工双重收益）** | **~6-10× 估值**（落 sept-evidence 升级范围内）|

**升级：** sext-evidence → **sept-evidence**（7 次实证）/ ROI 范围 5.2× - 16× / 平均 ~8.4×

**~6-10× ROI 的加速因子：**
- Phase 0 §0.4+§0.5 grep 抑制 main runtime 行为假设（但 panel JS ctx 归属漏查 → §5.3 改进）
- Phase 0 §0.6 配置矩阵假设虽 partial 错误（OFF +4 而非 0），但**主 build 没受影响**（错误仅在 ctest 数字校准 — §3.2 → §5.1 升级 P0）
- creative 4 维度全锁死 + 完整 cpp 代码片段 = build 阶段 0 设计探索成本
- 5 大范式 + 9 systemPatterns 协同度 100% = 0 学习成本

---

## §10 — 跨任务沉淀小结（5 项）

| # | 沉淀项 | 落实方式 | 引用 |
|:-:|---|---|---|
| 1 | **「config 矩阵 guard 边界假设漏审」第 2 次连续命中 → 升级 P0**：writing-plans.mdc audit 子条强化 + grep 范本 | **P0 — archive 阶段必须立即落实** | §3.2 + §4.2 + §5.1 |
| 2 | **plan ×0.6 矩阵新子档「creative 全锁死 + 范式 100% 复用极速区」0.05-0.15×**：第 73-77 数据点群组入库 + AI agent 加速因子标注 + Phase 0 投入定律 sept-evidence 升级 | **P0 — archive 阶段** | §1 + §4.4 + §9 |
| 3 | **isolated JSRuntime 隔离模式 + capability allowlist 范式新沉淀**：systemPatterns.md 加新段「DevTool isolated JSRuntime 协议」（spec §11.1 落地范本）| **P0 — archive 阶段** | §2.4 + §8 |
| 4 | **JS context 归属与 host binding 注册 ctx 一致性 audit**：Phase 0 grep 子段补强 — 反复模式 #1 第 4 个新形式 | P1 — 下次工作流元任务 | §3.1 + §4.1 + §5.3 |
| 5 | **资源类反向探针 SOP**：注释里字面量不应触发反向探针 / comment policy 推荐 | P1 — 下次工作流元任务 | §3.3 + §4.3 + §5.4 |

**P0 落实清单（archive 阶段必须）：**
- [ ] writing-plans.mdc «ctest 数量预期 config 矩阵» 段补强 audit 子条（§5.1 — 反复模式 #1 第 2 次连续命中升级）
- [ ] systemPatterns.md plan ×0.6 矩阵新子档入库（§5.2 — 0.05-0.15× 极速区候选）
- [ ] systemPatterns.md「DevTool isolated JSRuntime 协议」新段沉淀（§5 #3）
- [ ] activeContext.md 待处理事项段更新（P0 已落实标记 / P1 新增 2 项 / 旧 P1 状态同步）

---

## §11 — 估时校准（plan ×0.6 数据点入库）

**第 73-77 数据点群组**（5 子任务 + finalize）：

| # | 子任务 | plan ×0.6 估 | 实测（commit 间隔） | 比值 |
|:-:|---|:-:|:-:|:-:|
| 73 | D.1 ConsoleEngine + lifecycle | 30-40 min | ~3 min | 0.08-0.10× |
| 74 | D.2 RegisterConsoleBindings + bridge + drain | 40-50 min | ~3 min | 0.06-0.08× |
| 75 | D.3 Console panel + 4 tab | 40-50 min | ~3 min | 0.06-0.08× |
| — | CP1 自审 | — | <1 min | — |
| 76 | D.4 公开 C API + plan-fact reconcile #1 wiring fix | 30-40 min | ~4 min | 0.10-0.13× |
| 77 | D.5 5 维度 T1 + 3 dogfood + A14 + techContext | 40-60 min | ~3 min | 0.05-0.08× |
| — | CP2 自审 | — | <1 min | — |
| — | finalize | — | ~1 min | — |
| **总** | — | **180-240 min** | **~17 min** | **~0.07-0.10× 创历史新低** |

**新数据点中位 0.07-0.08×，分布偏左极速区**：5/5 实施子任务全部 ≤ 0.13×；D.5 = 0.05-0.08× 是最低（含 8 测 + A14 + techContext 仍 ~3 min）；D.4 = 0.10-0.13× 是上沿（含 plan-fact reconcile #1 修复）。

**vs 历史比较：**

| TASK | 实测 vs plan ×0.6 | 子档 |
|---|:-:|---|
| 502-01 Phase A | 0.64× | 大件实现下沿外 |
| 502-02 Phase B | 0.40× | 极窄档延续高效区 |
| 503-01 Phase C | 0.31× | 极窄档加速衰减区下沿 |
| 503-02 工作流元 | 0.21× | 纯文档/规则极速区 |
| 503-03 三件套收官 | 0.42× | 极窄档延续高效区 |
| 503-05 Interrupt | 0.16× | 最小代码改动 + Phase 0 高度预跑极速区（200 行下） |
| **503-04 Console** | **0.07-0.10×** | **「creative 全锁死 + 范式 100% 复用 + AI agent 加速极速区 0.05-0.15×」候选新子档**（含 +2181 行 / 32 测 / 6 commits / plan-fact reconcile 双 / DEVTOOL 双 config）|

**⚠️ AI agent 加速因子标注：** 0.07-0.10× 实测 build phase ~17 min 包含强 AI agent 加速因子（Edit/Write 接近瞬时 + ctest 27s 跑 + 工具调用并行）。**应在新子档明确区分「AI agent 实测」vs「人类开发实测」**，避免后续任务用此基线给人类开发预算造成误导。建议 plan ×0.6 矩阵段加新「实测条件」列：standalone-AI / human-only / mixed。本任务记 standalone-AI。

---

## §12 — 下一步

1. **立即落实 P0 改进建议**（archive 阶段必须）：
   - **writing-plans.mdc «ctest 数量预期 config 矩阵» 段补强 audit 子条**（反复模式 #1 第 2 次连续命中升级 P0 — 不能再等下次工作流元任务）
   - **systemPatterns.md plan ×0.6 矩阵新子档入库**（0.05-0.15× 极速区候选 + AI agent 加速因子标注）
   - **systemPatterns.md「DevTool isolated JSRuntime 协议」新段沉淀**（spec §11.1 落地范本）
2. **迁移 P1 改进建议**到 `activeContext.md` 待处理事项段：
   - JS context 归属与 host binding 注册 ctx 一致性 audit（writing-plans Phase 0 段）
   - 资源类反向探针 SOP（writing-plans 新子段）
3. **迁移 P2 改进建议**到 `systemPatterns.md` / `techContext.md` 长期沉淀：
   - LOC 估算附录加「隐性附加工作类型清单」
4. **后续连接：** archive 完成后 spec §11.1 Console 完整闭环 ✅ + DevTool 4 件套全部完整 ✅ → 解锁 TASK-30-04-E JS Debugger backend（如用户决策启动）

---

**回顾完成。下一步：** 用户调用 `/archive` 启动归档阶段。
