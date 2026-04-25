# 回顾：TASK-20260425-01 SDL2 窗口后端 + 输入事件桥接

**日期：** 2026-04-26
**任务 ID：** TASK-20260425-01
**复杂度级别：** Level 3（中等功能 — 引入新模块 + 抽象层扩展 + C API 扩展 + CMake 集成）
**分支：** `feature/TASK-20260425-01-sdl2-backend`（基于 main `e52868b`，15 commits）

---

## 1. 计划 vs 实际

| 维度 | 计划 | 实际 | 偏差原因 |
|------|------|------|---------|
| Phase 数 | 6（P0-P6） | 6（同） | — |
| 子任务数 | 14（含 P6.1-P6.3） | 14（含 P6.1 标遗留） | — |
| 预估时间（plan×0.6） | 180 min | **~40 min**（首次 build 起 commit 00:20 → 收尾 00:59，含 P6.2 release 90s 配置 + 78s 编译开销） | **0.22×**「最窄路径」第 4 次确认 |
| 新增文件 | 7（4 src + 3 test）+ 2 docs + 1 example | **13 新建 + 16 修改 = 29 files** | 与 plan 一致（plan 列了 4 src + 3 test + 1 cmake + 1 example + 2 docs ≈ 11，实测多出 cmake 子目录 1 个 + tests/CMakeLists 修改 + memory-bank 4 文件） |
| 净 LOC | ~2300（plan §0 估算） | **+3113 / -9 = 3104** | spec/plan 各 ~700 LOC + memory bank ~400 LOC + 实际代码 ~1300 LOC，与估算 1.35× 偏差小 |
| 新增测试 | 13 + smoke = 14 | **34 cases**（10 input + 6 surface + 8 loop + 6 api SDL2 + 2 Present base + 1 destroy 基类化 + 1 smoke ctest） | 实测每个测试用例都拆得更细，单测试覆盖更明确（plan 把多 EXPECT_EQ 计为 1 case） |
| 设计变更 | — | **3 处微调** | 见下表 |
| Commits | plan 未指定数量 | **15**（每 phase 1-2 个，符合 §3 TDD commit 节奏） | — |
| ctest baseline | 917/917 | **951/951**（+34 净增，0 回归） | — |
| Release `-O3 -Werror` | 0 errors / 0 warnings | **0 errors / 0 warnings** | ✅ |

### 设计微调（实证偏离 plan）

| # | 项 | plan 写法 | 实际 | 原因 |
|---|---|---|---|---|
| 1 | example 主循环 | plan §5.1 给出手工 pump + SDL_PollEvent 二次轮询 SDL_QUIT 的脏循环（25 行） | 改为 `vx_view_run()` 一行 + 在 `vx_view_run` 内 `dynamic_cast<Sdl2EventLoop>` 自动 wire `SetInputCallback` → `vx_view_inject_input` | 实施时发现：plan 的脏循环源于 `Sdl2EventLoop::PumpInputEvents` 已经消费 SDL_QUIT，example 没有 quit 信号回传通道；正解是让 `Sdl2EventLoop::Run()` 自管循环（已实现），example 只用 `vx_view_run()` 即可。**第三次实证微调 spec 范式** |
| 2 | `Application::Update()` 末尾调 `surface->Present()` | plan 假设 `OnFrame()` 路径调 Present | 改为 `Update()` 调（含 OnFrame() → Update()） | 因 example 用 `vx_view_run` → `Run()` → 内部 timer fire → `OnFrame()` 路径，但若 embedder 走手工 update 路径（hello_sdl2 备选）也需要 Present。统一到 Update 末尾保证幂等 |
| 3 | `build-release-sdl2/` 目录名 | plan §6.2 步骤 1 | 用 `build-release/` | 命名简化，无功能差异 |
| 4 | A2 验收 :hover 颜色变化（**plan 偏差未实施**） | plan §6.1 步骤 2 期望红块 hover → `#FF6B6B` | hello_sdl2.cc **没有 `:hover` CSS 规则**，A2 实际只能验「鼠标移动不崩溃」 | example 沿用 hello.cc 静态盒子样式；未注入 hover 状态。如果用户实测 P6.1 时按 plan §6.1 步骤 2 期待颜色变化，会得到「未变」的负面验收 → 需在归档前修正 plan/progress 的 A2 期望表述（详见改进建议 #4） |

