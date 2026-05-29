# 归档：G1.7 `GLESCanvas::Stroke*`（Stroke = Fill 转换）

**日期：** 2026-05-29
**任务 ID：** TASK-20260529-02
**复杂度级别：** Level 3（实施类 / GLES 蓝图实施第七步 / MVP-C 战略主线第七个实施任务）
**状态：** ✅ 已完成
**分支：** `feature/TASK-20260529-02-gles-canvas-stroke`

---

## 任务概述

将 `GLESCanvas` 中 4 个 `Stroke*` 空 stub 替换为 **Stroke = Fill 转换** 真实 GPU 实现，复用 G1.5（FillRect/FillRoundedRect）与 G1.6（FillPath/libtess2）基础设施，**0 新 fragment shader / 0 新第三方依赖**。

**前置链：** G1.5 ✅ → G1.6 ✅ → **G1.7（本任务）** → 解锁 G1.8 DrawText。

---

## 技术方案

| Stroke 方法 | Fill 转换策略 | 关键技术 | Source |
|---|---|---|---|
| `StrokeRect` | 4× 居中 `FillRect` 边条（top/bottom 含角 + left/right 填缝）| 几何分解 | creative §4.1 |
| `StrokeLine` | 旋转居中 `FillRect` | `transform_.Multiply(Translation(mid).Multiply(Rotation(angle)))` | creative §4.3 / plan §0.5 reconcile |
| `StrokeRoundedRect` | stencil 内描边环（2× `FillRoundedRect`）+ 矩形条 fallback | `GL_STENCIL_TEST` REPLACE/EQUAL | creative §4.2 / plan §0.3 |
| `StrokePath` | segment-quad → `FillPath`（每段 4 点 quad）| 复用 G1.6 flatten + libtess2 | rasterizer.cc:399-484 / plan §0.4 reconcile |

**两处 plan-fact reconcile（plan 阶段消化，build 0 阻塞）：**
- creative §4.4 `SoftwarePath::OffsetPath` **不存在** → 采 rasterizer segment-quad。
- creative §4.3 `Matrix3x2` 链式 `.Translate().Rotate()` **不存在** → 用 static `Translation/Rotation` + `Multiply`。

---

## 实现摘要

### 文件变更

| 操作 | 文件路径 | 说明 |
|------|---------|------|
| 修改 | `veloxa/graphics/gles/gles_canvas.h` | 4× `Stroke*` stub → override 声明 + `StrokeSegmentQuad` private helper（+~20 行）|
| 修改 | `veloxa/graphics/gles/gles_canvas.cc` | 4 方法实现 + `StrokeSegmentQuad` + stencil 环 + 矩形条 fallback（+~210 行）|
| 创建 | `tests/graphics/gles/gles_canvas_stroke_test.cc` | 14 单测（含 3 inline reverse probe，~340 行）|
| 修改 | `tests/CMakeLists.txt` | 注册 `gles_canvas_stroke_test`（gles guard，+~8 行）|

### 关键决策

1. **B1=A StrokeRect 4×FillRect** — 居中描边（对齐 SoftwareCanvas StrokePath-of-rect 语义），top/bottom 含角、left/right 填缝。
2. **B2=A StrokeLine 旋转 FillRect** — 居中 rect `{-len/2,-w/2,len,w}` 经 `Multiply(T×R)` 复合 + 前置 `transform_`，`PushState/PopState` 包裹。
3. **B3=A StrokeRoundedRect stencil 内描边环** — 外=原 rect 圆角 / 内=inset width 后圆角；`GL_STENCIL_BITS` 探针 + 错误吞噬 + 矩形条 fallback 防无 stencil 驱动 flaky。
4. **B4=A StrokePath segment-quad** — 局部空间收集 segment（不预 apply transform），每段 quad 交由 `FillPath` uniform 应用 `transform_`；curve 复用 G1.6 `FlattenQuadToContour/FlattenCubicToContour`。
5. **B6=A 0 新 shader** — 全部复用 G1.5/G1.6 program，`shader_injection_test` 0 改动。

