# 回顾：G1.8 `GlyphAtlas` + `GLESCanvas::DrawText`

**日期：** 2026-05-29
**任务 ID：** TASK-20260529-03
**复杂度级别：** Level 4（GPU 资源管理 + 文本渲染集成）

---

## 计划 vs 实际

| 维度 | 计划 | 实际 | 偏差原因 |
|------|------|------|---------|
| Build 轮次 | 2（GlyphAtlas / DrawText）| 2 | 一致 ✅ |
| 测试数 | ~22（atlas ~10 + text ~12）| **17**（atlas 10 + text 7）| text 测从 12→7：区域扫描覆盖替代多个逐像素解析测；裁掉色变/基线精度/cache 复用/空格/多字 advance 探针 |
| ctest gles | 期望 1417–1421（+18–22）| **1416**（+17）| text 测少 5 个，略低于期望区间 1 |
| 文件变更 | 9（plan §1 清单）| 9（完全一致）| **plan 文件清单 0 偏差** ✅ |
| 代码行（非 doc）| ~1053 | ~864（src+test，含 +157 gles_canvas.cc）| 估算落在 ×[0.85,1.5] 区内 |
| 设计变更 | — | MakeKey 位布局 / CreateTexture 零初始化 / HashMap API 名称 | 见下「经验教训」|

### 关键设计偏差

1. **CreateTexture 零初始化**（优于 plan）：plan 用 `glTexImage2D(..., nullptr)`，实际上传 1MB 零缓冲，杜绝未写区域经双线性采样把脏覆盖渗入字形 quad。
2. **MakeKey 位布局**：plan `font<<40 | g<<16 | px`，实际 `font<<40 | px<<24 | gid&0xFFFFFF` —— 给 glyph_id 留 24 位（真实字体 glyph 数 < 16M），更稳，功能等价。
3. **HashMap API 名称错**：plan §0.4 审计声称 `find/end/operator[]` 可用（类比 STL），实际 API 为 `Find/Insert`（首字母大写，`hash_map.h:175,182`）→ 编译期 1 次修正。**反复模式 #3 部分命中**（见§3.5）。

---

## 做得好的

1. **TDD 2 轮严格闭环**：轮次 1（atlas 8/10 FAIL→10/10 PASS）+ 轮次 2（text 2/7 FAIL→7/7 PASS），4 commit 精确对齐 plan 时间线（RED / GREEN ×2）。RED 阶段「空壳 stub 使部分断言 stub-pass」被正确识别为预期（missing-glyph / context-lost 2 个）。
2. **plan 文件清单 100% 准确**：连续抑制反复模式 #1（计划清单与实际不一致，历史 9+ 次）。
3. **系统化调试快速定位纹理重绑 bug**：全屏白 → 临时把 `kGlyphFrag` 改纯红 → 几何正常（nonwhite=1155）→ 排除几何/状态，精确定位到「`GetOrUpload` 上传时 `glBindTexture(...,0)` 解绑」与「循环外单次绑定」的交互。符合 systematic-debugging「二分管线」原则。
4. **复用既有基础设施 0 新依赖**：FT 栅格化镜像 `software_canvas.cc:207-230`；glyph program 复用 `CompileShader/LinkProgram/Matrix3x2ToMat3`；着色器对齐 `u_xform_px/u_viewport_px` 约定。
5. **承接 G1.7 反思 P1#1/P1#2 落地**：text 像素测用区域扫描（ascender band 解析边界）+ 双通道约束 `R>200 && green<60`，0 假绿。
6. **三矩阵零退化**：gles 1416 / software 1303 / no-devtool 1141，全绿（含 `shader_injection_test` 自动覆盖 2 新着色器）。

---

## 遇到的挑战

1. **纹理重绑 bug（最大挑战）**：初版把 `glBindTexture(atlas)` 放在 glyph 循环**外**，而 `GetOrUpload`（循环内调用）在 cache-miss 上传后会 `glBindTexture(GL_TEXTURE_2D, 0)` 解绑 → 绘制时纹理单元 0 无纹理 → 采样返回 0 → 全屏白。
   - **讽刺点**：plan §2B.3 的 DrawText 代码（line 346）**本就**在循环内含 `glBindTexture(...texture_id())`，是我实现时偏离了 plan 把它提到循环外。**plan 是对的，实现走样了**。
   - **根因**：plan 未显式注释「为何必须在循环内重绑」——即 `GetOrUpload` 会改 GL_TEXTURE_2D 绑定的副作用契约未被文档化，导致「看起来冗余的重绑」被我误优化掉。
2. **RED 阶段 stub-pass 噪声**：空壳实现使若干断言「碰巧」通过（missing-glyph 返回 `{valid=false}` 恰好满足、context-lost 纹理本就 0）。需人工甄别哪些 FAIL 是真 RED、哪些 PASS 是 stub 巧合，否则可能误判 RED 不充分。

---

## 经验教训

1. **GL 状态副作用必须文档化（新模式，P1）**：当一个「资源对象方法」（如 `GlyphAtlas::GetOrUpload`）在内部 mutate 全局 GL 状态（`GL_TEXTURE_2D` 绑定、`GL_UNPACK_ALIGNMENT`）时，调用方在该调用**之后**、draw **之前**必须重新建立自己依赖的状态。plan/接口需显式声明此契约，否则「循环内重绑」会被误读为冗余并删除。→ systemPatterns first-evidence。
2. **容器 API 审计须读真实 header 方法名**：不可凭 STL 直觉假设 `find/end/operator[]`。本仓 `HashMap` 用 `Find/Insert`（PascalCase）。→ 反复模式 #3 变体，迁移 activeContext 待处理。
3. **plan 内「看似冗余」的语句应保留并注释其必要性**：实现者偏离 plan 的「优化」是 bug 高发点；plan 评审时对每条 GL 状态调用标注「为何不可省」。
4. **RED 充分性判定**：对「空壳 stub 可能 stub-pass」的测试，RED 阶段应显式记录「哪些 FAIL 是目标、哪些 PASS 是 stub 巧合」，避免 RED 信号被稀释。

