# 活跃上下文

## 当前阶段
构建中

## 当前任务
- **ID**：TASK-20260405-01
- **描述**：构建 Foundation 基础库（内存管理/容器/字符串/日志）
- **复杂度**：Level 4
- **代码规范**：Google C++ Style Guide
- **分支**：feature/TASK-20260405-01-foundation

## 当前焦点
- 所有 7 个 Phase 已完成
- 228 个测试全部通过
- 下一步：进入 `/reflect` 进行回顾

## 设计决策摘要
| 决策项 | 选择 |
|--------|------|
| 开发策略 | 从 Foundation 子系统切入 |
| 内存管理 | 自定义分配器体系（Arena/Pool/MallocWrapper） |
| 字符串 | UTF-8 + fbstring 风格 SSO（sizeof=24, SSO 22 字节） |
| 容器 | 4 核心容器 + std:: 补充 |
| HashMap | Swiss Table（线性探测 + ctrl 字节 H2 过滤） |
| 日志 | 编译时可裁剪分级日志 |
| 测试 | Google Test（系统安装 v1.11.0） |
| 错误处理 | 不使用异常，Status/StatusOr + CHECK 断言 |

## 未合并分支
- `feature/TASK-20260405-01-foundation`（当前工作分支）

## 遗留改进建议
（无）
