# 系统模式与架构决策

## 整体架构（规划）

```
┌──────────────────────────────────────────────┐
│              宿主应用 (Host Application)        │
│        通过 C API (veloxa_api.h) 交互          │
├──────────────────────────────────────────────┤
│                                              │
│  ┌─────────┐  ┌──────────┐  ┌────────────┐  │
│  │ JS 绑定  │  │ DOM/CSSOM │  │  事件系统   │  │
│  │(QuickJS) │  │  Bridge   │  │  Events    │  │
│  └────┬─────┘  └─────┬────┘  └─────┬──────┘  │
│       │              │             │          │
│  ┌────┴──────────────┴─────────────┴──────┐  │
│  │          核心引擎 (Core Engine)          │  │
│  │  ┌────────┐ ┌───────┐ ┌──────────────┐ │  │
│  │  │ HTML   │ │  CSS  │ │    Layout    │ │  │
│  │  │ Parser │ │Engine │ │ (Flex/Grid)  │ │  │
│  │  └────────┘ └───────┘ └──────────────┘ │  │
│  └────────────────┬───────────────────────┘  │
│                   │                          │
│  ┌────────────────┴───────────────────────┐  │
│  │       图形抽象层 (Graphics HAL)          │  │
│  │  统一接口: fill/stroke/clip/layer/text  │  │
│  └──┬──────────┬──────────┬───────────┬───┘  │
│     │          │          │           │       │
│  ┌──┴───┐  ┌──┴───┐  ┌──┴────┐  ┌───┴───┐  │
│  │OpenGL│  │Vulkan│  │ Soft  │  │ Skia  │  │
│  │ ES   │  │      │  │Render │  │(可选) │  │
│  └──────┘  └──────┘  └───────┘  └───────┘  │
│                                              │
│  ┌──────────────────────────────────────────┐│
│  │        平台抽象层 (Platform HAL)          ││
│  │  窗口/输入/剪贴板/定时器/文件系统         ││
│  │  ┌─────┐ ┌────┐ ┌──────┐ ┌──────────┐  ││
│  │  │Linux│ │DRM │ │Wayland│ │ Windows  │  ││
│  │  │ FB  │ │KMS │ │      │ │ (dev)    │  ││
│  │  └─────┘ └────┘ └──────┘ └──────────┘  ││
│  └──────────────────────────────────────────┘│
│                                              │
│  ┌──────────────────────────────────────────┐│
│  │         基础库 (Foundation)               ││
│  │  内存管理 / 容器 / 字符串 / 编码 / IO     ││
│  └──────────────────────────────────────────┘│
└──────────────────────────────────────────────┘
```

## 核心设计原则

### 1. 分层隔离
- 每层只依赖下层，禁止跨层调用
- 层间通过抽象接口通信，实现可替换

### 2. 嵌入式优先
- 静态内存池优先于动态分配
- 零拷贝数据传递
- 编译时可裁剪模块（通过 CMake 选项）

### 3. 参考 Sciter 的设计模式
| Sciter 模式 | Veloxa 对应 | 差异 |
|-------------|-------------|------|
| `gool::graphics` 纯虚接口 | `vx::Graphics` 抽象基类 | 增加 GLES/Vulkan 后端 |
| `html::node/element/document` | `vx::dom::Node/Element/Document` | 精简，去除桌面特有部分 |
| `style_def` 选择器匹配 | `vx::css::Selector` | CSS 子集 |
| `update_queue` 脏区管理 | `vx::RenderScheduler` | 帧同步优化 |
| `app_factory()` 后端选择 | `vx::BackendFactory` | 编译时+运行时双选 |

### 4. 车载特有模式
- **多屏渲染**：一个引擎实例驱动多个 Surface
- **触摸优先**：事件系统触摸为主，鼠标兼容
- **DPI 自适应**：物理尺寸而非像素驱动布局
- **安全隔离**：关键信息与娱乐区域渲染隔离

## 模块划分（规划）

```
veloxa/
├── foundation/      # 基础库：内存、容器、字符串、编码
├── platform/        # 平台抽象：窗口、输入、文件系统
├── graphics/        # 图形 HAL：抽象接口 + 各后端
├── core/            # 核心引擎
│   ├── html/        # HTML 解析器
│   ├── css/         # CSS 引擎
│   ├── dom/         # DOM 树
│   ├── layout/      # 布局引擎
│   ├── render/      # 渲染管线
│   └── event/       # 事件系统
├── script/          # QuickJS 集成与 DOM 绑定
├── api/             # 对外 C API
└── tools/           # 资源打包、开发工具
```

## 已验证的模式（来自 Foundation 实现）

### Allocator 模板参数模式
- 所有容器和 String 通过 `typename Alloc` 模板参数接受分配器
- 默认使用 `MallocAllocator`，可替换为 `ArenaAllocator`（场景级批量释放）
- 分配器通过指针传递（`Alloc*`），不拥有分配器生命周期

### Header-Only 模板库模式
- 容器和基础类型均为 header-only，减少链接复杂度
- 仅有实现细节较多的模块使用 .cc 文件（UTF 编解码、日志后端、InternedString 全局表）

### 编译时裁剪模式
- 日志通过 `VX_MIN_LOG_LEVEL` 宏在编译时裁剪低级别日志
- 宏展开为 `if (constexpr_level >= compile_level)` 模式，编译器优化器消除死代码

### OOM 处理策略
- 当前策略：分配失败直接 `std::abort()`（嵌入式场景，OOM = 不可恢复）
- 待改进：可考虑 OOM 回调机制，允许宿主应用做清理

### FetchContent / C 子项目与全局编译选项（TASK-20260413-01）
- 根 `project(... LANGUAGES CXX)` 仍会在子目录中编译 **C** 依赖（如 quickjs-ng）
- 若根 CMake 使用 **全局** `add_compile_options(-Wpedantic -Werror …)`，会作用于 **所有语言**，导致上游 C 库在 `-Werror=pedantic` 等规则下失败
- **已验证做法：** 用生成表达式将严格告警限制为 C++，例如  
  `"$<$<COMPILE_LANGUAGE:CXX>:-Werror>"` 等；或对自有目标使用 `target_compile_options` 而非全局注入

## 已验证的模式（来自 Graphics/Platform HAL 实现）

### 纯虚接口 + 具体后端模式
- Canvas/Path/Surface/EventLoop 均为纯虚接口（`virtual ~T() = default`）
- 具体实现（SoftwareCanvas、MemorySurface、HeadlessEventLoop）在子目录中
- 上层代码仅依赖接口头文件，后端可编译时替换
- 与 Sciter 的 `gool::graphics` 模式对齐

