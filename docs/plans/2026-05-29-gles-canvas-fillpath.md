# G1.6 `GLESCanvas::FillPath` via libtess2 实现计划

**目标：** 将 G1.4/G1.5 骨架中的 `FillPath` 空 stub 替换为 libtess2 CPU tessellation → dynamic VBO/EBO → `glDrawElements` 真实 GPU 实现，复用 G1.5 solid fragment shader 与 uniform 缓存范式，并集成 libtess2 第三方依赖（MPL2 / SGI Free B）。

**架构：**
- `SoftwarePath::commands()` → CPU flatten（LineTo / QuadTo / CubicTo / ArcTo）→ 按 `MoveTo`/`Close` 切分 contour → `tessAddContour` → `tessTesselate(TESS_WINDING_NONZERO, TESS_POLYGONS, 3, 2)`
- 新 vertex shader `kPathVert`：`a_pos` = tess 输出像素坐标 → `u_xform_px` + `u_viewport_px` → NDC（Y 翻转，与 G1.5 一致）
- 复用 `kSolidFrag` + 新 `path_program_`（ctor 创建 / dtor 销毁，对称 G1.5 `solid_program_`）
- per-FillPath `glBufferData(STREAM_DRAW)` 上传 dynamic VBO + EBO；`glDrawElements(GL_TRIANGLES, ...)`

**技术栈：** GLES 3.0 / GLSL ES 3.0 / libtess2（FetchContent + `cmake/LibTess2.cmake` wrapper）/ Mesa swrast / GoogleTest / CMake 4.x / `VX_RENDERER=gles`

**复杂度级别：** Level 3（蓝图 plan §3.6 锁定 / +30% buffer 边界曲折子档）

**上游规格：** [`docs/plans/2026-05-05-gles-renderer-blueprint.md`](2026-05-05-gles-renderer-blueprint.md) §3.6 + [`docs/specs/2026-05-05-gles-renderer-blueprint-design.md`](../specs/2026-05-05-gles-renderer-blueprint-design.md) §4.2 + [`memory-bank/creative/creative-gles-canvas.md`](../../memory-bank/creative/creative-gles-canvas.md) §3

---

## 0. Phase 0 audit（VAN + plan 阶段实证）

### §0.1 ctest baseline fingerprint（VAN 实测 ✅ / 非 archive 引用）

| Matrix | DEVTOOL | VX_RENDERER | 实测 baseline | 本任务后期望 |
|:-:|:-:|:-:|:-:|:-:|
| A | ON | software | **1303** | 1303（0 退化）|
| B | OFF | software | **1141** | 1141（0 退化）|
| C | ON | gles | **1375** | **1383–1387**（+8–12 `gles_canvas_path_test`）|

**命令指纹：**
```bash
ctest --test-dir build -N | tail -3          # → 1303
ctest --test-dir build-no-devtool -N | tail -3  # → 1141
ctest --test-dir build-gles -N | tail -3     # → 1375
```

### §0.2 libtess2 API audit（plan 阶段 GitHub 实证 ✅）

| API | 用途 | 验证 |
|---|---|---|
| `tessNewTess` / `tessDeleteTess` | 生命周期 | ✅ `Include/tesselator.h` |
| `tessAddContour(tess, 2, ptr, stride=8, count)` | 2D contour | ✅ size=2, stride=sizeof(float)*2 |
| `tessTesselate(..., TESS_WINDING_NONZERO, TESS_POLYGONS, 3, 2, nullptr)` | 三角形输出 | ✅ polySize=3 → 每 element 3 indices |
| `tessGetVertices` / `tessGetVertexCount` | VBO 数据源 | ✅ TESSreal = float |
| `tessGetElements` / `tessGetElementCount` | EBO 数据源 | ✅ TESSindex = int → `GL_UNSIGNED_INT` |

**winding rule：** `TESS_WINDING_NONZERO` 与 `SoftwareCanvas` rasterizer 默认 fill rule 对齐（creative §3.3）。

### §0.3 libtess2 CMake 集成 audit（plan 阶段 plan-fact reconcile ⚠️）

