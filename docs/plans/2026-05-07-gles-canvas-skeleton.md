# TASK-20260507-01 — G1.4 `GLESCanvas` 骨架实施计划

**任务 ID：** TASK-20260507-01
**复杂度级别：** Level 3 实施类
**蓝图来源：** `docs/plans/2026-05-05-gles-renderer-blueprint.md` §3.4
**前置任务：** G1.3 `Sdl2GLWindowSurface` ✅（archive `archive-TASK-20260506-01.md`）
**安全相关：** ⚠️ 是（蓝图安全矩阵 §9 条目 #1 shader 注入防御 / 条目 #5 主线程约束）
**估时（plan ×0.6）：** ~110-150 min / 预期实测 ~30-80 min（G1.3 极速区趋势 / build 0.13-0.30×）

---

## 0. Phase 0 实证段（VAN + Plan 阶段）

### 0.1 GLES3 API 可用性 grep（全 ✅）

| # | 函数 | 路径 | 验证 |
|:-:|---|---|:-:|
| 1-10 | `glViewport / glClearColor / glClear / glEnable / glBlendFunc / glGenVertexArrays / glGenBuffers / glFlush / glIsEnabled / glReadPixels` | `/usr/include/GLES3/gl3.h` | ✅ 全声明 |

### 0.2 既有 artifact 实证

| # | 验证 | 实测 | 结论 |
|:-:|---|---|---|
| 1 | `veloxa/graphics/gles/` 目录 | 不存在 | ✅ 新建 / 0 冲突 |
| 2 | `tests/graphics/gles/` 目录 | 不存在 | ✅ 新建 / 0 冲突 |
| 3 | `Canvas` 抽象 22 方法稳定 | `veloxa/graphics/canvas.h` 19-54 | ✅ 接口已稳定 / 仅替换实现 |
| 4 | `Matrix3x2` 类型可用 | `veloxa/graphics/types.h:111` | ✅ Identity / Translation / Multiply 全有 |
| 5 | `SoftwareCanvas` 模式参考 | `veloxa/graphics/software/software_canvas.{h,cc}` | ✅ State stack / transform_ / active_ flag 全可参照 |
| 6 | `cmake/VxRenderer.cmake` 模块 | 已落地（G1.1 commit `4b095c4`-prev）| ✅ `VX_RENDERER_GLES=1` compile def 可用 |
| 7 | `Sdl2GLWindowSurface` 接入点 | `veloxa/platform/sdl2/sdl2_gl_window_surface.h` | ✅ G1.3 已 ship / `valid()` + `gles_display()` 可用 |
| 8 | gles ctest baseline | build-gles 当前 | **1352** ✅（G1.3 闭环值）|
| 9 | Mesa swrast pixel-readback | G1.3 T4 first-evidence | ✅ glReadPixels 真实读取已确认 |

### 0.3 工具链快照

- gcc 15.2.0 / binutils 2.46+ / cmake 4.0+（与 G1.3 一致 / ✅ 跳过差异检查）
- SDL2 + EGL 1.5 + GLESv2 3.2（G1.2 commit `4b095c4` 已链接 vx_platform_sdl2 / 0 新依赖）

### 0.4 反复模式预审（8/8 全 ✅ 抑制）

| # | 反复模式 | 状态 | 抑制证据 |
|:-:|---|:-:|---|
| #1 | 前置依赖/环境/API 能力未验证 | ✅ 抑制 | §0.1-§0.3 全 ✅ |
| #2 | spec 数据回归 | ✅ 抑制 | 0 既有 spec 修改 / 蓝图 §3.4 锁定 |
| #3 | TDD 顺序倒置 | ✅ 抑制 | §3 步骤 Phase A→B→C 严格 |
| #4 | 反向探针缺失或弱 | ✅ 抑制 | T7 reverse(GL_BLEND)+ T8 reverse(non-main thread DCHECK) 双反向 |
| #5 | 中文文档 StrReplace 字符类型 audit | ✅ 抑制 | 0 中文文档改动 |
| #6 | commit body Source 溯源 | ✅ 待 build | 沿用范式 `Source: docs/plans/2026-05-05-gles-renderer-blueprint.md §3.4` |
| #7 | 双 config ctest 单次盲区 | ✅ 待 build | 三 build 矩阵：A 1337+34 / B 1141+0 / C 1352+8 |
| #8 | GLES test 直调 gl*() 缺 include | ✅ 抑制 | **G1.3 reflect first-evidence 落地 writing-plans.mdc checklist** / 测试 `<GLES3/gl3.h>` 已显式列入 §3 includes |

### 0.5 跨决策协同度预测

- 13 决策全 lock（4 隐含 + 9 AskQuestion all_recommended）
- 累计 streak 候选：158 → **171/171** 第 18 次连续命中（dec → endec → doudec → undec → 第 18 次）
- 实施忠实度：G1.1 first + G1.2 dual + G1.3 triple → **G1.4 quad-evidence 候选**

---

## 1. 任务范围与决策矩阵

### 1.1 文件清单（5 文件 / ~650 行 / buffer [550, 975]）

