# Creative — GLES Canvas Trampolining 策略（B2）

**任务 ID：** TASK-20260505-03
**日期：** 2026-05-05
**状态：** 蓝图（V2=a）
**关联决策：** B2 = 混合（FillRect/RoundedRect = shader / FillPath = libtess2 + VBO / Stroke = Fill 转换）

---

## 0. 决策上下文

### 0.1 V/B 协同度矩阵

| 已锁定决策 | 影响 |
|---|---|
| V1 GLES only | OpenGL ES 3.0+ 功能集 / 不依赖 GL 4.x compute shader |
| V5 vx_renderer_flag | 与 SoftwareCanvas 像素级行为一致性优先 / SDF 抗锯齿对齐 |
| B6 静态嵌入 .glsl | shader 集合编译期固定 / 不接受运行时新 shader |
| B7 dual BM | 性能验收依赖 vs SoftwareCanvas 多倍优势 / 不能太复杂 |

---

## 1. 三方案对比（VAN 阶段未展开 / creative 阶段补充）

### 1.1 候选 A：混合（shader + libtess2） ⭐（已选）

**原理：** 简单几何（FillRect / FillRoundedRect / Solid Brush）走 shader 极速路径；复杂路径（FillPath）走 CPU libtess2 tessellation + GPU VBO 提交；Stroke* 全部转化为 Fill 等价（StrokeRect → 4 矩形 / StrokeRoundedRect → 2 RoundedRect 减法 / StrokeLine → 1 矩形 / StrokePath → tessellator + offset）。

**调用分发：**

```cpp
void GLESCanvas::FillRect(...)         { /* shader (kSolidVert + kSolidFrag) */ }
void GLESCanvas::FillRoundedRect(...)  { /* shader (kSolidVert + kRoundedRectFrag SDF) */ }
void GLESCanvas::FillPath(...)         { /* libtess2 + VBO + glDrawElements */ }
void GLESCanvas::StrokeRect(...)       { /* 4 个 FillRect 拼接 */ }
void GLESCanvas::StrokeRoundedRect(...){ /* 2 个 FillRoundedRect 减法 */ }
void GLESCanvas::StrokeLine(...)       { /* 1 个旋转 FillRect */ }
void GLESCanvas::StrokePath(...)       { /* libtess2 path offset + glDrawElements */ }
```

**优势：**

- ✅ 简单形状极速 GPU（每帧 N 个 FillRect = 1 shader switch + N draw calls / 与现代 GPU 友好）
- ✅ 复杂路径渐进（先 CPU tess / 不 block 主路径）
- ✅ 实现复杂度可控（SDF shader 仅 RoundedRect 一处 / 其余 solid color）
- ✅ 与 V5 vx_renderer_flag 行为一致性高（SDF 抗锯齿 vs SoftwareCanvas rasterizer 抗锯齿可对齐）

**劣势：**

- ⚠️ FillPath CPU tess 在路径复杂时可能成为瓶颈（B7 验收期望 ≥ 10x SW 在 LargeList，路径密集场景仅 5x SW）
- ⚠️ libtess2 是 MPL2 许可（与项目兼容 / 但需 attribution）

**协同度：**

| 已锁定决策 | 协同度 |
|---|:-:|
| V1 GLES only | ✅ |
| V5 vx_renderer_flag | ✅ |
| B6 静态嵌入 .glsl | ✅ |
| B7 dual BM | ✅ |

### 1.2 候选 B：全 shader-based（SDF + GPU tessellator）

**优势：** 全 GPU 路径 / 最高理论性能。

**劣势：**

- ❌ SDF FillPath 实现极复杂（任意路径 SDF 不能闭式 / 需要 GPU compute shader 或 voronoi 预处理）
- ❌ V1 GLES only 无 compute shader 保证（GLES 3.0 baseline）
- ❌ B6 静态嵌入与全 shader 数量不匹配（SDF 需要数十 shader 变体）

### 1.3 候选 C：全 stencil buffer based

**优势：** 经典 OpenGL 路径填充算法（Loop-Blinn）/ 单 stencil pass。

**劣势：**

