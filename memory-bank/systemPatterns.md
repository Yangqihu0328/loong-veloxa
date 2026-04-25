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

### 「带否定判据的发现型 Phase」方法论（TASK-03 + TASK-05 + TASK-09×2，**4/4 成熟实践**）

**状态**：**已成熟实践**（4/4 成功率，全部"否定原假设 + 意外定位真问题"）— TASK-09 reflection 升级，删除原"双实证"措辞。

**适用场景**：性能 bench / 正确性边界探查类任务，存在「假设 X 是瓶颈/问题」需要量化证伪或证实的情况。

**4 要素**：
1. **Hypothesis**：明确写出「X 是 hot path / 触发 cluster / 走慢路径」
2. **Threshold**：写出"超过 Y× 即视为命中"（TASK-03 / TASK-05 / TASK-09 都用 5×；与 google/benchmark 默认稳态噪音 ~5% 远超）
3. **对比组**：在同一 BM 文件内构造 baseline + suspect 双 BM（必要时设 single-key probe / cold-warm 对比 / size 扫描）
4. **接受任意结果**：方法论的价值不在"证实 X 是问题"，而在"任意结果都得到量化结论"

**已成功应用 4 次**：
| Task | Hypothesis | 结果 | 后续动作 |
|------|-----------|------|---------|
| TASK-03 P4 | PropertyMap djb2 触发 cluster | ❌ 否（最慢 single key 仅 2.75× HitHot5）| TASK-06 P1→P3 降级 |
| TASK-05 P6 | ImageCache Get 是 Replay hot path | ❌ 否（image 路径未触发，但**意外发现 DrawText 是 820× hot path**）| 立 TASK-09 候选 |
| TASK-09 P3 | DrawText 8200 ns/cmd 是 FreeType+HarfBuzz 真路径慢 | ❌ 否（实际是 fallback；真根因是 FT_Load+FT_Render 冷路径 9.1×）| K1' 修正归因写入 baseline + techContext |
| TASK-09 P2 | ImageCache 真路径整体瓶颈在 Decode | ❌ 否（**意外发现 Load hit 路径 O(N) 字符串扫，size=256 时 1162 ns 比 ReplayImageReal<16> 还慢**）| K6 推 HashMap 改造为高 ROI 候选 |

**关键观察**：方法论 4 次都在"否定原假设"的同时**意外定位真正的问题**（cluster 度量发现 djb2 + 短字符串场景免疫；hot path 度量发现 DrawText 才是瓶颈；真路径冷热对比定位 FT_Load+FT_Render；ImageCache size 扫描定位 O(N) Load）— 这是 plan 阶段单纯靠 grep / 推理无法获得的认知跳跃。

**落实**：写入 `writing-plans.mdc`「Phase 类型」附录段；任何性能/正确性边界 phase 都应套用这个 4 要素清单。**TASK-09 reflection 阶段：方法论标记为成熟实践（4/4），下次同类 phase 默认套用，不再标"实验性"。**

### bench 类任务估时校准（TASK-05 + TASK-09 双实证 4× 高估）

**现象**：连续两次 bench 类任务 plan 估时与实际严重偏离：
| 任务 | Plan 估 | 实际 | 比率 |
|------|--------|------|------|
| TASK-05 | 4.25h | 75 min | **3.4×** |
| TASK-09 | 3.5h | ~50 min | **4.2×** |

**根因**：bench 基础设施（`layout_corpus.h` / `vx_add_benchmark` / google/benchmark FetchContent / smoke 三件套 / baseline JSON 协议 / `library_build_type=release` 体检）已成熟，"复用率 ≥ 80%"；但 plan mental model 仍按"从零写一个 bench exe"计算，导致 3-4× 系统性高估。

**改进协议**（下次 bench 任务 plan §0 必填）：
- **复用率假设**：layout_corpus.h X% / vx_add_benchmark 100% / smoke 三件套 100% / baseline JSON 协议 100%
- **单 BM 实际编写时间** ：3-5 min（成熟期），非 10-15 min（从零）
- **单 phase 估时基线**：phase = 4-8 BMs → 30-50 min；phase = 文档 / baseline → 20-30 min

**触发升级**：如下次 bench 任务再次 > 3× 高估，升级到 `writing-plans.mdc` 强制条目。

**TASK-11 实证（2026-04-19）**：plan 55-80 min vs 实际 ~35-40 min = **~1.5-2.0×**，**protocol 首次生效**。偏差从 TASK-05/09 的 4× 收敛到 ~2×，距离"准确档"差 1 次校准；如下次 bench 任务 ≤ 1.5× 即可视为收敛，写入 `writing-plans.mdc` 作为基线。

**TASK-13 跨类型实证（2026-04-19）**：plan 85-95 min vs 实际 ~51 min = **~1.67-1.86×**（**文档类任务**）。数据点合并：

| 任务 | 类型 | 倍率 |
|------|------|:-:|
| TASK-05 | bench | 3.4× |
| TASK-09 | bench | 4.2× |
| TASK-11 | bench | 1.5-2.0× |
| TASK-13 | **文档** | **1.67-1.86×** |

**跨类型收敛结论**：偏差稳定落在 **~2×** 区间，与任务类型无关。**通用目标倍率：plan × 0.6**（警戒线 plan × 0.4 视为极端偏差需调查）。应用场景从"bench 类专属"扩展为**所有任务类型的 plan 估时校准基线**。下次任一类型任务 ≤ 1.5× 即视为"准确档"，写入 `writing-plans.mdc` 作为强制条目。

**TASK-24-01 极端数据点（2026-04-24）**：plan 115 min / 实测 ~33 min = **0.29×**（历史最快档）。首次突破 ≥1.5× 下限 → 触发**「路径宽度」子档分化**：

| 路径宽度 | 样本 | 典型倍率 | 预估指南 | 识别特征 |
|---|---|:-:|---|---|
| 全新系统 | TASK-05 / TASK-09 | 3-4× | plan × 0.25-0.3 | 首次集成某类基础设施，多文件 + CMake 新 target |
| 中等（2-5 文件） | TASK-11 / TASK-13 | 1.5-2× | plan × 0.5-0.6 | 扩展已有基础设施 / 文档沉淀 / 单模块重构 |
| **最窄（1-3 文件 + 100% 基础设施复用）** | **TASK-24-01 / TASK-24-03 / TASK-24-04** | **0.26-0.34×** | **plan × 0.3** | 单行代码修改 + 1 GTest + baseline 刷新；无 CMake 变更；脚本化扫描可替代手工实验（TASK-24-04 多文件但 100% 复用既有基础设施 → 仍入此档） |

