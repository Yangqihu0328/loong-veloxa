# 任务跟踪

## 当前任务

| ID | 描述 | 状态 | 复杂度 | 创建日期 |
|----|------|------|--------|---------|
| TASK-20260405-10 | 事件循环与应用壳（EventLoop / Application Shell） | 初始化 | Level 3 | 2026-04-05 |

## 任务详情

### TASK-20260405-10
- **描述**：构建 Application Shell，将所有已完成模块（DOM/CSS/Layout/Render/Event/Update）串联为可运行的交互式应用运行时
- **复杂度**：Level 3（新组件 + 跨模块编排 + 设计决策：所有权模型/帧调度/API 表面）
- **标签**：[新功能] [核心引擎] [里程碑]
- **代码规范**：Google C++ Style Guide
- **工作流路径**：`/van` → `/plan` → `/creative` → `/build` → `/reflect` → `/archive`

### 范围
1. Application 类 — 持有并连接 Document/Stylesheets/EventManager/UpdateManager/Canvas/Surface/EventLoop
2. LoadHTML/LoadCSS — 解析 HTML/CSS 并初始化渲染管线
3. 帧调度 — EventLoop 定时器驱动帧更新（16ms / 60fps）
4. 输入注入 — 外部 InputEvent → EventManager → UpdateManager 更新管线
5. 生命周期管理 — Init → Run → Quit
6. Headless Demo — 一个可运行的 headless 示例（加载 HTML/CSS → 注入鼠标事件 → 验证渲染结果）
7. 单元测试 + 集成测试

### 依赖分析
- **platform::EventLoop** — 已有纯虚接口 + HeadlessEventLoop 实现（Run/Quit/PostTask/SetTimer/PollOnce）
- **UpdateManager** — 已有 Invalidate/Update 管线
- **EventManager** — 已有 HandleInput + InvalidationCallback
- **platform::Surface** — 已有纯虚接口 + MemorySurface
- **gfx::Canvas** — 已有纯虚接口 + SoftwareCanvas
- **所有解析器** — HtmlTokenizer/HtmlParser/CssTokenizer/CssParser 已完成

### 关联技术债务
- 无直接阻塞项

### 关联待处理事项
- 无直接关联

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
| TASK-20260405-09 | 脏区更新与重绘机制 | ✅ 已完成 | 2026-04-05 |