---

## 2. 回顾检查清单

### 代码变更类
- [x] 计划精确度 — 文件清单与实际变更**一致**（多出 `veloxa/platform/sdl2/CMakeLists.txt` 在 plan §1.1 步骤 3 已列；其他都对得上）
- [x] TDD 执行情况 — **严格 RED→GREEN→REFACTOR**，每个新模块的核心行为都有先失败后通过的过程；P0.3 用「写测试覆盖现状 → 改造 → 复跑」模式（不是新代码不是 RED）已合理标注
- [ ] 子代理质量 — **本任务未使用子代理**，全部主线程串行
- [x] 测试隔离 — 出现 **2 处隔离故障并已修复**：
  - api_test.cc 把 `#include <SDL2/SDL.h>` 写进匿名 namespace 污染了 `<math.h>` 的 `std::abs` 解析（修复：移到 file scope）
  - sdl2_input_translate_test 之初用 `vx_add_test` helper 与 `gtest_discover_tests` 名称冲突（修复：直接 `add_executable` + `gtest_discover_tests` 加 `PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy"`）
- [x] 提交粒度 — **15 commits，每 phase 1-2 个**，符合 git-workflow.mdc 节奏，无大杂烩
- [ ] 非默认路径 — Surface::Present 在 SDL2 backend 用 `SDL_RenderReadPixels` 探针验证像素被上传（P2.2 测试）；error 路径（OOM、SDL_Init 失败）只覆盖了「opts == nullptr 返回 nullptr」的 C API 边界，未覆盖 SDL_CreateWindow 真失败场景（spec §5.4 已记为 follow-up）

### 配置/规则类
- [x] 文件位置验证 — Phase 0 grep 确认 EventLoop / Surface 抽象类位置 ✅
- [x] 交叉引用 — techContext.md 加 SDL2 行 + Platform Backends 段；productContext.md 同步加 SDL2 ✅ 行；progress.md 里程碑表全程同步

### 安全相关
- [x] 输入验证 — `vx_event_loop_pump_input(loop, view)` null 检查；`vx_surface_create_window(opts)` 检查 `opts == nullptr`；`dynamic_cast<Sdl2EventLoop*>` 失败返回 `VX_ERROR_INVALID_STATE`
- [N/A] 认证/授权 — 不涉及（本地引擎库）
- [N/A] 数据保护 — 不涉及
- [x] 依赖审计 — SDL2 2.0.20 系统包 `libsdl2-dev`，未引入 FetchContent 新源；spec §13 已记 SDL2 安全联系人 (security@libsdl.org) + 2.0.20 与最新稳定 2.30.x 差异为后续 P2 升级触发条件
- [x] 错误信息 — C API 错误码 (`VX_ERROR_NULL_PARAM` / `VX_ERROR_INVALID_STATE`) 不泄露内部状态
- [N/A] 敏感数据 — 不涉及

---

## 3. 做得好的

