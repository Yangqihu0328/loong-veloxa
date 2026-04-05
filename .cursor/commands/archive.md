---
description: "创建综合归档文档并更新 Memory Bank，为下一个任务做准备。遵循 .cursor/rules/workflow/memory-bank-paths.mdc 和 .cursor/rules/skills/git-workflow.mdc"
---

# /archive — 归档文档

你现在以 ARCHIVE（归档）模式运行。你的职责是创建综合归档文档，更新所有 Memory Bank 文件，为下一个任务做准备。

## 执行步骤

### 0. 前置条件检查

读取 `memory-bank/activeContext.md`，验证：
- 当前阶段必须是 `回顾中`（由 `/reflect` 设置）
- 如果阶段是 `构建中` → 提示用户先使用 `/reflect` 进行回顾
- 如果阶段是 `空闲` → 提示用户没有需要归档的任务
- 验证 `memory-bank/reflection/reflection-[task_id].md` 存在（Level 2+ 必须有回顾文档；Level 1 回顾可能仅记录在 `progress.md` 中，允许跳过独立回顾文件）

### 1. 读取上下文

- 读取 `memory-bank/tasks.md` 获取任务详情
- 读取 `memory-bank/reflection/reflection-[task_id].md` 获取回顾（Level 1 如果不存在则跳过）
- 读取实现计划和创意设计文档（如果有）
- 读取 `memory-bank/progress.md` 获取实现记录

### 2. 创建归档文档

创建 `memory-bank/archive/archive-[task_id].md`：

```markdown
# 归档：[任务名称]

**日期：** [YYYY-MM-DD]
**任务 ID：** [task_id]
**复杂度级别：** Level [1-4]
**状态：** ✅ 已完成

## 任务概述
[任务描述和目标]

## 技术方案
[选定的方案及理由]

## 实现摘要
[关键实现细节]

### 文件变更
| 操作 | 文件路径 | 说明 |
|------|---------|------|
| 创建 | path/to/new.ts | [说明] |
| 修改 | path/to/existing.ts | [说明] |

### 关键决策
1. [决策及理由]

### 安全决策
[安全相关的设计和实现决策，如认证方案、数据保护策略等；如无安全相关内容则注明"本任务不涉及安全变更"]

## 测试覆盖
[测试摘要]

## 经验教训
[从回顾中提取的关键教训]

## 参考文档
- 设计规格：`docs/specs/[path]`
- 实现计划：`docs/plans/[path]`
- 创意设计：`memory-bank/creative/[path]`（如果有）
- 回顾文档：`memory-bank/reflection/[path]`
```

### 3. 归档创意文档（Level 3-4）

如果有创意设计文档，将其引用添加到归档中。

### 4. 更新长期知识库

将任务中学到的东西持久化（如果 `/reflect` 未充分更新）：
- **techContext.md** — 追加新引入的依赖、工具、技术决策
- **systemPatterns.md** — 追加新发现的架构模式、编码规范
- **productContext.md** — 追加产品功能变化（如适用）

### 4.5 收尾检查与提交

归档前验证以下事项：

- **改进建议迁移确认：** 检查 reflection 文档中的 P0/P1 建议：
  - P0 建议是否已在本任务中落实？如未落实 → 提示用户处理
  - P1 建议是否已迁移到 `activeContext.md` 待处理事项？如未迁移 → 执行迁移

- **提交归档产出物：** 运行 `git status`，将当前任务相关的所有变更提交到当前分支：
  - `memory-bank/archive/archive-[task_id].md`
  - 所有已更新的 Memory Bank 文件和知识库文件
  - 提交消息格式：`docs(archive): add archive for [task_id]`
  - 如果有不属于当前任务的变更 → 警告用户并排除出本次提交
  - 如果 git 不可用，跳过提交步骤
  - 确保提交后 working tree 干净

### 5. 完成开发分支（Level 2+）

遵循 `.cursor/rules/skills/git-workflow.mdc` 的"完成开发分支"流程：
- 向用户展示选项：合并到主分支 / 创建 PR / 保留分支 / 放弃
- 执行用户选择（此时所有变更已提交，合并/PR 操作在干净状态下进行）
- 如果 git 不可用或 Level 1 在主分支直接修复，跳过此步骤

### 6. 更新 Memory Bank（最终状态重置）

- **activeContext.md** — 先将阶段更新为 `归档中`
- **tasks.md** — 标记任务为 `已完成`，更新任务历史表
- **progress.md** — 将当前任务进度归档到归档文档后，清理 `progress.md` 中该任务的临时记录，重置为等待新任务
- **activeContext.md** — 确认 reflection 中 P1 改进建议已在「待处理事项」中；归档操作全部完成后，重置为 `空闲` 状态，准备接受新任务

### 6.5 最终提交

在主分支（或合并后的当前分支）上提交 Memory Bank 状态重置：
- 提交消息格式：`chore(workflow): complete [task_id] and reset to idle`
- 如果步骤 5 选择了"创建 PR"或"保留分支"，先切回主分支后再执行此提交
- 如果 git 不可用，跳过此步骤
- 确保提交后 working tree 干净

### 7. 输出

```markdown
## 📦 归档完成

**归档文档：** `memory-bank/archive/archive-[task_id].md`
**任务状态：** ✅ 已完成

**本次任务产出：**
- [N] 个文件创建/修改
- [M] 个测试用例
- [L] 个提交

**Memory Bank 已更新，准备接受新任务。**

**下一步：** 使用 `/van` 开始新任务，或结束本次工作
```

## 复杂度级别适配

| 级别 | 归档深度 |
|------|---------|
| Level 1 | 简要归档：任务概述 + 文件变更 |
| Level 2 | 基础归档：+ 技术方案 + 测试覆盖 |
| Level 3 | 详细归档：+ 关键决策 + 经验教训 |
| Level 4 | 全面归档：+ 架构影响 + 长期维护建议 |

## 归档原则

- **完整性** — 未来的会话应能仅通过归档理解发生了什么
- **简洁性** — 不是复制所有文档，而是提供结构化摘要
- **可引用性** — 包含所有相关文档的路径引用
- **可操作性** — 经验教训应具体且可操作
