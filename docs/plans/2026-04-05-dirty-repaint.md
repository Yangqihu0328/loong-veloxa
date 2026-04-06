# 脏区更新与重绘机制 实现计划

**目标：** 构建 UpdateManager 编排 Event→Restyle→Relayout→DirtyRect→Repaint 管线，让交互状态变更实际驱动屏幕刷新。

**架构：** Push 回调触发失效标记 → 全量 restyle+relayout → DisplayList 逐项对比计算脏区 → 裁剪重绘。

**技术栈：** C++17 / CMake / Google Test

**复杂度级别：** Level 3

---

## 文件结构

### 新建文件

| 文件 | 职责 |
|------|------|
| `veloxa/core/update_manager.h` | UpdateManager 类声明 |
| `veloxa/core/update_manager.cc` | 管线实现 |
| `tests/core/update_manager_test.cc` | UpdateManager 单元测试 |
| `tests/core/update_integration_test.cc` | 全管线集成测试 |

### 修改文件

| 文件 | 变更 |
|------|------|
| `veloxa/graphics/types.h` | Rect::Union 静态方法 |
| `veloxa/core/render/paint_command.h` | PaintCommand::operator== |
| `veloxa/core/render/renderer.h` | +ComputeDirtyRect 声明 |
| `veloxa/core/render/renderer.cc` | +ComputeDirtyRect 实现 |
| `veloxa/core/layout/layout_utils.h` | LayoutContext + event_manager |
| `veloxa/core/css/style_resolver.h` | Resolve/CollectMatchingRules + em |
| `veloxa/core/css/style_resolver.cc` | 透传 em 到 SelectorMatcher |
| `veloxa/core/layout/layout_engine.h` | +Layout ArenaAllocator 重载 |
| `veloxa/core/layout/layout_engine.cc` | +重载实现 + BuildTree 透传 em |
| `veloxa/core/event/event_manager.h` | +InvalidationCallback |
| `veloxa/core/event/event_manager.cc` | +状态变更检测 + 回调调用 |
| `veloxa/core/CMakeLists.txt` | +update_manager.cc |
| `tests/CMakeLists.txt` | +2 个测试目标 |

---

## Phase 1：API 管线扩展 [TDD]

打通 EventManager → LayoutContext → StyleResolver → SelectorMatcher 的 EventManager 指针透传链。

### 任务 1.1：基础类型扩展

**文件：**
- 修改：`veloxa/graphics/types.h`
- 修改：`veloxa/core/render/paint_command.h`

**变更内容：**

`types.h` — 在 Rect struct 中添加：

```cpp
static Rect Union(const Rect& a, const Rect& b) {
  if (a.IsEmpty()) return b;
  if (b.IsEmpty()) return a;
  vx::f32 ux = std::min(a.x, b.x);
  vx::f32 uy = std::min(a.y, b.y);
  vx::f32 ur = std::max(a.right(), b.right());
  vx::f32 ub = std::max(a.bottom(), b.bottom());
  return {ux, uy, ur - ux, ub - uy};
}
```

`paint_command.h` — 在 PaintCommand struct 中添加：

```cpp
bool operator==(const PaintCommand& other) const {
  return type == other.type && rect == other.rect &&
         color == other.color && param == other.param && text == other.text;
}
bool operator!=(const PaintCommand& other) const { return !(*this == other); }
```

**测试：** 在 Phase 2 的 DirtyRect 测试中一并覆盖。

### 任务 1.2：LayoutContext 扩展

**文件：**
- 修改：`veloxa/core/layout/layout_utils.h`

**变更：** LayoutContext struct 添加 `event_manager` 字段：

```cpp
struct LayoutContext {
  TextShaper* text_shaper = nullptr;
  const Vector<css::Stylesheet>* stylesheets = nullptr;
  f32 viewport_width = 0;
  f32 viewport_height = 0;
  f32 root_font_size = 16.0f;
  const event::EventManager* event_manager = nullptr;
};
```

需要在 layout_utils.h 顶部增加前向声明：

