# 归档：G1.6 `GLESCanvas::FillPath` via libtess2

**日期：** 2026-05-29
**任务 ID：** TASK-20260529-01
**复杂度级别：** Level 3（实施类 / GLES 蓝图实施第六步 / MVP-C 战略主线第六个实施任务）
**状态：** ✅ 已完成
**分支：** `feature/TASK-20260529-01-gles-canvas-fillpath`（基于 main `8fabf98`）
**总提交数：** 6（plan + RED + deps + feat + reflect + archive）
**ctest 指纹：** A **1303** / B **1141** / C **1385**（+10）
**LOC 精度：** 0.94×（~624 代码行 vs plan ~666）

---

## 1. 任务概述

GLES 蓝图实施第六步 — 在 G1.5 已落地的 solid shader pipeline 之上，将 `FillPath` 由空 stub 替换为 **libtess2 CPU tessellation → dynamic VBO/EBO → `glDrawElements`** 真实 GPU 实现。

### 1.1 任务目标

- **libtess2 集成：** FetchContent v1.0.2 + `cmake/LibTess2.cmake` manual STATIC target（上游无 CMakeLists）
- **Path 数据路径：** `SoftwarePath::commands()` → CPU flatten → `tessAddContour` → `tessTesselate(TESS_WINDING_NONZERO)`
- **Shader：** 新增 `kPathVert` + 复用 `kSolidFrag` → `path_program_`（B2=A）
- **Geometry：** `path_vao_/vbo_/ebo_` ctor 分配 + per-FillPath STREAM_DRAW upload

### 1.2 战略价值（MVP-C 渲染管线里程碑）

- **复杂路径首次 GPU 绘制** ✅（三角形 / 星形 / Bezier / 多 contour）
- **Mesa swrast glDrawElements + dynamic VBO quad-evidence** ✅
- **解锁 G1.7 Stroke***（stroke = fill 转换 / 复用 tess 基础设施）

### 1.3 前置链

G1.1 ✅ → G1.2 ✅ → G1.3 ✅ → G1.4 ✅ → G1.5 ✅ → **G1.6（本任务）** → G1.7 解锁。

---

## 2. 技术方案

### 2.1 B1–B9 决策矩阵（plan 阶段 all_recommended 锁定）

| # | 决策 | 锁定 | 理由 |
|:-:|---|:-:|---|
| B1 | Path → contour | **A** private flatten helper | YAGNI / 模式来自 rasterizer |
| B2 | Vertex shader | **A** `kPathVert` + `kSolidFrag` | `kSolidVert` 依赖 `u_rect_px` 不适配 tess mesh |
| B3 | dynamic geometry | **A** ctor VAO/VBO/EBO + STREAM_DRAW | 蓝图 §3.4 无 tess cache |
| B4 | libtess2 集成 | **A** `LibTess2.cmake` FetchContent + manual STATIC | 上游无 CMakeLists |
| B5 | path_program 生命周期 | **A** ctor/dtor | 对称 G1.5 B5 |
| B6 | Transform | **A** document-space flatten + `u_xform_px` | 同 FillRect |
| B7 | Brush | **A** kSolid + gradient fallback | 沿用 G1.5 |
| B8 | 反向探针 | **A** inline 空 path / 透明 brush / 多次 draw | G1.5 范式续延 |
| B9 | shader_injection | **A** `kPathVert` → `kAllShaderSources[]` | B8=A 数组化 |

### 2.2 数据流

```
SoftwarePath::commands()
  → BuildTessContours() [flatten Line/Quad/Cubic/Arc]
  → tessNewTess → tessAddContour ×N
  → tessTesselate(TESS_WINDING_NONZERO, TESS_POLYGONS, 3, 2)
  → glBufferData(VBO vertices) + glBufferData(EBO indices)
  → glDrawElements(GL_TRIANGLES, GL_UNSIGNED_INT)
  → kPathVert + kSolidFrag (u_xform_px, u_viewport_px, u_color)
```

### 2.3 plan-fact reconcile（build 前已消化）

| 发现 | 决策 |
|---|---|
| libtess2 无 CMakeLists | B4=A `cmake/LibTess2.cmake` |
| `SoftwarePath` 无 `contours()` | B1=A 从 `commands()` flatten |
| 根 project 仅 LANGUAGES CXX | `LibTess2.cmake` 内 `enable_language(C)`（build 涌现）|

