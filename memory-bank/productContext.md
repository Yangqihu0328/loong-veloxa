# 产品上下文

## 解决的问题

1. 车载/嵌入式领域缺乏轻量级的 HTML/CSS 渲染引擎
2. 现有方案（CEF/Chromium）对嵌入式系统资源需求过高
3. Qt/QML 许可证限制与体积问题
4. Sciter 虽轻量但闭源且非嵌入式优化

## 竞品分析


| 方案               | 优势                  | 劣势（对嵌入式）                |
| ---------------- | ------------------- | ----------------------- |
| Chromium/CEF     | 完整 Web 兼容           | 体积 100MB+，内存 200MB+，启动慢 |
| Qt/QML           | 成熟工具链               | GPL/商用许可，体积大，学习曲线       |
| Sciter           | 轻量 5MB，HTML/CSS     | 闭源，非嵌入式优化               |
| LVGL             | 嵌入式优化               | 非 Web 技术栈，UI 表达力有限      |
| Flutter Embedded | 跨平台                 | Dart 生态，体积/内存偏大         |
| **Veloxa（目标）**   | 轻量 + Web 子集 + 嵌入式优化 | 需从零构建                   |


## 目标场景

1. **车载中控** — 导航、媒体、空调控制界面
2. **车载仪表盘** — 转速/速度/电量等数据仪表
3. **工业 HMI** — 工厂设备控制面板
4. **IoT 面板** — 智能家居控制屏
5. **医疗设备** — 监护仪/诊断设备界面

## 核心用户旅程

1. 开发者用 HTML/CSS/JS 编写 HMI 界面
2. 用 Veloxa 工具链预编译/打包资源
3. 通过 C API 将 Veloxa 引擎集成到嵌入式应用
4. 引擎在目标硬件上渲染 60fps 流畅界面
5. 应用通过 API 与 UI 双向通信（数据绑定/事件回调）

## 已实现的核心功能（截至 TASK-20260414-01）


| 功能                       | 状态  | 备注                                                                                                                               |
| ------------------------ | --- | -------------------------------------------------------------------------------------------------------------------------------- |
| HTML 解析器                 | ✅   | 子集 HTML5，隐式关闭规则                                                                                                                  |
| CSS 引擎（~45 属性）           | ✅   | 选择器匹配、层叠、继承                                                                                                                      |
| Block/Inline/Flex 布局     | ✅   | CSS Flexbox Level 1 §9                                                                                                           |
| 软件渲染器                    | ✅   | 覆盖率光栅化、Display List                                                                                                              |
| 事件系统                     | ✅   | W3C DOM Events 三阶段，:hover/:active/:focus                                                                                         |
| 脏区更新                     | ✅   | DisplayList diff                                                                                                                 |
| CSS Transitions          | ✅   | 单属性过渡                                                                                                                            |
| C API                    | ✅   | 不透明指针 ABI                                                                                                                        |
| QuickJS 脚本引擎             | ✅   | EvalGlobal、32MiB 内存限制                                                                                                            |
| border-radius 渲染         | ✅   | FillRoundedRect + StrokeRoundedRect                                                                                              |
| FreeType + HarfBuzz 字体渲染 | ✅   | 真实字形光栅化、GlyphCache                                                                                                               |
| PNG/JPEG 图片支持            | ✅   | 解码 + 缓存 + DrawImage                                                                                                              |
| JS-DOM 绑定                | ✅   | getElementById、属性、style proxy、事件                                                                                                 |
| **SDL2 实时窗口后端**          | ✅   | TASK-20260425-01；`Sdl2WindowSurface` + `Sdl2EventLoop`；WSLg/桌面真窗口 + 输入事件桥接；为 hot-reload / inspector / FPS overlay 等 DevTool 解锁前置 |


## 非目标

- 不做完整浏览器（不支持完整 HTML5 规范）
- 不做通用桌面 UI 框架（焦点在嵌入式）
- 不支持 WebRTC/WebGL 等重型 Web API

