# DevTool 三件套蓝图（Inspector + Performance Overlay + Hot Reload）— 设计规格

**任务：** TASK-20260430-04
**复杂度：** Level 4（多子系统蓝图 + 8 决策矩阵 + 8 威胁面）
**创建日期：** 2026-04-30
**状态：** /plan 头脑风暴完成（D1-D8 全部锁定），待用户审查
**安全相关：** ✅ 是（V5=✅；T2/T3/T5/T6/T7/T8 完整建模 + T1/T4 扩展段占位）
**主交付物：** 蓝图级 spec + plan + 2 篇 creative（V2=a 纯蓝图，build 由用户基于产出 plan 独立立项）

---

## 1. 目的（What）

为 Veloxa 引擎规划首个完整 **DevTool 主线**，落地三件套：**Inspector**（DOM / Style / Layout 可视化检查）+ **Performance Overlay**（FPS / 帧时长 / 各阶段 / dirty rect 可视化）+ **Hot Reload**（CSS-only 增量重载，Linux 嵌入式优先）。

DevTool 一期采用 **dogfood 自渲染路径**（V3=A）— DevTool UI 自身是一个 Veloxa View（HTML+CSS+JS），与目标 View 在单进程同 Application 共享 EventLoop / ImageCache，但持独立 Document。这是 Veloxa「嵌入式优先 + 自我应用」定位的最直接体现。

DevTool 主线的下游价值：
1. 解锁 Veloxa 引擎自身的开发体验闭环（CSS 调样式不重启 / FPS 实时观测 / DOM 树可视化诊断）
2. 闭环 4 项历史技术债 ROI 路径（#26 LayoutBox.Dump / #35 UpdateManager 帧钩子 / #40 C API introspection / #4 ImageCache 命名空间隔离）
3. 为未来扩展子系统（Console / JS Debugger / 完整 UI Editor / CDP 远程调试）预留架构插槽

## 2. 不做（Out of Scope，YAGNI）

- ❌ 不实施代码（**V2=a 纯蓝图**）— 主交付为 spec + plan + 2 creative；build 由用户基于产出 plan 拆出独立 Level 3 子任务（每件套一个）
- ❌ 不含 **Console**（JS REPL + console.log 桥接）— 列入 §11 扩展段（T1 威胁面占位）
- ❌ 不含 **JS Debugger**（断点 / 单步 / 变量检查）— 列入 §11 扩展段（依赖技术债 #44 QuickJS Interrupt Handler 基础设施先做）
- ❌ 不含 **完整 UI 编辑器**（拖拽布局 + 反向写回 HTML/CSS）— 列入 §11 扩展段（双向数据绑定层架构空白，量级 Level 4+ 单独立项）
- ❌ 不含 **跨平台 file watcher**（macOS kqueue / Windows ReadDirectoryChangesW）— D5=A 锁定 Linux inotify 一期；跨平台留 P2 候选
- ❌ 不含 **CDP（Chrome DevTools Protocol）远程调试**— D7=C 双层 API 已为未来对接预留 thin wrapper，但本任务不引 WebSocket / IPC
- ❌ 不含 **JS / HTML 增量 hot reload**（D5.2=b CSS-only 锁定）— JS re-eval / DOM diff 留 P2 候选

## 3. 三件套验收标准（A1-A12）

### Inspector（A1-A5）

| # | 标准 | 验证方式 |
|:-:|---|---|
| A1 | DevTool 主屏可见 DOM tree 文本 + 折叠树节点交互 | 手工运行 + 屏幕观察 + DOM tree JSON 序列化单测 |
| A2 | 鼠标 hover 目标 View 元素 → DevTool 高亮选中节点 + DisplayList overlay 红框 | 手工 + DisplayList overlay 命令单测 |
| A3 | 选中节点显示 ComputedStyle JSON（属性 + 计算值） | 手工 + `vx_node_get_computed_style_json()` C API 单测 |
| A4 | 选中节点显示 LayoutBox JSON（content / padding / border / margin 几何） | 手工 + `vx_node_get_layout_box_json()` C API 单测 |
| A5 | DOM 序列化默认 redact `<input type="password">` value 为 `[REDACTED]` | 单测：构造 password input → 序列化 → 断言无明文 |

### Performance Overlay（A6-A9）

| # | 标准 | 验证方式 |
|:-:|---|---|
| A6 | HUD 显示 FPS 数字（滑动 60 帧均值） | 手工运行 + 60 帧累计单测 |
| A7 | HUD 显示 4 阶段时长 bars（style / layout / render / paint） | 手工 + UpdateManager 五钩子触发顺序单测 |
| A8 | dirty rect 边框高亮（每帧标记重绘区域） | 手工 + dirty rect collector 单测 |
| A9 | UpdateManager 钩子 callback 1ms/frame 执行预算超时 abort frame | 单测：注入慢 callback → 断言 abort + 错误日志 |

### Hot Reload（A10-A12）

| # | 标准 | 验证方式 |
|:-:|---|---|
| A10 | 修改 watcher root 内 .css 文件 → 目标 View 自动 restyle 不重启 | 手工 + inotify 事件 → restyle 触发单测 |
| A11 | symlink 解析后超出 watcher root → 拒绝重载 + 安全日志 | 单测：构造越权 symlink → 断言 kPermissionDenied |
| A12 | 文件大小超过 max_size（默认 4 MiB CSS）→ 拒绝重载 | 单测：写 5 MiB css → 断言 kOutOfMemory |

### 全局约束（A13-A14）

| # | 标准 | 验证方式 |
|:-:|---|---|
| A13 | 现有 ctest 全绿（DevTool OFF 时零回归） | `ctest --output-on-failure`（默认 `VX_BUILD_DEVTOOL=OFF`）|
| A14 | DevTool 关闭时构建产物零变化（链接闭合 + binary size diff = 0） | `cmake --build` + `ls -l` size diff |

