# 归档：TASK-20260502-02 DevTool Phase B — Performance Overlay 实施

**日期：** 2026-05-03
**任务 ID：** TASK-20260502-02
**复杂度级别：** Level 3（中等功能 — 跨 5 子系统 / 10 子任务 / 触及 #35 但非 dogfood 子系统级 / **零 escalation**）
**安全相关：** ✅ 是（T5 + T6 双威胁面全程守门）
**状态：** ✅ 已完成

---

## 1. 任务概述

### 1.1 目标

为 Veloxa 引擎的 DevTool 子系统增加 **Performance Overlay** 能力 —— 实时显示 FPS / 4 阶段耗时 bar（style/layout/render/paint）+ dirty rect 边框可视化 + F11 hotkey toggle，让开发者能在真实运行环境中实时观测渲染管线性能瓶颈。

任务来源：
- 用户 `/van TASK-30-04-B Performance Overlay`（直接显式 Phase B 立项）
- TASK-20260502-01 archive §10 / §6.3 标注「立项条件就绪」
- TASK-20260430-04 蓝图（4.35 h plan ×0.6 基线）+ TASK-20260502-01 archive §10 估时回填校准 ~30%

### 1.2 范围（10 子任务跨 5 子系统）

| Phase | 子任务 | 描述 |
|:-:|:-:|---|
| **B.0 前置改造** | B.0.1 | UpdateManager PipelineHooks 五钩子 + `vx_view_set_pipeline_hooks` C API（**#35 阶段 1 闭环**）|
| | B.0.2 | dirty_rects_ Vector 累积扩展（保持 last_dirty_rect_ 既有契约）|
| **B.1 PerfOverlay 内核** | B.1.1 | PerfOverlay FrameStats ring buffer + 60 帧滑动聚合 + FPS 计算 |
| | B.1.2 | PerfOverlay::Attach + 5 trampolines + T6 1ms/frame budget abort + 单 instance 验证 |
| **B.2 HUD UI** | B.2.1 | HUD overlay HTML/CSS（absolute + opacity + 4 stage bars + R8/R9 mitigation 占位）|
| | B.2.2 | HUD JS updateHud + `vx_view_get_perf_stats` JS native binding |
| | B.2.3 | dirty rect 边框 OverlayDirtyRect 工厂 + InspectorOverlay::InjectDirtyRectHighlights |
| **B.3 集成** | B.3.1 | F11 toggle HUD（事件层 + flag）|
| | B.3.2 | hello_devtool perf smoke（C API ABI 端到端）|
| | B.3.3 | Phase B finalize + A14 smoke 黑名单 +PerfOverlay/InjectDirtyRectHighlights |

### 1.3 关键约束（VAN push-back 5 项）

1. **Level 3 锁定不 escalate**（vs Phase A 中途 escalation 升级）— 实际 Phase B 无 dogfood UI 子系统级风险 + 无 JS native binding 设计 + 无 SDL2 真窗口新例子
2. **#35 阶段 1 闭环 + 阶段 2 留 P3**（蓝图 plan 过于乐观 — 拆 LayoutEngine 内 style/layout 子阶段是独立子系统级改造）
3. **#4 ImageCache 不闭环留 P3**（HUD 不需 icon — 蓝图 plan 过于乐观）
4. **跳过 plan 精化诱惑拒绝**（Level 3 默认进 plan，不能用 VAN 直接进 build）
5. **dogfood R2 三连阻塞 → F8 plan §0.x grep 验证**（HUD CSS R2-verified 子集 + JS panel-side defensive 模式）

---

## 2. 技术方案

### 2.1 架构选型（plan 阶段 B1-B8 决策表 100% 锁定按推荐）

