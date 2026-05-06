# 归档：G1.3 Sdl2GLWindowSurface — GLES 窗口表面实施

**日期：** 2026-05-07
**任务 ID：** TASK-20260506-01
**复杂度级别：** Level 3
**状态：** ✅ 已完成
**分支：** `feature/TASK-20260506-01-sdl2-gl-window-surface`（已合并 main）

---

## 任务概述

GLES 蓝图实施第三步（G1.1 CMake flag ✅ → G1.2 GLESDisplay ✅ → **G1.3 Sdl2GLWindowSurface ✅**）。

将 G1.2 已落地的 `Sdl2EGLDisplay`（owned GL context）接入 `Surface` 抽象，实现 `vx::platform::Sdl2GLWindowSurface` —— 同时持有 `SDL_Window`（owns）+ `Sdl2EGLDisplay`（owns）的 GLES 窗口表面，并以 `glReadPixels` 实现 `SavePPM` 路径（GLES 路径首次实现 PPM 输出）。

完成 G1.3 后，G1.4 `GLESCanvas`（首个真实可绘制 GLES 画布）的前置链完整就位：G1.1 ✅ → G1.2 ✅ → G1.3 ✅ → G1.4。

---

## 技术方案

### 关键架构决策（13 决策 / spec 隐含 4 + AskQuestion 9 / all_recommended）

| # | 决策 | 选择 | 理由 |
|:-:|---|---|---|
| D1 | 构造签名 | `(u32 width, u32 height, const char* title)` | spec §3.3.3 隐含 / 与 Sdl2WindowSurface 对称 |
| D2 | 零维度软失败 | `valid()` 返回 false / 析构安全 | spec 隐含 / 无 crash 原则 |
| D3 | Y 翻转方向 | RGBA buf 原地翻转 → PPM top-left | GL bottom-left origin → PPM top-left |
| D4 | `glReadPixels` 格式 | `GL_RGBA + GL_UNSIGNED_BYTE` | GLES 3.0 保证支持此格式组合 |
| D5 | `Lock()` 返回值 | `nullptr`（GLES no-op） | GPU path 无 CPU pixel buffer |
| D6 | `Resize()` 实现 | `SDL_SetWindowSize` + 内部维度更新 | GL viewport 更新交由 G1.4 GLESCanvas |
| D7 | display 所有权 | `std::unique_ptr<Sdl2EGLDisplay>` | 析构顺序控制 / display first → window |
| D8 | 测试数量 | 7 TEST_F | T1 getters + T2 Lock + T3 Resize + T4 SavePPM + T5 Present + T6/T7 反向探针 |
| D9 | 测试 fixture | G1.2 `Sdl2GlSurfaceEnvironment` 复用模式 | 无 per-test fixture / 每测独立 surface |
| D10 | CMake 注册位置 | `vx_platform_sdl2` 同段 | 与 sdl2_egl_display.cc 聚合 |
| D11 | 测试 cmake guard | `if(VX_RENDERER STREQUAL "gles")` 共用 | G1.2 sdl2_egl_display_test 已有 guard |
| D12 | commit 策略 | 三段（init + plan + feat + MB finalize）| chore 分离清洁 |
| D13 | 本任务范围 | 仅 plan（build 独立立项）| 蓝图任务边界 |

---

## 实现摘要

### 文件变更

| 操作 | 文件路径 | 说明 |
|:----:|---------|------|
| 🆕 创建 | `veloxa/platform/sdl2/sdl2_gl_window_surface.h` | Surface 子类声明（forward declare Sdl2EGLDisplay） |
| 🆕 创建 | `veloxa/platform/sdl2/sdl2_gl_window_surface.cc` | ctor/dtor/Resize/SavePPM/Present 实现（~152 行） |
| 🆕 创建 | `tests/platform/sdl2_gl_window_surface_test.cc` | 7 TDD tests（~190 行）|
| 🟡 修改 | `veloxa/platform/sdl2/CMakeLists.txt` | 注册 sdl2_gl_window_surface.cc |
| 🟡 修改 | `tests/CMakeLists.txt` | 注册 sdl2_gl_window_surface_test（gles guard）|

**总计：** 5 files changed, **424 insertions(+)**（plan 估 490 / ×0.865）

### 核心实现细节

**构造函数软失败链（zero dim → SDL Init → SDL_CreateWindow → EGL Initialize）：**
```cpp
Sdl2GLWindowSurface::Sdl2GLWindowSurface(vx::u32 width, vx::u32 height, const char* title)
    : width_(width), height_(height) {
  if (width == 0 || height == 0) { return; }  // soft-fail
  if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) { return; }
  SDL_GL_ResetAttributes();  // test isolation
  window_ = SDL_CreateWindow(title ? title : "Veloxa GL", ..., SDL_WINDOW_OPENGL);
  if (!window_) { SDL_QuitSubSystem; return; }
  auto display = std::make_unique<Sdl2EGLDisplay>(window_);
  if (!display->Initialize().ok()) { SDL_DestroyWindow; SDL_QuitSubSystem; return; }
  display_ = std::move(display);
}
```

**析构顺序（display first → window）：**
```cpp
~Sdl2GLWindowSurface() {
  display_.reset();  // 必须先：GLContext while window alive
  if (window_) { SDL_DestroyWindow(window_); SDL_QuitSubSystem(SDL_INIT_VIDEO); }
}
```

**SavePPM（glReadPixels → Y-flip → P6 binary）：**
```cpp
glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, buf.data());
// Y-flip（GL bottom-left origin → PPM top-left）
for (u32 top=0, bot=h-1; top<bot; ++top, --bot) { swap rows... }
out << "P6\n" << w << " " << h << "\n255\n";
// write RGB bytes（drop alpha）
```

