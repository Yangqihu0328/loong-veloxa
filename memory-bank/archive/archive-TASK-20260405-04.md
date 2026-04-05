# 归档：CSS 引擎

**日期：** 2026-04-05
**任务 ID：** TASK-20260405-04
**复杂度级别：** Level 4
**状态：** ✅ 已完成

## 任务概述

为 Veloxa UI 引擎构建 CSS 引擎，支持车载 HMI 常用 CSS 子集（~45 属性）。包含完整的解析管线（CSS text → Tokenizer → Parser → Stylesheet）和匹配管线（Stylesheet + DOM → SelectorMatcher → StyleResolver → ComputedStyle）。

## 技术方案

| 决策 | 方案 | 理由 |
|------|------|------|
| 属性子集 | 核心最小集 ~45 属性 | 布局20 + Flex9 + 视觉8 + 文本8，覆盖车载 HMI 需求 |
| 值类型系统 | A+C 混合 | 解析用 CssValue(8B tagged union)，计算用 ComputedStyle 直存 |
| CSS Tokenizer | 主分支 + 子扫描器 | 与 HTML Tokenizer 同构，注释内循环跳过，空白 token 保留 |
| 选择器匹配 | 从右到左匹配 | 快速排除不匹配元素，支持 6 种选择器 + 2 种组合子 |
| 层叠/继承 | CSS3 标准 | specificity(a,b,c) + !important + 10 个可继承属性 |
| 颜色解析 | 排序数组 + 二分查找 | 18 命名颜色，constexpr 零初始化，~288 字节 |
| DOM 集成 | Element 缓存 id/classes | 快速选择器匹配，HTML Parser 自动填充 |

## 实现摘要

### 文件变更