## 最近能力更新（2026-04-30）

- ✅ **Layout 正确性显著提升（TASK-20260426-01）**
  - 完成 Block margin collapsing（CSS 2.1 §8.3.1）
  - 完成 Inline line box / vertical-align / line-height 半-leading（CSS 2.1 §10.8）
  - HTML inline style 解析路径与 JS 路径一致（含三件套安全护栏）
  - 对 DevTool 主线（Inspector / 几何可视化 / 后续 z-index/IFC 扩展）提供更可靠几何基础

## 最近能力更新（2026-05-02）

- ✅ **DevTool Phase A · Inspector 主线落地（TASK-20260502-01）** 🎉
  - **公开 C API 7 个新函数**：vx_view_attach_devtool / detach_devtool / devtool_loaded / vx_view_serialize_dom_json / vx_inspector_set_redaction_policy + VxRedactionPolicy enum + VxDevtoolOptions struct
  - **F12 hotkey 内化** — Application 层拦截 KeyDown(F12) 自动 toggle DevTool 可见性
  - **dogfood 完整链路** — Veloxa 引擎驱动 DevTool UI（panel.html/css/js + JS native binding `vx_devtool_get_dom_json()` 跨 Document inspection）
  - **`examples/hello_devtool.cc`** — SDL2 真窗口 + DevTool attach + F12 toggle + T3 redaction 运行时验证 user-facing 范例
  - **4 安全威胁面 mitigation**：T2 路径穿越（编译期嵌入消除）+ T3 redaction policy + T5 overlay 隔离 + T7 buffer overflow 守卫
  - **A14 链接闭包零自动化守门** — 任何后续 DevTool 子系统扩展自动验证零字节增长（pre-merge ctest smoke 强制守门）
  - **2 项历史技术债闭环**（#26 LayoutBox.Dump / #40 C API DOM introspection）
  - **3 项 R2 引擎缺陷暴露 → P3 候选**（DomBindings.Element.children / addEventListener / innerHTML setter）— P3 修复后 DevTool UI 视觉完整 + 第三方 DevTool-like 工具可在 Veloxa 上实现
  - **下一步路线图**：TASK-30-04-B Performance Overlay（估时下调 ~30% 至 ~3.5-5 h）+ TASK-30-04-C Hot Reload（估时下调 ~20% 至 ~2.5-4 h）+ DomBindings R2 P3 修复（~3-5 h plan ×0.6）

