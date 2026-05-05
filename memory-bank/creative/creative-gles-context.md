# Creative — GLES Context 创建策略（B1）

**任务 ID：** TASK-20260505-03
**日期：** 2026-05-05
**状态：** 蓝图（V2=a）
**关联决策：** B1 = SDL_GL_CreateContext + EGL 嵌入式接口预留

---

## 0. 决策上下文

### 0.1 V/B 协同度矩阵

| 已锁定决策 | 影响 |
|---|---|
| V3 desktop_first | 桌面路径优先 SDL_GL_CreateContext / 嵌入式 EGL 直接预留 |
| V4 co_design_boundary | G1 定义 GLESDisplay 抽象 / G2 实现 DrmKmsEGLDisplay 接入 |
| B8 完整预留（ContextLost/Restore + GLESDisplay + GpuFence）| 抽象接口需含 context lost 处理 + extension query + version negotiation |

---

## 1. 三方案对比（VAN 阶段未展开 / creative 阶段补充）

### 1.1 候选 A：SDL_GL_CreateContext + EGL 嵌入式接口预留 ⭐（已选）

**原理：**

桌面路径用 SDL2 的 GL context 包装（SDL_GL_CreateContext）/ 嵌入式路径在 GLESDisplay 抽象上预留 EGL 直接接口（G2 蓝图阶段实现 DrmKmsEGLDisplay）。

**桌面流程：**

```
SDL_Init(SDL_INIT_VIDEO)
  ↓
SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3)
SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0)
SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES)
SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1)
SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24)
SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8)
  ↓
SDL_CreateWindow(title, x, y, w, h, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN)
  ↓
SDL_GLContext ctx = SDL_GL_CreateContext(window)
  ↓
SDL_GL_MakeCurrent(window, ctx)
  ↓
glGetString(GL_VERSION)  // 验证：必须含 "OpenGL ES 3.x"
  ↓
[render loop]
  ↓
SDL_GL_SwapWindow(window)
  ↓
SDL_GL_DeleteContext(ctx) on shutdown
```

**嵌入式预留（G2 蓝图阶段实现）：**

```
EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY)
  ↓
eglInitialize(display, &major, &minor)
  ↓
EGLConfig config;
EGLint config_attrs[] = {
  EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
  EGL_DEPTH_SIZE, 24, EGL_STENCIL_SIZE, 8,
  EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
  EGL_NONE
};
eglChooseConfig(display, config_attrs, &config, 1, &num_configs)
  ↓
EGLContext ctx = eglCreateContext(display, config, EGL_NO_CONTEXT, ctx_attrs)
  ↓
EGLSurface surface = eglCreateWindowSurface(display, config, native_window, nullptr)
  ↓
eglMakeCurrent(display, surface, surface, ctx)
  ↓
[render loop]
  ↓
eglSwapBuffers(display, surface)
```

**优势：**

- ✅ 桌面零额外依赖（SDL2 已接入 / TASK-20260425-01）
- ✅ 嵌入式 EGL 接口预留 / G2 蓝图阶段零 rework
- ✅ 与 V3 desktop_first ✅ / V4 co_design_boundary ✅
- ✅ SDL_GL_CreateContext 内部已封装 EGL/GLX 平台差异 / 桌面跨 OS 兼容免费
- ✅ Mesa Desktop GLES 3.0+ 全支持

**劣势：**

- ⚠️ SDL2 抽象层会损失少量极端 GL extension（罕用）
- ⚠️ 嵌入式 SDL2 build 复杂（DRM/KMS SDL2 backend 不主流 / G2 用 EGL 直接）

**协同度：**

| 已锁定决策 | 协同度 |
|---|:-:|
| V3 desktop_first | ✅ |
| V4 co_design_boundary | ✅ |
| B8 完整预留 | ✅ |

### 1.2 候选 B：仅 SDL_GL_CreateContext

**劣势：**

- ❌ 嵌入式接口预留缺位 → V4 co_design_boundary 冲突
- ❌ G2 DRM/KMS rework 代价大

### 1.3 候选 C：仅 EGL 全平台

**劣势：**

- ❌ 桌面 SDL2 整合代价高（额外 EGL 依赖 + 桌面 EGL 不主流）
- ❌ V3 desktop_first 冲突

---

## 2. SDL_GL_CreateContext 详细流程

### 2.1 6 个 SDL_GL_SetAttribute（构造前必设）

```cpp
// veloxa/platform/sdl2/sdl2_egl_display.cc 构造前置
SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);          // GLES 3.x
SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);          // 3.0 baseline
SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                    SDL_GL_CONTEXT_PROFILE_ES);                 // ES 而非 Core
SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);                    // 双缓冲
SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);                     // depth buffer
SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);                    // PushClipPath 需要 stencil
```

