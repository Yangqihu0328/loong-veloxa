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
| **极窄（既有范本 100% 模式复用 + 单分支扩展）** | **TASK-30-01 P6 (0.21×) / TASK-30-02 (0.22×)** | **0.20-0.25×** | **plan × 0.22** | 既有同子系统范本 100% 复用（如 CSS shorthand 仿既有 padding/margin/border 范本扩展）+ Level 2 单子系统 + 0 创意决策 + 0 新护栏需求（即使 V5 安全标注但护栏复用既有 N-cap）|

**「最窄路径」识别清单**（满足全部 → 按 plan × 0.3 预估）：
- 核心改动 ≤ 3 文件
- 测试基础设施 100% 就绪（新增测试复用既有 test target，无新 CMake）
- 实验阶段可脚本化（for 循环 + python3 聚合）
- 无新第三方依赖 / 无 CMake 新 target / 无 FetchContent
- 无 API 变更 / 无接口破坏

**「极窄路径」追加识别清单**（在「最窄路径」基础上额外满足全部 → 按 plan × 0.22 预估）：
- 既有 same-subsystem 范本 100% 模式复用（grep 实证可指出参考范本的精确行号）
- D 决策矩阵全锁定（PLAN 阶段无 BUILD 期调整）
- 0 创意阶段（无 UI / 算法 / 架构空白）
- V5 安全相关时，威胁面 100% 被既有上游护栏 + 既有 parser 内部 N-cap 复用覆盖（**零新护栏需求**）

**触发条件**：下次若识别出「最窄/极窄路径」特征而 plan 估时按「中等」档（1.5-2×）给出 → plan 阶段直接下调预算 50-78%；反之若发现意外耗时膨胀需立即切换诊断模式。

**TASK-30-01 P6 + TASK-30-02 实证（2026-04-30）**：CSS parser shorthand 扩展场景双数据点定型「极窄路径 0.2-0.25×」档：
- TASK-30-01 P6（last-in-flow-block pointer hoisting，~30 LOC + 1 测试）：plan 25 min / 实测 ~5.3 min = **0.21×**
- TASK-30-02（CSS border shorthand 7 个补全，+212 LOC + 22 测试）：plan 170 min / 实测 ~37 min = **0.22×**

共同特征：既有 same-subsystem 范本（layout helper 函数 / CSS shorthand 既有 border + padding/margin 双范本）100% 复用 + D 决策全锁定 + 0 创意 + 0 新护栏。这构成「极窄路径」档的统计基础。

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

## 已验证的模式（来自 TASK-20260426-01 Layout 正确性消化 #25 + #28 + #20 + #21）

### 同窗口对照 bench — performance regression 验证范式

**问题**：跨时间窗 bench baseline 数据点不可比 — 系统抖动 ±3-7%（CV 5-8%）在 ±10% 退出门下严重放大假信号。本任务 R3 初版「+10.2%」用一周前 baseline (3709 ns) vs 新窗口测量 (4087 ns)，同窗口对照后真实增量仅 +3.2%（差异分解：~70% 时窗校准 + ~30% 真实优化）。

**模式**（升 P0，写入 `writing-plans.mdc` §7）：

```bash
# Performance regression 验证默认 stash-swap 同窗口对照
# Step 1: stash 改动测原 baseline
git stash
cmake --build build-release --target <bench>
build-release/<bench>_path --benchmark_repetitions=10 ... > baseline_in_window.json

# Step 2: stash pop 还原 + 强制重编（关键！）
git stash pop
touch <changed_src>            # 防 ninja 时间戳偏差跳过重编
cmake --build build-release --target <bench>

# Step 3: 测改动版
build-release/<bench>_path --benchmark_repetitions=10 ... > changed_in_window.json

# Step 4: diff（同窗口 cv 通常 5-8%，跨窗口 8-15%）
python3 -c "..." baseline_in_window.json changed_in_window.json
```

**判定准则**：
- 退化判定一律同窗口；bench baseline `.json` 仅作长期趋势监控
- 同窗口 CV ≤ 8% 视为可信（vs 跨窗口 CV ≤ 2% 协议不再适用）
- mean/median 双指标交叉，单指标偏移落 stddev 范围内 → 接近统计不显著
- 不允许跨日/跨重启比对绝对数字

**TASK-20260426-01 实证**：R3 V1 优化后 stash-swap 同窗口 mean +3.2% / median +3.4%（跨窗口曾误判 +10.2%）+ R4 同窗口 mean -3.6% / median +2.65%（跨窗口曾不可知）。两次实证「stash-swap 同窗口对照」是 ±10% 退出门唯一可靠手段。

→ 落实方式：升级 `writing-plans.mdc` §7 WSL2 协议。延伸：`bench/baseline/*.json` 仅作长期趋势监控注释。

---

### 跨 LayoutType 共用样式属性必须每路径独立 box-model 解析

**问题**：当 `display: X` 在 BuildTree 阶段映射成 `LayoutType::Y`（如 inline-block → kInline / block → kBlock / flex → kFlex）时，Layout::Y 必须独立完成 box-model（width/height/padding/border/margin）解析；不能假设单一 layout 路径已处理。

**模式**（升 P0，写入 `systemPatterns.md` 长期约束）：

| `display` | `LayoutType` | 必须独立解析的 box-model 属性 |
|---|---|---|
| `block` | `kBlock` | width/height（含 auto/explicit） |
| `inline-block` | `kInline`（atomic） | **width/height（atomic 高度边界）+ ascent/descent** |
| `inline` | `kInline` | width = max line.end_x（fit-content 默认）/ ascent/descent 来自 child |
| `flex` | `kFlex` | width/height + main/cross axis |

**TASK-20260426-01 实证**：vx BuildTree 把 `display: inline-block` 映射成 `LayoutType::kInline` → R4 初版 LayoutInline 不读 style.height → atomic ascent = border_box_height = 0 → vertical-align 关键字物理位置失效（RED2 测试触发）。修复：LayoutInline 末尾对 explicit height 走 ResolveLength 写 content_height（与 LayoutBlock 路径对称）。

→ 落实方式：写入 `writing-plans.mdc` Layout 类任务必检项：默认值边界（fit-content vs fill-available / explicit vs zero-fallback）必须在 plan §0 grep fingerprint + §3 设计决策一并锁定。

---

### Creative 阶段「单一坐标约定 + 公式表」一图（≥2 坐标系/方向算法）

**问题**：复杂坐标系算法（vertical-align 的 baseline / ascent / descent / offset / top-bottom 多方向多锚点）在 build 阶段反复出现 sign error。creative 锁定了算法（2-pass）但未锁定坐标约定。

**模式**（升 P0，写入 `creative.md` 命令模板）：

凡涉及 ≥2 坐标系/方向的算法 creative 必产出：
1. **统一坐标约定声明**（如 `item.y = baseline_y - item.ascent + offset`，offset > 0 下沉 / < 0 上升）
2. **全部公式按约定列出**（每条公式注释引用约定）
3. **build 阶段每条公式实施时引用约定**（代码注释含 `// per creative-X.md §Y coordinate convention`）

**TASK-20260426-01 实证**：creative-line-box-model.md D2.B 锁定了 strict 2-pass，但未画坐标约定单一图；R4.6 实施时 `ComputeNonExtremeAlign` 6 关键字 + Phase 1/2 max_ascent/max_descent 维护 + kTop/kBottom offset 多处 sign error，调试 ~30 min 系统性诊断后采用统一坐标约定 + 每条公式注释引用约定才解。

→ 落实方式：升级 `creative.md` 命令模板「6.algorithm」段：当涉及 baseline/ascent/descent/offset 等概念时强制要求坐标约定声明。

---

### TextMetrics ABI 兼容渐进式扩展模式

**问题**：跨 Round 共享数据结构（如 `TextMetrics struct`）需新增字段又不破现有 caller，多 Round 并行时风险高。

**模式**（升 P2，写入 `systemPatterns.md` 长期实践）：

```cpp
struct TextMetrics {
  f32 width;
  f32 height;
  // 新增字段：清晰文档 + spec 引用
  // CSS 2.1 §10.8.1 半-leading 公式依赖：line.height = max_ascent + max_descent
  f32 ascent;
  f32 descent;
  // DEPRECATED：与 `ascent` 同语义；保留仅为 ABI 兼容。新代码请用 `ascent`。
  [[deprecated("use ascent")]] f32 baseline;
};
```

写入路径用 `#pragma GCC diagnostic push/ignored "-Wdeprecated-declarations"` 抑制 baseline 字段初始化 warn；新代码统一用 ascent。

**TASK-20260426-01 实证**：R4.3 TextMetrics 拆 ascent/descent 同时保留 `[[deprecated]] baseline`，零 caller 改造（R1/R2/R3 同步推进零冲突）+ 给未来清理明确锚点。

→ 落实方式：写入 `systemPatterns.md`「ABI 兼容渐进扩展模式」长期段。

---

### wpt fixture 数值化适配模式

**问题**：直接拷贝 W3C wpt 标记可能 trivial fail（vx CSS 子集差距），不能简单按像素 ref 验。

**模式**（升 P2，写入 `systemPatterns.md` 长期段）：

拷贝 W3C wpt 标记前必查：
1. **ID 选择器范围** — vx StyleResolver 子集对 `#id` 选择器支持有限 → 改 class
2. **container display 与 vx 匿名 IFC 行为** — vx LayoutBlock 不创建匿名 IFC → 容器须显式 `display: inline` 才走 LayoutInline 路径
3. **像素级 ref 不可比** — 改数值化断言（坐标 / 尺寸 / 关系），不直接对 png

**TASK-20260426-01 实证**：R3 wpt 005 SKIP-w/-rationale（horizontal margin 像素 diff 不适配 R3 数值范围）+ R4 wpt 006/007 ID→class + container→display:inline 共 3 次同模式适配。

→ 落实方式：写入 `systemPatterns.md` + `writing-plans.mdc` Layout 类任务必检项。

---

### 测试边界发现 — `internal::` helper 提取模式

**问题**：当上层组件（如 CssParser）有自然错误恢复路径（unknown property → discard），e2e 测试可能 trivially PASS（over-match），看似实现存在但实际是依赖上层兜底。

**模式**（升 P2，写入 `systemPatterns.md`）：

当 e2e 测试 trivially PASS 即使实现是空 placeholder 时，必须提取内部 helper 到 `internal::` namespace 暴露 unit test 入口，直接测「实现真存在」。