```cpp
namespace vx::event { class EventManager; }
```

**测试：** 向后兼容，现有 LayoutContext 使用者无需修改。编译通过即验证。

### 任务 1.3：StyleResolver 透传 EventManager

**文件：**
- 修改：`veloxa/core/css/style_resolver.h`
- 修改：`veloxa/core/css/style_resolver.cc`

**style_resolver.h 变更：**

```cpp
static ComputedStyle Resolve(
    const dom::Element* element,
    const Vector<Stylesheet>& stylesheets,
    const ComputedStyle* parent_style,
    const SmallVector<Declaration, 8>* inline_decls = nullptr,
    const event::EventManager* em = nullptr);

// CollectMatchingRules 新增 em 参数
static void CollectMatchingRules(
    const dom::Element* element,
    const Vector<Stylesheet>& stylesheets,
    Vector<MatchedRule>& matched,
    const event::EventManager* em = nullptr);
```

需要在 style_resolver.h 顶部增加前向声明：

```cpp
namespace vx::event { class EventManager; }
```

**style_resolver.cc 变更：**

`Resolve` 签名更新，将 `em` 传递给 `CollectMatchingRules`：

```cpp
ComputedStyle StyleResolver::Resolve(
    const dom::Element* element,
    const Vector<Stylesheet>& stylesheets,
    const ComputedStyle* parent_style,
    const SmallVector<Declaration, 8>* inline_decls,
    const event::EventManager* em) {
  // ... existing code ...
  CollectMatchingRules(element, stylesheets, matched, em);
  // ... rest unchanged ...
}

void StyleResolver::CollectMatchingRules(
    const dom::Element* element,
    const Vector<Stylesheet>& stylesheets,
    Vector<MatchedRule>& matched,
    const event::EventManager* em) {
  // ...
  if (SelectorMatcher::Matches(sel, element, em)) {
  // ...
}
```

**测试：** 在现有 `style_resolver_test.cc` 中追加测试：

```cpp
TEST(StyleResolverTest, HoverPseudoClassWithEventManager) {
  // 创建 DOM + 包含 div:hover { background-color: red } 的 CSS
  // 创建 EventManager, 构造 LayoutBox 使 HitTest 命中 div
  // 调用 HandleInput(PointerMove) 使 div 进入 hover 状态
  // 调用 StyleResolver::Resolve(div, sheets, nullptr, nullptr, &em)
  // 验证 background_color == red
}

TEST(StyleResolverTest, NoPseudoMatchWithoutEventManager) {
  // 同上 CSS，但不传 em
  // 验证 background_color == 默认值（透明）
}
```

### 任务 1.4：LayoutEngine 扩展

**文件：**
- 修改：`veloxa/core/layout/layout_engine.h`
- 修改：`veloxa/core/layout/layout_engine.cc`

**layout_engine.h 变更 — 新增重载：**

```cpp
static LayoutBox* Layout(dom::Document* doc, const LayoutContext& ctx,
                         ArenaAllocator& arena);
```

**layout_engine.cc 变更：**

新增重载实现（与原有 Layout 逻辑相同，但使用外部 arena）：

```cpp
LayoutBox* LayoutEngine::Layout(dom::Document* doc, const LayoutContext& ctx,
                                ArenaAllocator& arena) {
  LayoutBox* root = BuildTree(doc, nullptr, arena, ctx);
  if (!root) return nullptr;
  LayoutBlock(root, ctx.viewport_width, ctx);
  ApplyPositioning(root, ctx);
  return root;
}
```

BuildTree 中透传 EventManager 到 StyleResolver：

```cpp
// 原来：
resolved = css::StyleResolver::Resolve(element, *ctx.stylesheets, parent_style);
// 改为：
resolved = css::StyleResolver::Resolve(element, *ctx.stylesheets, parent_style,
                                       nullptr, ctx.event_manager);
```

**测试：** 在 `tree_builder_test.cc` 或新增测试中验证：

```cpp
TEST(LayoutEngineTest, LayoutWithExternalArena) {
  // 使用外部 ArenaAllocator 调用 Layout
  // 验证返回有效 LayoutBox 树
  // arena.Reset() 后指针失效（不访问）
}
```

