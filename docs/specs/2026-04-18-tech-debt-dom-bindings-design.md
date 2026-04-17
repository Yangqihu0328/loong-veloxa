# 设计规格：消化关键技术债务（#45 / #46 / #48 / #50）

**任务 ID**：TASK-20260418-01
**日期**：2026-04-18
**类型**：技术债务消化（无新功能，纯重构 + 实现补全）
**复杂度**：Level 3
**目标规模**：单任务，分阶段 TDD 实施

---

## 1. 背景与目标

本任务针对 TASK-20260414-01（功能补全）归档时遗留的 4 条 P1 技术债务一次性清理。所有债务集中在 JS-DOM 绑定与软件渲染器的字体路径，属于内部重构 + 实现补全，**不改变对外 API 语义**。

- **#45**：`veloxa/script/dom_bindings.cc` 存在 4 个文件级全局变量，妨碍多 Document / 多 context 场景
- **#46**：`StyleGetProp` 一直返回空字符串，未实现读路径
- **#48**：`SoftwareCanvas::DrawText` 每次调用创建并销毁 HarfBuzz `hb_font_t`，性能损失
- **#50**：`addEventListener` lambda 捕获裸 `JSContext*` + `JSValue`，`Unbind` 未按正确顺序清理 EventManager listener，存在 Use-After-Free 风险

---

## 2. 代码现状核对

以下状态以 `main` 分支 `5bc6470` 提交为准（不以 `memory-bank/systemPatterns.md` 描述为准，后者早于本次审查）：

### 2.1 dom_bindings.cc（关键全局状态）

| 位置 | 声明 | 用途 | 是否已用 context opaque |
|------|------|------|-------------------------|
| line 84 | `JSClassID g_element_class_id` | Element JS 类 ID | 否（文件级全局） |
| line 103 | `JSClassID g_style_class_id` | Style proxy JS 类 ID | 否（文件级全局） |
| line 289 | `TrackedCallbacks g_tracked_callbacks` | 追踪 `addEventListener` 的 JSValue 用于 Unbind 释放 | 否 |
| line 420 | `dom::Document* g_bound_doc` | `document.getElementById` 的 DOM 入口 | 否（与 `DomBindings::doc_` 重复） |
| line 534 | `JS_SetContextOpaque(ctx, this)` | `GetBindings(ctx)` 已可用，当前仅 `GetEventManager` 走此路径 | 是（增补，未替代） |

### 2.2 StyleGetProp（dom_bindings.cc line 151-157）

```cpp
JSValue StyleGetProp(JSContext* ctx, JSValueConst this_val, int magic) {
  auto* so = GetStyleOpaque(this_val);
  if (!so || !so->element || magic < 0 || magic >= kStyleMappingCount) {
    return JS_NewString(ctx, "");
  }
  return JS_NewString(ctx, "");  // 始终返回空
}
```

消费者：JS 通过 `el.style.backgroundColor` 读取值。当前任何读取都得到 `""`，与 Web 规范不一致。

### 2.3 SoftwareCanvas::DrawText（software_canvas.cc line 143-268）

- line 172：`FT_Set_Pixel_Sizes(face, 0, pixel_size)`（改变 FT_Face 的当前尺寸）
- line 174：`hb_font_t* hb_font = hb_ft_font_create_referenced(face)`（**每次**创建）
- line 267：`hb_font_destroy(hb_font)`（**每次**销毁）

每字渲染都重建 HarfBuzz 字体对象，`hb_ft_font_create_referenced` 内部会遍历 FT_Face 的 charmap/metric 表，开销非零。

### 2.4 addEventListener Lambda（dom_bindings.cc line 293-330）

```cpp
JSValue callback = JS_DupValue(ctx, argv[1]);
// ...
g_tracked_callbacks.Track(callback);

JSContext* captured_ctx = ctx;
em->AddEventListener(
    el, event_type,
    [captured_ctx, callback](event::DOMEvent& event) mutable {
      // ... JS_Call(captured_ctx, callback, ...)
    });
```

`Unbind`（543-557 行）仅做：
1. `g_tracked_callbacks.FreeAll()` —— 释放所有 JSValue callback
2. 删除 `document` 全局对象、清空 `ctx_`