- ❌ stencil iteration 需要 ≥ 2 pass / 现代 GPU 不主推
- ❌ 与 dirty rect glScissor 交互复杂（stencil buffer 与 scissor 互相独立但需协调）
- ❌ V5 vx_renderer_flag 对照不利（SoftwareCanvas 无 stencil 路径 / 行为对齐困难）

---

## 2. shader-based 路径详细设计

### 2.1 FillRect — 单 quad + uniform color

**vertex shader (kSolidVert):**

```glsl
#version 300 es
precision highp float;

layout(location = 0) in vec2 a_pos;

uniform mat4 u_proj;          // ortho 2D
uniform mat3 u_xform;         // 2x3 仿射 / 列主序

void main() {
  vec3 xy = u_xform * vec3(a_pos, 1.0);
  gl_Position = u_proj * vec4(xy.xy, 0.0, 1.0);
}
```

**fragment shader (kSolidFrag):**

```glsl
#version 300 es
precision mediump float;

uniform vec4 u_color;
out vec4 frag_color;

void main() {
  frag_color = u_color;
}
```

**uniform 设计：**

- `u_proj` — ortho projection matrix（construction 时一次性设 / `glOrtho2D(0, w, h, 0, -1, 1)`）/ Y-down 与 SoftwareCanvas 一致
- `u_xform` — 当前 transform stack 顶元素（SetTransform / PushState 调时上传）
- `u_color` — Solid Brush 颜色（每 FillRect 调用上传 ）

**VAO + VBO 状态：**

```cpp
// 构造一次（quad 4 顶点）
glGenVertexArrays(1, &quad_vao_);
glBindVertexArray(quad_vao_);
glGenBuffers(1, &quad_vbo_);
glBindBuffer(GL_ARRAY_BUFFER, quad_vbo_);
glBufferData(GL_ARRAY_BUFFER, sizeof(f32) * 8, nullptr, GL_DYNAMIC_DRAW);
glEnableVertexAttribArray(0);
glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(f32) * 2, 0);

// FillRect 每次调用更新 quad 顶点（避免每帧重建 VBO）
f32 verts[8] = {rect.x, rect.y,
                rect.x + rect.w, rect.y,
                rect.x + rect.w, rect.y + rect.h,
                rect.x, rect.y + rect.h};
glBindBuffer(GL_ARRAY_BUFFER, quad_vbo_);
glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
```

### 2.2 FillRoundedRect — SDF shader

**vertex shader：** 复用 kSolidVert（同构）

**fragment shader (kRoundedRectFrag)：**

```glsl
#version 300 es
precision mediump float;

in vec2 v_uv;          // 0..1 within rect (从 vertex stage 传入)
uniform vec2 u_size;   // rect (width, height) in 像素
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
  // 1 像素 anti-alias
  float a = smoothstep(1.0, -1.0, d);
  frag_color = vec4(u_color.rgb, u_color.a * a);
}
```

**vertex shader 改写（添加 v_uv 输出）：**

```glsl
#version 300 es
precision highp float;

layout(location = 0) in vec2 a_pos;       // pixel space
layout(location = 1) in vec2 a_uv;        // 0..1 within rect

out vec2 v_uv;

uniform mat4 u_proj;
uniform mat3 u_xform;

void main() {
  v_uv = a_uv;
  vec3 xy = u_xform * vec3(a_pos, 1.0);
  gl_Position = u_proj * vec4(xy.xy, 0.0, 1.0);
}
```

VAO 增加 location=1 attribute（uv 0..1）。

### 2.3 LinearGradient / RadialGradient — SDF + gradient sampler

**LinearGradient fragment shader：**

```glsl
#version 300 es
precision mediump float;

in vec2 v_uv;
uniform vec2 u_p0;        // gradient start point
uniform vec2 u_p1;        // gradient end point
uniform vec4 u_color0;
uniform vec4 u_color1;
out vec4 frag_color;

void main() {
  vec2 d = u_p1 - u_p0;
  float t = clamp(dot(v_uv - u_p0, d) / dot(d, d), 0.0, 1.0);
  frag_color = mix(u_color0, u_color1, t);
}
```