1. **架构决策透明且事后被实证证实正确** — 6 个 AskQuestion 锁定的决策（pump callback / Surface::Present virtual / CMake 双轨 / `VX_PLATFORM_SDL2=OFF` 默认 / 独立 hello_sdl2.cc / options struct）全程 0 返工
2. **「Composition over Inheritance」第一次自觉应用** — `Sdl2EventLoop` 内组合 `HeadlessEventLoop` 复用 PostTask/SetTimer/CancelTimer 而不是继承，避开「两套 Run/Quit 状态机如何整合」的语义难题
3. **技术债主动识别** — Phase 0.3 把 `vx_event_loop_destroy` / `vx_surface_destroy` / `vx_surface_save_ppm` 改用基类指针 + 虚析构修掉，杜绝 SDL2 后端引入后 `delete` 错型 UB；这是 spec §5.3 主动找出的隐藏债，不是被 bug 逼出来的
4. **Test Hook 设计** — `VX_HELLO_SDL2_AUTOQUIT_MS` 环境变量让 GUI example 在 ctest dummy driver 下自终止 200ms，用 `SDL_AddTimer` 推送 `SDL_QUIT`；这种 process-level latched-once env hook 已在 TASK-24-04 用过 1 次（VX_SHAPE_CACHE_OFF），现成模式直接迁移
5. **embedder 零样板** — `vx_view_run` 内部 `dynamic_cast<Sdl2EventLoop*>` 自动 SetInputCallback；用户用 SDL2 backend 与 headless backend 调用方式完全一致，「即插即用」
6. **CMake 双轨 SDL2 lookup** — `find_package(SDL2 CONFIG QUIET)` 优先 + `pkg_check_modules(VX_SDL2_PC REQUIRED IMPORTED_TARGET sdl2)` 兜底；同 cmake config + ALIAS 提供 `SDL2::SDL2` 统一 target；兼容老发行版（Ubuntu 22.04 SDL2 2.0.20 的 cmake config 不带 SDL2::SDL2 target）
7. **极快的实测时长** — build 阶段 ~40 min vs plan ×0.6 估算 180 min（**0.22×「最窄路径」**，是历史最快），原因：6 个 AskQuestion 决策让设计 100% 锁定 + 现有 platform 抽象成熟 + 没有性能/微优化阶段 + TDD 节奏稳

---

## 4. 遇到的挑战

1. **`Sdl2EventLoopPumpFixture.SdlQuitTriggersLoopQuit` 测试挂死** — `PumpInputEvents()` 在 SDL_QUIT 分支误调 `inner_->Quit()`（关掉的是组合的 HeadlessEventLoop，不是自己），导致 `Sdl2EventLoop::Run()` 的 `while (running_)` 永不退出。**根因**：composition pattern 下两个 Loop 都有 running 状态，方法名相同（`Quit`），调错对象编译器不会报；**修复**：改为直接 `Quit()`（this），测试解挂
2. **`api_test` 编译失败 `'abs' has not been declared in '{anonymous}::std'`** — `#include <SDL2/SDL.h>` 不慎写进匿名 namespace，`<SDL_stdinc.h>` 间接拉入 `<math.h>` 时被命名空间污染。**修复**：移到 file scope。**根因**：复制粘贴 anon namespace 内现有 includes 时手滑。**反映 systemPatterns 已有「测试文件全局/匿名 namespace 边界」未规则化**
3. **`gtest_discover_tests` 与 `vx_add_test` helper 冲突** — vx_add_test 调 `gtest_discover_tests` 但不暴露目标名给后续 `set_tests_properties`；导致 SDL_VIDEODRIVER=dummy env 设不上。**修复**：SDL2 测试不用 helper，直接 `add_executable` + `gtest_discover_tests(... PROPERTIES ENVIRONMENT ...)`
4. **`VxInputEvent` 不完整类型** — `sdl2_event_loop.cc` 用 `VxInputEvent translated{}` 但只 forward declare 没 include 公共 header；C 类型在 `vx::platform` namespace 下被误识为该 namespace 类型。**修复**：在 `sdl2_event_loop.h` `#include "veloxa/api/veloxa_api.h"` + 用 `::VxInputEvent` 显式全局空间
5. **plan A2 验收依赖 :hover 但 example 没配置** — plan §6.1 期望 hover 颜色变化，hello_sdl2.cc 实施时沿用 hello.cc 静态盒子（无 :hover）；P6.1 标为遗留没暴露，但**plan 与实施不一致**是事实

---

## 5. 经验教训

### 5.1 Composition pattern 下「自方法 vs 内成员同名方法」的盲区

`Sdl2EventLoop` 组合 `HeadlessEventLoop`，两者都有 `Quit()`、`is_running()` 等同名方法。在 outer 实现里写 `inner_->Quit()` vs `Quit()` 字面差 6 字符，但语义完全不同。**一个隐式准则被发现**：

> **Composition pattern 内严禁同名方法贯通**：outer 类的 `Quit/Run/Reset/...` 等控制方法必须**显式调 `this->Quit()` 而不是 `Quit()`**（`this->` 让代码 reviewer 立即看到「这是自己的 Quit」），或把 inner 类同名方法重命名为 `inner_quit_internal()` 等避免歧义。

