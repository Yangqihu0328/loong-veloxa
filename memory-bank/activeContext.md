# 活跃上下文

## 当前阶段

**回顾中** — TASK-20260529-02 G1.7 Stroke* Reflect ✅（[reflection 文档](reflection/reflection-TASK-20260529-02.md)）/ 待 `/archive`。

**上一任务：** [TASK-20260529-01 G1.6 FillPath](archive/archive-TASK-20260529-01.md) — ✅ 归档。

---

## 当前焦点：TASK-20260529-02 — G1.7 Stroke*

**分支：** `feature/TASK-20260529-02-gles-canvas-stroke`（基于 G1.6 分支 ✅）

**plan 文档：** [`docs/plans/2026-05-29-gles-canvas-stroke.md`](../docs/plans/2026-05-29-gles-canvas-stroke.md)

**B1–B8 已锁定：** 4× Fill 转换 / segment-quad StrokePath / stencil RoundedRect / 0 新 shader

**ctest 期望：** Matrix C 1385 → **1397–1401**（+12–16）

**下一步：** `/build` — Phase A RED

---

## 下一推荐任务（G1.7 完成后）

| 优先 | 候选 | Level |
|:-:|---|:-:|
| 1 | G1.8 DrawText 部分 | L3 |
| 2 | R9 HitTest | L2-3 |

---

## 待处理事项

**来自 TASK-20260529-02（G1.7）回顾 — 下个 GLES 像素测任务前落实：**
- **P1 #1**（反复模式·已升级）像素测采样坐标必须解析推导：扩展至矩形/线/环描边边带（`[edge-hw,edge+hw]` 居中 / `[edge,edge+w]` 内描边）+ 像素中心 +0.5 偏移 → `writing-plans.mdc` 测试矩阵 checklist。T1 `(6,6)` 落空心内角复现 G1.6 T4 同类误判。
- **P1 #2** GLES 白底正向像素测双通道硬规则 `R>200 && green<50`（杜绝白底假绿）→ `writing-plans.mdc` / GLES 测试范式段。
- **P2** RoundedRect 精确圆角环（SDF discard `kRoundedRectStrokeFrag`）/ segment tess 对象池 / 居中描边语义对齐 → G2 优化任务（见 systemPatterns）。

**承接历史（G1.6 P1）：** #3 FetchContent `enable_language(C)` checklist、#4 Bezier 解析采样坐标（本次 P1#1 已涵盖并升级）。

---

## 最近归档

- [`archive-TASK-20260529-01.md`](archive/archive-TASK-20260529-01.md) — G1.6（2026-05-29）
