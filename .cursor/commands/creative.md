---
description: "为标记的组件执行结构化设计探索，探索多种设计方案。遵循 .cursor/rules/workflow/memory-bank-paths.mdc"
---

# /creative — 创意设计

你现在以 CREATIVE（创意设计）模式运行。你的职责是对标记为需要设计决策的组件进行结构化的设计探索。

本命令的方法论基于 Claude "Think" 工具方法论 — 通过结构化思考框架进行深度设计探索。

## 执行步骤

### 0. 前置条件检查

读取 `memory-bank/activeContext.md`，验证：
- 当前阶段必须是 `规划中`（由 `/plan` 设置）
- 如果阶段是 `空闲` 或 `初始化` → 提示用户先完成 `/van` → `/plan`
- 如果阶段是 `构建中` 或更后阶段 → 提示用户任务已进入构建，询问是否需要回退设计

验证 `memory-bank/tasks.md` 中存在标记为"需要创意阶段"的组件：
- 如果没有需要设计的组件 → 提示用户可以跳过 `/creative` 直接使用 `/build`

### 1. 读取上下文

- 读取 `memory-bank/tasks.md` 获取标记为需要创意设计的组件
- 读取实现计划获取组件需求
- 读取 `memory-bank/activeContext.md` 获取当前焦点
- 读取 `memory-bank/systemPatterns.md` 获取现有架构模式（设计决策参考）

### 2. 对每个需要设计的组件

执行以下结构化设计探索流程：

#### a. 定义设计挑战

```markdown
## 设计挑战：[组件名称]

**问题陈述：** [需要解决什么]
**约束条件：** [限制和要求]
**成功标准：** [什么算是好的设计]
```

#### b. 探索多种方案

对每个组件探索至少 2-3 种方案：

```markdown
### 方案 A：[方案名称]

**描述：** [方案概述]
**优点：**
- [优点 1]
- [优点 2]
**缺点：**
- [缺点 1]
- [缺点 2]
**复杂度：** [低/中/高]
**风险：** [风险评估]
```

#### c. 方案对比

使用表格对比：

| 维度 | 方案 A | 方案 B | 方案 C |
|------|--------|--------|--------|
| 复杂度 | | | |
| 可维护性 | | | |
| 性能 | | | |
| 扩展性 | | | |
| 安全性 | | | |
| 风险 | | | |

#### d. 推荐并论证

- 给出推荐方案
- 解释选择理由
- 说明如何缓解风险

#### d.1 算法/坐标约定一图（**P0 强制**，涉及 ≥2 坐标系/方向算法时必填）

> 反复模式「非默认路径遗漏验证」第 5 次升级 P0（TASK-20260426-01 R4 实证）：复杂坐标系算法（vertical-align 的 baseline / ascent / descent / offset / top-bottom 多方向多锚点）在 build 阶段反复出现 sign error。creative 锁定了算法但未锁定坐标约定，致 build 阶段调试 ~30 min 系统性诊断后才采用统一坐标约定。

**触发条件**：算法涉及以下任一情况时必须产出「单一坐标约定 + 公式表」一图：

- ≥2 个坐标系（屏幕坐标 / 局部坐标 / baseline-relative / ascent-relative 等）
- ≥2 个方向（top/bottom、left/right、ascent/descent、main/cross 等）
- 多锚点的偏移计算（vertical-align、flex-align、grid-place、transform-origin 等）

**强制产出 3 项**：

1. **统一坐标约定声明** — 文字 + 一行公式描述坐标系基准点 + 正负方向语义

   范例：`item.y = baseline_y - item.ascent + offset`，`offset > 0 下沉 / < 0 上升`

2. **全部公式按约定列出** — 每条算法分支（每个 enum case / 每个 branch）按统一约定写公式：

   | 分支 | offset 公式 | 半-leading 应用 | 备注 |
   |---|---|---|---|
   | `kBaseline` | `0` | 不应用 | 默认基线 |
   | `kSub` | `+0.2 × ascent` | 不应用 | 下沉 |
   | ... | ... | ... | ... |

3. **build 阶段引用注释格式** — 实施时每条公式实施时引用约定（代码注释含 `// per creative-X.md §Y coordinate convention`）

**反模式**：creative 锁定算法（如 "strict 2-pass"）但只用文字描述「向上偏移」「下移半-leading」而不画出统一公式表 → build 阶段每条分支独立推算坐标 → 系统性 sign error。

**TASK-20260426-01 实证**：creative-line-box-model.md D2.B 锁定了 strict 2-pass 但未画坐标约定单一图；R4.6 实施时 ComputeNonExtremeAlign 6 关键字 + Phase 1/2 max_ascent/max_descent 维护 + kTop/kBottom offset 多处 sign error，调试 ~30 min 才解。

**交叉引用**：`memory-bank/systemPatterns.md`「Creative 阶段「单一坐标约定 + 公式表」一图」段。

#### e. 用户确认

等待用户确认或调整后再继续下一个组件。

### 3. 保存设计文档

对每个完成设计的组件，创建文档：

`memory-bank/creative/creative-[feature-name].md`

文档格式：

```markdown
# 创意设计：[组件名称]

**日期：** [YYYY-MM-DD]
**状态：** 已批准
**关联任务：** [task_id]

## 设计挑战
[问题陈述和约束]

## 探索的方案
[每个方案的描述和权衡]

## 选定方案
[推荐方案及理由]

## 实现指导
[关键实现细节和注意事项]
```

### 4. 更新 Memory Bank

- 更新 `memory-bank/tasks.md`：记录设计决策
- 更新 `memory-bank/activeContext.md`：将阶段更新为 `设计中`
- 更新 `memory-bank/progress.md`：记录设计完成的里程碑

### 5. 输出

```markdown
## 🎨 创意设计完成

**已完成设计的组件：**
1. [组件 1] — 选定方案：[方案名]
2. [组件 2] — 选定方案：[方案名]

**设计文档：**
- `memory-bank/creative/creative-[name1].md`
- `memory-bank/creative/creative-[name2].md`

**下一步：** 使用 `/build` 开始实现
```

## 关键原则

- **渐进式文档** — 文档复杂度匹配任务复杂度
- **按需详细** — 简单决策简要记录，复杂决策深入分析
- **表格对比** — 使用表格简洁对比方案
- **用户参与** — 每个关键设计决策都需要用户确认
