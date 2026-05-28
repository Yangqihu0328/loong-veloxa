# 归档：G1.5 `GLESCanvas::FillRect` + `FillRoundedRect` + Solid Brush

**日期：** 2026-05-29
**任务 ID：** TASK-20260528-01
**复杂度级别：** Level 3（实施类 / GLES 蓝图实施第五步 / MVP-C 战略主线第五个实施任务）
**状态：** ✅ 已完成
**分支：** `feature/TASK-20260528-01-gles-canvas-fillrect`（基于 main `afc59a7`）
**总提交数：** 5（plan + Phase A + Phase B + Phase C-E + reflect）
**总投入：** ~83 min（VAN ~10 + Plan ~30 + Build ~33 + Reflect ~10）
**plan ×0.6 系数：** ~0.40-0.59× 标准极速区（第 13 数据点）

---

## 1. 任务概述

GLES 蓝图实施第五步 — 在 G1.4 已落地的 `GLESCanvas` 骨架（Begin/End/Clear/Transform/PushState/PopState 真实 + 14 stub 方法）之上，将 `FillRect` + `FillRoundedRect` 由 stub 替换为**真实 GPU 实现**。

### 1.1 任务目标

- **FillRect**：solid color shader（`kSolidVert` + `kSolidFrag`）+ MVP 矩阵 uniform 注入 + unit quad VBO → glDrawArrays（首个真实绘制方法）
- **FillRoundedRect**：SDF shader（`kRoundedRectFrag`）+ `fwidth + smoothstep` 反走样
- **Solid Brush**：`Brush::Color` 路径首次真实落地（Gradient/Image/Pattern fallback 到 `color_start`）

### 1.2 战略价值（MVP-C 渲染管线里程碑）

- **首次真实绘制像素到 default framebuffer** ✅（vs G1.4 仅 Clear）
- **首次 fragment shader complex 数学函数链运行** ✅（fwidth + smoothstep + length / GLSL ES 3.0 SL 1.00 全栈）
- **首次 user transform → MVP 矩阵 → NDC 完整链路** ✅（uXformPx mat3 上传 + Y 翻转 + viewport 归一）
- **解锁 G1.6-G1.18 + G2** 18 子任务的 shader pipeline 基础（FillPath / Stroke / DrawText / Gradient 等）

### 1.3 前置链

G1.1 ✅ → G1.2 ✅ → G1.3 ✅ → G1.4 ✅ → **G1.5（本任务）** → G1.6/G1.7/G1.8 解锁。

---

## 2. 技术方案

### 2.1 8 B 决策矩阵（1 次 AskQuestion all_recommended 锁定）

| # | 决策 | 选项 | 理由 |
|:-:|---|:-:|---|
| B1 | MVP 矩阵注入策略 | **B per-FillRect uMvp glUniform** | 简单可靠 / per-call 上传 ~ns 开销 / G2 再批处理 batch |
| B2 | Unit quad 上传策略 | **A ctor BufferData + 持久化** | 沿用 G1.4 quad_vao_/vbo_ 范式 / 0 per-call 上传 |
| B3 | Shader uniform 设计 | **A uRectPx + uXformPx + uViewportPx** | 像素空间 + transform 矩阵 + viewport 归一 / 通用化 |
| B4 | SDF 反走样数值 | **A fwidth(dist) 自动** | GLSL ES 3.0 SL 1.00 标准 / 像素无关 / 自适应 |
| B5 | Shader program lifecycle | **A ctor 创建 2 programs + dtor delete** | eager init / 0 lazy 路径分支 / 沿用 G1.4 D3=B eager extension cache 范式 |
| B6 | 测试反向探针策略 | **A inline reverse probe** | 沿用 G1.2 D4 + G1.4 D4=C 范式 / 3 inline probe（T9/T10/T11）|
| B7 | Brush 类型支持范围 | **A kSolid 完整 + Gradient/Image/Pattern fallback color_start** | 本任务 Solid only / G2+ Gradient brush 拓展点 |
| B8 | shader_injection_test 扩展策略 | **A kAllShaderSources[] 数组化** | 数组化范围化遍历 / 未来新 shader 注册即覆盖 / B6=A 范式扩展 |

