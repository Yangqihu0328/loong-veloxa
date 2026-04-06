# 活跃上下文

## 当前阶段
构建中

## 当前任务
- **ID**：TASK-20260405-09
- **描述**：脏区更新与重绘机制
- **复杂度**：Level 3
- **代码规范**：Google C++ Style Guide

## 范围（更新管线子系统）
1. 样式失效（Style Invalidation）— EventManager 状态变更时标记受影响元素
2. 脏区追踪（Dirty Region Tracking）— DisplayList 逐项对比计算脏区
3. 更新调度（Update Scheduling）— UpdateManager 编排全量 Restyle → Relayout → DirtyRect → Repaint
4. CSS 伪类透传修复 — StyleResolver → SelectorMatcher 透传 EventManager*
5. Arena 外部化 — LayoutEngine::Layout 接受外部 ArenaAllocator

## 设计文档
- **设计规格**：`docs/specs/2026-04-05-dirty-repaint-design.md`
- **实现计划**：`docs/plans/2026-04-05-dirty-repaint.md`

## 实现计划概要
- **Phase 1**：API 管线扩展（LayoutContext + StyleResolver + LayoutEngine + EventManager）
- **Phase 2**：UpdateManager + DirtyRect（新模块）
- **Phase 3**：全管线集成测试
- **预估新增测试**：~30 个

## 不需要创意阶段
设计决策均已在规划中确定（Push 回调 / 全量重建 / DisplayList 对比），无需 /creative。

## 待处理事项
- **P1**：补充 Benchmark（网络恢复后，来源 TASK-01）
- **P1**：子代理 prompt 模板增加「跨模块数据格式」段（来源 TASK-02）— 已验证有效
- **P1**：集成测试优先验证数据格式一致性（来源 TASK-02）
- **P1**：存根文件预创建策略固化到子代理开发规则（来源 TASK-04）
- **P1**：合并 Phase 给子代理的策略固化到计划模板（来源 TASK-04）
- **P1**：计划模板增加「边界输入清单」段——每个 Phase 列出非默认路径（来源 TASK-06，反复出现）
- **P1**：集成测试必须使用真实 HTML/CSS 解析器，禁止仅用手动 DOM 构建（来源 TASK-06，反复出现）
- **P1**：子代理 prompt 涉及 LayoutBox 坐标计算时须包含 x/y 语义定义（content origin vs border box origin）（来源 TASK-07）
- **P1**：集成测试像素验证优先用 DisplayList 检查和区域扫描，避免硬编码坐标（来源 TASK-07）
- **P1**：CSS 颜色测试禁止与 gfx::Color 编程常量直接比较，必须通过 CssColorToGfx 转换（来源 TASK-07）
- **P1**：集成测试禁止使用 HTML inline style（BuildTree 不解析），必须用外部 CSS 选择器（来源 TASK-08，API 能力假设错误第三次出现）
- **P1**：并行子代理可行条件：无共享 .cc + 共享 .h 已创建 + CMakeLists.txt 已更新（来源 TASK-08）
