# 流程规则 P0/P1 沉淀冲刺 — 设计规格

**任务 ID：** TASK-20260419-13
**日期：** 2026-04-19
**复杂度级别：** Level 2
**目标：** 一次性闭环 3 条积压流程规则条目（1 P0 + 2 P1），使它们从「反思建议」升级为「可执行守卫 + 可追溯文档」。

---

## 1. 范围与非目标

### 1.1 范围

**3 条规则条目：**


| #   | 优先级   | 来源                                                         | 核心问题                                                                  |
| --- | ----- | ---------------------------------------------------------- | --------------------------------------------------------------------- |
| 1   | 🔴 P0 | 反复 9+ 次（TASK-02/04/07 反思反复识别但只升 P1，TASK-03 Round 2 破例升 P0） | FetchContent 任务未先设 git 全局代理 → 首次 cmake 超时挂死，单次 5-10 min × N = 累计 ≥ 1h |
| 2   | 🟠 P1 | TASK-11 反思 #2 新增                                           | Plan 阶段假设 smoke 工具链可用（jq / bc / valgrind 等），Build 才发现缺失返工             |
| 3   | 🟠 P1 | TASK-03 Round 1 首发 + TASK-11 反思复确 3+ 次                     | Level 2+ phase ≥ 5 任务无「轮次完成」中间态，用户被迫等整个任务完成才能看回顾                      |


### 1.2 非目标

- ❌ 不调整工作流核心骨架（`/van` → `/plan` → ... → `/archive`）
- ❌ 不引入新 cursor 命令（如 `/round-complete`）
- ❌ 不硬编码开发环境代理地址（仅占位符 `<开发环境代理地址>`）
- ❌ 不自动在 VAN 阶段执行 `git config` 命令（**由用户确认后执行**，保持用户控制权）

---

## 2. 设计架构（双点/三点联动）

```
┌─────────────────────────────────────────────────────────────────┐
│ 条目 1：P0 FetchContent 网络代理守卫（双点联动）                 │
├─────────────────────────────────────────────────────────────────┤
│  writing-plans.mdc          .cursor/commands/van.md             │
│  ├─ (NEW) 「FetchContent     └─ (UPD) 步骤 1 检查 git 状态       │
│  │   网络代理守卫」段           └─ (NEW) 子项「FetchContent     │
│  │                                      代理状态检查」           │
│  │                                                               │
│  └─ 引用 techContext.md 「FetchContent 与代理」段（既有）        │
│     提供开发环境代理地址的单一真相来源                            │
│                                                                  │
│  techContext.md                                                  │
│  └─ (UPD) 「FetchContent 与代理」段尾部追加                       │
│     「→ plan 阶段守卫：见 writing-plans.mdc 新段」交叉引用         │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│ 条目 2：P1 smoke 工具链 grep（单点）                             │
├─────────────────────────────────────────────────────────────────┤
│  writing-plans.mdc                                               │
│  └─ (UPD) §5.4 「Bench Smoke 验收三件套」尾部追加子块            │
│     「前置要求：工具链可用性 grep」                               │
│     - 段号编号不变（不破坏 §5.5/5.6 的位置）                      │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│ 条目 3：P1 多轮次 Build 中间态（三点联动）                       │
├─────────────────────────────────────────────────────────────────┤
│  complexity-levels.mdc                                           │
│  └─ (NEW) L66 Level 4 迭代机制段之后，新增跨级别段               │
│     「多轮次 Build 中间态」（适用 Level 2+ 且 phase ≥ 5 任务）    │
│                                                                  │
│  .cursor/commands/build.md                                       │
│  └─ (NEW) §6 和 §7 之间插入 §6.5「轮次完成判断」分支             │
│                                                                  │
│  .cursor/commands/reflect.md                                     │
│  └─ (UPD) §0 前置守卫放宽：                                      │
│     「必须是 `构建中`」→「必须是 `构建中` 或                     │
│       `构建中·轮次 N 完成`」                                     │
└─────────────────────────────────────────────────────────────────┘
```

---

## 3. 详细文案设计

### 3.1 条目 1：FetchContent 网络代理守卫

#### 3.1.1 `writing-plans.mdc` 新段（插入到 L94 后）