| 决策维度 | 选项 | 理由 |
|---|---|---|
| **B1 plan 文档** | 独立 plan 文档 | Level 3 任务 plan 详细度足够，无需进 creative |
| **B2 子任务并行性** | 严格串行 B.0.1 → B.3.3 | 依赖链清晰（B.0.1 → B.1.1 / B.0.2 → B.2.3 / B.2.x → B.3.x）|
| **B3 测试组织** | 新建 `tests/devtool/overlay/` 子树 | 与 inspector/ 兄弟子目录，命名清晰 |
| **B4 dirty_rects 扩展形式** | 单矩形扩展 dirty_rects_ Vector（不替换 last_dirty_rect_）| 既有契约 100% 不破坏 |
| **B5 dirty rect PaintCommand** | 复用 kOverlayHighlight + 新工厂 OverlayDirtyRect | switch 零改动；T5 ResetOverlayCommands 协议自动覆盖 |
| **B6 commit 颗粒度** | 每子任务 1 commit | 与 Phase A 范式一致 |
| **B7 plan ×0.6 估时基线** | ~3-4 h plan ×0.6（VAN F2 修正后）| reflect 阶段验证落「极窄档延续高效区」候选新子档 0.30-0.45× |
| **B8 spec 复用** | 复用 TASK-30-04 spec | 无新需求 |

### 2.2 核心设计模式（5 大范式 100% 复用 + 1 新范式入库）

| 模式 | Phase A 来源 | Phase B 命中 |
|---|---|---|
| 中央调度协议（UpdateManager）| 已沉淀 | B.0.1 PipelineHooks 是该模式自然扩展 |
| 函数指针 + nullptr 分支预测优化 | 已沉淀 | B.0.1 5 个 hook 默认 nullptr + branch predictor friendly fire |
| 双层 API（C++ 内部 + 公开 C ABI）| 已沉淀 | B.0.1 / B.2.2 跨 4 个 C API 表面 |
| #ifdef + CMake 双层 conditional 子系统 | 已沉淀 | B.0.1-B.3.3 全程守护 + A14 黑名单 +2 项闭环 |
| dogfood 路径 = 探测性 acceptance test | 已沉淀 | B.2.1 HUD CSS 用 R2-verified 子集 |
| **lazy-attach C ABI 容错模式** | **新沉淀** | B.0.1 设计 + B.3.2 端到端验证（INVALID_STATE 提示 + cache hooks + EnsureUpdateManager 激活）|

### 2.3 安全威胁 mitigation（T5 + T6 双覆盖）

**T5（DisplayList overlay 跨帧累积）：**
- B.2.3 OverlayDirtyRect 复用 ResetOverlayCommands 协议（同 hover overlay 共用清理路径，零新 reset 路径）
- 范式：**协议复用 / 不扩增设计表面**

**T6（UpdateManager callback 任意代码执行）：**
- B.1.2 三层 mitigation：
  1. 单 instance Attach 守（第 2 次 Attach 拒绝 + last_attach_error 设值）
  2. budget=0 显式短路（plan-fact 校准 — 避免 sub-µs 硬件精度漏报，详见 §6.2）
  3. 1ms/frame budget guard 累加 abort + abort_count + last_abort_reason
- 每个 mitigation 都有 ≥ 1 单元测覆盖 + 反向探针精准捕获

---

## 3. 实现摘要

### 3.1 文件变更（24 文件 — 4 新建 + 20 修改）