### 覆盖率扫描线光栅化模式
- 边生成 → 分桶 → 逐行覆盖率累加 → SrcOver 混合
- 非零缠绕规则（CSS/SVG 兼容）
- de Casteljau 贝塞尔曲线细分（阈值 0.25px，最大深度 16）
- 描边转轮廓填充（逐段法线偏移）

### 像素格式约定
- 唯一格式：RGBA32 = R[0:7] | G[8:15] | B[16:23] | A[24:31]
- 定义在 `Color::ToRGBA32()`，所有渲染和存储代码引用此格式
- Surface::stride() 返回字节数（width * 4）

### Lock/Unlock 资源访问模式
- Surface 通过 Lock() 返回裸指针，Unlock() 释放
- VX_DCHECK 防止重复 Lock 和 Resize 冲突
- Canvas 在 Lock 期间操作像素

## 已验证的模式（来自 DOM/HTML Parser 实现）

### 精简扁平 DOM 层次模式
- Node → Element / Text / Comment / Document 四类
- DOM 与 Layout 完全分离，DOM 不携带布局信息
- 子节点使用双向链表（Node 的 next/prev sibling 指针），O(1) 追加/移除
- 与 Sciter 的差异：Sciter 混合 DOM 和 Layout（element → block → block_vertical），Veloxa 彻底分离

### 枚举 + 元数据表驱动模式
- TagId 枚举索引 → TagInfo 静态表（O(1) 查找）
- TagInfo 携带 TagType/ParseModel/ContentModel 三维元数据
- 隐式关闭规则表（20 条 ImplicitCloseRule）实现数据与逻辑分离
- 扩展新标签只需添加枚举值和表项

### 混合状态机 Tokenizer 模式
- 主循环处理顶层分支（text/tag/rawtext）
- 复杂子逻辑提取为独立方法（ScanTag/ScanAttribute/ScanComment）
- 基础操作复用（Peek/Advance/ScanName/SkipWhitespace）
- Token 使用 StringView 零拷贝指向原始输入

### 子代理精确签名 prompt 模式
- 向子代理提供完整 API 签名（参数类型、返回类型、namespace、构造函数形式）
- 效果：TASK-03 中 3 个子代理全部零返工
- 已验证优于描述性文字的 prompt 方式

## 已验证的模式（来自 CSS Engine 实现）

### CSS 引擎解析管线模式
- CSS text → CssTokenizer（零拷贝 StringView token）→ CssParser → Stylesheet（规则列表）
- 与 HTML 解析管线完全同构：均为 Tokenizer → Parser → 结构化数据
- Tokenizer 复用"主分支 + 子扫描器"模式（与 HTML Tokenizer 一致）

### 枚举 + 元数据表驱动模式（第二次验证）
- PropertyId 的设计与 TagId 完全同构：枚举索引 → 静态表 O(1) 查找 + 线性扫描名称查找
- 该模式已在两个模块（DOM tag、CSS property）验证，确认为 Veloxa 标准 ID 系统模式
- 扩展只需添加枚举值 + 表项，无需修改查找逻辑

### 子代理合并 Phase 策略
- 当两个 Phase 紧密耦合时（共享类型定义或 API），合并为一个子代理比分别启动更高效
- Phase 1+2（属性系统 + Tokenizer 共享 Unit 枚举）和 Phase 4+6（Matcher + Resolver 共享 Selector 类型）合并效果好
- 减少了 prompt 中重复的上下文传递，避免中间产物同步问题

### CMake 存根预创建策略
- 当 CMakeLists.txt 预列所有文件但并行构建时部分文件尚未创建，第一个子代理应负责创建空 .cc 存根
- 避免后续子代理因 CMake 配置失败而报错
- 存根内容：仅 include 对应头文件（如存在）或空文件

### CSS 值类型混合策略（A+C）
- 解析阶段使用 CssValue（8B tagged union）作为通用值容器
- 计算阶段使用 ComputedStyle 直存具体类型（Display、LengthValue、u32 color 等）
- ApplyDeclaration 负责 CssValue → 具体类型的转换（大 switch）
- 与 Sciter 的 `style::resolve` 阶段对应

## 已验证的模式（来自 Layout Engine 实现）

### DOM-Layout 完全分离 + 独立布局树模式
- LayoutBox 扁平 struct（非继承层次），通过 LayoutType 枚举区分 Block/Inline/Flex/Text
- 布局树独立于 DOM 树，LayoutBox 持有 Element*/Text* 反向引用但不耦合 DOM 结构
- ComputedStyle 在 BuildTree 阶段通过 StyleResolver 即时计算，arena 分配后存入 LayoutBox
- 与 Sciter 的差异：Sciter 将 layout 嵌入 DOM（element→block→block_vertical），Veloxa 彻底分离

### 空白文本节点过滤模式
- HTML 源码中的缩进/换行产生空白 Text 节点，必须在 BuildTree 阶段过滤
- 不过滤会导致 Flex 容器子元素数错误、Block 布局出现额外间隙
- 过滤逻辑：遍历字符检查是否全为 space/tab/newline/CR

### TextShaper 纯虚接口模式
- 文本测量通过 `TextShaper` 纯虚接口抽象，与具体字体引擎（FreeType/HarfBuzz）解耦
- SimpleTextShaper 提供固定比例测量（0.6×font_size 每字符），足够支持布局测试
- 后续集成真实字体引擎只需实现 TextShaper::Measure，布局代码零修改

### 共享文件冲突约束子代理分组模式
- 当多个 Phase 修改同一 .cc 文件时，必须合并为同一子代理
- 本次 Phase 2+3+5 合并（共享 layout_engine.cc），Phase 4 独立（flex_layout.cc）
- 分组策略：按"产出文件"而非"功能逻辑"划分子代理边界

## 已验证的模式（来自 Render Pipeline 实现）

### Display List / 命令缓冲模式
- 渲染分为 Record（生成 DisplayList）和 Replay（执行到 Canvas）两阶段
- PaintCommand 使用扁平 struct + Type 枚举 + 工厂函数，避免继承层次
- DisplayList 是 `Vector<PaintCommand>`，可序列化/缓存/diff
- 与 Sciter 的 `update_queue` 脏区管理模式可以组合使用

