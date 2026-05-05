# G1 OpenGL ES 硬件渲染后端蓝图 — 设计规格

**日期：** 2026-05-05
**任务 ID：** TASK-20260505-03
**状态：** 设计中（V2=a 纯蓝图任务 / 不含 build 实施）
**复杂度：** Level 4 多 Phase 蓝图
**MVP 档次：** MVP-C 核心（项目核心目标 #2「嵌入式硬件加速」第一刚需）

---

## 0. 上游依赖与文档定位

| 文档 | 关系 | 引用要点 |
|---|---|---|
| [`projectbrief.md`](../../memory-bank/projectbrief.md) | 核心目标 #2 来源 | 「嵌入式硬件加速 60fps 复杂界面」 |
| [`docs/specs/2026-05-04-mvp-scope.md`](../specs/2026-05-04-mvp-scope.md) §3.3 + §11.2 #5 | 立项依据 | C-G1 强阻塞 P0 / 估时 ~30-60+ h plan ×0.6 / Level 4 多 Phase |
| [`docs/specs/2026-04-05-graphics-platform-hal-design.md`](../specs/2026-04-05-graphics-platform-hal-design.md) | 上游基础 | Canvas / Path / Surface 纯虚抽象 / SoftwareCanvas 范式 |
| [`memory-bank/archive/archive-TASK-20260430-04.md`](../../memory-bank/archive/archive-TASK-20260430-04.md) | 工作流范式 | DevTool 蓝图任务 V2=a 范式（spec + plan + creative ×N）|
| [`memory-bank/archive/archive-TASK-20260504-01.md`](../../memory-bank/archive/archive-TASK-20260504-01.md) | 工作流范式 | MVP-scope 蓝图任务 V2=a 范式（同源 13 决策跳过 0 重审） |

---

## 1. 目标与非目标

### 1.1 目标（In-Scope）

1. **新增 `GLESCanvas`** 实现 `gfx::Canvas` 22 方法纯虚接口（FillRect / FillRoundedRect / FillPath / Stroke* / DrawText / DrawImage / Clip / Layer / Transform）
2. **新增 GLES 平台 Surface 适配层** —
   - `Sdl2GLWindowSurface`（桌面 SDL2 + EGL/GLX context）
   - `GLESDisplay` 抽象（EGL Display 抽象 / DRM/KMS 接口预留 / G2 共享）
3. **CMake `VX_RENDERER=software\|gles` flag** — 编译期分支 / SoftwareCanvas 作为 fallback
4. **Application::canvas_ 构造分支** — 根据 `VX_RENDERER` 选 SoftwareCanvas / GLESCanvas
5. **PaintCommand → GL 翻译** —
   - shader-based: FillRect / FillRoundedRect / Solid Brush
   - tessellator-based: FillPath / Complex Path（libtess2）
   - texture-based: DrawImage / DrawText（GlyphCache atlas → GL_R8）
6. **GPU dirty rect 优化** — 沿用 `ComputeDirtyRect` + `glScissor` + `glClear`
7. **shader 资源静态嵌入** — `.glsl` raw string literal / 编译期绑定
8. **G1 → G2 桥接接口预留** — `Surface::ContextLost()/Restore()` 虚接口 + `GLESDisplay` 抽象 + `GpuFence` 接口
9. **性能基线协议** — 新建 `BM_GLESReplay*` + 既有 `BM_Replay*` 同 corpus 双测对照 / 60fps 1080p budget 验收
10. **N 个 Level 3 实施子任务拆分** — 用户后续基于本蓝图独立立项

### 1.2 非目标（Out-of-Scope，明示）

- ❌ **不做 build 实施** — V2=a 纯蓝图变体 / 主交付 = spec + plan + creative ×N
- ❌ **Vulkan 后端不在本任务范围** — 仅在 Renderer abstraction 层预留接口位置（无实际设计）
- ❌ **DRM/KMS 完整设计不在本任务范围** — V4 co_design_boundary / G1 仅做接口预留 / G2 独立蓝图（C-G2）
- ❌ **WebGL / WebGPU 移植不在本任务范围** — 当前无 Web 平台目标
- ❌ **现有 SoftwareCanvas 移除不在本任务范围** — V5 vx_renderer_flag fallback 共存
- ❌ **glyph SDF / msdf-atlas-gen 离线工具链不在本任务范围** — B3 锁定 CPU 光栅化 + GPU atlas（沿用 FreeType）

---

## 2. 设计决策矩阵（V1-V5 + B1-B8 全锁定）

### 2.1 V 系列（VAN 阶段 5/5 锁定 — 跨决策协同度 100% 第 11 次）

| # | 决策 | 选择 | 协同度 / 根因 |
|:-:|---|---|---|
| **V1** | 范围 | **GLES only** | 与 spec §3.2 C.2 表述一致 / 集中火力 / Vulkan 仅预留接口位置 |
| **V2** | 蓝图深度 | **pure_blueprint_a** | V2=a 纯蓝图 / 不含 build / 沿用 TASK-20260430-04 + TASK-20260504-01 范式 |
| **V3** | 平台覆盖 | **desktop_first** | 桌面 SDL2+EGL/GLX 完整 + 嵌入式抽象接口预留（DRM/KMS 详设留 G2）|
| **V4** | G2 关联 | **co_design_boundary** | G1 定义 Renderer/Surface 抽象 / G2 独立蓝图 / 划界协同 |
| **V5** | SoftwareCanvas 共存 | **vx_renderer_flag** | VX_RENDERER=software\|gles CMake flag / SW 作 fallback |

### 2.2 B 系列（plan brainstorm 阶段 8/8 锁定 — 跨决策协同度 100% 第 12 次 / doudec-evidence 候选）

| # | 决策 | 选择 | 协同度（V/B 矩阵） / 根因 |
|:-:|---|---|---|
| **B1** | GL context 创建路径 | **SDL_GL_CreateContext + EGL 嵌入式接口预留** | 与 V3 desktop_first ✅ / 与 V4 co_design_boundary ✅ / 桌面 SDL2 复用 + 嵌入式 EGL 直接 |
| **B2** | Canvas 翻译策略 | **混合（shader + tessellator）** | 与 V1 GLES only ✅ / 简单形状极速 GPU + 复杂路径渐进 / FillRect/RoundedRect = shader / FillPath = libtess2 + VBO |
| **B3** | glyph 渲染 | **CPU 光栅化 + GPU texture atlas** | 与既有 SoftwareCanvas glyph 路径 ✅ / 复用 FreeType + GlyphCache / GL_R8 atlas / 风险最低 |
| **B4** | dirty rect GPU 化 | **ComputeDirtyRect + glScissor + glClear** | 与 V5 vx_renderer_flag ✅ / 复用既有 r3 dirty rect / 最小破坏 |
| **B5** | VX_RENDERER 默认值 | **software**（兼容性优先）| 与既有 1302 ctest baseline ✅ / 与 V5 fallback 语义一致 / GLES opt-in |
| **B6** | shader 资源管理 | **静态嵌入 .glsl raw string literal** | 与既有 inspector_panel.html inline_resources 范式 ✅ / 编译期绑定 / 零部署依赖 / 与项目核心目标 #4 嵌入性协同 |
| **B7** | 性能验收基线 | **既有 BM_Replay* + 新建 BM_GLESReplay* 同 corpus 双测对照** | 与 V5 vx_renderer_flag 双 build ✅ / replay-deepbench 范式复用 / 60fps 1080p budget |
| **B8** | G2 边界预留 | **完整预留**（ContextLost/Restore + GLESDisplay + GpuFence）| 与 V4 co_design_boundary ✅ / G2 DRM/KMS 可零 rework 接入 |