### 关键设计约定（GLES path）

- `Lock()` → `nullptr` / `Unlock()` no-op / `stride()` = 0：GPU path 无 CPU pixel buffer
- `Present()` → `display_->SwapBuffers()` → `SDL_GL_SwapWindow`
- `valid()` = `window_ != nullptr && display_ != nullptr`
- GL viewport 更新责任在 G1.4 GLESCanvas（本类仅维护逻辑尺寸）

---

## 测试覆盖

**测试文件：** `tests/platform/sdl2_gl_window_surface_test.cc`
**环境：** `SDL_VIDEODRIVER=offscreen`（Mesa swrast EGL / 无 X11 依赖 / CI 友好）
**编译 guard：** `if(VX_RENDERER STREQUAL "gles")`

| # | 测试名 | 验证内容 | 结果 |
|:-:|---|---|:-:|
| T1 | `Construct_BasicGetters` | width/height/stride/window/gles_display | ✅ PASS |
| T2 | `Lock_Returns_Nullptr` | GLES no-op contract / Unlock safe | ✅ PASS |
| T3 | `Resize_Updates_Dimensions` | 800×600 resize / width()/height() 更新 | ✅ PASS |
| T4 | `SavePPM_WritesValidFile` | P6 header + 真实红色像素（非 SKIP！） | ✅ PASS |
| T5 | `Present_DoesNotCrash` | 双次 SwapBuffers smoke | ✅ PASS |
| T6 | `ReverseProbe_NullTitle_StillConstructs` | nullptr title fallback | ✅ PASS |
| T7 | `ReverseProbe_ZeroDimensions_SoftFails` | valid()=false / nullptr 析构安全 | ✅ PASS |

**T4 关键实证：** Mesa swrast 在 `SDL_VIDEODRIVER=offscreen` 路径下确实写入 default framebuffer，`glReadPixels` 读出真实颜色值（非 all-zeros / 非 GTEST_SKIP）— **first-evidence，消除 GLES headless 测试最大不确定性**。

**三 build 矩阵验证：**

| 矩阵 | 配置 | 结果 |
|:--|:--|:--|
| A | software + DEVTOOL=ON | **1337/1337** ✅ |
| B | software + DEVTOOL=OFF | **1141/1141** ✅ |
| C | gles + DEVTOOL=ON | **1352/1352** ✅（+7 精确匹配 / baseline 1345 → 1352）|

---

## 提交记录

| commit | 说明 |
|---|---|
| `c0d67e2` | chore(workflow): initialize TASK-20260506-01 G1.3 Sdl2GLWindowSurface |
| `86d3425` | chore(plan): land plan + memory bank for TASK-20260506-01 |
| `7746925` | feat(platform): add Sdl2GLWindowSurface — G1.3 GLES window surface |
| `e5dd0da` | chore(build): finalize TASK-20260506-01 memory bank state |
| `aa73cc3` | docs(reflect): add reflection for TASK-20260506-01 |

---

## 经验教训

### 新 pattern 入库

1. **Mesa swrast default framebuffer 真实写入 first-evidence** — Mesa swrast + `SDL_VIDEODRIVER=offscreen` 路径下，`glReadPixels` 可读取 `glClear` 之后的真实颜色值。G1.4 GLESCanvas pixel-accurate 测试可安全使用此路径。（→ systemPatterns + techContext）

2. **GLES test include 协议** — 测试文件直接调 `gl*()` 函数时须显式 `#include <GLES3/gl3.h>`，不能依赖被测类 header 传递。（→ writing-plans.mdc checklist 新增）

3. **实施忠实度 triple-evidence** — 连续 3 个 GLES 子任务（G1.1/G1.2/G1.3）build 阶段 0 计划偏差。当 plan 包含完整 C++ 代码片段时，build ≈ 机械转化。（→ systemPatterns）

### 范式里程碑

| 里程碑 | 状态 |
|---|:-:|
| 跨决策协同度 第 17 次连续命中 / **158/158** | ✅ 历史最高 streak |
| 实施忠实度 triple-evidence（G1.1→G1.2→G1.3）| ✅ 首次确立 |
| Mesa swrast default framebuffer first-evidence | ✅ T4 非 SKIP |
| plan ×0.6 undec-evidence 第 11 数据点 | ✅ G1.3 build 0.13× |
| 反复模式抑制 0/7 / 累计 20+ 连续抑制 | ✅ 历史新高续刷 |

---

## 安全评估

**本任务不涉及安全变更**：
- 0 新公开 ABI / 0 用户输入路径
- `SavePPM` 路径参数来自内部调用（非外部输入）
- 0 新外部依赖（EGL/GLES3 已由 G1.2 引入）

---

## 参考文档

- 蓝图：[`docs/plans/2026-05-05-gles-renderer-blueprint.md`](../../docs/plans/2026-05-05-gles-renderer-blueprint.md) §3.3
- 实现计划：[`docs/plans/2026-05-06-sdl2-gl-window-surface.md`](../../docs/plans/2026-05-06-sdl2-gl-window-surface.md)
- 回顾文档：[`memory-bank/reflection/reflection-TASK-20260506-01.md`](../reflection/reflection-TASK-20260506-01.md)
- G1.2 归档：[`memory-bank/archive/archive-TASK-20260505-06.md`](archive-TASK-20260505-06.md)
- MVP-C 规格：[`docs/specs/2026-05-04-mvp-scope.md`](../../docs/specs/2026-05-04-mvp-scope.md)
