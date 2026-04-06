# 任务跟踪

## 当前任务

| ID | 描述 | 状态 | 复杂度 | 创建日期 |
|----|------|------|--------|---------|
| TASK-20260405-09 | 脏区更新与重绘机制 | 回顾中 | Level 3 | 2026-04-05 |

## 任务详情

### TASK-20260405-09
- **描述**：构建脏区更新与重绘机制，实现从事件状态变更到样式失效、增量重布局、脏区追踪、增量重绘的完整更新管线
- **复杂度**：Level 3（新组件 + 跨模块编排 Event/CSS/Layout/Render，需设计决策）
- **标签**：[新功能] [核心引擎]
- **代码规范**：Google C++ Style Guide
- **工作流路径**：`/van` → `/plan` → `/creative` → `/build` → `/reflect` → `/archive`

### 范围
1. 样式失效（Style Invalidation）— EventManager 状态变更时标记受影响元素
2. 脏区追踪（Dirty Region Tracking）— 记录需要重绘的屏幕区域
3. 更新调度（Update Scheduling）— 统一编排 Invalidate → Restyle → Relayout → Repaint 管线
4. 增量重绘（Incremental Repaint）— 只重绘脏区域而非全屏
5. 与 EventLoop 集成 — 调度 rAF 式帧同步更新
6. 单元测试 + 集成测试

### 依赖分析
- **Event**：EventManager 需要在状态变更时发出失效信号（当前仅更新指针）
- **CSS**：StyleResolver 可能需要增量重解析能力（或先全量重解析）
- **Layout**：LayoutEngine 当前全量重布局，可保持不变（HMI 场景树小）
- **Render**：Renderer 需要支持脏区裁剪重绘

### 关联技术债务
- P2 #29：EventManager 无「状态变更→样式失效→重绘」触发机制（本任务直接解决）
- P2 #19：LayoutEngine::Layout 的 static ArenaAllocator 线程不安全（相关但不阻塞）

## 任务历史

| ID | 描述 | 最终状态 | 归档日期 |
|----|------|---------|---------|
| TASK-20260405-01 | 构建 Foundation 基础库（内存管理/容器/字符串/日志） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-02 | 构建 Graphics HAL 图形抽象层与 Platform HAL 平台抽象层 | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-03 | 构建 DOM 树 + HTML 解析器 | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-04 | 构建 CSS 引擎（Tokenizer/Parser/选择器/属性/层叠） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-05 | 消化技术债务（Arena/HashMap/PPM/Parser Error） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-06 | 构建 Layout Engine 布局引擎 | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-07 | 构建渲染管线（Render Pipeline） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-08 | 构建事件系统（Event System） | ✅ 已完成 | 2026-04-05 |
