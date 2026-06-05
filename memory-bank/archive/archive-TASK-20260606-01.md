# 归档：GLES `GLESCanvas::PushClipRect/PushClipPath/PopClip`（G1.10 Clip / glScissor）

**日期：** 2026-06-06
**任务 ID：** TASK-20260606-01
**复杂度级别：** Level 3
**状态：** ✅ 已完成
**分支：** `feature/TASK-20260606-01-gles-canvas-clip`（基线 main）

## 任务概述

GLES 蓝图实施第十步（G1.10 Clip 部分 / 见 [`docs/plans/2026-05-05-gles-renderer-blueprint.md`](../../docs/plans/2026-05-05-gles-renderer-blueprint.md) §3.10）。将 `GLESCanvas` 三个 clip 方法（`PushClipRect`/`PushClipPath`/`PopClip`，原 gles_canvas.h:99-101 内联 no-op stub）实现为基于 `glScissor` 的矩形裁剪栈，镜像 `SoftwareCanvas` 的 `clip_stack_` + `CurrentClip()` 交集语义，保证跨后端一致。**PushLayer/PopLayer（FBO 层）明确排除，拆为后续独立任务。**

## 技术方案

**镜像 software 后端 + glScissor 直接消费设备空间窗口坐标**（D1-D6 全锁 = a）：

- **D1** 存储：`vx::Vector<Rect> clip_stack_`，每次 push 存 `CurrentClipDevice().Intersect(new)` 的设备空间交集 rect（与 SoftwareCanvas 同款）。
- **D2** 坐标空间：clip rect 直接视为设备空间，**不应用** `transform_`（与 software 一致 / glScissor 天然消费窗口坐标）。
- **D3** glScissor 应用：Push/Pop 立即同步 `glEnable/glDisable(GL_SCISSOR_TEST)` + `glScissor`，Y 翻转 `height_-(y+h)`；`Begin()` 帧复位（清栈 + disable）。
- **D4** PushClipPath：`path.Bounds()` AABB 近似（镜像 software）。
- **D5** 状态联动：`PushState` 存 `clip_stack_.size()`，`PopState` 弹栈至该深度 + reapply scissor。
- **D6** 测试：单轮 TDD ~8 像素测（C1-C8），跳过独立 creative。

选定理由：software clip_stack_ 存设备空间原始 rect 交集，与 glScissor（设备空间 bottom-left 窗口坐标）天然契合 → 镜像 = 最简实现 + 跨后端一致 + 零认知负担。

## 实现摘要

`ApplyScissor()` 统一同步 GL 状态：空栈 `glDisable`；非空 `glEnable` + Y 翻转 `glScissor(x, height_-(y+h), w, h)`；空交集（`Rect::Intersect` 返回 `{0,0,0,0}` / `IsEmpty()` 真）→ `glScissor(0,0,0,0)` 渲染零像素。延续「GL 全局状态副作用契约」：Begin 帧复位 + PopState reapply 防 scissor 跨帧/跨 state 泄露。

### 文件变更

| 操作 | 文件路径 | 说明 |
|------|---------|------|
| 修改 | `veloxa/graphics/gles/gles_canvas.h` | 3 clip stub 内联 `{}` → out-of-line 声明；新增 `vx::Vector<Rect> clip_stack_` 成员 + `CurrentClipDevice()`/`ApplyScissor()` 私有辅助声明；`State.clip_stack_depth` 注释更新 |
| 修改 | `veloxa/graphics/gles/gles_canvas.cc` | 实现 `PushClipRect`/`PushClipPath`/`PopClip`/`ApplyScissor`/`CurrentClipDevice`；`Begin()` 加清栈 + disable scissor；`PushState` 存 size；`PopState` 弹栈至深度 + reapply |
| 创建 | `tests/graphics/gles/gles_canvas_clip_test.cc` | 8 像素测（C1-C8 / 32×32 surface / IsRed 双通道 + IsWhite 判定） |
| 修改 | `tests/CMakeLists.txt` | gles guard 内注册 `gles_canvas_clip_test`（add_executable + target_link_libraries + gtest_discover_tests） |

**0 新依赖（glScissor GLES 3.0 core）/ 0 新 shader / 0 链接改动 / 0 抽象 Canvas ABI 变更。**

### 关键决策

1. **clip 存设备空间交集 rect 而非每层独立 rect**（D1）——`back()` 即当前有效裁剪，glScissor 直接消费，PopState 弹栈即恢复，无需重算交集链。
2. **clip 不应用 transform_**（D2）——镜像 software，旋转下保持屏幕轴对齐矩形裁剪；真变换裁剪需 stencil（G2 技术债）。
3. **PushClipPath = path.Bounds() AABB**（D4）——非目标做真路径裁剪；曲线/旋转 path 下为外接矩形过裁剪（G2 技术债）。
4. **Begin() 帧复位 scissor**（D3）——杜绝上一帧 clip 残留，与既有「Begin 刷新 viewport/blend」帧初始化契约一致。
5. **RED 路径：保留内联 stub 仅加测试**——RED 提交纯测试（test-only），GREEN 再改 header+cc，提交语义彻底分离（优于 plan 给出的两选项）。

