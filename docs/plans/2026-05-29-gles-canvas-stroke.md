# G1.7 `GLESCanvas::Stroke*`（Stroke = Fill 转换）实现计划

**目标：** 将 G1.4–G1.6 骨架中 4 个 `Stroke*` 空 stub 替换为 **Stroke = Fill 转换** 真实 GPU 实现，复用 G1.5 FillRect/FillRoundedRect 与 G1.6 FillPath/libtess2，0 新 fragment shader。

**架构：**
- `StrokeRect` → 4× `FillRect` 边条（creative §4.1）
- `StrokeLine` → `PushState` + `Matrix3x2::Multiply(Translation×Rotation)` + 居中 `FillRect`（creative §4.3 / plan-fact：`Matrix3x2` 无链式 `.Translate().Rotate()`）
- `StrokeRoundedRect` → outer/inner `FillRoundedRect` + **stencil 减法**（creative §4.2 / SDL 已请求 8-bit stencil）
- `StrokePath` → **segment-quad** 范式（对齐 `rasterizer.cc` StrokePath）→ 每段 mini-`SoftwarePath` + `FillPath`（非 creative §4.4 `OffsetPath`）

**技术栈：** GLES 3.0 / GLSL ES 3.0 / 复用 G1.5–G1.6 shader + libtess2 / Mesa swrast / GoogleTest / `VX_RENDERER=gles`

**复杂度级别：** Level 3（蓝图 plan §3.7 锁定）

**上游规格：** [`docs/plans/2026-05-05-gles-renderer-blueprint.md`](2026-05-05-gles-renderer-blueprint.md) §3.7 + [`docs/specs/2026-05-05-gles-renderer-blueprint-design.md`](../specs/2026-05-05-gles-renderer-blueprint-design.md) §3.3.1 + [`memory-bank/creative/creative-gles-canvas.md`](../../memory-bank/creative/creative-gles-canvas.md) §4

---

## 0. Phase 0 audit（VAN + plan 阶段）

### §0.1 ctest baseline fingerprint（VAN 实测 ✅）

| Matrix | DEVTOOL | VX_RENDERER | 实测 baseline | 本任务后期望 |
|:-:|:-:|:-:|:-:|:-:|
| A | ON | software | **1303** | 1303（0 退化）|
| B | OFF | software | **1141** | 1141（0 退化）|
| C | ON | gles | **1385** | **1397–1401**（+12–16 `gles_canvas_stroke_test`）|

```bash
ctest --test-dir build-gles -N | tail -3     # → 1385
ctest --test-dir build -N | tail -3          # → 1303
ctest --test-dir build-no-devtool -N | tail -3  # → 1141
```

### §0.2 G1.5/G1.6 依赖 audit（✅）

| 方法 | 依赖 | 状态 |
|---|---|---|
| `FillRect` | `solid_program_` + `quad_vao_` | ✅ G1.5 |
| `FillRoundedRect` | `rounded_program_` + SDF frag | ✅ G1.5 |
| `FillPath` | `path_program_` + libtess2 + `path_vao_/vbo_/ebo_` | ✅ G1.6 |
| `PushState` / `PopState` | `state_stack_` + `transform_` | ✅ G1.4 |

### §0.3 stencil buffer audit（plan-fact reconcile ✅）

**creative §4.2 假设：** default FBO 有 stencil 可用于 StrokeRoundedRect。

**实证：** `sdl2_egl_display.cc:41` — `SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8)` 已在 G1.2 就位；`creative-gles-context.md` 标注 PushClipPath / stroke 需要 stencil。

**build 探针（Phase B 首测）：** `StrokeRoundedRect` 测试前 `glGetIntegerv(GL_STENCIL_BITS, ...)` 断言 ≥ 8；若 0 → `GTEST_SKIP` + 记录 driver-strictness。

### §0.4 StrokePath 算法 audit（plan-fact reconcile ⚠️）

**creative §4.4 原假设：** `SoftwarePath::OffsetPath(±width/2)` + dual contour — **不存在**。

**实证：** `rasterizer.cc:399-484` — flatten path → `Segment{a,b}` → 每段法向 offset 半宽 → 4 点 quad → `FillPath(quad)`。

**修正（B4=A）：** `gles_canvas.cc` 内 private `CollectStrokeSegments()` + `StrokeSegmentQuad()` + 循环 `FillPath(mini_path, brush)`。document-space 折线 + `FillPath` 上传 `transform_`（与 G1.6 B6 一致）。

### §0.5 Matrix3x2 StrokeLine audit（plan-fact reconcile ⚠️）

**creative §4.3 伪代码：** `xform.Translate(...).Rotate(...)` — **不存在**（`types.h` 仅 `static Translation/Rotation` + `Multiply`）。