**TASK-20260426-01 实证**：R2.5 ContainsBlacklistKeyword 黑名单 e2e 4 case 因 CssParser 自然丢弃 `behavior:` unknown property 已 trivially PASS（over-match）；提取 `vx::html::internal::ContainsBlacklistKeyword(StringView) → bool` 暴露 unit test + 8 直接测试用例（5 match + 3 reject）+ RED 反向探针（临时 `return false` → 5/5 match 立即 FAIL → 恢复 8/8 PASS）才证「实现真存在」。

→ 落实方式：写入 `systemPatterns.md`「测试边界发现 — internal helper 提取模式」段。

---

### 「副产品修 pre-existing bug」scope 边界 3 标准

**问题**：build 阶段测试驱动发现的 pre-existing bug，是否要拆任务还是顺手修？

**模式**（升 P2，写入 `systemPatterns.md`）：

满足以下 3 条同时成立时，scope 内顺手 1 行 fix（不拆任务），在 progress.md「实证微调」列记：
1. **由本 Round 测试触发** — 不是其他模块的疑似回归
2. **修复 ≤ 5 行** — 真实结构性改动须拆任务
3. **不引入新结构性改动** — 不新增字段/不改接口

**TASK-20260426-01 实证**：R3.4 测试 `A7_OverflowAutoBlocksMarginCollapse` 触发 `overflow: auto` 经 ParseDeclaration 走 `ValueType::kAuto` 全局快路径不进 ParseEnumValue 致 enum 字段保持 default kVisible — 顺手在 `style_resolver.cc` 补 `kAuto` 路径合规（< 5 行），属「scope 内顺手 1 行 fix」典型，progress.md R3.3.x 列记。

→ 落实方式：写入 `systemPatterns.md`「scope 边界 — 副产品修复 3 标准」段；下次 build 阶段触发 pre-existing bug 直接对照判定。

---

### Level 4 多轮次任务 plan × 0.6 估时（首数据点 0.44×）

**问题**：Level 4 任务首次数据点 0.44× 落在「准确档（0.6×）下方 + 最窄档（0.3×）上方」中段，简单按子任务平均会失真。

**模式**（升 P1，写入 `systemPatterns.md`「bench 类任务估时校准」段扩展）：

| 任务类型 | plan × ratio | 触发条件 |
|---|---|---|
| 最窄路径（基础设施 100% 复用 + 单文件 D2 改动） | 0.22-0.34× | 4 数据点稳定（TASK-24-01/03/04 + TASK-25-01）|
| 准确档（D2 单文件 + 测试完整） | 0.50-0.56× | R1/R2 子任务数据点 |
| 中位档（D3 算法 creative 锁定 + helper 模式成熟） | 0.37-0.50× | R3/R4 子任务数据点 |
| **Level 4 多轮次整体** | **0.40-0.50×** | **首数据点 0.44×**（creative 完整锁定 + 多轮次协议成熟 + VAN grep 实证三联合） |

**关键洞察**：Level 4 整体不能简单按子任务平均（子任务平均会受最慢的 R3/R4 拉高），整体含 R0/R5 meta 反而拉低。Level 4 多轮次任务在「creative 完整锁定 + 多轮次协议成熟 + VAN grep 实证」三联合下应按 plan × 0.4-0.5× 预估。

→ 落实方式：扩展 `systemPatterns.md`「bench 类任务估时校准」段；累计 3 个 Level 4 数据点后写入 `writing-plans.mdc` 强制条目。

---

## 已验证的模式（来自 TASK-20260430-01）

### 递归算法 API 传递语义决策必检项

**问题**：递归算法（layout / parser / event bubble / clean-up cascade）跨函数传递「累积态 / chain / cursor / accumulator / state」时，by-value 在多级递归边界天然失效；plan 阶段仅推演单层「caller → callee」漏掉「Level N → N+1 → N+2」+「caller 是否需要看到 callee 的修改」。

**模式**（升 P0，已写入 `writing-plans.mdc` §9.4）：

任何递归算法 API 决策必须做以下 mental trace：

1. **≥3 层最小递归路径**（Level N → N+1 → N+2 具体调用链）
2. **每层共享状态修改清单**（chain / accumulator / cursor / counter，标注 read / write / read-modify-write）
3. **caller 可见性判定**：每层调用站点回答「callee 完成后，caller 是否需要看到 callee 的修改？」
   - 答 yes → by-value **必然失效**；必须 by-pointer / by-reference / 返回值显式回传
   - 答 no → by-value 等价 by-ref（同函数内变量）
   - 混合 → 选 in-out by-pointer + 显式 return

**TASK-20260430-01 实证**：D2 plan 锁 `MarginChain incoming`（by-value，理由：POD 12B + SROA 等价无开销）→ build P3 三层递归立即失效（grandchild 修改 chain 后 parent 物理化时 chain 已丢失）→ 改 in-out by-pointer (`MarginChain* incoming`) + return MarginChain；零 ABI 风险，编译器仍可 SROA 优化。

**反复模式定型**：「前置依赖/环境/API 能力未验证」第 9+ 次新维度；「API 传递语义决策未做多级 mental trace」首次显化定型。

---

### 既有测试隐式契约 fingerprint

**问题**：layout / parser / event 等富边界子系统，既有测试隐含的边界假设（如「root 不向上 propagate」「auto width 等于 fit-content」）是隐式 spec；plan §0 grep 仅查目标功能 fingerprint，不查既有测试期望反向 fingerprint → build 阶段 GREEN 实施完后既有测试退化。

**模式**（升 P1，已写入 `writing-plans.mdc` §0「既有测试隐式契约 fingerprint」段）：

plan §0 batch grep 必须包含「**既有测试边界期望反向 fingerprint 表**」，对 ≥ 1 个相关测试文件做关键字 grep（layout 类必 grep `EXPECT_FLOAT_EQ.*y` / `EXPECT_FLOAT_EQ.*content_height` / `EXPECT_TRUE.*collapsed`）。命中条目必须列出每条隐含的边界假设。

**TASK-20260430-01 实证**：plan §0 仅查 F1-F6（功能 fingerprint），未列「R3 sibling collapse 测试隐含 root 不 propagate」假设 → P3 GREEN 后既有 `MarginCollapseLayoutTest` 系列退化；P4 加 `box->parent == nullptr || box->parent->parent == nullptr` 隐式 BFC root 识别才修正。

---

### CSS shorthand 能力 grep 表

**问题**：vx CSS parser 对各 shorthand 支持参差不齐（`padding` ✅ / `margin` ✅ / `border` ⚠️ / `border-bottom` ❌ / `font` ?），plan / RED 假设 shorthand 可用未单独 grep → 测试虚假通过（测试用 shorthand → parser 不识别 → 无效果 → 反而符合「不应发生」断言）。

**模式**（升 P1，已写入 `writing-plans.mdc` §0「CSS shorthand 能力 grep 表」段）：

plan / RED 用例使用任意 CSS shorthand 时必须单独 grep 验证 parser 能力；命中 0 处 → 强制改用兜底方案（longhand 三件套）或列入独立 P3 触发型技术债。

**TASK-20260430-01 实证**：plan §1.2 测试 `BorderBottom_BlocksLastChildCollapse` 直接用 `border-bottom: 1px solid black`，VAN/plan 仅 grep 验证 `padding` 总体可解析；build P4 实施时发现 css parser 不处理 `border-bottom` shorthand → 改测试名为 `PaddingBottom_BlocksLastChildCollapse` + 用 `padding-bottom: 1px` 等价物理分隔符；border-bottom shorthand 缺失列入独立 P3 候选。

---

### 算法伪码累积语义 explicit method

**问题**：creative / spec 算法伪码涉及 chain / accumulator / state 累积时直接用 `=` 赋值符（如 `cur_chain = child_outgoing`）→ 歧义（覆盖？合并？swap？）→ build 阶段实施退化（既有测试合并语义被覆盖语义破坏）。

**模式**（升 P1，已写入 `creative.md` §d.2 段）：

算法伪码**禁止**对累积量直接用 `=` 赋值符，必须用 explicit method name 替代：

| 语义 | ❌ 反模式 | ✅ 正模式 |
|---|---|---|
| 合并（双向取极值 / union）| `cur_chain = child_outgoing` | `cur_chain.MergeFrom(other)` |
| 覆盖（丢弃旧值）| `cur_chain = child_outgoing` | `cur_chain.Replace(other)` / `cur = std::move(other)` |
| 交换 | 隐含 `swap` | `swap(cur_chain, other)` |
| 累积（求和）| `total = total + delta` | `total.Add(delta)` / `total += delta` |

复合赋值符 `+=` / `-=` / `|=` 视为 explicit（语义明确），不在禁用范围。

**TASK-20260430-01 实证**：spec §5.4 step 4 `cur_chain = child_outgoing` 字面赋值；build P3 GREEN 后 A5 既有 sibling collapse-through 测试 FAIL（cur_chain 累积值被覆盖丢失）→ 加 `MarginChain::MergeFrom(other)` 合并语义（max_positive / min_negative 双向取极值）+ 调用站点改为 `cur_chain.MergeFrom(child_outgoing)`，A5 PASS。

---

### scope 边界 — 副产品「优化」3 标准（与「修复」3 标准对称）

**问题**：build 阶段测试驱动发现的 pre-existing 性能优化机会（非 bug 修复，是更优算法），是否要拆任务还是顺手优化？

**模式**（升 P2，已写入 `systemPatterns.md`，与「副产品修复 3 标准」对称）：

满足以下 3 条同时成立时，scope 内顺手优化（不拆任务），在 progress.md「实证微调」列记：

1. **由本 Round bench / 测试触发** — 不是凭印象优化
2. **修复 ≤ 5 行** — 真实结构性改动须拆任务
3. **不引入新结构性改动** — 仅 hoist 局部变量 / 改循环边界 / 减少冗余调用

**TASK-20260430-01 实证**：P6.2 hoist `last_in_flow_block` 由 bench `BM_LayoutBuildTreeFlat/8` 边缘超 +15% 触发；O(N) end-of-loop scan → O(1) running pointer；实际 +7/-12 行；不引入新字段/接口；属典型「副产品优化」。Mixed bench 数据点 -9.84% 反向证明优化未引入 cache 抖动负面影响。

→ 与「副产品修复 3 标准」并列，覆盖 build 阶段「scope 内顺手做 vs 拆任务」决策完整边界。

---

## Spec 实施模式描述粒度准则（来自 TASK-20260430-02 反思）

> 反思来源：TASK-20260430-02 spec §5.3/5.4 描述「7 个独立 if 分支按字母序插入」实施形式，BUILD 阶段识别「与既有 margin/padding 同模式 → 单分支聚合 + name/Mode 映射」更优，产生形式偏差但符合 D1/D2=A 决策实质。这暴露了 spec 形式过具体的反复风险。

### 准则核心