---

## 3. 架构设计

### 3.1 整体结构（V/B 决策落地后）

```
┌──────────────────────────────────────────────────────────────────────┐
│ 上层消费者（render::Replay / Application::Update / DevTool）       │
└──────────────────────────────────────────────────────────────────────┘
                                  │
                                  ▼
┌──────────────────────────────────────────────────────────────────────┐
│ Graphics HAL (vx::gfx)                                               │
│   gfx::Canvas (纯虚 / 22 方法)                                       │
│   ├── SoftwareCanvas (既有 / VX_RENDERER=software 时构造)           │
│   └── ★ GLESCanvas (新增 / VX_RENDERER=gles 时构造)                  │
│       ├── ShaderProgram cache (vert + frag)                          │
│       ├── VAO / VBO / EBO pool                                       │
│       ├── GlyphAtlas (GL_R8 texture atlas)                           │
│       ├── ImageTexturePool (GL_RGBA8 texture cache)                  │
│       └── ClipStack / LayerStack / TransformStack (CPU shadow)       │
└──────────────────────────────────────────────────────────────────────┘
                                  │
                                  ▼
┌──────────────────────────────────────────────────────────────────────┐
│ Platform HAL (vx::platform)                                          │
│   platform::Surface (纯虚 + 新增 GL 扩展)                            │
│   ├── MemorySurface (既有 / SoftwareCanvas)                          │
│   ├── Sdl2WindowSurface (既有 / SoftwareCanvas + SDL_Texture present)│
│   └── ★ Sdl2GLWindowSurface (新增 / GLESCanvas + SDL_GL_SwapWindow) │
│   ★ platform::GLESDisplay (新增抽象 / EGL Display 抽象 / G2 共享)   │
│   ├── Sdl2EGLDisplay (新增 / SDL2 隐式 display)                     │
│   └── (G2) DrmKmsEGLDisplay (G2 蓝图 / 接口预留)                    │
└──────────────────────────────────────────────────────────────────────┘
                                  │
                                  ▼
┌──────────────────────────────────────────────────────────────────────┐
│ Foundation (既有 / status / vector / string_view)                    │
└──────────────────────────────────────────────────────────────────────┘
```

### 3.2 数据流：从 PaintCommand 到 GL 命令

```
PaintCommand DisplayList   (既有 vx::Vector<PaintCommand>)
        │
        ▼
render::Replay(list, GLESCanvas*, image_cache)   (既有签名 / 零修改)
        │
        ▼
GLESCanvas::FillRect(rect, brush)
GLESCanvas::FillRoundedRect(rect, radius, brush)
GLESCanvas::FillPath(path, brush)
GLESCanvas::DrawText(text, bounds, font_size, brush)
GLESCanvas::DrawImage(image, src, dst)
GLESCanvas::PushClipRect / PopClip / PushLayer / PopLayer
        │
        ▼
GLESCanvas 内部分发：
        ├── 简单形状 (FillRect / FillRoundedRect) → ShaderProgram + VBO + glDrawArrays
        ├── 复杂路径 (FillPath)      → libtess2 → VBO → glDrawElements
        ├── 文字 (DrawText)          → GlyphCache → GlyphAtlas (GL_R8) → quad VBO + sampler
        ├── 图片 (DrawImage)         → ImageTexturePool → quad VBO + GL_RGBA8 sampler
        ├── 裁剪 (PushClipRect)      → glScissor 栈 / 复杂裁剪 → stencil buffer
        └── 透明合成 (PushLayer)     → 临时 FBO + 子树绘制 + glBlendFunc 合成
        │
        ▼
glDrawArrays / glDrawElements + GL state machine
        │
        ▼
backbuffer ──glScissor (dirty rect)──► [DirtyRect 范围] ──► front buffer
        │                                                          │
        └────────────── eglSwapBuffers / SDL_GL_SwapWindow ─────────┘
        │
        ▼
Surface::Present()  (既有签名 / Surface 内部分发到 SDL_GL_SwapWindow)
```

### 3.3 GLES 后端关键组件

#### 3.3.1 `GLESCanvas`（核心组件）

```cpp
namespace vx::gfx::gles {

class GLESCanvas : public Canvas {
 public:
  GLESCanvas(GLESDisplay* display, u32 width, u32 height,
             text::FontManager* font_manager = nullptr,
             text::GlyphCache* glyph_cache = nullptr);
  ~GLESCanvas() override;

  // 全 22 方法 override（与 SoftwareCanvas 一一对应）
  void Begin() override;         // bind FBO + 清 state
  void End() override;            // flush + unbind
  void Clear(Color color) override;

  void FillRect(const Rect& rect, const Brush& brush) override;
  void FillRoundedRect(const Rect& rect, f32 radius,
                       const Brush& brush) override;
  void FillPath(const Path& path, const Brush& brush) override;
  void StrokeRect(const Rect& rect, const Brush& brush, f32 width) override;
  void StrokeRoundedRect(const Rect& rect, f32 radius,
                         const Brush& brush, f32 width) override;
  void StrokePath(const Path& path, const Brush& brush, f32 width) override;
  void StrokeLine(Point a, Point b, const Brush& brush, f32 width) override;

  void DrawText(StringView text, const Rect& bounds,
                f32 font_size, const Brush& brush) override;
  void DrawImage(const Image& image, const Rect& src_rect,
                 const Rect& dst_rect) override;

  void PushClipRect(const Rect& rect) override;
  void PushClipPath(const Path& path) override;
  void PopClip() override;

  void PushLayer(const Rect& bounds, f32 opacity) override;
  void PopLayer() override;

  void SetTransform(const Matrix3x2& m) override;
  Matrix3x2 GetTransform() const override;
  void PushState() override;
  void PopState() override;

  std::unique_ptr<Path> CreatePath() override;

  // 新增 GLES 特有 API（非接口 / 内部 + DevTool）
  void OnContextLost();   // B8 G2 接口预留 — 释放 GL 资源
  void OnContextRestored();  // 重建 shader / texture
  bool IsContextValid() const;

 private:
  // ShaderProgram cache
  struct ShaderProgram {
    GLuint program;
    std::unordered_map<std::string, GLint> uniforms;
  };
  ShaderProgram solid_shader_;       // FillRect / FillRoundedRect / Stroke 简单形状
  ShaderProgram texture_shader_;     // DrawImage / glyph
  ShaderProgram tess_shader_;        // FillPath（tessellated）

  // VAO / VBO 池
  GLuint quad_vao_, quad_vbo_;        // 单 quad 复用
  GLuint dynamic_vao_, dynamic_vbo_;  // FillPath / Path tessellation 输出

  // 资源池
  std::unique_ptr<GlyphAtlas> glyph_atlas_;       // CPU FreeType + GL_R8 atlas
  std::unique_ptr<ImageTexturePool> image_pool_;  // GL_RGBA8 texture cache

  // CPU shadow state（GLES state 黑盒，避免 redundant set）
  Vector<Matrix3x2> transform_stack_;
  Vector<Rect> clip_rect_stack_;
  Vector<LayerFrame> layer_stack_;

  // GL 资源管理
  GLESDisplay* display_;            // 不持有 / Application 持有
  GLuint framebuffer_;              // 主 FBO（display 关联）
  u32 width_, height_;
  bool context_valid_ = true;

  // 性能监控（B7 验收依赖）
  u64 draw_calls_ = 0;
  u64 state_changes_ = 0;
};

}  // namespace vx::gfx::gles
```

