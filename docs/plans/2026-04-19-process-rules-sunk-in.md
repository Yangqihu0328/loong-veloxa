# 流程规则 P0/P1 沉淀冲刺 实现计划

**目标：** 一次性闭环 3 条积压流程规则条目（1 P0 + 2 P1），使它们从「反思建议」升级为「可执行守卫 + 可追溯文档」

**架构：** 纯文档规则改造，通过 `.cursor/rules/*.mdc` + `.cursor/commands/*.md` + `memory-bank/techContext.md` **多点联动** 实现「规则定义 → 守卫执行 → 地址源」的三段式引用链。采用「追加 / 插入新兄弟段」而非「改写既有段」的无破坏模式。

**技术栈：** Markdown / YAML frontmatter（cursor mdc 规则系统）

**复杂度级别：** Level 2

---

## 全局约束

- **基线分支：** `main`（commit `b5e97cf`）
- **工作分支：** `feature/TASK-20260419-13-process-rules-sunk-in`（VAN 已建）
- **无 TDD**：纯文档规则类任务，以「反例追溯表」作为事后测试替代（见 spec §4.1）
- **每 Phase 1 commit**（D3 决策），commit subject 遵循 `type(scope): [task-id] <summary>` 格式
- **占位符模式**：规则文件**严禁**硬编码开发环境 IP（D1 决策）

## 文件结构

### 新增 / 修改清单

| 操作 | 路径 | 职责 | 共享/专属 |
|:-:|---|---|:-:|
| 修改 | `.cursor/rules/skills/writing-plans.mdc` | +新段 `FetchContent 网络代理守卫` (P1) + §5.4 末尾工具链 grep 子块 (P2) | **共享** |
| 修改 | `.cursor/rules/workflow/complexity-levels.mdc` | +新段 `多轮次 Build 中间态` (P3) | **共享** |
| 修改 | `.cursor/commands/van.md` | 步骤 1 新增 `FetchContent 代理状态检查` 子项 (P1) | **共享** |
| 修改 | `.cursor/commands/build.md` | §6 和 §7 之间新增 §6.5 `轮次完成判断` (P3) | **共享** |
| 修改 | `.cursor/commands/reflect.md` | §0 前置守卫放宽 (P3) | **共享** |
| 修改 | `memory-bank/techContext.md` | `FetchContent 与代理` 段尾部追加 `Plan/VAN 阶段守卫` 子段 (P1) | 专属 |
| 修改 | `memory-bank/activeContext.md` | 长期项 3 条标记「已落实」(P4) | 专属 |
| 修改 | `memory-bank/tasks.md` / `progress.md` | Phase 进度同步 | 专属 |

**总共：** 8 个文件修改，0 个新建。

### 设计文档参考

- 📋 `docs/specs/2026-04-19-process-rules-sunk-in-design.md` — 设计规格（已批准）

---

## 测试模式

所有任务均为 **[规则文档 / 无代码 / 反例追溯]** 模式 — 规则文案写完后用相关历史任务回溯验证「若规则当时生效，是否会捕获问题」，有效性通过 ≥ 3/3 反例追溯 case 全通过来证明。

---

## Phase 0：基线验证（5 min）

### 任务 0.1：锚点 grep + YAML frontmatter 基线

**文件：** 无修改，仅读取

- [ ] **步骤 0.1.1：** grep 3 条规则的插入锚点未漂移
  ```bash
  rg -n "FetchContent C 子项目编译选项审计" .cursor/rules/skills/writing-plans.mdc
  # 期待：line 81 附近，既有段完整
  rg -n "Bench Smoke 验收三件套" .cursor/rules/skills/writing-plans.mdc
  # 期待：line 203 附近
  rg -n "迭代机制" .cursor/rules/workflow/complexity-levels.mdc
  # 期待：line 66 附近
  rg -n "### 1\. 检测环境" .cursor/commands/van.md
  # 期待：line 25 附近
  ```

