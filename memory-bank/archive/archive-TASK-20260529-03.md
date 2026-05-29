# 归档：G1.8 `GlyphAtlas` + `GLESCanvas::DrawText`

**日期：** 2026-05-29
**任务 ID：** TASK-20260529-03
**复杂度级别：** Level 4（GPU 资源管理 + 文本渲染集成）
**状态：** ✅ 已完成

---

## 任务概述

GLES 硬件渲染后端蓝图实施第八步（G1.8）。为 `GLESCanvas` 补齐文本渲染能力：新增 `GlyphAtlas`（GL_R8 纹理图集 + CPU FreeType 栅格化 + row-pack 装箱 + 上下文丢失恢复）与 `GLESCanvas::DrawText`（HarfBuzz shaping → 逐字形纹理 quad 绘制），复用既有 `veloxa/text/` 基础设施（FontManager / GlyphCache / ShapeCache），实现 0 新依赖。

承接 MVP-C 战略主线，使 GLES 后端与 SoftwareCanvas 在文本绘制上对齐。

---

## 技术方案

**选定方案（D1-D8 = A / all_recommended 锁定）：**

| 维度 | 决策 | 理由 |
|---|---|---|
| atlas 格式 | GL_R8 单通道覆盖图集 1024² | 单色字形覆盖 alpha 足够，省 4× 显存 |
| 装箱算法 | row-pack 分层（first-fit shelf）+ 1px gutter | MVP 最简，gutter 防双线性渗色 |
| 栅格化 | GlyphAtlas 自栅格化（镜像 `software_canvas.cc:207-230`）| GlyphCache 仅缓存 CPU bitmap 不栅格化（reconcile R1）|
| cache key | u64 `font<<40 \| pixel_size<<24 \| glyph_id` | glyph_id 留 24 位避免与 pixel_size 碰撞 |
| 驱逐 | full → clear-all 重试一次（MVP）| LRU 留 G2（技术债登记）|
| shader | `kGlyphVert/kGlyphFrag` 对齐 `u_xform_px/u_viewport_px`（非 creative `u_proj` mat4）| 复用既有 NDC + Y-flip 约定（reconcile R3）|
| DrawText 流程 | FindFont→SetFacePixelSize→ShapeOrLookup→pen walk | 镜像 SoftwareCanvas（reconcile R4）|
| draw | 逐字形动态 VBO + `glDrawArrays`（MVP）| 批量化留 G2 |

**跳过独立 `/creative`：** 复用 `creative-gles-resources.md` §2.3/§3 已有设计，5 项 reconcile（R1-R5）折入 plan。

---

## 实现摘要

2 轮次 TDD（轮次 1 GlyphAtlas / 轮次 2 DrawText），每轮 RED→GREEN→REFACTOR→commit。

- **GlyphAtlas**：构造即 `CreateTexture`（零初始化 1MB 缓冲防脏区渗色）；`GetOrUpload` 查 atlas cache → miss 查 GlyphCache → 再 miss 调 FreeType `FT_Load_Glyph`/`FT_Render_Glyph` 栅格化并 `GlyphCache.Put` → `PackGlyph` 装箱 → `glTexSubImage2D` 上传 → 计算 UV 缓存返回；空格字形（width/height==0）记 advance 跳过上传；`OnContextLost`（texture=0 不 glDelete）/`OnContextRestored`（重建 + 清缓存）。
- **glyph shader**：`kGlyphVert`（pos+uv → NDC+Y-flip，`a_uv` 绑 attr loc 1）、`kGlyphFrag`（GL_R8 `.r` 作 coverage × `u_color`）；注册 `kAllShaderSources` 受 `shader_injection_test` S1 覆盖。
- **DrawText**：glyph program + 动态交错 VBO（pos.xy+uv.xy / stride 16B）；pen walk 逐字形 `GetOrUpload` + 流式 quad 绘制；早退覆盖空串/透明/无字体。
- **LinkProgram**：增 `glBindAttribLocation(program,1,"a_uv")`（对无该 attr 的程序无害）。

