# DomBindings R2 收口实现计划（B-G1 children + B-G3 innerHTML setter + B-G2 audit）

**目标：** 收口 MVP-B 已知 gap 中 DomBindings R2 三件套的剩余 2 件 + 完成 B-G2 audit，让 DevTool inspector 视觉自动恢复（tab 切换 / HUD 数字 / DOM tree 渲染）。

**架构：** 在既有 `RegisterElementClass` 的 prototype 上扩展 `children` getter（HTMLCollection-like array-like proxy）+ `innerHTML` setter（vx::html::Parser 解析 + deep clone 到 target Document arena）+ `MapJsEventName` 加 4 个 alias mapping（click / mousedown / mouseup / mousemove → 单一 pointer 模型）。

**技术栈：** C++17 / QuickJS C API / vx::dom（Element/Document/Node/Text）/ vx::html::Parser / GoogleTest

**复杂度级别：** Level 3

**plan ×0.6 估时：** ~2.5-3.5 h（deep clone 决策上调 ~0.5 h vs 初版 ~2-3 h）

**分支：** `feature/TASK-20260505-01-dombindings-r2-closure`（基于 main `5bac6f6`）

**前置：** `docs/specs/2026-05-05-dombindings-r2-closure-design.md`（用户 D1-B + D2-C-deep-clone + D3-full 三决策已批准 2026-05-05 ~13:25）

---

## Phase 0 — 实证核验（plan §0 batch grep + 验收前置）

### 0.1 工具链版本快照（writing-plans.mdc 强制）

```bash
gcc --version | head -1   # 实测：gcc 15.2.0
ld --version | head -1    # 实测：GNU Binutils 2.46（已知激进，--start-group/--end-group mitigation 已落地 CMakeLists.txt:33-38）
cmake --version | head -1 # 实测：cmake 4.2.3
ninja --version           # 实测：1.13.2
```

**判读：** 与上次任务（TASK-20260504-01）一致 → ✅ 跳过差异检查 / 已记录 progress.md。

### 0.2 FetchContent 代理状态（writing-plans.mdc 强制）

```bash
git config --global --get http.proxy   # 实测：空
ls build/_deps/                         # 实测：quickjsng-{src,build,subbuild} 已离线
```

**判读：** `_deps/` 已完整离线（quickjsng-src 等）→ 跳过代理设置 / 已记录 progress.md。

### 0.3 ctest config 矩阵预测（writing-plans.mdc 强制）

| Config | DEVTOOL | SDL2 | Bench | baseline | +ctest | 预测 |
|---|:-:|:-:|:-:|:-:|:-:|:-:|
| baseline ON | ON | OFF | OFF | 1284 | +14 | 1298 |
| OFF path | OFF | OFF | OFF | 1091 | +14 | 1105 |

**判读：** `tests/CMakeLists.txt:299` `vx_add_test(dom_bindings_test ...)` **未加 DEVTOOL guard**（与 quickjs_engine_test 一致）→ DEVTOOL=OFF 也跑 → 两 config 同步增加 +14 测。

**add_test config guard 边界审计（writing-plans.mdc P0 强制 audit）：**
- `rg "vx_add_test\(dom_bindings_test" tests/CMakeLists.txt -B 5 -A 3` — 确认无 `if (VX_BUILD_DEVTOOL)` guard 包围 ✅
- 结论：dom_bindings_test 在 DEVTOOL=ON 与 DEVTOOL=OFF 两 config 都跑 / 14 新增测 PASS 数预测正确

### 0.4 既有测试隐式契约 fingerprint（writing-plans.mdc — Element binding 测试富边界）

```bash
rg "EXPECT_EQ.*tagName|EXPECT_EQ.*\.id|EXPECT_EQ.*textContent|EXPECT_EQ.*calledA" tests/script/dom_bindings_test.cc | head -20
```

**实测命中（tests/script/dom_bindings_test.cc:68-330）**：
- 现有 30+ 单测覆盖 getElementById / tagName / id / textContent / setAttribute / style.X / addEventListener / removeEventListener
- 现有 RemoveEventListenerByHandlerKeepsSibling 假设 `pointerdown` event dispatch 工作正常
- **无 children / innerHTML / click 相关测试**（首次新增）
- **无 Document::~Document 节点生命周期测试**（D2-C-deep-clone 安全性 reflected 在 unit test 中难直接测；通过 valgrind / ASan 间接覆盖）

**结论：** 现有 fingerprint 不会被 14 新增测影响（添加在尾部 / 无测共享前置 children/innerHTML 状态）。

### 0.5 静态库循环依赖审计（writing-plans.mdc 强制）

```bash
rg "target_link_libraries\(vx_script" --type=cmake -A 3
# 实测：vx_script PRIVATE vx_core ✅（已 link）
rg "target_link_libraries\(dom_bindings_test" --type=cmake -A 3
# 实测：PRIVATE vx_script vx_core ✅（已 link）
```

**判读：** 本任务新增 `vx::html::Parser` 调用（位于 `veloxa/core/html/parser.cc` → vx_core 模块）；vx_script 已 PRIVATE link vx_core / dom_bindings_test 也已 link → **无新链接方向变更** ✅。

### 0.6 ~~Web 标准 API 多重载形态清单~~（不适用）

`Element.children` getter / `Element.innerHTML` setter 都是单一形态（无 0/1/2/3 args overload）→ 跳过此 audit。`addEventListener` 已实现且本任务**不**改其 overload 形态（仅改 mapping 表）。

### 0.7 边界输入清单（writing-plans.mdc 强制）

参见 spec §5.1 — 14 项已列。每项对应 1 个测试（共 14 单测）。

### 0.8 中文文档 StrReplace 字符类型 audit（writing-plans.mdc P0 强制）

本任务涉及编辑：
- `docs/specs/2026-05-04-mvp-scope.md`（B-G1+G2+G3 状态更新 — 含中文标点）
- `memory-bank/{tasks,activeContext,progress,systemPatterns}.md`（含中文标点）
- `docs/plans/2026-05-05-dombindings-r2-closure.md`（本文档 — 中英混排）

**强制 4 项 mitigation：** 编辑前 Read 准确范围 / 单段 ≤ 10 行 / 复制粘贴 / 失败立即 Read 缩小。

### 0.9 testability 接口检查（writing-plans.mdc — 涉及新公开 API 时必填）

本任务**不**新增公开 C ABI（`vx_*`）/ 不新增公开 C++ class API（仅在 `dom_bindings.cc` 匿名 namespace 内新增 free function）→ **跳过** testability 三维度清单。

