# 创意设计：DevTool Phase D · Console JS REPL（4 维度详细设计）

**日期：** 2026-05-04
**状态：** /creative 阶段产出，等待用户审查
**关联任务：** TASK-20260503-04（搁置任务恢复 / V1=B 扩展段）
**关联 spec：** `docs/specs/2026-04-30-devtool-design.md` §11.1 Console（JS REPL + console.log 桥接）
**关联 plan：** `docs/plans/2026-05-04-devtool-console-repl.md`
**关联 creative（兼容性自查）：** `creative-devtool-screen-layout.md` 决策 1+2 / `creative-quickjs-host.md` §组件 1 方案 C Phase 2 + 组件 2 方案 B + 组件 3 方案 B

## 概要

本文档针对 4 维度详细设计：

| # | 维度 | brainstorm 锁定方向 | creative 阶段范围 |
|:-:|---|---|---|
| C1 | DevTool 主屏第 4 件套布局 | A 第 4 tab 平级 | tab 切换协议 / dock-to-bottom 视觉 / Enter keydown 解决 / input element 子集对应 |
| C2 | isolated JSRuntime 实施 | B 新建 console_script_engine_ | lifecycle 时序图 / DEVTOOL=OFF 路径 / 与 creative-quickjs-host 兼容性 |
| C3 | capability allowlist | A 严格隔离 binding | RegisterConsoleBindings 全量 API 表 / 反向探针单测设计 / spec §11.1 安全证明 |
| C4 | console.log 桥接 | A drain 模式 | drain JSON envelope 完整规范 / 双上限 mitigation 算法 / utf8 边界截断 |

**CREATIVE 命令 §d.1 / §d.2 触发评估：**

- §d.1（坐标约定一图）：本任务 4 维度均无 ≥ 2 坐标系/方向算法 → **不触发**
- §d.2（chain/accumulator 必须 explicit method）：本任务无累积量赋值歧义；buffer 操作走 std::deque 标准 push_back/pop_front 已 explicit → **不触发**

---

## C1 设计：DevTool 主屏第 4 件套布局

### 设计挑战

**问题陈述：** 在既有 splitter dock 区域（DOM/Style/Layout 3 tab + HUD overlay）中，加入 Console 第 4 件套 — 同时满足：
1. 不破坏既有 3 tab 切换协议（`inspector_panel.js` `setActiveTab(name)`）
2. 不破坏既有 HUD overlay 行为（HR status / FPS 显示）
3. 兼容既有 splitter dock-to-right + dock-to-bottom 双模式（creative-devtool-screen-layout.md 决策 2）
4. REPL input 行为兼容 Veloxa 自渲染层 input element 子集（无浏览器原生 keyboard 输入支持）
5. Enter keydown 不与既有 F11/F12 hotkey 冲突 + 不与 splitter 拖拽冲突

**约束条件：**
- `tag.cc:67` `TagId::kInput, "input", TagType::kInlineBlock, ParseModel::kVoid` — input 已是合法元素但**无浏览器原生输入行为**（无 keyboard 自动捕获 / 无 value attribute 自动同步 / 无 caret 显示）
- `inspector_panel.html` 当前 4 区域：3 tab buttons + 3 panel divs + 1 HUD（含 HR row）
- 既有 hotkey：F12 toggle DevTool attach/detach（`Application::SetDevtoolHotkey`）；F11 toggle HUD visibility
- splitter dock 模式切换：creative-screen-layout 决策 2 锁定 dock-to-right + dock-to-bottom，由 splitter component 控制（一期未完整实施 — 留 P3）

**成功标准：**
- 4 tab 切换协议平滑（tab 1/2/3 不退化）
- Console panel 在 dock-to-right + dock-to-bottom 双模式下视觉合理
- REPL input 在 Veloxa 自渲染层可用（用户能输入 JS + 看到光标 + Enter 触发 eval）
- 0 keydown 冲突

### 探索的方案

#### 方案 A：第 4 tab 与既有 3 tab 平级 ⭐ 选定

**描述：** 在 `inspector_panel.html` 中扩展 `#devtool-tabs` 加 `<button data-tab="console">Console</button>` + `#devtool-content` 加 `<div id="console-panel"></div>`；console 内容（output 滚动 div + input 行）通过 `__INLINE_CONSOLE_HTML__` 占位符在 `Application::LoadDevtoolDocument` 拼接（与既有 `__INLINE_CSS__` / `__INLINE_JS__` 范式一致）。

```
┌────────────────────────────────────────────┐
│ [DOM] [Style] [Layout] [Console]   ← tabs │
├────────────────────────────────────────────┤
│ #console-panel (active when tab=console):  │
│   #console-output (scrollable)            │
│     >    1+1                              │
│     =    2                                │
│     log  hello                            │
│     ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━        │
│   #console-input-row:                     │
│     JS> [_______________________]          │
└────────────────────────────────────────────┘
```

**优点：**
- 复用现有 tab 切换协议 100%（`inspector_panel.js` `setActiveTab(name)` 自然加 1 case）
- splitter dock-to-right / dock-to-bottom 双模式天然兼容（4 tab 无差别）
- splitter dock-to-bottom 时，console tab 即变 Chrome DevTools 风格底部 console
- 资源嵌入范式 100% 一致（`console_panel.{html,css,js}` 加 3 extern + Python codegen + ReplaceFirst 拼接）
- A14 黑名单维护简单（`vx_devtool_console` 静态库 + 3 类符号 + Phase D 注释段）

**缺点：**
- 用户每次切到 console tab 才能看到 console.log 输出（vs Chrome 风格底部 console 始终可见）— mitigation：drain 在每帧执行，切换 tab 即可见所有历史日志（buffer 保留）
- splitter dock-to-right 时 console 与 inspector tabs 互斥可见（不能同时看 DOM tree + console）— mitigation：dock-to-bottom 模式即 Chrome 风格

**复杂度：** 低
**风险：** 低（纯 UI 扩展 / 0 新协议 / 0 新 hotkey）

#### 方案 B：splitter 内拆分上下区域（上 tabs + 下 fixed console pane）

**描述：** `inspector_panel.html` 引入第二级 splitter（双 splitter handle），上半为既有 3 tab，下半为始终可见的 console pane。

**优点：**
- Chrome DevTools 风格（同时可见 DOM tree + console）
- 用户体验更接近浏览器开发工具

**缺点：**
- 需新建 splitter 协议（双 splitter handle 拖拽 + 上下区域可调比例）— creative-screen-layout 决策 2 splitter 一期仅实现 dock-to-right/bottom，没有内部细分
- inspector_panel.css 大量改造 + JS splitter 拖拽事件
- 与既有 HUD overlay z-index 复杂度上升
- 复杂度高 / 工时翻倍

**复杂度：** 中-高
**风险：** 中（splitter 协议改造可能影响既有 dock 切换）

#### 方案 C：独立 toggle 键 F10（底部 dock console，独立于 splitter）

**描述：** Console 是独立 panel（不在 splitter 内），由 F10 hotkey 切换显隐，固定在窗口底部 200px 高度。

**优点：**
- 与 Chrome DevTools 完全一致
- 与既有 splitter / HUD 完全独立

**缺点：**
- 引入第 3 个 hotkey（F12/F11/F10）— 与 spec §11.1 Console 占位「依赖 D2=B JSON 协议 + D7=C 双层 API」一致性差
- 占用 target View 区域（与 dock-to-right 模式叠加时空间挤压）
- 需新建独立 panel 渲染管线 / 第二个 DevTool Document？
- 复杂度最高

**复杂度：** 高
**风险：** 高（架构层引入新概念）

### 方案对比

| 维度 | 方案 A | 方案 B | 方案 C |
|---|---|---|---|
| 复杂度 | 低 | 中-高 | 高 |
| 可维护性 | 高（复用既有协议） | 中（双 splitter 协议） | 中（独立 panel 管线） |
| 性能 | 优（与既有同管线） | 中（双 splitter 重布局） | 中（独立 paint） |
| 扩展性 | 高（未来加 Network/Sources tab 自然扩展） | 中（双 splitter 难再分） | 低（hotkey 命名空间狭窄） |
| 用户体验（开发者熟悉） | 中（需切 tab 看 console） | 高（同时可见） | 高（Chrome 一致） |
| 风险 | 低 | 中 | 高 |

### 推荐并论证

**选定方案：A — 第 4 tab 与既有 3 tab 平级。**

