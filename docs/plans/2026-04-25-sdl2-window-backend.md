# SDL2 窗口后端 + 输入事件桥接 实现计划

**目标：** 为 Veloxa 引入第一个有可见窗口的平台后端（基于 SDL2），让软件渲染输出在 WSLg / Linux Desktop 上开窗显示，并把 SDL 输入事件桥接到现有事件系统三阶段冒泡。

**架构：** 在现有 `vx::platform::EventLoop` / `Surface` 抽象基类下新增 `vx::platform::Sdl2EventLoop` / `Sdl2WindowSurface` 两个后端实现；`Surface` 抽象增加 `virtual void Present()` 默认 no-op，由 SDL2 后端重写为 `SDL_UpdateTexture + SDL_RenderPresent`；输入路径采用裸 callback 模式，`Sdl2EventLoop::SetInputCallback` 由 `vx_event_loop_pump_input` C API 内部桥接到 `vx_view_inject_input`，避免 EventLoop 反向持有 View。

**技术栈：** C++17 / CMake / SDL2 2.0.20（系统包 `libsdl2-dev`） / GTest / 现有 `vx::platform::*`

**复杂度级别：** Level 3

**任务 ID：** TASK-20260425-01

**分支：** `feature/TASK-20260425-01-sdl2-backend`（已基于 main `e52868b` 创建）

---

## Phase 0 — 全局约束与基线核验

### 任务 0.1：基线 + 工具链 grep

- [ ] **步骤 1：核对 SDL2 dev 已装**
  ```bash
  pkg-config --modversion sdl2
  ls /usr/include/SDL2/SDL.h
  ls /usr/lib/x86_64-linux-gnu/cmake/SDL2/sdl2-config.cmake
  ```
  预期：`2.0.20` + 两条 ls 全部命中

- [ ] **步骤 2：smoke 工具矩阵**
  ```bash
  for tool in pkg-config cmake ctest gcc rg; do
    command -v $tool || echo "MISS: $tool"
  done
  ```
  预期：全部命中（无 MISS）

- [ ] **步骤 3：FetchContent 代理状态确认（守卫规则）**
  ```bash
  git config --global --get http.proxy
  ls build/_deps/ build-bench/_deps/
  ```
  预期：proxy 空（无需补设；`_deps/` 已离线预置 quickjsng+benchmark；本任务不引入新 FetchContent → 跳过守卫）

- [ ] **步骤 4：现有 ctest 基线全绿**
  ```bash
  cd build && ctest --output-on-failure -j 4 2>&1 | tail -5
  ```
  预期：`100% tests passed`（基线快照写入 `progress.md`）

- [ ] **步骤 5：grep 现有 EventLoop / Surface 接口实现位置**
  ```bash
  rg "class.*EventLoop.*public" --type cpp veloxa/platform/
  rg "class.*Surface.*public" --type cpp veloxa/platform/
  ```
  预期：找到 `HeadlessEventLoop` / `MemorySurface` 一对（确认抽象层无遗漏）

- [ ] **步骤 6：提交 — Phase 0 基线快照**
  ```bash
  git add memory-bank/  # tasks.md / activeContext.md
  git commit -m "chore(workflow): TASK-20260425-01 P0 — VAN + plan baseline"
  ```

---

### 任务 0.2：`Surface::Present()` 接口扩展（前置抽象层改造）

[TDD]

**文件：**
- 修改：`veloxa/platform/surface.h`
- 测试：`tests/platform/headless/memory_surface_test.cc`（已存在；新增一例）

- [ ] **步骤 1：写失败测试**
  在 `memory_surface_test.cc` 末尾追加：
  ```cpp
  TEST(MemorySurfaceTest, PresentDefaultNoOpDoesNotCrash) {
    vx::platform::MemorySurface surface(100, 50);
    auto* pixels = surface.Lock();
    pixels[0] = 0xFF000000;
    surface.Unlock();
    surface.Present();   // 默认 no-op；只要不崩即通过
    EXPECT_EQ(surface.width(), 100u);
  }
  ```

- [ ] **步骤 2：运行测试验证失败**
  ```bash
  cmake --build build --target memory_surface_test 2>&1 | tail -5
  ```
  预期：编译错误 `'Present' is not a member of 'vx::platform::MemorySurface'` 或基类

- [ ] **步骤 3：实现最少代码**
  在 `veloxa/platform/surface.h` 的 `class Surface` 末尾（在 `SavePPM` 之后）添加：
  ```cpp
    // Push the rendered pixel buffer to the screen if the backend supports it.
    // Default no-op for headless / file-only backends.
    // Contract: call exactly once per frame, after Lock/Unlock pair completes.
    virtual void Present() {}
  ```

- [ ] **步骤 4：运行测试验证通过**
  ```bash
  cmake --build build --target memory_surface_test
  ./build/tests/memory_surface_test --gtest_filter='MemorySurfaceTest.PresentDefaultNoOpDoesNotCrash'
  ```
  预期：PASS

- [ ] **步骤 5：完整 ctest 防回归**
  ```bash
  cd build && ctest --output-on-failure -j 4
  ```
  预期：所有原有测试仍 PASS（含本次新增 1 例）

- [ ] **步骤 6：提交**
  ```bash
  git add veloxa/platform/surface.h tests/platform/headless/memory_surface_test.cc
  git commit -m "feat(platform): add Surface::Present() virtual no-op for backends [TASK-20260425-01 P0]"
  ```

---

### 任务 0.3：`vx_event_loop_destroy` / `vx_surface_destroy` / `vx_surface_save_ppm` 基类化（技术债清理）

[覆盖补充]

**文件：**
- 修改：`veloxa/api/veloxa_api.cc`
- 测试：`tests/api/veloxa_api_test.cc`（已存在）

- [ ] **步骤 1：编写测试覆盖正常路径（确认现状仍工作）**
  在 `veloxa_api_test.cc` 末尾追加：
  ```cpp
  TEST(VeloxaApiTest, SurfaceDestroyViaBaseClassPointerNoLeak) {
    VxSurface* s = vx_surface_create_memory(64, 48);
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(vx_surface_save_ppm(s, "/tmp/_vxtest.ppm"), VX_OK);
    vx_surface_destroy(s);
  }
  TEST(VeloxaApiTest, EventLoopDestroyViaBaseClassPointer) {
    VxEventLoop* loop = vx_event_loop_create_headless();
    ASSERT_NE(loop, nullptr);
    vx_event_loop_destroy(loop);
  }
  ```

- [ ] **步骤 2：运行测试验证通过**
  ```bash
  cmake --build build --target veloxa_api_test
  ./build/tests/veloxa_api_test --gtest_filter='VeloxaApiTest.*ViaBaseClass*'
  ```
  预期：PASS（实现已存在）

- [ ] **步骤 3：验证测试有效性（RED 反向探针）**
  临时把 `vx_surface_destroy` 改为 `delete static_cast<int*>(nullptr);`，确认测试 SIGSEGV / FAIL；恢复后再次确认 PASS。

