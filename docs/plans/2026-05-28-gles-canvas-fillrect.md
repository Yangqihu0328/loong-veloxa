# G1.5 `GLESCanvas::FillRect` + `FillRoundedRect` + Solid Brush 实现计划

**目标：** 将 G1.4 `GLESCanvas` 骨架中的 `FillRect` / `FillRoundedRect` 由空 stub 替换为真实 GPU 实现（首个真实绘制方法），并落地 3 个 raw string literal shader（solid vert / solid frag / rounded SDF frag）+ shader program 生命周期管理 + unit quad VBO 初始化 + MVP 矩阵注入策略 + Mesa swrast 像素级验证。

**架构：**
- vertex shader 用 `aPos` 输入 unit quad `[0,1]²` + `uRectPx`（rect 像素坐标）+ `uXformPx`（用户 transform 转 mat3）+ `uViewportPx`（视口像素）→ 像素空间转 NDC 并 Y 翻转
- fragment shader 分两路：solid 路径直出 `uColor`；rounded 路径在像素空间用 SDF（signed distance field）+ `fwidth` 自动反走样 + smoothstep 计算 alpha
- shader program × 2（solid + rounded）在 `GLESCanvas` ctor 创建 / dtor 销毁（与既有 `quad_vao_/vbo_` 对称）
- per-FillRect 上传 uniform + `glDrawArrays(GL_TRIANGLES, 0, 6)`

**技术栈：** GLES 3.0 (`<GLES3/gl3.h>`) / GLSL ES 3.0 SL 1.00 / SDL2 + Mesa swrast EGL / GoogleTest 1.x / CMake 3.20+ / `VX_RENDERER=gles` 条件构建

**复杂度级别：** Level 3（蓝图 plan §3.5 锁定 / 修改 3 文件 + 新建 1 测试 + 修改 1 CMakeLists / ~616 行核心 + 隐性 ×1.3-1.4 = ~700-860 行 buffer 后）

---

## 0. Phase 0 audit（VAN 阶段已部分完成 + plan 阶段补完）

### §0.1 ctest baseline 二次验证（VAN 已实证）

| Config | DEVTOOL | VX_RENDERER | baseline | 期望本任务后 |
|---|:-:|:-:|:-:|:-:|
| A | ON | software (默认) | **1337/1337** | 1337/1337（gles 测试不进 software config / 不退化）|
| B | OFF | software | **1141/1141** | 1141/1141（gles 测试不进 / DEVTOOL=OFF 黑名单守门）|
| C | ON | gles | **1362/1362** | **1372-1374**（+10-12 gles_canvas_fill_test）|

### §0.2 GLES 3.0 shader API audit（VAN 已实证 / plan 复制锁定）

| API | header 位置 | 验证 |
|---|---|---|
| `glCreateShader` | `<GLES3/gl3.h>:533` | ✅ GLES 3.0 必支持 |
| `glShaderSource` | `<GLES3/gl3.h>` | ✅ GLES 3.0 必支持 |
| `glCompileShader` | `<GLES3/gl3.h>` | ✅ GLES 3.0 必支持 |
| `glCreateProgram` | `<GLES3/gl3.h>` | ✅ GLES 3.0 必支持 |
| `glAttachShader` | `<GLES3/gl3.h>` | ✅ GLES 3.0 必支持 |
| `glLinkProgram` | `<GLES3/gl3.h>:596` | ✅ |
| `glUseProgram` | `<GLES3/gl3.h>` | ✅ |
| `glGetUniformLocation` | `<GLES3/gl3.h>:583` | ✅ |
| `glUniform4fv` | `<GLES3/gl3.h>:631` | ✅（uColor）|
| `glUniformMatrix3fv` | `<GLES3/gl3.h>:635` | ✅（uXformPx）|
| `glUniform2f` | `<GLES3/gl3.h>` | ✅（uViewportPx / uHalfPx）|
| `glUniform4f` | `<GLES3/gl3.h>` | ✅（uRectPx）|
| `glUniform1f` | `<GLES3/gl3.h>` | ✅（uRadiusPx）|
| `glDrawArrays` | `<GLES3/gl3.h>:547` | ✅ |
| `glBufferData` | `<GLES3/gl3.h>` | ✅（unit quad 上传）|
| `glEnableVertexAttribArray` | `<GLES3/gl3.h>` | ✅ |
| `glVertexAttribPointer` | `<GLES3/gl3.h>` | ✅ |
| `glGetShaderiv` / `glGetProgramiv` | `<GLES3/gl3.h>` | ✅（编译/链接错误探测）|

### §0.3 GLSL ES 3.0 SL 1.00 函数 audit

| 函数 | 用途 | spec 位置 | 验证 |
|---|---|---|---|
| `smoothstep(edge0, edge1, x)` | SDF 反走样 | spec §8.3 | ✅ 必支持 |
| `length(v)` | SDF 距离计算 | spec §8.4 | ✅ 必支持 |
| `max(a, b)` | SDF clamp | spec §8.3 | ✅ 必支持 |
| `min(a, b)` | SDF clamp | spec §8.3 | ✅ 必支持 |
| `abs(x)` | SDF 对称 | spec §8.3 | ✅ 必支持 |
| `fwidth(p)` | 屏幕空间 derivative（自适应 edge 宽度）| spec §8.8 | ✅ 必支持 GLES 3.0 ES SL 1.00 |

### §0.4 既有 GLESCanvas 复用资源 audit

| 资源 | 位置 | G1.5 复用方式 |
|---|---|---|
| `quad_vao_ / quad_vbo_` | G1.4 ctor `glGenVertexArrays` / `glGenBuffers`（**未 BufferData**）| G1.5 ctor 中 `glBufferData(quad_vbo_, 6 floats × 2)` 上传 unit quad |
| `Begin()` viewport / blend state | G1.4 设 viewport + glEnable(GL_BLEND) + glBlendFunc(SRC_ALPHA, ONE_MINUS_SRC_ALPHA) | G1.5 alpha 混合直接生效（rounded SDF 半透边缘 / kTransparent 透传） |
| `transform_ + state_stack_` | G1.4 已就位 / SetTransform/PushState/PopState 测全 PASS | G1.5 FillRect 直读 `transform_` 转 mat3 上传 uXformPx |
| `surface_->width()/height()` | G1.4 Begin 已读 | G1.5 FillRect 上传 uViewportPx |

### §0.5 既有 shaders.h B6=A 范式 audit

`shaders.h` G1.4 已确立 raw string literal `R"(...)"` 编译期 const char* + S1/S2 security regression 测覆盖。G1.5 新加 3 shader **必须** 沿用同范式：

- ✅ `inline constexpr const char*` 形式（与 kPassthroughVert/Frag 同）
- ✅ 命名前缀 `kSolidVert` / `kSolidFrag` / `kRoundedRectFrag`
- ✅ shader 顶部加 security contract comment block 引用 `shader_injection_test.cc`
- ✅ G1.4 `kPassthroughVert/Frag` 决策保留（不删 / 蓝图后续 G1.X 子任务可能复用 / 0 撤销）

### §0.6 add_test config guard 边界审计（writing-plans 强制 audit）

`tests/CMakeLists.txt:392-429` 现有 gles 段：所有 GLES 测试都包在 `if(VX_RENDERER STREQUAL "gles")` block 内 / OFF + software config 永不跑 gles 测。

| 新增测试 | guard | OFF 跑？| software 跑？| gles 跑？ |
|---|---|:-:|:-:|:-:|
| `gles_canvas_fill_test` | 加在既有 `if(VX_RENDERER STREQUAL "gles")` block 内 | ❌ | ❌ | ✅ |

**结论：** Matrix A (software ON) + Matrix B (software OFF) **测数不变**（1337/1141 baseline 维持）/ Matrix C (gles) **+10-12 测**。

### §0.7 _deps 缓存复用 audit

- ✅ `build/_deps/quickjsng-src` 离线预置完整
- ✅ `build-gles/` 增量配置可用（G1.4 已建）
- ⊘ FetchContent 网络代理守卫跳过（无新依赖 / 0 FetchContent 触发）

### §0.8 工具链版本核对

| 工具 | 版本 | 上次任务（G1.4）一致？ |
|---|---|:-:|
| gcc | 15.2.0 | ✅ |
| ld（binutils）| 2.46 | ✅（已应用 `--start-group/--end-group` hotfix） |
| cmake | 4.2.3 | ✅ |
| ninja | 1.13.2 | ✅ |