**「最窄路径」识别清单**（满足全部 → 按 plan × 0.3 预估）：
- 核心改动 ≤ 3 文件
- 测试基础设施 100% 就绪（新增测试复用既有 test target，无新 CMake）
- 实验阶段可脚本化（for 循环 + python3 聚合）
- 无新第三方依赖 / 无 CMake 新 target / 无 FetchContent
- 无 API 变更 / 无接口破坏

**触发条件**：下次若识别出「最窄路径」特征而 plan 估时按「中等」档（1.5-2×）给出 → plan 阶段直接下调预算 40-50%；反之若发现意外耗时膨胀需立即切换诊断模式。

### bench plan 阈值表对超低 ns BM 的「绝对增量兜底」附录（TASK-11 反思 #1）

**问题**：`< baseline × 1.2` 形式阈值在超低 ns BM 上对测量噪声极敏感，触发误警。

**TASK-11 实证两例**：
- `BM_ImageCacheGet` 0.94 → 1.16 ns = 1.23×（超 1.2×），但 Get 实现完全没改 — 纯属 0.22ns 噪声 / cache 抖动
- `BM_ImageCacheLoad_Hit<1>` 10.35 → 43.27 ns = 4.18×（超 1.2× 严重），但绝对回归 32ns 是 HashMap djb2 + probe 固有开销，绝对量微小且被 Hit<256> 1106ns 净增益完全压倒

**改进公式**（追加到任何 bench plan 阈值表）：
```
判定阈值 = max(baseline × 1.2,  baseline + 0.5ns)   for baseline < 5ns      （超低 ns，噪声敏感）
判定阈值 = max(baseline × 1.2,  baseline + 5ns)     for baseline ∈ [5, 50)ns（低 ns，HashMap/probe 量级）
判定阈值 = baseline × 1.2                            for baseline ≥ 50ns     （常规判定）
```

**关键判定原则**：性能改造任务的"主目标 BM"（本次 Hit<16>/Hit<256>）必须严格用乘法判定；"附带退化检查 BM"（本次 Get/Hit<1>/Miss）应用绝对增量兜底，避免噪声/固有开销误警阻塞合并。

**写入待处理事项**：下次 bench plan 阈值表必引此公式（`activeContext.md` P1）。

### Mixed TDD RED 反向探针实践（TASK-11 反思 #3 — 新模式）

**适用场景**：mixed TDD 模式下（用户协议或 plan §0 明示「先实现后测试」），为预防特定 bug 而新增的「回归网测试」（D3 类决策产物）。

**执行步骤**：
1. 写测试 + 实现一起完成（mixed TDD 标准流程）
2. 运行测试 → 应 PASS
3. **反向探针**：临时破坏被测路径的关键一行（注释 / 替换 sentinel 值），重新构建 + 跑该单测
4. **断言 RED**：测试必须立即 FAIL，且 FAIL 行号/断言指向被破坏的具体语义
5. 恢复实现 + 跑全套确认 GREEN

**TASK-11 D3 实证**：
```cpp
// veloxa/core/image/image_cache.cc
void ImageCache::Clear() {
  images_.clear();
  // path_to_handle_.clear();  // RED probe ← 临时注释
}
```
→ `ImageCacheTest.ClearAndReloadDeduplicates` 立即 FAIL（`EXPECT_EQ(h2.value(), 1)` 命中：HashMap 残留 handle 但 vector 已空，1-based 下标错位）
→ 恢复 → PASS
→ 总耗时 < 3 min

**核心价值**：mixed TDD 的最大风险 = 「测试因实现已正确而误以为有效」（实现侧巧合让测试 PASS 但断言其实没真覆盖目标语义）。反向探针强制证明「实现正确 是 测试 PASS 的必要条件」，等价于严格 TDD 的 RED 阶段证据链。

**适用判据**：D3 类「双索引 / 双向同步 / clear 一致性 / 缓存失效 / 转换闭环」等"易漏一处"型 bug 的回归测试**必做**反向探针；普通正向行为测试（LoadAndGet 等）不必。

### 数据结构改造与行为切换分离的 P1+P2 拆分模式（TASK-11 反思 #4 — 新模式）

**适用场景**：低风险性能优化任务，需引入新数据结构替代或补充现有数据结构。

**P1**：仅声明新字段 + 必要的辅助 functor（`Hash` / `Eq` 等模板参数），不切任何 hot path 行为
**P2**：切 hot path 调用方走新结构 + 加 D3 类回归测试

**TASK-11 实证**：
- P1（commit `ae72800`，23 行 + 0 行）：仅在 `ImageCache` 加 `HashMap<String, ImageHandle, StringHash, StringEq> path_to_handle_` 字段 + 2 个 functor 定义；Load/Clear 行为完全不变 → ctest 890/890 PASS 立即验证 F2 类型推导风险闭合
- P2（commit `47ecb1d`，37 行 +/- 6 行）：切 Load 路径用 `Find()` + `Insert()`；Clear 同步两个结构；新增 ClearAndReloadDeduplicates → ctest 891/891 PASS

**关键收益**：
1. **review 风险面独立可量化** — P1 仅看头文件 + 编译/链接；P2 仅看 17 行 cc + 26 行 test
2. **F 类编译风险与 K 类运行时风险解耦** — P1 失败 = 模板特化/类型推导问题（编译期）；P2 失败 = 行为问题（运行期），定位路径互不污染
3. **可独立 revert** — 若 P2 发现性能不达预期可单独 revert；P1 留下的字段未被 Load 路径使用，无副作用

**适用判据**：任务满足「(a) 引入新容器/索引/缓存层 + (b) 改造前后 ABI 不变 + (c) hot path 行为变更可机械替换」3 项时套用；不适用于 API 重构或语义重设计型任务。

**未来候选触发**：Layout `Vector<LayoutBox*> children` HashMap 化 / ResolvedCache 升级 / 任何 O(N) → O(1) lookup 改造。

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

### 规则沉淀类任务的 Meta-dogfooding Phase 0 模式（TASK-13 反思 #1 — 新模式）

**适用场景**：流程规则 / .cursor/rules / .cursor/commands 类文档沉淀任务。

**核心原理**：规则沉淀任务的**最强证据**来自"执行过程本身"；Phase 0 基线验证时**刻意检验新规则的触发路径**，若命中 → 规则获得 T-0 的实时自证。

**执行步骤**：