- [ ] **步骤 4：改造为基类指针**
  在 `veloxa/api/veloxa_api.cc`：
  ```cpp
  void vx_event_loop_destroy(VxEventLoop* loop) {
    delete reinterpret_cast<vx::platform::EventLoop*>(loop);
  }

  void vx_surface_destroy(VxSurface* surface) {
    delete reinterpret_cast<vx::platform::Surface*>(surface);
  }

  VxResult vx_surface_save_ppm(const VxSurface* surface, const char* path) {
    if (!surface || !path) return VX_ERROR_NULL_PARAM;
    auto* s = reinterpret_cast<const vx::platform::Surface*>(surface);
    auto status = s->SavePPM(path);
    return status.ok() ? VX_OK : VX_ERROR_INVALID_STATE;
  }
  ```
  在 `vx_event_loop_destroy` / `vx_surface_destroy` 上方各加一行注释：
  ```cpp
  // ABI contract: handle is always a vx::platform::EventLoop* (or derived);
  // base class has virtual ~. Adding multiply-inherited backends would break
  // this reinterpret_cast — see TASK-20260425-01 spec §5.3.
  ```
  同时确认 `#include "veloxa/platform/surface.h"` 已存在（基类头）。

- [ ] **步骤 5：补充测试 — Headless 销毁后再创建对称**
  ```cpp
  TEST(VeloxaApiTest, RepeatedHeadlessCreateDestroyNoStateLeak) {
    for (int i = 0; i < 5; ++i) {
      VxEventLoop* loop = vx_event_loop_create_headless();
      VxSurface* surf = vx_surface_create_memory(32, 32);
      vx_surface_destroy(surf);
      vx_event_loop_destroy(loop);
    }
    SUCCEED();
  }
  ```

- [ ] **步骤 6：运行所有 API 测试**
  ```bash
  cmake --build build --target veloxa_api_test
  ./build/tests/veloxa_api_test
  cd build && ctest --output-on-failure
  ```
  预期：全部 PASS

- [ ] **步骤 7：提交**
  ```bash
  git add veloxa/api/veloxa_api.cc tests/api/veloxa_api_test.cc
  git commit -m "refactor(api): use base class pointer for destroy/save_ppm to support multi-backend [TASK-20260425-01 P0]"
  ```

---

## Phase 1 — `sdl2_input_translate` 模块（输入事件转换）

> 优先做这个：纯函数无依赖，可独立 TDD 跑通；不依赖 SDL_Init，单元测试只用 `SDL_Event` struct 字面量。

### 任务 1.1：CMakeLists 骨架 + 选项开关

[TDD]

**文件：**
- 修改：`CMakeLists.txt` (root)
- 创建：`veloxa/platform/sdl2/CMakeLists.txt`

- [ ] **步骤 1：root CMakeLists 加 option**
  在 root `CMakeLists.txt` 适当位置（与 `option(VX_BUILD_BENCHMARKS ...)` 同段）：
  ```cmake
  option(VX_PLATFORM_SDL2 "Build SDL2 platform backend (window + input)" OFF)
  ```

- [ ] **步骤 2：root 条件 add_subdirectory**
  在 root CMakeLists 加载 `veloxa/platform/headless` 之后追加：
  ```cmake
  if(VX_PLATFORM_SDL2)
    add_subdirectory(veloxa/platform/sdl2)
  endif()
  ```

- [ ] **步骤 3：创建 `veloxa/platform/sdl2/CMakeLists.txt` 骨架**
  ```cmake
  # SDL2 dual-track lookup: prefer modern CMake config, fallback to pkg-config.
  find_package(SDL2 CONFIG QUIET)
  if(NOT TARGET SDL2::SDL2)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2_PC REQUIRED IMPORTED_TARGET sdl2)
    add_library(SDL2::SDL2 ALIAS PkgConfig::SDL2_PC)
  endif()

  add_library(vx_platform_sdl2 STATIC
    sdl2_input_translate.cc
    # sdl2_window_surface.cc  (Phase 2)
    # sdl2_event_loop.cc      (Phase 3)
  )
  target_include_directories(vx_platform_sdl2 PUBLIC ${PROJECT_SOURCE_DIR})
  target_link_libraries(vx_platform_sdl2
    PUBLIC vx_platform SDL2::SDL2
    PRIVATE vx_foundation
  )
  ```

- [ ] **步骤 4：fresh configure 验证**
  ```bash
  rm -rf build-sdl2 && cmake -B build-sdl2 -DVX_PLATFORM_SDL2=ON 2>&1 | tail -10
  ```
  预期：`-- Configuring done` 无错（即使 .cc 还不存在，CMake 配置阶段不应失败）

  > 若步骤 4 失败因 `.cc 文件未找到`：进入步骤 5 先创建空 .cc。

- [ ] **步骤 5：创建空占位 `sdl2_input_translate.{h,cc}`**
  ```cpp
  // veloxa/platform/sdl2/sdl2_input_translate.h
  #ifndef VELOXA_PLATFORM_SDL2_SDL2_INPUT_TRANSLATE_H_
  #define VELOXA_PLATFORM_SDL2_SDL2_INPUT_TRANSLATE_H_
  #include <SDL2/SDL.h>
  #include <cstdint>
  #include "veloxa/api/veloxa_api.h"
  namespace vx::platform {
  bool TranslateSdlEvent(const SDL_Event& sdl, VxInputEvent& out);
  uint32_t TranslateSdlKey(SDL_Keycode key);
  uint32_t TranslateSdlMod(SDL_Keymod mod);
  }
  #endif
  ```
  ```cpp
  // veloxa/platform/sdl2/sdl2_input_translate.cc
  #include "veloxa/platform/sdl2/sdl2_input_translate.h"
  namespace vx::platform {
  bool TranslateSdlEvent(const SDL_Event&, VxInputEvent&) { return false; }
  uint32_t TranslateSdlKey(SDL_Keycode) { return 0; }
  uint32_t TranslateSdlMod(SDL_Keymod) { return 0; }
  }
  ```

- [ ] **步骤 6：fresh configure + 占位编译**
  ```bash
  cmake -B build-sdl2 -DVX_PLATFORM_SDL2=ON
  cmake --build build-sdl2 --target vx_platform_sdl2 2>&1 | tail -5
  ```
  预期：`Built target vx_platform_sdl2`

- [ ] **步骤 7：提交骨架**
  ```bash
  git add CMakeLists.txt veloxa/platform/sdl2/
  git commit -m "build(sdl2): scaffold vx_platform_sdl2 target with VX_PLATFORM_SDL2 opt-in [TASK-20260425-01 P1]"
  ```

---

### 任务 1.2：`TranslateSdlEvent` — MouseMove + MouseButtonDown/Up

[TDD]

**文件：**
- 修改：`veloxa/platform/sdl2/sdl2_input_translate.cc`
- 创建：`tests/platform/sdl2/sdl2_input_translate_test.cc`
- 创建：`tests/platform/sdl2/CMakeLists.txt`