**未调用** `em_->RemoveEventListeners(...)`。EventDispatcher 中 lambda 仍保留；若之后 `EventManager::HandleInput` 触发事件 → lambda 用**已释放的 callback** 调用 `JS_Call` → UAF。

### 2.5 相关底层设施

- **`Element::inline_declarations()`**（element.h line 46）：返回 `const SmallVector<css::Declaration, 8>*`（空时返回 nullptr）
- **`CssValue`**（css_value.h）：tagged union，7 种 ValueType，对应 length / color / enum / number / auto / inherit / initial
- **`EventManager::RemoveEventListeners(Element*)`**（event_manager.h line 24）：按 Element 移除该元素所有 listener
- **`FontManager`**（font_manager.h）：持有 `FT_Face fonts_[kMaxFonts]`，`FontHandle = u32`（1-based，0 为 invalid）
- **`GlyphCache`**（glyph_cache.h）：按 `(FontHandle, glyph_id, pixel_size)` 缓存 FT 光栅化结果

---

## 3. 设计决策

### 3.1 #45 — 全局状态迁移（方案 A1）

**分类原则**：
- `JSClassID`：QuickJS 进程级 per-Runtime 单调递增 ID，**不是**实例状态 → 保持静态
- `TrackedCallbacks` 与 `Document*`：真正的实例状态 → 迁移到 `DomBindings` 成员

**实施**：

1. **`JSClassID` 静态化**：`dom_bindings.cc` 匿名 namespace 内保留 `static JSClassID g_element_class_id` / `g_style_class_id`，但规范使用 QuickJS 的幂等初始化约定：
   ```cpp
   if (g_element_class_id == 0) {
     JS_NewClassID(rt, &g_element_class_id);
     JS_NewClass(rt, g_element_class_id, &g_element_class_def);
   } else {
     if (!JS_IsRegisteredClass(rt, g_element_class_id))
       JS_NewClass(rt, g_element_class_id, &g_element_class_def);
   }
   ```
   - ID 仅分配一次（跨 Runtime 共享）
   - 每个 Runtime 需独立注册类（`JS_NewClass`），用 `JS_IsRegisteredClass` 判断
   - 改名以去除 `g_` 前缀，改为 `s_` 标记"static within file"

2. **`TrackedCallbacks` 迁移到实例**：
   - 从 `dom_bindings.cc` 匿名 namespace 的 `g_tracked_callbacks` 迁移为 `DomBindings::tracked_callbacks_`（嵌套 struct）
   - 所有使用点（`ElementAddEventListener`）通过 `GetBindings(ctx)->tracked_callbacks_` 访问

3. **`g_bound_doc` 删除**：
   - `DocGetElementById` 改为 `GetBindings(ctx)->document()`
   - `RegisterDocumentObject` 不再写入 `g_bound_doc`
   - `Unbind` 不再重置 `g_bound_doc`

4. **Header 层**：`DomBindings` 类暴露私有嵌套结构定义（或前置声明 + pimpl）。选择**私有嵌套**，避免 pimpl 引入堆分配：
   ```cpp
   class DomBindings {
     // ... public 接口不变
    private:
     struct TrackedCallbacks {
       JSContext* ctx = nullptr;
       Vector<JSValue> callbacks;
       void Track(JSValue v) { callbacks.push_back(v); }
       void FreeAll();
     };
     Vector<dom::Element*> listener_elements_;  // 见 §3.4
     TrackedCallbacks tracked_callbacks_;
     // 原有 ctx_ / doc_ / em_
   };
   ```
   - `Vector<JSValue>` 需要 `JSValue` 完整类型 → header 中 `extern "C" { #include "quickjs.h" }`
   - 若不希望污染 header，仅暴露 opaque struct 前置声明并在 .cc 中定义 —— 但这要求 `tracked_callbacks_` 通过 `std::unique_ptr` 持有 → 妥协为 **pimpl 风格的 `InstanceData* data_`**