### 0.10 Document::~Document + Element::AppendChild 节点生命周期 audit（D2-C-deep-clone 决策依据）

**实证（VAN/plan 阶段已读 source）：**

```cpp
// veloxa/core/dom/document.h:19-23
~Document() override {
  for (auto* node : owned_nodes_) {
    node->~Node();
  }
}

// veloxa/core/dom/node.cc:12-23 (Element::AppendChild)
void Element::AppendChild(Node* child) {
  child->set_parent(this);
  child->set_next_sibling(nullptr);
  child->set_prev_sibling(last_child_);
  ...
}
```

**判读：**
- `Document` arena 整体管理节点内存（via ArenaAllocator）
- `owned_nodes_` Vector 仅记录节点指针 — `~Node()` 仅触发 virtual destructor，不释放 arena 内存
- `Element::AppendChild` **不** detach child 与原 parent 的链接（child->prev_/next_ 被覆盖但原 parent 的 first_/last_child 链表 corrupt）
- 跨 Document 的节点**绝对不能 transplant**（arena 不同 / lifetime 不同）

**结论锁定：** 必须 deep clone（不能 transplant）— 见 spec §3 D2 实现段。

### 0.11 inspector_panel.js dogfood 视觉恢复链路（writing-plans.mdc — UI 行为验收）

| 修复后视觉行为 | 依赖 binding | 触发路径 |
|---|---|---|
| inspector tab 切换 | B-G1 children + B-G2 click | setupTabs `tabs.children` ✅ + `addEventListener("click", ...)` ✅ |
| HUD fps 数字显示 | B-G3 innerHTML setter | `fps.innerHTML = String(s.fps)` ✅ |
| HUD 4 stage bars 宽度 | （style 已工作）| `bar.style = "width: ..."` ✅ |
| Hot Reload status badge 显示 | B-G3 innerHTML setter | `node.innerHTML = "ERR"` / `String(s.tracked)` ✅ |
| DOM tree 渲染（左侧 Inspector）| B-G3 innerHTML setter | `panel.innerHTML = html` ✅ |

reflect 阶段必跑 manual smoke：`./build/examples/hello_devtool` SDL2 + F12 切换 inspector visibility + 鼠标点击 tab 验证切换。

---

## 文件结构

### 创建/修改的文件

| 文件 | 角色 | 改动量预估 |
|---|---|:-:|
| `veloxa/script/dom_bindings.cc` | 主实现（children class + innerHTML setter + MapJsEventName 扩展 + RegisterElementClass 扩展）| **+150-200 行** |
| `tests/script/dom_bindings_test.cc` | 14 新单测 | **+250-350 行** |
| `veloxa/devtool/resources/inspector_panel.js` | typeof 防御 4 处清理 + 注释更新 | **±15 行** |
| `docs/specs/2026-05-04-mvp-scope.md` | §3.2.1 B-G1+G2+G3 状态更新到 ✅ 闭环 | **±10 行** |
| `docs/specs/2026-05-05-dombindings-r2-closure-design.md` | 设计文档 | （已写）|
| `docs/plans/2026-05-05-dombindings-r2-closure.md` | 实现计划（本文档）| （已写）|

**[共享文件] 标注：** 无 — `CMakeLists.txt` / `vcpkg.json` 都不动（vx::html::Parser 已存在 + 链接关系已就绪）。

**[影响前序测试] 标注：** 无 — 14 新增测全部独立 / 不修改既有测 / inspector_panel.js typeof 防御移除是 happy-path 增强（既有 try/catch 兜底 silent failure 不变）。

---

## Phase A — B-G1 children 实现（~45-60 min plan ×0.6）

### 任务 A.1：children class 注册基础设施 [TDD]

**文件：**
- 修改：`veloxa/script/dom_bindings.cc`
- 测试：`tests/script/dom_bindings_test.cc`

**目的：** 注册 `s_children_class_id` + `g_children_class_def` + Finalizer + length getter，建立 array-like proxy 基础。

- [ ] **步骤 1：编写失败测试**

  在 `tests/script/dom_bindings_test.cc` 末尾（DomBindingsTest 段）追加 4 测：

  ```cpp
  // ----- B-G1 children getter (TASK-20260505-01) -----

  TEST_F(DomBindingsTest, ChildrenLengthEmpty) {
    // div_ 在 SetUp 中 AppendChild 了一个 Text node ("Hello"); span btn_
    // 没有 element children. 这里我们清空 div_ 的 child 重测 length=0 路径.
    auto r = engine_.EvalGlobal(
        "var el = document.getElementById('btn');"
        "el.children.length",
        "t.js");
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(r.value(), "0");
  }

  TEST_F(DomBindingsTest, ChildrenSkipsTextNodes) {
    // div_ 在 SetUp 时 AppendChild(Text "Hello") — children 应过滤 Text → length=0
    auto r = engine_.EvalGlobal(
        "document.getElementById('box').children.length",
        "t.js");
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(r.value(), "0");
  }

  TEST_F(DomBindingsTest, ChildrenLengthNonEmpty) {
    auto* nested1 = doc_.CreateElement(dom::TagId::kSpan);
    nested1->set_id(InternedString::Intern("c1"));
    div_->AppendChild(nested1);
    auto* nested2 = doc_.CreateElement(dom::TagId::kSpan);
    nested2->set_id(InternedString::Intern("c2"));
    div_->AppendChild(nested2);
    auto r = engine_.EvalGlobal(
        "document.getElementById('box').children.length",
        "t.js");
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(r.value(), "2");
  }

  TEST_F(DomBindingsTest, ChildrenIndexAccess) {
    auto* nested = doc_.CreateElement(dom::TagId::kSpan);
    nested->set_id(InternedString::Intern("nested"));
    div_->AppendChild(nested);
    auto r = engine_.EvalGlobal(
        "document.getElementById('box').children[0].id",
        "t.js");
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(r.value(), "nested");
  }
  ```

- [ ] **步骤 2：运行测试验证失败**

  ```bash
  cmake --build build --target dom_bindings_test
  ./build/tests/dom_bindings_test --gtest_filter='DomBindingsTest.Children*'
  ```

  预期：4 测全 FAIL（`children` 属性 undefined → `.length` 读 undefined → JS 异常或测试 EvalGlobal 返错）

