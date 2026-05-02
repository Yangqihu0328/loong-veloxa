# 进度记录

## 当前任务

### TASK-20260502-02：DevTool Phase B — Performance Overlay 实施（FPS / 4 阶段 bars / dirty rect 边框高亮）[安全相关]

#### `/van` 阶段产出快照（2026-05-02 ~21:35，~10 min）

- **任务来源：** 用户 `/van TASK-30-04-B Performance Overlay`（直接显式 Phase B 立项；TASK-20260502-01 archive §10 / §6.3 标注「立项条件就绪」）
- **复杂度：** Level 3（中等功能 — 跨 5 子系统 core/devtool-overlay/devtool-inspector_panel-extension/api/tests，蓝图 plan §Phase B 10 子任务，无新架构决策）
- **分支：** `feature/TASK-20260502-02-devtool-perf-overlay` 基于 main `8b2ead4`（已含 TASK-20260502-01 Phase A 全归档 + 5 大可复用架构范式）
- **环境检测：** Linux 6.6 / WSL2 + CMake 3.20 + clang/gcc + git 工作区干净 ✅
- **F1-F9 grep 实证（关键核心）：** 5 ✅ 已就绪 / 1 ⚠️ 部分 / 1 🔴 需新建 — **比 Phase A 启动时 5 ✅ / 2 ⚠️ / 2 🔴 更优**
  - F1 UpdateManager 五钩子：🔴 100% 未存在（#35 完全未闭环 → B.0.1 必做）
  - F2 dirty rect collector：✅ **已就绪 — 比 plan 预期高 ROI**（`Renderer::ComputeDirtyRect` + `UpdateManager::last_dirty_rect()` 已可用）
  - F3 mutable_display_list getter：✅ TASK-20260502-01 A.1.6 已暴露
  - F4 PaintCommand kOverlayHighlight + ResetOverlayCommands：✅ TASK-20260502-01 A.0.4 + A.1.1 完整可复用
  - F5 SteadyTimePoint / SteadyClock 时间抽象：✅ transition.h 已导出 `vx::css::SteadyTimePoint = std::chrono::steady_clock::time_point`
  - F6 ImageCache namespace 隔离（#4）：⚠️ **HUD 不需 icon → 不闭环留 P3**（蓝图 plan 过于乐观）
  - F7 Application::OnFrame() 单一函数：✅ 设计选择清晰（PipelineHooks 在 UpdateManager::Update() 内拆五钩子）
  - F8 HUD overlay 设计（DevTool Document 内 absolute positioned div + CSS opacity）：✅ creative #1 决策 3 锁定
  - F9 F11 toggle HUD 协议：✅ creative #1 决策 5 锁定 + Phase A F12 hotkey 范式可直接扩展
- **触及技术债 1 项一次性闭环 + 1 项不闭环留 P3：** ✅ #35 → B.0.1 / ⊘ #4（HUD 不需 icon）
- **Phase B 子任务清单（蓝图 plan 已就绪，待 plan 阶段精化）：** 10 个 — B.0 前置改造 2（五钩子 + dirty rect 扩展）+ B.1 PerfOverlay 内核 2（ring buffer + T6 budget）+ B.2 HUD UI 3（HTML/CSS + JS + dirty rect 渲染）+ B.3 集成 3（F11 toggle + example smoke + reflect prep）；估时 ~261 min plan ×0.6（4.35 h，蓝图基线）
- **创意决策：** ❌ 无新 creative 需求（creative #1 决策 3/5 已覆盖；纯算法层面 — FrameStats ring buffer + 滑动 60 帧聚合 + 1ms/frame budget abort 是常规嵌入式范式无新架构空白）
- **安全相关：** ✅ 是（T6 UpdateManager 钩子 callback 任意代码执行 — 1ms/frame budget abort + 单 instance 验证；T5 DisplayList overlay 跨帧累积 — dirty rect 边框高亮复用 ResetOverlayCommands 协议）
- **前置验证 6/6 全 PASS：** 依赖 / 环境 / artifact / ctest 1169 ON / 1065 OFF baseline 隐式继承 / FetchContent 跳过 / 待处理事项关联（极强 — 闭环 #35 + 与 5 大可复用架构范式协同 + DomBindings R2 P3 三连无相互依赖）
- **VAN push-back 6 项风险闭环：** R3 五钩子重构性能 → function pointer + nullptr branch predictor + nullptr 路径单测 / Phase B 巨大规模陷阱 → Level 3 锁定不 escalate（无 dogfood / 无 JS native binding 设计 / 无 SDL2 真窗口新例子）/ #4 蓝图过于乐观 → 不闭环留 P3 / 跳过 plan 精化诱惑 → 拒绝（Level 3 默认进 plan）/ 蓝图 plan 全照搬陷阱 → plan 阶段必须验证 main 终态后续大变 + F2 已就绪发现 + plan ×0.6 第 38 数据点 / dogfood R2 三连阻塞 → F8 plan §0.x grep 验证 `position: fixed` / `opacity` + HUD JS 复用 panel-side defensive 模式
- **估时假设（plan ×0.6 第 38 数据点候选）：** 蓝图 4.35 h plan ×0.6 → archive §10 估时回填校准下调 ~30% → ~3.5-5 h；本次 VAN F2 dirty rect 已就绪进一步发现 → **plan 假设 ~3-4 h plan ×0.6**（reflect 阶段验证落「大件实现」桶 0.6-1.1× vs「极窄档延续」可能）
- **MB 三件套同步：** tasks.md（新任务结构 + 任务历史 +1 行 + 上一任务折叠）/ activeContext.md（阶段 → 初始化）/ progress.md（本快照）

