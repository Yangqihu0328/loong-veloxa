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
- **QuickJS（quickjs-ng）** — 轻量 ECMAScript 引擎；**已集成**（TASK-20260413-01）
  - CMake：`veloxa/script/CMakeLists.txt` 使用 `FetchContent` 固定 **`v0.14.0`**，目标 `qjs` + `qjs-libc`
  - 模块：静态库 **`vx_script`**，`vx::script::QuickjsEngine`（`Init` / `Shutdown` / `EvalGlobal`）
  - 策略：`JS_SetMemoryLimit` 默认 **32MiB**/Runtime；**不**调用 `js_std_add_helpers`；源码 **256KiB** 上限；异常路径：`JS_FreeValue` → `JS_GetException`（见 `api-test.c`）
  - 构建：根目录全局 `-Werror`/`-Wpedantic` 已限制为 **`$<COMPILE_LANGUAGE:CXX>`**，避免污染上游 C 源码；WSL 无 DNS 时需 **`http_proxy`/`https_proxy`**（或离线预置 `_deps`）以便首次 `FetchContent`

### 第三方依赖
| 库 | 用途 | 备注 |
|---|---|---|
| quickjs-ng (QuickJS) | JavaScript 引擎 | **已接入** v0.14.0，FetchContent；周期性 CVE/发行说明对照（自动化工具缺位） |
| FreeType | 字体光栅化 | **已接入**（TASK-20260414-01），系统包 find_package(Freetype) |
| HarfBuzz | 文本整形 | **已接入**（TASK-20260414-01），系统包 pkg_check_modules(HARFBUZZ) |
| libpng | PNG 图片解码 | **已接入**（TASK-20260414-01），系统包 find_package(PNG) |
| libjpeg-turbo | JPEG 图片解码 | **已接入**（TASK-20260414-01），系统包 find_package(JPEG) |
| libwebp | WebP 图片解码 | 未接入（系统缺少 -dev 包），延期 |
| zlib | 压缩 | 待接入（资源打包） |
| libuv | 异步 I/O | 待接入（事件循环） |

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
- SoftwareCanvas 实现：有 FontManager* 时使用 FreeType+HarfBuzz 做真实字形渲染（HarfBuzz 塑形 → FreeType 光栅化 → GlyphCache 缓存 → SrcOver 混合）
- 无 FontManager* 时退化为旧存根（逐字符 FillRect）

### Canvas::DrawImage（TASK-20260414-01 新增）
- 纯虚接口，参数：image (const Image&), src_rect (Rect), dst_rect (Rect)
- SoftwareCanvas 实现：最近邻缩放 + SrcOver alpha 混合，裁剪到 canvas 边界

### Image 解码管线（TASK-20260414-01 新增）
- ImageDecoder：DecodeFromFile/DecodeFromMemory，magic bytes 自动检测 PNG/JPEG
- PNG：libpng，png_set_expand + gray_to_rgb + add_alpha 确保 RGBA 输出
- JPEG：libjpeg-turbo，RGB→RGBA（alpha=255）
- ImageCache：路径去重缓存，handle-based 查找

### QuickJS DOM 绑定（TASK-20260414-01 新增）
- DomBindings::Bind(ctx, doc, em) 注册 document.getElementById + Element 类 + Style proxy
- Element 属性：tagName（只读）、id（只读）、textContent（读写）、style（getter 返回 proxy）
- Element 方法：getAttribute、setAttribute、addEventListener、removeEventListener
- Style proxy：7 个 CSS 属性的 camelCase setter，通过 CssParser::ParseDeclarationList 解析
- addEventListener 通过 JS_DupValue 保持 callback 引用，TrackedCallbacks 在 Unbind 时释放

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

### CSS 伪类透传链完整路径（TASK-09 修复）
- 完整链路：LayoutContext.event_manager → BuildTree → StyleResolver::Resolve(em) → CollectMatchingRules(em) → SelectorMatcher::Matches(sel, el, em)
- TASK-08 只修改了 SelectorMatcher，未更新上游 StyleResolver 调用链，导致 restyle 阶段伪类永远不匹配
- 教训：跨模块参数透传修改必须端到端验证整条调用链

## Update Manager 实现经验（2026-04-05）

### 更新管线架构
- UpdateManager 编排 Invalidate → Restyle → Relayout → DirtyRect → Repaint
- 全量 restyle + relayout（HMI 树小，< 1ms），仅重绘阶段做脏区优化
- 拥有 ArenaAllocator，每帧 Reset 释放旧 LayoutBox 树
- LayoutEngine::Layout 新增 ArenaAllocator& 重载，旧签名（static arena）仍保留

### 脏区计算
- DisplayList 逐项对比（PaintCommand::operator==），差异项 rect 并集 = 脏区
- 列表长度不同时回退全屏重绘
- Canvas::PushClipRect 裁剪 + Clear + Replay 实现脏区重绘

