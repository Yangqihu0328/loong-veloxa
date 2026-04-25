# SDL2 窗口后端 + 输入事件桥接 — 设计规格

**任务：** TASK-20260425-01
**复杂度：** Level 3
**创建日期：** 2026-04-25
**状态：** /plan 头脑风暴完成，待用户审查

---

## 1. 目的（What）

为 Veloxa 引入第一个**有可见窗口**的平台后端：基于 SDL2 在 WSLg / Linux Desktop 上开窗显示软件渲染输出，并把 SDL 输入事件桥接到现有 `VxInputEvent` 结构体与事件系统三阶段冒泡。

这是「实时调试 UI」主线（hot reload / inspector / overlay）的前置依赖。SDL2 后端落地后，下一任务才有可见画布做交互验证。

## 2. 不做（Out of Scope，YAGNI）

- ❌ 不引入 OpenGL ES / Vulkan 渲染路径（仍然是软件像素 buffer 上传到 SDL_Texture）
- ❌ 不引入多窗口 / 子窗口
- ❌ 不引入剪贴板 / 拖拽 / IME 中文输入法
- ❌ 不引入文件监听 / hot reload（下一任务）
- ❌ 不引入 inspector overlay（下一任务）
- ❌ 不引入 X11 / Wayland 直连后端（仅通过 SDL 抽象）

## 3. 成功标准（Acceptance）

| # | 标准 | 验证方式 |
|:-:|---|---|
| A1 | `examples/hello_sdl2.cc` 在 WSLg 下打开 400×300 窗口，渲染 hello 例子的颜色块布局 | 手工运行 + 屏幕观察 |
| A2 | 鼠标移动 / 点击驱动 `:hover` / `:active` 状态变化（颜色块响应） | 手工运行 + 屏幕观察 |
| A3 | `vx_view_quit()` 或关闭窗口（X 按钮）能干净退出，无内存泄漏 | `valgrind` 或 `ASan` 跑 ≥ 5 秒 |
| A4 | 现有 ctest 全绿（headless 路径不回归） | `ctest --output-on-failure` |
| A5 | 不开 `-DVX_PLATFORM_SDL2=ON` 时构建零变化 | 默认 `cmake -B build && cmake --build build` 无新依赖 |
| A6 | `SDL_VIDEODRIVER=dummy ./hello_sdl2` 能在 CI / headless server 跑通 | 单元 smoke（可选入 ctest） |
| A7 | Release `-O3 -Werror` 零警告 | `cmake -B build-release -DCMAKE_BUILD_TYPE=Release -DVX_PLATFORM_SDL2=ON` |

## 4. 用户决策（/plan 头脑风暴产出）

| # | 维度 | 选择 | 理由 |
|:-:|---|---|---|
| Q1 | 输入事件投递路径 | **(B) `SDL2EventLoop::PumpInputEvents(callback)` 裸函数** | EventLoop 不反向持有 View；保持平台抽象单向；与 headless 对称 |
| Q2 | Surface 像素呈现时机 | **(B) `Surface` 抽象增加 `virtual void Present() {}` 默认 no-op** | 语义清晰；headless 默认 no-op；SDL2 重写为 `SDL_UpdateTexture + RenderPresent`；View 在每帧 `Update()` 末尾调一次 |
| Q3 | CMake SDL2 查找方式 | **(C) 双轨兜底** | `find_package(SDL2 CONFIG QUIET)` 优先，失败 fallback `pkg_check_modules(SDL2 REQUIRED sdl2)`；现代+老 distro 兼容 |
| Q4 | 默认构建可选性 | **(C) 默认 OFF，需 `-DVX_PLATFORM_SDL2=ON`** | 与 `VX_BUILD_BENCHMARKS` 风格一致；不破坏 CI minimal 镜像；显式可见 |
| Q5 | examples 形态 | **(B) 保留 `hello.cc` 不动 + 新增 `examples/hello_sdl2.cc`** | PPM 与 SDL2 两种后端独立参考；smoke 测试可选入 ctest（`SDL_VIDEODRIVER=dummy`）|
| Q6 | C API 工厂命名 | **(C) `vx_event_loop_create_sdl2()` 无参 + `vx_surface_create_window(const VxWindowOptions*)` options struct** | 与现有 `_headless` / `_memory` 风格一致 + options struct 与 `VxViewConfig` 一致；未来加 flags 不破 ABI |