| 操作 | 文件路径 | 说明 |
|:-:|---|---|
| 新建 | `veloxa/devtool/overlay/perf_overlay.h` | PerfOverlay class + FrameStats struct + friend class PerfOverlayTest |
| 新建 | `veloxa/devtool/overlay/perf_overlay.cc` | RecordFrame ring buffer + Attach/Detach + 5 trampolines + T6 budget guard |
| 新建 | `tests/api/perf_hooks_api_test.cc` | 4 测公开 C API（NULL_PARAM / INVALID_STATE / lazy attach / userdata）|
| 新建 | `tests/devtool/overlay/perf_overlay_test.cc` + `perf_overlay_attach_test.cc` + `CMakeLists.txt` | 8 单元测（pure algorithm）+ 12 integration 测（Attach/T6/dirty rect）|
| 修改 | `veloxa/core/update_manager.h` + `.cc` | PipelineHooks struct + Update() 5 注入点 + dirty_rects_ Vector |
| 修改 | `veloxa/core/application.h` + `.cc` | SetPipelineHooks lazy attach + hud_visible_ + F11 hotkey + UnloadDevtoolDocument reset |
| 修改 | `veloxa/api/veloxa_api.h` + `.cc` | VxPipelineHooks struct + 4 C API（set_pipeline_hooks / get_perf_stats / is_hud_visible / VX_KEY_F11）|
| 修改 | `veloxa/platform/sdl2/sdl2_input_translate.cc` | SDLK_F11 → VX_KEY_F11 映射 |
| 修改 | `veloxa/script/dom_bindings.h` + `.cc` | forward decl PerfOverlay 解循环依赖 + SetPerfOverlay setter + VxViewGetPerfStats JS binding |
| 修改 | `veloxa/core/render/paint_command.h` | OverlayDirtyRect 工厂复用 kOverlayHighlight |
| 修改 | `veloxa/devtool/inspector/inspector_overlay.h` + `.cc` | InjectDirtyRectHighlights 静态方法 |
| 修改 | `veloxa/devtool/resources/inspector_panel.html` + `.css` + `.js` | #devtool-hud + 4 stage bars + updateHud() JS |
| 修改 | `veloxa/devtool/CMakeLists.txt` | target_sources 加 overlay/perf_overlay.cc |
| 修改 | `tests/CMakeLists.txt` | perf_hooks_api_test + hello_devtool_perf_smoke ctest |
| 修改 | `tests/core/update_manager_test.cc` | +7 PipelineHooks 测 + 6 dirty_rects 测 |
| 修改 | `tests/api/devtool_attach_api_test.cc` | +5 F11/HUD 测 |
| 修改 | `tests/script/devtool_bindings_test.cc` | +3 vx_view_get_perf_stats 测 |
| 修改 | `tests/devtool/resources/inspector_panel_smoke_test.cc` | +9 HUD smoke 测（HTML + CSS + JS）|
| 修改 | `tests/devtool/inspector/inspector_overlay_test.cc` | +4 InjectDirtyRectHighlights 测 |
| 修改 | `tests/devtool/CMakeLists.txt` | add_subdirectory(overlay) |
| 修改 | `examples/hello_devtool.cc` | PerfHooks 安装段 + lazy-attach 实证 + PERF SMOKE print line |
| 修改 | `tests/smoke/devtool_a14_link_closure.cmake` | 黑名单 +PerfOverlay + InjectDirtyRectHighlights + Phase A/B 区分注释 + NOT in blacklist intentional 段 |

### 3.2 关键决策

1. **lazy-attach C ABI 容错设计（B.0.1）** — `Application::SetPipelineHooks` 在 update_manager_ 未初始化时返 INVALID_STATE 但 cache hooks → EnsureUpdateManager 时 apply。caller 不需要知道 init 顺序。
2. **dirty_rects_ Vector 单矩形扩展（B.0.2 / B4=A）** — 不替换 last_dirty_rect_ 既有契约，`last_dirty_rect_ = dirty_rects_.back()`-equivalent 兜底。既有 2 测全过。
3. **PerfOverlay friend class 引入（B.1.1 / plan §0.5 测试基础设施审计）** — 首次在 veloxa/devtool 子树引入 friend pattern；不用 `#define private public` 越权。
4. **T6 budget=0 显式短路（B.1.2 plan-fact 校准）** — 浮点 / 时间精度边缘场景用「显式语义状态」而非「数值阈值依赖」；详见 §6.2。
5. **R8 fallback `position: absolute`（B.2.1）** — `position: fixed` 不支持时视觉等价 absolute（plan §0.9 grep 实证）；smoke 测断言 css 不应有 position: fixed。
6. **R9 占位 `data-passthrough="1"`（B.2.1）** — `pointer-events: none` 不支持兜底；HitTest 改造留 R3+ EventManager 任务。
7. **OverlayDirtyRect 工厂复用 kOverlayHighlight（B.2.3 / B5=A）** — 不新增 enum；T5 ResetOverlayCommands 协议自动覆盖。
8. **forward declare PerfOverlay 解循环依赖（B.2.2）** — `dom_bindings.h` forward decl + raw pointer + 完整 include 仅在 `.cc` 的 `#ifdef VX_BUILD_DEVTOOL` 块内。
9. **F11 hotkey 范式精确扩展（B.3.1）** — 复用 Phase A F12 hotkey 范式 + UnloadDevtoolDocument 自动 reset hud_visible_=true 防泄漏。
10. **A14 黑名单 +2 项 + Phase 区分注释（B.3.3）** — Phase A → +8 / Phase B → +2 的递增模式 + NOT in blacklist intentional 排除项清单。