**修正（B2=A）：**
```cpp
Matrix3x2 line_xform = transform_.Multiply(
    Matrix3x2::Translation(mid.x, mid.y).Multiply(Matrix3x2::Rotation(angle)));
```

### §0.6 SoftwareCanvas 行为对齐 audit

| 方法 | SW 实现 | GLES 对齐策略 |
|---|---|---|
| `StrokeRect` | path → StrokePath | 4× FillRect（几何等价）|
| `StrokeLine` | 2 点 path → StrokePath | 旋转 FillRect（视觉等价）|
| `StrokeRoundedRect` | rounded path → StrokePath | stencil ring（视觉等价）|
| `StrokePath` | segment quads → FillPath | 同算法 ✅ |

### §0.7 add_test config guard

| 新增测试 | guard | OFF | software | gles |
|---|---|:-:|:-:|:-:|
| `gles_canvas_stroke_test` | 既有 `if(VX_RENDERER STREQUAL "gles")` | ❌ | ❌ | ✅ |

### §0.8 工具链 / FetchContent

- 0 新第三方依赖 / FetchContent 守卫 ⊘ 跳过
- gcc 15.2.0 / Mesa swrast / cmake 4.2.3 — 与 G1.6 一致 ✅

---

## 1. 决策矩阵（B1–B8 / all_recommended 锁定）

| # | 决策 | 锁定 | 理由 |
|:-:|---|---|---|
| **B1** | StrokeRect 实现 | **A** 4× `FillRect` | creative §4.1 / 0 新代码路径 |
| **B2** | StrokeLine 变换 | **A** `PushState` + `Multiply(T×R)` + 居中 FillRect | plan-fact Matrix3x2 API |
| **B3** | StrokeRoundedRect | **A** stencil outer/inner FillRoundedRect | creative §4.2 / stencil 8-bit 已配置 |
| **B4** | StrokePath 算法 | **A** rasterizer segment-quad → FillPath | OffsetPath 不存在 / 与 SW 对齐 |
| **B5** | 边界输入 | **A** `width<=0` / `alpha==0` / 零长度线段 early return | G1.6 反向探针范式 |
| **B6** | Shader 范围 | **A** 0 新 shader（defer `kRoundedRectStrokeFrag`）| creative §4.2 优化路径留 reflect |
| **B7** | Helper 位置 | **A** private helpers in `gles_canvas.cc` | YAGNI / 与 G1.6 flatten 同模式 |
| **B8** | 测试 + 反向探针 | **A** ~14 单测 + 3 inline reverse probe | G1.5/G1.6 像素双约束续延 |

---

## 2. 文件结构

| # | 文件 | 操作 | 估行 | 职责 |
|:-:|---|:-:|:-:|---|
| 1 | `veloxa/graphics/gles/gles_canvas.h` | 🟡 | +~15 | 移除 Stroke stub `{}` → 声明；可选 `Stroke*` private helper 声明 |
| 2 | `veloxa/graphics/gles/gles_canvas.cc` | 🟡 | +~200 | 4× Stroke* + CollectStrokeSegments + StrokeSegmentQuad + stencil RAII |
| 3 | `tests/graphics/gles/gles_canvas_stroke_test.cc` | 🆕 | ~380 | 14 单测 + 3 reverse probe |
| 4 | `tests/CMakeLists.txt` | 🟡 | +~8 | 注册 gles_canvas_stroke_test |
| **合计** | — | — | **~603** | LOC ×[0.85, 1.5] = **512–905** / +30% buffer → **666–1176** |

---

## 3. 实现步骤（TDD 严格顺序）

### Phase A — RED

**A.1** 创建 `tests/graphics/gles/gles_canvas_stroke_test.cc`（fixture 复用 `gles_canvas_fill_test.cc` / `gles_canvas_path_test.cc`）。

**A.2** 注册 `gles_canvas_stroke_test`（gles guard）。

**A.3** `ctest -R GLESCanvasStrokeTest` → 编译通过 + 像素测 **FAIL**（stub 无绘制）。

#### 测试矩阵（14 单测 + 3 反向探针）