### Stacking Context 排序模式
- 子元素按 z_index 做 stable_sort，相同 z_index 保持 DOM 源序
- opacity < 1.0 创建 PushLayer/PopLayer 合成层
- overflow:hidden 创建 PushClipRect/PopClip 裁剪层
- 绘制顺序：背景 → 边框 → 文本 → 排序后的子元素

### 跨层颜色格式桥接模式
- CSS 层使用 u32 RRGGBBAA 格式
- Graphics 层使用 gfx::Color 结构体 + ToRGBA32() 像素格式
- 桥接函数 CssColorToGfx() 位于 render_utils.h（header-only）
- CSS 命名颜色与代码颜色常量不一致，测试中必须通过桥接函数比较

### 集成测试多策略验证模式
- 结构验证：检查 DisplayList 命令类型/颜色/数量
- 区域扫描：HasColorInRegion 检查像素是否存在于指定区域
- 精确像素：仅用于最简单的无嵌套场景
- 避免硬编码像素坐标（受 HTML 隐式元素包裹影响）

## 已验证的模式（来自 Event System 实现）

### 两层事件模型模式（InputEvent + DOMEvent）
- InputEvent 扁平 struct 无 DOM 依赖（平台层），DOMEvent 包装 InputEvent 附加分发上下文
- 与 PaintCommand 的 Tagged Struct 模式一致
- 平台层可独立使用（headless 测试、输入录制回放）

### 中央状态指针模式
- EventManager 持有 hovered_/active_/focused_ 三个指针（24 字节）
- 不侵入 Element 类（DOM 层零修改）
- IsHovered 通过祖先链遍历实现，O(depth) 对 HMI 场景完全可接受

### 默认参数向后兼容注入模式
- SelectorMatcher::Matches 添加 `const EventManager* em = nullptr` 第三参数
- 现有 17 个 SelectorMatcher 测试 + StyleResolver 调用零修改通过
- 适用于给已有接口注入可选依赖的场景

### 并行子代理可行条件模式
- 两个子代理可并行执行的充分条件：(1) 无共享 .cc 文件 (2) 共享的 .h 在 Phase 1 已创建 (3) CMakeLists.txt 在 Phase 1 已更新
- TASK-08 首次验证：hit_test.cc + event_dispatcher.cc 并行，零冲突

## 已验证的模式（来自 Update Manager 实现）

### 中央管线编排器模式（UpdateManager）
- 单一 UpdateManager 持有 Document/Stylesheets/LayoutContext/EventManager/Canvas 引用
- 编排 Invalidate → Restyle → Relayout → DirtyRect → Repaint 全管线
- 拥有 ArenaAllocator（LayoutBox 树生命周期管理），每帧 Reset
- 与 Sciter 的 `update_queue` 脏区管理模式对应

### Push 回调失效触发模式
- EventManager 在 HandleInput 中检测状态实际变化（old != new）后调用 InvalidationCallback
- UpdateManager 注册回调，设置 dirty_ 标记
- 比 Pull 轮询更高效：无变化时零开销

### DisplayList 逐项对比脏区计算模式
- 新旧 DisplayList 同长度时逐项比较 PaintCommand（operator==），差异项的 rect 并集 = 脏区
- 不同长度（display:none 切换等结构变化）回退全屏重绘
- 对 hover/active/focus 变色场景最优：结构不变，仅颜色差异

### 默认参数向后兼容注入模式（第二次验证）
- StyleResolver::Resolve 和 CollectMatchingRules 添加 `const EventManager* em = nullptr`
- 现有 15 个 StyleResolver 测试 + 所有 BuildTree 调用零修改通过
- 该模式已在 TASK-08（SelectorMatcher）和 TASK-09（StyleResolver/LayoutEngine）两次验证

## 已验证的模式（来自 Application Shell 实现）

### 核心拥有 + 外部注入所有权模式
- Application 拥有核心引擎模块（Document, Stylesheets, EventManager, UpdateManager, TextShaper, Canvas）
- EventLoop 和 Surface 由外部注入（嵌入式场景宿主提供窗口/事件循环）
- 匹配 Sciter 的 `SciterCreateWindow + SciterLoadFile` 模式

### 延迟初始化编排器模式（EnsureUpdateManager）
- UpdateManager 依赖 Document + Canvas，但 LoadHTML/LoadCSS 可以任意顺序调用
- EnsureUpdateManager() 在首次 Update/InjectInput 时检查前置条件并创建
- LoadHTML/LoadCSS 调用后 reset UpdateManager，下次 Update 自动重建
- 解决了多模块初始化顺序依赖问题

### 编排层复杂度降级模式
- 当任务是纯编排层（连接已验证模块，无新算法/数据结构），实际复杂度低于 Level 3
- 本次 Application 实现仅 87 行，全部是模块连接代码
- 此类任务可考虑评为 Level 2，跳过 /creative 评估

## 已验证的模式（来自 C API 层实现）

### 不透明指针 C ABI 桥接模式
- C 公共头文件声明不透明 `typedef struct VxView VxView;`
- C++ 实现中 `reinterpret_cast` 在不透明指针和 C++ 类之间转换
- 每个 C 函数入口做 NULL 检查 → 返回枚举错误码
- 符合 Vulkan/SDL 惯例，ABI 稳定（C++ 类变更不影响头文件）

### 只读数据访问器补充排他锁模式
- Lock/Unlock 排他模型适合写操作，但测试/调试需要只读访问
- `data() const` 返回裸指针的只读视图，不影响 locked_ 互斥语义
- 当 production 代码已持有 Lock 时，测试代码可通过 data() 安全读取

## 已验证的模式（来自 CSS Transitions 实现）

### 前向声明打破循环依赖模式
- transition.h 前向声明 `struct ComputedStyle`，computed_style.h include transition.h
- ActiveTransition 存储 per-property from/to 值（u32/f32/LengthValue），避免引用完整 ComputedStyle
- 函数签名中 `const ComputedStyle&` 参数仅需前向声明，实现在 .cc 中 include 完整头文件
- 适用场景：两个头文件互相需要对方类型时，将其中一方的使用限于指针/引用

### 简写展开 → longhand 累积存储模式
- CSS transition 简写在 parser 中展开为 4 个 longhand Declaration（property/duration/timing/delay）
- StyleResolver::ApplyDeclaration 对每个 longhand 做 `if (transitions.empty()) push_back(default); transitions[0].field = value`
- 复用了 border/margin 简写展开的管线，无需新增 CssValue 类型或 Declaration 扩展
- 缺点：仅支持单条 transition（`transitions[0]`），多条需扩展为 per-index 累积