#### 3.3.2 `GLESDisplay`（B8 G2 桥接接口）

```cpp
namespace vx::platform {

// EGL Display 抽象 — 桌面 SDL2 + 嵌入式 DRM/KMS 共享
// G2 实现 DrmKmsEGLDisplay 接入此抽象 / 零 rework
class GLESDisplay {
 public:
  virtual ~GLESDisplay() = default;

  // EGL display / context 生命周期
  virtual Status Initialize() = 0;
  virtual void Shutdown() = 0;
  virtual bool IsValid() const = 0;

  // GL 资源加载（shader 编译 / texture 上传）须在 MakeCurrent 内
  virtual bool MakeCurrent() = 0;
  virtual void DoneCurrent() = 0;

  // Frame present
  virtual void SwapBuffers() = 0;

  // Context lost / restore（嵌入式 GLES 标配）
  virtual bool IsContextLost() const = 0;
  virtual Status RestoreContext() = 0;

  // GL extension query
  virtual bool HasExtension(const char* name) const = 0;
  virtual i32 gles_major_version() const = 0;
  virtual i32 gles_minor_version() const = 0;

  // GpuFence 接口（B8 G2 预留 / 嵌入式 sync 需要）
  // virtual std::unique_ptr<GpuFence> CreateFence() = 0;
  // (G2 蓝图阶段细化 / 本任务仅头注释占位)
};

class Sdl2EGLDisplay : public GLESDisplay {
 public:
  Sdl2EGLDisplay(SDL_Window* window);
  ~Sdl2EGLDisplay() override;

  Status Initialize() override;
  void Shutdown() override;
  bool IsValid() const override;

  bool MakeCurrent() override;
  void DoneCurrent() override;
  void SwapBuffers() override;

  bool IsContextLost() const override;
  Status RestoreContext() override;

  bool HasExtension(const char* name) const override;
  i32 gles_major_version() const override;
  i32 gles_minor_version() const override;

 private:
  SDL_Window* window_ = nullptr;        // 不持有
  SDL_GLContext gl_context_ = nullptr;
  bool context_lost_ = false;
  i32 gles_major_ = 3, gles_minor_ = 0;
};

// G2 蓝图（接口预留 / 本任务不实现）
// class DrmKmsEGLDisplay : public GLESDisplay { ... };

}  // namespace vx::platform
```

#### 3.3.3 `Sdl2GLWindowSurface`（GLES Window Surface）

```cpp
namespace vx::platform {

class Sdl2GLWindowSurface : public Surface {
 public:
  Sdl2GLWindowSurface(u32 width, u32 height, const char* title);
  ~Sdl2GLWindowSurface() override;

  // Surface 接口（GLES 路径 Lock/Unlock 是 no-op）
  u32 width() const override { return width_; }
  u32 height() const override { return height_; }
  u32 stride() const override { return 0; }  // GLES 不暴露 stride
  u32* Lock() override { return nullptr; }   // GLES 路径 no-op
  void Unlock() override {}                  // no-op
  void Resize(u32 width, u32 height) override;
  Status SavePPM(const char* path) const override;  // glReadPixels 实现

  void Present() override;  // SDL_GL_SwapWindow

  // 新增 GL-specific
  GLESDisplay* gles_display() { return display_.get(); }

 private:
  SDL_Window* window_ = nullptr;
  std::unique_ptr<Sdl2EGLDisplay> display_;
  u32 width_, height_;
};

}  // namespace vx::platform
```

#### 3.3.4 `GlyphAtlas`（B3 GPU glyph 渲染）

```cpp
namespace vx::gfx::gles {

// GPU glyph atlas — CPU FreeType 光栅化 + GL_R8 atlas upload
// 复用既有 text::GlyphCache CPU bitmap / 仅新增 GL upload 路径
class GlyphAtlas {
 public:
  GlyphAtlas(text::FontManager* font_manager, text::GlyphCache* glyph_cache,
             u32 atlas_width = 1024, u32 atlas_height = 1024);
  ~GlyphAtlas();

  // 获取 glyph atlas 子区域 UV 坐标（自动 fallback CPU 光栅 + GL upload）
  struct GlyphInfo {
    f32 u0, v0, u1, v1;     // atlas UV
    i32 bearing_x, bearing_y;
    i32 width, height;
    f32 advance;
  };
  GlyphInfo GetOrUpload(text::FontHandle font, u32 codepoint, f32 size);

  GLuint texture_id() const { return texture_; }
  u32 atlas_width() const { return atlas_width_; }
  u32 atlas_height() const { return atlas_height_; }

  // 上传时机控制（B7 perf 监控）
  u64 uploads_this_frame() const { return uploads_this_frame_; }
  void ResetFrameCounters();

 private:
  // Atlas packing — Skyline / Shelf / 简单 row-pack
  bool PackGlyph(u32 width, u32 height, u32* out_x, u32* out_y);

  text::FontManager* font_manager_ = nullptr;  // 不持有
  text::GlyphCache* glyph_cache_ = nullptr;    // 不持有
  GLuint texture_ = 0;
  u32 atlas_width_, atlas_height_;
  std::unordered_map<u64, GlyphInfo> cache_;  // key = font_id << 32 | codepoint << 16 | size_pt
  u32 cursor_x_ = 0, cursor_y_ = 0, row_height_ = 0;  // 简单 row-pack
  u64 uploads_this_frame_ = 0;
};

}  // namespace vx::gfx::gles
```

### 3.4 shader 资源（B6 静态嵌入）

shader 静态嵌入到 `veloxa/graphics/gles/shaders.h`：

