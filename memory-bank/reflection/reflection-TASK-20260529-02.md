# 回顾：G1.7 `GLESCanvas::Stroke*`（Stroke = Fill 转换）

**日期：** 2026-05-29
**任务 ID：** TASK-20260529-02
**复杂度级别：** Level 3（实施类 / GLES 蓝图实施第七步 / MVP-C 战略主线第七个实施任务）
**分支：** `feature/TASK-20260529-02-gles-canvas-stroke`
**前置任务：** TASK-20260529-01 G1.6 FillPath ✅ 已归档

---

## 1. 任务定位

GLES 蓝图实施第七步 — 在 G1.5（solid/rounded shader）+ G1.6（libtess2 path fill）之上，将 4 个 `Stroke*` 空 stub 替换为 **Stroke = Fill 转换** 真实实现，0 新 fragment shader。

**主交付：**
- `StrokeRect` → 4× 居中 `FillRect` 边条（creative §4.1）
- `StrokeLine` → `Matrix3x2::Multiply(T×R)` 旋转居中 `FillRect`（plan-fact：无链式变换）
- `StrokeRoundedRect` → stencil 内描边环（2× `FillRoundedRect`）+ 无 stencil 时矩形条 fallback（creative §4.2 + plan §0.3 实证）
- `StrokePath` → segment-quad → `FillPath`（rasterizer.cc:399-484 算法 / 复用 G1.6 flatten helper）
- 14 ctest 单测（含 3 inline reverse probe）

---

## 2. 计划 vs 实际

### 2.1 总体对照

| 维度 | 计划 | 实际 | 偏差原因 |
|:-:|---|---|---|
| Phase 数 | 4（A-D）| 4（A-D）| 0 / 精准匹配 ✅ |
| commit 数 | 4（plan + RED + GREEN + C/D）| **4**（含 plan 1 + build 3）| **精准命中**（G1.6 P1#5 落实）✅ |
| 估时（plan ×0.6）| ~105-165 min | Build phase ~14 min（21:43→21:57）| 远快于估时 / hex-evidence 机械转化区 |
| 文件变更 | 4 文件 | **4 文件** | 0 偏差（gles_canvas.{h,cc} + test + CMakeLists）✅ |
| 设计变更 | — | 2 项小偏差 | 见 §2.3 |
| ctest 增量 | Matrix C +12–16（1385→1397-1401）| Matrix C **+14**（1385→1399）| 精准命中区间 ✅ |
| 测试数 | 14 单测 + 3 probe | 14 单测（含 3 probe）| 表述统一，数量一致 ✅ |

### 2.2 文件变更详细对照

| # | 文件 | plan 估行 | 实际 | 备注 |
|:-:|---|:-:|:-:|---|
| 1 | `gles_canvas.h` | +~15 | +~20 | 4 override 声明 + `StrokeSegmentQuad` helper |
| 2 | `gles_canvas.cc` | +~200 | +~210 | 4 方法 + segment-quad helper + stencil 环 + fallback |
| 3 | `gles_canvas_stroke_test.cc` | ~380 | ~340 | 14 测；行数略低（fixture 复用紧凑）|
| 4 | `tests/CMakeLists.txt` | +~8 | +~8 | gles guard 注册 ✅ |

### 2.3 设计偏差（2 项，均 MVP 取舍）

1. **StrokeRoundedRect 内描边环（非居中）+ 矩形内孔。** SoftwareCanvas 走居中 StrokePath；本次 stencil 环采「外 = 原 rect 圆角 / 内 = inset width 后圆角」，得**内描边**且内孔矩形（角不圆）。理由：(a) 采样点鲁棒；(b) `kRoundedRectFrag` 用 alpha 混合不 discard → stencil REPLACE 标记整个包围盒矩形，内孔天然为矩形。**记技术债**。
2. **每段 `FillPath` 单独 tessellate。** StrokePath N 段 = N 次 `tessNewTess`/`tessDeleteTess`。与 rasterizer.cc 同范式、符合 MVP YAGNI，但性能非最优。**记技术债**（与 G1.6 同源）。

---

## 3. 做得好的（6 项）

