# 任务跟踪

## 当前任务

| 字段 | 内容 |
|------|------|
| **ID** | TASK-20260413-02 |
| **描述** | 消化技术债务子集：**#41** `HashMap` `const_iterator` / `cbegin` / `cend`；**#39** `tests/test_pixel_utils.h` + 2 处测试迁移；派生移除 `TransitionManager::active_count_`（依赖 #41）。 |
| **复杂度** | Level 2 — 简单增强（范围已锁定，见计划） |
| **安全** | 本迭代不涉及 `[安全相关]` 新增面 |
| **状态** | 规划中 |
| **创建日期** | 2026-04-13 |

### 设计 / 实现文档

| 类型 | 路径 |
|------|------|
| 设计规格 | `docs/specs/2026-04-13-tech-debt-subset-design.md` |
| 实现计划 | `docs/plans/2026-04-13-tech-debt-subset.md` |

### 需要创意阶段（`/creative`）的组件

无。

## 任务历史

| ID | 描述 | 最终状态 | 归档日期 |
|----|------|---------|---------|
| TASK-20260413-01 | QuickJS 脚本引擎集成（quickjs-ng、`vx_script`、`QuickjsEngine`） | ✅ 已完成 | 2026-04-13 |
| TASK-20260405-01 | 构建 Foundation 基础库（内存管理/容器/字符串/日志） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-02 | 构建 Graphics HAL 图形抽象层与 Platform HAL 平台抽象层 | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-03 | 构建 DOM 树 + HTML 解析器 | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-04 | 构建 CSS 引擎（Tokenizer/Parser/选择器/属性/层叠） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-05 | 消化技术债务（Arena/HashMap/PPM/Parser Error） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-06 | 构建 Layout Engine 布局引擎 | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-07 | 构建渲染管线（Render Pipeline） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-08 | 构建事件系统（Event System） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-09 | 脏区更新与重绘机制 | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-10 | 事件循环与应用壳（EventLoop / Application Shell） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-11 | C API 层（对外嵌入接口） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-12 | 示例应用（examples/hello） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-13 | CSS 动画系统（CSS Transitions） | ✅ 已完成 | 2026-04-05 |
