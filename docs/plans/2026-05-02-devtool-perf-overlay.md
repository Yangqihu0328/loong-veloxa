# 实现计划：DevTool Phase B — Performance Overlay 实施

**任务 ID：** TASK-20260502-02
**复杂度：** Level 3（中等功能 — 跨 5 子系统 / 蓝图 plan §Phase B 10 子任务 / 触及 #35 但非 dogfood 子系统级 / **不 escalate**，VAN push-back 5 项已沉淀）
**安全相关：** ✅ 是（T6 UpdateManager 钩子 callback 任意代码执行 + T5 DisplayList overlay 跨帧累积 — 2 威胁面全程守门）
**复用 spec：** `docs/specs/2026-04-30-devtool-design.md`（A6-A9 + A13-A14 + T5/T6 + I2/I3/I7 + R3）— 无新需求
**复用 creative：** `memory-bank/creative/creative-devtool-screen-layout.md` 决策 3（HUD overlay）+ 决策 5（F11 toggle）— 无新设计
**蓝图参照：** `docs/plans/2026-04-30-devtool.md` §Phase B 段（10 子任务粗模板）

---

## 0. Phase 0 — 全局约束与实证（writing-plans 必填 9 段全部就绪）

### 0.1 ctest baseline 二次验证（已实测 2026-05-02 21:42）

**结果：**
- DEVTOOL=ON: **1169/1169 PASS**（5.75 sec）+ 1 Skip（Wpt001 沿用）✅
- DEVTOOL=OFF: **1065/1065 PASS**（4.23 sec）+ 1 Skip ✅
- 与 main `8b2ead4` 终态（TASK-20260502-01 Phase A 完成快照）完全一致 ✅
- A14 链接闭包零自动化守门 `tests/smoke/devtool_a14_link_closure.cmake` 持续守护 ✅

### 0.2 CMake 链接审计

**新增/修改链接拓扑：**
- `vx_devtool` 静态库 += `overlay/perf_overlay.cc`（B.1）+ append 到既有 `veloxa/devtool/inspector/CMakeLists.txt`（参考 Phase A.1.5 InputDispatchSplitter 范式：`target_sources(vx_devtool PRIVATE ...)` append，避免新建子目录 lib，最小侵入）
- `vx_core` 静态库 += `update_manager.cc` 改造（PipelineHooks 注入点）+ `render/renderer.cc` 改造（dirty_rects_ Vector 累积扩展）
- `vx_api` 静态库 += `vx_view_set_pipeline_hooks` C API（B.0.1）+ `vx_view_get_perf_stats` C API（B.2.2）— `if(VX_BUILD_DEVTOOL)` 条件 link `vx_devtool`（沿用 Phase A.0.6 范式）
- 测试新加 `tests/devtool/overlay/CMakeLists.txt` + 注册到 `tests/devtool/CMakeLists.txt`（与 inspector/ 兄弟子目录）— 由 `if(VX_BUILD_DEVTOOL)` guard

**结论：零新静态库循环依赖**（vx_devtool → vx_core 既有 B 链 A 模式持续）

### 0.3 静态库循环依赖审计

| 新符号 | 归属 | 被谁调用 | 循环风险 |
|---|---|---|:-:|
| `vx::core::PipelineHooks` 结构体 | `vx_core/update_manager.h` | `vx_devtool/overlay/perf_overlay.cc` 注册 | ✅ 单向 |
| `vx::core::UpdateManager::SetPipelineHooks(const PipelineHooks*)` | `vx_core` | `vx_api::vx_view_set_pipeline_hooks` C API + `vx_devtool::PerfOverlay::Attach(UpdateManager&)` | ✅ 单向 |
| `vx::devtool::PerfOverlay` class | `vx_devtool/overlay/perf_overlay.h` | `vx_api::vx_view_get_perf_stats` C API + `tests/devtool/overlay/perf_overlay_test.cc` | ✅ 单向 |
| `vx_view_set_pipeline_hooks` / `vx_view_get_perf_stats` C ABI | `vx_api/veloxa_api.cc` | DevTool 内部 + 用户代码 | ✅ 公开 ABI 表面 |
| dirty_rects_ Vector accumulation 扩展 | `vx_core/update_manager.{h,cc}` | `vx_devtool::InspectorOverlay::InjectDirtyRectHighlight`（B.2.3 工厂复用 OverlayHighlight）| ✅ 单向 |

✅ 无静态库循环依赖。

### 0.4 FetchContent 网络代理守卫

- 本任务零新 FetchContent 依赖（FrameStats 用 `vx::css::SteadyTimePoint = std::chrono::steady_clock::time_point` 既有；HUD JS 在 panel 内复用既有 inspector_panel.js 范式；无第三方 perf 库）
- **跳过** FetchContent 守卫流程

### 0.5 测试基础设施审计（friend pattern 关键）

**Phase 0 grep 实证：** `friend class` 在 `veloxa/devtool/` 0 命中 → B.1.1 PerfOverlay 内部 ring buffer 访问需引入 friend pattern。

| 子任务 | 测试需访问 | 当前访问路径 | 引入方案 |
|---|---|---|---|
| B.0.1 PipelineHooks | hook 调用次数 + 顺序 | 新接口（callback 内 atomic counter）| 测试 lambda 闭包内置 counter，无需 friend |
| B.0.2 dirty_rects_ Vector | UpdateManager::dirty_rects() PUBLIC getter | 新增 PUBLIC getter（Vector copy 开销可接受 — perf 测试场景）| ✅ PUBLIC 自然语义 |
| B.1.1 FrameStats ring buffer | `frames_[60]` 数组 / `head_` index / 滑动窗口 average | 当前 PRIVATE | `friend class PerfOverlayTest;` 白名单 |
| B.1.2 1ms budget abort | hook abort flag + 错误日志 | 新接口（PerfOverlay::last_abort_reason() PUBLIC getter）| ✅ PUBLIC 自然语义 |
| B.2.3 InjectDirtyRectHighlight | DisplayList overlay command count | 复用 InspectorOverlay 既有 friend 模式 | ✅ 沿用 |

**禁止项：** ❌ 不用 `#define private public` / ❌ 不用 `reinterpret_cast` 越权 / ❌ 不在生产代码中暴露内部状态 PUBLIC（除非有 read-only 自然语义）

### 0.6 边界输入清单（writing-plans 必填，T5/T6 强相关）

| # | 类别 | 示例 | 预期行为 | 对应威胁 / 验收 |
|:-:|---|---|---|---|
| 1 | 默认 | 60 帧均匀 16.67 ms | FPS=60.0 ± 0.5 | A6 |
| 2 | 极快帧 | 60 帧均 1 ms | FPS=999.0 cap 或 actual | A6 |
| 3 | 极慢帧 | 60 帧均 100 ms | FPS=10.0 | A6 |
| 4 | 帧时长 0 | 帧 = 0.0 ms | 不除零 → FPS=∞ guard | A6 边界 |
| 5 | hook nullptr | PipelineHooks 全 nullptr | branch predictor 优化路径 ~0 开销（5 nullptr 检查 + 跳过）| A14 R3 性能 |
| 6 | 慢 hook callback | callback 含 sleep(2ms) | 1 ms budget 触发 abort + last_abort_reason() 设值 | A9 / T6 |
| 7 | 多 instance attach | 第 2 次 PerfOverlay::Attach(same UpdateManager) | 拒绝（Status::AlreadyExists 或 nullptr return）| T6 |
| 8 | dirty_rects 空 | 无变化帧 | dirty_rects_.empty() / Vector 0 元素 | A8 |
| 9 | dirty_rects 多 | 同帧 3 次 invalidate | 累积 Vector 3 元素 | A8 / B4 决策 |
| 10 | dirty rect 满屏 | 整 viewport invalidate | 边框包围整 viewport | A8 |
| 11 | F11 toggle | DevTool 关闭时按 F11 | no-op（不消费事件，转发 event_manager_）| A6 contract |
| 12 | F11 toggle 状态记忆 | 显示 → 隐藏 → 再显示 | 状态正确切换 + DOM display:none ↔ block | A6 |
| 13 | HUD JS perf_stats 读取 | JS 调 vx_view_get_perf_stats() | 返回最近 60 帧聚合 FrameStats | A6/A7 |
| 14 | overlay command 跨帧累积 | 5 帧未 reset | T5 ResetOverlayCommands 协议保证清零 | T5 |
| 15 | DevTool OFF 路径 | `vx_view_set_pipeline_hooks(view, NULL)` | 返回 INVALID_STATE / NULL_PARAM | A14 / 公开 ABI |
| 16 | binary size guard | DEVTOOL=OFF rebuild | libvx_api.a + libvx_core.a 不增 DevTool 内部符号（A14 ctest smoke 自动验证）| A14 |

**每条边界对应 ≥ 1 个测试** — 16 条边界 → ~14 个边界测（部分合并），分布在 B.0.1 / B.0.2 / B.1.1 / B.1.2 / B.2.3 / B.3.x。

### 0.7 调用链端到端验证（B.0.1 PipelineHooks 五钩子注入点关键）

**Phase 0 grep 实证：** `UpdateManager::Update()` 是 #35 五钩子的**唯一注入点**，对外 callsite 仅 1 处（`veloxa_api.cc:200 app->Update()` → `EnsureUpdateManager` + `update_manager_->Update()`）。

