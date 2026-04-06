# 任务跟踪

## 当前任务

| ID | 描述 | 状态 | 复杂度 | 创建日期 |
|----|------|------|--------|---------|
| TASK-20260405-08 | 构建事件系统（Event System） | 初始化 | Level 4 | 2026-04-05 |

## 任务详情

### TASK-20260405-08
- **描述**：构建事件系统，实现输入事件定义、Hit-Testing、DOM 事件捕获/冒泡分发、元素交互状态（:hover/:active/:focus）管理
- **复杂度**：Level 4（跨 DOM/CSS/Layout/Platform 四个子系统，含架构决策）
- **标签**：[新功能] [核心引擎] [里程碑]
- **代码规范**：Google C++ Style Guide
- **工作流路径**：`/van` → `/plan` → `/creative` → `/build` → `/reflect` → `/archive`

### 范围
1. 输入事件类型定义（InputEvent：Touch/Pointer/Keyboard）
2. Hit-Testing——给定 (x,y) 坐标，在 LayoutBox 树中找到目标元素
3. DOM 事件分发模型——Capture（Sinking）→ Target → Bubble
4. 元素交互状态管理——:hover / :active / :focus 状态跟踪
5. CSS 伪类回填——连接 SelectorMatcher 到元素事件状态
6. 平台输入抽象——扩展或新增输入事件源接口
7. 单元测试 + 集成测试

### 依赖分析
- **DOM**：Element 需要存储/查询交互状态（hovered/active/focused）
- **CSS**：SelectorMatcher 的 :hover/:active/:focus 分支需连接到实际状态
- **Layout**：Hit-Testing 需要遍历 LayoutBox 树（坐标 → 元素映射）
- **Platform**：EventLoop 需要接收并分发输入事件

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