## 4. 设计决策矩阵 D1-D8（VAN + 头脑风暴产出）

| # | 维度 | 锁定值 | 理由（核心 1-2 句） |
|:-:|---|---|---|
| **D1** | 三件套实施优先级 | **B** Inspector → Overlay → Hot Reload | Inspector 是 DevTool UI 渲染样板（dogfood V3=A），Overlay/Hot Reload 复用其 UI 区域；闭环 #26/#40/#4 三大技术债；与 Chrome / Sciter DevTools 业界惯例一致 |
| **D2** | Inspector 数据采集协议 | **B** 半结构化（JSON tree + DisplayList overlay + C API JSON） | C API 边界清晰未来可对接 CDP；DisplayList overlay 不污染目标 DOM 避免 hover 触发 restyle 闭环；与 Overlay/Hot Reload 协议同构 |
| **D3** | DevTool UI 主屏布局 | **B** 同窗口 splitter dock + Overlay 子系统 HUD 子模式 | 嵌入式 / 单显示器友好；splitter 即 DevTool UI 渲染样板（dogfood）；Performance Overlay 自然契合 HUD 语义 |
| **D4** | DevTool 隔离边界 | **B** 单进程共享容器（双 Document + 共享 EventLoop / Application / ImageCache） | 嵌入式硬约束 = 单进程无 IPC overhead；同地址空间访问简单；与 D2 DisplayList overlay 注入语义一致 |
| **D5** | Hot Reload file watcher + 增量策略 | **A** 嵌入式专注（Linux inotify + CSS-only 增量重载） | Veloxa 嵌入式定位主目标 = Linux；CSS hot reload 价值密度 80%；自实现 inotify ~150-200 LOC 远比 efsw 简单 |
| **D6** | Performance Overlay 数据采集点 | **B** Chrome DevTools 风格（五钩子 + 滑动 60 帧 + dirty rect 边框高亮） | 五钩子粒度匹配 4 阶段 pipeline + 整帧；滑动 60 帧实时性 + 内存恒定；dirty rect 边框高亮业界惯例 |
| **D7** | C API 扩展边界 | **C** 双层 API（内部 C++ 核心 + 公开 C API 薄封装） | 兼顾性能（DevTool 同进程零拷贝）+ 扩展性（C API JSON for CDP/IDE/外部插件）+ 与 D2=B 协议一致 |
| **D8** | 安全威胁建模 | **A** T2/T3/T5/T6/T7/T8 完整 + T1/T4 扩展段占位 | 与 V5=✅ + 三件套实际威胁面对齐；T1（Console JS REPL）/ T4（CDP 远程 port）扩展段引入时强制实施 |

## 5. 架构

### 5.1 模块边界

```
┌──────────────────────────────────────────────────────────────────────┐
│ examples/hello_devtool.cc  (用户空间 — 仅 VX_BUILD_DEVTOOL=ON 时编译) │
│   ├─ vx_view_create(&target_config)                                  │
│   ├─ vx_view_attach_devtool(target_view, &devtool_opts)              │
│   └─ vx_view_run(target_view)  // 内部驱动 splitter dispatch          │
└──────────────────────────────────────────────────────────────────────┘
                                ▼
┌──────────────────────────────────────────────────────────────────────┐
│ veloxa/devtool/  (新建子系统)                                         │
│                                                                      │
│  application_devtool.{h,cc}  (Application 双 Document 槽扩展)         │
│    + Document* devtool_document_;     // 第二槽，nullable             │
│    + Document* target_document_;      // 主槽（重命名 from document_） │
│    + InputDispatchSplitter splitter_; // 按 X/Y 区域路由 input        │
│                                                                      │
│  inspector/                                                          │
│    inspector_data.h         (内部 C++ API — DevTool 直用零拷贝)       │
│      + Serializer::ToJson(dom::Node*) → BasicString                  │
│      + LayoutBox::ToJson() → BasicString                             │
│      + ComputedStyle::ToJson(redaction_policy) → BasicString         │
│    inspector_overlay.{h,cc}  (DisplayList overlay 注入)               │
│      + InjectHoverHighlight(target_doc, dom_node) → PaintCommand     │
│      + ResetOverlayCommands(target_doc)  // 每帧开头复位              │
│    inspector_panel.html / .css / .js  (DevTool UI — 自渲染)          │
│      DOM tree view + Style panel + Layout panel + node selection     │
│                                                                      │
│  overlay/                                                            │
│    perf_overlay.{h,cc}      (UpdateManager 五钩子 + 滑动 60 帧)       │
│      + frame_stats_[60]: ring buffer of FrameStats                   │
│      + struct FrameStats { f32 style_ms, layout_ms, render_ms,       │
│        paint_ms, total_ms; Vector<DirtyRect> dirty_rects; }          │
│      + RenderHud(target_doc, hud_canvas)                             │
│    update_manager_hooks.{h,cc}  (技术债 #35 闭环)                     │
│      + struct PipelineHooks {                                        │
│          callback OnFrameStart, OnAfterStyle, OnAfterLayout,         │
│          OnAfterRender, OnFrameEnd; }                                │
│                                                                      │
│  hot_reload/                                                         │
│    file_watcher.h            (跨平台抽象基类 — 一期仅 Linux 实现)     │
│      + virtual void Start(WatchOpts) = 0;                            │
│      + virtual void Stop() = 0;                                      │
│      + callback OnFileChanged(path, kind {modify/close_write})       │
│    file_watcher_inotify.{h,cc}  (Linux inotify 实现)                  │
│      + ~150-200 LOC                                                  │
│      + canonicalize 守卫（symlink resolve + boundary check）           │
│      + max_size 守卫（默认 4 MiB CSS）                                 │
│    hot_reload_manager.{h,cc}                                         │
│      + ProcessFileChange(path) → CSS=restyle / HTML=full reload      │
│      + state preservation: scroll position / hover state             │
└──────────────────────────────────────────────────────────────────────┘
                                ▼
┌──────────────────────────────────────────────────────────────────────┐
│ veloxa/api/  (扩展)                                                   │
│   + vx_view_attach_devtool(view, opts) — 关键入口                     │
│   + vx_view_serialize_dom_json(view, out_buf, out_len)               │
│   + vx_node_get_computed_style_json(view, node_id, out_buf, out_len) │
│   + vx_node_get_layout_box_json(view, node_id, out_buf, out_len)     │
│   + vx_inspector_set_redaction_policy(view, policy)                  │
│   + vx_view_set_pipeline_hooks(view, hooks)  // 内部 DevTool 用       │
└──────────────────────────────────────────────────────────────────────┘
                                ▼
┌──────────────────────────────────────────────────────────────────────┐
│ veloxa/core/  (现有 — 受影响 / 扩展)                                  │
│   application.h            ! Document* → 双槽（target + devtool）     │
│   layout/layout_box.h      + Dump() / ToJson()  (技术债 #26 闭环)     │
│   render/update_manager.h  + PipelineHooks / 五钩子注入点             │
│                              (技术债 #35 闭环)                         │
│   dom/serializer.h         + ToJson(redaction_policy)  (新增)         │
│   image/image_cache.h      ! handle namespace 隔离 ("devtool:" prefix)│
│                              (技术债 #4 闭环)                         │
└──────────────────────────────────────────────────────────────────────┘
```