> **spec 应描述「契约 + 约束」，而非「具体分支结构」。让 BUILD 阶段「写最少代码」原则自然驱动最优实施。**

### 具体规则

1. **效果聚焦**：spec §5（接口签名 / 实施伪码）应描述：
   - **契约**：函数行为（输入 → 输出 + 副作用 + 失败条件）
   - **约束**：实施风格（如「与既有 X 同模式」「不引入 helper / template / function pointer」「N-cap 复用既有」）
   - **不必描述**：具体分支结构（如「4 个独立 if 分支按字母序」），除非该结构本身是契约的一部分

2. **既有范本引用**：当存在 same-subsystem 范本时，应**精确引用范本的行号 + 模式名称**，而非自行从零撰写实施伪码。例：
   - ❌ Spec 自行写 50 行伪码描述 4 方向 shorthand 解析逻辑
   - ✅ Spec 引用「parser.cc:517-597 既有 `border` shorthand 范本，4 方向 shorthand 与之同模式（仅 push 1 边 3 longhand 而非 4 边 12 longhand）」

3. **D 决策矩阵粒度匹配**：D 决策应锁定「实质」而非「形式」。如 D1/D2=A 「复制粘贴」决策的实质是「不引入 helper / template / function pointer」（避免类型擦除复杂度），不是「必须 4 个独立分支」。BUILD 时识别「if 多 condition 聚合」是既有惯用法的自然推广，无需回到 PLAN。

### Spec §5 实施伪码段范本（推荐结构）

```markdown
### 5.X 实施模式

**契约**：[函数行为输入/输出/副作用/失败条件]
**实施约束**：
- 与既有 `<file>:<line-range>` 范本同模式（参考范本：[模式名]）
- 不引入 helper / template / function pointer（D<N>=A 决策实质）
- N-cap 复用既有 `<line>` 模式
**具体分支结构由 BUILD 阶段决定**（要求满足上述约束 + 与既有 shorthand 风格一致）
```

### Anti-pattern

- ❌ Spec §5 写 200+ 行具体伪码，BUILD 时实施模式与之偏差但符合 D 决策实质 → 偏差被 PLAN 形式锁死，反而需要回到 PLAN 修订（增加沟通成本）
- ❌ D 决策锁定「必须 4 个独立分支」（形式而非实质）→ BUILD 阶段识别更优实施（聚合分支）时被迫违反 D 决策

### 实证

**TASK-20260430-02**：spec §5.3/5.4 描述「7 个独立分支」，BUILD 时识别 2 聚合分支更优 → 总 LOC 212 vs 预计 350（节省 39%）+ 可读性更高 + 与既有 margin/padding 模式一致；不悖 D1/D2=A 实质（未引入 helper/template/function pointer）。本次属良性偏差，但下次 spec 应改为「契约 + 约束」描述粒度避免形式锁死风险。

---

## 已验证的模式（来自 TASK-20260430-03 全代码库 Code Review）

### Background Agent 双轨模式 + Worktree 隔离协议（TASK-30-03 反思 §4.1 — 新模式）

**问题**：用户「主线 + 后台 agent」并发模式（用户在 04 任务，agent 在 03 background 推进）暴露**并发会话切分支 race condition** — 主 worktree 单一份，外部会话切分支自动 reset agent 的 working tree，导致：
1. agent commit 混入外部会话的文件状态（reflog 显示 23:41:23/30 双切，R2.1 commit fda0517 把 04 状态文件混入 03 commit）
2. agent 改动被 working tree reset 静默丢失（R2.3 StrReplace 改动 + ctest 通过，但 git status clean — 因为 ctest 跑在切分支前的旧 working tree）
3. ctest 通过率不能作为 agent commit 完整性的充分证据

**应对协议**（4 步）：

```
1. 检测信号：
   - activeContext.md 跨 commit 莫名变化（ours vs theirs）
   - git status 显示 clean 但 agent 刚做了 StrReplace
   - git reflog --date=iso | head -20 看到 1 秒内多次 checkout

2. 隔离物理 worktree：
   git worktree add .worktree-<task_id_short>/ feature/TASK-<id>-<slug>
   # 沙箱限制：worktree 路径必须在 workspace 内（用 .worktree- 前缀避免冲突）

3. 每 commit 前断言守门：
   [ "$(git symbolic-ref --short HEAD)" = "feature/TASK-<id>-<slug>" ] || exit 1

4. CMake 配置同步：
   cmake -S . -B build -DVX_BUILD_BENCHMARKS=ON -DVX_PLATFORM_SDL2=ON
   # worktree 默认 OFF，需主 worktree CMakeCache 同步选项才能比对相同 ctest 数（基线 1062 vs 默认 1031）

5. 完成后清理：
   git worktree remove --force .worktree-<task_id_short>/
   # 可能因 google/benchmark 子模块只读 hooks 报 busy → rm -rf <wt>/build 后重试，或留作下次 reflect 复用
```

**适用场景**：
- 长任务（> 2 h）启动时 agent 自动开 worktree
- 用户主线在另一分支演进期间 agent 在 feature 分支推进
- 最终合并由用户 `/archive` 决定时机（agent 不擅自 merge main）

**实证**：TASK-30-03 R2 阶段，前两次切分支冲突（R2.1 commit 混入 + R2.3 改动丢失）后切 worktree 模式，R2.3-R2.6 + MB 收尾 5 个 commits 零干扰推进至完成。

---

### plan ×0.6 估时按任务类型分桶系数矩阵（TASK-30-03 反思 §4.3 — 元模式升级）

**背景**：systemPatterns 既有「最窄路径表」的「极窄档 0.2-0.25×」未细分任务类型，TASK-30-03 R0/R1（review 类）vs R2（fix 类）数据点显示**任务类型决定 ×0.6 系数比复杂度更显著**。

**矩阵升级**（替代旧极窄档单一系数）：

| 任务类型 | plan ×0.6 系数 | 来源数据点 | 估时建议 |
|---|---|---|---|
| **蓝图任务 V2=a + 批量决策跳过 + 批量文档（≥ 3 篇）** | **0.33-0.59× ×0.6 / 0.20-0.35× plan** | **TASK-30-04 (0.27-0.35× plan / 0.46-0.59× ×0.6) + TASK-30-02 (0.22× plan) + TASK-30-01 P6 (0.21× plan) — 3 数据点群组** | **极窄档 — 取下限保守估，加 ~30% buffer 给 reflect/archive** |
| Review / 审查 / 检测（无新代码）| **0.4-0.7× ×0.6** | TASK-30-03 R0 (0.69×) / R1 (0.78×) + 历史 review-type | 取下限保守估 |
| Fast fix（笔误 / 注释 / 单行守卫）| **0.7-1.4× ×0.6** | TASK-30-03 R2 (扣冲突 1.36×) | 标准 |
| Race condition / 工程开销加权 | **1.4-2.5× ×0.6** | TASK-30-03 R2 (含冲突 2.12×) | 加 +25-50% buffer |
| 大件实现 / 跨子系统 | **0.6-1.1× ×0.6** | 历史 Level 3-4 + **TASK-20260502-01 总 0.64× 落桶下沿外** | 下调 — 详见 TASK-20260502-01 reflection §11.2 |
| **极细致 plan + 决策跳过率高 + plan-fact reconcile 低**（子桶）| **0.40-0.60× ×0.6** | TASK-20260502-01 Phase A.0 + A.2 + A.3（0.40-0.50×）| 子桶 — 应用条件：plan 步骤 ≥ 5 步 TDD 模板锁死 + 子任务粒度 ≤ 30 min plan ×0.6 + grep 实证已在 plan 阶段完成 |
| **批量小子任务**（子桶，Phase A.2 / A.3 风格）| **0.30-0.50× ×0.6** | TASK-20260502-01 Phase A.3.1 (0.26×) + A.3.2 (0.11×) + A.2.2/3 (0.33-0.39×) | 子桶 — 应用条件：连续 ≥ 4 子任务每个 ≤ 20 min plan ×0.6 + 复用既有基础设施 |
| **escalation 后子任务**（子桶）| **~1.0× ×0.6** | TASK-20260502-01 Phase A.1 escalation 后 (0.99×) | 子桶 — escalation 估时已校准；reflect 阶段对照「escalation 后 plan ×0.6」单独入库 |
| **极窄档延续高效区**（新子档 — TASK-20260502-02 反思入库） | **0.30-0.45× ×0.6** | **TASK-20260502-02 Phase B 10 子任务（0.40× 群组）— 第 38-47 数据点** | **触发条件叠加（4 项必须全满足）：(1) Phase 0 ≥ 9 子段实测填写（grep / ctest baseline / CMake / 测试基础设施审计 / CSS 能力 / 工具链）+ (2) 5 大可复用架构范式 ≥ 5 项命中（中央调度协议 / 函数指针 nullptr / 双层 API / #ifdef CMake / dogfood）+ (3) 零新子系统设计（仅扩展既有子系统）+ (4) plan-fact reconcile ≤ 1 处** |
| **极窄档加速衰减区**（新子档 — TASK-20260503-01 反思入库 / 候选）| **0.20-0.30× ×0.6** | **TASK-20260503-01 Phase C 11 子任务（0.31× 群组）— 第 48-58 数据点 / 命中下沿** | **触发条件叠加（5 项必须全满足）：(1) 极窄档延续高效区 4 触发条件**（已含）**+ (2) lazy-attach C ABI 容错模式直接复用**（即从 Phase B 复用 lazy-attach 范式无需新设计）**+ (3) dogfood 路径第三次（含）以上复用**（hello_devtool 第三次扩展即为成熟范式）**+ (4) Phase 0 投入定律 ≥ triple-evidence 历史**（前置任务连续 ≥ 3 次实证）**+ (5) 跨子任务 5 大范式 100% 命中（含 N/A 计入）** — Phase A 0.64× → Phase B 0.40× → Phase C 0.31× 三连递降趋势识别 |

**适用启示**：
- spec / plan 阶段先标注任务类型，再选系数估时
- VAN 阶段如识别可能 race condition（双 agent / 用户主线在同分支），plan 估时加 buffer
- **新档「蓝图任务 V2=a」触发条件**：N ≥ 5 决策连续按推荐默认锁定（Checkpoint 推荐默认 + 隐式批准协议叠加） + 批量文档产出（≥ 3 篇 1 个 session 内连续完成） + 无 build 实施 — 三因素叠加显著加速。**仅当用户决策真实合理时才适用**（推荐错误 + 跳过 = 设计缺陷），需在 reflect 阶段重新审视决策合理性