- [ ] **步骤 0.1.2：** 记录 6 个目标文件当前 line count + YAML frontmatter 完整性到 `progress.md`
  ```bash
  for f in .cursor/rules/skills/writing-plans.mdc \
           .cursor/rules/workflow/complexity-levels.mdc \
           .cursor/commands/van.md \
           .cursor/commands/build.md \
           .cursor/commands/reflect.md \
           memory-bank/techContext.md; do
    echo "$f: $(wc -l < "$f") lines, $(head -5 "$f" | grep -c '^---$') frontmatter markers"
  done
  ```
  预期：每个 `.mdc` 输出 `2` frontmatter markers，`.md` 输出 `0` 或 `2`

- [ ] **步骤 0.1.3：** 不提交（Phase 0 属于基线读取，不产出 commit）；若 grep 输出与 spec 不符 → 停下报告

---

## Phase 1：条目 1 P0 — FetchContent 网络代理守卫（25-30 min）**[共享文件 3]**

### 任务 1.1：`writing-plans.mdc` 新增「FetchContent 网络代理守卫」段

**文件：**
- 修改：`.cursor/rules/skills/writing-plans.mdc`

- [ ] **步骤 1.1.1：** 在 L94（既有 `FetchContent C 子项目编译选项审计` 段末尾）之后、现有 L96 `## 测试基础设施审计` 段之前插入新段

  精确插入内容（见 spec §3.1.1 完整文案）：

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

  3. **补设代理（VAN 阶段强制）**：
     开发环境代理地址的**单一真相来源**是 `memory-bank/techContext.md`「FetchContent 与代理」段。规则文件中**严禁硬编码** IP，用占位符：
     ```bash
     # 从 techContext.md 读取实际地址，替换占位符后执行
     git config --global http.proxy <开发环境代理地址>
     git config --global https.proxy <开发环境代理地址>
     ```
     执行后再次 `git config --global --get http.proxy` 确认生效并记录到 `progress.md`。

  4. **Plan Phase 0 固化**：
     plan 文档必须包含 Phase 0「代理状态核验」步骤，执行 grep 并把输出作为制品写入 `progress.md`，以便 `/reflect` 阶段回溯。

  5. **Archive 阶段决策**：
     `/archive` 阶段询问用户：保留代理 / unset (`git config --global --unset http.proxy`)。默认**保留**（下一任务多半也需要）。

  6. **交叉引用**：
     - 开发环境代理地址：`memory-bank/techContext.md`「FetchContent 与代理」段
     - VAN 命令守卫：`.cursor/commands/van.md` 步骤 1「FetchContent 代理状态检查」子项
  ```

- [ ] **步骤 1.1.2：** 验证插入正确性
  ```bash
  # 1. YAML frontmatter 完整
  head -5 .cursor/rules/skills/writing-plans.mdc | grep -c '^---$'
  # 期待：2

  # 2. 新段位置正确（应在 FetchContent C 子项目编译选项审计 与 测试基础设施审计 之间）
  rg -n "^## " .cursor/rules/skills/writing-plans.mdc | head -15
  # 期待：看到 "## FetchContent 网络代理守卫" 紧跟在
  #      "## FetchContent C 子项目编译选项审计" 之后

  # 3. 代码块三反引号完整
  rg -c '^```' .cursor/rules/skills/writing-plans.mdc
  # 期待：偶数（所有代码块都成对关闭）
  ```

### 任务 1.2：`van.md` 步骤 1 新增 FetchContent 代理状态检查子项

**文件：**
- 修改：`.cursor/commands/van.md`

- [ ] **步骤 1.2.1：** 在步骤 1 的最后一个子项「如果 git 不可用，跳过 git 相关步骤并记录」之后追加新子项

  完整新子项文案（见 spec §3.1.2）：

  ```markdown
  - **FetchContent 代理状态检查**（如项目含 `FetchContent_Declare`）：
    - grep 验证：`rg "FetchContent_Declare" --type cmake`（命中即触发检查）
    - 运行 `git config --global --get http.proxy`
      - 有输出 → 记录到初始化报告并继续
      - 空输出且环境为 Cursor 沙箱 / WSL → **触发 `writing-plans.mdc`「FetchContent 网络代理守卫」段 步骤 3**，从 `memory-bank/techContext.md`「FetchContent 与代理」段读取开发环境代理地址，询问用户确认后执行 `git config --global http.proxy <地址>`
    - 如 `_deps/` 已完整离线预置 → 跳过检查
  ```

- [ ] **步骤 1.2.2：** 验证插入正确性
  ```bash
  rg -A 10 "### 1\. 检测环境" .cursor/commands/van.md | head -20
  # 期待：子项列表以 "FetchContent 代理状态检查" 结尾
  ```

### 任务 1.3：`techContext.md`「FetchContent 与代理」段尾部追加交叉引用子段

**文件：**
- 修改：`memory-bank/techContext.md`

- [ ] **步骤 1.3.1：** 读取当前段尾位置
  ```bash
  rg -n "FetchContent 与代理|## " memory-bank/techContext.md | rg -A 100 "FetchContent 与代理" | head -30
  ```
  定位「FetchContent 与代理」段结束到下一个 `##` 段之间的最后一个子段尾部

