# 任务跟踪

## 当前任务

| ID | 描述 | 状态 | 复杂度 | 创建日期 |
|----|------|------|--------|---------|
| TASK-20260405-03 | 构建 DOM 树 + HTML 解析器 | 回顾完成 | Level 4 | 2026-04-05 |

## 任务详情

### TASK-20260405-03
- **描述**：为 Veloxa 引擎构建 DOM 树数据结构（Node/Element/Text/Document）和 HTML 解析器（Tokenizer + Parser + DOM Builder），参考 Sciter html-dom.h 设计
- **复杂度**：Level 4（多子系统，新模块目录 veloxa/core/，类层次设计）
- **标签**：[架构设计] [DOM] [HTML解析]
- **代码规范**：Google C++ Style Guide
- **工作流路径**：`/van` → `/plan` → `/creative` → `/build` → `/reflect` → `/archive`
- **参考源码**：`/mnt/d/workspace/stable-5.0.2.16/engine/html/`（Sciter HTML 引擎）

### Sciter 参考分析摘要
- `html::node` 基类：引用计数、NODE_TYPE 枚举、parent/owner 双指针、UID、遍历接口
- `html::element` 继承 node：子节点数组、属性表（symbol → value）、style 关联、tag symbol
- `html::text` 继承 node：文本内容存储
- `html::document` 继承 element（block_vertical）：文档根节点、URL、样式表集合
- `html::tag` 命名空间：symbol_t 符号表（InternedString 模式）、TAG_TYPE/PMODEL/CMODEL 定义
- `html::attr` 命名空间：属性名符号化
- `markup::scanner`：通用 HTML/XML tokenizer，产出 TT_TAG_START/TT_TAG_END/TT_ATTR/TT_TEXT 等 token
- `dom_builder`：接收 scanner token，构建 DOM 树，处理隐式标签关闭和包含规则

### 环境约束
- 编译器：GCC 11.4.0, C++17, -Wpedantic -Werror
- Foundation 设施可用：InternedString（符号表）、StringView（零拷贝）、String（UTF-8）、Vector、HashMap、Status/StatusOr
- 目标目录 `veloxa/core/` 尚未创建

### 相关待处理事项
- **P1**：子代理 prompt 模板增加「跨模块数据格式」段 → 本任务子代理需遵守
- **P1**：集成测试优先验证数据格式一致性 → DOM+Parser 集成测试

## 任务历史

| ID | 描述 | 最终状态 | 归档日期 |
|----|------|---------|---------|
| TASK-20260405-01 | 构建 Foundation 基础库（内存管理/容器/字符串/日志） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-02 | 构建 Graphics HAL 图形抽象层与 Platform HAL 平台抽象层 | ✅ 已完成 | 2026-04-05 |