```cpp
namespace vx::gfx::gles::shaders {

// FillRect / FillRoundedRect / Stroke 共享 vertex shader
constexpr const char* kSolidVert = R"(#version 300 es
precision highp float;
layout(location = 0) in vec2 a_pos;
uniform mat4 u_proj;          // ortho 2D
uniform mat3 u_xform;         // 2x3 仿射
void main() {
  vec3 xy = u_xform * vec3(a_pos, 1.0);
  gl_Position = u_proj * vec4(xy.xy, 0.0, 1.0);
}
)";

// FillRect 简单 fragment shader
constexpr const char* kSolidFrag = R"(#version 300 es
precision mediump float;
uniform vec4 u_color;
out vec4 frag_color;
void main() {
  frag_color = u_color;
}
)";

// FillRoundedRect SDF fragment shader
constexpr const char* kRoundedRectFrag = R"(#version 300 es
precision mediump float;
in vec2 v_uv;          // 0..1 within rect
uniform vec2 u_size;   // rect width, height
uniform float u_radius;
uniform vec4 u_color;
out vec4 frag_color;
float roundedBoxSDF(vec2 p, vec2 b, float r) {
  vec2 q = abs(p) - b + vec2(r);
  return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}
void main() {
  vec2 p = (v_uv - 0.5) * u_size;
  float d = roundedBoxSDF(p, u_size * 0.5, u_radius);
  float a = smoothstep(1.0, -1.0, d);
  frag_color = vec4(u_color.rgb, u_color.a * a);
}
)";

// Texture sample（DrawImage / glyph）
constexpr const char* kTextureVert = R"(...)";
constexpr const char* kTextureFrag = R"(...)";
constexpr const char* kGlyphFrag = R"(...)";

}  // namespace vx::gfx::gles::shaders
```

### 3.5 CMake `VX_RENDERER` flag（B5 software 默认）

```cmake
# 顶层 CMakeLists.txt
option(VX_RENDERER "Renderer backend (software|gles)" "software")

if(NOT VX_RENDERER MATCHES "^(software|gles)$")
  message(FATAL_ERROR "VX_RENDERER must be 'software' or 'gles', got: ${VX_RENDERER}")
endif()

if(VX_RENDERER STREQUAL "gles")
  find_package(OpenGLES REQUIRED)        # /usr/include/GLES3 + libGLESv2
  find_package(EGL REQUIRED)              # /usr/include/EGL + libEGL
  add_compile_definitions(VX_RENDERER_GLES=1)
else()
  add_compile_definitions(VX_RENDERER_SOFTWARE=1)
endif()

# veloxa/graphics/CMakeLists.txt
add_library(vx_graphics ...)

if(VX_RENDERER STREQUAL "gles")
  target_sources(vx_graphics PRIVATE
    gles/gles_canvas.cc
    gles/gles_canvas.h
    gles/glyph_atlas.cc
    gles/glyph_atlas.h
    gles/image_texture_pool.cc
    gles/image_texture_pool.h
    gles/shaders.h
  )
  target_link_libraries(vx_graphics PUBLIC OpenGLES::GLESv2 EGL::EGL)
  # libtess2 用于 FillPath 复杂路径 tessellation
  FetchContent_Declare(libtess2 GIT_REPOSITORY ... GIT_TAG ...)
  FetchContent_MakeAvailable(libtess2)
  target_link_libraries(vx_graphics PRIVATE tess2)
endif()
```

### 3.6 Application 构造分支（B5 落地）

```cpp
// veloxa/core/application.cc
Application::Application(const Config& config) : config_(config) {
  text_shaper_ = std::make_unique<layout::SimpleTextShaper>();
  font_manager_.Init();
  if (config_.surface) {
#if VX_RENDERER_GLES
    // GLES 路径 — 不需要 surface_pixels_，直接构造 GLESCanvas
    auto* gl_surface = dynamic_cast<platform::Sdl2GLWindowSurface*>(config_.surface);
    if (!gl_surface) {
      VX_LOG_ERROR("VX_RENDERER=gles requires Sdl2GLWindowSurface, got %s",
                   typeid(*config_.surface).name());
      // fallback 到 SoftwareCanvas（B5 兼容路径）
      surface_pixels_ = config_.surface->Lock();
      canvas_ = std::make_unique<gfx::sw::SoftwareCanvas>(...);
    } else {
      auto* gles_display = gl_surface->gles_display();
      canvas_ = std::make_unique<gfx::gles::GLESCanvas>(
          gles_display, gl_surface->width(), gl_surface->height(),
          &font_manager_, &glyph_cache_);
    }
#else
    surface_pixels_ = config_.surface->Lock();
    canvas_ = std::make_unique<gfx::sw::SoftwareCanvas>(
        surface_pixels_, config_.surface->width(), config_.surface->height(),
        config_.surface->stride(), &font_manager_, &glyph_cache_);
#endif
    canvas_->Begin();
    canvas_->Clear(config_.background_color);
  }
  // ...
}
```

---

## 4. PaintCommand → GL 翻译细节

### 4.1 简单形状（FillRect / FillRoundedRect / Stroke 简单形状）

```cpp
void GLESCanvas::FillRect(const Rect& rect, const Brush& brush) {
  if (brush.kind != Brush::Kind::kSolid) {
    // LinearGradient / RadialGradient → 转 SDF shader 路径
    FillRectGradient(rect, brush);
    return;
  }

  // 1. bind solid_shader_
  glUseProgram(solid_shader_.program);

  // 2. set uniforms
  Matrix3x2 xform = TopTransform();
  glUniformMatrix3fv(solid_shader_.uniforms["u_xform"], 1, GL_FALSE, xform.m);
  glUniform4f(solid_shader_.uniforms["u_color"],
              brush.solid.color.r / 255.0f, brush.solid.color.g / 255.0f,
              brush.solid.color.b / 255.0f, brush.solid.color.a / 255.0f);

  // 3. update VBO（4 顶点 quad）
  f32 verts[] = {rect.x, rect.y, rect.x + rect.w, rect.y,
                 rect.x + rect.w, rect.y + rect.h, rect.x, rect.y + rect.h};
  glBindBuffer(GL_ARRAY_BUFFER, quad_vbo_);
  glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

  // 4. draw
  glBindVertexArray(quad_vao_);
  glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
  ++draw_calls_;
}
```

### 4.2 复杂路径（FillPath via libtess2 → VBO）