1. Plan 阶段为每条待沉淀规则识别「触发条件」脚本（如 `command -v jq` / `git config --global --get http.proxy` / `grep FetchContent_Declare`）
2. Build Phase 0「基线验证」步骤并行执行所有触发脚本
3. 记录命中/缺失矩阵到 `progress.md`
4. **命中即记入反例追溯表**作为 T-0 实时证据；未命中按历史反例正常执行
5. 验收阶段合并 T-0 证据到「7/7 通过」类统计

**TASK-13 实证**：
```bash
# Phase 0 基线验证
command -v rg || echo MISS    # MISS → 条目 2 P1 规则 smoke 工具链 grep 的 T-0 自证
command -v jq || echo MISS    # MISS → 同上（TASK-11 P3 历史反例 + 本次 T-0 双证据）
git config --global --get http.proxy  # 已设 → 条目 1 P0 规则的反面证据
grep FetchContent_Declare CMakeLists.txt  # 命中 → 条目 1 触发条件 OK
```

**价值**：
- 节省"人工搜索历史案例"时间（反例追溯变为运行时检出）
- 规则在制定过程中被"压力测试"，若触发路径与规则期望不符 → 立即修正规则文案
- 提供**最新鲜、最权威的证据**（不依赖历史任务反思的主观转述）

**适用判据**：任何"反复出现 ≥ 3 次"的流程痛点被立项为规则沉淀任务时**必做**；偶发单次案例可跳过。

### 基础假设核查 — VAN/Plan 前置清单（TASK-13 反思 #2 — 新模式）

**触发场景**：涉及**非标准目录/约定**（非 `src/` / `tests/` / `build*/`）或**工具链约定**（Cursor 自定义目录、外部 MCP 资源等）的任务，在 VAN/Plan 阶段必做假设核查。

**核查维度**：

| 维度 | 核查方法 | 典型反例 |
|------|---------|---------|
| 目录可编辑性 | `Glob <dir>/**/*` 验证文件是普通 workspace 文件 | TASK-13: `.cursor/commands/*.md` 初期误判只读 |
| 文件格式约定 | `Read` 首 5 行验证 YAML frontmatter / mdc 扩展名语义 | — |
| 约定系统归属 | `grep` / 文档搜索确认 Cursor 官方 vs 项目自定义 | — |
| API 能力边界 | grep 现有调用方 + 签名匹配 | TASK-11 F2 `std::equal_to<String>` 依赖隐式转换 |

**执行时机**：
- **VAN 阶段**：快速 Glob 验证目录类文件类型（< 1 min）
- **Plan 阶段**：对 spec 中每个"假设依赖"标记 `[待 grep 验证]` → plan Phase 0 批量核查

**TASK-13 教训**：初期假设 `.cursor/commands/*.md` 是只读系统提示，范围仅限 `.cursor/rules/*.mdc`。Plan 阶段 Glob 验证发现是普通 workspace 文件，ROI 放大 2-3×（规则从被动文档升级为主动守卫）。

**价值**：避免"整个 plan 都建立在错误假设上"的返工；每次 1-5 min 核查 vs 最坏情况重写 plan。

### 配置管理 — 单一真相来源占位符模式（TASK-13 反思 #4 — 新模式）

**适用场景**：规则 / 文档 / 配置文件引用的**环境敏感 / 跨项目复用 / 可变**配置值（代理地址、端口、token 前缀、路径前缀等）。

**规则**：
- 规则文件中使用**占位符**（如 `<开发环境代理地址>` / `<数据库连接字符串>`）
- **实际值**集中到**单一真相来源**（通常是 `memory-bank/techContext.md` 或项目私有 `.env`）
- 交叉引用链条必须 grep 可验证（至少 2 节点：使用方 ↔ 真相来源）

**TASK-13 实证**：
- 规则文件（`writing-plans.mdc` / `van.md`）使用 `<开发环境代理地址>` 占位符
- 真相来源：`techContext.md` L98「Plan/VAN 阶段守卫」子段
- 验证：`grep -rn "192\.168\." .cursor/` 返回空 → 零硬编码 IP ✅

**Trade-off**：
- **写入时收益**：规则可安全公开、项目间迁移、避免硬编码泄漏
- **阅读时成本**：读规则需跨文件跳读确认实际值
- **判定条件**：
  - ✅ 用：环境敏感 / 跨项目复用 / 可变配置值
  - ❌ 不用：语义稳定值（固定 phase 号、固定 API 名、固定数学常量）

### 计划执行 — 实证微调 spec 范式（TASK-13 反思 #5 — 新模式）

**适用场景**：plan 执行到某 Phase 时，**实证数据**（grep 结果 / 工具 availability / 实际 API 行为）与 spec 中某个**细节**冲突。

**处理流程**：
1. **保留 spec 核心意图**（不改目标 / 范围 / 验收整体意图）
2. **微调具体细节**（工具清单、阈值数字、文案密度等）
3. **在 progress.md 明确说明**："基于 Phase N 实证数据微调 spec §X.Y — <具体偏差描述> → <调整决策>"
4. **验收阶段标记"改进"而非"偏离"**（9 ✅ + 1 改进 格式）
5. 如累计改进 ≥ 3 项，reflection 阶段回看是否 spec 需要结构性更新

**TASK-13 实证**：
- Spec §5.4 原设计"7 常见工具清单 + 4 替代方案分 2 表"
- Phase 0 实证：`rg MISS`（spec 未列）；`convert` 本项目无相关引用（spec 列了）
- 微调决策：合并为"6 工具 + 兜底同表"，加入 rg 替代 convert
- 验收标记 9 ✅ + 1 改进（非偏离），progress.md 明确说明依据

**反模式**：
- ❌ 固执执行 spec 的不精准细节（如遇 rg MISS 仍列 convert）
- ❌ 大刀阔斧改 spec 核心意图（如改变工具兜底表的存在本身）

**价值**：plan 的刚性与实证的灵活性之间的治理机制，避免"plan 是圣经"与"plan 是废纸"两极。

### Bench Smoke 自检模式（TASK-05 教训：避免空 list 假阳性）

性能 bench 的 phase 1 smoke 验收**仅靠「数字非零」不充分**：

- ❌ **假阳性案例**：`BM_ReplaySmoke = 1.65 ns + items_per_second=0/s` —「数字非零」但实际 list 为空，跑的是 `for (auto& cmd : empty_vector)` 的 range-for 进出开销
- ✅ **改进 smoke 验收三件套**：
  1. **数字非零**（基础）
  2. **`SetItemsProcessed(N)` 或 `SetBytesProcessed(N)` 给非零 N**（强迫 bench 作者写 `state.SetItemsProcessed(state.iterations() * list.size())` 之类，把"非空"语义入到 metric 中）
  3. **跑 smoke 后用 `--benchmark_format=json | jq .benchmarks[].items_per_second` 校验全 > 0**