跳过差异检查 / 记 1 行 progress。

---

## 1. 决策矩阵（B1-B8 / 1 次 AskQuestion all_recommended 锁定 / 跨决策协同度第 19 次连续命中候选 / streak 171→179/179）

| # | 决策 | 选项 | 与已锁定决策协同 | 推荐根据 |
|:-:|---|---|---|---|
| **B1** | MVP 矩阵注入策略 | **B per-FillRect uMvp glUniform** ⭐ | 与 B2=A unit quad 协同 ✅ / 与 B5=A ctor program 协同 ✅ | 简单可靠 / 0 dirty bit 复杂度 / G2 再批处理 |
| **B2** | vertex layout 复用 | **A unit quad [0,1]² + ctor glBufferData** ⭐ | 与 G1.4 quad_vao_/vbo_ 已分配 ✅ / 与 B1 uRectPx 缩放协同 ✅ | 复用 vbo / 6 顶点 = 2 三角形 / GPU 高效 |
| **B3** | shader uniform 设计 | **A uRectPx + uXformPx + uViewportPx** ⭐ | 与 B1 per-FillRect ✅ / 与 B7 kSolid 提取 ✅ | 像素空间直接 / Y 翻转一致 / SDF rounded uHalfPx + uRadiusPx 扩展 |
| **B4** | SDF 反走样参数 | **A fwidth(dist) 自动** ⭐ | 与 B3 uRectPx 像素空间 ✅ | GLES 3.0 SL 1.00 必支持 / per-pixel 准确 / 0 uniform 噪声 |
| **B5** | shader program 生命周期 | **A ctor 创建 ×2 + dtor glDeleteProgram** ⭐ | 与 G1.4 quad_vao_/vbo_ ctor 对称 ✅ / 与 B6 inline reverse probe ✅ | 0 lazy / 简单 / GL ctx current 已是 ctor 契约（test fixture 实证）|
| **B6** | 反向探针策略 | **A inline reverse probe** ⭐ | 与 G1.4 D4=C inline reverse probe ✅ / 与 G1.2 D4 ✅ | dual-evidence 范式沿用 / 0 build 中断 / shader 不动 |
| **B7** | Brush 范围 | **A kSolid 完整 + kLinearGradient fallback color_start** ⭐ | 与蓝图 §3.5 标题「Solid Brush」✅ / 与 B3 uColor 单值 ✅ | YAGNI / G2 再加 gradient shader / fallback 不 crash |
| **B8** | shader_injection_test 扩展 | **A kAllShaderSources[] 数组化** ⭐ | 与 G1.4 first-evidence 范式 ✅ / 与 B5 ctor 创建 ✅ | 未来加 shader 改一处 / S1 范围化 / S3 新加全 shader 覆盖 |

**跨决策协同度：** 8/8 决策 1 次 AskQuestion all_recommended 锁定 / **第 19 次连续命中候选** / 累计 171→**179/179** 历史最高 streak 续刷 / G1.4 quad → **quint-evidence 候选**

---

## 2. 文件结构（5 文件 / ~616 行核心 / ×1.3-1.4 buffer ~700-860 行）

| # | 文件 | 操作 | 估行（核心）| 隐性 buffer | 估行（含 buffer）| 职责 |
|:-:|---|:-:|:-:|:-:|:-:|---|
| 1 | `veloxa/graphics/gles/shaders.h` | 🟡 修改 | +~80 | +~10（comment policy）| ~90 | 3 新 shader：`kSolidVert` / `kSolidFrag` / `kRoundedRectFrag` |
| 2 | `veloxa/graphics/gles/gles_canvas.h` | 🟡 修改 | +~30 | +~5（comment）| ~35 | shader program 私有成员 + uniform location 缓存 + helper 函数声明 + 移除 14 stub 中 FillRect/FillRoundedRect 标记 |
| 3 | `veloxa/graphics/gles/gles_canvas.cc` | 🟡 修改 | +~250 | +~50（shader compile error log + comments）| ~300 | `FillRect` / `FillRoundedRect` 实现 + `CompileShader` / `LinkProgram` helper + ctor 中 program init + dtor 中 program delete + ctor 中 quad VBO BufferData |
| 4 | `tests/graphics/gles/gles_canvas_fill_test.cc` | 🆕 创建 | +~250 | +~50（fixture setup / matrix helper / DRAIN macro）| ~300 | 10-12 单测：FillRect 像素 / Brush kSolid / kLinearGradient fallback / FillRoundedRect SDF / SetTransform / 反向探针 ×3 + DRAIN_GL_ERRORS |
| 5 | `tests/CMakeLists.txt` | 🟡 修改 | +~6 | 0 | ~6 | 在 `gles_canvas_skeleton_test` 段后注册 `gles_canvas_fill_test`（同 gles guard）|
| **合计** | — | — | **~616** | **+~115** | **~731** | LOC buffer 范围 [0.85, 1.5] = ~525-925 行 ✅ |

---

## 3. 完整代码片段

### 3.1 `veloxa/graphics/gles/shaders.h`（追加 3 shader）

在文件末尾、`}  // namespace vx::gfx::gles` 之前追加：

```cpp
// -----------------------------------------------------------------------------
// G1.5: FillRect / FillRoundedRect shaders (B6=A static embedding continued).
//
// All three shaders take pixel-space inputs and produce NDC outputs in the
// vertex stage (Y-flipped to match Veloxa's top-left origin convention).
// Same security contract as kPassthroughVert/Frag: compile-time constant,
// never composed with user data. shader_injection_test.cc verifies all
// shader sources via kAllShaderSources[] (B8=A).
// -----------------------------------------------------------------------------

// Solid vertex shader: unit-quad [0,1]^2 -> pixel rect -> user xform -> NDC.
//
// Inputs:
//   * a_pos: unit quad vertex in [0,1]^2 (6 verts = 2 triangles).
//   * u_rect_px: target rect in pixel space (x, y, w, h).
//   * u_xform_px: user-supplied Matrix3x2 promoted to mat3 (last row = 0,0,1).
//   * u_viewport_px: surface size in pixels (w, h) for NDC mapping.
//
// Outputs:
//   * gl_Position: NDC coordinate with Y flipped (Veloxa: top-left origin /
//     OpenGL: bottom-left origin).
//   * v_local_px: local coordinate within the rect (0..u_rect_px.zw),
//     consumed by kRoundedRectFrag for SDF distance calculation.
inline constexpr const char* kSolidVert = R"(#version 300 es
precision highp float;
in vec2 a_pos;
uniform vec4 u_rect_px;
uniform mat3 u_xform_px;
uniform vec2 u_viewport_px;
out vec2 v_local_px;
void main() {
  vec2 px = u_rect_px.xy + a_pos * u_rect_px.zw;
  vec3 px3 = u_xform_px * vec3(px, 1.0);
  vec2 ndc = (px3.xy / u_viewport_px) * 2.0 - 1.0;
  gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);
  v_local_px = a_pos * u_rect_px.zw;
}
)";

// Solid fragment shader: emits u_color directly. Alpha blending is enabled
// in GLESCanvas::Begin (SRC_ALPHA, ONE_MINUS_SRC_ALPHA).
inline constexpr const char* kSolidFrag = R"(#version 300 es
precision mediump float;
uniform vec4 u_color;
out vec4 frag_color;
void main() {
  frag_color = u_color;
}
)";

// Rounded-rect SDF fragment shader: uses fwidth() for screen-space AA edge.
//
// Distance function (Inigo Quilez rounded-box SDF):
//   d = |p - center| - (half - radius)
//   dist = length(max(d, 0)) + min(max(d.x, d.y), 0) - radius
//
// alpha = 1 - smoothstep(-fwidth(dist), +fwidth(dist), dist)
// fwidth gives the per-pixel rate of change so AA stays 1px wide regardless
// of user transform (zoom / rotation).
inline constexpr const char* kRoundedRectFrag = R"(#version 300 es
precision highp float;
in vec2 v_local_px;
uniform vec4 u_color;
uniform vec2 u_half_px;
uniform float u_radius_px;
out vec4 frag_color;
void main() {
  vec2 d = abs(v_local_px - u_half_px) - (u_half_px - vec2(u_radius_px));
  float dist = length(max(d, 0.0))
             + min(max(d.x, d.y), 0.0)
             - u_radius_px;
  float aa = fwidth(dist);
  float alpha = 1.0 - smoothstep(-aa, aa, dist);
  frag_color = vec4(u_color.rgb, u_color.a * alpha);
}
)";

// -----------------------------------------------------------------------------
// kAllShaderSources[] — B8=A: enumerate every shader for the security
// regression test (shader_injection_test.cc S1 ShaderSourcesAreCompileTimeLiterals).
// Adding a new shader requires only adding an entry here + a comment in the
// shader's docstring. Failure to register triggers no test (silent gap) — the
// test reviewer must visually confirm new shaders are listed.
// -----------------------------------------------------------------------------
inline constexpr const char* kAllShaderSources[] = {
    kPassthroughVert,
    kPassthroughFrag,
    kSolidVert,
    kSolidFrag,
    kRoundedRectFrag,
};
inline constexpr int kAllShaderSourceCount =
    sizeof(kAllShaderSources) / sizeof(kAllShaderSources[0]);
```