**蓝图 §3.6 原假设：** `FetchContent_MakeAvailable(libtess2)` + `target_link_libraries(... tess2)`。

**实证（GitHub API 2026-05-29）：** 上游仓库 **无 `CMakeLists.txt`**（仅 `BUILD.bazel` + `premake4.lua`）。`FetchContent_MakeAvailable` 会直接失败。

**修正方案（B4=A）：** 新增 `cmake/LibTess2.cmake` wrapper：
1. `FetchContent_Declare(libtess2 GIT_REPOSITORY ... GIT_TAG <pinned-commit>)`
2. `FetchContent_Populate(libtess2)`（仅下载，不 `MakeAvailable`）
3. `add_library(tess2 STATIC` 7 个 `.c` 源文件 — 与 `BUILD.bazel` 对齐：`bucketalloc.c dict.c geom.c mesh.c priorityq.c sweep.c tess.c`)
4. `target_include_directories(tess2 PUBLIC ${libtess2_SOURCE_DIR}/Include)`
5. `target_compile_definitions(tess2 PRIVATE TESS2_STATIC=1)`（如需要）
6. 仅在 `VX_RENDERER STREQUAL "gles"` 时 `include(cmake/LibTess2.cmake)`（`veloxa/graphics/CMakeLists.txt`）

**GIT_TAG 策略：** 使用固定 commit hash（非 `v1.0.2` tag — tag 存在但无 CMake；hash 与 `Include/tesselator.h` API 兼容即可）。build Phase 0 首次 populate 时 pin 到 master 当前 HEAD 并写入 plan commit body。

**FetchContent 代理：** `_deps/` 无 libtess2 缓存 → build 阶段首次 configure 需 `git config http.proxy` 或离线预置（沿用 `techContext.md` §FetchContent 与代理 / writing-plans 守卫）。

### §0.4 SoftwarePath 数据结构 audit（plan-fact reconcile ⚠️）

**creative §3.2 原假设：** `sw_path->contours()` — **不存在**。

**实证：** `SoftwarePath` 仅暴露 `const Vector<Command>& commands()`（`software_path.h:41`）。`Command` 含 `CommandType` + `Point p[3]` + `float f[3]`。

**修正（B1=A）：** 在 `gles_canvas.cc` 内 private helper `BuildTessContours(const SoftwarePath&)`：
- 遍历 `commands()`，按 `kMoveTo` 开启新 contour、`kClose` 闭合
- `kLineTo` 直接追加顶点
- `kQuadTo` / `kCubicTo` / `kArcTo` CPU flatten（算法模式复用 `rasterizer.cc` `FlattenQuad`/`FlattenCubic`/arc 分段，**不** link 到 rasterizer 避免循环依赖）
- 输出 `Vector<Vector<Point>>` 供 `tessAddContour` 循环

**transform 应用点（B6=A）：** flatten 后在 tess 前对 contour 顶点应用 `transform_`（与 software rasterizer `GenerateEdges` 一致 — CPU 侧 transform，shader 上传 identity 或仍传 `u_xform_px` 若保留 double-transform 防护 — **推荐 CPU transform + shader identity 等效：仍传 `transform_` 到 `u_xform_px`，flatten 使用未 transform 的 path 坐标**）。

**最终锁定（B6 细化 = B6.A）：** path 坐标保持 document 空间（不 pre-transform），`u_xform_px` 在 draw 时上传 `transform_`（与 G1.5 FillRect 一致）。flatten 仅产生 document-space 折线点。

### §0.5 G1.5 shader pipeline 复用 audit

| 资源 | G1.5 状态 | G1.6 复用 |
|---|---|---|
| `solid_program_` | kSolidVert + kSolidFrag，需 `u_rect_px` | ❌ 不适用任意 mesh |
| `kSolidFrag` | 输出 `u_color` | ✅ 复用（`v_local_px` 未使用）|
| `CompileShader` / `LinkProgram` | static helpers | ✅ 复用 |
| `BrushSolidColor` / `Matrix3x2ToMat3` | 已就位 | ✅ 复用 |
| uniform 缓存 enum 模式 | `SolidUniform` | ✅ 新增 `PathUniform` 子集（无 `u_rect_px`）|

