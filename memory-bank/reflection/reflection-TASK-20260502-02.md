# 回顾：TASK-20260502-02 DevTool Phase B — Performance Overlay 实施

**日期：** 2026-05-02
**任务 ID：** TASK-20260502-02
**复杂度级别：** Level 3（中等功能 — 跨 5 子系统 / 10 子任务 / 触及 #35 但非 dogfood 子系统级 / **零 escalation**）
**安全相关：** ✅ 是（T5 + T6 双威胁面全程守门）
**分支：** `feature/TASK-20260502-02-devtool-perf-overlay`（基于 main `8b2ead4` 即 Phase A 完整归档）

---

## 1. 计划 vs 实际

### 1.1 总体对照

| 维度 | 计划（plan 落盘时） | 实际 | 偏差原因 |
|---|---|---|---|
| 子任务数 | 10 | 10 | 100% 匹配 ✅ |
| 估时（plan ×0.6） | 261 min（4.35 h） | ~104 min（1.7 h） | **-60%** — 实测 0.40× ×0.6（VAN 预测 0.55-0.75× 偏保守） |
| 子系统跨度 | 5（core / devtool-overlay / devtool-resources / api / tests）| 5（含 examples 加 hello_devtool 扩展，B.3.2 算 examples 子目录边缘）| 一致 |
| 文件变更（新建/修改）| ~20 文件（plan §0.2 估）| 24 文件（含新建 4 + 修改 20）| 略多 — 测试文件 +4 而非估的 +3（dirty_rects 单独测 +1）|
| 测试增量 | DEVTOOL=ON +~17 / DEVTOOL=OFF +~3 | **DEVTOOL=ON +59 / DEVTOOL=OFF +17** | 增量 **3.5 倍**于预期 — 各子任务测覆盖度更高（B.0.1 +11 / B.1.2 +12 / B.0.2 +6 等） |
| 设计变更 | — | **B.1.2 budget guard `budget_us == 0` 显式短路**（plan-fact reconcile）| 浮点精度边缘场景 — sub-µs 硬件计时器 |
| commits | 10 + 1 finalize = 11（B6=A 决策） | 10 + 1 finalize = 11 | 100% 匹配 ✅ |
| escalation | 0（VAN push-back 1 锁定 Level 3）| 0 | 100% 匹配 ✅（vs Phase A escalation 中途到 Level 4） |
| 子代理调用 | 0（plan §B6 单 agent）| 0 | 100% 匹配 ✅ |

### 1.2 子任务级 plan ×0.6 vs 实测对照表

| 子任务 | plan ×0.6 | 实测 | 比值 | 桶分类 | 偏差关键因素 |
|:---|---:|---:|:---:|:---:|:---|
| B.0.1 PipelineHooks 五钩子 + C API | 54 min | 25 min | **0.46×** | 极窄档 | 子系统位点已 grep 实证 + 测试 fixture 复用 + 反向探针清晰 |
| B.0.2 dirty_rects_ Vector 累积 | 36 min | 7 min | **0.19×** | 极窄档下限 | Vector + push_back + clear API 极简，单点修改 |
| B.1.1 PerfOverlay ring buffer | 36 min | 8 min | **0.22×** | 极窄档下限 | 纯算法零外部依赖 + friend pattern 引入清晰 |
| B.1.2 PerfOverlay::Attach + T6 budget | 27 min | 15 min | **0.56×** | 中件实施下端 | SteadyTimePoint 时间差 + 5 trampoline + budget guard 边缘场景 + plan-fact reconcile（budget=0）|
| B.2.1 HUD HTML/CSS | 27 min | 7 min | **0.26×** | 极窄档 | HTML/CSS 资源类纯文本编辑 |
| B.2.2 HUD JS + binding | 18 min | 13 min | **0.72×** | 中件实施下端 | 跨 vx_script ↔ vx_devtool 边界 + forward declare 解循环 + JSON envelope 设计 |
| B.2.3 OverlayDirtyRect + InjectDirtyRectHighlights | 18 min | 7 min | **0.39×** | 极窄档 | InspectorOverlay 范式精准复用 |
| B.3.1 F11 toggle HUD | 9 min | 10 min | **1.11×** | 中件实施 | 跨 5 文件更新 — 单文件计时小但跨度多 |
| B.3.2 hello_devtool perf smoke | 18 min | 7 min | **0.39×** | 极窄档 | examples 增量编辑 + ctest PASS_REGULAR_EXPRESSION 范式 |
| B.3.3 Phase B finalize + A14 smoke | 18 min | 5 min | **0.28×** | 极窄档 | 纯文本 cmake 黑名单追加 + ctest reverify |
| **合计** | **261 min** | **~104 min** | **0.40×** | **极窄档延续高效区** | **见下面 §2 / §4 详细分析** |