### 安全决策

本任务不涉及安全变更（纯 GL 裁剪状态 / 0 用户输入 / 0 GLSL 拼接 / 0 新依赖 / 0 新 ABI 表面）。

## 测试覆盖

单轮 TDD（RED 5/8 FAIL → GREEN 8/8 PASS），8 像素测：

| # | 测试 | 验证点 |
|---|------|--------|
| C1 | PushClipRect_ClipsFill | 单 clip 限制全屏 fill 到内部（内 IsRed / 外 IsWhite） |
| C2 | NestedClip_Intersection | 嵌套 clip 取交集（交集中心 IsRed / 两侧 IsWhite） |
| C3 | PopClip_RestoresFull | PopClip 恢复全屏（两角 IsRed） |
| C4 | PushClipPath_BoundsApprox | path bounds AABB 裁剪（内 IsRed / 外 IsWhite） |
| C5 | PushState_PopState_RestoresClip | PopState 还原 clip 栈深度 |
| C6 | ClipYFlip_Position | scissor Y 翻转正确（上半 IsRed / 下半 IsWhite） |
| C7 | EmptyIntersection_NoFill（反探针） | disjoint clip 空交集 → 全无 fill |
| C8 | BeginResetsClip（反探针） | Begin 清栈 + disable scissor |

像素测承接 G1.7 P1#1（采样坐标解析推导）+ P1#2（白底双通道硬规则 `R>200 && green<50`）。

**三 build 矩阵零退化：** gles 1437→**1445**（+8 / 命中预测上界）· build-sw-devtool **1337** · no-devtool **1141**（clip_test 仅 gles guard 内编译，software 矩阵计数不变）。

## 经验教训

1. **内联 stub → 正式实现类任务，「test-only RED」是最优范式**——保留内联 stub 仅加测试触发断言 FAIL，GREEN 再改生产代码，RED/GREEN 提交语义彻底分离。已升 systemPatterns first-evidence，GLES 后续 stub-填充任务（PushLayer/PopLayer/CreatePath）默认采用。
2. **RED 信号可在 plan 阶段精确预测**——预测 5 FAIL / 3 PASS（无裁剪时全屏 fill 恰好满足部分用例）实测完全一致，得益于解析采样 + 反向探针设计。
3. **镜像成熟后端 = 零认知负担**——直接照搬 SoftwareCanvas clip 语义，跨后端一致性天然保证。
4. **⚠️ 轻度反复模式（升 P1）**——plan 阶段写 CMake/容器/几何类型 API 凭记忆：本次 plan 写了不存在的 `vx_add_test` 宏、初版误用 `Rect.width/height`（实为 `w/h`），均靠 build 前 Grep 既有最近邻块 / 自查 types.h 兜住。与 G1.9「容器 API 名称」同源 → 已登记 activeContext P1 #B：plan 基础设施片段须 Grep 既有块逐字镜像。

## 遗留技术债

- **PushClipPath 仅 bounds AABB 近似**——真路径裁剪需 stencil buffer / SDF discard（G2）。
- **clip 不随 transform 旋转**——D2 轴对齐设备空间，真变换裁剪需 stencil（G2）。
- **clip 未与脏矩形 glScissor 整合**——G1.11 脏区优化复用 scissor 时需协调内容裁剪 vs 脏区裁剪栈关系。
- **glClear 受 scissor 影响**——当前契约依赖「Clear 在 Push clip 前调用」；若未来需 clip 激活时局部 Clear 须显式文档化。

## 参考文档

- 设计规格：[`docs/specs/2026-06-06-gles-canvas-clip-design.md`](../../docs/specs/2026-06-06-gles-canvas-clip-design.md)
- 实现计划：[`docs/plans/2026-06-06-gles-canvas-clip.md`](../../docs/plans/2026-06-06-gles-canvas-clip.md)
- 回顾文档：[`memory-bank/reflection/reflection-TASK-20260606-01.md`](../reflection/reflection-TASK-20260606-01.md)
- 蓝图：[`docs/plans/2026-05-05-gles-renderer-blueprint.md`](../../docs/plans/2026-05-05-gles-renderer-blueprint.md) §3.10
- 提交链：`2c98a90` plan → `58abe5b` RED → `826605d` GREEN → `4c866c2` finalize → `a712614` reflect
