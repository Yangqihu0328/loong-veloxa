# 创意设计：MVP-B 桌面 dogfood 完整档

**日期：** 2026-05-04
**状态：** 已批准（V1=D 三档分级 + B2=3 篇拆分粒度 user all_recommended 锁定）
**关联任务：** [TASK-20260504-01](../../docs/specs/2026-05-04-mvp-scope.md)
**对应 spec 段：** [`§3.2 MVP-B`](../../docs/specs/2026-05-04-mvp-scope.md) + 附录 B.1-B.10 + §3.2.1 4 项 gap 表 + §6.2 推荐立项顺序 1-2

---

## 1. 设计挑战

**问题陈述：**

MVP-B 已 ~90% 达成（B.1-B.8 全 ✅ + B.10 全 ✅ + B.9 部分 ⚠️）。**核心挑战：如何最经济地补全 4 项已识别 gap（B-G1/G2/G3 DomBindings R2 三连 + B-G4 Performance Overlay 持续 invalidate），解锁桌面 dogfood 完整工作流？**

**约束条件：**

1. **总投入 plan ×0.6 ≤ 6 h** — 短期 ROI 上限（避免成本失控）
2. **不破坏 MVP-A baseline** — 1284 ON / 1091 OFF 严守红线（沿用 [creative-mvp-A.md §6.2](./creative-mvp-A.md)）
3. **复用既有范式** — DomBindings 复用 dom_bindings.cc Phase A/B/C 三次扩展经验 / Performance Overlay 沿用 dirty_ 机制硬约束
4. **dogfood 完整链路验证** — 4 gap 补全后必须通过完整路径：编辑 HTML/CSS → Hot Reload → Inspector → Performance Overlay 多帧滑动 → Console eval → SDL2 真窗口验证
5. **lazy-attach C ABI 容错保持** — gap 补全不破坏 [TASK-20260502-02 lazy-attach 范式](../../memory-bank/archive/archive-TASK-20260502-02.md)

**成功标准：**

1. **4 项 gap 100% 闭环** — B.1-B.10 全 ✅
2. **dogfood 完整链路 smoke test 通过**（label `mvp-b-dogfood`）
3. **estimating 准确度** — 实测 plan ×0.6 落入 estimating 范围（防成本失控）
4. **0 反复模式命中** — 沿用 [TASK-20260503-04 archive §6 评估 4/7 守门记录](../../memory-bank/archive/archive-TASK-20260503-04.md)

---

## 2. 探索的方案（3 方案 — 子任务拆分粒度）

### 方案 A：全合并单 Level 4 任务（"MVP-B 一站达成"）

**描述：** 将 4 项 gap 合并为单个 Level 4 任务，1 个 spec + 1 个 plan + 多个 creative + 单分支 build → 一次性 reflection + archive。

**优点：**
- 单分支单 PR 评审成本低
- 所有 gap 间的相互依赖一次解决
- archive 文档信息密度高（4 gap 一站汇总）

**缺点：**
- Level 4 任务 plan ×0.6 估时 ~6 h 偏低（Level 4 通常 ≥ 10 h）→ Level 难定级
- 4 gap 之间**实际无强依赖**（DomBindings R2 三连与 Perf invalidate 独立）→ 强行合并丧失并行机会
- 任一 gap 阻塞 → 全部阻塞（如 perf invalidate 卡在路径选型 → DomBindings R2 也无法 archive）
- 与 [systemPatterns Level 分级标准](../../memory-bank/systemPatterns.md) 不符

**复杂度：** 中
**风险：** 中（阻塞传染 / Level 定级矛盾）

---

### 方案 B：4 项 gap 4 个独立 Level 1-3 任务（"完全拆分"）

**描述：** 每个 gap 一个独立任务（B-G1 / B-G2 / B-G3 / B-G4 各一个）/ 各自独立 spec + plan + commit + reflection + archive。

**优点：**
- 最大并行性（4 个任务可任意顺序 / 不相互阻塞）
- 每个任务 Level 1-2 / 估时 < 1 h / 可作为 onboarding 任务
- archive 数量增加（systemPatterns 数据点群组 +4）

