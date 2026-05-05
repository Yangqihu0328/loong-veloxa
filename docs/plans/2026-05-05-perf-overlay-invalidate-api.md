# 实现计划：Performance Overlay 持续 invalidate 机制 — `vx_view_invalidate()` 公开 C ABI

**任务 ID：** TASK-20260505-02
**复杂度级别：** Level 2
**日期：** 2026-05-05
**预估总耗时（plan ×0.6）：** ~70-110 min（实测期待落极速区 0.10-0.20× / ~10-20 min）
**关联设计：** [docs/specs/2026-05-05-perf-overlay-invalidate-api-design.md](../specs/2026-05-05-perf-overlay-invalidate-api-design.md)

---

## Phase 0 — 前置 audit（VAN 阶段已先跑 / 本节固化沉淀）

| # | audit 子段 | 结果 | 决策影响 |
|:-:|---|---|---|
| 0.1 | grep `@keyframes/animation:` in `veloxa/` | 0 命中 / 引擎不支持 CSS animation | 路径 (b) 不可行 / 改路径 (a) |
| 0.2 | `UpdateManager::Invalidate` 既有实现 | update_manager.cc:14 / `dirty_=true` 1 行 | 0 引擎层改造 |
| 0.3 | `Application::update_manager()` getter | application.h:71 / 返回 `const UpdateManager*` | 需新增 `Application::Invalidate()` 公开方法 |
| 0.4 | `Application::EnsureUpdateManager` 时序 | application.cc:262 / LoadHTML 后 lazy init | fresh view → INVALID_STATE 是正常语义 |
| 0.5 | `vx_view_set_pipeline_hooks` lazy attach 范式 | api.cc:413-438 / null update_manager → INVALID_STATE + cache | 范式复用（INVALID_STATE 语义）|
| 0.6 | `perf_hooks_api_test.cc` 测试范本 | 3 单测 / lazy attach + userdata round-trip | 4 单测设计沿用 |
| 0.7 | `hello_devtool_perf_smoke` ctest regex | `frames=[1-9][0-9]*`（≥1）| 升级到 `([2-9]\|[1-9][0-9]+)`（≥2）|
| 0.8 | A14 守门黑名单 | DevTool subsystem only / 不影响公开 ABI | 不需要更新 |
| 0.9 | on_frame_end hook 时序分析 | dirty_=false reset → transition rearm → on_frame_end | 钩子内调 invalidate 严格安全 |
| 0.10 | UpdateManager dirty_ 类型 | `bool`（非 atomic）| D2-A main thread only |
| 0.11 | hello_devtool.cc PERF SMOKE 输出格式 | `printf("PERF SMOKE: frames=%d hud_visible=%d\n", ...)` | 输出格式不变 / 仅 frames 数值升级 |

**Phase 0 投入：** ~15 min VAN 阶段已完成 + 5 min plan 阶段固化（11 子段 / 高密度 audit）

---

## Phase A — API 声明 + Application::Invalidate（~15 min plan ×0.6）

### Phase A.1 — `Application::Invalidate()` 公开方法 + `vx_view_invalidate()` C ABI 声明 + 实现

**文件变更：**

| 文件 | 操作 | 行数 | 共享文件？ |
|---|---|:-:|:-:|
| `veloxa/core/application.h` | 修改 | +5 | 否 |
| `veloxa/core/application.cc` | 修改 | +7 | 否 |
| `veloxa/api/veloxa_api.h` | 修改 | +30（含 Doxygen）| 否 |
| `veloxa/api/veloxa_api.cc` | 修改 | +6 | 否 |

**TDD 序：**

1. **RED — 新建 `tests/api/invalidate_api_test.cc`** 4 单测全 fail：
   - `InvalidateNullViewReturnsNullParam`
   - `InvalidateFreshViewReturnsInvalidState`
   - `InvalidateMakesNextUpdateRunHooks`
   - `InvalidateIdempotent`
   - 编译 fail（vx_view_invalidate 未声明）→ 期望 RED ✅

