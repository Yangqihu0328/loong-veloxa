# 实现计划：事件系统（Event System）

**任务 ID：** TASK-20260405-08
**复杂度：** Level 4
**创意设计：** `memory-bank/creative/creative-event-system.md`
**日期：** 2026-04-05

---

## 文件结构

### 新建文件（7 个源文件 + 5 个测试文件）

| 文件 | 职责 |
|------|------|
| `veloxa/core/event/event_types.h` | EventType 枚举, InputEvent, EventPhase, DOMEvent, EventHandler 类型 |
| `veloxa/core/event/hit_test.h` | HitTest 函数声明 |
| `veloxa/core/event/hit_test.cc` | HitTest 实现（LayoutBox 树后序遍历） |
| `veloxa/core/event/event_dispatcher.h` | EventDispatcher 类（三阶段分发 + 监听器管理） |
| `veloxa/core/event/event_dispatcher.cc` | EventDispatcher 实现 |
| `veloxa/core/event/event_manager.h` | EventManager 类（统一入口 + 交互状态管理） |
| `veloxa/core/event/event_manager.cc` | EventManager 实现 |
| `tests/core/event/event_types_test.cc` | InputEvent/DOMEvent 构造与方法测试 |
| `tests/core/event/hit_test_test.cc` | HitTest 单元测试 |
| `tests/core/event/event_dispatcher_test.cc` | 三阶段分发测试 |
| `tests/core/event/event_manager_test.cc` | 状态管理 + CSS 伪类回填测试 |
| `tests/core/event/integration_test.cc` | HTML→事件→CSS 全管线集成测试 |

### 修改文件（4 个）

| 文件 | 变更内容 | 标注 |
|------|---------|------|
| `veloxa/core/CMakeLists.txt` | +3 个 .cc 源文件 | [共享文件] |
| `tests/CMakeLists.txt` | +5 个测试目标 | [共享文件] |
| `veloxa/core/css/selector_matcher.h` | +EventManager* 参数（默认 nullptr） | [影响前序测试：现有测试不变] |
| `veloxa/core/css/selector_matcher.cc` | +:hover/:active/:focus 查询 EventManager | — |

---

## Phase 1：基础设施（headers + stubs + CMake + event_types 测试）

### 1.1 event_types.h

```cpp
#ifndef VELOXA_CORE_EVENT_EVENT_TYPES_H_
#define VELOXA_CORE_EVENT_EVENT_TYPES_H_

#include <functional>

#include "veloxa/foundation/base/types.h"

namespace vx::dom {
class Element;
}

namespace vx::event {

enum class EventType : u8 {
  kPointerDown,
  kPointerUp,
  kPointerMove,
  kKeyDown,
  kKeyUp,
  kTouchStart,
  kTouchEnd,
  kTouchMove,
  kFocusIn,
  kFocusOut,
};

struct InputEvent {
  EventType type;
  f32 x = 0;
  f32 y = 0;
  u8 button = 0;
  u32 key_code = 0;
  u32 modifiers = 0;
  u32 touch_id = 0;
  u64 timestamp_ms = 0;
};

enum class EventPhase : u8 {
  kNone,
  kCapture,
  kTarget,
  kBubble,
};

struct DOMEvent {
  InputEvent input;
  dom::Element* target = nullptr;
  dom::Element* current_target = nullptr;
  EventPhase phase = EventPhase::kNone;
  bool propagation_stopped = false;
  bool default_prevented = false;

  void StopPropagation() { propagation_stopped = true; }
  void PreventDefault() { default_prevented = true; }
};

using EventHandler = std::function<void(DOMEvent&)>;

}  // namespace vx::event

#endif  // VELOXA_CORE_EVENT_EVENT_TYPES_H_
```

### 1.2 hit_test.h

```cpp
#ifndef VELOXA_CORE_EVENT_HIT_TEST_H_
#define VELOXA_CORE_EVENT_HIT_TEST_H_

#include "veloxa/core/dom/element.h"
#include "veloxa/core/layout/layout_box.h"
#include "veloxa/foundation/base/types.h"

namespace vx::event {

dom::Element* HitTest(layout::LayoutBox* root, f32 x, f32 y);

}  // namespace vx::event

#endif  // VELOXA_CORE_EVENT_HIT_TEST_H_
```