```markdown
## FetchContent 网络代理守卫（涉及 FetchContent 且未离线 `_deps` 的任务必填）

> 反复出现 9+ 次（TASK-20260413-01 / TASK-20260419-02 / -04 / 反思反复识别）：
> Cursor 沙箱 / WSL 环境首次 `cmake -B build` 触发 FetchContent 因 git 全局代理未设而超时挂死；单次 5-10 min 恢复，累计 ≥ 1h。

在任务分解**之前**回答：

1. **是否触发 FetchContent 拉取**（命中任一即触发本段守卫）：
   - 根或子 `CMakeLists.txt` 已存在 `FetchContent_Declare`（grep 验证）
   - **且** 以下任一：
     - `build/` 或 `build-bench/` 目录不存在
     - `build*/_deps/` 未包含目标依赖源码
   - **或** 本次 plan 新增 `FetchContent_Declare`
   - **跳过条件**：已离线预置 `_deps/` 完整目录（含目标依赖）

2. **检测当前代理状态**（VAN 或 Plan Phase 0 执行）：
   ```bash
   git config --global --get http.proxy
```

- 有返回 → 记录到 `progress.md` 进入下一步核对
- 空返回 → **必须在 VAN 阶段补设**（见 3）

1. **补设代理（VAN 阶段强制）**：
  开发环境代理地址的**单一真相来源**是 `memory-bank/techContext.md`「FetchContent 与代理」段。规则文件中**严禁硬编码** IP，用占位符：
   执行后再次 `git config --global --get http.proxy` 确认生效并记录到 `progress.md`。
2. **Plan Phase 0 固化**：
  plan 文档必须包含 Phase 0「代理状态核验」步骤，执行 grep 并把输出作为制品写入 `progress.md`，以便 `/reflect` 阶段回溯。
3. **Archive 阶段决策**：
  `/archive` 阶段询问用户：保留代理 / unset (`git config --global --unset http.proxy`)。默认**保留**（下一任务多半也需要）。
4. **交叉引用**：
  - 开发环境代理地址：`memory-bank/techContext.md`「FetchContent 与代理」段
  - VAN 命令守卫：`.cursor/commands/van.md` 步骤 1「FetchContent 代理状态检查」子项

```

#### 3.1.2 `.cursor/commands/van.md` 步骤 1 子项（插入现有"检查 git 状态"之后）

```markdown
### 1. 检测环境

- 检测操作系统（Linux/macOS/Windows）
- 检查项目目录结构
- 识别技术栈（package.json, requirements.txt, go.mod 等）
- 检查 git 状态
  - 如果不是 git 仓库，询问用户是否需要 `git init`
  - 如果 git 不可用，跳过 git 相关步骤并记录
- **FetchContent 代理状态检查**（如项目含 `FetchContent_Declare`）：
  - grep 验证：`rg "FetchContent_Declare" --type cmake`（命中即触发检查）
  - 运行 `git config --global --get http.proxy`
    - 有输出 → 记录到初始化报告并继续
    - 空输出且环境为 Cursor 沙箱 / WSL → **触发 `writing-plans.mdc`「FetchContent 网络代理守卫」段 步骤 3**，从 `memory-bank/techContext.md`「FetchContent 与代理」段读取开发环境代理地址，询问用户确认后执行 `git config --global http.proxy <地址>`
  - 如 `_deps/` 已完整离线预置 → 跳过检查
```

#### 3.1.3 `techContext.md`「FetchContent 与代理」段尾部追加（交叉引用）

在现有段落末尾追加一段（不重写已有内容）：

```markdown
### Plan/VAN 阶段守卫

规则文件 `.cursor/rules/skills/writing-plans.mdc` 的「FetchContent 网络代理守卫」段要求每个 FetchContent 任务在 VAN / Plan Phase 0 检查并（必要时）补设 git 全局代理；`.cursor/commands/van.md` 步骤 1 已有对应自动检查子项。本段是代理地址的**单一真相来源**，规则文件中**严禁硬编码** IP 地址，统一用占位符 `<开发环境代理地址>`。
```

### 3.2 条目 2：smoke 工具链 grep

#### 3.2.1 `writing-plans.mdc` §5.4 末尾追加子块

在现有 §5.4「Bench Smoke 验收三件套」末尾（L214 左右）追加：

```markdown
#### 前置要求：工具链可用性 grep（Plan 阶段必执行，TASK-20260419-11 反思 #2 新增 P1）

smoke 验收引用的每个 CLI 工具必须在 **Plan 阶段**（而非 Build 阶段）grep 验证存在：

```bash
# 常见 smoke 工具清单（按任务实际引用的工具自行调整）：
for tool in jq bc valgrind awk xmllint python3 convert; do
  command -v "$tool" >/dev/null 2>&1 \
    && echo "[OK]   $tool = $(command -v $tool)" \
    || echo "[MISS] $tool not found"