**实证**：
- TASK-30-03 三轮次实测总 ~177 min（plan 295-345 min ×0.6 = 177-207 min），落在 ×0.6 区间内 0.85-1.00× — **plan ×0.6 模型有效，按任务类型分桶可进一步提升准度**
- TASK-30-04（**第 17 数据点入库**）实测 ~74 min 主线（plan 210-270 min ×0.6 = 126-162 min），0.46-0.59× ×0.6 — **蓝图任务 V2=a 极窄档首次完整记录，群组化 3 数据点（TASK-30-04 + TASK-30-02 + TASK-30-01 P6）**
- **TASK-20260502-01（第 18-37 数据点入库 = 20 个新数据点群组）** — DevTool Phase A 实施类 Level 4 任务，~281 min 主线 vs 441 min plan ×0.6 = **0.64×（落「大件实现」桶下沿外，触发桶系数下调 0.8-1.2× → 0.6-1.1×）**：
  - Phase A.0（前置改造 6 子任务）实测 0.50× → 「极细致 plan」子桶证据
  - Phase A.1（dogfood UI 8 子任务）实测 **0.99×** → escalation 后 plan 校准价值证明（详见「plan escalation 中途触发」段）
  - Phase A.2 + A.3（安全测 + example 6 子任务）实测 0.40× → 「批量小子任务」子桶证据
  - 中位 0.56×，分布偏左极窄档为主；A.0.1 = 0.18× 是新极值下限（plan 估时基于 spec 估而非 VAN 实证，下次类似情况应以 VAN 实证更新 plan 估时）；A.3.2 = 0.11× 是最快数据点（integration verify 工作大部分已分散到每轮 build step 6）；A.2.4 = 0.89× 是「小工具类」近 1× 唯一数据点
- **TASK-20260502-02（第 38-47 数据点入库 = 10 个新数据点群组）** — DevTool Phase B 实施类 Level 3 任务，~104 min 主线 vs 261 min plan ×0.6 = **0.40×（落「极窄档延续高效区」候选新子档 0.30-0.45×）**：
  - 10 子任务分布：B.0.1 0.46× / B.0.2 0.19× / B.1.1 0.22× / B.1.2 0.56× / B.2.1 0.26× / B.2.2 0.72× / B.2.3 0.39× / B.3.1 1.11× / B.3.2 0.39× / B.3.3 0.28×
  - 中位 0.39×，分布偏左极窄档（7/10 子任务 ≤ 0.5×）；B.0.2 = 0.19× 接近 Phase A 历史极值下限（Vector + push_back + clear 极简 API）；B.3.1 = 1.11× 是唯一接近 1.0× 数据点（跨 5 文件更新 — 单文件计时小但跨度多，反映 Phase 0 实证文件清单 ≥ 4 时的「跨度税」）
  - **反复模式命中 0/7**（连续两次任务零反复 — Phase A 同样 0/7） = 工作流规则 + Phase 0 + 范式复用 = 反复模式有效抑制器
  - VAN 预测 0.55-0.75× 偏保守 -50%（实测 0.40×） — VAN 预测应在 Phase 0 ≥ 9 子段实测填写完成后 +5 大范式 ≥ 5 项命中 + 零新子系统时下调到 0.30-0.45× 区间
- **TASK-20260503-01（第 48-58 数据点入库 = 11 个新数据点群组）** — DevTool Phase C 实施类 Level 3 任务，~104 min 主线 vs 333 min plan ×0.6 = **0.31×（落「极窄档加速衰减区」新子档 0.20-0.30× 候选 — 命中下沿）**：
  - 11 子任务分布：C.0.1 0.44× / C.1.1 0.20× / C.1.2 0.24× / C.1.3 0.09× / C.2.1 0.28× / C.2.3 0.39× / C.3.1 0.44× / C.4.1 0.44× / C.4.2 0.56× / C.5.1 0.26× / C.5.2 0.67×
  - 中位 0.28×，分布偏左极窄档（8/11 子任务 ≤ 0.4×）；**C.1.3 = 0.09× 是新极值下限**（C.1.2 测试基础设施已就绪 → 4 测增量极快，破 Phase A.3.2 = 0.11× 历史极值）；C.5.2 = 0.67× 是上沿（含 OFF 路径 unique_ptr incomplete type 修复 — plan 未识别工程细节）
  - **反复模式命中 1/7 部分命中**（前置依赖/环境/API 能力未验证 — CssParser 严格性假设 + 工具链 binutils 2.46+ 行为变化 2 项）— 比 Phase A/B 0/7 小幅回升；触发因子：(1) binutils 2.46+ 是激进新升级 + (2) 既有组件能力假设未在 Phase 0 grep 实测
  - **Phase 0 投入定律 triple-evidence 升级**：A 5.3× / B 5.2× / **C 7.6× ROI**（30 min Phase 0 投入 → 节省 ~229 min build phase）— C 比 A/B 更高 ROI 因为：(1) 5 大范式 100% 命中无新探索成本 + (2) lazy-attach C ABI 模式直接复用 + (3) dogfood 路径第三次复用 = 复合加速效应
  - **「极窄档延续高效区下沿挤压」触发新子档登记**：Phase A 0.64× → Phase B 0.40× → Phase C 0.31× 三连递降趋势识别；新子档「极窄档加速衰减区 0.20-0.30×」候选触发条件已沉淀到上方 plan ×0.6 矩阵段
  - VAN 预测 0.30-0.45×（命中下沿） — 预测准度高，比 Phase B VAN 预测 0.55-0.75× 偏保守 -50% 显著改善；预测应在 lazy-attach C ABI 模式 + dogfood 第三次复用条件下下调到 0.20-0.30× 区间

---

### Quick Fix 颗粒度估时基准（TASK-30-03 反思 §4 — 数据点）

**问题**：plan 通常估 quick fix 5-15 min/项（平均 9 min/项），实测发现颗粒度被低估。

**实测基准**（TASK-30-03 R2 6 项 quick fix）：

| Fix 类型 | 实测 | plan 估 | 偏差 |
|---|---|---|---|
| 注释 / 文档（read + 1-2 StrReplace + commit）| ~5 min | 5 min | 0 |
| 单行守卫 / 重构（含 verify + ctest）| ~10 min | 10 min | 0 |
| 加守卫 + 新单测（RED-GREEN-REFACTOR）| ~15 min | 15 min | 0 |
| 构建系统改动（CMake + 新文件）| ~15 min | 15 min | 0 |
| **平均（含上下文切换）** | **~12 min/项** | 9 min/项 | **+33%** |

**根因**：plan 估时通常只算"代码改动"，未计入：
- 上下文切换（读相关文件 + verify 改动）
- commit message 撰写
- 每项 ctest / build 验证

**校准建议**：
- spec / plan 估 quick fix 时按平均 **12 min/项**（含上下文切换 + 验证）
- 如 N 项 quick fix，总估 = N × 12 min

---

### Checkpoint 推荐默认 + 隐式批准协议（TASK-30-03 反思 §4.4 — 新模式）

**问题**：spec 协议「Checkpoint 用户决策」原版要求强制 AskQuestion 流程，但用户调 `/build` / `/reflect` 续推时未明确选项 → agent 等待 vs 推进的两难。

**推荐协议**（已在 TASK-30-03 Checkpoint 1 验证）：

```
agent 收到续推命令（/build /reflect 等）但 Checkpoint 选项未明确：

1. 检查 spec / plan 的「推荐方案」标记
2. 开头明确告知假设：
   "按 Checkpoint X 推荐选项 A 默认执行：[详细描述]。如有不同意请中断。"
3. 立刻开始执行，每步可中断
4. 用户跳过补问 / 不打断 = 隐式批准
```

**适用前置**：
- Checkpoint 推荐方案明确（spec 已标注 推荐 / 安全选项）
- 风险低（quick fix 类 / 文档 / 重构）
- 用户可随时中断（后续 commits 可 revert）

**不适用**（仍需强制 AskQuestion）：
- Checkpoint 涉及破坏性 / 不可逆操作（删除 / 强制 push / migration）
- 推荐方案不明确（多个等价选项）
- 高风险（生产部署 / 安全策略变更）

**实证**：TASK-30-03 Checkpoint 1 用户调 `/build` 隐式批准选项 A（continue_r2 全 6 项 quick fix）— 后续跳过补问 = 确认 — agent 推进 R2 至完成（除会话冲突外零打断）。**协议有效**。

---

### Review 类任务 6 维度 × 抽样深度矩阵 spec 模板（TASK-30-03 反思 §10.1 — 新元模板）

**适用**：未来 6-12 个月 / Q1 / 大版本前的引擎全代码库 review 任务（Level 4）。

**模板核心结构**：

```markdown
## V1 范围决策
- A 全代码库 / B 子系统 / C 焦点模块

## V2 维度选择
- 性能 / 正确性 / 可维护性 / 安全 / 测试 / 一致性（6 选 N 或 all）

## V3 输出形态
- A 仅报告 / B 报告 + Top fix / C 完整实施（多轮次 + Checkpoint）

## V4 视角
- D 混合（外部 reviewer + 内部 systemPatterns 验证）

## V5 安全标注
- 是 / 否

## 抽样深度矩阵（H/M/L）
- H 高优先 ~20 文件 — 深读全文
- M 中优先 ~80 文件 — grep + 浏览
- L 低优先 ~36 文件 — 跳过
- 总文件数 ≈ N（按子系统拆 a/b/c/...）

## 多轮次划分（策略 X）
- R0 prep（必然）：grep fingerprint + 抽样名单 + ctest 基线 + CVE 审计 + lcov 覆盖率
- R1 必然：6 维度全代码库 review 报告
- Checkpoint 1：用户审报告决定 R2 范围
- R2 条件触发：P0 quick fix 5-15 项
- Checkpoint 2：用户决定 R3+ 拆分
- R3+ 拆出独立后续任务（不在本任务内）

## 估时
- R0 ~90 min（plan ×0.6 ~54 min；实测 0.41-0.69× plan ×0.6）
- R1 ~150-200 min（plan ×0.6 ~90-120 min；实测 0.50-0.78×）
- R2 ~5-15 项 × 12 min ≈ 60-180 min（plan ×0.6 ~36-108 min；实测 0.82-2.12× 含 race condition）
- 总 ~5-8 h plan / ~3-5 h plan ×0.6
```

**实证**：TASK-30-03 完整实施该模板，55 项 findings 跨 6 维度（28 P1 + 19 P2 + 8 P3）+ 13 项 R3+ 拆分任务建议产出。

---

### Level 4 蓝图任务 V2=a 工作流变体（TASK-20260430-04 反思 §10 — 新元模板）