**落实**：写入 `writing-plans.mdc`「性能基准任务必检项」段；下次任何 bench plan 必加。

#### WSL2 / 云机 / Docker 稳态协议（TASK-20260424-03 反思 P1 #2 — 附录）

**问题**：WSL2 / 云 VM / Docker 容器内 CPU governor 在不活跃时下调频率（特别是 `ondemand` / `schedutil` governor），冷启动瞬态 + load average 抖动让 google/benchmark 默认 `--benchmark_min_time=0.05s` 单次测量 CV 飙到 8%，远超可作决策门槛。

**TASK-20260424-03 实证**：
- 首次测 Phase 6 stash baseline 得 6727 ns，CV 7.92%
- 真实稳态值 4689 ns，**偏离 43%** → 险些得出「Phase 7 倒退」的错误结论
- 引入 warm-up 协议后 CV 收敛到 ≤ 1%，run-to-run variance 降到噪声底

**稳态协议（4 步固定模板）：**

1. **sleep 等待**：`sleep 10` 让 CPU 进入 idle governor 的快速爬坡区（避免首次 bench 命中低频窗口）
2. **Warm-up 预热**：单 filter `--benchmark_repetitions=3 --benchmark_min_time=0.2s` **跑一次不记录结果**（让 L1/L2 cache 预热 + governor 升频 + glibc malloc 稳态）
3. **正式测量**：`--benchmark_repetitions=10 --benchmark_report_aggregates_only=true --benchmark_min_time=0.2s` 取 mean/median/stddev 三聚合行
4. **CV 门槛**：读取 stddev / mean = CV，`> 2%` 视为噪声不可决策 → 重跑 step 1-3；**连续 2 次 > 2%** 记为环境不可信，延后测量或换机

**样板（stash-swap 同窗口对比）：**

```bash
# --- BEFORE (先跑) ---
git stash push -u
cmake --build build-bench -j --target bench_xxx
sleep 10
./build-bench/benchmarks/bench_xxx --benchmark_filter='Warm_Medium' \
  --benchmark_repetitions=3 --benchmark_min_time=0.2s > /dev/null  # warm-up 丢弃
./build-bench/benchmarks/bench_xxx --benchmark_filter='Warm_Medium' \
  --benchmark_repetitions=10 --benchmark_report_aggregates_only=true \
  --benchmark_min_time=0.2s 2>&1 | tee /tmp/before.txt

# --- AFTER ---
git stash pop
cmake --build build-bench -j --target bench_xxx
sleep 10
./build-bench/benchmarks/bench_xxx --benchmark_filter='Warm_Medium' \
  --benchmark_repetitions=3 --benchmark_min_time=0.2s > /dev/null  # warm-up 丢弃
./build-bench/benchmarks/bench_xxx --benchmark_filter='Warm_Medium' \
  --benchmark_repetitions=10 --benchmark_report_aggregates_only=true \
  --benchmark_min_time=0.2s 2>&1 | tee /tmp/after.txt
```

**适用识别：** WSL2 / 云 VM (EC2 / GCE 等) / Docker / M1 Rosetta 等 CPU governor 受宿主影响的运行时。**裸机 + 固定频率 (`performance` governor) 可直接跑标准协议。**

**落实：** 已同步到 `writing-plans.mdc`「性能基准任务必检项」§7「WSL2 / 云机 bench 稳态协议」；Plan 阶段 Phase 0 工具核查 + bench 命令模板必引用本段。

### 扫描型研究任务脚本化模板 + 双指标交叉验证模式（TASK-24-01 反思 #3 — 新模式）

**适用场景**：研究/调查类任务中需对单变量（block_size、buffer_size、cache 容量、超参 N 等）做多档扫描以定位最优值时。

**样板脚本**：

```bash
# 设定扫描域（3-5 档为宜；过多噪声，过少不成曲线）
for VAR in VALUE_1 VALUE_2 VALUE_3 VALUE_4 VALUE_5; do
  # 单变量变更（仅修改目标参数）
  sed -i "s/PATTERN/${VAR}/" SOURCE_FILE

  # 静默 rebuild，仅保留错误/警告尾行
  cmake --build BUILD_DIR --target TARGET -j 2>&1 | tail -1

  # bench 以 json 输出到 /tmp 以便聚合
  ./BENCH_BINARY --benchmark_filter="BM_KEY" \
    --benchmark_format=json \
    --benchmark_out=/tmp/result_${VAR}.json \
    > /dev/null
done

# python3 fallback 聚合为 markdown 表（无 jq 依赖）
python3 - <<'EOF'
import json, glob, os
rows = []
for f in sorted(glob.glob('/tmp/result_*.json')):
    var = os.path.basename(f).removeprefix('result_').removesuffix('.json')
    data = json.load(open(f))
    # 提取目标 metric，如 time / items_per_second / ratio
    bm = next(b for b in data['benchmarks'] if 'YOUR_KEY' in b['name'])
    rows.append((var, bm['real_time']))
for var, t in rows:
    print(f'| {var} | {t:.2f} ns |')
EOF
```

**双指标交叉验证原则**：

- 单指标扫描会选到「某项最优但副作用指标恶化」的点 → 必然存在 sweet spot 但会误选
- **必须同时跑 ≥ 2 指标交叉**：如 `R256` 看 tree build 单项，`R_flex` 看 flex layout，二者联合判定才能识别真正最优档

**TASK-24-01 实证**：Phase 2 扫描 arena block_size ∈ {4K, 8K, 16K, 32K, 65K}：
- 单看 R256：{9.42, 4.68, 4.05, 3.84, 3.61} → 单调下降，65K 最优
- 同时看 R_flex：{16.49, 10.54, 8.10, **7.40**, 8.36} → 32K 最优，65K **回弹**
- 交叉判定：32K 为 sweet spot，65K R_flex 回弹是新发现（K8 L1D 抖动信号），**若只看 R256 会漏掉 L1D 约束**

**效率收益**：3 档扫描 + 聚合表 < 1 分钟，零手工粘贴。**反之**若每档手工 `sed / rebuild / bench / 粘数字 / 算比率` 则 5-10 min/档、5 档 = 30+ min 且易错。

**触发条件**：研究型任务的 plan 阶段若涉及扫描 ≥ 3 档的参数域 → plan Phase §步骤必带脚本化 for 循环 + python3 聚合样板 + ≥ 2 指标交叉判定列。

### 测试设计 — 公开行为锚定内部约束模式（TASK-24-01 反思 #5 — 新模式）