- [ ] **步骤 1.3.2：** 在该段尾部追加子段

  ```markdown
  ### Plan/VAN 阶段守卫

  规则文件 `.cursor/rules/skills/writing-plans.mdc` 的「FetchContent 网络代理守卫」段要求每个 FetchContent 任务在 VAN / Plan Phase 0 检查并（必要时）补设 git 全局代理；`.cursor/commands/van.md` 步骤 1 已有对应自动检查子项。本段是代理地址的**单一真相来源**，规则文件中**严禁硬编码** IP 地址，统一用占位符 `<开发环境代理地址>`。
  ```

- [ ] **步骤 1.3.3：** 验证 3 处交叉引用链路互通
  ```bash
  rg -n "FetchContent 网络代理守卫" \
    .cursor/rules/skills/writing-plans.mdc \
    .cursor/commands/van.md \
    memory-bank/techContext.md
  # 期待：3 处命中（writing-plans.mdc 定义 + van.md 引用 + techContext.md 引用）
  ```

### 任务 1.4：反例追溯验证（4 个 FetchContent 任务）

- [ ] **步骤 1.4.1：** 写追溯表到 `progress.md`

  | 任务 | 问题 | 若 P0 规则当时生效 | 结论 |
  |---|---|---|:-:|
  | TASK-20260413-01 QuickJS 接入 | 首次 FetchContent 超时挂死 | Phase 0 grep `http.proxy` 为空 → VAN 阶段补设 | ✅ 预防 |
  | TASK-20260419-02 Google Benchmark 接入 | 首次 FetchContent 超时挂死 | 同上 | ✅ 预防 |
  | TASK-20260419-04 `-Warray-bounds` 修复 | FetchContent 不触发但误判触发反复检查 | 规则跳过条件（已有 `_deps/`）→ 不触发 | ✅ 无误报 |
  | TASK-20260419-07 `-Werror` 修复 | 同上 | 同上 | ✅ 无误报 |

  **结果：4/4 ✅**

### 任务 1.5：提交 Phase 1

- [ ] **步骤 1.5.1：** 更新 `progress.md` 与 `tasks.md`（Phase 1 完成标记）

- [ ] **步骤 1.5.2：** 提交
  ```bash
  git add .cursor/rules/skills/writing-plans.mdc \
          .cursor/commands/van.md \
          memory-bank/techContext.md \
          memory-bank/progress.md \
          memory-bank/tasks.md
  git commit -m "docs(rules): TASK-20260419-13 P1 FetchContent proxy guard (P0)"
  ```