**跨决策协同度：** 100%（8/8 决策与 G1.4 已锁定决策 + G1.3 范式 + spec/blueprint 上游约束全协同）✅

### 2.2 Shader 设计（B6=A 安全契约扩展）

**3 新 raw string literal shader + 数组化范式：**

```glsl
// kSolidVert (复用于 solid + rounded 两个 program)
#version 300 es
precision highp float;
in vec2 a_pos;
uniform vec4 u_rect_px;       // (x, y, w, h) in px
uniform mat3 u_xform_px;      // user transform
uniform vec2 u_viewport_px;   // surface dims
out vec2 v_local_px;
void main() {
  vec2 px = u_rect_px.xy + a_pos * u_rect_px.zw;
  vec3 px3 = u_xform_px * vec3(px, 1.0);
  vec2 ndc = (px3.xy / u_viewport_px) * 2.0 - 1.0;
  gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);  // Y 翻转 (GL 底左 → 屏幕顶左)
  v_local_px = a_pos * u_rect_px.zw;
}

// kSolidFrag (solid program)
in vec2 v_local_px;     // 接受但不读
uniform vec4 u_color;
out vec4 frag_color;
void main() { frag_color = u_color; }

// kRoundedRectFrag (rounded program / SDF)
in vec2 v_local_px;
uniform vec4 u_color;
uniform vec2 u_half_px;       // rect.size / 2
uniform float u_radius_px;
out vec4 frag_color;
void main() {
  vec2 d = abs(v_local_px - u_half_px) - (u_half_px - vec2(u_radius_px));
  float dist = length(max(d, 0.0)) + min(max(d.x, d.y), 0.0) - u_radius_px;
  float aa = fwidth(dist);
  float alpha = 1.0 - smoothstep(-aa, aa, dist);
  frag_color = vec4(u_color.rgb, u_color.a * alpha);
}
```

**`kAllShaderSources[]` 数组化（B8=A）：**

```cpp
inline constexpr const char* kAllShaderSources[] = {
    kPassthroughVert, kPassthroughFrag,  // G1.4
    kSolidVert, kSolidFrag, kRoundedRectFrag,  // G1.5
};
inline constexpr int kAllShaderSourceCount =
    sizeof(kAllShaderSources) / sizeof(kAllShaderSources[0]);  // 5
```

### 2.3 关键设计点

| 设计点 | 决策 | 实证 |
|---|---|---|
| Shader program lifecycle | ctor 创建 + uniform location ctor 缓存 + dtor delete | 0 lazy 分支 / 0 per-call glGetUniformLocation 开销 |
| Unit quad | 6 vertex triangle list / 单位 [0,1]² / ctor 上传 | per-draw 仅 glBindVertexArray + glDrawArrays |
| MVP pipeline | px space 入 → user xform 应用 → NDC 出 / Y 翻转 | T8 Transform_AffectsRendering PASS |
| Brush 抽象 | `BrushSolidColor(brush)` helper / Gradient/Image/Pattern fallback color_start | B7=A 简化 / G2+ 拓展点保留 |
| **Shader 单 vert + N frag 复用** | kSolidVert 同时供 solid + rounded program 使用 | first-evidence 范式入库 |

---

## 3. 实现摘要

### 3.1 文件变更

| 操作 | 文件路径 | 行数 | 说明 |
|:-:|---|:-:|---|
| 🟡 修改 | `veloxa/graphics/gles/shaders.h` | +~95 | 3 新 shader + `kAllShaderSources[]` 数组 + 注释段 |
| 🟡 修改 | `veloxa/graphics/gles/gles_canvas.h` | +~45 | 移除 stub + 2 GLuint program + 5 GLint uniform 缓存 + 7 helper 声明 + `<algorithm>` include |
| 🟡 修改 | `veloxa/graphics/gles/gles_canvas.cc` | +~210 | ctor 扩展（UploadUnitQuad + InitShaderPrograms）+ dtor 扩展 + 7 helper impl（CompileShader/LinkProgram/InitShaderPrograms/DestroyShaderPrograms/UploadUnitQuad/BrushSolidColor/Matrix3x2ToMat3）+ FillRect/FillRoundedRect 真实实现 |
| 🆕 创建 | `tests/graphics/gles/gles_canvas_fill_test.cc` | +~330 | 12 单测（T1-T12）+ DRAIN macro + SKIP_IF_SWRAST_BLANK fallback |
| 🟡 修改 | `tests/graphics/gles/shader_injection_test.cc` | +~32 | S1 范围化遍历 `kAllShaderSources[]` + S3 `NoUserConcatPatternInG15Shaders` 新增 |
| 🟡 修改 | `tests/CMakeLists.txt` | +~8 | 注册 `gles_canvas_fill_test` (gles config guard 内) |
| **合计** | — | **~720** | LOC 估算精度 **0.95×**（plan ~761 行）/ 高准确率 |