**问题**：如何测试一个**只能从外部观测内部行为**的约束（如「默认 block_size ≥ 某值」），当该内部值**没有公开 getter**、也**不希望为了测试加 getter/friend/`#define private public`**？

**方案**：设计一个**只有在内部约束成立时才会成功的公开行为测试**。

**TASK-24-01 样板**：

```cpp
TEST(ArenaAllocatorTest, DefaultBlockSizeFitsLargeAllocations) {
  // 约束：default block_size >= 32768
  // 不扩 API、不 friend，靠指针连续性间接观测：
  // 若 default block >= 32768，两次 16K 分配落入同一 block 连续区
  // 若 default block < 32768，第二次 16K 触发 oversized-block 路径（新 malloc），
  //   指针跨 block → p2 - p1 != 16384
  ArenaAllocator arena;
  constexpr usize kChunk = 16384;
  char* p1 = static_cast<char*>(arena.Allocate(kChunk));
  char* p2 = static_cast<char*>(arena.Allocate(kChunk));
  EXPECT_EQ(p2 - p1, static_cast<ptrdiff_t>(kChunk))
      << "Two 16 KB allocations must fit contiguously...";
}
```

**设计哲学**：
- ✅ **不扩 API**：`ArenaAllocator` 无需加 `default_block_size()` getter
- ✅ **不扩可见性**：无需 `friend` / `protected` / `#define private public`
- ✅ **实证约束**：通过**公开行为** `Allocate()` 返回指针的连续性，间接约束**内部不变量** block 容量
- ✅ **回归敏感**：任何降低默认 block 到 < 32K 的回归都会被该测试精准抓住（RED 反向探针已验证）

**适用识别**：
- 目标约束是「内部资源容量」「内部阈值」「内部调度策略」等**实现细节**
- 存在**公开行为**（分配/访问/查询）其结果受该内部约束**必然影响**
- 约束失败时公开行为的变化**确定性可观测**（非概率性/非计时相关）

**反模式**：
- ❌ 为测试加 `size_t block_size() const { return block_size_; }` — 污染 public API
- ❌ `friend class ArenaAllocatorTest` — 破坏封装
- ❌ `#define private public` 前置 include — 脏破解

**触发条件**：任何需要测试 private/implementation-detail 约束时，**先尝试本模式**；仅当公开行为无法观测该约束时才考虑扩 API。

## 已验证的模式（来自 TASK-20260424-03 DrawText 真路径 warm 优化）

### 异构工作负载 SIMD 尺寸阈值 dispatch 模式（TASK-24-03 反思 #3 — 新模式）

**问题**：为异构尺寸工作负载选用单一 SIMD 宽度（SSE2 4 px / AVX2 8 px / AVX-512 16 px）时，**宽 SIMD 对短输入摊销不足**（setup + lane 对齐开销 > 单 iter 收益），但对长输入又是显著加速；**一刀切选窄则大输入浪费硬件**，**一刀切选宽则小输入回归**。

**方案**：**按输入尺寸阈值 dispatch**。为每个 SIMD 宽度定义 `kMinPixelsForWidth_N`，runtime 按 `count >= threshold` 选择最优宽度；线性回退到次窄宽度直到标量。

**TASK-24-03 样板**：

```cpp
// veloxa/graphics/software/blit_avx2.h
#if defined(__x86_64__) || defined(__i386__)

// Empirically calibrated: ASCII glyph 6-12 px 下 AVX2 cvtepu8_epi16 +
// permute4x64 setup 每 iter 2-3 cycles 开销 > 8 px/iter 摊销收益。
// 16 px (典型 CJK 字号 / 大字号 ASCII) 起收益由负转正。
static constexpr u32 kAVX2MinPixelsPerRow = 16;

static inline void BlendGlyphRow(u32* dst, const u8* alpha, u32 count,
                                 u8 sr, u8 sg, u8 sb, u8 sa) {
  static const bool has_avx2 = __builtin_cpu_supports("avx2") != 0;
  if (has_avx2 && count >= kAVX2MinPixelsPerRow) {
    BlendGlyphRowAVX2(dst, alpha, count, sr, sg, sb, sa);
    return;
  }
  BlendGlyphRowSSE2(dst, alpha, count, sr, sg, sb, sa);  // 4 px/iter
}
#endif
```

**设计哲学**：
- ✅ **不 revert 无收益的宽 SIMD 路径** — 保留为 workload evolution（CJK 大字号 / 图形元素 / 未来硬件）的 headroom
- ✅ **runtime dispatch 单次 static 缓存** — `static const bool` 首次 query CPU capability 零开销
- ✅ **函数级 target attribute** — `__attribute__((target("avx2")))` 让 binary baseline 仍是 x86-64，AVX2 代码仅在支持时激活
- ✅ **阈值有 comment rationale** — `kAVX2MinPixelsPerRow=16` 必须注释为什么是 16（实证数据 + workload 特征），而非神秘数字

**适用识别**：
- 工作负载尺寸分布明显异构（短尾 + 长尾共存）
- 宽 SIMD 有可观 setup cost（lane 对齐 / 寄存器 spilling / dispatch branch）
- 实测宽 SIMD 在短输入无净收益甚至回归
- 存在明确的「尺寸转折点」可实证标定

**反模式**：
- ❌ 实测宽 SIMD 无收益就直接 revert 删代码 — 丢失 workload evolution headroom
- ❌ 阈值凭直觉 hardcode `16` 不注释 — 下一个人看不懂为什么
- ❌ `if constexpr (count >= 16)` 编译期分支 — 运行时 count 变量无法编译期判定，应为运行时 if

**触发条件**：下次任何 SIMD 优化任务（NEON / AVX-512 / WASM SIMD128）如实测宽 SIMD 在部分输入尺寸无收益，**优先实施阈值 dispatch** 而非 revert。

### 负结果资产化模式（TASK-24-03 反思 #4 — 新模式）

**问题**：Mixed TDD D3 类「为预防特定 bug 新增回归测试 + 新增支持 helper」的优化 Phase，在主路径实测**倒退**时如何处理？

**方案**：**保留 helper header + test，仅回退主路径调用**。helper + test 作为未来升级分支的**精度契约对照基础设施**。

**TASK-24-03 样板（Phase 5 /255 乘-移位近似回退）**：