**桶分布统计：** 7 子任务落极窄档（≤ 0.5×）/ 2 子任务落中件实施下端（0.5-1.0×）/ 1 子任务接近 1.0×（B.3.1 跨 5 文件）/ 0 子任务 escalation 后 / 0 子任务 > 1.5×（plan-fact reconcile 严重场景）

---

## 2. 做得好的

### 2.1 Phase 0 投入越深 / build phase 越快定律的第二次实证

Phase A reflection §11.2 首次提出该定律（Phase A 0.64× = 大件实现桶下沿外）；本次 Phase B **0.40× 是该定律的第二次也是更强的实证**：

- Plan §0.1-0.11 11 子段全部实测填写（ctest baseline 二次验证 + CMake 链接拓扑 + 静态库循环依赖 + FetchContent 守卫 + 测试基础设施审计 + 边界输入清单 16 条 + 调用链端到端 + 既有测试隐式契约 fingerprint + CSS shorthand 能力 grep 表 + 工具链 + jq 兜底）
- 这些实测让 build phase 的「探索成本」降到接近零 — grep callsite 已知 / 测试 fixture 复用方案已锁 / friend pattern 引入位点已审计
- 实证比 plan 阶段 ~30 min 投入换来 build phase ~157 min 节省（以 plan ×0.6 为基线）= **5.2× ROI**

### 2.2 5 大可复用架构范式（来自 Phase A 沉淀）100% 命中

Phase A 沉淀的 5 大范式在 Phase B 全部精准复用：

| 范式 | Phase B 命中点 | 加深价值 |
|---|---|---|
| 中央调度协议（UpdateManager）| B.0.1 PipelineHooks 是该模式自然扩展 | ✅✅ 验证范式向新维度（performance hook）扩展性 |
| 函数指针 + nullptr 分支预测优化 | B.0.1 5 个 hook 默认 nullptr + branch predictor friendly fire helper | ✅✅ 验证范式在 hot path 的性能可接受性 |
| 双层 API（C++ 内部 + 公开 C ABI）| B.0.1 PipelineHooks / B.2.2 PerfStats 都遵循 | ✅✅ 验证范式跨 4 个 C API 表面（attach + get_stats + is_hud_visible + set_pipeline_hooks） |
| #ifdef + CMake 双层 conditional 子系统 | B.0.1-B.3.3 全程守护 — A14 黑名单 +2 项闭环 | ✅✅ 验证范式跨「Phase A → Phase B」迭代的稳定性 |
| dogfood 路径 = 探测性 acceptance test | B.2.1 HUD CSS 用 R2-verified 子集（持续验证 CSS 引擎） | ✅✅ 验证范式在新 UI 元素引入时的探测有效性 |

**结论：** Phase A 范式不是一次性沉淀 — 在 Phase B 是连续生效的「设计资产」。

### 2.3 plan-fact reconcile 数量大幅下降

| 维度 | Phase A | Phase B | 改善因素 |
|---|---|---|---|
| plan-fact reconcile 总数 | **11 处** | **1 处**（B.1.2 budget=0 浮点精度）| **-91%** |
| Plan 范围错估 | 5 处 | 0 处 | Phase 0 11 子段实测填写 + 蓝图 plan 复用 |
| 测试假设错误 | 3 处 | 0 处 | plan §B.0.2 RED 测假设按 update_manager_test:131 现实校准（在 plan 阶段就校准） |
| 实施技术决策修正 | 3 处 | 1 处 | B.1.2 sub-µs 硬件精度边缘是不可预知的 |