### 3.3 安全决策

- **T5（DisplayList overlay 跨帧累积）**：B.2.3 OverlayDirtyRect 复用 ResetOverlayCommands 协议 — 范式复用而非新增 reset 路径
- **T6（UpdateManager callback 任意代码执行）**：B.1.2 三层 mitigation（单 instance + budget=0 显式短路 + 1ms guard）
- **C ABI null check 全覆盖**：vx_view_set_pipeline_hooks NULL view → INVALID_STATE / hooks struct partial null → only non-null fires
- **错误信息脱敏**：last_attach_error / last_abort_reason 仅含错误类型描述，不含敏感信息
- **A14 链接闭包零自动化守门**：黑名单 +2 项闭环（PerfOverlay / InjectDirtyRectHighlights）+ ON+OFF 双绿
- **零新依赖**：FrameStats 用 `std::chrono::steady_clock` 既有；无第三方 perf 库；零 CVE 暴露面新增

---

## 4. 测试覆盖

### 4.1 测试增量统计

| 维度 | Phase B 起点 | Phase B 终点 | 增量 |
|---|:-:|:-:|:-:|
| DEVTOOL=ON ctest 数 | 1169 | **1228** | **+59** |
| DEVTOOL=OFF ctest 数 | 1065 | **1082** | **+17** |
| 新建测试文件数 | — | **3**（perf_hooks_api_test + perf_overlay_test + perf_overlay_attach_test） | — |

### 4.2 测试分布（按子任务）

| 子任务 | 测试增量（ON / OFF）| 测试类型 |
|:---|:-:|:---|
| B.0.1 PipelineHooks 五钩子 + C API | +11 / +11 | UpdateManager 7 测 + perf_hooks_api 4 测 |
| B.0.2 dirty_rects_ Vector | +6 / +6 | UpdateManager 6 测（hover-driven 累积模型）|
| B.1.1 PerfOverlay ring buffer | +8 / 0 | perf_overlay_test 8 测（pure algorithm，VX_BUILD_DEVTOOL guard）|
| B.1.2 PerfOverlay::Attach + T6 budget | +12 / 0 | perf_overlay_attach_test 12 测（integration with UpdateManager + Document + Canvas）|
| B.2.1 HUD HTML/CSS | +7 / 0 | inspector_panel_smoke 7 HUD 测 |
| B.2.2 HUD JS + binding | +5 / 0 | devtool_bindings 3 测 + inspector_panel_smoke 2 JS 测 |
| B.2.3 OverlayDirtyRect + InjectDirtyRectHighlights | +4 / 0 | inspector_overlay_test 4 测 |
| B.3.1 F11 toggle HUD | +5 / 0 | devtool_attach_api_test 5 F11/HUD 测 |
| B.3.2 hello_devtool perf smoke | +1 / 0 | hello_devtool_perf_smoke ctest（PASS_REGULAR_EXPRESSION）|
| B.3.3 finalize + A14 | 0 / 0 | A14 smoke reverify（既有 test #1151 ON + #1082 OFF）|
| **合计** | **+59 / +17** | |

### 4.3 反向探针实证（每子任务 ≥ 1 探针精准捕获）

| 子任务 | 反向探针 | 期望 FAIL 测 |
|:---|:---|:---|
| B.0.1 | 注释 OnAfterLayout fire 行 | AllFiveHooksFireInOrderOnUpdate + PartialNullHooksOnlyNonNullCallbackFires |
| B.0.2 | 注释 push_back | FirstUpdateAccumulatesOneDirtyRect |
| B.1.1 | 去 modulo（`head_ + 1` 不取模）| RingBufferOverwritesOldestFrameAfter60 |
| B.1.2 | 跳 single-instance check | AttachTwiceRejectsSecond |
| B.2.1 | rename `id="devtool-hud"` → `devtool-hud-PROBE` | HtmlContainsDevtoolHudId |
| B.2.2 | fps 设 0 | LiveStats 测 |
| B.2.3 | 跳 IsEmpty filter | SkipsEmpty 测 |
| B.3.1 | 删 F11 toggle 路径 | F11HotkeyTogglesHudVisibleWhenEnabled |
| B.3.2 | 零（smoke 测 print line 存在性显然 FAIL）| — |
| B.3.3 | 零（A14 黑名单 reverify）| — |

