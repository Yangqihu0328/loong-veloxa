# 回顾：G1.6 `GLESCanvas::FillPath` via libtess2

**日期：** 2026-05-29
**任务 ID：** TASK-20260529-01
**复杂度级别：** Level 3（实施类 / GLES 蓝图实施第六步 / MVP-C 战略主线第六个实施任务）
**分支：** `feature/TASK-20260529-01-gles-canvas-fillpath`
**前置任务：** TASK-20260528-01 G1.5 FillRect + FillRoundedRect ✅ 已闭环

---

## 1. 任务定位

GLES 蓝图实施第六步 — 在 G1.5 已落地的 solid shader pipeline 之上，将 `FillPath` 由 stub 替换为 libtess2 CPU tessellation → dynamic VBO/EBO → `glDrawElements` 真实 GPU 实现。

**主交付：**
- `cmake/LibTess2.cmake` — FetchContent v1.0.2 + manual `tess2` STATIC target
- `kPathVert` + 复用 `kSolidFrag` → `path_program_`（B2=A）
- `SoftwarePath::commands()` flatten → `tessAddContour` → `tessTesselate(TESS_WINDING_NONZERO)`
- `path_vao_/vbo_/ebo_` ctor 分配 + per-FillPath STREAM_DRAW upload
- 10 ctest 单测（含 3 inline reverse probe）+ shader_injection S1 自动覆盖 `kPathVert`

---

## 2. 计划 vs 实际

### 2.1 总体对照

| 维度 | 计划 | 实际 | 偏差原因 |
|:-:|---|---|---|
| Phase 数 | 4（A-D）| 4（A-D）| 0 / 精准匹配 ✅ |
| commit 数 | 5（plan + A + B1 + B2 + C/D）| **0**（全工作区未提交）| 单会话 build+reflect 合并 / 待用户显式 commit ⚠️ |
| 估时 | plan ×0.6 ~110-175 min | ~单会话（VAN+Plan 前序 + Build ~1h 量级）| 待 Archive 精确计时 |
| 文件变更 | 7 文件 | **7 代码文件** + plan + MB | 0 根 `CMakeLists.txt` 改动（YAGNI 收敛）✅ |
| 设计变更 | — | 2 项小偏差 | 见 §2.3 |
| ctest 增量 | Matrix C +8–12 | Matrix C **+10**（1375→1385）| 精准命中区间 ✅ |

### 2.2 文件变更详细对照

| # | 文件 | plan 估行 | 实际 | 系数 | 备注 |
|:-:|---|:-:|:-:|:-:|---|
| 1 | `cmake/LibTess2.cmake` | ~45 | 41 | 0.91× ✅ | + `enable_language(C)`（plan 未写）|
| 2 | `veloxa/graphics/CMakeLists.txt` | +~8 | +3 | 0.38× ✅ | include + link tess2 |
| 3 | `veloxa/graphics/gles/shaders.h` | +~30 | +18 | 0.60× ✅ | kPathVert + kAllShaderSources[] |
| 4 | `veloxa/graphics/gles/gles_canvas.h` | +~35 | +21 | 0.60× ✅ | path_program_ + path_vao/vbo/ebo |
| 5 | `veloxa/graphics/gles/gles_canvas.cc` | +~220 | +~253 net | 1.15× ✅ | flatten helpers + FillPath |
| 6 | `tests/graphics/gles/gles_canvas_path_test.cc` | ~320 | 280 | 0.88× ✅ | 10 单测 |
| 7 | `tests/CMakeLists.txt` | +~8 | +8 | 1.0× ✅ | gles guard 注册 |
| — | 根 `CMakeLists.txt` | +~10 | **0** | — | plan 过度估计 / 实际 graphics 子树 include 即可 ✅ |
| **合计** | — | **~666** | **~624 代码行** | **0.94×** ✅ | LOC buffer [566, 999] 内 |

### 2.3 设计变更

