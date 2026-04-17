# 消化关键技术债务 实现计划

**目标：** 一次性清理 4 条 P1 技术债（#45 dom_bindings 全局状态 / #46 StyleGetProp 读路径 / #48 hb_font 缓存 / #50 addEventListener 生命周期），不改变对外 API 行为。

**架构：** 每条债务一个独立 Phase 并独立提交，保证单条可回退。Phase 1 最先（改造 `DomBindings` 骨架为 pimpl），Phase 2 与 Phase 3 依赖 Phase 1 的 `InstanceData` 结构，Phase 4 完全独立于 script 模块。

**技术栈：** C++17, CMake, QuickJS-NG v0.14.0, FreeType, HarfBuzz, GoogleTest。

**复杂度级别：** Level 3（需要设计决策，涉及 script + text 两个子系统）。

**任务 ID：** TASK-20260418-01

**设计规格：** `docs/specs/2026-04-18-tech-debt-dom-bindings-design.md`

**分支：** `feature/TASK-20260418-01-tech-debt`（从 `main`）

---

## 全局约定

- **构建目录**：`build/`（已存在，用上一次 CMake 配置）
- **构建命令**：`cmake --build build -j$(nproc) --target <test_target>`
- **测试命令**：`ctest --test-dir build --output-on-failure -R <regex>`
- **完整回归**：`ctest --test-dir build --output-on-failure`
- **lint 检查**：`clang-tidy` 已由 `.clang-tidy` 管理，CMake 配置默认不跑，可选 `run-clang-tidy`
- **所有 `git commit` 采用 HEREDOC 形式**

---

## 文件结构

本次任务修改/创建的文件清单：

| 路径 | 职责 | 变更类型 |
|------|------|---------|
| `veloxa/script/dom_bindings.h` | DomBindings 公共接口（pimpl 后） | 修改 |
| `veloxa/script/dom_bindings.cc` | DomBindings 实现 + QuickJS 绑定 | 修改（较大） |
| `veloxa/text/font_manager.h` | FontManager 接口（新增 `GetHbFont`） | 修改 |
| `veloxa/text/font_manager.cc` | FontManager 实现（hb_font 缓存） | 修改 |
| `veloxa/graphics/software/software_canvas.cc` | DrawText 调用 `GetHbFont` 替代 `hb_ft_font_create_referenced` | 修改 |
| `tests/script/dom_bindings_test.cc` | 新增 ~9 个测试 | 修改 |
| `tests/text/font_manager_test.cc` | 新增 3 个测试 | 修改 |

**[共享文件] 标注**：
- **无** CMakeLists.txt 变更（新增文件均为 .h/.cc 修改；不新建编译单元）
- **无** vcpkg/依赖变更（HarfBuzz 已在 `vx_text` 链接）

**[影响前序测试] 标注**：
- Phase 1 pimpl 改造 → 现有 12 个 `dom_bindings_test` 必须零修改通过（DomBindings public API 未变）
- Phase 4 `GetHbFont` → `software_canvas` 已有集成测试必须零修改通过

---

## Phase 0 — 基线验证（5 分钟）

### 任务 0：确认当前主分支状态 & 创建特性分支

- [ ] **步骤 1：切换到 main 并同步**
  运行：
  ```bash
  cd /home/qihooz/code/loong-veloxa
  git checkout main
  git status
  ```
  预期：`On branch main`，`nothing to commit, working tree clean`

- [ ] **步骤 2：创建并切换到特性分支**
  运行：
  ```bash
  git checkout -b feature/TASK-20260418-01-tech-debt
  ```
  预期：`Switched to a new branch 'feature/TASK-20260418-01-tech-debt'`

- [ ] **步骤 3：运行完整基线测试**
  运行：
  ```bash
  cmake --build build -j$(nproc) 2>&1 | tail -20
  ctest --test-dir build --output-on-failure 2>&1 | tail -30
  ```
  预期：全部通过（作为 Phase 1+ 的回归基线）

- [ ] **步骤 4：专门记录 Phase 1 影响范围的基线**
  运行：
  ```bash
  ctest --test-dir build --output-on-failure -R "dom_bindings_test|font_manager_test|software_canvas_test" 2>&1 | tail -20
  ```
  预期：全部 PASS。记录测试数量（预期：`dom_bindings_test` 12 个，`font_manager_test` 5 个，`software_canvas_test` 若干）

---

## Phase 1 — #45 DomBindings 全局状态迁移 + pimpl 改造

**范围**：`dom_bindings.h` + `dom_bindings.cc`
**依赖**：无
**提交单元**：单一 commit `refactor(script): migrate DomBindings globals to pimpl InstanceData (#45)`

### 任务 1.1：先加新测试（保护性）— `JSClassIdStableAcrossBindings` [TDD]

**文件：**
- 测试：`tests/script/dom_bindings_test.cc`（追加新 TEST_F）

- [ ] **步骤 1：在 dom_bindings_test.cc 文件末尾（namespace 内）追加测试**

```cpp
// 在 namespace vx::script { 内，TEST_F(DomBindingsTest, AddEventListenerMultipleTypes) 之后追加：

TEST(DomBindingsLifecycleTest, JSClassIdStableAcrossBindings) {
  QuickjsEngine engine;
  ASSERT_TRUE(engine.Init().ok());

  dom::Document doc1;
  event::EventManager em1;
  auto* div1 = doc1.CreateElement(dom::TagId::kDiv);
  div1->set_id(InternedString::Intern("a"));
  doc1.AppendChild(div1);

  DomBindings b1;
  b1.Bind(engine.context(), &doc1, &em1);
  auto r1 = engine.EvalGlobal(
      "document.getElementById('a').tagName", "t1.js");
  ASSERT_TRUE(r1.ok());
  EXPECT_EQ(r1.value(), "div");
  b1.Unbind();

  // Rebind second DomBindings on same engine/context
  dom::Document doc2;
  event::EventManager em2;
  auto* span2 = doc2.CreateElement(dom::TagId::kSpan);
  span2->set_id(InternedString::Intern("b"));
  doc2.AppendChild(span2);

  DomBindings b2;
  b2.Bind(engine.context(), &doc2, &em2);
  auto r2 = engine.EvalGlobal(
      "document.getElementById('b').tagName", "t2.js");
  ASSERT_TRUE(r2.ok());
  EXPECT_EQ(r2.value(), "span");
  b2.Unbind();

  engine.Shutdown();
}

TEST(DomBindingsLifecycleTest, MultipleInstancesIndependentDocuments) {
  // 使用两个独立 QuickjsEngine 验证实例间不共享 document 状态
  QuickjsEngine eng1, eng2;
  ASSERT_TRUE(eng1.Init().ok());
  ASSERT_TRUE(eng2.Init().ok());

  dom::Document doc1, doc2;
  event::EventManager em1, em2;

  auto* e1 = doc1.CreateElement(dom::TagId::kDiv);
  e1->set_id(InternedString::Intern("only-in-1"));
  doc1.AppendChild(e1);

  auto* e2 = doc2.CreateElement(dom::TagId::kSpan);
  e2->set_id(InternedString::Intern("only-in-2"));
  doc2.AppendChild(e2);

  DomBindings b1, b2;
  b1.Bind(eng1.context(), &doc1, &em1);
  b2.Bind(eng2.context(), &doc2, &em2);

  auto r1 = eng1.EvalGlobal(
      "document.getElementById('only-in-2') === null ? 'null' : 'found'",
      "cross.js");
  ASSERT_TRUE(r1.ok());
  EXPECT_EQ(r1.value(), "null");

  auto r2 = eng2.EvalGlobal(
      "document.getElementById('only-in-1') === null ? 'null' : 'found'",
      "cross.js");
  ASSERT_TRUE(r2.ok());
  EXPECT_EQ(r2.value(), "null");

  b1.Unbind(); b2.Unbind();
  eng1.Shutdown(); eng2.Shutdown();
}
```