### §0.6 dynamic VAO/VBO/EBO audit

- G1.5 `quad_vao_/vbo_` = unit quad STATIC_DRAW — **不混用**
- G1.6 新增 `path_vao_` / `path_vbo_` / `path_ebo_`（ctor `glGen*` / dtor `glDelete*`，对称 G1.5）
- vertex attrib：`a_pos` location 0，2× float，stride 8
- EBO 绑定到 VAO（`glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, path_ebo_)` 在 VAO bind 期间设置一次 layout；每 draw 前 `glBufferData` 刷新）

### §0.7 add_test config guard audit

| 新增测试 | guard | OFF | software | gles |
|---|---|:-:|:-:|:-:|
| `gles_canvas_path_test` | 既有 `if(VX_RENDERER STREQUAL "gles")` block | ❌ | ❌ | ✅ |

Matrix A/B 不退化；Matrix C +8–12。

### §0.8 工具链版本核对

| 工具 | 版本 | 与 G1.5 一致 |
|---|---|:-:|
| gcc | 15.2.0 | ✅ |
| binutils ld | 2.46 + link-group hotfix | ✅ |
| cmake | 4.2.3 | ✅ |
| Mesa swrast | headless offscreen | ✅ |

---

## 1. 决策矩阵（B1–B9 / plan 阶段 all_recommended 锁定 / 跨决策协同度第 20 次连续命中候选）

| # | 决策 | 锁定 | 理由 |
|:-:|---|---|---|
| **B1** | Path → contour 转换 | **A** `gles_canvas.cc` 内 private flatten helper | YAGNI / 0 新公共模块 / 模式来自 rasterizer |
| **B2** | Vertex shader | **A** 新增 `kPathVert` + 复用 `kSolidFrag` → `path_program_` | `kSolidVert` 依赖 `u_rect_px` 不适配任意 tess mesh |
| **B3** | dynamic geometry 生命周期 | **A** ctor 分配 `path_vao_/vbo_/ebo_` + per-FillPath STREAM_DRAW upload | 蓝图 §3.4 不含 tess cache |
| **B4** | libtess2 集成 | **A** `cmake/LibTess2.cmake` FetchContent + manual `add_library(tess2 STATIC ...)` | 上游无 CMakeLists（§0.3 reconcile）|
| **B5** | path_program 生命周期 | **A** ctor 创建 + dtor 销毁（扩展 G1.5 B5 范式）| 0 lazy init |
| **B6** | Transform 语义 | **A** document-space flatten + `u_xform_px` = `transform_`（同 FillRect）| 与 G1.5 一致 / SetTransform 测试可复用 |
| **B7** | Brush 范围 | **A** kSolid + kLinearGradient fallback `color_start` | 沿用 G1.5 B7 |
| **B8** | 反向探针 | **A** inline：空 path / 透明 brush / 临时改 winding rule 期望 star center 变化 | G1.5 B6=A 范式续延 |
| **B9** | shader_injection | **A** `kPathVert` 注册到 `kAllShaderSources[]` | B8=A 数组化范式 / 0 新 frag |

---

## 2. 文件结构

| # | 文件 | 操作 | 估行 | 职责 |
|:-:|---|:-:|:-:|---|
| 1 | `cmake/LibTess2.cmake` | 🆕 | ~45 | FetchContent populate + tess2 STATIC target |
| 2 | `veloxa/graphics/CMakeLists.txt` | 🟡 | +~8 | gles guard 内 include + link tess2 |
| 3 | `veloxa/graphics/gles/shaders.h` | 🟡 | +~30 | `kPathVert` + `kAllShaderSources[]` 扩展 |
| 4 | `veloxa/graphics/gles/gles_canvas.h` | 🟡 | +~35 | path_program_ + path_vao/vbo/ebo + helper 声明 |
| 5 | `veloxa/graphics/gles/gles_canvas.cc` | 🟡 | +~220 | flatten + tess + FillPath + Init/DestroyPathProgram |
| 6 | `tests/graphics/gles/gles_canvas_path_test.cc` | 🆕 | ~320 | 10 单测 + 3 inline reverse probe |
| 7 | `tests/CMakeLists.txt` | 🟡 | +~8 | 注册 gles_canvas_path_test |
| **合计** | — | — | **~666** | LOC ×[0.85, 1.5] = **566–999** / +30% buffer → **736–1299** |