→ 落实方式：沉淀到 `systemPatterns.md`「Composition over Inheritance — 同名方法歧义防护模式」段。

### 5.2 第三方 C 库 header 在测试文件内的「namespace 卫生」必须 grep 兜底

`#include` 误置 anon namespace 是 5 秒手滑、5 分钟排查的典型场景。Plan 的 grep 矩阵应包含一条：

```bash
rg "^namespace .*\{" -A 5 tests/ | rg "include " || echo "OK: no include in any namespace"
```

→ 落实方式：追加到 `writing-plans.mdc`「smoke 工具链可用性检查」段「测试文件 include 卫生 grep」子条目，作为 Plan §0 batch grep 一员。

### 5.3 `gtest_discover_tests` 与 helper macro 的目标可见性约束

vx_add_test 是个有用 helper，但它内部直接 `gtest_discover_tests(...)` 没 hook 让外部追加 PROPERTIES。下次写需要 ENVIRONMENT 变量的 GTest，要么扩展 helper 接受可变参数，要么直接走原始 `add_executable` 路径。

→ 落实方式：作为 P2 沉淀到 `systemPatterns.md`「CMake test helper 设计 — PROPERTIES 透传」段；当前任务用「直接 add_executable」绕过即可。

### 5.4 plan 验收用例与 example 实现强耦合时必须同步设计

plan §6.1 步骤 2「红块 hover → #FF6B6B」是验收 A2 鼠标输入端到端的精彩用例，但**这要求 example CSS 包含 `:hover` 规则**。plan 假设了实现细节（且作者自己写 example 时忘了），导致 plan 内部不一致。

→ **新模式**：**plan 验收清单引用具体 UI 行为时（hover/click/keystroke→颜色变化），必须把对应 CSS/HTML/JS 片段直接写进 example 段，或在 plan §X 步骤 0 加一行「同步检查：example 已包含验收所需的 CSS 规则 X」**。

→ 落实方式：追加到 `writing-plans.mdc`「验收用例与 example 一致性检查」子段（新增）。这是反复模式新变体「计划文件清单与实际变更不一致」的 UI 维度延伸。

### 5.5 GUI example 的 ctest 自动化模式 — env hook 推送系统事件

`VX_HELLO_SDL2_AUTOQUIT_MS=200` + `SDL_AddTimer` push `SDL_QUIT` 是个干净的「让 GUI 程序在 CI 自终止」模式。**第二次应用** env hook（首发 TASK-24-04 `VX_SHAPE_CACHE_OFF`），可定型为通用模式：

> **GUI / 主循环型程序的 ctest 自终止协议**：用 `VX_<APP>_AUTOQUIT_MS` env 触发 timer → 推送平台 quit event；prod 用户从不设此变量。

→ 落实方式：沉淀到 `systemPatterns.md`「GUI/Loop 程序 ctest 自终止模式」段。

### 5.6 Composition over Inheritance 是 platform backend 多版本共存的最佳模式

如果 `Sdl2EventLoop` 继承 `HeadlessEventLoop`，会面临「Run/Quit 是否覆盖」「内部 timer 容器是否复用」「Sdl2 quit 是否触发 Headless quit」一连串语义死结。组合方案让两者各自管 running 状态，sdl2 增量职责（pump SDL events）显式聚焦，是 platform 抽象的可持续路径。

→ **下个 backend（Win32/X11/Wayland/Cocoa）应延续此模式**：每个 backend 都内部组合 HeadlessEventLoop 复用 task/timer，避免在 EventLoop 抽象基类硬塞 task/timer 容器（违反单一职责）。

→ 落实方式：沉淀到 `systemPatterns.md`「Platform Backend Composition 复用模式」段。

---

## 6. 改进建议（附优先级与落实方式）