**RadialGradient fragment shader：**

```glsl
#version 300 es
precision mediump float;

in vec2 v_uv;
uniform vec2 u_center;
uniform float u_radius;
uniform vec4 u_color0;
uniform vec4 u_color1;
out vec4 frag_color;

void main() {
  float t = clamp(distance(v_uv, u_center) / u_radius, 0.0, 1.0);
  frag_color = mix(u_color0, u_color1, t);
}
```

**多色 gradient stops（>= 3）扩展：** 用 1D texture 编码 stop 颜色 / sampler 采样（GLES 3.0 支持）。

---

## 3. libtess2 路径详细设计

### 3.1 集成路径

```cmake
# 顶层 CMakeLists.txt
include(FetchContent)
FetchContent_Declare(
  libtess2
  GIT_REPOSITORY https://github.com/memononen/libtess2.git
  GIT_TAG        v1.0.2  # 固定 tag
)
FetchContent_MakeAvailable(libtess2)

# veloxa/graphics/CMakeLists.txt
target_link_libraries(vx_graphics PRIVATE tess2)
```

### 3.2 SoftwarePath → tess contour

```cpp
void GLESCanvas::FillPath(const Path& path, const Brush& brush) {
  // 1. 抽取 contour（SoftwarePath 已有 contour 结构）
  const auto* sw_path = dynamic_cast<const sw::SoftwarePath*>(&path);
  if (!sw_path) {
    VX_LOG_WARN("FillPath: non-SoftwarePath input, using fallback");
    return;
  }
  const auto& contours = sw_path->contours();  // Vector<Vector<Point>>

  // 2. libtess2
  TESStesselator* tess = tessNewTess(nullptr);
  for (const auto& contour : contours) {
    tessAddContour(tess, 2, contour.data(), sizeof(f32) * 2,
                   static_cast<int>(contour.size()));
  }

  if (!tessTesselate(tess, TESS_WINDING_NONZERO, TESS_POLYGONS, 3, 2, nullptr)) {
    VX_LOG_ERROR("tessTesselate failed");
    tessDeleteTess(tess);
    return;
  }

  const f32* verts = tessGetVertices(tess);
  const i32* indices = tessGetElements(tess);
  i32 vert_count = tessGetVertexCount(tess);
  i32 elem_count = tessGetElementCount(tess);  // # of triangles

  // 3. upload to dynamic VBO + draw
  glBindBuffer(GL_ARRAY_BUFFER, dynamic_vbo_);
  glBufferData(GL_ARRAY_BUFFER, vert_count * 2 * sizeof(f32),
               verts, GL_STREAM_DRAW);

  glUseProgram(solid_shader_.program);
  // set u_xform / u_color uniform ...

  glBindVertexArray(dynamic_vao_);
  glDrawElements(GL_TRIANGLES, elem_count * 3, GL_UNSIGNED_INT, indices);
  ++draw_calls_;

  tessDeleteTess(tess);
}
```

### 3.3 winding rule 对齐 SoftwareCanvas

`TESS_WINDING_NONZERO` 与 SoftwareCanvas rasterizer 默认 fill rule 一致。

### 3.4 性能考量

- 每 FillPath = 1 次 CPU tess（O(n log n) where n = path 顶点数）+ 1 次 GPU upload + 1 次 glDrawElements
- 大 path（>1000 顶点）应缓存 tess 结果（path → vertex array 的 cache）
- 本任务范围**不含** path tess cache（G1.6 子任务可选优化 / 由 reflect 阶段决定）

---

## 4. Stroke = Fill 转换策略

### 4.1 StrokeRect → 4 FillRect

```cpp
void GLESCanvas::StrokeRect(const Rect& rect, const Brush& brush, f32 width) {
  // 4 边各 1 个 FillRect
  // top
  FillRect({rect.x, rect.y, rect.w, width}, brush);
  // bottom
  FillRect({rect.x, rect.y + rect.h - width, rect.w, width}, brush);
  // left（不重复 top/bottom）
  FillRect({rect.x, rect.y + width, width, rect.h - 2 * width}, brush);
  // right
  FillRect({rect.x + rect.w - width, rect.y + width,
            width, rect.h - 2 * width}, brush);
}
```