---

## 3. 实现步骤（TDD 严格顺序）

### Phase A — RED（测试先行）

**步骤 A.1：** 创建 `tests/graphics/gles/gles_canvas_path_test.cc`（fixture 复用 `gles_canvas_fill_test.cc` 的 `Sdl2GlSurfaceEnvironment` + `ReadPixel` + `SKIP_IF_SWRAST_BLANK`）。

**步骤 A.2：** 注册 `gles_canvas_path_test` 于 `tests/CMakeLists.txt` gles guard 内。

**步骤 A.3：** 运行 `ctest -R GLESCanvasPathTest` → 预期 **编译失败或全 FAIL**（FillPath stub）。

#### 测试矩阵（10 单测 + 3 反向探针）

| ID | 名称 | 类型 | 断言要点 |
|:-:|---|---|---|
| T1 | `Construct_InitsPathProgram` | 正向 | FillPath 后 `glGetError()==GL_NO_ERROR` |
| T2 | `FillPath_Triangle_CenterPixel` | 像素 | 3 点三角形铺满 16×16，中心 G>200 |
| T3 | `FillPath_Star_CenterFilled` | 像素 | 自相交星形，中心 opaque（winding nonzero）|
| T4 | `FillPath_QuadBezier_Coverage` | 像素 | `QuadTo` 弧覆盖区域有非零 R |
| T5 | `FillPath_TwoContours` | 像素 | 两个 `MoveTo` contour 均填充 |
| T6 | `FillPath_AfterSetTransform` | 像素 | `SetTransform(Translate)` 后像素位移 |
| T7 | `FillPath_KSolidBrush` | 正向 | 纯蓝 brush，B>200 |
| T8 | `ReverseProbe_EmptyPath` | 反向 | `IsEmpty()` path 不应改像素 |
| T9 | `ReverseProbe_TransparentBrush` | 反向 | alpha=0 不改像素 |
| T10 | `ReverseProbe_WindingRule` | 反向 | 临时 `TESS_WINDING_ODD` → T3 center 断言 FAIL（build 后恢复）|

**像素验证双约束（沿用 G1.5 范式）：** 正向测同时断言 `>200` 与 `<50` 防 false-PASS。

---

### Phase B — GREEN（实现）

**步骤 B.1：** 落地 `cmake/LibTess2.cmake` + `veloxa/graphics/CMakeLists.txt` link。

**步骤 B.2：** `shaders.h` 添加 `kPathVert`：

```glsl
// a_pos = document-space pixel coordinate from tess output
uniform mat3 u_xform_px;
uniform vec2 u_viewport_px;
out vec2 v_local_px;  // required by kSolidFrag link, unused
```

**步骤 B.3：** `gles_canvas.{h,cc}`：
- ctor/dtor：`path_vao_/vbo_/ebo_` + `InitPathProgram()` / `DestroyPathProgram()`
- `BuildTessContours()` + `FlattenPathCommands()` private helpers
- `FillPath()` 完整实现

**步骤 B.4：** `ctest -R GLESCanvasPathTest` → **10/10 PASS**。

---

### Phase C — REFACTOR

**步骤 C.1：** `shader_injection_test.cc` — 确认 S1 自动覆盖 `kPathVert`（`kAllShaderSources[]` 已注册则 0 改动；否则 +1 条目）。

**步骤 C.2：** ReadLints 全文件 0 错误。

---

### Phase D — 三 build 矩阵 ctest verify

```bash
# Matrix A
ctest --test-dir build -N | tail -3                    # 1303 不退化
# Matrix B
ctest --test-dir build-no-devtool -N | tail -3          # 1141 不退化
# Matrix C
cmake -B build-gles -DVX_RENDERER=gles -DVX_BUILD_DEVTOOL=ON  # 首次含 libtess2 fetch
cmake --build build-gles --target gles_canvas_path_test -j
ctest --test-dir build-gles -R GLESCanvasPathTest
ctest --test-dir build-gles -N | tail -3               # 1375 + 8~12
```