### 4.4 A14 链接闭包守门（DevTool=ON / OFF 双绿）

```
ON  test #1151: devtool_a14_link_closure_smoke ........ Passed
OFF test #1082: devtool_a14_link_closure_smoke ........ Passed

OFF nm verification: PerfOverlay + InjectDirtyRectHighlights NOT FOUND
in libvx_api.a + libvx_core.a (link closure verified zero DevTool symbols)
```

---

## 5. 关键决策（汇总）

10 项决策已在 §3.2 详细列出。架构层面 3 项最重要：

1. **lazy-attach C ABI 容错模式**（新沉淀范式）— 内部承担「记忆 + 自动激活」复杂度，不要求 caller 知道 init 顺序
2. **范式复用而非新增设计表面**（T5 ResetOverlayCommands / B5=A 不新增 enum）— 设计表面收敛是长期可维护性的核心
3. **「Phase A 沉淀的 5 大范式」100% 命中且加深**（设计资产化）— 验证文档质量 → 长期 ROI 工作流定律

---

## 6. 经验教训

### 6.1 「Phase 0 投入越深 / build phase 越快」定律 dual-evidence 入库

Phase A 首次发现：30 min Phase 0 投入 → 节省 160 min build phase = **5.3× ROI**
Phase B 第二次实证：30 min Phase 0 投入 → 节省 157 min build phase = **5.2× ROI**

**结论：** ROI ≈ 5× 是稳定数据点（dual-evidence），可作为「Phase 0 投入合理性」的硬指标。已入库 `systemPatterns.md`。

### 6.2 浮点 / 时间精度边界场景必须用「显式语义状态」

B.1.2 budget=0 测原实施依赖 `frame_total_us_ > budget_us_` 数值比较 → 在 sub-µs 硬件计时器精度下，capture-less callback 实际耗时 < 1 µs → cast 后归零 → `0 > 0` = false → 测 FAIL（false negative）。

修正：`if (budget_us_ == 0 || frame_total_us_ > budget_us_)` — 显式短路 disabled 语义。

**经验：** T6 / 性能 / 边界场景测时，如设计意图含「特殊值 = 特殊语义」（e.g. 0 = disabled），必须显式短路语义状态，不能依赖数值比较结果。已入库 `techContext.md`。

### 6.3 plan-fact reconcile 11→1 处验证「双重质量保护」范式

蓝图 plan 复用（TASK-30-04 §Phase B 段 10 子任务粗模板）+ Phase 0 11 子段实测填写 = **plan-fact reconcile 从 Phase A 的 11 处下降到 Phase B 的 1 处（-91%）**。

**操作建议：**
- 蓝图任务（V2=a）的 plan 应作为下游实施任务的「输入资产」
- 实施任务 plan 阶段必须做「蓝图 → 实测细化」转换（不能照搬，必须 grep 验证 + 当前 main 终态对照）
- TASK-20260502-02 plan §0 的 11 子段是该转换的标准模板

### 6.4 Level 3 vs Level 4 的子代理决策法则

实测数据（Phase A Level 4 / Phase B Level 3 双任务零子代理高效完成）支持以下法则：

- Level 3 ≤ 10 子任务 + 5 大范式 ≥ 5 项命中 → **单 agent 直跑（推荐）**
- Level 4 ≥ 12 子任务 + 涉及子系统级新设计 → 子代理可候选评估
- 临界点：N ≥ 12 子任务时子代理调度成本可能 < 单 agent 上下文管理成本

已入库 `systemPatterns.md`。

### 6.5 lazy-attach C ABI 容错模式 — 内部复杂度优于外部约束

`Application::SetPipelineHooks` lazy-attach 设计是 C ABI 「容错优于强约束」原则的优秀实践 — INVALID_STATE 是「提示」而非「失败」，hooks 已 cached，caller 可忽略也可重试。

已入库 `systemPatterns.md`。

---

## 7. 改进建议落实状态