- [ ] **步骤 1：创建测试 CMakeLists 骨架**
  `tests/platform/sdl2/CMakeLists.txt`：
  ```cmake
  add_executable(sdl2_input_translate_test sdl2_input_translate_test.cc)
  target_link_libraries(sdl2_input_translate_test
    PRIVATE vx_platform_sdl2 GTest::gtest_main
  )
  add_test(NAME sdl2_input_translate_test COMMAND sdl2_input_translate_test)
  set_tests_properties(sdl2_input_translate_test PROPERTIES
    ENVIRONMENT "SDL_VIDEODRIVER=dummy"
  )
  ```
  在 `tests/CMakeLists.txt` 最末追加（找已有 add_subdirectory 段）：
  ```cmake
  if(VX_PLATFORM_SDL2)
    add_subdirectory(platform/sdl2)
  endif()
  ```

- [ ] **步骤 2：写失败测试 — MouseMove + 2 个 MouseButton 用例**
  `sdl2_input_translate_test.cc`：
  ```cpp
  #include "veloxa/platform/sdl2/sdl2_input_translate.h"
  #include <SDL2/SDL.h>
  #include <gtest/gtest.h>

  TEST(Sdl2InputTranslate, MouseMoveBasic) {
    SDL_Event ev{};
    ev.type = SDL_MOUSEMOTION;
    ev.motion.x = 123;
    ev.motion.y = 456;
    VxInputEvent out{};
    ASSERT_TRUE(vx::platform::TranslateSdlEvent(ev, out));
    EXPECT_EQ(out.type, VX_EVENT_POINTER_MOVE);
    EXPECT_FLOAT_EQ(out.x, 123.0f);
    EXPECT_FLOAT_EQ(out.y, 456.0f);
  }

  TEST(Sdl2InputTranslate, MouseButtonDownLeft) {
    SDL_Event ev{};
    ev.type = SDL_MOUSEBUTTONDOWN;
    ev.button.x = 10;
    ev.button.y = 20;
    ev.button.button = SDL_BUTTON_LEFT;
    VxInputEvent out{};
    ASSERT_TRUE(vx::platform::TranslateSdlEvent(ev, out));
    EXPECT_EQ(out.type, VX_EVENT_POINTER_DOWN);
    EXPECT_EQ(out.button, 0);  // 左键 → 0
  }

  TEST(Sdl2InputTranslate, MouseButtonUpRight) {
    SDL_Event ev{};
    ev.type = SDL_MOUSEBUTTONUP;
    ev.button.button = SDL_BUTTON_RIGHT;
    VxInputEvent out{};
    ASSERT_TRUE(vx::platform::TranslateSdlEvent(ev, out));
    EXPECT_EQ(out.type, VX_EVENT_POINTER_UP);
    EXPECT_EQ(out.button, 2);  // 右键 → 2
  }
  ```

- [ ] **步骤 3：运行测试验证失败**
  ```bash
  cmake --build build-sdl2 --target sdl2_input_translate_test
  ./build-sdl2/tests/platform/sdl2/sdl2_input_translate_test
  ```
  预期：3 个测试 FAIL（占位实现总返回 false）

- [ ] **步骤 4：实现最少代码**
  替换 `sdl2_input_translate.cc::TranslateSdlEvent`：
  ```cpp
  bool TranslateSdlEvent(const SDL_Event& sdl, VxInputEvent& out) {
    out = {};
    switch (sdl.type) {
      case SDL_MOUSEMOTION:
        out.type = VX_EVENT_POINTER_MOVE;
        out.x = static_cast<float>(sdl.motion.x);
        out.y = static_cast<float>(sdl.motion.y);
        return true;
      case SDL_MOUSEBUTTONDOWN:
      case SDL_MOUSEBUTTONUP:
        out.type = (sdl.type == SDL_MOUSEBUTTONDOWN) ? VX_EVENT_POINTER_DOWN
                                                    : VX_EVENT_POINTER_UP;
        out.x = static_cast<float>(sdl.button.x);
        out.y = static_cast<float>(sdl.button.y);
        // SDL_BUTTON_LEFT=1, MIDDLE=2, RIGHT=3 → 0/1/2
        out.button = static_cast<uint8_t>(sdl.button.button - 1);
        return true;
      default:
        return false;
    }
  }
  ```

- [ ] **步骤 5：运行测试验证通过**
  ```bash
  cmake --build build-sdl2 --target sdl2_input_translate_test
  ./build-sdl2/tests/platform/sdl2/sdl2_input_translate_test
  ```
  预期：3 个测试 PASS

- [ ] **步骤 6：提交**
  ```bash
  git add veloxa/platform/sdl2/sdl2_input_translate.cc tests/platform/sdl2/
  git commit -m "feat(sdl2): translate mouse events to VxInputEvent [TASK-20260425-01 P1]"
  ```

---

### 任务 1.3：`TranslateSdlEvent` — KeyDown / KeyUp + 修饰位 + Quit 不映射

[TDD]

**文件：**
- 修改：`veloxa/platform/sdl2/sdl2_input_translate.cc`
- 修改：`tests/platform/sdl2/sdl2_input_translate_test.cc`

- [ ] **步骤 1：写失败测试**
  ```cpp
  TEST(Sdl2InputTranslate, KeyDownAscii) {
    SDL_Event ev{};
    ev.type = SDL_KEYDOWN;
    ev.key.keysym.sym = SDLK_a;
    ev.key.keysym.mod = KMOD_NONE;
    VxInputEvent out{};
    ASSERT_TRUE(vx::platform::TranslateSdlEvent(ev, out));
    EXPECT_EQ(out.type, VX_EVENT_KEY_DOWN);
    EXPECT_EQ(out.key_code, static_cast<uint32_t>('a'));
    EXPECT_EQ(out.modifiers, 0u);
  }

  TEST(Sdl2InputTranslate, KeyUpModifierBitField) {
    SDL_Event ev{};
    ev.type = SDL_KEYUP;
    ev.key.keysym.sym = SDLK_z;
    ev.key.keysym.mod = static_cast<SDL_Keymod>(KMOD_LSHIFT | KMOD_LCTRL);
    VxInputEvent out{};
    ASSERT_TRUE(vx::platform::TranslateSdlEvent(ev, out));
    EXPECT_EQ(out.type, VX_EVENT_KEY_UP);
    EXPECT_EQ(out.key_code, static_cast<uint32_t>('z'));
    EXPECT_NE(out.modifiers & 0x1, 0u);  // SHIFT bit
    EXPECT_NE(out.modifiers & 0x2, 0u);  // CTRL bit
  }

  TEST(Sdl2InputTranslate, KeyOutOfAsciiReturnsZero) {
    SDL_Event ev{};
    ev.type = SDL_KEYDOWN;
    ev.key.keysym.sym = SDLK_F1;  // 非 ASCII
    VxInputEvent out{};
    ASSERT_TRUE(vx::platform::TranslateSdlEvent(ev, out));
    EXPECT_EQ(out.key_code, 0u);
  }

  TEST(Sdl2InputTranslate, QuitEventReturnsFalse) {
    SDL_Event ev{};
    ev.type = SDL_QUIT;
    VxInputEvent out{};
    EXPECT_FALSE(vx::platform::TranslateSdlEvent(ev, out));
  }
  ```

