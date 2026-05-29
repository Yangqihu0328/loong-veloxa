# 活跃上下文

## 当前阶段

**🟢 空闲** — 无进行中的任务。使用 `/van` 开始新任务。

**上一任务：** [TASK-20260529-03 G1.8 GlyphAtlas + DrawText](archive/archive-TASK-20260529-03.md) — ✅ 归档（2026-05-29 / 已合并 main fast-forward + 分支删除）。

---

## 当前焦点：无（空闲）

**ctest 基线（main，最新）：** gles **1416** / software **1303** / no-devtool **1141**
**GLES 蓝图进度：** G1.1-G1.8 ✅（Display/Context/Canvas 骨架/FillRect/FillPath/Stroke*/DrawText）
**候选下一任务：** G1.9 DrawImage / R9 HitTest / G2 文本优化（批量化 + LRU + emoji + FT helper 抽取）。
**下一步：** `/van` 开始新任务。

---

## 待处理事项

**来自 TASK-20260529-03（G1.8）回顾 — 下个 GLES/容器任务前落实：**
- **P1 #A** GLES 资源对象方法的 GL 全局状态副作用契约：会 mutate GL 状态的 helper（如 `GlyphAtlas::GetOrUpload` 解绑 `GL_TEXTURE_2D` + 改 `GL_UNPACK_ALIGNMENT`）调用后、draw 前**必须重建依赖状态** → GLES plan checklist「看似冗余的 GL 状态调用逐条注释不可省原因」（已入 systemPatterns first-evidence）。
- **P1 #B**（反复模式 #3 变体）容器 API 审计须读真实 header 方法名：本仓 `HashMap` 用 `Find/Insert`（PascalCase），非 STL `find/end/operator[]`。plan §0.4 误审导致编译期修正 → Phase 0 audit 强化。
- **P2** text 像素测回补（plan T3/T5/T10/T11/T12 裁掉：cache 复用/色变/空格 advance/基线精度/多字 advance）+ FT 栅格化抽 helper + 逐字形 draw 批量化 + atlas LRU/emoji（见 techContext 技术债）。

**来自 TASK-20260529-02（G1.7）回顾 — 下个 GLES 像素测任务前落实：**
- **P1 #1**（反复模式·已升级）像素测采样坐标必须解析推导：扩展至矩形/线/环描边边带（`[edge-hw,edge+hw]` 居中 / `[edge,edge+w]` 内描边）+ 像素中心 +0.5 偏移 → `writing-plans.mdc` 测试矩阵 checklist。T1 `(6,6)` 落空心内角复现 G1.6 T4 同类误判。
- **P1 #2** GLES 白底正向像素测双通道硬规则 `R>200 && green<50`（杜绝白底假绿）→ `writing-plans.mdc` / GLES 测试范式段。
- **P2** RoundedRect 精确圆角环（SDF discard `kRoundedRectStrokeFrag`）/ segment tess 对象池 / 居中描边语义对齐 → G2 优化任务（见 systemPatterns）。

**承接历史（G1.6 P1）：** #3 FetchContent `enable_language(C)` checklist、#4 Bezier 解析采样坐标（本次 P1#1 已涵盖并升级）。

---

## 最近归档

- [`archive-TASK-20260529-02.md`](archive/archive-TASK-20260529-02.md) — G1.7 Stroke*（2026-05-29）
- [`archive-TASK-20260529-01.md`](archive/archive-TASK-20260529-01.md) — G1.6 FillPath（2026-05-29）
