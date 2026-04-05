# 归档：消化技术债务

**日期：** 2026-04-05
**任务 ID：** TASK-20260405-05
**复杂度级别：** Level 2
**状态：** ✅ 已完成

## 任务概述

消化四个任务积累的关键技术债务，为 Layout Engine 做准备。选定 4 项直接影响后续开发的技术债务：Arena 内存管理、ID 查找性能、测试路径隔离、解析器错误收集。

## 技术方案

### Phase 1：Document ArenaAllocator
Document 持有 ArenaAllocator，CreateElement/Text/Comment 使用 placement new 在 Arena 上分配节点。owned_nodes_ 保留用于析构时调用 `node->~Node()` 释放成员堆内存，之后 Arena 批量释放节点对象内存。构造函数使用默认参数保持向后兼容。

### Phase 2：ID 查找优化
TagIdFromName 和 PropertyIdFromName 从 O(N) 线性扫描升级为 HashMap O(1) 查找。使用 Meyer's singleton 懒初始化，调用 reserve() 预分配避免 rehash。TagIdFromName 在栈上 lowercase 输入后查找以保持大小写不敏感。

### Phase 3：PPM 测试路径
/tmp 硬编码改为 PID 隔离的动态路径 (`TempPpmPath` 辅助函数 + `getpid()`)，避免并发测试冲突。

### Phase 4：Parser 错误收集
Parser::Parse 新增 `Vector<ParseError>* errors = nullptr` 可选参数。ParseError 包含 line/column/description。默认 nullptr 保持向后兼容，所有既有调用方零修改。

## 实现摘要

### 文件变更

| 操作 | 文件路径 | 说明 |
|------|---------|------|
| 修改 | `veloxa/core/dom/document.h` | 新增 ArenaAllocator 成员，placement new 分配 |
| 修改 | `veloxa/core/dom/node.cc` | CreateElement/Text/Comment 改为 Arena 分配 |
| 修改 | `veloxa/core/dom/tag.cc` | TagIdFromName 改为 HashMap 查找 |
| 修改 | `veloxa/core/css/property.cc` | PropertyIdFromName 改为 HashMap 查找 |
| 修改 | `veloxa/core/html/token.h` | 新增 ParseError 结构体 |
| 修改 | `veloxa/core/html/parser.h` | Parse 新增 errors 参数 |
| 修改 | `veloxa/core/html/parser.cc` | 实现错误收集逻辑 |
| 修改 | `tests/core/dom/document_test.cc` | 新增 3 个 Arena 测试 |
| 修改 | `tests/core/html/parser_test.cc` | 新增 3 个错误收集测试 |
| 修改 | `tests/platform/memory_surface_test.cc` | PID 隔离临时路径 |

### 关键决策

1. **Arena 析构顺序**：先遍历 owned_nodes_ 调 `~Node()`，再让 Arena 析构批量释放 — 因为 Node 子类成员（String, SmallVector）仍需独立析构
2. **HashMap 懒初始化**：使用 static local + lambda 避免静态初始化顺序问题
3. **Parser::Parse 默认参数**：选择 `errors = nullptr` 而非返回值结构体，零侵入既有代码

### 安全决策

本任务不涉及安全变更。

## 测试覆盖

- **总测试数：** 534（新增 6，既有 528 全部回归通过）
- **新增测试：** ArenaAllocation, ArenaMultipleNodes, ArenaCleanupWithSanitizer, ErrorCollectionNoErrors, NullErrorsNoSegfault, ErrorCollectionDefaultParam
- **ASan/LSan：** 通过（验证 Arena 析构正确性）

## 经验教训

1. 对于需求明确的 Level 2 重构，精确的计划使实现时间从预估 35 分钟压缩到 ~10 分钟
2. Level 2 直接实现优于子代理，子代理开销仅在 Level 3+ 有收益
3. techContext.md 的编号技术债务清单是高效的任务来源，应持续维护

## 参考文档

- 实现计划：`docs/plans/2026-04-05-tech-debt.md`
- 回顾文档：`memory-bank/reflection/reflection-TASK-20260405-05.md`
