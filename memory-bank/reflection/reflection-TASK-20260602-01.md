# 回顾：GLES 图像采样过滤选项 NEAREST/LINEAR

**日期：** 2026-06-02
**任务 ID：** TASK-20260602-01
**复杂度级别：** Level 2（G1.9 技术债 #2 清理）

---

## 计划 vs 实际

| 维度 | 计划 | 实际 | 偏差原因 |
|------|------|------|---------|
| 文件变更 | 4（`types.h` / `gles_canvas.{h,cc}` / 测试）；pool + CMake 零改动 | 4，pool + CMake 零改动 | **完全一致** |
| 测试数 | +5（F1-F5）| +5（F1-F5），全部按 plan 命名 | 一致 |
| gles ctest | 1432 → ~1437 | 1432 → **1437** | 精确命中 |
| software / no-devtool | 1303 / 1141（0 退化）| 1303 / 1141 | 零退化 |
| TDD 轮次 | 单轮 RED→GREEN→C | 单轮，RED 编译失败 → GREEN 13/13 一次过 | 一致 |
| 估时（plan ×0.6）| ~30-36 min / 预期实测 ~20-25 min | 约 12 min（23:41→23:53）含三矩阵 | 范式复用 + 4 文件小改 + 现有 fixture 直接扩展 |
| debug 迭代 | — | **0 次** | 设计简单 + P1#A 已内化 |

---

## 做得好的

1. **设计决策前置消化彻底** — D1-D5 在 `/plan` 头脑风暴一次锁定，尤其 D1（GLES 局部 setter vs 抽象 API vs 改签名）选择最小侵入方案，使 `ImageTexturePool` + CMake + software + renderer 全部零改动，blast radius 收到最小。
2. **D2 决策（filter 不入缓存键、每 draw 设 `glTexParameteri`）一次写对** — 同一缓存纹理在 NEAREST/LINEAR 间切换正确，`DrawImage_FilterSwitch_NoGLError` 验证。避免了「缓存键纳入 filter」的复杂化。
3. **测试区分手法干净** — 2×1 红蓝图放大 64× 的「接缝紫色存在性」判别，把 NEAREST（硬边无紫）vs LINEAR（混合出紫）区分得清晰可断言，承接 P1#1（接缝 x≈32 解析推导）+ P1#2（红/蓝/紫双通道）。一次通过无阈值返工。
4. **正反向探针成对** — F2（默认 LINEAR 保留=有紫）与 F3（NEAREST=无紫）互为反向，既验证新功能又防止默认行为回归。
5. **计划精度极高** — 文件清单 4/4、测试 5/5、gles 1437 全部精确命中，零偏差。

---

## 遇到的挑战

1. **几乎无挑战** — Level 2 小任务，设计简单、范式成熟（复用 G1.9 测试 fixture + P1#A 重绑契约）。最大不确定性是 Mesa swrast 的 NEAREST/MAG 行为是否符合预期，但 F2/F3 早测即确认正常。
2. **progress.md 编辑工具一次 fuzzy-match 失配** — finalize 时 StrReplace 报「未找到」但 fuzzy 显示目标已含新内容（疑似工具重试已应用）。读取确认后无害，但提示编辑大文件时应先读取确认状态再补差异。

---

## 经验教训

1. **技术债清理任务的「最小侵入」优先原则有效** — D1 选 GLES 局部 setter 而非提升抽象 Canvas API，既清了债又零回归。技术债清理不必追求「一步到位的完美架构」，enum 入 `types.h` 留提升门即可，符合复杂度缩减原则。
2. **小任务也值得完整 `/plan` 头脑风暴** — 看似简单的「加个采样选项」实含真实接口设计决策（旋钮归属 + 跨后端语义）。提前锁定 D1-D5 让 build 阶段零返工、零 debug。
3. **跨后端语义分歧需显式文档化** — software 是 NEAREST-only、GLES 默认 LINEAR，两后端默认行为本就不一致（D3）。本任务文档化此差异并记 techContext，避免未来误判为 bug。

---

## 改进建议（附优先级与落实方式）

| # | 建议 | 优先级 | 落实方式 | 目标 |
|---|------|--------|---------|------|
| 1 | 跨后端采样语义分歧（software NEAREST-only vs GLES 可选，默认 LINEAR）需统一——同步给 software 加双线性，或在更高层文档化「后端渲染质量差异矩阵」 | P2 | 记 `techContext.md` 技术债（G2 候选）| 长期 |
| 2 | 编辑大 Memory Bank 文件（progress.md 等）前先 Read 确认当前状态再补差异，避免 StrReplace fuzzy 失配困惑 | P2 | 个人习惯 / `/build` finalize 段 | 长期 |

无 P0/P1 建议——本任务无影响当前工作流正确性或需下次同类任务立即处理的问题。

---

## 技术改进建议

- **软件后端双线性采样** — software `DrawImage` 当前整数截断 = NEAREST-only，无法匹配 GLES 的 LINEAR。补齐双线性插值是独立 Level 3 任务（涉及 4 邻域采样 + 边界 clamp + alpha 混合）。
- **filter 纳入 PushState/PopState** — 当前 `image_filter_` 不随 state stack 保存（D5 MVP 取舍）。若未来 transform/clip 状态化扩展，可一并纳入 State 结构。
- **GL sampler object** — 当前每 draw 设 `glTexParameteri` 改纹理对象状态；多图高频场景可考虑 `glGenSamplers` 分离采样器状态（YAGNI，G2 性能优化时评估）。

---

## 安全评估

| 维度 | 状态 | 备注 |
|------|------|------|
| 输入验证 | N/A | filter 为内部 enum，无外部输入 |
| 认证/授权 | N/A | 无 |
| 数据保护 | N/A | 无敏感数据 |
| 依赖审计 | ✅ | **0 新依赖** |
| 错误信息脱敏 | N/A | 无对外错误输出 |
| 敏感数据处理 | N/A | 无 |

**本任务不涉及安全变更**（纯 GL 采样状态 / 无 GLSL 拼接 / enum 内部传递）。

---

## 反复模式识别

| 已知模式 | 本次是否重复？ | 说明 |
|---------|-------------|------|
| 计划文件清单与实际变更不一致 | ✅ 已抑制 | 文件 4/4 + 测试 5/5 精确命中 |
| 子代理产出需大量返工 | ⊘ | 未用子代理 |
| 前置依赖/环境/API 能力未验证 | ✅ 已抑制 | Canvas/types/pool/software 采样 + Mesa NEAREST 行为均实证 |
| 非默认路径遗漏验证 | ✅ 已抑制 | 默认 LINEAR（F2）+ filter 切换（F5）+ NEAREST/LINEAR 各覆盖 |
| 测试隔离问题 | ⊘ | 每测独立 surface + MakeCurrent |
| 提交粒度偏离计划 | ✅ 一致 | van/plan/RED/GREEN 4 commit 严格对应 |
| TDD 严格度与场景不匹配 | ✅ 一致 | 单轮严格 RED（编译失败）→ GREEN |

**无新增反复模式。** 本任务为低风险技术债清理，所有已知反复模式均抑制到位。

---

**下一步：** 使用 `/archive` 归档任务
