# 归档：G1.9 `GLESCanvas::DrawImage`

**日期：** 2026-05-29
**任务 ID：** TASK-20260529-04
**复杂度级别：** Level 4（GLES 蓝图实施第九步 / MVP-C 战略主线第九个实施任务）
**状态：** ✅ 已完成

---

## 任务概述

为 `GLESCanvas` 补齐图像绘制能力——将 `DrawImage(const Image&, src_rect, dst_rect)`（原 `gles_canvas.h:88` no-op stub）实现为真实的 GPU 纹理 blit。延续 G1.8 的「纹理 + quad + shader」范式，由 GL_R8 单通道字形扩展到 **RGBA8 全彩图像纹理**：

- 图像纹理上传：`Image`（RGBA8 / `u32* pixels`）→ `glTexImage2D(GL_RGBA8)`。
- src/dst rect 采样：src_rect 子区（图像像素）→ UV，dst_rect（画布像素）→ quad，镜像 `software_canvas.cc:282-320` 的 tx/ty 映射。
- image shader：`kImageVert/kImageFrag`（RGBA 直采，alpha 由 GL_BLEND）。
- 完整版增项：`ImageTexturePool` 纹理缓存（避免每帧重传）+ 多图管理 + `OnContextLost/Restored`。

---

## 技术方案

**选定方案：** Level 4 完整版，D1-D8 推荐锁定（**D2=B** 指针键缓存），跳过独立 `/creative`（复用 `creative-gles-resources.md` §4，reconcile R1-R5 折入 spec）。两轮 TDD Build：轮次 1 `ImageTexturePool`（独立可测）→ 轮次 2 `GLESCanvas::DrawImage`（集成）。

**关键 reconcile（spec R1-R5）：**
- **R1（核心）：** `Image` 无 handle 字段 → creative §4 假设的 `image.handle()` 不可用 → 缓存键改为 `(u64)image.pixels()` + (w,h) 校验（指针复用到异尺寸图时 glDelete 旧 tex 重传）。
- R2 不生成 mipmap（Mesa swrast 风险规避）。
- R3 `OnContextRestored` 无参（无 ImageCache 外部依赖）。
- R4 image frag 纯采样（Canvas API 无 brush/opacity 入参）。
- R5 RGBA32 byte 序 = `GL_RGBA` 直传。

---

## 实现摘要

### 文件变更

| 操作 | 文件路径 | 说明 |
|------|---------|------|
| 创建 | `veloxa/graphics/gles/image_texture_pool.h` | `ImageTexturePool` 接口（指针键缓存）|
| 创建 | `veloxa/graphics/gles/image_texture_pool.cc` | GL_RGBA8 上传 / 命中复用 / 异尺寸重传 / Context Lost·Restored / dtor glDelete |
| 创建 | `tests/graphics/gles/image_texture_pool_test.cc` | 8 单测 |
| 创建 | `tests/graphics/gles/gles_canvas_image_test.cc` | 8 像素测 |
| 修改 | `veloxa/graphics/gles/shaders.h` | + `kImageVert`/`kImageFrag` + `kAllShaderSources` 注册 |
| 修改 | `veloxa/graphics/gles/gles_canvas.h` | image program/VAO/VBO/uniforms + `image_pool_` + `DrawImage` override + Init/Destroy 声明 |
| 修改 | `veloxa/graphics/gles/gles_canvas.cc` | `InitImageResources`/`DestroyImageResources` + ctor/dtor 接线 + `DrawImage` 实现 |
| 修改 | `veloxa/graphics/CMakeLists.txt` | 注册 `image_texture_pool.cc`（gles guard）|
| 修改 | `tests/CMakeLists.txt` | 注册 2 测（gles guard）|

文件清单与 plan §1 **9/9 完全一致**。

### 关键决策

1. **D2=B 指针键缓存** — 键 `(u64)image.pixels()` + (w,h) 校验。权衡：`Image` 无 handle，指针是唯一稳定可得的身份标识；接受 ABA 风险（释放后同址同尺寸新图，MVP 文档化）。
2. **`ImageTexturePool` 无条件构造** — 与 `GlyphAtlas`（需 FontManager+GlyphCache 才建）不同，image pool 无外部依赖，ctor 中恒建，`DrawImage` 无需字体即可工作。
3. **资源范式直接复用 G1.8** — image program/VAO/VBO 镜像 glyph（interleaved pos+uv，loc 0/1，dyn STREAM_DRAW），shader 复用 NDC+Y-flip 约定，`InitImageResources`/`DestroyImageResources` 对称 ctor/dtor。
4. **P1#A 副作用契约前置防御** — plan/spec 提前明示「`GetOrUpload` 后、draw 前重绑纹理」，`DrawImage` 中 `glBindTexture(tex)` 紧贴 `glDrawArrays`，规避 G1.8 全屏白 bug。
5. **OnContextLost/Restored 清缓存** — 上下文丢失时不 glDelete（句柄已失效），仅清缓存；lazy 重传于下次 `GetOrUpload`。

### 安全决策

**本任务不涉及安全风险变更。** 保持 GLES 既有安全契约：
- `kImageVert/kImageFrag` 为编译时常量，从不与调用方数据拼接，已注册 `kAllShaderSources` → `shader_injection_test` S1 自动覆盖。
- 输入早退校验：`image.valid()` + `src/dst.IsEmpty()`。
- 图像像素来自内部 `vx::gfx::Image`（RGBA8），src/dst rect 来自内部 layout，无外部输入面。
- 0 新依赖（无 FetchContent 变更）。