### 帧间样式快照 + const_cast 覆盖模式
- UpdateManager 维护 `prev_styles_` HashMap 存储上帧每个元素的"静态样式"
- Layout 后遍历 LayoutBox 树，对比 prev 和 new 检测变化，启动/更新过渡
- const_cast LayoutBox.style 覆盖动画值，再 Record 产生带动画效果的 DisplayList
- 妥协：const_cast 绕过了 const 语义，长期应引入可写样式覆盖层

### HashMap const 迭代与 const 查询模式
- `HashMap` 提供 `const_iterator`、`begin() const` / `end() const`、`cbegin()` / `cend()`，与 `iterator` 对称
- `TransitionManager::HasActive() const` 在 const 上遍历 `transitions_`，查找是否存在 `!tr.completed` 的项（TASK-20260413-02）
- 优先用容器语义表达状态，避免手写计数器与业务逻辑双轨维护

## 已验证的模式（来自功能补全 TASK-20260414-01）

### 可选依赖注入 + 退化模式
- SoftwareCanvas 构造函数接受可选 `FontManager*` + `GlyphCache*`；有值时用 FreeType 渲染字形，无值时退化为旧存根
- Record/Replay 接受可选 `ImageCache*`；无值时跳过图片处理
- 所有现有测试零修改通过——向后兼容验证
- 适用场景：给已有模块注入新的可选能力，不破坏现有调用方

### QuickJS C API 绑定模式
- `JS_NewClassID` + `JS_NewClass` 注册自定义 JS 类（Element、CSSStyleDeclaration）
- `JS_SetOpaque` / `JS_GetOpaque` 存储/取回 C++ 对象指针
- `JS_SetContextOpaque` 存储全局绑定上下文（DomBindings*），替代全局变量
- `JS_DupValue` 保持 JS callback 引用，Unbind 时 `JS_FreeValue` 释放
- getter/setter 通过 `JS_DefinePropertyGetSet` + `JS_CFUNC_getter_magic` / `JS_CFUNC_setter_magic` 注册

### 子代理 Prompt "文件内容嵌入 + 禁止修改清单" 模式
- prompt 中直接嵌入当前文件的关键代码片段（而非路径引用），子代理可精确修改
- 包含"不修改"清单（根 CMakeLists、memory-bank/、.cursor/ 等）
- 本次 5 个子代理全部零返工，验证该策略已成熟
- 与 TASK-03 的"精确签名 prompt"模式互补

### CMake 循环依赖解决模式
- 当 A（vx_core）需要链接 B（vx_script），而 B 的源码 include A 的头文件时
- B 不链接 A（仅通过 PUBLIC include path 获取头文件可见性）
- A 链接 B（PRIVATE），B 的符号在最终可执行文件中由 A 提供
- 关键：B 的 vx_foundation PUBLIC 链接已提供 ${PROJECT_SOURCE_DIR} include path
- **边界情况（TASK-20260419-01）**：当 B 不只是 include A 的头，还**实际调用**了 A 的符号（如 `dom_bindings.cc` 调用 `EnumValueToCssString`）→ 必须显式 `target_link_libraries(B PRIVATE A)` 形成双向 link。CMake 对静态库循环可解（archive 重复扫描），但需要双向 link 才能让 linker 解析符号。

## 已验证的模式（来自 TASK-20260418-01 消化关键技术债务）

### DomBindings pimpl + JSContext opaque 桥接模式
- `dom_bindings.h` 零 `quickjs.h` 依赖：仅 `struct JSContext;` 前向声明
- `DomBindings` 成员只剩 `std::unique_ptr<InstanceData> data_`；`InstanceData` 前向声明放 `public:`（供 `.cc` 匿名 namespace 内自由函数命名），定义放 `.cc` 并 `private` 成员
- `~DomBindings()` 必须在 `.cc` non-default 定义，否则不完整类型 delete 失败
- `friend struct DomBindingsInternal;` + 匿名 namespace 内 `DomBindingsInternal::Data(b)` 桥接，不扩 public API
- `Bind` 时 `JS_SetContextOpaque(ctx, this)`；所有 QuickJS C 回调通过 `JS_GetContextOpaque(ctx)` 拿回实例
- 适用场景：需要把 C++ 实例与 `JSContext` 绑定但又不想泄漏 quickjs 类型到 header 的任何集成

### QuickJS JSClassID 幂等注册模板
- `JSClassID` 是 **进程/runtime 全局** 常量，不是实例状态
- 必须用文件级 `static`（如 `s_element_class_id`），绝不嵌入 `InstanceData`
- 幂等注册模板：
  ```cpp
  if (s_class_id == 0) JS_NewClassID(rt, &s_class_id);
  if (!JS_IsRegisteredClass(rt, s_class_id)) {
    JS_NewClass(rt, s_class_id, &class_def);
  }
  ```
- `JS_IsRegisteredClass` 见 quickjs-ng v0.14.0 `quickjs.h`

### JS 回调生命周期（lambda-vs-JSValue 同寿命）
- C++ 代码注册 event listener 时 lambda 捕获 `JSValue callback`——lambda 拷贝数量 = 引用数
- 销毁顺序硬约束：**先** 销毁所有 lambda 拷贝（`EventManager::RemoveEventListeners`）**再** `JS_FreeValue(callback)`
- 维护 `listener_elements: Vector<Element*>` 去重记录已绑定元素，`Unbind` 反向清理
- `removeEventListener` 必须同步从该向量移除，避免 `Unbind` 重复操作

### hb_font_t 缓存模式（per-handle）
- `hb_ft_font_create_referenced(FT_Face)` 是重操作；在 `FontManager::FontEntry` 内缓存
- 调用方契约：先 `FT_Set_Pixel_Sizes(face, 0, new_size)` 再 `GetHbFont(handle, new_size)`
- 同 handle 同 size → 直接返回缓存指针
- 同 handle 异 size → `hb_ft_font_changed(hb_font)` 重读 metrics，指针稳定
- `Shutdown` 时先 `hb_font_destroy` 再 `FT_Done_Face`（防某些 HarfBuzz 版本 UB）

## 已验证的模式（来自 TASK-20260419-01 流程规则沉淀 + P2 收口）