### 5.2 注入点核对表（writing-plans「管线注入点代码级可行性验证」必填）

| # | 注入点 | 当前代码位置 | 可行性 | 需要的接口扩展 |
|:-:|---|---|:-:|---|
| I1 | Application 双 Document 槽 | `veloxa/core/application.h` `Document* document_;` | 🟡 需扩展 | 重命名 `document_` → `target_document_` + 加 `devtool_document_`（nullable）+ 加 `splitter_` 输入路由 |
| I2 | UpdateManager 五钩子 | `veloxa/core/render/update_manager.cc` `Update()` 末尾 | 🟢 已可访问 | 新增 `PipelineHooks` struct + `SetPipelineHooks()` API；钩子调用点 5 处插入 |
| I3 | DisplayList overlay 注入 | `veloxa/core/render/renderer.cc` 渲染末尾 | 🟢 已可访问 | 新增 `kOverlayHighlight` PaintCommand tag + 渲染顺序固定末位 + 每帧开头 `ResetOverlayCommands()` |
| I4 | LayoutBox.Dump | `veloxa/core/layout/layout_box.h` const 私有字段 | 🟡 需扩展 | 加 `BasicString ToJson() const` 方法（不修改字段，仅 read-only 序列化）|
| I5 | DOM Serializer.ToJson | `veloxa/core/dom/serializer.h` 已有 `Serialize` HTML 输出 | 🟢 已可访问 | 加 `BasicString ToJson(const Node*, RedactionPolicy)` 重载 |
| I6 | EventManager hover 选取 | `veloxa/core/event/event_manager.h` `HitTest()` + hover 状态 | 🟢 已可访问 | DevTool 注入 hover callback 即可，无需扩展 |
| I7 | inotify watcher | `veloxa/platform/` 当前无 file watcher | 🔴 需新建 | 新增 `veloxa/devtool/hot_reload/file_watcher_inotify.{h,cc}` |
| I8 | Hot Reload restyle 入口 | `veloxa/core/application.cc` `LoadCSS()` 已存在 | 🟢 已可访问 | `HotReloadManager::ProcessCssFileChange()` 调 `LoadCSS` 即可 |

每个注入点都已 grep 验证位置 + 评估改动范围。**关键风险点：I1 Application 双 Document 槽改造**是 DevTool 主线最大的架构变动，会影响 ~10-15 处现有代码（`document()` getter callsite + UpdateManager / EventManager 持引用 + 单测 fixture）。spec 阶段标 P1 风险，build 阶段独立 phase 前置改造。

### 5.3 数据流（Inspector hover 选取场景）

```
鼠标移动到目标 View 区域
       ↓
SDL2 SDL_MOUSEMOTION
       ↓
Sdl2EventLoop::PumpInputEvents → callback
       ↓
Application::HandleInput(VxInputEvent)
       ↓
splitter_.RouteToDocument(x, y) → target_document_
       ↓
target_document_.event_manager_.OnMouseMove(x, y)
       ↓
HitTest(x, y) → dom::Node* hovered_node
       ↓ [DevTool ON 时]
DevTool::Inspector::OnTargetHover(hovered_node)
       ↓
1. Serializer::ToJson(hovered_node) → JSON tree（更新 Inspector panel）
2. InjectHoverHighlight(target_document_, hovered_node) → PaintCommand
   ↓
target_document_.dirty_paint_commands_.Append(kOverlayHighlight, rect, color)
       ↓
下一帧 UpdateManager::Update()
   ↓ OnFrameStart
   ↓ OnAfterStyle (no-op for hover)
   ↓ OnAfterLayout (no-op for hover)
   ↓ OnAfterRender → 渲染 target 全部 commands
   ↓ Renderer 末位渲染 overlay commands（红框）
   ↓ OnFrameEnd → PerfOverlay 记录 FrameStats
       ↓
SDL_RenderPresent → 屏幕同时显示 target + overlay 红框 + DevTool panel JSON 更新
```

### 5.4 与 V3=A 自渲染（dogfood）路径的具体兑现