**适用**：Level 4 任务的 V2 决策 = **a 纯蓝图**（spec + plan + creative ×N，**不含 build 实施**），主交付为蓝图级文档；build 由用户基于 plan 后续独立立项（拆出 N 个 Level 3 子任务）。

**典型场景**：
- DevTool 套件设计（Inspector / Performance Overlay / Hot Reload — 见 TASK-20260430-04）
- Console / JS Debugger 蓝图阶段
- 大版本架构变更前的设计冲刺
- 跨子系统的协议/API 设计

**工作流路径**：
```
/van → /plan（含 brainstorming + creative 内联）→ /reflect → /archive
                                                  ↑ 跳过 /build
```

**核心机制**：「广义构建 = 主交付物落盘」— 蓝图任务的「构建中」语义 = 「文档产出中」，plan 完成后可直进 reflect。

**spec 12 段式样**（TASK-20260430-04 验证）：
1. Purpose / 目的
2. Out of scope / 不在范围
3. Acceptance criteria（≥ 10 项 A1-AN）
4. 锁定的 N 个设计决策（D1-DN，含选项 / 推荐 / 实证依据）
5. 架构图 + N 个注入点（I1-IN）+ 可行性评估
6. 数据流图（如 Inspector hover / Hot Reload watch）
7. Dogfood / 自渲染策略（如适用）
8. 分阶段实施计划（A/B/C/... 子系统）
9. 安全威胁模型（T1-TN，每项含触发 + 影响 + 缓解）
10. 测试策略
11. 风险项（R1-RN）+ 缓解
12. 兼容性 / 后续任务关系（含 ≥ N 项独立任务候选）

**自我对照 ≥ 30 模式**：spec 起草中持续对照 `systemPatterns.md` 已沉淀模式（建议 ≥ 30 处引用），确保新设计与已有架构决策一致，**禁止**重复发明轮子或与既有模式冲突。引用模式时使用『模式 X 第 N 段』格式标注。

**批量文档 batch 协议**：
- spec / plan / creative ×N 在**同一会话**内一次性产出（不切回用户审）
- 决策矩阵全部 lock 完成后才开始批量写文档
- 每篇文档完成后立即 `Write` 落盘（不等所有文档都写完）
- 全部文档落盘后 → 1 次性同步 MB 三件套（tasks.md + activeContext.md + progress.md）+ 1 次性 commit
- 分支策略：用户在 `/van` 阶段已切到独立分支（如 `task04-devtool-blueprint`），所有文档在该分支上线性提交

**估时桶**（与 plan ×0.6 矩阵对应）：
- 蓝图任务 V2=a + 批量决策跳过 + 批量文档（≥ 3 篇）→ **0.20-0.35× plan**（极窄区间，证据基 TASK-30-04 + TASK-30-02 + TASK-30-01 P6）
- 比同类 quick fix（0.25-0.50× plan ×0.6）更高效，因为决策跳过 + 批量产出消除了切换成本

**Reflect 命令前置条件灵活解读**：
- 原版 `reflect.md` 检查「当前阶段必须是 `构建中`」对蓝图任务应灵活解读
- 蓝图任务的「构建中」语义 = 「文档产出中」（spec / plan / creative）
- plan 完成 + 用户确认 → 即可进入 `/reflect`，无需「补 build 阶段」
- 见 `.cursor/rules/main.mdc` § Level 4 蓝图任务 V2=a 工作流变体段

**实证**：TASK-20260430-04 完整实施该变体，1 次会话产出 4 篇文档（spec + plan + 2 creative，~1879 行）+ 8 项 D1-D8 决策 lock + 4 项技术债（#26 #35 #40 #4）映射 + 7 项独立后续任务候选 + 8 项 T1-T8 威胁模型。实测 plan ×0.6 估时桶系数 0.33-0.59×（落入「极窄区间」），优于 quick fix 类型。

---

### Dogfood 路径 = 探测性 acceptance test（TASK-20260430-04 反思 §4.5 — 新长期模式）

**适用**：DevTool / Console / 完整 UI 编辑器 等「自渲染主线」（用引擎自身 HTML/CSS/JS 实现），即用 Veloxa 引擎实现 Veloxa 引擎自身的工具。

**双重价值**：

1. **功能性价值**（显性）：少引入新依赖，与嵌入式定位一致；Veloxa 自渲染 UI 不需要 ImGui / Qt / Electron 等额外 UI 库
2. **探测性价值**（隐性，更重要）：引擎自身能否驱动复杂交互式 UI = HTML/CSS/JS 子集成熟度的 acceptance test
   - 如能驱动 splitter 拖拽 + 实时数字更新 + 半透明合成 + DOM tree 渲染 → 子集足够构建真实 UI
   - 如不能 → 暴露的缺陷 = R3+ 候选输入源

**应用模式**：

1. **spec 阶段**：自渲染主线 spec 列出「依赖的 CSS / HTML / JS 子集 + grep 验证」（参考 TASK-30-04 plan §0.9 CSS shorthand 能力 grep 表）
2. **实施阶段**：记录「实际触及但失败的 CSS / HTML / JS 子集」 → R3+ 候选输入源
3. **回顾阶段**：反向暴露的缺陷立即列入 P3 候选（不阻塞主线）；补做候选独立立项 ROI 排期

**适用场景**：

- DevTool 三件套（V3=A）— TASK-30-04（已蓝图）
- Console / JS Debugger / 完整 UI Editor 扩展段 — TASK-30-04-D/E/G
- 任何「Veloxa 引擎自我应用」主线
- 反例：纯算法基线测试（不涉及 UI 渲染）/ 命令行工具（无 UI）

**与传统 acceptance test 对比**：

| 维度 | 传统 unit/integration test | Dogfood acceptance test |
|---|---|---|
| 测试主体 | 引擎单一函数 / 模块 | 引擎自身驱动一个真实 UI |
| 通过判据 | 期望值 == 实际值 | UI 能否正常交互（人观感）|
| 缺陷暴露 | 已知缺陷验证 | 未知缺陷探测 |
| 维护成本 | 高（每个 case 需维护）| 低（UI 即测试场景）|
| ROI | 单点缺陷修复 | 面状缺陷探测 + 自我推动改进 |

**实证**：TASK-20260430-04 spec V3=A 自渲染锁定 — DevTool UI（splitter dock + Inspector panel + Performance HUD overlay + 半透明合成）将作为 Veloxa CSS / HTML / 事件子集成熟度的探测性 acceptance test。预期 build 阶段（TASK-30-04-A/B/C）反向暴露 5-10 项 R3+ 候选缺陷（layout 边界 / event 冒泡 / image cache 命名空间冲突等）。

**实证更新（TASK-20260502-01 Phase A.1.8）：** dogfood 实质执行首次完整记录，A.1.8 inspector_panel.js 跑 setupTabs() 时**精准暴露 3 个 R2 引擎缺陷**：
1. `Element.children` 集合 getter 缺失（HTMLCollection 风格）
2. `element.addEventListener(type, handler[, options])` 缺失（已有 EventManager API，需 JS binding 暴露）
3. `element.innerHTML` setter 缺失（HTML parser 重新解析子树）

**dogfood 缺陷处理策略锁定（新增 SOP）：** 当 dogfood 暴露 R2 引擎缺陷，**优先选 panel-side defensive**（在自渲染 JS 加 try/catch + binding 可用性 check）让主链路验证打通，**不选 engine-side fix** 因后者是独立 P3 范围；缺陷沉淀给 P3 候选清单（含文件位置 + 缺陷描述 + 临时 mitigation + 优先级）；reflect 阶段提交 P3 立项预案。

---

## plan escalation 中途触发（TASK-20260502-01 Phase A.1 实证）

### 触发条件

- Build 阶段中途识别某 Phase 子任务量级显著超出初始 plan 估时（≥ 1.5×）
- 通常因 plan 段写作粗糙（概念列举非 executable 步骤）+ build 阶段实施时识别配套未规划维度（架构耦合 / 子系统级工作量）
- 用户在 build 中途选择 D「返回 /plan 修正」决策（plan ×0.6 矩阵的 escalation 路径）

### 触发实证（TASK-20260502-01 Phase A.1）

- 初始 plan §A.1 段：4 子任务概念描述（A.1.1-A.1.4），~120 min plan
- 轮次 3 中识别配套需求：overlay / resource embed / panel.html/css/js / JS binding / InputSplitter / 双 UpdateManager / C API / dogfood smoke = 8 维度，~240 min plan（×2 量级）
- escalation 触发：plan 文档 +384 行替换 A.1 整段为 8 子任务 5-步 TDD 模板 executable 段
- escalation 后实测：5 轮 build / ~143 min vs escalation 后 plan ×0.6 144 min = **0.99×（近 1× 完美匹配）**

### 价值证明

- escalation 投入 ~30 min plan 时间
- escalation 后 plan 估时质量显著优于初始 plan（系数从未 escalation 的 0.40-0.50× 跃升到 0.99×）
- 不 escalation 而硬干的反事实假设：~8 子任务无 5-步模板支撑 → 估算每子任务 +30-50% 时间 → 总 ~60-90 min 额外开销 + 质量更不稳定（无反向探针标准化 + plan-fact reconcile 散乱）
- **ROI ≈ 1:2-3**（escalation 30 min 换实际 60-90 min 节省 + 质量提升）

### SOP（流程化建议）

1. **识别信号**：build 阶段每子任务完成后 retrospect — 实测 vs plan ×0.6 比值 ≥ 1.5× 且后续未实施子任务的 plan 描述粗糙（概念列举非 executable）→ 触发 escalation 候选
2. **决策点**：用户 AskQuestion「选项 A 续 build / B 中途暂停审 commit 链 / **C 返回 /plan 中途精化** / D 提前 reflect」
3. **escalation 执行**：
   - Build 阶段 commit chain 不动（保留 round-pause commit + activeContext 子状态标签）
   - plan 文档替换原 Phase 段为详细 5-步 TDD 模板 executable 段
   - 单独 commit `docs(plan): TASK-XXX plan escalation — Phase X.Y Level Z + N subtasks detailed plan`
   - 切回 build 续做后续轮次
4. **escalation 后估时校准**：reflect 阶段对照「escalation 后 plan ×0.6 系数」单独入库，避免与 escalation 前数据点混淆

### 反模式

- ❌ **plan 段写作粗糙不 escalation 直接硬干** — 实施期质量不稳定 + plan-fact reconcile 散乱 + 反向探针无标准
- ❌ **escalation 后不 commit plan 变更** — git history 无法回溯 escalation 决策
- ❌ **escalation 后跳过 5-步 TDD 模板填充** — 复用未 escalation 的 plan 描述实质无 escalation 价值

### 数据点