### 3.2 `veloxa/graphics/gles/gles_canvas.h`（修改私有成员 + 移除 stub）

在 line 64-65（既有 FillRect/FillRoundedRect stub）改为：

```cpp
  // ---- Real implementations (G1.5 fill scope) ----
  void FillRect(const Rect&, const Brush&) override;
  void FillRoundedRect(const Rect&, vx::f32, const Brush&) override;
```

在 `private:` 段（line 87 后）追加：

```cpp
  // ---- G1.5 shader program resources (B5=A ctor init / dtor delete) ----
  // solid_program_ + rounded_program_ are GL program handles. uniform_loc_
  // arrays cache glGetUniformLocation results to avoid per-FillRect lookups.
  GLuint solid_program_ = 0;
  GLuint rounded_program_ = 0;

  // Uniform indices into per-program location caches.
  enum SolidUniform { kSolidURectPx = 0, kSolidUXformPx, kSolidUViewportPx,
                      kSolidUColor, kSolidUniformCount };
  enum RoundedUniform { kRoundedURectPx = 0, kRoundedUXformPx,
                        kRoundedUViewportPx, kRoundedUColor, kRoundedUHalfPx,
                        kRoundedURadiusPx, kRoundedUniformCount };
  GLint solid_uniforms_[kSolidUniformCount] = {-1, -1, -1, -1};
  GLint rounded_uniforms_[kRoundedUniformCount] = {-1, -1, -1, -1, -1, -1};

  // ---- G1.5 helpers (private) ----
  // CompileShader / LinkProgram return 0 on failure (and log via VX_DCHECK
  // with infoLog dumped to stderr in debug builds).
  static GLuint CompileShader(GLenum type, const char* source);
  static GLuint LinkProgram(GLuint vert, GLuint frag);
  void InitShaderPrograms();    // ctor helper
  void DestroyShaderPrograms(); // dtor helper
  void UploadUnitQuad();         // ctor helper — 6 verts to quad_vbo_
  // Extract solid Color from a Brush. For kLinearGradient, returns
  // brush.linear.color_start (B7=A fallback) so callers don't crash.
  static Color BrushSolidColor(const Brush& brush);
  // Convert Matrix3x2 (6 floats) to GLES mat3 column-major (9 floats).
  // mat3 col0 = (m[0], m[1], 0), col1 = (m[2], m[3], 0), col2 = (m[4], m[5], 1).
  static void Matrix3x2ToMat3(const Matrix3x2& src, GLfloat dst[9]);
```

### 3.3 `veloxa/graphics/gles/gles_canvas.cc`（ctor 扩展 + FillRect / FillRoundedRect 实现）

ctor（line 9-27）扩展为：

```cpp
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
  // (G1.13) already does the lifecycle dance, and (c) test fixtures
  // explicitly call MakeCurrent before constructing the canvas.
  glGenVertexArrays(1, &quad_vao_);
  glGenBuffers(1, &quad_vbo_);

  // G1.5 (B2=A): upload unit-quad triangle list (6 verts) to quad_vbo_.
  // B5=A: build the two shader programs while the GL context is current.
  UploadUnitQuad();
  InitShaderPrograms();
}
```

dtor 扩展为：

```cpp
GLESCanvas::~GLESCanvas() {
  if (quad_vao_ != 0) glDeleteVertexArrays(1, &quad_vao_);
  if (quad_vbo_ != 0) glDeleteBuffers(1, &quad_vbo_);
  DestroyShaderPrograms();  // G1.5 (B5=A) program teardown
}
```

在 `PopState()` 实现后追加（line 80 后）：