---

## Phase 2：条目 2 P1 — smoke 工具链 grep（15 min）**[共享文件 1]**

### 任务 2.1：`writing-plans.mdc` §5.4 末尾追加工具链 grep 子块

**文件：**
- 修改：`.cursor/rules/skills/writing-plans.mdc`

- [ ] **步骤 2.1.1：** 在 §5.4「Bench Smoke 验收三件套」最后一项（JSON 输出校验 items_per_second > 0）之后、下一个 `### 5.` 子节（RangeMultiplier 案例数预估）之前插入新子块

  精确插入内容（见 spec §3.2.1 完整文案）：

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

  | 缺失工具 | 常见替代 |
  |---------|---------|
  | `jq` | `python3 -c "import json,sys; ..."` heredoc |
  | `bc` | `python3 -c "print(...)"` 或 `awk 'BEGIN{...}'` |
  | `xmllint` | `python3 -c "import xml.etree.ElementTree as ET; ..."` |
  | `valgrind` | `sanitizers` (ASan/UBSan) |

  **反例（TASK-20260419-11 P3）：** plan §3 写 `jq '.benchmarks[] ...'` 做 smoke 自检，沙箱实际无 jq，Build 阶段临时改 python heredoc 损耗 ~3 min；本属"前置依赖未验证"反复模式，核心 API 已 5 处 grep 抑制，smoke 工具链漏验。
  ```

- [ ] **步骤 2.1.2：** 验证段号未破坏
  ```bash
  rg -n "^### [0-9]\." .cursor/rules/skills/writing-plans.mdc
  # 期待：§5.1 ... §5.6 段号序列完整，新子块是 §5.4 内部 H4（####）不占段号
  ```

### 任务 2.2：反例追溯验证

- [ ] **步骤 2.2.1：** 写追溯表到 `progress.md`

  | 任务 | 问题 | 若 P1 规则当时生效 | 结论 |
  |---|---|---|:-:|
  | TASK-20260419-11 P3 | plan 用 `jq` 做 smoke 自检，沙箱无 jq → 临时改 python 损 3 min | Plan 阶段 grep `command -v jq` 发现 MISS → plan 直接写 python heredoc | ✅ 预防 |

  **结果：1/1 ✅**

### 任务 2.3：提交 Phase 2

- [ ] **步骤 2.3.1：** 更新 `progress.md` 与 `tasks.md`

- [ ] **步骤 2.3.2：** 提交
  ```bash
  git add .cursor/rules/skills/writing-plans.mdc \
          memory-bank/progress.md \
          memory-bank/tasks.md
  git commit -m "docs(rules): TASK-20260419-13 P2 smoke toolchain grep (P1)"
  ```

---

## Phase 3：条目 3 P1 — 多轮次 Build 中间态（30-35 min）**[共享文件 3]**

### 任务 3.1：`complexity-levels.mdc` 新增「多轮次 Build 中间态」段

**文件：**
- 修改：`.cursor/rules/workflow/complexity-levels.mdc`

- [ ] **步骤 3.1.1：** 在 L66 Level 4 「迭代机制」段末尾之后、`## 复杂度决策树` 段之前插入**跨级别**新段

  精确插入内容（见 spec §3.3.1 完整文案）：

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

- [ ] **步骤 3.1.2：** 验证段落结构
  ```bash
  rg -n "^## " .cursor/rules/workflow/complexity-levels.mdc
  # 期待：看到 "## 多轮次 Build 中间态" 紧跟在 "## 级别定义" 之后、
  #       "## 复杂度决策树" 之前
  ```

### 任务 3.2：`build.md` §6 和 §7 之间新增 §6.5

**文件：**
- 修改：`.cursor/commands/build.md`

- [ ] **步骤 3.2.1：** 读取当前 §6 和 §7 位置
  ```bash
  rg -n "^### [67]\." .cursor/commands/build.md
  ```