## 5. 架构

### 5.1 模块边界

```
┌─────────────────────────────────────────────────────────────────┐
│  examples/hello_sdl2.cc  (用户空间 — 仅 SDL2 ON 时编译)         │
│    ├─ vx_event_loop_create_sdl2()                              │
│    ├─ vx_surface_create_window(&opts)                          │
│    ├─ vx_view_create(&config)  // 复用现有                     │
│    ├─ 循环：PumpInputEvents → vx_view_inject_input → Update    │
│    └─ vx_view_run() 阻塞主循环（内部驱动 PumpInputEvents）      │
└─────────────────────────────────────────────────────────────────┘
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│  veloxa/api/veloxa_api.{h,cc}   (修改：基类指针正确化)          │
│    + vx_event_loop_create_sdl2()                                │
│    + vx_surface_create_window(VxWindowOptions*)                 │
│    + vx_event_loop_pump_input(loop, view) // helper             │
│    + VxWindowOptions struct                                     │
│    ! vx_event_loop_destroy / vx_surface_destroy /               │
│      vx_surface_save_ppm 改用基类 dynamic_cast / virtual dtor   │
└─────────────────────────────────────────────────────────────────┘
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│  veloxa/platform/                                               │
│    surface.h          (修改：+ virtual void Present() {})       │
│    event_loop.h       (不变)                                    │
│                                                                 │
│    headless/          (不变)                                    │
│      memory_surface.{h,cc}     (Present() 默认继承 no-op)       │
│      headless_event_loop.{h,cc}                                 │
│                                                                 │
│    sdl2/              (新增)                                    │
│      sdl2_window_surface.{h,cc}                                 │
│      sdl2_event_loop.{h,cc}                                     │
│      sdl2_input_translate.{h,cc}                                │
└─────────────────────────────────────────────────────────────────┘
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│  libSDL2 (system, libsdl2-dev 2.0.20)                           │
│    SDL_Init(SDL_INIT_VIDEO) / SDL_CreateWindow /                │
│    SDL_CreateRenderer / SDL_CreateTexture (RGBA8888) /          │
│    SDL_PollEvent / SDL_UpdateTexture / SDL_RenderPresent        │
└─────────────────────────────────────────────────────────────────┘
```

### 5.2 关键接口

#### 5.2.1 `Surface` 抽象增加 `Present()`

```cpp
// veloxa/platform/surface.h
namespace vx::platform {
class Surface {
 public:
  virtual ~Surface() = default;
  virtual vx::u32 width() const = 0;
  virtual vx::u32 height() const = 0;
  virtual vx::u32 stride() const = 0;
  virtual vx::u32* Lock() = 0;
  virtual void Unlock() = 0;
  virtual void Resize(vx::u32 width, vx::u32 height) = 0;
  virtual vx::Status SavePPM(const char* path) const = 0;

  // 新增：把已渲染的 buffer 推到屏幕。默认 no-op（headless / file-only 后端）。
  // SDL2 后端重写为 SDL_UpdateTexture + RenderPresent。
  // 调用约定：仅在 Lock/Unlock 配对完成后调用，且每帧最多一次。
  virtual void Present() {}
};
}  // namespace vx::platform
```

#### 5.2.2 `Sdl2EventLoop` 输入泵

```cpp
// veloxa/platform/sdl2/sdl2_event_loop.h
namespace vx::platform {

class Sdl2EventLoop : public EventLoop {
 public:
  using InputCallback = std::function<void(const VxInputEvent&)>;

  Sdl2EventLoop();   // 内部 SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS)
  ~Sdl2EventLoop() override;  // SDL_Quit

  // EventLoop 接口实现
  void Run() override;        // while (running_) { PollOnce(); SDL_Delay(1); }
  void Quit() override;
  bool is_running() const override;
  void PostTask(Task task) override;
  TimerId SetTimer(vx::u32 interval_ms, Task callback,
                   bool repeat = false) override;
  void CancelTimer(TimerId id) override;
  void PollOnce() override;   // 内部循环：SDL_PollEvent + 转 VxInputEvent

  // 新增：注册输入回调（每帧主动 pull 模式）
  // 默认 callback 为空 → SDL 输入事件被静默 drain（用于不需要输入的场景）
  void SetInputCallback(InputCallback cb);

  // 新增：手工驱动单次输入泵（不由 PollOnce 自动调用，给 hello_sdl2 例子使用）
  void PumpInputEvents();

 private:
  void HandleSdlEvent(const SDL_Event& ev);
  void DispatchInput(const VxInputEvent& vxe);
  // ... task/timer 字段同 HeadlessEventLoop
  InputCallback input_callback_;
  bool running_;
};

}  // namespace vx::platform
```