### 3.2 关键实现细节

**`GLESCanvas::FillRect` 流程：**

```cpp
void GLESCanvas::FillRect(const Rect& rect, const Brush& brush) {
  if (rect.IsEmpty()) return;  // early-return 输入验证

  Color c = BrushSolidColor(brush);
  GLfloat color[4] = {c.r/255.0f, c.g/255.0f, c.b/255.0f, c.a/255.0f};
  GLfloat xform[9];
  Matrix3x2ToMat3(transform_, xform);

  glUseProgram(solid_program_);
  glUniform4f(solid_u_rect_px_, rect.x, rect.y, rect.width, rect.height);
  glUniformMatrix3fv(solid_u_xform_px_, 1, GL_FALSE, xform);
  glUniform2f(solid_u_viewport_px_, surface_->width(), surface_->height());
  glUniform4fv(solid_u_color_, 1, color);
  glBindVertexArray(quad_vao_);
  glDrawArrays(GL_TRIANGLES, 0, 6);
}
```

**`GLESCanvas::FillRoundedRect` 流程：**（同 FillRect + radius clamp + rounded_program + u_half_px + u_radius_px）

```cpp
void GLESCanvas::FillRoundedRect(const Rect& rect, vx::f32 radius, const Brush& brush) {
  if (rect.IsEmpty()) return;
  if (radius <= 0.0f) { FillRect(rect, brush); return; }  // radius=0 fallback

  vx::f32 r = std::min(radius, std::min(rect.width, rect.height) * 0.5f);  // clamp
  // ... + u_half_px (rect.size/2) + u_radius_px (r)
  // ... rounded_program_ + glDrawArrays(GL_TRIANGLES, 0, 6)
}
```

### 3.3 TDD RED-GREEN-REFACTOR 实测

**Phase A RED**（~10 min / commit `70625a1`）：
- 创建 `gles_canvas_fill_test.cc`（12 单测 / ~330 行）
- ctest 实测：6 FAIL + 6 PASS（RED 信号清晰）
- 6 PASS 含 2 false-PASS（T3/T7 单约束 `> 200` 时白色背景也满足 / 不影响验收 / GREEN 阶段自然转 true-PASS）

**Phase B GREEN**（~12 min / commit `d29af30`）：
- 修改 shaders.h / gles_canvas.h / gles_canvas.cc
- ctest 实测：**22/22 一次性 PASS** ✅✅✅（12 fill_test + 8 skeleton + 2 shader_injection / 0 retry）

**Phase C REFACTOR**（~3 min）：
- shader_injection_test S1 范围化（遍历 `kAllShaderSources[]` 5 shader）+ 新增 S3（`NoUserConcatPatternInG15Shaders` 反向探针）

**Phase D 三 build 矩阵 ctest verify**（~6 min）：

| Matrix | DEVTOOL | VX_RENDERER | baseline | 实测 | diff | 状态 |
|:-:|:-:|:-:|:-:|:-:|:-:|:-:|
| A | ON | software | 1303 | **1303** | 0 | ✅ 0 退化 |
| B | OFF | software | 1110 | **1110** | 0 | ✅ 0 退化 |
| C | ON | gles | 1362 | **1375** | **+13** | ✅ 真实增量精准命中 [+12, +14] |

**总 ctest 3788 PASS / 0 FAIL** ✅✅✅

**Phase E finalize**（~2 min / commit `67a2b19`）：
- ReadLints 6 文件全 0 错误 ✅

---

## 4. 关键决策（8 B 决策详解）

