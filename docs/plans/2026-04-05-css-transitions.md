# 实现计划：CSS Transitions

**日期：** 2026-04-05
**任务 ID：** TASK-20260405-13
**设计文档：** `docs/specs/2026-04-05-css-transitions-design.md`

## 文件结构

### 新建文件

| 文件 | 职责 |
|------|------|
| `veloxa/core/css/transition.h` | TransitionSpec, TimingFunction, CubicBezier, ActiveTransition 类型定义 |
| `veloxa/core/css/transition.cc` | CubicBezier::Solve, 插值函数, TransitionManager 实现 |
| `tests/core/css/transition_test.cc` | 缓动函数 + 插值 + TransitionManager 单元测试 |
| `tests/core/transition_integration_test.cc` | 端到端过渡集成测试（HTML/CSS → hover → 帧推进 → 像素验证） |

### 修改文件

| 文件 | 变更 | 标注 |
|------|------|------|
| `veloxa/core/css/property.h` | 新增 transition-* PropertyId 枚举值 | [共享文件] |
| `veloxa/core/css/property.cc` | 新增 PropertyInfo 表项 + IsAnimatable() 函数 | [共享文件] |
| `veloxa/core/css/computed_style.h` | 新增 `SmallVector<TransitionSpec, 2> transitions` 字段 | [共享文件] |
| `veloxa/core/css/parser.cc` | 解析 transition 简写和 longhand 属性 | [共享文件] |
| `veloxa/core/css/style_resolver.cc` | ApplyDeclaration 新增 transition 属性 case | [共享文件] |
| `veloxa/core/update_manager.h` | 新增 TransitionManager 成员 | [共享文件] |
| `veloxa/core/update_manager.cc` | 集成过渡检测 + tick + 持续 dirty | [共享文件] |
| `veloxa/core/CMakeLists.txt` | 添加 css/transition.cc | [共享文件] |
| `tests/CMakeLists.txt` | 添加测试目标 | [共享文件] |

## Phase 1：核心类型 + 缓动函数 + 插值

### 1.1 transition.h — 类型定义

```cpp
#ifndef VELOXA_CORE_CSS_TRANSITION_H_
#define VELOXA_CORE_CSS_TRANSITION_H_

#include <chrono>
#include "veloxa/core/css/computed_style.h"
#include "veloxa/core/css/property.h"
#include "veloxa/foundation/containers/small_vector.h"
#include "veloxa/foundation/containers/vector.h"

namespace vx::css {

using SteadyTimePoint = std::chrono::steady_clock::time_point;
using SteadyClock = std::chrono::steady_clock;

enum class TimingFunction : u8 {
  kLinear,
  kEase,
  kEaseIn,
  kEaseOut,
  kEaseInOut,
  kCubicBezier,
};

struct CubicBezier {
  f32 x1 = 0, y1 = 0, x2 = 1, y2 = 1;
  f32 Solve(f32 x) const;

  static CubicBezier Linear() { return {0, 0, 1, 1}; }
  static CubicBezier Ease() { return {0.25f, 0.1f, 0.25f, 1.0f}; }
  static CubicBezier EaseIn() { return {0.42f, 0, 1, 1}; }
  static CubicBezier EaseOut() { return {0, 0, 0.58f, 1}; }
  static CubicBezier EaseInOut() { return {0.42f, 0, 0.58f, 1}; }

  static CubicBezier FromTiming(TimingFunction tf);
};

struct TransitionSpec {
  PropertyId property = PropertyId::kUnknown;  // kUnknown = "all"
  f32 duration_ms = 0.0f;
  f32 delay_ms = 0.0f;
  TimingFunction timing = TimingFunction::kEase;
  CubicBezier bezier = CubicBezier::Ease();
};

bool IsAnimatable(PropertyId id);

f32 InterpolateF32(f32 from, f32 to, f32 t);
u32 InterpolateColor(u32 from, u32 to, f32 t);
LengthValue InterpolateLength(const LengthValue& from,
                               const LengthValue& to, f32 t);
void InterpolateStyle(const ComputedStyle& from, const ComputedStyle& to,
                      PropertyId prop, f32 t, ComputedStyle& out);

}  // namespace vx::css
#endif
```