**最终选择**：采用 **pimpl 风格**，`DomBindings::h` 仅持有 `struct InstanceData; std::unique_ptr<InstanceData> data_;`，所有 QuickJS 类型与 Vector 细节保留在 .cc 中。好处：header 不 include quickjs.h，仍前置声明即可；唯一代价是 `Bind` 时一次 heap 分配（生命周期 = DomBindings），在嵌入式场景可接受。

### 3.2 #46 — StyleGetProp 读路径（方案 B1）

**覆盖范围**：7 个 camelCase 映射属性中，除 `display`（Enum）外全部实现读路径：

| camelCase | PropertyId | ValueType | 序列化格式 |
|-----------|------------|-----------|------------|
| `backgroundColor` | `kBackgroundColor` | `kColor` | `"rgba(r, g, b, a)"` / `"rgb(r, g, b)"`（当 a==255） |
| `color` | `kColor` | `kColor` | 同上 |
| `width` | `kWidth` | `kLength` / `kAuto` | `"16px"` / `"50%"` / `"1em"` / `"auto"` |
| `height` | `kHeight` | `kLength` / `kAuto` | 同上 |
| `opacity` | `kOpacity` | `kNumber` | `"0.5"`（去尾 0，保留必要小数） |
| `fontSize` | `kFontSize` | `kLength` | 同 width |
| `display` | `kDisplay` | `kEnum` | 暂返回 `""`（文档注明） |

**实现点**：
- 新增匿名 namespace 辅助：`void SerializeCssValue(const CssValue&, String* out)`
- 无 inline_declarations 或未匹配时返回 `""`（与浏览器 `CSSStyleDeclaration.getPropertyValue` 一致）
- 数字序列化：`snprintf(buf, "%g", value)` 获得紧凑形式（`0.5` 而非 `0.500000`）

### 3.3 #48 — HarfBuzz font 缓存（方案 C1）

**设计**：将 `hb_font_t*` 作为 `FontManager::FontEntry` 的一部分，与 `FT_Face` 同生命周期：

```cpp
struct FontEntry {
  FT_FaceRec_* face = nullptr;
  hb_font_t* hb_font = nullptr;  // 新增
  u32 hb_pixel_size = 0;         // 新增：追踪 hb_font 当前已知的 pixel size
  FontHandle handle = kInvalidFont;
  // ... 其他字段不变
};
```

**新增 API**：
```cpp
// font_manager.h
class FontManager {
 public:
  // 按需创建并返回 hb_font；若 pixel_size 变化则调用 hb_ft_font_changed
  hb_font_t* GetHbFont(FontHandle handle, u32 pixel_size);
  // ...
};
```

**头文件依赖处理**：
- `font_manager.h` 前置声明 `struct hb_font_t;`（HarfBuzz 通过 typedef `typedef struct hb_font_t hb_font_t;`，可前置声明）
- 若 HarfBuzz 没有公开的 `struct hb_font_t` 只有 typedef，则在 `font_manager.h` 中用 `struct hb_font_t;` 前置声明即可（C 约定，typedef 会同名）

**`FontManager::Shutdown`** 增加释放：
```cpp
for (auto& entry : fonts_) {
  if (entry.hb_font) hb_font_destroy(entry.hb_font);
  if (entry.face) FT_Done_Face(entry.face);
}
```

**`SoftwareCanvas::DrawText`** 修改：
```cpp
FT_Set_Pixel_Sizes(face, 0, pixel_size);
hb_font_t* hb_font = font_manager_->GetHbFont(font, pixel_size);
// ... 不再 hb_font_destroy
```

**语义保证**：`GetHbFont` 内部规则：
- 首次调用：`hb_ft_font_create_referenced(face)` + 记录 `hb_pixel_size = pixel_size`
- 后续调用且 `pixel_size` 相同：直接返回已有 `hb_font`
- 后续调用且 `pixel_size` 不同：调用 `hb_ft_font_changed(hb_font)` 通知 HarfBuzz 重读 face 尺寸 + 更新 `hb_pixel_size`

**调用方契约**：调用者须**先**调用 `FT_Set_Pixel_Sizes(face, 0, pixel_size)`，**再**调用 `GetHbFont(handle, pixel_size)`。在代码注释与函数文档中明确。