**结论：** plan-fact reconcile 11 → 1 是 plan 质量显著提升的硬指标，非偶然。

### 2.4 零反复模式命中（创纪录）

按工作流规则的「7 已知反复模式表」对照：

| 已知模式 | 出现频率 | 本次是否重复？ |
|---|:-:|:-:|
| 计划文件清单与实际变更不一致 | 9+ 次 | ❌ **不重复**（10/10 子任务 plan §B.x.x 文件清单与实际改动 100% 匹配）|
| 子代理产出需大量返工 | 7+ 次 | ❌ **N/A**（零子代理调用，Level 3 单 agent 直跑）|
| 前置依赖/环境/API 能力未验证 | 8+ 次 | ❌ **不重复**（Phase 0 11 子段实测填写 + grep/ctest 全验证）|
| 非默认路径遗漏验证 | 4+ 次 | ❌ **不重复**（nullptr hooks / budget=0 / Attach 二次 / dirty rect empty 等边界全覆盖）|
| 测试隔离问题 | 7+ 次 | ❌ **不重复**（59 新测零 flaky / 零并行冲突）|
| 提交粒度偏离计划 | 7+ 次 | ❌ **不重复**（严格 1 commit/子任务 + 1 finalize commit = 11 commits）|
| TDD 严格度与场景不匹配 | 11+ 次 | ❌ **不重复**（每子任务 RED → GREEN → REFACTOR + 反向探针精准捕获）|

**0/7 命中是任务质量的最强证据。**

### 2.5 安全威胁全程守门（T5 + T6）

- **T5（DisplayList overlay 跨帧累积）**：B.2.3 OverlayDirtyRect 复用 ResetOverlayCommands 协议（同 hover 共用清理路径，零新 reset 路径）— 范式复用 / 不扩增设计表面
- **T6（UpdateManager callback 任意代码执行）**：B.1.2 三层 mitigation
  - 单 instance Attach 守（第 2 次 Attach 拒绝 + last_attach_error 设值）
  - budget=0 显式短路（plan-fact 校准 — 避免 sub-µs 硬件精度漏报）
  - 1ms/frame budget guard 累加 abort + abort_count + last_abort_reason

每个 mitigation 都有 ≥ 1 单元测覆盖 + 反向探针精准捕获。

### 2.6 lazy-attach 设计的端到端实证

B.0.1 设计的 lazy-attach 模式（`Application::SetPipelineHooks` 缓存到 `external_hooks_` + `EnsureUpdateManager` 时 apply）在 B.3.2 hello_devtool perf smoke 端到端验证：

- caller 顺序 `attach_devtool → load_html/css → set_pipeline_hooks(rc=-2) → vx_view_run` 工作正确
- INVALID_STATE 是「提示」而非「失败」— hooks 已 cached
- vx_view_run 内 timer 触发 EnsureUpdateManager 后 hooks 激活 → frames=1 ✅

**这是 C ABI 设计中「容错优于强约束」原则的优秀实践。**

### 2.7 A14 守门递增模式 — 黑名单 +2 项闭环

A14 黑名单的 Phase 演进：
- **Phase A 起点（main `8b2ead4`）**：8 项黑名单（RegisterDevtoolBindings / kInspectorPanelHtml/Css/Js / InspectorOverlay / InjectHoverHighlight / InputDispatchSplitter / SerializeDocument）
- **Phase B 终点（B.3.3 commit `3f4f266`）**：10 项黑名单（**+PerfOverlay + InjectDirtyRectHighlights**）+ Phase A/B 区分注释 + NOT in blacklist intentional 段说明

注释中明确标注「NOT in blacklist (intentional): vx_view_set_pipeline_hooks / vx_view_get_perf_stats / vx_view_is_hud_visible 公开 C ABI 是设计 OFF stub；VxViewGetPerfStats 在 vx_script anonymous namespace 不在检查 archive 内」— **黑名单维护透明度从「单纯列表」升级为「列表 + 维度区分注释 + 排除项清单」**。