- ✅ **DevTool Phase B · Performance Overlay 主线落地（TASK-20260502-02）** 🎉
  - **公开 C API 4 个新函数 / 宏**：`vx_view_set_pipeline_hooks` + `vx_view_get_perf_stats`（JS native binding 形式，不直接公开 C 函数）+ `vx_view_is_hud_visible` + `VX_KEY_F11` 宏 + `VxPipelineHooks` struct（5 callback 字段 + userdata）+ `VxPipelineCallback` typedef
  - **F11 hotkey 内化** — Application 层拦截 KeyDown(F11) 自动 toggle HUD 可见性（仅 DevTool attached 时生效；UnloadDevtoolDocument 自动 reset hud_visible_=true 防泄漏）
  - **PipelineHooks 五钩子机制** — UpdateManager 暴露 5 个 frame lifecycle 钩子（OnFrameStart / OnAfterStyle / OnAfterLayout / OnAfterRender / OnFrameEnd）+ lazy-attach 设计（caller 不需要先 LoadHTML）；**技术债 #35 阶段 1 闭环 ✅**（阶段 2 拆 LayoutEngine 内 style/layout 留 P3）
  - **PerfOverlay 子系统** — `vx::devtool::overlay::PerfOverlay` 新子系统：FrameStats 5 字段（style_ms/layout_ms/render_ms/paint_ms/total_ms）+ 60 帧 ring buffer + 滑动平均聚合 + FPS 计算守卫（除零 + 999 cap）+ T6 mitigation 三层（单 instance Attach + budget=0 显式短路 + 1ms/frame budget guard 累加 abort + abort_count + last_abort_reason）
  - **HUD overlay UI** — `#devtool-hud` absolute + opacity 0.85 + 4 stage bars（S/L/R/P style/layout/render/paint）+ FPS row + R8 fallback `position: absolute`（视觉等价 fixed）+ R9 占位 `data-passthrough="1"`（pointer-events 真支持留 R3+）
  - **dirty rect 边框可视化** — `PaintCommand::OverlayDirtyRect` 工厂复用 `kOverlayHighlight` 类型（不新增 enum）+ `InspectorOverlay::InjectDirtyRectHighlights` 静态方法 + T5 mitigation 复用 ResetOverlayCommands 协议（不扩增 reset 路径）
  - **`examples/hello_devtool.cc` 增强** — 新增 PerfHooks 安装段 + lazy-attach 实证 + `PERF SMOKE: frames=N hud_visible=M` 稳定 print + `hello_devtool_perf_smoke` ctest（PASS_REGULAR_EXPRESSION）
  - **2 安全威胁面 mitigation**：T5（DisplayList overlay 跨帧累积 — 复用 ResetOverlayCommands 协议）+ T6（callback 任意代码执行 — 三层 guard）
  - **A14 链接闭包黑名单 +2 项**（PerfOverlay + InjectDirtyRectHighlights 内部符号）+ Phase A/B 区分注释 + NOT in blacklist intentional 排除项清单
  - **3 项 R2/R3 P3 候选**：#35 阶段 2 拆 LayoutEngine / R9 EventManager HitTest 改造（HUD pointer-events 真支持）/ hello_devtool_perf_smoke 多帧验证（调 SDL2 dummy 帧率）
  - **效率指标**：~104 min 主线（vs plan ×0.6 261 min = **0.40×** 极窄档延续高效区候选新子档）— Phase 0 投入越深 / build phase 越快定律 dual-evidence 第二次实证（ROI 5.2× 接近 Phase A 5.3×）；连续两次任务 0/7 反复模式命中
  - **下一步路线图**：TASK-30-04-C Hot Reload（估时进一步下调 ~20% 至 ~2-3 h plan ×0.6 — 受益于 Phase A + Phase B 5 大范式 + 5 大 ROI 复用）+ DomBindings R2 P3 修复（~3-5 h plan ×0.6）+ #35 阶段 2 拆 LayoutEngine（~2-3 h plan ×0.6）

## 最近能力更新（2026-05-03）