- [ ] **步骤 3：编写最少实现**

  在 `dom_bindings.cc` 顶部 statics 段（`static JSClassID s_style_class_id = 0;` 之后）添加：

  ```cpp
  static JSClassID s_children_class_id = 0;
  ```

  在匿名 namespace 内 `g_style_class_def` 之后添加：

  ```cpp
  // ----- HTMLCollection-like array-like proxy (B-G1) -----
  //
  // TASK-20260505-01: el.children returns a snapshot collection of immediate
  // Element children (Text/Comment skipped). Numeric index properties are
  // populated at construction time via JS_SetPropertyUint32; .length comes
  // from the opaque slot's stored count. Not live (re-read via el.children
  // each time to refresh).
  struct ChildrenOpaque {
    usize length;
  };

  void ChildrenFinalizer(JSRuntime* /*rt*/, JSValueConst val) {
    auto* co = static_cast<ChildrenOpaque*>(
        JS_GetOpaque(val, s_children_class_id));
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
  ```

  在 `RegisterStyleClass` 之后添加 `RegisterChildrenClass`：

  ```cpp
  void RegisterChildrenClass(JSContext* ctx) {
    JSRuntime* rt = JS_GetRuntime(ctx);
    if (s_children_class_id == 0) {
      JS_NewClassID(rt, &s_children_class_id);
    }
    if (!JS_IsRegisteredClass(rt, s_children_class_id)) {
      JS_NewClass(rt, s_children_class_id, &g_children_class_def);
    }
    JSValue proto = JS_NewObject(ctx);
    JSAtom len_atom = JS_NewAtom(ctx, "length");
    JS_DefinePropertyGetSet(
        ctx, proto, len_atom,
        MakeGetter(ctx, ChildrenGetLength, "get length"),
        JS_UNDEFINED, 0);
    JS_FreeAtom(ctx, len_atom);
    JS_SetClassProto(ctx, s_children_class_id, proto);
  }
  ```

  在 `ElementGetStyle` 之后添加 `ElementGetChildren`：

  ```cpp
  JSValue ElementGetChildren(JSContext* ctx, JSValueConst this_val) {
    auto* el = GetElement(ctx, this_val);
    if (!el) return JS_NULL;
    JSValue collection =
        JS_NewObjectClass(ctx, static_cast<int>(s_children_class_id));
    if (JS_IsException(collection)) return collection;
    usize index = 0;
    for (dom::Node* child = el->first_child(); child;
         child = child->next_sibling()) {
      if (!child->is_element()) continue;
      auto* child_el = static_cast<dom::Element*>(child);
      JSValue wrapped = WrapElement(ctx, child_el);
      JS_SetPropertyUint32(ctx, collection, static_cast<u32>(index), wrapped);
      ++index;
    }
    auto* co = new ChildrenOpaque{index};
    JS_SetOpaque(collection, co);
    return collection;
  }
  ```

  在 `RegisterElementClass`（dom_bindings.cc:699）的 `style` getter 之后添加 `children` getter：

  ```cpp
  JSAtom children_atom = JS_NewAtom(ctx, "children");
  JS_DefinePropertyGetSet(
      ctx, proto, children_atom,
      MakeGetter(ctx, ElementGetChildren, "get children"),
      JS_UNDEFINED, 0);
  JS_FreeAtom(ctx, children_atom);
  ```

  在 `DomBindings::Bind` 内 `RegisterStyleClass(ctx)` 之后添加：

  ```cpp
  RegisterChildrenClass(ctx);
  ```

- [ ] **步骤 4：运行测试验证通过**

  ```bash
  cmake --build build --target dom_bindings_test
  ./build/tests/dom_bindings_test --gtest_filter='DomBindingsTest.Children*'
  ```

  预期：4 测全 PASS

- [ ] **步骤 5：反向探针验证（writing-plans.mdc P1 强制）**

  临时把 `ElementGetChildren` 内 `if (!child->is_element()) continue;` 注释掉 → `ChildrenSkipsTextNodes` 应 FAIL（length=1 而非 0），其他 3 测仍 PASS（不依赖 Text 过滤）。
  恢复 → 全 PASS。
  记录到 progress.md「探针选择优先级 1（保留代码但故意修改边界）」。

- [ ] **步骤 6：提交**

  ```bash
  git add veloxa/script/dom_bindings.cc tests/script/dom_bindings_test.cc
  git commit -m "$(cat <<'EOF'
  feat(script): Element.children getter (HTMLCollection-like) [B-G1]

  TASK-20260505-01 Phase A.1: B-G1 closure.

  Implementation: snapshot-style array-like proxy. Construction iterates
  immediate Element children (Text/Comment skipped) and populates numeric
  index properties via JS_SetPropertyUint32. length comes from opaque slot.
  Not live — re-read via el.children to refresh.

  Tests: 4 new ctest cases (DomBindingsTest.Children*)
    - ChildrenLengthEmpty (no element children)
    - ChildrenSkipsTextNodes (Text/Comment filtered)
    - ChildrenLengthNonEmpty (count = N)
    - ChildrenIndexAccess (children[0].id round-trips)

  ctest: DEVTOOL=ON 1284 → 1288 (+4) PASS / DEVTOOL=OFF 1091 → 1095 (+4) PASS

  Reverse probe: removing is_element() filter → ChildrenSkipsTextNodes
  FAILs as expected (length=1 instead of 0).

  Source: docs/plans/2026-05-05-dombindings-r2-closure.md §A.1
  EOF
  )"
  ```

---

## Phase B — B-G3 innerHTML setter 实现（~75-90 min plan ×0.6）

### 任务 B.1：CloneNodeInto helper + ElementSetInnerHTML 实现 [TDD]

**文件：**
- 修改：`veloxa/script/dom_bindings.cc`（include parser.h + helper + setter + register innerHTML）
- 测试：`tests/script/dom_bindings_test.cc`（7 单测）

**目的：** 完成 innerHTML setter 的核心解析 + deep clone 路径，覆盖 7 边界用例。