- TASK-20260502-01 Phase A.1（DevTool dogfood UI 8 子任务）：escalation 后 0.99× — 首次完整实证（第 30 数据点群组）

---

## 反向探针有效性陷阱清单（TASK-20260502-01 多子任务沉淀）

### 背景

5-步 TDD 模板的 step 5「反向探针」要求修改生产代码让目标测精准 FAIL 以验证 test 真有捕获能力。但**探针选择不当会导致探针无效**（修改生产代码后目标测仍 PASS），浪费 5-10 min/次 调试时间且不增 test quality。

### 4 类陷阱实证（TASK-20260502-01）

| # | 陷阱类型 | 实证子任务 | 失败原因 |
|:-:|---|:-:|---|
| 1 | **修改默认值 — 但测主动 set 该值** | A.0.2 探针 1 | `collapsed_through` 默认 `false→true`，但测 #665 主动 `box.collapsed_through = true`，默认值变化检测不到 |
| 2 | **修改路径与目标测无交集** | A.0.3 探针 2 | `IsSensitiveAttribute` 的 `kInput` 检查改 `return true`，但 input 元素本身就是 kInput tag，修改路径与目标测无交集 |
| 3 | **整体替换函数为 no-op — 但 null path 测仍 PASS** | A.1.1 探针 | 函数体改 `(void)list; ...`，但 `NullBoxIsNoOp` 测因 null path 本身就是 no-op 仍 PASS（无效探针） |
| 4 | **empty function body — 触发 -Werror=unused-function** | A.1.4 + A.1.6 探针 | 改 empty body 时编译错而非 test FAIL，虽是「捕获能力」但不是 test 信号 |

### SOP（探针选择优先级）

**优先级 1（推荐）：保留代码但故意修改 string literal / 参数 / 边界**
- 例：A.2.3 反向探针把 `if (needed > max_size)` 改 `>=`（off-by-one），`MaxSizeAtExactNeededAccepts` 测精准 FAIL
- 例：A.1.4 反向探针把 `register("vx_devtool_get_dom_json")` 改 `register("vx_devtool_get_dom_json_PROBE_TYPO")`，4 主测精准 FAIL
- 例：A.2.1 反向探针把 `set_redaction_policy` 改 noop，2 切换测精准 FAIL，3 不依赖切换测仍 PASS（**定向 FAIL** 期望符合）

**优先级 2（接受）：整体替换为正确语义但无效操作**
- 例：A.2.2 反向探针把 `ResetOverlayCommands` 改 noop，多帧隔离测精准 FAIL，负控制测仍 PASS

**优先级 3（避免）：empty body / 修改与目标测无交集 / 修改默认值**
- 见上表 4 类陷阱

### 检查清单

每次反向探针前 30s 自检：
1. ✅ 探针修改的代码路径**确实被目标测执行到**（grep 测代码 → 确认覆盖）
2. ✅ 探针不依赖编译错误（应让 RED 信号是 test FAIL 而非 build FAIL）
3. ✅ 期望「**定向 FAIL**」（明确哪些测 FAIL 哪些 PASS），无效探针即停止改用其他类型

### 数据点

- TASK-20260502-01：16 子任务 100% 反向探针，其中 4 次踩坑沉淀 4 类陷阱

---

## 子系统关闭守门 ctest smoke 范式（TASK-20260502-01 A.2.4 实证）

### 背景

A14 类 spec「子系统关闭时构建产物零变化（链接闭合 + binary size diff = 0）」的传统验证依赖**人工每轮 build 后手动 `nm | grep`**，成本高 + 漏验风险。TASK-20260502-01 A.2.4 首次实施**自动化 ctest smoke** 范式，让任何后续子系统扩展自动验证零字节增长。

### 实施模板

```cmake
# tests/smoke/<subsystem>_link_closure.cmake
# ============================================================================
# Pure CMake script — no compiled artifact, runs every ctest invocation.
#
# Required CMake-defined variables (-D on cmake -P):
#   LIB             — absolute path to public lib under test (e.g. libvx_api.a)
#   CORELIB         — absolute path to core lib under test (e.g. libvx_core.a)
#   SUBSYS_ON       — "ON" or "OFF" (e.g. derived from VX_BUILD_DEVTOOL)
# ============================================================================

if(NOT EXISTS "${LIB}")
  message(FATAL_ERROR "smoke: ${LIB} not found")
endif()

execute_process(COMMAND nm "${LIB}" OUTPUT_VARIABLE api_syms ERROR_QUIET)
execute_process(COMMAND nm "${CORELIB}" OUTPUT_VARIABLE core_syms ERROR_QUIET)
file(SIZE "${LIB}" api_size)
file(SIZE "${CORELIB}" core_size)
message(STATUS "smoke: ${LIB} = ${api_size} bytes")
message(STATUS "smoke: ${CORELIB} = ${core_size} bytes")

# 子系统内部符号黑名单（参数化）
set(subsystem_internal_syms
    "Subsystem_InternalSymbol1"
    "Subsystem_InternalSymbol2"
    "kSubsystem_Resource1"
    "kSubsystem_Resource2"
    "Subsystem_PrivateClass")

if(SUBSYS_ON STREQUAL "ON")
  # Sanity: 公开 C ABI stub 必须存在
  if(NOT api_syms MATCHES "vx_subsystem_public_api")
    message(FATAL_ERROR "ON sanity: vx_subsystem_public_api missing")
  endif()
else()
  # OFF: 验证零内部符号引用
  set(combined_syms "${api_syms}\n${core_syms}")
  foreach(sym IN LISTS subsystem_internal_syms)
    if(combined_syms MATCHES "${sym}")
      message(FATAL_ERROR "OFF FAIL: subsystem internal symbol '${sym}' leaked")
    endif()
  endforeach()
  message(STATUS "smoke: OFF link closure verified zero subsystem symbols")
endif()
```

```cmake
# tests/CMakeLists.txt 注册
if(VX_BUILD_SUBSYSTEM)
  set(_flag "ON")
else()
  set(_flag "OFF")
endif()
add_test(NAME subsystem_link_closure_smoke
  COMMAND ${CMAKE_COMMAND}
    -DLIB=$<TARGET_FILE:vx_api>
    -DCORELIB=$<TARGET_FILE:vx_core>
    -DSUBSYS_ON=${_flag}
    -P ${CMAKE_SOURCE_DIR}/tests/smoke/<subsystem>_link_closure.cmake)
```

### 价值

- **零编译产物开销**（pure CMake script）
- **每次 ctest 自动跑**（pre-merge 强制守门）
- **同时支持 ON + OFF 两侧验证**（ON sanity check + OFF zero closure）
- **Echo 字节大小到 ctest log**（人工对照备份）
- **反向探针支持**（临时加一个一定存在符号到黑名单 → smoke 精准 FAIL 验证检测能力）

### 数据点

- TASK-20260502-01 A.2.4：DevTool 子系统首次实施 — DEVTOOL=ON / OFF 双侧 PASS，反向探针 verified
- TASK-20260502-02 B.3.3：黑名单维护演进透明度子段实证 — Phase A → +8 项 / Phase B → +2 项的递增模式 + Phase A/B 区分注释 + NOT in blacklist intentional 段说明（vx_view_set_pipeline_hooks / vx_view_get_perf_stats / vx_view_is_hud_visible 公开 C ABI 是设计 OFF stub 不入名单 / VxViewGetPerfStats 在 vx_script anonymous namespace 不在检查 archive 内）；ON+OFF a14 smoke 双绿；连续两个任务零反复模式命中

---

## Phase 0 投入越深 / build phase 越快定律（TASK-20260502-02 反思 §2.1 — dual-evidence / TASK-20260503-01 反思 §6.1 — triple-evidence 升级）

### 背景

writing-plans 规则要求 plan 阶段 Phase 0 必填 ≥ 9 子段实测填写（grep / ctest baseline / CMake 链接拓扑 / 静态库循环依赖审计 / FetchContent 守卫 / 测试基础设施审计 / 边界输入清单 / 调用链端到端 / 工具链）。这些实测看似「重前期投入」，但实证显示有显著 ROI。

### 定律陈述

```
Phase 0 实测投入 X 分钟 → build phase 节省 ~5X 分钟（以 plan ×0.6 为基线）
```

### 实证（triple-evidence — 三个独立任务连续验证）

**TASK-20260502-01 Phase A**（首次发现）：
- Phase 0 实测投入 ~30 min
- build phase 实测 281 min vs plan ×0.6 441 min = 0.64× → 节省 160 min
- ROI = 160 / 30 = **5.3×**

**TASK-20260502-02 Phase B**（第二次实证）：
- Phase 0 实测投入 ~30 min（11 子段实测填写更细致）
- build phase 实测 104 min vs plan ×0.6 261 min = 0.40× → 节省 157 min
- ROI = 157 / 30 = **5.2×**
- **比 Phase A 单子任务平均还快**（Phase B 0.40× < Phase A 0.64×） — 因 Phase A 沉淀的 5 大范式在 Phase B 100% 命中，Phase 0 投入收益叠加范式复用收益

**TASK-20260503-01 Phase C**（第三次实证 — triple-evidence 升级）：
- Phase 0 实测投入 ~30 min（13 子段实测填写 — §0.7 LoadCSS 路径 100% 安全 grep + §0.12 std::thread 零既有用例 grep + §0.13 realpath 选型决策实证）
- build phase 实测 104 min vs plan ×0.6 333 min = 0.31× → 节省 229 min
- ROI = 229 / 30 = **7.6×**
- **比 Phase A/B 进一步加速**（C 0.31× < B 0.40× < A 0.64×） — 复合加速效应：(1) 5 大范式 100% 命中无新探索成本 + (2) lazy-attach C ABI 模式直接复用（Phase B 沉淀）+ (3) dogfood 路径第三次复用（hello_devtool 第三次扩展即成熟范式）

**ROI 公式入库：**

```
ROI = (plan ×0.6 总分钟数 - 实测 build 分钟数) / Phase 0 投入分钟数
```

**实证数据点（triple-evidence）：**

| 任务 | Phase 0 投入 | plan ×0.6 | 实测 build | 节省 | ROI |
|:--|--:|--:|--:|--:|:-:|
| TASK-20260502-01 Phase A | 30 min | 441 min | 281 min | 160 min | **5.3×** |
| TASK-20260502-02 Phase B | 30 min | 261 min | 104 min | 157 min | **5.2×** |
| TASK-20260503-01 Phase C | 30 min | 333 min | 104 min | 229 min | **7.6×** |
| **平均** | 30 min | 345 min | 163 min | 182 min | **6.0×** |

