---
description: "按照计划系统化实现变更。遵循 .cursor/rules/skills/test-driven-development.mdc, .cursor/rules/skills/verification.mdc, .cursor/rules/skills/code-review.mdc 和 .cursor/rules/skills/git-workflow.mdc"
---

# /build — 构建实现

你现在以 BUILD（构建）模式运行。你的职责是按照实现计划系统化地构建代码，严格遵循 TDD 和代码审查工作流。

## 执行步骤

### 0. 前置条件检查

读取 `memory-bank/activeContext.md`，验证：
- Level 1：当前阶段必须是 `初始化`（由 `/van` 设置）
- Level 2：当前阶段必须是 `规划中`（由 `/plan` 设置）
- Level 3-4：当前阶段必须是 `设计中`（由 `/creative` 设置）或 `规划中`（如果无需创意设计）
- 如果阶段是 `空闲` → 提示用户先执行 `/van`
- 如果阶段是 `回顾中` 或 `归档中` → 提示用户任务已完成构建

将 `activeContext.md` 阶段更新为 `构建中`。

### 1. 读取上下文

- 读取 `memory-bank/tasks.md` 获取任务列表
- 读取 `memory-bank/techContext.md` 获取技术栈信息
- **Level 2-4：** 读取实现计划（`docs/plans/` 目录）
- **Level 3-4：** 读取创意设计文档（`memory-bank/creative/` 目录）
- 读取 `memory-bank/activeContext.md` 获取当前状态

### 2. 审查计划

**Level 2-4：**
1. 阅读完整计划
2. 批判性审查 — 识别问题或顾虑
3. 如果有顾虑 → 与用户讨论后再开始
4. 如果无顾虑 → 创建任务列表并开始

**Level 1（无正式计划）：**
1. 基于 `/van` 的任务描述和代码审查，直接确认修改方案
2. 用 1-2 句话与用户确认修复方向
3. 创建简单任务列表并开始

### 2.5 确认 TDD 模式

如果用户明确要求不使用 TDD：
- 记录到 `activeContext.md`：`TDD 模式：已由用户豁免`
- 跳过 RED-GREEN-REFACTOR 循环中"先写测试"的严格顺序
- **仍然要求**：实现后编写测试（保证测试覆盖）、代码审查不可跳过、完成验证不可跳过
- 在 `/reflect` 回顾时标记"本次未使用 TDD"供评估

### 3. 执行每个任务

**严格遵循 TDD 流程（来自 `.cursor/rules/skills/test-driven-development.mdc`）：**

对每个任务：

```
标记任务：进行中
    ↓
RED — 编写失败测试
    ↓
验证 RED — 运行测试，确认失败
    ↓
GREEN — 写最少代码使测试通过
    ↓
验证 GREEN — 运行测试，确认通过
    ↓
REFACTOR — 清理代码（保持测试绿色）
    ↓
代码审查 — 自查规格合规性和代码质量
    ↓
标记任务：完成
    ↓
更新 Memory Bank
    ↓
提交 — git commit（含代码变更 + MB 更新）
```

### 4. TDD 铁律

> 没有失败的测试，就不能写生产代码。
> 在测试之前写了代码？删除它。重新开始。

- 先写测试，看它失败
- 写最少代码使测试通过
- 重构保持测试绿色
- 每个功能提交一次

### 5. 验证（来自 `.cursor/rules/skills/verification.mdc`）

在声称任何任务完成之前：

1. 运行完整测试套件
2. 检查所有测试通过
3. 检查没有 linter 错误
4. 确认输出干净
5. **然后才能**声明完成

> **永远不要**使用"应该通过"、"看起来正确"这样的措辞。用证据说话。

### 6. 持续更新 Memory Bank

每完成一个任务后：
- 更新 `memory-bank/tasks.md` — 标记任务完成
- 更新 `memory-bank/progress.md` — 记录进度和观察
- 更新 `memory-bank/activeContext.md` — 更新当前焦点

### 7. 所有任务完成后

执行**完成验证**（来自 `.cursor/rules/skills/verification.mdc`）：
1. 运行完整测试套件，确认全部通过
2. 检查 linter 无错误
3. 重读计划，逐项确认需求已满足
4. **依赖安全审计**（如果本次任务新增或更新了依赖）— 遵循 `.cursor/rules/skills/security.mdc` 第四章，运行依赖审计命令，CRITICAL/HIGH 漏洞必须处理
5. 用证据记录验证结果

```markdown
## 🔨 构建完成

**已完成任务：** [N/M]
**测试状态：** [X/X 通过] （附验证命令输出）
**提交记录：** [提交列表]
**分支：** [当前分支名]

**下一步：** 使用 `/reflect` 进行回顾
```

### 7.5 收尾提交

如果步骤 7 的最终验证后仍有未提交的 Memory Bank 变更（如 `activeContext.md` 焦点更新）：
- 将变更纳入一次收尾提交
- 提交消息：`chore(build): finalize [task_id] memory bank state`
- 确保 working tree 干净后再输出构建完成报告

## 遇到问题时

| 情况 | 处理方式 |
|------|---------|
| 阻塞（缺少依赖、测试失败、指令不清） | 停下来询问，不要猜测 |
| 计划有关键缺陷 | 返回 `/plan` 修正 |
| 发现需要设计决策 | 使用 `/creative` 处理后再继续 |
| 遇到 Bug | 使用 `/debug` 流程排查 |
| 任务比预期更复杂 | 升级复杂度级别：更新 `tasks.md` 中的级别，保存当前进度到 `progress.md`，根据新级别补充缺失步骤（如返回 `/plan` 或 `/creative`） |

## 子代理模式（如果可用）

如果环境支持子代理，使用子代理驱动开发（`.cursor/rules/skills/subagent-development.mdc`）：
- 每个任务分派新鲜子代理
- 两阶段审查：规格合规 → 代码质量
- 自动化迭代修复

## 关键原则

- **不要在 main/master 分支上开始实现**（除非用户明确同意）
- **严格遵循 TDD** — RED → GREEN → REFACTOR
- **频繁提交** — 每个功能提交一次
- **证据优于断言** — 运行验证命令后才声明完成
- **按计划执行** — 不偏离计划，有问题先讨论