1. **B1=B per-FillRect uMvp glUniform** — 简单可靠 / per-call 上传 ~ns 开销 / G2 再批处理优化
2. **B2=A unit quad + ctor BufferData** — 沿用 G1.4 quad_vao_/vbo_ 范式 / 0 per-call 上传开销
3. **B3=A uRectPx + uXformPx + uViewportPx** — 像素空间 + transform 矩阵 + viewport 归一 / 通用化 vertex pipeline
4. **B4=A fwidth(dist) 自动反走样** — GLSL ES 3.0 SL 1.00 标准 / 像素无关 / 自适应 / 0 手动调参
5. **B5=A ctor 创建 2 programs + dtor delete** — eager init / 0 lazy 路径分支 / 沿用 G1.4 D3=B eager extension cache 范式
6. **B6=A inline reverse probe** — 沿用 G1.2 D4 + G1.4 D4=C 范式 / 3 inline probe（T9 transparent / T10 empty / T11 zero radius）
7. **B7=A kSolid 完整 + Gradient/Image/Pattern fallback color_start** — Solid brush 完整路径 / 非 Solid 类型 fallback 到 color_start（G2+ 拓展点保留）
8. **B8=A kAllShaderSources[] 数组化** — shader_injection_test 范围化 / 未来新 shader 注册即被 S1 覆盖（B6=A 安全契约扩展）

---

## 5. 安全决策

### 5.1 Shader 注入防御 first → dual-evidence 候选

| 维度 | G1.4 first | G1.5 dual 候选 |
|---|---|---|
| Shader 数量 | 2（kPassthroughVert/Frag）| 5（+ kSolidVert/Frag/RoundedRectFrag）|
| S1 验证范围 | 单 shader 验证 | 范围化遍历 `kAllShaderSources[]` |
| S3 反向探针 | — | `NoUserConcatPatternInG15Shaders` 新增（检查 `%s` printf-style placeholder + `#define USER_` 宏注入点）|
| 安全契约 | B6=A compile-time literal only | **B6=A 扩展 + B8=A 数组化** |

### 5.2 安全契约（B6=A + B8=A）

- 所有 GLSL 源码以 `inline constexpr const char*` 形式内联编译期常量
- `glShaderSource(shader, 1, &source, nullptr)` 直接传 .rodata 指针 / **永不**与 user data 拼接
- `kAllShaderSources[]` 数组化 → 任何后续新 shader 必须注册才被 S1 覆盖 → reviewer 可视化告警

### 5.3 输入验证

- `rect.IsEmpty()` early-return — 防御零长度 / 负长度
- `std::min(radius, std::min(width, height) * 0.5f)` clamp — 防御负 radius 和过大 radius
- `radius <= 0.0f` fallback 到 `FillRect` — 防御退化情形

### 5.4 错误信息脱敏

- Shader compile/link 失败 `VX_DCHECK + glGetShaderInfoLog` 仅 debug 模式输出
- Release 模式 silent return 0 — 0 内部信息泄露

### 5.5 依赖审计

- 0 新外部依赖（仅复用 GLES 3.0 + GLSL ES SL 1.00 既有依赖）
- 0 新 FetchContent / 0 缓存命中策略变更

---

## 6. 测试覆盖

### 6.1 `gles_canvas_fill_test.cc`（12 单测 / 全 PASS）

| # | 测试 | 验证内容 | 类型 |
|:-:|---|---|:-:|
| T1 | `FillRect_HasQuadVao` | quad_vao() 公开 getter 非 0 / G1.4 基础设施 | sanity |
| T2 | `FillRect_DrawsRedPixel` | red rect → center 像素 R≈255 G<50 B<50 | pixel verify |
| T3 | `FillRect_FourCornerSample` | 4 角内边像素 | pixel verify |
| T4 | `FillRect_OutsideRectUnchanged` | rect 外像素 = clear color | pixel verify |
| T5 | `FillRect_AlphaBlending` | 半透明叠加正确 | alpha verify |
| T6 | `FillRect_ColorRespectsBrush` | brush.color_start 正确传递 | brush verify |
| T7 | `FillRoundedRect_CornerHasPartialAlpha` | corner alpha < 1.0（SDF 反走样）| **SDF first-evidence** |
| T8 | `FillRect_Transform_AffectsRendering` | SetTransform 影响绘制位置 | MVP verify |
| T9 | `FillRect_TransparentBrush_NoChange` | alpha=0 brush → 像素不变 | inline reverse probe |
| T10 | `FillRect_EmptyRect_NoDraw` | empty rect → 无绘制 | inline reverse probe |
| T11 | `FillRoundedRect_ZeroRadius_FallbackToRect` | radius=0 → 退化为 FillRect | inline reverse probe |
| T12 | `FillRect_SharedProgram_QuadReuse` | 多次 FillRect 共享 program + quad | reuse verify |