2. **GREEN — 实现 4.2.1 + 4.2.2 + 4.2.3 + 4.3.1（详 design §4）：**

   **`veloxa/core/application.h`** — `Update()` 声明后插入：
   ```cpp
   // TASK-20260505-02 — Force next Update() to run a full pass even when
   // dirty_ would have short-circuited. Returns false when update_manager_
   // is not yet initialized; the call is a silent no-op in that case
   // (consistent with SetPipelineHooks lazy attach pattern). Routing
   // target: target update_manager_ only — DevTool's UpdateManager runs
   // an independent state machine.
   //
   // Thread-safety: main thread only.
   bool Invalidate();
   ```

   **`veloxa/core/application.cc`** — `Update()` 之前插入：
   ```cpp
   bool Application::Invalidate() {
     if (!update_manager_) return false;
     update_manager_->Invalidate();
     return true;
   }
   ```

   **`veloxa/api/veloxa_api.h`** — 在 `vx_view_run` 之后（约 L130）插入完整 Doxygen 文档（详 design §4.1）+ 声明：
   ```c
   VxResult vx_view_invalidate(VxView* view);
   ```

   **`veloxa/api/veloxa_api.cc`** — `vx_view_update` 之前（约 L200）插入：
   ```cpp
   VxResult vx_view_invalidate(VxView* view) {
     if (!view) return VX_ERROR_NULL_PARAM;
     auto* app = reinterpret_cast<vx::Application*>(view);
     bool ok = app->Invalidate();
     return ok ? VX_OK : VX_ERROR_INVALID_STATE;
   }
   ```

3. **测试通过验证：** `cmake --build build --target tests` && ctest invalidate_api_test → 4/4 PASS ✅

4. **反向探针（4 项）：**
   - 注释 `if (!view)` 守门 → expect NullView 测 segfault（实证 守门必要性）
   - 注释 `if (!update_manager_)` 守门 → expect FreshView 测 segfault
   - 注释 `update_manager_->Invalidate()` → expect MakesNextUpdate 测 hook calls=0（实证 dirty_=true 必要）
   - 改 `app->Invalidate()` 为 no-op `return true;` → 同上验证

5. **commit `feat(api): vx_view_invalidate() public C ABI (TASK-20260505-02 A.1)`**
   - body: `Source: docs/plans/2026-05-05-perf-overlay-invalidate-api.md §A.1`
   - body: 反向探针实测 4/4 精准 FAIL
   - body: ctest invalidate_api_test 4/4 PASS

**A.1 估时：** ~30-45 min plan ×0.6 / 实测期待 ~5-7 min

---

## Phase B — `tests/CMakeLists.txt` 注册新测试 + 双 config 验证（~10 min）

### Phase B.1 — 在 vx_add_test 注册块新测试

**文件变更：**

| 文件 | 操作 | 行数 | 共享文件？ |
|---|---|:-:|:-:|
| `tests/CMakeLists.txt` | 修改 | +5（vx_add_test 新行）| **是 [共享文件]** |

**操作：** 在 perf_hooks_api_test 之后追加：

```cmake
vx_add_test(NAME invalidate_api_test
            SRCS api/invalidate_api_test.cc
            DEPENDS vx_api)
```

**TDD：**

1. cmake reconfigure — `cmake --build build --target tests`
2. ctest -R invalidate → 4/4 PASS ✅
3. **双 config full ctest baseline 验证：** DEVTOOL=ON 1298 → 1302（+4 PASS）/ DEVTOOL=OFF 1105 → 1109（+4 PASS）

4. **commit `test(api): register invalidate_api_test in CMake (TASK-20260505-02 B.1)`**

**B.1 估时：** ~10 min plan ×0.6 / 实测期待 ~2-3 min

---

## Phase C — hello_devtool dogfood smoke 升级（~15 min）

### Phase C.1 — hello_devtool.cc 注入 vx_view_invalidate

**文件变更：**

| 文件 | 操作 | 行数 | 共享文件？ |
|---|---|:-:|:-:|
| `examples/hello_devtool.cc` | 修改 | +10 / -3 | 否 |

**改动位置：** 替换 ≈L213-220 的 perf_hooks 安装段（详 design §4.3.2）：

```cpp
// 替换原 static int s_perf_smoke_frames = 0; + perf_hooks.on_frame_end lambda
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
// ...

// 同步替换 PERF SMOKE 输出（≈L270）：
std::printf("PERF SMOKE: frames=%d hud_visible=%d\n",
            s_perf_ud.frames, vx_view_is_hud_visible(view));
if (s_perf_ud.frames < 1) {
  std::fprintf(stderr,
               "ERROR: pipeline hooks did not fire — perf overlay broken\n");
}
```

**注意：** s_perf_smoke_frames 重命名为 s_perf_ud.frames / 保留向后兼容输出格式 / commit body 显式标注 PERF SMOKE 字串契约不变