done
```

如有 `[MISS]` 工具被 plan 脚本引用 → plan 必须切换为已验证可用的替代方案：


| 缺失工具       | 常见替代                                                   |
| ---------- | ------------------------------------------------------ |
| `jq`       | `python3 -c "import json,sys; ..."` heredoc            |
| `bc`       | `python3 -c "print(...)"` 或 `awk 'BEGIN{...}'`         |
| `xmllint`  | `python3 -c "import xml.etree.ElementTree as ET; ..."` |
| `valgrind` | `sanitizers` (ASan/UBSan)                              |


**反例（TASK-20260419-11 P3）：** plan §3 写 `jq '.benchmarks[] ...'` 做 smoke 自检，沙箱实际无 jq，Build 阶段临时改 python heredoc 损耗 ~3 min；本属"前置依赖未验证"反复模式，核心 API 已 5 处 grep 抑制，smoke 工具链漏验。

```

### 3.3 条目 3：多轮次 Build 中间态

#### 3.3.1 `complexity-levels.mdc` 新段（插入到 L66 Level 4 迭代机制段之后）

```markdown
## 多轮次 Build 中间态（跨级别通用，Level 2+ 且 phase ≥ 5 任务启用）

> 首发于 TASK-20260419-03 Round 1（10 phase / 2 轮次），TASK-20260419-11 反思复确 3+ 次。
> 问题：`/build` 阶段守卫只有「构建中 → 回顾中」二元跳转，phase ≥ 5 的任务中途无法暂存 checkpoint 汇报进度，用户被迫等整个任务完成才能看回顾；或被迫用 `wip: blocked on TASK-YY` 这种含外部状态的 commit subject（首发反例 TASK-03 Round 1）。

### 触发条件

任务满足以下任一即可启用「轮次完成」中间态：

- Level 2+ 且计划 phase 总数 ≥ 5
- Level 3/4 且计划按子系统分轮（原 Level 4 迭代机制的通用化）
- 用户在任何 Build 阶段主动要求「先做一轮完成汇报」

### 中间态定义

`activeContext.md` 当前阶段字段引入**子状态**（非独立阶段），形态：

- `构建中·轮次 N 完成` — 第 N 轮所有 phase 全部 GREEN，等待用户审查后启动第 N+1 轮或直接 `/reflect`
- `构建中`（原状态保留）— 未分轮或正在某轮内部进行中

### 阶段守卫调整

| 命令 | 调整 |
|---|---|
| `/build` | 在「所有任务完成」步骤之前增加「轮次完成判断」分支（详见 `.cursor/commands/build.md` §6.5） |
| `/reflect` | 前置检查允许 `构建中` ∪ `构建中·轮次 N 完成` 两种状态（详见 `.cursor/commands/reflect.md` §0）|
| `/archive` | 前置要求 `回顾中` 不变（无调整）|

### 提交约束（硬约束，P1 复确 3 次）

轮次完成时的 commit subject 格式：

- ✅ `chore(build): TASK-XX round N complete - <N 轮产出简述>`
- ❌ `wip: blocked on TASK-YY`（首发反例 TASK-20260419-03 Round 1）

commit subject **严禁**包含外部任务状态字样（`BLOCKED on ...` / `WAITING for ...` / `PENDING dep`）。若确需等待外部任务，在 commit body 说明，subject 保持独立语义。

### 交叉引用

- 命令守卫实现：`.cursor/commands/build.md` §6.5 / `.cursor/commands/reflect.md` §0
- 首发反例：TASK-20260419-03 归档 `archive-TASK-20260419-03.md`
```

#### 3.3.2 `.cursor/commands/build.md` §6 和 §7 之间新增 §6.5

