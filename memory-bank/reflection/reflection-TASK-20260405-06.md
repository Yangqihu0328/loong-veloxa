# 回顾：Layout Engine 布局引擎

**日期：** 2026-04-05
**任务 ID：** TASK-20260405-06
**复杂度级别：** Level 4

## 计划 vs 实际

| 维度 | 计划 | 实际 | 偏差原因 |
|------|------|------|---------|
| Phase 数 | 6 | 6 | 无偏差 |
| 新增文件 | ~17 | 16 | layout_box.cc 合并为 header-only（方法简单，内联更合理） |
| 新增测试 | ~66 | 76 | 子代理产出略多于预估；集成测试 8 个符合预期 |
| 修改文件 | 2（CMakeLists ×2） | 3（+layout_engine.h 移除 LayoutFlex 声明） | 发现 LayoutFlex 作为私有静态方法与 flex_layout.h 自由函数冲突 |
| 预估时间 | ~85 min | 构建阶段约 40 min | 子代理并行大幅加速；Phase 1 手工 + B/C 并行 |
| 设计变更 | — | 2 项运行时修复 | 空白文本节点过滤 + kText 节点文本测量 |
| 提交次数 | 计划 6（每 Phase 1 次） | 4（P1、P2-5 合并、P6+修复、MB） | Phase 2-5 由并行子代理完成后统一提交 |
| 子代理数 | 计划 B/C/D 3 个并行 | 实际 2 个并行（B=P2+3+5 合并，C=P4） | Phase 5 与 Phase 2+3 共享 layout_engine.cc，无法独立并行 |
| 代码行数 | 未预估 | 2860 行（含测试） | — |

## 回顾检查清单

- [x] **计划精确度** — 文件清单与实际高度一致（16/17），预估测试偏保守
- [x] **TDD 执行情况** — Phase 1 严格 RED→GREEN；Phase 2-5 由子代理批量实现+测试（覆盖补充模式）；Phase 6 先写测试后修 bug（集成 TDD）
- [x] **子代理质量** — 2 个子代理均零编译错误零返工，prompt 精确度高
- [x] **测试隔离** — 无测试间串扰；static ArenaAllocator 在测试间 Reset，但存在理论上的串扰风险
- [x] **提交粒度** — Phase 2-5 合并为一次提交（偏离计划的逐 Phase 提交），原因是并行子代理产出后统一验证
- [x] **非默认路径** — 空白文本节点和 Block 内文本测量两个非默认路径被集成测试捕获

## 做得好的

1. **Phase 1 手工实现 → API 签名精确**：自己实现基础数据结构，确保 LayoutBox/LayoutContext/ResolveLength 等 API 签名完全正确，子代理直接依赖这些签名零返工
2. **子代理合并 Phase 策略再次验证**：将 Phase 2+3+5 合并为一个子代理（共享 layout_engine.cc）避免了文件冲突，这是从 TASK-04 学到的模式
3. **存根文件预创建**：Phase 1 创建所有 .cc 存根和 CMake 目标，后续 Phase 无 CMake 配置问题
4. **集成测试发现真正 bug**：8 个集成测试暴露了 2 个单元测试无法覆盖的问题（空白文本节点 + Block 内文本测量），验证了集成测试的不可替代性
5. **跳过 `/creative` 阶段正确**：所有布局算法由 CSS 规范定义，无需额外设计决策
6. **架构与 Phase 1 一致**：扁平 struct LayoutBox + 独立布局树 + TextShaper 接口的设计在整个实现过程中保持稳定，无需返工

## 遇到的挑战

1. **空白文本节点干扰布局**：HTML 中的缩进和换行符被解析为 Text 节点，BuildTree 将它们转为 kText LayoutBox，导致 Flex 容器有 7 个子元素（应为 3 个）。这是单元测试（手动构建 DOM）和集成测试（HTML 解析器构建 DOM）之间的鸿沟
2. **Block 内 kText 节点不被测量**：LayoutChild 对 kText 类型直接跳过，导致 `<p>Hello</p>` 的自动高度为 0。计划中未明确 Block 布局如何处理文本子节点
3. **LayoutFlex 声明冲突**：layout_engine.h 将 LayoutFlex 声明为私有静态方法，但 flex_layout.h 声明为自由函数。需在 Phase 1 阶段修正 header（移除私有声明）
4. **Phase 2+3+5 与 Phase 4+5 的并行粒度**：原计划 B/C/D 三组并行，但 D（Phase 5）与 B（Phase 2+3）共享 layout_engine.cc 无法并行，需合并为 2 组