### 2.2 SDL_CreateWindow flag

`SDL_WINDOW_OPENGL` 是必须 / `SDL_WINDOW_SHOWN`（dogfood）/ `SDL_WINDOW_HIDDEN`（headless 测试）。

### 2.3 SDL_GL_CreateContext 错误处理

```cpp
SDL_GLContext ctx = SDL_GL_CreateContext(window);
if (!ctx) {
  VX_LOG_ERROR("SDL_GL_CreateContext failed: %s", SDL_GetError());
  return Status::Internal("SDL_GL_CreateContext failed");
}

// 验证版本（fallback ES 2.0 不支持本任务设计）
const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
if (!version || std::strstr(version, "OpenGL ES 3") == nullptr) {
  VX_LOG_ERROR("Required OpenGL ES 3.0+, got: %s", version ? version : "(null)");
  SDL_GL_DeleteContext(ctx);
  return Status::Internal("OpenGL ES 3.0+ not supported");
}

// 解析 major / minor
GLint major = 0, minor = 0;
glGetIntegerv(GL_MAJOR_VERSION, &major);
glGetIntegerv(GL_MINOR_VERSION, &minor);
gles_major_ = major;
gles_minor_ = minor;
```

### 2.4 SDL_GL_SwapWindow 时序

```cpp
void Sdl2GLWindowSurface::Present() {
  display_->SwapBuffers();  // 内部调 SDL_GL_SwapWindow(window)
}
```

注意：`SDL_GL_SwapWindow` 在 vsync 启用时会阻塞至 vblank（默认 vsync ON）/ 关闭 vsync 用 `SDL_GL_SetSwapInterval(0)`。本任务保持默认 vsync ON。

---

## 3. Context Lost 处理（B8 嵌入式硬性）

### 3.1 触发条件

- **桌面**：罕见 / GPU driver 重启（Win/Mac）或 SDL_RENDERER_RESET 信号
- **嵌入式**：常见 / Android 应用 background→foreground 切换 / DRM master 切换 / GPU 复位

### 3.2 检测机制

```cpp
bool Sdl2EGLDisplay::IsContextLost() const {
  // SDL2 桌面路径
  GLenum err = glGetError();
  if (err == GL_CONTEXT_LOST_KHR) return true;  // GL_KHR_robustness extension

  // SDL2 平台事件路径（更可靠）
  // 实际实现中由 EventLoop 监听 SDL_RENDER_DEVICE_RESET 事件
  return context_lost_flag_;  // 由事件回调置位
}
```

### 3.3 恢复流程

```cpp
Status Sdl2EGLDisplay::RestoreContext() {
  if (!context_lost_) return Status::Ok();

  // 1. 销毁旧 context（部分驱动需要）
  if (gl_context_) {
    SDL_GL_DeleteContext(gl_context_);
    gl_context_ = nullptr;
  }

  // 2. 重建 context
  gl_context_ = SDL_GL_CreateContext(window_);
  if (!gl_context_) {
    return Status::Internal("RestoreContext: SDL_GL_CreateContext failed");
  }

  // 3. MakeCurrent
  if (SDL_GL_MakeCurrent(window_, gl_context_) != 0) {
    return Status::Internal("RestoreContext: SDL_GL_MakeCurrent failed");
  }

  // 4. 通知 GLESCanvas 重建 GL 资源
  // （Application 在 Update() 开始处检测 IsContextLost() → 触发
  //  GLESCanvas::OnContextRestored() 重建 shader / texture）

  context_lost_ = false;
  return Status::Ok();
}
```

### 3.4 GLESCanvas 资源重建协议

```cpp
void GLESCanvas::OnContextLost() {
  // 1. 标记所有 GL handle 失效（不再 glDelete*，driver 已释放）
  solid_shader_.program = 0;
  texture_shader_.program = 0;
  tess_shader_.program = 0;
  quad_vao_ = 0;
  quad_vbo_ = 0;
  dynamic_vao_ = 0;
  dynamic_vbo_ = 0;
  // glyph_atlas_ / image_pool_ 内部 handle 同样失效

  glyph_atlas_->OnContextLost();
  image_pool_->OnContextLost();

  context_valid_ = false;
}

void GLESCanvas::OnContextRestored() {
  // 1. 重建 shader programs（B6 raw string literal / 静态嵌入 / 重新编译）
  CompileShaders();

  // 2. 重建 VAO / VBO
  glGenVertexArrays(1, &quad_vao_);
  glGenBuffers(1, &quad_vbo_);
  glGenVertexArrays(1, &dynamic_vao_);
  glGenBuffers(1, &dynamic_vbo_);

  // 3. 重建 atlas / image pool（CPU side cache 不变 / 仅 upload 重做）
  glyph_atlas_->OnContextRestored();
  image_pool_->OnContextRestored();

  context_valid_ = true;
}
```