#### 5.2.3 `Sdl2WindowSurface`

```cpp
// veloxa/platform/sdl2/sdl2_window_surface.h
namespace vx::platform {

struct Sdl2WindowOptions {
  vx::u32 width = 800;
  vx::u32 height = 600;
  const char* title = "Veloxa";
  bool resizable = false;
  bool high_dpi = false;
};

class Sdl2WindowSurface : public Surface {
 public:
  explicit Sdl2WindowSurface(const Sdl2WindowOptions& opts);
  ~Sdl2WindowSurface() override;

  // Surface 接口实现
  vx::u32 width() const override;
  vx::u32 height() const override;
  vx::u32 stride() const override;
  vx::u32* Lock() override;
  void Unlock() override;
  void Resize(vx::u32 width, vx::u32 height) override;
  vx::Status SavePPM(const char* path) const override;
  void Present() override;  // SDL_UpdateTexture + SDL_RenderClear + SDL_RenderCopy + SDL_RenderPresent

  // SDL 内部访问（用于事件循环关联到窗口；不暴露给 C API）
  SDL_Window* sdl_window() const { return window_; }

 private:
  SDL_Window* window_;
  SDL_Renderer* renderer_;
  SDL_Texture* texture_;
  vx::u32* pixels_;       // 本地 owned buffer，渲染管线写入
  vx::u32 width_;
  vx::u32 height_;
  bool locked_;
};

}  // namespace vx::platform
```

#### 5.2.4 输入事件转换

```cpp
// veloxa/platform/sdl2/sdl2_input_translate.h
namespace vx::platform {

// 把单个 SDL_Event 转为 VxInputEvent。
// 返回 false 表示该 SDL_Event 不映射任何输入事件（如窗口事件）。
// 对于 SDL_QUIT / SDL_WINDOWEVENT_CLOSE，调用方应直接调 EventLoop::Quit()。
bool TranslateSdlEvent(const SDL_Event& sdl, VxInputEvent& out);

// 把 SDL keycode 转为 Veloxa key_code（先用 ASCII 子集，扩展键留 0）
uint32_t TranslateSdlKey(SDL_Keycode key);

// 把 SDL keymod 转为 Veloxa modifiers bit field
uint32_t TranslateSdlMod(SDL_Keymod mod);

}  // namespace vx::platform
```

#### 5.2.5 C API 扩展

```c
// veloxa/api/veloxa_api.h（追加）

typedef struct {
  uint32_t width;
  uint32_t height;
  const char* title;
  uint32_t flags;  // 预留：bit 0 = resizable, bit 1 = high_dpi（本次仅占位）
} VxWindowOptions;

VxEventLoop* vx_event_loop_create_sdl2(void);
VxSurface*   vx_surface_create_window(const VxWindowOptions* opts);

/* 在 vx_view_run() 之外手动驱动 SDL 输入事件 → View 注入。
 * 用于 hello_sdl2.cc 这类自定义主循环。*/
VxResult vx_event_loop_pump_input(VxEventLoop* loop, VxView* view);
```

### 5.3 关键技术债清理（隐含范围）

> 现状缺陷：`vx_event_loop_destroy` / `vx_surface_destroy` / `vx_surface_save_ppm` 在 `veloxa_api.cc` 硬编码 `HeadlessEventLoop*` / `MemorySurface*`。加 SDL2 后端时 `delete` 错的派生类指针会 UB。

**改造**（必做，本任务范围内）：
- `vx_event_loop_destroy` 改为 `delete reinterpret_cast<vx::platform::EventLoop*>(loop)`（基类已 `virtual ~`）
- `vx_surface_destroy` 改为 `delete reinterpret_cast<vx::platform::Surface*>(surface)`（基类已 `virtual ~`）
- `vx_surface_save_ppm` 改为基类指针调用 `SavePPM`（基类已 virtual）

这是 ABI 单向兼容的 — 现有调用者无感知，但解除了多后端的隐患。