**结论：** ROI 区间 5.2-7.6×（triple-evidence），平均 6.0×。**ROI 不是固定值，而是随「跨任务范式累积」呈正反馈增长**：
- Phase A 沉淀 5 范式 → Phase B/C 100% 复用收益叠加
- Phase B 沉淀 lazy-attach C ABI 模式 → Phase C 复用 + 扩展（VX_WARNING_HOT_RELOAD_FAILED warning 语义层）
- Phase C 沉淀「极窄档加速衰减区」候选新子档 → 未来任务 ROI 预测应在 Phase 0 投入前先评估「范式累积深度」

### 启动条件（必须叠加 — Phase 0 投入的「触发器」）

满足全部 4 项时，Phase 0 投入 ROI 才能落入 5× 区间：

1. **任务范围跨 ≥ 3 子系统** — 单子系统改动不需要 Phase 0 拓扑审计
2. **既有代码库 ≥ 5 个可复用范式** — 范式复用是 ROI 的主要来源
3. **plan 阶段 plan-fact reconcile 历史频率 ≥ 5 处/任务** — Phase 0 实测填写是直接对治
4. **VAN 阶段已识别 ≥ 5 个前置依赖** — Phase 0 grep 实证是依赖验证的标准化形式

不满足时（e.g. quick fix / 单文件改动 / 全新子系统）：Phase 0 简化为 ≤ 3 子段（ctest baseline + CMake 链接 + 边界输入清单），避免过度投入。

### 反模式

❌ **「Phase 0 复制粘贴范式」** — 把上一个任务的 Phase 0 模板直接复制不实测填写
- 实测：Phase A.0.1 plan 估时基于 spec 估而非 VAN 实证 → 实测 0.18× 是新极值下限（plan 估时不准导致 ×0.6 比值失真，非 build phase 真的快）

❌ **「Phase 0 跳过 ctest baseline 二次验证」** — 直接用 main 分支基线数字
- 实测：TASK-20260502-02 plan §0.1 必跑 reconfigure 后实测 1169/1065 与 main `8b2ead4` 终态一致 ✅；如不验证可能漏过环境差异（e.g. CPU / SDL2 driver）

### 数据点

- TASK-20260502-01 Phase A：30 min 投入 → 节省 160 min = 5.3× ROI
- TASK-20260502-02 Phase B：30 min 投入 → 节省 157 min = 5.2× ROI
- 平均 ROI ≈ 5.25×

---

## lazy-attach C ABI 容错模式（TASK-20260502-02 反思 §4.5 — 新模式 / TASK-20260503-01 反思 §4.2 — warning 语义层扩展）

### 背景

C ABI 设计中常面临「caller 顺序约束」难题 — 例如 `set_pipeline_hooks` 需要 `update_manager_` 已初始化，但 caller 可能在 `update_manager_` 创建前就调用（lazy 创建场景）。传统设计返 ERROR 强制 caller 重排顺序，UX 差。

### 模式核心

```
内部状态：external_hooks_（缓存的 hooks 拷贝）+ external_hooks_set_（标志位）

C ABI 函数 set_xxx：
  1. 拷贝 hooks 到 external_hooks_
  2. external_hooks_set_ = true
  3. 如果 update_manager_ 已存在 → 立即 attach
  4. 如果 update_manager_ 不存在 → 返 INVALID_STATE（提示而非错误）

EnsureUpdateManager（创建 / 取得 update_manager_ 时调用）：
  - 如果 external_hooks_set_ → 应用 cached hooks 到新 update_manager_
```

### 关键设计点

1. **INVALID_STATE 是「提示」而非「失败」** — caller 可忽略也可重试，hooks 已 cached
2. **内部承担「记忆 + 自动激活」复杂度** — 不要求 caller 知道 init 顺序
3. **end-to-end 行为正确**（不需要文档解释顺序约束）— C ABI 设计的「容错优于强约束」原则

### 实证（TASK-20260502-02 B.0.1 + B.3.2 端到端）

- B.0.1 设计 Application::SetPipelineHooks lazy attach
- B.3.2 hello_devtool 端到端验证：
  - caller 顺序：`attach_devtool → load_html/css → set_pipeline_hooks(rc=-2 INVALID_STATE) → vx_view_run`
  - 实际行为：hooks cached 在 external_hooks_，vx_view_run 内 timer 触发 EnsureUpdateManager → cached hooks 应用 → on_frame_end fires → frames=1 ✅
  - 输出：`Pipeline hooks installed (rc=-2)` + `PERF SMOKE: frames=1 hud_visible=1` ✅

### 适用场景

- C ABI 函数操作的对象 lazy 创建（不在构造时初始化）
- 设置类 API（hooks / callbacks / config）— 可缓存
- caller 顺序难以约束（多种合法 caller 模式）

### 不适用场景

- 操作类 API（execute / send / commit）— 不能 cache 行为，必须强制顺序
- 状态查询类 API — 缓存值可能过期

### 子模式：warning 语义层扩展（TASK-20260503-01 C.4.1 实证）

**触发场景：** lazy-attach 设计中存在「子能力初始化失败但主路径仍可用」需求 — 例如 hot reload 是 DevTool 的可选子能力，hot reload attach 失败不应阻塞 DevTool 主功能（inspector / perf overlay）。

**模式扩展：**

```
原模式：失败返 INVALID_STATE（提示 + cache 重试）
扩展模式：失败返 WARNING_XXX（warning 语义 + 主路径继续）
```

**实证（C.4.1 vx_view_attach_devtool）：**
- `hot_reload_dir = nullptr / empty` → 跳过 hot reload attach，返 VX_OK
- `hot_reload_dir` 非空但 attach 失败（路径不存在 / inotify_init 失败 / realpath 失败）→ 返 `VX_WARNING_HOT_RELOAD_FAILED = 1`（非负值表示警告）
- DevTool 主功能（inspector + perf overlay）已 attach 成功，继续工作

**语义分层：**

| 返回值类型 | 语义 | caller 期望行为 |
|:--|---|---|
| VX_OK (0) | 完全成功 | 继续 |
| VX_WARNING_XXX (正值) | 主功能成功，子能力失败 | 可选择记录 warning + 继续 |
| VX_ERROR_XXX (负值) | 主功能失败 | 不应继续，须修正 caller 代码 |

**关键设计点：**
1. **正值返回 = warning 语义** — caller 可 `if (rc < 0) abort` 简洁判定
2. **warning 不阻塞 caller 后续 API 调用** — caller 后续仍可 `vx_view_get_perf_stats` 等
3. **子能力可独立 detach + 重 attach**（未来扩展点）

### 数据点

- TASK-20260502-02 B.0.1（设计）+ B.3.2（端到端验证）= dual-evidence
- TASK-20260503-01 C.4.1（warning 语义层扩展）= triple-evidence + sub-pattern 沉淀

---

## Level 3 vs Level 4 子代理决策法则（TASK-20260502-02 反思 §4.3 — 经验整合）

### 背景

子代理调度（subagent-development）有显著加速潜力但管理开销 + 失控风险也大。Phase A（Level 4 16 子任务）和 Phase B（Level 3 10 子任务）实测都未用子代理 → 需要总结判定法则避免下次摇摆。

### 法则（叠加判定）

| 任务复杂度 | 子任务数 | 5 大范式命中数 | 推荐 |
|:-:|:-:|:-:|:-:|
| Level 1 | ≤ 3 | — | 单 agent 直跑 |
| Level 2 | ≤ 6 | — | 单 agent 直跑 |
| **Level 3** | **≤ 10** | **≥ 5 项** | **单 agent 直跑（推荐）** |
| Level 3 | > 10 或 < 5 项命中 | — | 单 agent 仍可，子代理评估 |
| **Level 4** | ≥ 12 + 涉及子系统级新设计 | — | **子代理可候选评估** |
| Level 4 | 涉及多 Phase 跨长期会话 | — | **子代理可候选** |

### 实证

- TASK-20260502-01 Phase A（Level 4 / 16 子任务 / 5 大范式首次沉淀）— 单 agent 直跑（escalation 中途升级），实测 0.64× 桶下沿外
- TASK-20260502-02 Phase B（Level 3 / 10 子任务 / 5 大范式 100% 命中）— 单 agent 直跑，实测 0.40× 极窄档延续高效区
- TASK-20260503-01 Phase C（Level 3 / 11 子任务 / 5 大范式 100% 命中 + lazy-attach C ABI 复用 + dogfood 第三次复用）— 单 agent 直跑，实测 **0.31× 极窄档加速衰减区下沿**

### 适用启示

- 单 agent 直跑的优势：上下文连续 / 范式复用即时 / 反向探针调试快
- 子代理的成本：上下文重启（每次 ~30s）+ prompt 撰写（~5 min/agent）+ 两阶段审查（~10 min/agent）
- **临界点：N ≥ 12 子任务时子代理调度成本可能 < 单 agent 上下文管理成本**（Phase A 16 子任务实测显示单 agent 在 round 3-7 出现轻微 context 疲劳，但通过 round-pause + reload 缓解）

### 反模式

❌ **「子代理因为听起来高大上就用」** — Level 1-3 ≤ 10 子任务用子代理纯属过度工程
❌ **「子代理因为子任务多就用」** — 子任务数不是充分条件，要看子任务间依赖（强依赖 → 串行 → 单 agent）

### 数据点

- TASK-20260502-01 / TASK-20260502-02 双任务零子代理高效完成 — 验证 Level 3 / Level 4 < 12 子任务区间单 agent 优于子代理
- TASK-20260503-01 Phase C（11 子任务 / 5 大范式 100% 命中 + 复用 lazy-attach + dogfood 第三次）单 agent 进一步打破 0.40× 到 0.31×，三连递降验证「单 agent + 范式累积」复利效应

---

## baseline 阻塞 hotfix 分离协议（TASK-20260503-01 反思 §6.1 — 新模式 / 首次实证）

### 背景

build 阶段 §0.1 ctest baseline 二次验证可能遭遇「环境兼容性」阻塞 — 如工具链版本激进升级（binutils / gcc / cmake）、平台行为变化、依赖库 ABI 变化等。这些阻塞与业务任务（feature/...）正交，但会污染业务任务的 commit 历史和分支演进。

### 触发条件

- build §0.1 baseline 二次验证 FAILED（非业务范围 CI/test 回归）
- 失败根因明确指向「环境因子」（工具链版本 / 平台 / 依赖库 ABI），非业务代码缺陷
- 修复需要修改全局基础设施（根 CMakeLists / 工具链配置 / 依赖管理），不限于业务子系统

### 决策树