- [ ] **步骤 2：确认新测试在旧实现下能通过 or 暴露问题**
  运行：
  ```bash
  cmake --build build -j$(nproc) --target dom_bindings_test
  ctest --test-dir build --output-on-failure -R dom_bindings_test
  ```
  预期：
  - `JSClassIdStableAcrossBindings` 可能 PASS（当前 JSClassID 全局，不会重新分配）
  - `MultipleInstancesIndependentDocuments` **可能 FAIL**（`g_bound_doc` 被 b2 覆盖，b1 的查询会错乱）—— 这证明 #45 全局状态的问题

  **这是有意的设计：前置测试暴露旧实现的 bug，Phase 1 修复后使其转 PASS。**

  如果 `MultipleInstancesIndependentDocuments` 意外 PASS（可能因为 `b1.Unbind()` 在 Bind b2 前已清理），改写为交错 Bind：
  ```cpp
  // 交错场景：b1 与 b2 同时活跃
  b1.Bind(eng1.context(), &doc1, &em1);
  b2.Bind(eng2.context(), &doc2, &em2);
  // 现在在 b1 活跃时查询 —— 旧实现下 g_bound_doc 已被 b2 覆盖为 doc2
  auto r1 = eng1.EvalGlobal("document.getElementById('only-in-1') === null ? 'null' : 'found'", "t.js");
  EXPECT_EQ(r1.value(), "found");  // 旧实现会 FAIL（返回 "null"，因为 g_bound_doc 指向 doc2）
  ```

  记录哪个测试当前 FAIL，作为 Phase 1 修复的断言。

### 任务 1.2：pimpl 改造 `DomBindings` [TDD 的 GREEN 阶段]

**文件：**
- 修改：`veloxa/script/dom_bindings.h`
- 修改：`veloxa/script/dom_bindings.cc`

- [ ] **步骤 1：修改 `dom_bindings.h`（只动 private 部分）**

替换原文件为：

```cpp
#ifndef VELOXA_SCRIPT_DOM_BINDINGS_H_
#define VELOXA_SCRIPT_DOM_BINDINGS_H_

#include <memory>

#include "veloxa/core/dom/document.h"
#include "veloxa/core/event/event_manager.h"

struct JSContext;

namespace vx::script {

class DomBindings {
 public:
  DomBindings();
  ~DomBindings();

  DomBindings(const DomBindings&) = delete;
  DomBindings& operator=(const DomBindings&) = delete;

  void Bind(JSContext* ctx, dom::Document* doc,
            event::EventManager* em = nullptr);
  void Unbind();

  bool bound() const;
  event::EventManager* event_manager() const;
  dom::Document* document() const;
  JSContext* context() const;

 private:
  struct InstanceData;
  std::unique_ptr<InstanceData> data_;
};

}  // namespace vx::script

#endif  // VELOXA_SCRIPT_DOM_BINDINGS_H_
```

**关键点：**
- `~DomBindings();` 非默认（必须在 .cc 中定义）以避免不完整类型删除问题
- 移除所有 public 返回类型对 `ctx_`/`doc_`/`em_` 的直接引用（改为方法转发）

- [ ] **步骤 2：修改 `dom_bindings.cc`：定义 `InstanceData` 并迁移所有全局状态**

在 `dom_bindings.cc` 顶部（`extern "C" { #include "quickjs.h" }` 之后，`namespace vx::script` 之前）添加 InstanceData 定义。然后在文件内部完成以下迁移：

**(a) 将 `g_element_class_id` / `g_style_class_id` 改为 `s_` 静态，并用 `JS_IsRegisteredClass` 幂等注册：**

```cpp
// 原 line 84
namespace {
static JSClassID s_element_class_id = 0;
static JSClassID s_style_class_id = 0;
// ...
}  // namespace
```

**(b) 替换 `RegisterStyleClass`（原 line 438）和 `RegisterElementClass`（原 line 457）的 JSClassID 分配代码：**

```cpp
void RegisterStyleClass(JSContext* ctx) {
  JSRuntime* rt = JS_GetRuntime(ctx);
  if (s_style_class_id == 0) {
    JS_NewClassID(rt, &s_style_class_id);
  }
  if (!JS_IsRegisteredClass(rt, s_style_class_id)) {
    JS_NewClass(rt, s_style_class_id, &g_style_class_def);
  }
  // ... 其余不变（使用 s_style_class_id 替代 g_style_class_id）
}
```
对 `RegisterElementClass` 做对称修改。

**(c) 删除 `TrackedCallbacks g_tracked_callbacks;`（原 line 289）和 `dom::Document* g_bound_doc = nullptr;`（原 line 420）。**

**(d) 在 namespace vx::script 内部（匿名 namespace 结束后、`DomBindings::~DomBindings()` 之前）定义 `DomBindings::InstanceData`：**

```cpp
struct DomBindings::InstanceData {
  JSContext* ctx = nullptr;
  dom::Document* doc = nullptr;
  event::EventManager* em = nullptr;
  Vector<dom::Element*> listener_elements;  // 见 Phase 2
  struct {
    JSContext* ctx = nullptr;
    Vector<JSValue> callbacks;
    void Track(JSValue v) { callbacks.push_back(v); }
    void FreeAll() {
      if (!ctx) return;
      for (usize i = 0; i < callbacks.size(); ++i) {
        JS_FreeValue(ctx, callbacks[i]);
      }
      callbacks.clear();
      ctx = nullptr;
    }
  } tracked_callbacks;
};
```

**(e) 修改所有通过 `g_bound_doc` / `g_tracked_callbacks` 访问的回调函数，改为 `GetBindings(ctx)->data_->...`。**

因为 InstanceData 是 DomBindings 的私有嵌套，匿名 namespace 内的回调函数访问需要通过一个受限的访问器。为最小改动：

在 `class DomBindings` 内部（header 或 cc 文件内）添加 friend 声明或**更简洁**：将回调所需的访问通过**公开的访问器方法**暴露。由于回调函数与 InstanceData 都在 dom_bindings.cc 中，**用 helper 函数**：

```cpp
// 文件内，匿名 namespace 外，class DomBindings 内新增 private 方法（需要在 header 中暴露）
// 但为了不扩大 public API，改用 friend 机制：

// 在 dom_bindings.h 的 class DomBindings 内 private: 部分追加:
//   friend struct DomBindingsInternal;

// 然后在 dom_bindings.cc 匿名 namespace 内定义:
struct DomBindingsInternal {
  static DomBindings::InstanceData* Data(DomBindings* b) { return b->data_.get(); }
};

DomBindings::InstanceData* GetData(JSContext* ctx) {
  auto* b = GetBindings(ctx);
  return b ? DomBindingsInternal::Data(b) : nullptr;
}
```

更新 header 对应增加 `friend struct DomBindingsInternal;`。

- [ ] **步骤 3：修改 `DocGetElementById`（原 line 422）**