### u64 不透明句柄模式（ListenerToken / DestructionObserverToken）
- 当外部需要"精确撤销"已注册的资源（监听器、观察者、定时器、动画），使用单调递增 `u64` token 作为不透明句柄
- 注册接口返回 token（`ListenerToken AddXxx(...)`），撤销接口接受 token（`RemoveXxxByToken(Element*, ListenerToken)`）
- token 0（`kInvalidListenerToken`）作为哨兵；未知 token 静默 no-op，不报错
- 内部实现用 `Vector<Entry>` 线性查找（小规模 OK）或 `HashMap<Token, Index>`（>100 listener 时考虑）
- 与现有 `JSValue` 句柄风格一致；与 SetInvalidationCallback "失效推送" 模式互补
- 已复用 2 次：`EventDispatcher::ListenerToken`（B7）、`EventManager::DestructionObserverToken`（B6）

### 反向析构观察者模式（DestructionObserver）
- 当 A 持有 B 的非拥有指针（`B*`），但 A 与 B 的析构顺序无法静态保证（如 host 应用决定）
- B 在析构函数中触发观察者列表（A 在构造时通过 `B::AddDestructionObserver(callback)` 注册）
- A 在析构函数中调用 `B::RemoveDestructionObserver(token)` 注销，避免回调已死亡的 A
- 观察者快照在析构开始时拷贝，遍历期间允许 callback 内调用 RemoveObserver（不影响本次 fire）
- 适用场景：双向依赖的子系统、跨模块生命周期解耦（DomBindings ↔ EventManager）

### CMake 静态库 PUBLIC/PRIVATE 依赖传播准则
- 静态库 `A.a` 不捆绑其 `PRIVATE` 依赖；链接 `A` 的下游目标 `T` 不会自动获得 `B` 的头/符号
- 下游需 include 或间接依赖 `B` 符号时，`A` 必须 `PUBLIC` 链接 `B`
- 不要在下游 `CMakeLists.txt` 用 `pkg_check_modules` 补链接——`pkg_check_modules` 不应跨文件重复调用同一模块（`FindPkgConfig: Unknown arguments specified`）
- 正确动作：在提供方升 `PRIVATE → PUBLIC`

## 已验证的模式（来自 TASK-20260419-02 Benchmark 集成）

### 性能基准目录结构（一文件一可执行）
- 每个被测子模块对应一个 `benchmarks/bench_<module>.cc`，独立编译为单 exe，输出至 `${CMAKE_BINARY_DIR}/benchmarks/`
- `benchmarks/CMakeLists.txt` 提供 `vx_add_benchmark(name)` 函数封装 `add_executable + target_link_libraries(... vx_<lib> benchmark::benchmark) + RUNTIME_OUTPUT_DIRECTORY`
- 新增模块只需 `bench_xxx.cc` + 一行 `vx_add_benchmark(bench_xxx)`
- 崩溃隔离：单 exe 故障不影响其他基准
- 适用：`bench_allocators / bench_containers / bench_hash_map / bench_strings`（已落地 4 个）；规划中 `bench_css_*` / `bench_layout_*` / `bench_render_*`

### 第三方头文件 SYSTEM 隔离 -Werror
- 我方 target 全局 `-Werror -Wpedantic`（限定 `$<COMPILE_LANGUAGE:CXX>`），第三方头通过 `INTERFACE_INCLUDE_DIRECTORIES` 传播会被打到我方编译命令上触发 warning-as-error
- 标准做法：在引入第三方 target 后，把其 include 目录改为 `INTERFACE_SYSTEM_INCLUDE_DIRECTORIES`，下游所有 target 自动以 `-isystem` 形式 include，warning 全部抑制
  ```cmake
  get_target_property(_inc <real-tgt> INTERFACE_INCLUDE_DIRECTORIES)
  if(_inc)
    set_target_properties(<real-tgt> PROPERTIES
      INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_inc}")
  endif()
  ```
- 注意：`<real-tgt>` 必须是真实 target 名，不是 ALIAS（如对 `benchmark::benchmark` 操作会报 "ALIAS target 不可写"）
- 已应用：`benchmark`（TASK-20260419-02）；下次接入新 FetchContent C++ 库可复用

## 已验证的模式（来自 TASK-20260419-04 GCC IPA `-Warray-bounds` 修复）

### 数组+长度对的现代 C++ 习惯：宏化非模板查表

**问题模式：** 想给 N 个不同长度的常量数组写一个统一的「越界检查 + 索引取值」工具函数，最自然的写法是模板取数组引用：

```cpp
template <usize N>
StringView Lookup(const char* const (&table)[N], u16 v) {
  if (v >= N) return StringView();
  return ...;
}
```

调用点：`Lookup(kDisplay, v)` — 编译器自动推导 N，无失配风险。看起来很优雅。

**陷阱：** GCC 11+ 的 IPA（过程间分析）会按每个不同 N 分别 clone 这个模板特化（5 个 N 就 5 个 clone）。然后在 `-O2` 下做跨 clone 的值域传播时，可能将 clone-A 的访问路径与 clone-B 的数组类型元数据错误关联，得出「partly outside array bounds」的误判。`if (v >= N) return` 这层防御对 IPA 不可见。一旦项目开 `-Werror=array-bounds`，Release 构建直接挂掉，但 Debug `-O0` 完全不触发，问题潜伏到下游 target 链接才暴露。

**解法：** 拒绝模板，改宏 + 非模板辅助函数：

```cpp
StringView LookupImpl(const char* const* table, std::size_t n, u16 v) {
  if (v >= n) return StringView();
  ...
}
#define VX_LOOKUP(arr, v) LookupImpl((arr), std::size(arr), (v))
// 调用：VX_LOOKUP(kDisplay, v)
// TU 末尾：#undef VX_LOOKUP
```

**关键设计：**
1. 单一非模板 `LookupImpl` — 没有 clone，IPA 无法 mis-correlate
2. 宏用 `std::size(arr)` 自动派生长度 — 调用点写法跟模板版一样简洁，arr 与 size 不可能脱节（解决"显式传 size 易失配"这一传统反对意见）
3. 宏 `#define`/`#undef` 严格限制在 TU 内（.cc 文件末尾），不通过 header 泄漏

**适用场景：** 任何「N 个不同长度的同型常量数组 + 统一访问/检查逻辑」的查表函数。已应用：`veloxa/core/css/enum_serialization.cc`（13 个 enum 表 / 5 种长度）。

**反向教训：** 引入 `template<usize N>` / CRTP / SFINAE 等模板特化技巧时，必须在 PR 阶段验证 Release `-O2 -Werror` 通路 — Debug 验证不充分（已固化为 `activeContext.md` 待处理事项 P1）。

## 已验证的模式（来自 TASK-20260419-03 Round 1 CSS 基准 — 多 phase 任务工作流）