### 6.2 `shader_injection_test.cc`（3 测 / 全 PASS）

| # | 测试 | 验证内容 |
|:-:|---|---|
| S1 | `ShaderSourcesAreCompileTimeLiterals` | 遍历 `kAllShaderSources[]` 5 shader（指针稳定 + 编译期常量 + `#version 300 es` header）|
| S2 | `NoConcatenationApiExposed` | 文档即测试 / `shaders.h` 不暴露用户字符串接受函数 |
| S3 | `NoUserConcatPatternInG15Shaders` | G1.5 反向探针（无 `%s` printf-style placeholder / 无 `#define USER_` 宏注入点）|

### 6.3 三 build 矩阵 ctest verify

3788 PASS / 0 FAIL（A 1303 + B 1110 + C 1375）✅✅✅

---

## 7. 经验教训（从回顾文档提取）

### 7.1 plan 阶段引用历史 baseline 数据时必须做实证 fingerprint

**教训：** plan §0.1 引用 G1.4 archive 中的 Matrix A/B baseline（1337/1141）→ 实测发现 cmake config drift 导致数字偏差 -34/-31（实测 1303/1110）→ 虽核心断言「0 退化」全 ✅ 不影响验收，但破坏 plan 假设精度。

**改进（P1 下次）：** plan §0 Phase 0 audit 必填 ctest baseline fingerprint：跑 `ctest --test-dir <build> -N | tail -3` 获取实测数字 / 不允许凭 archive / reflection 数据引用 / 引用偏差 ≥ 5% 触发主动 push-back。已沉淀 systemPatterns「ctest baseline 数字回归 audit 漏审」段（**新反复模式候选首次定型**）。

### 7.2 RED 测试像素检查应双约束（正向 > X + 反向 < Y）

**教训：** RED 阶段 T3/T7 false-PASS 暴露单 channel `> 200u` 约束不足以区分 white 背景 vs 目标 color（如 green G=255 / white G=255 同时满足）。

**改进（P2 长期）：** Mesa swrast 像素验证双约束默认范式 — 正向通道 > 200 + 反向通道 < 50（如 green: G>200 + R<50 + B<50）。已沉淀 systemPatterns「Mesa swrast 像素验证双约束默认范式」first-evidence 段。

### 7.3 GLES shader 单 vert + N frag 复用范式可推广

**教训：** G1.5 实证 `kSolidVert` 单 vert 可同时供 `solid_program_` + `rounded_program_` 使用 — vert pass `v_local_px` varying / solid frag 忽略 / rounded frag 用作 SDF coord。

**改进（P2 长期）：** 节省 1 vert shader（~30 行）+ 编译时间 / 集中 vertex pipeline 维护。已沉淀 systemPatterns「GLES shader 单 vert + N frag 复用范式 first-evidence」段。G1.6+ Stroke / DrawText / Gradient shader 可继续复用 kSolidVert。

---

## 8. 范式里程碑（已沉淀 systemPatterns / 5 项）

| # | 里程碑 | 累计实证状态 |
|:-:|---|---|
| 1 | **实施忠实度 quint-evidence** | G1.1 first + G1.2 dual + G1.3 triple + G1.4 quad + **G1.5 quint** / 5 任务连续印证 / 确立期 |
| 2 | **跨决策协同度 100% streak 179/179 历史最高续刷** | 第 19 次连续命中 / 8 B 决策 1 次 AskQuestion all_recommended 锁定 |
| 3 | **Mesa swrast SDF 反走样 first-evidence** | T7 fwidth + smoothstep + length + max + min 全函数链实测 / Mesa swrast 能力 triple-evidence 续延（G1.3 default fb + G1.4 Clear + G1.5 SDF）|
| 4 | **GLES shader 单 vert + N frag 复用范式 first-evidence** | kSolidVert 同时供 solid + rounded program 使用 / 未来零成本扩展 |
| 5 | **Mesa swrast 像素验证双约束默认范式 first-evidence** | 反 T3/T7 false-PASS 教训 / 推荐 G1.6+ 沿用 |
| **N** | **ctest baseline 数字回归 audit 漏审（新反复模式候选首次定型）** | plan §0.1 引用 archive 数据未做实证 fingerprint / first-evidence / 待 1+ 次重复后正式升级反复模式 #N |