**完整流程拆解（update_manager.cc:16-48）：**

```
UpdateManager::Update() {
  if (!dirty_ || !config_.document) return;        ← OnFrameStart 注入位置 1（最早）
  arena_.Reset();
  layout_root_ = layout::LayoutEngine::Layout(...);  ← 包含 style resolve（CSS 解析在 Layout 内）
  DetectAndApplyTransitions();                       ← OnAfterStyle 注入位置 2（style 阶段结束）
                                                      ← OnAfterLayout 注入位置 3（紧跟，layout 阶段结束）
  render::DisplayList new_list = render::Record(...); ← OnAfterRender 注入位置 4（PaintCommand 录制完成）
  last_dirty_rect_ = render::ComputeDirtyRect(...);
  if (config_.canvas && !last_dirty_rect_.IsEmpty()) {
    render::Replay(...);
  }
  display_list_ = std::move(new_list);
  dirty_ = false;
                                                      ← OnFrameEnd 注入位置 5（最后）
}
```

**FrameStats 5 字段映射（spec §5.1 与 plan 一致）：**

| 字段 | 公式（SteadyTimePoint diff，ms 浮点）|
|---|---|
| `style_ms` | `t_after_style - t_frame_start` |
| `layout_ms` | `t_after_layout - t_after_style` |
| `render_ms` | `t_after_render - t_after_layout` |
| `paint_ms` | `t_frame_end - t_after_render` |
| `total_ms` | `t_frame_end - t_frame_start` |

**注意：** 当前 `LayoutEngine::Layout` 内含 style resolve；OnAfterStyle 实际应放 `DetectAndApplyTransitions()` 之后但在 Layout 之前是不可能的（Layout 已含 style）。**调整：** OnAfterStyle 与 OnAfterLayout **同点触发**（DetectAndApplyTransitions 之后），这是 spec §5.1 五钩子的妥协实现 — `style_ms` 和 `layout_ms` 字段会出现 `style_ms = ~0 + layout_ms = LayoutEngine 总时长`。后续 R3+ 拆分 LayoutEngine 内 style/layout 子阶段时可精化（属技术债 #35 完整闭环阶段 2，本任务仅闭环阶段 1）。

### 0.8 既有测试隐式契约 fingerprint

| 子系统 | 测试文件 | grep 关键字 | 命中实测 | 改造影响 |
|---|---|---|:-:|---|
| update_manager | `tests/core/update_manager_test.cc` | `DirtyRectComputedOnUpdate` / `HoverChangeProducesDirtyRect` | ≥ 2 ✅ | 🟢 dirty_rects_ Vector 扩展需保 `last_dirty_rect()` 既有契约（last == dirty_rects_.back() 或单 rect 合并） |
| renderer | `tests/core/render/{paint_command,renderer,border_radius}_test.cc` | `EXPECT.*PaintCommand` 等 | ≥ 19 ✅ | 🟢 不动 |
| event | `tests/core/event/*` | `hover_target` / `propagated` | 0 ✅ | 🟢 F11 hotkey 在 Application 层拦截，不触发 |
| application | `tests/core/application_test.cc + application_devtool_test.cc` | `EXPECT_EQ.*document` 等 | ≥ 2 ✅ | 🟢 PerfOverlay attach 复用 LoadDevtoolDocument 范式不污染 |
| inspector_panel | `tests/devtool/resources/inspector_panel_smoke_test.cc` | HTML/CSS 子集 grep | 14 ✅ | ⚠️ B.2.1 HUD HTML/CSS 加入 inspector_panel.css 时需复用 R2-verified CSS 子集 + StripCssComments helper（A.1.3 沉淀） |

### 0.9 CSS shorthand 能力 grep 表（HUD CSS 设计）

**Phase 0 grep 实证（针对 HUD 必需的 CSS 属性）：**

| CSS 属性 | parser 状态 | computed_style 状态 | layout 支持 | HUD 用途 | 决策 |
|---|:-:|:-:|:-:|---|---|
| `position: fixed` | ✅ parser.cc:1239 `Position::kFixed` | ✅ enum 已定义 | ⚠️ flex_layout.cc:84 仅 check `kAbsolute`（fixed 未单独处理）| HUD 容器固定屏幕 | **使用 `position: absolute`**（fallback 方案 — fixed 在当前 layout 行为同 absolute；HUD 在 DevTool Document 内 root 元素，absolute 等价）|
| `position: absolute` | ✅ | ✅ | ✅ flex_layout.cc:84 跳过 normal flow | 同上 | ✅ 选用 |
| `opacity` | ✅ property.h:45 / parser.cc:158 | ✅ computed_style.h:48 (`f32 opacity = 1.0f`) | ✅ transition 已支持（gradient animate）| HUD 半透明 0.85 | ✅ 直接用 |
| `top` / `left` | ✅（含在 longhand 子集）| ✅ | ✅ | HUD 定位 8px 8px | ✅ |
| `width` / `height` | ✅ | ✅ | ✅ | HUD 尺寸 180×90 | ✅ |
| `padding` | ✅ shorthand 已支持（A.1.3 grep）| ✅ | ✅ | HUD 内边距 6px 8px | ✅ |
| `background-color` | ✅ longhand | ✅ | ✅ | HUD 深色背景 #1a1a1a | ✅ |
| `color` | ✅ | ✅ | ✅ | HUD 文字 #00ff00 monospace | ✅ |
| `font-family: monospace` | ✅（A.1.3 已用）| ✅ | ✅ | HUD 数字等宽显示 | ✅ |
| `font-size` | ✅ | ✅ | ✅ | HUD 11px | ✅ |
| `border-radius` | ✅（A.1.3 已用）| ✅ | ✅ | HUD 圆角 4px | ✅ |
| `pointer-events: none` | ❌ 不支持（A.1.3 sentinel test 防御）| — | — | HUD 不拦截鼠标事件 | **不用** — fallback：HUD 区域 ~180×90 px 影响有限（spec creative #1 决策 3 已 documented） |
| `z-index` | ⚠️ 待 grep 验证 | — | — | HUD 在 panel 之上 | **不用** — HUD 在 DevTool Document 根级 absolute positioned，自然在最末渲染（无需 z-index） |
| `display: flex` | ✅（A.1.3 已用）| ✅ | ✅ | HUD bars row 横向布局 | ✅ |

**结论：** HUD CSS 可行性 100%，0 个 R2 缺陷阻塞。`position: fixed` fallback 为 `position: absolute`（视觉等价）+ `pointer-events: none` 兜底说明（spec creative #1 已 documented）。

### 0.10 工具链与子系统关闭守门

- ✅ Python3 + cmake nm 自动化（A14 ctest smoke `tests/smoke/devtool_a14_link_closure.cmake`）— B.0/B.1 完成后**追加** `PerfOverlay` / `RegisterPerfStatsBindings` / `kPerfHudHtml` 等内部符号到黑名单
- ✅ `vx_devtool` 子目录 + `vx_devtool_resources` STATIC 已建立 — B 不需新子目录（perf_overlay.cc append 到 inspector/ 库，HUD HTML/CSS 不需独立 resource lib，可 inline 到 inspector_panel.html/css 末尾或新增 `hud_resources` blob）

### 0.11 工具链 grep 命中

- `rg` 不可用 → 使用系统 `grep -rE` 或 Grep tool
- jq 缺失 → A14 size diff 用 python3 兜底（沿用 Phase A 范式）

---

## 1. B1-B8 build 级精化决策表（2026-05-02 21:50，用户 1 次 AskQuestion 选 all_recommended → 8/8 锁定）

| # | 决策维度 | 锁定值 | 理由 |
|:-:|---|---|---|
| **B1** | plan 文档形态 | **A 独立 plan 文档** `docs/plans/2026-05-02-devtool-perf-overlay.md` | 与 Phase A 范式一致，便于回溯 + reflect 阶段引用 |
| **B2** | 子任务并行性 | **A 严格串行 B.0.1 → B.3.3** | 依赖链清晰（B.0.1 PipelineHooks 是 B.1.1 PerfOverlay 前置；B.0.2 dirty_rects 扩展是 B.2.3 边框高亮前置；B.2.x 是 B.3.x 集成前置）|
| **B3** | 测试组织 | **A 新建 `tests/devtool/overlay/` 子树**（perf_overlay_test.cc + perf_hud_smoke_test.cc）| 与 inspector/ 兄弟子目录，命名清晰；引入 `tests/devtool/overlay/CMakeLists.txt` 注册到 `tests/devtool/CMakeLists.txt` |
| **B4** | dirty_rects 扩展形式 | **A 单矩形扩展 dirty_rects_ Vector**（**仅累积同帧多次 invalidate**，**不替换** `last_dirty_rect_`）| 既有契约 `last_dirty_rect()` 100% 不破坏（`last_dirty_rect_ = dirty_rects_.empty() ? Rect{} : Vector::Union(dirty_rects_)`）；既有 2 测 `DirtyRectComputedOnUpdate` / `HoverChangeProducesDirtyRect` 全过 |
| **B5** | dirty rect 边框 PaintCommand | **A 复用 kOverlayHighlight + 新工厂 `OverlayDirtyRect(rect, color, stroke_width)`**（不新增 enum）| paint_command.h / renderer.cc switch 零改动；语义在工厂签名层面区分；T5 ResetOverlayCommands 协议自动覆盖 |
| **B6** | commit 颗粒度 | **A 每子任务 1 commit**（10 commits + 可能 1-2 round-pause + reflect + archive）| 与 Phase A 范式一致；reflect 阶段易回溯每子任务实测耗时 |
| **B7** | plan ×0.6 估时基线 | **A 用 VAN F2 发现修正后估时 ~3-4 h plan ×0.6**（基于：蓝图 4.35 h → archive 校准下调 ~30% 到 ~3.5-5 h → VAN F2 dirty rect 已就绪 + F3/F4/F5 副产品复用进一步下调 ~3-4 h）| reflect 阶段验证落「大件实现」桶 0.6-1.1× vs「极窄档延续」可能；plan ×0.6 第 38 数据点入库 |
| **B8** | spec 复用 | **A 复用 TASK-30-04 spec**（A6-A9 + A13-A14 + T5/T6 + I2/I3/I7 + R3）| 蓝图阶段已锁定，无新需求；本任务专注 build 实施 |