- [ ] **步骤 2：运行测试验证失败**
  ```bash
  cmake --build build-sdl2 --target sdl2_input_translate_test
  ./build-sdl2/tests/platform/sdl2/sdl2_input_translate_test
  ```
  预期：4 个新测试 FAIL，原 3 个仍 PASS

- [ ] **步骤 3：实现 KeyDown/Up + Modifier 映射**
  在 `TranslateSdlEvent` switch 追加 case，并实现 `TranslateSdlKey` / `TranslateSdlMod`：
  ```cpp
  uint32_t TranslateSdlKey(SDL_Keycode key) {
    if (key >= 32 && key < 127) return static_cast<uint32_t>(key);
    return 0;  // F1-F12, arrow keys, etc. — 本任务暂不映射
  }

  uint32_t TranslateSdlMod(SDL_Keymod mod) {
    uint32_t m = 0;
    if (mod & (KMOD_LSHIFT | KMOD_RSHIFT)) m |= 0x1;
    if (mod & (KMOD_LCTRL  | KMOD_RCTRL )) m |= 0x2;
    if (mod & (KMOD_LALT   | KMOD_RALT  )) m |= 0x4;
    if (mod & (KMOD_LGUI   | KMOD_RGUI  )) m |= 0x8;
    return m;
  }

  // 在 TranslateSdlEvent switch 内追加：
  case SDL_KEYDOWN:
  case SDL_KEYUP:
    out.type = (sdl.type == SDL_KEYDOWN) ? VX_EVENT_KEY_DOWN : VX_EVENT_KEY_UP;
    out.key_code = TranslateSdlKey(sdl.key.keysym.sym);
    out.modifiers = TranslateSdlMod(sdl.key.keysym.mod);
    return true;
  ```

- [ ] **步骤 4：运行测试验证通过**
  ```bash
  cmake --build build-sdl2 --target sdl2_input_translate_test
  ./build-sdl2/tests/platform/sdl2/sdl2_input_translate_test
  ```
  预期：7 个测试全部 PASS

- [ ] **步骤 5：RED 反向探针 — 验证 KeyOutOfAsciiReturnsZero 测试有效性**
  临时修改 `TranslateSdlKey` 让 SDLK_F1 也返回 `key`，重跑测试确认 `KeyOutOfAsciiReturnsZero` FAIL；恢复后 PASS。

- [ ] **步骤 6：提交**
  ```bash
  git add veloxa/platform/sdl2/sdl2_input_translate.cc tests/platform/sdl2/sdl2_input_translate_test.cc
  git commit -m "feat(sdl2): translate keyboard events with modifier bits [TASK-20260425-01 P1]"
  ```

---

## Phase 2 — `Sdl2WindowSurface` 模块

### 任务 2.1：Surface 类骨架 + 构造/析构 RAII

[TDD]

**文件：**
- 创建：`veloxa/platform/sdl2/sdl2_window_surface.h`
- 创建：`veloxa/platform/sdl2/sdl2_window_surface.cc`
- 创建：`tests/platform/sdl2/sdl2_window_surface_test.cc`
- 修改：`veloxa/platform/sdl2/CMakeLists.txt`（启用 .cc）
- 修改：`tests/platform/sdl2/CMakeLists.txt`（加 surface_test）

- [ ] **步骤 1：创建头文件骨架**
  按 spec §5.2.3 的 `Sdl2WindowSurface` 类签名（`Sdl2WindowOptions` struct 为公开内部 struct，C API 中独立 `VxWindowOptions` 在 P4 引入）。

- [ ] **步骤 2：写失败测试 — 构造一个 dummy surface**
  ```cpp
  // sdl2_window_surface_test.cc
  #include "veloxa/platform/sdl2/sdl2_window_surface.h"
  #include <SDL2/SDL.h>
  #include <gtest/gtest.h>

  TEST(Sdl2WindowSurfaceTest, ConstructDestroyDummyDriver) {
    ASSERT_EQ(SDL_Init(SDL_INIT_VIDEO), 0) << SDL_GetError();
    {
      vx::platform::Sdl2WindowOptions opts{};
      opts.width = 100;
      opts.height = 50;
      opts.title = "test";
      vx::platform::Sdl2WindowSurface s(opts);
      EXPECT_EQ(s.width(), 100u);
      EXPECT_EQ(s.height(), 50u);
      EXPECT_EQ(s.stride(), 100u * sizeof(uint32_t));
    }
    SDL_Quit();
  }
  ```

- [ ] **步骤 3：运行测试验证失败**（链接失败 / 实现 missing）
  ```bash
  cmake --build build-sdl2 --target sdl2_window_surface_test 2>&1 | tail -5
  ```
  预期：链接错误或 `Sdl2WindowSurface` undefined

- [ ] **步骤 4：实现构造/析构 + 三个 getter**
  ```cpp
  // sdl2_window_surface.cc
  #include "veloxa/platform/sdl2/sdl2_window_surface.h"
  #include <cstdlib>
  #include <cstring>

  namespace vx::platform {

  Sdl2WindowSurface::Sdl2WindowSurface(const Sdl2WindowOptions& opts)
      : window_(nullptr), renderer_(nullptr), texture_(nullptr),
        pixels_(nullptr), width_(opts.width), height_(opts.height),
        locked_(false) {
    Uint32 flags = SDL_WINDOW_SHOWN;
    if (opts.resizable) flags |= SDL_WINDOW_RESIZABLE;
    if (opts.high_dpi)  flags |= SDL_WINDOW_ALLOW_HIGHDPI;

    window_ = SDL_CreateWindow(opts.title ? opts.title : "Veloxa",
                               SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                               static_cast<int>(opts.width),
                               static_cast<int>(opts.height), flags);
    if (!window_) return;

    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer_) return;

    texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888,
                                 SDL_TEXTUREACCESS_STREAMING,
                                 static_cast<int>(opts.width),
                                 static_cast<int>(opts.height));
    if (!texture_) return;

    pixels_ = static_cast<uint32_t*>(
        std::calloc(opts.width * opts.height, sizeof(uint32_t)));
  }

  Sdl2WindowSurface::~Sdl2WindowSurface() {
    if (pixels_) std::free(pixels_);
    if (texture_) SDL_DestroyTexture(texture_);
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_) SDL_DestroyWindow(window_);
  }

  vx::u32 Sdl2WindowSurface::width() const { return width_; }
  vx::u32 Sdl2WindowSurface::height() const { return height_; }
  vx::u32 Sdl2WindowSurface::stride() const { return width_ * sizeof(uint32_t); }
  // 占位（任务 2.2 实现）：
  vx::u32* Sdl2WindowSurface::Lock() { return pixels_; }
  void Sdl2WindowSurface::Unlock() {}
  void Sdl2WindowSurface::Resize(vx::u32, vx::u32) {}
  vx::Status Sdl2WindowSurface::SavePPM(const char*) const { return vx::Status::Error("unsupported"); }
  void Sdl2WindowSurface::Present() {}

  }  // namespace vx::platform
  ```
  CMakeLists 加入 `sdl2_window_surface.cc` 编译。

- [ ] **步骤 5：运行测试验证通过**
  ```bash
  SDL_VIDEODRIVER=dummy ./build-sdl2/tests/platform/sdl2/sdl2_window_surface_test
  ```
  预期：PASS