DevTool 是 Veloxa 的 **第二个 Document**：
- DevTool 用一个 HTML 文件描述布局（`devtool/inspector_panel.html`）
- DevTool 用 CSS 描述样式（`devtool/inspector_panel.css`）
- DevTool 用 JS 描述交互（`devtool/inspector_panel.js`）
- DevTool 通过 DomBindings 调用公开 C API 读取 target Document 状态
- DevTool 自渲染到 splitter dock 区域（同一 SDL2 window 的右侧 1/3 或下侧 1/3）

这是 Veloxa 引擎的「**自我应用样板**」— DevTool 跑通即证明引擎 HTML/CSS/JS 子集足够构建一个真实交互式 UI（虽然功能简单）。反向地，DevTool 在自渲染过程中暴露的引擎缺陷（layout 边界 / event 边界 / JS 异步 / image cache 命名空间冲突）即为下游 R3+ 候选任务的天然输入源。

## 6. 三件套实施优先级 + Plan Phase 划分（D1=B）

```
Phase A — Inspector（量级 Level 3，~15-20 子任务）
  A.0 前置改造（必做）
    - I1 Application 双 Document 槽（~10-15 callsite 改）
    - I4 LayoutBox.ToJson()（技术债 #26）
    - I5 DOM Serializer.ToJson()
    - I3 DisplayList overlay PaintCommand tag
  A.1 内部 C++ API（D7=C 第一层）
    - inspector_data.h Serializer / LayoutBox / ComputedStyle ToJson
  A.2 Inspector UI（自渲染）
    - inspector_panel.html / .css / .js
    - splitter dock 组件
    - DOM tree view + Style panel + Layout panel
  A.3 hover 选取 + DisplayList overlay
    - InjectHoverHighlight + ResetOverlayCommands
  A.4 公开 C API 薄封装（D7=C 第二层）
    - vx_view_serialize_dom_json / vx_node_get_*_json
    - vx_inspector_set_redaction_policy
  A.5 安全：T3 redact + T5 overlay 隔离 + T7 buffer 守卫

Phase B — Performance Overlay（量级 Level 2-3，~8-12 子任务）
  B.0 前置改造
    - I2 UpdateManager PipelineHooks 五钩子（技术债 #35）
  B.1 数据采集
    - perf_overlay.h FrameStats ring buffer + 滑动 60 帧
    - dirty rect collector
  B.2 HUD 渲染
    - 自渲染 HUD overlay（HTML+CSS in DevTool Document）
    - FPS 数字 + 4 bars + dirty rect 边框
  B.3 安全：T6 callback 1ms/frame budget + 单 instance 验证

Phase C — Hot Reload（量级 Level 3，~10-15 子任务）
  C.0 前置改造
    - 无（前两期已完成 Application 双 Document + 公开 C API）
  C.1 file watcher 抽象
    - file_watcher.h 抽象基类
    - file_watcher_inotify.{h,cc} Linux 实现 ~150-200 LOC
  C.2 增量重载
    - hot_reload_manager.{h,cc} CSS-only 路径
    - state preservation（scroll / hover）
  C.3 DevTool UI 状态指示器（自渲染小组件）
  C.4 安全：T2 路径穿越守卫（canonicalize + boundary check + max_size）
```

## 7. 安全威胁建模 T1-T8（D8=A 完整 + 扩展段占位）

| # | 威胁 | 子系统 | 风险 | mitigation 详细 |
|:-:|---|---|:-:|---|
| **T1** | JS REPL 任意 eval | Console（**§11 扩展段**） | 🟡 高 | 占位：未来 Console 引入时，eval 在隔离 JSRuntime + capability allowlist |
| **T2** | Hot Reload 路径穿越 | Hot Reload C.1 | 🔴 高 | 1) watcher root 必须绝对路径白名单（用户构造时显式 allowlist）<br>2) inotify mask 限定 `IN_MODIFY \| IN_CLOSE_WRITE`，**不**监听 `IN_CREATE`（防 atomic write + symlink 穿越）<br>3) 文件路径在 callback 内 `realpath()` canonicalize → 检查 prefix 仍在 allowlist 内（boundary check）<br>4) 文件大小上限默认 4 MiB CSS（沿用 R2.5 image_decoder max_size 模式）<br>5) symlink 跨 root 拒绝 + 安全日志（A11 验收） |
| **T3** | Inspector 敏感数据回显 | Inspector A.1 + A.4 | 🟡 中 | 1) `Serializer::ToJson(node, RedactionPolicy)` 默认 redact `<input type="password">` value 为 `[REDACTED]`<br>2) 公开 `vx_inspector_set_redaction_policy(policy)` 让用户扩展敏感字段（如 credit-card / email pattern）<br>3) JSON 输出 metadata 标 `"_local_only": true` 明示不应离机持久化<br>4) DevTool 默认禁止文件 / 网络写入路径（仅内存 / 屏幕） |
| **T4** | 远程调试 port 暴露 | CDP（**§11 扩展段**） | 🔴 高 | 占位：未来 CDP 引入强制 1) 默认 `127.0.0.1` loopback 不绑 0.0.0.0；2) 编译宏 `VX_DEVTOOL_REMOTE=ON` 显式启用；3) HMAC token 认证（token + nonce） |
| **T5** | DisplayList overlay 注入污染 | Inspector A.3 | 🟡 中 | 1) overlay 命令独立 `kOverlayHighlight` PaintCommand tag<br>2) 渲染顺序固定 = target Document 全部 commands → DevTool overlay commands（叠加在最末）<br>3) 每帧开头 `ResetOverlayCommands(target_doc)` 清除上一帧 overlay 命令 |
| **T6** | UpdateManager 钩子回调任意代码执行 | Performance Overlay B.0 | 🟡 中 | 1) callback 注册时验证 `instance_count == 1`（仅 DevTool 单 instance）<br>2) callback 内编译期禁止调用 mutation API（`const_cast` 检查 + runtime DCHECK）<br>3) callback 执行预算 1 ms/frame（与技术债 #44 QuickJS Interrupt 同源机制）；超时 abort frame + 错误日志 |
| **T7** | C API 扩展面 buffer overflow | DevTool C API D7=C 第二层 | 🟡 中 | 1) 双调用模式：第一次 `out_buf=NULL, out_len=NULL` 仅返回所需大小；第二次 caller alloc 后调用<br>2) 内部默认 max_size 16 MiB（DOM JSON）/ 1 MiB（ComputedStyle / LayoutBox JSON）；超限返回 `kOutOfMemory`<br>3) JSON serializer 走 BasicString growth 策略（已有，无新增）|
| **T8** | DevTool 共享容器 mutation propagation | D4=B 共享 Application | 🟡 中 | 1) 双 Document 严格独立 stylesheet / DOM tree（无共享 `shared_ptr`）<br>2) ImageCache 共享但 image handle namespace 隔离 `devtool:icon-...` prefix（技术债 #4 闭环）<br>3) Event dispatch 时区分 `source = kDevTool vs kTarget`，避免误读 ownership |