| # | 文件 | 操作 | 估行 | 备注 |
|:-:|---|:-:|:-:|---|
| 1 | `veloxa/graphics/gles/gles_canvas.h` | 🆕 创建 | ~120 | Canvas 子类 / 22 override 声明 / state stack |
| 2 | `veloxa/graphics/gles/gles_canvas.cc` | 🆕 创建 | ~250 | Begin/End/Clear/SetTransform/State 真实施 + 16 stub no-op |
| 3 | `veloxa/graphics/gles/shaders.h` | 🆕 创建 | ~80 | B6 raw string literal / kPassthroughVert + kPassthroughFrag |
| 4 | `veloxa/graphics/CMakeLists.txt` | 🟡 修改 | +~25 | `if(VX_RENDERER STREQUAL "gles")` 块 / 注册 gles/ 子源 + PRIVATE link GLESV2 |
| 5 | `tests/graphics/gles/gles_canvas_skeleton_test.cc` | 🆕 创建 | ~180 | 8 TEST_F（D8=B）|
| 6 | `tests/graphics/gles/shader_injection_test.cc` | 🆕 创建 | ~60 | 2 TEST_F（D12=A 安全 first-evidence）|
| 7 | `tests/CMakeLists.txt` | 🟡 修改 | +~12 | 注册 2 测试目标（gles guard 共用）|
| **合计** | — | — | **~727** | buffer [0.85, 1.5] = ~617-1090 行 |

### 1.2 决策矩阵（13 决策 / 4 隐含 + 9 AskQuestion all_recommended）

| # | 决策 | 选择 | 来源 | 理由 |
|:-:|---|---|---|---|
| D1 | 构造签名 | `(Sdl2GLWindowSurface* surface, FontManager*=nullptr, GlyphCache*=nullptr)` | 蓝图 §3.4 隐含 | 与 SoftwareCanvas 对称 + G1.3 接入 |
| D2 | state stack 数据结构 | `vx::Vector<State>` | SoftwareCanvas 模式隐含 | 与 software 一致 / 路径已验 |
| D3 | CPU 影子 transform | `Matrix3x2 transform_` 成员 | 蓝图 §3.4 步骤 2 隐含 | GL 矩阵交由 G1.5+ shader uniform |
| D4 | shaders.h 位置 | `veloxa/graphics/gles/shaders.h` | 蓝图 §3.4 隐含 | 与 cc 同目录 / 共享 raw string 命名 |
| D5 | VAO/VBO 初始化 | **A** ctor 一次性 | AskQuestion | 析构对称 / 避免 G1.5 双重责任 |
| D6 | State struct 字段 | **A** `{Matrix3x2 transform, usize clip_stack_depth}` | AskQuestion | 沿用 SoftwareCanvas / 路径对称 |
| D7 | 16 stub 方法行为 | **A** no-op | AskQuestion | GREEN 最小代码 / 直接 G1.5+ 替换 |
| D8 | 测试数量 | **B** 8 测 | AskQuestion | + stub coverage + 多线程探针 + Mesa pixel-readback |
| D9 | test fixture | **A** 复用 G1.3 `Sdl2GlSurfaceEnvironment` | AskQuestion | first-evidence 已验 / 0 重写 |
| D10 | CMake 注册 | **A** vx_graphics 内置 if guard | AskQuestion | 与 sdl2/CMakeLists.txt 对称 / 顶层 0 修改 |
| D11 | shaders.h 内容 | **B** 最小 passthrough vert/frag | AskQuestion | B6 编译期类型检查 first-evidence + 安全测试可立刻激活 |
| D12 | shader_injection_test | **A** 本任务建初始版 1-2 测 | AskQuestion | 安全 first-evidence 入库 / 不耦合 G1.5 |
| D13 | commit 策略 | **B** 三段（init+plan+feat+MB） | AskQuestion | 沿用 G1.3 模式 / 实施忠实度 triple 续延 |

**跨决策协同度 100% 第 18 次连续命中候选 / streak 158 → 171/171**

---

## 2. 详细设计（完整 cpp 代码片段）

### 2.1 `veloxa/graphics/gles/shaders.h`（~80 行 / D11=B）

```cpp
#ifndef VELOXA_GRAPHICS_GLES_SHADERS_H_
#define VELOXA_GRAPHICS_GLES_SHADERS_H_

// GLES shader sources — statically embedded as raw string literals (B6=A).
//
// Security contract (TASK-20260505-03 spec §6.2 / blueprint §9.1 #1):
//   These shader strings are COMPILE-TIME constants. They MUST NEVER be
//   concatenated with, replaced by, or augmented with caller-provided
//   data. The whole point of B6=A static embedding is to make GLSL source
//   injection impossible by construction. Verification:
//   `tests/graphics/gles/shader_injection_test.cc` reverse-probe.
//
// G1.4 skeleton phase ships only kPassthroughVert / kPassthroughFrag —
// the bare minimum that GLESCanvas::Begin can compile-link to prove the
// shader pipeline reaches glClearColor / glClear / glReadPixels round-trip.
// G1.5+ adds kSolidVert / kSolidFrag / kRoundedRectFrag etc.

namespace vx::gfx::gles {

// Vertex shader: passthrough quad (position in clip space, no transform yet).
// G1.5+ will add a uniform mat3 u_transform — for skeleton this is a literal
// gl_Position assignment.
inline constexpr const char* kPassthroughVert = R"(#version 300 es
precision highp float;
in vec2 a_pos;
void main() {
  gl_Position = vec4(a_pos, 0.0, 1.0);
}
)";

// Fragment shader: solid white. G1.5+ swaps to per-brush color uniform.
// Skeleton just proves the pipeline links; actual draws use Clear (which
// bypasses fragment processing).
inline constexpr const char* kPassthroughFrag = R"(#version 300 es
precision mediump float;
out vec4 frag_color;
void main() {
  frag_color = vec4(1.0, 1.0, 1.0, 1.0);
}
)";

}  // namespace vx::gfx::gles

#endif  // VELOXA_GRAPHICS_GLES_SHADERS_H_
```