```cpp
JSValue DocGetElementById(JSContext* ctx, JSValueConst /*this_val*/, int argc,
                          JSValueConst* argv) {
  auto* data = GetData(ctx);
  if (argc < 1 || !data || !data->doc) return JS_NULL;

  const char* id_str = JS_ToCString(ctx, argv[0]);
  if (!id_str) return JS_NULL;

  dom::Element* el = FindElementById(data->doc, StringView(id_str));
  JS_FreeCString(ctx, id_str);

  if (!el) return JS_NULL;
  return WrapElement(ctx, el);
}
```

- [ ] **步骤 4：修改 `ElementAddEventListener`（原 line 293）**

```cpp
JSValue ElementAddEventListener(JSContext* ctx, JSValueConst this_val, int argc,
                                JSValueConst* argv) {
  auto* el = GetElement(ctx, this_val);
  if (!el || argc < 2) return JS_UNDEFINED;

  const char* type_str = JS_ToCString(ctx, argv[0]);
  if (!type_str) return JS_UNDEFINED;

  event::EventType event_type;
  bool valid = MapJsEventName(type_str, &event_type);
  JS_FreeCString(ctx, type_str);
  if (!valid) return JS_UNDEFINED;

  JSValue callback = JS_DupValue(ctx, argv[1]);
  auto* data = GetData(ctx);
  if (!data || !data->em) {
    JS_FreeValue(ctx, callback);
    return JS_UNDEFINED;
  }

  data->tracked_callbacks.Track(callback);
  // listener_elements 去重插入（Phase 2 才有实际用途，但这里先加以避免返工）
  bool found = false;
  for (usize i = 0; i < data->listener_elements.size(); ++i) {
    if (data->listener_elements[i] == el) { found = true; break; }
  }
  if (!found) data->listener_elements.push_back(el);

  JSContext* captured_ctx = ctx;
  data->em->AddEventListener(
      el, event_type,
      [captured_ctx, callback](event::DOMEvent& event) mutable {
        JSValue js_event = JS_NewObject(captured_ctx);
        JS_SetPropertyStr(captured_ctx, js_event, "type",
                          JS_NewString(captured_ctx,
                                       EventTypeToString(event.input.type)));
        JSValue ret =
            JS_Call(captured_ctx, callback, JS_UNDEFINED, 1, &js_event);
        JS_FreeValue(captured_ctx, ret);
        JS_FreeValue(captured_ctx, js_event);
      });

  return JS_UNDEFINED;
}
```

- [ ] **步骤 5：修改 `ElementRemoveEventListener`（原 line 332）**

```cpp
JSValue ElementRemoveEventListener(JSContext* ctx, JSValueConst this_val,
                                   int /*argc*/, JSValueConst* /*argv*/) {
  auto* el = GetElement(ctx, this_val);
  auto* data = GetData(ctx);
  if (!el || !data || !data->em) return JS_UNDEFINED;
  data->em->RemoveEventListeners(el);
  // 同步从 listener_elements 移除
  for (usize i = 0; i < data->listener_elements.size(); ++i) {
    if (data->listener_elements[i] == el) {
      data->listener_elements[i] =
          data->listener_elements[data->listener_elements.size() - 1];
      data->listener_elements.pop_back();
      break;
    }
  }
  return JS_UNDEFINED;
}
```

- [ ] **步骤 6：重写 `DomBindings` 方法实现（.cc 文件底部）**

```cpp
DomBindings::DomBindings() : data_(std::make_unique<InstanceData>()) {}
DomBindings::~DomBindings() { Unbind(); }

bool DomBindings::bound() const { return data_->ctx != nullptr; }
event::EventManager* DomBindings::event_manager() const { return data_->em; }
dom::Document* DomBindings::document() const { return data_->doc; }
JSContext* DomBindings::context() const { return data_->ctx; }

void DomBindings::Bind(JSContext* ctx, dom::Document* doc,
                       event::EventManager* em) {
  Unbind();
  data_->ctx = ctx;
  data_->doc = doc;
  data_->em = em;

  JS_SetContextOpaque(ctx, this);
  data_->tracked_callbacks.ctx = ctx;

  RegisterStyleClass(ctx);
  RegisterElementClass(ctx);
  RegisterDocumentObject(ctx, doc);
}

void DomBindings::Unbind() {
  // Phase 2 会在此处插入 RemoveEventListeners 循环
  data_->listener_elements.clear();
  data_->tracked_callbacks.FreeAll();
  if (data_->ctx) {
    JS_SetContextOpaque(data_->ctx, nullptr);
    JSValue global = JS_GetGlobalObject(data_->ctx);
    JSAtom doc_atom = JS_NewAtom(data_->ctx, "document");
    JS_DeleteProperty(data_->ctx, global, doc_atom, 0);
    JS_FreeAtom(data_->ctx, doc_atom);
    JS_FreeValue(data_->ctx, global);
  }
  data_->ctx = nullptr;
  data_->doc = nullptr;
  data_->em = nullptr;
}
```

注：`RegisterDocumentObject` 已不再需要参数外的设置，但其原实现写 `g_bound_doc = doc;` 这一行要删除。

- [ ] **步骤 7：修改 `RegisterDocumentObject`（删除 g_bound_doc 赋值）**

```cpp
void RegisterDocumentObject(JSContext* ctx, dom::Document* /*doc*/) {
  // doc 已存储在 DomBindings::InstanceData，此处仅注册 JS 全局对象
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue doc_obj = JS_NewObject(ctx);

  JS_SetPropertyStr(
      ctx, doc_obj, "getElementById",
      JS_NewCFunction(ctx, DocGetElementById, "getElementById", 1));

  JS_SetPropertyStr(ctx, global, "document", doc_obj);
  JS_FreeValue(ctx, global);
}
```

- [ ] **步骤 8：编译并跑测试**

  ```bash
  cmake --build build -j$(nproc) --target dom_bindings_test 2>&1 | tail -20
  ctest --test-dir build --output-on-failure -R dom_bindings_test
  ```
  预期：
  - 原 12 个测试全部 PASS（pimpl 改造不影响行为）
  - 新增 2 个 `DomBindingsLifecycleTest` 测试 PASS
  - `MultipleInstancesIndependentDocuments` 从 Phase 1.1 的 FAIL 转为 PASS（如果当时记录了 FAIL）

- [ ] **步骤 9：全量回归**

  ```bash
  cmake --build build -j$(nproc) 2>&1 | tail -5
  ctest --test-dir build --output-on-failure 2>&1 | tail -10
  ```
  预期：全部 PASS

- [ ] **步骤 10：提交 Phase 1**

```bash
git add veloxa/script/dom_bindings.h veloxa/script/dom_bindings.cc \
  tests/script/dom_bindings_test.cc
git commit -m "$(cat <<'EOF'
refactor(script): migrate DomBindings globals to pimpl InstanceData (#45)

Move g_bound_doc, g_tracked_callbacks into DomBindings::InstanceData
accessible via JS_GetContextOpaque. JSClassID (s_element_class_id,
s_style_class_id) kept as file-scope statics with idempotent
JS_IsRegisteredClass init — they are QuickJS process-wide identifiers,
not instance state.

dom_bindings.h now uses pimpl to avoid leaking quickjs.h types; a
friend struct DomBindingsInternal bridges free-function callbacks
to InstanceData without widening public API.

Adds JSClassIdStableAcrossBindings and
MultipleInstancesIndependentDocuments regression tests covering the
previously UB behavior where g_bound_doc would be overwritten by a
second concurrent binding.

TASK-20260418-01.
EOF
)"
```

---