### SoftwareCanvas 构造 API
- SoftwareCanvas 接受 `(u32* pixels, u32 width, u32 height, u32 stride)`
- 不接受 Surface* 指针，需手动 Lock/Unlock Surface 获取 pixels
- 写测试时应参考现有测试的构造方式，避免 API 假设错误

## DomBindings pimpl 与 QuickJS 集成经验（TASK-20260418-01）

### pimpl 化 header 零 quickjs 依赖
- `dom_bindings.h` 仅前向声明 `struct JSContext;`，所有 `JSClassID`/`JSValue`/`JS_*` 都进 `.cc`
- **不完整类型坑**：`std::unique_ptr<InstanceData>` 在 header 中只有前向声明时，`~DomBindings()` 必须在 `.cc` 定义（non-default），否则 delete 时类型不完整
- 匿名 namespace 中的 C 风格回调无法访问 `private` 嵌套类型——把 `struct InstanceData;` 前向声明放 `public:`，定义和 `data_` 保持 `private`；用 `friend struct DomBindingsInternal;` 提供 `.cc` 内桥接
- `JS_SetContextOpaque(ctx, this)` + `JS_GetContextOpaque(ctx)` 是 ctx→C++ 实例的规范通道

### JSClassID 是进程级常量，不是实例状态
- `JSClassID` 由 `JS_NewClassID(rt, &id)` 全局分配，即使多个 `JSRuntime` 也共享；必须当作 file-scope static 处理
- 幂等注册模板（量产代码必用）：
  ```cpp
  if (s_class_id == 0) JS_NewClassID(rt, &s_class_id);
  if (!JS_IsRegisteredClass(rt, s_class_id)) JS_NewClass(rt, s_class_id, &def);
  ```
- 陷阱：把 `JSClassID` 作为实例成员每次 `Bind` 重新 `JS_NewClassID`，会在同 runtime 上创建重复 class 导致 `JS_NewObjectClass` 失败
- quickjs-ng v0.14.0 已支持 `JS_IsRegisteredClass`（`build/_deps/quickjsng-src/quickjs.h` 可核实）

### JS 回调生命周期 UAF 防护
- `addEventListener` 注册 C++ lambda 捕获了 `JSValue callback` —— lambda 与 `JSValue` **必须同寿命**
- `Unbind` 顺序硬约束：先 `EventManager::RemoveEventListeners(el)`（销毁 lambda 拷贝）**再** `JS_FreeValue(callback)`
- 维护 `InstanceData::listener_elements: Vector<Element*>` 去重记录已绑定元素，`Unbind` 遍历清理；`removeEventListener` 也必须同步移除
- 剩余风险：`DomBindings` 析构必须先于 `EventManager`；相反则 `RemoveEventListeners` 作用于悬垂 em 指针。当前由宿主手动保证，弱引用/观察者模式留待后续

## HarfBuzz + FreeType 生命周期与 CMake 传播（TASK-20260418-01）

### hb_font_t 缓存模式
- `hb_ft_font_create_referenced(FT_Face)` 是重操作（解析 charmap/metric），per-DrawText 调用是显著性能债
- 缓存粒度：每 `FontHandle` 一个 `hb_font_t`，跟 `FT_Face` 同寿命
- `FT_Face` 尺寸变化时 HarfBuzz 需显式通知：**先** `FT_Set_Pixel_Sizes(face, 0, new_size)`（调用方契约）**再** `hb_ft_font_changed(hb_font)`；否则 `hb_shape` 会用旧 metrics
- 释放顺序：`Shutdown` 中先 `hb_font_destroy(hb)` 再 `FT_Done_Face(face)` —— 某些 HarfBuzz 版本在 face 已释放后析构 hb_font 是 UB

