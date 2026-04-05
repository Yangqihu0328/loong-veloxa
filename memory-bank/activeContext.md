# 活跃上下文

## 当前阶段
设计中

## 当前任务
- **ID**：TASK-20260405-01
- **描述**：构建 Foundation 基础库（内存管理/容器/字符串/日志）
- **复杂度**：Level 4
- **代码规范**：Google C++ Style Guide

## 当前焦点
- 设计文档已批准：`docs/specs/2026-04-05-foundation-design.md`
- 实现计划已完成：`docs/plans/2026-04-05-foundation.md`
- 创意设计已完成：
  - `memory-bank/creative/creative-hashmap.md`（Swiss Table）
  - `memory-bank/creative/creative-string-sso.md`（fbstring 风格 SSO）
- 下一步：进入 `/build` 从 Phase 1（脚手架）开始

## 设计决策摘要
| 决策项 | 选择 |
|--------|------|
| 开发策略 | 从 Foundation 子系统切入 |
| 内存管理 | 自定义分配器体系（Arena/Pool/MallocWrapper） |
| 字符串 | UTF-8 + fbstring 风格 SSO（sizeof=24, SSO 22 字节） |
| 容器 | 4 核心容器 + std:: 补充 |
| HashMap | Swiss Table（Abseil 风格，SIMD 分组探测 + 标量回退） |
| 日志 | 编译时可裁剪分级日志 |
| 测试 | Google Test + Google Benchmark |
| 错误处理 | 不使用异常，Status/StatusOr + CHECK 断言 |

## 未合并分支
（无）

## 遗留改进建议
（无）
