# 任务跟踪

## 当前任务

| ID | 描述 | 状态 | 复杂度 | 创建日期 |
|----|------|------|--------|---------|
| TASK-20260405-11 | C API 层（对外嵌入接口） | 初始化 | Level 3 | 2026-04-05 |

## 任务详情

### TASK-20260405-11
- **描述**：构建稳定的 C ABI 对外接口层，使宿主应用可通过纯 C 函数嵌入 Veloxa 引擎
- **复杂度**：Level 3（新组件 + 设计决策：C ABI 约定/句柄模型/错误处理/Surface 注入）
- **标签**：[新功能] [对外 API] [里程碑]
- **代码规范**：C99 兼容公共头文件 + Google C++ Style Guide 实现
- **工作流路径**：`/van` → `/plan` → `/build` → `/reflect` → `/archive`

### 范围
1. 公共 C 头文件 `veloxa_api.h` — 不透明句柄、函数声明、错误码、常量
2. C++ 桥接实现 `veloxa_api.cc` — C 函数 → Application 方法转发
3. CMake 库目标 — 静态库 `vx_api`（链接 vx_core + vx_graphics + vx_platform）
4. C 语言调用测试 — 验证 ABI 兼容性
5. 单元测试 + 集成测试

### 依赖分析
- **Application** — C API 直接包装 Application 类的公共方法
- **platform::HeadlessEventLoop / MemorySurface** — C API 需提供创建 headless 后端的辅助函数
- 无新第三方依赖

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
| TASK-20260405-10 | 事件循环与应用壳（EventLoop / Application Shell） | ✅ 已完成 | 2026-04-05 |