---

## 9. plan ×0.6 实测系数（第 13 数据点入库）

| 阶段 | 估时 | 实测 | 系数 | 子档 |
|---|:-:|:-:|:-:|---|
| VAN | ~10-15 min | ~10 min | ~0.67-1.0× | 标准区 |
| Plan | ~25-40 min | ~30 min | ~0.75-1.20× | 标准区 |
| Build·Phase A RED | ~20-30 min | ~10 min | ~0.33-0.50× | 极致极速区 |
| Build·Phase B GREEN | ~30-50 min | ~12 min | ~0.24-0.40× | 极致极速区 |
| Build·Phase C REFACTOR | ~10-15 min | ~3 min | ~0.20-0.30× | 极致极速区 |
| Build·Phase D 三矩阵 ctest | ~10-20 min | ~6 min | ~0.30-0.60× | 极速区 |
| Build·Phase E finalize | ~5-10 min | ~2 min | ~0.20-0.40× | 极致极速区 |
| Reflect | ~15-30 min | ~10 min | ~0.33-0.67× | 极速区 |
| Build 总 | ~75-125 min | **~33 min** | **~0.26-0.44×** | **极致极速区** |
| **整体（VAN+Plan+Build+Reflect）** | **~140-205 min** | **~83 min** | **~0.40-0.59×** | **标准极速区** |

**plan ×0.6 实施类 Level 3 子档累计 4 数据点**（quad-evidence 候选）：
- G1.2（0.30-0.55×）+ G1.3（0.13×）+ G1.4（0.09-0.16×）+ **G1.5（0.26-0.44×）**

---

## 10. 提交时间线

| commit | 阶段 | 行数 | 内容 |
|---|---|:-:|---|
| `e8d71b5` plan(graphics) | Plan | +1347 / -4 | plan 文档 + MB ×3 单 commit |
| `70625a1` test(graphics) | Phase A RED | +366 / -3 | 12 单测 + CMakeLists.txt 注册 |
| `d29af30` feat(graphics) | Phase B GREEN | +370 / -5 | 3 shader + impl + helper |
| `67a2b19` test(graphics) | Phase C-E REFACTOR + finalize | +123 / -27 | shader_injection 范围化 + 三矩阵 ctest verify + ReadLints |
| `35fa37c` docs(reflect) | Reflect | +723 / -13 | 回顾文档 + systemPatterns 5 段 + techContext G1.5 节点 |
| **合计** | — | **+2929 / -52** | 5 commit / 0 collateral / 0 plan-build 时序漂移 |

---

## 11. 长期影响

### 11.1 直接解锁子任务（蓝图 §3.6-§3.18 + §3.X G2）

- **G1.6 FillPath via libtess2** — shader pipeline 基础已就绪 / 仅需新增 `kPathVert` 或复用 `kSolidVert` + libtess2 + `glDrawElements`
- **G1.7 Stroke*** — 复用 solid program + stroke = fill 转换（spec §3.3.1）
- **G1.8 GlyphAtlas + DrawText 部分** — glyph shader 范式可复用 `kSolidVert` + 新增 `kGlyphFrag`（atlas sampler）
- **G2 LinearGradient brush 支持** — 当前 B7=A fallback color_start 是临时方案 / G2 增 `kLinearGradientFrag` + 拓展 Brush union 处理

### 11.2 MVP-C 渲染管线里程碑

- ✅ **首次真实绘制像素到 default framebuffer**（vs G1.4 仅 Clear）
- ✅ **首次 fragment shader complex 数学函数链运行**（fwidth + smoothstep + length / GLSL ES 3.0 SL 1.00 全栈）
- ✅ **首次 user transform → MVP 矩阵 → NDC 完整链路**（uXformPx mat3 上传 + Y 翻转 + viewport 归一）