**理由：**
- 与既有 inspector_panel UI 协议 100% 一致 → 用户认知负担最小
- splitter dock-to-bottom 模式（creative-screen-layout 决策 2 已锁定）天然提供 Chrome 风格底部 console 体验
- 资源嵌入 / Python codegen / A14 黑名单 4 大范式 100% 复用
- 0 新 hotkey + 0 新协议 + 0 splitter 改造
- 一期最小复杂度交付，方案 B/C 留 P3 候选

**风险缓解：**
- 「console.log 切 tab 才可见」mitigation：drain 在每帧执行，切到 console tab 即可见全部历史日志（buffer 保留 1000 条，足够开发者回溯）
- 一期未实施 splitter dock-to-bottom 切换 mitigation：留 P3 候选（不影响 console 在 dock-to-right 默认模式下可用）

### 实现指导

#### inspector_panel.html 第 4 tab 集成

```html
<!-- existing: -->
<div id="devtool-tabs">
  <button data-tab="dom" class="active">DOM</button>
  <button data-tab="style">Style</button>
  <button data-tab="layout">Layout</button>
  <button data-tab="console">Console</button>  <!-- NEW -->
</div>
<div id="devtool-content">
  <div id="dom-tree-panel" class="active"></div>
  <div id="style-panel"></div>
  <div id="layout-panel"></div>
  <div id="console-panel">__INLINE_CONSOLE_HTML__</div>  <!-- NEW -->
</div>
```

#### console_panel.html（被 ReplaceFirst 替换占位符）

```html
<style>__INLINE_CONSOLE_CSS__</style>
<div id="console-output"></div>
<div id="console-input-row">
  <span id="console-prompt">JS&gt;</span>
  <span id="console-input-display"></span>
  <span id="console-caret">_</span>
</div>
<script>__INLINE_CONSOLE_JS__</script>
```

**注意：** 因 Veloxa 自渲染层 input element **无浏览器原生输入行为**（grep 实证 `tag.cc:67` 是 inline-block + void element，无 keyboard / value 自管理），**不使用 `<input>`**。改用 `<span>` + JS 自管理 input value state（`#console-input-display` 显示当前输入字符 + `#console-caret` 显示闪烁光标 = `_`）。

#### console_panel.js REPL 行为（关键代码片段）

```js
// console_panel.js — Veloxa Console panel REPL behavior
(function() {
  var input_value = "";        // self-managed input state
  var output = document.getElementById("console-output");
  var input_display = document.getElementById("console-input-display");
  var caret = document.getElementById("console-caret");

  function isConsoleActive() {
    var btn = document.querySelector("#devtool-tabs button[data-tab='console']");
    return btn && btn.className && btn.className.indexOf("active") >= 0;
  }

  function appendLine(level, text) {
    var div = document.createElement("div");
    div.className = "console-line console-" + level;
    div.textContent = text;
    output.appendChild(div);
    // auto-scroll to bottom (best-effort: relies on overflow:scroll CSS)
    output.scrollTop = output.scrollHeight;
  }

  function evalCurrent() {
    var src = input_value;
    if (src.length === 0) return;
    appendLine("input", "JS> " + src);
    input_value = "";
    input_display.textContent = "";
    if (typeof vx_console_eval === "function") {
      var result = vx_console_eval(src);
      appendLine("result", "= " + result);
    }
  }

  // Self-managed keydown handler. Only active when console tab is selected.
  // Avoids conflict with F11/F12 hotkeys (handled by host before reaching JS).
  window.addEventListener("keydown", function(e) {
    if (!isConsoleActive()) return;
    if (e.key === "Enter") {
      evalCurrent();
      e.preventDefault();
    } else if (e.key === "Backspace") {
      input_value = input_value.slice(0, -1);
      input_display.textContent = input_value;
      e.preventDefault();
    } else if (e.key && e.key.length === 1) {
      // Printable single-char key (letters/digits/punctuation/space).
      input_value += e.key;
      input_display.textContent = input_value;
      e.preventDefault();
    }
    // Other keys (arrows, F-keys, Ctrl/Alt/Meta combos) — let host handle.
  });

  // Drain console.log buffer every 200ms (5 fps) — cheap polling.
  // The host C ABI's vx_devtool_get_console_log_drain returns the JSON
  // envelope and atomically clears the buffer.
  function drainConsoleLog() {
    if (typeof vx_devtool_get_console_log_drain !== "function") return;
    var raw = vx_devtool_get_console_log_drain();
    var env;
    try { env = JSON.parse(raw); } catch (_) { return; }
    if (!env || !env.messages) return;
    for (var i = 0; i < env.messages.length; i++) {
      var m = env.messages[i];
      appendLine(m.level, m.text);
    }
    if (env.dropped_count > 0) {
      appendLine("warn", "[dropped " + env.dropped_count + " older messages]");
    }
  }
  setInterval(drainConsoleLog, 200);
})();
```

#### console_panel.css

```css
#console-output {
  width: 100%;
  height: 280px;            /* fixed; splitter dock height controls visible area */
  overflow: scroll;
  background: #1a1a1a;
  color: #d0d0d0;
  font-family: monospace;
  font-size: 12px;
  padding: 4px 6px;
}

.console-line {
  white-space: pre-wrap;     /* preserve user's spacing */
  word-break: break-all;
  line-height: 1.4;
}
.console-input  { color: #ffeb3b; }   /* yellow — user input */
.console-result { color: #a0d0ff; }   /* light blue — eval result */
.console-log    { color: #d0d0d0; }   /* default gray */
.console-warn   { color: #ffaa00; }   /* orange */
.console-error  { color: #ff6060; }   /* red */

#console-input-row {
  display: flex;
  background: #0a0a0a;
  color: #00ff00;
  font-family: monospace;
  font-size: 12px;
  padding: 4px 6px;
  border-top: 1px solid #444;
}

#console-prompt          { margin-right: 4px; }
#console-input-display   { flex: 1 1 auto; }
#console-caret {
  /* simple visual marker; no animation since CSS animation set may be limited */
  color: #00ff00;
}
```

#### Enter keydown 冲突分析

| 已有 keydown 监听 | 路径 | 与 Console Enter 冲突 |
|---|---|:-:|
| F11 | `Application::MaybeHandleDevtoolHotkey` 拦截 → 不进 JS | ❌ |
| F12 | `Application::MaybeHandleDevtoolHotkey` 拦截 → 不进 JS | ❌ |
| inspector_panel.js 现有 `keydown` listener | grep 实证仅 F11 toggle HUD | ❌（与 console.js 通过 `isConsoleActive()` 互斥） |
| **Enter** | host 不拦截 → 进入 DevTool Document JS | ✅ console.js handle 仅当 `isConsoleActive()` 为真 |

→ **0 冲突**。

---

## C2 设计：isolated JSRuntime 实施

### 设计挑战

**问题陈述：** spec §11.1 要求「eval 在 isolated JSRuntime」。Phase 0 grep 实证：
- `Application::devtool_script_engine_` 已是独立 `QuickjsEngine` 实例（`application.cc:322`）— 与 target Document 的 JS 上下文隔离 ✅
- 但 Console 与 DevTool UI 自身 JS（`inspector_panel.js`）共享同一 runtime 时，Console eval 会污染 inspector_panel 全局变量空间

**需要决策：** Console 是否再隔离一层（第 3 个 QuickjsEngine 实例）？

**约束条件：**
- `creative-quickjs-host.md` §组件 1 方案 C Phase 2 已闭环（TASK-05 commit `63a0bab`）— `QuickjsEngine::SetEvalInterruptBudget` + `WasInterrupted` API 全就位
- `creative-quickjs-host.md` §组件 2 方案 B — `QuickjsEngine::Init` 不调用 `js_std_add_helpers`（默认能力面最小）
- `creative-quickjs-host.md` §组件 3 方案 B — `JS_SetMemoryLimit ≈ 32 MiB` 每 `JSRuntime`（自动继承）
- `Application::LoadDevtoolDocument` 已有 lifecycle 顺序：`devtool_script_engine_ → devtool_dom_bindings_->Bind → SetDevtoolTargetDocument → SetRedactionPolicy → SetHotReloadManager → RegisterDevtoolBindings → EvalGlobal(inspector_panel.js)`
- `UnloadDevtoolDocument` 反序释放：`devtool_dom_bindings_.reset() → ...`