```markdown
### 6.5 轮次完成判断（Level 2+ 且 phase ≥ 5 任务适用）

如任务计划包含 ≥ 5 个 phase 且用户声明或规则判定为「轮次」型任务（见 `.cursor/rules/workflow/complexity-levels.mdc`「多轮次 Build 中间态」段）：

- 每个轮次最后一个 phase 完成后，询问用户：
  1. **继续下一轮次** → 保持 `activeContext.md` 阶段为 `构建中`
  2. **声明轮次完成（checkpoint）** → 将 `activeContext.md` 阶段更新为 `构建中·轮次 N 完成`，产出轮次小结到 `progress.md`，**停下等待**用户下一步指令
  3. **直接进入 /reflect**（跳过后续轮次）→ 跳到步骤 7 的完成验证

- commit subject 必须符合 `complexity-levels.mdc`「多轮次 Build 中间态」段提交约束：
  - ✅ `chore(build): [task_id] round N complete - <简述>`
  - ❌ 禁止含 `BLOCKED` / `WAITING` / `PENDING dep` 等外部状态字样

- 用户声明全部轮次完成时 → 进入标准「步骤 7 所有任务完成后」流程
```

#### 3.3.3 `.cursor/commands/reflect.md` §0 守卫放宽

```markdown
### 0. 前置条件检查

读取 `memory-bank/activeContext.md`，验证：
- 当前阶段必须是 `构建中` 或 `构建中·轮次 N 完成`（由 `/build` 设置，后者详见 `.cursor/rules/workflow/complexity-levels.mdc`「多轮次 Build 中间态」段）
- 如果阶段是 `空闲` 或 `初始化` → 提示用户尚未开始构建，先完成前置步骤
- 如果阶段是 `规划中` 或 `设计中` → 提示用户需先执行 `/build`
- 如果阶段是 `回顾中` → 提示用户回顾已完成，可以使用 `/archive`
- 如果阶段是 `归档中` → 提示用户任务已进入归档阶段
```

---

## 4. 验证策略（无 TDD，文档类任务）

### 4.1 反例追溯表（每条规则逐条回溯 ≥ 3 相关任务）


| 规则             | 反例追溯任务                                            | 若规则当时生效，是否会捕获？                      |
| -------------- | ------------------------------------------------- | ----------------------------------- |
| 条目 1（proxy）    | TASK-02, TASK-04, TASK-07, TASK-13-01（QuickJS 接入） | 4/4 ✅ 均会通过 Phase 0 代理状态核验预防         |
| 条目 2（工具链 grep） | TASK-11 P3 jq 缺失                                  | 1/1 ✅ 会在 Plan 阶段捕获                  |
| 条目 3（轮次完成）     | TASK-03 Round 1（10 phase, 2 轮）, TASK-04 多 phase   | 2/2 ✅ 均可用 `构建中·轮次 N 完成` 状态代替 WIP 提交 |


### 4.2 YAML frontmatter 完整性

每次编辑 `.mdc` / `.md` 后验证：

```bash
for f in .cursor/rules/skills/writing-plans.mdc \
         .cursor/rules/workflow/complexity-levels.mdc \
         .cursor/commands/van.md \
         .cursor/commands/build.md \
         .cursor/commands/reflect.md; do
  head -5 "$f" | grep -c '^---$'
done
# 期待：每个 mdc 输出 2（--- pair 完整），每个 md 输出 0 或 2
```

### 4.3 交叉引用一致性

P0 段在 3 处交叉引用（`writing-plans.mdc` / `van.md` / `techContext.md`），grep 验证链路互通：

```bash
rg "FetchContent 网络代理守卫" --type-add 'mdc:*.{mdc,md}' -t mdc .cursor/ memory-bank/techContext.md
# 期待命中：writing-plans.mdc (定义) + van.md (引用) + techContext.md (引用) = 3 处
```

### 4.4 Markdown 局部可读性

逐条规则写完后，读一遍确认：标题层级对齐、代码块三反引号闭合、表格对齐、列表缩进一致。

---

## 5. 风险与缓解


| 风险                                             | 概率  | 严重度 | 缓解                                                                                                                                                    |
| ---------------------------------------------- | --- | --- | ----------------------------------------------------------------------------------------------------------------------------------------------------- |
| **R1** 规则文案过长导致 `.mdc` 超 700 行难阅读              | 低   | 中   | 本次新增 ~180 行，现有 `writing-plans.mdc` 473 行 → 650 行内，在 Cursor 系统提示预算内                                                                                    |
| **R2** 命令文件守卫新增破坏既有 VAN/Build/Reflect 流程       | 中   | 高   | P0 基线 grep 现有守卫文案 → 用「追加」而非「替换」模式；每 Phase 末复跑 `/van` `/build` 流程读通确认无语义歧义                                                                             |
| **R3** 条目 3 子状态 `构建中·轮次 N 完成` 引入其他命令文件未兼容的 bug | 中   | 中   | 仅 `reflect.md` 需调整；`plan.md` / `archive.md` / `debug.md` 前置守卫 grep 验证与新子状态兼容（它们只检查 `空闲` / `初始化` / `构建中` / `回顾中` / `归档中`，对子状态 `·轮次 N 完成` 视为 `构建中` 的扩展） |
| **R4** 交叉引用链路断裂（P0 段 3 处引用不一致）                 | 低   | 中   | §4.3 交叉引用一致性 grep + 每 Phase 末复跑                                                                                                                       |