---

## 4. Commit 时间线（build 阶段）

| Phase | commit subject | 文件 |
|---|---|---|
| Plan | `chore(plan): land G1.6 FillPath plan + memory bank` | plan + MB ×3 |
| A RED | `test(graphics): add GLESCanvas FillPath tests — G1.6 RED` | test + CMakeLists |
| B1 | `build(deps): add libtess2 via LibTess2.cmake wrapper — G1.6` | cmake + graphics CMakeLists |
| B2 | `feat(graphics): implement GLESCanvas FillPath via libtess2 — G1.6` | shaders + gles_canvas + path test green |
| C | `test(graphics): extend shader_injection for kPathVert — G1.6` | 若需 |
| D | `chore(build): finalize TASK-20260529-01 ctest matrix — G1.6` | MB finalize |

---

## 5. 风险登记

| ID | 风险 | 级别 | 缓解 |
|---|---|:-:|---|
| R1 | libtess2 FetchContent 网络失败 | 🟡 | proxy 守卫 / 离线 `_deps` 预置 |
| R2 | 上游无 CMakeLists | 🟡 | ✅ B4=A wrapper 已 reconcile |
| R3 | Bezier flatten 精度 vs 测试 flaky | 🟡 | 宽松 tolerance + 16×16 小 canvas |
| R4 | `glDrawElements` + Mesa swrast EBO | 🟢 | G1.5 已证 Mesa 可靠；T2 探针 |
| R5 | creative `contours()` API 不存在 | 🟢 | ✅ B1=A flatten helper |

---

## 6. 反复模式预防（8/8）

| # | 模式 | 抑制 |
|:-:|---|---|
| #1 | 前置依赖未验证 | §0.1–0.8 全 audit ✅ |
| #2 | spec 数据回归 | §0.3/0.4 plan-fact reconcile 写入 plan ✅ |
| #3 | TDD 倒置 | Phase A 先于 B ✅ |
| #4 | 反向探针弱 | T8–T10 ✅ |
| #5 | 中文 StrReplace | 无中文 doc 改动 ✅ |
| #6 | Source 溯源 | commit body 必填 ✅ |
| #7 | 双 config 盲区 | Phase D 三矩阵 ✅ |
| #8 | ctest baseline 回归 | §0.1 fingerprint 实测 ✅ |

---

## 7. 估时（plan ×0.6）

| 阶段 | 估时 |
|---|---|
| VAN | ~10 min（已完成）|
| Plan | ~30–45 min |
| Build A RED | ~15–25 min |
| Build B GREEN | ~40–70 min（含 libtess2 首次 fetch + flatten 调试）|
| Build C–D | ~15–25 min |
| **总计 plan ×0.6** | **~110–175 min** |
| **预期实测** | **~90–150 min**（G1.5 quint-evidence 极速区 0.40–0.59× 续延）|

---

## 8. systemPatterns 协同度对照（G1.6 适用项）

| 范式 | 适用 |
|---|:-:|
| 实施忠实度 quint-evidence 候选 | ✅ |
| Mesa swrast 像素验证双约束 | ✅ T2/T3 |
| GLES shader 单 vert + N frag 复用 | ✅ kPathVert + kSolidFrag |
| shader_injection kAllShaderSources[] | ✅ B9 |
| ctest baseline fingerprint 协议 | ✅ §0.1 first 完整实践 |
| REFACTOR 涌现 cmake 子模块 | ✅ LibTess2.cmake 候选 |

---

**下一步：** `/build` — Phase A RED → Phase B GREEN → Phase C REFACTOR → Phase D 三矩阵

**Source:** `docs/plans/2026-05-05-gles-renderer-blueprint.md` §3.6 + `docs/specs/2026-05-05-gles-renderer-blueprint-design.md` §4.2 + `memory-bank/creative/creative-gles-canvas.md` §3