- [ ] **步骤 6：提交**
  ```bash
  git add veloxa/platform/sdl2/sdl2_window_surface.{h,cc} \
          veloxa/platform/sdl2/CMakeLists.txt \
          tests/platform/sdl2/sdl2_window_surface_test.cc \
          tests/platform/sdl2/CMakeLists.txt
  git commit -m "feat(sdl2): Sdl2WindowSurface ctor/dtor RAII + getters [TASK-20260425-01 P2]"
  ```

---

### 任务 2.2：`Lock` / `Unlock` / `Present` 完整实现

[TDD]

**文件：**
- 修改：`veloxa/platform/sdl2/sdl2_window_surface.cc`
- 修改：`tests/platform/sdl2/sdl2_window_surface_test.cc`

- [ ] **步骤 1：写失败测试 — Lock/Unlock/Present 单帧周期**
  ```cpp
  TEST(Sdl2WindowSurfaceTest, LockUnlockPresentSingleFrame) {
    ASSERT_EQ(SDL_Init(SDL_INIT_VIDEO), 0);
    vx::platform::Sdl2WindowOptions opts{ 64, 48, "frame", false, false };
    vx::platform::Sdl2WindowSurface s(opts);
    auto* px = s.Lock();
    ASSERT_NE(px, nullptr);
    for (uint32_t i = 0; i < 64u * 48u; ++i) px[i] = 0xFF00FF00;  // 绿
    s.Unlock();
    s.Present();   // dummy driver 下应不崩
    EXPECT_EQ(s.stride(), 64u * 4u);
    SDL_Quit();
  }
  ```

- [ ] **步骤 2：运行测试验证失败**
  当前 Lock/Unlock/Present 是占位，可能不会立即 FAIL，但加一个反向探针：
  临时让 `Lock` 返回 `nullptr`，确认 `ASSERT_NE(px, nullptr)` FAIL。

- [ ] **步骤 3：实现 Lock/Unlock/Present**
  ```cpp
  vx::u32* Sdl2WindowSurface::Lock() {
    if (locked_) return nullptr;
    locked_ = true;
    return pixels_;
  }

  void Sdl2WindowSurface::Unlock() {
    locked_ = false;
  }

  void Sdl2WindowSurface::Present() {
    if (!texture_ || !renderer_ || !pixels_) return;
    SDL_UpdateTexture(texture_, nullptr, pixels_,
                      static_cast<int>(stride()));
    SDL_RenderClear(renderer_);
    SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
    SDL_RenderPresent(renderer_);
  }
  ```

- [ ] **步骤 4：运行测试验证通过**
  ```bash
  SDL_VIDEODRIVER=dummy ./build-sdl2/tests/platform/sdl2/sdl2_window_surface_test
  ```
  预期：2 个测试 PASS

- [ ] **步骤 5：补充边界测试**
  ```cpp
  TEST(Sdl2WindowSurfaceTest, DoubleLockReturnsNull) {
    ASSERT_EQ(SDL_Init(SDL_INIT_VIDEO), 0);
    vx::platform::Sdl2WindowOptions opts{ 32, 32, "lock2", false, false };
    vx::platform::Sdl2WindowSurface s(opts);
    auto* p1 = s.Lock();
    EXPECT_NE(p1, nullptr);
    auto* p2 = s.Lock();
    EXPECT_EQ(p2, nullptr);  // double lock 拒绝
    s.Unlock();
    SDL_Quit();
  }
  ```

- [ ] **步骤 6：运行 + 提交**
  ```bash
  cmake --build build-sdl2 --target sdl2_window_surface_test
  SDL_VIDEODRIVER=dummy ./build-sdl2/tests/platform/sdl2/sdl2_window_surface_test
  git add veloxa/platform/sdl2/sdl2_window_surface.cc tests/platform/sdl2/sdl2_window_surface_test.cc
  git commit -m "feat(sdl2): Sdl2WindowSurface Lock/Unlock/Present [TASK-20260425-01 P2]"
  ```

---

## Phase 3 — `Sdl2EventLoop` 模块

### 任务 3.1：基础 EventLoop 接口（Run/Quit/PostTask/SetTimer）

[TDD]

**文件：**
- 创建：`veloxa/platform/sdl2/sdl2_event_loop.h`
- 创建：`veloxa/platform/sdl2/sdl2_event_loop.cc`
- 创建：`tests/platform/sdl2/sdl2_event_loop_test.cc`
- 修改：`veloxa/platform/sdl2/CMakeLists.txt`
- 修改：`tests/platform/sdl2/CMakeLists.txt`

- [ ] **步骤 1：头文件 + 基础测试（PostTask 行为）**
  ```cpp
  TEST(Sdl2EventLoopTest, PostTaskExecutedInPollOnce) {
    ASSERT_EQ(SDL_Init(SDL_INIT_EVENTS), 0);
    vx::platform::Sdl2EventLoop loop;
    int count = 0;
    loop.PostTask([&]{ count++; });
    loop.PollOnce();
    EXPECT_EQ(count, 1);
  }
  ```

- [ ] **步骤 2：失败 → 实现（直接复用 HeadlessEventLoop 的 task/timer 容器逻辑）**
  实现 Run/Quit/PostTask/SetTimer/CancelTimer/PollOnce，PollOnce 顺序：`ProcessTasks() → ProcessTimers() → PumpInputEvents()`（PumpInputEvents 在 3.2 加入）。

- [ ] **步骤 3：测试 — SetTimer 触发**
  ```cpp
  TEST(Sdl2EventLoopTest, SetTimerFiresAfterInterval) {
    vx::platform::Sdl2EventLoop loop;
    int fired = 0;
    loop.SetTimer(10, [&]{ fired++; }, /*repeat=*/false);
    SDL_Delay(20);
    loop.PollOnce();
    EXPECT_EQ(fired, 1);
  }
  ```

- [ ] **步骤 4：运行测试 + 提交**
  ```bash
  cmake --build build-sdl2 --target sdl2_event_loop_test
  SDL_VIDEODRIVER=dummy ./build-sdl2/tests/platform/sdl2/sdl2_event_loop_test
  git add veloxa/platform/sdl2/sdl2_event_loop.{h,cc} \
          tests/platform/sdl2/sdl2_event_loop_test.cc \
          veloxa/platform/sdl2/CMakeLists.txt tests/platform/sdl2/CMakeLists.txt
  git commit -m "feat(sdl2): Sdl2EventLoop base interface (task/timer) [TASK-20260425-01 P3]"
  ```

---

### 任务 3.2：`PumpInputEvents` + `SetInputCallback`

[TDD]

**文件：**
- 修改：`veloxa/platform/sdl2/sdl2_event_loop.{h,cc}`
- 修改：`tests/platform/sdl2/sdl2_event_loop_test.cc`

