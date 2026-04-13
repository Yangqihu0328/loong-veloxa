# 任务跟踪

## 当前任务

| 字段 | 内容 |
|------|------|
| **ID** | TASK-20260413-01 |
| **描述** | QuickJS 脚本引擎集成：依赖引入、构建接入、最小运行时封装与测试基线；后续再扩展 DOM/事件绑定。 |
| **复杂度** | Level 4 — 复杂系统 |
| **安全** | `[安全相关]` — 脚本执行属于不可信代码执行面，需明确沙箱/资源边界与错误处理策略 |
| **状态** | 设计中 |
| **创建日期** | 2026-04-13 |

### 创意决策摘要（`/creative` 2026-04-13）

详见 `memory-bank/creative/creative-quickjs-host.md`。

| 组件 | 选定 |
|------|------|
| 执行预算 / 中断 | **分阶段方案 C**：首合入不注册 `JS_SetInterruptHandler`；后续增加 `SetEvalInterruptBudget`，与 `EventLoop` 协作推迟到 Application 集成。 |
| `qjs-libc` / `js_std_*` | **方案 B**：可继续链接 `qjs-libc`，**不在 `Init` 调用** `js_std_add_helpers` / `js_std_init_handlers`；未来需 `import std/os` 时单独开关 + 安全评审。 |
| 内存 | **首版方案 B**：`JS_SetMemoryLimit`，默认约 **32MiB** / Runtime；自定义 `JSMallocFunctions` 推迟（技术债）。 |

### 设计 / 计划文档

| 类型 | 路径 |
|------|------|
| 设计规格 | `docs/specs/2026-04-13-quickjs-integration-design.md` |
| 实现计划 | `docs/plans/2026-04-13-quickjs-integration.md` |

### 需要创意阶段（`/creative`）的组件

> 已完成决策，见 `memory-bank/creative/creative-quickjs-host.md` 与上表「创意决策摘要」。

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
| TASK-20260405-11 | C API 层（对外嵌入接口） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-12 | 示例应用（examples/hello） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-13 | CSS 动画系统（CSS Transitions） | ✅ 已完成 | 2026-04-05 |
