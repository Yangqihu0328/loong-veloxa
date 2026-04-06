# 回顾：事件系统（Event System）

**日期：** 2026-04-05
**任务 ID：** TASK-20260405-08
**复杂度级别：** Level 4

## 计划 vs 实际

| 维度 | 计划 | 实际 | 偏差原因 |
|------|------|------|---------|
| Phase 数 | 4 | 4 | 无偏差 |
| 测试数 | ~52 | 52（8+12+10+14+8） | 一致 |
| 新增文件 | 12（7 源 + 5 测试） | 12 | 一致 |
| 修改文件 | 4 | 4 | 一致 |
| 子代理 | 1 个（Phase 2） | 2 个并行（Phase 2） | 优化：拆分为 HitTest + EventDispatcher 两个并行子代理 |
| 提交数 | 每 Phase 一次 | 1 次整体 | Phase 间强依赖，单次提交更合理 |
| 集成测试失败 | 0 | 7（已修复） | inline style 不被 BuildTree 解析 |

## 回顾检查清单

- [x] 计划精确度 — 文件清单完全一致，测试数精确命中
- [x] TDD 执行 — Phase 1 先测试后 stub；Phase 2 子代理交付代码+测试；Phase 3 先写测试后实现；Phase 4 集成测试发现问题后修复
- [x] 子代理质量 — Phase 2 两个子代理均零返工
- [x] 测试隔离 — 无串扰，全部可独立运行
- [x] 提交粒度 — 单次提交（合理偏差，同 TASK-07 模式）
- [x] 非默认路径 — 覆盖了 null root、空白区域 miss、visibility hidden、overflow hidden 裁剪、z-index 排序、text 节点回退、keyboard 无 focused、事件类型不匹配、监听器移除后不触发

## 做得好的

1. **并行子代理首次验证**：Phase 2 将 HitTest 和 EventDispatcher 拆分为两个并行子代理，两者无共享 .cc 文件，零冲突零返工。相比串行子代理节省了等待时间。

2. **SelectorMatcher 向后兼容 API 设计**：通过添加默认参数 `const event::EventManager* em = nullptr`，所有现有调用（17 个 SelectorMatcher 测试 + StyleResolver）无需任何修改即可编译通过。

3. **中央 EventManager 指针方案验证**：仅 3 个指针（hovered_/active_/focused_），不侵入 Element 类，IsHovered 祖先链遍历在集成测试中正确工作。

4. **计划精确度高**：文件清单、测试数、Phase 分解全部与实际一致。Creative + Plan 合并策略第二次验证有效。

## 遇到的挑战

1. **集成测试 inline style 陷阱**（P1 级别）：初始集成测试使用 HTML inline style（`<div style="width:100px">`），但 `LayoutEngine::BuildTree` 调用 `StyleResolver::Resolve` 时未传入 `inline_decls` 参数（使用默认 nullptr），导致 inline style 不生效。所有 HitTest 目标元素尺寸为 0，hit-test 全部返回 nullptr。
   - **根因**：对 LayoutEngine 的 inline style 处理能力做了错误假设。
   - **修复**：改用外部 CSS 选择器（`#d { width: 100px; height: 100px; }`），与 render integration test 模式一致。
   - **教训**：集成测试应参考同项目已有的集成测试模式，不应引入新的 DOM 构建方式。

## 经验教训

1. **inline style 在 Veloxa 中不可用于集成测试**：BuildTree 不解析 inline style 属性，所有集成测试必须使用外部 CSS 选择器定义样式。这是一个项目级约束，应记录到 techContext.md。

2. **并行子代理可行条件**：两个子代理可并行执行的充分条件是——(1) 无共享 .cc 文件，(2) 共享的 .h 文件在 Phase 1 已创建，(3) CMakeLists.txt 在 Phase 1 已更新。

3. **默认参数向后兼容模式**：当需要为已有接口注入新依赖时，使用 `= nullptr` 默认参数是最小侵入方案。现有 17 个 SelectorMatcher 测试零修改通过。

## 改进建议

| # | 建议 | 优先级 | 落实方式 | 目标 |
|---|------|--------|---------|------|
| 1 | 集成测试禁止使用 HTML inline style（BuildTree 不解析），必须用外部 CSS 选择器 | P1 | 加到 activeContext.md 待处理事项 + techContext.md | 集成测试约束 |
| 2 | 并行子代理的可行条件：无共享 .cc + 共享 .h 已创建 + CMakeLists.txt 已更新 | P1 | 加到子代理开发规则 | 子代理策略 |
| 3 | LayoutEngine 支持 inline style 解析 | P2 | 技术债 | layout_engine.cc |
| 4 | EventManager 暂无「样式失效」触发机制 | P2 | 技术债 | 事件→样式重算链路 |
| 5 | EventDispatcher 使用 std::function 作为 handler，嵌入式场景可能需替换为轻量回调 | P2 | 技术债 | event_dispatcher.h |

## 反复模式识别

| 已知模式 | 本次是否重复？ | 说明 |
|---------|-------------|------|
| 计划文件清单与实际变更不一致 | ❌ | 完全一致 |
| 子代理产出需大量返工 | ❌ | 零返工 |
| 前置依赖/环境/API 能力未验证 | ✅ | inline style 能力未验证（第三次出现 API 假设错误） |
| 非默认路径遗漏验证 | ❌ | 边界清单覆盖全面 |
| 测试隔离问题 | ❌ | 无串扰 |
| 提交粒度偏离计划 | ✅ | 单次提交（合理偏差，与 TASK-07 一致） |
| TDD 严格度与场景不匹配 | ❌ | Phase 1 严格 TDD，其余 test-with-code |

**反复模式处理**：
- **"API 能力未验证"第三次出现**（TASK-02 像素格式、TASK-07 border box 坐标、TASK-08 inline style）→ 已升级为 P1，计划中必须验证使用的 API 能力。

## 技术改进建议

1. LayoutEngine 应支持 inline style 解析（BuildTree 调用 StyleResolver 时解析 element 的 style 属性为 inline_decls）
2. EventManager 需增加「状态变更 → 样式失效 → 重新布局/渲染」的触发机制（当前仅更新指针，无自动重绘）
3. EventDispatcher::listeners_ 使用 HashMap<Element*, Vector<EventListener>>，元素被销毁时需手动调用 RemoveEventListeners，缺少自动清理
4. HitTest 的 z-index 排序每次调用都重新排序，可缓存

## 安全评估

本任务不涉及安全变更。

| 维度 | 状态 | 备注 |
|------|------|------|
| 输入验证 | N/A | 事件系统不处理外部网络输入 |
| 认证/授权 | N/A | |
| 数据保护 | N/A | |
| 依赖审计 | N/A | 无新增外部依赖 |
| 错误信息脱敏 | N/A | |
| 敏感数据处理 | N/A | |