```
baseline FAILED?
├─ 业务范围回归 → 不分离，继续在 feature 分支修复（正常 build 流程）
└─ 环境因子阻塞 → 决策方案 B：hotfix 分离协议
    ├─ A. 不分离（继续在 feature 分支修复） → 污染业务 commit 历史 ❌ 反模式
    └─ B. hotfix 分离协议 ✅ 推荐
```

### 协议链路（标准化 5 步）

1. **创建 hotfix 分支**（命名格式：`hotfix/<env-factor>-<short-desc>`，如 `hotfix/binutils-2.46-link-group`）从当前 main 拉
2. **业务 feature 分支 stash WIP**（`git stash push -u -m "WIP: <feature> before hotfix"`）
3. **hotfix 分支单独修复 + 验证**（最小变更 + verify all green + 1 commit + commit body 详述根因 + 修复方案）
4. **fast-forward merge to main**（hotfix 修复是环境兼容范畴，不需 PR review；main 保持线性历史）+ 删除 hotfix 分支
5. **业务 feature 分支 rebase 上 main**（`git checkout feature/...` + `git rebase main` + `git stash pop`）+ 继续业务 build

### 关键设计点

1. **单一职责分支** — hotfix 分支只解决环境因子，零业务功能改动
2. **commit message 显式分类** — body 第一段标注「环境兼容范畴 / 非业务范围」+ 列出根因（工具链版本 / 行为变化）+ 列出 mitigation 方案
3. **fast-forward 保线性** — hotfix 是基础设施改动，main 不应有 merge commit 增加历史复杂度
4. **业务 rebase 而非 merge** — feature 分支保持基于最新 main 线性历史，避免 cross-graph 污染

### 实证（TASK-20260503-01 binutils 2.46+ 事件）

- **触发**：build §0.1 baseline 二次验证遭遇 GNU ld 2.46（2026 Binutils）单次扫描静态归档严格化 → vx_core ↔ vx_script ↔ vx_devtool 循环依赖失效 → 6 测试 link FAILED
- **诊断**：根因是 ld 行为变化（CMake 传统「重复列出 .a」hack 失效），非业务代码缺陷
- **协议执行**：
  1. 创建 `hotfix/binutils-2.46-link-group` 分支
  2. feature 分支 stash plan WIP
  3. hotfix 分支：根 CMakeLists.txt +10 行 `-Wl,--start-group <LINK_LIBRARIES> -Wl,--end-group`（仅 GNU/Clang + Linux）→ 271/271 link OK + 1195/1195 ctest PASS → 1 commit
  4. fast-forward to main `ddc1e3c` + 删除 hotfix 分支
  5. feature 分支 rebase 上 main + stash pop WIP → 进入 C.0.1 RED 阶段
- **反向验证**：C.4.1 后续引入第二循环依赖（vx_core ↔ vx_devtool）叠加，hotfix 群组式包围方案**无需任何额外 CMake 改动即解决** — 实证 hotfix 设计的「叠加场景覆盖性」

### 反模式

❌ **「在 feature 分支直接修复 + 大杂烩 commit」** — 业务功能 + 环境兼容混在一起，commit 语义不清，未来 cherry-pick / revert 困难
❌ **「hotfix 分支用 PR + review」** — 环境兼容范畴不需要 PR review；如有审查需求是「业务范围争议」反模式
❌ **「不 fast-forward 改 merge commit」** — 增加 main 历史复杂度，hotfix 应保持线性

### 数据点

- TASK-20260503-01（首次实证）— binutils 2.46+ ld 严格化 → hotfix 分离协议 5 步执行 → 零业务范围污染 + 实证 hotfix 叠加场景覆盖性

---

## R12 工具链版本激进升级风险（TASK-20260503-01 反思 §7 — 新风险登记）

### 背景

工具链（gcc / clang / ld / cmake / ninja / pkg-config / 依赖库）的小版本升级有时会引入静默行为变化，影响既有项目的 build / link / 运行时行为。激进升级（如 binutils 2.45 → 2.46）的行为差异可能在 baseline 二次验证时才暴露。

### 风险描述

- **触发场景**：用户开发环境工具链升级（如 OS package manager 自动 upgrade / 容器基础镜像更新 / CI runner image refresh）后，feature 分支首次 build / ctest 失败
- **典型症状**：
  - link errors（undefined symbols / circular dependency）— ld 行为变化
  - compile errors（新警告升级为错误 / 新语法严格化）— gcc / clang 行为变化
  - cmake configure errors（policy 变化 / 模块行为变化）— cmake 版本升级
  - 依赖库 ABI 变化（结构体大小 / 函数签名） — 系统库升级
- **影响范围**：所有依赖该工具链的 build / test / 部署流程

### Mitigation 策略

1. **plan §0.10 工具链与子系统关闭守门段加版本检查子段**：
   - `gcc --version` / `ld --version` / `cmake --version` / `ninja --version` 输出快照
   - 与上次任务的版本对比（diff `<current>` vs `<previous>`）
   - 已知行为差异 grep（如 binutils 2.46+ release notes 中 link group / archive scanning 变化）
2. **plan §0.1 ctest baseline 二次验证作为环境兼容验证入口**：baseline 二次验证不仅是「数字记录」也是「环境兼容验证」首发触发点
3. **遭遇阻塞时立即采用「baseline 阻塞 hotfix 分离协议」**（详见上方 systemPattern）

### 实证

- TASK-20260503-01（首次实证）— binutils 2.46+ ld 单次扫描严格化阻塞 → hotfix 分离 → 实证 mitigation 协议有效

### 数据点

- 风险登记编号：R12（系统级风险）
- 历史频率：1 次（截至 2026-05-03）— 但工具链激进升级是周期性事件（OS 大版本更新），R12 应作为持久性风险登记

---

## #ifdef + CMake conditional 范式 — unique_ptr 子模式（TASK-20260503-01 反思 §5.2 — 子模式扩展）

### 背景

「#ifdef + CMake conditional 范式」（Phase A 沉淀）通常应用于「函数实现 / class 全部成员」级 #ifdef 包围。但 `std::unique_ptr<T>` 字段在 OFF 路径需要特殊处理 — 即使字段无条件存在，`~Application()` 析构 unique_ptr 时编译器需要 T 的完整类型定义，但 OFF 路径不 include T 的 header → incomplete type 编译错误。

### 子模式

```cpp
// application.h — 字段也需 #ifdef 包围
class Application {
 public:
#ifdef VX_BUILD_DEVTOOL
  vx::devtool::hot_reload::HotReloadManager* hot_reload_manager() const {
    return hot_reload_manager_.get();
  }
#endif
  // ...
 private:
#ifdef VX_BUILD_DEVTOOL
  std::unique_ptr<vx::devtool::hot_reload::HotReloadManager> hot_reload_manager_;
#endif
  // ...
};

// application.cc — 析构 reset 也需 #ifdef
Application::~Application() {
#ifdef VX_BUILD_DEVTOOL
  hot_reload_manager_.reset();  // thread-safe cleanup before other members
#endif
  // ...
}
```

### 关键设计点

1. **字段 + getter + 析构 reset 三处必须同步 #ifdef** — 缺任何一处都会触发编译错误
2. **getter 在 OFF 路径返 nullptr 行为合理** — caller 须 null-check（A14 守门验证）
3. **Phase 0 §0.x checklist 应包含「unique_ptr<DevTool 类> 字段需 #ifdef 包围」检查项**

### 反模式

❌ **「字段无条件存在 + .cc 在 OFF 时不 include header」** — 编译错误：`error: invalid application of 'sizeof' to incomplete type ...`
❌ **「字段 #ifdef + getter 无 #ifdef」** — 编译错误：getter 引用未定义字段
❌ **「使用 `std::unique_ptr<T> = nullptr` 默认初始化但仍需 T 完整类型」** — unique_ptr 析构必须知道 T 大小，nullptr 初始化无法绕过

### 实证

- TASK-20260503-01 C.5.2 finalize 阶段双绿 verify 发现 + 修复
- 修复后 DEVTOOL=OFF 1082 PASS + A14 link-closure 零 DevTool 符号 ✅

### 数据点

- 子模式首次实证（截至 2026-05-03）— 未来 DevTool Phase D/E/F/G 引入新 unique_ptr<DevTool 类> 字段时必须遵循此子模式

---

## CMake 静态库循环依赖处理 — binutils 2.46+ 双循环依赖叠加场景覆盖性（TASK-20260503-01 反思 §5.3 — 实证记录）

### 背景

CMake 静态库循环依赖（如 vx_core ↔ vx_script）历史上通过「重复列出 .a」hack 解决（链接器多次扫描同一归档解析符号）。binutils 2.46+ 单次扫描严格化使该 hack 失效，需要 `-Wl,--start-group <LINK_LIBRARIES> -Wl,--end-group` 群组式包围。

### 关键问题

群组式包围方案对**单循环依赖**已验证有效（hotfix 触发场景），但对**多循环依赖叠加**（如 vx_core ↔ vx_script + vx_core ↔ vx_devtool 同时存在）是否仍正确？

### 实证（TASK-20260503-01 双循环依赖叠加）

- **场景**：C.4.1 引入 vx_core PRIVATE link vx_devtool（HotReloadManager 调 app_->LoadCSS）→ 与既有 vx_core ↔ vx_script 循环叠加形成双循环依赖
- **验证结果**：binutils 2.46+ hotfix `-Wl,--start-group <LINK_LIBRARIES> -Wl,--end-group` 包围方案**无需任何额外 CMake 改动即解决双循环依赖** — 271/271 link OK
- **机制解释**：群组式包围让 ld 在 group 内多次扫描归档直到所有符号解析完成（或检测到无新符号解析停止），与依赖图复杂度无关，只与「group 内所有归档总符号」有关

### 启示

1. **群组式包围方案是「依赖图复杂度无关」的通用解** — 适用于 N 个循环依赖叠加场景
2. **未来引入新循环依赖时无需额外 CMake 改动** — 只要新依赖在 LINK_LIBRARIES 列表内即被覆盖
3. **群组式包围的代价**：link 时间 O(N×M) 其中 N 是 group 内归档数 / M 是 group 内未解析符号数 — 实测影响可忽略（271 target link 仍在秒级）

### 数据点

- TASK-20260503-01（双循环依赖叠加首次实证）— hotfix 设计的群组式包围方案覆盖未来叠加场景

---

## 待定架构决策
- [x] CSS 支持的具体子集范围 → 已确定：~45 属性（布局/Flex/视觉/文本）+ 4 transition 属性
- [ ] 是否内置 SVG 支持
- [x] 动画系统的实现策略 → CSS Transitions 已实现（TASK-13），CSS Animations（@keyframes）待定
- [ ] 资源加载策略（打包 vs 文件系统 vs 混合）
- [ ] 多进程 vs 单进程架构