## Phase 2 — #50 addEventListener 生命周期修复

**范围**：`dom_bindings.cc`（仅 `Unbind` 函数） + 新测试
**依赖**：Phase 1（`data_->listener_elements` + `data_->em` 已就绪）
**提交单元**：单一 commit `fix(script): call RemoveEventListeners before freeing JS callbacks (#50)`

### 任务 2.1：先加失败测试 — UAF 防护 [TDD]

- [ ] **步骤 1：在 `dom_bindings_test.cc` 追加测试**

```cpp
// 追加在 namespace vx::script 内（任务 1.1 的测试之后）

TEST(DomBindingsLifecycleTest, UnbindRemovesEventListeners) {
  QuickjsEngine engine;
  ASSERT_TRUE(engine.Init().ok());

  dom::Document doc;
  event::EventManager em;
  auto* span = doc.CreateElement(dom::TagId::kSpan);
  span->set_id(InternedString::Intern("btn"));
  auto* text = doc.CreateText(String("Click"));
  span->AppendChild(text);
  doc.AppendChild(span);

  DomBindings bindings;
  bindings.Bind(engine.context(), &doc, &em);

  // 在 JS 中注册一个 side-effect callback（设置全局标志）
  auto r1 = engine.EvalGlobal(
      "globalThis.__fired = 0;"
      "var el = document.getElementById('btn');"
      "el.addEventListener('pointerdown', function(e) { globalThis.__fired++; });"
      "'ok'",
      "t.js");
  ASSERT_TRUE(r1.ok());
  EXPECT_EQ(r1.value(), "ok");

  // Unbind 前先触发一次确认 callback 工作
  // 注：HandleInput 需 LayoutBox*，这里不方便走完整路径；改用 EventManager 内部
  // API 触发。如果 EventManager 没有直接 dispatch API，改写为下面的"不 crash" 测试。

  bindings.Unbind();

  // Unbind 后再次 HandleInput：不应 crash，也不应调用已释放的 callback
  // 由于 Unbind 已清空 em 中的 listener，HandleInput 即使被调用也应无副作用
  // 这里我们不调用 HandleInput（需要 LayoutBox），而是直接验证：
  //   Bind → addListener → Unbind → Rebind 同一 ctx → 新 JS 环境干净
  bindings.Bind(engine.context(), &doc, &em);
  auto r2 = engine.EvalGlobal(
      "typeof globalThis.__fired",  // 全局变量仍在（JS 全局没清）
      "t2.js");
  ASSERT_TRUE(r2.ok());
  // 只验证没 crash；__fired 的可见性不是重点
  bindings.Unbind();
  engine.Shutdown();
}

TEST(DomBindingsLifecycleTest, UnbindClearsListenerElements) {
  // 白盒：Bind → addListener → Unbind → Rebind → addListener 不累积旧 element
  QuickjsEngine engine;
  ASSERT_TRUE(engine.Init().ok());

  dom::Document doc;
  event::EventManager em;
  auto* e1 = doc.CreateElement(dom::TagId::kDiv);
  e1->set_id(InternedString::Intern("x"));
  doc.AppendChild(e1);

  DomBindings bindings;
  bindings.Bind(engine.context(), &doc, &em);
  engine.EvalGlobal(
      "document.getElementById('x').addEventListener('pointerdown', function(){});",
      "t.js");
  bindings.Unbind();

  // em 中该 element 的 listener 必须已被清除
  // 通过重新 Bind + 第二次 AddEventListener，验证 em 的状态干净
  event::EventManager em2;  // 全新 em 来避免旧 em 持有残留
  bindings.Bind(engine.context(), &doc, &em2);
  // 若 Phase 1 的 Unbind 未清理 em 的 listener，此 em2 是新的没事；
  // 真正的检查：对 em（旧的）调用 HandleInput 不应该通过残留 lambda 调用已释放 callback。
  // 由于 HandleInput 需要 LayoutBox，这里用更白盒的手段：
  // 直接检查 DomBindings 公开的 bound() 状态切换正确
  EXPECT_TRUE(bindings.bound());
  bindings.Unbind();
  EXPECT_FALSE(bindings.bound());
  engine.Shutdown();
}
```

**注**：完整的 UAF 复现需要 `HandleInput`，但 `HandleInput` 需要 `LayoutBox*`。我们采用**行为等价**的验证：Unbind 后 EventDispatcher 中应无该 element 的 listener。如果能从 `EventManager` 读取 listener 数，可做更强断言；查 `event_manager.h` 发现没有 `HasListener` 公开 API，所以用白盒方案。

**备选**：如果发现 `EventDispatcher` 有 `listener_count_for` 之类 API，改为直接断言；否则使用上述间接验证。实施时根据实际代码决定。

- [ ] **步骤 2：检查 EventManager/EventDispatcher API 是否有 listener 计数**

运行：
```bash
grep -rn "listener_count\|HasListener\|listeners_" veloxa/core/event/ | head
```

如果有 public 查询 API，改写测试为直接断言 `em.listener_count(el) == 0`。若没有，保持白盒测试，在 Phase 2 commit message 注明此限制。

- [ ] **步骤 3：运行新测试验证当前行为**

```bash
cmake --build build -j$(nproc) --target dom_bindings_test
ctest --test-dir build --output-on-failure -R "DomBindingsLifecycleTest"
```

预期：当前（Phase 1 之后但 Phase 2 之前）可能 PASS（因为白盒测试不直接触发 UAF）。这是可接受的 —— Phase 2 主要通过 Unbind 逻辑变更 + 运行时内存安全保证修复问题，不依赖测试 100% 复现 UAF。

### 任务 2.2：实施 Unbind 顺序修复

- [ ] **步骤 1：修改 `DomBindings::Unbind`（.cc 底部）**

在 Phase 1 写就的 Unbind 基础上，插入 RemoveEventListeners 循环：

```cpp
void DomBindings::Unbind() {
  // 1. 先从 EventManager 移除所有 lambda（触发 lambda 析构，此时 callback 仍有效）
  if (data_->em) {
    for (usize i = 0; i < data_->listener_elements.size(); ++i) {
      data_->em->RemoveEventListeners(data_->listener_elements[i]);
    }
  }
  data_->listener_elements.clear();

  // 2. 再释放 JS callback（顺序关键：先拆 lambda，后 Free）
  data_->tracked_callbacks.FreeAll();

  // 3. 清理 document global 与 context opaque
  if (data_->ctx) {
    JS_SetContextOpaque(data_->ctx, nullptr);
    JSValue global = JS_GetGlobalObject(data_->ctx);
    JSAtom doc_atom = JS_NewAtom(data_->ctx, "document");
    JS_DeleteProperty(data_->ctx, global, doc_atom, 0);
    JS_FreeAtom(data_->ctx, doc_atom);
    JS_FreeValue(data_->ctx, global);
  }
  data_->ctx = nullptr;
  data_->doc = nullptr;
  data_->em = nullptr;
}
```

- [ ] **步骤 2：编译 & 跑测试**

```bash
cmake --build build -j$(nproc) --target dom_bindings_test
ctest --test-dir build --output-on-failure -R dom_bindings_test
```

预期：全部 PASS（包括 Phase 1 + Phase 2 新测试）

- [ ] **步骤 3：全量回归**

```bash
ctest --test-dir build --output-on-failure 2>&1 | tail -10
```
预期：无回归

- [ ] **步骤 4：验证测试有效性（注入缺陷）**