#### `/plan` 阶段产出快照（2026-05-02 ~22:10，~30 min）

- **Phase 0 11 子段实测填写完成：**
  - §0.1 ctest baseline 二次验证 ✅ DEVTOOL=ON 1169/1169 PASS（5.75s）/ DEVTOOL=OFF 1065/1065 PASS（4.23s）— 与 main `8b2ead4` 终态一致
  - §0.2 CMake 链接拓扑 ✅（vx_devtool 静态库 += `overlay/perf_overlay.cc` append 到既有 inspector/CMakeLists.txt 范式 / vx_core += UpdateManager + Renderer 改造 / vx_api += 2 公开 C API stub / `tests/devtool/overlay/` 兄弟子树）
  - §0.3 静态库循环依赖审计 ✅ 无新循环（vx_devtool → vx_core 既有 B 链 A 模式持续 — 5 新符号清单 + 归属 + 调用方表）
  - §0.4 FetchContent 守卫 ✅ 跳过（零新依赖）
  - §0.5 测试基础设施审计 ✅（B.0.2 Vector dirty_rects PUBLIC 自然语义 / B.1.1 PerfOverlay friend pattern 引入 — veloxa/devtool 既有 0 friend 命中 / B.1.2 PUBLIC abort_count last_*_reason 自然语义）
  - §0.6 边界输入清单 16 条（每条对应 ≥ 1 测）
  - §0.7 调用链端到端验证 ✅ 五钩子 mapping 5 注入点 + FrameStats 5 字段公式表（style_ms = AfterStyle - FrameStart / layout_ms = AfterLayout - AfterStyle / render_ms = AfterRender - AfterLayout / paint_ms = FrameEnd - AfterRender / total_ms = FrameEnd - FrameStart）+ 标注 OnAfterStyle 与 OnAfterLayout 同点触发的 LayoutEngine 含 style 妥协（#35 阶段 2 P3 候选）
  - §0.8 既有测试隐式契约 fingerprint ✅（5 子系统命中表）
  - §0.9 CSS shorthand 能力 grep 表 ✅（13 条 CSS 属性 R2-verified 状态表 — `opacity` / `position: absolute` / `border-radius` 等 ✅ / `position: fixed` ⚠️ fallback 为 absolute 视觉等价 / `pointer-events: none` ❌ 用 `data-passthrough="1"` 兜底 / `z-index` ❌ 不需要 — HUD 自然在最末渲染）
  - §0.10 工具链与子系统关闭守门 ✅
  - §0.11 工具链 grep 命中 ✅（`rg` 不可用 → 系统 `grep -rE` + Grep tool；jq 缺失 → python3 兜底）

- **B1-B8 build 级精化决策表全部锁定（用户 1 次 AskQuestion 选 all_recommended → 8/8 按推荐）：**
  - B1=A 独立 plan 文档 / B2=A 严格串行 B.0.1 → B.3.3 / B3=A 新建 `tests/devtool/overlay/` 子树
  - B4=A 单矩形扩展 dirty_rects_ Vector 累积（不替换 last_dirty_rect_ 既有契约）
  - B5=A 复用 kOverlayHighlight + 新工厂 OverlayDirtyRect（不新增 enum，paint_command.h / renderer.cc switch 零改动）
  - B6=A 每子任务 1 commit / B7=A 假设 ~3-4 h plan ×0.6 / B8=A 复用 spec