**成功标准：**
- Console eval 不污染 `inspector_panel.js` 全局变量
- Console 死循环不阻塞 DevTool UI（独立 runtime 独立 InterruptHandler）
- 双 engine lifecycle 时序无 dangling JSContext callback
- DEVTOOL=OFF 路径 link 不退化（`unique_ptr<ConsoleEngine>` 在 application.h 中需 #ifdef 包围 — 沿用 Phase C R4 范式）
- 内存上限 32 MiB（自动继承 creative-quickjs-host §组件 3）

### 探索的方案

#### 方案 A：复用既有 `devtool_script_engine_`

**描述：** Console eval 直接调 `Application::EvalDevtoolScript(source, "console")`（既有 A.1.8 API）。

**优点：**
- 0 新 runtime / 0 新 lifecycle 管理 / 0 新 unique_ptr
- 直接复用 SetEvalInterruptBudget(10000)（在 EvalDevtoolScript 入口设置）
- 实施工时最少

**缺点：**
- Console eval 与 inspector_panel.js 共享全局变量空间：
  - 用户输入 `var output = ...` 会覆盖 inspector_panel.js 的 `output` 变量（破坏 panel JS）
  - inspector_panel.js 内部状态（如 `dragging`、`splitter` 等）暴露给 Console 用户
- 与 spec §11.1「isolated JSRuntime」字面要求不符（虽然 spec 原意更偏「与 target 隔离」，但严格隔离更稳健）

**复杂度：** 低
**风险：** 中（用户体验差 / 调试时 Console 影响 panel）

#### 方案 B：新建 `console_script_engine_`（第 3 个 QuickjsEngine 实例） ⭐ 选定

**描述：** Application 持 `unique_ptr<vx::devtool::console::ConsoleEngine> console_script_engine_`，与 `devtool_script_engine_` 平级，由 `LoadDevtoolDocument` 同步创建（在 `devtool_script_engine_` 之后），由 `UnloadDevtoolDocument` 反序释放（在 `devtool_dom_bindings_` 之前）。

`ConsoleEngine` 是 `script::QuickjsEngine` 的薄封装类（`veloxa/devtool/console/console_engine.{h,cc}`），在 `Init()` 中**强制**应用默认 budget（`SetEvalInterruptBudget(kDefaultInterruptBudgetCheckpoints = 10000)`），实现 T1 mitigation 维度 1（默认安全）+ 维度 3（不可绕过 — 因为 budget 在 ConsoleEngine 类内持有，user JS source 无法访问）。

**优点：**
- 完全隔离 Console scope（与 inspector_panel.js 全局空间互不影响）
- 双 engine 各自独立 InterruptHandler（Console 死循环不阻塞 DevTool UI）
- 自动继承 `creative-quickjs-host` §组件 3 方案 B（`JS_SetMemoryLimit ≈ 32 MiB`）每 runtime
- T1 mitigation 5 维度完整（默认安全 / opt-in / 不可绕过 / 状态可查 / 回调约束）
- lifecycle 与既有 `devtool_script_engine_` 平行，时序清晰
- 与 spec §11.1「isolated JSRuntime」严格符合

**缺点：**
- +1 个 unique_ptr lifecycle 管理
- +~50 KB binary（额外 QuickJS context overhead — 单 context ~30-50 KB）
- application.h 需 `#ifdef VX_BUILD_DEVTOOL` 包围 unique_ptr 字段（沿用 Phase C R4 范式 — 不完整类型 dtor 问题）

**复杂度：** 中
**风险：** 低（5 大可复用范式 100% 命中 — Phase A/B/C 沉淀）

#### 方案 C：复用 `devtool_script_engine_` + JS-side `Function` constructor 沙箱

**描述：** Console eval 包裹用户输入到 `Function("'use strict'; return (" + source + ")")` 内执行 — 利用 Function constructor 创建独立 lexical scope。

**优点：**
- 0 新 runtime（部分缓解全局污染）
- 严格模式下 var 提升语义不污染 outer scope

**缺点：**
- `globalThis` 仍然共享（用户 `globalThis.output = ...` 仍能污染）
- `var` 在 Function 内是局部，但 `function foo()` 仍会提升到 Function scope
- 异步 callback 仍能逃逸到 outer scope
- 实施复杂度比方案 B 还高（需 source 转写 + 错误信息 mapping 处理 Function constructor 的 1-line wrapper）
- 不彻底的隔离 → 仍有 surprise

**复杂度：** 中
**风险：** 中（隔离不彻底 + source 转写边界 case 多）

### 方案对比

| 维度 | 方案 A 复用 | 方案 B 新建 ⭐ | 方案 C JS-side 沙箱 |
|---|---|---|---|
| 复杂度 | 低 | 中 | 中 |
| 可维护性 | 中 | 高（lifecycle 清晰） | 低（source 转写边界） |
| 隔离彻底性 | 差 | 优 | 中（globalThis 仍共享） |
| 内存开销 | 0 | +~50 KB（额外 context） | 0 |
| T1 mitigation | 中（共享 InterruptHandler） | 优（独立 InterruptHandler） | 中 |
| 与 spec §11.1 字面符合 | 部分 | 严格 | 部分 |
| 与 5 大范式协同 | 中 | 优（双层 API + #ifdef + lazy-attach） | 低 |
| 风险 | 中 | 低 | 中 |

### 推荐并论证

**选定方案：B — 新建 `console_script_engine_`（第 3 个 QuickjsEngine 实例）。**

**理由：**
- 隔离彻底性最高（Console eval 0 污染 inspector_panel.js scope）
- 与 spec §11.1「isolated JSRuntime」严格符合
- T1 mitigation 5 维度完整性最高（独立 InterruptHandler + 不可绕过 budget）
- 5 大可复用架构范式 100% 命中（双层 API + #ifdef + CMake conditional + lazy-attach C ABI + 资源嵌入）
- +~50 KB binary 是可接受成本（DevTool ON 才有 / 不影响 OFF binary）
- lifecycle 管理与既有 `devtool_script_engine_` 平行，时序清晰（构造/析构次序明确）

**风险缓解：**
- application.h `#ifdef VX_BUILD_DEVTOOL` 包围 `unique_ptr<ConsoleEngine>` 字段 + getter（**沿用 Phase C R4 范式 / TASK-20260503-01 archive 沉淀**）
- 双 engine 时序图（见下文实现指导）确保析构顺序无 dangling

### 实现指导

#### lifecycle 时序图

```
LoadDevtoolDocument(width):
  1. canvas_ check
  2. Build inspector_panel.html with __INLINE_*__ replaced
     (incl. __INLINE_CONSOLE_HTML__ / __INLINE_CONSOLE_CSS__ / __INLINE_CONSOLE_JS__)
  3. UnloadDevtoolDocument()  // idempotent — releases prior ConsoleEngine first
  4. owned_devtool_document_.reset(html::Parser::Parse(...))
  5. devtool_document_ = owned_devtool_document_.get()
  6. EnsureDevtoolUpdateManager(width)
  7. devtool_script_engine_ = make_unique<QuickjsEngine>()
  8. devtool_script_engine_->Init()
  9. devtool_dom_bindings_ = make_unique<DomBindings>()
 10. devtool_dom_bindings_->Bind(devtool_engine.context, owned_devtool_doc, &em)
 11. devtool_dom_bindings_->SetDevtoolTargetDocument(target_document_)
 12. devtool_dom_bindings_->SetRedactionPolicy(redaction_policy_)
 13. devtool_dom_bindings_->SetHotReloadManager(hot_reload_manager_.get())
 14. RegisterDevtoolBindings(devtool_engine.context)
     ──────────────  NEW (D.1 + D.2) ──────────────
 15. console_script_engine_ = make_unique<ConsoleEngine>()
 16. console_script_engine_->Init()  // auto SetEvalInterruptBudget(10000)
 17. console_log_buffer_ = make_unique<ConsoleLogBuffer>()
 18. RegisterConsoleBindings(console_engine.context, console_log_buffer_.get())
     ─────────────────────────────────────────────
 19. devtool_engine.EvalGlobal(kInspectorPanelJs)  // existing

UnloadDevtoolDocument():
  ──────────────  NEW (reverse of D.1 + D.2)  ──────────────
  1. console_log_buffer_.reset()
  2. console_script_engine_.reset()  // engine_->Shutdown via dtor
  ─────────────────────────────────────────────────────────
  3. devtool_dom_bindings_.reset()
  4. devtool_script_engine_.reset()
  5. devtool_update_manager_.reset()
  6. owned_devtool_document_.reset()
  7. devtool_document_ = nullptr
```

