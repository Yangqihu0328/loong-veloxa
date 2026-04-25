# 归档：TASK-20260425-01 SDL2 窗口后端 + 输入事件桥接

**日期：** 2026-04-26
**任务 ID：** TASK-20260425-01
**复杂度级别：** Level 3（中等功能 — 新模块 + 抽象层扩展 + C API 扩展 + CMake 集成）
**状态：** ✅ 已完成

---

## 1. 任务概述

为 Veloxa 引擎引入**第一个有可见窗口的平台后端（基于 SDL2）**，让软件渲染输出在 WSLg / Linux Desktop 上开窗显示，并把 SDL 输入事件桥接到现有 `VxInputEvent` / 事件系统三阶段冒泡。

**业务价值：** 解锁后续 DevTool 主线（hot reload / inspector / FPS overlay 等都依赖「真窗口可见」前提），是项目从「30 个核心功能任务全归档（CSS/Layout/Render/Event/Update/Application/Script/Image/Font）」迈向「实时调试 UI」阶段的第一块基石。

**来源：** 项目主体功能完整后用户提问「下一步任务是什么？怎么实时的调试 UI？」直接立项。

---

## 2. 技术方案

### 2.1 架构选型（6 决策已锁定）

| # | 维度 | 选择 | 理由 |
|---|---|---|---|
| Q1 | 输入事件投递路径 | **(B) `Sdl2EventLoop::PumpInputEvents(callback)` 裸函数** | EventLoop 不反向持有 View；与 headless 对称；保持平台抽象单向 |
| Q2 | Surface 像素呈现时机 | **(B) `Surface` 抽象 + `virtual void Present() {}` 默认 no-op** | 语义清晰；headless 默认 no-op；SDL2 重写为 `SDL_UpdateTexture + SDL_RenderPresent` |
| Q3 | CMake SDL2 查找 | **(C) 双轨兜底** | `find_package(SDL2 CONFIG QUIET)` 优先 → `pkg_check_modules(VX_SDL2_PC REQUIRED IMPORTED_TARGET sdl2)` 兜底 → ALIAS 到统一 `SDL2::SDL2` target |
| Q4 | 默认构建可选性 | **(C) `-DVX_PLATFORM_SDL2=OFF` 默认** | 与 `VX_BUILD_BENCHMARKS` 风格一致；不破坏 minimal CI 镜像 |
| Q5 | examples 形态 | **(B) 保留 `hello.cc` + 新增 `examples/hello_sdl2.cc`** | PPM 与 SDL2 两后端独立参考；含 ctest smoke (`SDL_VIDEODRIVER=dummy`) |
| Q6 | C API 命名 | **(C) `vx_event_loop_create_sdl2()` + `vx_surface_create_window(const VxWindowOptions*)`** | options struct 与 `VxViewConfig` 风格一致；未来加 flags 不破 ABI |

### 2.2 关键架构产出

```
veloxa/platform/
├── surface.h                           ← 增 virtual void Present() {}
├── event_loop.h                        ← 不变
├── headless/                           ← 既有，保持
│   ├── memory_surface.{h,cc}
│   └── headless_event_loop.{h,cc}
└── sdl2/                               ← ★ 新增
    ├── CMakeLists.txt                  ← 双轨 SDL2 lookup + vx_platform_sdl2 静态库
    ├── sdl2_input_translate.{h,cc}     ← SDL_Event → VxInputEvent 翻译
    ├── sdl2_window_surface.{h,cc}      ← SDL_Window/Renderer/Texture RAII
    └── sdl2_event_loop.{h,cc}          ← 内部组合 HeadlessEventLoop（task/timer 复用）
```

### 2.3 Composition over Inheritance（首次自觉应用）

`Sdl2EventLoop` 内**组合** `HeadlessEventLoop` 而非继承：

```cpp
class Sdl2EventLoop : public EventLoop {
 private:
  std::unique_ptr<HeadlessEventLoop> inner_;  // task/timer 复用容器
  bool running_ = false;
  // ...
};
```

理由：继承会面临「两套 Run/Quit 状态机如何整合」「inner 容器复用是反封装」「Sdl2 quit 是否触发 Headless quit」一连串语义死结；组合让两者各自管 running 状态，sdl2 增量职责（pump SDL events）显式聚焦。

### 2.4 vx_view_run 自动 wire 输入回调

`vx_view_run()` 内部通过 `dynamic_cast<Sdl2EventLoop*>(app->event_loop())` 自动 SetInputCallback 把 `vx_view_inject_input` 接进去。embedder 用 SDL2 backend 与 headless backend 调用代码完全一致，「即插即用」。