---

## 2. Phase B 子任务清单（10 子任务 5-步 TDD 模板）

每子任务遵循 RED → GREEN → REFACTOR + 反向探针 + A14 守门 + commit 5 步范式（与 Phase A 一致）。

### B.0.1 — UpdateManager PipelineHooks 五钩子 + `vx_view_set_pipeline_hooks` C API（**#35 闭环**）

**估时：** plan 90 min / plan ×0.6 54 min（**最大子任务**）
**威胁面：** T6（callback 任意代码执行）— budget abort 留给 B.1.2，本子任务仅暴露 hook 接口
**验收：** A7 + A14 R3（nullptr 路径性能不退化）

#### Step 1 — RED 测

新建测加入 `tests/core/update_manager_test.cc`（既有套件复用，避免新建 file）：

```cpp
// tests/core/update_manager_test.cc 末尾（line ~150 后）

TEST_F(UpdateManagerTest, PipelineHooksAllNullByDefault) {
  EXPECT_EQ(update_manager_->pipeline_hooks(), nullptr);
}

TEST_F(UpdateManagerTest, SetPipelineHooksStoresPointer) {
  PipelineHooks hooks{};
  update_manager_->SetPipelineHooks(&hooks);
  EXPECT_EQ(update_manager_->pipeline_hooks(), &hooks);
  update_manager_->SetPipelineHooks(nullptr);
  EXPECT_EQ(update_manager_->pipeline_hooks(), nullptr);
}

TEST_F(UpdateManagerTest, AllFiveHooksFireInOrderOnUpdate) {
  std::vector<std::string> order;
  PipelineHooks hooks;
  hooks.on_frame_start  = [&](void*){ order.push_back("start"); };
  hooks.on_after_style  = [&](void*){ order.push_back("style"); };
  hooks.on_after_layout = [&](void*){ order.push_back("layout"); };
  hooks.on_after_render = [&](void*){ order.push_back("render"); };
  hooks.on_frame_end    = [&](void*){ order.push_back("end"); };
  update_manager_->SetPipelineHooks(&hooks);
  update_manager_->Invalidate();
  update_manager_->Update();
  EXPECT_EQ(order, (std::vector<std::string>{"start", "style", "layout", "render", "end"}));
}

TEST_F(UpdateManagerTest, NullHooksUpdateRemainsLossless) {
  // 反向 verify A14 R3 — nullptr 路径不影响既有契约（layout_root + display_list 都正确生成）
  update_manager_->SetPipelineHooks(nullptr);
  update_manager_->Invalidate();
  update_manager_->Update();
  EXPECT_NE(update_manager_->layout_root(), nullptr);
  EXPECT_FALSE(update_manager_->display_list().empty());
}

TEST_F(UpdateManagerTest, HooksNotFiredWhenNoUpdateNeeded) {
  std::atomic<int> n{0};
  PipelineHooks hooks;
  hooks.on_frame_start = [&](void*){ n++; };
  update_manager_->SetPipelineHooks(&hooks);
  update_manager_->Invalidate();
  update_manager_->Update();  // 第一次：dirty=true → fire
  EXPECT_EQ(n.load(), 1);
  update_manager_->Update();  // 第二次：dirty=false → skip
  EXPECT_EQ(n.load(), 1);
}
```

新增公开 C API 测 `tests/api/perf_hooks_api_test.cc`（与 Phase A `devtool_attach_api_test.cc` 兄弟）：

```cpp
TEST_F(PerfHooksApiTest, SetPipelineHooksWithNullViewReturnsNullParam) {
  EXPECT_EQ(vx_view_set_pipeline_hooks(nullptr, nullptr, nullptr),
            VX_ERROR_NULL_PARAM);
}

#ifdef VX_BUILD_DEVTOOL
TEST_F(PerfHooksApiTest, SetPipelineHooksWithValidViewReturnsOk) {
  /* 创建 view，set hooks，调 vx_view_run 一次（or invalidate+update）
     verify 5 callback 各调一次 */
}
#else
TEST_F(PerfHooksApiTest, SetPipelineHooksOffPathReturnsInvalidState) {
  /* 创建 view，set hooks → 返回 VX_ERROR_INVALID_STATE（A14 stub） */
}
#endif
```

#### Step 2 — 验证 RED

```bash
cd build && cmake --build . -j 2>&1 | grep -E "error:|FAIL"
# 预期：'PipelineHooks' was not declared / 'pipeline_hooks' is not a member / 'vx_view_set_pipeline_hooks' was not declared
```

#### Step 3 — GREEN 实施

**`veloxa/core/update_manager.h` 加入：**

```cpp
namespace vx {

// TASK-20260502-02 B.0.1 (技术债 #35 阶段 1 闭环) — Performance Overlay 五钩子。
// callback 在 UpdateManager::Update() 内严格按顺序触发：
//   on_frame_start  → 入口（dirty/config check 之后）
//   on_after_style  → DetectAndApplyTransitions() 之后
//   on_after_layout → 同 on_after_style 同点（当前 LayoutEngine::Layout 含 style resolve；
//                     style/layout 拆分留 #35 阶段 2，技术债登记）
//   on_after_render → render::Record() 之后（PaintCommand 录制完成）
//   on_frame_end    → Update() 末尾（Replay 之后）
// userdata 用于 callback context recover（PerfOverlay::Attach 时传 this 指针）。
// 全部 callback nullptr 时，分支预测器优化路径开销 ~0（5 nullptr check 跳过）。
struct PipelineHooks {
  using Callback = void(*)(void* userdata);
  Callback on_frame_start  = nullptr;
  Callback on_after_style  = nullptr;
  Callback on_after_layout = nullptr;
  Callback on_after_render = nullptr;
  Callback on_frame_end    = nullptr;
  void* userdata = nullptr;
};

class UpdateManager {
 public:
  // ... 既有 ...

  // B.0.1 — set hooks (nullptr to clear). UpdateManager 不持 PipelineHooks
  // 所有权 — 调用方（PerfOverlay）保证 hooks 生命周期 ≥ UpdateManager。
  void SetPipelineHooks(const PipelineHooks* hooks) { pipeline_hooks_ = hooks; }
  const PipelineHooks* pipeline_hooks() const { return pipeline_hooks_; }

 private:
  // ... 既有 ...
  const PipelineHooks* pipeline_hooks_ = nullptr;
};

}  // namespace vx
```

**`veloxa/core/update_manager.cc` 改造 Update()：**

```cpp
void UpdateManager::Update() {
  if (!dirty_ || !config_.document) return;

  // B.0.1 — fire-hook helper (inline branch-predictor friendly)
  auto fire = [this](PipelineHooks::Callback cb) {
    if (cb && pipeline_hooks_) cb(pipeline_hooks_->userdata);
  };

  fire(pipeline_hooks_ ? pipeline_hooks_->on_frame_start : nullptr);

  arena_.Reset();
  layout_root_ = layout::LayoutEngine::Layout(config_.document,
                                              config_.layout_context, arena_);
  DetectAndApplyTransitions();

  fire(pipeline_hooks_ ? pipeline_hooks_->on_after_style : nullptr);
  fire(pipeline_hooks_ ? pipeline_hooks_->on_after_layout : nullptr);

  render::DisplayList new_list;
  if (layout_root_) {
    new_list = render::Record(layout_root_, config_.image_cache);
  }

  fire(pipeline_hooks_ ? pipeline_hooks_->on_after_render : nullptr);

  last_dirty_rect_ = render::ComputeDirtyRect(
      display_list_, new_list, config_.layout_context.viewport_width,
      config_.layout_context.viewport_height);

  if (config_.canvas && !last_dirty_rect_.IsEmpty()) {
    config_.canvas->PushClipRect(last_dirty_rect_);
    config_.canvas->FillRect(last_dirty_rect_,
                             gfx::Brush::Solid(gfx::Color::White()));
    render::Replay(new_list, config_.canvas, config_.image_cache);
    config_.canvas->PopClip();
  }

  display_list_ = std::move(new_list);
  dirty_ = false;

  if (transition_mgr_.HasActive()) dirty_ = true;

  fire(pipeline_hooks_ ? pipeline_hooks_->on_frame_end : nullptr);
}
```

**`veloxa/api/veloxa_api.h` 加入：**