reflection 列出 11 项建议（0 P0 + 3 P1 + 8 P2）；归档前落实情况：

| # | 建议 | 优先级 | 落实状态 |
|:-:|---|:-:|:-:|
| 1 | plan ×0.6 第 38-47 数据点入库 + 「极窄档延续高效区 0.30-0.45×」新子档 | P1 | ✅ systemPatterns.md 已更新 |
| 2 | 「Phase 0 投入越深 / build phase 越快」定律 dual-evidence 入库 | P1 | ✅ systemPatterns.md 新增段 |
| 3 | lazy-attach C ABI 容错模式入库 | P1 | ✅ systemPatterns.md 新增段 |
| 4 | A14 黑名单维护演进透明度子段 | P2 | ✅ systemPatterns.md 已扩展 |
| 5 | 5 大可复用架构范式连续生效证明（Phase A → Phase B 都 100% 命中）| P2 | ✅ archive 本文档 §2.2 + §6.3 已记录 dual-evidence |
| 6 | Level 3 vs Level 4 子代理决策法则入库 | P2 | ✅ systemPatterns.md 新增段 |
| 7 | T6 边界场景测设计原则（显式语义状态优于数值阈值）| P2 | ✅ techContext.md 新增段 |
| 8 | 蓝图 plan + Phase 0 实测细化双重质量保护范式 | P2 | ✅ archive 本文档 §6.3 已记录 dual-evidence；reflection 已写入 §4 经验教训 |
| 9 | #35 阶段 2 P3 候选记录 | P2 | ✅ activeContext.md 待处理事项已迁移 |
| 10 | R9 EventManager HitTest 改造 P3 候选 | P2 | ✅ activeContext.md 待处理事项已迁移 |
| 11 | hello_devtool_perf_smoke 多帧 P3 候选 | P2 | ✅ activeContext.md 待处理事项已迁移 |

**P0：0 项（创纪录 — build 全按 plan 执行无重大偏差）**
**P1：3/3 全部落实 ✅**
**P2：8/8 全部落实 ✅（含 archive 本文档作为 P2 #5 + #8 的 dual-evidence 载体）**

---

## 8. 架构影响

### 8.1 新增公开 ABI 表面

- 4 公开 C API：`vx_view_set_pipeline_hooks` / `vx_view_get_perf_stats`（JS native binding 形式）/ `vx_view_is_hud_visible` / `VX_KEY_F11` 宏
- 1 ABI struct：`VxPipelineHooks`（5 callback 字段 + userdata）
- 1 typedef：`VxPipelineCallback`

**ABI 兼容性：** 所有新 API 都是新增（非修改），既有 C ABI 100% 兼容。

### 8.2 新增子系统

- `vx::devtool::overlay::PerfOverlay` 子系统（独立 namespace，与 `vx::devtool::InspectorOverlay` 兄弟）
- 内部包含：`FrameStats` struct + `PerfOverlay` class（kCapacity=60 ring buffer + Attach/Detach + 5 trampolines + T6 budget guard）

### 8.3 新增 UpdateManager 扩展点

- `PipelineHooks` struct（5 callback + userdata）
- `UpdateManager::SetPipelineHooks(const PipelineHooks*)` + `pipeline_hooks()` getter
- 5 注入点：OnFrameStart / OnAfterStyle / OnAfterLayout / OnAfterRender / OnFrameEnd
- `dirty_rects_` Vector + `dirty_rects()` getter + `ClearDirtyRects()`

**性能影响：** 函数指针 + nullptr branch predictor 优化（5 个 hook 默认 nullptr）— 实测 NullHooksUpdateRemainsLossless 测过，对未挂 hook 的 caller 零开销。

### 8.4 长期维护建议

- **#35 阶段 2 留 P3**：拆 LayoutEngine 内 style/layout 子阶段，让 OnAfterStyle 与 OnAfterLayout 真实分离时间差（当前同点触发是妥协）
- **R9 EventManager HitTest 改造留 R3+**：HUD pointer-events 真支持（当前 data-passthrough="1" 占位）
- **Phase C Hot Reload 估时下调**：受益于 Phase A + Phase B 5 大范式 + 5 大 ROI 复用，估时进一步下调 ~20% 至 ~2-3 h plan ×0.6

---

