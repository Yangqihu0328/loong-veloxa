# 活跃上下文

## 当前阶段

**空闲** — 上个任务 TASK-20260606-01 GLES `PushClipRect/PushClipPath/PopClip`（G1.10 Clip / glScissor）已归档闭环（归档文档 [`archive-TASK-20260606-01.md`](archive/archive-TASK-20260606-01.md)）。等待 `/van` 启动新任务（GLES 蓝图候选：G1.11 脏矩形 glScissor 优化 / PushLayer/PopLayer FBO 层 / R9 HitTest）。

**上一任务：** [TASK-20260602-01 GLES 图像采样过滤选项 NEAREST/LINEAR](archive/archive-TASK-20260602-01.md) — ✅ 归档（2026-06-02 / G1.9 技术债 #2 清理 / Level 2 / 三矩阵零退化 gles 1432→1437 / 单轮 TDD 计划精度满分 + 零 debug）。

---

## 当前焦点：TASK-20260606-01 — GLES G1.10 Clip（PushClipRect/PushClipPath/PopClip via glScissor）

**复杂度：** Level 3（坐标空间 + clip 栈交集 + 路径近似 + scissor 生命周期设计决策）
**分支：** `feature/TASK-20260606-01-gles-canvas-clip`（基线 main）
**ctest 基线：** gles 1437 / software 1303 / no-devtool 1141
**前置：** G1.5 FillRect ✅（clip 测试用 FillRect 验证 r 内有 fill / r 外无 fill）；gles_canvas.h 3 clip stub + State.clip_stack_depth 预留
**镜像参考：** software `clip_stack_`（`Vector<Rect>` 交集栈）+ `CurrentClip()`（`software_canvas.cc:322-407`）+ PushState/PopState clip_stack_depth 联动（L385-394）
**关键设计点（待 plan brainstorm）：** ① glScissor 窗口坐标 bottom-left → Y 翻转；② clip rect 经 transform_ 变换后取 AABB（旋转下 axis-aligned 近似）；③ 嵌套 clip = 交集（current.Intersect(rect)）；④ PushClipPath → path.Bounds() 近似（镜像 software）；⑤ glEnable/glDisable(GL_SCISSOR_TEST) 生命周期 + Begin/End 复位；⑥ PushState/PopState 还原 clip 栈深度
**下一步：** `/build` — 单轮 A RED → B GREEN → C 三矩阵 finalize（沿用 G1.7/G1.9 commit 链拆分范式）

---

## 待处理事项

**来自 TASK-20260606-01（G1.10 Clip）回顾：**
- **P1 #B**（⚠️ 反复轻度重复 / 与 G1.9「容器 API 名称」同源）plan 阶段 CMake / 容器 / 几何类型 API 片段必须 **Grep 既有最近邻块逐字镜像**，不凭记忆写宏或字段名。本次 plan A.2 写了不存在的 `vx_add_test` 宏、初版代码误用 `Rect.width/height`（实为 `w/h`），均靠 build 前 Grep/自查兜住。下次 `/plan` 写基础设施伪代码时自律执行。

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

- [`archive-TASK-20260602-01.md`](archive/archive-TASK-20260602-01.md) — GLES 图像采样过滤 NEAREST/LINEAR（2026-06-02）
- [`archive-TASK-20260529-04.md`](archive/archive-TASK-20260529-04.md) — G1.9 DrawImage（2026-05-29）
- [`archive-TASK-20260529-02.md`](archive/archive-TASK-20260529-02.md) — G1.7 Stroke*（2026-05-29）
- [`archive-TASK-20260529-01.md`](archive/archive-TASK-20260529-01.md) — G1.6 FillPath（2026-05-29）