临时把 Unbind 中的 `RemoveEventListeners` 循环注释掉，运行 `UnbindClearsListenerElements` + AddressSanitizer（如有 CI 开启）。
**省略条件**：如果项目 CI 未配置 ASan，改为代码审查验证（检查 Unbind 顺序正确即可），在 commit message 注明。

恢复代码。

- [ ] **步骤 5：提交 Phase 2**

```bash
git add veloxa/script/dom_bindings.cc tests/script/dom_bindings_test.cc
git commit -m "$(cat <<'EOF'
fix(script): call RemoveEventListeners before freeing JS callbacks (#50)

DomBindings::Unbind previously freed JSValue callbacks via
g_tracked_callbacks.FreeAll() but never removed the lambdas from
EventManager. A subsequent HandleInput could invoke a lambda whose
captured callback had been freed — use-after-free.

Fix: track each Element that received an addEventListener call in
InstanceData::listener_elements, then in Unbind: (1) iterate and call
em_->RemoveEventListeners(el) to destroy all lambdas, (2) only then
FreeAll() the tracked JSValue callbacks.

ElementRemoveEventListener also syncs listener_elements so Unbind
doesn't redundantly operate on already-cleared elements.

Known remaining constraint: hosts must destroy DomBindings before
EventManager. Out of scope for this task (recorded in techContext).

TASK-20260418-01.
EOF
)"
```

---

## Phase 3 — #46 StyleGetProp 读路径

**范围**：`dom_bindings.cc`（新增 `SerializeCssValue` + 重写 `StyleGetProp`） + 新测试
**依赖**：Phase 1（InstanceData 已就绪）
**提交单元**：单一 commit `feat(script): implement StyleGetProp read path for length/color/auto/number (#46)`

### 任务 3.1：先加失败测试 [TDD]

- [ ] **步骤 1：在 `dom_bindings_test.cc` 追加测试**

```cpp
// 追加在 namespace vx::script 内

TEST_F(DomBindingsTest, StyleGetBackgroundColor) {
  auto r = engine_.EvalGlobal(
      "var el = document.getElementById('box');"
      "el.style.backgroundColor = 'red';"
      "el.style.backgroundColor",
      "t.js");
  ASSERT_TRUE(r.ok());
  // red = rgba(255, 0, 0, 255)
  EXPECT_EQ(r.value(), "rgba(255, 0, 0, 255)");
}

TEST_F(DomBindingsTest, StyleGetWidthPx) {
  auto r = engine_.EvalGlobal(
      "var el = document.getElementById('box');"
      "el.style.width = '100px';"
      "el.style.width",
      "t.js");
  ASSERT_TRUE(r.ok());
  EXPECT_EQ(r.value(), "100px");
}

TEST_F(DomBindingsTest, StyleGetAuto) {
  auto r = engine_.EvalGlobal(
      "var el = document.getElementById('box');"
      "el.style.width = 'auto';"
      "el.style.width",
      "t.js");
  ASSERT_TRUE(r.ok());
  EXPECT_EQ(r.value(), "auto");
}

TEST_F(DomBindingsTest, StyleGetPercent) {
  auto r = engine_.EvalGlobal(
      "var el = document.getElementById('box');"
      "el.style.height = '50%';"
      "el.style.height",
      "t.js");
  ASSERT_TRUE(r.ok());
  EXPECT_EQ(r.value(), "50%");
}

TEST_F(DomBindingsTest, StyleGetOpacity) {
  auto r = engine_.EvalGlobal(
      "var el = document.getElementById('box');"
      "el.style.opacity = '0.5';"
      "el.style.opacity",
      "t.js");
  ASSERT_TRUE(r.ok());
  EXPECT_EQ(r.value(), "0.5");
}

TEST_F(DomBindingsTest, StyleGetUnsetReturnsEmpty) {
  auto r = engine_.EvalGlobal(
      "document.getElementById('box').style.color",
      "t.js");
  ASSERT_TRUE(r.ok());
  EXPECT_EQ(r.value(), "");
}

TEST_F(DomBindingsTest, StyleGetDisplayEnumCurrentlyEmpty) {
  // display is Enum — StyleGetProp returns "" (stub, see #46 remaining TODO)
  auto r = engine_.EvalGlobal(
      "var el = document.getElementById('box');"
      "el.style.display = 'block';"
      "el.style.display",
      "t.js");
  ASSERT_TRUE(r.ok());
  EXPECT_EQ(r.value(), "");  // 行为文档：Enum 暂未实现读路径
}
```

- [ ] **步骤 2：运行测试确认 FAIL**

```bash
cmake --build build -j$(nproc) --target dom_bindings_test
ctest --test-dir build --output-on-failure -R "StyleGet"
```
预期：
- `StyleGetBackgroundColor` / `StyleGetWidthPx` / `StyleGetAuto` / `StyleGetPercent` / `StyleGetOpacity` **FAIL**（当前返回 `""`）
- `StyleGetUnsetReturnsEmpty` / `StyleGetDisplayEnumCurrentlyEmpty` **PASS**（当前实现本就返回 `""`）

### 任务 3.2：实现 `SerializeCssValue` + 重写 `StyleGetProp`

- [ ] **步骤 1：在 `dom_bindings.cc` 匿名 namespace 中添加序列化辅助**

位置：紧挨 `StyleGetProp`（原 line 151）之前。

```cpp
// ----- CSS value → string serialization (read path for StyleGetProp) -----

const char* UnitSuffix(css::Unit u) {
  switch (u) {
    case css::Unit::kPx: return "px";
    case css::Unit::kEm: return "em";
    case css::Unit::kRem: return "rem";
    case css::Unit::kPercent: return "%";
    case css::Unit::kVw: return "vw";
    case css::Unit::kVh: return "vh";
    default: return "";
  }
}

void AppendNumber(f32 v, String* out) {
  char buf[32];
  // %g gives compact form: 0.5 not 0.500000; 100 not 100.000000
  int n = std::snprintf(buf, sizeof(buf), "%g", static_cast<double>(v));
  if (n > 0) out->append(buf, static_cast<usize>(n));
}

void SerializeCssValue(const css::CssValue& v, String* out) {
  switch (v.type) {
    case css::ValueType::kLength:
      AppendNumber(v.number, out);
      out->append(UnitSuffix(v.unit));
      break;
    case css::ValueType::kColor: {
      u32 rgba = v.color;  // CSS stores as 0xRRGGBBAA
      u8 r = static_cast<u8>((rgba >> 24) & 0xFF);
      u8 g = static_cast<u8>((rgba >> 16) & 0xFF);
      u8 b = static_cast<u8>((rgba >> 8) & 0xFF);
      u8 a = static_cast<u8>(rgba & 0xFF);
      char buf[48];
      int n = std::snprintf(buf, sizeof(buf),
                            "rgba(%u, %u, %u, %u)", r, g, b, a);
      if (n > 0) out->append(buf, static_cast<usize>(n));
      break;
    }
    case css::ValueType::kNumber:
      AppendNumber(v.number, out);
      break;
    case css::ValueType::kAuto:
      out->append("auto");
      break;
    case css::ValueType::kInherit:
      out->append("inherit");
      break;
    case css::ValueType::kInitial:
      out->append("initial");
      break;
    case css::ValueType::kEnum:
    case css::ValueType::kNone:
      // Enum not implemented — caller gets "" (see #46 residual TODO).
      break;
  }
}
```

**关键验证点**：在步骤 1 之前，先 `Read` 一下 `veloxa/foundation/strings/string.h` 确认 `String` 有 `append(const char*, usize)` 和 `append(const char*)` 成员；若 API 不同则调整。

