# 创意设计：事件系统（Event System）

**日期：** 2026-04-05
**状态：** 已批准
**关联任务：** TASK-20260405-08

## 设计挑战

构建 Veloxa 的事件系统，将原始平台输入（触摸/指针/键盘）转化为 DOM 事件分发，管理元素交互状态（:hover/:active/:focus），并回填 CSS 伪类选择器。

### 约束
- 嵌入式环境，内存敏感
- 触摸优先，鼠标兼容
- 未来需适配 QuickJS 脚本绑定
- 不侵入已有 DOM/CSS/Layout 核心结构（最小化修改）

---

## 决策 1：输入事件数据模型 — 两层模型（InputEvent + DOMEvent）

### 选定方案

平台层和 DOM 层使用两个独立结构体：

```cpp
// ===== 平台层：原始输入事件 =====
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
  f32 x = 0, y = 0;        // Pointer/Touch 屏幕坐标
  u8 button = 0;            // 鼠标按钮 (0=左, 1=中, 2=右)
  u32 key_code = 0;         // 键盘 key code
  u32 modifiers = 0;        // Ctrl=1, Shift=2, Alt=4 位掩码
  u32 touch_id = 0;         // 多点触控 ID
  u64 timestamp_ms = 0;     // 事件时间戳
};

// ===== DOM 层：分发上下文 =====

enum class EventPhase : u8 {
  kNone,
  kCapture,
  kTarget,
  kBubble,
};

struct DOMEvent {
  InputEvent input;
  dom::Element* target = nullptr;          // hit-test 确定的事件目标
  dom::Element* current_target = nullptr;  // 当前正在处理的节点
  EventPhase phase = EventPhase::kNone;
  bool propagation_stopped = false;
  bool default_prevented = false;

  void StopPropagation() { propagation_stopped = true; }
  void PreventDefault() { default_prevented = true; }
};

}  // namespace vx::event
```

### 理由
- InputEvent 无 DOM 依赖，平台层可独立使用（headless 测试、输入录制回放）
- DOMEvent 附加分发上下文（target、phase、propagation 控制），与 W3C Event 接口对齐
- 扁平 struct 与 PaintCommand 模式一致，cache-friendly

---

## 决策 2：Hit-Testing — LayoutBox 树后序遍历

### 选定方案

```cpp
namespace vx::event {

// 在 LayoutBox 树中找到 (x,y) 坐标处最顶层的元素
dom::Element* HitTest(layout::LayoutBox* root, f32 x, f32 y);

}  // namespace vx::event
```

### 算法

```
HitTest(box, x, y):
  // 1. 跳过不可见元素
  if style->visibility == kHidden → return null
  if style->display == kNone → return null  (实际不会出现在布局树中)

  // 2. overflow:hidden 裁剪检查
  if style->overflow == kHidden:
    计算 padding box 边界
    if (x,y) 不在 padding box 内 → return null

  // 3. 收集子元素并按 z-index 排序（与 RecordBox 同序）
  children = 收集 box 的直接子节点
  std::stable_sort(children, by z_index)

  // 4. 从后往前遍历（最上层优先）
  for child in reverse(children):
    result = HitTest(child, x, y)
    if result != null → return result

  // 5. 检查当前 box 自身
  计算 border box: bx, by, bw, bh
  bx = box->x - box->padding[kLeft] - box->border[kLeft]
  by = box->y - box->padding[kTop] - box->border[kTop]
  bw = box->border_box_width()
  bh = box->border_box_height()

  if bx <= x < bx+bw && by <= y < by+bh:
    if box->element != null → return box->element
    // Text 节点或匿名 box：返回父元素
    if box->parent && box->parent->element → return box->parent->element

  return null
```

### 理由
- 与渲染管线的 z-index 排序逻辑共享，保证 hit-test 与视觉一致
- HMI 场景元素数 <500，O(N) 遍历无性能问题
- border box 坐标计算复用 TASK-07 已验证的公式

---

## 决策 3：事件分发 — W3C DOM Events（Capture → Target → Bubble）

### 选定方案

```cpp
namespace vx::event {

// 事件监听器类型
using EventHandler = std::function<void(DOMEvent&)>;

// 事件监听器注册信息
struct EventListener {
  EventType type;
  EventHandler handler;
  bool use_capture = false;  // true = capture 阶段触发
};

class EventDispatcher {
 public:
  // 注册/移除监听器
  void AddEventListener(dom::Element* element, EventType type,
                        EventHandler handler, bool use_capture = false);
  void RemoveEventListeners(dom::Element* element);

  // 分发事件到目标（执行三阶段传播）
  void Dispatch(DOMEvent& event);

 private:
  // element → listeners 映射
  HashMap<dom::Element*, Vector<EventListener>> listeners_;
};

}  // namespace vx::event
```

### 三阶段分发算法

