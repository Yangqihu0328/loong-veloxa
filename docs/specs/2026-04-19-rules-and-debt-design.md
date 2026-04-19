# 设计规格：流程规则沉淀 + P2 功能技术债收口

**任务 ID**：TASK-20260419-01
**日期**：2026-04-19
**类型**：流程规则沉淀（Part A）+ 技术债务清理（Part B），无新功能
**复杂度**：Level 3
**目标规模**：分两批 7 个 Phase 实施；A 先 B 后

---

## 1. 背景与目标

`memory-bank/activeContext.md` 自 TASK-20260413-01 以来累积的 P1/P2 待处理事项已达 22 条。其中 14 条 P1 是「教训沉淀」（多次踩同一个坑），3 条 P2 是 TASK-20260418-01 显式留下的功能债。本次任务一次性收口，目标：

- **Part A（教训沉淀）**：把 14 条 P1 改进固化到 `.cursor/rules/skills/*.mdc` 与 `memory-bank/techContext.md`，让后续任务自动免疫
- **Part B（功能技术债）**：消化 #46 续作（StyleGetProp Enum 读路径）、#50 续作（DomBindings/EventManager 反向析构 UAF 防护）、#47 续作（removeEventListener 精确移除）

完成本任务后，`activeContext.md` 待办列表预计从 22 条减到 ≤ 5 条（仅剩 P2 优化项与跨任务长期改进）。

## 2. 范围边界

**纳入：**
- 修改 `.cursor/rules/skills/{writing-plans, subagent-development}.mdc`
- 新建 `.cursor/rules/skills/integration-testing.mdc`
- 修改 `memory-bank/techContext.md`（新增「FetchContent 与代理」段）
- 新建 `veloxa/core/css/enum_serialization.{h,cc}` + 单元测试
- 修改 `veloxa/core/event/event_{manager,dispatcher}.{h,cc}` + 单元测试
- 修改 `veloxa/script/dom_bindings.cc` + 集成测试

**排除（留作后续任务）：**
- Benchmark 补充（依赖 google benchmark，网络敏感）
- 剩余像素测试迁移到 `tests/test_pixel_utils.h`（P2，非阻塞）
- `enum_serialization` 在 `StyleSetProp` 写路径的反向应用（本次只做读路径）

## 3. Part A：流程规则沉淀

### 3.1 计划模板增段（writing-plans.mdc）

在「CMake 链接方向约束分析」段（已存在）之后，按以下顺序追加 5 段：

#### 3.1.1 段「FetchContent C 子项目编译选项审计」

**触发条件：** 计划中包含 `FetchContent_Declare` 或将引入第三方源码编译时
**回答问题：**
1. 第三方代码是否包含 C 源（QuickJS、libuv、zlib 等）？
2. 根 `add_compile_options(...)` 是否对 C 源生效？是否包含 `-Werror=pedantic`、`-Wpedantic`、`-Werror` 等会让上游 C 失败的标志？
3. 如果第 2 点为是 → 计划必须包含「将严格告警限制为 C++」的前置 Phase（用 `$<COMPILE_LANGUAGE:CXX>` 生成表达式或目标级 `target_compile_options`）

**来源：** TASK-20260413-01 反复出现「环境/编译前置未验证」

#### 3.1.2 段「测试基础设施审计」

**触发条件：** 任何 Phase 包含新写测试
**对每个测试任务列出：**
1. 测试需要访问哪些内部状态？（私有字段、内部容器大小、内部回调触发次数）
2. 该状态当前的访问路径是什么？（公开 getter / friend / `#define private public` 等）
3. 若无访问路径 → 计划必须包含「补充访问路径」的前置任务（优先暴露公开只读 getter，禁止 friend 滥用）

**来源：** TASK-11 反复出现

#### 3.1.3 段「边界输入清单」

**触发条件：** 任何 Phase 涉及解析、查找、转换或边界判断
**每个 Phase 须列出：**
1. 默认路径输入（happy path）
2. **非默认路径输入**：空、null、超长、Unicode、负值、零、最大值、未注册项、不存在 ID
3. 每个非默认路径的预期行为（返回错误码 / 抛异常 / 静默忽略 / clamp）

**来源：** TASK-06 反复出现

#### 3.1.4 段「调用链端到端验证」

**触发条件：** 任务涉及给已有函数增加参数、修改默认值、改变返回类型
**计划必须包含：**
1. **完整调用链清单**：从最上游入口（如 `Application::Update`）到最下游被改函数的全部中间层
2. 每一层的调用点是否需要透传参数？
3. 现有测试覆盖是否覆盖整条链？若否 → 加端到端集成测试

