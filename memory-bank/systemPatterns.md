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

## 待定架构决策
- [x] CSS 支持的具体子集范围 → 已确定：~45 属性（布局/Flex/视觉/文本）
- [ ] 是否内置 SVG 支持
- [ ] 动画系统的实现策略（CSS Transitions/Animations vs 脚本驱动）
- [ ] 资源加载策略（打包 vs 文件系统 vs 混合）
- [ ] 多进程 vs 单进程架构