```
Dispatch(event):
  // 1. 构建传播路径：target → ... → Document
  path = []
  el = event.target
  while el != null:
    path.push(el)
    el = el->parent()
  // path = [target, parent, ..., root]

  // 2. Capture 阶段：从 root → target.parent
  event.phase = kCapture
  for i in reverse(1..path.size()-1):  // root 到 target 的父节点
    event.current_target = path[i]
    触发 path[i] 上 use_capture=true 的监听器
    if event.propagation_stopped → return

  // 3. Target 阶段
  event.phase = kTarget
  event.current_target = event.target
  触发 target 上所有监听器（capture 和 bubble 均触发）
  if event.propagation_stopped → return

  // 4. Bubble 阶段：从 target.parent → root
  event.phase = kBubble
  for i in 1..path.size()-1:  // target 的父节点到 root
    event.current_target = path[i]
    触发 path[i] 上 use_capture=false 的监听器
    if event.propagation_stopped → return
```

### 理由
- 与 W3C DOM Events 规范对齐，未来 QuickJS 绑定可直接映射 `addEventListener(type, handler, useCapture)`
- capture 阶段仅多一次反向遍历，增量成本很小

---

## 决策 4：元素交互状态 — 中央 EventManager 指针

### 选定方案

```cpp
namespace vx::event {

class EventManager {
 public:
  EventManager() = default;

  // ----- 输入事件处理入口 -----
  // 接收平台层 InputEvent，执行 hit-test → 状态更新 → 事件分发
  void HandleInput(const InputEvent& input, layout::LayoutBox* layout_root);

  // ----- 交互状态查询（供 SelectorMatcher 使用）-----
  bool IsHovered(const dom::Element* el) const;
  bool IsActive(const dom::Element* el) const;
  bool IsFocused(const dom::Element* el) const;

  // ----- 事件监听器管理（委托给 EventDispatcher）-----
  void AddEventListener(dom::Element* element, EventType type,
                        EventHandler handler, bool use_capture = false);

  // ----- 状态访问 -----
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
```

### 状态更新逻辑

```
HandleInput(input, layout_root):
  switch input.type:
    case kPointerMove:
      new_target = HitTest(layout_root, input.x, input.y)
      if new_target != hovered_:
        // 旧元素触发 pointerleave（bubble）
        // 新元素触发 pointerenter（bubble）
        hovered_ = new_target
      分发 pointermove 事件到 new_target

    case kPointerDown:
      target = HitTest(layout_root, input.x, input.y)
      active_ = target
      if target != focused_:
        old_focused = focused_
        focused_ = target
        // 触发 focusout → focusin
      分发 pointerdown 事件

    case kPointerUp:
      target = HitTest(layout_root, input.x, input.y)
      if target == active_:
        // 生成合成 click 事件
      active_ = nullptr
      分发 pointerup 事件

    case kKeyDown/kKeyUp:
      分发到 focused_ 元素（无 hit-test）
```

### SelectorMatcher 连接

修改 `selector_matcher.cc` 第 88-96 行：

```cpp
case SimpleSelectorType::kPseudoClass: {
  StringView name = simple.value.view();
  if (name == StringView("first-child")) {
    return element->prev_sibling() == nullptr;
  }
  if (name == StringView("last-child")) {
    return element->next_sibling() == nullptr;
  }
  if (name == StringView("hover")) {
    return event_manager_ && event_manager_->IsHovered(element);
  }
  if (name == StringView("active")) {
    return event_manager_ && event_manager_->IsActive(element);
  }
  if (name == StringView("focus")) {
    return event_manager_ && event_manager_->IsFocused(element);
  }
  return false;
}
```

### IsHovered 实现

```cpp
bool EventManager::IsHovered(const dom::Element* el) const {
  // el 是 hovered_ 本身，或是 hovered_ 的祖先
  const dom::Element* current = hovered_;
  while (current != nullptr) {
    if (current == el) return true;
    current = current->parent();
  }
  return false;
}
// IsActive 同理
```

### 理由
- 仅 3 个指针（24 字节），不修改 Element 结构
- 状态变更 O(1)，查询 O(depth)（HMI 树深 <20，完全可接受）
- EventManager 是事件系统的统一入口：HandleInput → HitTest → 状态更新 → 事件分发
- SelectorMatcher 通过可选的 EventManager* 引用查询状态，不持有 EventManager 不影响现有行为（返回 false）

---

## 模块结构

```
veloxa/core/event/
├── event_types.h       # EventType, InputEvent, EventPhase, DOMEvent
├── hit_test.h          # HitTest 函数声明
├── hit_test.cc         # HitTest 实现
├── event_dispatcher.h  # EventDispatcher（三阶段分发）
├── event_dispatcher.cc
├── event_manager.h     # EventManager（状态管理 + 统一入口）
└── event_manager.cc

修改已有文件：
├── veloxa/core/css/selector_matcher.h   # +EventManager* 成员
├── veloxa/core/css/selector_matcher.cc  # +:hover/:active/:focus 查询
```

## 跨模块数据格式

### LayoutBox 坐标语义（从 TASK-07 继承）
- `x`, `y`：content area 原点
- border box 原点：`(x - padding[left] - border[left], y - padding[top] - border[top])`
- Hit-test 判定使用 border box 区域

### SelectorMatcher 接口变更
- `SelectorMatcher` 新增可选 `EventManager*` 成员
- 默认 `nullptr`（保持向后兼容：:hover/:active/:focus 返回 false）
- 使用时通过 `set_event_manager(EventManager*)` 注入
