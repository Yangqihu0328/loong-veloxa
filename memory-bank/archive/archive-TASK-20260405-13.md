# 归档：CSS 动画系统（CSS Transitions）

**日期：** 2026-04-05
**任务 ID：** TASK-20260405-13
**复杂度级别：** Level 3
**状态：** ✅ 已完成

## 任务概述

从零实现 CSS Transitions 子系统，使 hover/active 等状态变化触发的属性变更具有平滑过渡动画效果。覆盖缓动函数、值插值、CSS 解析、UpdateManager 集成全链路。

## 技术方案

- **缓动函数**：CubicBezier 通用求解器（二分法 20 次迭代）+ 5 个 CSS 预设（linear/ease/ease-in/ease-out/ease-in-out）
- **值插值**：按类型分发——u32 颜色逐通道 lerp、f32 线性 lerp、LengthValue 同单位 lerp
- **CSS 解析**：transition 简写展开为 4 个 longhand Declaration，StyleResolver 逐条累积到 `ComputedStyle::transitions[0]`
- **动画驱动**：UpdateManager 维护 prev_styles_ 快照，Layout 后对比检测变化 → TransitionManager 启动/tick/apply → const_cast 覆盖 LayoutBox.style → 活跃动画保持 dirty_ = true

## 实现摘要

### 文件变更

| 操作 | 文件路径 | 说明 |
|------|---------|------|
| 创建 | `veloxa/core/css/transition.h` | TimingFunction/CubicBezier/TransitionSpec/TransitionManager 类型定义 |
| 创建 | `veloxa/core/css/transition.cc` | 缓动求解、3 种类型插值、属性字段 Get/Set、TransitionManager 实现（467 行） |
| 创建 | `tests/core/css/transition_test.cc` | 21 个单元测试（缓动/插值/IsAnimatable/TransitionManager） |
| 创建 | `tests/core/css/transition_parse_test.cc` | 5 个 CSS 解析测试 |
| 创建 | `tests/core/transition_integration_test.cc` | 3 个端到端集成测试 |
| 修改 | `veloxa/core/css/property.h` | +4 个 PropertyId（kTransitionProperty/Duration/TimingFunction/Delay） |
| 修改 | `veloxa/core/css/property.cc` | +4 个 PropertyInfo 表项 |
| 修改 | `veloxa/core/css/computed_style.h` | +`SmallVector<TransitionSpec, 2> transitions` 字段 |
| 修改 | `veloxa/core/css/parser.cc` | +109 行 transition 简写解析 |
| 修改 | `veloxa/core/css/style_resolver.cc` | +32 行 transition longhand ApplyDeclaration |
| 修改 | `veloxa/core/update_manager.h` | +TransitionManager 成员 + prev_styles_ HashMap |
| 修改 | `veloxa/core/update_manager.cc` | +DetectAndApplyTransitions + TraverseForTransitions |
| 修改 | `veloxa/core/CMakeLists.txt` | +css/transition.cc |
| 修改 | `tests/CMakeLists.txt` | +3 个测试目标 |

### 关键决策

1. **前向声明打破循环依赖**：transition.h 前向声明 ComputedStyle，ActiveTransition 存储 per-property from/to 值避免引用完整 ComputedStyle
2. **Layout 后覆盖而非 Restyle 阶段注入**：原计划的 Restyle 注入不可行（Layout 封装了 restyle），改为 const_cast 覆盖
3. **active_count_ 计数器替代 const 遍历**：HashMap 缺少 const_iterator，用计数器保持 HasActive() const

### 安全决策

本任务不涉及安全变更。

## 测试覆盖

- **单元测试**（21 个）：CubicBezier 求解精度、5 种缓动曲线特性、F32/Color/Length 插值、IsAnimatable 属性分类、TransitionManager 生命周期
- **解析测试**（5 个）：shorthand 展开、all/linear/seconds 单位、none 处理、StyleResolver 集成
- **集成测试**（3 个）：hover 触发过渡、无声明时无过渡、dirty 标记持续
- **总计**：792/792 全项目测试通过

## 经验教训

1. 设计文档的管线注入点须做代码级可行性验证
2. 集成测试编写前必须确认 html::Parser / FindElement / HandleInput 等 API
3. HashMap const_iterator 缺失影响了接口设计

## 参考文档

- 设计规格：`docs/specs/2026-04-05-css-transitions-design.md`
- 实现计划：`docs/plans/2026-04-05-css-transitions.md`
- 回顾文档：`memory-bank/reflection/reflection-TASK-20260405-13.md`