- [ ] **步骤 3.2.2：** 在 §6 末尾、§7 之前插入新 §6.5

  精确插入内容（见 spec §3.3.2 完整文案）：

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

### 任务 3.3：`reflect.md` §0 守卫放宽

**文件：**
- 修改：`.cursor/commands/reflect.md`

- [ ] **步骤 3.3.1：** 将 §0 第 1 个 bullet 从：
  ```markdown
  - 当前阶段必须是 `构建中`（由 `/build` 设置）
  ```
  改为：
  ```markdown
  - 当前阶段必须是 `构建中` 或 `构建中·轮次 N 完成`（由 `/build` 设置，后者详见 `.cursor/rules/workflow/complexity-levels.mdc`「多轮次 Build 中间态」段）
  ```

- [ ] **步骤 3.3.2：** 验证
  ```bash
  rg -n "构建中·轮次" .cursor/commands/reflect.md
  # 期待：1 处命中（§0 前置守卫）
  ```

### 任务 3.4：反例追溯验证

- [ ] **步骤 3.4.1：** 写追溯表到 `progress.md`

  | 任务 | 问题 | 若 P1 规则当时生效 | 结论 |
  |---|---|---|:-:|
  | TASK-20260419-03 Round 1（10 phase / 2 轮）| 无法 checkpoint，用 `wip: blocked on TASK-XX` 绕过 | 用 `chore(build): TASK-03 round 1 complete - phase 1-5 green` subject + `构建中·轮次 1 完成` 状态 | ✅ 预防 |
  | TASK-20260419-04 多 phase noinline 修复 | 子 phase 完成难以汇报 | 可在 `构建中·轮次 N 完成` 状态停下等用户审查 | ✅ 改善 |

  **结果：2/2 ✅**

### 任务 3.5：提交 Phase 3

- [ ] **步骤 3.5.1：** 更新 `progress.md` 与 `tasks.md`

- [ ] **步骤 3.5.2：** 提交
  ```bash
  git add .cursor/rules/workflow/complexity-levels.mdc \
          .cursor/commands/build.md \
          .cursor/commands/reflect.md \
          memory-bank/progress.md \
          memory-bank/tasks.md
  git commit -m "docs(rules): TASK-20260419-13 P3 multi-round build state (P1)"
  ```

---

## Phase 4：收尾（10 min）

### 任务 4.1：`activeContext.md` 长期项段标记 3 条「已落实」

**文件：**
- 修改：`memory-bank/activeContext.md`

- [ ] **步骤 4.1.1：** 将「长期项（按优先级）」段中以下 3 条改为划线已落实样式

  - P0 FetchContent proxy（反复 9+ 次）：
    ```
    ~~**🔴 P0（紧急升级，反复 9+ 次，TASK-07 已验有效预防）：** ...~~ → ✅ **已于 TASK-20260419-13 /archive 阶段落实**：`writing-plans.mdc`「FetchContent 网络代理守卫」段 + `.cursor/commands/van.md` 步骤 1 子项 + `techContext.md` 交叉引用
    ```

  - P1 smoke 工具链 grep（TASK-11 反思 #2）：
    ```
    ~~**P1（新增, TASK-11 反思 #2）：** Plan 阶段需 grep `which <tool>` ...~~ → ✅ **已于 TASK-20260419-13 /archive 阶段落实**：`writing-plans.mdc` §5.4 末尾「前置要求：工具链可用性 grep」子块
    ```

  - P1 多轮次 Build（首发 TASK-03 Round 1，复确 3 次）：
    ```
    ~~**P1（已确认，本任务整体回顾再次复确）：** Level 2+ 多 phase 任务（phase 数 ≥ 5）需要支持「轮次完成」中间态 ...~~ → ✅ **已于 TASK-20260419-13 /archive 阶段落实**：`complexity-levels.mdc`「多轮次 Build 中间态」段 + `.cursor/commands/build.md` §6.5 + `.cursor/commands/reflect.md` §0 守卫放宽
    ```

  - 同时将「P1（已确认）：WIP / 中间 commit 的 subject 严禁包含外部任务状态字样」条目标记已落实（本次 `complexity-levels.mdc` 的「提交约束」段已收纳此规则）