### 任务 1.5：EventManager 失效回调

**文件：**
- 修改：`veloxa/core/event/event_manager.h`
- 修改：`veloxa/core/event/event_manager.cc`

**event_manager.h 变更：**

```cpp
class EventManager {
 public:
  using InvalidationCallback = std::function<void()>;

  // ... existing methods ...
  void SetInvalidationCallback(InvalidationCallback cb);

 private:
  // ... existing fields ...
  InvalidationCallback invalidation_callback_;
};
```

**event_manager.cc 变更：**

```cpp
void EventManager::SetInvalidationCallback(InvalidationCallback cb) {
  invalidation_callback_ = std::move(cb);
}
```

在 HandleInput 各分支中检测状态实际变化并调用回调：

```cpp
case EventType::kPointerMove: {
  dom::Element* new_hover = HitTest(layout_root, input.x, input.y);
  bool changed = (new_hover != hovered_);
  hovered_ = new_hover;
  if (changed && invalidation_callback_) invalidation_callback_();
  // ... dispatch event ...
  break;
}

case EventType::kPointerDown: {
  dom::Element* target = HitTest(layout_root, input.x, input.y);
  bool changed = (target != active_) || (target != focused_);
  active_ = target;
  if (target != focused_) focused_ = target;
  if (changed && invalidation_callback_) invalidation_callback_();
  // ... dispatch event ...
  break;
}

case EventType::kPointerUp: {
  dom::Element* target = HitTest(layout_root, input.x, input.y);
  bool changed = (active_ != nullptr);
  active_ = nullptr;
  if (changed && invalidation_callback_) invalidation_callback_();
  // ... dispatch event ...
  break;
}
```

**测试：** 在 `event_manager_test.cc` 中追加：

```cpp
TEST_F(EventManagerTest, InvalidationCallbackOnHoverChange)
TEST_F(EventManagerTest, NoCallbackWhenHoverUnchanged)
TEST_F(EventManagerTest, CallbackOnPointerDown)
TEST_F(EventManagerTest, CallbackOnPointerUp)
```

### Phase 1 边界输入清单

- LayoutContext::event_manager 为 nullptr 时所有现有行为不变
- StyleResolver::Resolve em 为 nullptr 时现有 17 个测试全通过
- EventManager 未设置 callback 时 HandleInput 正常工作
- LayoutEngine 原 Layout 签名（static arena）行为不变

---

## Phase 2：UpdateManager + DirtyRect [TDD]

### 任务 2.1：DirtyRect 辅助函数

**文件：**
- 修改：`veloxa/core/render/renderer.h`
- 修改：`veloxa/core/render/renderer.cc`
- 测试：`tests/core/render/renderer_test.cc`（追加）

**renderer.h 变更 — 新增声明：**

```cpp
gfx::Rect ComputeDirtyRect(const DisplayList& old_list,
                            const DisplayList& new_list,
                            f32 viewport_width, f32 viewport_height);
```

**renderer.cc 变更 — 实现：**

```cpp
gfx::Rect ComputeDirtyRect(const DisplayList& old_list,
                            const DisplayList& new_list,
                            f32 viewport_width, f32 viewport_height) {
  if (old_list.size() != new_list.size()) {
    return {0, 0, viewport_width, viewport_height};
  }
  gfx::Rect dirty{0, 0, 0, 0};
  for (usize i = 0; i < old_list.size(); ++i) {
    if (old_list[i] != new_list[i]) {
      dirty = gfx::Rect::Union(dirty, old_list[i].rect);
      dirty = gfx::Rect::Union(dirty, new_list[i].rect);
    }
  }
  return dirty;
}
```

**测试（追加到 renderer_test.cc）：**

```cpp
TEST(ComputeDirtyRectTest, IdenticalListsReturnEmpty)
TEST(ComputeDirtyRectTest, DifferentColorReturnsCmdRect)
TEST(ComputeDirtyRectTest, DifferentLengthsReturnFullViewport)
TEST(ComputeDirtyRectTest, MultipleDiffsUnionRects)
TEST(ComputeDirtyRectTest, EmptyListsReturnEmpty)
```

