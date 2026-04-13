# 设计规格：CSS Transitions

**日期：** 2026-04-05
**任务 ID：** TASK-20260405-13

## 概述

实现 CSS Transitions 子系统，使属性变化（hover/active 等触发的样式变更）具有平滑过渡动画效果。

## 设计决策

| 决策点 | 选定方案 | 理由 |
|--------|---------|------|
| 动画值注入点 | Restyle 阶段覆盖（方案 A） | 管线统一，HMI 树小 relayout <1ms |
| 时间源 | `std::chrono::steady_clock` | 单调时钟，不受系统时间调整影响 |
| 过渡声明存储 | `SmallVector<TransitionSpec, 2>` in ComputedStyle | 0-2 个声明避免动态分配 |
| 缓动函数 | cubic-bezier 通用实现 + 5 个预设名称 | CSS 规范标准集合 |
| 值插值 | 按类型分发（f32 lerp / 颜色通道 lerp / LengthValue lerp） | 覆盖所有可动画属性 |

## 架构

```
EventManager (hover/state change)
    → UpdateManager.Invalidate()
    → UpdateManager.Update():
        1. Snapshot old ComputedStyles (for elements with transitions)
        2. Restyle (compute new styles)
        3. TransitionManager.OnStyleChange(element, old_style, new_style)
           → Start/update transitions for changed animatable properties
        4. TransitionManager.Tick(now)
           → Interpolate values, write back to ComputedStyle
        5. Layout (with animated ComputedStyle values)
        6. Render
        7. if TransitionManager.HasActive() → dirty_ = true
```

## 新增类型

```cpp
// 缓动函数
enum class TimingFunction : u8 {
  kLinear,
  kEase,
  kEaseIn,
  kEaseOut,
  kEaseInOut,
  kCubicBezier,
};

struct CubicBezier {
  f32 x1, y1, x2, y2;
  f32 Solve(f32 t) const;  // 二分法求解
};

// 过渡声明（CSS 解析产物）
struct TransitionSpec {
  PropertyId property = PropertyId::kUnknown;  // kUnknown = "all"
  f32 duration_ms = 0.0f;
  f32 delay_ms = 0.0f;
  TimingFunction timing = TimingFunction::kEase;
  CubicBezier bezier = {0.25f, 0.1f, 0.25f, 1.0f};
};

// 活跃过渡（运行时状态）
struct ActiveTransition {
  PropertyId property;
  ComputedStyle from_style;  // snapshot at start
  ComputedStyle to_style;    // target style
  f32 duration_ms;
  f32 delay_ms;
  CubicBezier bezier;
  SteadyTimePoint start_time;
};
```

## 可动画属性

| 类型 | 属性 | 插值方式 |
|------|------|---------|
| u32 颜色 | background_color, color, border_color[4] | 逐通道 lerp |
| f32 | opacity, flex_grow, flex_shrink | 线性 lerp |
| LengthValue | width, height, margin_*, padding_*, inset, border_width[4], border_radius, font_size, line_height, letter_spacing, gap, flex_basis, min_*/max_* | 同单位 lerp |

## 缓动函数预设

| 名称 | cubic-bezier |
|------|-------------|
| linear | (0, 0, 1, 1) |
| ease | (0.25, 0.1, 0.25, 1.0) |
| ease-in | (0.42, 0, 1, 1) |
| ease-out | (0, 0, 0.58, 1) |
| ease-in-out | (0.42, 0, 0.58, 1) |
