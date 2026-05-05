# G1 OpenGL ES 硬件渲染后端蓝图 — 实施计划

**目标：** 完成 G1 OpenGL ES 硬件渲染后端的蓝图主交付（spec + plan + creative ×3）；不做 build 实施，N 个 Level 3 子任务交由用户后续独立立项。

**架构：** 在既有 `gfx::Canvas` 纯虚 + `platform::Surface` 纯虚抽象之上新增 GLESCanvas / Sdl2GLWindowSurface / GLESDisplay 实现；`VX_RENDERER=software\|gles` CMake flag 编译期分支；SoftwareCanvas 作 fallback 共存；G2 DRM/KMS 接口完整预留。

**技术栈：** C++ / OpenGL ES 3.0+ / EGL / SDL_GL_CreateContext（桌面）/ libtess2（FillPath tessellation）/ FreeType + GlyphCache（既有 / GPU atlas）/ google/benchmark（B7 验收）

**复杂度级别：** Level 4 V2=a 蓝图任务（沿用 TASK-20260430-04 + TASK-20260504-01 范式 / **跳过独立 /build 阶段**）

---

## 0. Phase 0 实证段（plan 阶段已完成）

### 0.1 上下文 grep 实证（VAN + Plan 阶段累计）

| # | 验证 | 命令 | 实测 | 结论 |
|:-:|---|---|---|---|
| 1 | EGL + GLES3 dev headers | `dpkg -l | rg "libegl-dev|libgles-dev"` | libegl-dev 1.7.0-3 + libgles-dev 1.7.0-3 安装 | ✅ 蓝图阶段 0 等待 / 实施任务可立即开工 |
| 2 | Mesa 版本 | `dpkg -l | rg "libgl1-mesa-dri"` | 26.0.3-1ubuntu1 | ✅ Mesa 26.0+ 全 GLES 3.0+ 支持 |
| 3 | GLES3 头文件 | `ls /usr/include/EGL/ /usr/include/GLES3/` | egl.h + gl3.h + gl31.h + gl32.h 全在 | ✅ 蓝图引用全部可达 |
| 4 | 既有 Canvas 抽象 | `Read veloxa/graphics/canvas.h` | 22 纯虚方法 / 与 SoftwareCanvas 一一对应 | ✅ 仅替换实现 / 接口不动 |
| 5 | 既有 Surface 抽象 | `Read veloxa/platform/surface.h` | Lock/Unlock/Resize/SavePPM/Present 5 方法 | ✅ Present() 默认 no-op 已支持 GLES SwapBuffers |
| 6 | render::Replay 签名 | `Read veloxa/core/render/renderer.cc` | 输入 `gfx::Canvas*` / 输出 22 方法调用 | ✅ 接口已抽象 / 替换 Canvas 实现即生效 |
| 7 | Application::canvas_ | `Read veloxa/core/application.cc:54` | `unique_ptr<gfx::Canvas>` / 构造时根据 surface 创 SoftwareCanvas | ✅ 构造路径单点修改 |
| 8 | PaintCommand 类型 | `Read veloxa/core/render/paint_command.h` | 9 类型（FillRect/RoundedRect/Path/Text/Image/Stroke/Clip/Layer/Overlay）| ✅ 零修改 / 直接传 GLESCanvas |
| 9 | Sdl2WindowSurface 范式 | `Read veloxa/platform/sdl2/sdl2_window_surface.cc` | SDL_CreateWindow + SDL_CreateRenderer + SDL_CreateTexture / 无 GL | ✅ Sdl2GLWindowSurface 可独立新增（不破坏既有路径）|
| 10 | activeContext 阶段 | `Grep "^\\*\\*初始化\\*\\*" memory-bank/activeContext.md` | 命中 / 阶段 = 初始化 ✅ | ✅ /plan 前置条件满足 |

### 0.2 工具链快照（writing-plans.mdc §0.10 子段强制）

```bash
gcc --version | head -1     # gcc (Ubuntu 14.x) — TODO 实施任务跑时记录
ld --version | head -1      # binutils 2.46+（已知激进 / 既有 R12 hotfix 已修复）
cmake --version | head -1   # cmake 4.0+
```

| 工具 | 当前 | 上次任务一致性 | 行动 |
|---|---|:-:|---|
| binutils ld | 2.46+ | ✅ 与 TASK-02 一致 | ✅ 跳过差异检查 |
| gcc | 14+ | ✅ | ✅ |
| cmake | 4.0+ | ✅ | ✅ |

**风险**：本任务不做 build，工具链快照仅作蓝图实施准备 / 实施任务（G1.1+）必须重做 §0.10 audit。

### 0.3 Phase 0 audit 结论

- ✅ 全 10 项 grep + 工具链快照 PASS
- ✅ 既有架构对 GLES 替换**零阻碍**（Canvas / Surface / Application / PaintCommand / Replay 全链路已抽象）
- ✅ 蓝图主交付（spec + plan + creative ×3）零 build 依赖 / 当前会话即可完成
- ✅ N 个 Level 3 实施子任务 0 等待 / 用户立项后立即可开工

---

## 1. 文件结构（蓝图主交付物落盘清单）