### 1.3 event_dispatcher.h

```cpp
#ifndef VELOXA_CORE_EVENT_EVENT_DISPATCHER_H_
#define VELOXA_CORE_EVENT_EVENT_DISPATCHER_H_

#include "veloxa/core/event/event_types.h"
#include "veloxa/foundation/containers/hash_map.h"
#include "veloxa/foundation/containers/vector.h"

namespace vx::event {

struct EventListener {
  EventType type;
  EventHandler handler;
  bool use_capture = false;
};

class EventDispatcher {
 public:
  void AddEventListener(dom::Element* element, EventType type,
                        EventHandler handler, bool use_capture = false);
  void RemoveEventListeners(dom::Element* element);
  void Dispatch(DOMEvent& event);

 private:
  HashMap<dom::Element*, Vector<EventListener>> listeners_;
};

}  // namespace vx::event

#endif  // VELOXA_CORE_EVENT_EVENT_DISPATCHER_H_
```

### 1.4 event_manager.h

```cpp
#ifndef VELOXA_CORE_EVENT_EVENT_MANAGER_H_
#define VELOXA_CORE_EVENT_EVENT_MANAGER_H_

#include "veloxa/core/event/event_dispatcher.h"
#include "veloxa/core/event/event_types.h"
#include "veloxa/core/layout/layout_box.h"

namespace vx::event {

class EventManager {
 public:
  EventManager() = default;

  void HandleInput(const InputEvent& input, layout::LayoutBox* layout_root);

  bool IsHovered(const dom::Element* el) const;
  bool IsActive(const dom::Element* el) const;
  bool IsFocused(const dom::Element* el) const;

  void AddEventListener(dom::Element* element, EventType type,
                        EventHandler handler, bool use_capture = false);
  void RemoveEventListeners(dom::Element* element);

  dom::Element* hovered_element() const { return hovered_; }
  dom::Element* active_element() const { return active_; }
  dom::Element* focused_element() const { return focused_; }

 private:
  dom::Element* hovered_ = nullptr;
  dom::Element* active_ = nullptr;
  dom::Element* focused_ = nullptr;
  EventDispatcher dispatcher_;
};

}  // namespace vx::event

#endif  // VELOXA_CORE_EVENT_EVENT_MANAGER_H_
```

### 1.5 Stub .cc 文件

**hit_test.cc (stub)**:
```cpp
#include "veloxa/core/event/hit_test.h"

namespace vx::event {

dom::Element* HitTest(layout::LayoutBox* /*root*/, f32 /*x*/, f32 /*y*/) {
  return nullptr;
}

}  // namespace vx::event
```

**event_dispatcher.cc (stub)**:
```cpp
#include "veloxa/core/event/event_dispatcher.h"

namespace vx::event {

void EventDispatcher::AddEventListener(dom::Element* /*element*/,
                                       EventType /*type*/,
                                       EventHandler /*handler*/,
                                       bool /*use_capture*/) {}

void EventDispatcher::RemoveEventListeners(dom::Element* /*element*/) {}

void EventDispatcher::Dispatch(DOMEvent& /*event*/) {}

}  // namespace vx::event
```

**event_manager.cc (stub)**:
```cpp
#include "veloxa/core/event/event_manager.h"

namespace vx::event {

void EventManager::HandleInput(const InputEvent& /*input*/,
                               layout::LayoutBox* /*layout_root*/) {}

bool EventManager::IsHovered(const dom::Element* /*el*/) const {
  return false;
}

bool EventManager::IsActive(const dom::Element* /*el*/) const {
  return false;
}

bool EventManager::IsFocused(const dom::Element* /*el*/) const {
  return false;
}

void EventManager::AddEventListener(dom::Element* element, EventType type,
                                    EventHandler handler, bool use_capture) {
  dispatcher_.AddEventListener(element, type, std::move(handler), use_capture);
}

void EventManager::RemoveEventListeners(dom::Element* element) {
  dispatcher_.RemoveEventListeners(element);
}

}  // namespace vx::event
```

### 1.6 CMakeLists.txt 变更