```c
typedef void (*VxPipelineCallback)(void* userdata);

typedef struct VxPipelineHooks {
  VxPipelineCallback on_frame_start;
  VxPipelineCallback on_after_style;
  VxPipelineCallback on_after_layout;
  VxPipelineCallback on_after_render;
  VxPipelineCallback on_frame_end;
} VxPipelineHooks;

/* TASK-20260502-02 B.0.1 — 设置 Pipeline 钩子（DevTool 内部用）。
   - hooks=NULL 清除既有钩子。
   - userdata 在每个 callback 调用时透传。
   - DEVTOOL=OFF 路径返回 VX_ERROR_INVALID_STATE（A14 stub）。 */
VxResult vx_view_set_pipeline_hooks(VxView* view,
                                    const VxPipelineHooks* hooks,
                                    void* userdata);
```

**`veloxa/api/veloxa_api.cc` 实施：**

```cpp
VxResult vx_view_set_pipeline_hooks(VxView* view, const VxPipelineHooks* hooks,
                                     void* userdata) {
  if (!view) return VX_ERROR_NULL_PARAM;
#ifndef VX_BUILD_DEVTOOL
  (void)hooks; (void)userdata;
  return VX_ERROR_INVALID_STATE;
#else
  Application* app = view->app.get();
  // EnsureUpdateManager 必须先调（OnFrame 隐含调）— 这里我们假设 view 已 run 过
  if (!app->update_manager_for_devtool()) return VX_ERROR_INVALID_STATE;
  if (!hooks) {
    app->update_manager_for_devtool()->SetPipelineHooks(nullptr);
    return VX_OK;
  }
  // 转换 C struct → C++ struct（持久化在 Application 内 — view 销毁时跟随）
  app->set_external_pipeline_hooks({
    hooks->on_frame_start,
    hooks->on_after_style,
    hooks->on_after_layout,
    hooks->on_after_render,
    hooks->on_frame_end,
    userdata
  });
  app->update_manager_for_devtool()->SetPipelineHooks(
      &app->external_pipeline_hooks());
  return VX_OK;
#endif
}
```

`Application` 加 `update_manager_for_devtool()` getter（非 const，返回 `UpdateManager*` 内部 update_manager_）+ `external_pipeline_hooks_` PipelineHooks 字段 + `set_external_pipeline_hooks()` setter。

#### Step 4 — 验证 GREEN

```bash
cd build && cmake --build . -j 2>&1 | tail -3
ctest -j 2>&1 | grep -E '100% tests passed|failed' | head -3
# 预期：1169 + 5 (UpdateManagerTest 新) + 2-3 (PerfHooksApiTest 新) = 1176-1177 PASS
```

#### Step 5 — 反向探针

把 `fire(... on_after_layout ...)` 注释掉 → `AllFiveHooksFireInOrderOnUpdate` 测精准 FAIL（order 缺 "layout"）✅；恢复后 GREEN。

#### Step 6 — A14 守门

- DEVTOOL=OFF rebuild + ctest 1065 维持（PerfHooksApiTest OFF 路径仅编译 stub 测）
- libvx_api.a OFF size + ~150-200 bytes（C ABI 公开 stub 表面，nm 验证零 PerfOverlay 内部符号引用）
- libvx_core.a OFF size 持平（PipelineHooks 仅 header struct + UpdateManager 私有成员 nullptr，不引入额外符号）
- 自动化 ctest smoke `devtool_a14_link_closure_smoke` 双侧 PASS

#### Commit

```
feat(core): UpdateManager PipelineHooks 五钩子 + vx_view_set_pipeline_hooks C API (B.0.1)

技术债 #35 阶段 1 闭环：UpdateManager 暴露 5 callback 注入点（OnFrameStart /
OnAfterStyle / OnAfterLayout / OnAfterRender / OnFrameEnd）+ C ABI
vx_view_set_pipeline_hooks（DEVTOOL=OFF 返 INVALID_STATE）。函数指针 +
nullptr 分支预测优化保证 nullptr 路径性能不退化（A14 R3）。
```

---

### B.0.2 — dirty_rects_ Vector 累积扩展

**估时：** plan 60 min / plan ×0.6 36 min
**威胁面：** —
**验收：** A8（dirty rect collector）

#### Step 1 — RED 测（加入 `update_manager_test.cc`）

```cpp
TEST_F(UpdateManagerTest, DirtyRectsEmptyBeforeUpdate) {
  EXPECT_TRUE(update_manager_->dirty_rects().empty());
}

TEST_F(UpdateManagerTest, SingleUpdateProducesOneDirtyRect) {
  update_manager_->Invalidate();
  update_manager_->Update();
  EXPECT_EQ(update_manager_->dirty_rects().size(), 1u);
  EXPECT_EQ(update_manager_->dirty_rects().back(), update_manager_->last_dirty_rect());
}

TEST_F(UpdateManagerTest, MultipleInvalidateInSameFrameAccumulates) {
  // 模拟同帧 3 次 Invalidate + Update（实际场景：JS 多次 setStyle 在 1 帧）
  // 注意：Update() 只会 fire 1 次（dirty 第 2 次 Update return early）
  // 累积语义：每次 Invalidate + Update 都 push 1 个 dirty rect 到 Vector
  // 而 Update 第 2/3 次因 dirty=false skip — 测试需手动 Invalidate 之间触发
  update_manager_->Invalidate(); update_manager_->Update();
  update_manager_->Invalidate(); update_manager_->Update();
  update_manager_->Invalidate(); update_manager_->Update();
  EXPECT_EQ(update_manager_->dirty_rects().size(), 3u);
}

TEST_F(UpdateManagerTest, ClearDirtyRectsResetsVector) {
  update_manager_->Invalidate(); update_manager_->Update();
  update_manager_->Invalidate(); update_manager_->Update();
  EXPECT_EQ(update_manager_->dirty_rects().size(), 2u);
  update_manager_->ClearDirtyRects();
  EXPECT_TRUE(update_manager_->dirty_rects().empty());
}

TEST_F(UpdateManagerTest, LastDirtyRectStaysContractCompatible) {
  // 既有契约 — last_dirty_rect() 必须返回最后一个 update 的 dirty rect
  // (与 dirty_rects().back() 一致)
  update_manager_->Invalidate(); update_manager_->Update();
  EXPECT_EQ(update_manager_->last_dirty_rect(), update_manager_->dirty_rects().back());
}
```

#### Step 2-3 — RED 验证 + GREEN 实施

`update_manager.h` 加入：

```cpp
class UpdateManager {
 public:
  // ... 既有 ...
  // B.0.2 — TASK-20260502-02
  // 同帧多次 Update 累积的 dirty rect 列表（Performance Overlay dirty rect
  // 边框高亮渲染用）。Vector 在 Update() 内 push_back，由 PerfOverlay 在
  // OnFrameStart 钩子内调 ClearDirtyRects() 清空（每帧一次）。
  // last_dirty_rect_ 仍然语义为「最近一次 Update 计算的 dirty rect」，
  // 保持向后兼容（== dirty_rects_.back() 当 dirty_rects_ 非空）。
  const Vector<gfx::Rect>& dirty_rects() const { return dirty_rects_; }
  void ClearDirtyRects() { dirty_rects_.clear(); }

 private:
  Vector<gfx::Rect> dirty_rects_;  // B.0.2
};
```

`update_manager.cc` Update() 内在 `last_dirty_rect_ = ...` 后追加：

```cpp
last_dirty_rect_ = render::ComputeDirtyRect(...);
if (!last_dirty_rect_.IsEmpty()) {
  dirty_rects_.push_back(last_dirty_rect_);  // B.0.2 累积
}
```

#### Step 4-5 — GREEN 验证 + 反向探针

把 `dirty_rects_.push_back(...)` 注释 → `SingleUpdateProducesOneDirtyRect` 精准 FAIL（size 0 vs 1）✅

#### Step 6 — A14 守门

DEVTOOL=OFF baseline 1065 维持；libvx_core.a OFF size + ~50-100 bytes（Vector 实例化字节，非 DevTool 链接）— A14 链接闭包零严格满足。

#### Commit

```
feat(core): dirty_rects_ Vector 累积扩展 (B.0.2)

UpdateManager 加 dirty_rects() getter + ClearDirtyRects() — 同帧多次 Update
累积。last_dirty_rect() 既有契约保持（== dirty_rects().back()）。
为 B.2.3 Performance Overlay dirty rect 边框高亮提供数据源。
```

---

### B.1.1 — PerfOverlay FrameStats ring buffer + 滑动 60 帧聚合

**估时：** plan 60 min / plan ×0.6 36 min
**威胁面：** —
**验收：** A6（FPS 滑动均值）

#### Step 1 — RED 测（新建 `tests/devtool/overlay/perf_overlay_test.cc`）