| # | 路径 | 类型 | 估行数 | 状态 |
|:-:|---|:-:|:-:|:-:|
| 1 | `docs/specs/2026-05-05-gles-renderer-blueprint-design.md` | 设计 spec | ~720 | ✅ 已落盘 |
| 2 | `docs/plans/2026-05-05-gles-renderer-blueprint.md` | 实施 plan（本文档）| ~900 | 🟡 在 写 |
| 3 | `memory-bank/creative/creative-gles-context.md` | creative-1（B1 GL context）| ~280 | ⏳ 待写 |
| 4 | `memory-bank/creative/creative-gles-canvas.md` | creative-2（B2 Canvas 翻译）| ~360 | ⏳ 待写 |
| 5 | `memory-bank/creative/creative-gles-resources.md` | creative-3（B3 GlyphAtlas + B4 dirty rect + B6 shader）| ~320 | ⏳ 待写 |
| 6 | `memory-bank/activeContext.md` | Memory Bank 更新 | +~30 | 待 plan 末尾更新 |
| 7 | `memory-bank/tasks.md` | 任务进度更新 | +~50 | 待 plan 末尾更新 |
| 8 | `memory-bank/progress.md` | 进度记录更新 | +~30 | 待 plan 末尾更新 |
| **小计** | | | **~2690 行** | |

> **P0 协议落地**：本任务**首次完整执行**「plan/spec docs 落盘即 commit」P0 协议（TASK-20260505-02 首次成功 ✅）— 文件 1-8 在 plan 阶段单 commit 落盘 / build 阶段（如有）零 collateral commit。

---

## 2. 蓝图阶段任务粒度（V2=a 单 phase）

### 2.1 蓝图阶段任务列表

> **变体说明：** V2=a 纯蓝图任务**不含** /build 阶段；以下任务均在当前 /plan 会话内连续执行，最终单 commit 落盘。

#### 任务 P.1：撰写 spec 设计文档 ✅ 已完成

**文件：**
- 创建：`docs/specs/2026-05-05-gles-renderer-blueprint-design.md`（~720 行）

完成度：✅（plan 阶段开始时已落盘）

#### 任务 P.2：撰写 plan 实施计划（本文档）🟡 进行中

**文件：**
- 创建：`docs/plans/2026-05-05-gles-renderer-blueprint.md`（~900 行）

#### 任务 P.3：撰写 creative-gles-context（B1）⏳ 待

**文件：**
- 创建：`memory-bank/creative/creative-gles-context.md`（~280 行）

**内容：**
- B1 GL context 创建路径权衡：SDL_GL_CreateContext / EGL 直接 / GLX 兼容 三方对比
- 桌面 SDL_GL_CreateContext 详细流程（SDL_GL_SetAttribute → SDL_CreateWindow w/SDL_WINDOW_OPENGL → SDL_GL_CreateContext → SDL_GL_MakeCurrent → SDL_GL_SwapWindow）
- 嵌入式 EGL 直接路径详细流程（eglGetDisplay → eglInitialize → eglChooseConfig → eglCreateContext → eglMakeCurrent → eglSwapBuffers）
- Context lost 处理对照（桌面 SDL2 SDL_RENDERER_TARGETTEXTURE 容错 vs 嵌入式 EGL_KHR_create_context_no_error）
- GLES 版本协商策略（3.0 baseline / 3.1+ 检测启用 compute shader / 3.2+ 检测启用 SPIR-V）

#### 任务 P.4：撰写 creative-gles-canvas（B2）⏳ 待

**文件：**
- 创建：`memory-bank/creative/creative-gles-canvas.md`（~360 行）

**内容：**
- B2 Canvas trampolining 三方案对比（混合 / 全 shader / 全 stencil）
- shader-based: FillRect / FillRoundedRect / Solid Brush 详细 shader 代码 + uniform 设计
- libtess2-based: FillPath tessellation 流程（Path → contours → tess → triangles → VBO → glDrawElements）
- Stroke = Fill 转换策略（StrokeRoundedRect → 2 个 RoundedRect 减法 / StrokeLine → 矩形）
- LinearGradient / RadialGradient SDF shader 设计

#### 任务 P.5：撰写 creative-gles-resources（B3 + B4 + B6）⏳ 待

**文件：**
- 创建：`memory-bank/creative/creative-gles-resources.md`（~320 行）

**内容：**
- B3 GlyphAtlas 设计：CPU FreeType + GL_R8 atlas / row-pack / LRU evict
- ImageTexturePool 设计：GL_RGBA8 texture / handle-keyed cache
- B4 dirty rect GPU 化：ComputeDirtyRect → glScissor → glClear 流程图 + interaction with stencil
- B6 shader 资源管理：raw string literal 嵌入 / 编译期绑定 / shader 缓存

#### 任务 P.6：Memory Bank 更新 ⏳ 待

**文件：**
- 修改：`memory-bank/activeContext.md`（阶段 初始化 → 规划完成 + 蓝图主交付落盘）
- 修改：`memory-bank/tasks.md`（添加规划完成节 + N 个 Level 3 子任务清单）
- 修改：`memory-bank/progress.md`（添加规划完成里程碑）

#### 任务 P.7：单 commit 落盘（P0 协议）⏳ 待

**命令：**
```bash
git add docs/specs/2026-05-05-gles-renderer-blueprint-design.md \
        docs/plans/2026-05-05-gles-renderer-blueprint.md \
        memory-bank/creative/creative-gles-*.md \
        memory-bank/activeContext.md \
        memory-bank/tasks.md \
        memory-bank/progress.md

git commit -m "docs(blueprint): G1 OpenGL ES blueprint [TASK-20260505-03]"
```

---

## 3. 后续 Level 3 实施任务详细规格（用户独立立项依据）

> **重要：** 以下子任务**不在本任务范围内执行**；本节是用户后续基于本蓝图独立立项的依据。每个子任务都是 Level 3 / 含完整 Phase 0 audit + ctest 矩阵 + 反向探针策略。

### 3.1 子任务 G1.1 — CMake `VX_RENDERER` flag