- [ ] **步骤 1：写失败测试 — 注入 SDL_PushEvent，验证 callback 命中**
  ```cpp
  TEST(Sdl2EventLoopTest, PumpInputEventsCallbackInvoked) {
    ASSERT_EQ(SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO), 0);
    vx::platform::Sdl2EventLoop loop;
    int hit = 0;
    loop.SetInputCallback([&](const VxInputEvent& ev) {
      if (ev.type == VX_EVENT_POINTER_MOVE) hit++;
    });
    SDL_Event ev{};
    ev.type = SDL_MOUSEMOTION;
    ev.motion.x = 50;
    ev.motion.y = 60;
    SDL_PushEvent(&ev);
    loop.PumpInputEvents();
    EXPECT_EQ(hit, 1);
    SDL_Quit();
  }

  TEST(Sdl2EventLoopTest, QuitEventTriggersLoopQuit) {
    vx::platform::Sdl2EventLoop loop;
    SDL_Event ev{};
    ev.type = SDL_QUIT;
    SDL_PushEvent(&ev);
    loop.PumpInputEvents();
    EXPECT_FALSE(loop.is_running());
  }
  ```

- [ ] **步骤 2：失败 → 实现 PumpInputEvents**
  ```cpp
  void Sdl2EventLoop::PumpInputEvents() {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_QUIT) {
        Quit();
        continue;
      }
      if (ev.type == SDL_WINDOWEVENT &&
          ev.window.event == SDL_WINDOWEVENT_CLOSE) {
        Quit();
        continue;
      }
      VxInputEvent vxe;
      if (TranslateSdlEvent(ev, vxe) && input_callback_) {
        input_callback_(vxe);
      }
    }
  }

  void Sdl2EventLoop::SetInputCallback(InputCallback cb) {
    input_callback_ = std::move(cb);
  }
  ```
  并在 `PollOnce` 末尾调 `PumpInputEvents()`。

- [ ] **步骤 3：运行测试 + RED 反向探针**
  临时把 `if (... && input_callback_)` 改成 `if (false)`，确认 `PumpInputEventsCallbackInvoked` FAIL；恢复后 PASS。

- [ ] **步骤 4：提交**
  ```bash
  cmake --build build-sdl2 --target sdl2_event_loop_test
  SDL_VIDEODRIVER=dummy ./build-sdl2/tests/platform/sdl2/sdl2_event_loop_test
  git add veloxa/platform/sdl2/sdl2_event_loop.{h,cc} tests/platform/sdl2/sdl2_event_loop_test.cc
  git commit -m "feat(sdl2): PumpInputEvents + SetInputCallback for view bridging [TASK-20260425-01 P3]"
  ```

---

## Phase 4 — C API 扩展

### 任务 4.1：`VxWindowOptions` + `vx_event_loop_create_sdl2` + `vx_surface_create_window`

[TDD]

**文件：**
- 修改：`veloxa/api/veloxa_api.h`
- 修改：`veloxa/api/veloxa_api.cc`
- 修改：`veloxa/api/CMakeLists.txt`
- 修改：`tests/api/veloxa_api_test.cc`

- [ ] **步骤 1：声明新 C API**
  在 `veloxa_api.h` 适当位置追加：
  ```c
  typedef struct {
    uint32_t width;
    uint32_t height;
    const char* title;
    uint32_t flags;  // bit 0=resizable, bit 1=high_dpi (reserved)
  } VxWindowOptions;

  VxEventLoop* vx_event_loop_create_sdl2(void);
  VxSurface*   vx_surface_create_window(const VxWindowOptions* opts);
  VxResult     vx_event_loop_pump_input(VxEventLoop* loop, VxView* view);
  ```

- [ ] **步骤 2：CMakeLists 条件链接 vx_platform_sdl2**
  ```cmake
  # veloxa/api/CMakeLists.txt
  target_link_libraries(vx_api
    PRIVATE vx_core vx_platform_headless
    $<$<BOOL:${VX_PLATFORM_SDL2}>:vx_platform_sdl2>
  )
  ```

- [ ] **步骤 3：测试 — 创建 SDL2 loop+window 完成对称生命周期**
  ```cpp
  // tests/api/veloxa_api_test.cc — 仅 SDL2 ON 时编译
  #ifdef VX_PLATFORM_SDL2
  TEST(VeloxaApiSdl2Test, CreateLoopWindowDestroyOk) {
    setenv("SDL_VIDEODRIVER", "dummy", 1);
    VxEventLoop* loop = vx_event_loop_create_sdl2();
    ASSERT_NE(loop, nullptr);
    VxWindowOptions opts{ 100, 60, "apitest", 0 };
    VxSurface* surf = vx_surface_create_window(&opts);
    ASSERT_NE(surf, nullptr);
    vx_surface_destroy(surf);
    vx_event_loop_destroy(loop);
  }

  TEST(VeloxaApiSdl2Test, CreateWindowNullOptsReturnsNull) {
    EXPECT_EQ(vx_surface_create_window(nullptr), nullptr);
  }
  #endif
  ```
  并在测试 CMakeLists 加 `target_compile_definitions(... PRIVATE $<$<BOOL:${VX_PLATFORM_SDL2}>:VX_PLATFORM_SDL2>)`.

- [ ] **步骤 4：失败 → 实现 C API**
  ```cpp
  // veloxa_api.cc — 顶部条件 include
  #ifdef VX_PLATFORM_SDL2
  #include "veloxa/platform/sdl2/sdl2_event_loop.h"
  #include "veloxa/platform/sdl2/sdl2_window_surface.h"
  #endif

  // 在 extern "C" {} 内：
  VxEventLoop* vx_event_loop_create_sdl2(void) {
  #ifdef VX_PLATFORM_SDL2
    auto* loop = new vx::platform::Sdl2EventLoop();
    return reinterpret_cast<VxEventLoop*>(loop);
  #else
    return nullptr;  // 未启用后端时返回 NULL
  #endif
  }

  VxSurface* vx_surface_create_window(const VxWindowOptions* opts) {
  #ifdef VX_PLATFORM_SDL2
    if (!opts) return nullptr;
    vx::platform::Sdl2WindowOptions copts{};
    copts.width  = opts->width;
    copts.height = opts->height;
    copts.title  = opts->title;
    copts.resizable = (opts->flags & 0x1) != 0;
    copts.high_dpi  = (opts->flags & 0x2) != 0;
    auto* surf = new vx::platform::Sdl2WindowSurface(copts);
    return reinterpret_cast<VxSurface*>(surf);
  #else
    (void)opts;
    return nullptr;
  #endif
  }
  ```

- [ ] **步骤 5：运行测试 + 提交**
  ```bash
  cmake --build build-sdl2 --target veloxa_api_test
  SDL_VIDEODRIVER=dummy ./build-sdl2/tests/veloxa_api_test
  git add veloxa/api/veloxa_api.{h,cc} veloxa/api/CMakeLists.txt tests/api/veloxa_api_test.cc
  git commit -m "feat(api): VxWindowOptions + sdl2 factory C API [TASK-20260425-01 P4]"
  ```

---

### 任务 4.2：`vx_event_loop_pump_input` helper

[TDD]

**文件：**
- 修改：`veloxa/api/veloxa_api.cc`
- 修改：`tests/api/veloxa_api_test.cc`