**缺点：**
- DomBindings R2 三项 gap 实际**强相关**（同一 dom_bindings.cc / 同一 binding 范式 / 同一测试集合）→ 拆分后 4 次重复读 / 写 / 测试范式
- 总 overhead 显著（4 × VAN + 4 × plan + 4 × reflection + 4 × archive ≈ 1-2 h 额外开销）
- 每个任务的 plan ×0.6 估时 0.5-1 h / 但 overhead 0.3-0.5 h / 实质成本 / 实际工作 ratio 偏高

**复杂度：** 低（每个任务）/ 高（总协调）
**风险：** 中（重复 overhead 浪费）

---

### 方案 C：按 gap 类型分组（推荐 ✅）

**描述：** 按**自然边界**分组：
- **任务 1：** B-G1+G2+G3 DomBindings R2 三连补全（合并 Level 3 / ~2-4 h plan ×0.6）
- **任务 2：** B-G4 Performance Overlay 持续 invalidate（Level 1-3 / ~30 min-2 h plan ×0.6）

**优点：**
- DomBindings 三项强相关 → 合并复用 binding 扩展范式（同 dom_bindings.cc / 同 binding 测范式 / 同分支）/ 减少重复 overhead
- Perf invalidate 独立无关 → 单立任务清晰边界
- 2 个独立子任务：可并行（开发者 A 做 DomBindings R2 / 开发者 B 做 perf invalidate）
- 每个子任务 Level 适当（Level 3 / Level 1-3 都符合 [systemPatterns Level 分级标准](../../memory-bank/systemPatterns.md)）
- 与 [TASK-20260502-01/02/03 单 Level 3 任务范式](../../memory-bank/archive/archive-TASK-20260502-01.md) 一致

**缺点：**
- 仍有 2 次 VAN/plan/reflection/archive overhead（不如方案 A 一次性，但好于方案 B 四次）
- 任务 2 路径选型（路径 a 公开 C ABI vs 路径 b CSS animation 注入）需要额外决策点

**复杂度：** 低
**风险：** 低

---

### 2.x 方案对比

| 维度 | 方案 A 全合并 | 方案 B 完全拆分 | 方案 C 按类型分组 |
|---|:-:|:-:|:-:|
| 子任务数 | 1 | 4 | **2** |
| Level 定级合理性 | ❌（Level 4 但估时 ~6 h 偏低）| ⚠️（4 × Level 1-2）| ✅（Level 3 + Level 1-3）|
| 并行机会 | 无 | 极高 | 中（2 任务可并行）|
| 阻塞风险 | 高（一损俱损）| 低 | 低 |
| 重复 overhead | 极低 | 极高（4 × VAN/plan/reflection）| **低** |
| 与既有范式协同 | ❌ | ⚠️ | ✅ 完全一致 |
| dom_bindings.cc 复用 binding 范式 | ✅ | ⚠️（4 次重读）| ✅ |
| archive 信息密度 | 高 | 低 | **中-高** |
| **推荐度** | ❌ | ❌ | **⭐ 主推** |

---

## 3. 选定方案：C 按 gap 类型分组（2 子任务）

**推荐方案：C — 2 个独立子任务**

### 3.1 选择理由

1. **DomBindings R2 三连补全的强相关性** — 三项 gap 同一 `dom_bindings.cc` 文件 / 同一 binding 范式 / 同一测试 fixture / 强行拆分浪费 ~30-50% 重复 overhead
2. **Perf invalidate 的独立性** — 涉及完全不同的代码路径（`UpdateManager` / `dirty_` 机制 / `vx_view_invalidate()` 候选 C ABI 或 CSS animation 注入）/ 与 DomBindings 零交叉
3. **Level 定级合理** — DomBindings R2 = Level 3（含 binding API 设计 + creative ×1）/ Perf invalidate = Level 1-3（视路径选型）
4. **可并行 / 优先级灵活** — 2 个子任务可独立排期 / DomBindings R2 优先（dogfood 视觉自动恢复）/ Perf invalidate 次之（dirty rect 多帧验证可见性）
5. **与 [TASK-20260502-01 R2 P3 候选清单](../../memory-bank/archive/archive-TASK-20260502-01.md) 设计意图一致** — 原本就是按 R2 三项一起记录

### 3.2 推荐立项顺序

| 顺序 | 子任务 | Level | plan ×0.6 | 优先级理由 |
|:-:|---|:-:|:-:|---|
| **1** | DomBindings R2 三连补全（B-G1+G2+G3）| L3 | ~2-4 h | dogfood 视觉自动恢复 / 第三方集成者最高 ROI |
| **2** | Performance Overlay 持续 invalidate 机制（B-G4）| L1-3 | ~30 min-2 h（视路径）| dogfood 视觉一致性提升 / 次优先级 |