```cpp
void GLESCanvas::FillPath(const Path& path, const Brush& brush) {
  // 1. CPU tessellation（libtess2 输出三角形列表）
  TESStesselator* tess = tessNewTess(nullptr);
  // path 是 SoftwarePath（CPU 边列表）/ 抽取 contour
  for (const auto& contour : SafeCastSoftwarePath(path).contours()) {
    tessAddContour(tess, 2, contour.data(), sizeof(f32) * 2, contour.size() / 2);
  }
  if (!tessTesselate(tess, TESS_WINDING_NONZERO, TESS_POLYGONS, 3, 2, nullptr)) {
    tessDeleteTess(tess);
    return;
  }
  const f32* verts = tessGetVertices(tess);
  const i32* indices = tessGetElements(tess);
  i32 vert_count = tessGetVertexCount(tess);
  i32 elem_count = tessGetElementCount(tess);

  // 2. upload to dynamic VBO + EBO
  glBindBuffer(GL_ARRAY_BUFFER, dynamic_vbo_);
  glBufferData(GL_ARRAY_BUFFER, vert_count * 2 * sizeof(f32), verts, GL_STREAM_DRAW);

  // 3. draw with solid_shader_（复用）
  glUseProgram(solid_shader_.program);
  // ... uniforms set 同 FillRect ...
  glBindVertexArray(dynamic_vao_);
  glDrawElements(GL_TRIANGLES, elem_count * 3, GL_UNSIGNED_INT, indices);

  tessDeleteTess(tess);
  ++draw_calls_;
}
```

### 4.3 文字（DrawText via GlyphAtlas）

```cpp
void GLESCanvas::DrawText(StringView text, const Rect& bounds,
                          f32 font_size, const Brush& brush) {
  if (!glyph_atlas_ || !font_manager_) return;

  // 1. shape via TextShaper（既有）→ glyph runs
  layout::SimpleTextShaper shaper;  // 或 FreeTypeTextShaper
  auto runs = shaper.Shape(text, font_size);

  // 2. for each glyph：GetOrUpload → quad VBO
  Vector<f32> verts;  // (x, y, u, v) per vertex
  for (const auto& glyph : runs.glyphs) {
    GlyphAtlas::GlyphInfo info = glyph_atlas_->GetOrUpload(
        glyph.font, glyph.codepoint, font_size);
    f32 x0 = bounds.x + glyph.x_offset + info.bearing_x;
    f32 y0 = bounds.y + bounds.h - info.bearing_y;
    f32 x1 = x0 + info.width;
    f32 y1 = y0 + info.height;
    // 4 顶点 quad
    verts.push_back(x0); verts.push_back(y0); verts.push_back(info.u0); verts.push_back(info.v0);
    verts.push_back(x1); verts.push_back(y0); verts.push_back(info.u1); verts.push_back(info.v0);
    verts.push_back(x1); verts.push_back(y1); verts.push_back(info.u1); verts.push_back(info.v1);
    verts.push_back(x0); verts.push_back(y1); verts.push_back(info.u0); verts.push_back(info.v1);
  }

  // 3. bind glyph_shader + atlas texture
  glUseProgram(glyph_shader_.program);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, glyph_atlas_->texture_id());
  glUniform1i(glyph_shader_.uniforms["u_atlas"], 0);
  glUniform4f(glyph_shader_.uniforms["u_color"], ...);

  // 4. draw
  glBindBuffer(GL_ARRAY_BUFFER, dynamic_vbo_);
  glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(f32),
               verts.data(), GL_STREAM_DRAW);
  glDrawArrays(GL_TRIANGLES, 0, verts.size() / 4);
  ++draw_calls_;
}
```

### 4.4 dirty rect（B4 glScissor 优化）

```cpp
// veloxa/core/application.cc 渲染循环
void Application::Update() {
  // ... layout / record ...
  Rect dirty = render::ComputeDirtyRect(prev_list_, new_list_, width, height);

  if (canvas_) {
    canvas_->Begin();

#if VX_RENDERER_GLES
    // GLES 路径 — glScissor + glClear 限定 dirty region
    auto* gles = static_cast<gfx::gles::GLESCanvas*>(canvas_.get());
    glEnable(GL_SCISSOR_TEST);
    glScissor((GLint)dirty.x, (GLint)(height - dirty.y - dirty.h),
              (GLsizei)dirty.w, (GLsizei)dirty.h);
    glClear(GL_COLOR_BUFFER_BIT);
#else
    canvas_->Clear(background_color_);  // SoftwareCanvas 整面 clear
#endif

    render::Replay(new_list_, canvas_.get(), &image_cache_);
    canvas_->End();

#if VX_RENDERER_GLES
    glDisable(GL_SCISSOR_TEST);
#endif
  }

  if (config_.surface) config_.surface->Present();
}
```

---

## 5. G1 → G2 桥接接口（B8 完整预留）

**G2 DRM/KMS 蓝图（独立 task）** 仅需实现：

1. `class DrmKmsEGLDisplay : public platform::GLESDisplay {...}` — DRM/KMS 端 EGL 实例
2. `class DrmKmsGLWindowSurface : public platform::Surface {...}` — DRM/KMS 端 Surface
3. CMake `VX_PLATFORM_DRM=ON` flag 使能 DRM/KMS 路径

G1 已预留：

- `GLESDisplay` 抽象接口（`MakeCurrent` / `SwapBuffers` / `IsContextLost` / `RestoreContext` / `HasExtension`）
- `Surface::Present()` 虚函数（GLES 路径分发到 SwapBuffers）
- `GLESCanvas::OnContextLost/Restored` 接口（context lost 触发 → 释放/重建 GL 资源）
- `GpuFence` 接口（头注释占位 / G2 蓝图阶段填充）

---

## 6. 错误处理 + 安全考量

### 6.1 错误处理

| 错误场景 | 处理策略 | 验收 |
|---|---|---|
| EGL display 创建失败 | `GLESDisplay::Initialize()` 返回 `Status::Internal`，`Application::Application()` 检测后 fallback 到 SoftwareCanvas | unit test + dogfood smoke |
| Shader 编译失败 | `glCompileShader` 后 `GL_COMPILE_STATUS` 检查，失败时 log + 整个 GLESCanvas 标记 invalid + fallback 到 SoftwareCanvas | unit test：故意错 shader → fallback 触发 |
| Context lost（GL_CONTEXT_LOST 或 SDL 通知）| `GLESDisplay::IsContextLost() = true` → `RestoreContext()` 重建 + GLESCanvas::OnContextRestored 重建资源 | integration test：模拟 context lost（call `glGetError() == GL_CONTEXT_LOST_KHR`）|
| Atlas 满（GlyphCache evict）| evict LRU + 重 upload；最坏退化到 per-frame 上传 | unit test：fill atlas 至 95% → evict 验证 |
| GL_OUT_OF_MEMORY | log + degrade（drop 当前 paint command + 保留 prev frame）| 真机难复现 / log + fallback 即可 |

### 6.2 安全考量（[安全相关] tag）