**来源：** TASK-09（CSS 伪类透传链 SelectorMatcher → StyleResolver → BuildTree → Layout 完整路径）

#### 3.1.5 段「设计文档管线注入点的代码级可行性验证」

**触发条件：** 设计文档提出「在管线 X 步骤后注入新逻辑 Y」
**头脑风暴阶段必须：**
1. 找到 X 步骤的实际代码位置（文件 + 函数 + 行号）
2. 验证 Y 所需的输入数据在 X 处是否可访问（不需要新增参数透传）
3. 若不可访问 → 设计文档必须明确「需要扩展 X 接口」并给出具体签名变更

**来源：** TASK-13 反复出现第 4 次

### 3.2 子代理 prompt 增段（subagent-development.mdc）

在「Prompt 骨架示例」段之后，按以下顺序追加 3 段：

#### 3.2.1 段「跨模块数据格式」

**适用：** 子代理任务涉及读写跨模块共享数据（像素格式、坐标系、字符编码、CSS 颜色编码）
**Prompt 必须包含：**
- 数据格式精确定义（如 `RGBA32 = R[0:7] | G[8:15] | B[16:23] | A[24:31]`）
- 上游/下游模块对该格式的引用位置（文件 + 函数）
- 已知的格式不一致风险（如 `gfx::Color::Black()` 与 CSS `black` 的 0x000000FF vs 0xFF000000 差异）

**来源：** TASK-02 验证有效（无格式 prompt 触发 24 小时返工）

#### 3.2.2 段「LayoutBox 坐标语义」

**适用：** 子代理任务涉及 hit-testing、边框/背景渲染、padding/border 计算
**Prompt 必须明示：**
- `LayoutBox::x`, `LayoutBox::y` 是 **content area 原点**（绝对坐标）
- padding box 原点 = `(x - padding[left], y - padding[top])`
- border box 原点 = `(x - padding[left] - border[left], y - padding[top] - border[top])`
- 子代理代码中任何坐标计算必须在注释中标注是哪一个 origin

**来源：** TASK-07（border box vs content box 假设错误）

#### 3.2.3 段「并行子代理可行条件」

**主线程在分派多个子代理并行执行前必须确认：**
1. 各子代理产出的 `.cc` 文件**完全不重叠**
2. 共享的 `.h` 文件已在前置 Phase 创建并就绪
3. `CMakeLists.txt` 已在前置 Phase 完成更新（包含全部即将创建的 `.cc` 文件）
4. 不可并行场景：多个子代理修改同一 `.cc`、共享 `.h` 的接口尚未定型、CMakeLists 待加目标

**配套规则（已在 systemPatterns 验证）：** 共享文件冲突约束子代理分组模式

**来源：** TASK-08 验证有效（hit_test.cc + event_dispatcher.cc 并行零冲突）

### 3.3 集成测试规范（新建 integration-testing.mdc）

新文件 `.cursor/rules/skills/integration-testing.mdc`，结构如下：

```markdown
---
description: 集成测试 - 编写跨模块端到端测试时使用，区别于单元测试
globs: []
alwaysApply: false
---

# 集成测试规范

## 适用范围
- 跨 ≥ 2 个 veloxa/ 子模块
- 验证完整管线（如 HTML → DOM → CSS 应用 → Layout → Render → Pixel）
- 验证子代理输出之间的数据流一致性

## 6 条铁律

### 1. 数据格式一致性优先
集成测试的首要价值是**捕获跨模块数据格式假设错误**。每个测试至少包含一个数据格式断言（颜色编码、像素格式、坐标系）。
（来源 TASK-02）

### 2. 必须使用真实 HTML/CSS 解析器
禁止仅用手动 DOM 构建（`Document::CreateElement` + `AppendChild`）作为集成测试输入。
正确做法：`html::Parser parser(html_source); auto doc = parser.Parse();`
理由：手动构建会绕过 Tokenizer 的实体解码、隐式关闭、空白处理。
（来源 TASK-06）

### 3. 像素验证优先 DisplayList + 区域扫描
禁止硬编码像素坐标（如 `EXPECT_EQ(GetPixel(50, 60), 0xFF0000FF)`）。
正确做法：
- 优先：检查 DisplayList 中是否存在 `FillRect{color, ...}` 命令
- 次选：`HasColorInRegion(canvas, Rect{x, y, w, h}, color)` 区域扫描
- 仅在最简单无嵌套场景下用精确像素
理由：HTML 隐式包裹（`<html><body>`）会偏移坐标。
（来源 TASK-07）

### 4. CSS 颜色测试禁止与 gfx::Color 编程常量直接比较
禁止：`EXPECT_EQ(box.background, gfx::Color::Green().ToRGBA32())`（CSS green 是 #008000，gfx green 是 #00FF00）
必须：`EXPECT_EQ(box.background, css::CssColorToGfx(0x008000FF))` 或直接用整数 `0x008000FF`
（来源 TASK-07）

### 5. 禁止使用 HTML inline style
`LayoutEngine::BuildTree` 调用 `StyleResolver::Resolve` 时 `inline_decls` 默认为 `nullptr`。
所以 `<div style="color: red">` 在集成测试中**永远不会**应用。
必须改用外部 CSS 选择器：`#test { color: red; }` + `<div id="test">`
（来源 TASK-08，API 能力假设错误已第 3 次出现）