### 2.8 R3-R9 风险登记 100% 闭环

| # | 风险 | 闭环证据 |
|:-:|---|---|
| R3 | 五钩子重构性能 | NullHooksUpdateRemainsLossless 测过；nullptr branch predictor 设计落地 |
| R7 | dirty_rects 无 cap | 实测 hover change 累积 1-2 矩形（非 unbounded）+ PerfOverlay::OnFrameStart ClearDirtyRects 自动清 |
| R8 | HUD `position: fixed` fallback | `position: absolute` 视觉等价；smoke 测 PASS（CssHudUsesAbsoluteNotFixed）|
| R9 | `pointer-events: none` 不支持 | data-passthrough="1" 占位；EventManager HitTest 改造留 R3+ P3 |

---

## 3. 遇到的挑战

### 3.1 sub-µs 硬件计时器精度边缘场景（B.1.2 plan-fact reconcile 唯一处）

**问题**：B.1.2 budget guard 原设计 `if (frame_total_us_ > budget_us_) abort()`，BudgetGuardZeroAlwaysAborts 测设 `budget_us_ = 0` 期望任何 hook 调用都触发 abort，但实测在某些机器上 cb_us（callback 时长 µs 转换）可能为 0（< 1 µs），导致 `0 > 0` = false → 测 FAIL（false negative）。

**根因**：`std::chrono::steady_clock::now()` 在某些硬件上分辨率 ≥ 1 µs，而 nullptr-body callback 实际耗时 < 1 µs → cast 后归零。

**解决**：改为 `if (budget_us_ == 0 || frame_total_us_ > budget_us_)` — 显式 disabled 语义短路，避免精度依赖。

**经验**：T6 边界场景测应该用「显式语义状态」而非「数值阈值依赖」，因为后者会受运行环境精度影响。

### 3.2 forward declaration 解决 vx_script ↔ vx_devtool 循环依赖（B.2.2）

**问题**：`vx_view_get_perf_stats` JS native binding 实施在 `vx_script/dom_bindings.cc`，需要持有 `PerfOverlay*`（来自 `vx_devtool/overlay/perf_overlay.h`）；但 `vx_script` 已被 `vx_devtool` 链接 → 反向 include 会破坏静态库链接拓扑。

**解决**：
- `dom_bindings.h` forward declare `namespace vx::devtool::overlay { class PerfOverlay; }` + 持 raw pointer
- `dom_bindings.cc` 仅在 `#ifdef VX_BUILD_DEVTOOL` 块内 include 完整 `perf_overlay.h`
- ABI 不变（公开头不依赖 vx_devtool）+ 链接拓扑不变（vx_script 不强依赖 vx_devtool）

**经验**：跨 lib 边界传递 raw pointer + forward declare 是解决「链接拓扑限制下持有对方对象」的标准范式（Phase A 已用 1 次 / Phase B 第 2 次复用 = 进入「已验证模式」）。

### 3.3 lambda 捕获不能赋 C function pointer（B.0.1 + B.3.2）

**问题**：测试 PipelineHooks 时希望用 lambda 闭包记录调用顺序，但 capture-ful lambda 不能赋给 C function pointer typedef。

**解决**：
- 测试代码：用 static recorder pattern（file-local `std::vector<std::string>*` + `std::atomic<int>` counter + 5 个 free function trampoline）
- examples/hello_devtool（B.3.2）：用 capture-less lambda（仅捕获 `static int s_perf_smoke_frames` 通过 userdata 指针传递）

**经验**：C function pointer 是 ABI 约束 — 设计阶段就应明确 userdata 传递机制；本次设计的 `void* userdata` 字段是必要的，不是可选的。

### 3.4 frames=1 而非多帧（B.3.2 实际输出 vs 预期）

**问题**：hello_devtool_perf_smoke 期望 300ms autoquit @ 60FPS = ~18 帧，但实测仅 frames=1。

