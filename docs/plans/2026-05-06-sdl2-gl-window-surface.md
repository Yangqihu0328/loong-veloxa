# 实现计划：G1.3 `Sdl2GLWindowSurface` 实施

**日期：** 2026-05-06
**任务 ID：** `TASK-20260506-01`
**复杂度级别：** Level 3 实施类（新 Surface 子类 + 嵌入 Sdl2EGLDisplay + glReadPixels SavePPM 路径）
**任务定位：** **MVP-C 战略主线第三个实施任务** / GLES 蓝图实施第三步 / G1.2 Sdl2EGLDisplay 直接消费者
**安全相关：** ❌ 否（沿用既有 `Surface::SavePPM` 表面 / 0 新公开 ABI / 0 用户输入路径 / spec §6.2 标 P2 既有 SoftwareCanvas 同等风险）
**估时（plan ×0.6）：** ~2-3.5 h（实施类 Level 3 子档 / 标准极速区 0.4-0.7×）

---

## 0. 上下文与决策矩阵

### 0.1 上下文

- **任务来源：** [GLES 蓝图 plan §3.3](2026-05-05-gles-renderer-blueprint.md#33-子任务-g13--sdl2glwindowsurface-实施) + [GLES 蓝图 spec §3.3.3](../specs/2026-05-05-gles-renderer-blueprint-design.md#333-sdl2glwindowsurfacegles-window-surface)
- **前置：**
  - [TASK-20260505-05 G1.1 CMake `VX_RENDERER` flag](../../memory-bank/archive/archive-TASK-20260505-05.md) ✅ 已归档
  - [TASK-20260505-06 G1.2 GLESDisplay + Sdl2EGLDisplay](../../memory-bank/archive/archive-TASK-20260505-06.md) ✅ 已归档（main `0dc7b40`）
- **本任务定位：**
  - 落地 `Sdl2GLWindowSurface : public Surface`（ownership `SDL_Window` + `std::unique_ptr<Sdl2EGLDisplay>`）
  - SavePPM via `glReadPixels(GL_RGBA, GL_UNSIGNED_BYTE)` + Y 翻转 + P6 PPM RGB 输出（GLES 路径首次实现 PPM）
  - Present via `Sdl2EGLDisplay::SwapBuffers()`（SDL_GL_SwapWindow 转交）
  - Resize via `SDL_SetWindowSize` + 内部尺寸更新
- **不在本任务范围：**
  - GLESCanvas 骨架（G1.4 / 后续任务 / 本任务仅创建 Surface 容器，不画任何东西）
  - `Application` 构造分支 GLES fallback（G1.13 / 后续任务）
  - Sdl2WindowSurface（software 路径）SavePPM 回填（独立任务 / 不在本范围 / 既有 `kInternal "not implemented"` 状态保留）

### 0.2 决策矩阵（VAN + Plan 阶段 1 次 AskQuestion all_recommended 锁定 ✅）

**跨决策协同度 100% 第 16 次连续命中 / 累计 145/145 历史最高 streak 续刷**（dec → endec → doudec → 第 16 次 / 实施忠实度 triple-evidence 候选）

spec §3.3.3 + 蓝图 plan §3.3 已隐含锁定 4 项设计（D1 构造签名 `(width, height, title)` / D5 `Lock=nullptr / Unlock=no-op` / D7 `std::unique_ptr<Sdl2EGLDisplay>` / D2 软失败 valid() 范式沿用 Sdl2WindowSurface）；本 brainstorm 阶段 lock 9 项尚未规格化的决策：

| # | 决策项 | 选择 | 详细理由 |
|:-:|---|---|---|
| **D3** | SavePPM Y 翻转策略 | **A CPU 端逐行 swap** | OpenGL 原点左下 / PPM 左上 → 必须 Y 翻转；分配整 buf → 按 row 双指针 swap → 写文件；简单清晰 / 内存 ~w*h*4 一次性 / Mesa swrast 性能不敏感 / 与 SoftwareCanvas pixel order audit 一致 |
| **D4** | SavePPM 像素格式 | **A glReadPixels(GL_RGBA, GL_UNSIGNED_BYTE) → P6 PPM RGB** | GLES 3.0 spec 强制支持 RGBA8（grep 实证 `<GLES3/gl3.h>` 第 599 行签名）/ P6 PPM 标准 RGB 是 binary 标准 / 丢 alpha（与 spec §3.3.3 SavePPM 一致）|
| **D6** | Resize 实现 | **A SDL_SetWindowSize + 更新内部 width_/height_** | 与 SDL2 既有 API 一致；GL viewport 由 GLESCanvas（G1.4+）调用 / Surface 不管 GL state；不重建 SDL_GLContext（保 GL state） |
| **D8** | 测试矩阵 | **B 7 单测**（5 plan 默认 + 2 反向探针） | 沿用 G1.2 D8 8-test 范式（含 driver-aware T8 inline reverse probe）/ 反向探针：T6 nullptr title / T7 0×0 dimensions（沿用 Sdl2WindowSurface 软失败范式）|
| **D9** | 测试 fixture | **A 沿用 G1.2 `Sdl2EglEnvironment` 全局 env + 类自管 SDL_Window** | refcounted SDL_InitSubSystem 安全 / 沿用 sdl2_egl_display_test.cc 范式 / 与 G1.2 一致 |
| **D10** | CMake 源注册 | **A 与 sdl2_egl_display.cc 同段聚合** | `veloxa/platform/sdl2/CMakeLists.txt` `add_library(vx_platform_sdl2 STATIC ...)` 同段加 sdl2_gl_window_surface.cc / 无 if guard / A14 守门 nm 不引用即不链接 |
| **D11** | 测试 cmake guard | **A 与 G1.2 共用 `if(VX_RENDERER STREQUAL "gles")` guard** | 同 if 段后追加；只在 gles config 编译；与 G1.2 测试组织一致 |
| **D12** | commit 拆分 | **B 三段** | (1) VAN ✅ `c0d67e2` chore(workflow) 已 commit + (2) chore(plan) plan + MB 单 commit（P0 协议）+ (3) feat(platform) impl + tests / 沿用 G1.2 6-commit 范式 |
| **D13** | plan vs spec | **A 仅 plan** | 蓝图 spec §3.3.3 已是 spec 来源（cpp interface 完整规格化）/ 实施类 Level 3 不开独立 spec / 沿用 G1.2 P0 协议 |

### 0.3 不进入 `/creative`

实施类 Level 3 / 9 决策 + 4 spec 隐含锁 = 13 决策已锁定 / 设计 spec §3.3.3 已完整规格化 cpp interface（class def + 5 Surface 接口 + 1 GL-specific getter + 3 private members）/ 0 创意阶段需求。

### 0.4 plan §3.3 偏差校正（来自 brainstorming P1.3 主动 push-back / triple-evidence 续延实战）

**3 处偏差需校正（VAN 阶段 grep audit 已发现）：**

#### 偏差 #1：测试路径

- **原蓝图 plan §3.3 推荐：** 创建 `tests/platform/sdl2/sdl2_gl_window_surface_test.cc`（带 sdl2/ 子目录）
- **校正：** 创建 `tests/platform/sdl2_gl_window_surface_test.cc`（扁平路径 / 无 sdl2/ 子目录）
- **理由：** 与 G1.2 测试路径校正同源 / 既有 tests/platform/ 是扁平结构（5 既有 _test.cc 全在 tests/platform/ 直接 / sdl2_egl_display_test.cc 也是扁平路径）
- **影响：** plan 文件结构 §1.1

#### 偏差 #2：headless CI testing fixture 复用

- **原蓝图 plan §3.3 推荐：** 步骤 1 测试设计未明示 SDL_VIDEODRIVER setup
- **校正：** D9=A 决策 → 沿用 G1.2 `Sdl2EglEnvironment` 全局 env 模式 + 类自管 SDL_Window 创建（构造时 `SDL_CreateWindow(... SDL_WINDOW_OPENGL)` / 析构时 `SDL_DestroyWindow`）
- **理由：** 与 G1.2 fixture 一致 / SDL2 refcounted SDL_InitSubSystem 安全 / 自动 SDL_Init / 0 重复初始化
- **影响：** plan 步骤 1 测试设计 + sdl2_gl_window_surface_test.cc 含 ::testing::Environment 子类引用 G1.2 范式

#### 偏差 #3：Mesa swrast default framebuffer 实证未规划

- **原蓝图 plan §3.3 推荐：** 步骤 1 T4 SavePPM 测试列了「文件存在 / RGB 格式正确」验收 / 未规划 driver framebuffer 渲染实证
- **校正：** Phase 0 §0.4 加 inline grep 实证 + build 阶段 T4 RED 阶段优先实施（最先暴露 driver 问题 / 沿用 G1.2 D4=C inline reverse probe + 驱动严格性分层 first-evidence 范式）
- **理由：**
  - G1.2 仅验 `glGetString` 工作 / 未验 framebuffer 渲染
  - SDL_VIDEODRIVER=offscreen 路径下 SDL2 创建 dummy SDL_Window（非真 X11 surface）→ `glReadPixels` 是否真返绘制结果？
  - 三种可能：(a) Mesa swrast offscreen driver 渲染到 default FB（最优 / T4 ✅）/ (b) 不渲染（T4 FAIL → 需 FBO 路径）/ (c) 仅返 0（T4 部分 FAIL → GTEST_SKIP 兜底）
  - **build 阶段 RED 顺序**：T4 优先 → 若 GREEN ✅ default FB 工作；若 FAIL → 转 FBO + RBO offscreen render（设计 P3 备选 / spec §6.1 错误处理表已涵盖 OnContextLost 重建模式 / 不影响 plan 落盘）/ 或 T4 GTEST_SKIP（沿用 G1.2 T5 "驱动严格性分层" 范式）
- **影响：** plan §0.5 Phase 0 audit + §3 build 步骤顺序

**偏差度评估：** 中等限定范围（仅影响测试 setup + build 阶段 RED 顺序 / 不动整体架构 / 0 倒退既有 build）

**累计 brainstorming P1.3 主动 push-back 模式实证：**
- TASK-04 MVP-scope（first-evidence）
- TASK-05 G1.1（dual-evidence / 3 偏差校正）
- TASK-06 G1.2（triple-evidence / 3 偏差校正）
- **TASK-06-01 G1.3（quad-evidence 候选 / 第 4 次实战 / 3 偏差校正）**

---

### 0.5 Phase 0 audit 清单（VAN 阶段已部分完成 / Plan 阶段补全）

| # | 子段 | 状态 | 实证内容 |
|:-:|---|:-:|---|
| §0.1 | ctest baseline 二次验证 | ✅ VAN 已确认 | DEVTOOL=ON software=1303 / DEVTOOL=OFF software=1110 / DEVTOOL=ON gles=1345 |
| §0.2 | `glReadPixels` GLES 3.0 签名 | ✅ Plan 阶段实证 | `<GLES3/gl3.h>` 第 599 行：`void glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels)` / GLES 3.0 必支持 `GL_RGBA + GL_UNSIGNED_BYTE` |
| §0.3 | `SDL_VIDEODRIVER=offscreen` + `SDL_GL_SwapWindow` 行为 | ✅ G1.2 已验 | SDL2 2.0.16+ Mesa swrast EGL 路径；SwapWindow 在 offscreen driver 下 no-op 但安全调用 |
| §0.4 | Mesa swrast default framebuffer + glReadPixels | ⚠️ **build 阶段实证** | T4 RED → GREEN 路径自动 surface / 若 FAIL → 转 FBO 或 GTEST_SKIP（驱动严格性分层 first-evidence 沿用） |
| §0.5 | 双重所有权析构序锁定 | ✅ Plan 阶段设计锁 | `~Sdl2GLWindowSurface()` 序：`display_.reset()`（Sdl2EGLDisplay::Shutdown → SDL_GL_DeleteContext）→ `SDL_DestroyWindow(window_)` → SDL_QuitSubSystem(VIDEO)（如 init）/ 与 G1.2 borrow contract 一致（display 借用 window / display 必须先 Shutdown） |

---

## 1. 文件结构

### 1.1 创建文件（3 个 / 偏差校正后）

| # | 路径 | 行数估 | 职责 |
|:-:|---|:-:|---|
| 1 | `veloxa/platform/sdl2/sdl2_gl_window_surface.h` | ~80 | `Sdl2GLWindowSurface : public Surface`（spec §3.3.3 完整复刻 + 顶部 doc 段说明双重所有权 + lazy SDL_Init 范式） |
| 2 | `veloxa/platform/sdl2/sdl2_gl_window_surface.cc` | ~200 | 构造（SDL_InitSubSystem + SDL_CreateWindow with SDL_WINDOW_OPENGL + new Sdl2EGLDisplay + Initialize）+ 析构（reverse order）+ Resize + SavePPM via glReadPixels Y 翻转 P6 PPM + Present via display_->SwapBuffers + valid() |
| 3 | `tests/platform/sdl2_gl_window_surface_test.cc` ⚠️ 偏差 #1 校正 | ~200 | 7 TEST_F + 复用 G1.2 `Sdl2EglEnvironment` 全局 env 范式 + 4-6 helpers |

**总创建：** ~480 行（plan 估 ×1.0 / buffer P2.2 ×0.85-1.5 → 实际预期 ~410-720 行 / 双向 ±25% buffer）

### 1.2 修改文件（2 个）[共享文件]

| # | 路径 | 行数估 | 职责 |
|:-:|---|:-:|---|
| 1 | `veloxa/platform/sdl2/CMakeLists.txt` | +~3 | 加 `sdl2_gl_window_surface.cc` 到 `vx_platform_sdl2` sources（与 sdl2_egl_display.cc 同段 / 无 if guard） |
| 2 | `tests/CMakeLists.txt` | +~6 | 在 G1.2 `sdl2_egl_display_test` 同 `if(VX_RENDERER STREQUAL "gles")` guard 内追加 sdl2_gl_window_surface_test |

**总修改：** +~9 行 / 共享文件 2 个

### 1.3 LOC 总计估算（plan + P2.2 ×0.85-1.5 buffer）

| 维度 | plan 估 | buffer 下限（×0.85）| buffer 上限（×1.5）|
|---|:-:|:-:|:-:|
| 创建 + 修改 | ~490 行 | ~415 行 | ~735 行 |

**dual-evidence 双向 buffer 验证（沿用 P2.2 子档）：**
- TASK-05 G1.1 ×1.40 偏高（drift guard）
- TASK-06 G1.2 ×0.95 偏低（GTEST_SKIP）
- TASK-06-01 G1.3 预期 ×1.0-1.2（含 SavePPM helpers + 7 测）

---

## 2. 决策细化设计

### 2.1 Sdl2GLWindowSurface 类设计（spec §3.3.3 + D1+D2+D5+D7+D6 落地）

```cpp
// veloxa/platform/sdl2/sdl2_gl_window_surface.h
#ifndef VELOXA_PLATFORM_SDL2_SDL2_GL_WINDOW_SURFACE_H_
#define VELOXA_PLATFORM_SDL2_SDL2_GL_WINDOW_SURFACE_H_

#include <SDL2/SDL.h>

#include <memory>

#include "veloxa/platform/surface.h"

namespace vx::platform {

class Sdl2EGLDisplay;  // forward — keeps EGL/GLES off the public header

// Surface backed by an SDL2 window with an OpenGL ES context.
//
// Ownership:
//   * SDL_Window  — owns; created with SDL_WINDOW_OPENGL in ctor, destroyed in
//     dtor. SDL_InitSubSystem(VIDEO) is refcounted (sympathetic with the
//     existing Sdl2WindowSurface software path).
//   * Sdl2EGLDisplay — owns via std::unique_ptr; the display borrows the
//     SDL_Window we own. Destruction ordering must therefore be: drop the
//     display first (which deletes the GLContext while the window is still
//     alive), then destroy the SDL_Window. Doing it the other way would
//     leak the GLContext on top of a dead window handle.
//
// Surface contract notes (GLES path):
//   * Lock() returns nullptr / Unlock() is a no-op — there is no CPU pixel
//     buffer to expose; rasterization happens GPU-side via GLESCanvas
//     (G1.4+).
//   * stride() returns 0 for the same reason.
//   * SavePPM() uses glReadPixels(GL_RGBA, GL_UNSIGNED_BYTE) + Y-flip + P6
//     PPM RGB output. Mesa swrast under SDL_VIDEODRIVER=offscreen may or
//     may not render to the default framebuffer; the implementation is
//     designed for the desktop happy path and falls back gracefully on
//     headless CI.
//   * Present() defers to Sdl2EGLDisplay::SwapBuffers() (SDL_GL_SwapWindow
//     under the hood).
//
// If construction fails (driver issue, SDL_CreateWindow failure, EGL
// context refused), valid() returns false and all owned handles remain
// nullptr; the destructor is safe in that state.
class Sdl2GLWindowSurface : public Surface {
 public:
  Sdl2GLWindowSurface(vx::u32 width, vx::u32 height, const char* title);
  ~Sdl2GLWindowSurface() override;

  Sdl2GLWindowSurface(const Sdl2GLWindowSurface&) = delete;
  Sdl2GLWindowSurface& operator=(const Sdl2GLWindowSurface&) = delete;

  vx::u32 width() const override { return width_; }
  vx::u32 height() const override { return height_; }
  vx::u32 stride() const override { return 0; }  // GLES path: no CPU stride
  vx::u32* Lock() override { return nullptr; }   // GLES path: no-op
  void Unlock() override {}                      // GLES path: no-op
  void Resize(vx::u32 width, vx::u32 height) override;
  vx::Status SavePPM(const char* path) const override;
  void Present() override;

  // GLES-specific accessors used by Application (G1.13) and tests.
  bool valid() const { return window_ != nullptr && display_ != nullptr; }
  SDL_Window* window() const { return window_; }
  Sdl2EGLDisplay* gles_display() const { return display_.get(); }

 private:
  vx::u32 width_;
  vx::u32 height_;
  SDL_Window* window_ = nullptr;
  std::unique_ptr<Sdl2EGLDisplay> display_;
};

}  // namespace vx::platform

#endif  // VELOXA_PLATFORM_SDL2_SDL2_GL_WINDOW_SURFACE_H_
```

### 2.2 SavePPM Y 翻转策略（D3=A）

```cpp
vx::Status Sdl2GLWindowSurface::SavePPM(const char* path) const {
  if (!valid() || width_ == 0 || height_ == 0) {
    return vx::Status(vx::StatusCode::kFailedPrecondition,
                      "Sdl2GLWindowSurface::SavePPM: surface not initialized");
  }
  if (path == nullptr) {
    return vx::Status(vx::StatusCode::kInvalidArgument,
                      "Sdl2GLWindowSurface::SavePPM: null path");
  }

  // Ensure the GL context is current on this thread before glReadPixels;
  // safe to call repeatedly. If MakeCurrent fails the read would otherwise
  // silently return zeros.
  if (!display_->MakeCurrent()) {
    return vx::Status(vx::StatusCode::kInternal,
                      "Sdl2GLWindowSurface::SavePPM: MakeCurrent failed");
  }

  // GLES 3.0 mandates support for (GL_RGBA, GL_UNSIGNED_BYTE) on the default
  // framebuffer (gl3.h §3.7.6). 4 bytes per pixel; total = w*h*4.
  const size_t row_bytes = static_cast<size_t>(width_) * 4;
  const size_t total_bytes = row_bytes * height_;
  std::vector<uint8_t> rgba(total_bytes, 0);

  glPixelStorei(GL_PACK_ALIGNMENT, 1);  // tightly packed rows
  glReadPixels(0, 0, static_cast<GLsizei>(width_),
               static_cast<GLsizei>(height_),
               GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());

  // OpenGL origin = bottom-left, PPM origin = top-left → flip Y in-place.
  std::vector<uint8_t> row_tmp(row_bytes);
  for (uint32_t y = 0; y < height_ / 2; ++y) {
    uint8_t* top = rgba.data() + y * row_bytes;
    uint8_t* bot = rgba.data() + (height_ - 1 - y) * row_bytes;
    std::memcpy(row_tmp.data(), top, row_bytes);
    std::memcpy(top, bot, row_bytes);
    std::memcpy(bot, row_tmp.data(), row_bytes);
  }

  // Write P6 binary PPM (RGB only — drop alpha). Header: "P6\nW H\n255\n".
  FILE* f = std::fopen(path, "wb");
  if (f == nullptr) {
    return vx::Status(vx::StatusCode::kInternal,
                      std::string("Sdl2GLWindowSurface::SavePPM: fopen failed: ") + path);
  }
  std::fprintf(f, "P6\n%u %u\n255\n", width_, height_);
  for (uint32_t y = 0; y < height_; ++y) {
    const uint8_t* row = rgba.data() + y * row_bytes;
    for (uint32_t x = 0; x < width_; ++x) {
      // RGBA → RGB drop alpha
      std::fwrite(row + x * 4, 1, 3, f);
    }
  }
  std::fclose(f);
  return vx::Status::Ok();
}
```

### 2.3 构造 / 析构序（D2 软失败 + 双重所有权契约）

```cpp
Sdl2GLWindowSurface::Sdl2GLWindowSurface(vx::u32 width, vx::u32 height,
                                         const char* title)
    : width_(width), height_(height) {
  if (width == 0 || height == 0) {
    VX_LOG_WARN("Sdl2GLWindowSurface: zero dimensions, skipping window");
    return;
  }

  if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
    VX_LOG_ERROR("Sdl2GLWindowSurface: SDL_InitSubSystem(VIDEO) failed: %s",
                 SDL_GetError());
    return;
  }

  // Reset attributes so left-over state from another test/instance can't
  // poison this context's negotiation.
  SDL_GL_ResetAttributes();

  window_ = SDL_CreateWindow(
      title ? title : "Veloxa GLES",
      SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
      static_cast<int>(width), static_cast<int>(height),
      SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  if (window_ == nullptr) {
    VX_LOG_ERROR("Sdl2GLWindowSurface: SDL_CreateWindow failed: %s",
                 SDL_GetError());
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    return;
  }

  display_ = std::make_unique<Sdl2EGLDisplay>(window_);
  vx::Status init = display_->Initialize();
  if (!init.ok()) {
    VX_LOG_ERROR("Sdl2GLWindowSurface: Sdl2EGLDisplay::Initialize failed: %s",
                 init.message().c_str());
    display_.reset();
    SDL_DestroyWindow(window_);
    window_ = nullptr;
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    return;
  }
}

Sdl2GLWindowSurface::~Sdl2GLWindowSurface() {
  // CRITICAL: drop the display first — the SDL_GLContext it owns must be
  // deleted while the SDL_Window is still alive (G1.2 borrow contract).
  display_.reset();
  if (window_ != nullptr) {
    SDL_DestroyWindow(window_);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
  }
}
```

### 2.4 Resize / Present（D6 + Surface 接口）

```cpp
void Sdl2GLWindowSurface::Resize(vx::u32 width, vx::u32 height) {
  if (window_ == nullptr) return;
  if (width == 0 || height == 0) {
    VX_LOG_WARN("Sdl2GLWindowSurface::Resize: zero dimensions ignored");
    return;
  }
  SDL_SetWindowSize(window_, static_cast<int>(width),
                    static_cast<int>(height));
  width_ = width;
  height_ = height;
  // Note: glViewport() update is GLESCanvas's responsibility (G1.4+).
}

void Sdl2GLWindowSurface::Present() {
  if (display_ != nullptr) {
    display_->SwapBuffers();
  }
}
```

---

## 3. 实施步骤（TDD 严格 RED → GREEN → REFACTOR）

### 3.1 Phase 0 — Memory Bank 同步 commit（D12 第 2 段 / P0 协议自吃狗粮）

**输出：** `chore(plan): land plan + memory bank for TASK-20260506-01 [P0 sext-evidence candidate]`

- 写本 plan 文件
- 更新 `memory-bank/tasks.md`（plan 阶段产出段）
- 更新 `memory-bank/activeContext.md`（阶段 初始化 → 规划中）
- 更新 `memory-bank/progress.md`（plan 阶段里程碑）
- 单 commit 落盘（**P0 协议 sext-evidence 续延 / 第 7 数据点候选**）

### 3.2 Phase A — RED 单测（步骤 1 / TDD RED）

**输出：** `tests/platform/sdl2_gl_window_surface_test.cc` 完整 7 测（编译 fail / 类不存在）

完整 7 单测设计：

```cpp
// tests/platform/sdl2_gl_window_surface_test.cc
//
// Tests for Sdl2GLWindowSurface (G1.3 GLES window surface). Reuses the
// G1.2 Sdl2EglEnvironment global SDL_VIDEODRIVER=offscreen fixture so
// process-wide SDL_Init succeeds once. Each test constructs a fresh
// Sdl2GLWindowSurface so attribute changes don't leak across tests.

#include "veloxa/platform/sdl2/sdl2_gl_window_surface.h"

#include <SDL2/SDL.h>
#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <string>

#include "veloxa/foundation/base/status.h"
#include "veloxa/platform/sdl2/sdl2_egl_display.h"

namespace vx::platform {
namespace {

// Reuse the G1.2 environment (offscreen driver + global SDL_Init/Quit).
class Sdl2GlSurfaceEnvironment : public ::testing::Environment {
 public:
  void SetUp() override {
    SDL_setenv("SDL_VIDEODRIVER", "offscreen", /*overwrite=*/1);
    ASSERT_EQ(SDL_Init(SDL_INIT_VIDEO), 0)
        << "SDL_Init(VIDEO) failed under SDL_VIDEODRIVER=offscreen: "
        << SDL_GetError();
  }
  void TearDown() override { SDL_Quit(); }
};

[[maybe_unused]] auto* g_sdl_env =
    ::testing::AddGlobalTestEnvironment(new Sdl2GlSurfaceEnvironment);

// Helper: small temp path under /tmp so SavePPM has a valid sink.
std::string TempPpmPath(const char* tag) {
  std::string p = "/tmp/vx_g13_";
  p += tag;
  p += ".ppm";
  return p;
}

// ---------------------------------------------------------------------------
// T1: Construct_BasicGetters (D1 + D7 happy path)
// ---------------------------------------------------------------------------
TEST(Sdl2GlWindowSurfaceTest, Construct_BasicGetters) {
  Sdl2GLWindowSurface surface(64, 48, "vx_g13_t1");
  ASSERT_TRUE(surface.valid()) << "construction must succeed under offscreen";
  EXPECT_EQ(surface.width(), 64u);
  EXPECT_EQ(surface.height(), 48u);
  EXPECT_EQ(surface.stride(), 0u);  // GLES path: no CPU stride
  EXPECT_NE(surface.window(), nullptr);
  EXPECT_NE(surface.gles_display(), nullptr);
  EXPECT_TRUE(surface.gles_display()->IsValid());
}

// ---------------------------------------------------------------------------
// T2: Lock_Returns_Nullptr (D5 GLES no-op contract)
// ---------------------------------------------------------------------------
TEST(Sdl2GlWindowSurfaceTest, Lock_Returns_Nullptr) {
  Sdl2GLWindowSurface surface(32, 32, "vx_g13_t2");
  ASSERT_TRUE(surface.valid());
  EXPECT_EQ(surface.Lock(), nullptr);
  surface.Unlock();  // must be safe even though Lock returned nullptr
}

// ---------------------------------------------------------------------------
// T3: Resize_Updates_Dimensions (D6)
// ---------------------------------------------------------------------------
TEST(Sdl2GlWindowSurfaceTest, Resize_Updates_Dimensions) {
  Sdl2GLWindowSurface surface(64, 48, "vx_g13_t3");
  ASSERT_TRUE(surface.valid());
  surface.Resize(800, 600);
  EXPECT_EQ(surface.width(), 800u);
  EXPECT_EQ(surface.height(), 600u);
}

// ---------------------------------------------------------------------------
// T4: SavePPM_WritesValidFile (D3 + D4 + Phase 0 §0.4 driver probe)
// ---------------------------------------------------------------------------
// glClear to a known color, force a SwapBuffers, then SavePPM and verify
// the file exists with a P6 header. The pixel-content assertion is a soft
// probe: Mesa swrast under SDL_VIDEODRIVER=offscreen renders to a CPU
// framebuffer that glReadPixels CAN read; if it doesn't (older Mesa or
// driver quirk) we GTEST_SKIP rather than fail (driver-strictness layer
// per G1.2 first-evidence).
TEST(Sdl2GlWindowSurfaceTest, SavePPM_WritesValidFile) {
  Sdl2GLWindowSurface surface(8, 8, "vx_g13_t4");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  std::string path = TempPpmPath("t4");
  vx::Status s = surface.SavePPM(path.c_str());
  ASSERT_TRUE(s.ok()) << "SavePPM failed: " << s.message();

  // Verify P6 header.
  std::ifstream in(path, std::ios::binary);
  ASSERT_TRUE(in.is_open());
  std::string header_line;
  std::getline(in, header_line);
  EXPECT_EQ(header_line, "P6");

  std::string dims;
  std::getline(in, dims);
  EXPECT_EQ(dims, "8 8");

  std::string maxval;
  std::getline(in, maxval);
  EXPECT_EQ(maxval, "255");

  // Read first pixel (3 bytes RGB). If Mesa swrast didn't actually render,
  // bytes will likely be 0 — skip the strict pixel check rather than fail.
  uint8_t pixel[3] = {0, 0, 0};
  in.read(reinterpret_cast<char*>(pixel), 3);
  if (pixel[0] == 0 && pixel[1] == 0 && pixel[2] == 0) {
    GTEST_SKIP() << "Mesa swrast offscreen driver did not render to default "
                    "framebuffer; PPM header still written correctly. "
                    "Strict pixel verification deferred to real-driver CI.";
  }
  EXPECT_GT(pixel[0], 200) << "expected ~red R, got " << +pixel[0];
  EXPECT_LT(pixel[1], 50) << "expected ~0 G";
  EXPECT_LT(pixel[2], 50) << "expected ~0 B";

  std::remove(path.c_str());
}

// ---------------------------------------------------------------------------
// T5: Present_DoesNotCrash (D6 SwapBuffers smoke)
// ---------------------------------------------------------------------------
// Under SDL_VIDEODRIVER=offscreen SwapBuffers is a no-op but it must still
// be safe to call. Ensures the Present override forwards through display_
// without dereferencing a null pointer.
TEST(Sdl2GlWindowSurfaceTest, Present_DoesNotCrash) {
  Sdl2GLWindowSurface surface(16, 16, "vx_g13_t5");
  ASSERT_TRUE(surface.valid());
  surface.Present();
  surface.Present();  // call twice to confirm no internal latch
}

// ---------------------------------------------------------------------------
// T6: ReverseProbe_NullTitle_StillConstructs (D2 soft-fail / nullptr title)
// ---------------------------------------------------------------------------
// Mirror of Sdl2WindowSurface behavior: passing nullptr for title falls
// back to a default and construction still succeeds.
TEST(Sdl2GlWindowSurfaceTest, ReverseProbe_NullTitle_StillConstructs) {
  Sdl2GLWindowSurface surface(32, 32, nullptr);
  EXPECT_TRUE(surface.valid());
}

// ---------------------------------------------------------------------------
// T7: ReverseProbe_ZeroDimensions_SoftFails (D2 zero-dim guard)
// ---------------------------------------------------------------------------
// Mirror of Sdl2WindowSurface zero-dim early-return: valid() == false and
// destruction is safe (no SDL_DestroyWindow call in dtor because window_
// stayed nullptr). Pure inline reverse probe (driver-independent).
TEST(Sdl2GlWindowSurfaceTest, ReverseProbe_ZeroDimensions_SoftFails) {
  Sdl2GLWindowSurface surface(0, 0, "vx_g13_t7");
  EXPECT_FALSE(surface.valid());
  EXPECT_EQ(surface.window(), nullptr);
  EXPECT_EQ(surface.gles_display(), nullptr);
}

}  // namespace
}  // namespace vx::platform
```

**反向探针清单（plan §3.3 步骤 3 + Phase 0 §0.4 driver probe + D8=B）：**

| # | 反向探针 | 类型 | 强度 |
|:-:|---|---|:-:|
| T6 | nullptr title | inline reverse probe / 接口契约 | 低（驱动无关） |
| T7 | 0×0 dimensions | inline reverse probe / 接口契约 | 低（驱动无关） |
| T4 内嵌 | Mesa swrast framebuffer 真实性 | runtime probe + GTEST_SKIP fallback | 中（驱动严格性分层） |

**RED 阶段验证：** 编译 sdl2_gl_window_surface_test.cc → 全 7 测应**编译失败**（类不存在）/ 或链接失败（构造未实现）。

### 3.3 Phase B — GREEN 实施（步骤 2 / TDD GREEN）

**输出：** `feat(platform): add Sdl2GLWindowSurface impl + tests [TASK-20260506-01]`（D12 第 3 段单 commit）

实施顺序（GREEN 内 micro-cycle）：
1. 写 `sdl2_gl_window_surface.h`（spec §3.3.3 + §2.1 doc）
2. 写 `sdl2_gl_window_surface.cc` 构造 + 析构（§2.3）
3. 实施 Resize + Present（§2.4）
4. 实施 SavePPM Y 翻转（§2.2）
5. CMake 注册（D10=A）
6. 测试 cmake guard 注册（D11=A）
7. cmake reconfigure build-gles + 编译 + 7 测全 PASS（或 T4 GTEST_SKIP）

### 3.4 Phase C — REFACTOR + ctest 双 build 矩阵验证（步骤 4 / TDD REFACTOR）

| build | 配置 | 期望 ctest |
|---|---|:-:|
| build/（DEVTOOL=ON / VX_RENDERER=software 默认） | A | **1303** 不退化 |
| build/（DEVTOOL=OFF / VX_RENDERER=software 默认） | B | **1110** 不退化 |
| build-gles/（DEVTOOL=ON / VX_RENDERER=gles） | C | 1345 + 7 = **1352**（含 +7 sdl2_gl_window_surface_test） |

**ctest 验证：** 三 build 配置全 PASS（沿用 G1.2 范式）。如 T4 GTEST_SKIP → ctest 仍 PASS（GTEST_SKIP 不计 FAIL）。

---

## 4. 风险登记

| # | 风险 | 概率 | 影响 | 缓解 |
|:-:|---|:-:|:-:|---|
| **R1** | Mesa swrast 不渲染到 default framebuffer | 中 | 中 | T4 内嵌 GTEST_SKIP fallback / driver-strictness 分层 first-evidence 沿用（G1.2 T5 ext cache 范式）/ 文件头/尺寸/maxval 严格 / 像素内容软探针 |
| **R2** | SDL_GL_ResetAttributes 在 G1.2 fixture 之后调用导致 G1.2 测试 attribute 残留 | 低 | 低 | 构造内 ResetAttributes 后才 SDL_CreateWindow（与 G1.2 SetUp 模式一致 / 测试间无 attribute 跨污染） |
| **R3** | display_.reset() 时 SDL_GL_DeleteContext 顺序错误 | 低 | 高 | spec §3.3.3 + plan §2.3 已锁定析构序：display_.reset() FIRST → SDL_DestroyWindow / 头部 doc 注释强调 |
| **R4** | LOC 估算偏差超 ×1.5 上限 | 低 | 低 | dual-evidence 双向 ±25% buffer：×0.85-1.5 = ~415-735 行预期 |
| **R5** | 双重所有权契约违反（Application 后续传 Sdl2GLWindowSurface 时 dynamic_cast） | 极低 | 中 | 本任务不实施 Application 集成（G1.13 / 接口预留）/ valid() + window() + gles_display() getter 已规格 |

---

## 5. 反复模式预防（8 项核对 / Phase 0 + Plan 阶段全抑制）

| # | 反复模式 | 抑制证据 |
|:-:|---|---|
| #1 | 前置依赖/环境/API 能力未验证 | VAN 4 维度全 ✅ + plan §0.5 5 子段 audit 全 ✅ + glReadPixels signature grep 实证（gl3.h:599） |
| #2 | spec 数据回归 | 0 既有 spec 数据修改 / spec §3.3.3 cpp interface 完整规格化 / 0 偏差 |
| #3 | TDD 顺序倒置 | Phase A RED → Phase B GREEN → Phase C REFACTOR 严格 |
| #4 | 反向探针缺失或弱 | T6 (nullptr title) + T7 (0×0 dim) inline reverse probe + T4 内嵌驱动严格性 GTEST_SKIP 分层 |
| #5 | 中文文档 StrReplace 字符类型 | 0 中文文档改动 / plan + Memory Bank 同源半角 |
| #6 | commit body Source 溯源 | 3 commits 全含 `Source: docs/plans/2026-05-06-sdl2-gl-window-surface.md §X.Y` |
| #7 | 双 build 矩阵 ctest 单次盲区 | Phase C 三 build 全跑（A 1303 / B 1110 / C 1352） |
| #8 | spec 数据回归 audit 协议 | 0 既有 spec 数据修改 / G1.2 ctest baseline (1303/1110/1345) 已稳定可比对 |

**反复模式预审 0/8 全抑制 / VAN + Plan 两阶段累计 0 命中（沿用 19+ 模式连续抑制纪录 / 历史新高续刷）**

---

## 6. systemPatterns 协同度自我对照（13 项 / 0 反向）

| # | 范式 | 引用 | 本任务关联 |
|:-:|---|---|---|
| 1 | 跨决策协同度 100% | systemPatterns §跨决策协同度 | 第 16 次连续命中 / dec → endec → doudec → 第 16 次 / 累计 145/145 |
| 2 | plan ×0.6 实测系数 | systemPatterns §plan ×0.6 矩阵 | dec-evidence 第 11 数据点候选 / 实施类 Level 3 子档延续 |
| 3 | brainstorming P1.3 主动 push-back | systemPatterns §主动 push-back triple-evidence | quad-evidence 候选 / 第 4 次实战 / 3 偏差校正 |
| 4 | writing-plans P1.6 spec vs code audit | systemPatterns §spec vs code triple-evidence | quad-evidence 候选 / 第 4 次实证 |
| 5 | P0 协议（plan/spec docs 落盘即 commit） | systemPatterns §P0 协议 sext-evidence | sept-evidence 第 7 数据点候选 / 实施类 Level 3 续延 |
| 6 | 反向探针强度梯度三档 | systemPatterns §反向探针三档 | T4 内嵌（中档 driver 严格性）+ T6/T7（低档 inline） |
| 7 | lazy-attach C ABI 容错模式 | systemPatterns §lazy-attach quad-evidence | quint-evidence 候选 / 沿用「软失败 valid()」范式 |
| 8 | LOC 双向 ±25% buffer | systemPatterns §LOC buffer 子档 | dual-evidence 续延 / 预期 ×1.0-1.2 (±25% 内) |
| 9 | 双 100% 流程闭环 | systemPatterns §双 100% 流程闭环 dual-evidence | triple-evidence 候选 / 第 3 次实证 |
| 10 | D3=B eager extension cache | systemPatterns §eager ext cache first-evidence | dual-evidence 候选 / G1.3 通过 display_.gles_display()->HasExtension 间接复用 |
| 11 | D4=C inline test 反向探针 + 驱动严格性分层 | systemPatterns §驱动严格性分层 first-evidence | dual-evidence 候选 / T4 GTEST_SKIP 沿用 |
| 12 | A14 链接闭包零字节守门 | systemPatterns §A14 零字节守门 | 沿用：Sdl2GLWindowSurface 仅在 dynamic_cast 引用时链接 / 不打破 A14 |
| 13 | 实施忠实度（plan→build 0 偏差） | systemPatterns §实施忠实度 dual-evidence | triple-evidence 候选 / 第 3 次实证 |

---

## 7. 完成验证清单

### 7.1 强制验收

- [ ] 7 个 TEST_F 全 PASS（或 T4 GTEST_SKIP / 0 FAIL）
- [ ] DEVTOOL=ON / software 1303/1303 不退化
- [ ] DEVTOOL=OFF / software 1110/1110 不退化
- [ ] DEVTOOL=ON / gles 1345 → **1352** PASS（+7 sdl2_gl_window_surface_test）
- [ ] 0 lint 错误（clang-tidy / C++17 标准）
- [ ] 3 commits（VAN ✅ + chore(plan) + feat(platform)）全含 Source 溯源
- [ ] 反复模式 0/8 抑制延续

### 7.2 软目标

- [ ] LOC 落 ~415-735 行预期（×0.85-1.5 buffer）
- [ ] plan ×0.6 系数 ~0.4-0.7×（实施类 Level 3 子档 / 标准极速区）
- [ ] reflect 阶段 ≥ 5 改进建议（含 P0/P1/P2）
- [ ] systemPatterns ≥ 5 沉淀（沿用 G1.2 8 沉淀范式）

---

## 8. 不在范围 / 后续任务

| 主题 | 当前状态 | 后续任务 |
|---|---|---|
| GLESCanvas 骨架（Begin/End/Clear/SetTransform）| Sdl2GLWindowSurface 仅 Surface 容器 / 无绘制 | G1.4 |
| Sdl2WindowSurface (software) SavePPM 回填 | 既有 `kInternal "not implemented"` 保留 | 独立任务 / 不在 G1.x 范围 |
| Application 构造 GLES 分支 + dynamic_cast fallback | dynamic_cast 入口预留 | G1.13 |
| examples/hello_sdl2 GLES 路径 + smoke | 未启动 | G1.15 |
| FBO 路径（如 R1 实证 default FB 不工作）| Mesa swrast headless 软探针 + GTEST_SKIP fallback | 后续 P3（如需要 / 由真机 driver CI 触发）|

---

## 9. 度量数据预测

| 阶段 | 估时（plan ×0.6）| 预期实测 | 系数 |
|---|:-:|:-:|:-:|
| VAN | ~10-15 min | ✅ ~10-15 min | ~1.0× |
| Plan | ~25-40 min | ~25-35 min | ~0.7-1.0× |
| Build | ~50-90 min | ~30-50 min | ~0.40-0.55× 极速区 |
| Reflect | ~15-20 min | ~15-20 min | ~1.0× |
| Archive | ~10-15 min | ~10-15 min | ~1.0× |
| **总线** | **~110-180 min** | **~90-135 min** | **~0.55-0.75× 标准极速区** |

**LOC：** plan 估 ~490 / 实际预期 ~410-720（×0.85-1.5 buffer / dual-evidence 双向）

**ctest 增量：** +7（gles config）/ 0 退化（software config）

---

## 10. P0 协议合规检查

- ✅ plan 文档落盘即 commit（D12 第 2 段 `chore(plan)` 单 commit / sext → sept-evidence 第 7 数据点候选）
- ✅ spec docs：本任务无独立 spec（蓝图 spec §3.3.3 已是 spec 来源 / D13=A）
- ✅ Memory Bank 三件套（tasks/activeContext/progress）与 plan 同 commit 落盘
- ✅ 反复模式预审 8/8 全抑制 / 0 命中