### 长任务暂停后 rebase main 时 MB 三件套冲突的标准处理法

**问题模式：** Level 2+ 长任务（多 phase / 多 commit）暂停期间，main 上其他任务的 plan/commit/archive 活动会持续触摸 Memory Bank 三件套（`activeContext.md` / `tasks.md` / `progress.md`）。续接时 `git rebase main` 必触发 MB 文件冲突，因为：
- feature 端：plan commit / WIP commit / pause commit 各自摸过这 3 个文件，反映「该 task 在该时点的视角」
- main 端：归档/合并/重置活动留下「此时点 idle 状态」的 MB 真相

**反例（容易踩的坑）：**
- 手动逐处合并冲突，把 feature 端的「过期任务视角」与 main 端的「当前 idle 真相」拼起来 — 产生混乱的 MB 状态，需要后续 cleanup commit
- `git rebase --abort` 改用 merge — 历史变非线性，且冲突不会少

**标准处理法：**
```bash
git checkout feature/TASK-X
git rebase main

# 每次 MB 冲突（plan / WIP / 其他 chore commit）：
git checkout --ours memory-bank/activeContext.md \
                    memory-bank/tasks.md \
                    memory-bank/progress.md
git add memory-bank/
git rebase --continue

# pause commit（chore(mb): pause TASK-X pending TASK-Y）专门处理：
git rebase --skip   # 因为 pause 信息已无意义，TASK-Y 已解锁

# rebase 完成后单独写一个续接 commit：
# chore(mb): resume TASK-X — phase to BUILD after TASK-Y unblock
```

**关键原则：**
1. **MB 三件套总是接受 main 端**（`--ours` 在 rebase 上下文 = main side）— 因为归档后的 idle 状态总是更新真相
2. **feature 端的 plan/WIP/pause 视角已通过 commit 自身 + plan/spec 文档保留** — 不需要 MB 文件再次承载
3. **pause commit 一律 skip** — 它的整个语义就是「依赖 TASK-Y」，依赖解锁后 commit 失去存在意义
4. **rebase 完成后写续接 commit** — 一次性把"当前要做什么"重写到 MB，不要试图逐 commit 修复

**适用场景：** 任何被 pause 后被 main 上其他活动追上的长任务续接。已应用：TASK-20260419-03（被 TASK-04 中断后续接）。

### 多 phase 任务的「轮次完成」工作流（待固化到规则）

**问题模式：** Level 2+ 任务（phase 数 ≥ 5）天然适合分轮次 build — 用户可能在某些 phase 之间想中断做其他高优先级任务，或者只想拿"最直接收益"的几 phase。但当前 `/build` → `/reflect` → `/archive` 命令链假设「一轮即完成」：
- `/reflect` 把阶段从「构建中」切到「回顾中」
- 「回顾中」隐含 invariant 是「下一步只能 `/archive`」
- 但任务还没整体完成，archive 不行；下轮 `/build` 又要把阶段切回「构建中」 — 违反隐含 invariant

**当前临时缓解（TASK-03 Round 1 已用）：**
- Reflection 文件命名 `reflection-TASK-X-roundN.md`（明确标 round 号，避免后续 round 覆盖）
- `activeContext.md` 阶段字段写「回顾中（TASK-X 第 N 轮已 reflect；任务整体未完成；不可 /archive）」
- 下轮 `/build` 启动时，命令文档的阶段守卫会拒绝；需要先手动把阶段切回「构建中」（或者 `/build` 自身识别此状态自动切）

**长期方案（P1 改进建议，见 `activeContext.md` 待处理事项）：**
- `complexity-levels.mdc` 增加「多轮次 Build 工作流」段
- 命令阶段守卫识别「构建中（轮次N完成，待续）」中间态
- 或引入 `/round` 命令显式标记轮次切换

**适用场景：** Level 2+ 多 phase 任务（典型 phase 数 ≥ 5）的分轮次实现。已应用：TASK-20260419-03（计划 6 phase，第 1 轮做了 0+1+2，第 2 轮做 3-6）。

### Plan「Phase 表」附加「轮次切分建议」列

**问题模式：** Plan 阶段的 Phase 表通常只列「目标 / 任务 / 验收 / 提交」，不告诉读者「哪几 phase 适合一轮做完、哪些适合分轮做」。结果是 BUILD 阶段时用户只能凭直觉/经验决定本轮做哪几 phase，容易要么做太少（频繁切换上下文）要么做太多（疲劳/失误）。

**改进做法（建议固化到 `writing-plans.mdc`）：** Phase 表追加「轮次切分建议」列：

| Phase | 内容 | 验收 | 提交 | 轮次切分建议 |
|-------|------|------|------|------------|
| P1 | scaffold | 编译过 | feat | 与 P2 同轮（验证 + 实施一气呵成） |
| P2 | 完整套件 | 10 BM 行 | feat | 与 P1 同轮 |
| P3 | parser 套件 | 11 BM 行 | feat | 单独 1 轮（独立模块） |
| P4 | property + cluster | 9 BM 行 + 度量 | feat | 与 P5/P6 同轮（依赖 P3 数据） |
| P5 | README | 文档审查 | docs | 与 P4 同轮 |
| P6 | baseline JSON + 收尾 | 全量回归 | feat | 与 P4/P5 同轮（收尾批次） |

**收益：** 用户和 BUILD 模式的 AI 都能看到清晰的「单元」边界，避免本轮范围决策的反复试探。

## 已验证的模式（来自 TASK-20260419-03 整体回顾 — 基准设计 + 失真兜底）

### 入仓基线数字的失真兜底设计

**问题：** 基准 JSON 数字与硬件 / OS 调度器 / 编译器版本强相关，跨机漂移可达 2-10×；但完全不入仓又失去「形态参考锚」。

**模式：** 入仓 baseline JSON 时**强制配套**独立 README 含以下 4 段（缺一不可）：

1. **失真警告 4 条**：枚举漂移来源 + 不可作 SLA / CI 卡点 + 正确用法 = 同机相对比较
2. **当前生成环境表 4-8 行**：host_name / OS / CPU / 编译器 / google/benchmark 版本 / 构建模式 / `--benchmark_min_time` / 生成日期 — 任何 baseline 更新必须同步刷新
3. **更新协议 5 条硬规**：(a) 算法/数据结构/hash 函数变更才更新（**禁止**rebase 后小幅波动入仓）；(b) Release 独立 `build-bench/`；(c) `--benchmark_min_time=0.5s`；(d) 同步刷新环境表；(e) PR 描述写明触发原因 + BM 量级变化 + `compare.py` 对比输出
4. **命令模板**：Release rebuild + JSON 体检（验证 `context.library_build_type=="release"`）+ `compare.py` 同机对比工作流

