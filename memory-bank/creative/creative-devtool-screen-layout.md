# 创意设计：DevTool UI 主屏布局协议（splitter dock + HUD overlay 混合模式）

**日期：** 2026-04-30
**状态：** /plan 头脑风暴产出，待用户审查
**关联任务：** TASK-20260430-04
**关联 spec：** `docs/specs/2026-04-30-devtool-design.md` §3 验收 A1-A2 / §4 D3 / §5 §6.A

## 设计挑战

DevTool 三件套（Inspector + Performance Overlay + Hot Reload）在 V3=A 自渲染 + V4=Level 4 + D3=B 同窗口布局 + D4=B 单进程共享容器约束下，UI 区域怎么排版？关键挑战：
1. **空间分配**：DevTool UI 与目标 View 共享一个 SDL2 window — 谁占多少空间，怎么分？
2. **模式切换**：用户希望 DevTool 关闭/重开（F12 toggle）时目标 View 立即恢复全屏，反之 DevTool 打开时目标 View 让出区域
3. **Overlay 子系统的特殊性**：Performance Overlay 是 HUD 风格（半透明覆盖目标 View），与 Inspector / Hot Reload 的 splitter dock 风格不同 — 怎么混合？
4. **dirty rect 边框高亮的合成顺序**：Overlay 需要在最末渲染（覆盖 target paint），但又不能被 Inspector overlay 覆盖（hover 高亮也是末位渲染） — 顺序怎么定？
5. **dogfood 路径下的复杂度**：splitter / dock 切换 / HUD 透明合成都要靠 Veloxa HTML/CSS 自渲染实现 — 哪些 CSS 子集已 ready？哪些缺失需扩展？

### 约束

- 嵌入式 / 单显示器友好（`hello_sdl2` 默认 800×600 窗口大小）
- DevTool OFF 时目标 View 全屏（A14 binary size 不变 + 视觉上 splitter 完全消失）
- splitter 拖拽不破坏目标 View 的事件冒泡（hover / focus 不能因 splitter 拖拽而丢失）
- HUD overlay 半透明合成不影响目标 View paint cost（Overlay 关闭时零开销）
- 现有 Veloxa CSS 子集已支持：flexbox / position absolute / overflow / opacity（grep 验证 ✅）；尚未支持：CSS grid / pointer-events: none-passthrough（仅原生层处理）

---

## 决策 1：DevTool UI 整体布局结构 — splitter dock + HUD overlay 双层结构

### 候选方案

**方案 A：纯 splitter dock**（DevTool UI 全部位于 splitter 区域，无 HUD）
- Performance Overlay 也放进 splitter 区域作为面板
- 优：模型最简单，单一 splitter
- 劣：违背 Performance Overlay 的 HUD 语义（FPS 数字 / dirty rect 红框需要 *叠加* 在目标 View 上才有意义）

**方案 B：纯 HUD overlay**（DevTool UI 全部 HUD 风格半透明覆盖目标 View）
- 优：目标 View 不让出区域
- 劣：Inspector DOM tree / Style panel 信息密度高，半透明背景不利阅读

**方案 C：双层结构 — splitter dock + HUD overlay 混合** ⭐
- splitter dock 区域承载：Inspector（DOM tree / Style panel / Layout panel）+ Hot Reload 状态指示器
- HUD overlay 区域承载：Performance Overlay（FPS 数字 + 4 阶段 bars + dirty rect 边框）
- HUD overlay 直接覆盖在目标 View 区域（不进 splitter dock）
- 两层独立切换：Inspector / Hot Reload F12 toggle splitter；Overlay F11 toggle HUD

**方案 D：可拖拽自由窗口**（每个 DevTool 子系统独立可拖拽窗口）
- 优：最灵活
- 劣：嵌入式 / 单显示器不友好；多窗口同步成本

### 选定方案：C 双层结构

