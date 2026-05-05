# 归档：TASK-20260505-02 Performance Overlay 持续 invalidate 机制 — `vx_view_invalidate()` 公开 C ABI（B-G4 — MVP-B 收口最后一项）

**日期：** 2026-05-05
**任务 ID：** TASK-20260505-02
**复杂度级别：** Level 2
**状态：** ✅ 已完成 / **MVP-B 100% 闭环里程碑达成 🎉**
**安全相关：** ❌ 否（公开 ABI 扩展 / 仅暴露既有 UpdateManager::Invalidate / 无新威胁面）
**主交付：** 6 commits（plan + 4 build phase + reflect）+ 1 finalize MB 重置 commit
**总产出：** +198 行 code + ~640 行 MB/docs（含 reflection 280 行 + plan 1001 行 + spec 544 行 / build 阶段固化）/ 4 新单测 / DEVTOOL=ON 1298→1302 + DEVTOOL=OFF 1105→1109 / hello_devtool_perf_smoke frames=1 → **18**（10x 增益）

---

## 1. 任务概述

为 Veloxa 闭环 MVP-B 收口的**最后一项** — **Performance Overlay 持续 invalidate 机制（B-G4）**。新增公开 C ABI `vx_view_invalidate(VxView*)`，让 embedder 主动 force-rearm `dirty_=true` 触发下一帧完整 update pass，从根源上解决了 [TASK-20260503-03 build 阶段 P3 候选 #0](../archive/archive-TASK-20260503-03.md) 暴露的 `UpdateManager::Update()` `dirty_` 短路下 `hello_devtool_perf_smoke` 仅 frames=1 的多帧验证盲区。

### 1.1 解决的具体问题

1. **`hello_devtool_perf_smoke` 多帧验证盲区** — 引擎 `dirty_` 短路在静态 DOM + 无 transition 场景下 Update() 第二帧起 no-op / on_frame_end hook 仅 fire 1 次 / 无法验证多帧 ring buffer / dirty rect 跨帧可视化 / Performance Overlay 滑动平均
2. **embedder 缺乏主动 invalidate 入口** — profilers / tracers / 外部动画时间线无法主动触发渲染 / 仅能依赖 transition_mgr 或输入事件被动 rearm
3. **MVP-B 完成度卡在 ~95%** — B-G4 是 MVP-B 收口最后一项 gap，阻塞 100% MVP-B 闭环

### 1.2 路径决策（VAN 阶段 audit 暴露 — 反复模式 #8 spec 数据回归第 3 次实证）

**初始 spec/archive 推荐路径：** [TASK-20260503-03 archive §9 P3 候选 #0](../archive/archive-TASK-20260503-03.md) + spec §11.1 推荐**路径 (b)**「hello_devtool 注入 CSS animation 让 transition_mgr_ 持续 rearm dirty_」

**VAN 阶段 Phase 0 audit 发现：** `grep -r "@keyframes\|animation:" veloxa/` 返回 **0 命中** → Veloxa 引擎**完全不支持 CSS animation**（仅 4 transition 属性 / 无 @keyframes / 无 AnimationManager 子系统）。spec 列出的路径 (b) **实际不可行**。

**调整后路径决策（用户 `perf_invalidate_path = path_a_full`）：**
- **路径 (a) 完整版** — 新增公开 `vx_view_invalidate()` C ABI（embedder 通用 + dogfood smoke 多帧验证）
- 拒绝路径 (b) CSS animation（不可行）+ 路径 (c) 仅输入注入（仅 dogfood）+ 路径 (d) 仅 transition（仅 dogfood）+ defer（搁置）

### 1.3 核心成果（5 项战略价值）

1. **🎉 MVP-B 100% 闭环里程碑** — B-G1+G2+G3+G4 全 4 项 gap 全部闭环 / 双任务（TASK-01 + TASK-02）连击实证 / 用户可基于完整 MVP-B 能力进入 MVP-C 战略目标
2. **`vx_view_invalidate()` 公开 C ABI 交付** — embedder 通用 / 沿用 lazy-attach 范式（quad-evidence）/ Doxygen 显式声明 idempotency + thread-safety + lazy-attach contract
3. **`hello_devtool_perf_smoke` frames=1 → 18（10x 增益）** — TASK-20260503-03 P3 候选 #0 闭环 ✅ / Performance Overlay 多帧验证 dogfood smoke 真正生效
4. **`Application::Invalidate()` 公开方法就位** — 未来 C++ 内部代码可直接调用（不必绕到 C ABI）/ 与 `vx_view_set_pipeline_hooks` 同源 lazy-attach 模式
5. **5 个范式同时升级（reflection 史上单任务沉淀升级数最高纪录）** — plan ×0.6 sext-evidence + 跨决策协同度 dec-evidence + 反向探针强度梯度三档 triple-evidence + 反复模式 #8 triple-evidence + lazy-attach C ABI 容错 quad-evidence

---

## 2. 技术方案

### 2.1 整体方案：Level 2 标准 TDD 工作流（plan 内联 brainstorm 决策）

| 维度 | 决策 |
|---|---|
| 路径 | **(a) 完整版** — 新增公开 vx_view_invalidate() C ABI（拒绝 (b) CSS animation 不可行 / (c) 仅输入注入 / (d) 仅 transition / defer）|
| TDD 模式 | 严格 RED→GREEN→REFACTOR + 反向探针强度梯度三档（A.1 NULL guard 过高 + A.1 dirty_ rearm 合适 + D.1 ctest regex 灵敏度 平衡）|
| 决策融合 | brainstorm 阶段 1 次 AskQuestion 锁 D1+D2+D3+D4 4 决策（dec-evidence 第 10 次连续 100% 命中）|
| 范式复用 | lazy-attach C ABI 模式（quad-evidence / vx_view_set_pipeline_hooks 范式）|
| 测试隔离 | static atomic counter（C 函数指针约束 / 无法 lambda capture）+ reset/cleanup 保护 |

### 2.2 4 个关键设计决策（brainstorm 阶段锁定）

| # | 决策 | 选择 | 理由 |
|:-:|---|---|---|
| **D1** | 双 update_manager 路由 | **D1-A** 仅 target update_manager_ | VxView 单一 view 抽象 / DevTool 独立状态机 / 实现最简 |
| **D2** | 线程安全语义 | **D2-A** main thread only | 与 LoadHTML/InjectInput 一致 / dirty_ 非 atomic / atomic 改造低 ROI |
| **D3** | hello_devtool 注入位置 | **D3-A** on_frame_end hook | 利用既有 perf_hooks 范式 / userdata 通道传 VxView* / 0 新机制 / 时序严格安全 |
| **D4** | API 单测覆盖范围 | **D4-A** 完整 4 单测 | null / fresh INVALID_STATE / 正常路径 / idempotent — 含反向探针锚定 4 项 |

### 2.3 关键 API 设计

**`Application::Invalidate()`（C++ 内部公开方法 / D1-A 路由）：**

```cpp
bool Application::Invalidate() {
  if (!update_manager_) return false;  // lazy-attach 范式 / silent no-op
  update_manager_->Invalidate();        // 仅 target / DevTool 独立状态机
  return true;
}
```

**`vx_view_invalidate()`（公开 C ABI / D2-A 主线程 / lazy-attach contract）：**

```cpp
VxResult vx_view_invalidate(VxView* view) {
  if (!view) return VX_ERROR_NULL_PARAM;
  auto* app = reinterpret_cast<vx::Application*>(view);
  return app->Invalidate() ? VX_OK : VX_ERROR_INVALID_STATE;
}
```

**hello_devtool.cc on_frame_end 注入（D3-A 范式 / PerfSmokeUd struct）：**

```cpp
struct PerfSmokeUd { int frames; VxView* view; };
static PerfSmokeUd s_perf_ud{0, nullptr};
s_perf_ud.view = view;
perf_hooks.on_frame_end = [](void* ud) {
  auto* state = static_cast<PerfSmokeUd*>(ud);
  if (state) {
    state->frames++;
    vx_view_invalidate(state->view);  // force-rearm dirty_ for next frame
  }
};
```

---

## 3. 实现摘要

### 3.1 5 个 commits 全景表

| Phase | commit | 类型 | 行数 | 说明 |
|---|---|---|:-:|---|
| Plan | `4feda52` | docs(plan) | +1545/-12 | plan + spec docs（**TASK-01 P1 #6 首次成功实施**：plan/spec docs 落盘即 commit）|
| A.1 + B.1 | `a7e6bed` | feat(api) | +198 | vx_view_invalidate() 公开 C ABI + 4 单测 + CMake 注册 / 反向探针 2/2 精准 |
| C.1 + D.1 | `929569a` | feat(devtool) | +34/-20 | hello_devtool on_frame_end 注入 + ctest regex ≥2 帧 / 反向探针 1/1 精准 / frames=18 |
| E.1 | `8d00aee` | docs(spec) | +8/-6 | MVP-scope spec §3.2.1 B-G4 闭环 + 完成度 95% → **100%** |
| Finalize | `27875a8` | chore(build) | +78/-26 | activeContext + progress + tasks Build 阶段输出 |
| Reflect | `f304a85` | docs(reflect) | +559/-4 | reflection-TASK-20260505-02.md（280 行）+ systemPatterns 5 段升级 + techContext + progress + tasks + activeContext |

### 3.2 文件变更（新增/修改）

| 操作 | 文件路径 | 行数 | 说明 |
|------|---------|:-:|------|
| **修改** | `veloxa/api/veloxa_api.h` | +34 | `vx_view_invalidate()` 声明 + Doxygen（lazy-attach contract / idempotency / thread-safety）|
| **修改** | `veloxa/api/veloxa_api.cc` | +11 | C ABI 实现（NULL guard + Application::Invalidate 桥接 + INVALID_STATE 映射）|
| **修改** | `veloxa/core/application.h` | +15 | `Application::Invalidate()` public 声明 + 注释（lazy-attach + main-thread-only + D1-A 路由）|
| **修改** | `veloxa/core/application.cc` | +10 | `Application::Invalidate()` 实现 |
| **新建** | `tests/api/invalidate_api_test.cc` | +118 | 4 测试 case（NullView / FreshView / MakesNextUpdateRunHooks / Idempotent）+ static atomic counter |
| **修改** | `tests/CMakeLists.txt` | +10 | `vx_add_test(invalidate_api_test ...)` + DEVTOOL=ON/OFF 双 config 注释 + perf_smoke regex 升级到 ≥2 帧 |
| **修改** | `examples/hello_devtool.cc` | +14/-3 | PerfSmokeUd struct 替换 int counter + on_frame_end 注入 vx_view_invalidate(view) |
| **修改** | `docs/specs/2026-05-04-mvp-scope.md` | +8/-6 | §3.1 B.9 ✅ + §3.2.1 B-G4 ✅ + 完成度 95% → **100%** + §3.3 短期路线图 ✅ |
| **新建** | `docs/specs/2026-05-05-perf-overlay-invalidate-api-design.md` | +544 | 设计文档（11 段 / 4 决策 + 4 风险登记）|
| **新建** | `docs/plans/2026-05-05-perf-overlay-invalidate-api.md` | +1001 | 实现计划（5 Phase / 8 任务 / Phase 0 11 子段 audit / commit 范本）|
| **新建** | `memory-bank/reflection/reflection-TASK-20260505-02.md` | +280 | Level 2 详细回顾（8 段 / 6 度量 / 8 改进建议）|
| **修改** | `memory-bank/systemPatterns.md` | +405 | 5 个范式段沉淀（sext / dec / triple / triple / quad / + MVP-B 100% 里程碑）|
| **修改** | `memory-bank/techContext.md` | +44 | 「CMake + GTest 增量加测工作流注意事项」段（P2）|
| **修改** | `memory-bank/{activeContext,progress,tasks}.md` | +210 | 阶段流转 + 详细产出记录 |

### 3.3 关键实现细节

#### 3.3.1 lazy-attach C ABI 容错模式（quad-evidence — 已成 Veloxa 默认范式）

`update_manager_` 在 LoadHTML/LoadCSS 调用前为 null（lazy 初始化范式 / 与 `vx_view_set_pipeline_hooks` 一致）。当 embedder 在 LoadHTML 之前调用 `vx_view_invalidate()` 时：
- `Application::Invalidate()` 返回 `false`（silent no-op）
- C ABI 映射到 `VX_ERROR_INVALID_STATE`（不是 NULL_PARAM / 不是 SEGFAULT）
- Doxygen 显式声明「same lazy-attach contract as vx_view_set_pipeline_hooks」

这是 Veloxa 公开 C ABI 第 4 次复用 lazy-attach 模式（TASK-20260502-02 B.0.1 + TASK-20260503-01 C.4.1 + TASK-20260503-04 + 本任务），已升级到 quad-evidence / 已成默认范式。

#### 3.3.2 PerfSmokeUd struct 设计（D3-A on_frame_end 注入范式）

GoogleTest C 函数指针 `void(*)(void*)` 不能 lambda capture，需要 userdata 通道传递 `VxView*`。设计：

```cpp
struct PerfSmokeUd { int frames; VxView* view; };
```

替换原 `static int s_perf_smoke_frames = 0;` → `static PerfSmokeUd s_perf_ud{0, nullptr}`。lambda 通过 `static_cast<PerfSmokeUd*>(ud)` 同时获取 frames counter + view handle，实现 force-rearm。**0 新机制**（沿用 vx_view_set_pipeline_hooks userdata 范式）。

#### 3.3.3 ctest regex 升级（D.1 — 多帧验证）

```cmake
# 升级前（仅 ABI smoke）
PASS_REGULAR_EXPRESSION "PERF SMOKE: frames=[1-9][0-9]* hud_visible=1"

# 升级后（多帧验证 ≥2 帧）
PASS_REGULAR_EXPRESSION "PERF SMOKE: frames=([2-9]|[1-9][0-9]+) hud_visible=1"
```

理论 60fps × 300ms autoquit = ~18 帧 / 实测 frames=18 / 反向探针验证 regex 灵敏度（注释 vx_view_invalidate 调用 → frames=1 → 精准 FAIL）。

### 3.4 关键决策（含安全决策）

#### 3.4.1 D1-A 路由：仅 target update_manager_ — 不影响 DevTool

**问题：** Application 持有 `update_manager_`（target Document）+ `devtool_update_manager_`（DevTool）双 UpdateManager。`vx_view_invalidate()` 应只作用于 target 还是双 invalidate？

**决策：** **仅 target** — 因为：
- DevTool 运行**独立的状态机** / 它不响应 embedder 的 invalidate 语义
- 实现最简 / 路由复杂度低
- 公开 ABI 不应暴露 DevTool 内部细节
- DevTool 内部需要 invalidate 时使用 `devtool_update_manager_->Invalidate()` 直接调用

#### 3.4.2 D2-A 线程安全：main thread only — dirty_ 非 atomic

**问题：** `UpdateManager::dirty_` 是 plain bool 而非 std::atomic。是否应让 `vx_view_invalidate()` 声明 thread-safe（暗示 dirty_ 改 atomic）？

**决策：** **main thread only** — 因为：
- 与 LoadHTML/LoadCSS/InjectInput 等其他公开 ABI 一致（all main-thread-only）
- atomic 改造 ROI 低（dirty_ 在 hot path / atomic 性能开销可观）
- multithread embedder 应自行用 mutex 桥接（standard pattern）
- Doxygen 显式声明「Thread-safety: main thread only」/ 契约明示

#### 3.4.3 D3-A on_frame_end hook 注入 — 严格时序安全

**问题：** hello_devtool 中如何让 hook 持续 fire？on_frame_end 调用 invalidate 是否安全？

**决策：** **on_frame_end hook 注入** — 因为：
- 时序分析（plan §0.9）：`Update()` 先 reset `dirty_=false` → 跑 style/layout/render → 检查 transition_mgr.HasActive() rearm → fire on_frame_end → 函数返回。因此 on_frame_end 内调 `Invalidate()` 设 `dirty_=true`，下一帧 `Update()` 进入时 `dirty_` 已为 true，正常进入完整 pass
- **关键安全特性：** hook 内调 invalidate 不依赖中间状态变更（dirty_=false reset 已在 hook 触发之前完成）
- 利用既有 `perf_hooks` 范式 / userdata 通道传 VxView* / **0 新机制**

#### 3.4.4 安全决策：本任务不涉及安全变更

详细评估见 §3.f 安全评估表（reflection §3.f）。**0 新依赖 / 0 新 IO / 0 新 syscall / 0 新外部输入路径**：
- 公开 ABI 扩展但仅暴露既有 `UpdateManager::Invalidate()`（`dirty_=true` 1 行操作）
- NULL guard + INVALID_STATE 守门 + idempotent 契约 + 主线程 lock-down 全在 ABI 设计中显式声明
- 错误信息全为枚举值 / 无字符串泄露
- DevTool subsystem 不被影响（D1-A 路由）

---

## 4. 测试覆盖

### 4.1 4 个新单测（tests/api/invalidate_api_test.cc）

| # | TestCase | 验证维度 | 预期结果 |
|:-:|---|---|---|
| 1 | `NullViewReturnsNullParam` | C ABI NULL guard | `VX_ERROR_NULL_PARAM` |
| 2 | `FreshViewReturnsInvalidState` | lazy-attach contract（fresh view 无 LoadHTML）| `VX_ERROR_INVALID_STATE` |
| 3 | `InvalidateMakesNextUpdateRunHooks` | dirty_ rearm 核心契约（pipeline hooks counter 验证）| 第二次 Update **fire hooks 1 次**（vs invalidate 前 0 次）|
| 4 | `InvalidateIdempotent` | 多次调用幂等性 | N=5 次 invalidate 等价 1 次（next Update fire hooks 1 次）|

### 4.2 反向探针（强度梯度三档全谱实证）

| 强度档 | 探针 | 实证 | 验证逻辑 |
|---|---|---|---|
| **过高（双重加固）** | A.1 NULL guard 注释 | **SEGFAULT** in NullViewReturnsNullParam | 守门是 C ABI UB 防御层 + 业务层双重加固 |
| **合适（精准 N/M）** | A.1 `update_manager_->Invalidate()` 注释 | **2/4 精准 FAIL** | dirty_ rearm 是核心契约 / null + fresh 测不依赖 |
| **平衡（等量 N/N）** | D.1 `vx_view_invalidate` 调用注释 | **frames=1 / ctest FAIL** | regex `([2-9]\|[1-9][0-9]+)` 灵敏度精准识别 |

**强度梯度三档全谱第 2 任务实证** → dual → **triple-evidence 升级** ✅

### 4.3 ctest 实测矩阵（双 config 验证）

| Config | baseline | 完成后 | 增量 | 状态 |
|---|:-:|:-:|:-:|:-:|
| **DEVTOOL=ON** (build/) | 1298 | **1302** | +4 | 100% PASS ✅ |
| **DEVTOOL=OFF** (build-no-devtool/) | 1105 | **1109** | +4 | 100% PASS ✅ |
| **dogfood smoke** (build-sdl2/) | 3 件套 | 3 件套 | 0 | 3/3 PASS ✅（hello_devtool_smoke + perf_smoke `frames=18` + hot_reload_smoke）|

**A14 link-closure：** 公开 ABI / 不属 DevTool subsystem / 不受 A14 守门影响（plan §0.8 audit 预证）✅

### 4.4 hello_devtool dogfood 实测

```
Veloxa 0.1.0 — Hello DevTool Example
DevTool attached on right-hand dock (width=270, F12 to toggle).
Pipeline hooks installed (rc=-2).
HUD visible after attach: 1 (1 expected).
T3 redaction OK: password value masked in DOM JSON.
PERF SMOKE: frames=18 hud_visible=1   ← 10x 增益（vs 升级前 frames=1）
Done.
```

---

## 5. 经验教训（从 reflection 提取）

### 5.1 范式成熟度顶峰 — 5 个范式同时升级（reflection 史上最高纪录）

| 范式 | 升级前 | 升级后 | 评估 |
|---|:-:|:-:|---|
| plan ×0.6 实测系数 | quint-evidence | **sext-evidence** | 第 6 数据点 / 极速区续延档新子档 0.20-0.35× |
| 跨决策协同度 100% | nona-evidence | **dec-evidence** | 第 10 次连续 / 累计 100/100 / 范式成熟度顶峰 |
| 反向探针强度梯度三档 | dual-evidence | **triple-evidence** | 第 2 任务实证 / 三档全谱稳定可复现 |
| 反复模式 #8 spec 数据回归 | dual-evidence | **triple-evidence** | 已达 writing-plans.mdc 固化阈值 |
| lazy-attach C ABI 容错模式 | triple-evidence | **quad-evidence** | 已成 Veloxa 公开 C ABI 默认范式 |

### 5.2 「plan/spec docs 落盘即 commit」首次成功实施 ✅（TASK-01 P1 #6 闭环）

TASK-20260505-01 reflection P1 #6 改进建议本任务首次实施落地：`/plan` 阶段产出 spec + plan + MB 更新合并为单 commit（`4feda52`）/ build 阶段 0 collateral commit / 工作流流程 cleanup ✅。建议升级到 P0 立即固化协议到 `.cursor/rules/skills/writing-plans.mdc`（已迁移到 activeContext 待处理事项 P1 #6）。

### 5.3 反复模式 #8 spec 数据回归 audit triple-evidence — 已达固化阈值

第 3 次实证（VAN audit 暴露 spec 推荐路径 (b) CSS animation **实际不可行**因引擎不支持 @keyframes）→ 修正决策为路径 (a) 完整版 / 避免 ~30 min CSS animation 路径死胡同实施。审查协议建议下次工作流元任务批量落地时同步固化到 `.cursor/rules/skills/writing-plans.mdc` Phase 0 audit 段（P1 #7 已迁移待处理事项）。

### 5.4 plan ×0.6 实测系数 sext-evidence — 极速区续延档新子档

实测 ~30 min vs plan ×0.6 95-115 min = **0.26-0.32×** / 略微高于纯极速区 0.10-0.20×，因 D.2 双 config full ctest 等待时间不可压缩（占总时长 ~33%）。剔除 D.2 后核心实施 phase 比值约 **0.20×** 命中极速区下沿。新子档「极速区续延档 0.20-0.35× — D.2/D.3 不可压缩 ctest 等待主导」候选触发条件已沉淀到 systemPatterns（4 项必须全满足）。

### 5.5 lazy-attach C ABI 容错模式 quad-evidence — 已成 Veloxa 默认范式

第 4 次复用（TASK-20260502-02 + TASK-20260503-01 + TASK-20260503-04 + 本任务）。`vx_view_invalidate(view)` 在 `update_manager_` 为 null 时返回 INVALID_STATE 而非 crash — 完美沿用 `vx_view_set_pipeline_hooks` 范式。**0 新设计成本** / Doxygen 文档可直接引用前序 API。建议下次工作流元任务批量落地时 (a) 固化到 `.cursor/rules/skills/writing-plans.mdc` C ABI 设计模式段 + (b) 在 `veloxa/api/veloxa_api.h` 顶部 doc 段添加「lazy-attach contract」一节统一引用（P1 #8 已迁移待处理事项）。

### 5.6 MVP-B 100% 闭环里程碑 — 双任务连击实证

TASK-20260505-01（B-G1+G2+G3 / DomBindings R2）+ TASK-20260505-02（B-G4 / vx_view_invalidate ABI）双任务连击 / 4/4 gap 全闭环 / 双任务总投入 ~65 min vs plan ×0.6 估时 ~285-365 min = 实测 **0.18-0.23×** 落极速区续延档 / dogfood 视觉链路完整恢复 / 5 件套 dogfood smoke 100% PASS。

---

## 6. 改进建议落实情况（8 项 P0+P1+P2）

| # | 建议 | 优先级 | 落实状态 |
|:-:|---|:-:|---|
| 1 | systemPatterns「plan ×0.6 sext-evidence — 极速区续延档」段 | **P0** | ✅ reflect 阶段直接沉淀 |
| 2 | systemPatterns「跨决策协同度 dec-evidence」段 | **P0** | ✅ reflect 阶段直接沉淀 |
| 3 | systemPatterns「反向探针强度梯度三档 triple-evidence」段 | **P0** | ✅ reflect 阶段直接沉淀 |
| 4 | systemPatterns「反复模式 #8 spec 数据回归 triple-evidence」段 | **P0** | ✅ reflect 阶段直接沉淀 |
| 5 | systemPatterns「lazy-attach C ABI 容错模式 quad-evidence」段 | **P0** | ✅ reflect 阶段直接沉淀 |
| 6 | `/plan` 命令固化「plan/spec docs 落盘即 commit」步骤 | **P0 → 升级**（首次实施成功）| ✅ 沉淀范例 + 📋 待处理 P1 #6 |
| 7 | systemPatterns「MVP-B 100% 闭环里程碑」段 | **P1** | ✅ reflect 阶段直接沉淀 |
| 8 | techContext「CMake + GTest 增量加测工作流注意事项」段 | **P2** | ✅ reflect 阶段直接沉淀 |

**P0 5/5 reflect 阶段直接落实 ✅**（沿用 [TASK-20260505-01 P0 4/4 全落实范式](archive-TASK-20260505-01.md)）+ **P0 升级 1/1 (#6) 沉淀范例 + 待处理事项更新** + **P1 1/1 (#7) reflect 阶段直接沉淀** + **P2 1/1 (#8) techContext 沉淀**

**累计 P1 待处理事项新增（等下次工作流元任务批量落地）：**
- **P1 #6**（升级）`/plan` 命令固化「plan/spec docs 落盘即 commit」步骤
- **P1 #7** 反复模式 #8 spec 数据回归 audit 协议固化到 `.cursor/rules/skills/writing-plans.mdc`（triple-evidence 已达固化阈值）
- **P1 #8** lazy-attach C ABI 容错模式固化（quad-evidence 已成默认范式 / writing-plans.mdc + veloxa_api.h 顶部 doc）

---

## 7. 反复模式核对（0/7 全抑制）

| # | 已知反复模式 | 出现频率 | 本次状态 | 抑制证据 |
|:-:|---|:-:|:-:|---|
| 1 | 计划文件清单与实际变更不一致 | 9+ | ✅ 抑制 | 8 计划文件 100% 命中 / 0 未列入 / 行数偏差 +20-50% 属正常注释扩张 |
| 2 | 子代理产出需大量返工 | 7+ | N/A | 单 agent 直跑（适合 Level 2）|
| 3 | 前置依赖/环境/API 能力未验证 | 8+ | ✅ 抑制 | Phase 0 audit 11 子段全预证 |
| 4 | 非默认路径遗漏验证 | 4+ | ✅ 抑制 | null + fresh + idempotent 全测 |
| 5 | 测试隔离问题 | 7+ | ✅ 抑制 | static atomic counter + reset/cleanup 范式 / 0 flaky |
| 6 | 提交粒度偏离计划 | 7+ | ✅ 抑制 | 6 commits 100% 按计划子阶段分组 |
| 7 | TDD 严格度与场景不匹配 | 11+ | ✅ 抑制 | RED→GREEN→REFACTOR 全跑 / 反向探针强度梯度三档全谱 |

**0/7 全抑制 ✅**（连续 4 任务保持 0 命中纪录 — TASK-20260503-04 / TASK-20260504-01 / TASK-20260505-01 / 本任务）

---

## 8. 度量数据汇总

| 维度 | 数据 |
|---|---|
| **复杂度级别** | Level 2 |
| **总耗时** | VAN ~10 min + Plan ~15 min + Build ~30 min + Reflect ~30 min + Archive ~15 min = **~100 min 总流程** |
| **Build 阶段耗时** | ~30 min（plan ×0.6 95-115 min × 0.26-0.32× sext-evidence）|
| **commits 数** | **6**（plan + 4 build + reflect / + 1 finalize / 总 7）|
| **代码行数** | +198 行（A.1+B.1 154 / C.1+D.1 34 / E.1 8/-6）|
| **文档行数** | +1825 行（reflection 280 + plan 1001 + spec 544）|
| **MB/系统模式行数** | +659 行（systemPatterns 405 + techContext 44 + activeContext + progress + tasks）|
| **新单测数** | 4（NullView / FreshView / MakesNextUpdateRunHooks / Idempotent）|
| **ctest baseline → 完成** | DEVTOOL=ON 1298 → 1302 / DEVTOOL=OFF 1105 → 1109 / dogfood 3/3 PASS |
| **dogfood frames** | 1 → **18**（10x 增益）|
| **反向探针** | 3 项 / 3/3 精准 FAIL / 强度梯度三档全谱（过高 + 合适 + 平衡）|
| **跨决策协同度** | D1+D2+D3+D4 1 次 AskQuestion 锁定 / 第 10 次连续 100% / 累计 100/100 |
| **plan ×0.6 实测系数** | **0.26-0.32×**（sext-evidence / 极速区续延档新子档）|
| **范式升级数** | **5 个范式同时升级**（reflection 史上单任务最高纪录）|
| **反复模式抑制** | 0/7 全抑制（连续 4 任务）|
| **安全表面变化** | 0（公开 ABI 扩展但 0 新依赖 / 0 新 IO / 0 新威胁面）|
| **MVP 里程碑** | **MVP-B 95% → 100% 闭环 🎉**（4/4 gap 全闭环）|

---

## 9. 参考文档

### 9.1 核心文档（按生命周期顺序）

- **设计规格：** `docs/specs/2026-05-05-perf-overlay-invalidate-api-design.md`（544 行 / 11 段 / 4 决策 + 4 风险登记）
- **实现计划：** `docs/plans/2026-05-05-perf-overlay-invalidate-api.md`（1001 行 / 5 Phase / 8 任务 / Phase 0 11 子段 audit / commit 范本）
- **回顾文档：** `memory-bank/reflection/reflection-TASK-20260505-02.md`（280 行 / Level 2 详细回顾 / 8 段 / 8 改进建议 P0/P1/P2 全分级）

### 9.2 spec 同步

- **MVP-scope spec 闭环：** `docs/specs/2026-05-04-mvp-scope.md` §3.1 B.9 + §3.2.1 B-G4 + §3.3 短期路线图（commit `8d00aee`）

### 9.3 systemPatterns 沉淀（5 段升级）

- `memory-bank/systemPatterns.md`：
  - 「plan ×0.6 实测系数 sext-evidence — 极速区续延档」段（新建 / quint → sext）
  - 「跨决策协同度 100% dec-evidence」段（新建 / nona → dec）
  - 「反向探针强度梯度三档 triple-evidence」段（dual → triple / 已存在段补充第 2 任务实证）
  - 「反复模式 #8 spec 数据回归 triple-evidence」段（dual → triple / 已存在段补充第 3 实证 + 能力假设 audit 子段）
  - 「lazy-attach C ABI 容错模式 quad-evidence」段（triple → quad / 已存在段升级实证表）
  - 「MVP-B 100% 闭环里程碑」段（新建 / 双任务连击实证）

### 9.4 techContext 沉淀

- `memory-bank/techContext.md` 「CMake + GTest 增量加测工作流注意事项」段（新建 / P2 长期沉淀）

### 9.5 关联任务交叉引用

- **前序任务：** [TASK-20260505-01 archive](archive-TASK-20260505-01.md)（DomBindings R2 收口 / B-G1+G2+G3 / MVP-B 90% → 95%）
- **闭环任务：** [TASK-20260503-03 archive](archive-TASK-20260503-03.md) §9 P3 候选 #0（首次暴露多帧验证盲区 / 本任务闭环）
- **设计范式来源：** [TASK-20260502-02 archive](archive-TASK-20260502-02.md)（lazy-attach C ABI 容错模式首次沉淀）

---

## 10. 后续候选任务

### 10.1 直接派生（基于本任务沉淀）

- 无 — MVP-B 收口完成 / 无后续 R2 P3 候选

### 10.2 MVP-C 战略目标（用户解锁）

基于 MVP-B 100% 闭环，用户可进入 MVP-C 阶段。推荐立项顺序（详 [`docs/specs/2026-05-04-mvp-scope.md` §11.2](../../docs/specs/2026-05-04-mvp-scope.md)）：

| 优先 | 候选任务 | MVP 档 | Level | plan ×0.6 |
|:-:|---|:-:|:-:|:-:|
| **1** | **G1 OpenGL ES 硬件渲染后端蓝图**（核心目标 #2 嵌入式硬件加速主线 P0 第一刚需）| MVP-C 核心 | **L4 多 Phase 蓝图** | ~30-60+ h |
| 2 | 资源加载策略蓝图（HTTP / file:// / data: URI 完整支持）| MVP-C 过渡 | L3 蓝图 + 实施 | ~5-10 h |
| 3 | R9 EventManager HitTest 改造（HUD pointer-events 真支持）| MVP-C | L2-3 | ~1.5-2 h |
| 4 | G2 DRM/KMS 嵌入式后端 | MVP-C 核心 | L3-4 | ~10-20 h |

### 10.3 工作流元任务（待批量落地）

累计 P1 待处理事项 **8 项**（TASK-20260503-04 #1+#2 + TASK-20260503-05 #2 + TASK-20260505-01 #5+#6 + 本任务 #6 升级+#7+#8 + P2 #4 = 详见 [`memory-bank/activeContext.md`](../activeContext.md) 「待处理事项」段），适合下次工作流元任务（沿用 [TASK-20260503-02 工作流元任务范式](archive-TASK-20260503-02.md)）批量沉淀到 `.cursor/rules/skills/*.mdc`。

---

**任务结束时间：** 2026-05-05 ~16:15
**归档时间：** 2026-05-05 ~16:15