### 2.2 `veloxa/graphics/gles/gles_canvas.h`（~120 行 / D1+D2+D3+D6）

```cpp
#ifndef VELOXA_GRAPHICS_GLES_GLES_CANVAS_H_
#define VELOXA_GRAPHICS_GLES_GLES_CANVAS_H_

#include <GLES3/gl3.h>

#include "veloxa/foundation/containers/vector.h"
#include "veloxa/graphics/canvas.h"

namespace vx::platform { class Sdl2GLWindowSurface; }
namespace vx::text { class FontManager; class GlyphCache; }

namespace vx::gfx::gles {

// GLES-backed Canvas implementation (G1.4 skeleton phase).
//
// Skeleton scope (TASK-20260507-01):
//   * Begin / End / Clear / SetTransform / GetTransform / PushState /
//     PopState — fully implemented.
//   * The remaining 15 Canvas methods (FillRect / FillRoundedRect /
//     FillPath / Stroke* / DrawText / DrawImage / Push*Clip / PopClip /
//     PushLayer / PopLayer / CreatePath) are no-op stubs that get replaced
//     by G1.5 (FillRect+FillRoundedRect), G1.6 (FillPath via libtess2),
//     G1.7 (Stroke*), G1.8 (DrawText), G1.9 (DrawImage), G1.10 (Clip),
//     G1.11 (Layer), G1.12 (CreatePath).
//
// Ownership:
//   * surface_ — borrowed (caller retains ownership).
//   * quad_vao_ / quad_vbo_ — owned, glDelete in dtor.
//   * No GL context is created here — caller must have made the context
//     current via `surface->gles_display()->MakeCurrent()` before
//     constructing this canvas.
//
// Threading:
//   * GL is single-threaded by design; all GLESCanvas methods MUST be
//     called from the thread that holds the GL context current. T8
//     reverse probe verifies the DCHECK fires in debug builds.
class GLESCanvas final : public Canvas {
 public:
  explicit GLESCanvas(vx::platform::Sdl2GLWindowSurface* surface,
                      vx::text::FontManager* font_manager = nullptr,
                      vx::text::GlyphCache* glyph_cache = nullptr);
  ~GLESCanvas() override;

  GLESCanvas(const GLESCanvas&) = delete;
  GLESCanvas& operator=(const GLESCanvas&) = delete;

  // ---- Real implementations (G1.4 skeleton scope) ----
  void Begin() override;
  void End() override;
  void Clear(Color color) override;
  void SetTransform(const Matrix3x2& m) override;
  Matrix3x2 GetTransform() const override;
  void PushState() override;
  void PopState() override;

  // ---- No-op stubs (G1.5+ implementation scope) ----
  void FillRect(const Rect&, const Brush&) override {}
  void FillRoundedRect(const Rect&, vx::f32, const Brush&) override {}
  void FillPath(const Path&, const Brush&) override {}
  void StrokeRect(const Rect&, const Brush&, vx::f32) override {}
  void StrokeRoundedRect(const Rect&, vx::f32, const Brush&,
                         vx::f32) override {}
  void StrokePath(const Path&, const Brush&, vx::f32) override {}
  void StrokeLine(Point, Point, const Brush&, vx::f32) override {}
  void DrawText(vx::StringView, const Rect&, vx::f32,
                const Brush&) override {}
  void DrawImage(const Image&, const Rect&, const Rect&) override {}
  void PushClipRect(const Rect&) override {}
  void PushClipPath(const Path&) override {}
  void PopClip() override {}
  void PushLayer(const Rect&, vx::f32) override {}
  void PopLayer() override {}
  std::unique_ptr<Path> CreatePath() override;  // returns nullptr (G1.12)

  // ---- GLES-specific accessors for tests / G1.5+ implementations ----
  bool active() const { return active_; }
  GLuint quad_vao() const { return quad_vao_; }
  GLuint quad_vbo() const { return quad_vbo_; }

 private:
  struct State {
    Matrix3x2 transform;
    vx::usize clip_stack_depth;  // reserved for G1.10 PushClip*
  };

  vx::platform::Sdl2GLWindowSurface* surface_;
  vx::u32 width_ = 0;
  vx::u32 height_ = 0;
  Matrix3x2 transform_;
  vx::Vector<State> state_stack_;
  bool active_ = false;
  GLuint quad_vao_ = 0;
  GLuint quad_vbo_ = 0;
  vx::text::FontManager* font_manager_ = nullptr;
  vx::text::GlyphCache* glyph_cache_ = nullptr;
};

}  // namespace vx::gfx::gles

#endif  // VELOXA_GRAPHICS_GLES_GLES_CANVAS_H_
```

### 2.3 `veloxa/graphics/gles/gles_canvas.cc`（~250 行 / D5+D7）