**TDD：**

1. cmake -B build-sdl2 -DVX_PLATFORM_SDL2=ON -DVX_BUILD_DEVTOOL=ON（如未配）
2. cmake --build build-sdl2 --target hello_devtool
3. ctest -R hello_devtool_perf_smoke -V → 当前 regex 仍 PASS（frames ≥1 含 ≥2 子集）

**C.1 估时：** ~15 min plan ×0.6 / 实测期待 ~3-5 min

---

## Phase D — ctest regex 升级 + 双 config full ctest（~10 min）

### Phase D.1 — hello_devtool_perf_smoke regex 升级

**文件变更：**

| 文件 | 操作 | 行数 | 共享文件？ |
|---|---|:-:|:-:|
| `tests/CMakeLists.txt` | 修改 | ±5（注释 + regex）| **是 [共享文件]** |

**操作：** 替换 ≈L412-420（详 design §4.3.3）：

```cmake
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

**TDD：**

1. cmake build-sdl2 reconfigure
2. ctest -R hello_devtool_perf_smoke -V → expect frames=10+ → PASS ✅
3. **反向探针：** 临时把 vx_view_invalidate 调用注释 → ctest expect FAIL（frames=1 失配 regex）→ 验证 regex 灵敏度

4. **commit `test(devtool): hello_devtool_perf_smoke multi-frame upgrade (TASK-20260505-02 D.1)`**

**D.1 估时：** ~10 min plan ×0.6 / 实测期待 ~2-3 min

### Phase D.2 — 双 config full ctest

**操作：**

```bash
# DEVTOOL=ON full ctest
cmake --build build --target tests
cd build && ctest --output-on-failure --parallel 8 2>&1 | tail -20