### 5.4 数据流

#### 帧渲染流（每帧）

```
  vx_view_run() 内部主循环
     ↓
  Sdl2EventLoop::PollOnce()
     ├─ ProcessTasks()    (PostTask 队列)
     ├─ ProcessTimers()   (SetTimer 回调)
     └─ PumpInputEvents() ← 内部 while (SDL_PollEvent) {
                              if (TranslateSdlEvent) input_callback_(vxe);
                              if (SDL_QUIT) Quit();
                            }
     ↓
  Application::Update()    (用户的 vx_view_update 等价)
     ├─ Layout / Style / Render
     ├─ surface->Lock()    (获得 pixel buffer)
     ├─ canvas paint       (写入 pixel buffer)
     ├─ surface->Unlock()
     └─ surface->Present() ← 新增：SDL_UpdateTexture + SDL_RenderPresent
     ↓
  SDL_Delay(1)             (避免 100% CPU 占用；后续可改 vsync)
```

#### 输入事件流

```
  SDL_PollEvent 拿到 SDL_Event
     ↓
  Sdl2EventLoop::HandleSdlEvent(ev)
     ├─ if (ev.type == SDL_QUIT || SDL_WINDOWEVENT_CLOSE) → Quit()
     ├─ else TranslateSdlEvent(ev, vxe)
     │    ├─ SDL_MOUSEMOTION → POINTER_MOVE (x, y)
     │    ├─ SDL_MOUSEBUTTONDOWN → POINTER_DOWN (x, y, button)
     │    ├─ SDL_MOUSEBUTTONUP → POINTER_UP (x, y, button)
     │    ├─ SDL_KEYDOWN → KEY_DOWN (key_code, modifiers)
     │    ├─ SDL_KEYUP → KEY_UP (key_code, modifiers)
     │    └─ SDL_TEXTINPUT (本次跳过) / SDL_WINDOWEVENT_RESIZE (本次跳过) → return false
     └─ if (translated && input_callback_) input_callback_(vxe)
                                                ↓
                                  hello_sdl2 设置的 callback：
                                    vx_view_inject_input(view, &vxe)
                                                ↓
                                  Application::InjectInput → 现有事件系统三阶段冒泡
```

## 6. 安全考量

> 本任务被 VAN 标注为非「安全相关」（无外部认证 / 无新网络栈 / 无新解析），但 SDL 输入事件来源外部，做轻量威胁建模：

| 攻击面 | 风险 | 缓解措施 |
|---|---|---|
| 恶意输入事件洪水（如长按 / scroll storm） | 事件队列堆积 → DoS | 现有 `Application::InjectInput` 是同步消费，不堆积；SDL 内部队列由 SDL 管理（默认 65535 上限）|
| 异常 keycode（负值 / 超大值）| 现有 `key_code` 是 `uint32_t`，但内部下游可能 cast 为 enum | `TranslateSdlKey` 仅映射已知 SDL_K_* → ASCII 子集；未知键映射为 0 |
| `SDL_TEXTINPUT` UTF-8 多字节 | 本次不接 IME，但需确认 SDL_KEYDOWN 不泄漏 UTF-8 byte | TranslateSdlKey 只输出 ASCII 范围；高位字节 → 0 |
| Window title 字符串 | 用户传入 `const char*` 可能 NULL 或非终止 | `vx_surface_create_window` 在 NULL 时使用默认 "Veloxa"；`strlen` 不检长度（与 `vx_view_load_html` 风格一致，由调用者负责）|
| SDL_Init 失败（无显示环境）| `SDL_GetError()` 描述外泄到 stderr | 使用 `vx::log` 统一输出，不泄漏 SDL 内部地址 |
| 资源泄漏：SDL_Window/Renderer/Texture | 析构顺序错误导致 SIGSEGV | RAII：`Sdl2WindowSurface::~` 严格 reverse 创建顺序 destroy |

## 7. 测试策略

### 7.1 单元测试（GTest）

