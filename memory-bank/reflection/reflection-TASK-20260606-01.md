# 回顾：GLES `GLESCanvas::PushClipRect/PushClipPath/PopClip`（G1.10 Clip / glScissor）

**日期：** 2026-06-06
**任务 ID：** TASK-20260606-01
**复杂度级别：** Level 3
**分支：** `feature/TASK-20260606-01-gles-canvas-clip`（基线 main）

## 计划 vs 实际

| 维度 | 计划 | 实际 | 偏差原因 |
|------|------|------|---------|
| 文件变更 | 4（gles_canvas.{h,cc} 改 / clip_test.cc 新建 / CMake 注册） | 4（完全一致） | 无偏差 |
| 测试数 | ~8（C1-C8） | 8（C1-C8 全实现） | 无偏差 |
| 提交链 | plan / RED / GREEN / finalize | plan / RED / GREEN / finalize（4 提交） | 无偏差（提交消息措辞微调：用 `G1.10` 简称替代 `TASK-...` 长前缀，正文保留任务 ID） |
| ctest gles | 1437 → ~1444-1445 | 1437 → **1445** | 命中预测上界（+8，对应 8 测全注册全过） |
| ctest sw | 1303 不变 | **build-sw-devtool 1337** 不变 | plan 引用的 sw 基线 1303 为旧值；实测 software+devtool 矩阵为 1337（计数不变即零退化，结论一致） |
| ctest no-devtool | 1141 不变 | 1141 不变 | 无偏差 |
| 预估时间 | plan×0.6 ~3-4.5h / 实测预期 ~25-40min | 实测 ~13min（含三矩阵构建） | 镜像 software 高度复用 + 零 debug 迭代，落在极速区下沿 |
| debug 迭代 | — | 0 | RED 信号精确 + 设计直接落地 |

## 做得好的

1. **RED 路径优于计划两选项。** plan A.3 给了两个 RED 方案（① .cc 加空实现占位 ② header+cc 骨架连续）。实际采用更干净的第三路径：**保留内联 stub `{}` 不动，仅加测试 + CMake** → 对既有 no-op stub 跑出断言 FAIL = RED，GREEN 再一次性改 header+cc。好处：RED 提交纯测试（test-only），GREEN 提交纯实现，提交语义最清晰。
2. **RED 信号精确命中预测。** 预测 C1/C2/C4/C6/C7 FAIL、C3/C5/C8 PASS（无裁剪时全屏 fill 恰好满足），实测完全一致（5 FAIL / 3 PASS）。说明像素测采样坐标解析推导（P1#1）+ 反向探针设计在 RED 阶段即可自验。
3. **镜像 software 设计 = 零认知负担。** clip_stack_ 存设备空间交集 rect 的语义直接照搬 `SoftwareCanvas`，`PushClipRect`/`PopClip`/`PushState`/`PopState` 逻辑一一对应，跨后端一致性天然保证，且 glScissor 直接消费设备空间窗口坐标。
4. **GL 副作用契约延续。** 沿用既有「Begin 帧复位 + PopState reapply」模式：`Begin()` 清栈 + `glDisable(GL_SCISSOR_TEST)`、`PopState` 弹栈后 `ApplyScissor()`，杜绝 scissor 全局状态跨帧/跨 state 泄露。
5. **三矩阵零退化证据完整。** gles 1445 / sw-devtool 1337 / no-devtool 1141，三档全绿，证据优于断言。

## 遇到的挑战

1. **`Rect` 字段名 `w/h` vs `width/height`。** 初版 ApplyScissor/CurrentClipDevice 误用 `c.width/c.height`，编译前自查 `types.h` 发现 `struct Rect { f32 x,y,w,h; }`，一次性修正。属低成本一次命中，但暴露「跨任务记忆字段名易混」。
2. **plan 的 CMake 片段用了不存在的宏。** plan A.2 写 `vx_add_test(gles_canvas_clip_test ...)`，但仓库实际无此宏，gles 测试统一用 `add_executable` + `target_link_libraries` + `gtest_discover_tests` 三段式（含 `SDL_VIDEODRIVER=offscreen` ENV property）。实际通过 Grep 既有 `gles_canvas_image_test` 注册块逐字镜像解决，未受影响，但 plan 的 CMake 伪代码若被直接照搬会失败。
3. **software 基线数字陈旧。** plan 引用 sw 1303，实测 build-sw-devtool 为 1337；不影响零退化结论（计数不变即可），但说明 plan 阶段抄录的基线 ctest 数随其他任务推进会过期。

## 经验教训