1. **⭐ commit 链拆分落实（G1.6 P1#5 闭环）。** RED `test(gles)` → GREEN `feat(graphics)` → finalize `chore(build)` 三提交严格对齐 plan §4 时间线 + plan 提交，0 大杂烩。**反复模式 #6 本次抑制成功**，反向印证 G1.6 建议有效。
2. **⭐ RED 假绿自查强化。** 初版 4 个正向测仅断言 `R>200`，但白底红通道也是 255 → 会假绿。RED 运行时识别（仅 5/9 失败异常），补 `green<50`（KSolid 测补 `R<50`）双约束后 9/9 真失败。体现「反向探针弱」模式的主动防御。
3. **plan-fact reconcile 前置消化。** creative §4.4 `OffsetPath`、§4.3 链式变换两处不存在的 API 已在 **plan 阶段**（§0.4/§0.5）reconcile 为 rasterizer segment-quad + `Matrix3x2::Multiply` → build 阶段 0 API 阻塞。
4. **0 新 shader（B6=A）。** 全部复用 G1.5/G1.6 program，`shader_injection_test` 0 改动，安全回归面零扩张。
5. **stencil fallback 韧性设计。** `glGetIntegerv(GL_STENCIL_BITS)` + 错误吞噬 + 矩形条 fallback，使无 stencil 驱动（Mesa swrast 严格态）下仍输出正确环、测试不 flaky。
6. **三 build 矩阵全绿。** gles 1399/1399（完整 66s 跑）、software 1303、no-devtool 1141 均无退化，双 config 盲区（反复模式 #7）持续抑制。

---

## 4. 遇到的挑战（3 项）

### 4.1 ⚠️ T1 StrokeRect 采样点几何误判（反复模式）

初版采样 veloxa `(6,6)`，但 width=4 **居中**描边的左/上边带为 `[2,6]`，`(6.5,6.5)` 落在空心内角（hollow）→ 白色，正向测对 GREEN 实现**误失败**。GREEN 阶段修正为左边带中点 `(4,16)`。

**这是 G1.6 P1#2「Bezier 像素采样解析坐标」的同类问题在矩形描边上的复现** —— 描边「居中带」的内外边界同样需在 plan 阶段解析推导采样点，不能凭直觉取整。

### 4.2 StrokeRoundedRect 圆角语义降级

见 §2.3#1。stencil + alpha-blend frag 的交互（REPLACE 不受 alpha 门控）导致内孔矩形化，圆角语义部分丢失。属 MVP 可接受但需文档化。

### 4.3 star StrokePath 采样点需手算验证

T8 星形自相交描边采样 `(16,5)` 靠近顶尖，需手算「点到边线垂距 < half_width」确认覆盖（width=2，hw=1，垂距≈0.52 < 1 ✓）。这次**事前算对**了（受 §4.1 教训驱动），未触发 GREEN 返工 —— 但印证采样点解析推导应成为 plan 标准动作。

---

## 5. 经验教训（5 项）

1. **描边采样点 = 解析边带推导，非直觉取整。** 居中描边带 `[edge-hw, edge+hw]`、内描边带 `[edge, edge+w]` 的覆盖区必须在 plan 测试矩阵用具体坐标标注（含像素中心 +0.5 偏移）。延伸自 G1.6 P1#2，扩展到**矩形/线/环描边**，不限于曲线。
2. **白底正向测必须双通道约束。** `R>200` 不足以区分红与白，必须并行 `green<50`（或对应背景通道）。应固化为 GLES 像素测范式硬规则。
3. **stencil + alpha-blend frag 不兼容精确形状遮罩。** 若 frag 不 `discard`（如 SDF alpha），stencil REPLACE 标记整个包围盒 → 形状退化为矩形。精确圆角环需 SDF discard 版 frag 或专用 `kRoundedRectStrokeFrag`（creative §4.2 已预留）。
4. **commit 链拆分有效且低成本。** 本次按 RED/GREEN/finalize 拆分仅多 2 次 commit 操作，却显著提升可追溯性 —— 应作为 Level 3 实施任务默认动作。
5. **plan-fact reconcile 越早越省。** API 不存在（OffsetPath/链式变换）在 plan 阶段 reconcile 的成本远低于 build 阶段编译失败回溯。