| 测试名 | 范围 | 类型 |
|---|---|---|
| `Sdl2InputTranslate.MouseMoveBasic` | TranslateSdlEvent 映射 SDL_MOUSEMOTION | TDD |
| `Sdl2InputTranslate.MouseButtonDownUp` | 左/中/右键映射 | TDD |
| `Sdl2InputTranslate.KeyDownAscii` | A-Z / 0-9 映射 | TDD |
| `Sdl2InputTranslate.KeyUnknownReturnsZero` | 不在 ASCII 子集的键返回 key_code=0 | TDD（边界） |
| `Sdl2InputTranslate.QuitEventNotTranslated` | SDL_QUIT 返回 false（不映射） | TDD（边界） |
| `Sdl2InputTranslate.ModifierBitField` | Shift / Ctrl / Alt 修饰位 | TDD |
| `Sdl2WindowSurface.LockUnlockPresentNoCrash` | 用 `SDL_VIDEODRIVER=dummy` 跑生命周期 | TDD（集成） |
| `Sdl2WindowSurface.PixelsBufferStrideMatchesWidth` | stride == width × 4 字节 | TDD |
| `Sdl2EventLoop.PostTaskExecutedInPollOnce` | 与 headless 对称行为 | TDD |
| `Sdl2EventLoop.SetTimerFiresAfterInterval` | 与 headless 对称 | TDD |
| `Sdl2EventLoop.PumpInputEventsCallback` | 注入 SDL_PushEvent + 验证 callback 命中 | TDD |

### 7.2 集成 / smoke

- `examples/hello_sdl2.cc` 在 `SDL_VIDEODRIVER=dummy` 下能跑完一帧不崩
  - 加入 ctest（仅 `VX_PLATFORM_SDL2=ON` 时生效）
- 手工 WSLg 验证 A1 / A2 / A3（无法自动化）

### 7.3 边界输入清单

| 类别 | 示例 | 预期行为 |
|---|---|---|
| 默认路径 | 800x600 + "title" | 创建窗口成功 |
| 空 | `vx_surface_create_window(NULL)` | 返回 NULL |
| `opts->title == NULL` | NULL title | 使用默认 "Veloxa" |
| 0 尺寸 | width=0 或 height=0 | SDL_CreateWindow 失败 → 返回 NULL |
| 巨大尺寸 | 100000x100000 | SDL 拒绝 → 返回 NULL（不内存爆炸）|
| 无显示环境 | `unset DISPLAY` 且无 dummy driver | `vx_event_loop_create_sdl2()` 返回 NULL |
| 多次 init | 同进程多次 `vx_event_loop_create_sdl2` | SDL_Init 内部 refcount 友好；用 `SDL_WasInit` 守卫 |

## 8. 注入点核对表

| 注入点 | 实际代码位置 | 数据可访问性 | 可写性 | 结论 |
|---|---|---|---|---|
| `Surface::Present()` 调用 | `Application::Update()` 末尾（`update_manager.cc` 中 paint 完成后）| 持有 `Surface*` | Surface 接口扩展 | ✅ 直接加 virtual 方法 |
| 输入 callback 注入 View | `Sdl2EventLoop::SetInputCallback` 在 hello_sdl2.cc 的 main() 设置；callback 闭包捕获 `VxView*` 调 `vx_view_inject_input` | hello_sdl2 拥有所有句柄 | C API 已有 `vx_view_inject_input` | ✅ 已通 |
| `vx_event_loop_pump_input` helper | 新增 C API，内部 `static_cast<Sdl2EventLoop*>` + 设置 callback 调 `vx_view_inject_input` | View 与 Loop 都有句柄 | C API 扩展 | ✅ 直接新增 |

## 9. CMake 链接方向约束分析

### 9.1 新增模块 `vx_platform_sdl2`（仅 `VX_PLATFORM_SDL2=ON` 时构建）

```cmake
# veloxa/platform/sdl2/CMakeLists.txt
if(NOT VX_PLATFORM_SDL2)
  return()
endif()

# 双轨 SDL2 查找
find_package(SDL2 CONFIG QUIET)
if(NOT SDL2_FOUND)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
  add_library(SDL2::SDL2 ALIAS PkgConfig::SDL2)
endif()

add_library(vx_platform_sdl2 STATIC
  sdl2_window_surface.cc
  sdl2_event_loop.cc
  sdl2_input_translate.cc
)
target_include_directories(vx_platform_sdl2 PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(vx_platform_sdl2
  PUBLIC vx_platform           # Surface / EventLoop 抽象
  PUBLIC SDL2::SDL2            # PUBLIC 因为 .h 暴露 SDL_Window* getter
  PRIVATE vx_foundation        # log / status
)
```