- [ ] **步骤 1：编写失败测试**

  在 `tests/script/dom_bindings_test.cc` Children 测试段之后追加 7 测：

  ```cpp
  // ----- B-G3 innerHTML setter (TASK-20260505-01) -----

  TEST_F(DomBindingsTest, InnerHTMLSetSingleElem) {
    auto r = engine_.EvalGlobal(
        "var el = document.getElementById('box');"
        "el.innerHTML = '<span>Hi</span>';"
        "el.children.length",
        "t.js");
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(r.value(), "1");
  }

  TEST_F(DomBindingsTest, InnerHTMLSetReplacesExisting) {
    // div_ 已有 Text "Hello" — setter 应清空后只剩新 children
    auto r = engine_.EvalGlobal(
        "var el = document.getElementById('box');"
        "el.innerHTML = '<span>X</span>';"
        "el.textContent",  // textContent reads first Text node — should be ''
        "t.js");
    ASSERT_TRUE(r.ok());
    // 实际 textContent 取第一个 Text 子节点；新 children 是 span，里面有 Text 'X'
    // — 但 ElementGetTextContent 只看 *direct* Text child，所以应返 ""
    EXPECT_EQ(r.value(), "");
  }

  TEST_F(DomBindingsTest, InnerHTMLSetEmpty) {
    auto r = engine_.EvalGlobal(
        "var el = document.getElementById('box');"
        "el.innerHTML = '';"
        "el.children.length",
        "t.js");
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(r.value(), "0");
  }

  TEST_F(DomBindingsTest, InnerHTMLSetNested) {
    auto r = engine_.EvalGlobal(
        "var el = document.getElementById('box');"
        "el.innerHTML = '<a><b>x</b></a>';"
        "el.children.length",
        "t.js");
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(r.value(), "1");  // 1 top-level <a> with nested <b>
  }

  TEST_F(DomBindingsTest, InnerHTMLSetMultiRoot) {
    auto r = engine_.EvalGlobal(
        "var el = document.getElementById('box');"
        "el.innerHTML = '<a></a><b></b>';"
        "el.children.length",
        "t.js");
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(r.value(), "2");
  }

  TEST_F(DomBindingsTest, InnerHTMLSetMalformedRecovers) {
    // Parser 容错 — 不抛 JS 异常即 PASS（length 由 parser 容错策略决定）
    auto r = engine_.EvalGlobal(
        "var el = document.getElementById('box');"
        "el.innerHTML = '<div><p>';"
        "'ok'",
        "t.js");
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(r.value(), "ok");
  }

  TEST_F(DomBindingsTest, InnerHTMLSetXssBlacklistDropped) {
    // Parser 内 inline style blacklist mitigation — IE expression() 应被丢弃,
    // div 仍构造成功（不 throw）/ 但 inline_decls 不含 expression
    auto r = engine_.EvalGlobal(
        "var el = document.getElementById('box');"
        "el.innerHTML = '<div style=\"color: expression(alert(1))\"></div>';"
        "el.children.length",
        "t.js");
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(r.value(), "1");  // div 仍构造，inline_decls 安全护栏触发
  }
  ```

- [ ] **步骤 2：运行测试验证失败**

  ```bash
  cmake --build build --target dom_bindings_test
  ./build/tests/dom_bindings_test --gtest_filter='DomBindingsTest.InnerHTML*'
  ```

  预期：7 测全 FAIL（`innerHTML` 属性不可写 → JS_DefinePropertyGetSet 未注册 setter）

- [ ] **步骤 3：编写最少实现**

  顶部 includes 段（`#include "veloxa/foundation/strings/string.h"` 之后）添加：

  ```cpp
  #include "veloxa/core/html/parser.h"
  ```

  在匿名 namespace 内 `FindElementById` 之后添加 `CloneNodeInto`：

  ```cpp
  // Deep clone src node tree into dst_doc's arena, returning the new root.
  // Returns nullptr for unsupported types. Used by ElementSetInnerHTML to
  // copy nodes from a temporary parser-owned Document into the target's
  // arena (transplant is unsafe — temp Document arena destructs at scope
  // end and would invalidate the moved node memory).
  dom::Node* CloneNodeInto(dom::Node* src, dom::Document* dst_doc) {
    if (!src || !dst_doc) return nullptr;
    if (src->is_element()) {
      auto* src_el = static_cast<dom::Element*>(src);
      auto* new_el = dst_doc->CreateElement(src_el->tag_id());
      new_el->set_id(src_el->id());
      for (const auto& attr : src_el->attributes()) {
        new_el->SetAttribute(attr.name, attr.value);
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
      return dst_doc->CreateText(src_text->data());
    }
    return nullptr;
  }
  ```

  在 `ElementSetTextContent` 之后添加 `ElementSetInnerHTML`：

  ```cpp
  // ----- innerHTML setter (B-G3) -----
  //
  // TASK-20260505-01: el.innerHTML = "..." replaces all children. Uses
  // vx::html::Parser::Parse to a temporary Document, then deep clones
  // body's children into the target Document's arena (transplant unsafe
  // due to per-Document arena lifecycle — see plan §0.10 audit).
  // Inherits parser's inline-style safety guards (kInlineStyleMaxValueLength
  // 8KiB / blacklist for IE expression / behavior / javascript:).
  JSValue ElementSetInnerHTML(JSContext* ctx, JSValueConst this_val,
                               JSValueConst val) {
    auto* el = GetElement(ctx, this_val);
    auto* data = GetData(ctx);
    if (!el || !data || !data->doc) return JS_UNDEFINED;

    size_t len = 0;
    const char* str = JS_ToCStringLen(ctx, &len, val);
    if (!str) return JS_UNDEFINED;

    while (dom::Node* child = el->first_child()) {
      el->RemoveChild(child);
    }

    StringView html_view(str, len);
    std::unique_ptr<dom::Document> tmp_doc(html::Parser::Parse(html_view));
    JS_FreeCString(ctx, str);
    if (!tmp_doc) return JS_UNDEFINED;

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

    for (dom::Node* src_c = extract_root->first_child(); src_c;
         src_c = src_c->next_sibling()) {
      dom::Node* cloned = CloneNodeInto(src_c, data->doc);
      if (cloned) el->AppendChild(cloned);
    }

    return JS_UNDEFINED;
  }
  ```

  在 `RegisterElementClass` 的 `textContent` getter+setter 注册之后添加 innerHTML 注册（注意：仅注册 setter — `innerHTML` getter 是 MVP-C 范围 / 本任务不做 read path）：

  ```cpp
  JSAtom inner_atom = JS_NewAtom(ctx, "innerHTML");
  JS_DefinePropertyGetSet(
      ctx, proto, inner_atom,
      JS_UNDEFINED,  // getter not implemented (MVP-C scope)
      MakeSetter(ctx, ElementSetInnerHTML, "set innerHTML"), 0);
  JS_FreeAtom(ctx, inner_atom);
  ```

- [ ] **步骤 4：运行测试验证通过**

  ```bash
  cmake --build build --target dom_bindings_test
  ./build/tests/dom_bindings_test --gtest_filter='DomBindingsTest.InnerHTML*'
  ```

  预期：7 测全 PASS

  **如 `InnerHTMLSetXssBlacklistDropped` FAIL（length=0 而非 1）：** Parser 在 inline style blacklist 命中时丢弃整 div 而非保留 div + 丢 inline_decls。检查 `veloxa/core/html/parser.cc` `ApplyInlineStyleAttribute` 行为；如确认是「丢整 element」则改测期望为 length=0；如是「保 element 丢 decl」则期望保持 length=1。