**veloxa/core/CMakeLists.txt** — 追加：
```cmake
  event/hit_test.cc
  event/event_dispatcher.cc
  event/event_manager.cc
```

**tests/CMakeLists.txt** — 追加：
```cmake
# Core: Event
vx_add_test(event_types_test core/event/event_types_test.cc)
target_link_libraries(event_types_test PRIVATE vx_core)

vx_add_test(hit_test_test core/event/hit_test_test.cc)
target_link_libraries(hit_test_test PRIVATE vx_core)

vx_add_test(event_dispatcher_test core/event/event_dispatcher_test.cc)
target_link_libraries(event_dispatcher_test PRIVATE vx_core)

vx_add_test(event_manager_test core/event/event_manager_test.cc)
target_link_libraries(event_manager_test PRIVATE vx_core)

vx_add_test(event_integration_test core/event/integration_test.cc)
target_link_libraries(event_integration_test PRIVATE vx_core vx_graphics vx_platform)
```

### 1.7 event_types_test.cc

```cpp
#include "veloxa/core/event/event_types.h"

#include <gtest/gtest.h>

namespace {

using namespace vx::event;

TEST(EventTypesTest, InputEventDefaultConstruction) {
  InputEvent e{};
  e.type = EventType::kPointerDown;
  EXPECT_EQ(e.x, 0.0f);
  EXPECT_EQ(e.y, 0.0f);
  EXPECT_EQ(e.button, 0);
  EXPECT_EQ(e.key_code, 0u);
  EXPECT_EQ(e.modifiers, 0u);
  EXPECT_EQ(e.timestamp_ms, 0u);
}

TEST(EventTypesTest, InputEventWithCoordinates) {
  InputEvent e{};
  e.type = EventType::kPointerMove;
  e.x = 100.5f;
  e.y = 200.0f;
  EXPECT_FLOAT_EQ(e.x, 100.5f);
  EXPECT_FLOAT_EQ(e.y, 200.0f);
}

TEST(EventTypesTest, InputEventKeyboard) {
  InputEvent e{};
  e.type = EventType::kKeyDown;
  e.key_code = 0x41;  // 'A'
  e.modifiers = 1;    // Ctrl
  EXPECT_EQ(e.key_code, 0x41u);
  EXPECT_EQ(e.modifiers, 1u);
}

TEST(EventTypesTest, InputEventTouch) {
  InputEvent e{};
  e.type = EventType::kTouchStart;
  e.touch_id = 1;
  e.x = 50.0f;
  e.y = 60.0f;
  EXPECT_EQ(e.touch_id, 1u);
}

TEST(EventTypesTest, DOMEventDefaultConstruction) {
  DOMEvent de{};
  EXPECT_EQ(de.target, nullptr);
  EXPECT_EQ(de.current_target, nullptr);
  EXPECT_EQ(de.phase, EventPhase::kNone);
  EXPECT_FALSE(de.propagation_stopped);
  EXPECT_FALSE(de.default_prevented);
}

TEST(EventTypesTest, DOMEventStopPropagation) {
  DOMEvent de{};
  EXPECT_FALSE(de.propagation_stopped);
  de.StopPropagation();
  EXPECT_TRUE(de.propagation_stopped);
}

TEST(EventTypesTest, DOMEventPreventDefault) {
  DOMEvent de{};
  EXPECT_FALSE(de.default_prevented);
  de.PreventDefault();
  EXPECT_TRUE(de.default_prevented);
}

TEST(EventTypesTest, EventPhaseValues) {
  EXPECT_NE(EventPhase::kNone, EventPhase::kCapture);
  EXPECT_NE(EventPhase::kCapture, EventPhase::kTarget);
  EXPECT_NE(EventPhase::kTarget, EventPhase::kBubble);
}

}  // namespace
```

### Phase 1 验证

```bash
cmake --build build && ./build/tests/event_types_test
# 预期：8 个测试通过
```

---

## Phase 2：Hit-Testing + EventDispatcher 实现

### 子代理策略

Phase 2 由单个子代理完成，包含两个独立实现模块（无共享 .cc 文件）。

### 2.1 hit_test.cc — 完整实现

**算法核心**：

