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

## 待定架构决策
- [x] CSS 支持的具体子集范围 → 已确定：~45 属性（布局/Flex/视觉/文本）+ 4 transition 属性
- [ ] 是否内置 SVG 支持
- [x] 动画系统的实现策略 → CSS Transitions 已实现（TASK-13），CSS Animations（@keyframes）待定
- [ ] 资源加载策略（打包 vs 文件系统 vs 混合）
- [ ] 多进程 vs 单进程架构
