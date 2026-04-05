# 任务跟踪

## 当前任务

| ID | 描述 | 状态 | 复杂度 | 创建日期 |
|----|------|------|--------|---------|
| TASK-20260405-04 | 构建 CSS 引擎（Tokenizer/Parser/选择器/属性/层叠） | 回顾完成 | Level 4 | 2026-04-05 |

## 任务详情

### TASK-20260405-04
- **描述**：为 Veloxa 引擎构建 CSS 引擎，支持车载 HMI 常用 CSS 子集。包括 CSS tokenizer、parser、选择器匹配、属性系统、层叠与继承。
- **复杂度**：Level 4（多子系统，CSS 子集范围决策，选择器匹配算法）
- **标签**：[架构设计] [CSS] [样式系统]
- **代码规范**：Google C++ Style Guide
- **工作流路径**：`/van` → `/plan` → `/creative` → `/build` → `/reflect` → `/archive`
- **参考源码**：`/mnt/d/workspace/stable-5.0.2.16/engine/html/html-style*.h/.cpp`（Sciter CSS 引擎）

### Sciter 参考分析摘要
- `css_istream`：CSS tokenizer，区分选择器 token（`s_token`）、属性体 token（`b_token`）、属性值 token（`a_token`）
- `style_parser`：解析 CSS 文本为规则集，含 `@media` / `@import` 支持
- `style` / `char_style`：计算样式结构体，分可继承属性（`char_style`）和元素属性
- `STYLE_ATTS` 枚举 + `html-style-atts.inl`：CSS 属性 ID（类似 TagId 模式）
- `style_prop_list`：声明列表（属性名 + 值 + !important）
- `style_bag`：文档级样式规则集合
- 选择器：`style_def` 含选择器链 + 声明列表，匹配通过遍历 DOM 元素

### 环境约束
- 编译器：GCC 11.4.0, C++17, -Wpedantic -Werror
- Foundation 设施可用：InternedString, StringView, String, Vector, SmallVector, HashMap
- DOM 系统可用：Node/Element/Text/Document, TagId, Parser
- 目标目录 `veloxa/core/css/` 待创建

### 相关待处理事项
- **P1**：Document 集成 ArenaAllocator → 可在本任务中顺带处理或独立 TASK

## 任务历史

| ID | 描述 | 最终状态 | 归档日期 |
|----|------|---------|---------|
| TASK-20260405-01 | 构建 Foundation 基础库（内存管理/容器/字符串/日志） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-02 | 构建 Graphics HAL 图形抽象层与 Platform HAL 平台抽象层 | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-03 | 构建 DOM 树 + HTML 解析器 | ✅ 已完成 | 2026-04-05 |