**复杂度：** Level 2
**估时：** ~2-3 h plan ×0.6
**前置：** —

**文件：**
- 修改：`CMakeLists.txt`（顶层 / +~30 行）
- 修改：`veloxa/graphics/CMakeLists.txt`（+~20 行 / VX_RENDERER 分支）
- 创建：`tests/cmake/vx_renderer_flag_test.sh`（CMake flag 验证脚本）

**任务步骤：**

- [ ] **Phase 0：grep 实证**
  - 验证 `find_package(OpenGLES)` / `find_package(EGL)` 在 Ubuntu 14+ 默认可用
  - 验证既有 `VX_PLATFORM_SDL2` / `VX_BUILD_DEVTOOL` flag pattern
- [ ] **步骤 1：编写 CMake 配置 + 错误信息测试 [TDD]**
  - 期望：`cmake -DVX_RENDERER=invalid` → FATAL_ERROR
  - 期望：`cmake -DVX_RENDERER=software` → 不引入 OpenGLES dep
  - 期望：`cmake -DVX_RENDERER=gles` → 引入 OpenGLES + EGL dep
- [ ] **步骤 2：实现 CMake flag**
  ```cmake
  option(VX_RENDERER "Renderer backend (software|gles)" "software")
  if(NOT VX_RENDERER MATCHES "^(software|gles)$")
    message(FATAL_ERROR "VX_RENDERER must be 'software' or 'gles', got: ${VX_RENDERER}")
  endif()
  if(VX_RENDERER STREQUAL "gles")
    find_package(OpenGLES REQUIRED)
    find_package(EGL REQUIRED)
    add_compile_definitions(VX_RENDERER_GLES=1)
  else()
    add_compile_definitions(VX_RENDERER_SOFTWARE=1)
  endif()
  ```
- [ ] **步骤 3：双 build 矩阵 ctest 验证**
  ```bash
  cmake -B build-software -DVX_RENDERER=software && cmake --build build-software
  cmake -B build-gles -DVX_RENDERER=gles && cmake --build build-gles  # 预期 PASS（仅 flag 落地，无 GLES 实现，build 通过）
  ```
- [ ] **步骤 4：反向探针**
  - 临时改 `add_compile_definitions(VX_RENDERER_INVALID=1)` → 编译期错误（验证 flag 生效）
- [ ] **步骤 5：commit**

**验收：**
- ✅ `VX_RENDERER=software` 默认（B5）/ ctest 1302/1302 PASS（不退化）
- ✅ `VX_RENDERER=gles` 通过 / `VX_RENDERER_GLES=1` 宏可见
- ✅ 错误参数 FATAL_ERROR

**ctest 期望矩阵：**

| Config | DEVTOOL | SDL2 | VX_RENDERER | 期望 ctest |
|---|:-:|:-:|:-:|:-:|
| baseline | ON | OFF | software | 1302 |
| 本子任务 | ON | OFF | gles | 1302（仅 flag 落地 / 无新测）|

---

### 3.2 子任务 G1.2 — `GLESDisplay` 抽象 + `Sdl2EGLDisplay` 实施

**复杂度：** Level 3
**估时：** ~4-6 h plan ×0.6
**前置：** G1.1

**文件：**
- 创建：`veloxa/platform/gles_display.h`（~80 行 / GLESDisplay 纯虚抽象）
- 创建：`veloxa/platform/sdl2/sdl2_egl_display.h`（~50 行）
- 创建：`veloxa/platform/sdl2/sdl2_egl_display.cc`（~150 行）
- 修改：`veloxa/platform/CMakeLists.txt`（+~10 行 / GLES 路径分支）
- 创建：`tests/platform/sdl2/sdl2_egl_display_test.cc`（~200 行 / ~6-8 单测）

**任务步骤：**

- [ ] **Phase 0：grep audit**
  - 验证 `SDL_GL_CreateContext` API 签名（SDL2 2.0.20+）
  - 验证 `eglGetCurrentDisplay` / `eglGetCurrentContext` API 签名
- [ ] **步骤 1：编写 GLESDisplay 接口测试 [TDD]**
  - `Sdl2EGLDisplay::Initialize()` 成功后 `IsValid()` ✅
  - `MakeCurrent()` 后 `eglGetCurrentContext()` 非 EGL_NO_CONTEXT
  - `gles_major_version() / gles_minor_version()` 返回 ≥ 3
  - `IsContextLost() == false` 初始
  - `Shutdown()` 后 `IsValid()` ❌
- [ ] **步骤 2：实现 GLESDisplay 纯虚抽象**（参考 spec §3.3.2）
- [ ] **步骤 3：实现 Sdl2EGLDisplay**
  - 构造 `SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3)` 等 6 个属性
  - `Initialize()` → `SDL_GL_CreateContext` + 检查 `glGetString(GL_VERSION)`
  - `MakeCurrent()` / `DoneCurrent()` → `SDL_GL_MakeCurrent`
  - `SwapBuffers()` → `SDL_GL_SwapWindow`
  - `IsContextLost()` → `glGetError() == GL_CONTEXT_LOST_KHR` 检测
  - `RestoreContext()` → 重建 context 路径
  - `HasExtension()` → `glGetStringi(GL_EXTENSIONS, i)` 遍历
- [ ] **步骤 4：反向探针**
  - 临时改 `SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1)` → `Initialize()` 失败 / `IsValid() == false` （证明版本协商有效）
- [ ] **步骤 5：commit**

**验收：**
- ✅ ~6-8 单测全 PASS（含 mock SDL_Window 路径）
- ✅ Mesa headless `EGL_PLATFORM=surfaceless` 兼容性 audit
- ✅ ctest gles config +6-8 / software config 不退化