```cpp
#include "veloxa/devtool/overlay/perf_overlay.h"

#include <gtest/gtest.h>

namespace vx::devtool::overlay {

class PerfOverlayTest : public ::testing::Test {
 protected:
  PerfOverlay overlay_;
};

TEST_F(PerfOverlayTest, InitialFrameCountIsZero) {
  EXPECT_EQ(overlay_.frame_count(), 0u);
}

TEST_F(PerfOverlayTest, RecordOneFramePopulatesRingBuffer) {
  FrameStats stats{0.5f, 1.0f, 2.0f, 1.5f, 5.0f};  // 5ms total
  overlay_.RecordFrame(stats);
  EXPECT_EQ(overlay_.frame_count(), 1u);
  EXPECT_FLOAT_EQ(overlay_.aggregated().total_ms, 5.0f);
}

TEST_F(PerfOverlayTest, SixtyFramesAggregatesAverages) {
  for (int i = 0; i < 60; i++) {
    overlay_.RecordFrame({1.0f, 2.0f, 3.0f, 4.0f, 10.0f});  // total = 10ms
  }
  EXPECT_EQ(overlay_.frame_count(), 60u);
  EXPECT_FLOAT_EQ(overlay_.aggregated().total_ms, 10.0f);
  EXPECT_FLOAT_EQ(overlay_.aggregated().style_ms, 1.0f);
  EXPECT_FLOAT_EQ(overlay_.aggregated().layout_ms, 2.0f);
  EXPECT_NEAR(overlay_.fps(), 100.0f, 0.01f);  // 1000 / 10
}

TEST_F(PerfOverlayTest, RingBufferOverwritesOldestFrameAfter60) {
  // First 60 frames: 10ms total
  for (int i = 0; i < 60; i++) overlay_.RecordFrame({0,0,0,0, 10.0f});
  // 61st frame: 20ms total → overwrites slot 0
  overlay_.RecordFrame({0,0,0,0, 20.0f});
  EXPECT_EQ(overlay_.frame_count(), 60u);  // capped at 60
  // Average: (59 * 10 + 20) / 60 = (590 + 20) / 60 = 10.166...
  EXPECT_NEAR(overlay_.aggregated().total_ms, 10.1667f, 0.01f);
}

TEST_F(PerfOverlayTest, ZeroTotalMsGuardedAgainstDivByZero) {
  overlay_.RecordFrame({0,0,0,0, 0.0f});
  EXPECT_FALSE(std::isinf(overlay_.fps()));  // FPS guard returns 0 or 999.0 cap
}

TEST_F(PerfOverlayTest, ResetClearsRingBuffer) {
  overlay_.RecordFrame({1,1,1,1, 4.0f});
  overlay_.Reset();
  EXPECT_EQ(overlay_.frame_count(), 0u);
}
```

#### Step 2-3 — GREEN 实施

新建 `veloxa/devtool/overlay/perf_overlay.h`：

```cpp
#ifndef VELOXA_DEVTOOL_OVERLAY_PERF_OVERLAY_H_
#define VELOXA_DEVTOOL_OVERLAY_PERF_OVERLAY_H_

#include <array>
#include "veloxa/foundation/types.h"

namespace vx::devtool::overlay {

// TASK-20260502-02 B.1.1 — Performance Overlay FrameStats 5 字段
// (与 spec §5.1 + B.0.1 五钩子时间差对齐)。
struct FrameStats {
  f32 style_ms  = 0.0f;
  f32 layout_ms = 0.0f;
  f32 render_ms = 0.0f;
  f32 paint_ms  = 0.0f;
  f32 total_ms  = 0.0f;
};

// PerfOverlay — 滑动 60 帧聚合 + ring buffer + FPS 计算。
// 与 UpdateManager::PipelineHooks 协同（B.1.2 / B.3.1 attach 模型）。
class PerfOverlay {
 public:
  static constexpr usize kCapacity = 60;

  PerfOverlay() = default;
  ~PerfOverlay() = default;

  void RecordFrame(const FrameStats& stats);
  void Reset();

  usize frame_count() const { return count_; }
  FrameStats aggregated() const;  // 滑动均值
  f32 fps() const;                // 1000 / aggregated.total_ms（含 guard）

 private:
  friend class PerfOverlayTest;  // 0.5 测试基础设施 — ring buffer 内部访问
  std::array<FrameStats, kCapacity> frames_{};
  usize head_ = 0;   // 下一个写入位置
  usize count_ = 0;  // 0..60，达到 60 后停止增长
};

}  // namespace vx::devtool::overlay

#endif  // VELOXA_DEVTOOL_OVERLAY_PERF_OVERLAY_H_
```

新建 `veloxa/devtool/overlay/perf_overlay.cc`：

```cpp
#include "veloxa/devtool/overlay/perf_overlay.h"

namespace vx::devtool::overlay {

void PerfOverlay::RecordFrame(const FrameStats& stats) {
  frames_[head_] = stats;
  head_ = (head_ + 1) % kCapacity;
  if (count_ < kCapacity) count_++;
}

void PerfOverlay::Reset() {
  for (auto& f : frames_) f = FrameStats{};
  head_ = 0;
  count_ = 0;
}

FrameStats PerfOverlay::aggregated() const {
  if (count_ == 0) return FrameStats{};
  FrameStats sum{};
  for (usize i = 0; i < count_; i++) {
    sum.style_ms  += frames_[i].style_ms;
    sum.layout_ms += frames_[i].layout_ms;
    sum.render_ms += frames_[i].render_ms;
    sum.paint_ms  += frames_[i].paint_ms;
    sum.total_ms  += frames_[i].total_ms;
  }
  f32 n = static_cast<f32>(count_);
  return {sum.style_ms / n, sum.layout_ms / n, sum.render_ms / n,
          sum.paint_ms / n, sum.total_ms / n};
}

f32 PerfOverlay::fps() const {
  f32 t = aggregated().total_ms;
  if (t <= 0.001f) return 0.0f;          // div-by-zero guard
  f32 raw = 1000.0f / t;
  return raw > 999.0f ? 999.0f : raw;    // cap
}

}  // namespace vx::devtool::overlay
```

`veloxa/devtool/inspector/CMakeLists.txt` `target_sources(vx_devtool PRIVATE ...)` 末尾追加：

```cmake
target_sources(vx_devtool PRIVATE
  # ... 既有 ...
  ${CMAKE_CURRENT_SOURCE_DIR}/../overlay/perf_overlay.cc  # B.1.1
)
```

新建 `tests/devtool/overlay/CMakeLists.txt`：

```cmake
vx_add_test(perf_overlay_test perf_overlay_test.cc)
target_link_libraries(perf_overlay_test PRIVATE vx_devtool)
```

`tests/devtool/CMakeLists.txt` 加 `add_subdirectory(overlay)`。

#### Step 4 — GREEN 验证

ctest +6 测 = 1176 → 1182 PASS（取决于 B.0.x 增量；待实测累计）

#### Step 5 — 反向探针

把 `head_ = (head_ + 1) % kCapacity` 改为 `head_ = head_ + 1` (off-by-one 模数缺失) → `RingBufferOverwritesOldestFrameAfter60` 精准 FAIL（数组越界 → undefined / aggregated 错值）✅

#### Step 6 — A14 守门

DEVTOOL=OFF baseline 1065 维持（perf_overlay_test 不参编译，零字节贡献）；A14 ctest smoke 黑名单**追加** `PerfOverlay`（在 B.3.3 整合后一次更新）

#### Commit

```
feat(devtool): PerfOverlay FrameStats ring buffer + 60 帧滑动聚合 (B.1.1)

新子系统 vx_devtool/overlay：FrameStats 5 字段 + 60 帧 ring buffer +
aggregated() 均值 + fps() 守卫（除零 guard + 999 cap）+ Reset()。
独立单元测 6 测，纯算法零外部依赖。friend class PerfOverlayTest
访问 ring buffer 内部状态（plan §0.5 测试基础设施审计）。
```

---

### B.1.2 — T6 callback 1ms/frame budget abort + 单 instance 验证

**估时：** plan 45 min / plan ×0.6 27 min
**威胁面：** **T6** ✅
**验收：** A9 + T6 mitigation

#### Step 1 — RED 测（加入 `tests/devtool/overlay/perf_overlay_test.cc`）

```cpp
TEST_F(PerfOverlayTest, AttachOnceSucceeds) {
  // PerfOverlay::Attach(UpdateManager*) — 设置 PipelineHooks → UpdateManager
  // 用 mock UpdateManager 或集成 fixture
  // ...
  EXPECT_TRUE(overlay_.Attach(&fake_update_manager_));
}

TEST_F(PerfOverlayTest, AttachTwiceRejectsSecond) {
  EXPECT_TRUE(overlay_.Attach(&fake_update_manager_));
  EXPECT_FALSE(overlay_.Attach(&fake_update_manager_));  // T6 单 instance
  EXPECT_EQ(overlay_.last_attach_error(), "already attached");
}

TEST_F(PerfOverlayTest, BudgetAbortTriggersOnSlowCallback) {
  // 注入慢 callback 模拟 — 实际通过设置 budget_us = 100 (0.1ms) + sleep(1ms)
  overlay_.set_budget_us(100);
  overlay_.Attach(&fake_update_manager_);
  // mock UpdateManager 触发钩子 → callback 内 sleep(1ms) → budget abort
  fake_update_manager_.SimulateSlowFrame();
  EXPECT_GT(overlay_.abort_count(), 0u);
  EXPECT_NE(overlay_.last_abort_reason().find("budget"), std::string::npos);
}

TEST_F(PerfOverlayTest, BudgetWithinLimitNoAbort) {
  overlay_.set_budget_us(10000);  // 10 ms ample
  overlay_.Attach(&fake_update_manager_);
  fake_update_manager_.SimulateNormalFrame();
  EXPECT_EQ(overlay_.abort_count(), 0u);
}
```