- **plan 落盘：** `docs/plans/2026-05-02-devtool-perf-overlay.md`（~600 行，10 子任务 5-步 TDD 模板 + 完整代码片段 + 4 个 R 风险登记 R3/R7/R8/R9 + 3 Checkpoint + 9 条 systemPatterns 协同度自我对照）

- **10 子任务总估时矩阵：**
  - B.0.1 PipelineHooks 五钩子 + C API: 90 min ×0.6 = 54 min（最大子任务）
  - B.0.2 dirty_rects_ Vector 扩展: 60 min ×0.6 = 36 min
  - B.1.1 PerfOverlay ring buffer + 60 帧聚合: 60 min ×0.6 = 36 min
  - B.1.2 T6 budget abort + 单 instance: 45 min ×0.6 = 27 min
  - B.2.1 HUD HTML/CSS（absolute + opacity）: 45 min ×0.6 = 27 min
  - B.2.2 HUD JS + vx_view_get_perf_stats binding: 30 min ×0.6 = 18 min
  - B.2.3 dirty rect 边框 OverlayDirtyRect 工厂: 30 min ×0.6 = 18 min
  - B.3.1 F11 toggle HUD: 15 min ×0.6 = 9 min
  - B.3.2 hello_devtool perf smoke: 30 min ×0.6 = 18 min
  - B.3.3 Phase B finalize + A14 smoke 更新: 30 min ×0.6 = 18 min
  - **合计：435 min（7.25 h）→ ×0.6 = 261 min（4.35 h，蓝图基线）→ 最终预测 ~3-4 h（0.55-0.75× plan ×0.6）**

- **plan ×0.6 第 38 数据点假设入库：** 蓝图 4.35 h → archive 校准 ~3.5-5 h → VAN F2 发现 ~3-4 h →最终预测 **~3-4 h plan ×0.6（0.55-0.75× 比值）**；reflect 阶段验证落「大件实现」桶 0.6-1.1× 中段 vs「极窄档延续」可能

- **R3-R9 风险登记完整：** R3 UpdateManager 五钩子重构性能 mitigation（function pointer + nullptr branch predictor + nullptr 路径单测）/ R7 dirty_rects Vector 累积无 cap（PerfOverlay::Attach OnFrameStart 调 ClearDirtyRects 每帧清空）/ R8 HUD `position: fixed` fallback 视觉差异（fallback 为 absolute 视觉等价）/ R9 `pointer-events: none` 不支持（data-passthrough 兜底，HUD 区域 ~180×90 px 影响有限）

- **MB 三件套同步：** tasks.md（plan 完成段更新 + 实现 plan 路径标记）/ activeContext.md（阶段 → 规划中 + 11 子段实测 + B1-B8 决策 + 估时假设）/ progress.md（本快照）

- **下一步：** `/build` 启动 B.0.1 PipelineHooks 五钩子（最大子任务 plan 90 min ×0.6 = 54 min）

#### `/build` Phase B.0.1 完成快照（2026-05-02 ~22:35，~25 min 实测）

- **任务：** UpdateManager PipelineHooks 五钩子 + vx_view_set_pipeline_hooks C API（**技术债 #35 阶段 1 闭环 ✅**）
- **plan ×0.6 估时：** 54 min / **实测：** ~25 min（**0.46× plan ×0.6**，落「极窄档延续」桶 — 子系统位点已 grep 实证 + 测试 fixture 复用 + 反向探针清晰）
- **改动文件：**
  - `veloxa/core/update_manager.h`：新 `PipelineHooks` struct（5 callback + userdata）+ `SetPipelineHooks` / `pipeline_hooks()` getter + `pipeline_hooks_` 私有字段
  - `veloxa/core/update_manager.cc`：`Update()` 内 5 注入点 fire（OnFrameStart entry / OnAfterStyle DetectAndApplyTransitions 后 / OnAfterLayout 同点 / OnAfterRender Record 后 / OnFrameEnd 末尾）
  - `veloxa/core/application.h`：新 `SetPipelineHooks(const PipelineHooks*)` + `pipeline_hooks()` + `external_hooks_` + `external_hooks_set_` 字段（lazy attach 设计 — caller 不需要先 LoadHTML）
  - `veloxa/core/application.cc`：`EnsureUpdateManager` lazy attach + `Application::SetPipelineHooks` 实施
  - `veloxa/api/veloxa_api.h`：`VxPipelineCallback` typedef + `VxPipelineHooks` struct + `vx_view_set_pipeline_hooks` 声明
  - `veloxa/api/veloxa_api.cc`：`vx_view_set_pipeline_hooks` 实施（C ABI 公开 — DEVTOOL=ON / OFF 都可用，profiler / tracing 用例支持）
  - `tests/core/update_manager_test.cc`：+7 PipelineHooks 测（all-null / SetStoresPointer / AllFiveFireInOrder / NullLossless / NotFiredNoNeed / PartialNullSelective / UserdataPassed）
  - `tests/api/perf_hooks_api_test.cc`：新建 4 PerfHooksApi 测（NullView / FreshViewInvalidState / LazyAttach / UserdataPassed）
  - `tests/CMakeLists.txt`：`vx_add_test(perf_hooks_api_test ...)` 注册