**适用场景：** 任何性能基准 JSON / 基准数据入仓。已应用：TASK-20260419-03 入仓 3 份 CSS baseline JSON + `benchmarks/baseline/README.md`（30 BMs，~15 KB JSON）。

**反模式（要避免）：** 入仓 baseline JSON 但不写失真说明 → 后续维护者拿数字做 ±10% CI 卡点 → 大量假阳性 → 卡点被禁用 → baseline 形同虚设。

### 「带否定判据的发现型 phase」的 plan 模板

**问题：** 部分 phase 的目的是「实测可能否定预期假设」（如 cluster 度量），plan 阶段需要预设两路结果对应的下一步动作；否则实测否定预期时容易陷入「这数据不对吗？」的犹豫。

**模式：** plan §风险预案表中对该 phase 必须有一行写出**两路结果的明确动作**：

```
| Phase X 实测数据偏离预期假设 | <具体阈值条件> | 接受结果；在回顾文档把『<预期被否定的具体表述>，<下游 task 优先级调整>』作为产出 |
```

**已验证样本：** TASK-20260419-03 Phase 4 cluster 度量，plan 风险预案 row 2 写明：「HitSingle 各 key 与 HitHot5 量级差 < 2× → 接受结果；在回顾文档把 'PropertyMap 实测均匀，TASK-05 优先级降为 P3' 作为产出」。实测确实未触发，预案直接照搬，零纠结决策出 TASK-06 P1→P3 降级。

**适用场景：** 「立项时基于已有发现假设新问题存在 → 用基准/测量证伪/证实」型 phase；常见于性能优化候选项的实证降级、技术债假设验证、API 性能特性探查。

**反模式：** 仅写「Phase X 实测可能不符合预期」而不写两路动作 → 实测否定预期时只能临时决策。

## 已验证的模式（来自 TASK-20260419-07 — GCC IPA `-Warray-bounds` 「阻断关联」元模式）

### GCC `-Warray-bounds` 误报 3 阶段诊断与修复表

**问题**：GCC 在 `-O2 -Werror=array-bounds` 下对模板查表、字符串拷贝、容器内存操作等场景频发误报，错误消息形态相似但**根因分散在 3 个编译阶段**，方案选错则白做工。

**诊断方法（30 秒判定）**：

| 发出阶段 | 错误消息特征 | 验证方法 | 适用方案族 |
|---------|------|---------|---------|
| **前端 / 解析期** | 数组定义点的明显越界（编译期常量下标） | 看错误位置是否在数组定义行附近 | 改下标 / 改大小 |
| **中端 IPA** | 含「forming offset [X, Y] is out of the bounds [0, Z] of object」+ 报错位置在**调用点**而非定义点 | 把 `std::memcpy` 改 `__builtin_memcpy`：消息函数名变 `__builtin_memcpy` 仍报 → 100% IPA | **阻断 IPA 关联**（首选治本） |
| **后端 fortify** | 含 `__builtin___memcpy_chk` / `__strncpy_chk` / `__glibc_objsize0` | 同上验证：消息消失 → fortify | `__builtin_xxx` 绕过 fortify wrapper |

### 「阻断 IPA 关联」方案族（中端 IPA 误报的治本手法）

GCC IPA pass 通过**跨函数推导指针 / 数组的可能范围**触发误报。修复元原理是**在某个边界强制 IPA 停止跨函数推导**：

| 手法 | 适用场景 | 实例 |
|------|---------|------|
| **去模板化** + `std::size()` 运行时取大小 | `template<usize N>` 取数组引用，多 N 值 clone 跨实例化 IPA 传播 | TASK-04 `Lookup<N>` → `LookupImpl + VX_LOOKUP` 宏 |
| **`[[gnu::noinline]]` 边界化函数视野** | 函数被内联到调用点后 IPA 关联对象内部布局与运行时大小 | TASK-07 `BasicString` 拷贝构造 noinline |
| **隐藏指针来源**（如 `asm volatile("" : "+r"(p))` barrier） | 指针 union 类型 / 多源汇合导致 IPA 误关联 | 暂未实例化 |

**判定优先级**：先选「阻断关联」族（治本，不留 `#pragma` 污染），再选「警告抑制」族（`#pragma GCC diagnostic ignored` / `-Wno-`）。后者只在阻断方案确实成本更高时使用。

**代价权衡**：
- 去模板化：失去模板的调用点简洁性，需配套 helper（如 `VX_LOOKUP` 宏）补回 ergonomics
- noinline：损失内联开销（~1-2ns indirect call）；只对**冷路径**或**包含较重操作**（分配 / 系统调用 / 大循环）的函数适用

### 候选方案表「根因验证步骤」要求

VAN / BUILD 阶段写候选方案表时，每个方案应附**一行根因验证操作**（grep / 读 1 个头 / 跑 1 个 build），而不是直接靠错误消息表面假设根因。

**反模式**（TASK-07 实测）：
- VAN 假设「`-Warray-bounds` 来自 `__memcpy_chk` fortify wrapper」 → 推荐 B3 `__builtin_memcpy`
- BUILD 试验失败 → 才发现根因是 IPA（先于 fortify 展开）
- 若 B3 行附「先 grep `string_fortified.h` 确认 chk 是诊断展示层还是诊断源头」，30 秒可排除

**落实**：候选方案表 ≥ 3 项时强制要求；2 项以内可豁免（决策成本与验证成本接近）。

### 「带否定判据的发现型 Phase」方法论（TASK-03 + TASK-05 双实证）

**适用场景**：性能 bench / 正确性边界探查类任务，存在「假设 X 是瓶颈/问题」需要量化证伪或证实的情况。

**4 要素**：
1. **Hypothesis**：明确写出「X 是 hot path / 触发 cluster / 走慢路径」
2. **Threshold**：写出"超过 Y× 即视为命中"（TASK-03 / TASK-05 都用 5×；与 google/benchmark 默认稳态噪音 ~5% 远超）
3. **对比组**：在同一 BM 文件内构造 baseline + suspect 双 BM（必要时设 single-key probe）
4. **接受任意结果**：方法论的价值不在"证实 X 是问题"，而在"任意结果都得到量化结论"