```
┌─────────────────────────────────────────────────────────────────────┐
│ SDL2 Window（800×600 默认）                                          │
│ ┌─────────────────────────────────┬─────────────────────────────┐   │
│ │ Target View 区域（左 ~530×600）  │ DevTool splitter dock        │   │
│ │                                 │ (右 ~270×600，dock-to-right) │   │
│ │  ┌───────────────────────────┐  │ ┌─────────────────────────┐ │   │
│ │  │ HUD overlay（始终在目标   │  │ │ [Inspector] [Hot Reload]│ │   │
│ │  │ View 之上半透明）         │  │ │   tab 切换               │ │   │
│ │  │                           │  │ ├─────────────────────────┤ │   │
│ │  │ FPS: 60   Style: 0.5ms    │  │ │ DOM tree:                │ │   │
│ │  │ Layout: 1.2ms Render:0.8  │  │ │   ▼ <html>              │ │   │
│ │  │                           │  │ │     ▼ <body>            │ │   │
│ │  │ ━━━ dirty rect 红框 ━━━   │  │ │       ▶ <div#header>    │ │   │
│ │  │ │   target paint        │ │  │ │       ▼ <div#main>      │ │   │
│ │  │ │   (button hover red)  │ │  │ │         <p>Hello</p>    │ │   │
│ │  │ ━━━━━━━━━━━━━━━━━━━━━━ │  │ ├─────────────────────────┤ │   │
│ │  └───────────────────────────┘  │ │ Style:                   │ │   │
│ │                                 │ │   color: red             │ │   │
│ │  ← Inspector hover 红框         │ │   font-size: 16px        │ │   │
│ │  ↓ DisplayList overlay (target  │ ├─────────────────────────┤ │   │
│ │    paint 之后渲染)              │ │ Layout:                  │ │   │
│ │                                 │ │   x: 100 y: 50           │ │   │
│ │                                 │ │   w: 200 h: 30           │ │   │
│ │                                 │ │   padding: 4 4 4 4       │ │   │
│ │                                 │ │   border: 1 1 1 1        │ │   │
│ │                                 │ └─────────────────────────┘ │   │
│ │                                 │ Hot Reload: ✓ watching      │   │
│ │                                 │   /path/to/css (3 files)    │   │
│ └─────────────────────────────────┴─────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
```

### 理由

- splitter 适合**结构化高密度信息**（DOM tree / Style / Layout）— 用户长时间阅读
- HUD 适合**实时连续数字 + 视觉信号**（FPS / dirty rect）— 用户瞥一眼即可
- 两种场景在认知负担和视觉聚焦上是不同模态，分层处理是正确的
- F12 / F11 双键独立切换让用户灵活控制（关 Inspector 看 HUD only / 关 HUD 看 Inspector only / 双关 = 全屏 target View）

---

## 决策 2：splitter dock 模式与切换协议

### 候选方案

**方案 A：固定右侧 dock（不支持位置切换）**
- 优：实施最简单；模型固定
- 劣：用户预期可切换 dock-to-bottom（Chrome DevTools 惯例）

**方案 B：右侧 / 底部双 dock 模式（用户可切换）**
- 优：与 Chrome DevTools 惯例一致；适配横向屏 / 竖向屏不同需求
- 劣：splitter 组件需支持双轴
- 复杂度：中

**方案 C：右侧 / 底部 / 左侧 / 顶部 / 浮动 五种模式** ⭐ 推荐 B（一期）+ C 列扩展
- 一期 B 双 dock 模式
- 浮动 / 左侧 / 顶部留 P2 候选（不在 V1=B 三件套主线）

### 选定方案：B（一期）

splitter 组件支持两种 dock 位置：
- **dock-to-right**（默认）：splitter 占右侧，宽度比例 = 270/800 = 33.75%（用户可拖拽 200..400 px）
- **dock-to-bottom**：splitter 占底部，高度比例 = 200/600 = 33.33%（用户可拖拽 150..300 px）

切换协议：
- 用户点击 splitter 顶栏的 dock-toggle 按钮（图标 ⌐ vs ⌙）
- 切换瞬间：保存当前 splitter 比例 → 切换 dock 方向 → 应用新方向的最近比例（首次切换用默认值）
- 切换后立即 trigger Document re-layout（splitter 是 DevTool Document 的根 div，CSS flex-direction 切换从 row → column）

### CSS 实现（dogfood 验证 ✓）

```css
/* DevTool Document 根元素 */
#devtool-root {
  display: flex;
  flex-direction: row;          /* dock-to-right; column = dock-to-bottom */
  width: 100%;
  height: 100%;
}

#devtool-target-area {
  flex: 1 1 auto;                /* target View 区域占剩余 */
}

#devtool-panel {
  flex: 0 0 270px;               /* splitter 固定宽度 */
  border-left: 1px solid #444;
}

/* dock-to-bottom 时 JS 改 flex-direction + 改 flex-basis */
```

