# Performance Overlay 持续 invalidate 机制 — `vx_view_invalidate()` 公开 C ABI 设计规格

**任务 ID：** TASK-20260505-02
**复杂度级别：** Level 2
**日期：** 2026-05-05
**作者：** Loong Veloxa AI Workflow
**关联：** [TASK-20260504-01 MVP-scope spec §3.2.1 B-G4](../../docs/specs/2026-05-04-mvp-scope.md) + [TASK-20260503-03 archive §9 P3 候选 #0](../../memory-bank/archive/archive-TASK-20260503-03.md) + [TASK-20260502-02 archive Phase B Performance Overlay](../../memory-bank/archive/archive-TASK-20260502-02.md)
**安全相关：** ❌ 否

---

## 1. 背景与问题陈述

### 1.1 历史背景

- **TASK-20260502-02 Phase B Performance Overlay 实施时**（2026-05-03）— 5 hooks ABI 落地 / `hello_devtool_perf_smoke` ctest 在 dummy SDL + 静态 CSS 场景**仅 frames=1**（被 dirty_ guard 永久阻断）/ ctest 当时被迫降级到 `PASS_REGULAR_EXPRESSION "PERF SMOKE: frames=[1-9][0-9]* hud_visible=1"`（接受 frames=1 ABI smoke 语义 / 不验证多帧 ring buffer 聚合）
- **TASK-20260503-03 主线收官**（2026-05-03）— 试图直接调长 `VX_HELLO_DEVTOOL_AUTOQUIT_MS` 强制多帧但失败 → 反复模式 #1 第 9 次命中（VAN/plan 阶段未深读 `update_manager.dirty_` 机制）→ 用户决策 `p1_fix=A` 完整回退 + 拆细化为 P3 候选 #0
- **TASK-20260505-01 DomBindings R2 收口**（2026-05-05 上午）— MVP-B 90% → 95% / B-G1+G2+G3 闭环 / 仅剩 B-G4 / spec §11.2 推荐立项 #1（已闭环）+ #2（本任务）

### 1.2 真实根因（VAN Phase 0 audit 锁定）

`UpdateManager::Update()` 第 17 行 `if (!dirty_) return;` 是 frame hooks 触发硬约束：

```cpp
void UpdateManager::Update() {
  if (!dirty_ || !config_.document) return;  // ← 短路守护
  // ... 5 hooks fire here ...
  dirty_ = false;                              // 帧末 reset
  if (transition_mgr_.HasActive()) {
    dirty_ = true;                             // 仅 transition 时 rearm
  }
  if (pipeline_hooks_->on_frame_end) {
    pipeline_hooks_->on_frame_end(...);        // ← 第 5 个 hook（rearm 后）
  }
}
```

**dirty_ rearm 4 个已知 invalidate 源**（详 `memory-bank/techContext.md` ≈L1060-1067）：

| # | 源 | 状态 | dogfood smoke 适用 |
|:-:|---|:-:|:-:|
| 1 | 用户输入 (`Application::InjectInput`) | ✅ 引擎已支持 | ⚠️ 需周期模拟 |
| 2 | active CSS transition (`transition_mgr_.HasActive`) | ✅ 引擎已支持 | ⚠️ 需 DOM 注入 transition + 周期属性翻转 |
| 3 | CSS animation (`@keyframes`) | ❌ **引擎不支持**（grep `veloxa/` 0 命中）| ❌ 不可行 |
| 4 | 公开 `vx_view_invalidate()` API | ❌ **未实现** ← 本任务范围 | ✅ on_frame_end hook 调一行 |

### 1.3 用户场景

**embedder 通用需求：**

1. **Profiler / tracing embedder** — 已用 `vx_view_set_pipeline_hooks` 安装 hooks / 需要在 hook 回调中**主动 force 下一帧**采集多帧数据
2. **持续动画/视频流场景** — embedder 自维护 dirty 状态 / 需要主动 invalidate 触发渲染
3. **DevTool dogfood smoke** — `hello_devtool_perf_smoke` 升级到真多帧验证（本任务次要交付物）

**当前缺口：** `UpdateManager::Invalidate()` 已实现（update_manager.cc:14 `dirty_ = true;`），但**无公开 C ABI 暴露**。embedder 必须 hack 通过：

- 周期 `vx_view_inject_input(MOUSE_MOVE)`（污染输入流 + 不直观）
- 调 `vx_view_load_css()` 重新触发 EnsureUpdateManager（成本高 + 副作用）

---

## 2. 设计目标

| # | 目标 | 验证方式 |
|:-:|---|---|
| **G1** | 公开 `vx_view_invalidate(VxView*)` C ABI / Doxygen 文档清晰 / 与既有 ABI 风格一致 | API 单测 4 个 + Doxygen 评审 |
| **G2** | 内部实现 = 转发到 `Application::Invalidate()` → `update_manager_->Invalidate()`（既有 1 行 `dirty_ = true;`）| 无 ASan / 无 thread sanitizer 警告 |
| **G3** | hello_devtool 在 `on_frame_end` hook 中调用 → `hello_devtool_perf_smoke` ctest 升级到真多帧（≥2 frames）验证 | ctest `PASS_REGULAR_EXPRESSION` 升级 + 实测 PASS |
| **G4** | A14 守门通过（vx_view_invalidate 是公开 API / 不属 DevTool subsystem）| ctest devtool_a14_link_closure ON+OFF PASS |
| **G5** | spec §3.2.1 B-G4 状态从 ⚠️ 部分 → ✅ 闭环 / MVP-B 完成度 95% → 100% | spec 文档更新 |

---

## 3. 设计决策矩阵（4/4 锁定 / 跨决策协同度 100% 第 10 次命中 / dec-evidence 升级）

### 3.1 D1: 双 update_manager 路由 → **D1-A 仅 target update_manager_**

**理由：**
- VxView 是单一 view 抽象 / embedder 视角下「我的应用」即 target document
- DevTool 作为调试 UI 是**独立状态机**，不应受 embedder 主动控制（避免 fps 计数干扰 + 架构隔离）
- 实现最简 — 1 行 `app->update_manager()->Invalidate()`（仅缺非 const 访问，详 §4.2）

**实现路径：** 在 `Application` 增加 `void Invalidate();` 公开方法 / `vx_view_invalidate()` C ABI 调用 `app->Invalidate()`

### 3.2 D2: 线程安全语义 → **D2-A main thread only**

**理由：**
- 与 Veloxa 现有 thread model 一致 — `LoadHTML/LoadCSS/InjectInput/Update/SetPipelineHooks` 全部要求 main thread
- `UpdateManager::Invalidate()` 实现 = `dirty_ = true;`（非 atomic / 单赋值），跨线程语义未定义
- atomic 改造（D2-B）需要修改 `update_manager.h` `dirty_` 类型为 `std::atomic<bool>`，影响 `Update()` 双 reads + 1 write 路径，超出本任务范围 + 低 ROI（无真实多线程 embedder 需求）

**Doxygen 文档明示：** `Thread-safety: main thread only (consistent with vx_view_load_*, vx_view_inject_input).`

### 3.3 D3: hello_devtool 注入位置 → **D3-A on_frame_end hook**

**理由：**
- 利用既有 `perf_hooks.on_frame_end` 范式 / userdata 通道传 VxView* / 0 新机制
- on_frame_end hook 触发时序：`dirty_=false` reset 后 + `transition_mgr_.HasActive()` rearm 后 → 在 hook 内 `vx_view_invalidate(view)` 强制再次 rearm 用于下一帧 → 严格幂等 + 安全（详 §6.2 数据流）
- 实现最简（≤2 行改动）

**注入示例：**

```cpp
// hello_devtool.cc Phase DOG.1
struct PerfSmokeUd { int frames; VxView* view; };
static PerfSmokeUd s_perf_ud{0, nullptr};
perf_hooks.on_frame_end = [](void* ud) {
  auto* state = static_cast<PerfSmokeUd*>(ud);
  if (state) {
    state->frames++;
    vx_view_invalidate(state->view);  // ← 本任务新增
  }
};
s_perf_ud.view = view;
vx_view_set_pipeline_hooks(view, &perf_hooks, &s_perf_ud);
```

### 3.4 D4: API 单测覆盖范围 → **D4-A 完整 4 单测**

| # | 单测 | 覆盖路径 | 反向探针目标 |
|:-:|---|---|---|
| 1 | `InvalidateNullViewReturnsNullParam` | NULL view → VX_ERROR_NULL_PARAM | 删除 `if (!view)` 守门 → 应该 segfault |
| 2 | `InvalidateFreshViewReturnsInvalidState` | 未 LoadHTML 的 view → VX_ERROR_INVALID_STATE（update_manager_ 为 null）| 删除 `if (!app->update_manager()) return INVALID_STATE` → 应该 segfault |
| 3 | `InvalidateMakesNextUpdateRunHooks` | LoadHTML → Update → 再 Update（无变化 → hooks 不触发）→ Invalidate → Update（hooks 触发）| 注释 `Invalidate()` 内 `dirty_ = true;` → hook 计数应不增 |
| 4 | `InvalidateIdempotent` | 连续 2 次 Invalidate → next Update 仅触发 1 次 hooks | 验证 `dirty_=true` 单赋值幂等 |

**ctest 期望：** DEVTOOL=ON 1298 → 1302（+4 PASS）/ DEVTOOL=OFF 1105 → 1109（+4 PASS）

---

## 4. 详细架构设计

### 4.1 头文件变更（`veloxa/api/veloxa_api.h`）

在「View」段（≈L130）`vx_view_run` 之后插入：

```c
/* Force the next vx_view_update / vx_view_run frame to do a full
 * style-resolve-layout-render pass even when nothing else triggered an
 * invalidation. Useful for embedders that:
 *   - install pipeline hooks (vx_view_set_pipeline_hooks) and need to
 *     force per-frame work for profiling / tracing,
 *   - drive an animation timeline outside the engine and want to keep
 *     the view repainting every frame,
 *   - run smoke tests that need to validate multi-frame behavior in
 *     a static-DOM scenario.
 *
 * Internally this calls UpdateManager::Invalidate() (sets dirty_=true
 * for the next frame). Routing target: target Document only — DevTool
 * Document update is governed by its own state machine and is NOT
 * affected by this call.
 *
 * Thread-safety: main thread only (consistent with vx_view_load_*,
 * vx_view_inject_input).
 *
 * Idempotent — calling N times before the next Update is equivalent to
 * a single call (single dirty_=true assignment).
 *
 * Returns:
 *   VX_OK on success.
 *   VX_ERROR_NULL_PARAM when view is NULL.
 *   VX_ERROR_INVALID_STATE when no Document has been loaded yet
 *     (vx_view_load_html has not been called) — the call is a no-op
 *     and the next vx_view_update will still no-op the same way.
 */
VxResult vx_view_invalidate(VxView* view);
```

### 4.2 实现变更

#### 4.2.1 `veloxa/core/application.h` — 增加 `Invalidate` 公开方法

在「Update」段（≈L60）`Update()` 声明之后插入：

```cpp
// TASK-20260505-02 — Force next Update() to run a full pass even when
// dirty_ would have short-circuited. Returns false when update_manager_
// is not yet initialized (no LoadHTML/LoadCSS run); the call is a
// silent no-op in that case (consistent with SetPipelineHooks lazy
// attach pattern). Routing target: target update_manager_ only —
// DevTool's UpdateManager runs an independent state machine.
//
// Thread-safety: main thread only (consistent with LoadHTML/LoadCSS/
// InjectInput).
bool Invalidate();
```

#### 4.2.2 `veloxa/core/application.cc` — 实现 `Application::Invalidate`

在 `Application::Update()` 之前（≈L210）插入：

```cpp
bool Application::Invalidate() {
  if (!update_manager_) return false;
  update_manager_->Invalidate();
  return true;
}
```

#### 4.2.3 `veloxa/api/veloxa_api.cc` — 实现 `vx_view_invalidate`

在 `vx_view_update`（≈L200）之前插入：

```cpp
VxResult vx_view_invalidate(VxView* view) {
  if (!view) return VX_ERROR_NULL_PARAM;
  auto* app = reinterpret_cast<vx::Application*>(view);
  bool ok = app->Invalidate();
  return ok ? VX_OK : VX_ERROR_INVALID_STATE;
}
```

### 4.3 测试变更

#### 4.3.1 新建 `tests/api/invalidate_api_test.cc`

```cpp
#include "veloxa/api/veloxa_api.h"

#include <atomic>
#include <gtest/gtest.h>

namespace {

constexpr uint32_t kW = 200, kH = 200;

class InvalidateApiTest : public ::testing::Test {
 protected:
  void SetUp() override {
    loop_ = vx_event_loop_create_headless();
    surface_ = vx_surface_create_memory(kW, kH);
    VxViewConfig cfg{};
    cfg.event_loop = loop_;
    cfg.surface = surface_;
    cfg.target_fps = 60;
    view_ = vx_view_create(&cfg);
  }
  void TearDown() override {
    if (view_) vx_view_destroy(view_);
    if (surface_) vx_surface_destroy(surface_);
    if (loop_) vx_event_loop_destroy(loop_);
  }
  VxEventLoop* loop_ = nullptr;
  VxSurface* surface_ = nullptr;
  VxView* view_ = nullptr;
};

TEST_F(InvalidateApiTest, NullViewReturnsNullParam) {
  EXPECT_EQ(vx_view_invalidate(nullptr), VX_ERROR_NULL_PARAM);
}

TEST_F(InvalidateApiTest, FreshViewReturnsInvalidState) {
  // No LoadHTML → update_manager_ is null → INVALID_STATE.
  EXPECT_EQ(vx_view_invalidate(view_), VX_ERROR_INVALID_STATE);
}

namespace {
std::atomic<int> g_invalidate_hook_calls{};
void OnFrameEndCounter(void*) { g_invalidate_hook_calls++; }
}  // namespace

TEST_F(InvalidateApiTest, InvalidateMakesNextUpdateRunHooks) {
  // 1. LoadHTML → first Update — hooks not yet installed.
  EXPECT_EQ(vx_view_load_html(view_, "<div></div>", 11), VX_OK);
  EXPECT_EQ(vx_view_update(view_), VX_OK);

  // 2. Install hooks; second Update without dirty_ rearm → hooks NOT fire.
  g_invalidate_hook_calls.store(0);
  VxPipelineHooks hooks{};
  hooks.on_frame_end = &OnFrameEndCounter;
  EXPECT_EQ(vx_view_set_pipeline_hooks(view_, &hooks, nullptr), VX_OK);
  EXPECT_EQ(vx_view_update(view_), VX_OK);
  EXPECT_EQ(g_invalidate_hook_calls.load(), 0);  // dirty_=false short-circuit

  // 3. Invalidate → next Update fires hooks once.
  EXPECT_EQ(vx_view_invalidate(view_), VX_OK);
  EXPECT_EQ(vx_view_update(view_), VX_OK);
  EXPECT_EQ(g_invalidate_hook_calls.load(), 1);

  // 4. Cleanup.
  EXPECT_EQ(vx_view_set_pipeline_hooks(view_, nullptr, nullptr), VX_OK);
}

TEST_F(InvalidateApiTest, InvalidateIdempotent) {
  EXPECT_EQ(vx_view_load_html(view_, "<div></div>", 11), VX_OK);
  EXPECT_EQ(vx_view_update(view_), VX_OK);  // dirty_ now false

  g_invalidate_hook_calls.store(0);
  VxPipelineHooks hooks{};
  hooks.on_frame_end = &OnFrameEndCounter;
  EXPECT_EQ(vx_view_set_pipeline_hooks(view_, &hooks, nullptr), VX_OK);

  // 5 invalidates before next update → still 1 frame fires hooks.
  EXPECT_EQ(vx_view_invalidate(view_), VX_OK);
  EXPECT_EQ(vx_view_invalidate(view_), VX_OK);
  EXPECT_EQ(vx_view_invalidate(view_), VX_OK);
  EXPECT_EQ(vx_view_invalidate(view_), VX_OK);
  EXPECT_EQ(vx_view_invalidate(view_), VX_OK);
  EXPECT_EQ(vx_view_update(view_), VX_OK);
  EXPECT_EQ(g_invalidate_hook_calls.load(), 1);

  EXPECT_EQ(vx_view_set_pipeline_hooks(view_, nullptr, nullptr), VX_OK);
}

}  // namespace
```

#### 4.3.2 `examples/hello_devtool.cc` — DOG.1 改动

替换 ≈L213-220 的 perf_hooks 安装段：

```cpp
struct PerfSmokeUd { int frames; VxView* view; };
static PerfSmokeUd s_perf_ud{0, nullptr};
s_perf_ud.view = view;
VxPipelineHooks perf_hooks{};
perf_hooks.on_frame_end = [](void* ud) {
  auto* state = static_cast<PerfSmokeUd*>(ud);
  if (state) {
    state->frames++;
    /* TASK-20260505-02 B-G4 — force-rearm dirty_ for next frame so
     * smoke ctest can validate true multi-frame hook firing instead
     * of the frames=1 ABI-only smoke previously needed by the
     * dirty_ short-circuit (TASK-20260503-03 P3 #0 closure). */
    vx_view_invalidate(state->view);
  }
};
VxResult set_hooks_rc = vx_view_set_pipeline_hooks(view, &perf_hooks, &s_perf_ud);
// ... PERF SMOKE: frames=N print uses s_perf_ud.frames now
```

#### 4.3.3 `tests/CMakeLists.txt` — DOG.2 改动

升级 `hello_devtool_perf_smoke` PASS_REGULAR_EXPRESSION 到多帧（≥2）+ 注释更新：

```cmake
# 替换 L416-420
set_tests_properties(hello_devtool_perf_smoke PROPERTIES
  ENVIRONMENT "SDL_VIDEODRIVER=dummy;VX_HELLO_DEVTOOL_AUTOQUIT_MS=300"
  # TASK-20260505-02 B-G4 closure: hello_devtool's on_frame_end hook now
  # calls vx_view_invalidate() force-rearming dirty_ each frame, so the
  # 300ms autoquit yields 10+ frames at 60 fps target. Match >=2 frames
  # to keep resilient on slow CI hardware while still proving multi-frame
  # behavior (TASK-20260503-03 P3 #0 closed).
  PASS_REGULAR_EXPRESSION "PERF SMOKE: frames=([2-9]|[1-9][0-9]+) hud_visible=1"
  TIMEOUT 10
)
```

#### 4.3.4 `docs/specs/2026-05-04-mvp-scope.md` — DOC.1 改动

| 段 | 改动 |
|---|---|
| §3.1 表 B.9 | 状态 ⚠️ 部分 → ✅ + 「TASK-20260505-02 闭环」备注 |
| §3.2.1 B-G4 行 | strikethrough + ✅ 闭环 标记 + commit 引用 |
| §3.2.1 「MVP-B 完成度」 | ~95% → **100%** |
| §3.3 路线图「短期」段 #2 | strikethrough + ✅ 已闭环 |

---

## 5. 数据流

### 5.1 正常路径

```
embedder
  ↓ vx_view_invalidate(view)
veloxa_api.cc::vx_view_invalidate
  ↓ NULL check / cast VxView* → vx::Application*
Application::Invalidate
  ↓ update_manager_ null check
UpdateManager::Invalidate
  ↓ dirty_ = true  // 1 行赋值

[next frame]

Application::Update
  ↓
UpdateManager::Update
  ↓ dirty_ check → 通过
  → 5 hooks 触发完整 pipeline
  → dirty_ = false
  → on_frame_end hook 调 vx_view_invalidate（dogfood smoke 路径）
  → next frame loop
```

### 5.2 错误路径

| 错误 | 返回码 | 后续语义 |
|---|---|---|
| view == NULL | VX_ERROR_NULL_PARAM | embedder 编程错误 / 不修复 dirty_ |
| update_manager_ == null（fresh view 未 LoadHTML）| VX_ERROR_INVALID_STATE | silent no-op / next vx_view_update 仍是 no-op（与 vx_view_set_pipeline_hooks lazy attach 行为一致）|

---

## 6. 错误处理与边界

### 6.1 输入参数验证

- `view == NULL` → VX_ERROR_NULL_PARAM（与 ABI 风格一致 — 详 `vx_view_set_pipeline_hooks` / `vx_view_load_html` 范式）
- `update_manager_` 未 init → VX_ERROR_INVALID_STATE（与 `vx_view_set_pipeline_hooks` lazy attach 模式一致）

### 6.2 时序保证

- on_frame_end hook 内调 `vx_view_invalidate(view)` 时机分析：
  1. UpdateManager::Update 第 69 行 `dirty_ = false;`
  2. 第 71-73 行 `if (transition_mgr_.HasActive()) dirty_ = true;`
  3. 第 76-78 行 `on_frame_end` hook fire
  4. **hook 内调 vx_view_invalidate(view) → dirty_ = true 再次设置**
  5. Update 退出 / 下一帧 Update 检 `dirty_=true` → 通过 → 5 hooks 再次触发
- 此模式严格安全 — 不依赖 hook 触发顺序内的中间状态变更

### 6.3 双 Document 边界（D1-A 决策）

- vx_view_invalidate 只 invalidate target update_manager_ / 不 invalidate devtool_update_manager_
- DevTool UI 由其自身状态机管理（panel JS 内部 dirty / hot reload tracked count rearm 等独立路径）
- 测试覆盖：`InvalidateMakesNextUpdateRunHooks` 仅在 target 安装 hooks，验证 target dirty_ 路径

---

## 7. 测试策略

| 类型 | 测试 | 文件 | 数量 |
|---|---|---|:-:|
| API 单测 | InvalidateNullViewReturnsNullParam / InvalidateFreshViewReturnsInvalidState / InvalidateMakesNextUpdateRunHooks / InvalidateIdempotent | `tests/api/invalidate_api_test.cc`（新建）| 4 |
| Dogfood smoke | hello_devtool_perf_smoke ctest 升级到 multi-frame regex | `tests/CMakeLists.txt` 修改 | +0 测但语义升级 |
| A14 守门 | DEVTOOL=ON + DEVTOOL=OFF | `devtool_a14_link_closure.cmake`（既有）| 不变 |
| 反向探针 | 每个新单测均设计反向探针锚定（详 §3.4 D4 表）| 反向探针 4 测 | RED 阶段验证 |

**ctest 期望矩阵：**

| Config | Baseline | 实施后 | 增量 |
|---|:-:|:-:|:-:|
| DEVTOOL=ON | 1298 | 1302 | +4 PASS |
| DEVTOOL=OFF | 1105 | 1109 | +4 PASS |

`hello_devtool_perf_smoke` regex 从 `frames=[1-9][0-9]*` → `frames=([2-9]|[1-9][0-9]+)` （≥2 帧）。

---

## 8. 安全考量

**本任务不涉及新安全变更。** 详细分析：

| 维度 | 评估 |
|---|---|
| 新依赖 | 0 |
| 新外部输入面 | 0（API 仅暴露既有 `UpdateManager::Invalidate`，1 行 `dirty_=true`）|
| 认证授权 | N/A |
| 敏感数据 | N/A |
| DoS 表面 | embedder 调用频率自限（main thread / 同步），无 amplification |
| 竞态条件 | D2-A 决策 main thread only / Doxygen 明示 |

**继承既有安全护栏：** 0 / 不引入既有威胁面。

---

## 9. 风险登记

| 风险 | 缓解 |
|---|---|
| dirty_ 在 atomic 之外被多线程访问（ASan / TSan 警告）| D2-A 决策明示 main thread only / 与 LoadHTML/LoadCSS 一致 |
| hello_devtool_perf_smoke regex 过严导致 CI 闪烁 | regex `([2-9]\|[1-9][0-9]+)` 允许 2-99+ 帧 / 60 fps × 300ms autoquit 至少 10 帧理论上限稳定 |
| DevTool 自身 update_manager 不被 invalidate 导致 panel 数据陈旧 | D1-A 决策 / DevTool 由独立状态机管理 / 本 ABI 不应跨界 |
| 老 perf_hooks_api_test 受影响 | userdata 通道改造仅在 hello_devtool.cc / api 测试文件不受影响 / 验收 |

---

## 10. 完成定义

- [x] D1-A + D2-A + D3-A + D4-A 全部锁定
- [ ] `vx_view_invalidate(VxView*)` C ABI 声明 + Doxygen
- [ ] `Application::Invalidate()` 公开方法
- [ ] `vx_view_invalidate` 实现（≤10 行 + null 双层守门）
- [ ] 4 API 单测（含反向探针锚定）
- [ ] hello_devtool.cc 升级（PerfSmokeUd struct / on_frame_end 调 invalidate）
- [ ] hello_devtool_perf_smoke regex 升级（frames ≥2）
- [ ] DEVTOOL=ON 1298 → 1302（+4）/ DEVTOOL=OFF 1105 → 1109（+4）全 PASS
- [ ] dogfood smoke 14/14 → 14/14 PASS（不退化 / `hello_devtool_perf_smoke` 升级到 multi-frame）
- [ ] spec §3.2.1 B-G4 ✅ 闭环 + 路线图短期 #2 ✅ + MVP-B 完成度 95% → 100%
- [ ] reflection + archive 闭环

---

## 11. 参考

- `veloxa/core/update_manager.cc:14`（既有 `UpdateManager::Invalidate`）
- `veloxa/api/veloxa_api.cc:200-205`（vx_view_update 范本）
- `tests/api/perf_hooks_api_test.cc:48-130`（API 测试范本 / lazy attach 模式）
- `examples/hello_devtool.cc:213-275`（perf hooks 安装范本）
- `tests/CMakeLists.txt:412-420`（hello_devtool_perf_smoke ctest 范本）
- `tests/smoke/devtool_a14_link_closure.cmake:45-50`（A14 黑名单 — 不影响）
- `memory-bank/techContext.md` ≈L1035-1095（dirty_ 短路机制 + 4 invalidate 源 + frame-hook smoke SOP）
