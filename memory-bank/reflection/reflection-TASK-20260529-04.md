# 回顾：G1.9 `GLESCanvas::DrawImage`

**日期：** 2026-05-29
**任务 ID：** TASK-20260529-04
**复杂度级别：** Level 4（GLES 蓝图实施第九步 / MVP-C 战略主线第九个实施任务）

---

## 计划 vs 实际

| 维度 | 计划 | 实际 | 偏差原因 |
|------|------|------|---------|
| 轮次数 | 2（ImageTexturePool → DrawImage）| 2 | 一致 |
| 测试数 | ~16（pool ~8 + image ~8）| 16（pool 8 + image 8）| 一致 |
| 文件变更 | 9（4 🆕 + 5 🟡）| 9（4 🆕 + 5 🟡）| **完全一致** |
| gles ctest | 1416 → ~1432 | 1416 → **1432** | 精确命中 |
| software / no-devtool | 1303 / 1141（0 退化）| 1303 / 1141 | **零退化** |
| 估时（plan ×0.6）| ~125-175 min / 预期实测 ~70-110 min | 远低于预期（两轮均一次过、零 debug）| 范式复用 + P1 预防生效 |
| debug 迭代 | — | **0 次**（G1.8 全屏白 bug 未复发）| P1#A 提前落地 |
| 设计变更 | R1-R5 reconcile（已折入 spec/plan）| 无额外变更 | 规划阶段已消化 |

**测试命名偏差（轻微）：** plan 测试矩阵 T1-T8 命名与实际略有出入——计划的 `T3 DstScaleUp`（纹理放大）未单列，实际改为 `EmptySrcRectNoDraw` + `EmptyDstRectNoDraw` 拆分两测。覆盖等价或更强（早退路径双分支均覆盖），属可接受的实现期微调。

---

## 做得好的

1. **P1#A（GL 状态副作用契约）主动预防成功** — 上一任务 G1.8 因 `GetOrUpload` 解绑 `GL_TEXTURE_2D` 导致全屏白 bug，经纯红 frag 二分调试才定位。本次 plan 在 spec §3.3 + 风险登记 R4 提前明示「`GetOrUpload` 后、draw 前必须重绑纹理」，`DrawImage` 一次写对（`glBindTexture(tex)` 紧贴 `glDrawArrays` 前），**零 debug 迭代**。reflection→activeContext→plan checklist 闭环的正面实证。
2. **P1#B（容器 API 名称）主动预防成功** — 1B 实现前先读 `hash_map.h` 确认 `Find/Insert/Erase`（PascalCase）+ `begin/end` + `it->key/value`，dtor 遍历与 GetOrUpload 均无编译期返工。
3. **两轮 RED→GREEN 各一次过** — RED 均精确产出 4/8 fail（需绘制/上传的 4 测红、no-op/empty 的 4 测绿），GREEN 一次 8/8。TDD 节律干净。
4. **范式直接复用** — image program/VAO/VBO 完全镜像 G1.8 glyph 资源（interleaved pos+uv，loc 0/1，dyn STREAM_DRAW），shader 复用 NDC+Y-flip 约定，`InitImageResources`/`DestroyImageResources` 对称 ctor/dtor。新代码几乎是"填模板"。
5. **D2=B 指针键 reconcile 落地干净** — `Image` 无 handle，缓存键 `(u64)image.pixels()` + (w,h) 校验，指针复用到不同尺寸图时 glDelete 旧 tex 重传，逻辑被 `CacheHit`/`CacheMiss` 测验证。
6. **UV 子区采样一次正确** — `SubRectSampling` 测（左红右蓝图，src 取右半→dst 全蓝且无红）一次通过，验证 src_rect→UV 映射与 Y 方向约定正确，无需调试坐标。
7. **三矩阵零退化** — gles 1432 / software 1303 / no-devtool 1141，双 config 盲区 #7 抑制到位。

---

## 遇到的挑战

1. **几乎无实质挑战** — 这是连续第 4 个 GLES 实施任务（G1.6→G1.7→G1.8→G1.9），范式高度稳定，本次为目前摩擦最低的一次。主要"挑战"是规划阶段已前置消化（R1-R5 reconcile）。
2. **测试矩阵命名漂移（轻微）** — 实现期把 `DstScaleUp` 替换为 `EmptySrc/EmptyDst` 拆分，未回写 plan 矩阵。属反复模式「计划清单与实际不一致」的轻微变体，但覆盖未缩水。

---

## 经验教训

1. **reflection 的 P1 建议确实能阻断 bug 复发** — G1.8 付出全屏白调试代价换来的 P1#A，本次以"plan 一行注释 + 实现一行重绑"零成本规避了同类 bug。这验证了「把调试教训固化为 plan checklist 项」的高 ROI，应继续。
2. **GLES 资源范式已成熟到可模板化** — glyph（G1.8）与 image（G1.9）的 program/VAO/VBO/shader/dtor 结构高度同形。未来 G1.10+（clip/layer 的 FBO 资源）可考虑抽公共 helper 或文档化"GLES 纹理-采样资源对象范式"。
3. **轻微测试命名漂移应在 finalize 前回写 plan** — 虽不影响覆盖，但保持 plan↔实际一致便于归档审计。

