# 归档：Layout Engine 布局引擎

**日期：** 2026-04-05
**任务 ID：** TASK-20260405-06
**复杂度级别：** Level 4
**状态：** ✅ 已完成

## 任务概述

构建 Veloxa UI 引擎的核心布局子系统，实现从 DOM+CSS 到具体像素坐标的转换。支持 Block（垂直流）、Inline（文本行）、Flex（弹性盒子）三种布局模式，以及 relative/absolute 定位系统。布局树独立于 DOM 树，通过 ArenaAllocator 高效分配。

## 技术方案

### 架构决策

1. **扁平 struct LayoutBox**：使用 `LayoutType` 枚举（kBlock/kInline/kFlex/kText）区分类型，而非继承层次。布局状态（x/y/content_width/content_height/padding/border/margin）直存 struct 字段
2. **独立布局树**：LayoutBox 通过 parent/first_child/last_child/next_sibling/prev_sibling 指针形成独立树，持有 Element*/Text* 反向引用但不耦合 DOM 结构
3. **TextShaper 纯虚接口**：文本测量抽象为 `TextShaper::Measure(text, font_size, font_weight) → TextMetrics`，与具体字体引擎解耦。SimpleTextShaper 提供固定比例测量供测试使用
4. **ComputedStyle 即时计算**：BuildTree 阶段通过 `StyleResolver::Resolve` 计算样式，结果 arena 分配存入 LayoutBox::style

### 与 Sciter 的差异

Sciter 将布局嵌入 DOM（element→block→block_vertical/horizontal），Veloxa 彻底分离 DOM 和 Layout 树，使两棵树可独立变更和优化。

## 实现摘要

### 文件变更

| 操作 | 文件路径 | 说明 |
|------|---------|------|
| 创建 | `veloxa/core/layout/layout_box.h` | LayoutType 枚举、LayoutBox 扁平 struct（含 AppendChild、box 尺寸计算） |
| 创建 | `veloxa/core/layout/text_shaper.h` | TextMetrics、TextShaper 纯虚接口、SimpleTextShaper |
| 创建 | `veloxa/core/layout/text_shaper.cc` | SimpleTextShaper 实现（0.6×font_size 每字符） |
| 创建 | `veloxa/core/layout/layout_utils.h` | LayoutContext、ResolveLength、ResolveBoxModel 声明 |
| 创建 | `veloxa/core/layout/layout_utils.cc` | 长度解析（px/%/em/rem/vw/vh/auto）+ 盒模型计算 |
| 创建 | `veloxa/core/layout/layout_engine.h` | LayoutEngine 类（Layout/BuildTree/LayoutBlock/LayoutInline/ApplyPositioning） |
| 创建 | `veloxa/core/layout/layout_engine.cc` | 布局树构建 + Block/Inline 布局 + 定位系统（333 行） |
| 创建 | `veloxa/core/layout/flex_layout.h` | LayoutFlex 自由函数声明 |
| 创建 | `veloxa/core/layout/flex_layout.cc` | CSS Flexbox Level 1 §9 完整实现（424 行） |
| 创建 | `tests/core/layout/layout_box_test.cc` | 9 个测试：默认值、AppendChild、box 尺寸计算 |
| 创建 | `tests/core/layout/layout_utils_test.cc` | 16 个测试：ResolveLength 各单位 + ResolveBoxModel |
| 创建 | `tests/core/layout/tree_builder_test.cc` | 8 个测试：空文档、display:none、嵌套、混合内容 |
| 创建 | `tests/core/layout/block_layout_test.cc` | 12 个测试：固定/auto 宽高、border-box、auto margin、min/max |
| 创建 | `tests/core/layout/flex_layout_test.cc` | 15 个测试：grow/shrink/justify/align/gap/wrap |
| 创建 | `tests/core/layout/inline_position_test.cc` | 8 个测试：文本测量、换行、relative/absolute 定位 |
| 创建 | `tests/core/layout/integration_test.cc` | 8 个测试：HTML→DOM→CSS→Layout 全管线 |
| 修改 | `veloxa/core/CMakeLists.txt` | 新增 4 个 layout .cc 源文件 |
| 修改 | `tests/CMakeLists.txt` | 新增 7 个 layout 测试目标 |

### 关键决策

1. **LayoutBox 内联方法**：AppendChild/child_count/box 尺寸计算全部内联于 header，无需单独 .cc 文件（方法体均 < 10 行）
2. **static ArenaAllocator**：`Layout()` 入口使用 `static ArenaAllocator`，每次调用 Reset 后重用。简化方案，但线程不安全（已记入技术债）
3. **空白文本节点过滤**：BuildTree 遍历字符判断是否纯空白，纯空白节点不创建 LayoutBox（集成测试暴露的必要修复）
4. **kText 节点测量**：LayoutChild 对 kText 类型调用 TextShaper::Measure，使 Block 容器的 auto height 能反映文本高度
5. **LayoutFlex 为自由函数**：独立于 LayoutEngine 类，便于独立测试和子代理并行开发

### 安全决策

本任务不涉及安全变更。

## 测试覆盖

| 测试套件 | 测试数 | 覆盖内容 |
|---------|--------|---------|
| LayoutBox | 9 | 默认值、树操作、padding/border/margin box 尺寸 |
| LayoutUtils | 16 | px/%/em/rem/vw/vh/auto/none 长度解析、盒模型计算 |
| TreeBuilder | 8 | 空文档、单元素、Text、display:none、嵌套、inline/flex 类型 |
| BlockLayout | 12 | 固定/auto 宽高、垂直堆叠、border-box、auto margin、min/max |
| FlexLayout | 15 | row/column/grow/shrink/basis/justify(5种)/align/align-self/gap |
| InlinePosition | 8 | 文本测量、换行、relative/absolute 定位 |
| Integration | 8 | HTML→DOM→CSS→Layout 全管线（header+content+footer/flex/nested/display:none） |
| **合计** | **76** | 全部 610/610 通过（含之前的 534 个） |

## 经验教训

1. **集成测试不可替代**：单元测试中手动构建 DOM 不会产生空白 Text 节点，真实 HTML 解析一定会。空白文本节点和 Block 内文本测量两个 bug 只有集成测试才能捕获
2. **共享文件约束子代理分组**：Phase 2+3+5 共享 layout_engine.cc 必须合并为一个子代理，原计划的 3 组并行实际只能 2 组
3. **Phase 1 手工 + 精确签名 = 子代理零返工**：自己实现基础 API 后为子代理提供完整签名，2 个子代理均零编译错误零返工
4. **非默认路径需提前枚举**：空白文本是典型的边界输入，计划中应有显式的"边界情况清单"

## 技术债务（新增）

| # | 项目 | 影响 |
|---|------|------|
| 19 | `LayoutEngine::Layout` 的 static ArenaAllocator 线程不安全 | 多线程场景崩溃 |
| 20 | Block 布局缺少 margin collapsing | 渲染与 CSS 规范不符 |
| 21 | LayoutInline 缺少真正的 line box 模型 | 多字体混排行高不正确 |
| 22 | Arena 分配的 ComputedStyle 含 InternedString，析构函数不被调用 | 潜在引用计数泄露 |

## 参考文档

- 设计规格：`docs/specs/2026-04-05-layout-engine-design.md`
- 实现计划：`docs/plans/2026-04-05-layout-engine.md`
- 回顾文档：`memory-bank/reflection/reflection-TASK-20260405-06.md`