| ID | 名称 | 断言要点 |
|:-:|---|---|
| T1 | `StrokeRect_BorderPixel` | 32×32 框 stroke w=4，边中心 R>200 / 角外白 |
| T2 | `StrokeLine_Diagonal` | 对角线 w=3，中点红色 |
| T3 | `StrokeRoundedRect_Ring` | radius=8 w=2，角外环红 / 内白 |
| T4 | `StrokePath_TriangleOutline` | 三角形描边，边中心红 |
| T5 | `StrokePath_AfterSetTransform` | Translate 后边位移 |
| T6 | `StrokeRect_AfterSetTransform` | 同 T5 rect 版 |
| T7 | `StrokeLine_Horizontal` | 水平线 y 固定 |
| T8 | `StrokePath_StarOutline` | 星形自相交描边中心红（nonzero）|
| T9 | `StrokePath_KSolidBrush` | 纯蓝 B>200 |
| T10 | `ReverseProbe_ZeroWidth` | width=0 不改像素 |
| T11 | `ReverseProbe_TransparentBrush` | alpha=0 不改像素 |
| T12 | `ReverseProbe_ZeroLengthLine` | 同点线段 no-op |
| T13 | `StrokeRect_MultipleDraws_NoGLError` | 3× draw glGetError clean |
| T14 | `StrokeRoundedRect_SmallWidth` | width=1 细环仍可测 |

**像素双约束：** 正向测 R>200 且 背景通道 <50（沿用 G1.5 范式）。

---

### Phase B — GREEN

**B.1** `StrokeRect` — creative §4.1 四边 FillRect；`width` clamp 至 `min(w,h)*0.5`。

**B.2** `StrokeLine` — 长度/角度/Multiply 变换 + PushState/PopState。

**B.3** `StrokeRoundedRect` — stencil 6 步（creative §4.2）；`Begin()` 不全局开 stencil — **局部 RAII** `ScopedStencilRing` 保存/恢复 `GL_STENCIL_TEST` + color mask。

**B.4** `StrokePath` — `CollectStrokeSegments`（复用 G1.6 flatten 模式）+ per-segment quad FillPath。

**B.5** `ctest -R GLESCanvasStrokeTest` → **14/14 PASS**。

---

### Phase C — REFACTOR

- 提取 `ScopedStencilRing` 若 StrokeRoundedRect 超过 ~40 行
- ReadLints 全文件 0 错误
- 0 改动 `shader_injection_test`（B6=A 无新 shader）

---

### Phase D — 三 build 矩阵

```bash
ctest --test-dir build-gles -R GLESCanvasStrokeTest
ctest --test-dir build -N | tail -3          # 1303
ctest --test-dir build-no-devtool -N | tail -3  # 1141
ctest --test-dir build-gles -N | tail -3     # 1385 + 12~16
```

---

## 4. Commit 时间线（build 阶段）

| Phase | commit subject |
|---|---|
| Plan | `chore(plan): land G1.7 Stroke* plan + memory bank` |
| A RED | `test(graphics): add GLESCanvas Stroke* tests — G1.7 RED` |
| B GREEN | `feat(graphics): implement GLESCanvas Stroke* via Fill conversion — G1.7` |
| C–D | `chore(build): finalize TASK-20260529-02 ctest matrix — G1.7` |

---

## 5. 风险登记

| ID | 风险 | 级别 | 缓解 |
|---|---|:-:|---|
| R1 | Mesa swrast stencil 无效 | 🟡 | T3 `GL_STENCIL_BITS` 探针 + GTEST_SKIP |
| R2 | StrokePath 多段 FillPath 性能 | 🟢 | MVP 可接受 / G1.7 reflect 评估 batch |
| R3 | creative OffsetPath 不存在 | 🟢 | ✅ B4=A reconcile |
| R4 | StrokeLine 变换 API | 🟢 | ✅ B2=A Multiply |

---

## 6. 反复模式预防（8/8）

| # | 模式 | 抑制 |
|:-:|---|---|
| #1 | 前置依赖未验证 | §0.1–0.8 ✅ |
| #2 | spec 数据回归 | creative OffsetPath reconcile 写入 plan ✅ |
| #3 | TDD 倒置 | Phase A 先于 B ✅ |
| #4 | 反向探针弱 | T10–T12 ✅ |
| #5 | 中文 StrReplace | 无中文 doc 改动 ✅ |
| #6 | Source 溯源 | commit body 必填 ✅ |
| #7 | 双 config 盲区 | Phase D 三矩阵 ✅ |
| #8 | ctest baseline | §0.1 实测 1385 ✅ |

---

## 7. 估时（plan ×0.6）

| 阶段 | 估时 |
|---|---|
| Plan | ~30–45 min |
| Build A RED | ~15–25 min |
| Build B GREEN | ~45–70 min（stencil + StrokePath 调试）|
| Build C–D | ~15–25 min |
| **总计 plan ×0.6** | **~105–165 min** |
| **预期实测** | **~90–140 min**（G1.5/G1.6 hex-evidence 极速区续延）|

---

**下一步：** `/build` — Phase A RED → Phase B GREEN → Phase C/D 验证

**Source:** `docs/plans/2026-05-05-gles-renderer-blueprint.md` §3.7 + `memory-bank/creative/creative-gles-canvas.md` §4