### 3.3 风险缓解

| 风险 | mitigation |
|---|---|
| DomBindings R2 估时偏离（如 archive §6 校准 -30% 经验失效）| 沿用 [archive-TASK-20260502-01 §6 校准记录](../../memory-bank/archive/archive-TASK-20260502-01.md) / reflection 阶段更新校准范围 |
| Perf invalidate 路径选型分歧 | 见 §6.2.B-G4 路径决策表 / 推荐路径 b 优先（成本更低）|
| 任务 1 阻塞任务 2 | 2 任务设计上无依赖 / 可并行启动 |
| 总成本失控（> 6 h plan ×0.6）| 各任务独立 reflection 阶段实测 / 累积超 6 h 时停止评估 |

---

## 4. 算法/坐标约定一图（§d.1 audit）

**N/A** — 本 creative 无算法 / 无 ≥2 坐标系 / 无 ≥2 方向多锚点偏移 → [`/creative` §d.1 P0 强制项](../../.cursor/commands/creative.md) 不适用本 creative。

> **注：** 本 creative 的 §6 子任务 plan-fact 占位（DomBindings R2 三连 + perf invalidate）涉及的具体算法决策（如 `Element.children` 是否 live collection / `addEventListener` 的 capture/bubble 阶段实现）将在**各子任务**自己的 creative 阶段做 §d.1 audit；本 creative 仅做"子任务拆分粒度"决策，不展开算法细节。

---

## 5. 算法伪码累积语义 explicit method（§d.2 audit）

**N/A** — 本 creative 无算法伪码 / 无累积量 → [`/creative` §d.2 P1 强制项](../../.cursor/commands/creative.md) 不适用本 creative。

> **注：** 同上，各子任务自己 creative 阶段做 §d.2 audit。

---

## 6. 实现指导（4 项 gap 详细补全方案）

### 6.1 子任务 1：B-G1+G2+G3 DomBindings R2 三连补全

#### 6.1.A B-G1：`Element.children` 集合 getter

**API 设计：**

```javascript
// JS 端
let children = element.children;  // 返回 HTMLCollection-like array
console.log(children.length);
console.log(children[0]);  // 第一个子元素
```

**C++ 端（dom_bindings.cc）：**

```cpp
// 参考 Phase A/B/C 已建立的 binding 范式
JSValue Element_children_get(JSContext* ctx, JSValueConst this_val, int magic) {
  Element* el = JS_GetOpaque(this_val, element_class_id);
  if (!el) return JS_UNDEFINED;

  JSValue arr = JS_NewArray(ctx);
  uint32_t i = 0;
  for (auto& child : el->children) {
    if (child->IsElement()) {
      JS_SetPropertyUint32(ctx, arr, i++, ElementToJS(ctx, child.get()));
    }
  }
  return arr;
}
```

**设计决策（待子任务 creative 阶段确认）：**

- 静态 array snapshot vs live HTMLCollection？— 推荐**静态 snapshot**（简单 / 与 dogfood 场景一致 / live 需要 mutation observer）
- 仅 Element children vs 含 TextNode？— 推荐**仅 Element children**（与 W3C `Element.children` 标准一致 / `childNodes` 含全部 / 留待 C-G5 节点动态创建删除时一并实现）

#### 6.1.B B-G2：`element.addEventListener`

**API 设计：**

```javascript
element.addEventListener('click', (e) => { console.log('clicked'); });
element.addEventListener('mouseover', handler, { capture: true });
```

**C++ 端：**

```cpp
JSValue Element_addEventListener(JSContext* ctx, JSValueConst this_val,
                                  int argc, JSValueConst* argv) {
  Element* el = JS_GetOpaque(this_val, element_class_id);
  if (!el || argc < 2) return JS_UNDEFINED;

  const char* event_type = JS_ToCString(ctx, argv[0]);
  JSValue callback = JS_DupValue(ctx, argv[1]);
  // 解析 capture / passive / once options
  bool capture = false;
  if (argc >= 3 && JS_IsObject(argv[2])) {
    JSValue cap = JS_GetPropertyStr(ctx, argv[2], "capture");
    if (JS_IsBool(cap)) capture = JS_ToBool(ctx, cap);
    JS_FreeValue(ctx, cap);
  }

  el->event_listeners.push_back({event_type, callback, capture});
  JS_FreeCString(ctx, event_type);
  return JS_UNDEFINED;
}
```

