---
description: "初始化项目、检测环境、确定任务复杂度、路由到合适的工作流。请先阅读 .cursor/rules/main.mdc 和 .cursor/rules/workflow/complexity-levels.mdc"
---

# /van — 初始化与入口

你现在以 VAN（初始化）模式运行。你的职责是初始化项目环境、分析任务并路由到正确的工作流。

## 执行步骤

### 0. 前置条件检查

> **注意：** 本系统不支持并发任务。同一时间只能有一个活跃任务。如果需要处理多个任务，请使用 `/archive` 完成当前任务后再开始新任务。

- 如果用户未提供任务描述，**立即询问**用户要做什么再继续
- 读取 `memory-bank/activeContext.md`，检查当前阶段
  - **状态一致性检查：**
    - 如果 `tasks.md` 中任务状态为 `已完成` 或 `已取消` 或 `已搁置`，但 `activeContext.md` 不是 `空闲` → 归档/清理可能中断，自动将 `activeContext.md` 重置为 `空闲`
    - 如果 `activeContext.md` 显示 `调试中` 但无调试前状态记录 → 异常状态，提示用户选择恢复方式
    - 如果 `activeContext.md` 显示活跃状态但 `tasks.md` 中无对应的活跃任务 → 孤立上下文，自动将 `activeContext.md` 重置为 `空闲`
    - 如果 `activeContext.md` 记录的任务 ID 与 `tasks.md` 中最新活跃任务 ID 不匹配 → 提示用户确认以哪个为准
  - 如果当前阶段不是 `空闲`、`已完成`、`已搁置` 或 `已取消`，说明有未完成任务 → 进入**恢复模式**（见下方）
  - 如果当前阶段是 `空闲`、`已完成`、`已搁置` 或 `已取消` → 正常初始化

### 1. 检测环境

- 检测操作系统（Linux/macOS/Windows）
- 检查项目目录结构
- 识别技术栈（package.json, requirements.txt, go.mod 等）
- 检查 git 状态
  - 如果不是 git 仓库，询问用户是否需要 `git init`
  - 如果 git 不可用，跳过 git 相关步骤并记录

### 2. 验证 Memory Bank

检查 `memory-bank/` 目录是否存在：

**如果不存在：**
- 创建 `memory-bank/` 目录结构（含 creative/, reflection/, archive/ 子目录）
- 创建核心文件（tasks.md, activeContext.md, progress.md, projectbrief.md, techContext.md, systemPatterns.md, productContext.md）
- 创建 `docs/specs/` 和 `docs/plans/` 目录
- 扫描项目结构填充 projectbrief.md 和 techContext.md

**如果存在：**
- 读取 `memory-bank/tasks.md` 检查待办任务
- 读取 `memory-bank/activeContext.md` 恢复上下文
- 读取 `memory-bank/progress.md` 检查进度
- 读取 `memory-bank/techContext.md` 了解技术栈
- 读取 `memory-bank/systemPatterns.md` 了解架构模式
- 读取 `memory-bank/productContext.md` 了解产品上下文

### 3. 生成任务 ID

格式：`TASK-YYYYMMDD-NN`（当天日期 + 序号）

检查 `memory-bank/tasks.md` 的任务历史，确定当天序号。

### 4. 分析任务需求

基于用户描述和项目上下文，确定复杂度级别：

| 级别 | 特征 | 路由 |
|------|------|------|
| **Level 1** | 1-2 文件修改，修复路径明确 | → `/build` |
| **Level 2** | 多文件修改，需求清晰 | → `/plan` |
| **Level 3** | 新组件，需要设计决策 | → `/plan` |
| **Level 4** | 多个子系统，架构决策 | → `/plan` |

**安全需求识别：** 检查任务是否涉及以下安全相关领域，如有则在 `tasks.md` 中标注 `[安全相关]`：
- 用户输入处理（表单、API 参数、文件上传）
- 认证/授权（登录、权限、Token）
- 数据存储（数据库、缓存、文件系统）
- 外部集成（第三方 API、WebHook）
- 部署配置（环境变量、网络、HTTPS）

### 4.5 前置验证清单（Level 2+）

在确定复杂度级别后，对以下维度做快速检查：

| 维度 | 检查内容 | 方法 |
|------|---------|------|
| **依赖可获取性** | 任务需要的第三方库/模型/服务是否可用？ | 检查 vcpkg.json、网络访问、模型文件 |
| **环境就绪** | 构建工具链、测试框架、CI 能否支持？ | 快速 `cmake --build` 或 `npm run build` 验证 |
| **已有 artifact** | 计划新建的文件/组件是否已存在？ | Glob 扫描目标路径 |
| **待处理事项** | `activeContext.md` 中是否有与本任务相关的遗留改进建议？ | 读取并标注关联 |

