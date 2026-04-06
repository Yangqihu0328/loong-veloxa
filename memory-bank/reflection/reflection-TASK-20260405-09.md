# 回顾：脏区更新与重绘机制

**日期：** 2026-04-05
**任务 ID：** TASK-20260405-09
**复杂度级别：** Level 3

## 计划 vs 实际

| 维度 | 计划 | 实际 | 偏差原因 |
|------|------|------|---------|
| Phase 数 | 3 | 3 | 一致 |
| 新增测试 | ~30 | 29 | 几乎一致 |
| 新建文件 | 4 | 4 | 一致 |
| 修改文件 | 13 | 13 | 一致 |
| 子代理 | 0 | 0 | 一致（各 Phase 间有依赖，无并行机会） |
| 设计变更 | — | 无 | 设计方案全部按原方案执行 |
| 需要 /creative | 否 | 否 | 设计决策空间有限，/plan 中已充分确定 |
| 提交数 | 按 Phase | 2（实现+MB） | 批量提交，但因无子代理+变更紧密耦合，合理 |

## 回顾检查清单

- [x] 计划精确度 — 文件清单与实际完全一致，预估测试数接近
- [x] TDD 执行情况 — Phase 1 先修改 API 再验证现有测试通过（变体：向后兼容验证），Phase 2/3 先写测试后验证
- [x] 测试隔离 — 所有测试独立，无串扰
- [ ] 提交粒度 — 计划按 Phase 提交，实际合并为 1 个功能提交（规模适中，可接受）
- [x] 非默认路径 — 覆盖了空列表、不同长度列表、多项差异等非默认路径

## 做得好的

1. **代码审查阶段发现关键缺口**：在 /plan 阶段读取 `style_resolver.cc` 时发现 `CollectMatchingRules` 未传 `EventManager*` 给 `SelectorMatcher::Matches`。这是一个 TASK-08 遗留的集成断点（TASK-08 只修改了 SelectorMatcher 的签名，但未更新 StyleResolver 的调用链）。在规划阶段发现比在构建阶段发现节省了大量调试时间。

2. **跳过 /creative 的判断正确**：3 个设计决策（Push 回调 / 全量重建 / DisplayList 对比）每个都有明确的最优选项，HMI 场景约束使替代方案不合理。跳过 /creative 节省了一个交互轮次。

3. **向后兼容策略一致性**：所有 API 扩展均使用默认参数（`= nullptr`），现有 702 个测试零修改通过。这是从 TASK-08 学到的"默认参数向后兼容注入模式"的第二次成功应用。

4. **DisplayList 对比脏区策略简洁有效**：利用已有的 `PaintCommand` 结构和 `gfx::Rect`/`gfx::Color` 的 `operator==`，仅 ~15 行代码实现了完整的脏区计算，覆盖了结构不变（hover/active/focus 变色）和结构变化（display:none 回退全屏）两种场景。

5. **构建零返工**：Phase 1 的 SoftwareCanvas 构造方式修正是唯一的小调整（从 Surface* 改为 raw pixels），其余代码一次编译通过。

## 遇到的挑战

1. **SoftwareCanvas 构造 API 记忆偏差**：在 `update_manager_test.cc` 中错误使用了 `std::make_unique<gfx::SoftwareCanvas>(surface_.get())` 构造方式，实际 SoftwareCanvas 接受 `(u32* pixels, u32 width, u32 height, u32 stride)`。这是对 API 构造方式的假设错误，与 TASK-08 的 inline style 假设属于同类问题（API 能力假设错误）。

2. **LayoutEngine static ArenaAllocator 处理**：需要保留原有 `Layout(doc, ctx)` 签名（使用 static arena）同时新增 `Layout(doc, ctx, arena)` 重载。通过让旧签名调用新签名实现代码复用，但这使得 `static ArenaAllocator` 的线程安全问题仍然存在于旧调用者中（tech debt #19 部分缓解但未完全解决）。

## 经验教训

1. **跨模块 API 透传链必须在规划阶段端到端验证**：TASK-08 修改了 SelectorMatcher 但未更新 StyleResolver 的调用链，导致伪类在 restyle 中失效。教训：当在模块 A 添加一个新参数时，必须追溯所有调用者（A 的上游 B、B 的上游 C...）确认参数是否需要透传。本次在 /plan 阶段发现此问题，避免了构建阶段的调试。

2. **Level 3 无设计分歧时可合并 /plan 和 /creative**：本任务的 3 个设计决策都有明确最优解（被 HMI 场景约束决定），/creative 步骤无附加价值。未来 Level 3 任务可在 /plan 中评估是否需要 /creative，而非总是要求。

3. **构造函数签名是高频 API 假设错误点**：Canvas、Surface 等图形对象的构造方式多样（指针 vs 引用、raw vs smart pointer），应在写测试前查阅头文件确认。

## 改进建议

| # | 建议 | 优先级 | 落实方式 | 目标 |
|---|------|--------|---------|------|
| 1 | 跨模块参数透传修改时，计划模板增加「调用链端到端验证」段 | P1 | 改计划模板 | 避免 TASK-08 遗留断链问题 |
| 2 | 计划中涉及 Canvas/Surface 构造的测试代码须引用现有测试的构造方式 | P2 | 记 techContext | 减少 API 构造假设错误 |

## 技术改进建议

| # | 描述 | 优先级 |
|---|------|--------|
| 1 | UpdateManager::Update 中 canvas 操作（PushClipRect + Clear + Replay + PopClip）可提取为 RepaintDirtyRegion 辅助方法 | P2 |
| 2 | UpdateManager 可增加 OnBeforeUpdate/OnAfterUpdate 钩子，供宿主应用监听帧生命周期 | P2 |
| 3 | ComputeDirtyRect 的 DisplayList 对比可增加 PaintCommand::BoundsRect() 方法以统一 rect 语义 | P2 |
| 4 | LayoutEngine::Layout 旧签名（static ArenaAllocator）仍线程不安全，应逐步迁移所有调用者到新签名 | P2 |

## 安全评估

本任务不涉及安全变更。