- **测试增量：** **DEVTOOL=ON 1169 → 1180 PASS（+11）/ DEVTOOL=OFF 1065 → 1076 PASS（+11）**
- **A14 守门：** ✅ ctest `devtool_a14_link_closure_smoke` PASS（黑名单 8 符号 0 命中 in OFF build）；libvx_api.a OFF + ~700 bytes（C ABI 公开 stub 表面）/ libvx_core.a OFF + ~10 KB（PipelineHooks struct + UpdateManager + Application 内部公开扩展，零 vx_devtool 内部符号泄漏）
- **反向探针：** ✅ 注释 OnAfterLayout fire 行 → `AllFiveHooksFireInOrderOnUpdate` 精准 FAIL（order 缺 "layout"）+ `PartialNullHooksOnlyNonNullCallbackFires` 精准 FAIL（layout calls = 0）；恢复后双绿
- **Lessons learned：**
  - lambda 捕获不能赋给 C function pointer → 改用 static recorder pattern（`g_pipeline_hook_order` + 5 free functions）— 与 plan §B.0.1 RED 测设计修正
  - lazy attach 设计（Application 持 PipelineHooks 拷贝 + EnsureUpdateManager 时 attach）让 caller 不需要预先 LoadHTML，**但必须返 INVALID_STATE 提示 caller 还没附加** — perf_hooks_api_test `ClearHooksOnFreshViewReturnsInvalidState` + `AfterUpdateLazyAttachInstallsHooks` 双测覆盖
  - C ABI VxPipelineHooks struct 设 `userdata` 在 `vx_view_set_pipeline_hooks` 函数参数（vs C++ PipelineHooks 内字段）— 公开 ABI 简化设计
- **commit：** `28811f3 feat(core): UpdateManager PipelineHooks 五钩子 + vx_view_set_pipeline_hooks C API (B.0.1)`

#### `/build` Phase B.0.2 完成快照（2026-05-02 ~22:42，~7 min 实测）

- **任务：** dirty_rects_ Vector 累积扩展（plan §B.0.2 RED 测假设按 update_manager_test:131 现实校准 — "size=3 假设" → "hover change 累积模型"）
- **plan ×0.6 估时：** 36 min / **实测：** ~7 min（**0.19× plan ×0.6**，落「极窄档延续」桶最低端 — Vector + push_back + clear API 极简，单点修改）
- **改动文件：**
  - `veloxa/core/update_manager.h`：新 `dirty_rects()` getter + `ClearDirtyRects()` 方法 + `dirty_rects_` 私有字段
  - `veloxa/core/update_manager.cc`：在 `last_dirty_rect_ = ComputeDirtyRect(...)` 后追加 `if (!last_dirty_rect_.IsEmpty()) dirty_rects_.push_back(last_dirty_rect_)`
  - `tests/core/update_manager_test.cc`：+6 dirty_rects 测（EmptyBeforeUpdate / FirstUpdateAccumulates / ReInvalidateNoChange / ClearResets / HoverChangeAccumulates / LastDirtyContractCompat）