CSS 子集 grep 验证：
- `display: flex` / `flex-direction` ✅（grep `kFlex` veloxa/core/css/）
- `flex: 1 1 auto` 三参 shorthand ✅
- `flex-basis: 270px` ✅
- `border-left` ✅（TASK-20260430-02 已落地）

### Splitter 拖拽事件协议

```js
// devtool/splitter.js（DevTool Document 内的脚本，通过 DomBindings）
let splitter = document.getElementById("devtool-splitter-handle");
let panel = document.getElementById("devtool-panel");
let dragging = false;
let dock_mode = "right";  // or "bottom"

splitter.addEventListener("pointerdown", (e) => {
  dragging = true;
  e.preventDefault();
});

window.addEventListener("pointermove", (e) => {
  if (!dragging) return;
  if (dock_mode === "right") {
    let new_w = window.innerWidth - e.clientX;
    panel.style.flexBasis = clamp(new_w, 200, 400) + "px";
  } else {
    let new_h = window.innerHeight - e.clientY;
    panel.style.flexBasis = clamp(new_h, 150, 300) + "px";
  }
});

window.addEventListener("pointerup", () => { dragging = false; });
```

### 拖拽事件不影响目标 View 冒泡的隔离

splitter 拖拽 handle 是 DevTool Document 内的一个 `<div>`，hit test 由 EventManager 的 `splitter_.RouteToDocument(x, y)` 决定。事件归属 DevTool Document 时，目标 View 的 EventManager 完全不参与（事件不进 target_document_.event_manager_）。这避免了拖拽过程中 target hover / focus 状态被意外破坏。

---

## 决策 3：HUD overlay 透明合成

### 挑战

HUD overlay 半透明（如 background: rgba(0,0,0,0.7)）覆盖在目标 View 上 — 既要让 HUD 内容（FPS 数字 / bars）清晰可读，又要不遮挡过多目标 View 内容。

### 候选方案

**方案 A：HUD 是独立 Document 渲染到独立 surface 然后 alpha blend**
- 优：完全独立；HUD bug 不影响目标
- 劣：双 surface 合成成本（CPU 端 alpha blending 是 ~6-10% 帧时长开销）

**方案 B：HUD 是 DevTool Document 内的 absolute positioned 元素，CSS opacity 实现半透明** ⭐
- HUD `<div>` 在 DevTool Document 内 `position: fixed; top: 8px; left: 8px; opacity: 0.85`
- 优：复用现有渲染管线；零额外 surface
- 劣：HUD 与 splitter dock 在同一 Document → 视觉上 HUD 也仅出现在 target 区域（splitter 内不出现）— 但这本来就是预期行为

**方案 C：HUD 是 target Document 的 DisplayList overlay 注入**
- 优：与 Inspector hover overlay 注入路径同构
- 劣：HUD UI 是动态文本（FPS 数字每帧变），用 DisplayList 注入需手写 paint 命令而非 CSS（违背 V3=A dogfood）

### 选定方案：B（DevTool Document 内 absolute positioned + opacity）

```css
/* DevTool Document 内 */
#devtool-hud {
  position: fixed;              /* 相对 DevTool Document 视口 */
  top: 8px;
  left: 8px;
  width: 180px;
  pointer-events: none;          /* HUD 不接收鼠标事件，穿透到 target */
  opacity: 0.85;
  background: #1a1a1a;
  color: #00ff00;
  padding: 6px 8px;
  border-radius: 4px;
  font-family: monospace;
  font-size: 11px;
  z-index: 9999;
}
```

CSS 子集 grep 验证：
- `position: fixed` ✅
- `opacity` ✅
- `pointer-events: none` ❓ — 待 grep；若未实现，HUD 会拦截鼠标事件（mitigation：HUD 区域很小 ~180×60 px，影响有限）
- `border-radius` ✅
- `z-index` ✅

`pointer-events: none` 缺失的兜底：在 EventManager 的 HitTest 中，DevTool Document 的元素如果带 `data-passthrough="1"` attribute 则跳过 hit。这是 DevTool 内部约定，不需要 CSS 标准支持。