### 任务 2.2：UpdateManager 类

**文件：**
- 创建：`veloxa/core/update_manager.h`
- 创建：`veloxa/core/update_manager.cc`
- 修改：`veloxa/core/CMakeLists.txt`（+update_manager.cc）
- 修改：`tests/CMakeLists.txt`（+update_manager_test）
- 创建：`tests/core/update_manager_test.cc`

**update_manager.h：**

```cpp
#ifndef VELOXA_CORE_UPDATE_MANAGER_H_
#define VELOXA_CORE_UPDATE_MANAGER_H_

#include "veloxa/core/event/event_manager.h"
#include "veloxa/core/layout/layout_engine.h"
#include "veloxa/core/render/paint_command.h"
#include "veloxa/core/render/renderer.h"
#include "veloxa/foundation/memory/arena_allocator.h"
#include "veloxa/graphics/canvas.h"

namespace vx {

class UpdateManager {
 public:
  struct Config {
    dom::Document* document = nullptr;
    const Vector<css::Stylesheet>* stylesheets = nullptr;
    layout::LayoutContext layout_context;
    gfx::Canvas* canvas = nullptr;
    event::EventManager* event_manager = nullptr;
  };

  explicit UpdateManager(const Config& config);

  void Invalidate();
  void Update();

  bool is_dirty() const { return dirty_; }
  layout::LayoutBox* layout_root() const { return layout_root_; }
  const render::DisplayList& display_list() const { return display_list_; }
  gfx::Rect last_dirty_rect() const { return last_dirty_rect_; }

 private:
  Config config_;
  bool dirty_ = true;
  ArenaAllocator arena_{8192};
  layout::LayoutBox* layout_root_ = nullptr;
  render::DisplayList display_list_;
  gfx::Rect last_dirty_rect_;
};

}  // namespace vx

#endif  // VELOXA_CORE_UPDATE_MANAGER_H_
```

**update_manager.cc：**

```cpp
#include "veloxa/core/update_manager.h"

namespace vx {

UpdateManager::UpdateManager(const Config& config) : config_(config) {
  if (config_.event_manager) {
    config_.event_manager->SetInvalidationCallback(
        [this]() { Invalidate(); });
  }
  config_.layout_context.event_manager = config_.event_manager;
  config_.layout_context.stylesheets = config_.stylesheets;
}

void UpdateManager::Invalidate() {
  dirty_ = true;
}

void UpdateManager::Update() {
  if (!dirty_ || !config_.document) return;

  arena_.Reset();
  layout_root_ = layout::LayoutEngine::Layout(
      config_.document, config_.layout_context, arena_);

  render::DisplayList new_list;
  if (layout_root_) {
    new_list = render::Record(layout_root_);
  }

  last_dirty_rect_ = render::ComputeDirtyRect(
      display_list_, new_list,
      config_.layout_context.viewport_width,
      config_.layout_context.viewport_height);

  if (config_.canvas && !last_dirty_rect_.IsEmpty()) {
    config_.canvas->PushClipRect(last_dirty_rect_);
    config_.canvas->FillRect(last_dirty_rect_,
                             gfx::Brush::Solid(gfx::Color::White()));
    render::Replay(new_list, config_.canvas);
    config_.canvas->PopClip();
  }

  display_list_ = std::move(new_list);
  dirty_ = false;
}

}  // namespace vx
```

**测试（update_manager_test.cc）：**

```cpp
TEST(UpdateManagerTest, InitiallyDirty)
TEST(UpdateManagerTest, UpdateClearsDirty)
TEST(UpdateManagerTest, InvalidateSetsDirecty)
TEST(UpdateManagerTest, UpdateGeneratesLayoutRoot)
TEST(UpdateManagerTest, UpdateGeneratesDisplayList)
TEST(UpdateManagerTest, NoUpdateWhenClean)
TEST(UpdateManagerTest, EventManagerCallbackIntegration)
TEST(UpdateManagerTest, HoverChangeTriggersInvalidation)
TEST(UpdateManagerTest, DirtyRectComputedOnUpdate)
TEST(UpdateManagerTest, ReInvalidateAfterUpdate)
```