### 1.2 transition.cc — 缓动 + 插值实现

- `CubicBezier::Solve(x)`: 二分法求 t 使 `bezier_x(t) ≈ x`，然后返回 `bezier_y(t)`
- `InterpolateF32(a, b, t)`: `a + (b - a) * t`
- `InterpolateColor(a, b, t)`: 逐通道提取 → lerp → 重组（CSS RRGGBBAA 格式）
- `InterpolateLength(a, b, t)`: 同单位 lerp value，异单位返回 `to`
- `InterpolateStyle(from, to, prop, t, out)`: switch(prop) 分发到具体字段

### 1.3 CMakeLists.txt

在 `veloxa/core/CMakeLists.txt` 添加 `css/transition.cc`。

## Phase 2：CSS 属性解析 + ComputedStyle 扩展

### 2.1 property.h 扩展

在 `kLetterSpacing` 和 `kMaxProperty` 之间新增：

```cpp
kTransitionProperty,
kTransitionDuration,
kTransitionTimingFunction,
kTransitionDelay,
kTransition,  // shorthand
```

### 2.2 property.cc 扩展

- 新增 5 个 PropertyInfo 表项（name, inherited=false）
- 新增 `IsAnimatable(PropertyId)` 函数：返回属性是否可动画

### 2.3 computed_style.h 扩展

```cpp
#include "veloxa/core/css/transition.h"
// 在 ComputedStyle 末尾添加:
SmallVector<TransitionSpec, 2> transitions;
```

### 2.4 parser.cc 扩展

解析 `transition` 简写：`transition: <property> <duration> <timing> <delay>`

- `<property>`: 属性名 → PropertyIdFromName，或 "all" → kUnknown
- `<duration>`: 数字 + "s"/"ms" 单位
- `<timing>`: "linear"/"ease"/"ease-in"/"ease-out"/"ease-in-out" 或 cubic-bezier(...)
- `<delay>`: 同 duration

也支持 longhand：`transition-property`, `transition-duration`, `transition-timing-function`, `transition-delay`

### 2.5 style_resolver.cc 扩展

`ApplyDeclaration` 新增 `kTransition` / `kTransitionProperty` / ... case，将解析结果写入 `ComputedStyle::transitions`。

## Phase 3：TransitionManager + UpdateManager 集成

### 3.1 TransitionManager（在 transition.h/cc 中）

```cpp
class TransitionManager {
 public:
  void OnStyleChange(const dom::Element* el,
                     const ComputedStyle& old_style,
                     const ComputedStyle& new_style,
                     SteadyTimePoint now);
  void Tick(SteadyTimePoint now);
  void ApplyTo(const dom::Element* el, ComputedStyle& style) const;
  bool HasActive() const;
  void Clear();

 private:
  struct ActiveTransition {
    PropertyId property;
    ComputedStyle from_snapshot;
    ComputedStyle to_snapshot;
    f32 duration_ms;
    f32 delay_ms;
    CubicBezier bezier;
    SteadyTimePoint start_time;
    f32 progress = 0.0f;
    bool completed = false;
  };

  HashMap<const dom::Element*, Vector<ActiveTransition>> transitions_;
};
```

### 3.2 UpdateManager 集成

在 `Update()` 中：

1. **Restyle 前**：快照有 transition 声明的元素的旧 ComputedStyle
2. **Restyle 后**：调用 `transition_manager_.OnStyleChange(el, old, new, now)` 检测变化
3. **Tick**：`transition_manager_.Tick(now)` 推进进度
4. **Apply**：对有活跃过渡的元素，`transition_manager_.ApplyTo(el, style)` 覆盖插值结果
5. **Layout + Render**：使用覆盖后的 ComputedStyle
6. **持续 dirty**：`if (transition_manager_.HasActive()) dirty_ = true;`

