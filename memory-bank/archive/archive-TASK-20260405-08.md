# 归档：事件系统（Event System）

**日期：** 2026-04-05
**任务 ID：** TASK-20260405-08
**复杂度级别：** Level 4
**状态：** ✅ 已完成

## 任务概述

构建 Veloxa 的事件系统模块 `vx::event`，实现从平台层原始输入到 DOM 事件分发的完整链路，包括 Hit-Testing、W3C 三阶段事件传播、元素交互状态管理（:hover/:active/:focus），以及 CSS 伪类选择器的回填连接。

## 技术方案

### 四项关键设计决策

| 决策点 | 选定方案 | 理由 |
|--------|---------|------|
| 事件数据模型 | 两层模型（InputEvent + DOMEvent） | 平台层/DOM 层解耦，与 PaintCommand 模式一致 |
| Hit-Testing | LayoutBox 树后序遍历 | 共享渲染管线 z-index 排序逻辑，HMI 场景 O(N) 足够 |
| 事件分发 | W3C Capture → Target → Bubble | 标准三阶段，为未来 QuickJS 绑定打基础 |
| 状态管理 | 中央 EventManager（3 指针） | 不侵入 Element，24 字节，IsHovered 祖先遍历 |

## 实现摘要

### 文件变更

| 操作 | 文件路径 | 说明 |
|------|---------|------|
| 创建 | `veloxa/core/event/event_types.h` | EventType 枚举, InputEvent, EventPhase, DOMEvent, EventHandler |
| 创建 | `veloxa/core/event/hit_test.h/cc` | LayoutBox 树 HitTest（z-index 排序 + overflow 裁剪 + visibility 跳过） |
| 创建 | `veloxa/core/event/event_dispatcher.h/cc` | W3C 三阶段分发 + 监听器管理 |
| 创建 | `veloxa/core/event/event_manager.h/cc` | 统一入口：HandleInput → HitTest → 状态更新 → Dispatch |
| 创建 | `tests/core/event/event_types_test.cc` | 8 个测试 |
| 创建 | `tests/core/event/hit_test_test.cc` | 12 个测试 |
| 创建 | `tests/core/event/event_dispatcher_test.cc` | 10 个测试 |
| 创建 | `tests/core/event/event_manager_test.cc` | 14 个测试 |
| 创建 | `tests/core/event/integration_test.cc` | 8 个测试（HTML→DOM→CSS→Layout→Event 全管线） |
| 修改 | `veloxa/core/css/selector_matcher.h` | +EventManager* 参数（默认 nullptr，向后兼容） |
| 修改 | `veloxa/core/css/selector_matcher.cc` | +:hover/:active/:focus 查询 EventManager |
| 修改 | `veloxa/core/CMakeLists.txt` | +3 个 .cc 源文件 |
| 修改 | `tests/CMakeLists.txt` | +5 个测试目标 |

### 关键决策

1. **SelectorMatcher 向后兼容 API**：添加 `const EventManager* em = nullptr` 默认参数，现有 17 个测试 + StyleResolver 零修改通过
2. **IsHovered 祖先链遍历**：:hover 同时作用于目标及所有祖先，通过 `while (current) { if (current == el) return true; current = current->parent(); }` 实现
3. **并行子代理**：HitTest + EventDispatcher 拆分为两个并行子代理，无共享 .cc 文件，零冲突

### 安全决策

本任务不涉及安全变更。

## 测试覆盖

| 测试文件 | 测试数 | 覆盖内容 |
|---------|--------|---------|
| event_types_test.cc | 8 | InputEvent/DOMEvent 构造、StopPropagation、PreventDefault |
| hit_test_test.cc | 12 | null root、命中/未命中、border 边缘、z-index、overflow 裁剪、visibility、text 回退、深嵌套 |
| event_dispatcher_test.cc | 10 | 三阶段顺序、capture/bubble、StopPropagation、类型过滤、监听器移除 |
| event_manager_test.cc | 14 | 状态更新、祖先链查询、focus 切换、keyboard 分发、SelectorMatcher 集成 |
| integration_test.cc | 8 | HTML→Event→CSS 全管线：HitTest、:hover/:active/:focus、冒泡、祖先链 |
| **总计** | **52** | 702/702 全量通过 |

## 经验教训

1. **inline style 在 Veloxa 集成测试中不可用**：BuildTree 不解析 HTML inline style 属性，必须使用外部 CSS 选择器。这是"API 能力假设错误"第三次出现。
2. **并行子代理可行条件**：无共享 .cc + 共享 .h 已创建 + CMakeLists.txt 已更新。首次验证成功。
3. **默认参数向后兼容注入模式**：为已有接口注入可选依赖的最小侵入方案。

## 参考文档

- 实现计划：`docs/plans/2026-04-05-event-system.md`
- 创意设计：`memory-bank/creative/creative-event-system.md`
- 回顾文档：`memory-bank/reflection/reflection-TASK-20260405-08.md`