| 变更 | 计划 | 实际 | 理由 |
|---|---|---|---|
| CMake 入口 | 根 `CMakeLists.txt` FetchContent | 仅 `cmake/LibTess2.cmake` + graphics include | YAGNI / gles guard 内 lazy attach |
| T10 反向探针 | `ReverseProbe_WindingRule`（临时 TESS_WINDING_ODD）| `FillPath_MultipleDraws_NoGLError` | 不暴露 tess 内部到生产 API / 多次 draw 稳定性探针 |
| C 语言启用 | 未显式写 | `LibTess2.cmake` 内 `enable_language(C)` | 根 project 仅 LANGUAGES CXX → tess2 编译失败 |

---

## 3. 回顾检查清单（代码变更类）

- [x] **计划精确度** — 文件清单 7/7 命中；根 CMakeLists 未改属正向收敛；LOC 0.94×
- [x] **TDD 执行情况** — Phase A RED 4/10 FAIL（像素测）→ Phase B GREEN 10/10 PASS；严格 RED→GREEN
- [ ] **子代理质量** — N/A（单 agent 直写）
- [x] **测试隔离** — 每测独立 `Sdl2GLWindowSurface`；沿用 G1.5 fixture
- [ ] **提交粒度** — ⚠️ 偏离计划（0 commit / 全工作区 pending）
- [x] **非默认路径** — 空 path / alpha=0 brush / 双 contour / SetTransform 已测

---

## 4. 做得好的（8 项）

### 4.1 ⭐ plan-fact reconcile 前置消化

libtess2 无 CMakeLists、SoftwarePath 无 `contours()` 两项在 plan Phase 0 已 reconcile，build 阶段 0 API 惊喜。

### 4.2 ⭐ B1–B9 决策 build 阶段 0 偏差

`kPathVert` + `path_program_`、`LibTess2.cmake` wrapper、document-space flatten + shader `u_xform_px` 全部按 plan 锁定实施。

### 4.3 ⭐ TDD RED 证据清晰

stub 阶段 4 像素测 FAIL（白底 R=255 误通过 G/B 约束捕获），GREEN 后 10/10，无 flaky。

### 4.4 ⭐ 三 build 矩阵 0 退化

A 1303 / B 1141 / C 1385（+10），与 fingerprint 协议一致。

### 4.5 ⭐ flatten 算法复用 rasterizer 模式

`FlattenQuad`/`FlattenCubic`/arc 分段内联于 `gles_canvas.cc`，0 循环依赖、0 新公共模块（B1=A YAGNI）。

### 4.6 ⭐ shader 安全范式续延

`kPathVert` 注册 `kAllShaderSources[]`，shader_injection S1 零改动自动覆盖（B9=A）。

### 4.7 ⭐ Mesa swrast glDrawElements 首次实证

dynamic VBO/EBO + `GL_UNSIGNED_INT` indices 在 headless offscreen 路径 10/10 PASS，扩展 G1.5 triple-evidence。

### 4.8 ⭐ REFACTOR 涌现 cmake 子模块

`cmake/LibTess2.cmake` 与 plan §8 systemPatterns 预测一致，可复用于 G1.7 stroke tess 或离线 vendored 切换。

---

## 5. 遇到的挑战（5 项）

### 5.1 FetchContent 网络 / DNS

沙箱内首次 populate 失败（`Could not resolve host: github.com`）；`required_permissions: all` 后成功。与 techContext §FetchContent 代理守卫一致。

### 5.2 `enable_language(C)` 缺失

根 `project(... LANGUAGES CXX)` 导致 `CMAKE_C_COMPILE_OBJECT` 未定义；LibTess2.cmake 补 `enable_language(C)` 后 configure 通过。**plan §0.3 未列此项。**

### 5.3 libtess2 API 类型名

plan 示例写 `Tess*`，实际头文件为 `TESStesselator*`；编译期一次修正。

### 5.4 测试编译依赖

`gles_canvas_path_test.cc` 缺 `sdl2_egl_display.h`（`MakeCurrent()` 不完整类型）；RED 阶段编译失败，非运行时 FAIL。

### 5.5 QuadBezier 像素采样点几何错误

T4 初版采样 veloxa y=2 在二次曲线**上方**（曲线中心 y≈4），RED 后修正为 y=10。属测试设计误差，非实现 bug。

---

## 6. 经验教训（6 项）