## 经验教训

1. **集成测试必须使用真实解析器**：单元测试中手动构建 DOM 树不会产生空白文本节点，真实 HTML 解析一定会。今后所有新布局功能必须有 HTML→DOM→CSS→Layout 全管线集成测试
2. **计划应明确标注"共享文件冲突"**：当多个 Phase 修改同一文件时，计划中应标注 [共享文件]，子代理分组时自动合并
3. **非默认路径应在计划中枚举**：空白文本节点是典型的非默认输入路径，计划中应有一个 "边界情况" 清单
4. **static local 变量引入线程安全风险**：`Layout()` 中的 `static ArenaAllocator` 是简化方案，后续多线程场景需重构

## 反复模式识别

| 已知模式 | 出现频率 | 本次是否重复？ | 说明 |
|---------|---------|-------------|------|
| 计划文件清单与实际变更不一致 | 9+ 次 | 轻微 | layout_engine.h 修改未在计划中（移除 LayoutFlex 声明） |
| 子代理产出需大量返工 | 7+ 次 | **否** | 2 个子代理均零返工 |
| 前置依赖/环境/API 能力未验证 | 8+ 次 | **是** | 未验证 HTML 解析器对空白文本的行为，导致集成测试失败 |
| 非默认路径遗漏验证 | 4+ 次 | **是（反复）** | 空白文本节点 + Block 内 kText 测量均为非默认路径 |
| 测试隔离问题 | 7+ 次 | 潜在 | static ArenaAllocator 跨测试共享，当前无问题但有风险 |
| 提交粒度偏离计划 | 7+ 次 | 是 | Phase 2-5 合并为一次提交 |
| TDD 严格度与场景不匹配 | 11+ 次 | 轻微 | 子代理采用批量实现+测试而非严格 RED→GREEN |

## 改进建议（附优先级与落实方式）

| # | 建议 | 优先级 | 落实方式 | 目标 |
|---|------|--------|---------|------|
| 1 | 计划模板增加「边界输入清单」段：每个 Phase 列出 2-3 个非默认输入路径 | P1 | 修改 `/plan` 模板 | writing-plans.mdc |
| 2 | 集成测试必须使用真实 HTML/CSS 解析器，禁止仅用手动 DOM 构建 | P1 | 加到 verification.mdc checklist | verification.mdc |
| 3 | 计划中标注 [共享文件] 并自动约束子代理分组 | P2 | 补充到子代理开发规则 | subagent-development.mdc |
| 4 | `LayoutEngine::Layout` 的 static ArenaAllocator 重构为调用者持有 | P2 | 记技术债 | techContext.md |
| 5 | 补充 margin collapsing（Block 布局规范要求） | P2 | 记技术债 | techContext.md |
| 6 | LayoutInline 需要真正的 line box 模型（当前简化） | P2 | 记技术债 | techContext.md |

## 技术改进建议

1. **static ArenaAllocator → 调用者持有**：当前 `Layout()` 使用 `static ArenaAllocator`，线程不安全且调用者无法控制生命周期。应改为 `Layout(doc, ctx, arena)` 或返回 `LayoutResult` 持有 arena
2. **Block margin collapsing**：当前 Block 布局不实现相邻 margin 折叠，与 CSS 规范不符。这是后续渲染正确性的前提
3. **Inline line box 模型**：当前 LayoutInline 是简化的水平排列+换行，缺少真正的 line box 高度计算（应基于最大 ascent+descent）
4. **ComputedStyle 析构**：Arena 分配的 ComputedStyle 包含 InternedString 成员（有引用计数），当前 arena 批量释放不会调用析构函数，可能导致 InternedString 引用泄露
5. **LayoutBox 树遍历 API**：当前通过 first_child/next_sibling 手动遍历，可考虑提供迭代器或 visitor 模式简化遍历代码

## 安全评估

本任务不涉及安全变更（无外部输入处理、无认证/授权、无敏感数据）。

| 维度 | 状态 | 备注 |
|------|------|------|
| 输入验证 | N/A | 布局引擎接收已解析的 DOM/CSS 数据 |
| 认证/授权 | N/A | — |
| 数据保护 | N/A | — |
| 依赖审计 | N/A | 无新外部依赖 |
| 错误信息脱敏 | N/A | — |
| 敏感数据处理 | N/A | — |