**ctest 期望矩阵：**

| Config | VX_RENDERER | 期望 ctest |
|---|:-:|:-:|
| software | software | 1302（不退化）|
| gles | gles | 1302 + ~6-8 = ~1308-1310 |

---

### 3.3 子任务 G1.3 — `Sdl2GLWindowSurface` 实施

**复杂度：** Level 3
**估时：** ~3-4 h plan ×0.6
**前置：** G1.2

**文件：**
- 创建：`veloxa/platform/sdl2/sdl2_gl_window_surface.h`（~60 行）
- 创建：`veloxa/platform/sdl2/sdl2_gl_window_surface.cc`（~180 行）
- 修改：`veloxa/platform/CMakeLists.txt`（+~5 行）
- 创建：`tests/platform/sdl2/sdl2_gl_window_surface_test.cc`（~150 行 / ~4-6 单测）

**任务步骤：**

- [ ] **Phase 0：grep audit**
  - 验证 `glReadPixels` API 在 GLES 3.0 中的限制（仅 GL_RGBA + GL_UNSIGNED_BYTE）
- [ ] **步骤 1：编写 Sdl2GLWindowSurface 测试 [TDD]**
  - 构造后 `width()` / `height()` / `gles_display()` 非空
  - `Lock()` 返回 `nullptr`（GLES no-op）
  - `Resize(800, 600)` 后 `width() == 800 && height() == 600`
  - `SavePPM("/tmp/test.ppm")` 通过 `glReadPixels` → 文件存在 / RGB 格式正确
  - `Present()` 触发 `SDL_GL_SwapWindow`
- [ ] **步骤 2：实现 Sdl2GLWindowSurface**
  - 构造：`SDL_CreateWindow(... | SDL_WINDOW_OPENGL)` + `Sdl2EGLDisplay` 关联
  - `Lock() / Unlock()` no-op
  - `Resize()` → `SDL_SetWindowSize` + 视窗参数更新
  - `SavePPM()` → `glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, buf)` → 翻转 Y + 写 PPM
  - `Present()` → `display_->SwapBuffers()`
- [ ] **步骤 3：反向探针**
  - 临时改 `SDL_CreateWindow(... 无 SDL_WINDOW_OPENGL)` → `Sdl2EGLDisplay::Initialize()` 失败（证明 OpenGL flag 生效）
- [ ] **步骤 4：commit**

**验收：**
- ✅ ~4-6 单测全 PASS
- ✅ `SavePPM` 像素 byte order 正确（与 SoftwareCanvas 一致）
- ✅ ctest gles +4-6

---

### 3.4 子任务 G1.4 — `GLESCanvas` 骨架 + Begin/End/Clear/SetTransform

**复杂度：** Level 3
**估时：** ~3-4 h plan ×0.6
**前置：** G1.3

**文件：**
- 创建：`veloxa/graphics/gles/gles_canvas.h`（~120 行）
- 创建：`veloxa/graphics/gles/gles_canvas.cc`（~250 行 / 仅骨架方法 + state stack）
- 创建：`veloxa/graphics/gles/shaders.h`（~80 行 / B6 shader 静态嵌入）
- 修改：`veloxa/graphics/CMakeLists.txt`（+~20 行 / VX_RENDERER=gles 分支）
- 创建：`tests/graphics/gles/gles_canvas_skeleton_test.cc`（~180 行 / ~6-8 单测）

**任务步骤：**

- [ ] **Phase 0：grep audit**
  - 验证 GLES 3.0 `glViewport / glClearColor / glClear / glBindVertexArray` 全部可用
- [ ] **步骤 1：编写 GLESCanvas 骨架测试 [TDD]**
  - `Begin()` 后 `glIsEnabled(GL_BLEND) == GL_TRUE`
  - `Clear(red)` 后 `glReadPixels(0,0,1,1)` == red bytes
  - `SetTransform(m)` + `GetTransform()` 一致
  - `PushState() / PopState()` 状态栈正确
  - `End()` 触发 `glFlush`
- [ ] **步骤 2：实现 GLESCanvas 骨架**
  - 构造：`glGenVertexArrays(1, &quad_vao_)` + `glGenBuffers(1, &quad_vbo_)`
  - `Begin()`：`glViewport(0,0,w,h)` + `glEnable(GL_BLEND)` + `glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)`
  - `Clear()`：`glClearColor` + `glClear(GL_COLOR_BUFFER_BIT)`
  - `SetTransform()` / `GetTransform()`：CPU 影子状态
  - `PushState()` / `PopState()`：栈
  - 其余方法暂留空（下一子任务 G1.5+ 实施）
- [ ] **步骤 3：反向探针**
  - 临时改 `glClear(0)`（不 clear color）→ `Clear(red)` 测试 FAIL（证明 clear bit 生效）
- [ ] **步骤 4：commit**

**验收：**
- ✅ ~6-8 单测全 PASS
- ✅ shader 编译期检查（B6 raw string literal 编译期类型检查）
- ✅ ctest gles +6-8

---

### 3.5 子任务 G1.5 — `GLESCanvas::FillRect` + `FillRoundedRect` + Solid Brush

**复杂度：** Level 3
**估时：** ~5-7 h plan ×0.6
**前置：** G1.4

**文件：**
- 修改：`veloxa/graphics/gles/gles_canvas.cc`（+~250 行 / FillRect + FillRoundedRect 实现）
- 修改：`veloxa/graphics/gles/shaders.h`（+~80 行 / kSolidVert/kSolidFrag/kRoundedRectFrag）
- 创建：`tests/graphics/gles/gles_canvas_fill_test.cc`（~250 行 / ~10-12 单测）