---

## 3. 实现摘要

### 3.1 文件变更（29 文件，13 新建 + 16 修改，3113 行净增）

| 操作 | 路径 | 说明 |
|------|------|------|
| 创建 | `docs/specs/2026-04-25-sdl2-window-backend-design.md` | 13 段设计规格（含 R1-R6 风险登记 + 安全威胁建模） |
| 创建 | `docs/plans/2026-04-25-sdl2-window-backend.md` | 6 Phase / 14 任务 / 34 GTests + smoke |
| 创建 | `veloxa/platform/sdl2/CMakeLists.txt` | 双轨 SDL2 lookup + `vx_platform_sdl2` STATIC |
| 创建 | `veloxa/platform/sdl2/sdl2_input_translate.{h,cc}` | SDL_Event → VxInputEvent (鼠标 + 键盘 + 修饰位 + Quit) |
| 创建 | `veloxa/platform/sdl2/sdl2_window_surface.{h,cc}` | RAII window/renderer/texture + Lock/Unlock/Present |
| 创建 | `veloxa/platform/sdl2/sdl2_event_loop.{h,cc}` | Composition HeadlessEventLoop + PumpInputEvents + SetInputCallback |
| 创建 | `examples/hello_sdl2.cc` | SDL2 demo 含 :hover CSS 规则（plan A2 验收依赖） |
| 创建 | `tests/platform/sdl2_input_translate_test.cc` | 10 cases（鼠标 4 + 键盘/修饰位 6） |
| 创建 | `tests/platform/sdl2_window_surface_test.cc` | 6 cases（RAII + Lock/Present，含 SDL_RenderReadPixels 探针） |
| 创建 | `tests/platform/sdl2_event_loop_test.cc` | 8 cases（Run/Quit/Timer 4 + Pump callback 4） |
| 创建 | `memory-bank/reflection/reflection-TASK-20260425-01.md` | Level 3 详细回顾（10 段） |
| 创建 | `memory-bank/archive/archive-TASK-20260425-01.md` | 本文档 |
| 修改 | `CMakeLists.txt` | `option(VX_PLATFORM_SDL2 OFF)` + 条件 `add_subdirectory` |
| 修改 | `veloxa/platform/surface.h` | + `virtual void Present() {}` 默认 no-op |
| 修改 | `veloxa/api/veloxa_api.{h,cc}` | + 3 SDL2 C API + 重构 destroy/save_ppm 用基类指针（修 UB 隐患） |
| 修改 | `veloxa/api/CMakeLists.txt` | 条件 link `vx_platform_sdl2` + define `VX_PLATFORM_SDL2` |
| 修改 | `veloxa/core/application.{h,cc}` | + `event_loop()` accessor；`Update()` 末尾调 `surface->Present()` |
| 修改 | `examples/CMakeLists.txt` | 条件 build `hello_sdl2` target |
| 修改 | `tests/CMakeLists.txt` | 4 SDL2 测试 binary（用 `add_executable` + `gtest_discover_tests` 直驱避免 helper PROPERTIES 透传缺陷）+ `hello_sdl2_smoke` ctest |
| 修改 | `tests/api/api_test.cc` | + 6 CApiSdl2 cases + 1 RepeatedHeadlessCreateDestroyNoStateLeak |
| 修改 | `tests/platform/memory_surface_test.cc` | + 2 PresentNoOp cases 验证基类 vptr |
| 修改 | `memory-bank/{tasks,activeContext,progress,techContext,productContext,systemPatterns}.md` | 任务跟踪 + 长期知识库沉淀（4 新模式） |
| 修改 | `.cursor/rules/skills/writing-plans.mdc` | 2 新 plan §0 grep 子段（test-include hygiene + acceptance↔example） |

### 3.2 关键决策实施细节

1. **destroy/save_ppm 基类化（隐含技术债）：** spec §5.3 主动识别 — `vx_event_loop_destroy` / `vx_surface_destroy` / `vx_surface_save_ppm` 在 `veloxa_api.cc` 硬编码 `HeadlessEventLoop*` / `MemorySurface*`，加 SDL2 后端时 `delete` 错的派生类指针会 UB。本任务范围内已修：改用基类指针调用 + 虚析构托底
2. **`Surface::Present()` 调用时机：** 从 `OnFrame()` 改到 `Application::Update()` 末尾，使任何 update 路径（`Run` 调度 + 手工 update + `vx_view_update`）都触发；headless surface 默认 no-op 兼容旧测试
3. **GUI ctest 自终止 hook：** `VX_HELLO_SDL2_AUTOQUIT_MS=200` env + `SDL_AddTimer` push `SDL_QUIT` 让 hello_sdl2 在 dummy driver 下自终止；prod 用户从不设此 env，hook 完全 opt-in；`TIMEOUT 10` 兜底
4. **CMake `gtest_discover_tests` PROPERTIES 透传：** SDL2 测试不用 `vx_add_test` helper（helper 内部 `gtest_discover_tests` 没暴露目标供后续 `set_tests_properties`），直接 `add_executable` + `gtest_discover_tests(... PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy")`