**优势：** 4 个 draw call vs SoftwareCanvas 4 行像素遍历 / GPU 仍快得多。

### 4.2 StrokeRoundedRect → 2 FillRoundedRect 减法

```cpp
void GLESCanvas::StrokeRoundedRect(const Rect& rect, f32 radius,
                                   const Brush& brush, f32 width) {
  // 1. 启用 stencil buffer
  glEnable(GL_STENCIL_TEST);
  glStencilFunc(GL_ALWAYS, 1, 0xFF);
  glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  glClear(GL_STENCIL_BUFFER_BIT);

  // 2. 写 outer rounded rect 到 stencil = 1
  FillRoundedRect(rect, radius, brush);

  // 3. 写 inner rounded rect 到 stencil = 0
  glStencilOp(GL_KEEP, GL_KEEP, GL_ZERO);
  Rect inner = {rect.x + width, rect.y + width,
                rect.w - 2 * width, rect.h - 2 * width};
  FillRoundedRect(inner, radius - width, brush);

  // 4. 启用 color write + stencil = 1 才画
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glStencilFunc(GL_EQUAL, 1, 0xFF);
  glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

  // 5. 重画一次 outer rounded rect（colorize）
  FillRoundedRect(rect, radius, brush);

  // 6. 关闭 stencil
  glDisable(GL_STENCIL_TEST);
}
```

**性能：** 3 个 draw call + 1 stencil clear / 单帧多个 RoundedRect stroke 时性能可观。

**优化路径（实施任务 G1.7 reflect 后决定）：** 编写专用 `kRoundedRectStrokeFrag` SDF shader，单 draw call 完成。

### 4.3 StrokeLine → 旋转 FillRect

```cpp
void GLESCanvas::StrokeLine(Point a, Point b, const Brush& brush, f32 width) {
  // 1. 计算线段中点 + 长度 + 角度
  Point mid = {(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f};
  f32 len = Distance(a, b);
  f32 angle = std::atan2(b.y - a.y, b.x - a.x);

  // 2. PushState + 平移到 mid + 旋转 angle
  PushState();
  Matrix3x2 xform = TopTransform();
  xform = xform.Translate(mid.x, mid.y).Rotate(angle);
  SetTransform(xform);

  // 3. FillRect(centered) — 中心在原点的 len x width 矩形
  FillRect({-len * 0.5f, -width * 0.5f, len, width}, brush);

  PopState();
}
```

### 4.4 StrokePath → libtess2 path offset

复杂路径 stroke 转 fill 需要计算 path offset（沿曲线生成两条 offset path 后构成闭合区域）。

```cpp
void GLESCanvas::StrokePath(const Path& path, const Brush& brush, f32 width) {
  // 1. 对原 path 计算 offset path（offset = ±width/2）
  //    SoftwarePath 已有 OffsetPath 实现（可复用 / 或 G1.7 新增）
  auto outer = sw_path->OffsetPath(width * 0.5f);
  auto inner = sw_path->OffsetPath(-width * 0.5f);

  // 2. 构造 stroke 区域 path（outer + inner reverse）
  Path stroke_region;
  stroke_region.AddContour(outer.contours()[0]);
  stroke_region.AddContour(Reverse(inner.contours()[0]));

  // 3. FillPath 已实现的 libtess2 路径
  FillPath(stroke_region, brush);
}
```

**注：** Path offset 算法本身复杂（曲线 offset 不是闭式）/ 实施任务 G1.7 可暂用「直边线段 offset」简化版本，曲线退化为直线。

---

## 5. 性能预期

### 5.1 vs SoftwareCanvas 多倍预期（B7 验收）