**关键时序约束：**
- **Console 创建在 DevTool engine 之后**（步骤 15 在 14 之后）— RegisterConsoleBindings 不依赖 devtool_engine 但要保证 RegisterDevtoolBindings 完成（避免 EvalGlobal 时漏函数）
- **Console 释放在 DevTool engine 之前**（Unload 步骤 2 在 4 之前）— 避免 console_engine 释放时 inspector_panel.js 仍引用 console binding（虽然不太可能，但 lifecycle 反序更稳健）
- **console_log_buffer_ 释放在 console_engine 之前**（Unload 步骤 1 在 2 之前）— 避免 ConsoleLogPushImpl 还在引用已释放的 buffer 指针（理论不可能，因为 console_engine 释放后无 JS 调用，但反序更稳健）

#### `application.h` 字段（#ifdef 包围 — Phase C R4 范式）

```cpp
class Application {
  // ... existing ...
 private:
  // ... existing devtool fields ...

#ifdef VX_BUILD_DEVTOOL
  // TASK-20260503-04 D.1 — Console isolated JSRuntime + log buffer.
  // Lifecycle synced with LoadDevtoolDocument / UnloadDevtoolDocument.
  // Both fields are #ifdef-guarded because dtor of unique_ptr requires
  // complete type (Phase C R4 范式 / TASK-20260503-01 archive 沉淀).
  std::unique_ptr<vx::devtool::console::ConsoleEngine> console_script_engine_;
  std::unique_ptr<vx::devtool::console::ConsoleLogBuffer> console_log_buffer_;
#endif
};
```

#### `ConsoleEngine` 类（详见 plan §3.D.1 代码骨架）

`ConsoleEngine` 是 `script::QuickjsEngine` 的薄封装：
- ctor 创建 `unique_ptr<QuickjsEngine>`
- `Init()` 调 `engine_->Init()` + 强制 `engine_->SetEvalInterruptBudget(10000)`（T1 mitigation 维度 1+3）
- `EvalGlobal(source, filename)` 转发
- `WasInterrupted() const` 转发
- `context() const` 转发（供 RegisterConsoleBindings 用）

#### 与 `creative-quickjs-host` 兼容性自查

| § | creative-quickjs-host 决策 | 本任务对照 | 兼容性 |
|---|---|---|:-:|
| §组件 1 方案 C Phase 2 | `SetEvalInterruptBudget` + `WasInterrupted` | ConsoleEngine::Init 强制 budget=10000 / WasInterrupted 转发 | ✅ |
| §组件 2 方案 B | `Init` 不调 `js_std_add_helpers` | `ConsoleEngine::Init` 仅调 `QuickjsEngine::Init`（其内部不调 `js_std_*`，已闭环） | ✅ |
| §组件 3 方案 B | `JS_SetMemoryLimit ≈ 32 MiB` 每 runtime | `QuickjsEngine` 已实施（待确认 — 见下方 audit） | ⚠️ 需 audit |
| 实现指导 | 「不在中断回调里分配复杂 C++ 对象；仅 atomic 计数与返回码」 | InterruptHandler 由 QuickjsEngine 已实施（TASK-05 闭环） | ✅ |

**📌 audit 待办：** 检查 `quickjs_engine.cc` 是否调用 `JS_SetMemoryLimit`。如未调用 → 留 P3 候选（不在本任务范围 / 与 Console 引入无直接冲突 / 不影响 T1 mitigation 主线）。

#### DEVTOOL=OFF 路径

`#ifdef VX_BUILD_DEVTOOL` 包围所有 console 字段 + LoadDevtoolDocument 中 D.1+D.2 代码段 + UnloadDevtoolDocument 中反序段。`vx_view_eval_console` / `vx_view_console_log_drain` C API 在 OFF 时 stub 返 `VX_CONSOLE_EVAL_NOT_ATTACHED`。

A14 黑名单加 3 项：`ConsoleEngine` + `ConsoleLogBuffer` + `RegisterConsoleBindings`。

---

## C3 设计：capability allowlist 实施

### 设计挑战

**问题陈述：** spec §11.1 要求 Console 引入时强制：
1. eval 在 isolated JSRuntime（C2 已解决）
2. **capability allowlist**（默认仅 read-only DOM access，禁 file / network / Hot Reload trigger）

需要决策：哪些 binding 暴露给 Console / 哪些不暴露？反向探针单测如何验证？

**约束条件：**
- `RegisterDevtoolBindings` 已 grep 实证 read-only query-only（3 函数：`vx_devtool_get_dom_json` / `vx_view_get_perf_stats` / `vx_devtool_get_hot_reload_status`）— 0 mutator
- `DomBindings::Bind(ctx, doc)` 注册了**大量 mutator**：`Element.setAttribute` / `Style.setProperty` / `Element.addEventListener` / `Document.getElementById` / `Element.textContent` / `Element.innerHTML` 等（grep 实证 dom_bindings.cc 700+ 行）
- 当前 `vx_devtool_get_dom_json` 通过 `bindings->devtool_target_document()` 拿到 target Document，序列化时 `kRedactSensitive` policy（input[type=password] value 已 redact）

**成功标准：**
- Console binding 仅暴露 console.log/error/warn + 现有 query-only DevTool 函数
- Console 用户**不能**调用 `Element.setAttribute` / `Style.setProperty` 等 mutator（`undefined` is not a function）
- Console 用户**不能**触发 file / network / Hot Reload（这些 API 本就不暴露）
- 反向探针单测覆盖 5+ mutator API 的不可访问性

### 探索的方案

#### 方案 A：严格隔离 binding ⭐ 选定

**描述：** Console 走独立 `console_script_engine_`（C2.B），**不**调用 `DomBindings::Bind`。新建 `RegisterConsoleBindings(ctx, buffer)` 函数仅注册：
- `console.log(arg)` / `console.error(arg)` / `console.warn(arg)` — 3 个 console 方法（push 到 buffer）
- `vx_devtool_get_console_log_drain()` — 1 个 drain query
- `vx_console_eval(source)` — 1 个 eval helper（让 console_panel.js 可调用 host eval）
- 复用 `vx_devtool_get_dom_json()` / `vx_view_get_perf_stats()` / `vx_devtool_get_hot_reload_status()` — 3 个既有 query-only DevTool 函数（让 Console 用户能 inspect target）

**全量 API 表（共 7 个 console_engine binding）：**

| # | 函数名 | 类型 | 来源 | mutator? |
|:-:|---|---|---|:-:|
| 1 | `console.log(arg)` | console push | RegisterConsoleBindings 新建 | 否（push to buffer） |
| 2 | `console.error(arg)` | console push | RegisterConsoleBindings 新建 | 否 |
| 3 | `console.warn(arg)` | console push | RegisterConsoleBindings 新建 | 否 |
| 4 | `vx_devtool_get_console_log_drain()` | drain query | RegisterConsoleBindings 新建 | 否（query / clears buffer） |
| 5 | `vx_console_eval(src)` | eval helper | RegisterConsoleBindings 新建 | 否（forward to host） |
| 6 | `vx_devtool_get_dom_json()` | DOM read-only | 复用 RegisterDevtoolBindings | 否（read target / redacted） |
| 7 | `vx_view_get_perf_stats()` | stats read | 复用 RegisterDevtoolBindings | 否 |

**注意：** 不暴露 `vx_devtool_get_hot_reload_status` — 因为它是 DevTool UI 的状态指示器，Console 用户不需要（如未来用户报需求可加 P3）。

**优点：**
- 隔离最严格 — 0 mutator API 暴露
- spec §11.1「禁 file/network/Hot Reload trigger」自动满足（这些 API 本就不暴露）
- T1 mitigation 维度 3（不可绕过）最强
- 反向探针单测设计简单（断言 mutator API undefined）

**缺点：**
- Console 用户**不能** mutate target DOM（如 `getElementById('foo').setAttribute('class', 'bar')` 报错）
  - mitigation：本就是 V1=B 一期范围（spec §11.1「默认仅 read-only DOM access」一致），mutator 暴露留 V2 / P3 候选

**复杂度：** 中（新建 RegisterConsoleBindings 函数 ~150 行 + 3 函数复用 + 5+ 反向探针单测）
**风险：** 低（与 spec 严格对齐）