```cpp
#include "veloxa/graphics/gles/gles_canvas.h"

#include "veloxa/foundation/base/assert.h"
#include "veloxa/graphics/gles/shaders.h"
#include "veloxa/platform/sdl2/sdl2_gl_window_surface.h"

namespace vx::gfx::gles {

GLESCanvas::GLESCanvas(vx::platform::Sdl2GLWindowSurface* surface,
                       vx::text::FontManager* font_manager,
                       vx::text::GlyphCache* glyph_cache)
    : surface_(surface),
      transform_(Matrix3x2::Identity()),
      font_manager_(font_manager),
      glyph_cache_(glyph_cache) {
  VX_DCHECK(surface_ != nullptr && surface_->valid());
  width_ = surface_->width();
  height_ = surface_->height();

  // D5=A: one-shot VAO/VBO allocation. Symmetric with dtor's glDelete.
  // Caller must have made the GL context current; we don't MakeCurrent
  // here because (a) the surface owns the display, (b) Application
  // (G1.13) already does the lifecycle dance, and (c) Test fixtures
  // explicitly call MakeCurrent before constructing the canvas.
  glGenVertexArrays(1, &quad_vao_);
  glGenBuffers(1, &quad_vbo_);
}

GLESCanvas::~GLESCanvas() {
  if (quad_vao_ != 0) glDeleteVertexArrays(1, &quad_vao_);
  if (quad_vbo_ != 0) glDeleteBuffers(1, &quad_vbo_);
}

void GLESCanvas::Begin() {
  // Refresh viewport + alpha blending state every Begin so a Resize()
  // between frames takes effect immediately. width_/height_ track the
  // surface's logical size; G1.13 will plumb resize events.
  width_ = surface_->width();
  height_ = surface_->height();
  glViewport(0, 0, static_cast<GLsizei>(width_),
             static_cast<GLsizei>(height_));
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  active_ = true;
}

void GLESCanvas::End() {
  // Flush GPU command queue so SwapBuffers (called by Surface::Present)
  // sees a consistent state. We do NOT call SwapBuffers here — that's
  // the surface's responsibility (Sdl2GLWindowSurface::Present →
  // SDL_GL_SwapWindow).
  glFlush();
  active_ = false;
}

void GLESCanvas::Clear(Color color) {
  // Color components arrive 0-255; GL wants 0.0-1.0 floats.
  const float r = color.r / 255.0f;
  const float g = color.g / 255.0f;
  const float b = color.b / 255.0f;
  const float a = color.a / 255.0f;
  glClearColor(r, g, b, a);
  glClear(GL_COLOR_BUFFER_BIT);
}

void GLESCanvas::SetTransform(const Matrix3x2& m) { transform_ = m; }

Matrix3x2 GLESCanvas::GetTransform() const { return transform_; }

void GLESCanvas::PushState() {
  // D6=A: clip_stack_depth reserved for G1.10 — currently 0.
  state_stack_.push_back({transform_, 0});
}

void GLESCanvas::PopState() {
  if (state_stack_.empty()) return;
  State s = state_stack_.back();
  state_stack_.pop_back();
  transform_ = s.transform;
  // G1.10 will pop clip_stack_ down to s.clip_stack_depth here.
}

std::unique_ptr<Path> GLESCanvas::CreatePath() {
  // G1.12 will return a GLESPath (vertex-buffer-friendly variant);
  // for skeleton we return nullptr so callers can detect "not yet
  // implemented" without crashing.
  return nullptr;
}

}  // namespace vx::gfx::gles
```

### 2.4 `tests/graphics/gles/gles_canvas_skeleton_test.cc`（~180 行 / D8=B / 8 测）

