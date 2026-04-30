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

#### d.2 算法伪码 chain / accumulator / state 累积语义必须 explicit method（**P1 强制**，TASK-20260430-01 升级）

> 反复模式新候选定型（TASK-20260430-01 实证）：spec §5.4 算法 step 4 写 `if (child->collapsed_through) cur_chain = child_outgoing` 用赋值符 `=`（覆盖语义）；build P3 GREEN 实施后 A5 既有 sibling collapse-through 测试退化 — 父循环已累积 sibling chain，遇到 collapse-through child 时 `cur_chain` 累积值被 `child_outgoing` 完全覆盖丢失。根因 = 算法草拟时未演练「父循环已累积 → 遇到 child outgoing → 应合并而非覆盖」场景；赋值符 `=` 是高歧义符号（覆盖？合并？swap？）。

**触发条件**：creative / spec 阶段算法伪码涉及以下任一情形 → 必填本段：

- `chain` / `accumulator` / `cursor` / `running pointer` / `state machine` 累积量
- 双向取极值（`max(positive)` + `min(negative)`）
- list / queue / stack 等容器追加
- counter / aggregator 跨循环 / 跨递归累积

**强制规则**：算法伪码**禁止**对累积量直接用 `=` 赋值符，**必须**用 explicit method name 替代：

| 语义 | ❌ 反模式 | ✅ 正模式 | 说明 |
|---|---|---|---|
| 合并（双向取极值 / union）| `cur_chain = child_outgoing` | `cur_chain.MergeFrom(child_outgoing)` / `cur_chain.Union(child_outgoing)` | 保留双方所有信息 |
| 覆盖（丢弃旧值）| `cur_chain = child_outgoing` | `cur_chain.Replace(child_outgoing)` / `cur_chain = std::move(child_outgoing)` | 显式声明丢弃 |
| 交换（双向）| `swap(a, b)` 隐含 | `swap(cur_chain, child_outgoing)` | 标准库语义清晰 |
| 追加 | `list = list + new` | `list.Append(new)` / `list.PushBack(new)` | 容器语义 |
| 累积（计数 / 求和）| `total = total + delta` | `total.Add(delta)` / `total += delta` | `+=` 比 `=` 安全；method 更显式 |
| 重置 / 清空 | `cur = {}` | `cur.Clear()` / `cur.Reset()` | 显式语义 |

**强制产出**：creative / spec 算法伪码段必须配套「**累积量语义对照表**」，列出每个累积量的 explicit method 与对应行为：

| 累积量 | 类型 | 累积语义 | 操作 method | 反模式（禁用）|
|---|---|---|---|---|
| `cur_chain` | `MarginChain` | 合并（max-pos / min-neg）| `MergeFrom(other)` | `cur_chain = other` |
| `last_in_flow_block` | `LayoutBox*` | 覆盖（最后一个 in-flow）| `Replace(box)` 或 `cur = box` 显式注释「覆盖」| 隐式 `=` 无注释 |
| `outgoing_chain` | `MarginChain` | 合并（含 parent.mb）| `MergeFrom(parent_mb)` + `Add(box.margin_bottom)` | `outgoing = box.margin_bottom` |

**判读规则**：

- 算法伪码出现「累积量 `=` 表达式」 → **必须**改写为 explicit method 或加注释明确「覆盖」/「合并」语义
- 同一变量在算法不同分支既被「覆盖」又被「合并」→ 必须用不同 method 区分（`Replace()` vs `MergeFrom()`），不能两处都用 `=`
- 编程语言原生 `+=` / `-=` / `|=` 等复合赋值符视为 explicit（语义明确），不在禁用范围

**反模式**（plan / creative 阶段必拦截）：

- ❌ `cur_chain = child_outgoing`（歧义：覆盖？合并？swap？）
- ❌ `state.value = new_value`（无注释）当 `state` 是累积量
- ❌ 算法伪码用「赋值」描述既有可能覆盖又有可能合并的操作
- ❌ 用 `=` 但靠注释 `// merge` 解释 — 注释易丢失/不一致；method name 强制对齐语义

**TASK-20260430-01 实证**：spec §5.4 step 4 `cur_chain = child_outgoing` 字面赋值；build P3 GREEN 后 A5 既有 sibling collapse-through 测试 FAIL（cur_chain 累积值被覆盖丢失）→ 加 `MarginChain::MergeFrom(other)` 合并语义（max_positive / min_negative 双向取极值）+ 调用站点改为 `cur_chain.MergeFrom(child_outgoing)`，A5 PASS。

**与 §d.1 协同**：§d.1 锁定坐标约定单一图（防 sign error）；本节锁定累积量语义对照表（防赋值符歧义）。两者覆盖 creative 阶段算法伪码完整边界。

**交叉引用**：`.cursor/rules/skills/writing-plans.mdc` §9.4「递归算法 API 传递语义决策必检项」（同源 reflection §6 建议 #1，强制配套使用）；`memory-bank/systemPatterns.md`「算法伪码累积语义 explicit method」段（待 archive 阶段补）。

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