#### 方案 B：复用 `DomBindings::Bind` + RegisterDevtoolBindings 全套（含 mutator）

**描述：** Console 走 console_script_engine_ 但调用 `DomBindings::Bind`，全量暴露 Element/Style mutator + DevTool query。

**优点：**
- 用户体验最强（开发者友好 / Chrome DevTools 风格 — 用户能 mutate 任意元素）

**缺点：**
- 与 spec §11.1「默认仅 read-only DOM access」**字面冲突**
- T1 mitigation 维度 3（不可绕过）削弱（用户能调用所有 mutator）
- 安全审查难通过

**复杂度：** 低
**风险：** 高（安全冲突）

#### 方案 C：复用 `DomBindings::Bind` + JS-side `Object.freeze` 包裹

**描述：** RegisterConsoleBindings 在 binding 完成后调用 JS-side `Object.freeze(globalThis.document); Object.freeze(Element.prototype); ...`。

**优点：**
- 部分缓解 mutator 暴露（freeze 后不能 reassign properties）

**缺点：**
- `freeze` 不递归 — 嵌套对象（如 `element.style`）仍可写
- 用户能 unfreeze 自己创建的副本（绕过 freeze）
- `addEventListener` 不在 prototype 上 freeze → 仍可调
- 实施复杂度高（递归 freeze / 防绕过 / 边界 case 多）
- 不彻底的隔离 → 仍有 surprise

**复杂度：** 高
**风险：** 中（隔离不彻底）

### 方案对比

| 维度 | 方案 A 严格 ⭐ | 方案 B 全开放 | 方案 C freeze |
|---|---|---|---|
| 复杂度 | 中 | 低 | 高 |
| 隔离彻底性 | 优 | 差 | 中 |
| 与 spec §11.1 字面符合 | ✅ | ❌ | 部分 |
| T1 mitigation 维度 3 | 优 | 差 | 中 |
| 反向探针测设计简洁性 | 优（assert undefined） | N/A | 复杂（递归测） |
| 用户体验 | 中（read-only） | 高 | 高 |
| 安全风险 | 低 | 高 | 中 |

### 推荐并论证

**选定方案：A — 严格隔离 binding。**

**理由：**
- 与 spec §11.1 严格对齐（「默认仅 read-only DOM access，禁 file / network / Hot Reload trigger」）
- T1 mitigation 维度 3（不可绕过）最强
- 反向探针单测设计简单 + 可机械化验证
- mutator 暴露作为 V2 / P3 候选（待用户实际需求驱动 + 安全评审）

**风险缓解：**
- spec §11.1 安全证明（见下方）：3 维度全自动满足，不需要额外代码

### 实现指导

#### `RegisterConsoleBindings` vs `RegisterDevtoolBindings` 对比表

| 函数 | 在 RegisterDevtoolBindings | 在 RegisterConsoleBindings | 复用方式 |
|---|:-:|:-:|---|
| `vx_devtool_get_dom_json` | ✅（既有） | ✅ 复用 | 直接调既有 `JS_NewCFunction(ctx, VxDevtoolGetDomJson, ...)` |
| `vx_view_get_perf_stats` | ✅（既有） | ✅ 复用 | 直接调既有 `JS_NewCFunction(ctx, VxViewGetPerfStats, ...)` |
| `vx_devtool_get_hot_reload_status` | ✅（既有） | ❌ 不暴露 | Console 不需要 HR status |
| `console.log/error/warn` | ❌ | ✅ 新建 | 3 个 JS_NewCFunction，push to buffer |
| `vx_devtool_get_console_log_drain` | ❌ | ✅ 新建 | 1 个 JS_NewCFunction，drain buffer |
| `vx_console_eval` | ❌ | ✅ 新建 | 1 个 JS_NewCFunction，forward to host eval |

#### `RegisterConsoleBindings` 实现骨架

```cpp
// veloxa/devtool/console/console_bindings.cc

namespace vx::devtool::console {

namespace {

// Helper: convert any JSValue to JS string (toString() semantics).
std::string ToUtf8String(JSContext* ctx, JSValueConst val) {
  const char* s = JS_ToCString(ctx, val);
  if (s == nullptr) return "<error>";
  std::string out(s);
  JS_FreeCString(ctx, s);
  return out;
}

// Recover ConsoleLogBuffer pointer from JSContext opaque.
// We use a side-channel struct since DomBindings already uses
// JS_GetContextOpaque for itself. So we register the buffer pointer
// as a property on globalThis (non-enumerable) and recover it there.
ConsoleLogBuffer* GetBuffer(JSContext* ctx) {
  // Implementation: store buffer pointer in a JS hidden property
  // via JS_DefinePropertyValueStr with JS_PROP_NORMAL (no enumerable).
  // For simplicity, use a static thread-local pointer — but that
  // breaks multi-Application isolation. Better: use JS_NewClassID +
  // JS_SetClassProto + JS_GetOpaque on a sentinel object.
  //
  // Pragmatic: since each ConsoleEngine has 1 ctx and is owned by 1
  // Application (no sharing), we attach buffer ptr to ctx via
  // JS_SetContextOpaque at registration time. (DomBindings uses opaque
  // for itself in devtool_engine but Console's own engine has nothing
  // there, so this slot is free.)
  return static_cast<ConsoleLogBuffer*>(JS_GetContextOpaque(ctx));
}

JSValue ConsoleLogImpl(JSContext* ctx, JSValueConst /*this_val*/,
                       int argc, JSValueConst* argv,
                       int level_int) {
  auto* buf = GetBuffer(ctx);
  if (buf == nullptr || argc < 1) return JS_UNDEFINED;
  std::string text;
  for (int i = 0; i < argc; ++i) {
    if (i > 0) text += " ";
    text += ToUtf8String(ctx, argv[i]);
  }
  buf->Push(static_cast<ConsoleLogEntry::Level>(level_int), std::move(text));
  return JS_UNDEFINED;
}

JSValue ConsoleLogVariant(JSContext* ctx, JSValueConst this_val,
                          int argc, JSValueConst* argv) {
  return ConsoleLogImpl(ctx, this_val, argc, argv,
                        static_cast<int>(ConsoleLogEntry::Level::kLog));
}
JSValue ConsoleErrorVariant(JSContext* ctx, JSValueConst this_val,
                            int argc, JSValueConst* argv) {
  return ConsoleLogImpl(ctx, this_val, argc, argv,
                        static_cast<int>(ConsoleLogEntry::Level::kError));
}
JSValue ConsoleWarnVariant(JSContext* ctx, JSValueConst this_val,
                           int argc, JSValueConst* argv) {
  return ConsoleLogImpl(ctx, this_val, argc, argv,
                        static_cast<int>(ConsoleLogEntry::Level::kWarn));
}

JSValue ConsoleLogDrain(JSContext* ctx, JSValueConst /*this_val*/,
                        int /*argc*/, JSValueConst* /*argv*/) {
  auto* buf = GetBuffer(ctx);
  if (buf == nullptr) {
    static constexpr char kEmpty[] =
        "{\"messages\":[],\"truncated\":false,\"dropped_count\":0}";
    return JS_NewStringLen(ctx, kEmpty, sizeof(kEmpty) - 1);
  }
  std::string env = buf->DrainAsJson();
  return JS_NewStringLen(ctx, env.data(), env.size());
}

}  // namespace

void RegisterConsoleBindings(JSContext* ctx, ConsoleLogBuffer* buffer) {
  // Establish opaque-ptr channel for binding -> buffer recovery.
  JS_SetContextOpaque(ctx, buffer);

  JSValue global = JS_GetGlobalObject(ctx);

  // 1. console object with .log / .error / .warn methods.
  JSValue console = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, console, "log",
                    JS_NewCFunction(ctx, ConsoleLogVariant, "log", 1));
  JS_SetPropertyStr(ctx, console, "error",
                    JS_NewCFunction(ctx, ConsoleErrorVariant, "error", 1));
  JS_SetPropertyStr(ctx, console, "warn",
                    JS_NewCFunction(ctx, ConsoleWarnVariant, "warn", 1));
  JS_SetPropertyStr(ctx, global, "console", console);

  // 2. drain query (called by console_panel.js every 200ms).
  JS_SetPropertyStr(ctx, global, "vx_devtool_get_console_log_drain",
                    JS_NewCFunction(ctx, ConsoleLogDrain,
                                    "vx_devtool_get_console_log_drain", 0));

  // 3. NOTE: vx_console_eval is registered in D.4 (depends on host C
  //    API which calls back into ConsoleEngine — circular).

  // 4. NOTE: NOT calling DomBindings::Bind here — Console scope is
  //    strictly isolated from target DOM mutation per spec §11.1.

  // 5. NOTE: NOT registering vx_devtool_get_dom_json /
  //    vx_view_get_perf_stats here — those need DomBindings* opaque
  //    which is already used by Console for ConsoleLogBuffer*. Resolve
  //    by registering them in RegisterConsoleBindings as standalone
  //    closures that find DomBindings via Application::dom_bindings()
  //    (cross-engine reference). See D.2 implementation.
  //    [Decision deferred to D.2 build phase — see plan §3.D.2 R-block.]

  JS_FreeValue(ctx, global);
}

}  // namespace vx::devtool::console
```

