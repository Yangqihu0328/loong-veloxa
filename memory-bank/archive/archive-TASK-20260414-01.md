# 归档：功能补全（border-radius / 字体渲染 / 图片支持 / JS-DOM 绑定）

**日期：** 2026-04-14
**任务 ID：** TASK-20260414-01
**复杂度级别：** Level 4
**状态：** ✅ 已完成
**分支：** `feature/TASK-20260414-01-feature-completion`

## 任务概述

为 Veloxa 轻量 HTML/CSS 渲染引擎补全四项核心功能，使其从骨架引擎向可用 HMI 引擎迈进：
1. **border-radius 渲染** — CSS 圆角从 ComputedStyle 接入渲染管线
2. **FreeType + HarfBuzz 字体渲染** — 替换 DrawText 存根为真实字形光栅化
3. **PNG/JPEG 图片支持** — `<img>` 标签从解码到像素渲染的完整链路
4. **QuickJS DOM 绑定** — JS 脚本通过 `document.getElementById` 操作 DOM 和监听事件

## 技术方案

### 设计决策

| 决策点 | 选择 | 理由 |
|--------|------|------|
| 字体加载策略 | 宿主显式加载（`vx_view_load_font`） | 嵌入式场景宿主控制资源路径，简单可预测 |
| 图片资源加载 | 文件路径同步加载 | HMI 资源预部署，无需异步；MVP 简化 |
| DOM 绑定范围 | 查询 + 事件（B 方案） | 覆盖核心 HMI 交互（读 DOM + 改样式 + 监听事件），不过度复杂化 JS 侧生命周期 |

### 架构特点

- **可选依赖注入 + 退化模式**：所有新能力通过可选指针注入（FontManager*、GlyphCache*、ImageCache*），无注入时退化为旧行为，842 个现有测试零修改通过
- **CMake 循环依赖解决**：vx_script 不链接 vx_core（仅 include path），vx_core PRIVATE 链接 vx_script，由最终可执行文件统一提供符号

## 实现摘要

**变更规模：** 52 文件，+3593/-40 行，9 个提交，842/842 测试通过（新增 37 个）

### 文件变更

| 操作 | 文件路径 | 说明 |
|------|---------|------|
| **Iteration 1: border-radius** | | |
| 修改 | `veloxa/core/render/paint_command.h` | kFillRoundedRect、kStrokeRoundedRect、param2 字段 |
| 修改 | `veloxa/core/render/renderer.cc` | RecordBox 圆角分支 + Replay 新 case |
| 创建 | `tests/core/render/border_radius_test.cc` | 8 个测试 |
| **Iteration 2: 字体渲染** | | |
| 创建 | `veloxa/text/CMakeLists.txt` | vx_text 库（FreeType + HarfBuzz） |
| 创建 | `veloxa/text/font_manager.h/.cc` | Init/Shutdown/LoadFont/FindFont/GetFace |
| 创建 | `veloxa/text/glyph_cache.h/.cc` | HashMap 缓存，(font, glyph_id, pixel_size) → GlyphBitmap |
| 创建 | `veloxa/text/freetype_shaper.h/.cc` | HarfBuzz 塑形 + FreeType metrics，退化为 SimpleTextShaper |
| 修改 | `veloxa/graphics/software/software_canvas.h/.cc` | DrawText 真实字形渲染（有 FontManager 时）+ DrawTextFallback |
| 修改 | `veloxa/core/application.h/.cc` | FontManager/GlyphCache 生命周期、LoadFont 方法 |
| 修改 | `veloxa/api/veloxa_api.h/.cc` | `vx_view_load_font` C API |
| 创建 | `tests/text/{font_manager,glyph_cache,freetype_shaper}_test.cc` | 14 个测试 |
| **Iteration 3: 图片支持** | | |
| 创建 | `veloxa/graphics/image.h/.cc` | Image 类（RGBA32 像素存储） |
| 创建 | `veloxa/core/image/image_decoder.h/.cc` | PNG (libpng) + JPEG (libjpeg-turbo) 解码 |
| 创建 | `veloxa/core/image/image_cache.h/.cc` | 路径去重缓存 |
| 修改 | `veloxa/core/layout/layout_box.h` | kReplaced 类型 + image_handle 字段 |
| 修改 | `veloxa/core/layout/layout_engine.cc` | `<img>` 标签检测 → ImageCache 加载 → intrinsic 尺寸 |
| 修改 | `veloxa/graphics/canvas.h` | DrawImage 虚方法 |
| 修改 | `veloxa/graphics/software/software_canvas.cc` | 最近邻缩放 + SrcOver 混合 |
| 修改 | `veloxa/core/render/paint_command.h` | kDrawImage + image_handle |
| 修改 | `veloxa/core/render/renderer.h/.cc` | Record/Replay 传入 ImageCache* |
| 创建 | `tests/core/image/{image_decoder,image_cache}_test.cc` | 6 个测试 |
| **Iteration 4: DOM 绑定** | | |
| 修改 | `veloxa/core/dom/element.h/.cc` | inline_decls_ + SetInlineDeclaration |
| 修改 | `veloxa/core/layout/layout_engine.cc` | 传递 inline_declarations 给 StyleResolver（修复技术债 #28） |
| 修改 | `veloxa/script/quickjs_engine.h` | context() 访问器 |
| 创建 | `veloxa/script/dom_bindings.h/.cc` | 560 行完整 JS-DOM 桥接 |
| 修改 | `veloxa/core/application.h/.cc` | LoadScript + script_engine_ + dom_bindings_ |
| 修改 | `veloxa/api/veloxa_api.h/.cc` | `vx_view_load_script` C API |
| 创建 | `tests/script/dom_bindings_test.cc` | 12 个测试 |