---

## 3. 实现摘要

### 3.1 文件变更

| 操作 | 文件路径 | 说明 |
|------|---------|------|
| 创建 | `cmake/LibTess2.cmake` | FetchContent v1.0.2 + tess2 STATIC（7× .c）|
| 修改 | `veloxa/graphics/CMakeLists.txt` | gles guard 内 include + link tess2 |
| 修改 | `veloxa/graphics/gles/shaders.h` | +`kPathVert` + `kAllShaderSources[]` |
| 修改 | `veloxa/graphics/gles/gles_canvas.h` | path_program_ + path_vao/vbo/ebo |
| 修改 | `veloxa/graphics/gles/gles_canvas.cc` | flatten + tess + FillPath (~253 行) |
| 创建 | `tests/graphics/gles/gles_canvas_path_test.cc` | 10 单测 + 3 reverse probe |
| 修改 | `tests/CMakeLists.txt` | 注册 gles_canvas_path_test |
| 创建 | `docs/plans/2026-05-29-gles-canvas-fillpath.md` | 实现计划 ~293 行 |

### 3.2 关键决策

1. **不修改根 CMakeLists.txt** — FetchContent 仅在 gles graphics 子树 lazy attach（YAGNI）
2. **不暴露 tess winding API** — T10 改为多次 draw 稳定性探针，非生产 API 探针
3. **flatten 内联** — 不 link rasterizer，避免循环依赖

### 3.3 安全决策

本任务不涉及安全变更。shader 注入防御通过 `kAllShaderSources[]` 续延（B9=A）；libtess2 为 MPL2 编译期依赖，无用户 GLSL 拼接。

---

## 4. 测试覆盖

| ID | 测试名 | 类型 | 结果 |
|:-:|---|:-:|:-:|
| T1 | Construct_InitsPathProgram | GL 无错误 | ✅ |
| T2 | FillPath_Triangle_CenterPixel | 像素 R>200 | ✅ |
| T3 | FillPath_Star_CenterFilled | 自相交星形 | ✅ |
| T4 | FillPath_QuadBezier_Coverage | QuadTo 区域 | ✅ |
| T5 | FillPath_TwoContours | 双 MoveTo | ✅ |
| T6 | FillPath_AfterSetTransform | SetTransform 位移 | ✅ |
| T7 | FillPath_KSolidBrush | 纯蓝 B>200 | ✅ |
| T8 | ReverseProbe_EmptyPath | 空 path 不改像素 | ✅ |
| T9 | ReverseProbe_TransparentBrush | alpha=0 不改像素 | ✅ |
| T10 | FillPath_MultipleDraws_NoGLError | 多次 draw | ✅ |

**TDD：** Phase A RED 4/10 FAIL → Phase B GREEN 10/10 PASS  
**三矩阵：** A 1303 / B 1141 / C 1385（0 退化）

---

## 5. 经验教训（摘自 Reflect）

1. FetchContent C 库必须 checklist `enable_language(C)`
2. Bezier 像素测应给出曲线内解析采样坐标（T4 y=2→y=10）
3. GLES 测试调用 `MakeCurrent()` 须 include `sdl2_egl_display.h`
4. libtess2 API 类型为 `TESStesselator*`，非 `Tess*`

---

## 6. 改进建议（已迁移 activeContext）

| 优先级 | 建议 |
|--------|------|
| P1 | FetchContent C 依赖 `enable_language(C)` checklist |
| P1 | Bezier 像素测 plan 模板补解析坐标 |
| P2 | libtess2 `_deps` 离线预置 |
| P2 | T10 winding rule 探针 deferred → G1.7 |

---

## 7. 参考文档

- 蓝图：`docs/plans/2026-05-05-gles-renderer-blueprint.md` §3.6
- 规格：`docs/specs/2026-05-05-gles-renderer-blueprint-design.md` §4.2
- 创意：`memory-bank/creative/creative-gles-canvas.md` §3
- 实现计划：`docs/plans/2026-05-29-gles-canvas-fillpath.md`
- 回顾：`memory-bank/reflection/reflection-TASK-20260529-01.md`

---

## 8. 下一步

- **G1.7 Stroke*** — 复用 path tess + offset / Level 3
- **G1.8 DrawText 部分** — glyph atlas + GLES draw

**Source:** `docs/plans/2026-05-05-gles-renderer-blueprint.md` §3.6