### 6. API 备忘清单（写测试前必读）
- HTML：`html::Parser(source)` → `Parser::Parse()` → `unique_ptr<Document>`
- DOM 查找：`Document::FindElement(StringView id)`（**不是** `getElementById`）
- 输入注入：`EventManager::HandleInput(InputEvent, LayoutBox* root)`
- CSS 解析：`css::CssParser parser(source); parser.Parse(&stylesheet)`
- 样式应用：`css::StyleResolver resolver(...); resolver.Resolve(el, em=nullptr)`
- 布局：`layout::LayoutEngine::Layout(doc, stylesheets, viewport, arena, em=nullptr)`
- 渲染：`render::Paint(layout_root, canvas, image_cache=nullptr)`
（来源 TASK-13 反复出现第 4 次）

## 验证有效性
- 集成测试必须能在某个子模块被引入回归时失败（写测试时手动注入缺陷验证）
- 不能仅断言「测试运行未崩溃」

## 与 Memory Bank 集成
- 测试发现的跨模块假设错误必须记入 `progress.md` 的「遇到的问题」段
- 反复出现的同类问题升级为 systemPatterns 模式
```

### 3.4 techContext 增段「FetchContent 与代理」

在「### 第三方依赖」段后追加：

```markdown
## FetchContent 与代理（开发环境注意）

### http_proxy / HTTPS_PROXY 设置
首次 `cmake -B build` 触发 `FetchContent` 拉取 quickjs-ng（v0.14.0）等依赖。WSL2 / 内网环境若无系统 DNS，必须导出代理：

```bash
export http_proxy=http://your-proxy:port
export https_proxy=$http_proxy
cmake -B build
```

### 离线场景
若需完全离线构建，预先把 `_deps` 目录从可联网机器拷贝到 `build/_deps`，再运行 `cmake -B build`，FetchContent 会跳过下载。

### 已知首次配置失败模式
| 现象 | 原因 | 解决 |
|------|------|------|
| `Could not resolve host: github.com` | 无代理 | 设置 `http_proxy` |
| `error: '#pragma' is not allowed here` | 全局 `-Werror=pedantic` 污染 C 源 | 用 `$<$<COMPILE_LANGUAGE:CXX>:-Wpedantic>` 限制 |
| `Unknown arguments specified` (FindPkgConfig) | 子目录重复 `pkg_check_modules` | 提供方一次声明 + `PUBLIC` 链接 |
```

## 4. Part B：功能技术债

### 4.1 B5：StyleGetProp Enum 读路径

#### 4.1.1 现状

`dom_bindings.cc` 的 `SerializeCssValue` 在 `case ValueType::kEnum:` 分支落空（line 250-254 注释「P2 residual」）。`kStyleMappings` 当前唯一 Enum 类型属性是 `display`。

#### 4.1.2 设计

**新建 `veloxa/core/css/enum_serialization.{h,cc}`，与 `property.cc` 同构（枚举 + 元数据表驱动模式）**：

```cpp
// veloxa/core/css/enum_serialization.h
#ifndef VELOXA_CORE_CSS_ENUM_SERIALIZATION_H_
#define VELOXA_CORE_CSS_ENUM_SERIALIZATION_H_

#include "veloxa/core/css/property.h"
#include "veloxa/foundation/strings/string_view.h"

