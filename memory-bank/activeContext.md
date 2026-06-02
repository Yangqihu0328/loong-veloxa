# 活跃上下文

## 当前阶段

**规划中** — TASK-20260602-01 GLES 图像采样过滤选项 NEAREST/LINEAR（G1.9 技术债 #2 清理）/ Level 2 / VAN ✅ + Plan ✅（D1-D5 锁定 / 4 文件改 / pool+CMake 零改动 / 单轮 TDD）→ 待 `/build`。

**上一任务：** [TASK-20260529-04 G1.9 GLESCanvas::DrawImage](archive/archive-TASK-20260529-04.md) — ✅ 归档（2026-05-29 / 合并 main fast-forward + 分支删除）。

---

## 当前焦点：TASK-20260602-01 — GLES 图像采样过滤选项 NEAREST/LINEAR

**复杂度：** Level 2（含接口设计决策 D1 过滤旋钮归属）
**分支：** `feature/TASK-20260602-01-gles-image-sampling-filter`（基线 main）
**ctest 基线：** gles 1432 / software 1303 / no-devtool 1141
**前置：** G1.9 ✅；`ImageTexturePool` 硬编码 LINEAR / `Canvas::DrawImage` 无 filter 参数 / software 当前 NEAREST 整数截断
**决策（已锁定）：** D1=① GLES 局部 setter / D2 不入缓存键每 draw 设 / D3 仅 GLES / D4 默认 LINEAR / D5 不入 PushState
**下一步：** `/build` — 单轮 A RED → B GREEN → C 三矩阵 finalize

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