- **测试增量：** **DEVTOOL=ON 1180 → 1186 PASS（+6）/ DEVTOOL=OFF 1076 → 1082 PASS（+6）**
- **A14 守门：** ✅ 隐式继承（dirty_rects_ 是 vx_core 内部字段，零 vx_devtool 泄漏）
- **反向探针：** ✅ 注释 push_back → `FirstUpdateAccumulatesOneDirtyRect` 精准 FAIL（size = 0 vs 1）+ vx::Vector `back()` on empty CHECK abort 兜底
- **plan-fact 校准：** plan §B.0.2 「同帧 3 次 invalidate 累积 size=3」假设错误（既有 `DirtyRectComputedOnUpdate` 测显示第 2 次无变化时 last_dirty_rect 返 empty）→ 实际累积模型按「visual change 才 push 非空」语义，与 last_dirty_rect_ 既有 empty 语义一致；测改为 hover-driven 累积场景
- **commit：** 待 B.0.2 commit
- **下一步：** `/plan` 进入 build 级精化（Phase 0.1 reconfigure ctest baseline 二次验证 + Phase 0 grep callsite + B1-B8 决策表 + 10 子任务 5-步 TDD 模板 + plan ×0.6 第 38 数据点假设入库 + R2-verified CSS `position: fixed` / `opacity` grep 验证）
- **实测耗时：** ~10 min（环境检测 + grep 实证 F1-F9 + 蓝图 plan §Phase B 阅读 + creative #1 决策 3/5 复用确认 + MB 同步 + 分支创建）

---

## 上次任务（已归档闭环）

### TASK-20260502-01：DevTool Phase A — Inspector 实施 [安全相关] — ✅ 已归档（2026-05-02 ~18:10）

- **复杂度：** Level 4（plan escalation 自 Level 3 升级 — Phase A.1 dogfood UI 实质子系统级 + 8 子任务；Phase A 总 16/16 子任务跨 4 Phase / 8 轮 build）
- **归档文档：** `memory-bank/archive/archive-TASK-20260502-01.md`（10 段 Level 4 全面归档 / ~340 行）
- **回顾文档：** `memory-bank/reflection/reflection-TASK-20260502-01.md`（13 段全维度 / ~880 行）
- **任务时长：** ~338 min（VAN ~10 min + Plan ~22 min + Build ~281 min + Reflect ~25 min；不含 archive）
- **最终 ctest：** DEVTOOL=ON 1062 → **1169 PASS（+107 测）**；DEVTOOL=OFF 1062 → **1065 PASS（+3 测）**
- **A14 链接闭包零自动化守门：** `tests/smoke/devtool_a14_link_closure.cmake` 每次 ctest 自动 nm 验证 8 符号黑名单
- **核心成果：** 16/16 子任务（28 commits / 4 安全威胁 mitigation 全到位 / 2 历史技术债 #26 + #40 闭环 / 3 R2 P3 候选沉淀 / 7 公开 C API + JS native binding + Hello example + dogfood 完整链路打通）

#### Phase A 总览（16 子任务，详细见 archive 文档 §2.2）

| Phase | 子任务数 | 实测耗时 | plan ×0.6 估时 | 比值 |
|:--|:--:|--:|--:|:--:|
| A.0 前置改造 | 6 | ~95 min | 189 min | **0.50×** |
| A.1 dogfood UI（escalation 后 8 子任务）| 8 | ~143 min | 144 min | **0.99×** |
| A.2 + A.3 安全测 + example | 6 | ~43 min | 108 min | **0.40×** |
| **合计** | **16** | **~281 min（4.7 h）** | **441 min（7.35 h）** | **0.64×** |

注 A.1 实测 0.99× 是因 escalation 后 plan 估时已比初始更准（验证 escalation 投入 ~30 min plan 时间换 5 轮 build 实测高度匹配的高 ROI）。

#### 28 commits 演进时间线（详细见 archive §2.3）

`e43a5be (plan)` → A.0.1-A.0.6（6 commits + round 1/2 pause）→ `c3cab4e/e2ff26a (round 3 abort + plan escalation)` → A.1.1-A.1.8（8 commits + round 3-7 pause）→ A.2.1-A.2.4 + A.3.1-A.3.2（5 commits）→ `57d46b6 (reflect)` → archive commit。

#### Phase A 总教训沉淀（4 大类，全跨子任务复用 — 详细见 archive §5 + reflection §5）