```cpp
// veloxa/graphics/software/glyph_blend.h（保留）
// Although GCC -O3 Granlund-Montgomery magic multiply now outperforms
// this mul-shift approximation on scalar paths, DivBy255Approx + the
// unified BlendGlyphPixel helper are retained as the ±1 LSB precision
// contract reference for SSE2 / NEON / AVX2 SIMD fast paths where the
// compiler can NOT auto-vectorize to pmulhuw-style instructions.
static inline u8 DivBy255Approx(u32 n) {
  u32 plus = n + 128u;
  return static_cast<u8>((plus + (plus >> 8)) >> 8);
}
```

```cpp
// tests/graphics/pixel_blend_test.cc（保留 5 个精度契约测试）
// Phase 7 (B3 SIMD) 直接复用这些测试作为 SSE2/AVX2 与 scalar ±1 LSB 对照
TEST(PixelBlendTest, DivBy255ApproxMatchesIntegerDivision) { /* ... */ }
TEST(PixelBlendTest, BlendGlyphPixelParityWithReferenceScalar) { /* ... */ }
```

**设计哲学**：
- ✅ **负结果 ≠ 浪费** — Phase 5 的 helper + test 零成本支撑 Phase 7 SSE2/AVX2 开发
- ✅ **失效的主路径可 revert，支撑基础设施不 revert** — 降低未来升级分支的认知/实现成本
- ✅ **在 header 内 doc comment 标注 rationale** — 下一个维护者知道为什么 helper 还在但主路径没用

**适用识别**：
- 优化 Phase 主路径因实现细节（编译器已做更优 / 硬件分布不匹配）在当前实测回退
- 伴生的 helper / test 对未来升级路径（SIMD / 新硬件 / 新 workload）有**独立价值**
- revert 主路径但保留 helper 不引入死代码或维护负担

**反模式**：
- ❌ 整 Phase 回退含 helper + test 一起删 — 下次升级 SIMD 重复造精度契约轮子
- ❌ 保留 helper 但 header doc comment 不解释为什么 — 下一个人会质疑死代码并删除

**触发条件**：Mixed TDD D3 类 Phase 实测回退时，评估 helper/test 是否对**将来升级分支**（SIMD/新硬件/新 workload）有独立价值；有则保留 + doc comment 标注 rationale，无则整体 revert。

### 刚性目标 + R1 升级路径 plan 模式（TASK-24-03 反思 #6 — 新模式）

**问题**：多候选优化类任务如何处理 plan 阶段**不确定是否能达成刚性目标**的情况？一次性承诺所有工作量（含 SIMD）会过度设计；silent accept 不达标又违反验收合约。

**方案**：Plan 主线只承诺「便宜候选」+ **显式预留 R1 升级路径 AskQuestion 节点**（触发条件、选项、代价全部 plan 阶段写死）；用户基于**实测数据**做升级决定，而非 plan 阶段凭空决定。

**TASK-24-03 样板（2 次 R1 升级按协议执行）**：

```markdown
# docs/plans/2026-04-24-drawtext-warm-opt.md §7
Phase 6 (B2) 完成后跑 bench_drawtext：
├── warm Medium < 3000 ns → ✅ 进入 Phase 7 收尾
├── warm Medium ≥ 3000 ns → 🔴 触发 AskQuestion：
│   ├── (i) 升 B3 SIMD 分支（估加 40-60 min plan，需决策 x86_64-only vs SSE2+NEON）
│   ├── (ii) 接受当前结果为「中间态」，K7 标为 partial-resolved，留残余入新候选
│   └── (iii) 搁置本任务（/archive 为 `已搁置`），重规划
```

**实际执行**：
- Phase 6 末 warm=4689 ns 触发 AskQuestion → 用户选 (i) 升 B3 SSE2 → Phase 7 warm=3354 ns 仍 ≥ 3000
- Phase 7 末再次触发 AskQuestion → 用户选 (C) AVX2 dispatch（新候选）→ warm=3499 ns 仍 ≥ 3000，业务目标 (< Fallback) 达成 → 用户知情接受

**设计哲学**：
- ✅ **主线 plan 不过度承诺** — 避免 plan 阶段估时膨胀到不可管理
- ✅ **升级触发条件 + 选项 plan 阶段写死** — 不是「到时候再看」而是「实测达标则进 Phase 7，否则 AskQuestion 触发三选项」
- ✅ **用户数据驱动决策** — 每次升级都是基于**前一阶段实测数据**的用户决定，而非 plan 阶段凭空决定
- ✅ **升级路径选项包含「接受 + 搁置」** — 不强制持续升级，允许用户基于性价比中止

**适用识别**：
- 多候选优化类任务，候选从「便宜到贵」排序
- 刚性目标是否能达成**取决于便宜候选累计效果**（不确定，实测才知）
- 昂贵候选（SIMD / 架构重构）估时可观，**不值得为 hypothetical 场景一次性承诺**

**反模式**：
- ❌ Plan 一次性承诺所有候选（便宜 + SIMD + 架构重构）→ 估时膨胀，用户 AskQuestion 选项受限
- ❌ 不预留 R1 升级节点，Phase 末不达标直接 silent accept → 违反刚性验收合约
- ❌ 升级触发条件不写死，Phase 末才现场头脑风暴 → 中间态混乱

**触发条件**：任何多候选优化类任务的 plan §验收决策段，如刚性目标达成不确定 → 必须写明 R1 升级路径三选项（升级 / 接受 / 搁置）+ 触发条件 + 各选项代价。

### 编译器已做优化识别 — 位运算近似反模式（TASK-24-03 反思 L1 — 新教训）

**问题**：手写 `/常量 → 位运算` 近似（如 `(x * 257 + 32768) >> 16` 替代 `x / 255`）在标量路径**几乎必然输给编译器**，因 GCC 自 4.x / Clang 自早期版本已对所有常量除法自动应用 **Granlund-Montgomery 魔数乘法**（imul + shr，2 instructions），而手写 add-shift 链（3 adds + 2 shifts = 5 instructions）不优且破坏 u8↔u32 扩展优化。

**教训**：
- ❌ **标量路径手写 `/常量 → 位运算` 近似几乎必然倒退** — 本任务 Phase 5 实证 +1.1%
- ✅ **SIMD 场景才值得手写** — 编译器无法 auto-vectorize 到 pmulhuw-style 指令时，手写 `_mm_mulhi_epu16` + 修正项才是真正的加速

**落实**：Plan 阶段任何「`/常量 → 位运算`」「常量模替换」「幂次魔数乘法」类优化候选，**必须 godbolt 确认 GCC `-O3` 原生输出**（是否已应用魔数乘法）；仅 SIMD 路径值得手写。

**适用识别**：
- `x / C` 或 `x % C`（C 为编译期常量）
- `x * C / D`（C, D 为编译期常量）
- 任何「常量位运算近似」候选