1. **FetchContent C 库 checklist：** 凡 manual `add_library` 含 `.c` 源，plan/build 必须写 `enable_language(C)` 或根 project 加 `C`。
2. **Bezier 像素测坐标：** plan 测试矩阵应给出曲线内参考点（t=0.5 解析坐标），避免 RED 阶段 false-negative 混淆。
3. **GLES 测试 include 清单：** 凡调用 `gles_display()->MakeCurrent()` 的测试必须 include `sdl2_egl_display.h`（与 fill_test 对齐）。
4. **libtess2 类型名：** 以 `Include/tesselator.h` 为准（`TESStesselator`），plan 代码片段可用 typedef 注释。
5. **quint-evidence 续延：** G1.6 为第 6 个 GLES 实施子任务，plan Phase 0 + B 决策锁定 → build 仍接近机械转化（1 次 GREEN 修正：T4 采样点 + 3 处 compile fix）。
6. **提交粒度：** 单会话完成 build 未按 plan commit 时间线拆分 — 应在 `/reflect` 前或 Archive 前补 commit 链。

---

## 7. 反复模式识别

| 已知模式 | 本次是否重复？ | 说明 |
|---|:-:|---|
| 计划文件清单与实际不一致 | ⚠️ 轻微 | 根 CMakeLists 计划有、实际无（正向）|
| 子代理产出需大量返工 | ❌ | N/A |
| 前置依赖/环境未验证 | ⚠️ **是** | `enable_language(C)` plan 漏项 → build 阻塞一次 |
| 非默认路径遗漏验证 | ❌ | 空 path / 透明 brush 已测 |
| 测试隔离/flaky | ❌ | 0 flaky |
| 提交粒度偏离 | ⚠️ **是** | 0 commit（反复模式 #6）|
| TDD 严格度不匹配 | ❌ | 标准 RED→GREEN |

---

## 8. 改进建议

| # | 建议 | 优先级 | 落实方式 | 目标 |
|---|------|--------|---------|------|
| 1 | FetchContent C 依赖必须 checklist `enable_language(C)` | **P1** | `writing-plans.mdc` FetchContent 审计段 + `LibTess2.cmake` 注释 | 下次 C 第三方 wrapper |
| 2 | GLES 像素测：Bezier/弧线路径给出解析采样坐标 | **P1** | plan 测试矩阵模板 / fill_test 范式 | 减少 RED 误判 |
| 3 | libtess2 `_deps` 离线预置文档化 | **P2** | `techContext.md` §FetchContent | CI / 无网环境 |
| 4 | T10 winding rule 探针（TESS_WINDING_ODD）| **P2** | G1.7 或 debug 工具任务 | creative §3.3 完整覆盖 |
| 5 | Build 阶段按 plan commit 时间线拆分 | **P1** | `/build` 收尾或 Archive 前补 3-4 commit | 反复模式 #6 抑制 |

---

## 9. 技术改进建议

- **性能：** per-FillPath `tessNewTess`/`tessDeleteTess` + STREAM_DRAW 符合蓝图 YAGNI；G2 可考虑 tess 对象池。
- **覆盖：** ArcTo flatten 有实现但无专属单测 — G1.7 stroke 路径可补。
- **债务：** `dynamic_cast<SoftwarePath>` 与 SoftwareCanvas 同范式；G1.12 GLESPath 后可能消除。

---

## 10. 安全评估

| 维度 | 状态 | 备注 |
|------|------|------|
| 输入验证 | N/A | Path 数据来自内部 SoftwarePath；无用户 GLSL 拼接 |
| 认证/授权 | N/A | |
| 数据保护 | N/A | |
| 依赖审计 | ✅ | libtess2 v1.0.2 MPL2 / SGI Free B；仅 gles build 链接 |
| 错误信息脱敏 | N/A | |
| 敏感数据处理 | N/A | |

**结论：** 本任务不涉及安全变更；shader 注入防御通过 `kAllShaderSources[]` 续延。

---

## 11. 下一步

- `/archive` 归档 TASK-20260529-01
- 解锁 **G1.7 Stroke***（stroke = fill 转换 / 复用 path tess 基础设施）
- 补 build 阶段 git commit 链（用户确认后）

**Source:** `docs/plans/2026-05-05-gles-renderer-blueprint.md` §3.6 + `docs/plans/2026-05-29-gles-canvas-fillpath.md`