| # | 建议 | 优先级 | 落实方式 | 目标 |
|---|------|--------|---------|------|
| 1 | Composition pattern 内 outer 类自方法显式 `this->Method()` 防与 inner 同名方法混淆 | **P1** | 沉淀 `systemPatterns.md` 新模式段 | 下次组合型类（如 Win32EventLoop）实现时引用 |
| 2 | Plan §0 batch grep 矩阵加「测试文件 include 卫生」条目 | **P1**（反复出现「测试隔离问题」第 8 次新变体） | 追加到 `writing-plans.mdc`「smoke 工具链可用性检查」段 | 下次 Plan §0 自动覆盖 |
| 3 | Plan 验收清单引用具体 UI 行为时必须同步 example CSS/HTML，或加同步检查项 | **P1**（反复出现「计划清单不一致」第 10 次新维度） | 追加到 `writing-plans.mdc`「验收用例与 example 一致性检查」新子段 | 下次涉及 example 的功能 plan 强制检查 |
| 4 | hello_sdl2.cc CSS 加 `#box-red:hover { background: #FF6B6B }` 等规则使 P6.1 A2 验收可落地 | **P0**（归档前应做完，否则 plan 与代码不一致永久存在） | 直接修 hello_sdl2.cc + 重跑 smoke ctest | 在归档前 commit |
| 5 | GUI/loop 程序 ctest 自终止协议：env hook + timer push platform quit event | **P2** | 沉淀 `systemPatterns.md` | 后续 GUI test target 复用 |
| 6 | Platform backend 用 Composition over Inheritance 复用 HeadlessEventLoop task/timer | **P2** | 沉淀 `systemPatterns.md` | 下次新 backend (Win32/Wayland) 引用 |
| 7 | `gtest_discover_tests` helper PROPERTIES 透传不便 — vx_add_test 加可变参数 | **P3 触发型** | 累计 3+ 次同类需要时再立项；当前直接 `add_executable` 绕过 | tests/CMakeLists.txt |
| 8 | plan × 0.6 第 9 数据点「最窄路径」第 4 次确认（0.22×） | **P1**（数据点足够,可写入规则） | `writing-plans.mdc`「最窄路径子档」段从「3 数据点确认」升级到「4 数据点稳定」，附判定标准：基础设施 100% 复用 + 6 决策提前锁定 + 无性能/微优化 phase + 现有抽象成熟 | 下次类似条件任务直接按 plan ×0.22-0.30 预估 |

---

## 7. 反复模式识别

| 已知模式 | 历史频率 | 本次是否重复？ | 详情 |
|---------|---------|-------------|------|
| 计划文件清单与实际变更不一致 | 9+ | **是（10 次）** | A2 验收 :hover 与 example 不一致；建议 #3 升 P1 |
| 子代理产出需大量返工 | 7+ | 否（未用） | — |
| 前置依赖/环境/API 能力未验证 | 8+ | 部分（VxInputEvent forward decl 算 1 次微观） | sdl2_event_loop.h 漏 include public C header；建议 #2 升 P1 |
| 非默认路径遗漏验证 | 4+ | 部分 | SDL_CreateWindow 真失败路径未测；spec §5.4 已记 follow-up |
| 测试隔离问题（flaky/并行/环境） | 7+ | **是（8 次新变体）** | api_test include 进 anon namespace；建议 #2 直接对应 |
| 提交粒度偏离计划（大杂烩） | 7+ | 否 | 15 commits 每 phase 1-2 个，干净 |
| TDD 严格度与场景不匹配 | 11+ | 否 | P0.3 用「测试已知行为 → 改造 → 复跑」是 plan 明确的合理变体 |

→ **3 个反复模式新数据点**，全部已对应 P0/P1 改进建议落实路径。

---

## 8. 技术改进建议