### 9.2 链接拓扑变更

| 目标 | 当前依赖 | 新增依赖 | 链接关键字 |
|---|---|---|---|
| `vx_platform_sdl2` (新) | — | `vx_platform`, `SDL2::SDL2`, `vx_foundation` | PUBLIC / PUBLIC / PRIVATE |
| `vx_api` | `vx_core`, `vx_platform_headless` | + `vx_platform_sdl2`（条件） | PRIVATE |
| `hello_sdl2` (新 example) | — | `vx_api` | PRIVATE |

**循环依赖审计：** `vx_platform_sdl2 → vx_platform`（已有），`vx_api → vx_platform_sdl2`（新），无循环。

**头文件暴露：**
- `sdl2_window_surface.h` 暴露 `SDL_Window*` getter → 必须 PUBLIC link `SDL2::SDL2`
- `vx_api` 实现里 `#include "veloxa/platform/sdl2/sdl2_window_surface.h"` → 通过 `vx_platform_sdl2` PUBLIC 传递 SDL2 头路径

## 10. 文件清单

### 新增

| 文件 | 职责 |
|---|---|
| `veloxa/platform/sdl2/CMakeLists.txt` | 仅 SDL2 ON 时定义 `vx_platform_sdl2` 静态库 |
| `veloxa/platform/sdl2/sdl2_window_surface.h` | `Sdl2WindowSurface` 类声明 + `Sdl2WindowOptions` |
| `veloxa/platform/sdl2/sdl2_window_surface.cc` | SDL_Window/Renderer/Texture 生命周期 + Lock/Unlock/Present |
| `veloxa/platform/sdl2/sdl2_event_loop.h` | `Sdl2EventLoop` 类声明 + `InputCallback` typedef |
| `veloxa/platform/sdl2/sdl2_event_loop.cc` | EventLoop 接口实现 + SDL_PollEvent 泵 |
| `veloxa/platform/sdl2/sdl2_input_translate.h` | `TranslateSdlEvent` / `TranslateSdlKey` / `TranslateSdlMod` 函数声明 |
| `veloxa/platform/sdl2/sdl2_input_translate.cc` | SDL_Event → VxInputEvent 映射实现 |
| `tests/platform/sdl2/sdl2_input_translate_test.cc` | 6 个 input translate 单元测试 |
| `tests/platform/sdl2/sdl2_window_surface_test.cc` | 2 个 surface 集成测试（dummy driver） |
| `tests/platform/sdl2/sdl2_event_loop_test.cc` | 3 个 event loop 单元测试 |
| `tests/platform/sdl2/CMakeLists.txt` | 仅 SDL2 ON 时定义 sdl2 测试目标 |
| `examples/hello_sdl2.cc` | SDL2 后端版本的 hello 例子（仅 SDL2 ON 时构建） |
| `docs/specs/2026-04-25-sdl2-window-backend-design.md` | 本设计规格 |
| `docs/plans/2026-04-25-sdl2-window-backend.md` | 实现计划 |

### 修改

| 文件 | 改动 |
|---|---|
| `CMakeLists.txt` (root) | 新增 `option(VX_PLATFORM_SDL2 ...)` + `add_subdirectory(veloxa/platform/sdl2)` 条件 |
| `veloxa/platform/surface.h` | `Surface` 添加 `virtual void Present() {}` |
| `veloxa/platform/CMakeLists.txt` | 把 `surface.h` 的 Present 默认实现纳入（或在子类各自实现）|
| `veloxa/api/veloxa_api.h` | 新增 `VxWindowOptions` + 3 个工厂 / pump 函数 |
| `veloxa/api/veloxa_api.cc` | 新增工厂实现 + 修复 destroy / save_ppm 用基类指针 |
| `veloxa/api/CMakeLists.txt` | 条件 link `vx_platform_sdl2`（仅 SDL2 ON 时）|
| `examples/CMakeLists.txt` | 条件添加 `hello_sdl2` 目标 |
| `tests/CMakeLists.txt` | 条件 add_subdirectory(`platform/sdl2`)（仅 SDL2 ON 时）|
| `memory-bank/techContext.md` | 追加「SDL2 平台后端」段（依赖、构建开关、WSLg 注意事项）|
| `memory-bank/productContext.md` | 「已实现的核心功能」表追加 SDL2 窗口后端行 |