1. **plan-fact reconcile 是 Level 4 大件任务的常态**（共 11 处修正）— plan 完美率不可期待，build phase 持续 grep + 实证才是质量保证
2. **多层 A14 zero-byte guard 范式锁定**（共 5 子任务复用）— `.cc 文件 #ifdef block + CMake if(VX_BUILD_DEVTOOL) 条件 link` 双层保护 + ctest cmake script 自动化 nm 验证
3. **C ABI stub 公开表面 vs DevTool 闭包精确区分**（共 3 处出现）— A14 spec 「链接闭合零」严格条件是「子目录不参与 link」（用 nm 验证），不是「字节零增长」
4. **dogfood = R2 缺陷暴露清单产出**（A.1.8 集中产出 3 个 P3 候选）— DomBindings 缺 `Element.children` / `addEventListener` / `innerHTML setter` 三连；处理策略 = panel-side defensive try/catch

#### 长期知识库反馈（已生效）

- `memory-bank/systemPatterns.md`: 3 新段（plan escalation 中途触发 + 反向探针有效性陷阱清单 + 子系统关闭守门 ctest smoke 范式）+ plan ×0.6 矩阵第 18-37 数据点入库 + 「大件实现」桶系数下调 0.8-1.2× → 0.6-1.1× + 3 子桶
- `memory-bank/techContext.md`: TASK-20260502-01 DevTool Phase A 实施摘要段 + Status / StatusOr 使用规范段
- `memory-bank/productContext.md`: § 最近能力更新（2026-05-02）段

#### 改进建议落实状态（详细见 archive §6）

- **P0 立即（3 项）：** 全部沉淀到 systemPatterns（Level 升级触发条件 + plan 假设 grep 验证 + 反向探针陷阱清单）✅
- **P1 下次（5 项）：** A14 smoke template ✅ + dogfood SOP ✅ + 余 3 项已迁移到 `activeContext.md` 待处理事项（git-workflow 多 commit 拆分 / StatusOr codebase audit / spec A14 解读附录）
- **P2 长期（2 项）：** escalation 估时校准价值入 systemPatterns ✅ + DomBindings R2 三连 P3 立项预案 ✅（已入 activeContext §R2 P3 候选）

---

## 上上次任务（已归档闭环）

**TASK-20260430-04：规划 UI 编辑器 + 调试器（DevTool 三件套蓝图设计）[安全相关]** — ✅ 已归档于 2026-05-01 ~03:00，已 `--no-ff` 合入 main。**A 子任务已立项并完成为 TASK-20260502-01。**

- 归档文档 `memory-bank/archive/archive-TASK-20260430-04.md`
- 4 篇蓝图文档（spec + plan + 2 creative，~1879 行）+ D1-D8 决策矩阵 + T1-T8 威胁建模 + 7 项独立立项候选 + 4 项历史技术债闭环 ROI 路径
- plan ×0.6 第 17 数据点入库（核心轮次 0.27-0.35× plan / 0.46-0.59× plan ×0.6）

**TASK-20260430-03：全代码库 Code Review** — ✅ 已归档于 2026-05-01 ~00:30，已 `--no-ff` 合并到 main `2445990`。

- 归档文档 `memory-bank/archive/archive-TASK-20260430-03.md`
- R0 prep + R1 报告（55 findings / 6 维度归集）+ R2 P0 quick fix 6 项 + Reflect + Archive 全闭环
- R3+ 13 项 P1 候选 待用户决策拆分顺序后独立立项

---

## 最近归档完成（速查）

- **TASK-20260502-01：DevTool Phase A — Inspector 实施 [安全相关]** — Level 4 ✅（2026-05-02）— **本批最新**
- **TASK-20260430-04：规划 UI 编辑器 + 调试器（DevTool 三件套蓝图设计）[安全相关]** — Level 4 V2=a ✅（2026-05-01）
- **TASK-20260430-03：全代码库 Code Review（6 维度 × 7 子系统 + 多轮次 Build + Checkpoint）[安全相关]** — Level 4 ✅（2026-05-01）
- **TASK-20260430-02：CSS border shorthand 补全（4 方向 + 3 属性级）[安全相关]** — Level 2 ✅（2026-04-30）
- **TASK-20260430-01：first/last child margin collapse with parent（CSS 2.1 §8.3.1）** — Level 3 ✅（2026-04-30）
- **TASK-20260426-01：Layout 正确性消化（#25 + #28 + #20 + #21）** — Level 4 ✅（2026-04-30）
- **TASK-20260425-01：SDL2 窗口后端 + 输入事件桥接** — Level 3 ✅（2026-04-26）

## 说明

详细实现过程、测试细节与经验教训均已沉淀至各任务的 `archive-*` / `reflection-*` 文档。
