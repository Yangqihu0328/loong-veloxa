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