威胁建模与既有 systemPatterns 协同：
- T2 实施模式与 R2.5 `image_decoder.cc max_size` 守卫同源（`f >= max_size → kOutOfMemory`）
- T3 redact 策略与 systemPatterns「敏感数据保护」段一致
- T5/T6/T8 实施模式与「中央调度协议 + 两层事件模型」一致（DevTool 是注入到中心 pipeline 的 capability）

## 8. 测试策略

| Phase | 测试覆盖目标 | 主要测试文件 |
|:-:|---|---|
| A.0 | 前置改造单测（Application 双 Document / LayoutBox.ToJson / DOM Serializer.ToJson / DisplayList overlay tag） | `tests/core/application_test.cc` / `tests/core/layout/layout_box_test.cc` / `tests/core/dom/serializer_test.cc` / `tests/core/render/renderer_test.cc` |
| A.1 | inspector_data 内部 C++ API 单测（JSON schema 一致性 + redaction） | `tests/devtool/inspector/inspector_data_test.cc`（新建 dir） |
| A.3 | hover 选取 + DisplayList overlay 集成测试 | `tests/devtool/inspector/inspector_overlay_test.cc` |
| A.4 | 公开 C API JSON 输出单测（双调用模式 + max_size 守卫 = T7） | `tests/api/devtool_api_test.cc` |
| A.5 | T3 redact 单测 + T5 overlay 隔离单测 + T7 buffer 守卫单测 | 同上 |
| B.0 | UpdateManager 五钩子触发顺序单测（A7） | `tests/core/render/update_manager_hooks_test.cc` |
| B.1 | FrameStats ring buffer + 滑动 60 帧聚合单测（A6） | `tests/devtool/overlay/perf_overlay_test.cc` |
| B.2 | HUD 自渲染 smoke + dirty rect 收集单测（A8） | 同上 |
| B.3 | T6 callback budget abort 单测（A9） | 同上 |
| C.1 | inotify watcher 抽象单测 + canonicalize 守卫单测（T2 = A11） | `tests/devtool/hot_reload/file_watcher_test.cc` |
| C.2 | hot_reload_manager CSS restyle 集成测试（A10） + max_size 守卫（A12） | `tests/devtool/hot_reload/hot_reload_manager_test.cc` |
| 全局 | DevTool OFF 时 ctest 1062/1062 不回归（A13） + binary size 不变（A14） | 既有 + CI smoke |

预期新增 ~50-70 单测（每件套 ~15-25 个），分布在 `tests/devtool/`（新增）+ `tests/core/`（前置改造）+ `tests/api/`（C API 扩展）。

## 9. 风险登记 R1-R6

| # | 风险 | 概率 | 影响 | 缓解 |
|:-:|---|:-:|:-:|---|
| **R1** | Application 双 Document 槽改造 callsite 漏改（I1） | 🟡 中 | 🔴 高 | 1) build A.0 phase 前置 grep `application_->document()` 全 callsite；2) 重命名 `document_` → `target_document_` 让漏改在编译期暴露；3) 单测覆盖率 ≥ 90% 覆盖 callsite |
| **R2** | DevTool 自渲染（V3=A）反过来暴露引擎缺陷阻塞主线 | 🟡 中 | 🟡 中 | 1) DevTool UI 主屏布局尽量用「已验证 OK」CSS 子集（avoid flex 边角 + bidi + transform）；2) 缺陷暴露后立即列入 R3+ 任务，不阻塞 DevTool 主线（降级 UI 简化）|
| **R3** | UpdateManager 五钩子重构影响目标 View 性能 | 🟢 低 | 🟡 中 | 1) 五钩子 callback ABI 是 `function pointer + void* user_data`（避免虚函数调用）；2) 钩子 default = nullptr 时分支可被 branch predictor 优化；3) DevTool OFF 时 binary size 不变（A14）|
| **R4** | inotify 跨平台抽象层一期仅 Linux，未来扩展 macOS/Windows 时接口可能不兼容 | 🟡 中 | 🟢 低 | 1) `file_watcher.h` 抽象基类设计参考 efsw 库 ≥ 3 平台共有接口（不绑死 inotify 特性）；2) 跨平台扩展列入 spec §扩展段 P2 候选；3) 一期不暴露 inotify-specific API |
| **R5** | DevTool C API JSON 序列化性能影响 hover 选取响应（每次 hover 都序列化整个 ComputedStyle JSON ~10-50KB） | 🟡 中 | 🟡 中 | 1) hover 选取的 Inspector 用内部 C++ API（D7=C 第一层零拷贝）；2) C API JSON 走第二层薄封装，仅外部接入用（非 hover hot path）；3) ComputedStyle JSON 默认 lazy（仅用户点开 Style panel 时序列化）|
| **R6** | Hot Reload CSS-only 增量后 DOM 状态保留与 hover/focus 不一致（restyle 后 hover 状态可能丢） | 🟡 中 | 🟢 低 | 1) hot_reload_manager 在 restyle 前后 snapshot hover_target / focus_target / scroll_position；2) restyle 后恢复（按 dom::Node identity，restyle 不改变 Node tree）；3) creative #2 详细设计状态保留协议 |