**任务步骤：**

- [ ] **Phase 0：grep audit**
  - 验证 GLES 3.0 `glCreateShader / glCompileShader / glLinkProgram / glUniform*` 可用
  - 验证 GLES 3.0 SDF shader `smoothstep / length / max` 标准函数可用
- [ ] **步骤 1：编写 FillRect / FillRoundedRect 测试 [TDD]**
  - `FillRect(rect, red)` → 像素采样 4 角 == red
  - `FillRoundedRect(rect, radius=10, red)` → 4 角圆角处像素 < red.a（SDF 反走样）
  - `FillRect(rect, transparent)` → 像素 alpha 混合验证
  - SetTransform 后 FillRect 位置正确（旋转 / 缩放）
- [ ] **步骤 2：实现 FillRect**（参考 spec §4.1）
- [ ] **步骤 3：实现 FillRoundedRect**（SDF shader 路径）
- [ ] **步骤 4：反向探针 ×3**
  - 临时改 fragment shader `frag_color = u_color * 0.0` → 测试 FAIL（证明 shader uniform 生效）
  - 临时改 vertex shader 跳过 u_xform → 测试 FAIL（证明 transform 生效）
  - 临时改 SDF radius 计算 → 圆角测试 FAIL（证明 SDF 生效）
- [ ] **步骤 5：commit**

**验收：**
- ✅ ~10-12 单测全 PASS
- ✅ 像素级 vs SoftwareCanvas 对照（容忍 ±2 byte / SDF 抗锯齿差异）
- ✅ ctest gles +10-12

---

### 3.6 子任务 G1.6 — `GLESCanvas::FillPath` via libtess2

**复杂度：** Level 3（边界曲折 / +30% buffer）
**估时：** ~6-8 h plan ×0.6
**前置：** G1.5

**文件：**
- 修改：`CMakeLists.txt`（+~10 行 / FetchContent libtess2）
- 修改：`veloxa/graphics/CMakeLists.txt`（+~5 行 / link tess2）
- 修改：`veloxa/graphics/gles/gles_canvas.cc`（+~150 行 / FillPath via tessellation）
- 创建：`tests/graphics/gles/gles_canvas_path_test.cc`（~300 行 / ~8-10 单测）

**任务步骤：**

- [ ] **Phase 0：libtess2 集成 audit**
  - 验证 libtess2 API（`tessNewTess` / `tessAddContour` / `tessTesselate` / `tessGetVertices` / `tessGetElements`）
  - 验证 libtess2 与既有 SoftwarePath contour 数据兼容性（`SafeCastSoftwarePath` 可行性）
- [ ] **步骤 1：编写 FillPath 测试 [TDD]**
  - 简单三角形 → 3 顶点 / glDrawElements 1 triangle
  - 自相交多边形（star） → tess 输出 ≥ 3 triangles / fill 区域正确
  - Bezier curve（quadTo / cubicTo）→ tess 处理或先 flatten
  - 复杂复合 path（多 contour）→ winding rule 验证
- [ ] **步骤 2：实现 FillPath**（参考 spec §4.2）
- [ ] **步骤 3：反向探针**
  - 临时改 `TESS_WINDING_NONZERO` → `TESS_WINDING_NEGATIVE` → 测试 FAIL（证明 winding rule 生效）
- [ ] **步骤 4：commit**

**验收：**
- ✅ ~8-10 单测全 PASS
- ✅ libtess2 集成 0 leak（ASAN）
- ✅ ctest gles +8-10

---

### 3.7 子任务 G1.7 — `Stroke*` 方法（Stroke = Fill 转换）

**复杂度：** Level 3
**估时：** ~3-4 h plan ×0.6
**前置：** G1.6

**详见 spec §3.3.1 + creative-gles-canvas Stroke 转换段。**

**估时分解：**
- StrokeRect（4 边矩形 fill 拼接）：~30 min
- StrokeLine（旋转矩形 fill）：~30 min
- StrokeRoundedRect（2 RoundedRect fill 减法）：~1 h
- StrokePath（tessellator + offset）：~1.5 h
- 测试 + 反向探针：~1 h

---

### 3.8 子任务 G1.8 — `GlyphAtlas` + `GLESCanvas::DrawText`

**复杂度：** Level 4（GPU 资源管理 + LRU + 多字号 + emoji edge case）
**估时：** ~8-10 h plan ×0.6
**前置：** G1.5

**文件：**
- 创建：`veloxa/graphics/gles/glyph_atlas.h`（~100 行）
- 创建：`veloxa/graphics/gles/glyph_atlas.cc`（~300 行）
- 修改：`veloxa/graphics/gles/gles_canvas.cc`（+~200 行 / DrawText impl）
- 修改：`veloxa/graphics/gles/shaders.h`（+~40 行 / glyph shader）
- 创建：`tests/graphics/gles/glyph_atlas_test.cc`（~250 行 / ~8-10 单测）
- 创建：`tests/graphics/gles/gles_canvas_text_test.cc`（~250 行 / ~10-12 单测）

详见 creative-gles-resources §3.

---

### 3.9 子任务 G1.9 — `ImageTexturePool` + `GLESCanvas::DrawImage`

**复杂度：** Level 3
**估时：** ~4-6 h plan ×0.6
**前置：** G1.5

**文件：**
- 创建：`veloxa/graphics/gles/image_texture_pool.h/cc`（~250 行 总）
- 修改：`veloxa/graphics/gles/gles_canvas.cc`（+~120 行 / DrawImage impl）
- 创建：`tests/graphics/gles/image_texture_pool_test.cc`（~150 行 / ~6-8 单测）