# DEVTOOL=OFF full ctest
cmake --build build-no-devtool --target tests
cd build-no-devtool && ctest --output-on-failure --parallel 8 2>&1 | tail -20
```

**期望：**
- DEVTOOL=ON: 1298 → **1302** (+4 PASS / 100%)
- DEVTOOL=OFF: 1105 → **1109** (+4 PASS / 100%)
- A14 link-closure 通过（vx_view_invalidate 是公开 ABI / 不属 DevTool subsystem）

**D.2 估时：** ~10-15 min plan ×0.6 / 实测期待 ~5-8 min（含 build 时间）

---

## Phase E — Spec 同步 + 收尾（~10 min）

### Phase E.1 — MVP-scope spec §3.2.1 B-G4 闭环

**文件变更：**

| 文件 | 操作 | 行数 |
|---|---|:-:|
| `docs/specs/2026-05-04-mvp-scope.md` | 修改 | ±15 |

**4 处改动：**

1. **§3.1 表 B.9 行：** ⚠️ 部分 → ✅ + 备注「[TASK-20260505-02 闭环]」
2. **§3.2.1 表 B-G4 行：** strikethrough + ✅ 闭环 + commit 引用
3. **§3.2.1 「MVP-B 完成度」段：** ~95% → **100%**
4. **§3.3 路线图「短期」段 #2：** strikethrough + ✅ 已闭环

**TDD：**

- 文档改动 / 无单测
- 中文文档 StrReplace 字符类型 audit（反复模式 #5）— 半角/全角字符确认
- grep `B-G4` 验证所有 mentions 已同步

**E.1 估时：** ~10 min plan ×0.6 / 实测期待 ~3-5 min

### Phase E.2 — Memory Bank 更新

**文件变更：**

| 文件 | 操作 |
|---|---|
| `memory-bank/activeContext.md` | 更新阶段 → 构建中 / 写入 build 闭环数据 |
| `memory-bank/tasks.md` | 当前任务行 build 闭环 |
| `memory-bank/progress.md` | Build 阶段 5 phase 详细记录 + 反复模式预防 7+1=8 项核对 |

**E.2 估时：** ~5 min plan ×0.6 / 实测期待 ~2-3 min

### Phase E.3 — 单 collateral commit `chore(memory-bank): TASK-20260505-02 build phase progress sync`

---

## 反复模式预防清单核对（8 项 — 含 #8 spec 数据回归 / TASK-20260505-01 入库）

| # | 已知反复模式 | 本任务预防策略 |
|:-:|---|---|
| #1 | 前置依赖/环境/API 能力未验证 | ✅ Phase 0 audit 11 子段先跑 / 0.4 EnsureUpdateManager 时序 + 0.5 lazy attach 范式实证 |
| #2 | spec 数据回归（实现 vs 文档不一致）| ✅ §3.2.1 B-G4 状态本任务即同步对齐（实施后 ⚠️ 部分 → ✅ 闭环 / Phase E.1）|
| #3 | TDD 顺序倒置 | ✅ Phase A.1 严格 RED 编译 fail → GREEN 实现 → REFACTOR / 反向探针每子段必跑 |
| #4 | 反向探针缺失或弱 | ✅ Phase A.1 4 项反向探针 + Phase D.1 ctest regex 反向探针 |
| #5 | 中文文档 StrReplace 字符类型 audit | ✅ Phase E.1 同步 spec 时半角/全角字符确认 + grep `B-G4` 全 mentions 核对 |
| #6 | commit body Source 溯源缺失 | ✅ 5 commits 全含 `Source: docs/plans/2026-05-05-perf-overlay-invalidate-api.md §X.Y` |
| #7 | 双 config ctest 单次测验证盲区 | ✅ Phase D.2 双 config full ctest（DEVTOOL=ON 1302 + DEVTOOL=OFF 1109）|
| #8 | spec 数据回归 audit 协议（VAN 阶段 dual-evidence 升级三 evidence）| ✅ VAN 阶段已对 spec §3.2.1 B-G4 行核对 + Phase 0 audit 0.1 暴露引擎不支持 CSS animation（spec 数据回归 #2 第 2 次命中）/ 修正决策为路径 (a) |

**预期反复模式命中：** 0/8 / 全抑制（8 个抑制策略明确 + Phase 0 audit 已暴露所有已知陷阱）

---

## 时间估算汇总

| Phase | 任务 | plan ×0.6 | 实测期待 |
|:-:|---|:-:|:-:|
| Phase A.1 | API 声明 + 实现 + 4 单测（TDD 严格序）| ~30-45 min | ~5-7 min |
| Phase B.1 | tests/CMakeLists.txt 注册 invalidate_api_test | ~10 min | ~2-3 min |
| Phase C.1 | hello_devtool.cc 注入 vx_view_invalidate | ~15 min | ~3-5 min |
| Phase D.1 | hello_devtool_perf_smoke regex 升级 | ~10 min | ~2-3 min |
| Phase D.2 | 双 config full ctest 验证 | ~10-15 min | ~5-8 min |
| Phase E.1 | MVP-scope spec §3.2.1 B-G4 闭环 | ~10 min | ~3-5 min |
| Phase E.2 | Memory Bank 更新 | ~5 min | ~2-3 min |
| Phase E.3 | finalize commit | ~5 min | ~1-2 min |
| **总计** | **8 phase** | **~95-115 min** | **~23-36 min** |

**预期 plan ×0.6 比值：** ~0.20-0.31×（落「最小代码改动 + Phase 0 高度预跑极速区 0.10-0.20×」候选续延 / 第 5-6 次命中）

**ctest 期望矩阵：**

| Config | Baseline | 实施后 | 增量 |
|:-:|:-:|:-:|:-:|
| DEVTOOL=ON | 1298 | 1302 | +4 PASS |
| DEVTOOL=OFF | 1105 | 1109 | +4 PASS |

---

## 完成定义（DoD）

- [ ] vx_view_invalidate(VxView*) C ABI 公开 / Doxygen 文档完整
- [ ] Application::Invalidate() 公开方法
- [ ] 4 API 单测（含反向探针锚定 4 项）
- [ ] hello_devtool.cc 升级（PerfSmokeUd struct）
- [ ] hello_devtool_perf_smoke regex 升级（frames ≥2）
- [ ] DEVTOOL=ON 1302/1302 PASS
- [ ] DEVTOOL=OFF 1109/1109 PASS
- [ ] dogfood smoke 14/14 PASS（hello_devtool_perf_smoke 升级到 multi-frame）
- [ ] spec §3.2.1 B-G4 ✅ 闭环 + MVP-B 完成度 95% → 100%
- [ ] 5 commits 全含 Source 溯源
- [ ] reflection + archive 闭环
- [ ] feature 分支合并到 main + 删除

---

## 需要 `/creative` 阶段？

**❌ 不需要。** D1+D2+D3+D4 全部 plan brainstorming 阶段锁定 / 无新 UI/算法/架构组件。直接进入 `/build`。