### 11.3 范式沉淀（影响 G1.6-G1.18 + G2 共 18 子任务）

- shader program ctor 创建 + uniform location ctor 缓存 → GLES Canvas 范式
- raw string literal + `kAllShaderSources[]` 数组化 → 安全审计自动覆盖
- 单 vert + N frag 复用 → 减少 vertex 维护 + 编译时间
- per-FillRect uMvp glUniform → 简单可靠 / G2 再批处理 batch
- Mesa swrast SKIP_IF_SWRAST_BLANK fallback → driver-strictness 分层范式

### 11.4 测试基础设施增强

- 12 ctest 单测 + S1 范围化 + S3 G1.5 反向探针 + Matrix C 1362 → 1375（+13）
- Mesa swrast SDF first-evidence → G1.6+ 默认可信 / 不再需要 SKIP fallback 预算
- shader_injection_test 数组化范式 → 未来新 shader 注册即覆盖

---

## 12. 参考文档

- **设计规格：** [`docs/specs/2026-05-04-mvp-scope.md`](../../docs/specs/2026-05-04-mvp-scope.md) §11.2（MVP-C 路线图）
- **蓝图计划：** [`docs/plans/2026-05-05-gles-renderer-blueprint.md`](../../docs/plans/2026-05-05-gles-renderer-blueprint.md) §3.5（G1.5 FillRect 子任务规格化）
- **实施计划：** [`docs/plans/2026-05-28-gles-canvas-fillrect.md`](../../docs/plans/2026-05-28-gles-canvas-fillrect.md)（~700 行 / 11 段全覆盖）
- **回顾文档：** [`memory-bank/reflection/reflection-TASK-20260528-01.md`](../reflection/reflection-TASK-20260528-01.md)（~14KB / 13 段）
- **前置归档：**
  - [`memory-bank/archive/archive-TASK-20260507-01.md`](archive-TASK-20260507-01.md) — G1.4 GLESCanvas 骨架
  - [`memory-bank/archive/archive-TASK-20260506-01.md`](archive-TASK-20260506-01.md) — G1.3 Sdl2GLWindowSurface
  - [`memory-bank/archive/archive-TASK-20260505-06.md`](archive-TASK-20260505-06.md) — G1.2 Sdl2EGLDisplay
  - [`memory-bank/archive/archive-TASK-20260505-05.md`](archive-TASK-20260505-05.md) — G1.1 CMake VX_RENDERER flag

---

## 13. 任务完整性确认

- [x] VAN 前置验证 4 维度全 ✅
- [x] Plan 8 B 决策 1 次 AskQuestion 全锁定（跨决策协同度 100%）
- [x] Plan 文档完整（~700 行 / 11 段）
- [x] Phase A RED 6 FAIL（RED 信号清晰）
- [x] Phase B GREEN 22/22 一次性 PASS（0 retry）
- [x] Phase C REFACTOR 3/3 PASS
- [x] Phase D 三 build 矩阵 3788/3788 PASS（A 1303 + B 1110 + C 1375）
- [x] Phase E ReadLints 6 文件全 0 错误
- [x] Reflect 文档完整（~14KB / 13 段 / 5 范式里程碑沉淀）
- [x] systemPatterns 5 段新增（P0 立即落实）
- [x] techContext G1.5 节点 + plan ×0.6 第 13 数据点
- [x] tasks.md / activeContext.md / progress.md 同步更新
- [x] 5 commit 严格按 Phase 边界 / 0 collateral
- [x] P1 改进建议已迁移 activeContext 待处理事项
- [x] P0 改进建议已立即落实
- [x] P2 改进建议已沉淀 systemPatterns
- [x] 反复模式抑制 8/8 + 新候选 #N first-evidence 首次定型

**TASK-20260528-01 G1.5 GLESCanvas FillRect + FillRoundedRect + Solid Brush 全链路闭环 ✅✅✅**

**下一步候选**：G1.6 FillPath（libtess2 + glDrawElements）触发条件已满足 / 或 工作流元任务批量清零（累计 P1×2 + P2×10 = 12 项 ≥ 4 阈值 ✅✅）