- [ ] **步骤 2：重写 `StyleGetProp`（原 line 151）**

```cpp
JSValue StyleGetProp(JSContext* ctx, JSValueConst this_val, int magic) {
  auto* so = GetStyleOpaque(this_val);
  if (!so || !so->element || magic < 0 || magic >= kStyleMappingCount) {
    return JS_NewString(ctx, "");
  }

  const auto* decls = so->element->inline_declarations();
  if (!decls) return JS_NewString(ctx, "");

  css::PropertyId target = kStyleMappings[magic].prop;
  for (usize i = 0; i < decls->size(); ++i) {
    const auto& d = (*decls)[i];
    if (d.property == target) {
      String out;
      SerializeCssValue(d.value, &out);
      return JS_NewStringLen(ctx, out.data(), out.size());
    }
  }
  return JS_NewString(ctx, "");
}
```

- [ ] **步骤 3：编译验证**

```bash
cmake --build build -j$(nproc) --target dom_bindings_test 2>&1 | tail -10
```
预期：编译通过。

**如果编译失败**，常见问题：
- `String::append(const char*)` 不存在 → 改为 `append(StringView(s))` 或循环 `push_back`
- `<cstdio>` 未 include → 在 .cc 顶部加 `#include <cstdio>`
- `String::data()` 返回 `const char*` 但 `JS_NewStringLen` 需 `const char*` → 应已兼容

- [ ] **步骤 4：运行测试**

```bash
ctest --test-dir build --output-on-failure -R dom_bindings_test
```
预期：
- 所有 `StyleGet*` 测试 PASS
- 原 `StyleSetBackgroundColor` 测试仍 PASS
- 全部 dom_bindings_test 测试通过

- [ ] **步骤 5：验证测试有效性**

临时把 `SerializeCssValue` 中 `case css::ValueType::kLength:` 分支改为 `break;`，确认 `StyleGetWidthPx` 和 `StyleGetPercent` 转 FAIL。恢复代码。

- [ ] **步骤 6：全量回归**

```bash
ctest --test-dir build --output-on-failure 2>&1 | tail -10
```
预期：无回归

- [ ] **步骤 7：提交 Phase 3**

```bash
git add veloxa/script/dom_bindings.cc tests/script/dom_bindings_test.cc
git commit -m "$(cat <<'EOF'
feat(script): implement StyleGetProp read path for length/color/auto/number (#46)

StyleGetProp previously returned "" unconditionally. Now walks
element->inline_declarations() for the requested PropertyId and
serializes via SerializeCssValue:

  - kLength → "100px" / "50%" / "1em" (compact %g number + unit)
  - kColor  → "rgba(r, g, b, a)" (0-255 integers)
  - kNumber → "0.5" (compact %g)
  - kAuto   → "auto"
  - kInherit/kInitial → "inherit" / "initial"
  - kEnum/kNone → "" (stub, see remaining #46 TODO)

Unset property still returns "" (matches CSSStyleDeclaration spec).
Enum properties like display remain "" until per-enum reverse-lookup
tables are added in a follow-up task.

TASK-20260418-01.
EOF
)"
```

---

## Phase 4 — #48 HarfBuzz font 缓存

**范围**：`veloxa/text/font_manager.{h,cc}` + `veloxa/graphics/software/software_canvas.cc`
**依赖**：无（与 script 模块解耦）
**提交单元**：单一 commit `perf(text): cache hb_font_t per FontHandle in FontManager (#48)`

### 任务 4.1：先加失败测试 [TDD]

- [ ] **步骤 1：在 `tests/text/font_manager_test.cc` 追加测试**

```cpp
// 追加在 namespace vx::text 内

TEST(FontManagerTest, GetHbFontReusesPerHandle) {
  FontManager fm;
  ASSERT_TRUE(fm.Init().ok());
  auto r = fm.LoadFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                       "DejaVuSans");
  ASSERT_TRUE(r.ok());
  FontHandle h = r.value();

  // 调用方契约：先 FT_Set_Pixel_Sizes，再 GetHbFont
  FT_Face face = fm.GetFace(h);
  ASSERT_NE(face, nullptr);
  FT_Set_Pixel_Sizes(face, 0, 16);
  auto* hb1 = fm.GetHbFont(h, 16);
  FT_Set_Pixel_Sizes(face, 0, 16);
  auto* hb2 = fm.GetHbFont(h, 16);
  EXPECT_NE(hb1, nullptr);
  EXPECT_EQ(hb1, hb2);  // 同 handle + 同 size 必须命中缓存

  fm.Shutdown();
}

TEST(FontManagerTest, GetHbFontHandlesSizeChange) {
  FontManager fm;
  ASSERT_TRUE(fm.Init().ok());
  auto r = fm.LoadFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                       "DejaVuSans");
  ASSERT_TRUE(r.ok());
  FontHandle h = r.value();
  FT_Face face = fm.GetFace(h);

  FT_Set_Pixel_Sizes(face, 0, 12);
  auto* hb1 = fm.GetHbFont(h, 12);

  FT_Set_Pixel_Sizes(face, 0, 24);
  auto* hb2 = fm.GetHbFont(h, 24);

  EXPECT_NE(hb1, nullptr);
  EXPECT_EQ(hb1, hb2);  // 同 handle 复用同一 hb_font 指针，但内部已 reconfigure

  fm.Shutdown();
}

TEST(FontManagerTest, GetHbFontInvalidHandle) {
  FontManager fm;
  ASSERT_TRUE(fm.Init().ok());
  EXPECT_EQ(fm.GetHbFont(kInvalidFont, 16), nullptr);
  EXPECT_EQ(fm.GetHbFont(999, 16), nullptr);
  fm.Shutdown();
}
```

需要 include：在文件顶部加 `#include <ft2build.h>` + `#include FT_FREETYPE_H` 供 `FT_Set_Pixel_Sizes` 使用。

- [ ] **步骤 2：运行测试确认 FAIL（编译失败）**

```bash
cmake --build build -j$(nproc) --target font_manager_test 2>&1 | tail -10
```
预期：编译错误 — `GetHbFont` 未声明。这确认测试有效。

### 任务 4.2：扩展 `FontManager` 添加 `GetHbFont`

- [ ] **步骤 1：修改 `veloxa/text/font_manager.h`**

```cpp
#ifndef VELOXA_TEXT_FONT_MANAGER_H_
#define VELOXA_TEXT_FONT_MANAGER_H_

#include "veloxa/foundation/base/status.h"
#include "veloxa/foundation/base/types.h"
#include "veloxa/foundation/strings/string_view.h"

struct FT_LibraryRec_;
struct FT_FaceRec_;
struct hb_font_t;  // forward declare HarfBuzz font (typedef'd C struct)

namespace vx::text {

using FontHandle = u32;
static constexpr FontHandle kInvalidFont = 0;

class FontManager {
 public:
  FontManager();
  ~FontManager();

  FontManager(const FontManager&) = delete;
  FontManager& operator=(const FontManager&) = delete;

  Status Init();
  void Shutdown();
  bool initialized() const { return ft_library_ != nullptr; }

  StatusOr<FontHandle> LoadFont(StringView path, StringView family);
  FontHandle FindFont(StringView family, u16 weight = 400) const;

  FT_FaceRec_* GetFace(FontHandle handle) const;

  // Returns (and lazily creates) an hb_font_t* bound to this handle's FT_Face.
  // Caller contract: call FT_Set_Pixel_Sizes(face, 0, pixel_size) BEFORE
  // this function so HarfBuzz can observe the new metrics via
  // hb_ft_font_changed. Returns nullptr for invalid handle or if FT_Face
  // is missing.
  hb_font_t* GetHbFont(FontHandle handle, u32 pixel_size);

  usize font_count() const;

 private:
  struct FontEntry {
    FT_FaceRec_* face = nullptr;
    hb_font_t* hb_font = nullptr;
    u32 hb_pixel_size = 0;
    FontHandle handle = kInvalidFont;
    char family[64] = {};
    u16 weight = 400;
  };

  FT_LibraryRec_* ft_library_ = nullptr;
  static constexpr usize kMaxFonts = 32;
  FontEntry fonts_[kMaxFonts] = {};
  usize font_count_ = 0;
  FontHandle next_handle_ = 1;
};

}  // namespace vx::text

#endif  // VELOXA_TEXT_FONT_MANAGER_H_
```