## 9. 参考文档

- 设计规格：`docs/specs/2026-04-30-devtool-design.md`（A6-A9 + A13-A14 + T5/T6 + I2/I3/I7 + R3 — 复用，无修改）
- 实现计划：`docs/plans/2026-05-02-devtool-perf-overlay.md`（独立 plan ~600 行 + 10 子任务 5-步 TDD 模板 + 完整代码片段 + 4 R 风险登记 + 3 Checkpoint + 9 条 systemPatterns 协同度自我对照）
- 创意设计：`memory-bank/creative/creative-devtool-screen-layout.md`（决策 3 HUD overlay + 决策 5 F11 toggle — 复用，无修改）
- 蓝图 plan：`docs/plans/2026-04-30-devtool.md` §Phase B 段（10 子任务粗模板 — 作为输入资产）
- 回顾文档：`memory-bank/reflection/reflection-TASK-20260502-02.md`（Level 3 详细 11 段 ~370 行）
- 上一任务归档：`memory-bank/archive/archive-TASK-20260502-01.md`（Phase A 主线归档 — 5 大可复用架构范式来源 + Phase 0 投入定律首次发现）

---

## 10. commit 链（11 commits — 巨型简洁 / 严格 1 commit/子任务）

```
3f4f266 chore(build): finalize TASK-20260502-02 Phase B 10/10 complete
9dc0a50 feat(devtool): TASK-20260502-02 B.3.2 hello_devtool perf smoke
03ec274 feat(devtool): F11 toggle HUD 可见性 (B.3.1)
55a8c68 feat(devtool): OverlayDirtyRect 工厂 + InjectDirtyRectHighlights (B.2.3)
647df3b feat(devtool): HUD JS updateHud + vx_view_get_perf_stats native binding (B.2.2)
5be448d feat(devtool): HUD overlay HTML/CSS — absolute + opacity (B.2.1)
b0a87ea feat(devtool): PerfOverlay::Attach + T6 budget abort + 单 instance + 自动填 FrameStats (B.1.2)
df2c050 feat(devtool): PerfOverlay FrameStats ring buffer + 60 帧聚合 + FPS (B.1.1)
e3189ec feat(core): dirty_rects_ Vector 累积扩展 (B.0.2)
28811f3 feat(core): UpdateManager PipelineHooks 五钩子 + vx_view_set_pipeline_hooks C API (B.0.1)
a87795a docs(plan): TASK-20260502-02 Phase B Performance Overlay 实施 plan 落盘
70cf724 docs(reflect): add reflection for TASK-20260502-02
```

后续将追加：
- archive commit：`docs(archive): add archive for TASK-20260502-02`
- 收尾 commit：`chore(workflow): complete TASK-20260502-02 and reset to idle`

---

## 11. 后续任务依赖估时调整

基于 Phase B 实测数据（0.40× 极窄档延续高效区）+ 5 大范式 100% 复用，对蓝图任务 V2=a 候选下游任务的估时调整：

| 任务 | 蓝图估时（plan ×0.6）| Phase A archive 校准 | Phase B archive 进一步校准 |
|---|---|---|---|
| TASK-30-04-B Performance Overlay | 4.35 h | ~3.5-5 h（-30%）| **✅ 已完成 ~1.7 h（实际 0.40× — 比再校准还低）** |
| TASK-30-04-C Hot Reload | 3.5 h | ~2.5-4 h（-20%）| **~2-3 h（-30% 进一步下调）** — 受益于 Phase B 沉淀的 lazy-attach 模式 + dogfood path 复用 |
| #35 阶段 2 拆 LayoutEngine | 未估 | — | **~2-3 h plan ×0.6 推荐 Level 3** — 涉及 LayoutEngine 子系统局部改造 + 既有契约维护 |
| R9 EventManager HitTest | 未估 | — | **~1.5-2 h plan ×0.6 推荐 Level 2-3** — pointer-events 真支持改造，HitTest 跳过 data-passthrough 元素 |
| DomBindings R2 P3 三连 | 未估 | ~3-5 h | **持平** — 与 Phase B 范式重叠少，按原估 |

---

**归档完成。任务 ✅ 已完成，Memory Bank 准备进入空闲状态接受新任务。**