---

## 6. 反复模式识别

| 已知模式 | 本次是否重复？ | 说明 |
|---|:-:|---|
| 计划文件清单与实际不一致 | ❌ | 4 文件精准匹配 |
| 子代理产出需返工 | ❌ | N/A（未用子代理）|
| 前置依赖/环境/API 未验证 | ❌ | plan §0 audit + reconcile 充分，build 0 阻塞 |
| 非默认路径遗漏验证 | ❌ | 零宽/透明/零长线 3 反向探针 |
| 测试隔离/flaky | ❌ | 0 flaky；stencil fallback 防驱动差异 |
| 提交粒度偏离 | ✅ **已抑制** | RED/GREEN/finalize 三提交（G1.6 P1#5 落实）|
| TDD 严格度不匹配 | ❌ | 标准 RED→GREEN，RED 强化为双约束 |
| **像素测采样坐标几何误判** | ⚠️ **是（复现）** | T1 `(6,6)` 落空心内角 → 与 G1.6 T4 同类，**需升级固化** |

---

## 7. 改进建议

| # | 建议 | 优先级 | 落实方式 | 目标 |
|---|------|--------|---------|------|
| 1 | **像素测采样坐标解析推导**（扩展至矩形/线/环描边边带，非仅曲线）| **P1**（反复出现，需固化）| `writing-plans.mdc` 测试矩阵段加「采样点解析坐标 + 像素中心 +0.5」checklist | 抑制采样误判反复模式 |
| 2 | **GLES 白底正向像素测双通道约束硬规则**（`R>200 && green<50`）| **P1** | `writing-plans.mdc` 或 GLES 测试范式段 | 杜绝白底假绿 |
| 3 | StrokeRoundedRect 精确圆角环（SDF discard 版 `kRoundedRectStrokeFrag`）| **P2** | G2 优化任务 / creative §4.2 已预留 | 圆角语义完整 |
| 4 | StrokePath segment tess 对象池 / 批量上传 | **P2** | G2 性能任务（与 G1.6 同源债务合并）| 多段描边性能 |
| 5 | StrokeRoundedRect 居中描边语义对齐 SoftwareCanvas | **P2** | 视觉一致性任务 | 跨后端像素一致 |

---

## 8. 技术改进建议

- **技术债（新增）：** (a) RoundedRect 内描边环 + 矩形内孔（角不圆）；(b) stencil 每次 `glClear` 整缓冲（多 stroke 浪费）；(c) 每段 FillPath 独立 tessellate。三者均为 MVP YAGNI 取舍，G2 评估。
- **覆盖：** ArcTo 在 StrokePath 已实现路径但无专属单测（与 G1.6 同遗留）；G1.8+ 或 debug 工具任务可补 arc 描边测。
- **复用：** `StrokeSegmentQuad` + flatten helper 模式可供 G1.10 PushClipPath（stencil clip）借鉴。

---

## 9. 安全评估

| 维度 | 状态 | 备注 |
|------|------|------|
| 输入验证 | N/A | Stroke 几何来自内部 Rect/Point/SoftwarePath；无用户 GLSL 拼接 |
| 认证/授权 | N/A | |
| 数据保护 | N/A | |
| 依赖审计 | ✅ | 0 新第三方依赖（复用 G1.6 libtess2）|
| 错误信息脱敏 | N/A | |
| 敏感数据处理 | N/A | |

**结论：** 本任务不涉及安全变更；0 新 shader → shader 注入防御面零扩张。

---

## 10. 下一步

- `/archive` 归档 TASK-20260529-02
- 解锁 **G1.8 DrawText 部分**（GlyphAtlas + 文本渲染）或 R9 HitTest
- P1 #1/#2 迁移至 `activeContext.md` 待处理事项，下个 GLES 像素测任务前落实

**Source:** `docs/plans/2026-05-05-gles-renderer-blueprint.md` §3.7 + `docs/plans/2026-05-29-gles-canvas-stroke.md` + `memory-bank/creative/creative-gles-canvas.md` §4