## 10. 与既有 systemPatterns 兼容性（≥ 30 模式自我对照）

DevTool 是 Veloxa 自我应用，几乎所有既有 systemPatterns 都需自我验证。以下是 Top 关键兑现：

| 模式 | DevTool 兑现情况 | 验证点 |
|---|---|---|
| 「中央调度协议 UpdateManager」 | ✅ D6=B 五钩子注入是该模式的扩展，钩子归属 UpdateManager 而非分散注入 | I2 注入点位置 |
| 「两层事件模型」 | ✅ DevTool 自渲染 panel 自身复用三阶段冒泡（capture/target/bubble）— dogfood | inspector_panel.js 事件绑定 |
| 「allocator template pattern」 | ✅ DevTool 容器（DOM tree view 节点 / FrameStats ring buffer）走 ArenaAllocator | DevTool Document 持自己的 ArenaAllocator |
| 「Pimpl + JSContext opaque bridge」 | ✅ DevTool C API 双层（D7=C）符合 thin wrapper + 内部不暴露 | veloxa_api.h 扩展段 |
| 「敏感数据保护」 | ✅ T3 redact 策略对齐 | Serializer::ToJson(RedactionPolicy) |
| 「Background agent 双轨模式 + worktree 隔离」 | 🟡 不直接相关；DevTool 主线由用户主线推进 | — |
| 「plan ×0.6 任务类型分桶」 | ✅ 本任务 V2=a 纯蓝图，预期落「review 类 0.4-0.7×」（蓝图属于 review 子类）| reflection 阶段 plan ×0.6 第 17 数据点 |
| 「Quick fix 颗粒度估时基准 12 min/项」 | 🟡 不直接相关；本任务无 quick fix | — |
| 「Checkpoint 推荐默认 + 隐式批准协议」 | ✅ 用户两次跳过 AskQuestion → 按 VAN 推荐默认锁定（V1-V5 + D1-D8 共 13 项决策）| 全程 |
| 「Review 类任务 6 维度 × 抽样深度矩阵」 | 🟡 不直接相关；本任务非 review | — |
| 「最窄路径 + 极窄档 0.2-0.25×」 | 🟡 不直接相关；本任务是 Level 4 蓝图，非最窄路径 | — |
| 「Spec 实施模式描述粒度准则」 | ✅ 本 spec 12 段式样符合该准则（每段量级匹配 Level 4）| 自我检查 |
| 「既有测试隐式契约 fingerprint」 | ✅ build phase A.0 前置改造前必须 grep `application_->document()` 全 callsite | R1 mitigation |
| 「CSS shorthand 能力 grep 表」 | ✅ DevTool 自渲染 CSS 子集需 grep 验证（特别是 splitter dock 用的 flexbox / overflow / position）| Phase A.2 前置 |
| 「Mixed TDD D3 类回归测试默认含反向探针」 | ✅ DevTool 三件套测试 ≥ 5 项 D3 类回归（如 T6 callback budget / T2 symlink boundary / T5 overlay reset），每项必加反向探针 | Phase A.5 / B.3 / C.4 |
| 「递归算法 API 传递语义决策必检项」 | 🟢 低 — DOM tree view 有递归但 by-pointer trivial | — |
| 「FetchContent 网络代理守卫」 | ✅ 一期 D5=A 不引新依赖（自实现 inotify）→ 跳过守卫；扩展段 efsw 引入时触发 | Phase A.0 前置 |
| 「测试基础设施审计」 | ✅ DevTool 单测需访问 perf_overlay 内部 ring buffer / hot_reload_manager 内部状态 → 加 friend 测试类（白名单）| Phase B.1 / C.2 |
| 「边界输入清单」 | ✅ T2 路径穿越 / T7 buffer overflow / A12 max_size 都属边界输入测试范畴 | T2/T7 测试 |
| 「Render Bench 前置清单」 | 🟡 一期不做 perf bench；二期 Hot Reload + Overlay 落地后做 DevTool ON/OFF binary size + frame time A/B bench | 留 P3 候选 |
| 「Env Toggle A/B 对照模式」 | ✅ `VX_BUILD_DEVTOOL=OFF/ON` 编译时开关 + 运行时 `vx_view_attach_devtool()` 显式调用 = 双重 toggle | A14 验收 |
| 「同窗口对照 bench performance regression」 | 🟡 不直接相关；DevTool ON 时性能影响是预期的（不属退化）| — |
| 「Background agent 双轨 worktree 隔离工程指引」 | 🟡 不直接相关；本任务用户主线单轨推进 | — |
| 「ninja 时间戳偏差 — stash-swap 防护」 | 🟡 不直接相关；非 bench 任务 | — |
| 「编译器已做优化识别 — 位运算近似反模式」 | 🟢 不直接相关 | — |
| 「Layout 类任务必检项」 | 🟡 部分相关；DevTool 自渲染 panel 用现有 layout（不扩展），但需验证 fit-content / fill-available 边界 | Phase A.2 前置 |
| 「CMake 链接方向约束分析」 | ✅ DevTool 子系统作为新静态库 `vx_devtool` 需 grep 链接方向（与 vx_core / vx_script / vx_platform 之间）| Phase A.0 前置 plan §CMake 段必填 |
| 「静态库循环依赖审计」 | ✅ vx_devtool 调用 vx_core 公开符号但 vx_core 不调用 vx_devtool → 单向无循环 | 同上 |
| 「Web 标准 API 多重载形态清单」 | 🟡 不直接相关；DevTool C API 是 vx-prefixed 自定义而非 Web 标准 | — |
| 「commit 前防御断言（multi-session safety）」 | ✅ 全程遵守 `git symbolic-ref --short HEAD` 守门 | 全程 commit |
| 「并发会话 / 多 agent 冲突诊断（git reflog）」 | ✅ 已在 TASK-30-04 init 阶段实战应用（reflog 诊断双 reset 冲突）| TASK init |