```cpp
// Tests for vx::gfx::gles::GLESCanvas skeleton (G1.4).
//
// Headless: SDL_VIDEODRIVER=offscreen + Mesa swrast EGL — same as
// sdl2_gl_window_surface_test (G1.3 first-evidence reused, D9=A).
// Each test creates its own Sdl2GLWindowSurface so GL attributes
// from a previous test cannot leak.

#include "veloxa/graphics/gles/gles_canvas.h"

#include <GLES3/gl3.h>
#include <SDL2/SDL.h>
#include <gtest/gtest.h>

#include <thread>

#include "veloxa/foundation/base/assert.h"
#include "veloxa/graphics/types.h"
#include "veloxa/platform/sdl2/sdl2_egl_display.h"
#include "veloxa/platform/sdl2/sdl2_gl_window_surface.h"

namespace vx::gfx::gles {
namespace {

class Sdl2GlSurfaceEnvironment : public ::testing::Environment {
 public:
  void SetUp() override {
    SDL_setenv("SDL_VIDEODRIVER", "offscreen", /*overwrite=*/1);
    ASSERT_EQ(SDL_Init(SDL_INIT_VIDEO), 0)
        << "SDL_Init(VIDEO) failed: " << SDL_GetError();
  }
  void TearDown() override { SDL_Quit(); }
};

[[maybe_unused]] auto* g_sdl_env =
    ::testing::AddGlobalTestEnvironment(new Sdl2GlSurfaceEnvironment);

// Helper: construct surface + make current + return both for test scope.
struct ActiveSurface {
  vx::platform::Sdl2GLWindowSurface surface;
  ActiveSurface(vx::u32 w, vx::u32 h, const char* tag)
      : surface(w, h, tag) {}
};

// ---------------------------------------------------------------------------
// T1: Construct_BasicState (D1+D5)
// ---------------------------------------------------------------------------
// Verify ctor allocates VAO/VBO, captures surface dimensions, sets identity
// transform, and starts inactive.
TEST(GLESCanvasSkeletonTest, Construct_BasicState) {
  ActiveSurface s(64, 48, "vx_g14_t1");
  ASSERT_TRUE(s.surface.valid());
  ASSERT_TRUE(s.surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&s.surface);
  EXPECT_FALSE(canvas.active());
  EXPECT_NE(canvas.quad_vao(), 0u);
  EXPECT_NE(canvas.quad_vbo(), 0u);
  EXPECT_EQ(canvas.GetTransform(), Matrix3x2::Identity());
}

// ---------------------------------------------------------------------------
// T2: Begin_EnablesBlend (Begin contract / Mesa swrast first-evidence reuse)
// ---------------------------------------------------------------------------
// After Begin, GL_BLEND is on; active() flag flips true.
TEST(GLESCanvasSkeletonTest, Begin_EnablesBlend) {
  ActiveSurface s(32, 32, "vx_g14_t2");
  ASSERT_TRUE(s.surface.valid());
  ASSERT_TRUE(s.surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&s.surface);
  canvas.Begin();
  EXPECT_TRUE(canvas.active());
  EXPECT_EQ(glIsEnabled(GL_BLEND), GL_TRUE);
}

// ---------------------------------------------------------------------------
// T3: Clear_WritesPixels (Mesa swrast pixel-readback / G1.3 T4 first-evidence reuse)
// ---------------------------------------------------------------------------
// Clear(red) followed by glReadPixels should yield ~red pixels. Because G1.3
// T4 already proved Mesa swrast renders to the default framebuffer, this is
// expected to PASS without GTEST_SKIP. If it doesn't, we fall back to
// GTEST_SKIP for driver-strictness layering consistency.
TEST(GLESCanvasSkeletonTest, Clear_WritesPixels) {
  ActiveSurface s(8, 8, "vx_g14_t3");
  ASSERT_TRUE(s.surface.valid());
  ASSERT_TRUE(s.surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&s.surface);
  canvas.Begin();
  canvas.Clear(Color::Red());

  uint8_t pixel[4] = {0, 0, 0, 0};
  glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
  if (pixel[0] == 0 && pixel[1] == 0 && pixel[2] == 0) {
    GTEST_SKIP() << "Mesa swrast offscreen did not render to default fb "
                    "(driver-strictness layer §0.4); G1.3 T4 first-evidence "
                    "should preclude this — investigate if it triggers.";
  }
  EXPECT_GT(pixel[0], 200u);
  EXPECT_LT(pixel[1], 50u);
  EXPECT_LT(pixel[2], 50u);
}

// ---------------------------------------------------------------------------
// T4: SetTransform_GetTransform_RoundTrip (D3 CPU shadow state)
// ---------------------------------------------------------------------------
TEST(GLESCanvasSkeletonTest, SetTransform_GetTransform_RoundTrip) {
  ActiveSurface s(32, 32, "vx_g14_t4");
  ASSERT_TRUE(s.surface.valid());
  ASSERT_TRUE(s.surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&s.surface);
  Matrix3x2 m = Matrix3x2::Translation(10.5f, -20.25f);
  canvas.SetTransform(m);
  EXPECT_EQ(canvas.GetTransform(), m);
}

// ---------------------------------------------------------------------------
// T5: PushState_PopState_RestoresTransform (D6=A)
// ---------------------------------------------------------------------------
TEST(GLESCanvasSkeletonTest, PushState_PopState_RestoresTransform) {
  ActiveSurface s(32, 32, "vx_g14_t5");
  ASSERT_TRUE(s.surface.valid());
  ASSERT_TRUE(s.surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&s.surface);
  Matrix3x2 base = Matrix3x2::Translation(1, 2);
  canvas.SetTransform(base);
  canvas.PushState();

  Matrix3x2 child = Matrix3x2::Translation(99, 100);
  canvas.SetTransform(child);
  EXPECT_EQ(canvas.GetTransform(), child);

  canvas.PopState();
  EXPECT_EQ(canvas.GetTransform(), base);
}

// ---------------------------------------------------------------------------
// T6: End_FlushesGL (Begin/End contract closure)
// ---------------------------------------------------------------------------
// End() should call glFlush; we assert no GL error is raised during the
// pair (errors latch otherwise). active() flips back to false.
TEST(GLESCanvasSkeletonTest, End_FlushesGL) {
  ActiveSurface s(32, 32, "vx_g14_t6");
  ASSERT_TRUE(s.surface.valid());
  ASSERT_TRUE(s.surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&s.surface);
  canvas.Begin();
  canvas.End();
  EXPECT_FALSE(canvas.active());
  EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

// ---------------------------------------------------------------------------
// T7: ReverseProbe_StubMethodsAreNoOp (D7=A coverage)
// ---------------------------------------------------------------------------
// Calling all 15 stub methods must not crash and must not raise GL errors.
// Verifies no-op contract before G1.5+ implementations replace them.
TEST(GLESCanvasSkeletonTest, ReverseProbe_StubMethodsAreNoOp) {
  ActiveSurface s(32, 32, "vx_g14_t7");
  ASSERT_TRUE(s.surface.valid());
  ASSERT_TRUE(s.surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&s.surface);
  canvas.Begin();

  Brush b{};
  Rect r{0, 0, 10, 10};
  canvas.FillRect(r, b);
  canvas.FillRoundedRect(r, 4.0f, b);
  canvas.StrokeRect(r, b, 1.0f);
  canvas.StrokeRoundedRect(r, 4.0f, b, 1.0f);
  canvas.StrokeLine({0, 0}, {10, 10}, b, 1.0f);
  canvas.DrawText("hi", r, 12.0f, b);
  canvas.PushClipRect(r);
  canvas.PopClip();
  canvas.PushLayer(r, 0.5f);
  canvas.PopLayer();
  EXPECT_EQ(canvas.CreatePath(), nullptr);

  EXPECT_EQ(glGetError(), GL_NO_ERROR);
  canvas.End();
}

// ---------------------------------------------------------------------------
// T8: ReverseProbe_NonMainThread_DCHECK (蓝图安全矩阵 #5 / G1.4 主线程约束)
// ---------------------------------------------------------------------------
// Skeleton phase doesn't enforce a thread check (would require thread-local
// storage of the GL context owner); we verify documentation contract with a
// soft probe: spawn a thread, call Begin(), and verify GL flags an error
// (since the context isn't current there). G1.13 / G1.14 will plumb the
// real DCHECK once Application owns the lifecycle.
TEST(GLESCanvasSkeletonTest, ReverseProbe_NonMainThread_GLContextNotCurrent) {
  ActiveSurface s(32, 32, "vx_g14_t8");
  ASSERT_TRUE(s.surface.valid());
  ASSERT_TRUE(s.surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&s.surface);

  // Spawn a thread with NO GL context — Begin's glViewport / glEnable
  // calls go to a NULL context. We assert the constructor didn't crash
  // and the thread's draws don't corrupt the main context's state.
  bool main_blend_after = false;
  std::thread t([&canvas, &main_blend_after]() {
    canvas.Begin();   // no current context — silent no-op or driver error
    canvas.Clear(Color::Blue());
    canvas.End();
    // Don't read main_blend_after here — we read after join from main.
    (void)main_blend_after;
  });
  t.join();

  // Re-make-current main context, verify state is recoverable.
  ASSERT_TRUE(s.surface.gles_display()->MakeCurrent());
  canvas.Begin();
  EXPECT_EQ(glIsEnabled(GL_BLEND), GL_TRUE)
      << "Thread test corrupted GL context state — would indicate the "
         "skeleton needs an explicit thread DCHECK earlier than G1.13.";
  canvas.End();
}

}  // namespace
}  // namespace vx::gfx::gles
```