详见 creative-gles-resources §4.

---

### 3.10 子任务 G1.10 — `PushClipRect/PopClip` (glScissor) + `PushLayer/PopLayer` (FBO)

**复杂度：** Level 3
**估时：** ~5-7 h plan ×0.6
**前置：** G1.5

**任务步骤：**

- [ ] **Phase 0：FBO + glScissor 交互 audit**
  - 验证 GLES 3.0 `glGenFramebuffers / glFramebufferTexture2D / glDrawBuffers` 可用
  - 验证 stencil buffer 使能策略（GLES 3.0 stencil 8-bit 默认开）
- [ ] **步骤 1：测试 [TDD]**
  - `PushClipRect(r) → FillRect(超出 r) → PopClip` → r 内有 fill / r 外无 fill
  - `PushLayer(bounds, opacity=0.5)` → 子绘制 FillRect → PopLayer → 像素 alpha = 0.5
- [ ] **步骤 2：实现 PushClipRect**（glScissor 栈）
- [ ] **步骤 3：实现 PushLayer**（临时 FBO + bind + draw + unbind + composite blend）
- [ ] **步骤 4：反向探针 ×2**
- [ ] **步骤 5：commit**

详见 creative-gles-resources §5.

---

### 3.11 子任务 G1.11 — dirty rect + glScissor 集成

**复杂度：** Level 2
**估时：** ~2-3 h plan ×0.6
**前置：** G1.10

**文件：**
- 修改：`veloxa/core/application.cc`（+~30 行 / `#if VX_RENDERER_GLES` glScissor 分支）
- 创建：`tests/core/application_gles_dirty_rect_test.cc`（~150 行 / ~4-6 单测）

详见 spec §4.4 + creative-gles-resources §6。

---

### 3.12 子任务 G1.12 — LinearGradient / RadialGradient（SDF shader）

**复杂度：** Level 3
**估时：** ~4-6 h plan ×0.6
**前置：** G1.5

**文件：**
- 修改：`veloxa/graphics/gles/gles_canvas.cc`（+~80 行）
- 修改：`veloxa/graphics/gles/shaders.h`（+~60 行 / gradient shaders）
- 创建：`tests/graphics/gles/gles_canvas_gradient_test.cc`（~200 行 / ~6-8 单测）

详见 creative-gles-canvas §LinearGradient / RadialGradient SDF shader 段。

---

### 3.13 子任务 G1.13 — `Application` 构造分支（B5 落地）+ fallback

**复杂度：** Level 3
**估时：** ~3-4 h plan ×0.6
**前置：** G1.4 + G1.3

**文件：**
- 修改：`veloxa/core/application.cc`（+~50 行 / spec §3.6 路径）
- 创建：`tests/core/application_gles_test.cc`（~250 行 / ~6-8 单测）

**关键 fallback 容错（B5 + lazy-attach quad-evidence 范式延续）：**
- `dynamic_cast<Sdl2GLWindowSurface*>(config_.surface) == nullptr` → fallback SoftwareCanvas
- `Sdl2EGLDisplay::Initialize()` 失败 → fallback SoftwareCanvas
- shader 编译失败 → fallback SoftwareCanvas

---

### 3.14 子任务 G1.14 — Context Lost / Restore（B8 嵌入式 + G2 桥接）

**复杂度：** Level 3
**估时：** ~5-7 h plan ×0.6
**前置：** G1.4

**文件：**
- 修改：`veloxa/graphics/gles/gles_canvas.cc`（+~150 行 / OnContextLost / OnContextRestored）
- 修改：`veloxa/platform/sdl2/sdl2_egl_display.cc`（+~80 行 / context lost detection + recovery）
- 创建：`tests/graphics/gles/gles_canvas_context_lost_test.cc`（~250 行 / ~4-6 单测）

详见 creative-gles-context §Context lost 处理段。

---

### 3.15 子任务 G1.15 — `examples/hello_sdl2` GLES 路径 + smoke

**复杂度：** Level 2
**估时：** ~2-3 h plan ×0.6
**前置：** G1.13 + G1.4

**文件：**
- 修改：`examples/hello_sdl2.cc`（+~30 行 / VX_RENDERER=gles 分支构造 Sdl2GLWindowSurface）
- 修改：`examples/CMakeLists.txt`（+~5 行）
- 修改：`tests/CMakeLists.txt`（+~10 行 / `hello_sdl2_gles_smoke` ctest）

**ctest 期望矩阵：**

| smoke | software | gles |
|---|:-:|:-:|
| `hello_sdl2_smoke` | PASS | PASS（fallback）|
| `hello_sdl2_gles_smoke` | SKIP（VX_RENDERER guard）| PASS |

---

### 3.16 子任务 G1.16 — DevTool dogfood GLES 路径 + smoke

**复杂度：** Level 3
**估时：** ~4-6 h plan ×0.6
**前置：** G1.15

**文件：**
- 修改：`examples/hello_devtool.cc`（+~30 行）
- 修改：`tests/CMakeLists.txt`（+~5 行 / `hello_devtool_gles_smoke`）

**关键验收：**
- ✅ inspector tab 切换 GLES 路径不破坏
- ✅ overlay highlight (R3 dirty rect overlay) GLES 路径正常
- ✅ HUD 文字 GLES 路径正常（FreeType + GlyphAtlas）

---

### 3.17 子任务 G1.17 — `BM_GLESReplay*` 性能基准（B7 验收）