- [ ] **步骤 1：写测试**
  ```cpp
  #ifdef VX_PLATFORM_SDL2
  TEST(VeloxaApiSdl2Test, PumpInputForwardsToView) {
    setenv("SDL_VIDEODRIVER", "dummy", 1);
    VxEventLoop* loop = vx_event_loop_create_sdl2();
    VxWindowOptions wopts{ 64, 48, "pump", 0 };
    VxSurface* surf = vx_surface_create_window(&wopts);
    VxViewConfig vc{};
    vc.event_loop = loop;
    vc.surface = surf;
    vc.target_fps = 60;
    VxView* view = vx_view_create(&vc);
    ASSERT_NE(view, nullptr);

    // Inject SDL mouse motion via SDL_PushEvent
    SDL_Event ev{};
    ev.type = SDL_MOUSEMOTION;
    ev.motion.x = 10; ev.motion.y = 20;
    SDL_PushEvent(&ev);

    EXPECT_EQ(vx_event_loop_pump_input(loop, view), VX_OK);
    // 行为验证：测试不直接观察事件分发结果；只要不崩 + 返回 OK 即通过
    // 反向探针：临时让实现返回 VX_ERROR_NULL_PARAM 应让此 EXPECT 失败

    vx_view_destroy(view);
    vx_surface_destroy(surf);
    vx_event_loop_destroy(loop);
  }
  #endif
  ```

- [ ] **步骤 2：失败 → 实现 helper**
  ```cpp
  VxResult vx_event_loop_pump_input(VxEventLoop* loop, VxView* view) {
  #ifdef VX_PLATFORM_SDL2
    if (!loop || !view) return VX_ERROR_NULL_PARAM;
    auto* base = reinterpret_cast<vx::platform::EventLoop*>(loop);
    auto* sdl_loop = dynamic_cast<vx::platform::Sdl2EventLoop*>(base);
    if (!sdl_loop) return VX_ERROR_INVALID_STATE;
    sdl_loop->SetInputCallback([view](const VxInputEvent& ev) {
      vx_view_inject_input(view, &ev);
    });
    sdl_loop->PumpInputEvents();
    return VX_OK;
  #else
    (void)loop; (void)view;
    return VX_ERROR_INVALID_STATE;
  #endif
  }
  ```
  > 注意：`dynamic_cast` 需要 `EventLoop` 至少一个 virtual 方法 — 已有 `virtual ~ + Run/Quit/...` 全 virtual，OK。

- [ ] **步骤 3：运行测试 + 提交**
  ```bash
  cmake --build build-sdl2 --target veloxa_api_test
  SDL_VIDEODRIVER=dummy ./build-sdl2/tests/veloxa_api_test
  git add veloxa/api/veloxa_api.cc tests/api/veloxa_api_test.cc
  git commit -m "feat(api): vx_event_loop_pump_input helper bridging SDL→view [TASK-20260425-01 P4]"
  ```

---

## Phase 5 — Example + Smoke Test

### 任务 5.1：`examples/hello_sdl2.cc`

**文件：**
- 创建：`examples/hello_sdl2.cc`
- 修改：`examples/CMakeLists.txt`

- [ ] **步骤 1：创建 example**
  ```c
  /*
   * Veloxa — Hello SDL2 Example
   * Renders the hello page to a real on-screen window via SDL2 backend.
   * Build:  cmake --build build-sdl2 --target hello_sdl2
   * Run:    ./build-sdl2/examples/hello_sdl2
   */
  #include "veloxa/api/veloxa_api.h"
  #include <stdio.h>
  #include <string.h>

  static const char kHTML[] =
      "<div id='page'>"
      "  <div id='header'>Veloxa SDL2</div>"
      "  <div id='content'>"
      "    <div id='box-red'></div>"
      "    <div id='box-green'></div>"
      "    <div id='box-blue'></div>"
      "  </div>"
      "</div>";

  static const char kCSS[] =
      "#page { width: 400px; background-color: white; }"
      "#header { height: 40px; background-color: #2C3E50; color: white; }"
      "#content { display: flex; gap: 20px; padding: 30px;"
      "           background-color: #ECF0F1; }"
      "#box-red { width: 80px; height: 80px; background-color: #E74C3C; }"
      "#box-red:hover { background-color: #FF6B6B; }"
      "#box-green { width: 80px; height: 80px; background-color: #2ECC71; }"
      "#box-blue { width: 80px; height: 80px; background-color: #3498DB; }";

  int main(void) {
    printf("Veloxa %s — Hello SDL2 Example\n", vx_version());
    VxEventLoop* loop = vx_event_loop_create_sdl2();
    if (!loop) { fprintf(stderr, "ERROR: SDL2 backend init failed\n"); return 1; }
    VxWindowOptions wopts = { 400, 300, "Veloxa Hello", 0 };
    VxSurface* surface = vx_surface_create_window(&wopts);
    if (!surface) { fprintf(stderr, "ERROR: window create failed\n"); return 1; }

    VxViewConfig config;
    memset(&config, 0, sizeof(config));
    config.event_loop = loop;
    config.surface = surface;
    config.target_fps = 60;
    config.background_color = 0xFFFFFFFF;

    VxView* view = vx_view_create(&config);
    vx_view_load_html(view, kHTML, (uint32_t)strlen(kHTML));
    vx_view_load_css (view, kCSS,  (uint32_t)strlen(kCSS));

    // 主循环
    for (int frame = 0; frame < 60 * 30; ++frame) {  // 30 秒上限（防卡死）
      vx_event_loop_pump_input(loop, view);
      vx_view_update(view);
      // SDL_Delay 已由 EventLoop 内部处理；此处空帧间隔
    }

    vx_view_destroy(view);
    vx_surface_destroy(surface);
    vx_event_loop_destroy(loop);
    printf("Done.\n");
    return 0;
  }
  ```

- [ ] **步骤 2：CMakeLists 条件添加 hello_sdl2 target**
  ```cmake
  if(VX_PLATFORM_SDL2)
    add_executable(hello_sdl2 hello_sdl2.cc)
    target_link_libraries(hello_sdl2 PRIVATE vx_api)
  endif()
  ```

- [ ] **步骤 3：smoke 编译**
  ```bash
  cmake --build build-sdl2 --target hello_sdl2 2>&1 | tail -5
  ```
  预期：`Built target hello_sdl2`

- [ ] **步骤 4：dummy driver smoke 跑**
  ```bash
  timeout 3 env SDL_VIDEODRIVER=dummy ./build-sdl2/examples/hello_sdl2 || true
  echo "exit=$?"
  ```
  预期：30 秒上限前因 dummy driver 不阻塞 → 1-2 秒内自然跑完，exit 0；或被 timeout 杀（仍可接受）

- [ ] **步骤 5：提交**
  ```bash
  git add examples/hello_sdl2.cc examples/CMakeLists.txt
  git commit -m "feat(example): hello_sdl2 demonstrates window backend [TASK-20260425-01 P5]"
  ```

---

### 任务 5.2：ctest smoke（dummy driver 跑 hello_sdl2 一帧不崩）

**文件：**
- 修改：`examples/CMakeLists.txt`（加 add_test）

