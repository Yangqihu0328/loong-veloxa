# DomBindings R2 收口设计（Spec / 二连补全 + B-G2 audit）

**文档 ID：** `2026-05-05-dombindings-r2-closure-design`
**任务 ID：** `TASK-20260505-01`（Level 3 / V2=b 二连补全 + audit 变体）
**创建日期：** 2026-05-05
**状态：** 已批准（用户 2026-05-05 ~13:25 D1-B + D2-C + D3-full 三决策锁定）
**复杂度级别：** Level 3
**plan ×0.6：** ~2-3 h
**分支：** `feature/TASK-20260505-01-dombindings-r2-closure`
**前置：** [TASK-20260504-01 MVP-scope spec §3.2.1 §11.2 推荐 #1](2026-05-04-mvp-scope.md)

---

## 1. 目的（What）

收口 MVP-B 已知 gap 中 DomBindings R2 三件套的剩余 **2 件**（B-G1 children + B-G3 innerHTML setter）+ 完成 B-G2 addEventListener audit（修复 `MapJsEventName` 缺 `click/mouse*` 事件别名导致 inspector 视觉不工作的隐性 bug）。

### 1.1 解决的具体问题

1. **DomBindings.Element.children 缺失**（spec §3.2.1 B-G1 / dogfood 暴露 R2 P3 #1） → inspector_panel.js setupTabs `tabs.children` undefined 早期 return
2. **DomBindings.Element.innerHTML setter 缺失**（spec §3.2.1 B-G3 / dogfood 暴露 R2 P3 #3） → inspector_panel.js 中 `panel.innerHTML = html` / `node.innerHTML = String(s.fps)` 全 silent no-op，DOM tree / HUD 数字 / Hot Reload status 视觉**完全不显示**
3. **B-G2 audit 数据回归 + 新发现 click 缺失**（VAN 阶段实证）：
   - spec §3.2.1 标注 addEventListener 缺失 — 实际已实现（commit `00deaca` 落地 + `#47` `#50` 优化）
   - 但 `MapJsEventName`（dom_bindings.cc:113-137）**仅含 pointer/key/touch/focus**，**缺 `click` / `mousedown` / `mouseup` / `mousemove`** → 即使 children 修复后，inspector_panel.js setupTabs 第 69 行 `btn.addEventListener("click", ...)` 仍 silent fail（mapping 返回 false → 直接 return）

### 1.2 dogfood 完整自动恢复链路

只有 **B-G1 + B-G3 + B-G2 audit 三件齐**才能让 inspector tab 切换 / HUD 数字显示 / DOM tree 渲染**视觉完整工作**。任一缺失即 dogfood 部分坏：

| 缺失项 | dogfood 视觉影响 |
|---|---|
| 仅 B-G1 | tab 按钮的 children 集合可访问，但 click handler 不挂（B-G2）→ tab 切换仍坏；DOM tree 渲染不出（B-G3）|
| 仅 B-G3 | DOM tree 渲染出，但 setupTabs `tabs.children` 仍 undefined → 整 setupTabs 早 return → tab 切换不工作 |
| 仅 B-G2 audit | click 监听挂上但 setupTabs `tabs.children` 仍 undefined 早 return → click handler 永不注册 |
| **三件齐 ✅** | tab 切换工作 + HUD fps/bars 显示 + DOM tree 渲染 + Hot Reload status badge 显示 — **dogfood 完整自动恢复** |

### 1.3 本 spec 不做（YAGNI）

1. **不扩展 `EventType` enum** — 沿用单一 pointer 模型（click → kPointerUp 别名 / mousedown* → kPointerDown* 别名）；如未来需 W3C 复合 click 事件（mousedown + mouseup composite）按独立 P3 立项
2. **不做 live HTMLCollection** — `Element.children` 单次构造 array-like proxy / inspector_panel.js 使用模式（snapshot iterate）不依赖 live 行为
3. **不做 `Element.children[i].children` 链式 lazy proxy** — 嵌套访问每次构造新 proxy（简单 / 与 inspector 用法一致）
4. **不做 DOM 节点动态创建删除**（C-G5 范畴 / spec §3.3.1）— `createElement` / `removeChild` / `appendChild` JS bindings 留 MVP-C
5. **不做 `Element.parentNode` / `firstChild` / `nextSibling` 等 getter** — 留下次推进；当前 dogfood 不依赖

---

## 2. 架构（Architecture）

在既有 `RegisterElementClass`（dom_bindings.cc:699-753）prototype 上扩展 2 个 binding + 在 `MapJsEventName`（dom_bindings.cc:113-137）扩展 4 个 event alias：

```
Element.prototype:
  ├── 既有 (unchanged):
  │     getAttribute / setAttribute (methods)
  │     addEventListener / removeEventListener (methods)
  │     tagName / id (getter)
  │     textContent / style (getter+setter)
  │
  ├── 🆕 B-G1: children → HTMLCollection-like array-like proxy (getter)
  │     - JS_NewObjectClass(s_children_class_id) + opaque{element*}
  │     - .length getter (count immediate Element children, skip Text/Comment)
  │     - [i] indexed property (snapshot at construction time)
  │     - 范式参考：现有 Style proxy（dom_bindings.cc:194-215）
  │
  └── 🆕 B-G3: innerHTML setter (textContent has no innerHTML; new property)
        - 复用 vx::html::Parser::Parse(StringView) → tmp_doc
        - 遍历 tmp_doc 子节点 → tmp_doc->RemoveChild + el->AppendChild
        - 旧 children 先 element->RemoveChild 全清空（textContent setter 范式）
        - tmp_doc 析构由 unique_ptr 管理（owns_nodes_ Vector 自动 cleanup）

MapJsEventName extension (dom_bindings.cc:113-137):
  ├── 既有: pointerdown/up/move + keydown/up + touchstart/end/move + focusin/out (10 mappings)
  └── 🆕 B-G2 audit: 4 alias mappings to pointer model
        click       → kPointerUp     (W3C: click = pointer release on same target; inspector tab UX 友好)
        mousedown   → kPointerDown   (直接 pointer alias)
        mouseup     → kPointerUp     (直接 pointer alias)
        mousemove   → kPointerMove   (直接 pointer alias)
```

### 2.1 组件依赖图

```
dom_bindings.cc
  ├── 既有依赖 (unchanged):
  │   - vx::dom::{Element, Document, Text, Node}
  │   - vx::event::{EventManager, EventType, DOMEvent}
  │   - vx::css::{CssParser, EnumValueToCssString}
  │   - vx::foundation::{InternedString, String, StringView}
  │   - QuickJS C API
  │
  └── 🆕 新增依赖 (for B-G3 only):
      - vx::html::Parser (header: veloxa/core/html/parser.h)
        - 已 link：vx_script PRIVATE vx_core ✅（Parser 是 vx_core 模块）
        - 静态库循环依赖审计：dom_bindings_test 已 link vx_core，无新链接方向变更
```

---

## 3. 设计决策（D1-D3 已锁定）

### D1 — Element.children = HTMLCollection-like array-like proxy ✅（D1-B）

**选择：** 单次构造 array-like JS object，含 `length` getter + numeric index direct properties

**实现要点：**

```cpp
// dom_bindings.cc 新增段（建议位置：在 ElementGetStyle 之后）

static JSClassID s_children_class_id = 0;  // process-wide; same lifecycle as
                                            // s_element_class_id / s_style_class_id

struct ChildrenOpaque {
  // 仅持引用计数 0 的 Element*；snapshot 在构造时已产出 numeric index properties，
  // 此 opaque 仅供后续 length getter 直接读 size 而无需重新遍历父节点。
  usize length;
};

void ChildrenFinalizer(JSRuntime* /*rt*/, JSValueConst val) {
  auto* co = static_cast<ChildrenOpaque*>(JS_GetOpaque(val, s_children_class_id));
  delete co;
}

JSClassDef g_children_class_def = {
    "HTMLCollection",
    ChildrenFinalizer,
    nullptr, nullptr, nullptr,
};

JSValue ChildrenGetLength(JSContext* ctx, JSValueConst this_val) {
  auto* co = static_cast<ChildrenOpaque*>(
      JS_GetOpaque(this_val, s_children_class_id));
  if (!co) return JS_NewInt32(ctx, 0);
  return JS_NewInt32(ctx, static_cast<int>(co->length));
}

JSValue ElementGetChildren(JSContext* ctx, JSValueConst this_val) {
  auto* el = GetElement(ctx, this_val);
  if (!el) return JS_NULL;

  // Construct collection JS object
  JSValue collection =
      JS_NewObjectClass(ctx, static_cast<int>(s_children_class_id));
  if (JS_IsException(collection)) return collection;

  // Snapshot iterate immediate Element children (skip Text/Comment).
  usize index = 0;
  for (dom::Node* child = el->first_child(); child;
       child = child->next_sibling()) {
    if (!child->is_element()) continue;
    auto* child_el = static_cast<dom::Element*>(child);
    JSValue child_wrapped = WrapElement(ctx, child_el);
    JS_SetPropertyUint32(ctx, collection, static_cast<u32>(index),
                          child_wrapped);  // collection takes ownership
    ++index;
  }

  auto* co = new ChildrenOpaque{index};
  JS_SetOpaque(collection, co);
  return collection;
}
```

**RegisterChildrenClass：** 类似 RegisterStyleClass，注册 length getter。

**RegisterElementClass 扩展：** 在 `style` getter 后添加 `children` getter + 调用 `RegisterChildrenClass`。

### D2 — Element.innerHTML setter = HTML Parser + 深拷贝到 target arena ✅（D2-C-deep-clone）

**选择：** 复用 `vx::html::Parser::Parse` 解析到临时 Document，**深拷贝**子节点到目标 Document 的 arena（不 transplant）

**关键设计决策（plan 阶段 audit 修正）：**
- 初版方案 D2-C 设计为 transplant（remove + re-append）— 但 Phase 0 audit 发现 `Document::~Document`（document.h:19-23）对 `owned_nodes_` Vector 内**所有节点**调用 `~Node()` + arena 整体释放 → tmp_doc 析构时 transplanted 节点 use-after-free（R3 风险实测真实）
- 修正方案：**深拷贝**到目标 Document arena → tmp_doc 析构清理自己 / target el 节点全部在 target arena
- 代价：2× 内存使用（parse + clone）— 对 inspector 用例（短 HTML 串）可忽略
- 替代方案 audit：扩展 `dom::Document::TransferNode` 公共 API → 影响 vx_core / 复杂度高 / 不选

**实现要点：**

```cpp
// dom_bindings.cc 顶部 includes
#include "veloxa/core/html/parser.h"

// dom_bindings.cc 匿名 namespace 内新增 helper（建议位置：在 FindElementById 之后）

// Deep clone src node tree into dst_doc's arena, returning new root node.
// Returns nullptr for unsupported node types (Comment is skipped — innerHTML
// spec doesn't preserve comments through round-trip).
dom::Node* CloneNodeInto(dom::Node* src, dom::Document* dst_doc) {
  if (!src || !dst_doc) return nullptr;
  if (src->is_element()) {
    auto* src_el = static_cast<dom::Element*>(src);
    auto* new_el = dst_doc->CreateElement(src_el->tag_id());
    new_el->set_id(src_el->id());
    for (const auto& attr : src_el->attributes()) {
      new_el->SetAttribute(attr.name, attr.value);  // String copy ctor
    }
    for (auto cls : src_el->classes()) {
      new_el->AddClass(cls);
    }
    if (auto* decls = src_el->inline_declarations()) {
      for (usize i = 0; i < decls->size(); ++i) {
        new_el->SetInlineDeclaration((*decls)[i].property, (*decls)[i].value);
      }
    }
    for (dom::Node* c = src_el->first_child(); c; c = c->next_sibling()) {
      auto* cloned = CloneNodeInto(c, dst_doc);
      if (cloned) new_el->AppendChild(cloned);
    }
    return new_el;
  }
  if (src->is_text()) {
    auto* src_text = static_cast<dom::Text*>(src);
    return dst_doc->CreateText(src_text->data());  // String copy
  }
  return nullptr;  // Comment / Document not preserved
}

JSValue ElementSetInnerHTML(JSContext* ctx, JSValueConst this_val,
                             JSValueConst val) {
  auto* el = GetElement(ctx, this_val);
  auto* data = GetData(ctx);
  if (!el || !data || !data->doc) return JS_UNDEFINED;

  size_t len = 0;
  const char* str = JS_ToCStringLen(ctx, &len, val);
  if (!str) return JS_UNDEFINED;

  // Step 1: clear existing children (spec: setter replaces all children).
  // Note: removed children remain in data->doc->owned_nodes_ Vector and
  // get ~Node()d when data->doc destructs — same lifecycle pattern as
  // textContent setter's text node updates. Memory not freed eagerly;
  // long-running pages with frequent innerHTML churn would leak. (TODO P3:
  // arena reset / GC — matches existing setTextContent behavior.)
  while (dom::Node* child = el->first_child()) {
    el->RemoveChild(child);
  }

  // Step 2: parse HTML string → temp Document.
  StringView html_view(str, len);
  std::unique_ptr<dom::Document> tmp_doc(vx::html::Parser::Parse(html_view));
  JS_FreeCString(ctx, str);
  if (!tmp_doc) return JS_UNDEFINED;

  // Step 3: HTML parser wraps content in <html><body>...</body></html>;
  // walk to <body> for content extraction. If structure unexpected, fall
  // back to tmp_doc root (Document is-a Element with kUnknown tag).
  dom::Element* extract_root = tmp_doc.get();
  for (dom::Node* c = tmp_doc->first_child(); c; c = c->next_sibling()) {
    if (!c->is_element()) continue;
    auto* el_ch = static_cast<dom::Element*>(c);
    if (el_ch->tag_id() != dom::TagId::kHtml) continue;
    for (dom::Node* h = el_ch->first_child(); h; h = h->next_sibling()) {
      if (!h->is_element()) continue;
      auto* h_el = static_cast<dom::Element*>(h);
      if (h_el->tag_id() == dom::TagId::kBody) {
        extract_root = h_el;
        break;
      }
    }
    break;
  }

  // Step 4: deep clone each child of extract_root into target Document's
  // arena, then AppendChild to el. tmp_doc destructs at scope end and
  // safely cleans up its own arena (we never linked any of its nodes
  // into el's tree — only created copies).
  for (dom::Node* src_c = extract_root->first_child(); src_c;
       src_c = src_c->next_sibling()) {
    dom::Node* cloned = CloneNodeInto(src_c, data->doc);
    if (cloned) el->AppendChild(cloned);
  }

  return JS_UNDEFINED;
}
```

**安全考量（重要）：** `vx::html::Parser` 内置安全护栏自动继承：
- `kInlineStyleMaxValueLength = 8 KiB`（spec §6 T7 buffer overflow mitigation）
- `kInlineStyleMaxDeclarationCount = 1000`（DoS 防御）
- 黑名单关键字：IE expression / behavior / javascript:（XSS T1 mitigation）

**不引入新威胁面**。但需测试边界：空串 / 超长串 / 含 `<style="expression(...)">` / 含 script tag。

### D3 — B-G2 audit 范围 = MapJsEventName 别名 + 单测 + dogfood 清理 ✅（D3-full）

**选择：** 修 `MapJsEventName` 加 4 个 alias mapping + 单测补充 click 注册 + dispatch 触发 / mousedown 注册 + inspector_panel.js 移除 4 处 typeof/防御 + spec/code 注释同步

**MapJsEventName 扩展（dom_bindings.cc:118-129 修改）：**

```cpp
static const Mapping kMappings[] = {
    {"pointerdown", event::EventType::kPointerDown},
    {"pointerup", event::EventType::kPointerUp},
    {"pointermove", event::EventType::kPointerMove},
    {"keydown", event::EventType::kKeyDown},
    {"keyup", event::EventType::kKeyUp},
    {"touchstart", event::EventType::kTouchStart},
    {"touchend", event::EventType::kTouchEnd},
    {"touchmove", event::EventType::kTouchMove},
    {"focusin", event::EventType::kFocusIn},
    {"focusout", event::EventType::kFocusOut},
    // TASK-20260505-01 B-G2 audit — single pointer model alias mappings.
    // Veloxa 不扩展 EventType enum（保持单一 pointer 模型）；这里把传统
    // mouse 系列事件别名到对应 pointer 类型。click 别名 pointerup（W3C
    // 传统 click = mousedown + mouseup composite，单 pointer 模型简化为
    // release 时触发）。详见 spec §3 D3 决策。
    {"click", event::EventType::kPointerUp},
    {"mousedown", event::EventType::kPointerDown},
    {"mouseup", event::EventType::kPointerUp},
    {"mousemove", event::EventType::kPointerMove},
};
```

**inspector_panel.js 清理（4 处）：**

```diff
 function setupTabs() {
-  // R2 (Phase A.1.8 暴露) — 当前 DomBindings 缺 Element.children
-  // 集合 / addEventListener / innerHTML setter 三件套；这些缺陷被 spec
-  // §9 R2「dogfood UI 暴露引擎缺陷」清单覆盖，将在独立 P3 任务中修复。
-  // 此处 setupTabs 临时性内联防御：只在 children/addEventListener
-  // 都可用时才挂监听，否则 silent skip，让 renderDomTree 仍能运行
-  // 完成主链路验证（vx_devtool_get_dom_json 闭环）。
+  // TASK-20260505-01: R2 三件套已修复（B-G1 children + B-G3 innerHTML +
+  // B-G2 audit click event alias）。setupTabs 现在直接挂载监听器。
   var tabs = document.getElementById("devtool-tabs");
-  if (!tabs || !tabs.children) return;
+  if (!tabs) return;
   var buttons = tabs.children;
-  if (typeof buttons.length !== "number") return;
   for (var i = 0; i < buttons.length; i++) {
     (function(btn) {
-      if (typeof btn.addEventListener !== "function") return;
       btn.addEventListener("click", function() {
```

**spec 文档同步：** `docs/specs/2026-05-04-mvp-scope.md §3.2.1` B-G1+G2+G3 状态更新到「✅ 已闭环（TASK-20260505-01）」。

---

## 4. 数据流（Data Flow）

### 4.1 B-G1 children 调用链

```
JS: el.children
  → ElementGetChildren (getter via JS_DefinePropertyGetSet)
  → GetElement(ctx, this_val) → dom::Element*
  → Snapshot iterate first_child/next_sibling chain, filter is_element()
  → For each child Element: WrapElement → JS_SetPropertyUint32(collection, i, ...)
  → Allocate ChildrenOpaque{length=count} → JS_SetOpaque
  → Return collection JSValue (with length=N + indexed properties [0..N-1])

JS: el.children.length
  → ChildrenGetLength (getter via JS_DefinePropertyGetSet)
  → GetOpaque(this_val, s_children_class_id) → ChildrenOpaque*
  → Return JS_NewInt32(co->length)

JS: el.children[i]
  → JS direct property lookup on collection object (snapshot at construction)
  → Already JS_SetPropertyUint32'd; native O(1) hash lookup
```

### 4.2 B-G3 innerHTML setter 调用链

```
JS: el.innerHTML = "<div>Hi</div>"
  → ElementSetInnerHTML (setter via JS_DefinePropertyGetSet)
  → JS_ToCStringLen → C str + len
  → Step 1 (clear): while (el->first_child()) el->RemoveChild
  → Step 2 (parse): vx::html::Parser::Parse(str_view) → unique_ptr<Document> tmp_doc
  → Step 3 (extract): find <body> in tmp_doc->first_child chain (or fallback root)
  → Step 4 (transplant): for each body child: extract_root->RemoveChild + el->AppendChild
  → tmp_doc destructs at scope end (owned_nodes_ Vector cleanup wrapping nodes,
     transplanted children already detached from extract_root child chain).
  → Return JS_UNDEFINED
```

### 4.3 B-G2 audit click → kPointerUp dispatch 路径

```
JS: btn.addEventListener("click", fn)
  → ElementAddEventListener
  → MapJsEventName("click") → kPointerUp ✅（修复后）
  → data->em->AddEventListener(el, kPointerUp, lambda)
  → js_listener_entries.push_back({el, kPointerUp, callback_ptr, token})

(later) C++: em.HandleInput({type: kPointerUp, ...}) → DispatchEvent
  → For all listeners on (target, kPointerUp): invoke lambda
  → JS callback fired with event.type = "pointerup" (注意 EventTypeToString 返
    回 pointerup 而非 click — 这是单一 pointer 模型 trade-off，inspector_panel.js
    不依赖 event.type 细分)
```

---

## 5. 错误处理 + 边界输入（Error Handling）

### 5.1 边界输入清单（plan §0 必填）

| binding | 类别 | 输入 | 期望行为 |
|---|---|---|:-:|
| `children` | 默认 | 非空 Element 子节点 | snapshot collection / length=N |
| `children` | 空 | 无 child | length=0 / 索引访问 undefined |
| `children` | 混合 | Element + Text 混合 child | 仅含 Element 子节点 / Text 跳过 |
| `children` | 未挂载 | el 未 AppendChild 到 doc | length=0（first_child=nullptr）|
| `innerHTML` | 默认 | `<div>Hi</div>` | parse 后 1 child Element / textContent="Hi" |
| `innerHTML` | 空串 | `""` | 全清空 children / 不 parse |
| `innerHTML` | 嵌套 | `<a><b>x</b></a>` | parse 后 1 child Element / 嵌套保留 |
| `innerHTML` | 多 root | `<a/><b/>` | parse 后 2 child Element |
| `innerHTML` | malformed | `<div><p>` | parser 容错 / 部分树构造 |
| `innerHTML` | XSS | `<div style="expression(alert(1))">` | parser blacklist mitigation 拦截 / inline_decls 不含 expression |
| `innerHTML` | 超长 | 100KB string | parser 内 kInlineStyleMaxValueLength 护栏触发 / 仍构造但 inline style 被丢弃 |
| `innerHTML` | replace | el 已有 children + setter | 旧清空 + 新挂载 |
| `addEventListener` | 默认 | `("click", fn)` | ✅ register（修复后）|
| `addEventListener` | 别名 | `("mousedown", fn)` | ✅ register / dispatch on kPointerDown |
| `addEventListener` | 未映射 | `("foo", fn)` | silent return（保现行为）|

---

## 6. 测试策略（Test Strategy）

### 6.1 单测覆盖矩阵

| binding | 测试名 | 模式 | 期望 |
|---|---|:-:|---|
| children | `ChildrenLengthEmpty` | TDD | length=0 |
| children | `ChildrenLengthNonEmpty` | TDD | length=N（仅 Element）|
| children | `ChildrenIndexAccess` | TDD | `[0].id` 等于第一个子 Element id |
| children | `ChildrenSkipsTextNodes` | TDD | text + element 混合时仅含 element |
| innerHTML | `InnerHTMLSetSingleElem` | TDD | `<div>x</div>` → 1 child / textContent='x' |
| innerHTML | `InnerHTMLSetReplacesExisting` | TDD | 旧 children 全清空 |
| innerHTML | `InnerHTMLSetEmpty` | TDD | empty string → 全清空 / 不 parse |
| innerHTML | `InnerHTMLSetNested` | TDD | 嵌套 element 保留 |
| innerHTML | `InnerHTMLSetMultiRoot` | TDD | 多 root 同级保留 |
| innerHTML | `InnerHTMLSetMalformedRecovers` | TDD | parse 容错 / 不抛 JS 异常 |
| innerHTML | `InnerHTMLSetXssBlacklistDropped` | TDD | inline expression() 被丢弃 |
| audit click | `ClickEventListenerRegisters` | TDD | addEventListener('click', fn) → 'ok' |
| audit click | `ClickEventListenerFiresOnPointerUp` | TDD | DispatchPointerUp → JS handler 触发 |
| audit mouse | `MouseDownEventListenerRegisters` | TDD | addEventListener('mousedown', fn) → 'ok' |

**总计：14 单测**

### 6.2 dogfood 视觉自动恢复验证

不新写 ctest，复用既有 `examples/hello_devtool` smoke：
- 既有 ctest 测的是 `vx_devtool_get_dom_json` JSON 闭环（不依赖视觉）
- 修复后 inspector_panel.js 内 try/catch 仍然是 silent failure 兜底（保留），但 happy path 应触达：tab 切换 / HUD 数字 / DOM tree 渲染**人眼可见**
- reflect 阶段 finalize 跑一次 manual SDL2 hello_devtool 验证视觉
- 不新增 ctest（因 dogfood smoke 当前已 PASS / 视觉恢复属人眼验证范畴）

### 6.3 ctest 期望矩阵

| Config | DEVTOOL | SDL2 | Bench | baseline | 本任务 +ctest | 期望 |
|---|:-:|:-:|:-:|:-:|:-:|:-:|
| baseline | ON | OFF | OFF | 1284 | +14 | 1298 |
| OFF path | OFF | OFF | OFF | 1091 | +14 | 1105 |

**关键判读：** dom_bindings_test 在 `tests/CMakeLists.txt:299` 调用 `vx_add_test`**未加 DEVTOOL guard**（与 quickjs_engine_test 一致），所以 DEVTOOL=OFF 也跑 → +14 测两 config 同步增加。

---

## 7. 安全考量（Security Considerations）

### 7.1 威胁面分析

| 威胁 | 来源 | mitigation |
|---|---|:-:|
| T1 任意 eval（XSS via innerHTML） | `el.innerHTML = "<script>..."` | ✅ 继承 `vx::html::Parser` script tag handling（默认不执行 script content）+ inline style blacklist `javascript:` |
| T7 buffer overflow（innerHTML 超长） | `el.innerHTML = "x".repeat(1e9)` | ✅ 继承 Parser 的 `kInlineStyleMaxValueLength=8KB` + `kInlineStyleMaxDeclarationCount=1000`；innerHTML 整 string 长度无显式上限但 `JS_ToCStringLen` 内 QuickJS 内存上限（V1=B 32MB）作为 ceiling |
| T1 inline style XSS（IE expression）| `<div style="expression(alert(1))">` | ✅ 继承 Parser blacklist（`internal::ContainsBlacklistKeyword`）|

### 7.2 不新引入威胁面

- **B-G1 children**：纯 read-only 遍历 + JS_SetPropertyUint32 / 无外部输入 / 无网络 / 无文件 → 无新威胁面
- **B-G3 innerHTML**：所有威胁面都在 `vx::html::Parser` 已有 mitigation 内 → 无新威胁面
- **B-G2 audit click**：仅扩展事件名 mapping / 无新 dispatch 路径 / 无外部输入 → 无新威胁面

**结论：** 本任务**不标 [安全相关]** — 仅完整继承既有 `vx::html::Parser` 安全护栏，无新威胁面。

---

## 8. 落地验收清单（Definition of Done）

| # | 验收项 | 验证方式 |
|:-:|---|---|
| 1 | B-G1 children getter 实现完成 + 4 单测 PASS | `ctest -R DomBindings --output-on-failure` |
| 2 | B-G3 innerHTML setter 实现完成 + 7 单测 PASS | 同上 |
| 3 | B-G2 audit MapJsEventName 扩展 + 3 单测 PASS | 同上 |
| 4 | inspector_panel.js typeof 防御 4 处清理 | `rg "typeof.*addEventListener" veloxa/devtool/resources/` 应空 |
| 5 | spec §3.2.1 B-G1+G2+G3 状态更新 | `rg "B-G1.*缺" docs/specs/2026-05-04-mvp-scope.md` 应空 |
| 6 | DEVTOOL=ON ctest 1298/1298 PASS | full ctest run |
| 7 | DEVTOOL=OFF ctest 1105/1105 PASS | OFF baseline 验证 |
| 8 | A14 link closure 0 byte 增长 | `cmake -DVX_BUILD_DEVTOOL=OFF` + binary size diff |
| 9 | dogfood 视觉自动恢复（manual） | `./build/examples/hello_devtool` SDL2 实测 tab 切换 / HUD 数字 / DOM tree 渲染 |
| 10 | reflect 阶段补 P0/P1/P2 改进建议 | reflection 文档 §5 |

---

## 9. 与既有 systemPatterns 兼容性对照

| systemPattern | 影响 | 协同 |
|---|---|:-:|
| DomBindings pimpl + JSContext opaque 桥接（systemPatterns §2-DomBindings）| children 类同 Style proxy 范式 | ✅ |
| EventManager destruction observer（systemPatterns §2-EventManager）| 不变更 | ✅ |
| HTML Parser 安全护栏（spec §6 T1-T7）| 自动继承到 innerHTML setter | ✅ |
| 单一 pointer 事件模型（event_types.h kPointer*）| click/mouse* 别名而非扩展 enum | ✅ |
| 反向探针有效性陷阱清单（systemPatterns §反向探针）| 修改 string literal / Parser entry 路径 / mapping 表均易触发反向探针 | ✅ |

---

## 10. 风险登记

| # | 风险 | 概率 | 影响 | mitigation |
|:-:|---|:-:|:-:|---|
| R1 | `vx::html::Parser::Parse` 用于 fragment 解析时返回 wrapper Document（含 `<html><body>...</body></html>` 结构）| 中 | 低 | plan §B.2 Phase 0 grep parser 行为 + plan §B.2 实现段含 body 提取逻辑 |
| R2 | `dom::Element::AppendChild` 可能不 idempotent re-parent | 中 | 中 | plan §B.2 Phase 0 grep AppendChild 实现 + Element.cc 显式 RemoveChild + AppendChild 协议 |
| R3 | ~~tmp_doc 析构时 owned_nodes_ Vector 残余孤儿节点的 `~Node()` 二次调用可能 use-after-free~~（已 mitigation：D2-C-deep-clone 替代 transplant — 详 §3 D2）| ~~低~~ ✅ 已规避 | ~~高~~ N/A | spec §3 D2 改为 deep clone 路径 / Phase 0 audit document.h:19-23 owned_nodes_ Vector 析构 + arena 整体释放语义 → 决策深拷贝 |
| R4 | inspector tab click 注册到 kPointerUp 但 dogfood 期望 kPointerDown 触发感受 | 低 | 低 | reflect 阶段 manual 验证 / 如不符可 plan §B.3 调整 alias |
| R5 | binutils 2.46 链接策略已 `--start-group/--end-group` mitigation；但本任务新增 vx::html::Parser 依赖可能触发新循环 | 低 | 中 | plan §0 grep 静态库循环依赖审计 / `vx_script` PRIVATE link `vx_core` 已存在 |

---

## 11. 后续推进路径（DoR 未涉及）

本任务收口后 MVP-B 状态（参考 spec `2026-05-04-mvp-scope.md §3.2`）：

| gap | 状态 | 后续 |
|---|:-:|---|
| B-G1 children | ✅ 闭环 | — |
| B-G2 addEventListener audit | ✅ 闭环 | — |
| B-G3 innerHTML setter | ✅ 闭环 | — |
| B-G4 Performance Overlay 持续 invalidate | ⚠️ 仍 gap | 推荐立项 #2（Level 1-3 / ~30 min-2 h）|

**MVP-B 闭环后完成度：** ⚠️ ~90% → ⚠️ **~95%**（仅剩 B-G4 / 1 项 gap）

下一推荐立项（按 spec §11.2 顺序）：
- **#2 B-G4 Performance Overlay 持续 invalidate**（MVP-B 完整收口 / Level 1-3 / ~30 min-2 h）
- **#5 G1 OpenGL ES 硬件渲染后端蓝图**（MVP-C 核心 P0 / Level 4 多 Phase 蓝图 / ~30-60+ h）

---

**文档结束** | 用户审查通过后进入 `/plan` 步骤 4 编写实现计划