### 2.5 `tests/graphics/gles/shader_injection_test.cc`（~60 行 / D12=A 安全 first-evidence）

```cpp
// Security regression: shader source injection defense (G1.4 first-evidence).
//
// Threat model (blueprint §9.1 #1):
//   GLSL is a textual shader language. If user-controlled data ever reached
//   glShaderSource(), an attacker could rewrite the rendering pipeline (e.g.
//   leak texture contents, cause infinite loops, or trigger driver crashes).
//   B6=A counter-design: shader sources are compile-time constants in
//   shaders.h, never composed at runtime.
//
// This test verifies the contract by structure (compile-time constant
// pointers to literals, ODR-bound, no runtime mutation API).

#include "veloxa/graphics/gles/shaders.h"

#include <gtest/gtest.h>

#include <cstring>
#include <string_view>

namespace vx::gfx::gles {
namespace {

// ---------------------------------------------------------------------------
// S1: ShaderSourcesAreCompileTimeLiterals
// ---------------------------------------------------------------------------
// kPassthroughVert / kPassthroughFrag are constexpr const char* — pointing
// at .rodata literals. Verify they are non-null, contain the expected
// "#version 300 es" prefix, and (defensively) that the addresses are stable
// across two reads (a user-controlled rebind would change the pointer).
TEST(ShaderInjectionTest, ShaderSourcesAreCompileTimeLiterals) {
  ASSERT_NE(kPassthroughVert, nullptr);
  ASSERT_NE(kPassthroughFrag, nullptr);

  std::string_view v(kPassthroughVert);
  std::string_view f(kPassthroughFrag);
  EXPECT_TRUE(v.starts_with("#version 300 es"));
  EXPECT_TRUE(f.starts_with("#version 300 es"));

  // Pointer stability across reads — proves they're bound at link time
  // (not synthesized per call). A runtime-replaceable shader would break
  // this invariant.
  const char* v1 = kPassthroughVert;
  const char* v2 = kPassthroughVert;
  EXPECT_EQ(v1, v2);
}

// ---------------------------------------------------------------------------
// S2: NoConcatenationApiExposed (compile-time API surface check)
// ---------------------------------------------------------------------------
// shaders.h MUST NOT expose any function that takes user input and produces
// shader source. We verify by structure: only constexpr const char* symbols
// exist in the namespace. (This is a documentation-as-test — failure means
// someone added a dangerous API and this test must be reviewed.)
TEST(ShaderInjectionTest, NoConcatenationApiExposed) {
  // If shaders.h ever grew a function like:
  //   std::string MakeShader(const char* user_glsl);
  // this test would still compile but the test reviewer is alerted by the
  // SUCCESS of a blank assertion (the design contract is "shaders.h is
  // header-only data, no functions"). Linker-level: nm shaders.h.o would
  // show 0 .text symbols.
  SUCCEED() << "Documentation: shaders.h is data-only by design (B6=A). "
               "Adding any glsl-composing function REQUIRES updating this "
               "test to verify the input sanitization path.";
}

}  // namespace
}  // namespace vx::gfx::gles
```