---

## 改进建议（附优先级与落实方式）

| # | 建议 | 优先级 | 落实方式 | 目标 |
|---|------|--------|---------|------|
| 1 | GLES 纹理-采样资源范式（glyph/image 同形：program+交错 VAO/VBO+NDC/Y-flip shader+对称 Init/Destroy）已 2 次实证，固化为可复用范式 | P2 | 记 `systemPatterns.md` | 长期 / G1.10+ FBO 资源复用 |
| 2 | reflection P1→plan checklist 闭环已 1 次正面实证（P1#A 阻断 G1.8 bug 复发），登记为「反复模式抑制成功」正面案例 | P2 | 记 `systemPatterns.md`（P1#A 段补正面实证）| 长期 / 强化闭环信心 |
| 3 | 实现期测试矩阵微调（命名/拆分）应在 finalize 前回写 plan，保持 plan↔实际一致 | P2 | `writing-plans.mdc` 补一句 / 个人习惯 | 长期 |
| 4 | DrawImage MVP 缺口：无 clip 裁剪（G1.10）/ 无 opacity/tint（API 无 brush，D4=A）/ 仅 LINEAR 采样（用户 scope 提及"采样过滤选项"但 MVP 锁 LINEAR）/ 无 LRU 驱逐（无界缓存）/ ABA 指针复用同址同尺寸风险 | P2 | 记 `techContext.md` 技术债 | G2 / G1.10 |

无 P0/P1 建议——本次无影响当前工作流正确性或需下次同类任务立即处理的问题（P1#A/P1#B 已在上轮落地并本次验证生效）。

---

## 技术改进建议

- **缓存驱逐策略** — `ImageTexturePool` 当前无界（与 G1.8 `GlyphAtlas` 的"满则全清"不同，image pool 永不驱逐）。多图大场景下显存可能膨胀，G2 需引入 LRU 或容量上限。
- **采样过滤选项** — 用户初始 scope 含"采样过滤选项"，MVP 锁定 LINEAR。G2 可加 nearest/linear 切换（如像素艺术需 nearest）。
- **opacity/tint** — 当前 `kImageFrag` 纯采样，无全局透明度或着色。Canvas API 扩展 brush/opacity 后可加 `u_color` 调制。
- **批量化** — 与 G1.8 glyph 同样逐 draw 一个 quad（dyn VBO 每次 `glBufferData`）。多图场景可批量。

---

## 安全评估

| 维度 | 状态 | 备注 |
|------|------|------|
| 输入验证 | ✅ | `image.valid()` + `src/dst.IsEmpty()` 早退；尺寸经 GLsizei 转换 |
| 认证/授权 | N/A | 无 |
| 数据保护（加密/脱敏） | N/A | 无敏感数据 |
| 依赖审计 | ✅ | **0 新依赖**（无 FetchContent 变更）|
| 错误信息脱敏 | N/A | 无对外错误输出 |
| 敏感数据处理 | N/A | 无 |
| Shader 注入面 | ✅ | `kImageVert/kImageFrag` 编译时常量，已注册 `kAllShaderSources` → `shader_injection_test` S1 自动覆盖；从不与调用方数据拼接 |

**本任务无安全风险变更**，但保持了 GLES 既有安全契约（shader 编译时常量 + 输入早退校验）。

---

## 反复模式识别

| 已知模式 | 本次是否重复？ | 说明 |
|---------|-------------|------|
| 计划文件清单与实际变更不一致 | 🟡 轻微 | 文件清单 9/9 完全一致；仅测试矩阵命名漂移（T3→拆分），覆盖未缩水 |
| 子代理产出需大量返工 | ⊘ | 未用子代理 |
| 前置依赖/环境/API 能力未验证 | ✅ 已抑制 | §0.1-0.4 + Image/Rect/HashMap 实证 |
| 非默认路径遗漏验证 | ✅ 已抑制 | Invalid/EmptySrc/EmptyDst/NoGLError/CacheReuse 全覆盖 |
| 测试隔离问题 | ⊘ | 每测独立 surface + MakeCurrent，无串扰 |
| 提交粒度偏离计划 | ✅ 一致 | 4 commit（2 RED + 2 GREEN）严格对应 plan 时间线 |
| TDD 严格度与场景不匹配 | ✅ 一致 | 两轮严格 RED→GREEN |
| **P1#A GL 状态副作用契约** | ✅ **主动预防成功** | G1.8 全屏白 bug 未复发（plan 提前标注重绑）|
| **P1#B 容器 API 名称** | ✅ **主动预防成功** | 1B 前读 header，无编译期返工 |

**正面信号：** 上轮两个 P1 建议（P1#A / P1#B）本次均主动预防成功，无新增反复模式。reflection 闭环有效性获实证。

---

**下一步：** 使用 `/archive` 归档任务