- [ ] **步骤 2：修改 `veloxa/text/font_manager.cc`**

在文件顶部 include 块中加入 HarfBuzz 头（FreeType hook 必需）：

```cpp
#include "veloxa/text/font_manager.h"

#include <cstring>

#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#include <hb-ft.h>

namespace vx::text {
```

修改 `Shutdown` 释放 hb_font：

```cpp
void FontManager::Shutdown() {
  for (usize i = 0; i < font_count_; ++i) {
    if (fonts_[i].hb_font) {
      hb_font_destroy(fonts_[i].hb_font);
      fonts_[i].hb_font = nullptr;
    }
    if (fonts_[i].face) {
      FT_Done_Face(fonts_[i].face);
      fonts_[i].face = nullptr;
    }
  }
  font_count_ = 0;
  if (ft_library_) {
    FT_Done_FreeType(ft_library_);
    ft_library_ = nullptr;
  }
}
```

在文件末尾 `usize FontManager::font_count() const { ... }` 之后追加：

```cpp
hb_font_t* FontManager::GetHbFont(FontHandle handle, u32 pixel_size) {
  for (usize i = 0; i < font_count_; ++i) {
    if (fonts_[i].handle != handle) continue;
    FontEntry& entry = fonts_[i];
    if (!entry.face) return nullptr;

    if (!entry.hb_font) {
      entry.hb_font = hb_ft_font_create_referenced(entry.face);
      entry.hb_pixel_size = pixel_size;
    } else if (entry.hb_pixel_size != pixel_size) {
      hb_ft_font_changed(entry.hb_font);
      entry.hb_pixel_size = pixel_size;
    }
    return entry.hb_font;
  }
  return nullptr;
}
```

- [ ] **步骤 3：修改 `veloxa/text/CMakeLists.txt`（如 HarfBuzz 非 PUBLIC 链接，font_manager_test 无法找到 hb 符号）**

当前状态：
```
target_link_libraries(vx_text
  PUBLIC vx_foundation
  PRIVATE Freetype::Freetype ${HARFBUZZ_LIBRARIES}
)
```

HarfBuzz 是 PRIVATE，但现在 `font_manager.cc` 内部仍用 HarfBuzz API（Shutdown + GetHbFont），不泄漏到 header（只 forward declare）。因此**无需**改 CMake —— 测试链接 `vx_text`，HarfBuzz 符号会通过静态库传递。

**验证**：先尝试编译；如果 link 失败（undefined reference to `hb_ft_font_create_referenced`），将 `Freetype::Freetype` 和 `${HARFBUZZ_LIBRARIES}` 改为 PUBLIC（或使用 `target_include_directories(vx_text PRIVATE ${HARFBUZZ_INCLUDE_DIRS})` 已够）。

实际上测试只需要头文件可见性和符号链接。`vx_text` 是静态库，链接 `vx_text` 的目标（如 font_manager_test）会解析 `vx_text` 内部调用的 HarfBuzz 符号 —— 这需要 HarfBuzz 在测试目标的链接列表中。如果 `font_manager_test` 只链接 `vx_text`，而 HarfBuzz 是 PRIVATE，则 HarfBuzz 符号不会传递。

**决策**：为确保测试能 link，在测试链接时补充：

修改 `tests/CMakeLists.txt` line 183 附近：
```cmake
vx_add_test(font_manager_test text/font_manager_test.cc)
target_link_libraries(font_manager_test PRIVATE vx_text Freetype::Freetype ${HARFBUZZ_LIBRARIES})
target_include_directories(font_manager_test PRIVATE ${HARFBUZZ_INCLUDE_DIRS})
```

**[共享文件]** 这是对 `tests/CMakeLists.txt` 的修改 —— 已标注。

- [ ] **步骤 4：编译 & 运行新测试**

```bash
cmake --build build -j$(nproc) --target font_manager_test 2>&1 | tail -15
ctest --test-dir build --output-on-failure -R font_manager_test
```
预期：全部 PASS（原 5 个 + 新 3 个）

### 任务 4.3：让 SoftwareCanvas::DrawText 使用缓存

- [ ] **步骤 1：修改 `veloxa/graphics/software/software_canvas.cc`（`DrawText` 函数）**

- 删除原 line 174 的 `hb_font_t* hb_font = hb_ft_font_create_referenced(face);`
- 替换为：
  ```cpp
  hb_font_t* hb_font = font_manager_->GetHbFont(font, pixel_size);
  if (!hb_font) {
    hb_buffer_destroy(buf);
    DrawTextFallback(text, bounds, font_size, brush);
    return;
  }
  ```
  注意：`hb_buffer_create` 发生在 line 175，所以要先创建 buf 再 GetHbFont？看原顺序。重新整理：

原顺序（line 170-180）：
```cpp
u32 pixel_size = static_cast<u32>(font_size);
if (pixel_size == 0) pixel_size = 1;
FT_Set_Pixel_Sizes(face, 0, pixel_size);

hb_font_t* hb_font = hb_ft_font_create_referenced(face);
hb_buffer_t* buf = hb_buffer_create();
hb_buffer_add_utf8(buf, text.data(), static_cast<int>(text.size()), 0, -1);
hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
hb_buffer_set_script(buf, HB_SCRIPT_LATIN);
hb_buffer_guess_segment_properties(buf);
hb_shape(hb_font, buf, nullptr, 0);
```

新顺序：
```cpp
u32 pixel_size = static_cast<u32>(font_size);
if (pixel_size == 0) pixel_size = 1;
FT_Set_Pixel_Sizes(face, 0, pixel_size);

hb_font_t* hb_font = font_manager_->GetHbFont(font, pixel_size);
if (!hb_font) {
  DrawTextFallback(text, bounds, font_size, brush);
  return;
}

hb_buffer_t* buf = hb_buffer_create();
hb_buffer_add_utf8(buf, text.data(), static_cast<int>(text.size()), 0, -1);
hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
hb_buffer_set_script(buf, HB_SCRIPT_LATIN);
hb_buffer_guess_segment_properties(buf);
hb_shape(hb_font, buf, nullptr, 0);
```

- 删除原 line 267 `hb_font_destroy(hb_font);`
- 保留 `hb_buffer_destroy(buf);`

- [ ] **步骤 2：编译 software_canvas + 运行其测试**

```bash
cmake --build build -j$(nproc) --target software_canvas_test 2>&1 | tail -5
ctest --test-dir build --output-on-failure -R software_canvas_test
```
预期：全部 PASS（行为等价，仅少了 per-call 创建/销毁）