---

## Phase 3：集成测试 [TDD]

### 任务 3.1：全管线集成测试

**文件：**
- 创建：`tests/core/update_integration_test.cc`
- 修改：`tests/CMakeLists.txt`（+update_integration_test）

**测试场景：**

```cpp
// 测试夹具：HTML + CSS + Document + Stylesheets + EventManager + Canvas + UpdateManager

TEST_F(UpdateIntegrationTest, InitialUpdateRendersContent)
// 初始 Update 产生 DisplayList 并渲染到 Canvas

TEST_F(UpdateIntegrationTest, HoverChangeTriggersDirtyRect)
// PointerMove 到 div:hover 元素 → Invalidate → Update → 验证 dirty_rect 非空

TEST_F(UpdateIntegrationTest, HoverChangeUpdatesBackgroundColor)
// div:hover { background: #ff0000; } → 验证 Update 后 DisplayList 中 FillRect 颜色变化

TEST_F(UpdateIntegrationTest, ActiveStateChangesOnPointerDown)
// PointerDown → active_ 设置 → Invalidate → Update → 验证 :active 样式应用

TEST_F(UpdateIntegrationTest, FocusChangeOnPointerDown)
// PointerDown 到新元素 → focus 切换 → 验证 :focus 样式

TEST_F(UpdateIntegrationTest, NoUpdateWhenHoverUnchanged)
// PointerMove 到同一元素 → 不触发 Invalidate → is_dirty() == false

TEST_F(UpdateIntegrationTest, MultipleInvalidatesCoalesced)
// 多次 PointerMove + PointerDown → 只调用一次 Update

TEST_F(UpdateIntegrationTest, DirtyRectMatchesChangedElements)
// 验证 last_dirty_rect() 大致匹配变化元素的 border box
```

**约束（来自历史教训）：**
- 必须使用外部 CSS 选择器（`#id { ... }`），禁止 HTML inline style
- 颜色比较使用 CssColorToGfx 转换
- DisplayList 结构验证优先于像素检查

---

## CMakeLists.txt 变更

**veloxa/core/CMakeLists.txt：** 添加 `update_manager.cc`

**tests/CMakeLists.txt：** 添加：

```cmake
# Core: Update Manager
vx_add_test(update_manager_test core/update_manager_test.cc)
target_link_libraries(update_manager_test PRIVATE vx_core vx_graphics vx_platform)

vx_add_test(update_integration_test core/update_integration_test.cc)
target_link_libraries(update_integration_test PRIVATE vx_core vx_graphics vx_platform)
```

---

## 测试汇总

| Phase | 测试文件 | 新增测试数 |
|-------|---------|-----------|
| 1 | style_resolver_test.cc | ~2 |
| 1 | tree_builder_test.cc | ~1 |
| 1 | event_manager_test.cc | ~4 |
| 2 | renderer_test.cc | ~5 |
| 2 | update_manager_test.cc | ~10 |
| 3 | update_integration_test.cc | ~8 |
| **总计** | | **~30** |

## 执行策略

三个 Phase 必须顺序执行（Phase 2 依赖 Phase 1，Phase 3 依赖 Phase 2）。各 Phase 内部无并行子代理机会（每个 Phase 修改的文件相互依赖）。

## 风险与缓解

| 风险 | 缓解 |
|------|------|
| StyleResolver 新参数导致现有测试编译失败 | 默认参数 nullptr，零修改通过 |
| LayoutEngine::Layout 新重载与 static arena 版本冲突 | 保留原签名，新增重载 |
| DisplayList 长度变化（display:none 切换）导致脏区计算回退 | 回退全屏重绘，仍然正确 |
| Canvas::PushClipRect 对 empty rect 行为 | DirtyRect empty 时跳过重绘 |