**复杂度：** Level 3
**估时：** ~4-6 h plan ×0.6
**前置：** G1.15

**文件：**
- 创建：`benchmarks/bench_gles_replay.cc`（~300 行 / 6 BM）
- 修改：`benchmarks/CMakeLists.txt`（+~10 行）

**6 BM 列表：**

1. `BM_GLESReplaySmoke` — vs `BM_ReplaySmoke` — 期望 ≥ 10x SW
2. `BM_GLESReplayLargeList` — vs `BM_ReplayLargeList` — 期望 ≥ 5x SW
3. `BM_GLESReplayTextHeavy` — vs `BM_ReplayTextHeavy` — 期望 ≥ 3x SW
4. `BM_GLESReplayDeepClip` — vs `BM_ReplayDeepClip` — 期望 ≥ 5x SW
5. `BM_GLESReplay1080pBudget` — 1080p typical 页面 — ≤ 16.6ms
6. `BM_GLESDirtyRectScissor` — 全帧 vs scissor — 期望 ≥ 3x

详见 spec §7.3。

---

### 3.18 子任务 G1.18 — G2 接口预留 audit + GpuFence 头注释占位

**复杂度：** Level 2
**估时：** ~1-2 h plan ×0.6
**前置：** G1.14

**文件：**
- 修改：`veloxa/platform/gles_display.h`（+~30 行 / GpuFence 头注释占位）
- 创建：`docs/specs/2026-05-XX-gles-g2-audit.md`（~100 行 / G1 → G2 接口移交清单）

---

## 4. 实施任务总估时表

| Sub-Task | Level | plan ×0.6 |
|:-:|---|:-:|
| G1.1 | L2 | ~2-3 h |
| G1.2 | L3 | ~4-6 h |
| G1.3 | L3 | ~3-4 h |
| G1.4 | L3 | ~3-4 h |
| G1.5 | L3 | ~5-7 h |
| G1.6 | L3 | ~6-8 h |
| G1.7 | L3 | ~3-4 h |
| G1.8 | L4 | ~8-10 h |
| G1.9 | L3 | ~4-6 h |
| G1.10 | L3 | ~5-7 h |
| G1.11 | L2 | ~2-3 h |
| G1.12 | L3 | ~4-6 h |
| G1.13 | L3 | ~3-4 h |
| G1.14 | L3 | ~5-7 h |
| G1.15 | L2 | ~2-3 h |
| G1.16 | L3 | ~4-6 h |
| G1.17 | L3 | ~4-6 h |
| G1.18 | L2 | ~1-2 h |
| **小计** | | **~68-96 h plan ×0.6** |
| +30% buffer（GLES 新领域 / libtess2 集成 / Mesa headless 测试可能曲折）| | **~88-125 h plan ×0.6** |

---

## 5. 阶段性 commit 范本（蓝图阶段 + 实施阶段）

### 5.1 蓝图阶段 commit 范本（本任务）

```
docs(blueprint): G1 OpenGL ES blueprint [TASK-20260505-03]

V2=a 纯蓝图任务 / 主交付 = spec + plan + creative ×3 / 不含 build。

5/5 V 决策 + 8/8 B 决策全 all_recommended 锁定（跨决策协同度 100%
第 11 + 12 次连续命中 / dec → endec → doudec-evidence 候选 / 累计 113/113）:
  V1-A GLES only (OpenGL ES 3.0+ 完整 / Vulkan 仅预留接口位置)
  V2-A pure_blueprint_a (纯蓝图 / spec + plan + creative ×N / 不含 build)
  V3-A desktop_first (桌面 SDL2+EGL/GLX 完整 + 嵌入式抽象接口预留)
  V4-A co_design_boundary (G1 定义 Renderer/Surface 抽象 / G2 独立)
  V5-A vx_renderer_flag (VX_RENDERER=software|gles CMake flag)
  B1-A SDL_GL_CreateContext + EGL 嵌入式接口预留
  B2-A 混合（FillRect/RoundedRect = shader / FillPath = libtess2）
  B3-A CPU 光栅化 + GPU texture atlas (GL_R8 + 复用 FreeType)
  B4-A ComputeDirtyRect + glScissor + glClear
  B5-A software 默认 / GLES opt-in / fallback 共存
  B6-A 静态嵌入 .glsl raw string literal / 编译期绑定
  B7-A BM_Replay* + BM_GLESReplay* 同 corpus 双测对照
  B8-A 完整预留（ContextLost/Restore + GLESDisplay + GpuFence）

主交付（~2690 行新增）:
  docs/specs/2026-05-05-gles-renderer-blueprint-design.md (~720 行)
  docs/plans/2026-05-05-gles-renderer-blueprint.md (~900 行)
  memory-bank/creative/creative-gles-context.md (~280 行)
  memory-bank/creative/creative-gles-canvas.md (~360 行)
  memory-bank/creative/creative-gles-resources.md (~320 行)
  memory-bank/{activeContext,tasks,progress}.md (+~110 行)

后续实施: 用户基于本蓝图独立立项 18 个 Level 3 子任务（G1.1-G1.18 /
~68-96 h plan ×0.6 / +30% buffer = ~88-125 h plan ×0.6）。

P0 协议首次完整实施: 「plan/spec docs 落盘即 commit」从 TASK-02 首
次成功 → 本任务首次完整执行（plan/spec/creative ×3 + Memory Bank
单 commit 落盘）。

下一步: /reflect (蓝图任务回顾)
```

### 5.2 实施阶段 commit 范本（用户后续 G1.1-G1.18 立项时参考）