#### Step 2-3 — GREEN 实施

`perf_overlay.h` 扩展：

```cpp
class PerfOverlay {
 public:
  // ... 既有 ...

  // B.1.2 — Attach to UpdateManager + install PipelineHooks。
  // 单 instance 验证（T6）：第二次 Attach 同 UpdateManager 拒绝。
  // 返回 false + last_attach_error() 设值 当：
  //   - update_manager == nullptr
  //   - 已 attached（任何 UpdateManager — 强单例）
  bool Attach(UpdateManager* update_manager);
  void Detach();
  bool attached() const { return attached_to_ != nullptr; }

  // T6 budget guard
  void set_budget_us(u64 budget_us) { budget_us_ = budget_us; }
  u64 budget_us() const { return budget_us_; }
  usize abort_count() const { return abort_count_; }
  const std::string& last_attach_error() const { return last_attach_error_; }
  const std::string& last_abort_reason() const { return last_abort_reason_; }

 private:
  // 5 个 PipelineHooks callback（C 函数指针 → 通过 userdata recover this）
  static void OnFrameStartTrampoline(void* userdata);
  static void OnAfterStyleTrampoline(void* userdata);
  static void OnAfterLayoutTrampoline(void* userdata);
  static void OnAfterRenderTrampoline(void* userdata);
  static void OnFrameEndTrampoline(void* userdata);

  void HandleHookFire(const char* hook_name);  // budget check + 时间记录

  UpdateManager* attached_to_ = nullptr;
  PipelineHooks hooks_storage_{};
  u64 budget_us_ = 1000;  // 1 ms default (T6 spec §7)
  usize abort_count_ = 0;
  std::string last_attach_error_;
  std::string last_abort_reason_;

  // 时间记录（5 钩子各一）— 用于 FrameStats 计算
  css::SteadyTimePoint t_frame_start_{};
  css::SteadyTimePoint t_after_style_{};
  // ...
};
```

`perf_overlay.cc` 实施 5 trampoline + Attach/Detach + budget abort 逻辑（详细代码 ~70-90 LOC）。budget 计时用 `auto t0 = SteadyClock::now(); cb(); auto dt = duration_cast<microseconds>(SteadyClock::now() - t0).count(); if (dt > budget_us_) { abort_count_++; last_abort_reason_ = "...budget exceeded..."; }`

#### Step 4-5 — GREEN 验证 + 反向探针

把 budget check `if (dt > budget_us_)` 改为 `false` → BudgetAbortTriggersOnSlowCallback 精准 FAIL（abort_count = 0）✅

#### Step 6 — A14 守门 + Commit

```
feat(devtool): PerfOverlay::Attach + T6 budget abort + 单 instance 验证 (B.1.2)

T6 mitigation：1 ms/frame callback 预算（默认）+ 超时 abort + 错误日志；
强单 instance（第 2 次 Attach 拒绝）。5 PipelineHooks trampoline +
SteadyClock 时间差计算 → FrameStats 自动填充。
```

---

### B.2.1 — DevTool Document HUD overlay HTML/CSS（absolute positioned + opacity 0.85）

**估时：** plan 45 min / plan ×0.6 27 min
**威胁面：** —
**验收：** A6 + A7

#### Step 1-3 — RED + GREEN

修改 `veloxa/devtool/resources/inspector_panel.html` — 在 `<div id="devtool-root">` 之外（兄弟节点）加入：

```html
<div id="devtool-hud" data-passthrough="1">
  <div class="hud-row hud-fps">
    <span class="hud-label">FPS:</span>
    <span class="hud-value" id="hud-fps">--</span>
  </div>
  <div class="hud-row hud-stages">
    <div class="hud-bar"><span>S</span><div class="bar-fill" id="bar-style"></div></div>
    <div class="hud-bar"><span>L</span><div class="bar-fill" id="bar-layout"></div></div>
    <div class="hud-bar"><span>R</span><div class="bar-fill" id="bar-render"></div></div>
    <div class="hud-bar"><span>P</span><div class="bar-fill" id="bar-paint"></div></div>
  </div>
</div>
```

修改 `veloxa/devtool/resources/inspector_panel.css` 末尾追加 HUD CSS 块：

```css
/* B.2.1 HUD overlay (creative #1 决策 3) — DevTool Document 内 absolute
   positioned；creative 决策写 fixed 但 layout fallback 同 absolute — 视觉等价。
   pointer-events: none 不支持 → data-passthrough 兜底（hit_test_skip） */
#devtool-hud {
  position: absolute;
  top: 8px;
  left: 8px;
  width: 180px;
  background-color: #1a1a1a;
  color: #00ff00;
  padding: 6px 8px;
  border-radius: 4px;
  font-family: monospace;
  font-size: 11px;
  opacity: 0.85;
}

.hud-row {
  display: flex;
  flex-direction: row;
  margin-bottom: 4px;
}

.hud-bar {
  display: flex;
  flex-direction: row;
  margin-right: 6px;
  width: 28px;
}

.hud-bar .bar-fill {
  height: 8px;
  background-color: #00aa00;
  /* width JS 设置（按 stage_ms / total_ms 比例）*/
}
```

新增 `tests/devtool/resources/inspector_panel_smoke_test.cc` 末尾追加：

```cpp
TEST_F(InspectorPanelSmokeTest, HudHtmlContainsDevtoolHudId) {
  EXPECT_TRUE(StringContains(devtool::resources::kInspectorPanelHtml,
                              "id=\"devtool-hud\""));
}

TEST_F(InspectorPanelSmokeTest, HudHtmlContainsFpsBars) {
  EXPECT_TRUE(StringContains(devtool::resources::kInspectorPanelHtml,
                              "id=\"hud-fps\""));
  for (auto* bar : {"bar-style", "bar-layout", "bar-render", "bar-paint"}) {
    EXPECT_TRUE(StringContains(devtool::resources::kInspectorPanelHtml, bar));
  }
}

TEST_F(InspectorPanelSmokeTest, HudCssAvoidsUnsupportedShorthand) {
  // R2 mitigation guard — 与 A.1.3 既有 Avoids* 测同模式
  std::string css = StripCssComments(devtool::resources::kInspectorPanelCss);
  EXPECT_EQ(css.find("position: fixed"), std::string::npos);  // 用 absolute 不用 fixed
  EXPECT_EQ(css.find("pointer-events"), std::string::npos);
}
```

#### Step 4-5 — GREEN 验证 + 反向探针

把 HTML 中 `id="devtool-hud"` 改为 `id="other-hud-PROBE"` → `HudHtmlContainsDevtoolHudId` 精准 FAIL ✅

#### Commit

```
feat(devtool): DevTool HUD overlay HTML/CSS — absolute positioned + opacity (B.2.1)

inspector_panel.html 加 #devtool-hud 子树（兄弟于 #devtool-root）含 FPS row +
4 stage bars；inspector_panel.css 加 HUD 样式（absolute / opacity 0.85 / R2-
verified CSS 子集）；3 smoke 测覆盖（id 存在 + bars 命名 + Avoids unsupported）。
data-passthrough="1" attribute 占位（HitTest 兜底待 B.3.1 落地）。
```

---

### B.2.2 — HUD JS 每帧读 perf_stats + 更新 DOM（含 `vx_view_get_perf_stats` C API + JS native binding）

**估时：** plan 30 min / plan ×0.6 18 min
**威胁面：** —
**验收：** A6 + A7

#### Step 1-3 — RED + GREEN

`veloxa/devtool/resources/inspector_panel.js` 末尾追加 HUD update loop：

```js
// B.2.2 HUD update loop — 每帧读 perf_stats + 更新 DOM
function updateHud() {
  try {
    if (typeof vx_view_get_perf_stats !== "function") return;
    const stats = vx_view_get_perf_stats();  // {fps, style, layout, render, paint, total}
    if (!stats) return;
    const fpsEl = document.getElementById("hud-fps");
    if (fpsEl) fpsEl.innerHTML = stats.fps.toFixed(1);
    // bars: width 比例 = stage_ms / max(stage_ms_array)
    const max = Math.max(stats.style, stats.layout, stats.render, stats.paint, 0.001);
    for (const [stage, id] of [["style", "bar-style"], ["layout", "bar-layout"],
                                ["render", "bar-render"], ["paint", "bar-paint"]]) {
      const bar = document.getElementById(id);
      if (bar) bar.style.width = (24 * stats[stage] / max).toFixed(0) + "px";
    }
  } catch (e) { /* R2 防御 */ }
}

// 由 PerfOverlay::Attach 在 OnFrameEnd 钩子触发（C++ 侧调 JS_Eval("updateHud()")）
// 或者通过 setInterval(updateHud, 16)（fallback if hook integration incomplete）
```

`veloxa/script/dom_bindings.cc`（沿用 A.1.4 范式）加 `vx_view_get_perf_stats()` 全局函数 — 注册到 DevTool ctx：