**设计决策：**

- 复用既有 `EventManager` W3C 三阶段事件分发？— 推荐**复用**（避免双轨 dispatcher）
- callback 生命周期管理 → callback 必须 `JS_DupValue` 入存储 / Element 析构时 `JS_FreeValue` 释放
- removeEventListener 是否一并实现？— 推荐**一并实现**（W3C 配套 API / 边际成本极低）

#### 6.1.C B-G3：`element.innerHTML` setter

**API 设计：**

```javascript
element.innerHTML = '<span>hello</span>';
```

**C++ 端：**

```cpp
JSValue Element_innerHTML_set(JSContext* ctx, JSValueConst this_val, JSValueConst val) {
  Element* el = JS_GetOpaque(this_val, element_class_id);
  if (!el) return JS_UNDEFINED;

  const char* html = JS_ToCString(ctx, val);
  if (!html) return JS_UNDEFINED;

  // 复用 HTMLParser 解析 fragment
  auto fragment = HTMLParser::ParseFragment(html);
  el->ReplaceChildren(std::move(fragment));
  el->MarkDirty();  // 触发 layout + render

  JS_FreeCString(ctx, html);
  return JS_UNDEFINED;
}
```

**设计决策：**

- 是否触发 `MarkDirty()` 自动 invalidate？— 推荐**自动**（dogfood 场景预期）
- 是否实现 `outerHTML`？— 推荐**仅 innerHTML**（W3C 子集 / outerHTML 留待后续 C-G5）
- 是否需要 sanitize（防 XSS）？— **不需要**（Veloxa 加载 HTML/CSS 来自本地 / 不暴露给不可信 input）但留 systemPatterns 注释提示

#### 6.1.D 单子任务测试矩阵

| smoke test | 验证 |
|---|---|
| `dom_bindings_element_children_smoke` | B-G1 静态 snapshot 正确 |
| `dom_bindings_element_children_filter_text_smoke` | B-G1 仅 Element 不含 TextNode |
| `dom_bindings_element_addEventListener_click_smoke` | B-G2 click 事件触发 |
| `dom_bindings_element_addEventListener_capture_smoke` | B-G2 capture 阶段 |
| `dom_bindings_element_innerHTML_set_smoke` | B-G3 fragment 解析 + 替换 |
| `dom_bindings_element_innerHTML_dirty_smoke` | B-G3 自动 MarkDirty 触发 |
| `dom_bindings_r2_lazy_attach_smoke` | DEVTOOL=OFF lazy-attach 容错（沿用 [TASK-20260502-02 范式](../../memory-bank/archive/archive-TASK-20260502-02.md)）|

预计新增 ctest 数量：~30-50（DEVTOOL=ON）+ ~10（DEVTOOL=OFF stub）

---

### 6.2 子任务 2：B-G4 Performance Overlay 持续 invalidate

#### 6.2.A 路径决策表

| 路径 | 描述 | 优点 | 缺点 | 估时 plan ×0.6 |
|:-:|---|---|---|:-:|
| **路径 a** | 公开 `vx_view_invalidate()` C ABI | 通用 / 第三方集成者可调用 / 适用所有 demo | 新公开 ABI / 维护成本上升 / 需稳定语义保证 | ~1-2 h |
| **路径 b** | `hello_devtool` 注入 CSS animation | 零新 ABI / 利用 existing transition manager 的持续 invalidate / 演示效果良好 | 仅适用 example demo / 第三方集成者无 API 可用 | ~30-60 min |
| **路径 c** | DevTool 内部启动 Performance Overlay 时强制注入 invalidate timer | 自动开启 / 无需用户配合 | 60fps 强制 invalidate 增加 CPU 负担 / 与节能场景冲突 | ~1-2 h |

#### 6.2.B 推荐路径

**优先路径 b**（~30-60 min plan ×0.6）：

1. **成本最低** — 无新 ABI / 无维护负担 / 仅修改 `hello_devtool.cc` 注入一个 CSS keyframes 让某元素持续旋转
2. **演示效果一致** — Performance Overlay 多帧 dirty rect 边框可视化 / 60 帧 ring buffer 滑动平均显著
3. **可作为 dogfood smoke test 基线** — 与 `mvp-b-dogfood` label 自然集成