namespace vx::css {

// Returns the CSS-canonical string for an enum value of the given property,
// or an empty StringView if (property, enum_value) is not a registered enum.
//
// Examples:
//   EnumValueToCssString(PropertyId::kDisplay, 1) == "block"
//   EnumValueToCssString(PropertyId::kPosition, 2) == "absolute"
//   EnumValueToCssString(PropertyId::kBackgroundColor, 0) == ""  // not enum
StringView EnumValueToCssString(PropertyId property, u16 enum_value);

}  // namespace vx::css
#endif
```

**实现策略：**

`enum_serialization.cc` 用「PropertyId → 静态 string 表」二维查表：

```cpp
namespace {
constexpr const char* kDisplayNames[] = {"none", "block", "inline", "inline-block", "flex"};
constexpr const char* kPositionNames[] = {"static", "relative", "absolute", "fixed"};
// ...对全部 13 个 enum 类型同样定义...

struct EnumTable {
  const char* const* names;
  u16 count;
};

constexpr EnumTable GetEnumTable(PropertyId p) {
  switch (p) {
    case PropertyId::kDisplay:        return {kDisplayNames, 5};
    case PropertyId::kPosition:       return {kPositionNames, 4};
    case PropertyId::kFlexDirection:  return {kFlexDirectionNames, 4};
    case PropertyId::kFlexWrap:       return {kFlexWrapNames, 2};
    case PropertyId::kJustifyContent: return {kJustifyContentNames, 5};
    case PropertyId::kAlignItems:
    case PropertyId::kAlignSelf:      return {kAlignItemsNames, 6};
    case PropertyId::kBoxSizing:      return {kBoxSizingNames, 2};
    case PropertyId::kOverflow:       return {kOverflowNames, 4};
    case PropertyId::kVisibility:     return {kVisibilityNames, 2};
    case PropertyId::kTextAlign:      return {kTextAlignNames, 4};
    case PropertyId::kWhiteSpace:     return {kWhiteSpaceNames, 4};
    case PropertyId::kBorderTopStyle:
    case PropertyId::kBorderRightStyle:
    case PropertyId::kBorderBottomStyle:
    case PropertyId::kBorderLeftStyle: return {kBorderStyleNames, 4};
    case PropertyId::kFontStyle:      return {kFontStyleNames, 2};
    default:                          return {nullptr, 0};
  }
}
}  // namespace

StringView EnumValueToCssString(PropertyId p, u16 v) {
  EnumTable t = GetEnumTable(p);
  if (t.names == nullptr || v >= t.count) return StringView();
  return StringView(t.names[v]);
}
```

**dom_bindings.cc 接入点：**

`SerializeCssValue` 签名扩展，加 `PropertyId` 参数：

```cpp
void SerializeCssValue(const css::CssValue& v, css::PropertyId prop, String* out) {
  switch (v.type) {
    // 其他分支不变...
    case css::ValueType::kEnum: {
      auto sv = css::EnumValueToCssString(prop, v.enum_value);
      out->append(sv);
      break;
    }
    case css::ValueType::kNone: break;
  }
}
```

调用处（`StyleGetProp` 内）传入 `kStyleMappings[magic].prop`。

#### 4.1.3 测试

- **单元测试** `tests/css/enum_serialization_test.cc`：覆盖 13 个 Enum × 边界（合法值、超界、非 Enum 属性）
- **集成测试** `tests/script/dom_bindings_test.cc`：新增 4 个用例
  - `StyleGetDisplayBlock`、`StyleGetDisplayFlex`、`StyleGetDisplayNone`、`StyleGetDisplayInvalidEnumReturnsEmpty`

### 4.2 B6：DomBindings/EventManager 反向析构弱引用

#### 4.2.1 现状

`DomBindings::Unbind`（dom_bindings.cc:679-704）调用 `data_->em->RemoveEventListeners(...)`。若宿主反序销毁（`~EventManager` 在 `~DomBindings` 之前），这一行就是 UAF。`memory-bank/systemPatterns.md` 已标记为「剩余风险」，留给本任务。

#### 4.2.2 设计

**EventManager 加 `AddDestructionObserver` 接口**（与现有 `SetInvalidationCallback` 同构，push 回调失效触发模式）：

```cpp
// event_manager.h
class EventManager {
 public:
  using DestructionObserver = std::function<void()>;

  // ... existing API ...