---

## 改进建议（附优先级与落实方式）

| # | 建议 | 优先级 | 落实方式 | 目标 |
|---|------|--------|---------|------|
| 1 | GLES 资源对象方法的 GL 状态副作用契约（GetOrUpload mutate texture binding）→ 调用方 draw 前必须重建状态 | **P1** | 新 systemPattern first-evidence + 下次 GLES 任务 plan checklist | `systemPatterns.md` |
| 2 | 容器 API 审计读真实 header 方法名（HashMap=Find/Insert 非 find/end）| **P1**（反复模式 #3 变体）| activeContext 待处理 + plan Phase 0 audit 强化 | `activeContext.md` |
| 3 | plan 内 GL 状态调用逐条标注「不可省原因」，防实现者误优化 | P2 | writing-plans.mdc GLES 段建议 | `systemPatterns.md` |
| 4 | text 像素测回补：cache 复用 / 空格 advance / 基线精度 / 色变 / 多字 advance 探针（plan T3/T5/T10/T11/T12 裁掉）| P2（测试债）| G2 文本优化任务 | `techContext.md` |
| 5 | Level 4 scope「LRU/emoji」实为 MVP（clear-all 驱逐 / GL_R8 单色无 emoji）| P2（技术债显式标注）| 记技术债 | `techContext.md` |

优先级定义见命令模板。P1 已迁 activeContext，P2 记 systemPatterns/techContext。

---

## 技术改进建议

- **FT 栅格化重复**（R3）：`GlyphAtlas` 与 `SoftwareCanvas::DrawText` 各有一份 FT_Load/Render_Glyph → GlyphBitmap 构造逻辑。G2 可抽 `text::RasterizeGlyph(face, glyph_id) → GlyphBitmap` 共享 helper。
- **逐字形 draw call**（R4）：每 glyph 一次 `glBufferData + glDrawArrays`。G2 批量化（单帧聚合所有 glyph quad → 一次 draw + 共享 atlas binding）。
- **atlas 驱逐策略**：当前 full→clear-all（plan §1B.2 步骤 3），Level 4 scope 标称 LRU 未实现。长文本/多字号混排可能频繁 clear。G2 引入 LRU 或多页 atlas。
- **emoji / 彩色字形**：GL_R8 单通道仅覆盖单色描边字形；CBDT/COLR 彩色 emoji 未处理（scope 标称但 MVP 跳过）。

---

## 安全评估

| 维度 | 状态 | 备注 |
|------|------|------|
| 输入验证 | N/A | 文本内容来自内部 layout 树 / glyph_id 来自 HarfBuzz shaping，非外部 |
| 认证/授权 | N/A | — |
| 数据保护 | N/A | — |
| 依赖审计 | ✅ | 0 新依赖（复用 FreeType / FontManager / GlyphCache）|
| 错误信息脱敏 | N/A | — |
| GLSL 注入 | ✅ | `kGlyphVert/kGlyphFrag` 编译时常量 + 注册 `kAllShaderSources` → `shader_injection_test` S1 自动覆盖；无 caller 数据拼接 |

**结论：** 本任务不涉及外部输入安全面；唯一安全相关项（GLSL 注入）通过 B6=A 静态嵌入 + 安全回归测试覆盖闭环。

---

## 反复模式识别

| 已知模式 | 历史频率 | 本次 |
|---------|---------|------|
| 计划文件清单与实际不一致 | 9+ | ❌ 未命中（清单 100% 准确）|
| 子代理产出需返工 | 7+ | N/A（未用子代理）|
| 前置依赖/环境/API 能力未验证 | 8+ | ⚠️ **部分命中**：HashMap API 名称审计错（find/end vs Find/Insert），编译期 1 次低成本修正 |
| 非默认路径遗漏验证 | 4+ | ⚠️ **部分命中**：text 测裁掉 cache-reuse/空格/基线探针（但 empty/transparent/no-font 反向探针保留）|
| 测试隔离问题 | 7+ | ❌ 未命中（每测独立 surface + offscreen）|
| 提交粒度偏离 | 7+ | ❌ 未命中（4 commit 对齐 plan）|
| TDD 严格度不匹配 | 11+ | ❌ 未命中（2 轮严格 RED→GREEN）|

**新增首次模式：** GLES 资源对象方法的 GL 全局状态副作用契约（GetOrUpload 解绑纹理 → 调用方 draw 前须重绑）→ 升 P1 入 systemPatterns first-evidence。

---

## 总结

Level 4 文本渲染首见任务，2 轮 TDD 严格闭环、plan 文件清单零偏差、三矩阵零退化。最大教训是「plan 中看似冗余的循环内纹理重绑实为 GetOrUpload GL 状态副作用的必要补偿」——实现者偏离 plan 的优化引入全屏白 bug，经系统化二分调试（纯红 frag 隔离）快速定位。沉淀 1 个新 first-evidence 模式（GL 状态副作用契约）+ 强化 1 个反复模式（容器 API 审计）。MVP 范围明确收敛（clear-all 驱逐 / 单色 / 逐字形 draw），LRU/emoji/批量化作为 G2 技术债显式登记。