### HUD 内容布局结构

```html
<div id="devtool-hud">
  <div class="hud-row hud-fps">
    <span class="hud-label">FPS:</span>
    <span class="hud-value" id="hud-fps">60</span>
  </div>
  <div class="hud-row hud-stages">
    <div class="hud-bar"><span>S</span><div class="bar-fill" id="bar-style"></div></div>
    <div class="hud-bar"><span>L</span><div class="bar-fill" id="bar-layout"></div></div>
    <div class="hud-bar"><span>R</span><div class="bar-fill" id="bar-render"></div></div>
    <div class="hud-bar"><span>P</span><div class="bar-fill" id="bar-paint"></div></div>
  </div>
</div>
```

JS 每帧从 `vx_view_get_perf_stats(target_view, &stats)` 读取 FrameStats → 更新 DOM。

---

## 决策 4：dirty rect 边框高亮 + Inspector hover 红框的渲染顺序

### 挑战

两种 overlay 都需要在 target paint 之后渲染：
- **dirty rect 边框**（Performance Overlay）：每帧标记 target Document 的 dirty rect 区域（红色矩形边框 1px）
- **Inspector hover 红框**（DisplayList overlay）：hover 选中节点的 LayoutBox border-box 红色矩形（红色边框 2px）

如果两者顺序错了：dirty rect 边框可能被 hover 红框覆盖（看不到 dirty 区域），或 hover 红框被 dirty 边框覆盖（看不出 hover 准确位置）。

### 候选方案

**方案 A：dirty rect 在 hover 红框之前**（hover 红框在最末位）
- 优：hover 是用户主动操作，应优先可见
- 劣：dirty rect 边框与 hover 红框重叠时，dirty 部分被覆盖

**方案 B：hover 红框在 dirty rect 之前**（dirty rect 在最末位）
- 优：dirty rect 是诊断信号（重绘热区），全可见有助 perf 调优
- 劣：hover 红框可能被 dirty 边框遮挡

**方案 C：双线宽 + 不同颜色避免视觉冲突** ⭐
- dirty rect = 1px 红色边框 +  虚线（dashed）
- hover 红框 = 2px 红色边框 + 实线（solid）
- 渲染顺序：dirty rect 先 → hover 红框后（hover 在最末）
- 视觉上：用户能同时看到两层（虚线 vs 实线区分）；hover 红框稍粗优先可见

### 选定方案：C（双线宽 + 不同颜色）

具体规格：

| Overlay 类型 | PaintCommand tag | 顺序 | 边框 |
|---|---|:-:|---|
| **dirty rect** | `kOverlayDirtyRect` | 先（target paint 之后第 1 层）| 1px solid `rgba(255, 100, 100, 0.6)` |
| **Inspector hover red** | `kOverlayHoverHighlight` | 后（最末层）| 2px solid `rgba(255, 0, 0, 0.9)` |
| **Inspector layout box overlay** | `kOverlayLayoutBox` | 中间 | 1px dashed `rgba(0, 255, 255, 0.7)`（cyan，区分 hover 红框）|

### 渲染管线 hook 落点

```cpp
// veloxa/core/render/renderer.cc 末尾
void Renderer::Render(Document& target_doc, Document* devtool_doc, Canvas& canvas) {
  // 1. 渲染 target Document 全部 commands
  RenderDocument(target_doc, canvas);

  // 2. 渲染 target Document 的 overlay commands（按顺序）
  RenderOverlayCommandsByTag(target_doc, canvas, kOverlayDirtyRect);
  RenderOverlayCommandsByTag(target_doc, canvas, kOverlayLayoutBox);
  RenderOverlayCommandsByTag(target_doc, canvas, kOverlayHoverHighlight);

  // 3. 渲染 DevTool Document（如果存在）— 自渲染
  if (devtool_doc) {
    RenderDocument(*devtool_doc, canvas);
  }

  // 4. ResetOverlayCommands（每帧开头复位 = 当前帧末尾清除）
  target_doc.ClearOverlayCommands();
}
```

### 反模式 — 为什么不直接给 DevTool Document 加 overlay div

DevTool Document 的元素（包括 hover 红框）经过完整 layout pipeline，需要计算 LayoutBox / paint 命令。如果 hover 红框是 DevTool Document 内的 `<div>`，每次 hover 移动都会触发 DevTool Document restyle + relayout（即使只改 left/top）。