## 11. 估时（plan × 0.6 协议）

| Phase | 内容 | plan 估时 | × 0.6 实测预期 |
|---|---|---:|---:|
| P0 | 基线核验 + Surface::Present 接口扩展 + destroy/save_ppm 基类化 | 25 min | 15 min |
| P1 | sdl2_input_translate.{h,cc} + 6 GTests TDD | 50 min | 30 min |
| P2 | sdl2_window_surface.{h,cc} + 2 集成测试 | 70 min | 42 min |
| P3 | sdl2_event_loop.{h,cc} + 3 单元测试 | 50 min | 30 min |
| P4 | C API 扩展（VxWindowOptions / 3 函数）+ veloxa_api.cc 修复 | 35 min | 21 min |
| P5 | examples/hello_sdl2.cc + ctest smoke (`SDL_VIDEODRIVER=dummy`) | 40 min | 24 min |
| P6 | WSLg 手工验证 A1/A2/A3 + Release `-Werror` 通路 + memory-bank 文档更新 | 30 min | 18 min |
| **合计** | | **~300 min (5h)** | **~180 min (3h)** |

> 第 8 数据点（首个新模块类任务）；预期 ≥ 1.5× "准确档"，非"最窄"档。

## 12. 风险登记

| # | 风险 | 触发概率 | 影响 | 预案 |
|:-:|---|:-:|:-:|---|
| R1 | WSLg 实际显示器协议（X11/Wayland）行为差异导致 SDL_CreateWindow 失败 | 中 | 高 | P5 手工验证发现立刻 fallback `SDL_VIDEODRIVER=x11` 或加 hint |
| R2 | SDL2_TEXTURESTREAMING 在 dummy driver 下 SDL_LockTexture 行为异常 | 低 | 中 | 改用本地 owned `vx::u32*` buffer + SDL_UpdateTexture 上传（已采用此方案）|
| R3 | `find_package(SDL2 CONFIG)` 在某些 Ubuntu 上 export target 名为 `SDL2::SDL2-static` 而非 `SDL2::SDL2` | 低 | 低 | CMake 双轨用 `if(TARGET ...)` 兜底两种命名 |
| R4 | `delete reinterpret_cast<EventLoop*>(loop)` 对原 `HeadlessEventLoop*` 已通过 `reinterpret_cast<VxEventLoop*>` 类型擦除 — 多继承未来加入会破裂 | 低 | 高 | 当前 `HeadlessEventLoop` 单继承，等价；P4 加注释 + 写到 reflection #1 入未来规则 |
| R5 | `pkg_check_modules` 与 `find_package` 同 build 内冲突（之前 TASK-19-02 反思 #2 提及）| 中 | 低 | CMake 段 `if(NOT SDL2_FOUND) ... endif()` 严格短路；不双调用 |
| R6 | SDL2 ABI 在 2.0.20 缺 `SDL_HINT_RENDER_DRIVER=software` → 老硬件可能跑不起来 | 低 | 低 | 显式 `SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software")` 强制软件渲染（避免 GPU 驱动问题）|

## 13. 与未来任务的关系

| 后续任务 | 本任务为其铺路 |
|---|---|
| TASK-20260425-02（拟）— DevTool MVP（hot reload + FPS overlay + 脏区可视化）| 必须有可见窗口才有调试意义 |
| TASK-20260425-03（拟）— DOM Inspector（dogfood）| 同上 |
| TASK-20260426+（拟）— DRM/KMS 嵌入式后端 | `Surface::Present()` 抽象 + 双轨 CMake 模板可直接复用 |
| TASK-20260427+（拟）— OpenGL ES 渲染路径 | `Sdl2WindowSurface` 可换为 `SDL_GL_CreateContext`，接口不变 |

---

**审查检查点：** 用户在阅读本规格后，确认以下任一条款（在批准 / 修改后启动 /build）：
1. ✅ 6 个 Q1-Q6 决策选项是否合意
2. ✅ §5.3 技术债清理是否纳入本任务（推荐：是；否则下个任务必须立项 fix）
3. ✅ §11 估时（180 min × 0.6 实测）是否合理；是否要拆为 2 任务（如 P1-P4 一个 + P5-P6 一个）
4. ✅ §12 R1 WSLg 风险是否可接受（无法在沙箱内手工 WSLg 验证 A1/A2/A3，需用户协助跑）