## 11. 扩展段（V1=B 三件套外的候选子系统）

以下 3 个扩展子系统在本 spec 阶段仅做架构占位 + 威胁面提及，不展开详细设计。每个扩展子系统按用户后续独立立项（独立 Level 3-4 任务）：

### 11.1 Console（JS REPL + console.log 桥接）

- **依赖：** D2=B JSON 协议 + D7=C 双层 API
- **架构占位：** `veloxa/devtool/console/` + `console_panel.html/js`
- **威胁面：** T1 任意 eval —— Console 引入时强制：1) eval 在 isolated JSRuntime（独立 QuickJS context）；2) capability allowlist（默认仅 read-only DOM access，禁 file / network / Hot Reload trigger）
- **量级：** Level 3（依赖技术债 #44 QuickJS Interrupt Handler 先做）

### 11.2 JS Debugger（断点 / 单步 / 变量检查）

- **依赖：** Console 先做 + 技术债 #44 QuickJS Interrupt Handler
- **架构占位：** `veloxa/devtool/debugger/` + 调试器 UI panel
- **威胁面：** 与 T1 同源 + 调试器 attach 时暴露内部状态（敏感数据 redact 沿用 T3 策略）
- **量级：** Level 3-4 单独立项

### 11.3 完整 UI 编辑器（拖拽布局 + 反向写回 HTML/CSS）

- **依赖：** Inspector + Hot Reload + 双向数据绑定层（架构空白）
- **架构占位：** `veloxa/devtool/editor/` + 编辑器 UI（拖拽 + ruler + grid）
- **威胁面：** 反向写回需 T2 升级（写权限边界）+ 数据绑定需 T8 升级（mutation propagation 严格化）
- **量级：** Level 4+（双向绑定层是大件）单独立项

### 11.4 CDP（Chrome DevTools Protocol 远程调试）

- **依赖：** D7=C 第二层公开 C API JSON 已就绪 + WebSocket 库引入
- **架构占位：** `veloxa/devtool/cdp/` + `cdp_websocket_server.{h,cc}`
- **威胁面：** T4 远程 port 暴露 —— 强制 1) 默认 loopback only（127.0.0.1）；2) `VX_DEVTOOL_REMOTE=ON` 编译宏 + default off；3) HMAC token 认证（token + nonce）
- **量级：** Level 3-4 单独立项（依赖 D7=C 已铺好基础）

## 12. 与未来任务的关系

### 12.1 本 TASK-30-04 主交付（V2=a 纯蓝图）

- spec 主文档（本文件）
- plan 主文档（`docs/plans/2026-04-30-devtool.md`）
- creative #1（`memory-bank/creative/creative-devtool-screen-layout.md`）
- creative #2（`memory-bank/creative/creative-devtool-hot-reload.md`）
- 不进入 build；reflect / archive 收尾后任务闭环

### 12.2 用户后续独立立项 TASK 候选（基于本 plan 拆出）

| 候选任务 ID | 描述 | 量级 | 推荐顺序 |
|---|---|:-:|:-:|
| TASK-30-04-A | DevTool Phase A — Inspector 实施 | Level 3 | 1 |
| TASK-30-04-B | DevTool Phase B — Performance Overlay 实施 | Level 2-3 | 2 |
| TASK-30-04-C | DevTool Phase C — Hot Reload 实施（Linux only）| Level 3 | 3 |
| TASK-30-04-Ext-Console | 扩展段 Console 子系统 | Level 3 | 4（可推迟）|
| TASK-30-04-Ext-Debugger | 扩展段 JS Debugger 子系统 | Level 3-4 | 5（可推迟）|
| TASK-30-04-Ext-CDP | 扩展段 CDP 远程调试 | Level 3-4 | 6（可推迟）|
| TASK-30-04-Ext-Editor | 扩展段完整 UI 编辑器 | Level 4+ | 7（最远，依赖前 6 项）|

### 12.3 与 TASK-30-03（codebase review）R3+ 13 项 P1 候选交叉关系

| TASK-30-03 R3+ 候选 | 与 DevTool 主线关系 |
|---|---|
| #2 EventDispatcher snapshot iteration（F-046）| 弱依赖 — DevTool 注入 hover callback 时遵循 snapshot iteration 协议（避免 listener mutation UAF）|
| #3 LoadHTML 重置 dom_bindings_（F-025）| 强依赖 — Hot Reload C.2 增量重载触发 LoadHTML / LoadCSS 时必须前置闭环，否则 use-after-free 风险 |
| #4 CSS 属性元数据表（F-022）| 中依赖 — Inspector Style panel 显示 ComputedStyle 时复用元数据表 prop name → kind 映射；F-022 闭环可让 Inspector Style 展示更结构化 |
| 其余 10 项 | 弱依赖 / 无依赖 |