### 文件变更

| 操作 | 文件路径 | 说明 |
|------|---------|------|
| 创建 | `veloxa/graphics/gles/glyph_atlas.h` | GlyphAtlasInfo + GlyphAtlas 接口（~83 行）|
| 创建 | `veloxa/graphics/gles/glyph_atlas.cc` | GL_R8 atlas / row-pack / FT 栅格化 / Context Lost（~179 行）|
| 修改 | `veloxa/graphics/gles/shaders.h` | +kGlyphVert/kGlyphFrag + kAllShaderSources 注册（+36 行）|
| 修改 | `veloxa/graphics/gles/gles_canvas.h` | glyph program/uniforms/vao/vbo + glyph_atlas_ + DrawText 声明（+28 行）|
| 修改 | `veloxa/graphics/gles/gles_canvas.cc` | glyph 资源 init/destroy + DrawText 实现 + LinkProgram a_uv（+157 行）|
| 修改 | `veloxa/graphics/CMakeLists.txt` | 注册 glyph_atlas.cc 到 vx_graphics（gles guard，+1）|
| 创建 | `tests/graphics/gles/glyph_atlas_test.cc` | 10 单测（~160 行）|
| 创建 | `tests/graphics/gles/gles_canvas_text_test.cc` | 7 像素测（~168 行）|
| 修改 | `tests/CMakeLists.txt` | 注册 2 测（gles guard，+18）|

### 关键决策

1. **CreateTexture 零初始化**（偏离 plan 优化）：上传 1MB 零缓冲而非 `nullptr`，杜绝未写区域经双线性采样把脏覆盖渗入字形 quad。
2. **逐字形循环内重绑纹理**（bug 修复后回归 plan）：`GetOrUpload` 上传时会 `glBindTexture(...,0)` 解绑，故必须在每次 `glDrawArrays` 前重绑 atlas 纹理 —— 否则全屏白。
3. **MakeKey 位布局** `font<<40 | px<<24 | gid`：给 glyph_id 留 24 位防碰撞。
4. **MVP 范围收敛**：clear-all 驱逐（非 LRU）/ GL_R8 单色（无 emoji）/ 逐字形 draw（非批量）。

### 安全决策

本任务不涉及外部输入安全面（文本内容来自内部 layout 树、glyph_id 来自 HarfBuzz shaping）。唯一安全相关项为 **GLSL 注入防御**：`kGlyphVert/kGlyphFrag` 为编译期常量 raw string literal（B6=A 静态嵌入），注册 `kAllShaderSources[]` 后由 `shader_injection_test` S1 反向探针自动覆盖，无 caller 数据拼接，注入不可能。0 新依赖（复用 FreeType/FontManager/GlyphCache），无依赖审计缺口。

---

## 测试覆盖

- **`GlyphAtlasTest`（10/10 PASS）**：Ctor texture≠0 / Ascii valid info / cache hit·miss 计数 / space zero-size / UV in-range / missing glyph invalid / many-glyphs no-GLError / OnContextLost texture=0 / OnContextRestored 重建+缓存清空。
- **`GlesTextTest`（7/7 PASS）**：RendersCoverage（区域扫描 + 双通道）/ EmptyString no-draw / TransparentBrush no-draw / NoGLError / AfterSetTransform（双区域：原点空 + 平移区有墨）/ MultipleDraws no-GLError / ReverseProbe-NoFontManager no-crash。
- **三 build 矩阵零退化：** gles 1399→**1416**（+17）/ software **1303** / no-devtool **1141**；完整 build-gles **1416/1416 PASS**（~78s）。
- 测试范式承接 G1.7 P1#1（解析采样：ascender band 区域边界）+ P1#2（白底双通道 `R>200 && green<60`）。

