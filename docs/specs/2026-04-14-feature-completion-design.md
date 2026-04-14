# 功能补全设计规格

**任务 ID：** TASK-20260414-01  
**日期：** 2026-04-14  
**复杂度：** Level 4

---

## 概述

四项功能补全，按迭代顺序：

1. **border-radius 渲染** — 将已解析的 CSS border-radius 接入渲染管线
2. **FreeType + HarfBuzz 字体渲染** — 替换 SimpleTextShaper / DrawText 存根
3. **图片支持** — `<img>` 全链路（解码 → 布局 → 渲染）
4. **QuickJS DOM 绑定** — JS ↔ DOM 桥接（查询 + 事件）

## 设计决策

| 决策点 | 选择 | 原因 |
|--------|------|------|
| 字体加载策略 | 宿主显式加载 | 嵌入式最可控，零隐式依赖 |
| 图片资源加载 | 文件路径同步加载 | 嵌入式 HMI 资源预部署，最简实现 |
| DOM 绑定范围 | 查询 + 事件 | 覆盖 HMI 核心场景，避免 DOM 操控的 GC 复杂度 |

---

## Iteration 1：border-radius 渲染

### 现状
- `ComputedStyle.border_radius`（LengthValue）已解析
- `Canvas::FillRoundedRect` / `StrokeRoundedRect` 已实现
- `renderer.cc` 忽略 border_radius，始终使用 FillRect

### 方案
- `PaintCommand` 新增 `kFillRoundedRect`、`kStrokeRoundedRect` 类型
- `RecordBox()` 检测 `border_radius > 0`：背景用 FillRoundedRect，边框用 StrokeRoundedRect
- `Replay()` 映射到对应 Canvas 方法
- 边框圆角 MVP：统一颜色（top）、统一宽度（top）

### 局限
- 单一 radius 值（非 per-corner）
- 圆角边框使用统一颜色/宽度

### 影响文件
- `veloxa/core/render/paint_command.h`
- `veloxa/core/render/renderer.cc`

---

## Iteration 2：FreeType + HarfBuzz 字体渲染

### 架构

```
Host ──vx_view_load_font()──→ FontManager ──FT_New_Face()──→ FT_Face
                                    │
                                    ├──→ FreeTypeTextShaper::Measure()  [Layout]
                                    │       HarfBuzz shaping → metrics
                                    │
                                    └──→ SoftwareCanvas::DrawText()     [Render]
                                            FT_Render_Glyph → alpha bitmap → blend
```

### 组件

| 组件 | 文件 | 职责 |
|------|------|------|
| FontManager | `veloxa/text/font_manager.h/.cc` | FT_Library 持有、FT_Face 注册/查询、FontHandle 分配 |
| GlyphCache | `veloxa/text/glyph_cache.h/.cc` | 字形位图缓存（key=glyph_id+size），LRU 淘汰 |
| FreeTypeTextShaper | `veloxa/text/freetype_shaper.h/.cc` | TextShaper::Measure 实现，HarfBuzz shaping |

### 接口变更
- `Canvas::DrawText` 增加 `FontHandle` 参数
- `PaintCommand::kDrawText` 增加 `font_handle` 字段
- `TextShaper::Measure` 增加 `FontHandle` 参数
- C API：`vx_view_load_font(VxView*, const char* path, const char* family)`

### CMake
- `find_package(Freetype REQUIRED)` + pkg-config harfbuzz
- 新目标 `vx_text` 链接 FreeType + HarfBuzz + vx_foundation

### 测试字体
- 使用系统字体（如 `/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf`）做测试
- 测试验证：加载成功、Measure 返回合理值、DrawText 写入非零像素

---

## Iteration 3：图片支持

### 全链路

```
<img src="logo.png">
  → BuildTree: Element(kImg).GetAttribute("src") → ImageCache::Load()
  → ImageDecoder: 文件 I/O → PNG/JPEG 解码 → Image{pixels, w, h}
  → Layout: LayoutBox(kReplaced) intrinsic_width/height = Image 尺寸
  → Record: PaintCommand::DrawImage(image_handle, dst_rect)
  → Replay: Canvas::DrawImage(image, src_rect, dst_rect)
```

### 组件

| 组件 | 文件 | 职责 |
|------|------|------|
| Image | `veloxa/graphics/image.h` | RGBA 像素 + 宽高，std::shared_ptr 共享 |
| ImageDecoder | `veloxa/core/image/image_decoder.h/.cc` | 格式识别 + 分发 PNG/JPEG |
| PngDecoder | `veloxa/core/image/png_decoder.cc` | libpng 包装 |
| JpegDecoder | `veloxa/core/image/jpeg_decoder.cc` | libjpeg-turbo 包装 |
| ImageCache | `veloxa/core/image/image_cache.h/.cc` | 按路径去重缓存 |

### 接口变更
- `Canvas::DrawImage(const Image& image, const Rect& src, const Rect& dst)`
- `PaintCommand` 新增 `kDrawImage`
- `LayoutType` 新增 `kReplaced`
- `LayoutBox` 新增 `u32 image_handle`
- `LayoutContext` 新增 `ImageCache*`

### 布局规则
- CSS width/height 明确 → 使用指定值
- auto × auto → intrinsic 尺寸
- 一维 auto → 按宽高比缩放
- 加载失败 → 0×0

### SoftwareCanvas::DrawImage
- 最近邻缩放（MVP）
- SrcOver alpha 混合

### CMake
- `find_package(PNG REQUIRED)` + `find_package(JPEG REQUIRED)`

---

## Iteration 4：QuickJS DOM 绑定

### 架构

```
JS: document.getElementById("btn")
  → DomBindings → Document::FindElement(id) → Element*
  → 包装为 JS opaque object → 返回 JS

JS: el.textContent = "Hello"
  → setter callback → 修改 Text child → Invalidate

JS: el.style.backgroundColor = "red"
  → style proxy setter → CSS 解析 → inline_declarations → Invalidate

JS: el.addEventListener("click", fn)
  → JsEventBridge → EventDispatcher::AddEventListener(wrapper)
```

### JS API

```javascript
document.getElementById(id)        → Element | null
document.querySelector(selector)   → Element | null

el.tagName                         → String (只读)
el.id                              → String (只读)
el.textContent                     → String (读写)
el.getAttribute(name)              → String | null
el.setAttribute(name, value)       → void
el.style.{property}                → String (写)
el.addEventListener(type, fn)      → void
el.removeEventListener(type, fn)   → void
```

### 生命周期
- Element* 由 Document(ArenaAllocator) 拥有
- JS wrapper 存储裸 Element*，不影响 C++ 生命周期
- 约束：Document outlive JSContext（Application 编排保证）
- JS 回调 JS_DupValue 持有引用，清理时 JS_FreeValue

### inline style 注入（同时修复技术债 #28）
- `el.style.xxx = "value"` → 解析为 Declaration → 存入 Element inline_declarations
- BuildTree 读取 inline_declarations 传入 StyleResolver::Resolve

### CMake
- `vx_script` 链接 `vx_core`

---

## 不在范围内
- per-corner border-radius
- per-side 圆角边框颜色
- WebP 图片格式（libwebp-dev 未安装）
- 异步图片加载
- 双线性图片缩放（MVP 使用最近邻）
- DOM 节点创建/删除（createElement, removeChild 等）
- JS 模块加载（import/export）
- 中断处理器 / 执行预算