| 操作 | 文件路径 | 说明 |
|------|---------|------|
| 创建 | `veloxa/core/css/css_value.h` | CssValue(8B tagged union), LengthValue, Unit/ValueType 枚举 |
| 创建 | `veloxa/core/css/enums.h` | 13 个 CSS 属性枚举（Display, Position, FlexDirection 等） |
| 创建 | `veloxa/core/css/property.h` | PropertyId 枚举（~55 个值）+ PropertyInfo 元数据 |
| 创建 | `veloxa/core/css/property.cc` | kPropertyTable[] 静态表 + O(1)/O(N) 查找 |
| 创建 | `veloxa/core/css/tokenizer.h` | CssTokenizer（22 种 token 类型）+ CssToken 结构 |
| 创建 | `veloxa/core/css/tokenizer.cc` | Tokenizer 状态机（主分支 + 子扫描器，零拷贝） |
| 创建 | `veloxa/core/css/selector.h` | 选择器数据结构（SimpleSelector/CompoundSelector/Selector/StyleRule/Stylesheet） |
| 创建 | `veloxa/core/css/parser.h` | CssParser（Parse + ParseDeclarationList） |
| 创建 | `veloxa/core/css/parser.cc` | 选择器/声明/值/简写展开/颜色解析（1035 行） |
| 创建 | `veloxa/core/css/selector_matcher.h` | SelectorMatcher（Matches/MatchCompound/MatchSimple） |
| 创建 | `veloxa/core/css/selector_matcher.cc` | 右到左选择器匹配实现 |
| 创建 | `veloxa/core/css/computed_style.h` | ComputedStyle 结构体（~200 字节，直存所有计算属性） |
| 创建 | `veloxa/core/css/style_resolver.h` | StyleResolver（Resolve + 层叠/继承） |
| 创建 | `veloxa/core/css/style_resolver.cc` | 规则收集 + specificity 排序 + ApplyDeclaration + 继承 |
| 创建 | `veloxa/core/dom/element.cc` | Element id/class 方法实现 |
| 修改 | `veloxa/core/dom/element.h` | 新增 id_/classes_ 缓存 + 6 个访问方法 |
| 修改 | `veloxa/core/html/parser.cc` | ProcessStartTag 中提取 id/class 属性填充缓存 |
| 修改 | `veloxa/core/CMakeLists.txt` | 添加 css/*.cc + dom/element.cc |
| 创建 | `tests/core/css/property_test.cc` | 14 个属性系统测试 |
| 创建 | `tests/core/css/tokenizer_test.cc` | 24 个 Tokenizer 测试 |
| 创建 | `tests/core/css/parser_test.cc` | 35 个 Parser 测试 |
| 创建 | `tests/core/css/selector_matcher_test.cc` | 17 个匹配器测试 |
| 创建 | `tests/core/css/style_resolver_test.cc` | 15 个层叠/继承测试 |
| 创建 | `tests/core/css/integration_test.cc` | 12 个端到端测试 |
| 修改 | `tests/core/dom/element_test.cc` | 8 个 id/class 测试 |
| 修改 | `tests/CMakeLists.txt` | 6 个新测试目标 |

### 关键决策

1. **选择器存储从右到左**：解析时从左到右，存储时反转。匹配从 compounds[0]（目标元素）开始，根据 combinator 向上遍历 DOM 树。compounds[i].combinator 描述 i 与 i-1 之间的关系。

2. **CssValue 8B tagged union**：`{ValueType(1B) + Unit(1B) + union{f32/u32/u16}(4B)}`。解析阶段的通用值容器，由 ApplyDeclaration 转换为 ComputedStyle 的具体类型。

3. **RGBA32 像素格式统一**：CSS 颜色使用与 Graphics HAL 相同的 R[0:7]|G[8:15]|B[16:23]|A[24:31] 格式，跨模块验证通过。

4. **层叠应用顺序**：非 important 样式表 → 非 important 内联 → important 样式表 → important 内联。10 个可继承属性自动从父元素复制。

5. **简写展开在 Parser 层**：margin/padding（1-4 值扩展为 4 方向）、border（width+style+color 扩展为 12 个 longhand）在解析时完成，StyleResolver 只处理 longhand 属性。

### 安全决策

本任务不涉及安全变更。CSS 引擎处理开发者编写的 HMI 样式（非用户输入）。未来接入动态样式更新（OTA 主题包）时需添加输入验证。

## 测试覆盖

| 测试套件 | 用例数 | 覆盖范围 |
|---------|--------|---------|
| PropertyTest | 14 | PropertyId 查找、继承标记、元数据表完整性 |
| CssTokenizerTest | 24 | 22 种 token 类型、注释跳过、行号跟踪、混合输入 |
| CssParserTest | 35 | 选择器（6 种）、组合子、声明、颜色（4 种格式）、简写、!important、错误恢复 |
| SelectorMatcherTest | 17 | 6 种选择器匹配/不匹配、后代/子代组合子、:first-child/:last-child |
| StyleResolverTest | 15 | 层叠、specificity、!important、继承、inherit/initial 关键字、内联优先级 |
| CssIntegrationTest | 12 | 完整管线（HTML→DOM→CSS→ComputedStyle）、HMI 仪表盘、三层继承链 |
| ElementIdClassTest | 8 | id/class CRUD、HTML Parser 自动填充 |
| **合计** | **125 新增** | **528 总测试全通过** |

## 经验教训

1. **合并 Phase 给子代理**：紧耦合 Phase（共享类型定义）合并为一个子代理，减少 prompt 冗余和中间同步开销
2. **CMake 存根预创建**：第一个子代理负责为 CMakeLists.txt 中预列的后续文件创建空存根
3. **枚举 + 元数据表模式**：PropertyId 与 TagId 完全同构，确认为 Veloxa 标准 ID 系统模式
4. **RGBA32 跨模块一致性**：像素格式从 Graphics HAL 到 CSS Color 验证通过

## 技术债务（新增）

| # | 描述 | 优先级 |
|---|------|--------|
| 15 | parser.cc 过大（1035 行），考虑拆分 | P2 |
| 16 | ApplyDeclaration switch ~55 case，可用宏简化 | P2 |
| 17 | 选择器匹配 O(rules×elements) 需哈希索引优化 | P2 |
| 18 | :hover/:active/:focus 伪类返回 false（stub） | P2 |

## 架构影响

### 对后续模块的接口保证

- **Layout Engine**：ComputedStyle 已包含 display/position/width/height/margin/padding/flex-* 等所有布局属性，接口就绪
- **事件系统**：伪类 stub 需事件系统实现后回填状态查询
- **JS DOM API**：ParseDeclarationList 可服务 `element.style.cssText`，Resolve 可服务 `getComputedStyle()`

### CSS 属性扩展路径

新增属性四点修改：PropertyId 枚举值 → kPropertyTable 表项 → ApplyDeclaration case → ComputedStyle 字段。完全数据驱动。

## 参考文档

- 设计规格：`docs/specs/2026-04-05-css-engine-design.md`
- 实现计划：`docs/plans/2026-04-05-css-engine.md`
- 创意设计：`memory-bank/creative/creative-css-tokenizer.md`
- 创意设计：`memory-bank/creative/creative-color-parsing.md`
- 回顾文档：`memory-bank/reflection/reflection-TASK-20260405-04.md`