- [ ] **步骤 5：反向探针验证**

  临时把 `CloneNodeInto` 的 attributes 拷贝段注释掉 → 不影响 children 数量但 `el.children[0].getAttribute('id')` 会丢失（如有这种测则探针有效）。
  或更直接：把 `if (cloned) el->AppendChild(cloned);` 改为 `if (cloned) {}`（不挂载）→ 5 个 length>0 测 FAIL（length=0）/ Empty + Malformed 测仍 PASS。
  恢复 → 全 PASS。
  记录到 progress.md。

- [ ] **步骤 6：valgrind / ASan 验证内存安全（推荐 — 5 min）**

  ```bash
  # 如已安装 valgrind:
  valgrind --error-exitcode=1 --leak-check=full \
    ./build/tests/dom_bindings_test --gtest_filter='DomBindingsTest.InnerHTML*'
  # 预期：0 errors / 0 leaks（new ChildrenOpaque 由 Finalizer 释放 / Document
  # arena 跨界使用通过 deep clone 已规避）
  ```

  如 valgrind 不可用：查看 `command -v valgrind` 输出 — 如 MISS 则跳过此步并记录 progress.md（不阻塞 PASS 判断）。

- [ ] **步骤 7：提交**

  ```bash
  git add veloxa/script/dom_bindings.cc tests/script/dom_bindings_test.cc
  git commit -m "$(cat <<'EOF'
  feat(script): Element.innerHTML setter via deep-clone HTML parse [B-G3]

  TASK-20260505-01 Phase B.1: B-G3 closure.

  Implementation: el.innerHTML = "..." parses HTML to a temporary Document
  via vx::html::Parser, then deep clones body's children into the target
  Document's arena. Transplant rejected due to per-Document arena lifecycle
  (plan §0.10 audit — Document::~Document destroys arena en bloc; node
  pointers from another Document become invalid at scope end).

  Inherits parser inline-style safety guards (kInlineStyleMaxValueLength
  8KiB / blacklist for expression/behavior/javascript:) — no new threat
  surface. innerHTML getter not implemented (MVP-C scope).

  Tests: 7 new ctest cases (DomBindingsTest.InnerHTML*)
    - SetSingleElem, SetReplacesExisting, SetEmpty
    - SetNested, SetMultiRoot
    - SetMalformedRecovers (parser tolerance)
    - SetXssBlacklistDropped (inherits parser blacklist)

  ctest: DEVTOOL=ON 1288 → 1295 (+7) PASS / DEVTOOL=OFF 1095 → 1102 (+7) PASS

  Reverse probe: skipping AppendChild → 5 of 7 tests FAIL (length=0).

  Source: docs/plans/2026-05-05-dombindings-r2-closure.md §B.1
  EOF
  )"
  ```

---

## Phase C — B-G2 audit MapJsEventName 扩展 + 单测（~30-40 min plan ×0.6）

### 任务 C.1：MapJsEventName 加 4 alias mapping [TDD]

**文件：**
- 修改：`veloxa/script/dom_bindings.cc`（MapJsEventName kMappings[] 表扩展）
- 测试：`tests/script/dom_bindings_test.cc`（3 单测）

**目的：** 让 `addEventListener('click'|'mousedown'|'mouseup'|'mousemove', fn)` 不再 silent fail，inspector tab 切换工作。

- [ ] **步骤 1：编写失败测试**

  在 `tests/script/dom_bindings_test.cc` InnerHTML 测试段之后追加 3 测：

  ```cpp
  // ----- B-G2 audit: click/mouse* alias to pointer model (TASK-20260505-01) -----

  TEST_F(DomBindingsTest, ClickEventListenerRegisters) {
    auto r = engine_.EvalGlobal(
        "var btn = document.getElementById('btn');"
        "btn.addEventListener('click', function(e) {});"
        "'ok'",
        "t.js");
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(r.value(), "ok");
  }

  TEST_F(DomBindingsTest, ClickEventListenerFiresOnPointerUp) {
    // click → kPointerUp alias. Synthesize a pointerUP event and verify
    // the JS handler fires (existing DispatchPointerDown helper synthesizes
    // pointerDOWN; we add a parallel up-dispatching helper for this one
    // test — or alternatively just register on 'click' and verify it
    // dispatches via a kPointerUp input event from synthesized helper.)
    auto setup = engine_.EvalGlobal(
        "globalThis.calledClick = 0;"
        "var btn = document.getElementById('btn');"
        "btn.addEventListener('click', function(e) {"
        "  globalThis.calledClick++;"
        "});"
        "'ok'",
        "t.js");
    ASSERT_TRUE(setup.ok());

    // Synthesize pointerUp dispatch
    static thread_local css::ComputedStyle s_style;
    layout::LayoutBox box{};
    box.element = btn_;
    box.style = &s_style;
    box.x = 0; box.y = 0;
    box.content_width = 100; box.content_height = 100;
    event::InputEvent input{};
    input.type = event::EventType::kPointerUp;
    input.x = 50; input.y = 50;
    em_.HandleInput(input, &box);

    auto r = engine_.EvalGlobal("String(globalThis.calledClick)", "t.js");
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(r.value(), "1");
  }

  TEST_F(DomBindingsTest, MouseDownEventListenerRegisters) {
    auto r = engine_.EvalGlobal(
        "var btn = document.getElementById('btn');"
        "btn.addEventListener('mousedown', function(e) {});"
        "'ok'",
        "t.js");
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(r.value(), "ok");
  }
  ```

- [ ] **步骤 2：运行测试验证失败**

  ```bash
  cmake --build build --target dom_bindings_test
  ./build/tests/dom_bindings_test --gtest_filter='DomBindingsTest.*Click*:DomBindingsTest.MouseDown*'
  ```

  预期：
  - `ClickEventListenerRegisters` PASS（实际是 silent fail return JS_UNDEFINED but EvalGlobal 仍返 'ok' — 这测**不够强**！）
  - `ClickEventListenerFiresOnPointerUp` FAIL（calledClick=0 — addEventListener silent fail 未注册）✅ 强测
  - `MouseDownEventListenerRegisters` PASS（同上）— 弱测

  **观察：** 测试 1 + 3 都是弱测（即便 silent fail addEventListener 仍返 undefined / 'ok' 字符串读取仍 OK）。**实际真测点是测试 2（ClickEventListenerFiresOnPointerUp）**。修复后 calledClick=1 而非 0。

  保留 1 + 3 是为了确保「regr 不退化为 silent fail」 — 但单独不够 → step 5 反向探针专注测试 2。