**根因分析**（在快照中已记录）：
- SDL2 dummy driver + DevTool attach 路径内有 lazy 初始化
- update_manager_ 不是在 attach_devtool 时创建，而是在第一次 update timer 触发时创建（lazy attach 设计）
- 实际 EnsureUpdateManager → 应用 cached hooks → on_frame_end fires = 1 次（在 autoquit 触发前）

**评估**：ctest regex `[1-9][0-9]*` 接受 1+，符合 smoke 测「证 ABI 工作」语义而非「证性能」语义 — **frames=1 是 acceptable**。

**未推进**：如需多帧验证可后续 P3 任务调 SDL2 dummy 帧率或减少 EnsureUpdateManager 拖延，但本任务范围内不必要。

---

## 4. 经验教训

### 4.1 「极窄档延续高效区」是新桶系数子档（候选入库）

10 子任务平均 0.40× ×0.6 比 systemPatterns 既有「极窄档延续 0.30-0.50×」桶下沿更优 — 这是 **Phase 0 投入越深 / build phase 越快定律 + 5 大可复用架构范式 ≥ 5 项命中** 的复合结果。

候选新桶名：「**极窄档延续高效区 0.30-0.45× ×0.6**」
- 触发条件（叠加）：
  1. Phase 0 ≥ 9 子段实测填写（grep / ctest baseline / CMake / 测试基础设施审计 / CSS 能力 / 工具链）
  2. 5 大可复用架构范式 ≥ 5 项命中（中央调度协议 / 函数指针 nullptr / 双层 API / #ifdef CMake / dogfood）
  3. 零新子系统设计（仅扩展既有子系统）
  4. plan-fact reconcile ≤ 1 处
- 数据点：TASK-20260502-02 Phase B 全 10 子任务（0.40× 群组）

### 4.2 「VAN 预测保守 vs 实测惊喜」的 +信号识别

VAN 阶段 F2 dirty rect 已就绪发现 → 估时下调到 ~3-4 h plan ×0.6（0.55-0.75× 比值）；实测 0.40× — **比 VAN 预测低 50%**。

**这是「实测 < 预测」的 + 信号**（不是 - 信号），代表：
- 范式复用 ROI 大于预期
- Phase 0 投入收益大于预期
- 子任务粒度切分准（每个子任务大部分 ≤ 30 min plan，便于桶系数适用）

**反例的 - 信号**（应警惕）：
- 实测 > 预测 1.5× → plan 范围错估或子代理失控
- 实测 > 预测 2.5× → escalation 信号（建议中途进 `/plan` 修正）

### 4.3 Level 3 vs Level 4 的子代理决策

Phase A（Level 4）实施时也未用子代理，但当时 8 子任务 escalation 链很长 → 反思认为「子代理可能加速但风险高」。

Phase B（Level 3）10 子任务实测确认：**Level 3 单 agent 直跑高效**（0.40× plan ×0.6），不需要子代理调度。

**判定法则**（候选入库 systemPatterns）：
- Level 3 ≤ 10 子任务 + 5 大范式 ≥ 5 项命中 → 单 agent 直跑（推荐）
- Level 4 ≥ 12 子任务 + 涉及子系统级新设计 → 子代理可候选评估
- Level 4 涉及多 Phase 跨长期 → 子代理可候选

### 4.4 蓝图 plan 复用 + Phase 0 实测填写 = 双重质量保护

Phase B 复用 TASK-30-04 蓝图 plan §Phase B 段（10 子任务粗模板）+ Phase 0 11 子段实测填写实测细化 — **两层质量保护让 plan-fact reconcile 从 11 → 1 处下降 91%**。

**操作建议**（更新 plan 阶段流程）：
- 蓝图任务（V2=a）的 plan 应作为下游实施任务的「输入资产」
- 实施任务 plan 阶段必须做「蓝图 → 实测细化」转换（不能照搬，必须 grep 验证 + 当前 main 终态对照）
- TASK-20260502-02 plan §0 的 11 子段是该转换的标准模板

### 4.5 lazy-attach 设计模式 — C ABI 容错优于强约束