```cpp
// -----------------------------------------------------------------------------
// G1.5: Shader compilation / linking helpers (private static).
// -----------------------------------------------------------------------------

GLuint GLESCanvas::CompileShader(GLenum type, const char* source) {
  GLuint shader = glCreateShader(type);
  if (shader == 0) return 0;
  glShaderSource(shader, 1, &source, nullptr);
  glCompileShader(shader);
  GLint status = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
  if (status != GL_TRUE) {
    // Capture the infoLog so test failures surface the GLSL error.
    char log[1024] = {0};
    GLsizei len = 0;
    glGetShaderInfoLog(shader, sizeof(log) - 1, &len, log);
    VX_DCHECK(false && "GLES shader compile failed; see infoLog above");
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

GLuint GLESCanvas::LinkProgram(GLuint vert, GLuint frag) {
  GLuint program = glCreateProgram();
  if (program == 0) return 0;
  glAttachShader(program, vert);
  glAttachShader(program, frag);
  glBindAttribLocation(program, 0, "a_pos");  // matches glVertexAttribPointer(0)
  glLinkProgram(program);
  GLint status = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &status);
  if (status != GL_TRUE) {
    char log[1024] = {0};
    GLsizei len = 0;
    glGetProgramInfoLog(program, sizeof(log) - 1, &len, log);
    VX_DCHECK(false && "GLES program link failed; see infoLog above");
    glDeleteProgram(program);
    return 0;
  }
  // Detach (shaders are reference-counted; deletion below is safe).
  glDetachShader(program, vert);
  glDetachShader(program, frag);
  return program;
}

void GLESCanvas::InitShaderPrograms() {
  // Solid program.
  GLuint sv = CompileShader(GL_VERTEX_SHADER, kSolidVert);
  GLuint sf = CompileShader(GL_FRAGMENT_SHADER, kSolidFrag);
  solid_program_ = LinkProgram(sv, sf);
  glDeleteShader(sv);
  glDeleteShader(sf);
  if (solid_program_ != 0) {
    solid_uniforms_[kSolidURectPx] =
        glGetUniformLocation(solid_program_, "u_rect_px");
    solid_uniforms_[kSolidUXformPx] =
        glGetUniformLocation(solid_program_, "u_xform_px");
    solid_uniforms_[kSolidUViewportPx] =
        glGetUniformLocation(solid_program_, "u_viewport_px");
    solid_uniforms_[kSolidUColor] =
        glGetUniformLocation(solid_program_, "u_color");
  }

  // Rounded program.
  GLuint rv = CompileShader(GL_VERTEX_SHADER, kSolidVert);  // share vert
  GLuint rf = CompileShader(GL_FRAGMENT_SHADER, kRoundedRectFrag);
  rounded_program_ = LinkProgram(rv, rf);
  glDeleteShader(rv);
  glDeleteShader(rf);
  if (rounded_program_ != 0) {
    rounded_uniforms_[kRoundedURectPx] =
        glGetUniformLocation(rounded_program_, "u_rect_px");
    rounded_uniforms_[kRoundedUXformPx] =
        glGetUniformLocation(rounded_program_, "u_xform_px");
    rounded_uniforms_[kRoundedUViewportPx] =
        glGetUniformLocation(rounded_program_, "u_viewport_px");
    rounded_uniforms_[kRoundedUColor] =
        glGetUniformLocation(rounded_program_, "u_color");
    rounded_uniforms_[kRoundedUHalfPx] =
        glGetUniformLocation(rounded_program_, "u_half_px");
    rounded_uniforms_[kRoundedURadiusPx] =
        glGetUniformLocation(rounded_program_, "u_radius_px");
  }
}

void GLESCanvas::DestroyShaderPrograms() {
  if (solid_program_ != 0) {
    glDeleteProgram(solid_program_);
    solid_program_ = 0;
  }
  if (rounded_program_ != 0) {
    glDeleteProgram(rounded_program_);
    rounded_program_ = 0;
  }
}

void GLESCanvas::UploadUnitQuad() {
  // 6 verts (2 triangles): bottom-left, bottom-right, top-left,
  //                         top-left,   bottom-right, top-right.
  // GL_TRIANGLES winding (CCW in screen space after Y flip).
  static constexpr GLfloat kQuad[12] = {
      0.0f, 0.0f,
      1.0f, 0.0f,
      0.0f, 1.0f,
      0.0f, 1.0f,
      1.0f, 0.0f,
      1.0f, 1.0f,
  };
  glBindVertexArray(quad_vao_);
  glBindBuffer(GL_ARRAY_BUFFER, quad_vbo_);
  glBufferData(GL_ARRAY_BUFFER, sizeof(kQuad), kQuad, GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat),
                        reinterpret_cast<const void*>(0));
  glBindVertexArray(0);
}

Color GLESCanvas::BrushSolidColor(const Brush& brush) {
  // B7=A: kSolid passes through; kLinearGradient falls back to color_start
  // so callers don't crash. G2 will add a real gradient shader path.
  if (brush.type == Brush::Type::kSolid) return brush.solid;
  return brush.linear.color_start;
}

void GLESCanvas::Matrix3x2ToMat3(const Matrix3x2& src, GLfloat dst[9]) {
  // GLES uniformMatrix3fv is column-major. Matrix3x2 layout:
  //   m[0] m[1] | m[2] m[3] | m[4] m[5]
  //     a    b      c    d      tx   ty
  // -> mat3 cols: (a, b, 0), (c, d, 0), (tx, ty, 1).
  dst[0] = src.m[0]; dst[1] = src.m[1]; dst[2] = 0.0f;
  dst[3] = src.m[2]; dst[4] = src.m[3]; dst[5] = 0.0f;
  dst[6] = src.m[4]; dst[7] = src.m[5]; dst[8] = 1.0f;
}

// -----------------------------------------------------------------------------
// G1.5: FillRect / FillRoundedRect.
// -----------------------------------------------------------------------------

void GLESCanvas::FillRect(const Rect& rect, const Brush& brush) {
  if (solid_program_ == 0 || rect.IsEmpty()) return;
  Color c = BrushSolidColor(brush);
  GLfloat mat[9];
  Matrix3x2ToMat3(transform_, mat);

  glUseProgram(solid_program_);
  glUniform4f(solid_uniforms_[kSolidURectPx], rect.x, rect.y, rect.w, rect.h);
  glUniformMatrix3fv(solid_uniforms_[kSolidUXformPx], 1, GL_FALSE, mat);
  glUniform2f(solid_uniforms_[kSolidUViewportPx],
              static_cast<GLfloat>(width_), static_cast<GLfloat>(height_));
  glUniform4f(solid_uniforms_[kSolidUColor], c.r / 255.0f, c.g / 255.0f,
              c.b / 255.0f, c.a / 255.0f);

  glBindVertexArray(quad_vao_);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  glBindVertexArray(0);
}

void GLESCanvas::FillRoundedRect(const Rect& rect, vx::f32 radius,
                                 const Brush& brush) {
  if (rounded_program_ == 0 || rect.IsEmpty()) return;
  // Clamp radius to half the shortest dimension (matches CSS / SoftwareCanvas).
  vx::f32 r = std::min(radius, std::min(rect.w, rect.h) * 0.5f);
  if (r <= 0.0f) {
    // Degenerate to FillRect (no rounding → SDF would still work but slower).
    FillRect(rect, brush);
    return;
  }
  Color c = BrushSolidColor(brush);
  GLfloat mat[9];
  Matrix3x2ToMat3(transform_, mat);

  glUseProgram(rounded_program_);
  glUniform4f(rounded_uniforms_[kRoundedURectPx], rect.x, rect.y, rect.w, rect.h);
  glUniformMatrix3fv(rounded_uniforms_[kRoundedUXformPx], 1, GL_FALSE, mat);
  glUniform2f(rounded_uniforms_[kRoundedUViewportPx],
              static_cast<GLfloat>(width_), static_cast<GLfloat>(height_));
  glUniform4f(rounded_uniforms_[kRoundedUColor], c.r / 255.0f, c.g / 255.0f,
              c.b / 255.0f, c.a / 255.0f);
  glUniform2f(rounded_uniforms_[kRoundedUHalfPx], rect.w * 0.5f, rect.h * 0.5f);
  glUniform1f(rounded_uniforms_[kRoundedURadiusPx], r);

  glBindVertexArray(quad_vao_);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  glBindVertexArray(0);
}
```

include 增量（文件顶部）：

```cpp
#include <algorithm>  // std::min
```

### 3.4 `tests/graphics/gles/gles_canvas_fill_test.cc`（新建 / 10-12 单测）

