# 脏区更新与重绘机制 — 设计规格

**任务 ID：** TASK-20260405-09
**日期：** 2026-04-05
**状态：** ✅ 已批准

## 问题陈述

当前 `EventManager::HandleInput` 更新 `hovered_/active_/focused_` 指针后不触发任何后续动作。导致：
- `:hover/:active/:focus` 状态变更后样式不重算，屏幕不刷新
- 整个交互管线断在「状态变更 → 视觉更新」环节

## 关键发现

**集成缺口：** `StyleResolver::CollectMatchingRules` 调用 `SelectorMatcher::Matches(sel, element)` 未传 `EventManager*`。即使 SelectorMatcher 已支持伪类，restyle 阶段伪类规则永远不匹配。

## 设计决策

### 决策 1：更新管线架构 → Push 回调

EventManager 在 `HandleInput` 中检测到状态实际变化时，调用注册的 `InvalidationCallback`。UpdateManager 设置回调并标记 `dirty_`。

### 决策 2：Restyle + Relayout 粒度 → 全量重建

HMI 场景元素数 ~10-100，全量 restyle + relayout < 1ms。同时修复 tech debt #19（static ArenaAllocator → UpdateManager 持有）。

### 决策 3：脏区追踪策略 → DisplayList 逐项对比

对比新旧 DisplayList，变化项的 rect 并集 = 脏区。列表长度不同时回退全屏重绘。对 hover/active/focus 变更（仅颜色变化，结构不变）效果最优。

## 架构

```
Input Event
    │
    ▼
EventManager::HandleInput()
    │ (状态变更检测)
    ▼
InvalidationCallback → UpdateManager::Invalidate()
    │ (设 dirty_=true)
    ▼  (调用 Update())
UpdateManager::Update()
    ├─ arena_.Reset()
    ├─ Restyle+Relayout: LayoutEngine::Layout(doc, ctx, arena_)
    │     └─ ctx.event_manager 透传到 StyleResolver → SelectorMatcher
    ├─ Record: new_list = render::Record(layout_root_)
    ├─ DirtyRect: diff(old_list, new_list) → dirty_rect
    ├─ Repaint: clip to dirty_rect + replay new_list
    └─ Swap: display_list_ = new_list, dirty_ = false
```

## API 变更（向后兼容）

| 接口 | 变更 | 向后兼容 |
|------|------|---------|
| `LayoutContext` | +`const event::EventManager* event_manager = nullptr` | ✅ 默认值 |
| `StyleResolver::Resolve` | +`const event::EventManager* em = nullptr` | ✅ 默认值 |
| `StyleResolver::CollectMatchingRules` | +`const event::EventManager* em = nullptr` | ✅ 默认值 |
| `LayoutEngine::Layout` | +重载 `Layout(doc, ctx, arena)` | ✅ 原签名保留 |
| `EventManager` | +`SetInvalidationCallback` + 状态变更检测 | ✅ 新增方法 |
| `gfx::Rect` | +`Union()` 静态方法 | ✅ 新增方法 |
| `PaintCommand` | +`operator==` | ✅ 新增运算符 |