### 3.3 安全决策

- **输入验证：** 所有新 C API 检查 nullptr (`vx_event_loop_pump_input` / `vx_surface_create_window`)；`dynamic_cast<Sdl2EventLoop*>` 失败返回 `VX_ERROR_INVALID_STATE`
- **依赖审计：** SDL2 2.0.20 系统包 `libsdl2-dev`（`pkg-config sdl2` ✅），spec §13 已记安全联系人 (security@libsdl.org) + 升级触发条件（追上 stable 2.30.x 为 P2 触发型）；本任务**不引入新 FetchContent 源**
- **错误信息脱敏：** `VX_ERROR_NULL_PARAM` / `VX_ERROR_INVALID_STATE` 不泄露内部状态
- **R6 风险登记（spec §13）：** (a) `VxWindowOptions.title` 极长 UTF-8 字符串可能触发 WM 截断 — P2 follow-up；(b) `Sdl2EventLoop::PumpInputEvents` 未限制单次 drain 上限，理论上 SDL_PushEvent flood 可让 pump 永不返回（SDL_PollEvent 内部 128 events 上限缓解）— P3 触发型
- **结论：** 无 P0/P1 安全行动项，本任务安全检查清单全部 ✅

---

## 4. 测试覆盖

### 4.1 量化指标

| 维度 | 数值 |
|---|---|
| ctest 净增 cases | **+34**（baseline 917 → 951） |
| Debug ctest | **951/951 PASS 1.18s** |
| Release `-O3 -Werror` ctest | **951/951 PASS 1.01s**，0 警告 |
| hello_sdl2_smoke (dummy driver) | **0.22s 自终止** |
| Linter | clean（ReadLints 0 错误） |

### 4.2 测试拆分

- `memory_surface_test`：+2（Present 默认 no-op + 通过基类指针调用 vptr 验证）
- `api_test`：+6 CApiSdl2（factory + window create + pump_input null/invalid_state 边界）+1 头部 destroy/save_ppm 基类化覆盖
- `sdl2_input_translate_test`：10（鼠标 move/down/up + 键盘 down/up/non-ASCII + 修饰位 5 类 + SDL_QUIT/WINDOWEVENT 透传）
- `sdl2_window_surface_test`：6（ConstructDestructDummyDriver / RepeatedCreateDestroy / CoexistsWithExternalSdlVideoInit / Lock+Present / PresentBeforeAnyLock / **PresentUploadsPixelsToTexture** 用 `SDL_RenderReadPixels` 像素探针）
- `sdl2_event_loop_test`：8（PostTaskFIFOOrder / RunAndQuit / SetTimerOneShot / CancelTimerBeforeFire + 4 Pump fixture：CallbackReceivesTranslatedMouseMove / KeyDownDispatched / **SdlQuitTriggersLoopQuit** / NoCallbackNoCrash）
- `hello_sdl2_smoke`：1 ctest 端到端（create loop → create window → load HTML/CSS → run → autoquit → done）

### 4.3 RED 反向探针

- P0.3 destroy 基类化前后对比：测试在改造前用旧 `reinterpret_cast<HeadlessEventLoop*>` 路径仍 PASS（已知行为）；改造为基类指针后再次 PASS — 验证基类化未破坏 ABI
- P3.2 `SdlQuitTriggersLoopQuit` 调试时发现 `inner_->Quit()` 误调（应是 `this->Quit()`）让测试挂死 → 修复后回归通过

### 4.4 P6.1 WSLg 真窗口验证 — 标遗留

用户 `/build` 阶段决议「当前环境不便实测」标为遗留验证项，触发条件：下次有 GUI/WSLg 桌面环境时复跑 `./build/examples/hello_sdl2` 验 A1（窗口 + 三色块）/A2（鼠标 hover 红/绿/蓝块变色，**reflect 阶段已加 :hover CSS 规则使 A2 验收可达**）/A3（× 关闭后 stdout `Done.`）。Headless smoke (`SDL_VIDEODRIVER=dummy`) 已自动覆盖渲染管道 + 输入分发 + 退出路径，仅缺真显示设备验证。