```
HitTest(root, x, y):
  if root == null → return null
  return HitTestBox(root, x, y)

HitTestBox(box, x, y):
  style = box->style
  if style != null:
    if style->visibility == kHidden → return null

  // overflow:hidden 裁剪
  if style != null && style->overflow == kHidden:
    计算 padding box 边界
    px = box->x - box->padding[kLeft]
    py = box->y - box->padding[kTop]
    pw = box->padding_box_width()
    ph = box->padding_box_height()
    if x < px || x >= px+pw || y < py || y >= py+ph → return null

  // 收集并排序子元素
  children = Vector<LayoutBox*>
  for child in box->children: children.push(child)
  std::stable_sort(children, by style->z_index)

  // 从后往前（z-index 最大 = 最上层优先）
  for i = children.size()-1 downto 0:
    result = HitTestBox(children[i], x, y)
    if result != null → return result

  // 检查自身 border box
  bx = box->x - box->padding[kLeft] - box->border[kLeft]
  by = box->y - box->padding[kTop] - box->border[kTop]
  bw = box->border_box_width()
  bh = box->border_box_height()

  if x >= bx && x < bx+bw && y >= by && y < by+bh:
    if box->element → return box->element
    if box->parent && box->parent->element → return box->parent->element

  return null
```

**关键约束（必须包含在子代理 prompt 中）**：
- LayoutBox 的 `x`, `y` 是 **content area 原点**（绝对坐标）
- border box 原点 = `(x - padding[kLeft] - border[kLeft], y - padding[kTop] - border[kTop])`
- padding box 原点 = `(x - padding[kLeft], y - padding[kTop])`
- 使用 `border_box_width()`/`border_box_height()` 和 `padding_box_width()`/`padding_box_height()` 辅助函数

### hit_test_test.cc 测试用例（12 个）

| # | 测试名 | 描述 |
|---|--------|------|
| 1 | NullRoot | HitTest(nullptr, 0, 0) → nullptr |
| 2 | SingleBoxHit | 单个 100x100 box，点击中心 → 返回 element |
| 3 | SingleBoxMiss | 单个 box，点击外部 → nullptr |
| 4 | SingleBoxBorderEdge | 点击 border box 左上角 → 命中 |
| 5 | SingleBoxMarginMiss | 点击 margin 区域（border box 外）→ nullptr |
| 6 | NestedBoxes | parent + child，点击 child 区域 → 返回 child element |
| 7 | NestedBoxParentArea | parent + child，点击 parent 的非 child 区域 → 返回 parent |
| 8 | ZIndexOrder | 两个重叠 sibling，高 z-index 在上 → 返回高 z-index 元素 |
| 9 | OverflowHiddenClip | parent overflow:hidden，child 超出范围，点击超出部分 → nullptr |
| 10 | VisibilityHidden | visibility:hidden 元素 → 跳过，返回 nullptr |
| 11 | TextNodeReturnsParent | LayoutBox type=kText（无 element），→ 返回 parent->element |
| 12 | DeepNesting | 3 层嵌套，点击最深层 → 返回最深层 element |

### 2.2 event_dispatcher.cc — 完整实现

**算法核心**：

```
Dispatch(event):
  if event.target == null → return

  // 1. 构建路径：target → ... → root
  path = Vector<Element*>
  el = event.target
  while el != null:
    path.push(el)
    el = el->parent()

  // 2. Capture 阶段：从 root → target.parent
  event.phase = kCapture
  for i = path.size()-1 downto 1:
    event.current_target = path[i]
    FireListeners(path[i], event, true)
    if event.propagation_stopped → return

  // 3. Target 阶段
  event.phase = kTarget
  event.current_target = event.target
  FireListeners(event.target, event, true)   // capture listeners
  FireListeners(event.target, event, false)  // bubble listeners
  if event.propagation_stopped → return

  // 4. Bubble 阶段：从 target.parent → root
  event.phase = kBubble
  for i = 1 to path.size()-1:
    event.current_target = path[i]
    FireListeners(path[i], event, false)
    if event.propagation_stopped → return

FireListeners(element, event, capture):
  auto it = listeners_.find(element)
  if it == end → return
  for listener in it->second:
    if listener.type == event.input.type && listener.use_capture == capture:
      listener.handler(event)
      if event.propagation_stopped → return
```

