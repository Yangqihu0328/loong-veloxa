# 技术上下文

## 技术栈

### 核心语言
- **C++17** — 引擎核心实现
- **C** — 对外 API 层（ABI 稳定性）

### 代码规范
- **Google C++ Style Guide** — 命名、格式、异常（禁用）等遵循 Google 规范
- `.clang-format`：BasedOnStyle: Google
- `.clang-tidy`：启用 bugprone/google/modernize/performance/readability 检查

### 构建系统
- **CMake** — 主构建系统（跨平台，嵌入式工具链友好）
- **vcpkg** — 第三方依赖管理（可选）

### 图形后端（按优先级）
- **OpenGL ES 2.0/3.0** — 嵌入式主力后端
- **Vulkan** — 高性能可选后端
- **Skia（精简）** — 可选 2D 渲染路径
- **Software Renderer** — 无 GPU 回退

### 脚本引擎
- **QuickJS** — 轻量 ECMAScript 引擎（参考 Sciter 的选择）

### 第三方依赖（规划）
| 库 | 用途 | 备注 |
|---|---|---|
| QuickJS | JavaScript 引擎 | 嵌入式友好 |
| FreeType + HarfBuzz | 字体渲染与文本整形 | 车载多语言支持 |
| libpng / libjpeg / libwebp | 图片解码 | 按需裁剪 |
| zlib | 压缩 | 资源打包 |
| libuv | 异步 I/O | 事件循环 |

## Sciter 架构分析摘要

### 分层架构（参考）
```
┌─────────────────────────────────────┐
│          宿主应用 (C API)            │
├─────────────────────────────────────┤
│  脚本绑定 (xdomjs + QuickJS)         │
├─────────────────────────────────────┤
│  HTML 引擎 (DOM + CSS + Layout)      │
├─────────────────────────────────────┤
│  图形对象层 GOOL (抽象绘制接口)       │
├─────────────────────────────────────┤
│  平台后端 (D2D / GDI+ / Cairo / CGX) │
├─────────────────────────────────────┤
│  基础工具 (tool: 容器/字符串/IO)      │
└─────────────────────────────────────┘
```

### Sciter 关键设计点
1. **GOOL 抽象**：`gool::graphics` 纯虚接口，后端通过 `app_factory()` 选择
2. **DOM 类层次**：`node → element → block → block_vertical/horizontal/grid/stack`
3. **CSS 管线**：词法(`css_istream`) → 规则(`style_def`) → 匹配/层叠(`style_bag`) → 计算(`style::resolve`)
4. **布局引擎**：block/inline/flex(spring)/grid(kiwi约束)/table 多种流
5. **事件模型**：捕获(sinking) → 目标 → 冒泡，支持委托与选择器匹配
6. **视图更新**：`update_queue` 脏区管理，按 `STYLE_CHANGE_TYPE` 排队

### Veloxa 差异化方向
- 去除桌面重型依赖（D2D/GDI+），聚焦 OpenGL ES / Vulkan
- CSS 子集裁剪（保留 flex/grid，去除极少使用的属性）
- 内存池化与零拷贝设计
- 帧缓冲直出（适配 DRM/KMS）
- 车载特有：DPI 适配、多屏、触摸优先、旋钮/HUD 交互

## Foundation 实现经验（2026-04-05）

### 编译器约束
- `-Wpedantic` 禁止 C99 柔性数组成员（`char data[]`），需用偏移计算替代
- `-Wtype-limits` 与编译时常量比较冲突，需 `-Wno-type-limits` 豁免
- GCC 11 对 C++17 支持完整，`if constexpr`、结构化绑定均可用

### 依赖管理
- FetchContent 依赖网络可用性，离线环境应优先使用系统包
- 系统 GTest v1.11.0 功能足够（`EXPECT_DEATH`、`TEST_F` 均可用），但缺少 v1.14+ 的 `EXPECT_THAT`

## Graphics/Platform HAL 实现经验（2026-04-05）

### 像素格式
- RGBA32 格式统一为 R[0:7] | G[8:15] | B[16:23] | A[24:31]
- 跨模块操作（渲染 → 存储 → 导出）必须引用同一格式定义
- SavePPM 导出需按 R, G, B 顺序提取字节（bits 0-7, 8-15, 16-23）

### 子代理协作约束
- 涉及多模块数据交互的子代理 prompt 必须包含精确的数据格式规范
- 不同子代理的像素格式假设可能矛盾，集成测试是唯一防线

