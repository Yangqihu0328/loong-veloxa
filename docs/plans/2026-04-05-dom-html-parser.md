# 实现计划：DOM 树 + HTML 解析器

**日期：** 2026-04-05
**任务 ID：** TASK-20260405-03
**复杂度：** Level 4

## 环境约束

- 编译器：GCC 11.4.0, C++17, `-Wpedantic -Werror`
- 系统 GTest v1.11.0
- Foundation 设施：InternedString, StringView, String, Vector, SmallVector, HashMap, ArenaAllocator, Status/StatusOr
- SmallVector 模板参数：`SmallVector<T, N>`

## Phase 结构

### Phase 1：CMake 脚手架 + 目录结构（~15min）

**目标：** 创建 `veloxa/core/dom/` 和 `veloxa/core/html/` 目录，配置 CMake。

| 操作 | 文件 | 说明 |
|------|------|------|
| 创建 | `veloxa/core/CMakeLists.txt` | vx_core 库 [共享文件] |
| 创建 | `veloxa/core/dom/` | DOM 节点目录 |
| 创建 | `veloxa/core/html/` | HTML 解析器目录 |
| 修改 | `CMakeLists.txt` | 添加 `veloxa/core` 子目录 [共享文件] |
| 修改 | `tests/CMakeLists.txt` | 添加测试目标 [共享文件] |

### Phase 2：Tag 系统（~30min）

**目标：** TagId 枚举、TagInfo 查找表、TagType/ParseModel/ContentModel 元数据。

| 操作 | 文件 | 说明 |
|------|------|------|
| 创建 | `veloxa/core/dom/tag.h` | TagId 枚举 + TagType/ParseModel/ContentModel 枚举 + TagInfo 结构体 |
| 创建 | `veloxa/core/dom/tag.cc` | TagInfo 查找表实现 + TagId ↔ 字符串转换 |
| 创建 | `tests/core/dom/tag_test.cc` | TagId 查找、已知/未知标签、枚举完整性 |

**TDD：** 测试 → TagId/TagInfo 查找 → 已知标签 → 未知标签

### Phase 3：DOM 节点（~45min）

**目标：** Node/Element/Text/Comment/Document 类层次 + 树操作。

| 操作 | 文件 | 说明 |
|------|------|------|
| 创建 | `veloxa/core/dom/node.h` | Node 基类 + NodeType 枚举 |
| 创建 | `veloxa/core/dom/element.h` | Element 类 |
| 创建 | `veloxa/core/dom/text.h` | Text 类 |
| 创建 | `veloxa/core/dom/comment.h` | Comment 类 |
| 创建 | `veloxa/core/dom/document.h` | Document 类 |
| 创建 | `veloxa/core/dom/node.cc` | Node/Element/Text/Comment/Document 实现 |
| 创建 | `veloxa/core/dom/attribute.h` | Attribute 结构体 |
| 创建 | `tests/core/dom/node_test.cc` | 节点创建/类型/遍历 |
| 创建 | `tests/core/dom/element_test.cc` | 子节点操作/属性操作/tag |
| 创建 | `tests/core/dom/document_test.cc` | Document 创建/Arena 生命周期 |

**TDD：** 测试 Node 创建 → Element 子节点 → 属性 → Document → 遍历 → 序列化

### Phase 4：HTML Tokenizer（~60min）

**目标：** 状态机 tokenizer，输入 HTML 字符串，输出 Token 流。

**设计参考：** `memory-bank/creative/creative-html-tokenizer.md`（/creative 产出）

| 操作 | 文件 | 说明 |
|------|------|------|
| 创建 | `veloxa/core/html/token.h` | Token 结构体 + TokenType 枚举 |
| 创建 | `veloxa/core/html/tokenizer.h` | Tokenizer 类声明 |
| 创建 | `veloxa/core/html/tokenizer.cc` | Tokenizer 状态机实现 |
| 创建 | `tests/core/html/tokenizer_test.cc` | 各 token 类型、边界、行号 |

**TDD：** 测试简单标签 → 属性 → 文本 → 注释 → void → 自闭合 → 实体 → 错误

### Phase 5：HTML Parser + DOM Builder（~45min）

**目标：** 消费 Token 流，构建 DOM 树。

**设计参考：** `memory-bank/creative/creative-implicit-close-rules.md`（/creative 产出）

| 操作 | 文件 | 说明 |
|------|------|------|
| 创建 | `veloxa/core/html/parser.h` | Parser 类声明 |
| 创建 | `veloxa/core/html/parser.cc` | Parser 实现 + 隐式关闭规则 |
| 创建 | `tests/core/html/parser_test.cc` | 文档解析/隐式关闭/void/嵌套 |

**TDD：** 测试空文档 → 简单元素 → 嵌套 → 隐式关闭 → void → 文本 → 属性 → 完整文档

### Phase 6：集成测试 + DOM 序列化（~30min）

**目标：** HTML → DOM → 序列化往返测试，验证管线完整性。

| 操作 | 文件 | 说明 |
|------|------|------|
| 创建 | `veloxa/core/dom/serializer.h` | DOM → HTML 序列化 |
| 创建 | `veloxa/core/dom/serializer.cc` | 序列化实现 |
| 创建 | `tests/core/html/integration_test.cc` | 端到端测试 |

**TDD：** 往返测试 → 车载 HMI 典型页面 → Arena 生命周期验证

### Phase 7：CMake 收尾 + 最终验证（~10min）

**目标：** 确保所有测试通过，clean build 成功。

| 操作 | 文件 | 说明 |
|------|------|------|
| 修改 | `veloxa/core/CMakeLists.txt` | 确认所有源文件 |
| 运行 | 全量测试 | ctest --output-on-failure |

## 预估

- **总时间：** ~4h
- **测试数量：** ~85 个
- **新增文件：** ~20 个（含测试）
- **子代理使用：** Phase 2+3 可并行，Phase 4+5 串行（5 依赖 4）

## 子代理约束

- 所有子代理 prompt 必须包含 RGBA32 像素格式等跨模块格式规范（P1 改进）
- 本任务主要跨模块格式：UTF-8 字符串编码，InternedString 指针比较语义