### 3.5 Application 集成

```cpp
void Application::Update() {
  // 1. context lost 检测（每帧开头）
#if VX_RENDERER_GLES
  auto* gl_surface = dynamic_cast<platform::Sdl2GLWindowSurface*>(config_.surface);
  if (gl_surface) {
    auto* display = gl_surface->gles_display();
    if (display->IsContextLost()) {
      VX_LOG_WARN("GL context lost, attempting restore");
      auto* gles_canvas = static_cast<gfx::gles::GLESCanvas*>(canvas_.get());
      gles_canvas->OnContextLost();

      Status restore = display->RestoreContext();
      if (!restore.ok()) {
        VX_LOG_ERROR("RestoreContext failed: %s", restore.message().c_str());
        return;  // 该帧跳过
      }
      gles_canvas->OnContextRestored();
    }
  }
#endif

  // ... layout / record / replay ...
}
```

---

## 4. GLES 版本协商

### 4.1 baseline 选择：3.0

- ✅ Mesa 26.0+ 桌面 100% 支持
- ✅ 嵌入式 GPU 主流支持率高（Mali T6+ / Adreno 3xx+ / PowerVR Rogue+ / 2014+ Android 设备）
- ✅ 已含 instancing / VAO / sampler object 等本蓝图依赖的核心特性

### 4.2 检测启用更高版本特性

```cpp
void GLESCanvas::DetectFeatureSet() {
  i32 major = display_->gles_major_version();
  i32 minor = display_->gles_minor_version();

  // 3.1+ 启用 compute shader（未来 GPU SDF 路径）
  has_compute_shader_ = (major == 3 && minor >= 1) || major > 3;

  // 3.2+ 启用 SPIR-V 预编译（B6 候选 c 升级路径）
  has_spirv_ = (major == 3 && minor >= 2) || major > 3;

  // GL_EXT_disjoint_timer_query — perf profiling
  has_timer_query_ = display_->HasExtension("GL_EXT_disjoint_timer_query");

  // GL_KHR_robustness — context lost 检测
  has_robustness_ = display_->HasExtension("GL_KHR_robustness");
}
```

### 4.3 fallback 路径

| 缺失 | 处理 |
|---|---|
| GLES 3.0 | `Sdl2EGLDisplay::Initialize()` 失败 → Application fallback SoftwareCanvas |
| GL_KHR_robustness | context lost 检测降级到 SDL_RENDER_DEVICE_RESET 事件 |
| GL_EXT_disjoint_timer_query | `BM_GLESReplay*` 退化到 CPU side timing（QueryPerformanceCounter / clock_gettime）|

---

## 5. 参考实现（生产级 GLES 引擎）

| 引擎 | context 创建 | 备注 |
|---|---|---|
| Skia | EGL 直接 + Vulkan / Metal 多后端 | 桌面用 GLX / Win32 wgl 自实现 / 不依赖 SDL2 |
| Cairo | 仅 SoftwareCanvas（无 GLES 路径主推）| Veloxa 现状路径 |
| Sciter | EGL + Vulkan / Direct3D | 嵌入式优先 / 与 Veloxa MVP-C 路线最接近 |
| Servo | OpenGL（桌面） + EGL（嵌入式）| Rust / 与 Veloxa 蓝图相似度高 |

**Veloxa 选择理由：** SDL_GL（桌面） + EGL（嵌入式）是 Sciter / Servo 范式的最简化版本，与项目核心目标 #4「嵌入性」**一致**（最小依赖 / 桌面 SDL2 复用 / 嵌入式 EGL 直接）。

---

## 6. 实施任务关联

- **G1.2** GLESDisplay 抽象 + Sdl2EGLDisplay — 本 creative 设计的直接落地任务
- **G1.14** Context Lost / Restore — 本 creative §3 处理流程的实施任务
- **G1.18** G2 接口预留 audit — 验证 GLESDisplay 抽象对 DRM/KMS 接入的充分性

---

## 7. 反向探针候选（实施任务 G1.2 + G1.14）

| 反向探针 | 验证点 | 强度档 |
|---|---|:-:|
| 改 SDL_GL_CONTEXT_MAJOR_VERSION = 1 | 版本协商 fallback 触发 | 合适 |
| 注释掉 `SDL_GL_DeleteContext(ctx)` | RAII / context 泄露 ASAN 触发 | 合适 |
| 注释掉 `OnContextLost()` 中的 `solid_shader_.program = 0` | 重建后旧 handle 还原失败 | 平衡 |
| 强制 `display_->IsContextLost() = true` 一次 | RestoreContext 触发路径 | 合适 |

---

**END OF CREATIVE — GLES Context (B1)**