---

## 6. 预估成本


| Phase | 内容                                                       | 预估        |
| ----- | -------------------------------------------------------- | --------- |
| P0    | 基线验证（grep 3 处锚点 + YAML frontmatter md5）                  | 5 min     |
| P1    | 条目 1 proxy（3 文件：writing-plans + van + techContext）+ 反例追溯 | 25-30 min |
| P2    | 条目 2 smoke（1 文件）+ 反例追溯                                   | 15 min    |
| P3    | 条目 3 多轮次（3 文件：complexity-levels + build + reflect）+ 反例追溯 | 30-35 min |
| P4    | 收尾（activeContext 长期项标记 + MB 同步 + 自审）                     | 10 min    |


**合计：** **85-95 min**（1.4-1.6h）

---

## 7. 决策清单（D1-D3）


| #      | 决策           | 选项                                             | 选择                                     | 理由                                                                   |
| ------ | ------------ | ---------------------------------------------- | -------------------------------------- | -------------------------------------------------------------------- |
| **D1** | 代理地址处理       | 占位符 / 环境变量 / techContext 位置引用                  | **占位符 `<开发环境代理地址>`**                   | 最安全，具体地址已沉淀在 techContext.md 单一真相来源；规则文件零硬编码 IP（用户决策）                 |
| **D2** | 条目 3 子状态实现方式 | 新阶段名 / 子状态标签 / 命令参数                            | **子状态标签 `构建中·轮次 N 完成`**                | 子状态而非新阶段，保持 5 主阶段骨架不变；对未感知此状态的命令（plan/archive/debug）视为 `构建中` 扩展，向后兼容 |
| **D3** | Phase 提交粒度   | 每 phase 1 commit / 每条目 1 commit / 1 big commit | **每 Phase 1 commit**（共 5 commits + 收尾） | 与 TASK-01/11 同模式；每条目规则可独立 review / 回滚                                |


---

## 8. 非功能需求

- **可追溯性**：每条规则文案带 `TASK-XX 反思 #Y` 来源注释
- **可测试性**：反例追溯表作为「事后测试」的替代（文档类无 TDD）
- **可回滚性**：每 Phase 独立 commit，任一条目发现不妥可单独 revert
- **向后兼容**：条目 3 子状态对既有命令守卫透明（不感知子状态的命令视作 `构建中`）

---

## 9. 验收标准（10 项）

1. `writing-plans.mdc` 新增「FetchContent 网络代理守卫」段，完整 6 点内容（触发条件 / 检测 / 补设 / Plan Phase 0 固化 / Archive 决策 / 交叉引用）
2. `van.md` 步骤 1 新增「FetchContent 代理状态检查」子项，grep 触发 + 自动检查 + 用户确认补设流程完整
3. `techContext.md`「FetchContent 与代理」段尾部追加「Plan/VAN 阶段守卫」子段，包含 3 处交叉引用
4. `writing-plans.mdc` §5.4 末尾追加「前置要求：工具链可用性 grep」子块，含 7 常见工具清单 + 4 替代方案表
5. `complexity-levels.mdc` L66 后新增「多轮次 Build 中间态」段，含触发条件 / 子状态定义 / 阶段守卫调整 / 提交约束 / 交叉引用
6. `build.md` §6 和 §7 之间新增 §6.5「轮次完成判断」，3 分支明确
7. `reflect.md` §0 前置守卫放宽为「`构建中` 或 `构建中·轮次 N 完成`」
8. 3 条反例追溯表全部 ✅（6/6 case 若规则当时生效均可捕获/预防）
9. YAML frontmatter 完整性检查通过（每个 .mdc 输出 2）
10. 交叉引用 grep 命中 3 处（`writing-plans.mdc` / `van.md` / `techContext.md`）

---

**设计批准：** 用户已确认全部范围 + 占位符模式（2026-04-19 Plan 阶段）