### CMake 静态库依赖传播
- 核心原则：静态库 `A` 的 `.a` 内包含 `A.o` 但**不**捆绑其 `PRIVATE` 依赖 `B.a`；链接 `A` 的目标 `T` 不会自动得到 `B` 的头路径和符号
- 当 `T` 的源文件**直接** `#include <b_header.h>` 或**间接**依赖通过 `A` 内联/模板暴露的 `B` 符号时，`B` 必须在 `A` 中 `PUBLIC` 链接
- 本次反面教训：先尝试在 `tests/CMakeLists.txt` 添加 `pkg_check_modules(HARFBUZZ …)` 通过 `font_manager_test` 补链接，触发 `FindPkgConfig.cmake` "Unknown arguments specified"（父 scope 已调用导致参数状态冲突）
- 正确解法：在 `veloxa/text/CMakeLists.txt` 把 `Freetype::Freetype`、`${HARFBUZZ_LIBRARIES}` 从 `PRIVATE` 改 `PUBLIC`，同时 `${HARFBUZZ_INCLUDE_DIRS}` 也改 `PUBLIC`；所有下游（含 tests）自动继承
- 决策准则：**公开 header 中前向声明的类型**（`struct hb_font_t;`）即使在 .h 里不 include，只要 **测试或下游需要定义并调用这些类型的 API**，依赖就必须 `PUBLIC`

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
18. ~~:hover/:active/:focus 伪类当前返回 false（stub）~~ ✅ 已回填（TASK-08 SelectorMatcher + TASK-09 StyleResolver 透传）
19. LayoutEngine::Layout 的 static ArenaAllocator 线程不安全（旧签名仍存在，新签名接受外部 arena）
20. Block 布局缺少 margin collapsing（CSS 规范要求，影响渲染正确性）
21. LayoutInline 是简化实现，缺少真正的 line box 模型（ascent+descent 计算）
22. Arena 分配的 ComputedStyle 含 InternedString 成员，arena 释放不调用析构函数，可能泄露引用计数
23. SoftwareCanvas::DrawText 是存根（逐字符 FillRect），需集成 FreeType+HarfBuzz
24. border-radius 渲染未实现（ComputedStyle 有值但 renderer 忽略）
25. LayoutBox 缺少 border_box_origin()/padding_box_origin() 辅助方法，坐标计算分散在多处
26. DisplayList 无 Dump() 调试方法
27. vx_core 新增 vx_graphics 依赖，所有 core 代码（包括不需要 graphics 的 HTML/CSS/Layout）都链接了 vx_graphics
28. LayoutEngine::BuildTree 不解析 inline style（inline_decls 始终为 nullptr）
29. ~~EventManager 无「状态变更→样式失效→重绘」触发机制~~ ✅ 已实现（TASK-09 UpdateManager + InvalidationCallback）
30. EventDispatcher::listeners_ 元素销毁时需手动 RemoveEventListeners（无自动清理）
31. HitTest z-index 排序每次调用重新排序（可缓存）
32. EventDispatcher 使用 std::function 作为 handler（嵌入式场景可能需轻量替代）
33. UpdateManager::Update 中 canvas 操作（PushClipRect+Clear+Replay+PopClip）可提取为 RepaintDirtyRegion 辅助方法
34. ComputeDirtyRect 仅支持 PaintCommand 逐项对比，不处理命令重排序（假设绘制顺序不变）
35. UpdateManager 缺少 OnBeforeUpdate/OnAfterUpdate 钩子（宿主应用无法监听帧生命周期）
36. Application 缺少 Resize 支持（需重建 Canvas + 更新 LayoutContext viewport）
37. Application 缺少 HTML/CSS 解析错误回调
38. Application::LoadHTML 使用裸 delete 管理 Document，可改用 unique_ptr
39. ~~像素测试缺少标准化工具库（PixelR/G/B/A 提取函数），同类代码分散在多个测试文件中~~ ✅ 已提供 `tests/test_pixel_utils.h`（TASK-20260413-02，部分测试已迁移）
40. C API 缺少 vx_view_resize、vx_view_get_document、错误回调等扩展 API
41. ~~HashMap 缺少 const_iterator / cbegin / cend，const 方法无法遍历~~ ✅ 已实现（TASK-20260413-02）；`TransitionManager::HasActive` 已改为扫描
42. transition shorthand 仅支持单条声明，不支持逗号分隔多条（如 `transition: bg 300ms, opacity 200ms`）
43. LayoutBox.style 是 const 指针，动画覆盖需 const_cast，应考虑引入可写样式覆盖层或改为 non-const
44. `vx_script`：`JS_SetInterruptHandler` / 执行预算未实现（见 `creative-quickjs-host.md` Phase 2）；`JSMallocFunctions` 与 Foundation 分配器未对齐
45. ~~dom_bindings.cc 使用全局变量（g_bound_doc、g_element_class_id、g_tracked_callbacks）~~ ✅ 已迁移到 `DomBindings::InstanceData`（pimpl，`JS_SetContextOpaque` 桥接）；JSClassID 保留为文件级 `s_` 静态 + `JS_IsRegisteredClass` 幂等注册（TASK-20260418-01）
46. ~~StyleGetProp getter 始终返回空字符串~~ ✅ 已实现 length/color/auto/number/inherit/initial 序列化，遍历 `inline_declarations()`（TASK-20260418-01）；Enum（display 等）读路径仍返回 `""`，留作后续
47. removeEventListener 简化为移除元素所有监听器，未实现按 type+handler 精确移除
48. ~~SoftwareCanvas::DrawText 每次调用创建 hb_font_t~~ ✅ 已缓存到 `FontManager::FontEntry`，`GetHbFont(handle, pixel_size)` 按需创建 + `hb_ft_font_changed` 响应 size 变化（TASK-20260418-01）
49. textContent setter 不处理无 Text 子节点的情况（不创建新 Text 节点，静默返回）
50. ~~addEventListener lambda 捕获 JSContext*~~ ✅ `DomBindings::Unbind` 先 `em->RemoveEventListeners(el)` 再 `FreeAll callbacks`，消除 UAF（TASK-20260418-01）；遗留约束：宿主必须确保 `DomBindings` 先于 `EventManager` 析构