### 关键决策

1. **Canvas::DrawText 签名不变**：原计划增加 FontHandle 参数，实际改为 SoftwareCanvas 内部持有可选 FontManager*，避免 Canvas 接口对 text 层耦合
2. **Renderer 可选参数 vs RenderContext 结构体**：选择直接加 `ImageCache* = nullptr` 可选参数，比引入新结构体更简单
3. **Element inline_declarations 直接在 Element 上扩展**：虽然引入了 CSS 类型到 DOM 层，但比引入中间抽象更直接

### 安全决策

本任务不涉及认证/授权/数据保护。新增 4 个系统库依赖（FreeType、HarfBuzz、libpng、libjpeg-turbo），均为成熟开源库，未执行 CVE 扫描（记录为待办）。JS→C++ 桥接有基本的 NULL 检查。

## 测试覆盖

| 模块 | 新增测试数 | 覆盖范围 |
|------|-----------|---------|
| PaintCommand | 4 | 工厂函数、相等性 |
| BorderRadius Render | 4 | Record 分支（有/无 radius）、Replay 像素验证 |
| FontManager | 5 | Init/Shutdown、加载、查找、GetFace |
| GlyphCache | 4 | 缓存命中/未命中、独立性、清空 |
| FreeTypeTextShaper | 5 | 正维度、缩放、空文本、字号缩放、回退 |
| ImageDecoder | 3 | PNG 解码、无效路径、无效数据 |
| ImageCache | 3 | 加载、去重、无效路径 |
| DomBindings | 12 | getElementById、tagName/id/textContent、getAttribute/setAttribute、style proxy、addEventListener（4 个事件测试） |
| Application | 1 | LoadFont + 文本渲染像素验证 |
| **总计** | **37** | 842/842 全量通过 |

## 经验教训

1. **子代理 prompt "文件内容嵌入 + 禁止修改清单" 策略成熟**：5 个子代理零返工，是项目历史上首次 Level 4 任务全程无子代理返工
2. **CMake 循环依赖需在 /plan 阶段预判**：新增 P1 待办到计划模板
3. **非默认路径仍有遗漏**（style getter 空实现、removeEventListener 简化）：反复模式第 5+ 次出现
4. **新增 6 项技术债（#45-#50）**：dom_bindings 全局状态、style 读路径、hb_font 缓存、事件回调生命周期等

## 长期影响

- **渲染能力大幅提升**：引擎从"矩形+存根文本"跃迁为"圆角+真实字形+图片"，可呈现基本 HMI 界面
- **JS 交互能力建立**：宿主或脚本可通过 `document.getElementById` 动态修改 DOM 和样式，满足 HMI 数据绑定需求
- **新增 2 个模块依赖**：vx_text（FreeType+HarfBuzz）和系统级 PNG/JPEG 库，嵌入式部署需确保目标平台有这些库
- **inline style 通路打通**：修复技术债 #28 后，CSS inline style 和 JS style proxy 都能正常工作

## 参考文档

- 设计规格：`docs/specs/2026-04-14-feature-completion-design.md`
- 实现计划：`docs/plans/2026-04-14-feature-completion.md`
- 回顾文档：`memory-bank/reflection/reflection-TASK-20260414-01.md`