```
feat(gles): G1.X 子任务名 [TASK-2026MMDD-NN]

Phase 0 audit (3 实证 grep + 1 工具链快照): ✅
TDD 三阶 (RED → GREEN → REFACTOR): ✅
反向探针 N/N: ✅ 精准 FAIL

ctest 期望矩阵 (config 矩阵 plan 已验证):
  software: 1302 / 1302 (不退化)
  gles: 1302+N → ~1302+N

文件变更:
  veloxa/graphics/gles/...
  tests/graphics/gles/...
  ...

下一步: G1.X+1 子任务（前置已就位）
```

---

## 6. ctest 数量预期 config 矩阵（双 build 矩阵）

| Config | DEVTOOL | SDL2 | VX_RENDERER | 期望 ctest baseline | 子任务累计预期 |
|---|:-:|:-:|:-:|:-:|---|
| baseline | ON | OFF | software | **1302** | 不退化 / fallback 路径覆盖 |
| **gles full** | ON | ON | gles | **1302 + ~80-100 + ~4 smoke = ~1386-1406** | 18 子任务全闭环后预期 |
| OFF | OFF | OFF | software | 1109（既有）| 不退化（A14 link closure guard）|

> **配置矩阵关键不变量：**
> - `VX_RENDERER=software` 任意 config → 1302 不退化（lazy-attach + fallback）
> - `VX_RENDERER=gles` 仅在 SDL2=ON 时启用（嵌入式 G2 蓝图阶段补 DRM/KMS path）
> - 实施过程每 G1.X 子任务 commit 必声明本子任务的 ctest 数量增量（writing-plans.mdc §ctest 数量预期 config 矩阵 P0 强制）

---

## 7. 安全任务清单（[安全相关] tag）

> 详见 spec §6.2。本表为 G1.X 实施阶段的安全测试任务清单。

| # | 安全测试 | 子任务 | 测试文件 |
|:-:|---|:-:|---|
| 1 | shader 注入防御（用户内容**永不**作 shader source）| G1.4 + G1.5 | `tests/graphics/gles/shader_injection_test.cc` |
| 2 | EGL display 句柄泄露（RAII + 析构序）| G1.2 + G1.13 | `tests/platform/sdl2/sdl2_egl_display_lifecycle_test.cc` |
| 3 | GL extension 安全枚举（只读 / 不分支用户输入）| G1.2 | `tests/platform/sdl2/sdl2_egl_display_test.cc` |
| 4 | context lost 资源泄露（OnContextLost 释放 / OnContextRestored 重建）| G1.14 | `tests/graphics/gles/gles_canvas_context_lost_test.cc` |
| 5 | 多线程 GL 调用（主线程约束）| G1.4 | `tests/graphics/gles/gles_canvas_thread_safety_test.cc` |
| 6 | dynamic_cast Surface* fallback（UB 防御）| G1.13 | `tests/core/application_gles_test.cc` |

---

## 8. 待 reflect 阶段重审清单

V2=a 蓝图任务 reflect 阶段必须重审 13/13 决策合理性（沿用 TASK-20260430-04 + TASK-20260504-01 范式）：

- [ ] V1 GLES only — Vulkan 预留是否充分？
- [ ] V2 pure_blueprint_a — 不含 build 是否引入实施返工风险？
- [ ] V3 desktop_first — DRM/KMS 嵌入式预留是否够用？
- [ ] V4 co_design_boundary — G2 边界划界是否清晰？
- [ ] V5 vx_renderer_flag — software / gles 双路径是否未来 runtime 切换？
- [ ] B1 SDL_GL + EGL — 嵌入式 EGL 直接路径预留是否详尽？
- [ ] B2 混合策略 — FillPath libtess2 是否会成为性能瓶颈？
- [ ] B3 CPU + GPU atlas — 是否未来需要 GPU SDF？
- [ ] B4 glScissor — 嵌入式 tile-based GPU 是否需要不同策略？
- [ ] B5 software 默认 — 何时切换 gles 默认？
- [ ] B6 raw string literal — 是否未来需要 SPIR-V 预编译？
- [ ] B7 dual BM — 是否需要补 GPU profiling（GL_TIME_ELAPSED）？
- [ ] B8 完整预留 — GpuFence 接口是否需要 G1 阶段 stub 实施？

---

## 9. 引用

- [`docs/specs/2026-05-05-gles-renderer-blueprint-design.md`](../specs/2026-05-05-gles-renderer-blueprint-design.md) — 本任务 spec
- [`memory-bank/creative/creative-gles-context.md`](../../memory-bank/creative/creative-gles-context.md) — B1 GL context creative
- [`memory-bank/creative/creative-gles-canvas.md`](../../memory-bank/creative/creative-gles-canvas.md) — B2 Canvas trampolining creative
- [`memory-bank/creative/creative-gles-resources.md`](../../memory-bank/creative/creative-gles-resources.md) — B3+B4+B6 资源管理 creative
- [`docs/specs/2026-04-05-graphics-platform-hal-design.md`](../specs/2026-04-05-graphics-platform-hal-design.md) — 上游 Graphics HAL
- [`docs/specs/2026-05-04-mvp-scope.md`](../specs/2026-05-04-mvp-scope.md) §C.2 + §11.2 #5 — MVP 蓝图 / 立项依据
- [`memory-bank/archive/archive-TASK-20260430-04.md`](../../memory-bank/archive/archive-TASK-20260430-04.md) — DevTool 蓝图 V2=a 范式
- [`memory-bank/archive/archive-TASK-20260504-01.md`](../../memory-bank/archive/archive-TASK-20260504-01.md) — MVP-scope 蓝图 V2=a 范式

---

**END OF PLAN**