**长期路径 a**（候选 / 视第三方需求触发）：

- 当第 N 个第三方集成者询问"如何主动触发 invalidate"时再立项
- 设计时复用 [TASK-20260502-01 vx_view_set_dirty_callback 双层 API 范式](../../memory-bank/archive/archive-TASK-20260502-01.md)

#### 6.2.C smoke test

| smoke test | 验证 |
|---|---|
| `perf_overlay_persistent_invalidate_smoke` | 路径 b — CSS animation 注入后 dirty rect 多帧可见 |
| `perf_overlay_60_frame_ring_buffer_smoke` | 60 帧滑动平均 frame time 计算正确 |

预计新增 ctest 数量：~5-10（DEVTOOL=ON）/ DEVTOOL=OFF 0（stub 不暴露）

---

## 7. dogfood 工作流完整链路

### 7.1 期望 dogfood 链路（4 gap 闭环后）

```
┌────────────────────────────────────────────────────────────────────┐
│  开发者 dogfood 工作循环                                            │
│                                                                    │
│  1. 编辑 HTML/CSS/JS 文件                                           │
│         ↓                                                          │
│  2. Hot Reload (DevTool Phase C) 自动检测变化 → 重载              │
│         ↓                                                          │
│  3. Inspector (DevTool Phase A) F12 toggle / 节点树 / 计算样式    │
│         ↓                                                          │
│  4. Performance Overlay (DevTool Phase B) F11 toggle             │
│         + dirty rect 多帧持续可见 ✨ (B-G4)                        │
│         + 60 帧 ring buffer 滑动平均 frame time                  │
│         ↓                                                          │
│  5. Console JS REPL (DevTool Phase D) ` toggle / eval / log    │
│         + element.children ✨ (B-G1)                               │
│         + element.addEventListener ✨ (B-G2)                       │
│         + element.innerHTML = '...' ✨ (B-G3)                      │
│         ↓                                                          │
│  6. SDL2 真窗口验证 (A.5)                                          │
│         ↓                                                          │
│  loop back to step 1 ─────────────────────────────────────────────┘
└────────────────────────────────────────────────────────────────────┘
```

### 7.2 dogfood smoke test（label `mvp-b-dogfood`）

建议在 MVP-B 4 gap 全部闭环后加：

```cmake
add_test(NAME mvp_b_dogfood_full_loop
         COMMAND $<TARGET_FILE:hello_devtool> --smoke --dogfood-loop)
set_tests_properties(mvp_b_dogfood_full_loop PROPERTIES
                     LABELS mvp-b-dogfood)
```

---

## 8. 推荐 example 范例（hello_mvp_b.cc 占位 / 非本任务实施）

```cpp
// examples/hello_mvp_b.cc — MVP-B 桌面 dogfood 完整 demo
// 验收：完整 dogfood 链路一站演示

#include <veloxa/veloxa_api.h>

