# 任务跟踪

## 当前任务

| ID | 描述 | 状态 | 复杂度 | 创建日期 |
|----|------|------|--------|---------|
| TASK-20260405-05 | 消化技术债务（Arena 内存管理、ID 查找优化、测试修复、错误收集） | 回顾完成 | Level 2 | 2026-04-05 |

## 任务详情

### TASK-20260405-05
- **描述**：消化四个任务积累的关键技术债务，为 Layout Engine 做准备
- **复杂度**：Level 2（多文件修改，需求明确）
- **标签**：[重构] [性能优化] [质量改进]
- **代码规范**：Google C++ Style Guide
- **工作流路径**：`/van` → `/plan` → `/build` → `/reflect` → `/archive`

### 范围
1. **#12 Document ArenaAllocator**：Document 节点分配从 new/delete 迁移到 ArenaAllocator
2. **#11 TagIdFromName/PropertyIdFromName O(N) 优化**：线性扫描升级为 HashMap 或排序数组二分查找
3. **#9 PPM 测试路径修复**：/tmp 硬编码改为 tmpfile() 或 std::filesystem::temp_directory_path
4. **#13 Parser 错误收集**：HTML Parser 收集 kError token 信息（行号/列号/描述）

## 任务历史

| ID | 描述 | 最终状态 | 归档日期 |
|----|------|---------|---------|
| TASK-20260405-01 | 构建 Foundation 基础库（内存管理/容器/字符串/日志） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-02 | 构建 Graphics HAL 图形抽象层与 Platform HAL 平台抽象层 | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-03 | 构建 DOM 树 + HTML 解析器 | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-04 | 构建 CSS 引擎（Tokenizer/Parser/选择器/属性/层叠） | ✅ 已完成 | 2026-04-05 |