**反模式验证方法**：
```bash
# godbolt CLI 或 gcc -S 直接看
echo 'unsigned f(unsigned x) { return x / 255; }' | \
  gcc -O3 -S -x c - -o - | grep -E 'imul|shr|mul'
# 预期：imul $0x80808081, ... + shr $0x7, ... → Granlund-Montgomery 已应用
```

## 已验证的模式（来自 TASK-20260424-04 DrawText hb_shape 结果缓存）

### Env Toggle A/B 对照模式（TASK-24-04 反思 #1 — 新模式）

**问题**：引入新 cache/short-circuit 类优化后，如何**快速、可复现、可审计**地验证 "cache 贡献量" 而不必来回切 branch 或 git stash？传统做法是 `git checkout pre && bench` + `git checkout post && bench` + `compare.py`，但 setup 成本高（~30 min），且 pre/post 各跑一轮还是背景噪声大。

**方案**：为新优化 feature 附带**同名环境变量禁用开关** `VX_<FEATURE>_OFF`，process-level latched-once（避免 benchmark 循环内反复读 env）。用户可在一次 build 内做 A/B 对照，pre-baseline 用 `VX_<FEATURE>_OFF=1 ./bench`，post 用 default env。

**TASK-24-04 样板**：

```cpp
// veloxa/text/font_manager.cc
static bool ShapeCacheDisabled() {
  static const bool disabled = []() {
    const char* v = std::getenv("VX_SHAPE_CACHE_OFF");
    return v != nullptr && v[0] != '\0' && v[0] != '0';
  }();
  return disabled;
}

const ShapedRun* FontManager::ShapeOrLookup(...) {
  if (!ShapeCacheDisabled()) {
    if (const auto* hit = shape_cache_.LookupOrNull(key)) return hit;
  }
  // miss path ...
}
```

**验证**：Cache ON Warm_Medium **1788 ns** vs `VX_SHAPE_CACHE_OFF=1` Warm_Medium **3542 ns** vs 上游 baseline **3499 ns**（env-off 与无优化版本差异 1.2% 在噪音带），**精确剥离 cache 贡献量**。

**适用识别**：
- Cache / memoization / lookup 表 / short-circuit fast path
- 任何「是否启用优化」可在单 build 内通过 env 切换的 feature

**反模式**：
- ❌ 用编译期宏 `#ifdef` 切换 → 必须两次 build，setup 2×
- ❌ Env 读取放热路径 → 每次调用 getenv syscall 抵消优化收益
- ✅ Process-level latched-once（`static const bool = []{...}();`）

**落实**：下次引入 cache / memoization / short-circuit 类优化 → 默认加同名 env 禁用开关（命名约定：`VX_<SHORT_FEATURE>_OFF`）。

---

### 预提取依赖 Header 原则（TASK-24-04 反思 #2 — 新模式）

**问题**：多 phase 任务中，**后续 phase 若需要 include 当前 phase 尚未暴露的 symbol**，如果不预先提取到独立 header，容易触发**循环依赖** (A.h ↔ B.h) 或需要**第二次改同一文件**（破坏 phase 隔离）。

**方案**：Phase 开始前识别"透明重构"需求（纯文件移动，语义不变），**在第一次改到相关文件时就一并完成**，而不是等到触发依赖问题时抢救。

**TASK-24-04 样板**：
- P2 `shape_cache.h` 需要 `FontHandle`（定义在 `font_manager.h`）；`font_manager.h` 下一步也需要 include `shape_cache.h`（P3）→ **预见循环依赖**。
- 解法：P2 一并提取 `FontHandle` 到新 header `veloxa/text/font_handle.h`（20 lines），`font_manager.h` / `shape_cache.h` 各自 include 它。
- 同理 P3 `FontManager::ShapeOrLookup` 需要复用 `software_canvas.cc` 的 thread_local `hb_buffer_t` → **预见第二次改 software_canvas.cc**。
- 解法：P3 一并提取 `HbBufferHolder` 到 `veloxa/text/hb_buffer_holder.{h,cc}`（~60 lines），`font_manager.cc` / `software_canvas.cc` 各 include。

**结果**：零循环依赖、零 ODR、零下游 include 破坏、零同文件二次修改。Plan 阶段若不预见，最晚在 P3 触发时 plan 阶段会需要新增 Refactor phase（+30 min），破坏 phase 粒度。

**适用识别**：
- 多 phase 任务（≥ 3 phase）
- 当前 phase 引入新 header X，预期下一 phase 在已有 header Y 中 include X
- Y 已存在 symbol Z，X 需要 Z

**触发检查清单**：
1. 新 header X 是否 include 已有 header Y？→ 若是，检查 Y 是否也会 include X（未来 phase）
2. 若是，**本 phase 必须**把 X 与 Y 共需的 Z 提取到独立 header

---

### 第三方 API 消除型优化估时下限公式（TASK-24-04 反思 #3 — 新模式）

**问题**：Plan 阶段对「消除一族第三方 API 连续调用」类优化估时容易过于保守（经验常数 -200 ~ -500 ns），导致**意外超额达成门槛**（本任务门槛 <3200 ns，实测 1877 ns single / 2350 ns mean）。

**方案**：估时下限按 **`N × single_call_cost × (1 - miss_rate)`** 公式，而非经验常数：
- `N` = 被消除的 API 连续调用次数
- `single_call_cost` = 单次 API 代价经验值（profile 或 godbolt 反汇编粗估）
- `miss_rate` = 目标 workload 的 cache miss 率（warm steady-state 通常 < 5%）

**TASK-24-04 样板**：
- 消除 6 次 hb_* 调用（`hb_buffer_reset` / `add_utf8` / `guess_segment_properties` / `hb_shape` / `get_glyph_infos` / `get_glyph_positions`）
- 单次 hb_* 均摊 ~350 ns（profile 估算，hb_shape 本身占大头但其他 5 次也有 ~50-150 ns 各自）
- miss_rate ≈ 0（warm 稳态）
- 下限 = 6 × 350 × 1.0 ≈ **2100 ns**
- 实测节省 = 3499 - 1745 ≈ **1754 ns**（误差 <17%，验证公式）

**适用识别**：
- 候选方案能**一次性消除一族连续第三方 API 调用**（shape / layout / rasterize / codec 等）
- Warm 路径稳态 cache hit rate > 80%

**反模式**：
- ❌ 直接套用经验常数 -200 / -500 ns → plan 目标保守，意外超额不是坏事但削弱规划精度
- ❌ 不区分单次 API 成本差异（如 hb_shape 单独占 60%）→ 公式只是下限兜底，实际可能更高