### 任务 4.2：整体 self-review

- [ ] **步骤 4.2.1：** `git diff main...HEAD --stat` 确认修改范围
  ```bash
  git diff main...HEAD --stat
  # 期待：8 个文件（6 规则/命令 + 2 MB）+ 1 spec + 1 plan = 10 个
  # 总增行数 < 500
  ```

- [ ] **步骤 4.2.2：** 交叉引用一致性最终 grep
  ```bash
  rg "FetchContent 网络代理守卫" --type-add 'mdc:*.mdc' -t mdc -t md .cursor/ memory-bank/techContext.md
  # 期待：≥ 3 处命中
  rg "多轮次 Build" --type-add 'mdc:*.mdc' -t mdc -t md .cursor/ memory-bank/
  # 期待：≥ 3 处命中
  rg "构建中·轮次" --type-add 'mdc:*.mdc' -t mdc -t md .cursor/ memory-bank/
  # 期待：≥ 3 处命中（complexity-levels.mdc 定义 + build.md 引用 + reflect.md 引用）
  ```

- [ ] **步骤 4.2.3：** YAML frontmatter 完整性最终检查
  ```bash
  for f in .cursor/rules/skills/writing-plans.mdc \
           .cursor/rules/workflow/complexity-levels.mdc; do
    head -5 "$f" | grep -c '^---$'
    # 期待：每个输出 2
  done
  ```

- [ ] **步骤 4.2.4：** 验收标准 10 项逐条对照（见 spec §9）

### 任务 4.3：提交 Phase 4

- [ ] **步骤 4.3.1：** 更新 `tasks.md` 状态 → `🟢 构建完成`；`progress.md` Build milestone 标记 completed

- [ ] **步骤 4.3.2：** 提交
  ```bash
  git add memory-bank/activeContext.md \
          memory-bank/progress.md \
          memory-bank/tasks.md
  git commit -m "chore(build): TASK-20260419-13 finalize memory bank state"
  ```

---

## 完成验证（Build 阶段全量）

- [ ] **CMD 1：** `git log --oneline feature/TASK-20260419-13-process-rules-sunk-in ^main`
  - 期待：5-6 commits（VAN `ec78f1c` + Plan `[即将]` + P1 + P2 + P3 + P4 final）

- [ ] **CMD 2：** 10 项验收标准逐条通过（spec §9）

- [ ] **CMD 3：** 反例追溯表总计 7/7 ✅（4 proxy + 1 smoke + 2 multi-round）

- [ ] **CMD 4：** `git status` working tree 干净

- [ ] **CMD 5：** 5 个规则/命令文件 + 1 `techContext.md` 修改行数 < 500 总计

---

## 安全任务

**无** — 本任务不涉及外部输入、认证、数据存储、部署配置。规则文件中代理地址严格占位符化（D1 决策落实）。

---

## 依赖安全审计

**无新增依赖** — 纯 Markdown 规则修改，无 vcpkg / pip / npm 变动。

---

## 执行交接

保存计划后：

**"计划完成并保存到 `docs/plans/2026-04-19-process-rules-sunk-in.md`。准备执行？"**

→ 使用 `/build` 命令在当前会话中 TDD 替代（反例追溯模式）执行 Phase 0-4。

---

## 与 Memory Bank 集成

- `memory-bank/tasks.md` — Plan 阶段完成后写入任务计划结构
- `memory-bank/activeContext.md` — 阶段从 `初始化` → `规划中`
- `memory-bank/progress.md` — 记录 Plan milestone completed