1. **「保留 stub 仅加测试」是内联 stub→正式实现类任务的最优 RED 范式。** 当目标方法已是内联 no-op `{}`（可链接），RED 阶段只需加测试触发断言 FAIL，无需动生产代码，使 RED/GREEN 提交语义彻底分离。可固化为 GLES 蓝图后续 stub-填充任务（PushLayer/PopLayer/CreatePath）的默认手法。
2. **plan 的 CMake/容器 API 伪代码应「Grep 既有块逐字镜像」而非凭记忆写宏。** 与 G1.9「容器 API 名称」反复模式同源——plan 阶段写基础设施片段时应直接引用仓库现存最近邻块。
3. **plan 引用的 ctest 基线应标注「采集时点」或改为相对断言（计数不变 / +N）。** 绝对基线数字跨任务易过期；回顾/验证只需「该矩阵计数相对自身基线不减」。

## 改进建议（附优先级与落实方式）

| # | 建议 | 优先级 | 落实方式 | 目标 |
|---|------|--------|---------|------|
| 1 | 「内联 stub → 正式实现」类任务，RED 阶段保留 stub 仅加测试（test-only 提交），GREEN 再改生产代码 | P2 | 记 systemPatterns（GLES stub-填充范式） | systemPatterns.md |
| 2 | plan 阶段 CMake / 容器 API 片段必须 Grep 既有最近邻块逐字镜像，不凭记忆写宏（与 G1.9「容器 API 名称」同源，反复出现） | P1 | 迁移至 activeContext 待处理事项 | activeContext.md（/plan 自律） |
| 3 | plan 引用的 ctest 基线标注采集时点，或验证改用相对断言（矩阵计数不减） | P2 | 记 techContext | techContext.md |
| 4 | `Rect` 等高频几何类型字段名（`w/h` 非 `width/height`）易混，写 GL 坐标代码前先 Grep 确认 | P2 | 记 techContext（已知陷阱） | techContext.md |

## 技术改进建议

1. **PushClipPath 仅 bounds AABB 近似**（D4）——真路径裁剪需 stencil buffer 或 SDF discard，旋转/曲线 path 下 over-clip 为 AABB 外接。→ G2 技术债。
2. **clip 不随 transform 旋转**（D2 轴对齐设备空间）——与 software 一致，但旋转变换下 clip 仍是屏幕轴对齐矩形，非随内容旋转。真变换裁剪需 stencil。→ G2 技术债。
3. **clip 未与脏矩形 glScissor 整合**——G1.11 脏矩形优化会复用 scissor，需协调「内容裁剪 scissor」与「脏区裁剪 scissor」的栈/交集关系。→ G1.11 独立任务前置注意。
4. **glClear 受 scissor 影响**——当前契约依赖「测试均在 Push clip 前 Clear」。若未来出现「clip 激活时 Clear 局部区域」需求，行为是 clip 内清除（可能正是所需），但需显式文档化。

## 安全评估

| 维度 | 状态 | 备注 |
|------|------|------|
| 输入验证 | N/A | 纯 GL 状态裁剪，无外部输入 |
| 认证/授权 | N/A | 不涉及 |
| 数据保护（加密/脱敏） | N/A | 不涉及 |
| 依赖审计 | N/A | 0 新依赖（glScissor GLES 3.0 core） |
| 错误信息脱敏 | N/A | 不涉及 |
| 敏感数据处理 | N/A | 不涉及 |

**结论：本任务不涉及安全变更**（纯 GL 裁剪状态 / 0 用户输入 / 0 GLSL 拼接 / 0 新 ABI 表面）。

## 反复模式识别

| 已知模式 | 本次是否重复？ | 说明 |
|---------|-------------|------|
| 计划文件清单与实际变更不一致 | ❌ 否 | 4/4 文件精确命中 |
| 子代理产出需大量返工 | N/A | 本次未用子代理 |
| 前置依赖/环境/API 能力未验证 | ❌ 否 | glScissor GLES 3.0 core 已 VAN 验证 |
| 非默认路径遗漏验证 | ❌ 否 | 空交集 + Begin 复位双反向探针已覆盖 |
| 测试隔离问题 | ❌ 否 | 每测独立 fixture，无串扰 |
| 提交粒度偏离计划 | ❌ 否 | 4 提交链与 plan 一致 |
| TDD 严格度与场景不匹配 | ❌ 否 | 严格 RED→GREEN，RED 路径反而优化 |
| **容器/基础设施 API 名称凭记忆**（plan CMake 宏 + Rect 字段名） | ⚠️ **轻度重复** | 与 G1.9「容器 API 名称」同源 → 建议 #2 升 P1 |