### event_dispatcher_test.cc 测试用例（10 个）

| # | 测试名 | 描述 |
|---|--------|------|
| 1 | DispatchToTarget_NoListeners | 无监听器时不崩溃 |
| 2 | DispatchToTarget_SingleListener | 目标上注册 bubble 监听器 → 触发 |
| 3 | BubblePhase | child→parent 冒泡，parent 的 bubble 监听器触发 |
| 4 | CapturePhase | parent 的 capture 监听器先于 child 的 bubble 触发 |
| 5 | CaptureBeforeBubble | 验证完整顺序：capture(root→parent) → target → bubble(parent→root) |
| 6 | StopPropagationInCapture | capture 阶段 stopPropagation → bubble 不触发 |
| 7 | StopPropagationInBubble | bubble 阶段 stopPropagation → 后续祖先不触发 |
| 8 | TargetPhase_BothListeners | target 上同时有 capture 和 bubble listener → 都触发 |
| 9 | WrongEventType_NotFired | 注册 kPointerDown 监听器，发送 kPointerMove → 不触发 |
| 10 | RemoveEventListeners | 移除后不再触发 |

### Phase 2 边界输入清单

- HitTest: nullptr root, nullptr element on LayoutBox, nullptr style, kText type LayoutBox
- EventDispatcher: nullptr target, empty listeners map, EventType mismatch

---

## Phase 3：EventManager + CSS 伪类回填

### 3.1 event_manager.cc — 完整实现

**HandleInput 状态机**：

```
HandleInput(input, layout_root):
  switch input.type:
    case kPointerMove:
      new_hover = HitTest(layout_root, input.x, input.y)
      if new_hover != hovered_:
        hovered_ = new_hover
      // 分发 pointermove 事件
      if new_hover:
        DOMEvent event{input, new_hover, nullptr, kNone, false, false}
        dispatcher_.Dispatch(event)

    case kPointerDown:
      target = HitTest(layout_root, input.x, input.y)
      active_ = target
      if target != focused_:
        focused_ = target
      if target:
        DOMEvent event{input, target, ...}
        dispatcher_.Dispatch(event)

    case kPointerUp:
      target = HitTest(layout_root, input.x, input.y)
      active_ = nullptr
      if target:
        DOMEvent event{input, target, ...}
        dispatcher_.Dispatch(event)

    case kKeyDown, kKeyUp:
      if focused_:
        DOMEvent event{input, focused_, ...}
        dispatcher_.Dispatch(event)
```

**IsHovered/IsActive 祖先链检查**：

```cpp
bool EventManager::IsHovered(const dom::Element* el) const {
  const dom::Element* current = hovered_;
  while (current != nullptr) {
    if (current == el) return true;
    current = current->parent();
  }
  return false;
}
// IsActive 同构
// IsFocused: return el == focused_
```

### 3.2 SelectorMatcher 修改

**selector_matcher.h** — 增加 EventManager 前向声明和参数：

```cpp
namespace vx::event { class EventManager; }

namespace vx::css {
class SelectorMatcher {
 public:
  static bool Matches(const Selector& selector, const dom::Element* element,
                      const event::EventManager* em = nullptr);
 private:
  static bool MatchCompound(const CompoundSelector& compound,
                            const dom::Element* element,
                            const event::EventManager* em);
  static bool MatchSimple(const SimpleSelector& simple,
                          const dom::Element* element,
                          const event::EventManager* em);
};
}
```

**selector_matcher.cc** — kPseudoClass 分支扩展：

```cpp
#include "veloxa/core/event/event_manager.h"

case SimpleSelectorType::kPseudoClass: {
  StringView name = simple.value.view();
  if (name == StringView("first-child")) {
    return element->prev_sibling() == nullptr;
  }
  if (name == StringView("last-child")) {
    return element->next_sibling() == nullptr;
  }
  if (name == StringView("hover")) {
    return em != nullptr && em->IsHovered(element);
  }
  if (name == StringView("active")) {
    return em != nullptr && em->IsActive(element);
  }
  if (name == StringView("focus")) {
    return em != nullptr && em->IsFocused(element);
  }
  return false;
}
```