### 3.4 #50 — addEventListener 生命周期（方案 D1）

**核心规则**：Unbind 时必须先拆除 EventManager 中的 lambda，再释放 lambda 捕获的 JSValue。

**实施**：

1. `DomBindings`（pimpl 的 InstanceData）新增字段：
   ```cpp
   Vector<dom::Element*> listener_elements_;  // 去重的 Element 清单
   ```

2. `ElementAddEventListener` 修改：
   ```cpp
   auto* bindings = GetBindings(ctx);
   if (!bindings) { JS_FreeValue(ctx, callback); return JS_UNDEFINED; }
   // 去重 push
   bool found = false;
   for (auto* e : bindings->listener_elements_) { if (e == el) { found = true; break; } }
   if (!found) bindings->listener_elements_.push_back(el);
   bindings->tracked_callbacks_.Track(callback);
   // lambda 捕获方式不变（captured_ctx + callback）
   ```

3. `DomBindings::Unbind` 新顺序：
   ```cpp
   // 1. 先从 EventManager 拆除 lambda（会触发 lambda 析构）
   if (em_) {
     for (auto* el : data_->listener_elements_) {
       em_->RemoveEventListeners(el);
     }
   }
   data_->listener_elements_.clear();
   // 2. 再释放 JSValue callback
   data_->tracked_callbacks_.FreeAll();
   // 3. 其余清理（document global、ctx opaque）
   ```

4. **析构顺序保障**：`~DomBindings` 调用 `Unbind`，因此 DomBindings 析构早于 EventManager 时也安全；若相反顺序（EventManager 先析构）——此场景当前代码也会 UAF，不在本次修复范围，在 `progress.md` 记录为已知约束（宿主须确保 DomBindings 先于 EventManager 析构）。

5. **`ElementRemoveEventListener`**（现为简化的"移除该元素所有 listener"）：
   - 需要同步从 `listener_elements_` 中移除 `el`（避免 Unbind 时对已无 listener 的 el 重复调用 —— 虽然 `RemoveEventListeners` 幂等，但保持清单精确）
   - 本次**不**实现按 `(type, handler)` 精确移除（属于技术债 #47，留到后续任务）

---

## 4. 测试策略

### 4.1 新增测试（基于 `tests/script/dom_bindings_test.cc`）

| 测试 | 债务 | 描述 |
|------|------|------|
| `JSClassIdStableAcrossBindings` | #45 | 两个 `DomBindings` 实例交替 Bind 同一 JSRuntime，JSClassID 保持不变 |
| `NoGlobalDocumentLeakAcrossInstances` | #45 | 实例 A `Bind(docA)` → 实例 B `Bind(docB)` → A 通过 A.context() 仍见 docA，B 通过 B.context() 见 docB（多 context 隔离） |
| `StyleGetBackgroundColor` | #46 | 设置 `el.style.backgroundColor = 'red'`，读回返回 `"rgba(255, 0, 0, 255)"` 或 `"rgb(255, 0, 0)"` |
| `StyleGetWidth` | #46 | 设置 `el.style.width = '100px'`，读回 `"100px"` |
| `StyleGetAuto` | #46 | 设置 `el.style.width = 'auto'`，读回 `"auto"` |
| `StyleGetUnsetReturnsEmpty` | #46 | 未设置的属性读回 `""` |
| `StyleGetOpacity` | #46 | `opacity = '0.5'` → `"0.5"` |
| `StyleGetDisplayStubReturnsEmpty` | #46 | 注明 enum 未实现，断言返回 `""`（保证契约稳定） |
| `UnbindRebindNoCrash` | #50 | Bind → addEventListener → Unbind → HandleInput（应无事件触发、无 crash） |
| `UnbindClearsEventListeners` | #50 | Bind → addEventListener → Unbind → EventManager 再触发 → JS 环境不崩溃（测试 UAF 修复） |

### 4.2 新增测试（基于 `tests/text/font_manager_test.cc`，如不存在则创建）