**已成功应用 2 次**：
| Task | Hypothesis | 结果 | 后续动作 |
|------|-----------|------|---------|
| TASK-03 P4 | PropertyMap djb2 触发 cluster | ❌ 否（最慢 single key 仅 2.75× HitHot5）| TASK-06 P1→P3 降级 |
| TASK-05 P6 | ImageCache Get 是 Replay hot path | ❌ 否（image 路径未触发，但**意外发现 DrawText 是 820× hot path**）| 立 TASK-09 候选 |
| TASK-09 P3 | DrawText 8200 ns/cmd 是 FreeType+HarfBuzz 真路径慢 | ❌ 否（实际是 fallback；真根因是 FT_Load+FT_Render 冷路径 9.1×）| K1' 修正归因写入 baseline + techContext |
| TASK-09 P2 | ImageCache 真路径整体瓶颈在 Decode | ❌ 否（**意外发现 Load hit 路径 O(N) 字符串扫，size=256 时 1162 ns 比 ReplayImageReal<16> 还慢**）| K6 推 HashMap 改造为高 ROI 候选 |

**关键观察**：方法论 4 次都在"否定原假设"的同时**意外定位真正的问题**（cluster 度量发现 djb2 + 短字符串场景免疫；hot path 度量发现 DrawText 才是瓶颈；真路径冷热对比定位 FT_Load+FT_Render；ImageCache size 扫描定位 O(N) Load）— 这是 plan 阶段单纯靠 grep / 推理无法获得的认知跳跃。

**落实**：写入 `writing-plans.mdc`「Phase 类型」附录段；任何性能/正确性边界 phase 都应套用这个 4 要素清单。

### Render Bench 前置清单（TASK-05 K1+K4 教训）

`vx::render::RecordBox`（`renderer.cc:56-148`）对 paint 命令的 emit 全部带 gating，bench corpus 必须满足下列条件才能产出非空 DisplayList：

| Paint 命令 | 触发条件（缺一即 emit 跳过） |
|-----------|---------------------------|
| `FillRect` | `style->background_color.a > 0` AND `border_radius == 0` |
| `FillRoundedRect` | `style->background_color.a > 0` AND `border_radius > 0` |
| `StrokeRoundedRect` | `border_radius > 0` AND `border_style[0] != kNone` AND `border[kTop] > 0` |
| `DrawText` | `box->type == kText` AND `box->text_node != nullptr` AND `text_data.empty() == false`；**且**真路径需 `SoftwareCanvas` ctor 同时传 `&FontManager + &GlyphCache`，否则走 `DrawTextFallback`（每字符 1 个 FillRect，TASK-09 K1' 教训）|
| `DrawImage` | `box->type == kReplaced` AND `box->image_handle != 0`（**需 layout 时 ctx.image_cache 非空**）|
| `PushClipRect` | `style->overflow == kHidden` |
| `PushLayer` | `style->opacity < 1.0f` |

**三个隐式契约**（极易踩坑，TASK-05 Phase 6 全空 list 假阳性来源 + TASK-09 phase-3 fallback 误测来源）：
1. **inline declaration 须经 StyleResolver 才生效** — `LayoutContext::stylesheets` 必须非 null（即使是空 Vector）；nullptr 时 LayoutEngine 走默认 ComputedStyle，inline `background-color` 直接被忽略
2. **ImageCache 是三阶段管道契约** — layout（生成 handle）/ Record（读 handle emit cmd）/ Replay（拿 handle 取 image）三阶段必须同传 cache 指针；任一传 nullptr 链断、image 路径完全测不到
3. **DrawText 真路径需 `SoftwareCanvas` ctor 显式传 `&FontManager + &GlyphCache`**（TASK-09 K1' 教训）— 任一为 nullptr 则走 `DrawTextFallback`（每字符 1 个 FillRect，性能特征与真路径完全不同）。bench 名命名应明确区分 `Fallback_*` 与 `Real_*` 避免 K1 类归因错误

### 跨阶段管道型 API 的 default-nullptr 反模式（TASK-05 K5 教训）

`vx::render::Record(root, image_cache=nullptr)` 与 `vx::render::Replay(list, canvas, image_cache=nullptr)` 的 default nullptr 在管道中传递隐式语义：

- 看似 cache **可选**（开发者印象：「不传就降级行为」）
- 实际 cache **必需**（不传则 layout 阶段 image_handle 永远为 0 → Record 不 emit DrawImage → Replay 没 cmd 可分发）

**反模式的本质**：管道型 API 的 default 参数应表达"完整路径 vs 短路路径"，但当前实现把"短路"当默认值，对调用方完全不透明。

**改进方向**（候选）：
- (a) 把 cache 改为非 default（必传），强迫调用方显式决策
- (b) 注释里明文标注"三阶段必须同传"
- (c) 在 dev 模式下加 `DCHECK(image_cache != nullptr || box 中无 image)` 一致性检查

**警示用途**：未来 Veloxa 引入新的 cross-phase pipeline（如 GPU 命令 buffer / accessibility tree / animation context），API 设计 review 时应**禁止把可空指针作为 default**，避免重蹈覆辙。

### Bench Smoke 自检模式（TASK-05 教训：避免空 list 假阳性）

性能 bench 的 phase 1 smoke 验收**仅靠「数字非零」不充分**：

- ❌ **假阳性案例**：`BM_ReplaySmoke = 1.65 ns + items_per_second=0/s` —「数字非零」但实际 list 为空，跑的是 `for (auto& cmd : empty_vector)` 的 range-for 进出开销
- ✅ **改进 smoke 验收三件套**：
  1. **数字非零**（基础）
  2. **`SetItemsProcessed(N)` 或 `SetBytesProcessed(N)` 给非零 N**（强迫 bench 作者写 `state.SetItemsProcessed(state.iterations() * list.size())` 之类，把"非空"语义入到 metric 中）
  3. **跑 smoke 后用 `--benchmark_format=json | jq .benchmarks[].items_per_second` 校验全 > 0**

**落实**：写入 `writing-plans.mdc`「性能基准任务必检项」段；下次任何 bench plan 必加。

## 待定架构决策
- [x] CSS 支持的具体子集范围 → 已确定：~45 属性（布局/Flex/视觉/文本）+ 4 transition 属性
- [ ] 是否内置 SVG 支持
- [x] 动画系统的实现策略 → CSS Transitions 已实现（TASK-13），CSS Animations（@keyframes）待定
- [ ] 资源加载策略（打包 vs 文件系统 vs 混合）
- [ ] 多进程 vs 单进程架构