```cpp
static JSValue VxViewGetPerfStats(JSContext* ctx, JSValueConst /*this_val*/,
                                   int argc, JSValueConst* /*argv*/) {
  if (argc != 0) return JS_ThrowTypeError(ctx, "expected 0 args");
  auto* bindings = static_cast<DomBindings*>(JS_GetContextOpaque(ctx));
  if (!bindings || !bindings->perf_overlay()) return JS_NULL;
  auto agg = bindings->perf_overlay()->aggregated();
  f32 fps = bindings->perf_overlay()->fps();
  // 构造 JS object {fps, style, layout, render, paint, total}
  JSValue obj = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, obj, "fps",    JS_NewFloat64(ctx, fps));
  JS_SetPropertyStr(ctx, obj, "style",  JS_NewFloat64(ctx, agg.style_ms));
  JS_SetPropertyStr(ctx, obj, "layout", JS_NewFloat64(ctx, agg.layout_ms));
  JS_SetPropertyStr(ctx, obj, "render", JS_NewFloat64(ctx, agg.render_ms));
  JS_SetPropertyStr(ctx, obj, "paint",  JS_NewFloat64(ctx, agg.paint_ms));
  JS_SetPropertyStr(ctx, obj, "total",  JS_NewFloat64(ctx, agg.total_ms));
  return obj;
}

// RegisterDevtoolBindings 末尾追加：
JS_SetPropertyStr(ctx, global, "vx_view_get_perf_stats",
                  JS_NewCFunction(ctx, VxViewGetPerfStats,
                                   "vx_view_get_perf_stats", 0));
```

`DomBindings::InstanceData` 加 `PerfOverlay* perf_overlay = nullptr;` + `SetPerfOverlay(PerfOverlay*)` setter；Application::LoadDevtoolDocument 在 attach 路径调 `devtool_dom_bindings_->SetPerfOverlay(&perf_overlay_);`

测：`tests/devtool/overlay/perf_hud_smoke_test.cc` 新建（集成测 — 用 dogfood 路径）— 调 `EvalDevtoolScript("typeof vx_view_get_perf_stats === 'function'")` 等。

#### Step 4-5 + Commit

```
feat(devtool): vx_view_get_perf_stats JS native binding + HUD update loop (B.2.2)

DomBindings 加 PerfOverlay 字段 + vx_view_get_perf_stats 全局 JS 函数（返回
{fps, style, layout, render, paint, total} 对象）；inspector_panel.js 加
updateHud() — 读 perf_stats + 更新 #hud-fps innerHTML + 4 bars width；
panel-side defensive 防御 R2 缺陷（typeof check + try/catch）。
```

---

### B.2.3 — dirty rect 边框高亮渲染（复用 kOverlayHighlight + 新工厂 OverlayDirtyRect）

**估时：** plan 30 min / plan ×0.6 18 min
**威胁面：** **T5** ✅（复用 ResetOverlayCommands 协议）
**验收：** A8

#### Step 1-3 — RED + GREEN

`veloxa/core/render/paint_command.h` 加新工厂（不新增 enum）：

```cpp
struct PaintCommand {
  // ... 既有 ...

  // B.2.3 (TASK-20260502-02) — Performance Overlay dirty rect 边框（红色 1px stroke）
  // 复用 kOverlayHighlight enum（语义在工厂签名层面区分）+ T5 ResetOverlayCommands 自动覆盖。
  static PaintCommand OverlayDirtyRect(const gfx::Rect& r,
                                        gfx::Color c = gfx::Color::FromRGBA(255, 100, 100, 153),
                                        f32 stroke_width = 1.0f) {
    return {Type::kOverlayHighlight, r, c, stroke_width, 0, {}, 0};
  }
};
```

`veloxa/devtool/inspector/inspector_overlay.h` 加新方法：

```cpp
class InspectorOverlay {
 public:
  // ... 既有 InjectHoverHighlight ...

  // B.2.3 — Performance Overlay dirty rect 边框注入（每帧）。复用 OverlayHighlight
  // 渲染范式 + ResetOverlayCommands 协议（T5）。
  static void InjectDirtyRectHighlights(render::DisplayList& list,
                                         const Vector<gfx::Rect>& dirty_rects);
};
```

`veloxa/devtool/inspector/inspector_overlay.cc` 实施：

```cpp
void InspectorOverlay::InjectDirtyRectHighlights(
    render::DisplayList& list, const Vector<gfx::Rect>& dirty_rects) {
  for (const auto& r : dirty_rects) {
    if (r.IsEmpty()) continue;
    list.push_back(render::PaintCommand::OverlayDirtyRect(r));
  }
}
```

测加入 `tests/devtool/inspector/inspector_overlay_test.cc`：

```cpp
TEST(InspectorOverlayDirtyRect, EmptyVectorNoOp) {
  render::DisplayList list;
  InspectorOverlay::InjectDirtyRectHighlights(list, {});
  EXPECT_EQ(list.size(), 0u);
}

TEST(InspectorOverlayDirtyRect, ThreeRectsProduceThreeOverlayCommands) {
  render::DisplayList list;
  Vector<gfx::Rect> rects;
  rects.push_back({0, 0, 100, 100});
  rects.push_back({200, 0, 100, 100});
  rects.push_back({0, 200, 100, 100});
  InspectorOverlay::InjectDirtyRectHighlights(list, rects);
  EXPECT_EQ(list.size(), 3u);
  for (const auto& cmd : list) {
    EXPECT_EQ(cmd.type, render::PaintCommand::Type::kOverlayHighlight);
  }
}

TEST(InspectorOverlayDirtyRect, EmptyRectsSkipped) {
  render::DisplayList list;
  Vector<gfx::Rect> rects;
  rects.push_back({0, 0, 0, 0});      // empty
  rects.push_back({0, 0, 100, 100});  // non-empty
  InspectorOverlay::InjectDirtyRectHighlights(list, rects);
  EXPECT_EQ(list.size(), 1u);
}

TEST(InspectorOverlayDirtyRect, ResetOverlayCommandsClearsDirtyRectHighlights) {
  // T5 验证 — dirty rect overlay 与 hover highlight 同 enum，统一被 reset
  render::DisplayList list;
  Vector<gfx::Rect> rects = {{0, 0, 100, 100}};
  InspectorOverlay::InjectDirtyRectHighlights(list, rects);
  EXPECT_EQ(list.size(), 1u);
  render::ResetOverlayCommands(list);
  EXPECT_EQ(list.size(), 0u);  // T5 协议保证
}
```

#### Step 4-5 + Commit

```
feat(devtool): InspectorOverlay::InjectDirtyRectHighlights + OverlayDirtyRect 工厂 (B.2.3)

复用 PaintCommand::kOverlayHighlight enum（不新增）+ 新 OverlayDirtyRect
工厂封装（红色 1px stroke）+ T5 ResetOverlayCommands 协议自动覆盖。
4 单测含 T5 隔离验证（与 hover highlight 同 enum 统一被 reset）。
```

---

### B.3.1 — F11 toggle HUD（仅 DevTool 已开启时）

**估时：** plan 15 min / plan ×0.6 9 min
**威胁面：** —
**验收：** A6（toggle 协议）

#### Step 1-3 — RED + GREEN

`veloxa/api/veloxa_api.h` 加 `#define VX_KEY_F11 0x40000044u`（SDLK_F11 标准值）。

`veloxa/platform/sdl2/sdl2_input_translate.cc` 加 `case SDLK_F11: return VX_KEY_F11;`。

`veloxa/core/application.cc` `MaybeHandleDevtoolHotkey` 扩展：

```cpp
bool Application::MaybeHandleDevtoolHotkey(const event::InputEvent& input) {
  if (!devtool_hotkey_enabled_ || input.type != event::InputEventType::kKeyDown)
    return false;
  if (input.key_code == kVxKeyF12) {
    // ... 既有 F12 toggle ...
    return true;
  }
  // B.3.1 — F11 toggle HUD（仅 DevTool 已开启时；否则 no-op 不消费）
  if (input.key_code == kVxKeyF11 && devtool_loaded()) {
    EvalDevtoolScript("(function(){var h=document.getElementById('devtool-hud');"
                      "if(h)h.style.display=h.style.display==='none'?'block':'none';})()",
                      "f11_toggle_hud");
    return true;  // 消费事件
  }
  return false;  // F11 在 DevTool 未开启时转发 event_manager_
}
```

加 const `static const u32 kVxKeyF11 = 0x40000044u;` 在 anon namespace。

测加入 `tests/api/devtool_attach_api_test.cc`：

```cpp
TEST_F(DevtoolAttachApiTest, F11WithDevtoolLoadedConsumesEvent) {
  vx_view_attach_devtool(view_, &default_opts_);
  // 模拟 F11 KeyDown
  VxInputEvent ev{};
  ev.type = VX_INPUT_KEYDOWN;
  ev.key_code = VX_KEY_F11;
  EXPECT_EQ(vx_view_inject_input(view_, &ev), VX_OK);
  // 验证 F11 被消费（target Document event_manager_ 不应看到 F11）
  // ... 通过 dom JSON 检查 #devtool-hud display 状态 ...
}

TEST_F(DevtoolAttachApiTest, F11WithoutDevtoolDoesNotConsumeEvent) {
  // 不调 attach
  // F11 应该被转发 event_manager_（视为普通 key）
  // ...
}
```

#### Step 4-5 + Commit

