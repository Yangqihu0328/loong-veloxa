# 归档：GLES 图像采样过滤选项 NEAREST/LINEAR

**日期：** 2026-06-02
**任务 ID：** TASK-20260602-01
**复杂度级别：** Level 2（G1.9 技术债 #2 清理）
**状态：** ✅ 已完成

## 任务概述

清理 G1.9（TASK-20260529-04）遗留技术债 #2——`ImageTexturePool::GetOrUpload` 硬编码 `GL_TEXTURE_MIN/MAG_FILTER = GL_LINEAR`，调用方无法选择 NEAREST（像素艺术 / 精确像素映射）。为 `GLESCanvas` 补齐采样过滤选项，保持 LINEAR 默认（向后兼容）。

## 技术方案

**选定：** GLES 局部 setter（D1=①）——`SamplingFilter` enum 入共享 `types.h`，`GLESCanvas::SetImageSamplingFilter()` 作为 GLES 专属公有 API（**不**上抽象 `Canvas`），`DrawImage` 每 draw 按当前 filter 设 `glTexParameteri`。

**理由：** 复杂度缩减 / 最小侵入——`ImageTexturePool` + CMake + software + renderer 全部零改动；enum 入 `types.h` 为未来提升抽象 API 留门。否决方案②（提升抽象 Canvas，引入跨后端语义分歧 + 范围蔓延）、③（改 DrawImage 签名，最大 blast + 虚函数默认参陷阱）。

**决策锁定（D1-D5）：** D1=① 局部 setter / D2 filter 不入缓存键、每 draw `glTexParameteri` / D3 仅 GLES（software 维持 NEAREST-only，差异记技术债）/ D4 默认 kLinear / D5 不入 PushState。

## 实现摘要

### 文件变更

| 操作 | 文件路径 | 说明 |
|------|---------|------|
| 修改 | `veloxa/graphics/types.h` | + `enum class SamplingFilter{kLinear,kNearest}`（共享，纯新增）|
| 修改 | `veloxa/graphics/gles/gles_canvas.h` | + `SetImageSamplingFilter`/`image_sampling_filter()`（GLES 专属公有）+ `image_filter_=kLinear` 成员 |
| 修改 | `veloxa/graphics/gles/gles_canvas.cc` | `DrawImage` 绑定纹理（P1#A 重绑）后按 `image_filter_` 设 `glTexParameteri(MIN/MAG)` |
| 修改 | `tests/graphics/gles/gles_canvas_image_test.cc` | +5 测（F1-F5）+ `SeamImage`/`CountPurple` helper |

`ImageTexturePool` + CMake **零改动**。文件清单与 plan **4/4 一致**。

### 关键决策

1. **D1 GLES 局部 setter** — 不污染抽象 Canvas API，blast radius 最小；enum 放共享 `types.h` 留提升门。
2. **D2 filter 不入缓存键** — filter 是采样器状态而非纹理内容；`DrawImage` 每 draw `glTexParameteri` 权威覆盖 pool 上传时的 LINEAR 初值，使同一缓存纹理在 NEAREST/LINEAR 间切换正确（`DrawImage_FilterSwitch_NoGLError` 验证），pool 无需感知 filter。
3. **filter 设置紧贴 P1#A 重绑之后** — `glBindTexture(tex)`（G1.9 副作用契约重绑）后立即设 filter，再 `glDrawArrays`，符合 GL 状态副作用契约。

### 安全决策

**本任务不涉及安全变更。** filter 为内部 `enum`，无外部输入；无 GLSL 拼接；0 新依赖。

## 测试覆盖

`gles_canvas_image_test` 扩展 +5（F1-F5），单轮 TDD（RED 编译失败 API 未声明 → GREEN 13/13）：
- **F1 `SetGetSamplingFilter`** — setter/getter 往返 + 默认 kLinear（D4）。
- **F2 `DrawImage_DefaultLinear_BlendsAtSeam`** — 不调 setter，接缝有紫（默认 LINEAR 保留 / 反向探针防回归）。
- **F3 `DrawImage_Nearest_HardSeam`** — setter(kNearest)，接缝整行无紫 + 红蓝各有（硬边）。
- **F4 `DrawImage_Linear_SoftSeam`** — 显式 setter(kLinear)，接缝有紫。
- **F5 `DrawImage_FilterSwitch_NoGLError`** — kNearest↔kLinear 切换多 draw 无 GL error。

**区分手法：** 2×1 红蓝图放大 64× → LINEAR 接缝混合出紫（`R∈[60,200]&&B∈[60,200]`）/ NEAREST 硬边无紫。承接 P1#1（接缝 x≈32 解析）+ P1#2（红/蓝/紫双通道）。

**三 build 矩阵（完成验证）：** gles **1437/1437**（1432+5 精确命中）/ software **1303/1303** / no-devtool **1141/1141**，均零退化（`SamplingFilter` enum 纯新增未破坏既有 `types.h` 消费者）。

## 经验教训

1. **技术债清理「最小侵入」优先有效** — D1 选 GLES 局部 setter 而非提升抽象 Canvas API，既清债又零回归；技术债清理不必追求一步到位的完美架构，留提升门即可。
2. **小任务也值得完整 `/plan` 头脑风暴** — 「加个采样选项」实含真实接口设计决策（旋钮归属 + 跨后端语义），提前锁定 D1-D5 使 build 零返工、零 debug 迭代。
3. **跨后端语义分歧需显式文档化** — software NEAREST-only vs GLES 默认 LINEAR，两后端默认行为本就不一致（D3），文档化记 techContext 避免未来误判为 bug。

## 遗留技术债（记 techContext）

- **跨后端采样语义分歧（D3）：** software `DrawImage` 整数截断 NEAREST-only，不读 `SamplingFilter`；统一方案（给 software 加双线性 / 高层「后端渲染质量差异矩阵」文档）= 独立 Level 3 任务（G2 候选）。
- filter 未纳入 PushState/PopState（D5 MVP 取舍）；GL sampler object 分离采样器状态（YAGNI，G2 性能优化时评估）。

## 参考文档

- 设计规格：[`docs/specs/2026-06-02-gles-image-sampling-filter-design.md`](../../docs/specs/2026-06-02-gles-image-sampling-filter-design.md)
- 实现计划：[`docs/plans/2026-06-02-gles-image-sampling-filter.md`](../../docs/plans/2026-06-02-gles-image-sampling-filter.md)
- 回顾文档：[`memory-bank/reflection/reflection-TASK-20260602-01.md`](../reflection/reflection-TASK-20260602-01.md)
- 技术上下文：`memory-bank/techContext.md`「GLES 图像采样过滤 NEAREST/LINEAR」段
- 前序任务：[`archive-TASK-20260529-04.md`](archive-TASK-20260529-04.md)（G1.9 DrawImage，本任务清理其技术债 #2）

## Commit 链

| Phase | commit |
|---|---|
| VAN | `chore(van): init TASK-20260602-01 GLES image sampling filter` |
| Plan | `chore(plan): land GLES image sampling filter plan + memory bank` |
| A RED | `test(gles): TASK-20260602-01 RED — image sampling filter tests (compile fail)` |
| B/C GREEN | `feat(gles): TASK-20260602-01 — DrawImage NEAREST/LINEAR sampling filter` |
| Reflect | `docs(reflect): add reflection for TASK-20260602-01` |
