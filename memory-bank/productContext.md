# 产品上下文

## 解决的问题
1. 车载/嵌入式领域缺乏轻量级的 HTML/CSS 渲染引擎
2. 现有方案（CEF/Chromium）对嵌入式系统资源需求过高
3. Qt/QML 许可证限制与体积问题
4. Sciter 虽轻量但闭源且非嵌入式优化

## 竞品分析

| 方案 | 优势 | 劣势（对嵌入式） |
|------|------|-----------------|
| Chromium/CEF | 完整 Web 兼容 | 体积 100MB+，内存 200MB+，启动慢 |
| Qt/QML | 成熟工具链 | GPL/商用许可，体积大，学习曲线 |
| Sciter | 轻量 5MB，HTML/CSS | 闭源，非嵌入式优化 |
| LVGL | 嵌入式优化 | 非 Web 技术栈，UI 表达力有限 |
| Flutter Embedded | 跨平台 | Dart 生态，体积/内存偏大 |
| **Veloxa（目标）** | 轻量 + Web 子集 + 嵌入式优化 | 需从零构建 |

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

| 功能 | 状态 | 备注 |
|------|------|------|
| HTML 解析器 | ✅ | 子集 HTML5，隐式关闭规则 |
| CSS 引擎（~45 属性） | ✅ | 选择器匹配、层叠、继承 |
| Block/Inline/Flex 布局 | ✅ | CSS Flexbox Level 1 §9 |
| 软件渲染器 | ✅ | 覆盖率光栅化、Display List |
| 事件系统 | ✅ | W3C DOM Events 三阶段，:hover/:active/:focus |
| 脏区更新 | ✅ | DisplayList diff |
| CSS Transitions | ✅ | 单属性过渡 |
| C API | ✅ | 不透明指针 ABI |
| QuickJS 脚本引擎 | ✅ | EvalGlobal、32MiB 内存限制 |
| border-radius 渲染 | ✅ | FillRoundedRect + StrokeRoundedRect |
| FreeType + HarfBuzz 字体渲染 | ✅ | 真实字形光栅化、GlyphCache |
| PNG/JPEG 图片支持 | ✅ | 解码 + 缓存 + DrawImage |
| JS-DOM 绑定 | ✅ | getElementById、属性、style proxy、事件 |
| **SDL2 实时窗口后端** | ✅ | TASK-20260425-01；`Sdl2WindowSurface` + `Sdl2EventLoop`；WSLg/桌面真窗口 + 输入事件桥接；为 hot-reload / inspector / FPS overlay 等 DevTool 解锁前置 |

## 非目标
- 不做完整浏览器（不支持完整 HTML5 规范）
- 不做通用桌面 UI 框架（焦点在嵌入式）
- 不支持 WebRTC/WebGL 等重型 Web API