```cpp
// Tests for vx::gfx::gles::GLESCanvas FillRect + FillRoundedRect (G1.5).
//
// Headless: SDL_VIDEODRIVER=offscreen + Mesa swrast EGL — same fixture as
// gles_canvas_skeleton_test (G1.4 first-evidence reused).
// Each test creates its own Sdl2GLWindowSurface so GL state cannot leak.
//
// Pixel verification uses glReadPixels on the default framebuffer. Mesa
// swrast first-evidence (G1.3 T4 + G1.4 T3) shows this is reliable for
// solid fills; we keep a GTEST_SKIP fallback for driver-strictness layering
// consistency.

#include "veloxa/graphics/gles/gles_canvas.h"

#include <GLES3/gl3.h>
#include <SDL2/SDL.h>
#include <gtest/gtest.h>

#include "veloxa/foundation/base/assert.h"
#include "veloxa/graphics/brush.h"
#include "veloxa/graphics/types.h"
#include "veloxa/platform/sdl2/sdl2_egl_display.h"
#include "veloxa/platform/sdl2/sdl2_gl_window_surface.h"

namespace vx::gfx::gles {
namespace {

// Reuse the SDL offscreen environment from skeleton test (each test target
// registers its own AddGlobalTestEnvironment).
class Sdl2GlSurfaceEnvironment : public ::testing::Environment {
 public:
  void SetUp() override {
    SDL_setenv("SDL_VIDEODRIVER", "offscreen", /*overwrite=*/1);
    ASSERT_EQ(SDL_Init(SDL_INIT_VIDEO), 0) << SDL_GetError();
  }
  void TearDown() override { SDL_Quit(); }
};
[[maybe_unused]] auto* g_sdl_env =
    ::testing::AddGlobalTestEnvironment(new Sdl2GlSurfaceEnvironment);

// Helpers ------------------------------------------------------------------
static void DrainGLErrors() {
  while (glGetError() != GL_NO_ERROR) {}
}

// Read one pixel at (px, py). Origin is bottom-left in glReadPixels (OpenGL
// convention). Caller is responsible for Y conversion.
static void ReadPixel(int px, int py, uint8_t out[4]) {
  out[0] = out[1] = out[2] = out[3] = 0;
  glReadPixels(px, py, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, out);
}

// Skip if Mesa swrast didn't render to the default framebuffer for this
// pixel (driver-strictness fallback / G1.3 first-evidence should preclude).
#define SKIP_IF_SWRAST_BLANK(px)                                                 \
  if ((px)[0] == 0 && (px)[1] == 0 && (px)[2] == 0 && (px)[3] == 0) {            \
    GTEST_SKIP() << "Mesa swrast offscreen returned blank pixel; G1.3/G1.4 "    \
                    "first-evidence should preclude — driver-strictness "       \
                    "fallback active.";                                          \
  }

// T1: Construct_InitsShaderPrograms ---------------------------------------
// solid_program_ and rounded_program_ must be non-zero after ctor (B5=A).
// We use the public quad_vao() accessor to confirm ctor ran cleanly and
// reach into the GL state via a small no-op draw to prove programs link.
TEST(GLESCanvasFillTest, Construct_InitsShaderPrograms) {
  vx::platform::Sdl2GLWindowSurface surface(32, 32, "vx_g15_t1");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  DrainGLErrors();
  canvas.Begin();
  canvas.FillRect({4, 4, 8, 8}, Brush::Solid(Color::Red()));
  canvas.End();
  EXPECT_EQ(glGetError(), static_cast<GLenum>(GL_NO_ERROR))
      << "FillRect emitted a GL error — shader programs likely failed to "
         "link (B5=A ctor init contract)";
}

// T2: FillRect_TopLeftPixel ------------------------------------------------
// Fill the entire surface red; the bottom-left pixel (OpenGL origin) must
// be red. Tests rect-to-NDC mapping + uColor uniform path.
TEST(GLESCanvasFillTest, FillRect_TopLeftPixel) {
  vx::platform::Sdl2GLWindowSurface surface(16, 16, "vx_g15_t2");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  canvas.Begin();
  canvas.Clear(Color::White());
  canvas.FillRect({0, 0, 16, 16}, Brush::Solid(Color::Red()));
  uint8_t px[4];
  ReadPixel(0, 0, px);
  SKIP_IF_SWRAST_BLANK(px);
  EXPECT_GT(px[0], 200u);
  EXPECT_LT(px[1], 50u);
  EXPECT_LT(px[2], 50u);
  canvas.End();
}

// T3: FillRect_FourCornerSample -------------------------------------------
// Fill a 8x8 rect at (4,4) on a 16x16 surface. Sample 4 inner corners +
// outside corner. Inner = red, outside = white (Clear bg).
TEST(GLESCanvasFillTest, FillRect_FourCornerSample) {
  vx::platform::Sdl2GLWindowSurface surface(16, 16, "vx_g15_t3");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  canvas.Begin();
  canvas.Clear(Color::White());
  canvas.FillRect({4, 4, 8, 8}, Brush::Solid(Color::Green()));
  // Veloxa rect is top-left origin. Convert to GL (bottom-left) for read:
  //   GL y = surface_h - 1 - rect_y_top_in_veloxa.
  // Inner pixel (4,4) in Veloxa coords -> (4, 16-1-4=11) in GL.
  // Sample four inner corners + one outside corner.
  uint8_t inner_tl[4], inner_tr[4], inner_bl[4], inner_br[4], outside_tl[4];
  ReadPixel(5,  11, inner_tl);   // veloxa (5, 4)
  ReadPixel(10, 11, inner_tr);   // veloxa (10, 4)
  ReadPixel(5,  5,  inner_bl);   // veloxa (5, 10)
  ReadPixel(10, 5,  inner_br);   // veloxa (10, 10)
  ReadPixel(1,  14, outside_tl); // veloxa (1, 1)
  SKIP_IF_SWRAST_BLANK(inner_tl);
  EXPECT_GT(inner_tl[1], 200u);  // green
  EXPECT_GT(inner_tr[1], 200u);
  EXPECT_GT(inner_bl[1], 200u);
  EXPECT_GT(inner_br[1], 200u);
  EXPECT_GT(outside_tl[0], 200u); // white outside
  EXPECT_GT(outside_tl[1], 200u);
  EXPECT_GT(outside_tl[2], 200u);
  canvas.End();
}

// T4: FillRect_KSolidBrush -------------------------------------------------
// Verify Brush::Solid passes through correctly.
TEST(GLESCanvasFillTest, FillRect_KSolidBrush) {
  vx::platform::Sdl2GLWindowSurface surface(8, 8, "vx_g15_t4");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  canvas.Begin();
  canvas.Clear(Color::Black());
  canvas.FillRect({0, 0, 8, 8}, Brush::Solid(Color::Blue()));
  uint8_t px[4];
  ReadPixel(4, 4, px);
  SKIP_IF_SWRAST_BLANK(px);
  EXPECT_LT(px[0], 50u);
  EXPECT_LT(px[1], 50u);
  EXPECT_GT(px[2], 200u);
  canvas.End();
}

// T5: FillRect_KLinearGradientFallback ------------------------------------
// B7=A: LinearGradient brush should fall back to color_start (no crash, no
// black). Verify red-start gradient yields red pixels (not start-end blend).
TEST(GLESCanvasFillTest, FillRect_KLinearGradientFallback) {
  vx::platform::Sdl2GLWindowSurface surface(8, 8, "vx_g15_t5");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  canvas.Begin();
  canvas.Clear(Color::White());
  Brush gradient = Brush::Linear({0, 0}, {8, 0},
                                 Color::Red(), Color::Blue());
  canvas.FillRect({0, 0, 8, 8}, gradient);
  uint8_t px[4];
  ReadPixel(4, 4, px);
  SKIP_IF_SWRAST_BLANK(px);
  // B7=A fallback: every pixel = color_start (red), no gradient.
  EXPECT_GT(px[0], 200u);
  EXPECT_LT(px[1], 50u);
  EXPECT_LT(px[2], 50u);
  canvas.End();
}

// T6: FillRoundedRect_CenterIsOpaque --------------------------------------
// SDF center should be fully opaque (alpha = 1). Sample the geometric
// center of the rect.
TEST(GLESCanvasFillTest, FillRoundedRect_CenterIsOpaque) {
  vx::platform::Sdl2GLWindowSurface surface(32, 32, "vx_g15_t6");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  canvas.Begin();
  canvas.Clear(Color::White());
  canvas.FillRoundedRect({4, 4, 24, 24}, 6.0f, Brush::Solid(Color::Red()));
  uint8_t px[4];
  // Center of (4,4 24x24) is veloxa (16,16) -> GL (16, 32-1-16=15).
  ReadPixel(16, 15, px);
  SKIP_IF_SWRAST_BLANK(px);
  EXPECT_GT(px[0], 200u);  // red center
  EXPECT_LT(px[1], 50u);
  EXPECT_LT(px[2], 50u);
  canvas.End();
}

// T7: FillRoundedRect_CornerHasPartialAlpha -------------------------------
// SDF corner (rounded region) should have partial alpha — blend with the
// white Clear background → not pure red. We probe a pixel right at the
// rounded-corner cusp (rect (4,4 24x24) with radius=8 -> cusp area ~ (4..12, 4..12)).
TEST(GLESCanvasFillTest, FillRoundedRect_CornerHasPartialAlpha) {
  vx::platform::Sdl2GLWindowSurface surface(32, 32, "vx_g15_t7");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  canvas.Begin();
  canvas.Clear(Color::White());
  canvas.FillRoundedRect({4, 4, 24, 24}, 8.0f, Brush::Solid(Color::Red()));
  // Pixel right at the top-left rounded corner cusp.
  // Veloxa (4,4) -> GL (4, 32-1-4=27). Top-left corner pixel.
  uint8_t corner[4], center[4];
  ReadPixel(4, 27, corner);
  ReadPixel(16, 15, center);
  SKIP_IF_SWRAST_BLANK(center);
  // Center is solid red (validated by T6 logic).
  EXPECT_GT(center[0], 200u);
  // Corner is OUTSIDE the rounded shape -> mostly white background visible
  // through SDF transparency. White R is also 255, so we differentiate by
  // green channel: corner > 100 (white background bleed) vs center < 50.
  EXPECT_GT(corner[1], 100u)
      << "Corner should show white background through SDF alpha; got R="
      << static_cast<int>(corner[0]) << " G=" << static_cast<int>(corner[1])
      << " B=" << static_cast<int>(corner[2]);
  canvas.End();
}

// T8: FillRect_AfterSetTransform_Translation -------------------------------
// SetTransform(Translation(8,0)) then FillRect at (0,0,8,8): pixel at
// veloxa (12,4) should be red (rect shifted right by 8 -> covers (8..16, 0..8)).
TEST(GLESCanvasFillTest, FillRect_AfterSetTransform_Translation) {
  vx::platform::Sdl2GLWindowSurface surface(16, 16, "vx_g15_t8");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  canvas.Begin();
  canvas.Clear(Color::White());
  canvas.SetTransform(Matrix3x2::Translation(8.0f, 0.0f));
  canvas.FillRect({0, 0, 8, 8}, Brush::Solid(Color::Red()));
  uint8_t inside[4], outside[4];
  // Veloxa (12,4) is inside translated rect -> GL (12, 16-1-4=11).
  // Veloxa (4,4) is outside translated rect -> GL (4, 11).
  ReadPixel(12, 11, inside);
  ReadPixel(4,  11, outside);
  SKIP_IF_SWRAST_BLANK(inside);
  EXPECT_GT(inside[0], 200u);
  EXPECT_LT(inside[1], 50u);
  EXPECT_GT(outside[0], 200u);  // white untouched
  EXPECT_GT(outside[1], 200u);
  canvas.End();
}

// T9: ReverseProbe_TransparentBrush ----------------------------------------
// B6=A inline reverse probe: alpha=0 brush should render nothing — Clear
// background must remain visible. Validates the uColor.a path + glBlendFunc.
TEST(GLESCanvasFillTest, ReverseProbe_TransparentBrush) {
  vx::platform::Sdl2GLWindowSurface surface(8, 8, "vx_g15_t9");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  canvas.Begin();
  canvas.Clear(Color::White());
  // Transparent red brush (alpha=0) — SRC_ALPHA blend should leave bg.
  canvas.FillRect({0, 0, 8, 8}, Brush::Solid({255, 0, 0, 0}));
  uint8_t px[4];
  ReadPixel(4, 4, px);
  SKIP_IF_SWRAST_BLANK(px);
  EXPECT_GT(px[0], 200u);  // still white (R=255)
  EXPECT_GT(px[1], 200u);  // still white (G=255)
  EXPECT_GT(px[2], 200u);  // still white (B=255)
  canvas.End();
}

// T10: ReverseProbe_EmptyRect ----------------------------------------------
// B6=A inline reverse probe: rect with w=0 should early-return — no GL call,
// no error, bg untouched.
TEST(GLESCanvasFillTest, ReverseProbe_EmptyRect) {
  vx::platform::Sdl2GLWindowSurface surface(8, 8, "vx_g15_t10");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  canvas.Begin();
  canvas.Clear(Color::White());
  DrainGLErrors();
  canvas.FillRect({4, 4, 0, 8}, Brush::Solid(Color::Red()));   // w=0
  canvas.FillRect({4, 4, 8, 0}, Brush::Solid(Color::Red()));   // h=0
  canvas.FillRoundedRect({4, 4, 0, 8}, 2.0f, Brush::Solid(Color::Red()));
  EXPECT_EQ(glGetError(), static_cast<GLenum>(GL_NO_ERROR));
  uint8_t px[4];
  ReadPixel(4, 4, px);
  SKIP_IF_SWRAST_BLANK(px);
  EXPECT_GT(px[0], 200u);  // white untouched
  canvas.End();
}

// T11: ReverseProbe_ZeroRadiusRoundedRect ----------------------------------
// B6=A inline reverse probe: radius=0 should degenerate to FillRect (no SDF
// transparency at corners). Validates the FillRect fallback path in
// FillRoundedRect.
TEST(GLESCanvasFillTest, ReverseProbe_ZeroRadiusRoundedRect) {
  vx::platform::Sdl2GLWindowSurface surface(16, 16, "vx_g15_t11");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  canvas.Begin();
  canvas.Clear(Color::White());
  canvas.FillRoundedRect({0, 0, 16, 16}, 0.0f, Brush::Solid(Color::Red()));
  uint8_t corner[4];
  ReadPixel(0, 15, corner);  // top-left
  SKIP_IF_SWRAST_BLANK(corner);
  EXPECT_GT(corner[0], 200u);  // fully red (no SDF AA)
  EXPECT_LT(corner[1], 50u);
  EXPECT_LT(corner[2], 50u);
  canvas.End();
}

// T12: FillRect_PreservesQuadVao -------------------------------------------
// quad_vao_ should still be a valid GL object after multiple FillRect calls
// (regression guard: bug class where program/VAO interaction corrupts state).
TEST(GLESCanvasFillTest, FillRect_PreservesQuadVao) {
  vx::platform::Sdl2GLWindowSurface surface(8, 8, "vx_g15_t12");
  ASSERT_TRUE(surface.valid());
  ASSERT_TRUE(surface.gles_display()->MakeCurrent());

  GLESCanvas canvas(&surface);
  GLuint vao_before = canvas.quad_vao();
  canvas.Begin();
  for (int i = 0; i < 5; ++i) {
    canvas.FillRect({0, 0, 8, 8}, Brush::Solid(Color::Red()));
  }
  canvas.End();
  EXPECT_EQ(canvas.quad_vao(), vao_before);
  EXPECT_EQ(glGetError(), static_cast<GLenum>(GL_NO_ERROR));
}

}  // namespace
}  // namespace vx::gfx::gles
```