**📌 Build 阶段 D.2 待解决：** opaque-ptr channel 冲突 — `vx_devtool_get_dom_json` 既有实现用 `JS_GetContextOpaque(ctx) → DomBindings*`，但本任务 RegisterConsoleBindings 占用同一 opaque slot 给 `ConsoleLogBuffer*`。两种解决路径：
- **路径 1**：复用 query-only 函数时不通过 opaque，改为闭包 capture（CFunction 不支持 capture，需用 `JS_NewCFunctionData` + magic data 槽）
- **路径 2**：Console scope 不再暴露 `vx_devtool_get_dom_json` / `vx_view_get_perf_stats` —  Console 用户从 console_panel.js 内通过 `vx_console_eval` 间接访问（host eval 路径自然有 DomBindings ctx）— **更简洁，推荐**

**Build 阶段决策**：默认走路径 2（不在 console_engine 内暴露 DOM/perf query；Console 用户需要 inspect 时切到 DOM/Style/Layout tab 看），保留 vx_console_eval 作为 Console 唯一 host 入口。

#### 反向探针单测设计

```cpp
// tests/devtool/console/console_isolation_test.cc

TEST(ConsoleIsolationTest, DomMutatorsNotExposed) {
  ConsoleEngine engine;
  ASSERT_TRUE(engine.Init().ok());
  ConsoleLogBuffer buffer;
  RegisterConsoleBindings(engine.context(), &buffer);

  // Reverse probe: each mutator must be undefined in Console scope.
  struct Probe {
    const char* expr;
    const char* expected_undefined_keyword;
  };
  const Probe probes[] = {
    {"typeof Element",                 "undefined"},  // no DOM Element class
    {"typeof document",                "undefined"},  // no document object
    {"typeof setAttribute",            "undefined"},
    {"typeof addEventListener",        "undefined"},
    {"typeof vx_view_attach_devtool",  "undefined"},  // no host attach API
    {"typeof vx_devtool_get_hot_reload_status", "undefined"},  // not exposed
  };
  for (const auto& p : probes) {
    auto r = engine.EvalGlobal(StringView(p.expr), StringView("probe"));
    ASSERT_TRUE(r.ok()) << "eval failed for: " << p.expr;
    EXPECT_EQ(r.value(), p.expected_undefined_keyword)
        << "Probe leaked: " << p.expr;
  }
}

TEST(ConsoleIsolationTest, AllowedBindingsExposed) {
  ConsoleEngine engine;
  ASSERT_TRUE(engine.Init().ok());
  ConsoleLogBuffer buffer;
  RegisterConsoleBindings(engine.context(), &buffer);

  // Forward probe: each allowlisted binding must be a function/object.
  struct Probe { const char* expr; const char* expected; };
  const Probe probes[] = {
    {"typeof console",                          "object"},
    {"typeof console.log",                      "function"},
    {"typeof console.error",                    "function"},
    {"typeof console.warn",                     "function"},
    {"typeof vx_devtool_get_console_log_drain", "function"},
  };
  for (const auto& p : probes) {
    auto r = engine.EvalGlobal(StringView(p.expr), StringView("probe"));
    ASSERT_TRUE(r.ok()) << p.expr;
    EXPECT_EQ(r.value(), p.expected) << p.expr;
  }
}
```

#### spec §11.1 安全证明

| spec §11.1 要求 | 本任务实施 | 自动满足证明 |
|---|---|---|
| isolated JSRuntime | C2.B 新建 console_script_engine_ | 独立 QuickjsEngine 实例 / 独立 JSRuntime / 独立 InterruptHandler ✅ |
| capability allowlist 默认仅 read-only DOM access | C3.A 不暴露 vx_devtool_get_dom_json | Console scope 0 DOM API ✅（read-only 比 spec 字面更严格 — 一期决策） |
| 禁 file | inherits creative-quickjs-host §组件 2 方案 B | 不调 `js_std_add_helpers` → 无 `os.read` / `std.loadFile` ✅ |
| 禁 network | inherits creative-quickjs-host §组件 2 方案 B | 同上 → 无网络 API ✅ |
| 禁 Hot Reload trigger | C3.A 不暴露 vx_view_attach_devtool | Console scope 0 host attach API ✅ |

---

## C4 设计：console.log 桥接（drain JSON envelope + 双上限 + utf8 截断）

### 设计挑战

**问题陈述：** Console 内 JS 调用 `console.log/error/warn` 时，如何把消息从 isolated `console_script_engine_` runtime 桥接到 DevTool UI Document 的 console panel 显示区？需要：
1. **drain 协议**：与既有 `vx_devtool_get_dom_json` query-style 范式一致（B5.A 锁定 envelope 格式）
2. **双上限 mitigation**：max_count = 1000 entries + 单条 max_text = 4 KiB（B3.A 锁定）
3. **utf8 边界截断**：单条超长 truncate 时不能在多字节字符中间切（破坏 UTF-8 编码）
4. **level 颜色 mapping**：log/warn/error 不同视觉
5. **ts_ms 时间源**：host time（unix ms）便于多日志关联

**约束条件：**
- 本任务为单线程（Application::Update 主线程）— buffer 操作无并发
- Veloxa 自渲染层 CSS 子集已支持 color / background / font-family / overflow:scroll / padding（grep 实证 Phase A/B/C）
- `veloxa/foundation/strings/utf.cc` 已有 `EncodeUtf8` / `DecodeUtf8` 但**无 FindUtf8Boundary**（需自实现）
- 既有 hot_reload status binding (`VxDevtoolGetHotReloadStatus`) 已实现 envelope 范式（`{"tracked":N,"last_error":"..."}` + JSON-escape ` \" \\ \uXXXX`）— 可参考

**成功标准：**
- drain envelope JSON round-trip（host push → JS drain → JSON parse → render）
- 双上限触发场景：count=1001 时 drop oldest + dropped_count=1；text=4097 bytes 时 truncate 至 ≤ 4096 + UTF-8 边界 + append "... [truncated]"
- level 颜色 mapping 视觉清晰（log 灰 / warn 橙 / error 红）
- ts_ms 单调递增（host time 来源 std::chrono::system_clock）
- 反向探针：buffer 满 + 超长 text 同时出现时正确处理（dropped_count + truncated 同时为 true）

### 探索的方案

#### 方案 A：drain 模式 + 双上限 + utf8 边界截断 ⭐ 选定

**描述：** Application 持 `unique_ptr<ConsoleLogBuffer>`，每条 `console.log` 在 buffer 内 push（含 level / text / ts_ms）。push 时若 text 超 4 KiB 则 truncate 至最近 utf8 边界 + append `... [truncated]`；若 buffer count 超 1000 则 `pop_front` + `dropped_count++`。`vx_devtool_get_console_log_drain` 返回 envelope JSON 字符串 + 清空 buffer。

**优点：**
- 与既有 query-style 范式 100% 一致
- 单线程同步无 race
- 双上限 mitigation 简单清晰
- utf8 边界截断保证 envelope JSON 合法（不破坏 UTF-8 编码）
- T6（buffer 上限）+ T1（截断 message 防 OOM）双 mitigation

**缺点：**
- 200ms 轮询延迟（drain 5 fps）— 不影响开发者体验
- buffer 在 Application 内长驻 ~~1000 × 4 KiB = 4 MiB worst case~~ — 但实际场景 buffer 通常 < 100 entries × 100 bytes = 10 KB，4 MiB 是极限