`Application::SetPipelineHooks` 在 update_manager_ 未初始化时返 INVALID_STATE 但 cache hooks → 在 EnsureUpdateManager 时激活。

这是 C ABI 设计的「容错优于强约束」原则的优秀实践：
- 不要求 caller 知道 init 顺序
- INVALID_STATE 是「提示」而非「错误」（caller 可忽略也可重试）
- 实施代码内承担「记忆 + 自动激活」复杂度

**候选入库 systemPatterns**：「lazy-attach C ABI 容错模式」（属于 Quirks-Mode HTML5 解析的 ABI 设计领域哲学映射）

---

## 5. 改进建议

| # | 建议 | 优先级 | 落实方式 | 目标 |
|:-:|---|:-:|---|---|
| 1 | plan ×0.6 第 38 数据点入库（10 子任务 0.40× 群组）+ 「极窄档延续高效区 0.30-0.45×」新子档候选 | **P1** | 更新 systemPatterns.md「plan ×0.6 估时按任务类型分桶系数矩阵」段 | systemPatterns.md |
| 2 | 「Phase 0 投入越深 / build phase 越快」工作流定律 dual-evidence 入库（Phase A 0.64× + Phase B 0.40× 双数据点群组）| **P1** | 新增 systemPatterns.md「Phase 0 投入 ROI 定律」段 | systemPatterns.md |
| 3 | lazy-attach C ABI 容错模式入库（INVALID_STATE 提示 + 内部缓存 + EnsureXX 激活）| **P1** | 新增 systemPatterns.md「lazy-attach C ABI 设计模式」段 | systemPatterns.md |
| 4 | A14 黑名单维护「Phase 区分注释 + NOT in blacklist 排除项清单」范式入库（Phase A → +8 / Phase B → +2 → 未来 +N 的递增模式）| **P2** | 更新 systemPatterns.md 「子系统关闭守门 ctest smoke 范式」段 — 加「黑名单维护演进透明度」子段 | systemPatterns.md |
| 5 | 5 大可复用架构范式（Phase A 沉淀）的连续生效证明 — 更新「Phase A → Phase B 都 100% 命中 = 设计资产」备注 | **P2** | 更新 systemPatterns.md 5 大范式各段，加「Phase B 复用证明」标注 | systemPatterns.md |
| 6 | 「Level 3 vs Level 4 子代理决策法则」入库（Level 3 ≤ 10 子任务 + 范式 ≥ 5 项命中 → 单 agent 直跑推荐）| **P2** | 更新 systemPatterns.md 子代理选型相关段 | systemPatterns.md |
| 7 | T6 边界场景测设计原则 — 用「显式语义状态」而非「数值阈值依赖」（B.1.2 budget=0 教训）| **P2** | techContext.md 安全测原则段 | techContext.md |
| 8 | 蓝图 plan + Phase 0 实测细化双重质量保护范式（plan-fact reconcile 11 → 1）| **P2** | systemPatterns.md「蓝图任务 V2=a 工作流变体」段附注下游实施任务对接范式 | systemPatterns.md |
| 9 | #35 阶段 2 P3 候选记录 — 拆 LayoutEngine 内 style/layout 子阶段实现 OnAfterStyle 与 OnAfterLayout 真实分离 | **P2** | tasks.md「技术债追踪」段 + activeContext.md 待处理事项 | tasks.md |
| 10 | R9 EventManager HitTest 改造 P3 候选 — HUD pointer-events 真支持（当前 data-passthrough="1" 占位）| **P2** | tasks.md「技术债追踪」段 | tasks.md |
| 11 | hello_devtool_perf_smoke frames=1 → 多帧 P3 候选 — 调 SDL2 dummy 帧率或减少 EnsureUpdateManager 拖延 | **P2** | tasks.md（低优先级，frames=1 已验证 ABI 工作）| tasks.md |

---

## 6. 技术改进建议

### 6.1 PerfOverlay 内部状态测试访问 — friend pattern 引入

**现状：** B.1.1 引入 `friend class PerfOverlayTest;` 是 veloxa/devtool 子树首次使用 friend pattern。