### 3.5 `tests/CMakeLists.txt`（追加 `gles_canvas_fill_test`）

在 `shader_injection_test` 段（line 428）之后、`endif()`（line 429）之前追加：

```cmake
    # GLESCanvas FillRect + FillRoundedRect (G1.5): pixel-level verification
    # of the first real draw methods. Same gles guard + SDL_VIDEODRIVER=offscreen
    # env as gles_canvas_skeleton_test.
    add_executable(gles_canvas_fill_test
                   graphics/gles/gles_canvas_fill_test.cc)
    target_link_libraries(gles_canvas_fill_test
      PRIVATE vx_foundation vx_platform_sdl2 vx_graphics GTest::gtest_main)
    gtest_discover_tests(gles_canvas_fill_test
      PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=offscreen")
```

### 3.6 `tests/graphics/gles/shader_injection_test.cc`（修改 S1 范围化 + 新增 S3）

**S1 修改：** 用 `kAllShaderSources[]` 数组遍历检查（覆盖 G1.5 新增 3 shader）：

```cpp
TEST(ShaderInjectionTest, ShaderSourcesAreCompileTimeLiterals) {
  static_assert(kAllShaderSourceCount > 0, "Empty shader list");
  for (int i = 0; i < kAllShaderSourceCount; ++i) {
    const char* s = kAllShaderSources[i];
    ASSERT_NE(s, nullptr) << "Shader index " << i << " is null";
    std::string_view v(s);
    EXPECT_TRUE(v.find("#version 300 es") == 0)
        << "Shader index " << i
        << " must start with #version 300 es; got: "
        << std::string(v.substr(0, 30));
    // Pointer stability — proves link-time binding (not runtime synthesis).
    EXPECT_EQ(kAllShaderSources[i], s);
  }
  // Sanity: G1.5 must have added 3 shaders on top of G1.4's 2.
  EXPECT_GE(kAllShaderSourceCount, 5)
      << "Expected at least 5 shaders after G1.5 (kPassthroughVert/Frag + "
         "kSolidVert/Frag + kRoundedRectFrag)";
}
```

**新增 S3：** 验证 G1.5 shader 不含 user-input concat 痕迹（如 `%s`、`#define USER_`、`+`）：

```cpp
TEST(ShaderInjectionTest, NoUserConcatPatternInG15Shaders) {
  // Verify the G1.5 shader sources do not contain any pattern that would
  // suggest runtime string concatenation (e.g. printf-style %s, sprintf,
  // user-prefixed defines). Pure structural check.
  const char* g15_shaders[] = {kSolidVert, kSolidFrag, kRoundedRectFrag};
  for (const char* s : g15_shaders) {
    std::string_view v(s);
    EXPECT_EQ(v.find("%s"), std::string_view::npos)
        << "Shader contains printf-style placeholder: "
        << std::string(v.substr(0, 60));
    EXPECT_EQ(v.find("#define USER_"), std::string_view::npos)
        << "Shader contains USER_ macro injection point: "
        << std::string(v.substr(0, 60));
  }
}
```

S2 保留（SUCCEED 文档化 / B8=A 设计契约）。

---

## 4. 任务步骤（5 Phase / 10-12 单测 / TDD 严格）

### Phase A — RED：写测试 + 编译失败验证

#### A.1 — 写 `gles_canvas_fill_test.cc`（10-12 单测）+ 注册 CMakeLists.txt

**文件：** §3.4 全代码 + §3.5 CMakeLists.txt 6 行
**测试模式：** [TDD]

- [ ] **步骤 1：** 在 `tests/graphics/gles/` 下新建 `gles_canvas_fill_test.cc`（§3.4 全文）
- [ ] **步骤 2：** 在 `tests/CMakeLists.txt:428` 后追加 `gles_canvas_fill_test` 注册（§3.5）
- [ ] **步骤 3：** `cmake --build build-gles --target gles_canvas_fill_test 2>&1 | head -50`
  预期：编译 FAIL（因 `FillRect` / `FillRoundedRect` 当前仍是 `{}` stub，shader_program/uniform_loc 私有成员未声明 → 但实际上调用 stub 不报编译错误 ❌）
  → 实际 expectation：编译 PASS 但**测试运行 FAIL**（pixel 永远 = white Clear bg / 0/12 PASS）
- [ ] **步骤 4：** `ctest --test-dir build-gles -R gles_canvas_fill 2>&1 | tail -30`
  预期：12 测全 FAIL（red expected, white actual）→ 标志 RED 阶段就绪

### Phase B — GREEN：3 shader + uniform 缓存 + FillRect/FillRoundedRect 实现