### 安全决策

本任务不涉及安全变更：Stroke 几何来自内部 `Rect/Point/SoftwarePath`，无用户 GLSL 拼接；0 新 shader → shader 注入防御面零扩张；0 新第三方依赖（复用 G1.6 libtess2）。

---

## 测试覆盖

- **14 单测**（含 3 inline reverse probe）：StrokeRect/Line/RoundedRect/Path 像素验证 + AfterSetTransform 变换 + KSolid 蓝色 + star 自相交（nonzero）+ 反向探针（零宽/透明 brush/零长线）+ 多次绘制 glGetError 干净。
- **像素双约束**：正向 `R>200 && green<50`（RED 阶段实证 4 处纯红断言在白底假绿 → 补强）。
- **TDD**：Phase A RED 9/14 像素测对 stub 失败 → Phase B GREEN 14/14 PASS。
- **三 build 矩阵**：gles 1385→**1399**（+14）/ software 1303 / no-devtool 1141 无退化；完整 build-gles **1399/1399 PASS**（~66s，1 无关 WPT skip）。

---

## 经验教训

1. **描边采样点 = 解析边带推导，非直觉取整**（反复模式 dual-evidence：G1.6 T4 + G1.7 T1）— 居中带 `[edge-hw,edge+hw]` / 内描边带 `[edge,edge+w]` + 像素中心 +0.5。**已升级 P1 固化**。
2. **白底正向测必须双通道约束** `R>200 && green<50` — `R>200` 无法区分红与白。
3. **stencil + alpha-blend frag 不兼容精确形状遮罩** — frag 不 `discard` 时 REPLACE 标记整个包围盒 → 形状退化为矩形。精确圆角需 SDF discard 版 frag。
4. **commit 链拆分有效且低成本**（G1.6 P1#5 闭环）— RED/GREEN/finalize 三提交显著提升可追溯性。
5. **plan-fact reconcile 越早越省** — API 不存在在 plan 阶段 reconcile 远优于 build 编译失败回溯。

---

## 技术债（G2 评估）

- (a) RoundedRect 内描边环 + 矩形内孔（角不圆）；(b) stencil 每次 `glClear` 整缓冲（多 stroke 浪费）；(c) 每段 FillPath 独立 tessellate。三者均 MVP YAGNI 取舍。
- 精确圆角环修复路径：creative §4.2 预留 `kRoundedRectStrokeFrag`（SDF discard）。

---

## 参考文档

- 上游蓝图：[`docs/plans/2026-05-05-gles-renderer-blueprint.md`](../../docs/plans/2026-05-05-gles-renderer-blueprint.md) §3.7
- 设计规格：[`docs/specs/2026-05-05-gles-renderer-blueprint-design.md`](../../docs/specs/2026-05-05-gles-renderer-blueprint-design.md) §3.3.1
- 实现计划：[`docs/plans/2026-05-29-gles-canvas-stroke.md`](../../docs/plans/2026-05-29-gles-canvas-stroke.md)
- 创意设计：[`memory-bank/creative/creative-gles-canvas.md`](../creative/creative-gles-canvas.md) §4
- 回顾文档：[`memory-bank/reflection/reflection-TASK-20260529-02.md`](../reflection/reflection-TASK-20260529-02.md)
- 前置归档：[`archive-TASK-20260529-01.md`](archive-TASK-20260529-01.md)（G1.6 FillPath）

---

## 提交记录

| Phase | commit | subject |
|---|---|---|
| Plan | `595af02` | chore(plan): land G1.7 Stroke* plan + memory bank |
| A RED | `0c4fbba` | test(gles): G1.7 Stroke* RED — 14 pixel/probe tests |
| B GREEN | `63a5401` | feat(graphics): implement GLESCanvas Stroke* via Fill conversion |
| C-D finalize | `d5fe5ba` | chore(build): finalize TASK-20260529-02 ctest matrix |
| Reflect | `1707b8f` | docs(reflect): add reflection for TASK-20260529-02 |