1. **example 多样化** — hello_sdl2.cc 当前是静态盒子，实际能验证 input 端到端的功能薄弱。建议拆出 `examples/interactive_sdl2.cc`：含 hover、click、keystroke 三类反馈用例，作为长期 SDL2 backend 的 reference example。**触发条件**：下次 input 系统改动 / FPS overlay / DevTool 立项时
2. **SDL2 video 子系统 LazyInit 退化** — 当前 `Sdl2WindowSurface` ctor 调 `SDL_InitSubSystem(VIDEO)`，多 surface 实例时每次都走一遍。spec §6.1 已说 SDL_InitSubSystem 是 refcount 安全，但可加一个 process-singleton sentinel 避免重复 InitSubSystem 调用栈开销。**触发条件**：性能 profiling 发现 ctor 是 hot path（不太可能）
3. **`vx_view_run` 自动 wire callback 用 `dynamic_cast` — 性能开销可接受但语义不优** — 每次 `vx_view_run` 调一次（不在帧循环中），dynamic_cast 单次 ~50ns 完全 OK。但语义上 view 不应「猜」backend 类型。**改进方向**：在 `EventLoop` 抽象基类加个 `virtual void SetInputCallback(...)` 默认 no-op，sdl2 重写。这样 `vx_view_run` 直接调 `app->event_loop()->SetInputCallback(...)` 不用 cast。**触发条件**：第二个有 input 的 backend 接入时（Win32/Wayland）
4. **`Surface::Present()` 的「调用约定」需明文化进 contract test** — 当前 spec §6.2 注释了「Lock/Unlock 配对完成后调用」「每帧最多一次」，但没有 contract test。建议加 `tests/platform/surface_contract_test.cc` 用 mock surface 验证 Application::Update 的调用次数。**触发条件**：第二个 surface backend (e.g. OpenGL) 接入时
5. **测试目录 SDL_VIDEODRIVER 设为 dummy 的范围** — 当前 sdl2_*_test 和 api_test、hello_sdl2_smoke 各自加 `SDL_VIDEODRIVER=dummy`。建议在 tests/CMakeLists.txt 顶层加一个 helper 函数 `vx_add_sdl2_test()` 自动加 env，避免 4 处重复字符串。**触发条件**：累计 5+ 处再立项

---

## 9. 安全评估

| 维度 | 状态 | 备注 |
|------|------|------|
| 输入验证 | ✅ | 所有新 C API 检查 nullptr；dynamic_cast 失败返回 INVALID_STATE；VxWindowOptions 字段无 size_t/指针类外部内存引用，无 OOB 风险 |
| 认证/授权 | N/A | 本地引擎库 |
| 数据保护（加密/脱敏） | N/A | 不涉及敏感数据 |
| 依赖审计 | ✅ | SDL2 2.0.20 系统包，spec §13 已记联系人 + 升级触发条件；未引入新 FetchContent 源 |
| 错误信息脱敏 | ✅ | 所有错误码不含内部状态泄露 |
| 敏感数据处理 | N/A | 不涉及 |

**附加安全考虑（spec §13.3 R6 风险登记）：**
- SDL2 接口接受外部窗口标题（`VxWindowOptions.title`）作为 UTF-8 字符串传给 OS API；目前直接透传，没做长度截断。**风险**：极长 title 可能在某些 WM 触发字符串截断/缓冲。**缓解**：spec 已记 follow-up「title 长度限制 + UTF-8 BOM 拒绝」为 P2 触发型
- `Sdl2EventLoop::PumpInputEvents` 未限制每次最多 drain 多少个事件 → 理论上 SDL_PushEvent flood 可让单次 pump 永不返回。**缓解**：实际 SDL_PollEvent 内部队列有上限（128 events），WSL2/Linux 实测无此风险；spec §5.5 已记为 R3 低风险 follow-up

→ **无 P0/P1 安全行动项**，2 项 P2 触发型已 spec 记录。

---

## 10. 总结

TASK-20260425-01 是 Veloxa 项目自启动以来**第一个引入真窗口/真平台后端**的任务，技术上里程碑级（解锁 hot reload / inspector / FPS overlay 等 DevTool）。实测以**最窄路径子档 0.22×（plan × 0.6 第 9 数据点）**完成，是历史最快记录之一。流程上 6 个 AskQuestion 提前锁定全部决策、TDD 严格执行、Composition over Inheritance 模式首次自觉应用、技术债主动识别（destroy/save_ppm 基类化）等 6 项「做得好」充分体现规则沉淀的复利。挑战集中在 4 个微观 bug（composition 同名方法/include 卫生/gtest helper/forward decl）+ 1 个 plan-vs-implementation A2 :hover 不一致，全部已转化为 P0/P1 改进建议落实路径。

下一步：归档前完成 P0 建议 #4（hello_sdl2.cc 加 :hover），sync `systemPatterns.md` + `writing-plans.mdc` 的 4 处规则更新，然后 `/archive` 进入归档。