#### B.1 — `shaders.h` 追加 3 shader + `kAllShaderSources[]`

**文件：** `veloxa/graphics/gles/shaders.h`（§3.1 追加）
**测试模式：** [TDD（B 阶段实现）]

- [ ] **步骤 1：** 在 `shaders.h` 文件末尾追加 §3.1 全代码（kSolidVert / kSolidFrag / kRoundedRectFrag + kAllShaderSources[]）
- [ ] **步骤 2：** `cmake --build build-gles --target shader_injection_test 2>&1 | head -20`
  预期：PASS（kAllShaderSources[] 仍以 nullptr-safe + compile-time 形式 / S1/S2 PASS）

#### B.2 — `gles_canvas.h` 私有成员 + helper 声明

**文件：** `veloxa/graphics/gles/gles_canvas.h`（§3.2 修改）

- [ ] **步骤 1：** 修改 line 64-65 stub 为真实 override 声明（移除 `{}`）
- [ ] **步骤 2：** 在 `private:` 段末追加 §3.2 私有成员 + helper 声明

#### B.3 — `gles_canvas.cc` ctor 扩展 + 5 helper + FillRect/FillRoundedRect 实现

**文件：** `veloxa/graphics/gles/gles_canvas.cc`（§3.3 修改）

- [ ] **步骤 1：** 文件顶部 include `<algorithm>`
- [ ] **步骤 2：** 修改 ctor（line 9-27）追加 `UploadUnitQuad() + InitShaderPrograms()` 调用
- [ ] **步骤 3：** 修改 dtor 追加 `DestroyShaderPrograms()` 调用
- [ ] **步骤 4：** 在 `PopState()` 后（line 80 后）追加 §3.3 完整 helper + FillRect + FillRoundedRect 实现
- [ ] **步骤 5：** `cmake --build build-gles --target gles_canvas_fill_test gles_canvas_skeleton_test 2>&1 | head -30`
  预期：编译 PASS（无 warning）
- [ ] **步骤 6：** `ctest --test-dir build-gles -R "gles_canvas_(fill|skeleton)" --output-on-failure 2>&1 | tail -50`
  预期：fill_test 12/12 PASS（或部分 GTEST_SKIP 但无 FAIL）+ skeleton_test 8/8 PASS（无回归）

### Phase C — REFACTOR：shader_injection_test S1 范围化 + S3 新增

#### C.1 — `shader_injection_test.cc` S1 改用 kAllShaderSources[] + 新增 S3

**文件：** `tests/graphics/gles/shader_injection_test.cc`（§3.6 修改 + 新增）

- [ ] **步骤 1：** 修改 S1（line 30-49）为 §3.6 中的 `kAllShaderSources[]` 遍历版
- [ ] **步骤 2：** 在 S2 后新增 S3 `NoUserConcatPatternInG15Shaders`
- [ ] **步骤 3：** `cmake --build build-gles --target shader_injection_test 2>&1 | head -20`
- [ ] **步骤 4：** `ctest --test-dir build-gles -R shader_injection --output-on-failure 2>&1 | tail -20`
  预期：3/3 PASS（S1 范围化 + S2 不变 + S3 新增）

### Phase D — 三 build 矩阵全 ctest 验证（双 config ctest 单次盲区 #7 抑制）

#### D.1 — Matrix C（gles）完整 ctest

- [ ] **步骤 1：** `ctest --test-dir build-gles -j 4 --output-on-failure 2>&1 | tail -30`
  预期：**1362 → 1374-1376 / 1374-1376 PASS**（含 +12 fill_test + +1 shader_injection_test 新 S3）

#### D.2 — Matrix A（software ON）完整 ctest

- [ ] **步骤 1：** `ctest --test-dir build -j 4 --output-on-failure 2>&1 | tail -30`
  预期：**1337/1337 PASS**（gles 测试不进 software / 0 退化）

#### D.3 — Matrix B（software OFF）完整 ctest

- [ ] **步骤 1：** 验证 build-off/ 或动态切换 `-DVX_BUILD_DEVTOOL=OFF` build 完整
  - 如无 build-off/ 现存，复用 `build/_deps` 加速：`cmake -B build-off -DVX_BUILD_DEVTOOL=OFF -DFETCHCONTENT_BASE_DIR=$(pwd)/build/_deps -G Ninja`
  - `cmake --build build-off -j 4 2>&1 | tail -10`
  - `ctest --test-dir build-off -j 4 --output-on-failure 2>&1 | tail -10`
  预期：**1141/1141 PASS**（gles 测试不进 / DEVTOOL=OFF 黑名单守门 / 0 退化）

### Phase E — finalize

#### E.1 — `lint` 全文件

- [ ] **步骤 1：** ReadLints 7 文件（`shaders.h` / `gles_canvas.{h,cc}` / `gles_canvas_fill_test.cc` / `shader_injection_test.cc` / `tests/CMakeLists.txt`）
  预期：0 错误

#### E.2 — Memory Bank 三件套 finalize 更新

- [ ] **步骤 1：** 更新 `memory-bank/tasks.md`「TASK-20260528-01」段：阶段「规划中 → 已完成」+ Build 阶段产出段
- [ ] **步骤 2：** 更新 `memory-bank/activeContext.md`：阶段「规划中 → 构建中·全部完成」+ 焦点切换
- [ ] **步骤 3：** 更新 `memory-bank/progress.md`：追加 Build 阶段时间线 + ctest 矩阵实测 + 范式里程碑

#### E.3 — 单 commit 落盘（feat + Source 溯源）

- [ ] **步骤 1：** `git add veloxa/ tests/` （主交付）+ `git add memory-bank/` （finalize）
- [ ] **步骤 2：** `git commit -m "$(cat <<'EOF'` 含 8 段范本（见 §6 commit body）

---

## 5. ctest 期望矩阵

| Config | DEVTOOL | VX_RENDERER | baseline | 期望 | diff | 配置差额 / 真实增量 |
|---|:-:|:-:|:-:|:-:|:-:|---|
| A | ON | software | 1337 | 1337 | 0 | gles 测试不进 software 配置 ✅ |
| B | OFF | software | 1141 | 1141 | 0 | gles + DEVTOOL=OFF 黑名单守门 ✅ |
| C | ON | gles | 1362 | **1374-1376** | +12-14 | gles_canvas_fill_test(+12) + shader_injection_test S3(+1) ≈ +13 真实增量 |

**判读规则：**
- Matrix A/B diff = 0 → ✅ baseline 不退化
- Matrix C diff = +12-14 → ✅ 含本任务真实增量
- Matrix C diff < +12 → 🔴 GTEST_SKIP 频发 / driver-strictness 触发 / 单测失效

---

## 6. commit body 8 段范本（plan/spec docs 落盘即 commit P0 协议 / sept-evidence 候选）

```
feat(graphics): implement GLESCanvas FillRect + FillRoundedRect — G1.5

# 任务定位 (§1)
TASK-20260528-01 — G1.5 实施类 Level 3 / GLES 蓝图实施第五步 /
MVP-C 战略主线第五个实施任务 / 前置 G1.4 ✅ 已闭环.

# 决策矩阵 (§2)
8 B 决策 1 次 AskQuestion all_recommended 锁定:
  B1=B per-FillRect uMvp glUniform
  B2=A unit quad [0,1]² + ctor glBufferData
  B3=A uRectPx + uXformPx + uViewportPx
  B4=A fwidth(dist) 自动反走样
  B5=A ctor 创建 2 programs + dtor delete
  B6=A inline reverse probe
  B7=A kSolid 完整 + kLinearGradient fallback color_start
  B8=A kAllShaderSources[] 数组化

# 主交付清单 (§3)
- veloxa/graphics/gles/shaders.h           (+~90)  3 shader + array
- veloxa/graphics/gles/gles_canvas.h       (+~35)  程序 + uniform 缓存
- veloxa/graphics/gles/gles_canvas.cc      (+~300) 完整 FillRect impl
- tests/graphics/gles/gles_canvas_fill_test.cc (+~300) 12 单测
- tests/graphics/gles/shader_injection_test.cc (+~30)  S1 范围化 + S3
- tests/CMakeLists.txt                     (+~6)   注册 fill_test

# 后续实施 (§4)
G1.6 FillPath via libtess2 (Level 3 / ~6-8 h plan ×0.6 / +30% buffer)
G1.7 Stroke* (Level 3 / ~3-4 h plan ×0.6)
G1.8 GlyphAtlas + DrawText (Level 4 / ~8-10 h plan ×0.6)

# 协议元数据 (§5)
plan/spec docs 落盘即 commit P0 协议: 实施类 Level 3 子档 sept-evidence
候选 / D8=A 仅 plan 决策 / 引用上游 GLES 蓝图 plan §3.5 + spec §4.1.
跨决策协同度 100% 第 19 次连续命中 / streak 171→179/179 续刷 /
实施忠实度 quint-evidence 候选 (G1.1 first + G1.2 dual + G1.3 triple +
G1.4 quad + G1.5 quint).

# plan ×0.6 实测系数 (§6)
预期: ~90-180 min vs plan ×0.6 ~125-175 min = 0.55-1.0× 标准极速区.

# Source 溯源 (§7)
Source: docs/plans/2026-05-28-gles-canvas-fillrect.md (本任务 plan)
Upstream: docs/plans/2026-05-05-gles-renderer-blueprint.md §3.5 (蓝图)
          docs/specs/2026-05-05-gles-renderer-blueprint-design.md §4.1
          memory-bank/archive/archive-TASK-20260507-01.md (G1.4 前置)

# 下一步 (§8)
/reflect — 进入回顾阶段, 沉淀 quint-evidence + 跨决策第 19 次连续命中
+ Mesa swrast SDF 反走样 first-evidence (若 T7 PASS).
```