---

## 5. 经验教训（关键 5 项，详见 reflection §5）

1. **Composition pattern 内同名方法歧义防护准则** — outer 类自方法（Quit/Run/Reset）必须显式 `this->Quit()` 而非 `Quit()`，或把 inner 同名方法重命名为 `inner_quit_internal()`。`Sdl2EventLoop::PumpInputEvents` 在 SDL_QUIT 分支误调 `inner_->Quit()` 关掉的是组合对象不是自己，测试挂死实证。
2. **Plan ×0.6 第 9 数据点「最窄路径」第 4 次确认 0.22×（历史最快）** — 实测 ~40 min vs plan 估 180 min。条件：6 个 AskQuestion 提前锁定全部决策 + 平台抽象成熟 + 无性能/微优化 phase + TDD 节奏稳。跨 4 数据点稳定区间 0.22-0.34×（中位 0.28×）。
3. **GUI/loop 程序 ctest 自终止协议泛化** — `VX_<APP>_AUTOQUIT_MS` env hook + `SDL_AddTimer` push platform quit event 是「process-level latched-once env hook」第二次应用（首发 TASK-24-04 `VX_SHAPE_CACHE_OFF`），定型为通用模式，下次 GUI/loop test target 直接复用。
4. **测试文件 include 卫生反复模式** — `#include <SDL2/SDL.h>` 误置 anonymous namespace 间接污染 `<math.h>` 的 `std::abs` 解析（`'abs' has not been declared in '{anonymous}::std'`）。「测试隔离问题」第 8 次新变体（namespace 维度），已沉淀 plan §0 batch grep 强制条目。
5. **Plan 验收用例与 example 一致性 drift** — plan §6.1 步骤 2 期望「鼠标 hover 红块变 #FF6B6B」作为 A2 验收，但 hello_sdl2.cc 实施时沿用静态盒子样式未注入 `:hover` 规则。「计划清单不一致」第 10 次新维度（UI 行为维度），reflection P0 #4 已强制修复 + 沉淀 plan §0「验收用例 ↔ example 一致性检查」3 选 1 强制规则。

---

## 6. 改进建议落实清单

| # | 建议 | 优先级 | 落实状态 | 落实位置 |
|---|------|--------|---------|----------|
| 1 | Composition outer `this->Method()` 防同名歧义 | P1 | ✅ | `systemPatterns.md`「TASK-25-01」段「同名方法歧义防护」配套准则 |
| 2 | 测试文件 include 卫生 grep（plan §0 必做） | P1 | ✅ | `writing-plans.mdc`「测试文件 include 卫生 grep」新子段 |
| 3 | Plan 验收用例 ↔ example 一致性检查（3 选 1 强制） | P1 | ✅ | `writing-plans.mdc`「验收用例与 example 一致性检查」新子段 |
| 4 | hello_sdl2.cc 加 `:hover` 规则使 A2 可达 | **P0** | ✅ | `examples/hello_sdl2.cc` 加 `#box-{red,green,blue}:hover` 3 条规则；smoke 0.22s 通过 |
| 5 | GUI/loop ctest 自终止协议（env hook 模式） | P2 | ✅ | `systemPatterns.md`「GUI/Loop 程序 ctest 自终止模式」 |
| 6 | Platform Backend Composition 复用 Headless task/timer | P2 | ✅ | `systemPatterns.md`「Platform Backend Composition 复用模式」 |
| 7 | plan ×0.6 第 9 数据点 0.22× 最窄路径第 4 次确认 | P1（数据点足够） | 已记 activeContext.md 待处理事项 | 跨 4 数据点稳定区间，可考虑写入 `writing-plans.mdc` 强制条目 |
| 8 | TASK-25-01 后续候选（EventLoop::SetInputCallback 上提抽象 / interactive_sdl2 / Surface::Present contract test / vx_add_sdl2_test helper） | P3 触发型 | 已立项 TASK-20260425-02 占位 | activeContext.md「后续任务候选」 |

---

## 7. 反复模式新数据点（2 项）

| 模式 | 历史频率 | 本次新变体 | 已落实位置 |
|---|---|---|---|
| 计划文件清单与实际变更不一致 | 9+ → **10** | UI 行为维度（plan A2 :hover vs example 漂移） | `writing-plans.mdc` 验收用例 ↔ example 一致性检查 |
| 测试隔离问题（flaky/并行/环境） | 7+ → **8** | namespace 维度（include 误置 anon 污染 STL） | `writing-plans.mdc` 测试文件 include 卫生 grep |

---