### 软件渲染器实现
- 覆盖率光栅化使用中点近似（非解析面积），正确但 AA 质量有限
- StrokePath 逐段独立渲染，无 join/cap 处理
- PushClipPath 使用 bounds 近似替代真正的路径裁剪
- `std::function<void()>` 用于 EventLoop::Task，嵌入式场景可能需要替换为轻量回调

## DOM/HTML Parser 实现经验（2026-04-05）

### DOM 架构
- 精简四类节点（Element/Text/Comment/Document），与 Layout 完全分离
- Document 使用 ArenaAllocator 分配节点（placement new），owned_nodes_ 追踪析构
- 子节点双向链表，属性 SmallVector<Attribute, 4>

### HTML 解析管线
- Tokenizer（混合状态机）→ Parser（DOM Builder）→ Document
- Tokenizer 零拷贝：Token 的 name/value 是 StringView 指向原始输入
- 实体解码延迟到 Parser 层（Token 的 has_entities 标志）
- 隐式关闭使用 20 条数据驱动规则表

## CSS Engine 实现经验（2026-04-05）

### CSS 解析管线
- CssTokenizer：22 种 token 类型，主分支 + 子扫描器模式，零拷贝 StringView
- CssParser：选择器解析（6 种简单选择器 + 2 种组合子）+ 声明解析 + 值类型分发
- 颜色解析：#hex（3/6/8 位）、rgb()/rgba()、18 种命名颜色（排序数组 + 二分查找）
- 简写展开：margin/padding（1-4 值）、border（width+style+color）

### 选择器匹配
- 选择器解析时从左到右，存储时反转为从右到左
- 匹配时从 compounds[0]（最右端，目标元素）开始，根据 combinator 向上遍历
- compounds[i].combinator 描述 compounds[i] 与 compounds[i-1] 之间的关系

### 层叠与继承
- 应用顺序：非 important 样式表 → 非 important 内联 → important 样式表 → important 内联
- 10 个可继承属性：color, visibility, font-family/size/weight/style, line-height, text-align, white-space, letter-spacing
- ComputedStyle ~200 字节，直存所有计算后的属性值

## Layout Engine 实现经验（2026-04-05）

### 布局架构
- LayoutBox 扁平 struct + LayoutType 枚举，arena 分配（ArenaAllocator）
- 独立布局树，通过 BuildTree 递归 DOM 树构建
- ComputedStyle 在 BuildTree 阶段通过 StyleResolver::Resolve 即时计算，结果 arena 分配存入 LayoutBox
- Block/Inline/Flex 三种布局模式 + relative/absolute 定位

### 布局算法覆盖
- **Block**：垂直堆叠、auto width/height、border-box、min/max 约束、auto margin 居中（无 margin collapsing）
- **Flex**：CSS Flexbox Level 1 §9——grow/shrink/basis、justify-content 5 种、align-items/self、gap、wrap、reverse
- **Inline**：简化文本行排列（水平堆叠+换行），无真正 line box 模型
- **Positioning**：relative 偏移、absolute 定位+递归布局

### 关键实现细节
- `LayoutEngine::Layout` 使用 `static ArenaAllocator`（线程不安全，需重构）
- BuildTree 过滤纯空白文本节点（缩进/换行），避免布局干扰
- LayoutChild 对 kText 节点调用 TextShaper::Measure 测量尺寸
- LayoutChild 分发：Block→LayoutBlock, Flex→LayoutFlex(自由函数), Inline→LayoutInline, Text→测量

## Render Pipeline 实现经验（2026-04-05）

### Display List 架构
- PaintCommand 6 种类型（FillRect/DrawText/PushClipRect/PopClip/PushLayer/PopLayer）
- Record 递归遍历 LayoutBox 树，生成有序 DisplayList
- Replay 遍历 DisplayList，调用 Canvas 方法
- Paint = Record + Replay 便捷入口

### 跨模块颜色格式
- CSS ComputedStyle: RRGGBBAA — `0xFF0000FF` = red opaque
- gfx::Color::ToRGBA32(): R[0:7]G[8:15]B[16:23]A[24:31] — `0xFF0000FF` = red opaque（巧合）
- **不可混用**：black 在 CSS 是 `0x000000FF`，在 gfx 是 `0xFF000000`
- CssColorToGfx() 转换函数位于 render_utils.h
- CSS 命名颜色 `green` = #008000 ≠ gfx::Color::Green() = #00FF00

### LayoutBox 坐标语义
- `x`, `y`：content area 原点（绝对坐标）
- padding box 原点：`(x - padding[left], y - padding[top])`
- border box 原点：`(x - padding[left] - border[left], y - padding[top] - border[top])`
- 此语义对渲染、hit-testing 等所有坐标计算至关重要