| 风险 | 缓解 | 优先级 |
|---|---|---|
| **shader 注入** | shader 全部静态嵌入 raw string literal（B6）/ 用户内容**永不**作为 shader source | P0（B6 决策已防御）|
| **EGL display 句柄泄露** | RAII（`Sdl2EGLDisplay::~Sdl2EGLDisplay()` 调 `eglDestroyContext` + `eglTerminate`）/ `Application` 析构序约束 | P0 |
| **GL extension 安全枚举** | `HasExtension` 仅查询 `GL_EXTENSIONS` / 不动态加载 / 不解析用户输入选 extension | P1 |
| **glReadPixels timing attack** | `SavePPM` 仅测试用途 / 生产路径不暴露 | P2（既有 SoftwareCanvas 同等风险）|
| **context lost 资源泄露** | `OnContextLost` 必须释放所有 GL 资源（VAO/VBO/texture/FBO/shader）/ `OnContextRestored` 重建 | P0（嵌入式硬性要求）|
| **AddressSanitizer 兼容** | dynamic_cast `Surface*` 时 GLES 路径 fallback / 无 unsafe cast | P1 |
| **多线程 GL 调用** | 全部 GL 调用限定**主线程**（与 Veloxa main thread 约束一致 / lazy-attach quad-evidence 范式延续）| P0 |

### 6.3 威胁建模（轻量）

```
Attack Surface:
  1. shader 资源加载 → 静态嵌入 / 关闭加载点 ✅
  2. EGL display 配置 → 仅 SDL_GL_GetAttribute 受信源 ✅
  3. GL extension query → 只读 / 不分支用户输入 ✅
  4. texture 上传（DrawImage 用户图像）→ 走既有 ImageCache 受信路径 ✅
  5. context lost recovery → 严格 RAII 与状态机 ✅

无外部输入 / 无网络 / 无文件读写（除 shader 字符串编译期常量）
威胁等级：低（与 SoftwareCanvas 同等级）
```

---

## 7. 测试策略（蓝图阶段不实施 / 仅设计）

### 7.1 单元测试矩阵（实施任务划分依据）

| 测试组 | 文件 | 覆盖 | 估测数（实施任务）|
|---|---|---|:-:|
| GLESCanvas 基本 | `tests/graphics/gles/gles_canvas_test.cc` | FillRect / FillRoundedRect / Clear / SetTransform / 状态栈 | ~15-20 |
| GLESCanvas 路径 | `tests/graphics/gles/gles_canvas_path_test.cc` | FillPath / StrokePath / 复杂路径 + libtess2 | ~8-10 |
| GLESCanvas 文字 | `tests/graphics/gles/gles_canvas_text_test.cc` | DrawText / GlyphAtlas hit/miss / atlas full evict | ~10-12 |
| GLESCanvas 图像 | `tests/graphics/gles/gles_canvas_image_test.cc` | DrawImage / ImageTexturePool / RGBA8 upload | ~6-8 |
| GLESCanvas 裁剪 / 图层 | `tests/graphics/gles/gles_canvas_clip_test.cc` | PushClipRect / glScissor / PushLayer / 透明合成 | ~8-10 |
| GLESCanvas context lost | `tests/graphics/gles/gles_canvas_context_lost_test.cc` | OnContextLost / OnContextRestored / 资源重建 | ~4-6 |
| GLESDisplay | `tests/platform/sdl2/gles_display_test.cc` | Initialize / MakeCurrent / SwapBuffers / extensions | ~6-8 |
| Sdl2GLWindowSurface | `tests/platform/sdl2/sdl2_gl_window_surface_test.cc` | 构造 / Present / Resize / SavePPM(glReadPixels) | ~4-6 |
| GlyphAtlas | `tests/graphics/gles/glyph_atlas_test.cc` | GetOrUpload / row-pack / atlas full / evict | ~8-10 |
| ImageTexturePool | `tests/graphics/gles/image_texture_pool_test.cc` | RGBA8 upload / handle-keyed cache / evict | ~6-8 |
| Application GLES 分支 | `tests/core/application_gles_test.cc` | VX_RENDERER=gles 构造 / fallback 到 SoftwareCanvas | ~4-6 |
| **小计** | | | **~79-104 单测** |

### 7.2 集成测试矩阵

| smoke | 命令 | 覆盖 | 验收 |
|---|---|---|---|
| `hello_sdl2_gles_smoke` | `VX_RENDERER=gles ./hello_sdl2 --headless` | 完整管线（layout → render → GLES → present）| `PERF SMOKE: frames=N hud_visible=1` (N≥10) |
| `hello_devtool_gles_smoke` | `VX_RENDERER=gles ./hello_devtool` | DevTool overlay GLES 路径 / inspector tab 切换 | inspector OK / overlay OK |
| `hello_sdl2_gles_dirty_smoke` | `VX_RENDERER=gles ./hello_sdl2 --animate` | dirty rect / glScissor 路径 | dirty rect 触发 ≥ 5 帧 |
| `hello_sdl2_gles_context_lost_smoke` | 模拟 context lost | OnContextLost 触发 + 资源重建 | 重建后正常出帧 |

### 7.3 性能基准（B7 验收）

| BM | 文件 | 对照 | 验收 |
|---|---|---|:-:|
| `BM_GLESReplaySmoke` | `benchmarks/bench_gles_replay.cc` | vs `BM_ReplaySmoke`（既有 SW 路径）| GLES ≥ 10x SW |
| `BM_GLESReplayLargeList` | 同 | vs `BM_ReplayLargeList` | GLES ≥ 5x SW |
| `BM_GLESReplayTextHeavy` | 同 | vs `BM_ReplayTextHeavy` | GLES ≥ 3x SW |
| `BM_GLESReplayDeepClip` | 同 | vs `BM_ReplayDeepClip` | GLES ≥ 5x SW |
| `BM_GLESReplay1080pBudget` | 新建 | 1080p typical 页面 | ≤ 16.6ms / 60fps ✅ |
| `BM_GLESDirtyRectScissor` | 新建 | 全帧 vs scissor | ≥ 3x |

### 7.4 反向探针策略（实施任务延续）

每个 GREEN 子任务**强制**配反向探针（沿用 [TASK-20260505-01](../../memory-bank/archive/archive-TASK-20260505-01.md) + [TASK-20260505-02](../../memory-bank/archive/archive-TASK-20260505-02.md) triple-evidence 范式）。

---

## 8. 实施任务拆分（用户后续独立立项依据）

### 8.1 N 个 Level 3 子任务概览

V2=a 蓝图任务**不做** build；以下子任务用户后续基于本蓝图独立立项（推荐顺序按依赖关系）：