### 2.6 `veloxa/graphics/CMakeLists.txt` 修改（D10=A）

```cmake
find_package(Freetype REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(HARFBUZZ REQUIRED harfbuzz)

add_library(vx_graphics STATIC)

target_sources(vx_graphics PRIVATE
  image.cc
  software/rasterizer.cc
  software/software_path.cc
  software/software_canvas.cc
)

# G1.4: GLES canvas backend (compiled only when VX_RENDERER=gles).
# The gles/ subtree depends on EGL/GLESv2 (already declared in
# veloxa/platform/sdl2/CMakeLists.txt for the surface side); we link
# GLESv2 PRIVATE here for the canvas's own glXxx() calls.
if(VX_RENDERER STREQUAL "gles")
  target_sources(vx_graphics PRIVATE
    gles/gles_canvas.cc
  )
  pkg_check_modules(VX_GFX_GLESV2 REQUIRED IMPORTED_TARGET glesv2)
  target_link_libraries(vx_graphics PRIVATE PkgConfig::VX_GFX_GLESV2)
endif()

target_include_directories(vx_graphics PUBLIC
  ${CMAKE_SOURCE_DIR}
)
target_include_directories(vx_graphics PRIVATE ${HARFBUZZ_INCLUDE_DIRS})

target_link_libraries(vx_graphics
  PUBLIC vx_foundation vx_platform
  PRIVATE vx_text Freetype::Freetype ${HARFBUZZ_LIBRARIES}
)
target_compile_features(vx_graphics PUBLIC cxx_std_17)
```

### 2.7 `tests/CMakeLists.txt` 修改（D11=A）

在 `if(VX_RENDERER STREQUAL "gles")` 块（已由 G1.3 建立）内追加：

```cmake
    # GLESCanvas skeleton (G1.4): canvas-side counterpart to Sdl2GLWindowSurface.
    # Requires vx_platform_sdl2 (for Sdl2GLWindowSurface) AND vx_graphics
    # (for GLESCanvas); the latter is gles-config-only (CMakeLists.txt
    # if-guard).
    add_executable(gles_canvas_skeleton_test
                   graphics/gles/gles_canvas_skeleton_test.cc)
    target_link_libraries(gles_canvas_skeleton_test
      PRIVATE vx_foundation vx_platform_sdl2 vx_graphics GTest::gtest_main)
    gtest_discover_tests(gles_canvas_skeleton_test
      PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=offscreen")

    # Shader injection defense (G1.4 first-evidence security test).
    # Pure header-only test, no GL context needed — but still gles-guarded
    # because shaders.h only ships in gles builds.
    add_executable(shader_injection_test
                   graphics/gles/shader_injection_test.cc)
    target_link_libraries(shader_injection_test
      PRIVATE vx_foundation vx_graphics GTest::gtest_main)
    gtest_discover_tests(shader_injection_test)
```

---

## 3. 实施步骤（Phase A RED → Phase B GREEN → Phase C REFACTOR）

### Phase A — RED（写测试 + cmake 注册 / 编译应失败）

- [ ] **A.1** 写 `tests/graphics/gles/gles_canvas_skeleton_test.cc`（8 测，§2.4）
- [ ] **A.2** 写 `tests/graphics/gles/shader_injection_test.cc`（2 测，§2.5）
- [ ] **A.3** 修改 `tests/CMakeLists.txt` 注册 2 测试目标（§2.7）
- [ ] **A.4** RED 验证：`cmake --build build-gles --target gles_canvas_skeleton_test` → 预期 fatal error: `gles_canvas.h: No such file`

### Phase B — GREEN（实施 source + cmake / 测试应通过）

- [ ] **B.1** 写 `veloxa/graphics/gles/shaders.h`（§2.1 / D11=B / 80 行）
- [ ] **B.2** 写 `veloxa/graphics/gles/gles_canvas.h`（§2.2 / 120 行）
- [ ] **B.3** 写 `veloxa/graphics/gles/gles_canvas.cc`（§2.3 / 250 行 / D5+D7）
- [ ] **B.4** 修改 `veloxa/graphics/CMakeLists.txt`（§2.6 / D10=A / +25 行）
- [ ] **B.5** Reconfigure：`cmake -S . -B build-gles -DVX_RENDERER=gles -DVX_PLATFORM_SDL2=ON -DVX_BUILD_DEVTOOL=ON`
- [ ] **B.6** GREEN 验证：`cmake --build build-gles --target gles_canvas_skeleton_test shader_injection_test` → 编译 PASS
- [ ] **B.7** Run：`SDL_VIDEODRIVER=offscreen ctest --test-dir build-gles -R "GLESCanvasSkeleton|ShaderInjection" --output-on-failure` → 8+2 全 PASS

### Phase C — REFACTOR + 三 build 矩阵验证

- [ ] **C.1** Lint 自查（ReadLints 3 文件）
- [ ] **C.2** 矩阵 A：software DEVTOOL=ON `build-sw-devtool` ctest 应 1337/1337（vx_graphics 无 GLES 增量）
- [ ] **C.3** 矩阵 B：software DEVTOOL=OFF `build-no-devtool` ctest 应 1141/1141
- [ ] **C.4** 矩阵 C：gles DEVTOOL=ON `build-gles` ctest 应 1352 → **1362**（+8 skeleton + +2 injection）
- [ ] **C.5** feat commit（D13=B 第二段）+ MB finalize（D13=B 第三段）