**评估：** plan §0.5 测试基础设施审计预判正确 — 没有用 `#define private public` 越权或 `reinterpret_cast` 反 cast。

**未来扩展：** 如其他 vx_devtool 内部组件需要白盒测访问内部状态，应延续 friend pattern。

### 6.2 PaintCommand 工厂复用而非新增 enum（B.2.3 决策 B5=A）

**现状：** B.2.3 OverlayDirtyRect 工厂复用 `kOverlayHighlight` enum + 默认黄绿色 + 1.0px stroke，**不新增 enum kOverlayDirtyRect**。

**收益：**
- paint_command.h / renderer.cc switch 零改动
- T5 ResetOverlayCommands 协议自动覆盖（dirty rect overlay 同 hover overlay 共用 reset 路径）

**经验：** 「枚举值新增 vs 工厂方法变体」的决策应优先选「工厂方法」— 类型新增会扩散到 switch / 工厂方法仅在调用点变体。

### 6.3 测试目录组织 — `tests/devtool/overlay/` 子树（B3=A 决策落实）

**现状：** B.1.1 新建 `tests/devtool/overlay/` 子目录（与 `tests/devtool/inspector/` 兄弟），由 `if(VX_BUILD_DEVTOOL)` guard。

**收益：** 命名清晰 + 测试组织与生产代码 `veloxa/devtool/overlay/` 镜像 + 易于后续扩展（e.g. `tests/devtool/profiler/` 等）

### 6.4 浮点 / 时间精度边界场景 — 显式状态短路

详见 §3.1 + §4 改进建议 #7。

---

## 7. 安全评估

| 维度 | 状态 | 备注 |
|---|:-:|---|
| 输入验证 | ✅ | C ABI null check（vx_view_set_pipeline_hooks NULL view → INVALID_STATE）+ hooks struct 字段验证（partial null hooks → only non-null fires）|
| 认证/授权 | N/A | DevTool 不涉及认证场景 |
| 数据保护（加密/脱敏）| N/A | PerfStats 数据非敏感（FPS/ms 数值）|
| 依赖审计 | N/A | 零新依赖（FrameStats 用 `std::chrono::steady_clock` 既有 + HUD JS 复用 inspector_panel.js 范式）|
| 错误信息脱敏 | ✅ | last_attach_error / last_abort_reason 仅含错误类型描述，不含敏感信息 |
| 敏感数据处理 | N/A | |
| **专项 T5（DisplayList overlay 跨帧累积）** | ✅ | B.2.3 OverlayDirtyRect 复用 ResetOverlayCommands 协议（同 hover overlay 共用清理路径）|
| **专项 T6（UpdateManager callback 任意代码执行）** | ✅ | B.1.2 三层 mitigation：单 instance Attach + budget=0 显式短路 + 1ms/frame budget guard 累加 abort |

---

## 8. 反复模式识别（基于 27+ 份历史回顾统计）

| 已知模式 | 出现频率 | 本次是否重复？ | 备注 |
|---|:-:|:-:|---|
| 计划文件清单与实际变更不一致 | 9+ 次 | ❌ | 10/10 子任务 plan §B.x.x 文件清单与实际改动 100% 匹配 |
| 子代理产出需大量返工 | 7+ 次 | ❌ N/A | 零子代理调用 — Level 3 单 agent 直跑高效 |
| 前置依赖/环境/API 能力未验证 | 8+ 次 | ❌ | Phase 0 11 子段实测填写 + grep/ctest 全验证 |
| 非默认路径遗漏验证 | 4+ 次 | ❌ | nullptr hooks / budget=0 / Attach 二次 / dirty rect empty 等边界全覆盖 |
| 测试隔离问题 | 7+ 次 | ❌ | 59 新测零 flaky / 零并行冲突 |
| 提交粒度偏离计划 | 7+ 次 | ❌ | 严格 1 commit/子任务 + 1 finalize commit = 11 commits |
| TDD 严格度与场景不匹配 | 11+ 次 | ❌ | 每子任务 RED → GREEN → REFACTOR + 反向探针精准捕获 |