  // Register a callback to be invoked when this EventManager is destroyed.
  // Used by long-lived holders (DomBindings) to clear stale pointers
  // before any further method dispatch reaches the dangling EventManager.
  //
  // Observers are invoked in registration order. Observer callbacks must NOT
  // call back into this EventManager (which is mid-destruction).
  void AddDestructionObserver(DestructionObserver cb);

  ~EventManager();

 private:
  // ... existing fields ...
  Vector<DestructionObserver> destruction_observers_;
};
```

`event_manager.cc` 实现：

```cpp
void EventManager::AddDestructionObserver(DestructionObserver cb) {
  destruction_observers_.push_back(std::move(cb));
}

EventManager::~EventManager() {
  for (usize i = 0; i < destruction_observers_.size(); ++i) {
    if (destruction_observers_[i]) destruction_observers_[i]();
  }
}
```

**DomBindings 接入：**

`Bind` 末尾注册一个观察者，把 `data_->em` 置 nullptr：

```cpp
void DomBindings::Bind(JSContext* ctx, dom::Document* doc,
                       event::EventManager* em) {
  // ... existing init ...
  if (em) {
    em->AddDestructionObserver([data_ptr = data_.get()]() {
      data_ptr->em = nullptr;
    });
  }
}
```

`Unbind` 增加 nullptr 检查（已经有 `if (data_->em)`，但要注意 destruction observer 触发后如果再调用 Unbind，要安全）：

```cpp
void DomBindings::Unbind() {
  if (data_->em) {  // already exists; remains correct after observer fires
    for (...) data_->em->RemoveEventListeners(...);
  }
  // ...
}
```

#### 4.2.3 安全性论证

| 销毁顺序 | 旧行为 | 新行为 |
|---------|-------|-------|
| DomBindings → EventManager（正序） | OK：Unbind 显式清理 listener，~EventManager 后续无副作用 | OK：相同 |
| EventManager → DomBindings（反序） | UAF：~EventManager 完成后，~DomBindings 调用 `em->RemoveEventListeners` 解引用悬垂指针 | OK：~EventManager 触发观察者把 `data_->em` 置 nullptr；~DomBindings 走 nullptr 分支 |
| 重入调用（observer 调 EM 方法） | N/A | **禁止**：文档明示，违反 → UB（与 std::function deleter 重入约束一致） |

#### 4.2.4 测试

- **单元测试** `tests/event/event_manager_test.cc`：
  - `DestructionObserverFiredOnDestroy`、`MultipleObserversFiredInOrder`、`NoObserverNoCrash`
- **集成测试** `tests/script/dom_bindings_test.cc`：
  - `EventManagerDestroyedFirstThenDomBindingsUnbindSafe`（构造局部 EM，析构后 Unbind 不应崩溃）

### 4.3 B7：removeEventListener 精确移除

#### 4.3.1 现状

`EventDispatcher::RemoveEventListeners(Element*)` 删除该元素的所有 listener；`ElementRemoveEventListener`（dom_bindings.cc:445-462）忽略 JS 传入的 `(type, callback)`，全删。

#### 4.3.2 设计

**EventDispatcher / EventManager 引入 ListenerToken**：

```cpp
// event_dispatcher.h
struct EventListener {
  EventType type;
  EventHandler handler;
  bool use_capture = false;
  u64 token = 0;  // unique within this EventDispatcher's lifetime
};

class EventDispatcher {
 public:
  // Returns a unique token (>= 1) identifying this listener.
  // Token 0 is reserved for "not found".
  u64 AddEventListener(dom::Element* element, EventType type,
                       EventHandler handler, bool use_capture = false);

  void RemoveEventListeners(dom::Element* element);  // unchanged: bulk
  bool RemoveEventListenerByToken(dom::Element* element, u64 token);  // new

  // ... existing ...

 private:
  HashMap<dom::Element*, Vector<EventListener>> listeners_;
  u64 next_token_ = 1;  // 0 reserved
};
```

`EventManager` 镜像扩展，`AddEventListener` 返回 `u64 token`，新增 `RemoveEventListenerByToken`。

**dom_bindings.cc 维护 (Element*, EventType, JSValue ptr) → token 索引：**

```cpp
struct ListenerKey {
  dom::Element* element;
  event::EventType type;
  void* js_ptr;  // JS_VALUE_GET_PTR(callback) — used as key only, not deref
  bool operator==(const ListenerKey& o) const { ... }
};
struct ListenerKeyHash { usize operator()(const ListenerKey& k) const { ... } };