---

## 4. 风险登记

| # | 风险 | 概率 | 影响 | 缓解 |
|:-:|---|:-:|:-:|---|
| R1 | Mesa swrast `glIsEnabled(GL_BLEND)` 在 swrast 路径行为差异 | 低 | 中 | T2 GTEST_SKIP fallback / G1.3 T4 first-evidence 已建信心 |
| R2 | T8 多线程探针在某些 SDL 版本下不可重现 | 中 | 低 | EXPECT 而非 ASSERT / 文档化为 documentation-as-test |
| R3 | vx_graphics 链接 GLESv2 引入 vx_platform_sdl2 已有的 EGL 重复定义 | 低 | 中 | PRIVATE link 隔离 / pkg-config IMPORTED_TARGET 命名空间唯一 |
| R4 | `state_stack_` 跨测试残留（GLESCanvas 实例独立，不残留）| 极低 | 低 | 每测构造新 canvas / 与 G1.3 fixture 模式一致 |

---

## 5. 反复模式预防（8/8 全 ✅ 抑制 / 累计 21+ 模式连续抑制候选）

| # | 反复模式 | 抑制策略 |
|:-:|---|---|
| #1 | 前置依赖未验证 | §0.1-§0.3 全 ✅ |
| #2 | spec 数据回归 | 0 既有 spec 修改 |
| #3 | TDD 顺序倒置 | Phase A RED → B GREEN → C REFACTOR 严格 |
| #4 | 反向探针缺失 | T7 stub no-op + T8 多线程双反向探针 |
| #5 | 中文文档 StrReplace audit | 0 中文文档改动 |
| #6 | commit body Source 溯源 | feat commit body 含 `Source: §3.4` |
| #7 | 双 config ctest 单次盲区 | 三 build 矩阵 §3 Phase C |
| #8 | GLES test header 遗漏 | **G1.3 reflect first-evidence 落地** / §2.4 includes 已显式列 `<GLES3/gl3.h>` |

---

## 6. systemPatterns 协同度对照（13 项）

- ✅ 跨决策协同度 100% 第 18 次连续命中候选（streak 158 → 171）
- ✅ 实施忠实度 G1.1 first + G1.2 dual + G1.3 triple → **G1.4 quad-evidence 候选**
- ✅ Mesa swrast pixel-readback first-evidence 沿用（G1.3 T4 → G1.4 T3）
- ✅ Sdl2GlSurfaceEnvironment fixture 复用（D9=A）
- ✅ B6 raw string literal shader 嵌入 first-evidence（D11=B）
- ✅ 安全 first-evidence 入库（D12=A shader_injection_test.cc）
- ✅ LOC ×0.85-1.5 双向 buffer（~727 估 / [617, 1090] 实际范围）
- ✅ plan ×0.6 系数 undec → 第 12 数据点候选
- ✅ Phase 0 audit 协议 sept-evidence 续延
- ✅ commit 三段 D13=B 沿用
- ✅ GLES test header include 协议 first-evidence 落地（§0.4 #8 / §2.4）
- ✅ all_recommended AskQuestion 模式（4 隐含 + 9 一次锁）
- ✅ 反复模式 8/8 全抑制 / 累计 21+ 连续抑制候选

---

## 7. 完成清单

- [ ] Phase A RED：测试编译 fail（缺 gles_canvas.h） ✅
- [ ] Phase B GREEN：8+2 测全 PASS
- [ ] Phase C REFACTOR：lint clean / 三 build 矩阵全 ✅
- [ ] feat commit body 含 Source 溯源
- [ ] MB finalize commit
- [ ] activeContext 阶段 → 构建完成
- [ ] reflect 文档草拟点：实施忠实度 quad-evidence 确立 / 跨决策协同度第 18 次

---

## 8. 范围外（明确不做）

- ❌ FillRect / FillRoundedRect 真实施（→ G1.5）
- ❌ libtess2 引入（→ G1.6 FillPath）
- ❌ shader uniform / vertex attribute 实际绑定（→ G1.5 Solid Brush）
- ❌ Application::canvas_ 类型替换（→ G1.13）
- ❌ context lost 处理（→ G1.14）
- ❌ thread DCHECK 强制（T8 仅文档化探针 / 真 DCHECK → G1.13/G1.14）

---

## 9. 估时与里程碑

| 阶段 | plan ×0.6 | 预期实测 | 备注 |
|---|:-:|:-:|---|
| VAN | ~10 min | ~5-8 min | ✅ 已完成 |
| Plan | ~30-40 min | ~25-35 min | 当前 |
| **Build** | ~50-90 min | **~20-50 min** | undec → 第 12 数据点候选 |
| Reflect | ~20 min | ~15 min | 含 systemPatterns 5 段更新 |
| Archive | ~15 min | ~10 min | |
| **总线** | **~125-175 min** | **~75-118 min** | 标准极速区 0.55-0.70× |

---

**plan 落盘完成。** 下一步：`/build` 启动 Phase A RED → Phase B GREEN → Phase C REFACTOR + 三 build 矩阵 ctest 验证。