**0/7 命中** — Phase A reflection 同样 0/7 命中（连续两个任务零反复模式） = **工作流规则 + Phase 0 实测填写 + 5 大范式复用 = 反复模式有效抑制器**。

---

## 9. 与 Phase A reflection 对照（独有段 — Phase B 是 Phase A 的延续）

| 维度 | Phase A | Phase B | 趋势 |
|---|---|---|---|
| 复杂度级别 | Level 4（escalation 中途升级）| Level 3（VAN 锁定不升级）| **更准** |
| 子任务数 | 16 | 10 | **更紧凑** |
| 实测耗时（主线）| 281 min | ~104 min | **-63%** |
| plan ×0.6 比值 | 0.64×（落「大件实现」桶下沿外） | **0.40×（落「极窄档延续高效区」候选新子档）** | **-37%** |
| plan-fact reconcile 处数 | 11 | 1 | **-91%** |
| 反复模式命中 | 0/7 | 0/7 | **保持** |
| commits 数 | 28 | 11 | **更少（无 abort / 无 escalation pause）** |
| 子代理调用 | 0 | 0 | **持平** |
| 安全威胁 mitigation | 4（T1/T2/T3/T4 + A14 守门）| 2（T5 + T6） | **范围窄但深度同**  |
| 5 大可复用架构范式 | **首次沉淀** | **100% 命中且加深** | **设计资产化** |
| Phase 0 子段数 | 9 | 11（+ FetchContent 守卫 + 工具链 grep）| **更细致** |

**核心发现：** Phase B 是 Phase A 的「设计资产应用样本」— Phase A 沉淀的 plan ×0.6 矩阵 / 范式库 / Phase 0 模板 / A14 守门 / dogfood 路径 在 Phase B 全部连续生效，validate「文档质量 → 长期 ROI」工作流定律。

---

## 10. plan ×0.6 第 38 数据点群组入库（候选）

```
| 任务类型 | plan ×0.6 系数 | 来源数据点 | 估时建议 |
|---|---|---|---|
| ...（既有桶）| ... | ... | ... |
| **极窄档延续高效区**（新子档） | **0.30-0.45× ×0.6** | **TASK-20260502-02 Phase B 10 子任务（0.40× 群组）— 第 38-47 数据点** | **触发条件叠加：Phase 0 ≥ 9 子段实测填写 + 5 大可复用架构范式 ≥ 5 项命中 + 零新子系统设计 + plan-fact reconcile ≤ 1 处** |
```

数据点细分（10 子任务）：
- B.0.1 0.46× / B.0.2 0.19× / B.1.1 0.22× / B.1.2 0.56× / B.2.1 0.26× / B.2.2 0.72× / B.2.3 0.39× / B.3.1 1.11× / B.3.2 0.39× / B.3.3 0.28×

**将在 Memory Bank 系统化更新阶段同步入库。**

---

## 11. 后续动作清单（archive 阶段执行）

- [ ] systemPatterns.md：plan ×0.6 第 38 数据点群组入库（10 子任务 0.40× + 「极窄档延续高效区 0.30-0.45×」新子档候选）
- [ ] systemPatterns.md：「Phase 0 投入越深 / build phase 越快」定律 dual-evidence 入库（Phase A + Phase B 双群组）
- [ ] systemPatterns.md：lazy-attach C ABI 容错模式新段
- [ ] systemPatterns.md：「Level 3 vs Level 4 子代理决策法则」段
- [ ] techContext.md：T6 边界场景测设计原则（显式语义状态优于数值阈值依赖）
- [ ] productContext.md：DevTool Phase B Performance Overlay 能力段
- [ ] tasks.md：技术债 #35 阶段 2 P3 候选 / R9 EventManager HitTest P3 候选 / hello_devtool_perf_smoke 多帧 P3 候选
- [ ] activeContext.md 待处理事项：3 项 P3 候选迁移（来源 TASK-20260502-02）

---

**回顾完成。下一步：使用 `/archive` 归档任务。**