注意：步骤 1-4 需要在 LayoutEngine::Layout 调用前完成。当前管线中 Layout 内部调用 StyleResolver，所以需要调整：
- 选项：在 UpdateManager 层做独立的 restyle pass（遍历 DOM 树 resolve style），然后将带动画覆盖的 style 传入 Layout
- 或者：让 TransitionManager 在 Layout 后、Render 前生效（仅限非 layout 属性）

**简化方案**：TransitionManager 在 Layout 完成后遍历 LayoutBox 树，对有活跃过渡的元素覆盖 computed_style 中的非 layout 属性（颜色、opacity），然后这些值在 Record 阶段被渲染。对于 layout 属性的过渡，第一版暂不支持（需要更深度的管线改造），仅支持视觉属性（颜色、opacity、border_color）过渡。

### 3.3 UpdateManager.Config 扩展

无需修改 Config，TransitionManager 作为 UpdateManager 的内部成员。

## Phase 4：测试

### 4.1 transition_test.cc — 单元测试

| # | 测试名 | 验证内容 |
|---|--------|---------|
| 1 | `CubicBezierLinear` | Solve(0.5) ≈ 0.5 |
| 2 | `CubicBezierEase` | Solve(0)=0, Solve(1)=1, 中间单调递增 |
| 3 | `CubicBezierEaseIn` | Solve(0.5) < 0.5 |
| 4 | `CubicBezierEaseOut` | Solve(0.5) > 0.5 |
| 5 | `InterpolateF32` | lerp(0,100,0.3) = 30 |
| 6 | `InterpolateColorRedToBlue` | 逐通道验证中间值 |
| 7 | `InterpolateLengthSameUnit` | Px(10) → Px(50) at 0.5 = Px(30) |
| 8 | `InterpolateLengthDiffUnit` | Px → Percent 返回 to |
| 9 | `IsAnimatableProperties` | background_color/opacity/width 返回 true, display 返回 false |
| 10 | `TransitionManagerStartOnChange` | 颜色变化 → HasActive() = true |
| 11 | `TransitionManagerTickProgress` | Tick 推进 → progress 更新 |
| 12 | `TransitionManagerComplete` | duration 后 → completed, HasActive() = false |
| 13 | `TransitionManagerApply` | 中间帧 ApplyTo 覆盖颜色值 |

### 4.2 transition_integration_test.cc — 集成测试

| # | 测试名 | 验证内容 |
|---|--------|---------|
| 1 | `HoverColorTransition` | 加载带 transition 的 CSS → hover → 多帧推进 → 颜色渐变 |
| 2 | `TransitionCompletes` | 过渡完成后颜色到达目标值 |
| 3 | `NoTransitionWithoutDeclaration` | 无 transition CSS → 颜色立即变化 |

#### 边界输入清单
- transition: none → 不启动过渡
- transition-duration: 0s → 立即变化
- transition-delay: 500ms → 延迟后开始
- transition-property: 不可动画属性（如 display）→ 忽略
- 多个 transition 声明 → 各自独立运行

## 预估

- **新增文件**：4 个
- **修改文件**：7 个
- **新增测试**：~16 个
- **实现代码**：~400 行（类型 ~80 + 缓动/插值 ~120 + Manager ~100 + 解析 ~100）

## 测试基础设施审计

| 测试需要访问的状态 | 访问路径 | 状态 |
|---|---|---|
| TransitionManager 活跃过渡 | TransitionManager::HasActive() | 需新建 |
| 插值中间值 | TransitionManager::ApplyTo() 后读 ComputedStyle | 需新建 |
| 帧推进时间控制 | TransitionManager 接受 SteadyTimePoint 参数 | 需新建 |
| 像素验证 | MemorySurface::data() | 已有 |