### Canvas::DrawText
- 纯虚接口，参数：text (StringView), bounds (Rect), font_size (f32), brush (Brush)
- SoftwareCanvas 实现：逐字符 FillRect（char_width = 0.6 × font_size, 空格跳过）
- 后续集成 FreeType+HarfBuzz 后替换实现，上层代码零修改

## Event System 实现经验（2026-04-05）

### 事件系统架构
- 两层事件模型：InputEvent（平台层）+ DOMEvent（DOM 分发上下文）
- W3C DOM Events 三阶段分发：Capture → Target → Bubble
- 中央 EventManager 管理 hover/active/focus 状态（3 指针，不侵入 Element）
- HitTest 使用 LayoutBox 树后序遍历，共享渲染管线的 z-index 排序逻辑

### CSS 伪类连接
- SelectorMatcher::Matches 添加可选 `const EventManager* em = nullptr` 参数
- :hover → em->IsHovered(el)（祖先链遍历）
- :active → em->IsActive(el)（祖先链遍历）
- :focus → em->IsFocused(el)（精确匹配）
- em == nullptr 时保持原有行为（返回 false）

### 集成测试约束（重要）
- **LayoutEngine::BuildTree 不解析 HTML inline style**
- BuildTree 调用 StyleResolver::Resolve 时 inline_decls 参数默认为 nullptr
- 所有集成测试必须使用外部 CSS 选择器（`#id { ... }`），不可使用 `style="..."` 属性
- 这是 API 能力假设错误第三次出现（TASK-02 像素格式、TASK-07 border box 坐标、TASK-08 inline style）

### 技术债务清单
1. Benchmark 延期（需 google benchmark）
2. HashMap SIMD Group 探测未实现（当前标量线性探测）
3. InternedString 全局表非线程安全
4. BasicString 含 Alloc* 指针，sizeof 为 32 而非纯 24
5. Status::message 使用 std::string，与自有 String 存在循环依赖
6. Rasterizer 覆盖率算法待升级为解析面积计算（AA 质量）
7. PushClipPath 仅用 bounds 近似，待实现真正路径裁剪
8. StrokePath 无 join/cap 处理（当前 butt 端帽、无连接）
9. ~~PPM 测试使用硬编码 /tmp 路径~~ ✅ 已修复（TASK-05，PID 隔离）
10. CMake: vx_graphics 链接 vx_platform 可能引入不必要耦合
11. ~~TagIdFromName/PropertyIdFromName 均为 O(N) 线性扫描~~ ✅ 已优化为 HashMap O(1)（TASK-05）
12. ~~Document 节点管理用 Vector<Node*> + delete~~ ✅ 已集成 ArenaAllocator（TASK-05）
13. ~~Parser 静默忽略 kError token~~ ✅ 已支持错误收集（TASK-05）
14. Serializer 不做空白规范化
15. parser.cc 过大（1035 行），考虑拆分为 parser_selector.cc + parser_value.cc
16. ApplyDeclaration switch 规模大（~55 case），可用宏或代码生成简化
17. 选择器匹配 O(rules × elements) 全量遍历，大页面需哈希索引优化
18. :hover/:active/:focus 伪类当前返回 false（stub），需事件系统回填
19. LayoutEngine::Layout 的 static ArenaAllocator 线程不安全，需重构为调用者持有
20. Block 布局缺少 margin collapsing（CSS 规范要求，影响渲染正确性）
21. LayoutInline 是简化实现，缺少真正的 line box 模型（ascent+descent 计算）
22. Arena 分配的 ComputedStyle 含 InternedString 成员，arena 释放不调用析构函数，可能泄露引用计数
23. SoftwareCanvas::DrawText 是存根（逐字符 FillRect），需集成 FreeType+HarfBuzz
24. border-radius 渲染未实现（ComputedStyle 有值但 renderer 忽略）
25. LayoutBox 缺少 border_box_origin()/padding_box_origin() 辅助方法，坐标计算分散在多处
26. DisplayList 无 Dump() 调试方法
27. vx_core 新增 vx_graphics 依赖，所有 core 代码（包括不需要 graphics 的 HTML/CSS/Layout）都链接了 vx_graphics
28. LayoutEngine::BuildTree 不解析 inline style（inline_decls 始终为 nullptr）
29. EventManager 无「状态变更→样式失效→重绘」触发机制（仅更新指针）
30. EventDispatcher::listeners_ 元素销毁时需手动 RemoveEventListeners（无自动清理）
31. HitTest z-index 排序每次调用重新排序（可缓存）
32. EventDispatcher 使用 std::function 作为 handler（嵌入式场景可能需轻量替代）
