# 活跃上下文

## 当前阶段
回顾中

## 当前任务
- **ID**：TASK-20260405-10
- **描述**：事件循环与应用壳（EventLoop / Application Shell）
- **复杂度**：Level 3
- **代码规范**：Google C++ Style Guide

## 范围（Application Shell）
1. Application 类 — 持有并连接 Document/Stylesheets/EventManager/UpdateManager/Canvas/Surface
2. LoadHTML/LoadCSS — 解析 HTML/CSS 并初始化渲染管线
3. 帧调度 — EventLoop 定时器驱动帧更新
4. 输入注入 — InjectInput → EventManager → UpdateManager
5. 生命周期 — Run/Quit
6. 单元测试 + 集成测试

## 设计文档
- **设计规格**：`docs/specs/2026-04-05-app-shell-design.md`
- **实现计划**：`docs/plans/2026-04-05-app-shell.md`

## 实现计划概要
- **Phase 1**：Application 类核心（头文件 + 实现 + 单元测试）
- **Phase 2**：集成测试（全管线交互验证）
- **预估新增测试**：~16 个

## 不需要创意阶段
3 个设计决策均有明确最优选项（核心拥有+外部注入 / 按需帧+心跳 / 持久 Lock），无需 /creative。

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
- **P1**：跨模块参数透传修改时，计划模板增加「调用链端到端验证」段（来源 TASK-09）