- [ ] **步骤 3：全量回归**

```bash
cmake --build build -j$(nproc) 2>&1 | tail -5
ctest --test-dir build --output-on-failure 2>&1 | tail -10
```
预期：全部 PASS，含 examples/hello 若有集成测试也通过

- [ ] **步骤 4：验证测试有效性**

临时把 `GetHbFont` 改成始终返回 nullptr，确认 `GetHbFontReusesPerHandle` FAIL 以及 SoftwareCanvas 走 fallback（软件画文本变为矩形存根，集成测试若断言具体像素会 FAIL）。恢复。

- [ ] **步骤 5：提交 Phase 4**

```bash
git add veloxa/text/font_manager.h veloxa/text/font_manager.cc \
  veloxa/graphics/software/software_canvas.cc \
  tests/text/font_manager_test.cc tests/CMakeLists.txt
git commit -m "$(cat <<'EOF'
perf(text): cache hb_font_t per FontHandle in FontManager (#48)

SoftwareCanvas::DrawText previously called
hb_ft_font_create_referenced() + hb_font_destroy() on every DrawText
invocation. Each creation walks FT_Face charmap/metric tables.

Move hb_font_t ownership into FontEntry (alongside FT_Face) and expose
FontManager::GetHbFont(handle, pixel_size). The call is idempotent per
handle; when pixel_size differs from the cached value, calls
hb_ft_font_changed() so HarfBuzz observes the new FT_Face size.

Caller contract: FT_Set_Pixel_Sizes(face, 0, pixel_size) must be
called BEFORE GetHbFont so HarfBuzz's subsequent reads see the new
metrics. Documented on the API.

FontManager::Shutdown now destroys cached hb_font_t before FT_Face.

tests/CMakeLists.txt: font_manager_test now links HarfBuzz directly
(previously PRIVATE on vx_text).

TASK-20260418-01.
EOF
)"
```

---

## Phase 5 — 收尾与文档更新

### 任务 5.1：更新 Memory Bank 技术债清单

- [ ] **步骤 1：修改 `memory-bank/techContext.md` 的技术债清单**

将第 243-293 行的技术债务清单中 #45/#46/#48/#50 标记为已解决（类似已有的 `~~...~~ ✅` 格式），追加本次的残留说明：

```
45. ~~dom_bindings.cc 使用全局变量...~~ ✅ 已迁移到 DomBindings::InstanceData (pimpl)（TASK-20260418-01）；JSClassID 保留为 s_ 前缀静态并幂等注册
46. ~~StyleGetProp getter 始终返回空字符串...~~ ✅ 已实现 length/color/auto/number/inherit/initial 序列化（TASK-20260418-01）；Enum（如 display）读路径仍为空，留作后续
48. ~~SoftwareCanvas::DrawText 每次调用创建 hb_font_t...~~ ✅ 已缓存到 FontManager::FontEntry，通过 GetHbFont(handle, pixel_size) 按需创建 + hb_ft_font_changed 重配置（TASK-20260418-01）
50. ~~addEventListener lambda 捕获 JSContext*...~~ ✅ DomBindings::Unbind 先调用 em_->RemoveEventListeners 再 FreeAll callbacks，顺序保证（TASK-20260418-01）；已知遗留约束：宿主须确保 DomBindings 先于 EventManager 析构
```

- [ ] **步骤 2：修改 `memory-bank/activeContext.md` 待处理事项**

删除 4 条已完成的 P1：
- `dom_bindings.cc 全局状态迁移...`
- `StyleGetProp 实现读路径...`
- `SoftwareCanvas::DrawText 缓存 HarfBuzz font 对象...`
- `addEventListener lambda 中 JSContext* 生命周期保证...`

新增残留（如有）：
- **P2**：`StyleGetProp` Enum 读路径（display 等）—— 需要 PropertyId→enum string 反查表（来源 TASK-20260418-01）
- **P2**：DomBindings/EventManager 析构顺序假设 —— 当前仅保证 DomBindings 先析构；反向场景需弱引用机制（来源 TASK-20260418-01）

- [ ] **步骤 3：提交文档更新**

```bash
git add memory-bank/techContext.md memory-bank/activeContext.md
git commit -m "$(cat <<'EOF'
docs(memory-bank): mark #45/#46/#48/#50 resolved, record residuals

TASK-20260418-01.
EOF
)"
```

注：其他 Memory Bank 更新（tasks.md / progress.md / activeContext 阶段）由 `/reflect` 和 `/archive` 完成，不在本计划中。

### 任务 5.2：最终验证

- [ ] **步骤 1：干净构建 + 全量回归**

```bash
ctest --test-dir build --output-on-failure 2>&1 | tail -20
```
预期：**所有**测试 PASS，无新增失败

- [ ] **步骤 2：分支状态检查**

```bash
git log --oneline main..HEAD
git status
```
预期：
- 5 个 commit：Phase 1-4 + 文档更新
- 工作区干净

- [ ] **步骤 3：回报给用户**

输出格式：
```
## ✅ 构建完成

- Phase 1 (#45): <commit hash>
- Phase 2 (#50): <commit hash>
- Phase 3 (#46): <commit hash>
- Phase 4 (#48): <commit hash>
- Phase 5: 文档更新 <commit hash>

测试：X 通过 / 0 失败
下一步：/reflect
```

---

## 风险与回退

| 风险 | 缓解 |
|------|------|
| pimpl 改造后外部代码 include `dom_bindings.h` 却需要访问成员 | 公共 API 通过方法暴露（`event_manager()`, `document()`, `context()`），无外部直接字段访问 |
| `JS_IsRegisteredClass` 不存在 | 已确认 quickjs-ng v0.14.0 有此 API（`build/_deps/quickjsng-src/quickjs.h:711`） |
| `hb_font_t` 前置声明冲突 | HarfBuzz 使用 `typedef struct hb_font_t hb_font_t;`（C 惯例），`struct hb_font_t;` 前置声明兼容 |
| `String::append(const char*)` API 不兼容 | 步骤中要求先 Read string.h 确认；不兼容时 fallback 到 `for (char c : str) out->push_back(c)` |
| Phase 4 链接错误（HarfBuzz PRIVATE） | 已在 tests/CMakeLists.txt 显式添加 HarfBuzz 链接 |
| DrawText fallback 路径被触发 | 新增 `GetHbFont` 返回 nullptr 时走 fallback，不改变外部行为 |

**回退策略**：每 Phase 一个独立 commit。任一 Phase 出现回归，`git revert <phase_commit>` 即可隔离问题，其他 Phase 不受影响。

---

## 预估时间

| Phase | 任务 | 预估 |
|-------|------|------|
| 0 | 基线验证 | 5 分钟 |
| 1 | pimpl + JSClassID 静态化 + g_bound_doc 迁移 | 40 分钟 |
| 2 | Unbind 顺序修复 + 测试 | 15 分钟 |
| 3 | StyleGetProp 读路径 + SerializeCssValue | 25 分钟 |
| 4 | FontManager::GetHbFont + SoftwareCanvas 切换 | 30 分钟 |
| 5 | 文档更新 + 最终验证 | 10 分钟 |
| **合计** | | **~2 小时** |

## 创意阶段判定

**不需要 `/creative` 阶段。** 所有方案决策已在头脑风暴阶段确定（A1 / B1 / C1 / D1），本计划中不存在需要探索多种实现方案的组件。可直接进入 `/build`。