| Sub-Task | 名称 | Level | plan ×0.6 | 前置依赖 |
|:-:|---|:-:|:-:|---|
| **G1.1** | CMake `VX_RENDERER` flag + Phase 0 build matrix | L2 | ~2-3 h | — |
| **G1.2** | `GLESDisplay` 抽象 + `Sdl2EGLDisplay` 实施 | L3 | ~4-6 h | G1.1 |
| **G1.3** | `Sdl2GLWindowSurface` 实施 | L3 | ~3-4 h | G1.2 |
| **G1.4** | `GLESCanvas` 骨架 + Begin/End/Clear/SetTransform | L3 | ~3-4 h | G1.3 |
| **G1.5** | `GLESCanvas::FillRect` + `FillRoundedRect` + Solid Brush（B2 简单形状）| L3 | ~5-7 h | G1.4 |
| **G1.6** | `GLESCanvas::FillPath` via libtess2（B2 复杂路径）| L3 | ~6-8 h | G1.5 |
| **G1.7** | `GLESCanvas::Stroke*` (Stroke = Fill 转换) | L3 | ~3-4 h | G1.6 |
| **G1.8** | `GlyphAtlas` + `GLESCanvas::DrawText`（B3 GPU glyph）| L4 | ~8-10 h | G1.5 |
| **G1.9** | `ImageTexturePool` + `GLESCanvas::DrawImage` | L3 | ~4-6 h | G1.5 |
| **G1.10** | `GLESCanvas::PushClipRect/PopClip` (glScissor) + `PushLayer/PopLayer` (FBO)| L3 | ~5-7 h | G1.5 |
| **G1.11** | dirty rect (B4 glScissor + glClear in Application::Update) | L2 | ~2-3 h | G1.10 |
| **G1.12** | LinearGradient / RadialGradient（SDF shader）| L3 | ~4-6 h | G1.5 |
| **G1.13** | Application 构造分支（B5 VX_RENDERER 落地）+ fallback 容错 | L3 | ~3-4 h | G1.4 + G1.3 |
| **G1.14** | Context Lost / Restore（B8 嵌入式硬性 + G2 桥接） | L3 | ~5-7 h | G1.4 |
| **G1.15** | examples/hello_sdl2 GLES 路径 + smoke ctest | L2 | ~2-3 h | G1.13 + G1.4 |
| **G1.16** | DevTool dogfood GLES 路径 + smoke | L3 | ~4-6 h | G1.15 |
| **G1.17** | `BM_GLESReplay*` 基准 + 60fps budget 验收（B7）| L3 | ~4-6 h | G1.15 |
| **G1.18** | G2 接口预留 audit + GpuFence 头注释占位（B8） | L2 | ~1-2 h | G1.14 |
| **小计** | | | **~68-96 h plan ×0.6** | |

> **注：** 上述估时按 plan ×0.6 实测系数（[sext-evidence](../../memory-bank/systemPatterns.md)）调整后给出；考虑到 GLES 是新领域 + libtess2 集成 + Mesa headless 测试可能曲折，建议按 +30% buffer 预留 90-125 h plan ×0.6。

### 8.2 子任务依赖拓扑（DAG）

```
G1.1 (CMake flag)
  │
  ▼
G1.2 (GLESDisplay) ──► G1.3 (GLWindowSurface) ──► G1.13 (Application 分支)
                                                       │
                                                       ▼
G1.4 (GLESCanvas 骨架) ──┬──► G1.5 (FillRect/RoundedRect) ───┬──► G1.10 (Clip/Layer)
                          │                                    │       │
                          │                                    │       ▼
                          │                                    │   G1.11 (dirty rect)
                          │                                    │
                          │                                    ├──► G1.6 (FillPath) ──► G1.7 (Stroke)
                          │                                    │
                          │                                    ├──► G1.8 (GlyphAtlas + DrawText)
                          │                                    │
                          │                                    ├──► G1.9 (ImageTexturePool + DrawImage)
                          │                                    │
                          │                                    └──► G1.12 (Gradient SDF)
                          │
                          └──► G1.14 (Context Lost) ──► G1.18 (G2 接口预留 audit)

G1.13 + G1.4 ──► G1.15 (hello_sdl2 GLES smoke) ──► G1.16 (DevTool dogfood)
                                                ──► G1.17 (BM_GLESReplay*)
```

### 8.3 立项推荐顺序（最快可见效）

1. **第 1 轮（建立 GLES 基础设施）**：G1.1 → G1.2 → G1.3 → G1.4（骨架可见 / 不出像素）
2. **第 2 轮（核心绘图）**：G1.5 → G1.13 → G1.15（hello_sdl2 GLES 可出像素 / dogfood 可见效）
3. **第 3 轮（功能补全）**：G1.6 → G1.7 → G1.10 → G1.11（FillPath / Clip / dirty rect）
4. **第 4 轮（高质量出图）**：G1.8 → G1.9 → G1.12（DrawText / DrawImage / Gradient）
5. **第 5 轮（生产级稳定）**：G1.14 → G1.16 → G1.17 → G1.18（context lost / DevTool / 基准 / G2 桥接）

---

## 9. 风险与缓解

| 风险 | 等级 | 缓解 |
|---|:-:|---|
| **Mesa headless GLES 在 CI 不稳定** | 🟡 中 | CI 默认 `VX_RENDERER=software`（B5）/ GLES smoke 仅在 `linux + xvfb + Mesa` 环境跑；emergency fallback 单独 ctest tag `gles_optional` |
| **libtess2 集成复杂度低估** | 🟡 中 | G1.6 单独 Level 3 子任务 / Phase 0 audit libtess2 API 与既有 Path 数据结构兼容性 |
| **GLES context lost 行为差异（驱动间）** | 🔴 高 | G1.14 Context Lost 子任务设计 ≥ 3 套测试场景（SDL2 模拟 / 真实 GPU 重启 / Mesa headless）/ 缓解：G1.14 实施时记录每驱动行为差异表 |
| **glyph atlas 内存泄漏 / 状态机 bug** | 🟡 中 | G1.8 GlyphAtlas 子任务设计严格 LRU + ASAN 持续 audit / unit test 覆盖 atlas full evict 场景 |
| **shader 编译失败（GLES 版本不兼容）** | 🟡 中 | shader 头部声明 `#version 300 es`（GLES 3.0+ baseline）/ G1.4 骨架阶段 grep 验证 GLES 3.0 在所有目标平台支持 |
| **VX_RENDERER=gles 时 SoftwareCanvas 失活回归** | 🟡 中 | G1.13 Application 分支子任务确保**双 build 矩阵**（software path 1302 + gles path ~1380）持续 PASS / B5 fallback 容错路径 unit test |
| **dirty rect glScissor 与 stencil clip 交互** | 🟡 中 | G1.11 dirty rect + G1.10 Clip 子任务交叉 unit test / interaction matrix audit |
| **嵌入式 GLES 实现差异（PowerVR / Mali / Adreno）** | 🟢 低（蓝图阶段不涉及）| G2 蓝图阶段细化 / G1 仅 Mesa Desktop GLES 验证 |
| **plan/spec docs 落盘漂移**（P1 #6 升级为 P0）| 🟢 低（已 active）| **本任务首次完整实施「plan/spec docs 落盘即 commit」P0 协议**（plan + spec + creative ×N 同 commit / build 阶段无 docs 漂移）|

---

## 10. 验收要点

### 10.1 蓝图主交付物（V2=a）