**复杂度：** 中
**风险：** 低

#### 方案 B：push-style C API callback

**描述：** `vx_devtool_register_console_log_callback(callback, opaque)` C API + console.log JS 函数 toString args 后调 callback → DevTool UI append 节点。

**优点：**
- 实时（无轮询延迟）
- 无 buffer 内存占用

**缺点：**
- 需新 push-style API（与既有 query-style 范式不一致）
- 跨 runtime 回调线程安全考量（虽然单线程但范式不同）
- callback 在 console.log 同步执行栈中调用 → DevTool UI render 在 Console eval 中嵌套发生 → 复杂度上升

**复杂度：** 高
**风险：** 中

#### 方案 C：共享 in-process queue + 主线程 drain

**描述：** Push to in-process queue + main thread drain (类似 Phase C HotReload event queue 范式).

**优点：**
- 与 Phase C HotReload 跨线程 queue 范式同源

**缺点：**
- 本任务无跨线程需求（console.log 与 drain 都在主线程）
- 引入不必要的 mutex / condition_variable

**复杂度：** 高
**风险：** 中

### 方案对比

| 维度 | 方案 A drain ⭐ | 方案 B callback | 方案 C queue |
|---|---|---|---|
| 复杂度 | 中 | 高 | 高 |
| 与既有范式一致性 | 优（与 vx_devtool_get_*) | 差（新范式） | 中 |
| 实时性 | 中（200ms 延迟） | 优 | 中 |
| 实施工时 | 中 | 高 | 高 |
| 内存占用 | 中（buffer 长驻） | 0 | 中 |
| 风险 | 低 | 中 | 中 |

### 推荐并论证

**选定方案：A — drain 模式 + 双上限 + utf8 边界截断。**

**理由：**
- 与既有 query-style 范式 100% 一致（vx_devtool_get_dom_json / get_perf_stats / get_hot_reload_status 都是 query-only）
- 单线程同步无 race
- 双上限 mitigation 简单清晰
- 200ms 轮询延迟不影响开发者体验
- buffer ~4 MiB worst case 是可接受成本（DevTool ON 场景）

**风险缓解：**
- buffer 上限 mitigation（详见下方算法）
- DEVTOOL=OFF 时 stub 返空 envelope（A14 黑名单）

### 实现指导

#### drain JSON envelope 完整规范（B5.A）

```json
{
  "messages": [
    {
      "level": "log",         // string: "log" | "warn" | "error"
      "text": "hello world",  // string: JSON-escaped UTF-8
      "ts": 1714809600000     // number: unix epoch milliseconds (host time)
    },
    ...
  ],
  "truncated": false,         // boolean: true if any single message in this drain was truncated
  "dropped_count": 0          // number: count of oldest messages dropped since last drain (cumulative buffer overflow count)
}
```

**字段详细：**

| 字段 | 类型 | 语义 | 边界处理 |
|---|---|---|---|
| `messages` | array | 排序：按 push 顺序（FIFO） | 空 buffer 时 `[]` |
| `messages[].level` | string | `"log"` / `"warn"` / `"error"` | enum 严格 |
| `messages[].text` | string | UTF-8 / JSON-escaped | 单条 ≤ 4 KiB（含 truncate 后缀）|
| `messages[].ts` | number | unix epoch ms | 来自 `std::chrono::system_clock::now()` |
| `truncated` | boolean | 当前 drain 是否含被截断的 message | 与 dropped_count 独立 |
| `dropped_count` | number | 自上次 drain 以来 buffer overflow 丢弃的旧消息数 | drain 后归零 |

#### 双上限 mitigation 算法（B3.A）

```cpp
// veloxa/devtool/console/console_bindings.cc

void ConsoleLogBuffer::Push(ConsoleLogEntry::Level lvl, std::string text) {
  // Step 1: truncate text if over per-text cap (UTF-8 boundary safe).
  if (text.size() > kMaxTextBytes) {
    static constexpr StringView kTruncSuffix = "... [truncated]";
    // Reserve room for suffix so total <= kMaxTextBytes.
    usize keep = kMaxTextBytes - kTruncSuffix.size();
    keep = TruncateAtUtf8Boundary(text, keep);  // see algo below
    text.resize(keep);
    text.append(kTruncSuffix.data(), kTruncSuffix.size());
    truncated_ = true;
  }

  // Step 2: enforce count cap (drop oldest).
  if (entries_.size() >= kMaxCount) {
    entries_.pop_front();
    dropped_count_++;
  }

  // Step 3: push.
  entries_.push_back({
      lvl,
      std::move(text),
      static_cast<u64>(std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch()).count())
  });
}
```

#### utf8 边界截断算法（自实现，无累积量歧义 → §d.2 不触发）

```cpp
// veloxa/devtool/console/console_bindings.cc (anonymous namespace)

// Returns the largest n' ≤ n such that buf[0..n') is a valid prefix of
// a UTF-8 string (i.e. doesn't end mid-codepoint). UTF-8 continuation
// bytes have the high bits 10xxxxxx (mask 0xC0 == 0x80). Lead bytes
// are 0xxxxxxx (ASCII), 110xxxxx, 1110xxxx, or 11110xxx.
//
// Algorithm: walk backwards from buf[n-1], skip continuation bytes
// (mask 0xC0 == 0x80). Stop at the first lead byte. Determine the
// expected codepoint length from the lead byte; if the codepoint extends
// past n, drop it (return lead_pos). Otherwise the codepoint fits, return n.
//
// Edge cases:
//   - n == 0: return 0
//   - all bytes are ASCII: return n unchanged
//   - n bytes ends mid 4-byte codepoint: return position of that codepoint's lead
usize TruncateAtUtf8Boundary(const std::string& text, usize n) {
  if (n == 0 || n >= text.size()) {
    return std::min(n, text.size());
  }
  // Walk backwards skipping continuation bytes (10xxxxxx).
  usize i = n;
  while (i > 0) {
    auto b = static_cast<u8>(text[i - 1]);
    if ((b & 0xC0) == 0x80) {  // continuation byte
      i--;
      continue;
    }
    // Found lead byte at position i-1.
    usize lead_pos = i - 1;
    // Expected codepoint length from lead byte.
    usize expect_len;
    if ((b & 0x80) == 0x00) expect_len = 1;       // 0xxxxxxx
    else if ((b & 0xE0) == 0xC0) expect_len = 2;  // 110xxxxx
    else if ((b & 0xF0) == 0xE0) expect_len = 3;  // 1110xxxx
    else if ((b & 0xF8) == 0xF0) expect_len = 4;  // 11110xxx
    else expect_len = 1;  // invalid lead byte; treat as solo (defensive)
    // Does this codepoint fit entirely within [0, n)?
    if (lead_pos + expect_len <= n) {
      return n;  // current n is fine
    } else {
      return lead_pos;  // drop the partial codepoint
    }
  }
  return 0;  // text starts with continuation bytes (invalid UTF-8)
}
```

**单测覆盖（5 case）：**

| # | 输入 | n | 期望返回 |
|:-:|---|:-:|:-:|
| 1 | "Hello" (ASCII) | 3 | 3（ASCII 任意截断） |
| 2 | "你好" (中文，2 chars × 3 bytes = 6 bytes) | 4 | 3（保留第 1 个完整字符 "你"） |
| 3 | "你好" | 6 | 6（完整保留） |
| 4 | "🎉Hi" (emoji 4 bytes + ASCII) | 3 | 0（emoji 半截 → 全 drop） |
| 5 | "" | 0 | 0 |

#### `DrainAsJson` envelope 构建（参考既有 `VxDevtoolGetHotReloadStatus` JSON-escape 范式）

```cpp
std::string ConsoleLogBuffer::DrainAsJson() {
  std::string out;
  out.reserve(64 + entries_.size() * 80);  // rough estimate
  out.append("{\"messages\":[");
  bool first = true;
  for (const auto& e : entries_) {
    if (!first) out.push_back(',');
    first = false;
    out.append("{\"level\":\"");
    switch (e.level) {
      case ConsoleLogEntry::Level::kLog:   out.append("log");   break;
      case ConsoleLogEntry::Level::kError: out.append("error"); break;
      case ConsoleLogEntry::Level::kWarn:  out.append("warn");  break;
    }
    out.append("\",\"text\":\"");
    AppendJsonEscaped(e.text, out);  // helper: \" \\ \uXXXX for c<0x20
    out.append("\",\"ts\":");
    char num_buf[32];
    std::snprintf(num_buf, sizeof(num_buf), "%llu",
                  static_cast<unsigned long long>(e.ts_ms));
    out.append(num_buf);
    out.push_back('}');
  }
  out.append("],\"truncated\":");
  out.append(truncated_ ? "true" : "false");
  out.append(",\"dropped_count\":");
  char num_buf[32];
  std::snprintf(num_buf, sizeof(num_buf), "%u", dropped_count_);
  out.append(num_buf);
  out.push_back('}');

  // Atomically clear all state so next drain starts fresh.
  Clear();
  return out;
}

void ConsoleLogBuffer::Clear() {
  entries_.clear();
  dropped_count_ = 0;
  truncated_ = false;
}
```