- ✅ **DevTool Phase C · Hot Reload 主线落地（TASK-20260503-01）— DevTool 三件套主线收官 🎉🎉🎉**
  - **公开 C API 2 个新增 / 扩展**：`vx_view_attach_devtool` 加 `hot_reload_dir` 参数（hot reload 启用入口）+ `vx_view_hot_reload_tracked_count`（hot reload 计数 read 接口，dogfood smoke 验证用）
  - **新错误码 / 警告码**：`VX_WARNING_HOT_RELOAD_FAILED = 1`（warning 语义层 — hot reload attach 失败但 DevTool 主功能继续工作的非阻塞警告，lazy-attach C ABI 模式扩展）
  - **新子系统** `vx::devtool::hot_reload::{FileWatcher, InotifyFileWatcher, HotReloadManager}`：
    - **FileWatcher 抽象基类** — 跨平台抽象层 + Platform factory `#if defined(__linux__)` + 非 Linux 平台 nullptr 退化（macOS kqueue / Windows ReadDirectoryChangesW 留 P3 候选）
    - **InotifyFileWatcher Linux 实现** — ~268 LOC inotify_init1(IN_CLOEXEC | IN_NONBLOCK) + IN_MODIFY/IN_CLOSE_WRITE only（不监听 IN_CREATE 防 atomic+symlink 穿越）+ std::thread watch loop + thread-safe queue（mutex + condition_variable + atomic running_）+ sleep_for(50ms) 兜底 read EAGAIN 路径 + 50ms 事件 debouncing
    - **HotReloadManager** — CSS-only 增量重载 + brace imbalance 启发式 CSS 解析失败检测 + Application::LoadCSS 触发 restyle + R9 F-025 不踩契约（仅识别 .css 文件 + 仅调 LoadCSS / 不调 LoadHTML / 反向探针单测验证）
  - **T2 路径穿越 8 步守卫完整实施**（高威胁面 dual-probe 16 测全覆盖）：absolute root allowlist + locked inotify mask（IN_MODIFY/IN_CLOSE_WRITE only）+ realpath canonicalization + canonical path boundary check + max_file_size 4 MiB 上限 + .css extension filter + WARN-level logging + 50ms event debouncing
  - **DevTool UI 状态指示器** — inspector_panel.html `#hot-reload-status` div + inspector_panel.css `.status-watching`（绿）/`.status-error`（红）+ inspector_panel.js `updateHotReloadStatus()` 函数 + `vx_devtool_get_hot_reload_status` JS native binding（返 `{tracked_count, last_error}` JSON）
  - **`examples/hello_devtool.cc` 第三次扩展** — 新增 hot reload smoke（`VX_HELLO_DEVTOOL_HOT_RELOAD_TEST` env var 触发 mkdtemp temp dir → 初始 `body { background-color: red; }` write → SDL timer 100ms 触发 modify 为 blue → 读 `vx_view_hot_reload_tracked_count` print `HOT RELOAD: triggered count=N` → cleanup）+ `hello_devtool_hot_reload_smoke` ctest 端到端 0.87s 验证（PASS_REGULAR_EXPRESSION `count=1`）
  - **A14 链接闭包黑名单 +3 项**（FileWatcher / InotifyFileWatcher / HotReloadManager 内部符号）+ Phase A/B/C 区分注释 + 公开 C ABI（vx_view_hot_reload_tracked_count）+ anonymous namespace local symbols（VxDevtoolGetHotReloadStatus）排除注释
  - **效率指标**：~104 min 主线（vs plan ×0.6 333 min = **0.31×** 极窄档加速衰减区下沿候选新子档）— Phase 0 投入越深 / build phase 越快定律 **triple-evidence 升级**（A 5.3× / B 5.2× / **C 7.6× ROI**）；三任务连续递降趋势：A 0.64× → B 0.40× → C 0.31×；5 大可复用范式 100% 命中第三次连续生效；lazy-attach C ABI 模式扩展（warning 语义层新沉淀）；反复模式 1/7 部分命中（前置组件能力假设未实测 — 已识别因子 + P1 改进建议闭环）
  - **额外事件**：build §0.1 baseline 二次验证遭遇 binutils 2.46+ ld 单次扫描静态归档严格化阻塞 → `hotfix/binutils-2.46-link-group` 单独分支修复（根 CMakeLists.txt +10 行 `--start-group/--end-group` 包围 LINK_LIBRARIES）→ fast-forward to main `ddc1e3c` → feature 分支 rebase 上 main → 进入 C.0.1 RED 阶段；「baseline 阻塞 hotfix 分离协议」首次实证 + R12「工具链版本激进升级」风险登记
  - **DevTool 三件套主线收官** — Inspector + Performance Overlay + Hot Reload 完整闭环 ✅
  - **下一步路线图**：DevTool 三件套已完整收官；后续候选见 activeContext 待处理事项 §「Phase C 完成后 P3 候选」段：#35 阶段 2 拆 LayoutEngine + R9 EventManager HitTest 改造 + DomBindings R2 三连补全 + R3+ #1 image_decoder 安全三件套 + Phase D/E/F/G 扩展段（Console JS REPL / JS Debugger / CDP 远程 port / 完整 UI Editor）按需独立立项