- ✅ 本 spec 文档（~700 行，含 V/B 决策矩阵 + 架构 + PaintCommand→GL 翻译细节 + 安全 + 风险）
- ✅ plan 文档（`docs/plans/2026-05-05-gles-renderer-blueprint.md` / N 个 Level 3 子任务 + Phase 0 audit + commit 范本 + ctest 矩阵）
- ✅ creative ×3：
  - `creative-gles-context.md`（B1 GL context 创建 / EGL/SDL2 路径权衡 / context lost 处理）
  - `creative-gles-canvas.md`（B2 Canvas trampolining / shader-based vs tessellator）
  - `creative-gles-resources.md`（B3 GlyphAtlas + ImageTexturePool / B4 dirty rect GPU 化）

### 10.2 工作流元数据

- ✅ 5/5 V 决策 + 8/8 B 决策全 all_recommended 锁定（跨决策协同度 100% **第 11 + 12 次连续命中** / dec-evidence → endec → **doudec-evidence** 候选 / 累计 113/113）
- ✅ Phase 0 grep + 既有架构 audit 完成（Canvas / Surface / Application / PaintCommand / Replay 全链路 ✅）
- ✅ 8 项 P1 待处理事项全部活态 / 本任务首次实施 P0「plan/spec docs 落盘即 commit」协议

### 10.3 后续 Level 3 实施任务期望（用户独立立项）

- ✅ ~79-104 单测 + ~4 dogfood smoke + ~6 BM_GLESReplay* 基准（基线 60fps 1080p）
- ✅ 双 build 矩阵 PASS（software ~1302 不退化 / gles ~1380+）
- ✅ Mesa Desktop GLES 3.0+ 全部 ctest PASS / 嵌入式 GLES 留 G2 蓝图阶段验证

---

## 11. 与既有规则协同

| 规则段 | 协同方式 |
|---|---|
| `brainstorming.mdc`「跨决策协同度」 | V/B 矩阵 13 决策全标注协同度 ✅ |
| `brainstorming.mdc`「决策跳过率监控」 | 13/13 决策 1 次 AskQuestion 全锁定 / 协同度全 ✅ / reflect 阶段重审 13/13 / 沿用 TASK-20260430-04 + TASK-20260504-01 范式 |
| `writing-plans.mdc`「公开 API testability 检查清单」 | GLES 公开 API 全部 testability OK（GLESCanvas 22 方法可 unit test / GLESDisplay 接口可 mock / Sdl2GLWindowSurface 可 SavePPM via glReadPixels）|
| `writing-plans.mdc`「CMake 链接方向约束分析」 | `vx_graphics PUBLIC OpenGLES::GLESv2 EGL::EGL` / `vx_graphics PRIVATE tess2` / 下游 `tests/` 仅链 `vx_graphics` |
| `writing-plans.mdc`「FetchContent 网络代理守卫」 | libtess2 通过 FetchContent / VAN 阶段已确认代理状态 |
| `writing-plans.mdc`「测试基础设施审计」 | GLESCanvas 内部状态通过 `draw_calls() / state_changes() / atlas_uploads()` getter 暴露 |
| `writing-plans.mdc`「ctest 数量预期 config 矩阵」 | software path 1302+0 / gles path 1302+~80 / G1.15 smoke 增 4 / 双 build 矩阵 plan 阶段已声明 |
| `systemPatterns.md`「lazy-attach C ABI 容错模式 quad-evidence」 | `Application` 构造时 `dynamic_cast<Sdl2GLWindowSurface*>` 失败 → fallback SoftwareCanvas / 同模式 5th-evidence 候选 |
| `systemPatterns.md`「Mixed TDD RED 反向探针实践」 | 实施阶段每 GREEN 子任务强制反向探针（triple-evidence 范式延续）|
| `systemPatterns.md`「跨 Document arena 节点转移 — deep clone 必选范式」| 不直接相关 / 但 GlyphAtlas / ImageTexturePool 跨生命周期资源管理沿用「严格 RAII + 析构序」原则 |

---

## 12. 与上游 spec 协同

### 12.1 `mvp-scope.md` §C.2 + §11.2 #5

本蓝图实现 [`mvp-scope.md` §C.2](../../docs/specs/2026-05-04-mvp-scope.md) C-G1（OpenGL ES 硬件渲染后端）的**蓝图阶段**：

- 状态：`mvp-scope.md` §C.2 → C-G1「❌ 缺」→ 本蓝图完成后状态升级为 `🟡 蓝图`
- 完整实施需 G1.1 ~ G1.18 共 18 个 Level 3 子任务（用户后续独立立项 / ~68-96 h plan ×0.6 / +30% buffer 90-125 h）
- 全部子任务闭环后 C-G1 状态升级为「✅ 闭环」/ MVP-C 完成度 70% → ~78-80%

### 12.2 `mvp-scope.md` §11.2 #5 推荐落地

| 字段 | spec 推荐 | 本蓝图实际 |
|---|---|---|
| 任务名 | G1 OpenGL ES 硬件渲染后端蓝图 | ✅ 一致 |
| MVP 档 | MVP-C 核心 | ✅ |
| Level | L4 多 Phase | ✅（V2=a 纯蓝图）|
| plan ×0.6 估时 | ~30-60+ h | ~25-40 h（蓝图） + ~68-96 h（实施 / 用户后续独立立项）|

---

## 13. 引用

- [`docs/specs/2026-04-05-graphics-platform-hal-design.md`](../specs/2026-04-05-graphics-platform-hal-design.md) — 上游 Graphics HAL 设计（Canvas + Surface 纯虚抽象）
- [`docs/specs/2026-05-04-mvp-scope.md`](../specs/2026-05-04-mvp-scope.md) §3.3 + §11.2 #5 — C-G1 立项依据
- [`memory-bank/archive/archive-TASK-20260430-04.md`](../../memory-bank/archive/archive-TASK-20260430-04.md) — DevTool 蓝图 V2=a 范式
- [`memory-bank/archive/archive-TASK-20260504-01.md`](../../memory-bank/archive/archive-TASK-20260504-01.md) — MVP-scope 蓝图 V2=a 范式
- [`memory-bank/archive/archive-TASK-20260505-01.md`](../../memory-bank/archive/archive-TASK-20260505-01.md) — TASK-01 P1 #6 协议范例
- [`memory-bank/archive/archive-TASK-20260505-02.md`](../../memory-bank/archive/archive-TASK-20260505-02.md) — TASK-02 P1 #6 协议首次成功实施
- WHATWG / W3C OpenGL ES Specification 3.0+ — https://registry.khronos.org/OpenGL/specs/es/3.0/es_spec_3.0.pdf
- libtess2 — https://github.com/memononen/libtess2 （MPL2 / 用于 FillPath tessellation）
- EGL Specification — https://registry.khronos.org/EGL/sdk/docs/man/

---

**END OF SPEC**