| 场景 | SW（µs/iter）| GLES（µs/iter）| 倍数 |
|---|---|---|:-:|
| `BM_ReplaySmoke` (1 FillRect) | ~1.65 | ~0.16 | 10x |
| `BM_ReplayLargeList` (100 FillRect) | ~5 | ~0.5 | 10x |
| `BM_ReplayTextHeavy` (100 DrawText) | ~784 | ~50 | 16x |
| `BM_ReplayDeepClip` (10 PushClipRect) | ~50 | ~10 | 5x |
| `BM_ReplayPath` (FillPath × 10) | ~200 | ~40 | 5x（CPU tess 限制）|
| `BM_Replay1080p_Typical` (~50 cmds) | ~3000 | ~300 | 10x ≈ 60fps budget ≤ 16.6ms |

> **风险：** 上述预期基于 desktop Mesa GLES + 10x speedup 假设 / 嵌入式 GPU 实测可能仅 3-5x（B7 验收门槛已设 LargeList ≥ 5x）。

### 5.2 draw call 数量目标

| 场景 | draw calls / 帧（GLES）|
|---|:-:|
| baseline 静态页面 | 5-20 |
| LargeList 100 div | 100-200（每 div 1-2 calls）|
| TextHeavy 100 字 | 1-5（glyph atlas batching）|
| 1080p typical | 50-100 |

**优化路径（实施任务 G1.5+ reflect 后决定）：** 简单 FillRect 批量化（同 shader + 同 color → 1 instanced draw call）/ 实施任务 G1.5 不做 / G1.17 BM_GLESReplay* 验收时决定是否需要。

---

## 6. shader 缓存与重复编译避免

### 6.1 编译期检查

```cpp
// veloxa/graphics/gles/shaders.h（B6 静态嵌入）
constexpr const char* kSolidVert = R"(...)";  // raw string literal
static_assert(sizeof(kSolidVert) > 0, "kSolidVert must be non-empty");
```

### 6.2 运行期编译（一次性 / 构造时）

```cpp
GLuint GLESCanvas::CompileShader(const char* src, GLenum type) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &src, nullptr);
  glCompileShader(shader);

  GLint status = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
  if (status == GL_FALSE) {
    char log[1024];
    glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
    VX_LOG_ERROR("shader compile failed: %s\n%s", log, src);
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

void GLESCanvas::CompileShaders() {
  // 一次性 / 构造时调用
  GLuint vs = CompileShader(shaders::kSolidVert, GL_VERTEX_SHADER);
  GLuint fs = CompileShader(shaders::kSolidFrag, GL_FRAGMENT_SHADER);
  solid_shader_.program = LinkProgram(vs, fs);

  vs = CompileShader(shaders::kSolidVert, GL_VERTEX_SHADER);
  fs = CompileShader(shaders::kRoundedRectFrag, GL_FRAGMENT_SHADER);
  rounded_rect_shader_.program = LinkProgram(vs, fs);

  // ... 其余 shader ...
}
```

### 6.3 Context Lost 后重编译

`OnContextRestored()` 内 `CompileShaders()` 重做（详见 creative-gles-context §3.4）。

---

## 7. 实施任务关联

- **G1.4** GLESCanvas 骨架 — shader 编译 + VAO/VBO 创建
- **G1.5** FillRect / FillRoundedRect / Solid Brush — 本 creative §2.1 + §2.2 落地
- **G1.6** FillPath via libtess2 — 本 creative §3 落地
- **G1.7** Stroke* — 本 creative §4 落地
- **G1.12** LinearGradient / RadialGradient — 本 creative §2.3 落地

---

## 8. 反向探针候选

| 反向探针 | 验证点 | 强度档 |
|---|---|:-:|
| 改 fragment shader `frag_color = vec4(0)` | shader uniform 生效 | 合适 |
| 改 vertex shader 跳过 u_xform | transform 生效 | 合适 |
| 改 SDF roundedBoxSDF 的 `r` 参数为 0 | SDF 生效 / 圆角不出现 | 平衡 |
| 改 libtess2 winding 为 NEGATIVE | winding rule 生效 | 平衡 |
| 注释掉 `glClear(GL_COLOR_BUFFER_BIT)` | Clear 生效 | 过高（导致大量测试 FAIL）|

---

**END OF CREATIVE — GLES Canvas Trampolining (B2)**
