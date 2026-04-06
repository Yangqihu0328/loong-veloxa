# 归档：脏区更新与重绘机制

**日期：** 2026-04-05
**任务 ID：** TASK-20260405-09
**复杂度级别：** Level 3
**状态：** ✅ 已完成

## 任务概述

构建脏区更新与重绘机制，实现从事件状态变更（:hover/:active/:focus）到样式失效、全量 Restyle/Relayout、脏区追踪、增量重绘的完整更新管线。这是使 Veloxa UI 引擎能够响应用户输入并产生视觉反馈的关键管线。

## 技术方案

### 设计决策

| 决策点 | 选项 | 选定方案 | 理由 |
|--------|------|---------|------|
| 失效触发方式 | Push 回调 / Pull 轮询 | Push 回调 | 无变化时零开销，HMI 输入频率低 |
| Restyle/Relayout 策略 | 全量重建 / 增量差量 | 全量重建 | HMI 树 < 200 节点，全量 < 1ms，增量复杂度不值得 |
| 脏区计算方式 | DisplayList 逐项对比 / LayoutBox 标记 | DisplayList 对比 | 利用已有 PaintCommand 结构，~15 行实现 |

### 架构

```
EventManager::HandleInput()
  → 检测 hovered_/active_/focused_ 变化
  → 调用 InvalidationCallback
  → UpdateManager::Invalidate() 设置 dirty_ = true

UpdateManager::Update()
  → arena_.Reset()
  → LayoutEngine::Layout(doc, ctx, arena_)  // 全量 restyle + relayout
  → render::Record(root)                     // 生成新 DisplayList
  → render::ComputeDirtyRect(old, new, vp)   // 逐项对比计算脏区
  → canvas->PushClipRect(dirty_rect)         // 裁剪到脏区
  → canvas->FillRect(dirty_rect, bg)         // 清空脏区
  → render::Replay(new_list, canvas)         // 重绘
  → canvas->PopClip()
```

## 实现摘要

### Phase 1：API 管线扩展（6 个新测试）

扩展现有模块 API 以支持更新管线所需的参数透传和功能。

### Phase 2：UpdateManager + DirtyRect（15 个新测试）

创建中央更新管线编排器和脏区计算辅助函数。

### Phase 3：全管线集成测试（8 个新测试）

HTML→CSS→Event→Update→Render 全链路集成验证。

### 文件变更

| 操作 | 文件路径 | 说明 |
|------|---------|------|
| 创建 | `veloxa/core/update_manager.h` | UpdateManager 类定义，Config/Invalidate/Update |
| 创建 | `veloxa/core/update_manager.cc` | 更新管线编排实现 |
| 创建 | `tests/core/update_manager_test.cc` | 10 个 UpdateManager 单元测试 |
| 创建 | `tests/core/update_integration_test.cc` | 8 个全管线集成测试 |
| 修改 | `veloxa/graphics/types.h` | Rect::Union 静态方法 |
| 修改 | `veloxa/core/render/paint_command.h` | PaintCommand::operator== / operator!= |
| 修改 | `veloxa/core/layout/layout_utils.h` | LayoutContext 添加 event_manager 字段 |
| 修改 | `veloxa/core/css/style_resolver.h` | Resolve/CollectMatchingRules 添加 em 参数 |
| 修改 | `veloxa/core/css/style_resolver.cc` | 透传 EventManager* 到 SelectorMatcher::Matches |
| 修改 | `veloxa/core/layout/layout_engine.h` | Layout 新增 ArenaAllocator& 重载 |
| 修改 | `veloxa/core/layout/layout_engine.cc` | 实现新重载 + BuildTree 透传 em |
| 修改 | `veloxa/core/event/event_manager.h` | InvalidationCallback 类型和 setter |
| 修改 | `veloxa/core/event/event_manager.cc` | HandleInput 中状态变化检测 + 回调触发 |
| 修改 | `veloxa/core/render/renderer.h` | ComputeDirtyRect 声明 |
| 修改 | `veloxa/core/render/renderer.cc` | ComputeDirtyRect 实现（DisplayList 对比） |
| 修改 | `veloxa/core/CMakeLists.txt` | 添加 update_manager.cc |
| 修改 | `tests/CMakeLists.txt` | 添加 update_manager_test + update_integration_test |
| 修改 | `tests/core/css/style_resolver_test.cc` | 2 个伪类透传测试 |
| 修改 | `tests/core/event/event_manager_test.cc` | 4 个 InvalidationCallback 测试 |
| 修改 | `tests/core/render/renderer_test.cc` | 5 个 ComputeDirtyRect 测试 |

### 关键决策

1. **全量 Restyle + Relayout**：HMI 场景 DOM 树小（< 200 节点），全量重建延迟 < 1ms，避免增量差量的大量复杂度。
2. **DisplayList 逐项对比计算脏区**：结构不变时（hover/active/focus 变色）仅 union 差异项的 rect；结构变化时回退全屏重绘。
3. **ArenaAllocator 外部化**：UpdateManager 拥有 ArenaAllocator，每帧 Reset 释放旧 LayoutBox 树，解决了 LayoutEngine static arena 的生命周期管理问题。
4. **CSS 伪类透传链修复**：发现并修复 TASK-08 遗留的 StyleResolver→SelectorMatcher `EventManager*` 透传断链，使伪类在 restyle 阶段正确匹配。

### 安全决策

本任务不涉及安全变更。

## 测试覆盖

- **新增测试**：29 个（6 + 15 + 8）
- **总测试**：731/731 通过
- **覆盖场景**：
  - StyleResolver 伪类匹配（有/无 EventManager）
  - EventManager InvalidationCallback 触发/不触发
  - ComputeDirtyRect（相同/颜色变化/长度不同/多差异/空列表）
  - UpdateManager dirty 状态管理、布局生成、DisplayList 生成、EventManager 集成
  - 全管线集成：初始渲染、hover/active/focus 状态变更、脏区计算、失效合并

## 经验教训

1. **规划阶段端到端验证跨模块透传链**：TASK-08 修改了 SelectorMatcher 但未更新 StyleResolver 调用链。在 /plan 阶段发现此断链，避免了构建阶段的调试。
2. **HMI 场景约束简化设计空间**：3 个设计决策均被场景约束限定为单一最优解，/creative 阶段正确跳过。
3. **默认参数向后兼容注入模式第二次验证成功**：`= nullptr` 策略使 702 个现有测试零修改通过。
4. **构造函数签名是高频假设错误点**：SoftwareCanvas 构造方式假设错误，写测试前应查阅头文件或现有测试。

## 已解决技术债务

- **#18**：:hover/:active/:focus 伪类 stub → ✅ 已回填完整透传链（SelectorMatcher + StyleResolver）
- **#19**：LayoutEngine static ArenaAllocator → 部分缓解（新增外部 arena 重载，旧签名仍存在）
- **#29**：EventManager 无重绘触发机制 → ✅ 已实现 UpdateManager + InvalidationCallback

## 新增技术债务

- **#33**：UpdateManager::Update 中 canvas 操作可提取为 RepaintDirtyRegion 辅助方法
- **#34**：ComputeDirtyRect 仅支持逐项对比，不处理命令重排序
- **#35**：UpdateManager 缺少 OnBeforeUpdate/OnAfterUpdate 钩子

## 参考文档

- 设计规格：`docs/specs/2026-04-05-dirty-repaint-design.md`
- 实现计划：`docs/plans/2026-04-05-dirty-repaint.md`
- 回顾文档：`memory-bank/reflection/reflection-TASK-20260405-09.md`
