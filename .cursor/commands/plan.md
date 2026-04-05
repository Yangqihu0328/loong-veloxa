---
description: "创建详细实现计划，融合头脑风暴和计划编写技能。遵循 .cursor/rules/skills/brainstorming.mdc 和 .cursor/rules/skills/writing-plans.mdc"
---

# /plan — 任务规划

你现在以 PLAN（规划）模式运行。你的职责是通过头脑风暴细化需求，然后创建详细的实现计划。

## 执行步骤

### 0. 前置条件检查

读取 `memory-bank/activeContext.md`，验证：
- 当前阶段必须是 `初始化`（由 `/van` 设置）
- 如果阶段是 `空闲` → 提示用户先执行 `/van` 初始化任务
- 如果阶段是 `规划中` 或 `设计中` → 提示用户任务已过了规划阶段，询问是否需要重新规划
  - 如果用户确认重新规划：检查 `memory-bank/creative/` 中是否有当前任务的设计文档；如果有 → 提示"重新规划将使已有的创意设计文档失效"，用户确认后在创意文档头部标注 `**状态：已失效（因重新规划）**`，并清空 `tasks.md` 中"需要创意阶段的组件"列表
- 如果阶段是 `构建中` → 提示用户"任务已在构建中，重新规划将回退到规划阶段并使已有的实现进度需要重新评估"，用户确认后：
  - 将 `activeContext.md` 阶段回退为 `规划中`
  - 检查 `memory-bank/creative/` 中是否有当前任务的设计文档，如有则标注失效（同上）
  - 在 `progress.md` 中记录回退原因
  - 继续执行规划流程

### 1. 读取上下文

- 读取 `memory-bank/tasks.md` 获取任务需求
- 读取 `memory-bank/activeContext.md` 获取当前焦点
- 读取 `memory-bank/techContext.md` 获取技术栈和约束
- 读取 `memory-bank/systemPatterns.md` 获取现有架构模式
- 审查代码库结构

### 2. 头脑风暴阶段（来自 Superpowers brainstorming 技能）

遵循 `.cursor/rules/skills/brainstorming.mdc` 规则：

1. **探索项目上下文** — 检查文件、文档、最近提交
2. **逐一提问** — 理解目的、约束、成功标准
3. **提出 2-3 种方案** — 附带权衡分析和推荐
4. **展示设计** — 按部分展示，覆盖架构、组件、数据流、错误处理、测试策略、**安全考量**，每部分后确认
5. **安全分析**（如果任务在 `/van` 中被标注为 `[安全相关]`）— 遵循 `.cursor/rules/skills/security.mdc` 执行轻量威胁建模，将安全需求纳入设计
6. **用户批准设计**

> **铁律：** 在设计被批准之前，不编写任何代码。

### 3. 保存设计文档

头脑风暴产出的设计规格保存到 `docs/specs/YYYY-MM-DD-[feature-name]-design.md`，请用户审查规格文档后再继续。

### 4. 编写实现计划（来自 Superpowers writing-plans 技能）

遵循 `.cursor/rules/skills/writing-plans.mdc` 规则：

1. **列出文件结构** — 将创建或修改的文件及职责
2. **标注变更影响** — 每个任务标注：
   - 是否修改共享文件（CMakeLists.txt、vcpkg.json、package.json 等）→ 如是，标注"[共享文件]"
   - 是否影响前序任务的测试 → 如是，标注"[影响前序测试]"并说明
   - 是否涉及子代理 → 如是，引用 `.cursor/rules/skills/subagent-development.mdc` 中的治理规范
3. **分解为小块任务** — 每步 2-5 分钟
4. **遵循 TDD 结构** — 每个任务包含：写测试 → 验证失败 → 实现 → 验证通过 → 提交
5. **安全任务**（如果涉及安全需求）— 为输入验证、权限校验、敏感数据处理等生成专门的安全测试任务
6. **包含完整代码** — 不只是描述
7. **保存计划** — 到 `docs/plans/YYYY-MM-DD-[feature-name].md`

### 5. 技术验证（Level 2-4）

- 验证依赖可用性
- 确认 API 兼容性
- 检查潜在冲突

### 6. 识别创意阶段需求

对于 Level 3-4 任务，标识需要 `/creative` 命令处理的组件：
- 需要 UI/UX 设计决策的组件
- 需要架构设计决策的组件
- 需要算法设计决策的组件

### 7. 更新 Memory Bank

- 更新 `memory-bank/tasks.md`：写入完整计划，在"需要创意阶段的组件"部分明确列出（如有）
- 更新 `memory-bank/activeContext.md`：将阶段更新为 `规划中`
- 更新 `memory-bank/progress.md`：记录规划完成的里程碑

### 8. 输出

```markdown
## 📋 规划完成

**设计文档：** `docs/specs/[path]`（如有）
**实现计划：** `docs/plans/[path]`
**任务总数：** [N] 个任务
**预估时间：** [时间]
**需要创意阶段：** [是/否]

**下一步：**
- 需要创意阶段 → 使用 `/creative`
- 不需要 → 使用 `/build` 开始实现
```