#### level 颜色 mapping（C1 console_panel.css 已示）

| level | CSS class | 颜色 | RGB |
|---|---|---|---|
| `log` | `.console-log` | 灰 | `#d0d0d0` |
| `warn` | `.console-warn` | 橙 | `#ffaa00` |
| `error` | `.console-error` | 红 | `#ff6060` |
| (input) | `.console-input` | 黄 | `#ffeb3b`（user input echo） |
| (result) | `.console-result` | 浅蓝 | `#a0d0ff`（eval result） |

#### ts_ms 时间源选择

**选定：** host time（`std::chrono::system_clock::now()` unix epoch ms）

**理由：**
- 便于多日志关联（与 OS 日志、文件 mtime、网络日志对齐）
- vs eval-relative time（`steady_clock` 自 ConsoleEngine::Init 起）— 重启后无法对齐外部日志
- vs frame number — 与帧率耦合，不适合开发者用

**注意：** `system_clock` 不保证单调递增（NTP 校时可能后跳）。在测试中如果断言 ts 单调递增，需用 `>=` 而非 `>`。

---

## 总体决策汇总

| 决策 | 锁定值 | 实施位置 |
|---|---|---|
| C1 第 4 件套布局 | A 第 4 tab 平级（DOM/Style/Layout/Console） | `inspector_panel.html` 加 button + panel div / `console_panel.{html,css,js}` 新建 / Application::LoadDevtoolDocument 加 3 ReplaceFirst |
| C1 input 行为 | JS 自管理 value state + 全局 keydown + `<span>` 显示 | `console_panel.js` 自实现（不用 `<input>`，因 Veloxa 无原生输入行为） |
| C1 keydown 冲突解决 | `isConsoleActive()` 守卫 + Enter/Backspace/printable 单字符自处理 + 其他键 let host handle | `console_panel.js` |
| C2 isolated runtime | B 新建 console_script_engine_（第 3 个 QuickjsEngine 实例） | `veloxa/devtool/console/console_engine.{h,cc}` 薄封装 + Application 持 unique_ptr |
| C2 lifecycle 时序 | Load: target → devtool → console / Unload: console → devtool → target | `Application::LoadDevtoolDocument` + `UnloadDevtoolDocument` |
| C2 DEVTOOL=OFF | application.h `#ifdef VX_BUILD_DEVTOOL` 包围 unique_ptr 字段（Phase C R4 范式） | `application.h` |
| C3 capability allowlist | A 严格隔离 binding（不调 DomBindings::Bind / 仅 console.* + drain query） | `RegisterConsoleBindings` 函数 |
| C3 反向探针 | 5+ mutator API 断言 undefined + 5 allowlist 断言 typeof | `tests/devtool/console/console_isolation_test.cc` |
| C4 drain JSON envelope | B5.A 完整格式（messages/level/text/ts + truncated + dropped_count） | `ConsoleLogBuffer::DrainAsJson` |
| C4 双上限 | max_count = 1000 + max_text = 4 KiB（drop oldest / truncate） | `ConsoleLogBuffer::Push` |
| C4 utf8 截断 | TruncateAtUtf8Boundary 自实现（向前扫描非 continuation byte） | `console_bindings.cc` 匿名命名空间 |
| C4 level 颜色 | log 灰 / warn 橙 / error 红 + input 黄 / result 蓝 | `console_panel.css` |
| C4 ts_ms 时间源 | host time `std::chrono::system_clock::now()` unix ms | `ConsoleLogBuffer::Push` |

---

## 与 spec / 既有 creative 的交叉引用

- **spec `docs/specs/2026-04-30-devtool-design.md` §11.1**：本文件全面落地 C1-C4 4 维度；spec §11.1「依赖 D2=B JSON 协议 + D7=C 双层 API + 威胁面 T1：1) eval 在 isolated JSRuntime；2) capability allowlist」全部对应实施
- **creative `creative-devtool-screen-layout.md` 决策 1**：splitter dock + HUD overlay 双层结构 — C1.A 第 4 tab 复用 splitter dock 区域，HUD 不变
- **creative `creative-devtool-screen-layout.md` 决策 2**：splitter dock-to-right / dock-to-bottom 双模式 — Console tab 在 dock-to-bottom 时天然变 Chrome 风格底部 console（无需新协议）
- **creative `creative-quickjs-host.md` §组件 1 方案 C Phase 2**：TASK-05 已闭环 SetEvalInterruptBudget API — C2.B ConsoleEngine 复用 + 强制 budget=10000
- **creative `creative-quickjs-host.md` §组件 2 方案 B**：不调 js_std_add_helpers — C2.B ConsoleEngine 严格遵守（继承 QuickjsEngine 默认行为）
- **creative `creative-quickjs-host.md` §组件 3 方案 B**：JS_SetMemoryLimit ~32 MiB — C2.B ConsoleEngine 自动继承（待 audit `quickjs_engine.cc` 是否调用 JS_SetMemoryLimit；如未调用 → 留 P3 候选）
- **TASK-20260502-01 archive 5 大范式**：双层 API + #ifdef + CMake / 资源嵌入 / canvas Translate / dogfood — 本任务全部复用且加深
- **TASK-20260502-02 archive lazy-attach C ABI 容错模式**：vx_view_eval_console + vx_view_console_log_drain 复用此模式（DEVTOOL=OFF 或未 attach 返 NOT_ATTACHED）
- **TASK-20260503-01 archive #ifdef + CMake conditional 范式 unique_ptr 子模式**：C2.B `#ifdef` 包围 application.h `unique_ptr<ConsoleEngine>` 直接复用 Phase C R4 教训

---

## 不在本 creative 范围（V2 / P3 候选）

- **mutator binding 暴露**：C3.B 候选 — 用户实际需求驱动 + 安全评审后启用
- **splitter 内拆分上下区域 / 独立 toggle 键 F10**：C1.B/C 候选 — 非 V1=B 一期范围
- **JS_SetMemoryLimit audit 与显式调用**：creative-quickjs-host §组件 3 — 留 P3 候选（与本任务无直接冲突）
- **kCancelled 进一步语义拆分**：TASK-05 reflection §6 P3 #2 — 非本任务范围
- **Console 历史命令 + 上下方向键回溯**：未来增强（V2）
- **Console 自动补全 + 高亮**：未来增强（V2）
- **Console source map / breakpoint 集成**：依赖 TASK-30-04-E JS Debugger backend（独立立项）
- **vx_view_console_clear() 手动清空 buffer**：B6.C 候选 — 留 P3
- **drain 频率 200ms → 自适应**：用户报需求驱动

---

## 版本记录

| 版本 | 日期 | 说明 |
|---|---|---|
| 1.0 | 2026-05-04 | `/creative` 初版 — 4 维度详细设计落盘 |

---

**待用户审查重点：**

1. C1 决策 — `<span>` + JS 自管理 value state（vs `<input>`）的方案是否可接受？是否需要在 V2 阶段补足 `<input>` 原生支持（涉及 EventManager focus + value attribute mutator）？
2. C2 决策 — application.h `#ifdef` 包围 `unique_ptr<ConsoleEngine>` 字段 + getter 双向条件编译是否需要更细粒度（如分别 `#ifdef` 字段 / getter / 构造析构）？
3. C3 决策 — 不暴露 `vx_devtool_get_dom_json` / `vx_view_get_perf_stats` 给 Console（用户从 console_panel.js 内调 vx_console_eval 间接访问）— 是否符合用户预期？
4. C4 决策 — 双上限 max_count=1000 + max_text=4 KiB 是否合理？还是需要可配置（B6.C `vx_view_console_clear` + 可配置上限留 P3）？