---

## 经验教训

1. **GL 全局状态副作用契约（新 first-evidence / P1）**：会 mutate GL 状态的 helper（`GetOrUpload` 解绑 `GL_TEXTURE_2D`）调用后、draw 前必须重建依赖状态。plan 中「看似冗余」的 GL 状态调用须注释「不可省原因」防实现者误优化 —— 本任务正因此引入全屏白 bug。
2. **容器 API 审计读真实 header 方法名**：本仓 `HashMap` 用 `Find/Insert`（PascalCase），非 STL `find/end`（反复模式 #3 变体，编译期低成本修正）。
3. **系统化二分调试**：全屏白时临时纯红 frag 隔离几何 vs 纹理采样，快速锁定根因。
4. **plan 文件清单 0 偏差**：连续抑制反复模式 #1。

详见 [`reflection-TASK-20260529-03.md`](../reflection/reflection-TASK-20260529-03.md)（Level 4 全面 / 6 维度 + 反复模式识别 + 安全 checklist + 5 改进建议）。

---

## 架构影响与长期维护建议

- **架构影响：** GLES 后端文本渲染管线确立（atlas + glyph shader + 逐字形 draw），与 SoftwareCanvas 文本能力对齐。新增 GL 资源生命周期对象 `GlyphAtlas`（含 Context Lost/Restored 协议，为 G1.13 Application 生命周期接入预留）。
- **新沉淀模式：** systemPatterns「GLES 资源对象方法的 GL 全局状态副作用契约 first-evidence」。
- **技术债（G2 登记，见 techContext G1.8 段）：**
  1. FT 栅格化重复（GlyphAtlas vs SoftwareCanvas）→ 抽 `text::RasterizeGlyph(face, glyph_id)` 共享 helper。
  2. atlas 驱逐 clear-all → LRU 或多页 atlas（Level 4 scope 标称 LRU 未实现）。
  3. GL_R8 单色 → emoji/CBDT/COLR 彩色字形未处理。
  4. 逐字形 draw call → 单帧批量聚合（共享 atlas binding）。
  5. text 像素测裁剪（cache 复用/色变/空格 advance/基线精度/多字 advance 探针待回补）。

---

## 参考文档

- 设计规格：[`docs/specs/2026-05-29-gles-glyph-atlas-drawtext-design.md`](../../docs/specs/2026-05-29-gles-glyph-atlas-drawtext-design.md)
- 实现计划：[`docs/plans/2026-05-29-gles-glyph-atlas-drawtext.md`](../../docs/plans/2026-05-29-gles-glyph-atlas-drawtext.md)
- 创意设计（复用）：[`memory-bank/creative/creative-gles-resources.md`](../creative/creative-gles-resources.md) §2.3/§3
- 回顾文档：[`memory-bank/reflection/reflection-TASK-20260529-03.md`](../reflection/reflection-TASK-20260529-03.md)
- 上游蓝图：GLES 硬件渲染蓝图 §3.8（TASK-20260505-03）

---

## Commit 链

| Phase | commit | subject |
|---|---|---|
| Plan | `c084403` | chore(plan): land G1.8 GlyphAtlas + DrawText plan + memory bank |
| 1A RED | `c2552ec` | test(gles): G1.8 round1 RED — GlyphAtlas unit tests (8/10 fail on stub) |
| 1B GREEN | `7e7e498` | feat(gles): G1.8 round1 GREEN — GlyphAtlas GL_R8 atlas + FT raster (10/10) |
| 2A RED | `569af36` | test(gles): G1.8 round2 RED — DrawText pixel coverage tests (2/7 fail on stub) |
| 2B GREEN | `3187ecc` | feat(gles): G1.8 round2 GREEN — GLESCanvas::DrawText via GlyphAtlas (7/7) |
| Reflect | (reflection commit) | docs(reflect): add reflection for TASK-20260529-03 |