---

## 测试覆盖

**轮次 1 — `image_texture_pool_test`（8）：** Ctor_Empty / GetOrUpload_ValidTexture / CacheHit / CacheMiss / InvalidZero / NoGLError / OnContextLost_ClearsCache / OnContextRestored_LazyReupload。RED 4/8 fail（stub）→ GREEN 8/8。

**轮次 2 — `gles_canvas_image_test`（8）：** DrawImage_RendersOpaqueColor / InvalidImageNoDraw / EmptySrcRectNoDraw / EmptyDstRectNoDraw / NoGLError / AfterSetTransform / **SubRectSampling**（左红右蓝图 src 取右半 → dst 全蓝且无红，验 UV 映射）/ RepeatDrawCacheReuse_NoGLError。RED 4/8 fail（stub）→ GREEN 8/8。像素断言用 P1#2 双通道（红 `R>200 && G<60 && B<60` / 蓝对应）。

**三 build 矩阵（完成验证）：** gles **1432/1432**（1416 + 16 新）/ software **1303/1303** / no-devtool **1141/1141**，均零退化（1 项 WPT 预存跳过）。

---

## 经验教训

1. **reflection P1 建议确能阻断 bug 复发** — G1.8 全屏白调试代价换来的 P1#A，本次以「plan 一行注释 + 实现一行重绑」零成本规避同类 bug，**两轮 RED→GREEN 各一次过、零 debug 迭代**。P1#B（容器 API 名称）同样主动预防成功。验证「调试教训固化为 plan checklist」的高 ROI。
2. **GLES 纹理-采样资源范式已成熟可模板化** — glyph（G1.8）与 image（G1.9）的 program/VAO/VBO/shader/Init-Destroy/Context-Lost 高度同形，新代码近乎「填模板」。G1.10+ FBO 资源可沿用骨架。
3. **轻微测试命名漂移应在 finalize 前回写 plan** — 实现期把 `T3 DstScaleUp` 改为 EmptySrc/EmptyDst 拆分，覆盖等价但未回写 plan 矩阵；不影响验收，但保持 plan↔实际一致便于审计。

---

## 架构影响与长期维护建议（Level 4）

**架构影响：** `GLESCanvas` 16 个 Canvas 方法已实现 12 个（Begin/End/Clear/Transform/State + FillRect/FillRoundedRect/FillPath + Stroke* + DrawText + **DrawImage**）。剩余 4 个为 G1.10 Clip / G1.11 Layer / G1.12 CreatePath。`ImageTexturePool` 与 `GlyphAtlas` 共同确立了 GLES 后端「资源池对象 + Context Lost/Restored + 副作用契约」的资源管理骨架。

**技术债（MVP 取舍 / 记 techContext，G2/G1.10 处理）：**
1. `ImageTexturePool` 无界缓存（无 LRU/容量上限，多图大场景显存膨胀）。
2. 仅 LINEAR 采样（用户 scope 提及「采样过滤选项」，MVP 锁 LINEAR，nearest 切换待加）。
3. 无 opacity/tint（`kImageFrag` 纯采样，Canvas API 无 brush/opacity）。
4. 无 clip 裁剪（待 G1.10，与 software DrawImage 的 CurrentClip 路径不对齐）。
5. ABA 风险（pixels 指针释放后同址同尺寸新图，MVP 文档化接受）。
6. 逐图 draw call（未批量化）。

---

## 参考文档

- 设计规格：[`docs/specs/2026-05-29-gles-canvas-drawimage-design.md`](../../docs/specs/2026-05-29-gles-canvas-drawimage-design.md)
- 实现计划：[`docs/plans/2026-05-29-gles-canvas-drawimage.md`](../../docs/plans/2026-05-29-gles-canvas-drawimage.md)
- 创意设计：[`memory-bank/creative/creative-gles-resources.md`](../creative/creative-gles-resources.md) §4（复用，reconcile R1-R5 折入 spec）
- 回顾文档：[`memory-bank/reflection/reflection-TASK-20260529-04.md`](../reflection/reflection-TASK-20260529-04.md)
- 系统模式：`memory-bank/systemPatterns.md`「GL 全局状态副作用契约」dual-evidence + 「GLES 纹理-采样资源对象范式」dual-evidence
- 技术上下文：`memory-bank/techContext.md`「GLES G1.9 ImageTexturePool + DrawImage」

---

## Commit 链

| Phase | commit |
|---|---|
| Plan | `chore(plan): land G1.9 DrawImage plan + memory bank` |
| 1A RED | `test(gles): G1.9 round1 RED — ImageTexturePool unit tests (4/8 fail on stub)` |
| 1B GREEN | `feat(gles): G1.9 round1 GREEN — ImageTexturePool RGBA8 cache (8/8 pass)` |
| 2A RED | `test(gles): G1.9 round2 RED — DrawImage pixel tests (4/8 fail on stub)` |
| 2B GREEN | `feat(gles): G1.9 round2 GREEN — GLESCanvas::DrawImage (RGBA8 blit, 8/8 pass)` |
| Reflect | `docs(reflect): add reflection for TASK-20260529-04` |