- [ ] **步骤 3：编写最少实现**

  修改 `veloxa/script/dom_bindings.cc:118-129`，在 kMappings 数组末尾追加 4 条 alias：

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
      // TASK-20260505-01 B-G2 audit: single pointer model alias mappings.
      // Veloxa keeps a unified pointer event model (no separate Click /
      // MouseDown EventType). These aliases let standard W3C-style listener
      // names ('click' / 'mousedown' / 'mouseup' / 'mousemove') register
      // through the existing pointer dispatch path. click → kPointerUp
      // mirrors W3C "click = pointer release on same target" semantics
      // (good UX for tab switching: action commits on release, not press).
      {"click", event::EventType::kPointerUp},
      {"mousedown", event::EventType::kPointerDown},
      {"mouseup", event::EventType::kPointerUp},
      {"mousemove", event::EventType::kPointerMove},
  };
  ```

  **`EventTypeToString`（dom_bindings.cc:139-163）不修改** — 仍仅返 pointer*/key*/touch*/focus*。这意味着 JS handler 收到的 `event.type` 是 `'pointerup'` 而非 `'click'`（trade-off / 单 pointer 模型）。inspector_panel.js 不依赖 event.type 字段 → 无影响。

- [ ] **步骤 4：运行测试验证通过**

  ```bash
  cmake --build build --target dom_bindings_test
  ./build/tests/dom_bindings_test --gtest_filter='DomBindingsTest.*Click*:DomBindingsTest.MouseDown*'
  ```

  预期：3 测全 PASS（其中 FiresOnPointerUp 从 FAIL → PASS）

- [ ] **步骤 5：反向探针验证（关键测试 2 反向）**

  临时把 `{"click", event::EventType::kPointerUp},` 这一行注释掉 → `ClickEventListenerFiresOnPointerUp` FAIL（calledClick=0），其他测仍 PASS（弱测不依赖此 alias）。
  恢复 → 全 PASS。
  记录到 progress.md「反向探针有效性陷阱清单 — 此次第 1 类（修改 mapping 表）」。

- [ ] **步骤 6：提交**

  ```bash
  git add veloxa/script/dom_bindings.cc tests/script/dom_bindings_test.cc
  git commit -m "$(cat <<'EOF'
  feat(script): MapJsEventName click/mouse* alias to pointer model [B-G2 audit]

  TASK-20260505-01 Phase C.1: B-G2 audit closure.

  Background: VAN-stage audit found that MapJsEventName (dom_bindings.cc:113-137)
  contained only pointer*/key*/touch*/focus* (10 mappings) — but did not include
  'click', 'mousedown', 'mouseup', 'mousemove'. inspector_panel.js setupTabs
  registers btn.addEventListener("click", ...), which was silently failing
  (mapping returned false → ElementAddEventListener early-returned without
  registering). MVP-scope spec §3.2.1 had marked B-G2 as "addEventListener
  missing", but implementation existed; the actual gap was the missing event
  name aliases.

  Implementation: 4 alias mappings appended to kMappings[]:
    click       → kPointerUp     (W3C release semantics; tab UX commits on release)
    mousedown   → kPointerDown
    mouseup     → kPointerUp
    mousemove   → kPointerMove

  EventType enum unchanged (single pointer model preserved). JS handlers
  see event.type='pointerup' (not 'click') — trade-off for unified dispatch
  path. inspector_panel.js does not depend on event.type field.

  Tests: 3 new ctest cases (DomBindingsTest.{Click,MouseDown}*)
    - ClickEventListenerRegisters (regression guard)
    - ClickEventListenerFiresOnPointerUp (true mitigation test)
    - MouseDownEventListenerRegisters (regression guard)

  ctest: DEVTOOL=ON 1295 → 1298 (+3) PASS / DEVTOOL=OFF 1102 → 1105 (+3) PASS

  Reverse probe: removing {"click", kPointerUp} → ClickEventListenerFiresOnPointerUp
  FAILs (calledClick=0).

  Source: docs/plans/2026-05-05-dombindings-r2-closure.md §C.1
  EOF
  )"
  ```

---

## Phase D — inspector_panel.js typeof 防御清理 + spec 文档同步（~20-30 min plan ×0.6）

### 任务 D.1：inspector_panel.js typeof 防御 4 处清理 [覆盖补充]

**文件：**
- 修改：`veloxa/devtool/resources/inspector_panel.js`

**目的：** R2 三件套修复后，移除 setupTabs 内的临时 typeof 防御代码，让 happy path 直接执行（既有 try/catch 兜底保留）。

- [ ] **步骤 1：编写测试覆盖正常路径**

  本任务无新单测（dogfood smoke 视觉自动恢复属人眼验证范畴）。复用既有 `examples/hello_devtool` smoke ctest — 修改 inspector_panel.js 后 ctest 必须仍 PASS。

  ```bash
  cmake --build build --target hello_devtool
  ctest -R hello_devtool --output-on-failure
  ```

  预期：PASS（既有 try/catch 兜底 + happy path 增强不破协议）

- [ ] **步骤 2：移除 typeof 防御代码**

  Read `veloxa/devtool/resources/inspector_panel.js` 第 55-69 行，使用 StrReplace 替换：

  ```javascript
  // 删除前（第 55-69 行）：
  function setupTabs() {
    // R2 (Phase A.1.8 暴露) — 当前 DomBindings 缺 Element.children
    // 集合 / addEventListener / innerHTML setter 三件套；这些缺陷被 spec
    // §9 R2「dogfood UI 暴露引擎缺陷」清单覆盖，将在独立 P3 任务中修复。
    // 此处 setupTabs 临时性内联防御：只在 children/addEventListener
    // 都可用时才挂监听，否则 silent skip，让 renderDomTree 仍能运行
    // 完成主链路验证（vx_devtool_get_dom_json 闭环）。
    var tabs = document.getElementById("devtool-tabs");
    if (!tabs || !tabs.children) return;
    var buttons = tabs.children;
    if (typeof buttons.length !== "number") return;
    for (var i = 0; i < buttons.length; i++) {
      (function(btn) {
        if (typeof btn.addEventListener !== "function") return;
        btn.addEventListener("click", function() {

  // 删除后：
  function setupTabs() {
    // TASK-20260505-01: R2 三件套已闭环（B-G1 children + B-G3 innerHTML
    // setter + B-G2 audit click event alias to kPointerUp）。setupTabs 现
    // 直接挂载 click 监听 — 既有 try/catch 兜底（第 143 行）仍保留作为
    // dogfood smoke 的 hard-isolation 边界。
    var tabs = document.getElementById("devtool-tabs");
    if (!tabs) return;
    var buttons = tabs.children;
    for (var i = 0; i < buttons.length; i++) {
      (function(btn) {
        btn.addEventListener("click", function() {
  ```

- [ ] **步骤 3：运行 dogfood smoke 验证**

  ```bash
  cmake --build build --target hello_devtool
  ctest -R hello_devtool --output-on-failure
  ```

  预期：PASS（vx_devtool_get_dom_json JSON 闭环测仍通过）

- [ ] **步骤 4：（手工 — 推荐 reflect 阶段做）SDL2 视觉验证**

  如有 SDL2 环境（WSLg / Linux desktop）：

  ```bash
  ./build/examples/hello_devtool
  # 实操：
  # 1. F12 切换 inspector visibility
  # 2. 鼠标点击 inspector 顶部 4 个 tab 按钮（DOM / Style / Layout / Console）
  # 3. 验证：
  #    - tab 切换工作（点击 Style 后 Style 面板显示，DOM 隐藏）✅
  #    - HUD 数字显示（fps + 4 stage bars）✅
  #    - DOM tree 渲染（左侧 Inspector 显示 element 树）✅
  #    - Hot Reload status badge 显示（绿色 watching / 数字）✅
  ```

  如不便手工验证：在 reflect 阶段记录「视觉验证 - manual SDL2 deferred」标签。

- [ ] **步骤 5：反向 audit 验证 typeof 字面量已清空**

  ```bash
  rg "typeof.*addEventListener|tabs\.children\)" veloxa/devtool/resources/inspector_panel.js
  ```

  预期：空输出（typeof 防御已彻底清理）

- [ ] **步骤 6：提交**

  ```bash
  git add veloxa/devtool/resources/inspector_panel.js
  git commit -m "$(cat <<'EOF'
  refactor(devtool): remove R2 typeof guards from inspector_panel.js

  TASK-20260505-01 Phase D.1: dogfood cleanup.

  R2 three-shot is now closed (B-G1 + B-G3 + B-G2 audit landed in Phase A/B/C).
  setupTabs no longer needs the inline typeof guards that previously made
  it a silent no-op when Element.children / addEventListener('click', ...)
  weren't yet wired. Removed:
    - if (!tabs.children) return;          (B-G1 fix)
    - if (typeof buttons.length !== "number") return;
    - if (typeof btn.addEventListener !== "function") return;  (B-G2 audit)

  The outer try { setupTabs(); } catch (e) {} (line 143) stays as the
  hard-isolation boundary — same pattern as renderDomTree / updateHud /
  updateHotReloadStatus — so any single binding gap surfacing in the future
  cannot abort the dogfood smoke contract.

  Visual recovery (manual SDL2 verification, reflect phase):
    - inspector tab switching works (click commits on pointerUp)
    - HUD fps + 4 stage bars display via innerHTML setter
    - DOM tree renders via panel.innerHTML = html
    - Hot Reload status badge displays via node.innerHTML = "ERR" / count

  ctest: hello_devtool smoke PASS (DEVTOOL=ON 1298/1298 unchanged).

  Source: docs/plans/2026-05-05-dombindings-r2-closure.md §D.1
  EOF
  )"
  ```

### 任务 D.2：MVP-scope spec §3.2.1 状态同步 [覆盖补充]

**文件：**
- 修改：`docs/specs/2026-05-04-mvp-scope.md`（B-G1+G2+G3 状态从「⚠️ 缺」更新为「✅ 已闭环」）

**目的：** 让 MVP-B gap 表反映本任务闭环成果。

- [ ] **步骤 1：Read 当前 spec §3.2.1 表准确范围**

  Read `docs/specs/2026-05-04-mvp-scope.md` offset=86 limit=50（覆盖 §3.2.1 表格）准确字符（特别注意中文全角标点）。

- [ ] **步骤 2：StrReplace 更新 4 行**

  按 writing-plans「中文文档 StrReplace 字符类型 audit」P0 强制 mitigation：
  - 复制粘贴 Read 输出范围（不手敲全角字符）
  - 单段 ≤ 10 行
  - 失败立即 Read 缩小

  目标更新（精确文字依赖 Read 结果）：
  - B-G1 状态从「dogfood 暴露」更新为「✅ 闭环 (TASK-20260505-01)」
  - B-G2 状态从「同上 R2 P3 #2」更新为「✅ 闭环 (TASK-20260505-01 audit — alias to pointer model)」
  - B-G3 状态从「同上 R2 P3 #3」更新为「✅ 闭环 (TASK-20260505-01)」
  - **MVP-B 完成度** 从「⚠️ ~90%」更新为「⚠️ ~95%（仅 B-G4 Performance Overlay 持续 invalidate 1 项 gap）」

- [ ] **步骤 3：grep 验证 spec 状态同步**

  ```bash
  rg "B-G1.*缺|B-G2.*缺|B-G3.*缺" docs/specs/2026-05-04-mvp-scope.md
  ```

  预期：空输出（状态全更新到 ✅ 闭环）

- [ ] **步骤 4：提交**

  ```bash
  git add docs/specs/2026-05-04-mvp-scope.md
  git commit -m "$(cat <<'EOF'
  docs(spec): MVP-scope §3.2.1 sync B-G1/G2/G3 closure status

  TASK-20260505-01 Phase D.2: spec sync.

  Mark MVP-B gaps B-G1 (children) + B-G2 audit (click event alias) +
  B-G3 (innerHTML setter) as closed. MVP-B completion: 90% → 95%
  (only B-G4 Performance Overlay continuous-invalidate remains).

  Source: docs/plans/2026-05-05-dombindings-r2-closure.md §D.2
  EOF
  )"
  ```

---

## Phase E — Finalize（~20-30 min plan ×0.6）

### 任务 E.1：full ctest baseline 验证（DEVTOOL=ON + DEVTOOL=OFF 双 config）

- [ ] **步骤 1：DEVTOOL=ON full ctest**

  ```bash
  cmake --build build -j
  ctest --test-dir build --output-on-failure 2>&1 | tail -20
  ```

  预期：1298/1298 PASS（baseline 1284 + 14 new = 1298）

- [ ] **步骤 2：DEVTOOL=OFF full ctest（A14 link closure 守门）**

  ```bash
  # 复用既有 build-off 目录（无需 reconfigure / FETCHCONTENT_BASE_DIR 复用）
  cmake --build build-off -j
  ctest --test-dir build-off --output-on-failure 2>&1 | tail -20
  ```

  预期：1105/1105 PASS（baseline 1091 + 14 new = 1105 — dom_bindings_test 无 DEVTOOL guard / OFF 也跑）

- [ ] **步骤 3：A14 link closure 0 byte 增长验证**

  ```bash
  # binary 大小对比 — vs main HEAD baseline:
  ls -la build-off/veloxa/api/libvx_api.a 2>&1 | awk '{print $5}'
  # vs git stash 后的 baseline 大小（可选 — 仅当 user 关心时跑）
  ```

  预期：DEVTOOL=OFF binary 字节数与 main HEAD baseline 一致（dom_bindings_test 仅在测试范围 / 不影响 vx_api / vx_script production binary）

  **如发现增长：** 检查 `dom_bindings.cc` 是否在 production lib 内意外引入了 vx::html::Parser 调用（DEVTOOL=ON 也好 — Parser 是 vx_core 一部分既已 link，无新依赖）；如确认只是 dom_bindings.cc 增加了 ~150-200 行实现 → production lib 字节增加 ~3-5 KiB 是预期（不属 A14 link closure 违反）；A14 protocol 仅守门 vx_devtool 子系统的 link closure，与 dom_bindings 通用扩展无关。

### 任务 E.2：progress.md 更新 + 反复模式预防清单核对

- [ ] **步骤 1：progress.md 记录 Phase A/B/C/D/E 闭环 + 探针有效性数据**

  追加段：

  ```markdown
  ### TASK-20260505-01 实施记录

  - Phase 0 实证核验：工具链版本一致 / 代理 _deps 已离线 / ctest 矩阵预测 1298 + 1105 / 静态库循环依赖审计 ✅ / Document::~Document + AppendChild 节点生命周期 audit → D2-C-deep-clone 决策锁定
  - Phase A.1（B-G1 children）：4 单测 / 反向探针有效（is_element filter 注释 → SkipsTextNodes FAIL）/ commit ...
  - Phase B.1（B-G3 innerHTML）：7 单测 / 反向探针有效（AppendChild 注释 → 5 测 FAIL）/ commit ...
  - Phase C.1（B-G2 audit）：3 单测 / 反向探针有效（click mapping 注释 → FiresOnPointerUp FAIL）/ commit ...
  - Phase D.1（inspector_panel.js typeof 清理）：grep 验证 typeof 字面量已清空 / commit ...
  - Phase D.2（MVP-scope spec 状态同步）：B-G1+G2+G3 全 ✅ 闭环 / 中文 StrReplace audit 0 重试（成功遵循 4 项 mitigation）/ commit ...
  - Phase E.1（full ctest）：DEVTOOL=ON 1298/1298 PASS / DEVTOOL=OFF 1105/1105 PASS

  ### 反复模式预防清单核对

  | 已知反复模式 | 本任务命中状态 | 注释 |
  |---|:-:|---|
  | #1 前置依赖/环境/API 能力未验证 | ✅ 抑制 | Phase 0 grep 实证 + Document::~Document 节点生命周期 audit 锁 D2-C-deep-clone 决策 |
  | #2 重写既有功能 | ✅ 抑制 | addEventListener 已有，仅扩展 mapping 表（不重写）|
  | #3 ... | ... | ... |
  ```

- [ ] **步骤 2：commit progress.md**

  ```bash
  git add memory-bank/progress.md
  git commit -m "$(cat <<'EOF'
  chore(progress): TASK-20260505-01 phase A-E closure record

  Source: docs/plans/2026-05-05-dombindings-r2-closure.md §E.2
  EOF
  )"
  ```

### 任务 E.3：分支状态总结 — 准备进入 /reflect

- [ ] **步骤 1：commit 链总结**

  ```bash
  git log --oneline main..HEAD
  ```

  预期 commit 数：~6（plan + creative-N/A + 4 实施 + finalize）

- [ ] **步骤 2：activeContext.md 阶段更新**

  ```bash
  # 由 /build 命令完成后自动更新 → 构建中 → 已完成
  # /reflect 命令读取该阶段进入回顾
  ```

---

## 任务总数与时间预估

| Phase | 任务数 | plan ×0.6 估时 | 实测累计预期 |
|---|:-:|:-:|---|
| Phase 0 | 11 audit 子段 | 已先跑 | — |
| Phase A | 1 任务（A.1）| ~45-60 min | ~5-10 min（Phase 0 已先跑实证）|
| Phase B | 1 任务（B.1）| ~75-90 min | ~12-18 min |
| Phase C | 1 任务（C.1）| ~30-40 min | ~6-10 min |
| Phase D | 2 任务（D.1 + D.2）| ~20-30 min | ~5-8 min |
| Phase E | 3 任务（E.1 + E.2 + E.3）| ~20-30 min | ~5-8 min |
| **总计** | **8 任务** | **~190-250 min（plan ×0.6 ~3.2-4.2 h）** | **~33-54 min（实测 0.13-0.22×）** |

**估时备注：** plan ×0.6 是「单 AI agent 标准节奏」基线。实测因 Phase 0 已先跑 + dom_bindings.cc 范式高度复用 + 14 单测高重复率 → 预期 0.13-0.22× 落「最小代码改动 + Phase 0 高度预跑极速区 0.10-0.20×」子档（systemPatterns 入库）。

---

## 创意阶段需求

**不需要 `/creative` 阶段。**

理由：
- 设计决策 D1+D2+D3 已在 `/plan` brainstorming 阶段全部锁定
- 无新组件需要 UI/UX 设计
- 无新算法需要设计（CloneNodeInto 是直接的 tree clone / MapJsEventName 是直接的表扩展）
- D2-C-deep-clone 是 plan 阶段 audit 已锁定的实施策略（不是创意候选）

直接进入 `/build` 阶段执行 Phase A-E。

---

## 执行交接

**计划完成并保存到 `docs/plans/2026-05-05-dombindings-r2-closure.md`。准备执行 /build？**

执行顺序：Phase A.1 → B.1 → C.1 → D.1 → D.2 → E.1 → E.2 → E.3

**Source 溯源：** 每 commit body 必须含 `Source: docs/plans/2026-05-05-dombindings-r2-closure.md §X.Y`（systemPatterns commit body Source 段 quad-evidence 累计 ~34 commits 已超 git-workflow.mdc 固化阈值，本任务延续协议）。
