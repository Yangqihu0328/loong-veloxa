# 活跃上下文

## 当前阶段

**归档中** — TASK-20260529-04 G1.9 归档闭环（`archive-TASK-20260529-04.md`）→ 重置空闲。

**上一任务：** [TASK-20260529-03 G1.8 GlyphAtlas + DrawText](archive/archive-TASK-20260529-03.md) — ✅ 归档（2026-05-29 / 已合并 main fast-forward + 分支删除）。

---

## 当前焦点：TASK-20260529-04 — G1.9 GLESCanvas::DrawImage

**复杂度：** Level 4（GPU 图像纹理资源管理 + ImageHandle 缓存 + 多图 + Context Lost/Restored + 采样过滤）
**分支：** `feature/TASK-20260529-04-gles-canvas-drawimage`（基线 main）
**ctest 基线：** gles 1416 / software 1303 / no-devtool 1141
**前置：** G1.8 ✅；复用纹理/quad/shader 范式；`Image` RGBA8 + software DrawImage（`software_canvas.cc:282`）镜像源就位
**关联待处理：** G1.8 P1#A（纹理上传 GL 状态副作用契约）+ P1#B（HashMap Find/Insert）直接适用本任务
**下一步：** `/plan` — 设计 RGBA8 纹理上传 + image shader + ImageHandle 缓存 + src/dst rect 采样

---

## 待处理事项

**来自 TASK-20260529-03（G1.8）回顾 — ✅ 已在 TASK-20260529-04（G1.9）主动预防成功：**
- **P1 #A** ✅（已验证）GLES 资源对象 GL 状态副作用契约：G1.9 `DrawImage` 在 `GetOrUpload` 后、draw 前重绑纹理，**G1.8 全屏白 bug 未复发，零 debug 迭代**。已升级为 systemPatterns first-evidence → second-evidence（见 systemPatterns「GL 全局状态副作用契约」段）。
- **P1 #B** ✅（已验证）容器 API 审计读真实 header：G1.9 1B 前读 `hash_map.h` 确认 `Find/Insert/Erase` + `begin/end` + `it->key/value`，无编译期返工。
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