直接 PaintCommand overlay 注入避免了这个开销：每次 hover 只追加一个 `kOverlayHoverHighlight` PaintCommand，不进 layout pipeline。这是 D2=B「半结构化」决策的核心 ROI 体现。

---

## 决策 5：F12 / F11 toggle 协议 + DevTool 关闭时性能影响

### F12 toggle Inspector / Hot Reload splitter

```cpp
// veloxa/devtool/application_devtool.cc
void Application::ToggleDevToolPanel() {
  if (devtool_document_ == nullptr) {
    // 首次启用：创建 DevTool Document + load HTML/CSS/JS
    devtool_document_ = LoadDevtoolDocument();
    // splitter 默认 dock-to-right，target View 让出 270px
  } else {
    // 关闭：销毁 DevTool Document
    DestroyDevtoolDocument();
    devtool_document_ = nullptr;
    // target View 自动占满整窗（splitter 消失）
  }
  Invalidate();  // trigger 全量重绘
}
```

### F11 toggle HUD overlay

HUD 是 DevTool Document 内的元素，不能在 DevTool 关闭时单独保留 — 所以 F11 仅在 DevTool 已开启时有效：

```js
// devtool/inspector_panel.js
window.addEventListener("keydown", (e) => {
  if (e.key === "F11") {
    let hud = document.getElementById("devtool-hud");
    hud.style.display = hud.style.display === "none" ? "block" : "none";
  }
});
```

### DevTool 关闭时性能影响验证（A14）

- DevTool Document = nullptr 时：
  - Application::Update 跳过 devtool_document_ 的 Update
  - UpdateManager::Update 钩子 callback = nullptr 时分支可被 branch predictor 优化
  - target Document 的 dirty_paint_commands_ 不再受 overlay 注入污染
- binary size：
  - DevTool 模块通过 `VX_BUILD_DEVTOOL=OFF` 编译时去除（`vx_devtool` 静态库不链接）
  - DevTool ON 但运行时未启用（devtool_document_ = nullptr）：仅多 ~50 KB DevTool 代码 + ~5 KB DevTool HTML/CSS/JS resource

---

## 总体决策汇总

| 决策 | 锁定值 |
|---|---|
| 1. 整体布局结构 | C 双层结构 — splitter dock + HUD overlay 混合 |
| 2. splitter dock 模式 | B 一期双 dock（right + bottom），其他模式 P2 候选 |
| 3. HUD 透明合成 | B DevTool Document 内 absolute positioned + opacity（无独立 surface） |
| 4. overlay 渲染顺序 | C 双线宽 + 不同颜色（dirty rect 1px solid → layout box 1px dashed cyan → hover 2px solid red 在最末） |
| 5. F12/F11 toggle 协议 | F12 toggle splitter；F11 toggle HUD（仅 DevTool 已开启时） |

## 与 spec 的交叉引用

- **§4 D3=B**：本 creative 详化了 D3 决策的具体实施模型
- **§5.1 模块边界**：splitter / HUD overlay / DisplayList overlay 三层结构本 creative 落地
- **§5.2 注入点 I3 DisplayList overlay**：本 creative 决策 4 给出了 PaintCommand tag 优先级与渲染顺序
- **§7 T5 DisplayList overlay 注入污染**：本 creative 决策 4 的 ResetOverlayCommands 是 T5 mitigation 落地

## 不在本 creative 范围

- HUD 内容数据采集协议（FrameStats 字段定义 + 滑动窗口算法）→ 见 spec §5.1 perf_overlay 模块边界 + plan Phase B
- splitter dock-to-right 与 dock-to-bottom 之外的位置（左 / 顶 / 浮动）→ spec §11 扩展段
- Hot Reload 状态指示器 UI 详细 → 一期仅简单文本提示（"watching: 3 files"），不作单独 creative

---

**待用户审查重点：**
1. 决策 1 的双层结构（splitter + HUD）是否符合用户对 DevTool 的认知预期？
2. 决策 4 的渲染顺序（dirty rect → layout box → hover）是否避免视觉冲突？
3. 决策 3 中 `pointer-events: none` 的兜底（`data-passthrough="1"` attribute）是否可接受作为 V1=B 三件套一期方案？