---

## 已验证的模式（来自 TASK-20260425-01 SDL2 窗口后端 + 输入事件桥接）

### Composition over Inheritance — Platform Backend 复用模式

**问题**：新 platform backend（如 `Sdl2EventLoop` / `Win32EventLoop`）需要 PostTask / SetTimer / CancelTimer 等通用调度逻辑，但又有自己的事件循环主体（pump SDL events / pump win32 messages）。

**反模式（继承）**：`Sdl2EventLoop : public HeadlessEventLoop` —
- 两套 Run/Quit 状态机如何整合？outer Run 调 inner Run 还是替代？
- inner 的 task 容器是 protected 还是 public？子类能直接动是反封装
- Sdl2 quit 是否触发 Headless quit？反过来呢？语义死结

**正解（组合）**：`Sdl2EventLoop` 内 `std::unique_ptr<HeadlessEventLoop> inner_;`，PostTask/SetTimer/CancelTimer **委托**给 inner，自己只管 Run/Quit/PumpInputEvents/SetInputCallback。

**TASK-25-01 实测**：composition 让 `Sdl2EventLoop` 类只 ~80 LOC，复用 HeadlessEventLoop ~150 LOC 的 task/timer 实现，且语义清晰。

**适用识别**：
- 新 backend 与 headless backend 共享通用部分但有自己的 driver loop
- 现有 backend 不是 final 类（可被组合复用）

**配套准则 — 同名方法歧义防护**：composition 内 outer 类的控制方法（`Quit / Reset / PostTask` 等）与 inner 同名时，outer 实现必须**显式 `this->Quit()` 而非 `Quit()`**，或把 inner 同名方法重命名（如 `inner_quit_internal()`）。**TASK-25-01 实证**：`PumpInputEvents` 在 SDL_QUIT 分支误调 `inner_->Quit()`（关掉的是组合对象，不是自己），导致 `while (running_)` 永不退出，测试挂死。

### GUI / 主循环型程序 ctest 自终止模式

**问题**：GUI example（hello_sdl2 等）在 ctest 跑 smoke 时会 block 等用户关闭窗口，CI 永不结束。

**模式**：用 `VX_<APP>_AUTOQUIT_MS=N` 环境变量 + `SDL_AddTimer / equivalent` 在指定 ms 后推送平台 quit event：

```cpp
if (const char* env = std::getenv("VX_HELLO_SDL2_AUTOQUIT_MS")) {
  int ms = std::atoi(env);
  if (ms > 0) {
    SDL_InitSubSystem(SDL_INIT_TIMER);  // AddTimer 需要 TIMER 子系统
    SDL_AddTimer(ms, [](Uint32, void*) -> Uint32 {
      SDL_Event ev{}; ev.type = SDL_QUIT; SDL_PushEvent(&ev); return 0;
    }, nullptr);
  }
}
```

CMake：
```cmake
add_test(NAME hello_sdl2_smoke COMMAND $<TARGET_FILE:hello_sdl2>)
set_tests_properties(hello_sdl2_smoke PROPERTIES
  ENVIRONMENT "SDL_VIDEODRIVER=dummy;VX_HELLO_SDL2_AUTOQUIT_MS=200"
  TIMEOUT 10)
```

**关键原则**：
- prod 用户**永不**设此 env，hook 完全 opt-in
- ms 取 200-500（足够走 1-2 帧渲染 + 触发关闭路径）
- `TIMEOUT N`（N >= 5×ms）兜底，防止 hook 失效
- env 名前缀必须 `VX_<APP>_*` 避免污染全局命名空间

**已应用 2 次**：
1. TASK-24-04 `VX_SHAPE_CACHE_OFF` — bench A/B 对照
2. TASK-25-01 `VX_HELLO_SDL2_AUTOQUIT_MS` — GUI smoke 自终止

→ **泛化为「Process-level latched-once env hook」模式**：任何「prod 路径不触发，仅 test/bench 需要的 process-wide 状态切换」都可用此模式，无需改 API。

### 测试文件 include 卫生模式

**问题**：`#include` 误置 anonymous namespace 内会污染该 namespace 解析（如 `<SDL2/SDL.h>` 间接拉 `<math.h>` 时把 `std::abs` 解析到 `{anonymous}::std`）。

**模式**：所有 `#include` 必须在 file scope（namespace 之外）。**Plan §0 batch grep 检查**：

```bash
rg "^namespace .*\{" -A 30 tests/ | rg "include " && echo "BAD: include inside namespace" || echo "OK"
```

**TASK-25-01 实证**：api_test.cc 把 `#include <SDL2/SDL.h>` 放进 anon namespace，编译错误 `'abs' has not been declared in '{anonymous}::std'`，5 分钟排查。如果 Plan §0 grep 即可 0 秒发现。

→ 落实方式：追加到 `writing-plans.mdc`「smoke 工具链可用性检查」段；本任务 P1 改进建议 #2 已对应。

### Plan 验收用例与 example 实现一致性检查

**问题**：plan 写「鼠标 hover 红块变 #FF6B6B」作为 A2 验收用例，但 example.cc 实现时忘了加 `:hover` CSS 规则 → plan 与代码不一致永久存在直到归档。

**模式**：plan 验收清单引用具体 UI 行为（hover/click/keystroke→样式变化）时，必须满足下列**至少一项**：
1. example 段直接附上对应 CSS/HTML/JS 片段（不是只写「会有 hover」）
2. plan §X 步骤 0 加显式同步检查项「example 含验收所需的 CSS 规则 X」
3. example PR 与 plan 同 PR 提交（强制 reviewer 看到两者）

**TASK-25-01 实证**：plan §6.1 步骤 2 期望 hover 颜色变化，但 hello_sdl2.cc 没有 `:hover` 规则；P6.1 标为遗留没暴露。回顾阶段 P0 #4 强制加上才一致。

→ 反复模式「计划文件清单与实际变更不一致」第 10 次新维度（UI 行为维度）。落实方式：追加到 `writing-plans.mdc`「验收用例与 example 一致性检查」新子段。

---

## 待定架构决策
- [x] CSS 支持的具体子集范围 → 已确定：~45 属性（布局/Flex/视觉/文本）+ 4 transition 属性
- [ ] 是否内置 SVG 支持
- [x] 动画系统的实现策略 → CSS Transitions 已实现（TASK-13），CSS Animations（@keyframes）待定
- [ ] 资源加载策略（打包 vs 文件系统 vs 混合）
- [ ] 多进程 vs 单进程架构