**style_resolver.cc** — 无修改（现有 `SelectorMatcher::Matches(sel, element)` 调用不变，新的第三参数默认 nullptr）

### event_manager_test.cc 测试用例（14 个）

| # | 测试名 | 描述 |
|---|--------|------|
| 1 | InitialStateNull | 初始 hovered/active/focused 均为 nullptr |
| 2 | PointerMoveUpdatesHover | 发送 kPointerMove → hovered_ 更新为 hit-test 目标 |
| 3 | PointerMoveToEmpty | move 到空白处 → hovered_ = nullptr |
| 4 | PointerDownUpdatesActive | kPointerDown → active_ 设为目标 |
| 5 | PointerDownUpdatesFocus | kPointerDown → focused_ 设为目标 |
| 6 | PointerUpClearsActive | kPointerUp → active_ = nullptr |
| 7 | FocusChanges | 连续两次 pointerdown 到不同元素 → focused 切换 |
| 8 | KeyboardToFocused | kKeyDown → 分发到 focused_ 元素 |
| 9 | KeyboardNoFocused | 无 focused 元素时 kKeyDown → 不崩溃 |
| 10 | IsHoveredAncestor | parent > child，hover child → IsHovered(child)=true, IsHovered(parent)=true |
| 11 | IsHoveredUnrelated | hover A → IsHovered(B)=false |
| 12 | IsActiveAncestor | 同 IsHovered 逻辑 |
| 13 | IsFocusedExact | focus A → IsFocused(A)=true, IsFocused(parent)=false |
| 14 | SelectorMatcherHoverIntegration | 创建 :hover 选择器 → Matches(sel, el, &em) 返回 true/false 根据 hover 状态 |

### Phase 3 边界输入清单

- EventManager: HandleInput with nullptr layout_root, pointer events outside all boxes
- SelectorMatcher: em=nullptr (backward compat), unknown pseudo-class names

---

## Phase 4：全管线集成测试

### integration_test.cc 测试用例（8 个）

| # | 测试名 | 描述 |
|---|--------|------|
| 1 | HitTestSimpleDiv | HTML `<div style="width:100px;height:100px">` → 布局 → HitTest(50,50) 命中 |
| 2 | HitTestNestedElements | `<div><span>` 嵌套 → HitTest 返回最内层 |
| 3 | HitTestMiss | 点击空白区域 → nullptr |
| 4 | HoverPseudoClass | 设 EventManager hover → :hover 选择器匹配 → ComputedStyle 应含 hover 样式 |
| 5 | ActivePseudoClass | pointerdown → :active 匹配 |
| 6 | FocusPseudoClass | pointerdown → :focus 匹配 |
| 7 | EventDispatchBubble | 注册 parent listener，点击 child → listener 触发 |
| 8 | HoverAncestorChain | `<div><p>text</p></div>` hover p → div 也 IsHovered |

### Phase 4 集成测试策略

- 使用真实 HTML/CSS 解析器构建 DOM（禁止手动 DOM 构建）
- 通过 LayoutEngine::Layout 获取布局树
- 通过 EventManager::HandleInput 模拟输入
- 验证 SelectorMatcher::Matches 结合 EventManager 的结果
- 验证 StyleResolver::Resolve 结合 EventManager 的结果（需传入 EventManager* 或手动调用 SelectorMatcher）

---

## 子代理分配策略

| Phase | 执行方式 | 理由 |
|-------|---------|------|
| 1 | 直接实现 | headers + stubs + CMake，模式固定，无算法 |
| 2 | 单个子代理 | hit_test.cc + event_dispatcher.cc 无共享文件，但逻辑紧凑可合并 |
| 3 | 直接实现 | 修改 selector_matcher（共享文件），需精确控制 |
| 4 | 直接实现 | 集成测试需要调试灵活性 |

## 测试预估

| Phase | 测试数 |
|-------|--------|
| Phase 1 | 8 |
| Phase 2 | 22 (12 + 10) |
| Phase 3 | 14 |
| Phase 4 | 8 |
| **总计** | **~52** |