- [ ] **步骤 1：测试钩子**
  ```cmake
  if(VX_PLATFORM_SDL2)
    add_executable(hello_sdl2 hello_sdl2.cc)
    target_link_libraries(hello_sdl2 PRIVATE vx_api)

    # smoke：dummy driver 下跑 5 秒上限确保不崩
    add_test(NAME hello_sdl2_smoke
             COMMAND ${CMAKE_COMMAND} -E env "SDL_VIDEODRIVER=dummy"
                     "VX_HELLO_SDL2_FRAME_LIMIT=10"
                     $<TARGET_FILE:hello_sdl2>)
    set_tests_properties(hello_sdl2_smoke PROPERTIES TIMEOUT 10)
  endif()
  ```
  对应 `hello_sdl2.cc` 末尾的 frame loop 改读 env：
  ```c
  const char* fl = getenv("VX_HELLO_SDL2_FRAME_LIMIT");
  int frame_limit = fl ? atoi(fl) : 60 * 30;
  for (int frame = 0; frame < frame_limit; ++frame) { ... }
  ```

- [ ] **步骤 2：跑 smoke**
  ```bash
  cd build-sdl2 && ctest -V -R hello_sdl2_smoke
  ```
  预期：PASS（10 帧 + 自然退出）

- [ ] **步骤 3：完整 ctest 防回归**
  ```bash
  ctest --output-on-failure -j 4
  ```
  预期：所有测试 PASS（含 smoke）

- [ ] **步骤 4：提交**
  ```bash
  git add examples/hello_sdl2.cc examples/CMakeLists.txt
  git commit -m "test(example): hello_sdl2 smoke under dummy driver [TASK-20260425-01 P5]"
  ```

---

## Phase 6 — 验证收尾 + 文档

### 任务 6.1：WSLg 手工验证 A1/A2/A3

**注意：** 此 Phase 需在用户的 WSLg 终端手工执行，sandbox 内无法替代。

- [ ] **步骤 1：用户在外部终端跑**
  ```bash
  cmake --build build-sdl2 --target hello_sdl2 -j
  ./build-sdl2/examples/hello_sdl2
  ```
  预期 A1：400×300 窗口出现，看到 hello 例子的 header（深蓝）+ 灰色 content + 红绿蓝三色块

- [ ] **步骤 2：A2 鼠标 hover 验证**
  在窗口内移动鼠标到红色块上方，预期：颜色由 `#E74C3C` 变为 `#FF6B6B`（CSS `:hover` 已配置）

- [ ] **步骤 3：A3 关闭验证**
  点击窗口 X 按钮或按 SDL_QUIT 等价输入；预期：窗口干净关闭，进程 exit 0

- [ ] **步骤 4：用户回报结果**
  把上述 3 项结果（PASS/FAIL + 截图或描述）写入 `progress.md`

---

### 任务 6.2：Release `-O3 -Werror` 通路验证

- [ ] **步骤 1：fresh release configure**
  ```bash
  rm -rf build-release-sdl2
  cmake -B build-release-sdl2 -DCMAKE_BUILD_TYPE=Release -DVX_PLATFORM_SDL2=ON
  cmake --build build-release-sdl2 -j 2>&1 | tee /tmp/release.log | tail -20
  ```
  预期：0 errors, 0 warnings

- [ ] **步骤 2：grep 验证零 warning**
  ```bash
  grep -E 'warning:|error:' /tmp/release.log | wc -l
  ```
  预期：`0`

- [ ] **步骤 3：完整 ctest Release**
  ```bash
  cd build-release-sdl2 && ctest --output-on-failure -j 4
  ```
  预期：100% PASS

- [ ] **步骤 4：提交（如有 fix）或仅记录到 progress**
  ```bash
  echo "Release -Werror clean: ✅" >> memory-bank/progress.md
  ```

---

### 任务 6.3：文档更新（techContext + productContext）

- [ ] **步骤 1：`techContext.md` 追加「SDL2 平台后端」段**
  在「第三方依赖」表追加一行 SDL2 + 版本 + 用途；
  在「FetchContent 与代理」之前新增段落「平台后端」：
  ```markdown
  ## 平台后端

  | 后端 | 默认开关 | 依赖 | 用途 |
  |---|:-:|---|---|
  | headless | ON | — | PPM 输出 / CI 测试 / 嵌入式集成 |
  | sdl2 | OFF（`-DVX_PLATFORM_SDL2=ON`） | libsdl2-dev ≥ 2.0.20 | WSLg / Linux Desktop 开窗 + 输入；调试 UI 主路径 |

  ### SDL2 启用
  ```bash
  sudo apt install -y libsdl2-dev
  cmake -B build-sdl2 -DVX_PLATFORM_SDL2=ON
  cmake --build build-sdl2 --target hello_sdl2
  ./build-sdl2/examples/hello_sdl2
  ```

  ### WSLg 注意
  - SDL_VIDEODRIVER 默认 `wayland`（WSLg 兼容）；不可用时 fallback `x11`
  - dummy driver 下 ctest smoke：`SDL_VIDEODRIVER=dummy`
  ```

- [ ] **步骤 2：`productContext.md` 「已实现的核心功能」表追加 SDL2 行**
  ```markdown
  | SDL2 窗口后端 | ✅ | TASK-20260425-01；可见窗口 + 输入桥接；实时调试 UI 前置 |
  ```

- [ ] **步骤 3：提交**
  ```bash
  git add memory-bank/techContext.md memory-bank/productContext.md memory-bank/progress.md
  git commit -m "docs(memory-bank): document SDL2 platform backend [TASK-20260425-01 P6]"
  ```

---

## 收尾（自动由 /reflect 命令处理）

- 所有 Phase 完成后调 `/reflect`：
  - 撰写 `memory-bank/reflection/reflection-TASK-20260425-01.md`
  - 评估 plan × 0.6 第 8 数据点（首个新模块类任务）
  - 沉淀模式：双轨 CMake 兜底 / 输入 callback 单向解耦 / Surface::Present() 加法不破现存
  - 评估 R1-R6 风险触发情况
- 然后 `/archive` 归档 + `--no-ff` 合并到 main

---

## 全局约束清单（每 Phase 通用）

- 每个 Phase 完成后跑 `cd build && ctest -j 4` + `cd build-sdl2 && ctest -j 4`，确认双 build 全绿
- 每 commit subject 必须含 `[TASK-20260425-01 P<N>]` tag（git-workflow.mdc 规范）
- 严禁 wip subject 含 `BLOCKED on`/`WAITING for`/`PENDING dep`（writing-plans.mdc P1 规则）
- 任何 Mixed TDD D3 类回归测试加 RED 反向探针（TASK-24-03 第 3 次成熟，TASK-24-04 P3 已升级为标配）
- Release `-O3 -Werror` 在 P6 必跑；任何 GCC IPA / array-bounds 误报立刻列入 fix 子任务（TASK-04 / TASK-19-07 反复模式）

---

**计划完成并保存。准备执行？**
- 路径：`docs/plans/2026-04-25-sdl2-window-backend.md`
- 设计规格：`docs/specs/2026-04-25-sdl2-window-backend-design.md`
- 进入 `/build` 前请用户审查 spec §11 估时（180 min × 0.6 实测预期）+ 范围（含技术债清理）