如果发现阻碍项，在初始化报告中标注并建议处理方式（先解决/绕过/降级）。
如果所有检查通过，在报告中注明"前置验证通过"。

### 5. Git 分支（Level 2+）

对于 Level 2 及以上的任务，遵循 `.cursor/rules/skills/git-workflow.mdc`：

#### 工作区清理（创建分支前）

在创建功能分支前，运行 `git status` 检查工作区状态：
- **干净状态** → 继续创建分支
- **有未提交变更** → 分析变更归属：
  - 属于上一任务的工作流产出物（Memory Bank、reflection、archive 文件）→ 自动提交到当前分支，消息：`chore(workflow): commit residual artifacts from previous task`
  - 属于未完成的代码变更 → 提示用户选择：提交 / stash / 放弃
  - 混合情况 → 分别处理，先提交工作流产出物，再处理代码变更

#### 分支基线分析

创建功能分支前，确定合适的基线分支（不一定是 main）：

1. **检查任务代码依赖：**
   - 任务描述是否引用了某个功能分支的代码？（如"落实 TASK-xxx 回顾的改进"、"补充 Phase X.Y 的测试"）
   - 任务涉及的源文件是否仅存在于某个未合并分支？

2. **交叉比对未合并分支列表：**
   - 读取 `activeContext.md` 的「未合并分支」表
   - 对每个候选分支执行 `git log --oneline main..<branch> -- <涉及的文件路径>` 检查是否包含相关代码

3. **基线决策：**

   | 场景 | 基线分支 | 示例 |
   |------|---------|------|
   | 任务修改的代码在 main 上已存在 | `main` | 修改工作流规则文件 |
   | 任务依赖的代码仅在某个未合并分支上 | 该功能分支 | 为 Phase 1.4 的 aegisctl bench 补充测试 |
   | 任务依赖多个未合并分支的代码 | 最近的公共祖先分支，或先合并依赖分支 | 跨子阶段的集成改进 |

4. **向用户报告基线建议：**
   ```
   分支基线分析：
   - 待修改文件：[文件列表]
   - 文件在 main 上：[是/否]
   - 建议基线：[main / feature/xxx]
   - 原因：[简要说明]
   ```

- 建议创建功能分支：`git checkout -b feature/[task-id]-[short-description] <基线分支>`
- 征求用户确认后执行

### 6. 更新 Memory Bank

- 更新 `memory-bank/tasks.md`：使用完整任务结构记录新任务
- 更新 `memory-bank/activeContext.md`：**将当前阶段设为 `初始化`**，设置当前任务 ID 和焦点

### 7. 输出初始化报告

```markdown
## 🔍 项目初始化报告

**环境：** [OS] | [技术栈] | [Git 状态]
**Memory Bank：** [已存在/新创建]
**当前任务：** [任务描述]
**复杂度级别：** Level [1-4] — [级别名称]
**推荐工作流：** [工作流路径]

**下一步：** 使用 `/[下一命令]` 继续
```

## 路由规则

- Level 1 → 直接推荐 `/build`
- Level 2 → 推荐 `/plan`
- Level 3-4 → 推荐 `/plan`（之后会进入 `/creative`）
- Bug/异常 → 推荐 `/debug`

## 恢复模式

如果检测到未完成的任务（从 Memory Bank），询问用户：
1. **继续未完成的任务** — 恢复上下文，路由到任务当前所处阶段对应的命令
2. **开始新任务（搁置旧任务）** — 执行轻量归档：
   - 将旧任务在 `tasks.md` 中标记为 `已搁置` 并更新任务历史表
   - 在 `progress.md` 中简要记录搁置原因和当前进度快照
   - **清理中间文件**：检查 `docs/specs/`、`docs/plans/`、`memory-bank/creative/` 中与旧任务相关的文件，列出并询问用户是保留还是删除
   - 重置 `activeContext.md` 为空闲
   - 正常初始化新任务
3. **放弃旧任务** — 执行完整清理：
   - 将旧任务在 `tasks.md` 中标记为 `已取消`
   - **删除旧任务产生的所有中间文件**：`docs/specs/`、`docs/plans/`、`memory-bank/creative/` 中与旧任务相关的文件
   - 清理 `progress.md` 中的旧任务内容
   - 重置 `activeContext.md` 为空闲
   - 正常初始化新任务
