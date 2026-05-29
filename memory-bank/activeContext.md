# 活跃上下文

## 当前阶段

**构建中·轮次 1 完成** — TASK-20260529-03 G1.8 / GlyphAtlas ✅（10/10）；下次进入轮次 2 DrawText（glyph shader + DrawText 像素测）。

**上一任务：** [TASK-20260529-02 G1.7 Stroke*](archive/archive-TASK-20260529-02.md) — ✅ 归档（2026-05-29）。

---

## 当前焦点：TASK-20260529-03 — G1.8 GlyphAtlas + DrawText

**复杂度：** Level 4（GPU 资源管理 + LRU + 多字号 + emoji edge case）
**建议分支：** `feature/TASK-20260529-03-gles-glyph-atlas-drawtext`（基线 main）
**ctest 基线：** gles 1399 / software 1303 / no-devtool 1141
**creative：** `creative-gles-resources.md` §3 GlyphAtlas 设计已就位（可复用）
**前置：** G1.5 ✅；复用 `veloxa/text/` 基础设施

**spec：** `docs/specs/2026-05-29-gles-glyph-atlas-drawtext-design.md`（5 项 R1-R5 reconcile）
**plan：** `docs/plans/2026-05-29-gles-glyph-atlas-drawtext.md`（2 轮次 Build / ~22 测 / ctest +18-22）
**下一步：** `/build` — 轮次 1 GlyphAtlas RED → GREEN → 轮次 2 DrawText

---

## 待处理事项

**来自 TASK-20260529-02（G1.7）回顾 — 下个 GLES 像素测任务前落实：**
- **P1 #1**（反复模式·已升级）像素测采样坐标必须解析推导：扩展至矩形/线/环描边边带（`[edge-hw,edge+hw]` 居中 / `[edge,edge+w]` 内描边）+ 像素中心 +0.5 偏移 → `writing-plans.mdc` 测试矩阵 checklist。T1 `(6,6)` 落空心内角复现 G1.6 T4 同类误判。
- **P1 #2** GLES 白底正向像素测双通道硬规则 `R>200 && green<50`（杜绝白底假绿）→ `writing-plans.mdc` / GLES 测试范式段。
- **P2** RoundedRect 精确圆角环（SDF discard `kRoundedRectStrokeFrag`）/ segment tess 对象池 / 居中描边语义对齐 → G2 优化任务（见 systemPatterns）。

**承接历史（G1.6 P1）：** #3 FetchContent `enable_language(C)` checklist、#4 Bezier 解析采样坐标（本次 P1#1 已涵盖并升级）。

---

## 最近归档

- [`archive-TASK-20260529-02.md`](archive/archive-TASK-20260529-02.md) — G1.7 Stroke*（2026-05-29）
- [`archive-TASK-20260529-01.md`](archive/archive-TASK-20260529-01.md) — G1.6 FillPath（2026-05-29）