```
feat(devtool): F11 toggle HUD overlay (creative #1 决策 5) (B.3.1)

VX_KEY_F11 = 0x40000044 (SDLK_F11) + Application::MaybeHandleDevtoolHotkey
扩展 + EvalDevtoolScript 切换 #devtool-hud display:none/block；F11 仅在
devtool_loaded() 时消费事件（否则转发 event_manager_）。复用 Phase A
F12 hotkey 范式 + anon namespace kVxKeyF11 常量解耦头依赖。
```

---

### B.3.2 — examples/hello_devtool.cc — Overlay smoke 扩展

**估时：** plan 30 min / plan ×0.6 18 min
**威胁面：** —
**验收：** A6 + A7 + A8

#### Step 1-3 — RED + GREEN

修改 `examples/hello_devtool.cc`，在 attach 之后加：

```cpp
// B.3.2 — Performance Overlay smoke 扩展
if (vx_view_devtool_loaded(view) == 1) {
  // 触发几帧 update（headless smoke 需主动 invalidate）
  for (int i = 0; i < 5; i++) { vx_view_run_one_iteration(view); }
  // 调 EvalDevtoolScript 验证 vx_view_get_perf_stats() 返回非 null
  /* ...  */
  std::printf("Phase B HUD smoke OK: PerfOverlay attached + perf stats accessible.\n");
}
```

`tests/CMakeLists.txt` 加 `hello_devtool_perf_smoke` ctest（复用 hello_devtool 例子，环境变量 `VX_HELLO_DEVTOOL_PERF_SMOKE=1` 触发新行为）。

#### Step 4-5 + Commit

```
test(examples): hello_devtool 扩展 Performance Overlay smoke (B.3.2)

attach 后跑 5 帧 + EvalDevtoolScript 验证 perf stats 可访问；新加
hello_devtool_perf_smoke ctest（VX_HELLO_DEVTOOL_PERF_SMOKE=1 触发）。
```

---

### B.3.3 — Phase B integration verify + reflect prep + A14 smoke 黑名单更新

**估时：** plan 30 min / plan ×0.6 18 min
**威胁面：** —
**验收：** A6-A9 + A14

#### Step 1-3

- 全 Phase B 16/16 ctest verify（DEVTOOL=ON 1169+~17 = ~1186 / DEVTOOL=OFF 1065+~3 = ~1068 PASS）
- `tests/smoke/devtool_a14_link_closure.cmake` 黑名单加 `PerfOverlay` / `OverlayDirtyRect` / `InjectDirtyRectHighlights` / `RegisterPerfStatsBindings` / `kVxKeyF11` 等内部符号
- progress.md 更新「Phase B 完成快照」段
- activeContext.md 切换为「构建中·已完成」状态等待 `/reflect`

#### Commit

```
chore(build): finalize TASK-20260502-02 round N — Phase B 10/10 complete

ctest DEVTOOL=ON 1169 → ~1186 PASS / DEVTOOL=OFF 1065 → ~1068 PASS。
A14 smoke 黑名单追加 5 PerfOverlay 内部符号；progress.md 更新 Phase B
完成快照；准备进入 /reflect。
```

---

## 3. Phase B 总估时矩阵

| 子任务 | 估时 plan | plan ×0.6 |
|:-:|---:|---:|
| B.0.1 PipelineHooks 五钩子 + C API | 90 min | 54 min |
| B.0.2 dirty_rects_ Vector 扩展 | 60 min | 36 min |
| B.1.1 PerfOverlay ring buffer + 60 帧聚合 | 60 min | 36 min |
| B.1.2 T6 budget abort + 单 instance | 45 min | 27 min |
| B.2.1 HUD HTML/CSS（absolute + opacity）| 45 min | 27 min |
| B.2.2 HUD JS + vx_view_get_perf_stats binding | 30 min | 18 min |
| B.2.3 dirty rect 边框 OverlayDirtyRect 工厂 | 30 min | 18 min |
| B.3.1 F11 toggle HUD | 15 min | 9 min |
| B.3.2 hello_devtool perf smoke | 30 min | 18 min |
| B.3.3 Phase B finalize + A14 smoke 更新 | 30 min | 18 min |
| **合计** | **435 min（7.25 h）** | **261 min（4.35 h）— 蓝图基线** |

**plan ×0.6 第 38 数据点假设（reflect 阶段沉淀）：** 蓝图 4.35 h → archive 校准 ~3.5-5 h → VAN F2 发现 ~3-4 h → 最终预测 **~3-4 h（实际 0.55-0.75× plan ×0.6）**。验证时分类落「大件实现」桶 0.6-1.1× 中段 vs「极窄档延续」可能。

---

## 4. R2-R4 风险登记 + mitigation

| # | 风险 | 严重度 | mitigation | 落地 |
|:-:|---|:-:|---|---|
| **R3** | UpdateManager 五钩子重构影响目标 View 性能（spec §9）| 🟡 中 | function pointer + nullptr branch predictor + nullptr 路径单测验证 layout_root + display_list 既有契约（B.0.1 Step 1 RED 含 NullHooksUpdateRemainsLossless 测）| B.0.1 |
| **R7** | dirty_rects Vector 累积无 cap → 内存退化 | 🟢 低 | 当前每帧 PerfOverlay::Attach OnFrameStart 调 ClearDirtyRects()；如 PerfOverlay detach 后忘清 → 单测 `MultipleInvalidateAccumulates` 仅模拟 3 次（实际不 unbounded grow）| B.0.2 |
| **R8** | HUD `position: fixed` fallback 视觉差异 | 🟢 低 | Phase 0 grep 实证 fixed 当前等价 absolute（HUD 在 DevTool Document root 下 absolute 视觉等价）；spec creative #1 决策 3 已 documented | B.2.1 |
| **R9** | `pointer-events: none` 不支持 → HUD 拦截鼠标 | 🟡 中 | data-passthrough="1" attribute 兜底（HitTest 逻辑跳过 — 留 R3+ 任务 EventManager HitTest 改造）；HUD 区域 ~180×90 px 影响有限 | B.2.1 + R3+ 候选 |

---

## 5. Checkpoint

- **Checkpoint 1**：B.0.x 完成（PipelineHooks + dirty_rects 扩展）— 用户审 ctest baseline 增量 + A14 守门；默认协议 = 隐式批准继续 B.1.x
- **Checkpoint 2**：B.1.x 完成（PerfOverlay 内核 + T6 budget）— 用户审 perf_overlay 独立单元测覆盖率；默认协议 = 隐式批准继续 B.2.x
- **Checkpoint 3**：B.2.x 完成（HUD UI + dirty rect 边框）— 用户审 HUD smoke 视觉效果（如有真窗口环境）；默认协议 = 隐式批准继续 B.3.x

---

## 6. 与既有 systemPatterns 协同度自我对照（≥ 5 条关键模式验证）

| 既有模式 | 本任务实践 | 协同度 |
|---|---|:-:|
| 中央调度协议（UpdateManager）| B.0.1 PipelineHooks 是该模式自然扩展 | ✅✅ |
| 函数指针 + nullptr 分支预测优化 | B.0.1 5 个 hook nullptr 默认值 + branch predictor friendly fire helper | ✅✅ |
| 双层 API（D7=C 内部 + 公开）| B.0.1 / B.2.2 都遵循（C++ PerfOverlay 内部 + vx_view_set_pipeline_hooks / vx_view_get_perf_stats 公开）| ✅✅ |
| #ifdef + CMake 双层 conditional 子系统 | B.0.1 公开 stub + B.1/B.2 vx_devtool 内 + CMake `if(VX_BUILD_DEVTOOL)` link | ✅✅ |
| dogfood 路径 = 探测性 acceptance test | B.2.1 HUD HTML/CSS 用 R2-verified CSS 子集（继续验证 CSS 引擎在 absolute / opacity 真实场景）| ✅✅ |
| 子系统关闭守门 ctest smoke 范式 | B.3.3 A14 smoke 黑名单追加 5 PerfOverlay 内部符号 | ✅✅ |
| 反向探针有效性 | 每子任务 Step 5 反向探针「修改 string literal / 参数 / 边界」（不用 empty body 避免 -Werror=unused-function 陷阱）| ✅ |
| StatusOr / Status 三元守卫 | 不涉及（PerfOverlay 不返 StatusOr）| ⊘ |
| 渐进式文档（plan 复杂度匹配任务复杂度）| Level 3 单 plan 文档 + 10 子任务 5-步 TDD 模板 + 完整代码片段（与 Phase A 范式一致）| ✅✅ |

---

## 7. 与 reflect 阶段联动准备

- **plan ×0.6 第 38 数据点入库**（reflect 必填）：实测累计后填入 systemPatterns plan ×0.6 矩阵
- **5 大可复用架构范式实证更新**（archive 阶段交叉记录）：本任务实证 B.0.1 五钩子是「中央调度协议」自然扩展 + B.2.x HUD 是「dogfood 路径」延续
- **R7-R9 风险闭环**：reflect 阶段验证每条风险的实际命中情况
- **#35 阶段 1 闭环 / 阶段 2 留 P3**：reflect 阶段记录拆分 LayoutEngine 内 style/layout 子阶段为 P3 候选（精化 OnAfterStyle 与 OnAfterLayout 时间差）

---

**plan 落盘完成。下一步：`/build` 启动 Phase 0.1 reconfigure 二次验证 + B.0.1 PipelineHooks 五钩子（最大子任务）。**