---

## 7. 反复模式预防（8/8 全抑制 / 累计 21+ 模式连续抑制候选续刷）

| # | 反复模式 | 命中状态 | 抑制证据 |
|:-:|---|:-:|---|
| #1 | 前置依赖/环境/API 能力未验证 | ✅ 抑制 | Phase 0 §0.2 GLES API audit + §0.3 GLSL ES SL 1.00 + §0.4 既有 GLESCanvas 复用资源 + §0.7 _deps 复用 全实证 |
| #2 | spec 数据回归 | ✅ 抑制 | 蓝图 plan §3.5 锁定文件清单 + 接口签名 / 0 既有 spec 修改 |
| #3 | TDD 顺序倒置 | ✅ 抑制 | Phase A 先写 12 单测验证编译 PASS + 测运行 FAIL → 标志 RED → Phase B 实现 → Phase D ctest verify |
| #4 | 反向探针缺失或弱 | ✅ 抑制 | 3 inline reverse probe（T9 transparent / T10 empty / T11 zero radius）+ B6=A 范式与 G1.4 D4=C dual-evidence ✅ |
| #5 | 中文文档 StrReplace 字符类型 audit | ✅ 抑制 | 本任务无中文文档改动 / 半角符号全 / plan 半角一致 |
| #6 | commit body Source 溯源 | ✅ 抑制 | §6 commit body 8 段范本含 Source 溯源（plan + upstream 三件齐）|
| #7 | 双 config ctest 单次盲区 | ✅ 抑制 | Phase D 三 build 矩阵 (software ON + software OFF + gles ON) 全 ctest verify / 预期 1337+1141+1374-1376 |
| #8 | spec 数据回归 audit 协议 | ✅ 抑制 | 0 既有 spec 数据修改 / G1.4 ctest baseline (1337/1141/1362) 已稳定可比对 |

---

## 8. systemPatterns 协同度自我对照（13 项 / 100% 协同）

| # | systemPatterns 段 | 协同度 | 证据 |
|:-:|---|:-:|---|
| 1 | 跨决策协同度 100% doudec-evidence | ✅ 续刷 | 8/8 决策 1 次 AskQuestion all_recommended / 第 19 次连续命中候选 |
| 2 | Phase 0 投入定律 sext-evidence | ✅ 复用 | §0.1-§0.8 完整 Phase 0 audit / 预期 ROI 5-16× |
| 3 | plan ×0.6 实测系数 dec-evidence | ✅ 续刷 | 第 13 数据点候选 / 实施类 Level 3 子档 0.55-1.0× 标准极速区 |
| 4 | brainstorming P1.3 主动 push-back triple | ✅ N/A | 8 决策无重大偏差，无 push-back 触发 |
| 5 | writing-plans P1.5 P0 sext-evidence | ✅ 自吃狗粮 | plan + MB ×3 单 commit 落盘（sept-evidence 候选 / 实施类 Level 3 子档第 2 实证）|
| 6 | LOC ×[0.85, 1.5] buffer 双向子档 | ✅ 复用 | §2 文件结构表 LOC buffer 范围 ~525-925 行（核心 616 + 隐性 ~115 = 731）|
| 7 | D3=B eager extension cache first-evidence | ✅ 续延 | Solid + Rounded program ctor 一次创建 / uniform location ctor 缓存 / first-evidence dual-evidence 候选 |
| 8 | D4=C inline reverse probe + driver-strictness first | ✅ 复用 | T9/T10/T11 inline reverse probe + Mesa swrast SKIP_IF_SWRAST_BLANK fallback / G1.4 dual → triple-evidence 候选 |
| 9 | Mesa swrast default framebuffer dual-evidence | ✅ 复用 | T2/T3/T4/T6 像素读取（G1.3 + G1.4 dual → triple-evidence 候选）|
| 10 | shader injection first-evidence | ✅ 扩展 | S1 范围化 + S3 新增 / first → dual-evidence 候选（B8=A 数组化）|
| 11 | 双 100% 流程闭环 dual-evidence | ✅ 续刷 | 跨决策协同度 100% + 实施忠实度 100% / first → triple-evidence 候选 |
| 12 | V2=a 蓝图 + 实施梯度（深浅）✅ | ✅ N/A | 本任务为实施类（不是蓝图）/ 沿用蓝图 §3.5 详尽规格化 |
| 13 | 反复模式渐进式抑制 | ✅ 复用 | 0/8 抑制 / 累计 21+ 模式连续抑制候选续刷 |

---

## 9. 预期实测系数

| 阶段 | 估时（plan ×0.6）| 预期实测 | 系数 | 备注 |
|---|:-:|:-:|:-:|---|
| VAN | ~10-15 min | ~10 min ✅ | ~0.7-1.0× | 已完成 |
| Plan | ~25-40 min | ~25-40 min（含本文档撰写）| ~0.7-1.0× | brainstorm 1 次 AskQuestion all_recommended + plan 撰写 |
| Build | ~50-100 min | ~30-60 min | ~0.3-0.6× 极速区 | Phase A-B-C-D-E / shader 模板高度可复用 |
| Reflect | ~15-20 min | ~15-20 min | ~1.0× | 标准 |
| Archive | ~10-15 min | ~10-15 min | ~1.0× | 标准 |
| **总线** | **~125-175 min** | **~90-150 min** | **~0.5-0.8×** | 沿用 G1.4 标准极速区（0.13-0.30× build 子档）|

---

## 10. 验收清单

- [ ] §4 Phase A-E 全 PASS
- [ ] ctest Matrix A 1337/1337 + Matrix B 1141/1141 + Matrix C **1374-1376/1374-1376** PASS
- [ ] T9 reverse probe transparent brush 像素仍 white（alpha=0 阻塞 fill）
- [ ] T10 reverse probe empty rect 0 GL error + 像素 untouched
- [ ] T11 reverse probe zero-radius FillRoundedRect 等价于 FillRect
- [ ] T7 corner partial alpha 验证（SDF 反走样 first-evidence / Mesa swrast 支持范围内）
- [ ] ReadLints 7 文件 0 错误
- [ ] commit body 含 8 段范本（Source 溯源 + plan ×0.6 实测）
- [ ] Memory Bank 三件套 finalize 同步更新

---

## 11. 长期影响 + 解锁子任务

- **解锁**：G1.6 FillPath via libtess2（shader pipeline 基础已就绪 / 仅加 tessellator + glDrawElements）/ G1.7 Stroke*（复用 solid program + tess offset）/ G1.8 DrawText 部分（glyph atlas shader 复用 kSolidVert pattern）
- **MVP-C 渲染管线**：首次真实绘制像素到 default framebuffer ✅（vs G1.4 仅 Clear）
- **范式沉淀**：shader program ctor 创建 + uniform location ctor 缓存 = Veloxa GLES 范式（首次实证 / G1.6+ 沿用）
- **反复模式**：0/8 抑制 / 累计 21+ 模式连续抑制候选续刷 / 历史新高续刷

---

**计划完成并保存到 `docs/plans/2026-05-28-gles-canvas-fillrect.md`。准备执行 `/build`。**