struct InstanceData {
  // ... existing ...
  HashMap<ListenerKey, u64, ListenerKeyHash> listener_tokens;
};
```

`ElementAddEventListener`：

```cpp
JSValue callback = JS_DupValue(ctx, argv[1]);
data->tracked_callbacks.Track(callback);

u64 token = data->em->AddEventListener(el, event_type, [...](...) { ... });

ListenerKey key{el, event_type, JS_VALUE_GET_PTR(callback)};
data->listener_tokens.Insert(key, token);
// listener_elements unchanged: still tracks elements for Unbind cleanup
```

`ElementRemoveEventListener`：

```cpp
JSValue ElementRemoveEventListener(JSContext* ctx, JSValueConst this_val,
                                   int argc, JSValueConst* argv) {
  auto* el = GetElement(ctx, this_val);
  auto* data = GetData(ctx);
  if (!el || !data || !data->em || argc < 2) return JS_UNDEFINED;

  const char* type_str = JS_ToCString(ctx, argv[0]);
  if (!type_str) return JS_UNDEFINED;
  event::EventType event_type;
  bool valid = MapJsEventName(type_str, &event_type);
  JS_FreeCString(ctx, type_str);
  if (!valid) return JS_UNDEFINED;

  ListenerKey key{el, event_type, JS_VALUE_GET_PTR(argv[1])};
  u64* token = data->listener_tokens.Find(key);
  if (!token) return JS_UNDEFINED;  // 静默：W3C spec 也要求 no-op

  data->em->RemoveEventListenerByToken(el, *token);
  data->listener_tokens.Erase(key);
  // listener_elements 不再 swap-and-pop——元素可能仍有其他 listener
  return JS_UNDEFINED;
}
```

`Unbind` 不变（仍走 `RemoveEventListeners(el)` 批量清理）。

#### 4.3.3 向后兼容

- `EventManager::AddEventListener` 旧签名返回 `void` → 新签名返回 `u64`：旧调用方丢弃返回值即可，源码兼容
- `EventDispatcher::RemoveEventListeners` 不变
- 旧测试零修改通过

#### 4.3.4 测试

- **单元测试** `tests/event/event_manager_test.cc` / `event_dispatcher_test.cc`：
  - `AddEventListenerReturnsUniqueToken`、`RemoveByTokenRemovesOnly`、`RemoveUnknownTokenIsNoOp`、`TokensSurviveOtherRemovals`
- **集成测试** `tests/script/dom_bindings_test.cc`：
  - `RemoveEventListenerOnlyRemovesMatchingHandler`、`RemoveEventListenerWrongTypeIsNoOp`、`RemoveEventListenerThenDispatchOtherHandlerStillCalled`

## 5. 安全考量

本任务**不涉及**外部输入处理 / 认证授权 / 数据存储 / 部署配置 / 第三方集成。

特别地：
- Part A 全部为内部规则文档，不影响运行时行为
- Part B5 Enum 反查表：u16 enum_value 来源于 CSS 解析器（`StyleResolver::ApplyDeclaration`），已在 parser 层完成范围校验；`enum_serialization` 内部对 `v >= count` 做边界检查
- Part B6 析构观察者：仅清理悬垂指针，不引入新外部接口
- Part B7 ListenerToken：token 单调递增（u64，不会回绕），无伪造风险（外部代码无法直接构造合法 token）

## 6. 验收标准

| 维度 | 标准 |
|------|------|
| 测试 | 全部测试通过；新增至少 18 个测试（B5: 6+, B6: 4+, B7: 8+） |
| 编译 | `cmake --build build` 零警告（保持当前基线）|
| API 兼容 | 现有 856 个测试零修改通过 |
| 文档完整性 | `activeContext.md` 待办减至 ≤ 5 条；`techContext.md` / `systemPatterns.md` 追加新模式 |
| Part A 校验 | 每条 P1 待办在对应 .mdc 文件中可定位（grep 关键词）|
| Part B 校验 | `StyleGetProp` 对 display 返回 `"block"` 等真实值；EM 反序销毁不崩溃；`removeEventListener(type, fn)` 仅移除匹配项 |

## 7. 不做什么（YAGNI）

- 不重构 `EventDispatcher::FireListeners` 的 token 化（保持 type+capture 匹配的现状）
- 不为 `enum_serialization` 添加反向 `CssStringToEnum`（写路径已由 CSS parser 覆盖）
- 不引入 `ListenerToken` 强类型 wrapper（u64 足够；YAGNI）
- 不改 C API（`vx_view_*`）以暴露 ListenerToken
