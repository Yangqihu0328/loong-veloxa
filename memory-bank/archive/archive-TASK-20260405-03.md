# 归档：DOM 树 + HTML 解析器

**日期：** 2026-04-05
**任务 ID：** TASK-20260405-03
**复杂度级别：** Level 4
**状态：** ✅ 已完成

## 任务概述

为 Veloxa 引擎构建 DOM 树数据结构和 HTML 解析器。参考 Sciter `html-dom.h` / `html-parser.cpp` 的设计，但将 DOM 与 Layout 完全分离，裁剪桌面特有功能，面向车载 HMI 场景。

## 技术方案

### DOM 节点架构
精简扁平层次：`Node`(base) → `Element` / `Text` / `Comment` / `Document`。
- 子节点：双向链表（Node 的 next/prev sibling 指针），O(1) 追加/移除
- 属性：`SmallVector<Attribute, 4>`，InternedString 名 + String 值
- 所有权：Document 持有 `Vector<Node*> owned_nodes_`，析构时统一释放

### Tag 系统
`TagId` 枚举（~70 个已知标签）索引 `TagInfo` 静态表。每个 TagInfo 携带 TagType / ParseModel / ContentModel 三维元数据。未知/自定义标签退化为 `kUnknown`。

### HTML Tokenizer
方案 C（混合方案）：主循环 + 辅助方法。Token 使用 StringView 零拷贝指向原始输入。支持基本字符实体（named + decimal + hex），实体解码延迟到 Parser 层（has_entities 标志）。

### HTML Parser
Tokenizer 驱动的 DOM Builder。维护开放元素栈。20 条数据驱动隐式关闭规则表（`ImplicitCloseRule`），支持按 TagType 或 TagId 匹配。处理 void 元素、self-closing、rawtext（script/style）。

## 实现摘要

### 文件变更

| 操作 | 文件路径 | 说明 |
|------|---------|------|
| 修改 | `CMakeLists.txt` | 添加 `veloxa/core` 子目录 |
| 创建 | `veloxa/core/CMakeLists.txt` | vx_core 静态库 |
| 创建 | `veloxa/core/dom/tag.h/.cc` | TagId 枚举 + TagInfo 查找表 |
| 创建 | `veloxa/core/dom/node.h` | Node 基类 + NodeType 枚举 |
| 创建 | `veloxa/core/dom/element.h` | Element 类（子节点链表 + 属性） |
| 创建 | `veloxa/core/dom/text.h` | Text 类 |
| 创建 | `veloxa/core/dom/comment.h` | Comment 类 |
| 创建 | `veloxa/core/dom/document.h` | Document 类（工厂 + 所有权） |
| 创建 | `veloxa/core/dom/attribute.h` | Attribute 结构体 |
| 创建 | `veloxa/core/dom/node.cc` | Node/Element/Document 实现 |
| 创建 | `veloxa/core/dom/serializer.h/.cc` | DOM → HTML 序列化 |
| 创建 | `veloxa/core/html/token.h` | Token/TokenType |
| 创建 | `veloxa/core/html/tokenizer.h/.cc` | Tokenizer + DecodeEntities |
| 创建 | `veloxa/core/html/parser.h/.cc` | Parser + 隐式关闭规则 |
| 修改 | `tests/CMakeLists.txt` | 添加 7 个测试目标 |
| 创建 | `tests/core/dom/tag_test.cc` | Tag 系统测试（12 个） |
| 创建 | `tests/core/dom/node_test.cc` | 节点测试（10 个） |
| 创建 | `tests/core/dom/element_test.cc` | 元素测试（14 个） |
| 创建 | `tests/core/dom/document_test.cc` | 文档测试（7 个） |
| 创建 | `tests/core/html/tokenizer_test.cc` | Tokenizer 测试（25 个） |
| 创建 | `tests/core/html/parser_test.cc` | Parser 测试（20 个） |
| 创建 | `tests/core/html/integration_test.cc` | 集成测试（12 个） |

**总计：** 32 个文件变更，~3000 行新增（1340 行生产代码 + ~1200 行测试 + 文档）

### 关键决策

1. **DOM 与 Layout 分离**：不同于 Sciter 的混合层次，Veloxa DOM 只管树结构和属性，Layout 由独立模块处理。
2. **枚举 + 静态表 vs HashMap**：TagId 使用枚举索引静态数组（O(1) 查找），TagIdFromName 使用线性扫描（O(N)，已知债务）。
3. **零拷贝 Tokenizer**：Token 的 name/value 是 StringView 指向原始输入，实体解码延迟到构建 DOM 节点时。
4. **数据驱动隐式关闭**：20 条规则的 `ImplicitCloseRule` 表，支持按类型或具体标签匹配，规则与逻辑分离。
5. **子代理精确签名 prompt**：向子代理提供完整 API 签名，本任务 3 个子代理零返工。

### 安全决策

本任务不涉及安全变更。HTML 解析器处理开发者编写的 HMI HTML（非用户输入）。

## 测试覆盖

| 测试套件 | 测试数 | 覆盖内容 |
|---------|--------|---------|
| TagTest | 12 | 查找、大小写、void/rawtext/block 判断、枚举完整性 |
| NodeTest | 10 | 类型、parent/sibling 链接、is_element/is_text |
| ElementTest | 14 | 子节点增删、属性 CRUD、tag_id |
| DocumentTest | 7 | 工厂方法、所有权释放 |
| TokenizerTest | 25 | 各 token 类型、实体解码、rawtext、行号 |
| ParserTest | 20 | 嵌套、隐式关闭、void、属性、错误容忍 |
| IntegrationTest | 12 | HTML 往返、HMI 页面、实体编码 |
| **总计** | **100** | **403/403 全量通过** |

## 经验教训

1. **精确 API 签名是子代理零返工的关键** — 已验证 TASK-02 P1 改进有效
2. **Creative 阶段投入产出比高** — 设计文档直接成为实现规范
3. **数据驱动表优于硬编码** — HandleImplicitClose 仅 ~30 行

## 已知技术债务

1. TagIdFromName O(N) 线性扫描 → P2 升级为 HashMap
2. Document 使用 new/delete → P1 集成 ArenaAllocator
3. Parser 静默忽略 kError → P2 支持错误收集
4. Serializer 不做空白规范化

## 参考文档

- 设计规格：`docs/specs/2026-04-05-dom-html-parser-design.md`
- 实现计划：`docs/plans/2026-04-05-dom-html-parser.md`
- 创意设计：`memory-bank/creative/creative-html-tokenizer.md`
- 创意设计：`memory-bank/creative/creative-implicit-close-rules.md`
- 回顾文档：`memory-bank/reflection/reflection-TASK-20260405-03.md`