**强依赖（#3 F-025）建议优先级：** 在 DevTool Phase C（Hot Reload）实施前先单独立项 R3+ #3 LoadHTML 改造任务（Level 2，~1-2h）。本任务 spec 阶段已记录该依赖。

### 12.4 已识别但本 spec 不展开的细节

- DevTool UI 主屏布局协议详细（splitter 拖拽 / dock 模式切换 / overlay 透明合成）→ 见 `creative-devtool-screen-layout.md`
- inotify 抽象层接口设计 + CSS-only 增量重载状态保留协议 → 见 `creative-devtool-hot-reload.md`
- 每个 phase 的具体子任务步骤 1-N（TDD RED/GREEN/REFACTOR）→ 见 `plans/2026-04-30-devtool.md`

---

**文档结束**。审查者请重点关注：
1. §3 验收 A1-A12 是否覆盖三件套核心使用场景
2. §4 决策矩阵 D1-D8 是否有疏漏（特别是用户实际开发体验最痛点）
3. §5.2 注入点核对表是否全部 grep 验证（I1-I8）
4. §7 威胁面 T1-T8 mitigation 是否充分（T2 路径穿越 / T6 callback budget / T7 buffer overflow 是核心三项）
5. §10 与 ≥ 30 systemPatterns 自我对照是否漏掉关键模式

---

## 附录 A：A14 解读 — 链接闭合 vs binary size diff 双层语义

> 来源：TASK-20260502-01 A.1.7/A.1.8 实证 + reflection §6 P1 #8 沉淀；TASK-20260503-02 落实。

### A.1 A14 双层语义区分

A14「DevTool 关闭时构建产物零变化（链接闭合 + binary size diff = 0）」实际包含两层独立语义：

| 层 | 名称 | 验证方法 | 严格度 | 失败后果 |
|:-:|---|---|:-:|---|
| **L1** | 链接闭合（Hard Constraint）| `tests/smoke/devtool_a14_link_closure.cmake` 用 `nm` 黑名单符号验证 DevTool 内部符号零泄漏到 `DEVTOOL=OFF` binary | 🔴 强制 | A14 违规，必须修复（#ifdef 守门 + CMakeLists.txt 条件 link） |
| **L2** | binary size diff（Soft Indicator）| `cmake --build` + `ls -l <binary>` size 对比 `git stash` 前后 | 🟡 软指示 | 由公开 stub 引起 → 合规；由 leak 引起 → 升级为 L1 违规 |

### A.2 L1 链接闭合（严格契约）

- 黑名单符号定义在 `tests/smoke/devtool_a14_link_closure.cmake`
- 每个 DevTool 子系统新增 1+ 符号到黑名单（如 Phase A 8 项 / Phase B 2 项 / Phase C 3 项）
- ctest 自动 nm 验证：`DEVTOOL=OFF` binary 中黑名单符号必须 0 命中
- **L1 PASS = A14 主契约满足**

### A.3 L2 binary size diff（软指示）

- A.1.7/A.1.8 实证：`vx_view_attach_devtool` always-fail stub（OFF 路径返 `VX_ERROR_INVALID_STATE`）+ `vx_view_get_perf_stats` 等 6 个 C ABI stub 累计 +4196 bytes 在 OFF binary 中
- 这些 bytes 不是 DevTool 实现 leak，而是**公开 C ABI 表面的 stub 占位**（保持 ABI 兼容性 — OFF binary 仍可 link 调用方代码，只是返错码）
- **L2 > 0 由公开 stub 引起 = 合规**；L2 > 0 由实现 leak 引起 = L1 违规升级

### A.4 区分公式

```
A14 严格 = L1 PASS（链接闭合）AND L2 ≥ 0（diff 由公开 stub 解释）
```

判读流程：

1. ctest A14 link closure smoke → L1 PASS/FAIL
2. L1 FAIL → 直接 A14 违规，修复 #ifdef + CMakeLists.txt
3. L1 PASS + L2 = 0 → 理想 A14（无新公开 stub）
4. L1 PASS + L2 > 0 → 验证 L2 增量来源：
   - `nm <binary> | grep vx_` 确认增量符号是公开 C ABI（前缀 `vx_`）
   - 公开 stub → 合规
   - 内部符号泄漏 → 升级为 L1 违规

### A.5 实证

- **TASK-20260502-01 Phase A**：A.1.7 + A.1.8 累计 +4196 bytes（490 + 3706）；L1 PASS（黑名单 8 项 nm 0 命中）；L2 增量全部来源 7 个公开 C ABI stub → 合规
- **TASK-20260502-02 Phase B**：黑名单 +2 项（PerfOverlay + InjectDirtyRectHighlights）；L1 PASS；L2 ~+800 bytes 来源新 stub → 合规
- **TASK-20260503-01 Phase C**：黑名单 +3 项（FileWatcher + InotifyFileWatcher + HotReloadManager）；L1 PASS；L2 ~+900 bytes 来源 `vx_view_hot_reload_tracked_count` + `vx_view_attach_devtool(hot_reload_dir)` 扩展 → 合规

### A.6 反模式

- ❌ 把 L2 = 0 当作 A14 唯一判据 → 误判公开 stub 为违规
- ❌ 仅看 L1 不看 L2 → 错过 leak 升级信号（L2 > 0 + 来源不是公开 ABI 时）
- ❌ A14 验证仅在 reflect 阶段做 → 应在每个子任务 finalize 时由 ctest A14 smoke 守门

### A.7 交叉引用

- `tests/smoke/devtool_a14_link_closure.cmake` — L1 自动化守门
- `memory-bank/systemPatterns.md`「子系统关闭守门 ctest smoke 范式」段 — 范式总结
- `memory-bank/systemPatterns.md`「黑名单维护演进透明度」子段 — 黑名单更新协议