| 测试 | 债务 | 描述 |
|------|------|------|
| `GetHbFontCachesFontPerHandle` | #48 | 两次 `GetHbFont(handle, same_size)` 返回同一指针 |
| `GetHbFontReconfiguresOnSizeChange` | #48 | `GetHbFont(handle, 12)` → `GetHbFont(handle, 16)`：指针相同但内部 pixel size 已更新（通过 hb 的 ppem 或 text metrics 间接验证） |
| `ShutdownReleasesHbFonts` | #48 | `FontManager` Shutdown 后所有 hb_font 都被销毁（无内存泄漏；通过构造/析构计数或 LSan 验证） |

### 4.3 现有测试保持通过

`tests/script/dom_bindings_test.cc` 现有 12 个测试必须零修改全部通过，作为回归防线。

### 4.4 集成验证

- `ctest --output-on-failure`：所有测试通过
- 可选：连续渲染 1000 字的 benchmark 对比（仅作观察，不纳入 CI）

---

## 5. 安全考量

此任务为内部重构，不涉及外部输入、认证、网络或数据持久化。**安全分析的主要维度**：

- **Use-After-Free 消除**：#50 的核心即为修复 UAF，测试覆盖 Unbind → HandleInput 路径
- **引用计数一致性**：QuickJS JSValue 通过 `JS_DupValue` 持有引用，必须一一对应 `JS_FreeValue`；测试覆盖 Bind/Unbind 循环
- **多 Runtime 隔离**：虽然当前只有单 Runtime 场景，但方案 A1 为未来多 Runtime 做好准备（JSClassID 全进程共享 + per-Context 实例状态）
- **依赖变更**：无新增第三方依赖；HarfBuzz/FreeType API 已经在项目中使用

---

## 6. 范围外（out of scope）

以下同源技术债**不**在本次任务处理，在归档时保留到 `activeContext.md`：

- **#47**：`removeEventListener` 按 `(type, handler)` 精确移除（需要 EventManager 扩展 API）
- **#42**：transition shorthand 多条支持
- **#43**：LayoutBox.style 的 const_cast 改造
- **#46 补充**：StyleGetProp 对 Enum 类型的完整序列化（需要 PropertyId → enum string 表）
- **宿主 EventManager/DomBindings 析构顺序**：本次仅保证 `DomBindings 先析构` 的场景安全；若需支持反向顺序，需要引入弱引用机制（未来工作）

---

## 7. 风险与回退

| 风险 | 缓解 |
|------|------|
| pimpl 改造破坏 `tests/script/dom_bindings_test.cc` | 所有 public API 保持不变，先跑测试基线 |
| `JS_IsRegisteredClass` 不存在（QuickJS-NG 版本差异） | 若不存在，使用 `JS_GetClassProto + JS_IsNull` 探测 |
| `hb_font_t` 前置声明冲突 | 若 HarfBuzz header 将 `hb_font_t` 定义为 `struct hb_font_t {}` 之外的形式，改为在 font_manager.h 中 include `<hb.h>` |
| pimpl 的 `std::unique_ptr<InstanceData>` 要求 header 知道 InstanceData 析构 | 在 `dom_bindings.h` 中声明 `~DomBindings();`（非 default），在 .cc 中定义，触发完整类型的析构生成 |

**回退策略**：每条债务一个独立提交，如 `#48` 出现 HarfBuzz 前置声明问题，可单独 revert 该提交不影响其他三条。

---

## 8. 完成条件（Definition of Done）

1. ✅ `tests/script/dom_bindings_test.cc` 全部 12 + 新增 ~8 个测试通过
2. ✅ `tests/text/font_manager_test.cc` 新增 3 个测试通过
3. ✅ `dom_bindings.cc` 无文件级 `g_` 前缀的实例状态变量（保留 `s_` 前缀的静态 JSClassID 并附注释）
4. ✅ `SoftwareCanvas::DrawText` 无 `hb_ft_font_create_referenced` / `hb_font_destroy` 调用
5. ✅ `DomBindings::Unbind` 按顺序 RemoveEventListeners → FreeAll callbacks
6. ✅ 完整 `ctest` 通过，无新增 lint 错误
7. ✅ `memory-bank/techContext.md` 技术债清单更新（#45/#46/#48/#50 标记已解决；注明 #46 Enum 部分和析构顺序限制）