int main() {
  auto* event_loop = vx_event_loop_create_sdl2();
  auto* view = vx_view_create();
  auto* surface = vx_surface_create_window(800, 600, "MVP-B Demo");

  vx_view_load_html(view, R"(
    <div id="container">
      <button id="btn">Click me</button>
      <p id="msg">Hello, MVP-B.</p>
      <img src="logo.png" alt="logo">
    </div>
  )");

  vx_view_load_css(view, R"(
    #btn { padding: 10px; background: blue; color: white; }
    #btn:hover { background: red; transition: background 0.3s; }
    #msg { font-size: 24px; }
  )");

  vx_view_load_js(view, R"(
    let btn = document.getElementById('btn');
    btn.addEventListener('click', () => {
      let msg = document.getElementById('msg');
      msg.innerHTML = '<strong>Clicked!</strong>';
      console.log('Children of container:', document.getElementById('container').children.length);
    });
  )");

  // F12 / F11 / ` 三键打开 DevTool 4 件套
  vx_devtool_attach(view);

  vx_event_loop_run(event_loop);
  return 0;
}
```

---

## 9. 安全考量

### 9.1 与 MVP-B 范围一致的威胁面

完全沿用 MVP-A + B.10 已就位的 5+T 威胁面 mitigation：

| Threat | 来源 | 与 MVP-B 4 gap 的关系 |
|---|---|---|
| T1 任意 eval（QuickJS interrupt handler）| TASK-20260503-05 | DomBindings R2 不引入新 eval / Console JS REPL 已 isolated |
| T2 路径穿越 | foundation 文件系统 sandbox | 无变化 |
| T3 redaction | C ABI audit | 无变化 |
| T5 process 隔离 | DevTool isolated UpdateManager | Perf invalidate 不破坏隔离 |
| T6 callback budget | DevTool callback 限速 | DomBindings R2 listener 数量 / `addEventListener` 不限制（dogfood 场景）|
| T7 buffer overflow | SoftwareCanvas 边界 | innerHTML setter parser 已经做边界检查 |

### 9.2 DomBindings R2 三连引入的新威胁面（轻微）

| 风险 | 评估 | mitigation |
|---|:-:|---|
| `innerHTML` setter XSS | 低（Veloxa 加载 HTML/CSS 来自本地 / 不暴露给不可信 input）| 在 systemPatterns 注释提示 / 不在 MVP-B 范围 sanitize |
| `addEventListener` 监听器无限增长（DoS）| 低 | 每个 Element listener 数量 ≤ 1024 软上限（与 callback budget 协同 — 待子任务实施时确认）|
| `Element.children` JS 端持有引用导致 C++ Element 提前释放 | 低 | 静态 snapshot 不持有指针（按值复制 element ID）|

### 9.3 Perf invalidate 引入的新威胁面（无）

路径 b（CSS animation 注入）= 利用 existing transition manager / 不引入新威胁面。

---

## 10. 与既有 systemPatterns 协同度自检

| # | systemPattern | 本 creative 对应 |
|:-:|---|---|
| 1 | DevTool 5 大可复用范式（双 UpdateManager / 双层 API / lazy-attach C ABI / canvas Translate / 资源嵌入）| ✅ 直接复用（B-G1/G2/G3 沿用 dom_bindings.cc binding 范式 + B-G4 路径 b 复用 transition manager）|
| 2 | DevTool isolated JSRuntime 协议 + DomBindings host data channel | 引用（Console JS REPL 已建立 / DomBindings R2 三连不破坏隔离）|
| 3 | Phase 0 投入越深 / build phase 越快定律 sept-evidence | ✅ 各子任务 plan 阶段沿用 |
| 4 | commit body Source 溯源 + 实测数据 quad-evidence | ✅ 各子任务 commit 沿用 |
| 5 | Level 4 蓝图任务 V2=a 工作流变体 | 引用（本任务是蓝图 / 子任务非蓝图 / 走标准 Level 3 / Level 1-3 流程）|
| 6 | A14 链接闭包零字节增长守门 | ✅ DEVTOOL=OFF 时 DomBindings R2 三连不应膨胀 binary（lazy-attach 容错保证）|
| 7 | reflection 沉淀回流模式 | ✅ 各子任务 reflection 阶段更新 archive §6 estimating 校准范围 |

**协同度自评：** 7/7（100% — 4 ✅ 直接命中 + 3 引用 / 0 新模式入库）

---

## 11. 验收标准（本 creative 文档级）

- [x] 设计挑战清晰（§1 问题陈述 + 5 项约束 + 4 项成功标准）
- [x] ≥2-3 个方案探索（§2 / 3 方案 A/B/C 全面对比）
- [x] 方案对比表（§2.x / 9 维度对比）
- [x] 推荐方案 + 论证（§3 / C 按类型分组 + 5 项理由 + 推荐立项顺序 + 4 项风险缓解）
- [x] §d.1 / §d.2 audit（§4 + §5 / 标 N/A 已 audit / 标注子任务自己做）
- [x] 实现指导（§6 / 4 gap 详细 + DomBindings R2 三连 API + 路径决策表 + smoke test 矩阵）
- [x] dogfood 工作流完整链路图（§7）
- [x] hello_mvp_b.cc example 占位（§8）
- [x] 安全考量（§9 / 沿用 MVP-A 5+T + 新威胁面评估 3 项 / 全 mitigation）
- [x] systemPatterns 协同度自检 ≥ 7 模式（§10 / 7/7 100%）

---

**文档结束。** 本 creative 由 [TASK-20260504-01 plan §4.3 子任务 3](../../docs/plans/2026-05-04-mvp-scope.md) 直接产出。下一步：commit 3 落盘 + 进入 creative-mvp-C.md。