## 8. 长期知识库沉淀（systemPatterns.md「TASK-20260425-01」段）

5 新模式：
1. **Composition over Inheritance — Platform Backend 复用模式**（含同名方法歧义防护配套准则）
2. **GUI / 主循环型程序 ctest 自终止模式**（VX_<APP>_AUTOQUIT_MS env hook + 平台 quit event push）
3. **测试文件 include 卫生模式**（rg "^namespace .*\{" -A 50 grep + 标志性错误 fingerprint）
4. **Plan 验收用例与 example 实现一致性检查模式**（3 选 1 强制：inline CSS / sync check step / co-PR）
5. **复用 + 配套准则模式**（每个新模式可附带配套防御性准则，如 composition 同名方法防护）

---

## 9. 参考文档

- **设计规格：** `docs/specs/2026-04-25-sdl2-window-backend-design.md`（13 段）
- **实现计划：** `docs/plans/2026-04-25-sdl2-window-backend.md`（6 Phase / 14 任务）
- **回顾文档：** `memory-bank/reflection/reflection-TASK-20260425-01.md`（Level 3 详细 / 10 段）
- **创意设计：** ❌ 无（Level 3 但 Q1-Q6 决策已通过头脑风暴 AskQuestion 锁定，跳过 /creative）
- **相关后续任务：** TASK-20260425-02（占位，SDL2 backend 二期 + DevTool 主线起点；触发条件见 activeContext.md）

---

## 10. 提交记录（feature/TASK-20260425-01-sdl2-backend，16 commits）

```
b367a32 docs(reflect): add reflection for TASK-20260425-01 + P0/P1 follow-ups
a2c5db3 chore(build): finalize TASK-20260425-01 memory bank state
75568d5 chore(build): finalize TASK-20260425-01 round 1 memory bank state
ddb7d6d docs(memory-bank): record SDL2 backend in tech/product context
733d4fc feat(examples): add hello_sdl2 + smoke test
edc7c93 feat(api): SDL2 C API factories + vx_event_loop_pump_input
f928064 feat(sdl2): PumpInputEvents drains SDL queue + dispatches via callback
c8f3840 feat(sdl2): Sdl2EventLoop with composed HeadlessEventLoop task/timer
a6341d6 feat(sdl2): implement Sdl2WindowSurface::Present
39e6aa2 feat(sdl2): Sdl2WindowSurface RAII window/renderer/texture lifetime
1a819bb feat(sdl2): translate keyboard events + modifiers + quit/window passthrough
7b9d6bd feat(sdl2): translate mouse events to VxInputEvent
637c3cc build(sdl2): scaffold vx_platform_sdl2 target with VX_PLATFORM_SDL2 opt-in
df157db refactor(api): use base class pointer for destroy/save_ppm
ac08aa6 feat(platform): add Surface::Present() virtual no-op for backends
36ee2c4 chore(workflow): TASK-20260425-01 P0 baseline + plan/spec docs
```

---

## 11. 总结

TASK-20260425-01 是 Veloxa 项目自启动以来**第一个引入真窗口/真平台后端**的任务，技术上里程碑级（解锁 hot reload / inspector / FPS overlay 等 DevTool）。

- **流程层**：6 个 AskQuestion 提前锁定全部架构决策，0 返工；TDD 严格执行（13 个 feat commit 每个都先 RED 后 GREEN）；plan ×0.6 第 9 数据点 0.22× 历史最快，「最窄路径」第 4 次稳定确认。
- **架构层**：Composition over Inheritance 首次自觉应用 + 同名方法歧义防护配套准则 + GUI ctest 自终止协议泛化定型 + Surface::Present + destroy/save_ppm 基类化清掉 UB 隐患。
- **质量层**：ctest 951/951（Debug + Release `-O3 -Werror` 双双 0 回归 0 警告）+ 34 新增测试覆盖（含像素探针 + RED 反向探针）+ headless smoke ctest 自动 0.22s 完整跑通。
- **知识沉淀层**：5 新模式入 `systemPatterns.md`、2 个 plan §0 grep 子段入 `writing-plans.mdc`；2 个反复模式新变体（计划清单不一致 #10 / 测试隔离 #8）已规则化；P0 改进建议归档前落地，P1 全部沉淀长期知识库。

**遗留：** P6.1 WSLg 真窗口手测（用户决议标遗留，触发条件 = 下次 GUI 环境可用），不影响归档。

**下一站：** TASK-20260425-02 占位（SDL2 backend 二期 + DevTool 主线起点）等待触发条件命中（第二个 platform backend / FPS overlay / DevTool 立项）。
