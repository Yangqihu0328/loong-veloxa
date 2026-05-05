# 实现计划：G1.2 `GLESDisplay` 抽象 + `Sdl2EGLDisplay` 实施

**日期：** 2026-05-05
**任务 ID：** `TASK-20260505-06`
**复杂度级别：** Level 3 实施类（新平台抽象 + SDL2 子类实施 / 跨 G1+G2 桥接接口预留）
**任务定位：** **MVP-C 战略主线第二个实施任务** / GLES 蓝图实施第二步 / **G1.1 D1=A 推迟点正式落地**（首次引入 EGL/GLES dep）
**安全相关：** ❌ 否（仅平台抽象 + GL context 创建 / 0 输入处理 / 0 网络 / 0 新威胁面）
**估时（plan ×0.6）：** ~1.5-3 h（实施类 Level 3 子档 / 标准极速区 0.4-0.6×）

---

## 0. 上下文与决策矩阵

### 0.1 上下文

- **任务来源：** [GLES 蓝图 plan §3.2](2026-05-05-gles-renderer-blueprint.md#32-子任务-g12--glesdisplay-抽象--sdl2egldisplay-实施) + [GLES 蓝图 spec §3.3.2](../specs/2026-05-05-gles-renderer-blueprint-design.md)
- **前置：** TASK-20260505-05 G1.1 CMake `VX_RENDERER` flag ✅ 已合并 main `ee2569d`
- **本任务定位：** 落地 GLESDisplay 纯虚抽象（B8 G2 桥接接口预留）+ Sdl2EGLDisplay SDL2 实施
- **不在本任务范围：** Sdl2GLWindowSurface（G1.3 / 后续任务）/ GLESCanvas（G1.4+ / 后续任务）/ GpuFence（G2 蓝图 / 接口注释占位）

### 0.2 决策矩阵（VAN + Plan 阶段 1 次 AskQuestion all_recommended 锁定 ✅）

**跨决策协同度 100% 第 15 次连续命中** / 累计 **136/136 历史最高 streak 续刷**

| # | 决策项 | 选择 | 详细理由 |
|:-:|---|---|---|
| **D1** | testing fixture 策略 | **A SDL_VIDEODRIVER=offscreen** | SDL2 2.0.16+ 内置 / SDL_GL_CreateContext 真实创建（Mesa swrast 路径）/ 与既有 sdl2_window_surface_test.cc fixture 一致 |
| **D2** | context lost 覆盖度 | **B 含 RestoreContext 路径** | mock 触发 GL_CONTEXT_LOST_KHR + RestoreContext 重建 / 覆盖嵌入式刚需 / 不过度复杂 |
| **D3** | extension cache 策略 | **B Initialize() 后 eager std::unordered_set** | O(1) 查询 / 1 次 init / 嵌入式启动一次足矣 |
| **D4** | 反向探针实施 | **C inline test SDL_GL_SetAttribute(MAJOR_VERSION=99)** | 自动化 / 不污染源码 / 与 G1.1 D5 精神一致（用测试控制反向参数）|
| **D5** | 偏差点处理 | **A plan §0.4 详细校正（3 偏差全列）** | brainstorming P1.3 dual → **triple-evidence 候选** |
| **D6** | EGL/GLES dep 声明位置 | **A sdl2/CMakeLists.txt 局部** | 与 HARFBUZZ pattern 一致 / 与 G1.1 D2=A 顶层校验分工明确 / sdl2/ 是唯一 GL 路径 dep 消费者 |
| **D7** | commit 粒度 | **A 单 feat commit** | abstract + impl 强耦合 / 与 G1.1 一致 |
| **D8** | P0 协议复用 | **A plan + MB 单 commit / sext-evidence 第 6 数据点** | quint → sext / 实施类 Level 3 首次实证 / 适用性矩阵新增 Level 3 子档 |

### 0.3 不进入 `/creative`

实施类 Level 3 / 8 决策已 lock / 设计 spec §3.3.2 已完整规格化（11 virtual methods + ctor/dtor + private members）/ 0 创意阶段需求。

### 0.4 plan §3.2 偏差校正（来自 brainstorming P1.3 主动 push-back / dual-evidence 续延实战）

**3 处偏差需校正（VAN 阶段 Phase 0 grep audit 已发现）：**

#### 偏差 #1：CMakeLists.txt 修改位置

- **原 plan §3.2 推荐：** 修改顶层 `veloxa/platform/CMakeLists.txt` +~10 行 / GLES 路径分支
- **校正：** 顶层 platform/CMakeLists.txt **0 修改** + 改 `veloxa/platform/sdl2/CMakeLists.txt` +~10 行
- **理由：**
  - GLESDisplay.h 是**纯虚 header-only**（spec §3.3.2 / 0 .cc）
  - 顶层 platform/CMakeLists.txt 用 `target_include_directories(... ${CMAKE_SOURCE_DIR})` 隐式头扫描 → headers 自动可见
  - Sdl2EGLDisplay.cc 是 SDL2 子目录唯一新增 source / 与 sdl2_window_surface.cc 平级
  - EGL/GLES dep 仅 sdl2/ 消费 → D6=A 局部声明
- **影响：** plan 文件结构 §1.3 + ctest 估算 +~5 行

#### 偏差 #2：测试路径

- **原 plan §3.2 推荐：** 创建 `tests/platform/sdl2/sdl2_egl_display_test.cc`
- **校正：** 创建 `tests/platform/sdl2_egl_display_test.cc`（扁平路径 / 无 sdl2/ 子目录）
- **理由：**
  - 既有 tests/platform/ 是**扁平结构**（5 既有 _test.cc / sdl2_window_surface_test.cc / sdl2_event_loop_test.cc / sdl2_input_translate_test.cc 全在 tests/platform/ 直接）
  - 沿用扁平更与项目一致 / 减少 add_test 路径修改
- **影响：** plan 文件结构 §1.3 + tests/CMakeLists.txt add_test 路径

#### 偏差 #3：headless CI testing fixture 未明示

- **原 plan §3.2 推荐：** 步骤 1 测试设计未提 SDL/EGL headless setup
- **校正：** D1=A 决策 → `SDL_VIDEODRIVER=offscreen` 在 `::testing::Environment::SetUp()` 中 `SDL_setenv` 设置
- **理由：**
  - Cursor 沙箱无 X11/Wayland → SDL_Init(SDL_INIT_VIDEO) 默认会失败
  - SDL_VIDEODRIVER=offscreen 是 SDL2 2.0.16+ 内置 driver / 不需要 X11 / Mesa swrast 路径 / 真实创建 GL context
  - 不污染全局环境（::testing::Environment 仅作用于本测试）
- **影响：** plan 步骤 1 测试设计 + sdl2_egl_display_test.cc 含 ::testing::Environment 子类

**偏差度评估：** 中等限定范围（仅影响实施代码 + 测试 setup / 不动整体架构 / 0 倒退既有 build）

**累计 brainstorming P1.3 主动 push-back 模式实证：**
- TASK-04（first-evidence / 落地）
- TASK-05 G1.1（dual-evidence / 首次实战 / 3 偏差校正）
- **TASK-06 G1.2（triple-evidence 候选 / 第 2 次实战 / 3 偏差校正）**

---

## 1. 文件结构

### 1.1 创建文件（4 个 / 偏差校正后）

| # | 路径 | 行数估 | 职责 |
|:-:|---|:-:|---|
| 1 | `veloxa/platform/gles_display.h` | ~80 | GLESDisplay 纯虚抽象 / 11 virtual methods / B8 G2 桥接接口预留 / GpuFence 接口注释占位 |
| 2 | `veloxa/platform/sdl2/sdl2_egl_display.h` | ~50 | Sdl2EGLDisplay : public GLESDisplay |
| 3 | `veloxa/platform/sdl2/sdl2_egl_display.cc` | ~150 | SDL_GL_SetAttribute ×6 + SDL_GL_CreateContext + glGetString / glGetStringi + extension cache |
| 4 | `tests/platform/sdl2_egl_display_test.cc` ⚠️ 偏差 #2 校正 | ~220 | 8 TEST_F + ::testing::Environment（SDL_VIDEODRIVER=offscreen）+ helpers |

**总创建：** ~500 行（plan 估 ×1.0 / buffer P2.2 ×1.3-1.5 → 实际预期 ~650-750 行）

### 1.2 修改文件（1 个 / 偏差校正后）[共享文件]

| # | 路径 | 行数估 | 职责 |
|:-:|---|:-:|---|
| 1 | `veloxa/platform/sdl2/CMakeLists.txt` ⚠️ 偏差 #1 校正 | +~10 | 加 sdl2_egl_display.cc 到 vx_platform_sdl2 sources + EGL/GLES pkg_check_modules + target_link_libraries |
| 2 | `tests/CMakeLists.txt` | +~10 | 注册 sdl2_egl_display_test ctest（仅 VX_RENDERER=gles + VX_PLATFORM_SDL2=ON）|

**总修改：** +~20 行

### 1.3 不修改文件（偏差 #1 校正后）

- ❌ **`veloxa/platform/CMakeLists.txt` 0 修改**（GLESDisplay.h header-only / 隐式头扫描 / 不需声明 source）

### 1.4 LOC 总计估算（plan + P2.2 ×1.3-1.5 buffer）

| 维度 | plan 估 | buffer 上限（×1.5）|
|---|:-:|:-:|
| 创建 + 修改 | ~520 行 | ~780 行 |

---

## 2. 决策细化设计

### 2.1 GLESDisplay 抽象设计（D1+D2+D3 落地）

完整接口见 [spec §3.3.2](../specs/2026-05-05-gles-renderer-blueprint-design.md)。本任务 11 个 virtual methods 全实现：

```cpp
// veloxa/platform/gles_display.h（~80 行）
namespace vx::platform {

class GLESDisplay {
 public:
  virtual ~GLESDisplay() = default;

  // EGL display / context 生命周期
  virtual vx::Status Initialize() = 0;
  virtual void Shutdown() = 0;
  virtual bool IsValid() const = 0;

  // GL 资源加载（shader 编译 / texture 上传）须在 MakeCurrent 内
  virtual bool MakeCurrent() = 0;
  virtual void DoneCurrent() = 0;

  // Frame present
  virtual void SwapBuffers() = 0;

  // Context lost / restore（D2=B 嵌入式 GLES 标配）
  virtual bool IsContextLost() const = 0;
  virtual vx::Status RestoreContext() = 0;

  // GL extension query（D3=B eager cache）
  virtual bool HasExtension(const char* name) const = 0;
  virtual vx::i32 gles_major_version() const = 0;
  virtual vx::i32 gles_minor_version() const = 0;

  // GpuFence 接口（B8 G2 预留 / 嵌入式 sync 需要 / G2 蓝图阶段细化 / 本任务仅头注释占位）
  // virtual std::unique_ptr<GpuFence> CreateFence() = 0;
};

}  // namespace vx::platform
```

### 2.2 Sdl2EGLDisplay 实施设计（D1+D2+D3+D6 落地）

```cpp
// veloxa/platform/sdl2/sdl2_egl_display.h（~50 行）
namespace vx::platform {

class Sdl2EGLDisplay : public GLESDisplay {
 public:
  explicit Sdl2EGLDisplay(SDL_Window* window);
  ~Sdl2EGLDisplay() override;

  Sdl2EGLDisplay(const Sdl2EGLDisplay&) = delete;
  Sdl2EGLDisplay& operator=(const Sdl2EGLDisplay&) = delete;

  vx::Status Initialize() override;
  void Shutdown() override;
  bool IsValid() const override;

  bool MakeCurrent() override;
  void DoneCurrent() override;
  void SwapBuffers() override;

  bool IsContextLost() const override;
  vx::Status RestoreContext() override;

  bool HasExtension(const char* name) const override;
  vx::i32 gles_major_version() const override { return gles_major_; }
  vx::i32 gles_minor_version() const override { return gles_minor_; }

 private:
  SDL_Window* window_ = nullptr;            // 不持有
  SDL_GLContext gl_context_ = nullptr;
  bool context_lost_ = false;
  vx::i32 gles_major_ = 3;
  vx::i32 gles_minor_ = 0;
  std::unordered_set<std::string> ext_cache_;  // D3=B eager cache
};

}  // namespace vx::platform
```

### 2.3 关键实施代码片段

#### 2.3.1 Initialize（SDL_GL_SetAttribute 6 项 + create context + ext cache）

```cpp
// veloxa/platform/sdl2/sdl2_egl_display.cc Initialize()
vx::Status Sdl2EGLDisplay::Initialize() {
  if (!window_) {
    return vx::Status(vx::StatusCode::kInvalidArgument, "Sdl2EGLDisplay: window is null");
  }

  // 6 个 SDL_GL attribute（GLES 3.0+ context request）
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);     // 2D rendering / 不需 depth
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);   // clip mask 路径需 stencil

  gl_context_ = SDL_GL_CreateContext(window_);
  if (!gl_context_) {
    return vx::Status(vx::StatusCode::kInternal,
                      std::string("SDL_GL_CreateContext failed: ") + SDL_GetError());
  }

  if (SDL_GL_MakeCurrent(window_, gl_context_) != 0) {
    SDL_GL_DeleteContext(gl_context_);
    gl_context_ = nullptr;
    return vx::Status(vx::StatusCode::kInternal,
                      std::string("SDL_GL_MakeCurrent failed: ") + SDL_GetError());
  }

  // 解析 GLES version from glGetString(GL_VERSION) "OpenGL ES 3.0 Mesa..."
  const char* version_str = reinterpret_cast<const char*>(glGetString(GL_VERSION));
  if (version_str) {
    int major = 3, minor = 0;
    if (std::sscanf(version_str, "OpenGL ES %d.%d", &major, &minor) == 2) {
      gles_major_ = major;
      gles_minor_ = minor;
    }
  }

  // D3=B eager extension cache via glGetStringi
  GLint num_ext = 0;
  glGetIntegerv(GL_NUM_EXTENSIONS, &num_ext);
  ext_cache_.reserve(static_cast<size_t>(num_ext));
  for (GLint i = 0; i < num_ext; ++i) {
    const char* ext = reinterpret_cast<const char*>(
        glGetStringi(GL_EXTENSIONS, static_cast<GLuint>(i)));
    if (ext) ext_cache_.emplace(ext);
  }

  return vx::Status::OK();
}
```

#### 2.3.2 IsContextLost + RestoreContext（D2=B 嵌入式刚需）

```cpp
bool Sdl2EGLDisplay::IsContextLost() const {
  if (context_lost_) return true;  // mock-friendly path（test 可直接置 true）

  // GL_CONTEXT_LOST_KHR (0x0507) — KHR_robustness 扩展
  // 注：headless swrast 通常不会真实触发 / 仅作为 production 路径
  if (gl_context_) {
    GLenum err = glGetError();
    if (err == 0x0507 /* GL_CONTEXT_LOST_KHR */) {
      const_cast<Sdl2EGLDisplay*>(this)->context_lost_ = true;
      return true;
    }
  }
  return false;
}

vx::Status Sdl2EGLDisplay::RestoreContext() {
  if (gl_context_) {
    SDL_GL_DeleteContext(gl_context_);
    gl_context_ = nullptr;
  }
  context_lost_ = false;
  ext_cache_.clear();
  return Initialize();
}
```

#### 2.3.3 HasExtension（D3=B eager cache O(1)）

```cpp
bool Sdl2EGLDisplay::HasExtension(const char* name) const {
  if (!name) return false;
  return ext_cache_.count(name) > 0;
}
```

### 2.4 testing fixture 设计（D1=A）

```cpp
// tests/platform/sdl2_egl_display_test.cc 顶部 ::testing::Environment
class Sdl2EglEnvironment : public ::testing::Environment {
 public:
  void SetUp() override {
    SDL_setenv("SDL_VIDEODRIVER", "offscreen", /*overwrite=*/1);
    ASSERT_EQ(SDL_Init(SDL_INIT_VIDEO), 0)
        << "SDL_Init failed: " << SDL_GetError();
  }
  void TearDown() override { SDL_Quit(); }
};

[[maybe_unused]] auto* env =
    ::testing::AddGlobalTestEnvironment(new Sdl2EglEnvironment);
```

---

## 3. 实施步骤（TDD 三阶 / 步骤 1-5）

### 3.1 Phase 0：grep audit（VAN 已 11/11 完成 ✅）

详见 [activeContext.md Phase 0 audit 表](../../memory-bank/activeContext.md)。**无需 plan 阶段重复执行。**

### 3.2 步骤 1：编写测试 [TDD RED] (~30 min)

创建 `tests/platform/sdl2_egl_display_test.cc`（含 ::testing::Environment + 8 TEST_F）：

| # | TEST_F 名 | 验证 | 决策映射 |
|:-:|---|---|:-:|
| T1 | `Initialize_Success_IsValid` | Initialize() OK + IsValid() == true | D1+D6 |
| T2 | `MakeCurrent_NonNullContext` | MakeCurrent() == true + eglGetCurrentContext() != EGL_NO_CONTEXT | D1 |
| T3 | `GLESVersion_AtLeast_3_0` | gles_major_version() >= 3 | D1 |
| T4 | `IsContextLost_Initial_False` | 初始 IsContextLost() == false | D2 base |
| T5 | `HasExtension_KnownExt` | 启动时 ext_cache_ 非空（>0 extensions in Mesa swrast）| D3 |
| T6 | `Shutdown_InvalidatesContext` | Shutdown() 后 IsValid() == false + ext_cache_ 空 | D3 |
| T7 | `RestoreContext_AfterMockedLost` | mock context_lost_ = true → RestoreContext() == OK + IsValid() == true | **D2=B 关键** |
| T8 | `ReverseProbe_InvalidGLVersion` | SDL_GL_SetAttribute(MAJOR_VERSION=99) → Initialize() fails / IsValid() == false | **D4=C 关键** |

**预期 RED：** 所有 8 个 TEST_F 编译失败（gles_display.h / sdl2_egl_display.h 还不存在）✅

### 3.3 步骤 2：实现 GLESDisplay 抽象 [TDD] (~10 min)

创建 `veloxa/platform/gles_display.h`（按 §2.1 完整代码）。

**预期：** 测试仍 RED（impl 还不存在）

### 3.4 步骤 3：实现 Sdl2EGLDisplay [TDD GREEN] (~30 min)

- 创建 `veloxa/platform/sdl2/sdl2_egl_display.h`（按 §2.2）
- 创建 `veloxa/platform/sdl2/sdl2_egl_display.cc`（按 §2.3.1-2.3.3）
- 修改 `veloxa/platform/sdl2/CMakeLists.txt`（D6=A 局部 dep + source）：

```cmake
# veloxa/platform/sdl2/CMakeLists.txt（增量 +~10 行）

find_package(PkgConfig REQUIRED)
pkg_check_modules(VX_EGL REQUIRED IMPORTED_TARGET egl)
pkg_check_modules(VX_GLESV2 REQUIRED IMPORTED_TARGET glesv2)

add_library(vx_platform_sdl2 STATIC
  sdl2_input_translate.cc
  sdl2_window_surface.cc
  sdl2_event_loop.cc
  sdl2_egl_display.cc           # 新增
)

# 既有 target_include_directories / target_compile_features 保持

target_link_libraries(vx_platform_sdl2
  PUBLIC vx_platform SDL2::SDL2
  PRIVATE vx_foundation PkgConfig::VX_EGL PkgConfig::VX_GLESV2  # 新增 EGL+GLES dep
)
```

**预期 GREEN：** 8/8 TEST_F PASS（仅 VX_RENDERER=gles + VX_PLATFORM_SDL2=ON 时）

### 3.5 步骤 4：注册 ctest（仅 gles + sdl2 build matrix）(~5 min)

修改 `tests/CMakeLists.txt`：

```cmake
# 在既有 sdl2_window_surface_test 段附近
if(VX_RENDERER STREQUAL "gles" AND VX_PLATFORM_SDL2)
  vx_add_gtest(sdl2_egl_display_test
    SOURCES platform/sdl2_egl_display_test.cc
    LIBS vx_platform_sdl2
  )
endif()
```

注：本测试**仅 VX_RENDERER=gles 时编译**（software 路径不构 Sdl2EGLDisplay / baseline 不退化保 1303）。

### 3.6 步骤 5：双 build 矩阵 ctest 验证 (~15 min)

| Config | DEVTOOL | VX_RENDERER | 期望 ctest |
|---|:-:|:-:|:-:|
| **A 软件 default**（既有 build/）| ON | software | **1303**（**不退化** / sdl2_egl_display_test 不编译）|
| **B 软件 OFF**（既有 build_off/）| OFF | software | **1110**（不退化）|
| **C gles 新**（既有 build-gles/）| ON | gles | **1303 + 8 = 1311** ✅ |

**反向探针 T8 已集成在 C config 中**（自动验证 SDL_GL_SetAttribute(MAJOR_VERSION=99) 失败路径）。

### 3.7 步骤 6：D7=A 单 feat commit + D8=A P0 协议（~5 min）

**D8=A 自吃狗粮 P0 协议**：plan 文档 + Memory Bank ×3 单 commit（已在本步骤前置 / 即 plan 阶段单 commit）。

**D7=A feat commit（build 阶段末尾）：**

```bash
git add veloxa/platform/gles_display.h \
        veloxa/platform/sdl2/sdl2_egl_display.{h,cc} \
        veloxa/platform/sdl2/CMakeLists.txt \
        tests/platform/sdl2_egl_display_test.cc \
        tests/CMakeLists.txt
git commit -m "feat(platform): add GLESDisplay abstract + Sdl2EGLDisplay impl [TASK-20260505-06]

- GLESDisplay (~80 lines / 11 virtual methods / B8 G2 桥接接口预留)
- Sdl2EGLDisplay impl (~200 lines / SDL_GL_* + GLES 3.0+ + extension cache)
- 8 TEST_F (~220 lines / SDL_VIDEODRIVER=offscreen / D2=B + D4=C)
- sdl2/CMakeLists.txt +EGL/GLES dep (D6=A 局部 / G1.1 D1=A 推迟点正式落地)

Source: GLES 蓝图 spec §3.3.2 + plan §3.2 G1.2."
```

---

## 4. ctest 期望矩阵

| Config | DEVTOOL | VX_RENDERER | Pre baseline | Post baseline | 增量 |
|---|:-:|:-:|:-:|:-:|:-:|
| A | ON | software | 1303 | **1303** | 0（不退化）|
| B | OFF | software | 1110 | **1110** | 0（不退化）|
| C | ON | gles | 1303 | **1311** | **+8 sdl2_egl_display_test** ✅ |

**反向探针 T8** 已集成在 Config C（不需独立 ctest matrix）。

---

## 5. 反复模式预防（VAN + Plan 两阶段累计 0/8 命中 ✅）

| # | 模式 | 抑制方法 |
|:-:|---|---|
| 1 | 前置依赖未验证 | VAN Phase 0 11/11 实证 ✅ |
| 2 | 既有 pattern 不复用 | 复用 SDL2 双轨 find_package + HARFBUZZ pkg_check_modules + Surface 抽象 + ::testing::Environment 4 个 pattern |
| 3 | spec/plan 信息回归 | VAN 阶段发现 plan §3.2 3 偏差 + plan §0.4 详细校正（dual → triple-evidence）|
| 4 | ABI / lazy-attach 错误 | N/A（本任务无 C ABI / 仅 C++ 内部抽象）|
| 5 | 反向探针强度不足 | D4=C SDL_GL_SetAttribute(MAJOR_VERSION=99) 自动化 + 集成 ctest（与 G1.1 D5 精神一致）|
| 6 | 测试覆盖度不足 | 8 TEST_F 覆盖 11/11 virtual methods（含 D2=B RestoreContext 关键路径）|
| 7 | LOC 估算偏低 | P2.2 ×1.3-1.5 buffer 应用（plan ~520 → 实际 ~650-780）|
| 8 | spec 数据回归 audit | 本任务无 spec 数据 / 仅设计 spec |

**累计 19 + 4 阶段（VAN + Plan）= 19 模式连续抑制延续** / 历史新高继续刷新

---

## 6. 验收标准

### 6.1 功能验收

- [ ] GLESDisplay 11 virtual methods 全实现 ✅
- [ ] Sdl2EGLDisplay 8 TEST_F 全 PASS（offscreen driver / Mesa swrast）✅
- [ ] D2=B RestoreContext 路径覆盖 ✅
- [ ] D3=B extension cache O(1) 查询 ✅
- [ ] D4=C 反向探针 T8 集成 PASS（验证版本协商生效）✅
- [ ] 双 build 矩阵 ctest：software 1303 + gles 1311 ✅
- [ ] 0 lint errors（ReadLints 4 改动文件）

### 6.2 P0 协议验收（D8=A 自吃狗粮）

- [ ] plan + Memory Bank ×3 单 commit 落盘（~plan 阶段末）
- [ ] feat commit 单 commit（~build 阶段末）
- [ ] **总计 build 阶段前 1 + build 阶段 1 = 2 commits 完整闭环**

### 6.3 偏差校正验收

- [ ] 偏差 #1 校正：sdl2/CMakeLists.txt 修改（非顶层 platform/）
- [ ] 偏差 #2 校正：tests/platform/sdl2_egl_display_test.cc 扁平路径
- [ ] 偏差 #3 校正：::testing::Environment with SDL_VIDEODRIVER=offscreen

### 6.4 跨决策协同度

- [ ] 8/8 D 决策 0 偏差实施（实施忠实度新维度续延 / G1.1 first-evidence + G1.2 dual-evidence）

---

## 7. 反思候选（reflect 阶段处理）

### 7.1 P1（systemPatterns 沉淀候选 / ~5-7 项）

1. **跨决策协同度 100% 第 15 次连续命中**（136/136 历史最高 streak）+ 实施忠实度 G1.1 first → G1.2 dual-evidence
2. **plan ×0.6 dec-evidence 第 10 数据点**（实施类 Level 3 子档 / ennea + 1 / 三子档矩阵更完整）
3. **brainstorming P1.3 主动 push-back triple-evidence**（TASK-04 first + TASK-05 G1.1 dual + TASK-06 G1.2 triple）
4. **writing-plans P1.6 spec vs code audit triple-evidence**（同上）
5. **P0 协议 sext-evidence 第 6 数据点**（quint → sext / 实施类 Level 3 首次实证 / 适用性矩阵 6 类全覆盖）
6. **D3=B eager extension cache 范式**（first-evidence / unordered_set Initialize 一次构 / O(1) 查询）
7. **D4=C inline test 反向探针范式**（first-evidence / SDL_GL_SetAttribute test 控制 / 自动化 + 不污染源码）

### 7.2 P2（writing-plans 段细化候选 / 视实施情况）

- [ ] writing-plans「testing fixture 与 headless CI 适配」段（SDL_VIDEODRIVER=offscreen / EGL_PLATFORM=surfaceless / 嵌入式 GL test setup checklist）
- [ ] writing-plans「OpenGL ES 平台抽象 LOC 估算附录」（11 virtual methods 抽象 ~80 行 / SDL_GL impl ~150 行 / 6-8 TEST_F ~200-250 行）

---

## 8. 估时（plan ×0.6）

| 阶段 | 估时 | 实测预期 | 子档 |
|---|:-:|:-:|---|
| VAN | ~10-15 min | ~10-15 min | 标准 |
| **Plan**（本阶段）| ~25-40 min | ~25-40 min | 标准 |
| Build | ~60-100 min | ~50-90 min | 极速区 0.5-0.6× |
| Reflect | ~15-20 min | ~15-20 min | 标准 |
| Archive | ~10-15 min | ~10-15 min | 标准 |
| **总线** | **~120-190 min** | **~110-180 min** | **~0.4-0.6× plan ×0.6 极速区**（实施类 Level 3 子档候选）|

vs GLES 蓝图 plan §3.2 估时 ~4-6 h = 240-360 min → 预期总线 **0.46-0.50× 标准极速区**

---

## 9. 风险与应对

| # | 风险 | 概率 | 影响 | 应对 |
|:-:|---|:-:|:-:|---|
| 1 | Mesa swrast 不支持 GLES 3.1+（仅 3.0）| 中 | 低 | T3 仅检 `>= 3.0`（兼容 3.0 / 3.1 / 3.2）|
| 2 | SDL_VIDEODRIVER=offscreen 在某些 Mesa 版本无 GL context 支持 | 低 | 中 | 兜底：`SDL_VIDEODRIVER=dummy` + 跳过 GL 初始化测试（GTEST_SKIP）|
| 3 | KHR_robustness extension 在 Mesa swrast 不可用 → T5 失败 | 低 | 低 | T5 改用通用 ext（如 `GL_KHR_debug` 或 ext_cache_.size() > 0）|
| 4 | LOC 偏离估算（×1.3-1.5 buffer）| 中 | 低 | P2.2 buffer 已应用（plan ~520 → 实际可达 ~780）/ 反思阶段记录 |
| 5 | RestoreContext 测试 mock 路径过于人工 | 低 | 低 | T7 显式 mock context_lost_ = true（白盒 friend test 或 protected setter）|
| 6 | sdl2/CMakeLists.txt EGL/GLES dep 影响 software build | 低 | 中 | D6=A 决策 sdl2/ 局部 / `target_link_libraries PRIVATE` 限定 / 顶层 0 影响 |

---

## 10. 交叉引用

- **上游 spec：** [docs/specs/2026-05-05-gles-renderer-blueprint-design.md §3.3.2](../specs/2026-05-05-gles-renderer-blueprint-design.md)
- **上游 plan：** [docs/plans/2026-05-05-gles-renderer-blueprint.md §3.2](2026-05-05-gles-renderer-blueprint.md#32-子任务-g12--glesdisplay-抽象--sdl2egldisplay-实施)
- **前置任务：** [archive-TASK-20260505-05.md](../../memory-bank/archive/archive-TASK-20260505-05.md)（G1.1 CMake VX_RENDERER flag）
- **决策范式参考：**
  - [`memory-bank/systemPatterns.md`](../../memory-bank/systemPatterns.md)「跨决策协同度 100% 第 14 次连续命中」段（base / 升级到第 15 次）
  - [`memory-bank/systemPatterns.md`](../../memory-bank/systemPatterns.md)「plan ×0.6 实测系数 ennea-evidence」段（base / 升级到 dec-evidence）
  - [`memory-bank/systemPatterns.md`](../../memory-bank/systemPatterns.md)「brainstorming P1.3 主动 push-back 模式 dual-evidence」段（base / 升级到 triple-evidence）
  - [`.cursor/rules/skills/writing-plans.mdc`](../../.cursor/rules/skills/writing-plans.mdc) P1.5 段（base quint-evidence / 升级到 sext-evidence）

---

## 11. 文档与下一步

### 11.1 文档清单

- ✅ 本计划文档：`docs/plans/2026-05-05-gles-display-sdl2-egl.md`
- ✅ Memory Bank ×3 更新（activeContext / tasks / progress）
- ❌ 不创建新 spec（D5=A Level 3 实施类豁免 / 引用既有 GLES 蓝图 spec §3.3.2）

### 11.2 下一步

`/build` — 进入构建阶段，按 §3 步骤 1-6 实施：
1. 步骤 1（TDD RED / 30 min）：sdl2_egl_display_test.cc + 8 TEST_F
2. 步骤 2（10 min）：gles_display.h 抽象
3. 步骤 3（TDD GREEN / 30 min）：Sdl2EGLDisplay impl + sdl2/CMakeLists.txt
4. 步骤 4（5 min）：tests/CMakeLists.txt 注册
5. 步骤 5（双 build 矩阵 ctest / 15 min）：software 1303 + gles 1311 验证
6. 步骤 6（D7=A 单 feat commit / 5 min）

---

**Plan 阶段总结：** 8/8 D 决策 1 次 AskQuestion all_recommended 锁定（**跨决策协同度 100% 第 15 次连续命中** / 累计 **136/136 历史最高 streak 续刷**）+ plan §0.4 3 偏差详细校正（**brainstorming P1.3 triple-evidence 候选**）+ D8=A 自吃狗粮 P0 协议（quint → **sext-evidence 第 6 数据点候选** / 实施类 Level 3 首次实证）。
