# 工作流/规则类技术债批量清理 实现计划

**目标：** 一次性清理 6 项 P1 工作流/规则类技术债，全部来自 TASK-20260503-01 + TASK-20260502-01 reflection §6/§7 沉淀，落实跨任务反复模式抑制器。

**架构：** 4 个目标文件批量修改 + 1 项全 codebase audit 文档化（无 fix 必要 — Phase 0 已预跑确认）；自我吃狗粮（A-P1#4 `git add -p` 范式应用于 C-#1/#2/#4 三段同文件 commit 拆分）。

**技术栈：** `.cursor/rules/skills/*.mdc`（writing-plans + git-workflow）+ `docs/specs/2026-04-30-devtool-design.md` + 全 codebase Grep audit + ctest 1247 baseline 不退化 verify。

**复杂度级别：** Level 2（多文件修改 / 需求清晰 / 纯文档调整 + 1 项 codebase audit / 无新代码逻辑变更 / 无 dogfood UI / 无 JS native binding / 无 example 扩展）

---

## §0 Phase 0 — 极简检查（B5 锁定 1 子段）

### §0.1 工具链与基础设施实测快照（C-#4 自我落实首版数据）

```bash
git status                                    # 期望：working tree clean
git symbolic-ref --short HEAD                 # 期望：feature/TASK-20260503-02-techdebt-workflow-cleanup
git log --oneline -3                          # 期望：head 是 main 55dea8f
gcc --version | head -1                       # 期望：gcc 15.2.0
ld --version | head -1                        # 期望：GNU ld 2.46（binutils 2026 — R12 已登记）
cmake --version | head -1                     # 期望：cmake 4.2.3
ninja --version                               # 期望：1.13.2
command -v rg || echo "MISS"                  # 期望：MISS（用 Grep tool 替代）
command -v grep                               # 期望：present（POSIX 系统兜底）
ctest --test-dir build -j 2>&1 | tail -3      # 期望：1247/1247 PASS（DEVTOOL=ON baseline）
```

**已知行为差异速查表（C-#4 入库基线）：**

| 工具 | 当前 | 已知行为差异（vs 上次任务） |
|---|---|---|
| binutils ld | 2.46 | 单次扫描静态归档严格化（破坏 `vx_core ↔ vx_script ↔ vx_devtool` 循环依赖）→ 已 hotfix `--start-group/--end-group`（commit `ddc1e3c`）|
| gcc | 15.2.0 | 无新发现 |
| cmake | 4.2.3 | 无新发现 |
| ninja | 1.13.2 | 无新发现 |

### §0.2 A-P1#6 audit 预跑结果固化

**全 codebase `.status()` 调用 audit（Grep tool）**：

```bash
# 等价 rg "StatusOr.*\.status\(\)" — 用 Grep tool 替代 rg
# 命中 6 处（5 文件，5 上下文）
```

| # | 文件:行 | 上下文 | 守卫模式 | 评估 |
|:-:|---|---|---|:-:|
| 1 | `veloxa/core/application.cc:113` | 单行 | `if (!result.ok()) return result.status();` | ✅ |
| 2 | `veloxa/core/application.cc:135` | 单行 | `if (!result.ok()) return result.status();` | ✅ |
| 3 | `veloxa/core/application.cc:348` | 三元 | `eval.ok() ? Status::Ok() : eval.status();` | ✅ 范本 |
| 4 | `veloxa/core/image/image_cache.cc:16` | 多行 | `if (!result.ok()) { return result.status(); }` | ✅ |
| 5 | `veloxa/devtool/hot_reload/file_watcher_inotify.cc:239` | 守卫块内 | `if (!resolved.ok()) { ... resolved.status().code() ... }` | ✅ |
| 6 | `veloxa/devtool/hot_reload/file_watcher_inotify.cc:242` | 守卫块内 | 同上块内 `resolved.status().message()` | ✅ |

**结论**：6/6 全部正确，零误用，零 fix 必要 → 任务 5 仅做文档化 + 留 P3 clang-tidy enforce。

### §0.3 工具可用性矩阵

| 工具 | 状态 | 兜底 |
|---|:-:|---|
| `rg` | ❌ MISS | Grep tool（已使用）|
| `grep -rn` | ✅ present | — |
| `git add -p` | ✅ present | — |
| Read / StrReplace / Write tool | ✅ present | — |
| ctest | ✅ present | — |

---

## §1 文件结构 + B1-B8 决策锁定

### §1.1 修改文件清单

| # | 文件 | 改动 | 原长度 | 预增 LOC |
|:-:|---|---|:-:|:-:|
| 1 | `.cursor/rules/skills/writing-plans.mdc` | 新增 3 段（C-#1 + C-#2 + C-#4）| ~820 | +60-80 |
| 2 | `.cursor/rules/skills/git-workflow.mdc` | 新增 1 段（A-P1#4）| ~173 | +25-35 |
| 3 | `docs/specs/2026-04-30-devtool-design.md` | 新增 1 附录段（A-P1#8）| - | +30-40 |
| 4 | `memory-bank/techContext.md` | 新增 audit 结论摘要（A-P1#6）| - | +10-15 |

**零代码逻辑变更**（B4 文档调整模式）— 无新增 .h/.cc/CMakeLists.txt 修改。

### §1.2 B1-B8 决策表（用户 1 次 AskQuestion 选 all_recommended）

| # | 维度 | 锁定 | 理由速记 |
|:-:|---|---|---|
| B1 | writing-plans 3 项 commit 粒度 | **3 commits**（每项独立）| 自我吃 A-P1#4 `git add -p` 狗粮 — 任务 4 的范式应用于任务 1-3 |
| B2 | A-P1#6 audit 输出 | **audit only + 0 fix** | Phase 0 §0.2 预跑确认 6/6 ✅ |
| B3 | 子任务执行顺序 | **按文件分组**（writing-plans 3 → git-workflow → audit → spec）| 同文件 batch + writing-plans 在前避免 audit 时切换 mental model |
| B4 | 测试模式 | **[文档调整模式]**（新模式）| TDD 不适用文档；改 git diff 可见 + ctest 1247 不退化 verify |
| B5 | Phase 0 检查 | **极简 1 子段** | Level 2 + 纯文档零代码 = 极简 |
| B6 | Checkpoint | **CP1（任务 1-3 后）+ CP2（任务 5 后）** | CP1 自审段落语义连贯 + 过度工程检测；CP2 自审 audit 范围是否需扩到 build_*_test_* |
| B7 | plan 估时 | **~85-100 min plan ×0.6 → 实测 ~50-70 min 期待** | 极窄档延续假设第 7 数据点群组（继 Phase A/B/C 后）|
| B8 | 反复模式记录 | **C-#2 标注「第二次同类反复 → 必须落实」** | reflect 阶段统计反复抑制有效性 |

### §1.3 测试模式约定（B4 锁定 [文档调整模式]）

```markdown
### 任务 N：[文件名] [文档调整]

- [ ] **步骤 1**：StrReplace / Write 添加新段
- [ ] **步骤 2**：Read 验证段落正确插入 + ReadLints 检查 markdown 无错误
- [ ] **步骤 3**：（仅 audit 任务）跑 Grep 命令验证 audit 结果与 §0.2 一致
- [ ] **步骤 4**：`git add -p <file>` 选择性 stage（B1 自我吃狗粮）
- [ ] **步骤 5**：`git diff --cached` 验证 staged 仅含本子任务变更
- [ ] **步骤 6**：commit（消息见 §6）
```

---

## §2 子任务清单（6 子任务 — 按 B3 顺序）

### 任务 1：writing-plans 新增「公开 API testability 检查清单」段（C-#1）[文档调整]

**文件：** 修改 `.cursor/rules/skills/writing-plans.mdc`

**插入位置：** 「文件结构」段后（line ~30，「CMake 链接方向约束分析」段前）

**插入内容（完整）：**

```markdown
## 公开 API testability 检查清单（涉及新增公开 C ABI / public API 时**必填**）

> 反复出现升级 P1（TASK-20260503-01 C-#1 实证）：C.4.2 dogfood smoke 需要 `vx_view_hot_reload_tracked_count` 才能验证「count=1」契约，plan 阶段未识别该 testability 接口需求 → build §C.5.2 finalize 才补加 → 1 行 ABI 加 1 commit 但破坏 plan 原子性。

**触发条件**：本次 plan 涉及以下任一**必填**：

- 新增公开 C ABI 函数（`vx_*` / `VX_*` 前缀，对外可见）
- 新增公开 C++ class / namespace API（`veloxa::*` 头部 export）
- 扩展既有公开 API 行为（新增参数 / 新增返回字段 / 新增错误码）

**强制产出 testability 接口需求清单**：

| 维度 | 问题 | 接口需求 | 落实子任务 |
|---|---|---|---|
| **smoke 可观测** | 该 API 的副作用可被 `examples/<name>` smoke 测试观测吗？ | 如 hot_reload 触发计数 → `vx_view_hot_reload_tracked_count()` getter | 写 ctest smoke 时同步加 |
| **dogfood 可达** | 该 API 的状态可被 DevTool / inspector / debugger 读取吗？ | 如 hot_reload status → `vx_devtool_get_hot_reload_status()` JS binding | DevTool UI 子任务前置 |
| **observability 可断言** | 该 API 的内部状态可被 ctest / valgrind / log assert 验证吗？ | 如 last_error 字段 → 单测 `EXPECT_TRUE(mgr.last_error().has_value())` | unit test 子任务覆盖 |

**判读规则**：

- 任一维度回答「否」→ plan 必须补「testability 子任务」前置（getter / binding / log accessor 暴露）
- 三维度均「是」→ plan 标注「testability OK」并在验收要点段重复 ✅

**反模式**：

- ❌ plan 仅写「实现 API X」未列 testability 清单 → build 阶段 dogfood smoke 才发现缺 getter
- ❌ 把 internal-only `friend class XxxTest;` 混淆为公开 API testability — 内部 friend 不暴露给 example/dogfood/observability 工具
- ❌ 假设「下一个任务再加」— testability 接口与 API 同子任务设计才能保证 commit 原子性

**TASK-20260503-01 实证**：plan §C.4.1 设计 `vx_view_attach_devtool` + `hot_reload_dir` 参数，未识别 dogfood smoke 需要「reload 触发次数 getter」→ §C.4.2 才追加 `vx_view_hot_reload_tracked_count()` 1 行 ABI + 1 commit；如 plan 阶段三问 testability 清单则可前置到 C.4.1 同 commit。

**交叉引用**：`memory-bank/systemPatterns.md`「lazy-attach C ABI 容错模式」段（同源 C ABI 设计模式）；`.cursor/rules/skills/test-driven-development.mdc`（testability 是 TDD 前置条件）。
```

**步骤 1**：StrReplace 在「文件结构」段后插入新段
**步骤 2**：Read 行 30-100 验证段落正确插入 + 标题层级（##）一致
**步骤 3**：ReadLints `.cursor/rules/skills/writing-plans.mdc` 检查 markdown 无错误
**步骤 4**：`git add -p .cursor/rules/skills/writing-plans.mdc` 选择性 stage 仅本任务变更
**步骤 5**：`git diff --cached` 验证 staged 仅含 testability 段（不含 C-#2/#4 段）
**步骤 6**：

```bash
git commit -m "$(cat <<'EOF'
docs(writing-plans): add public API testability checklist (C-#1)

Source: TASK-20260503-01 reflection §7 P1 #1.
Closes the gap where C.4.2 dogfood smoke needed
vx_view_hot_reload_tracked_count() but was not identified at plan stage.
EOF
)"
```

**预估**：~10-15 min plan ×0.6 → 实测 ~6-9 min

---

### 任务 2：writing-plans 新增「ctest 数量预期 config 矩阵」段（C-#2 ⚠️ 第二次反复）[文档调整]

**文件：** 修改 `.cursor/rules/skills/writing-plans.mdc`

**插入位置：** 「测试基础设施审计」段后（约行 ~135）

**反复模式标注**：⚠️ **第二次同类反复**（与 TASK-20260502-02 P1 #2 同源）— 本次必须落实，否则进入「3 次反复 = P0 升级」轨道。

**插入内容（完整）：**

```markdown
## ctest 数量预期 config 矩阵（plan 验收段写「期望 ctest N PASS」时**必填**）

> 反复出现 P1（TASK-20260502-02 P1 #2 + TASK-20260503-01 C-#2 第二次同类反复升 P1 警戒线）：plan §验收写「期望 ctest 1228+」未明示「DEVTOOL=ON + SDL2=ON + Benchmarks=ON 矩阵」假设；实测 1247 是 SDL2/Benchmarks=OFF 的活跃测试集（DEVTOOL=ON），属正常配置差额，但 plan vs 实测 偏差产生疑似回归恐慌 → 浪费 reflect 阶段澄清时间。

**触发条件**：plan / spec / reflection 中出现 ctest 数量声明（"期望 N PASS" / "+M cases" / "baseline N/N" 等）时**必填**。

**强制产出 ctest config 矩阵**：

```markdown
| Config | DEVTOOL | SDL2 | Benchmarks | 期望 ctest | 实测 ctest | diff |
|---|:-:|:-:|:-:|:-:|:-:|:-:|
| baseline | ON | OFF | OFF | 1195 | - | - |
| 本任务 ON path | ON | OFF | OFF | +N → 1195+N | - | - |
| OFF path（A14 验证）| OFF | OFF | OFF | -M（黑名单 M 测试）| - | - |
| SDL2 path（example smoke）| ON | ON | OFF | +K（含 hello_*_smoke）| - | - |
```

**判读规则**：

- 期望 vs 实测 diff = 0 → ✅ PASS，plan 假设正确
- diff = 配置差额（如 SDL2 套件未启用）→ ✅ 配置差额，plan 写明则非回归
- diff ≠ 0 且非配置差额 → 🔴 真实回归，触发 systematic-debugging skill

**最小标注模板**（plan 验收段必备 1 行）：

```markdown
**ctest 期望矩阵**：DEVTOOL=ON 1195+N / DEVTOOL=OFF 1082+0（A14 黑名单守门）/ SDL2=ON 1265+K（含 example smoke）
```

**反模式**：

- ❌ plan 写「期望 1228+」未明示配置矩阵 → 实测 1247 / 1265 / 1082 等数字时无法快速判定哪个是真 baseline
- ❌ 仅声明 baseline 数字未声明配置 → 跨任务跨 host 跨配置难以对照
- ❌ 把「ctest 1228 PASS」当作所有配置永真 — 配置矩阵差额可达 ±200 测试

**TASK-20260502-02 / TASK-20260503-01 双重实证**：

- TASK-20260502-02 P1 #2 首次出现：plan §B.5.2 验收写「DEVTOOL=ON 1228 / DEVTOOL=OFF 1082」未明示 SDL2/Benchmarks 配置 → reflect 阶段才补
- TASK-20260503-01 C-#2 第二次同源：plan §C.5.2 写「期望 1260+」未识别 SDL2 配置差 → 实测 1247（DEVTOOL=ON + SDL2=OFF）vs 1265（DEVTOOL=ON + SDL2=ON）反 plan 假设
- **第二次同源 = P1 警戒线** — 本规则首版即避免第三次同源

**交叉引用**：`memory-bank/systemPatterns.md`「ctest baseline 配置矩阵」段（待本任务 archive 阶段补）；`memory-bank/techContext.md`「CMake basic vs full 配置矩阵」段（VX_BUILD_BENCHMARKS + VX_PLATFORM_SDL2 配置语义说明）。
```

**步骤 1-6**：同任务 1 模板（StrReplace → Read → ReadLints → git add -p → git diff --cached → commit）

**commit 消息**：

```bash
docs(writing-plans): add ctest config matrix declaration (C-#2)

Source: TASK-20260502-02 P1 #2 + TASK-20260503-01 C-#2 (2nd recurrence).
Forces plans to declare DEVTOOL/SDL2/Benchmarks config matrix when
asserting expected ctest counts. Closes the recurrence loop before
3rd-occurrence P0 escalation.
```

**预估**：~10 min plan ×0.6 → 实测 ~6 min

---

### 任务 3：writing-plans 新增「toolchain 版本激进升级行为变化检查」段（C-#4）[文档调整]

**文件：** 修改 `.cursor/rules/skills/writing-plans.mdc`

**插入位置：** 「smoke 工具链可用性检查」段后（约行 ~365，紧邻 §0.10 工具链检查段语境）

**插入内容（完整）：**

```markdown
## Toolchain 版本激进升级行为变化检查（VAN/Plan Phase 0 §0.10 子段**必做**）

> 反复出现升级 P1 + R12 风险登记（TASK-20260503-01 §0.1 实证）：binutils 2.46（2026 升级）单次扫描静态归档严格化，破坏既有 `vx_core ↔ vx_script ↔ vx_devtool` 双重循环依赖 → 271 link target 中 ~6 测试 link FAILED → baseline 阻塞 hotfix 分离协议触发（feature 分支外 hotfix 分支独立修复 fast-forward main）。

**触发条件**：每次 VAN/Plan Phase 0 **必做** — 不论任务大小（成本 30 秒，收益避免 baseline 阻塞）。

**强制产出工具链版本快照表**：

```bash
gcc --version | head -1
ld --version | head -1                # 重点关注 binutils 版本（2.40+ 是激进期）
cmake --version | head -1
ninja --version
```

输出对比 `memory-bank/techContext.md`「环境就绪」段记录的上次任务版本。

**已知行为差异速查表**（每发现 1 项激进变化 → 增 1 行）：

| 工具 | 版本起 | 行为变化 | 影响 | 兜底 |
|---|---|---|---|---|
| binutils ld | 2.46 (2026) | 单次扫描静态归档严格化 | 破坏循环依赖 link | `--start-group ... --end-group` 包围循环组 |
| gcc | 14+ | (待登记) | - | - |
| cmake | 4.0+ | (待登记) | - | - |

**判读规则**：

- 版本与上次任务一致 → ✅ 跳过差异检查，记 1 行 progress
- 版本主版本号变化（如 2.45 → 2.46） → 🟡 必须 grep 已知差异表 + 跑 baseline ctest 验证
- 已知差异速查表未覆盖 + baseline FAIL → 🔴 触发「baseline 阻塞 hotfix 分离协议」（systemPatterns 段）

**反模式**：

- ❌ Phase 0 跳过工具链版本核对 → build 阶段第一步 ctest 阻塞才发现
- ❌ 工具链升级后默认「行为不变」— 激进版本（major bump 或 binutils 跨年版本）必须 grep 行为差异
- ❌ 工具链 baseline FAIL 在 feature 分支内修复 → 污染 feature 分支语义；正确做法是 hotfix 分支分离协议

**TASK-20260503-01 实证**：

- VAN §0.1 baseline 写「期望 1228 PASS」未做工具链版本核对 → build 阶段 binutils 2.46 严格化导致 ~6 测试 link FAIL
- 修复路径：`hotfix/binutils-2.46-link-group` 分支独立修复（10 行 CMake 加 `--start-group/--end-group`）→ fast-forward merge `ddc1e3c` → feature 分支 rebase
- 如 Phase 0 §0.10 子段先行 → 提前发现 binutils 2.46 与 2.45 的差异 → 直接在 plan 阶段加 hotfix 子任务，无 build 中断

**交叉引用**：

- `memory-bank/systemPatterns.md`「baseline 阻塞 hotfix 分离协议」段（hotfix 分支 → fast-forward main → feature rebase 链路）
- `memory-bank/systemPatterns.md`「R12 工具链版本激进升级风险」段
- `memory-bank/systemPatterns.md`「CMake 静态库循环依赖处理 — binutils 2.46+ 双循环依赖叠加场景覆盖性」段
```

**步骤 1-6**：同任务 1 模板

**commit 消息**：

```bash
docs(writing-plans): add toolchain version aggressive upgrade check (C-#4)

Source: TASK-20260503-01 reflection §7 P1 #4.
First-evidence: binutils 2.46 strict static archive scan broke
existing circular deps; led to baseline-blocking hotfix protocol.
Adds Phase 0 §0.10 sub-step to grep toolchain versions and check
known behavior diff table before build kicks off.
```

**预估**：~10 min plan ×0.6 → 实测 ~6 min

---

### 🛑 CP1 自审（B6 锁定）— 任务 1-3 完成后

**自审清单**：

- [ ] 三段标题层级一致（`##` 一级，子段 `###` / `####`）
- [ ] 三段「触发条件 / 强制产出 / 判读规则 / 反模式 / 实证 / 交叉引用」5 段结构齐整（与既有段保持一致）
- [ ] 三段没有过度工程（每段 ≤ 80 行，与既有段 ≤ 120 行同档）
- [ ] 三段交叉引用指向真实存在的 systemPatterns 段（或标注「待 archive 阶段补」）
- [ ] writing-plans.mdc 总长 ≤ 1100 行（原 ~820 + 新增 ~200-280 = ~1100 上限）
- [ ] 3 commits 各自 git log 单独可读 + 不交叉污染（self-test A-P1#4 范式）

**通过条件**：以上全 ✅ → 进入任务 4。

---

### 任务 4：git-workflow 新增「Multi-subtask commit 拆分」段（A-P1#4）[文档调整]

**文件：** 修改 `.cursor/rules/skills/git-workflow.mdc`

**插入位置：** 「提交频率」段后（约行 ~99，「Commit 前防御断言」段前）

**插入内容（完整）：**

```markdown
## Multi-subtask commit 拆分 — `git add -p` 推荐范式（多子任务连续完成时**必做**）

> 反复出现 P1（TASK-20260502-01 A.2.1/A.2.4/A.3.1 实证）：3 个连续子任务在同一文件 `tests/CMakeLists.txt` 累积变更，因 `git add tests/CMakeLists.txt` 而非 `git add -p` 导致 A.2.1 commit 隐含包含 A.2.4 + A.3.1 部分变更（功能正确，语义不洁）→ git bisect 定位回归时无法精确到子任务级。

**触发条件**：以下任一**必做**：

- 同一 plan 内 ≥ 2 个子任务修改同一文件（如 `CMakeLists.txt` / `Cargo.toml` / `package.json`）
- 子任务连续完成但中间未 commit（累积 working tree 含多任务变更）
- 大型重构 + 跨任务的同文件 hunk

**强制操作模板**：

```bash
# Step 1: working tree 状态自检
git status --short                            # 期望：仅本子任务相关文件
git diff <file>                                # 视觉确认 hunks

# Step 2: 选择性 stage（关键 — 拒绝整文件 git add）
git add -p <file>                              # 交互式选择 hunks
# 或直接路径过滤：git add path/to/specific/sub/file.cc

# Step 3: 验证 staged 内容
git diff --cached                              # 期望：仅含本子任务变更
git diff                                       # 期望：剩余 hunks 留给下一子任务

# Step 4: commit + 验证
git commit -m "..."                            # 含子任务 ID
git status --short                             # 期望：剩余 hunks 仍 unstaged

# Step 5: 下一子任务继续
# 重复 Step 2-4
```

**checklist（每次 commit 前必查）**：

- [ ] `git diff --cached` 仅含本子任务相关 hunks（无其他子任务的 stray hunk）
- [ ] commit 消息明确标注子任务 ID（`feat(scope): xxx (#N/M)` 或 `(C-#N)` 等）
- [ ] commit 后 `git status` 显示剩余 hunks unstaged 等待下一子任务
- [ ] 如必须把多子任务 hunk 合并到一个 commit → commit 消息明确说明范围（"includes A.2.1 + A.2.4 due to circular CMake dep"）

**反模式**：

- ❌ `git add <file>` 整文件 stage — 隐含累积所有子任务变更
- ❌ commit 消息只写最后一个子任务 ID — 实际 hunk 跨多任务
- ❌ 用 `git stash push --keep-index` 隔离再 commit — 引入额外操作易出错（`git add -p` 是首选）

**实证**：

- TASK-20260502-01 A.2.1 commit `feat(devtool): add inspector_panel CMake target` 实际隐含 A.2.4 + A.3.1 部分 `tests/CMakeLists.txt` 变更 → reflect 阶段 §6 P1 #4 标 P1 改进
- 反例：TASK-20260503-01 11 个子任务 11 commits 均严格 1 commit/子任务 — 因子任务文件无重叠，未触发本场景

**交叉引用**：

- `memory-bank/systemPatterns.md`「commit 颗粒度」段
- `.cursor/rules/skills/git-workflow.mdc` 本文件「Level 适配」段（Level 4 强制按子系统提交，本规则是 Level 2-4 通用前置）
```

**步骤 1-6**：同任务 1 模板

**commit 消息**：

```bash
docs(git-workflow): add multi-subtask commit splitting via git add -p (A-P1#4)

Source: TASK-20260502-01 reflection §6 P1 #4.
Forces git add -p (not git add <file>) when multiple subtasks touch
the same file, preventing implicit cross-subtask hunks that pollute
git bisect granularity. Self-applied by tasks 1-3 of this plan.
```

**预估**：~10-15 min plan ×0.6 → 实测 ~6-9 min

---

### 任务 5：StatusOr<T>::status() codebase audit 文档化（A-P1#6）[文档调整 — 无 fix]

**文件：** 修改 `memory-bank/techContext.md`

**插入位置：** 「Status / StatusOr 使用规范」段（既有段，约行 820+）末尾追加 audit 结论小段

**任务范围**：仅文档化 Phase 0 §0.2 已固化的 audit 结果（6/6 ✅），并标注 P3 候选（clang-tidy enforce）。

**插入内容（完整）：**

```markdown
### TASK-20260503-02 StatusOr<T>::status() codebase audit 结果（2026-05-03）

**Audit 命令**（Grep tool 替代 `rg`，POSIX `grep` 兜底亦可）：

```bash
# 等价 rg "StatusOr.*\.status\(\)" + 等价 rg "\.status\(\)" 双重 sweep
grep -rn "\.status()" veloxa/ --include="*.cc" --include="*.h"
```

**结果**（6/6 ✅ 全部正确，零误用）：

| # | 文件:行 | 守卫模式 | 评估 |
|:-:|---|---|:-:|
| 1 | `veloxa/core/application.cc:113` | `if (!result.ok()) return result.status();` | ✅ |
| 2 | `veloxa/core/application.cc:135` | `if (!result.ok()) return result.status();` | ✅ |
| 3 | `veloxa/core/application.cc:348` | `eval.ok() ? Status::Ok() : eval.status();` | ✅ 三元守卫范本 |
| 4 | `veloxa/core/image/image_cache.cc:16` | `if (!result.ok()) { return result.status(); }` | ✅ 多行守卫 |
| 5 | `veloxa/devtool/hot_reload/file_watcher_inotify.cc:239` | `if (!resolved.ok()) { ... .status().code() ... }` | ✅ 守卫块内合法 |
| 6 | `veloxa/devtool/hot_reload/file_watcher_inotify.cc:242` | 同 #5 块内 `.status().message()` | ✅ |

**结论**：

- 0 fix 必要 — A.1.8 修复后的三元守卫范本（#3）+ 既有 `if (!ok())` 前置守卫（#1/#2/#4）+ 守卫块内合法引用（#5/#6）已覆盖 codebase 全部 `.status()` 用例
- 无新引入误用风险点
- **P3 候选保留**：clang-tidy custom check enforce（编译时强制守卫规范）— 估时 ~1-2 h，待 codebase 整体 clang-tidy 化任务时合并执行（与 TASK-30-03-style review round 类型任务合流）

**audit 范式可复用**：本任务首次实证「plan Phase 0 阶段预跑 audit」模式 — audit 在 plan 阶段完成而非 build 阶段，避免「audit 发现需 fix → plan 不准 → 重新 plan」的低效循环。
```

**步骤 1**：StrReplace 在 `techContext.md` 「Status / StatusOr 使用规范」段末尾追加新小段
**步骤 2**：Read 验证段落正确插入
**步骤 3**：跑 audit 命令（Grep tool）验证当前结果与 §0.2 一致 — 若有新增 `.status()` 调用则更新表格
**步骤 4**：`git add memory-bank/techContext.md`
**步骤 5**：`git diff --cached` 验证仅含 audit 段
**步骤 6**：

```bash
git commit -m "$(cat <<'EOF'
docs(techContext): document StatusOr.status() codebase audit (A-P1#6)

Source: TASK-20260502-01 reflection §6 P1 #6.
Audit result: 6/6 PASS, zero misuse, zero fix needed. Phase 0 of
TASK-20260503-02 pre-ran the audit and confirmed A.1.8 fix +
existing if (!ok()) guards cover all .status() call sites.
clang-tidy enforce stays as P3 candidate.
EOF
)"
```

**预估**：~15-20 min plan ×0.6（原 30-60 min 因 0 fix 大幅缩短）→ 实测 ~10-15 min

---

### 🛑 CP2 自审（B6 锁定）— 任务 5 完成后

**自审清单**：

- [ ] audit 范围确实覆盖全 codebase（不仅 `veloxa/` 还需 `tests/` + `examples/` + `benchmarks/`）— 抽样验证
- [ ] audit 命令兜底 `grep -rn` 等价于 `rg` 输出
- [ ] 任务 5 的 commit 仅含 techContext.md 段，未污染其他文件
- [ ] P3 clang-tidy enforce 候选已迁移到 `activeContext.md` 的「待处理事项」段（archive 阶段最终落实）

**通过条件**：以上全 ✅ → 进入任务 6。

---

### 任务 6：spec 新增「附录 A: A14 解读」段（A-P1#8）[文档调整]

**文件：** 修改 `docs/specs/2026-04-30-devtool-design.md`

**插入位置：** §6「全局约束 A13-A14」段后（约行 67 附近）；或文档末尾作为「附录 A」（推荐 — 不破坏既有 §1-§13 主体结构）。

**插入内容（完整）：**

```markdown
## 附录 A：A14 解读 — 链接闭合 vs binary size diff 双层语义

> 来源：TASK-20260502-01 A.1.7/A.1.8 实证 + reflection §6 P1 #8 沉淀。

### A.1 A14 双层语义区分

A14 「DevTool 关闭时构建产物零变化（链接闭合 + binary size diff = 0）」实际包含两层独立语义：

| 层 | 名称 | 验证方法 | 严格度 | 失败后果 |
|:-:|---|---|:-:|---|
| **L1** | 链接闭合（Hard Constraint）| `tests/smoke/devtool_a14_link_closure.cmake` 用 `nm` 黑名单符号验证 DevTool 内部符号零泄漏到 `DEVTOOL=OFF` binary | 🔴 强制 | A14 违规，必须修复（#ifdef 守门 + CMakeLists.txt 条件 link） |
| **L2** | binary size diff（Soft Indicator）| `cmake --build` + `ls -l <binary>` size 对比 `git stash` 前后 | 🟡 软指示 | 由公开 stub 引起 → 合规；由 leak 引起 → 升级为 L1 违规 |

### A.2 L1 链接闭合（严格契约）

- 黑名单符号定义在 `tests/smoke/devtool_a14_link_closure.cmake`
- 每个 DevTool 子系统新增 1+ 符号到黑名单（如 Phase A 8 项 / Phase B 2 项 / Phase C 3 项）
- ctest 自动 nm 验证：`DEVTOOL=OFF` binary 中黑名单符号必须 0 命中
- **L1 PASS = A14 主契约满足**

### A.3 L2 binary size diff（软指示）

- A.1.7/A.1.8 实证：`vx_view_attach_devtool` always-fail stub（OFF 路径返 `VX_ERROR_INVALID_STATE`）+ `vx_view_get_perf_stats` 等 6 个 C ABI stub 累计 +4196 bytes 在 OFF binary 中
- 这些 bytes 不是 DevTool 实现 leak，而是**公开 C ABI 表面的 stub 占位**（保持 ABI 兼容性 — OFF binary 仍可 link 调用方代码，只是返错码）
- **L2 > 0 由公开 stub 引起 = 合规**；L2 > 0 由实现 leak 引起 = L1 违规升级

### A.4 区分公式

```
A14 严格 = L1 PASS（链接闭合）AND L2 ≥ 0（diff 由公开 stub 解释）
```

判读流程：

1. ctest A14 link closure smoke → L1 PASS/FAIL
2. L1 FAIL → 直接 A14 违规，修复 #ifdef + CMakeLists.txt
3. L1 PASS + L2 = 0 → 理想 A14（无新公开 stub）
4. L1 PASS + L2 > 0 → 验证 L2 增量来源：
   - `nm <binary> | grep vx_` 确认增量符号是公开 C ABI（前缀 `vx_`）
   - 公开 stub → 合规
   - 内部符号泄漏 → 升级为 L1 违规

### A.5 实证

- **TASK-20260502-01 Phase A**：A.1.7 + A.1.8 累计 +4196 bytes（490 + 3706）；L1 PASS（黑名单 8 项 nm 0 命中）；L2 增量全部来源 7 个公开 C ABI stub → 合规
- **TASK-20260502-02 Phase B**：黑名单 +2 项（PerfOverlay + InjectDirtyRectHighlights）；L1 PASS；L2 ~+800 bytes 来源新 stub → 合规
- **TASK-20260503-01 Phase C**：黑名单 +3 项（FileWatcher + InotifyFileWatcher + HotReloadManager）；L1 PASS；L2 ~+900 bytes 来源 `vx_view_hot_reload_tracked_count` + `vx_view_attach_devtool(hot_reload_dir)` 扩展 → 合规

### A.6 反模式

- ❌ 把 L2 = 0 当作 A14 唯一判据 → 误判公开 stub 为违规
- ❌ 仅看 L1 不看 L2 → 错过 leak 升级信号（L2 > 0 + 来源不是公开 ABI 时）
- ❌ A14 验证仅在 reflect 阶段做 → 应在每个子任务 finalize 时由 ctest A14 smoke 守门

### A.7 交叉引用

- `tests/smoke/devtool_a14_link_closure.cmake` — L1 自动化守门
- `memory-bank/systemPatterns.md`「子系统关闭守门 ctest smoke 范式」段 — 范式总结
- `memory-bank/systemPatterns.md`「黑名单维护演进透明度」子段 — 黑名单更新协议
```

**步骤 1-6**：同任务 1 模板

**commit 消息**：

```bash
docs(spec): add Appendix A on A14 link-closure vs size-diff semantics (A-P1#8)

Source: TASK-20260502-01 reflection §6 P1 #8.
Disambiguates A14 dual-layer semantics: L1 link-closure (Hard) +
L2 binary size diff (Soft Indicator). Phase A/B/C all implicitly
followed L1=PASS + L2 explained by public C ABI stubs = compliant.
Provides explicit formula for future devtool-style conditional
subsystems' compliance assessment.
```

**预估**：~15-20 min plan ×0.6 → 实测 ~10-12 min

---

## §3 验收要点

### §3.1 ctest config 矩阵（自我吃 C-#2 狗粮）

| Config | DEVTOOL | SDL2 | Benchmarks | 期望 ctest | 实测 ctest |
|---|:-:|:-:|:-:|:-:|:-:|
| baseline | ON | OFF | OFF | 1247 | - |
| 本任务 | ON | OFF | OFF | **1247（不退化）** | - |
| OFF path | OFF | OFF | OFF | 1082（不退化）| - |
| SDL2 path | ON | ON | OFF | 1265（不退化）| - |

**判读**：本任务零代码逻辑变更 → 三 config 全部不退化为 PASS。

### §3.2 git diff 可见性

- 6 commits 各自 git log 单独可读 + commit 消息含子任务 ID
- `git diff main..HEAD` 仅含 4 个目标文件（writing-plans.mdc + git-workflow.mdc + spec + techContext.md）
- 总 diff 行数预估 ≤ 350 行（4 文件 + ~250 净增 + 50-100 上下文行）

### §3.3 Memory Bank 待处理事项段更新

- `activeContext.md`「待处理事项」段：6 项 P1 全部标 ✅ 已落实并迁移到「长期沉淀」段
- A-P1#6 P3 clang-tidy enforce 候选保留在「待处理事项」段

---

## §4 Checkpoint 总览（B6 锁定）

| CP | 触发位置 | 自审重点 | 失败处理 |
|:-:|---|---|---|
| **CP1** | 任务 1-3 完成后（writing-plans 3 段同文件 batch） | 段落语义连贯 / 无过度工程 / 标题层级 / 交叉引用真实 | 退回任务级修订 |
| **CP2** | 任务 5 完成后（audit 文档化） | audit 范围完整性 / 命令兜底正确 / P3 候选迁移 | 扩展 audit 范围 |

---

## §5 反复模式抑制 + plan ×0.6 数据点假设

### §5.1 反复模式抑制清单

| # | 模式 | 本任务抑制方式 |
|:-:|---|---|
| 1 | C-#2「ctest 数量未明示 config 假设」第二次反复 | 任务 2 落实 + 自我吃狗粮（§3.1 含 config 矩阵）|
| 2 | 「前置依赖/环境/API 能力未验证」反复模式 | Phase 0 §0.2 audit 预跑 + §0.3 工具可用性矩阵 |
| 3 | 「commit 颗粒度」反复 | B1 锁定 3 commits + 自我吃 A-P1#4 狗粮 |

### §5.2 plan ×0.6 数据点假设入库

- 蓝图估时：~85-130 min plan / plan ×0.6 ~50-80 min → **实测预测 ~40-65 min**（极窄档延续高效区第 7 数据点群组）
- 比 Phase C 0.31× 略高（0.40-0.50× 比值）— 因子任务 6 项 < Phase C 11 项，规模效应不显著
- 假设入库：plan ×0.6 第 56+ 数据点群组（Phase C +12 数据点后续）

---

## §6 Commit 消息模板汇总

| # | 子任务 | 类型 | 消息模板 |
|:-:|---|---|---|
| 1 | C-#1 | docs | `docs(writing-plans): add public API testability checklist (C-#1)` |
| 2 | C-#2 | docs | `docs(writing-plans): add ctest config matrix declaration (C-#2)` |
| 3 | C-#4 | docs | `docs(writing-plans): add toolchain version aggressive upgrade check (C-#4)` |
| 4 | A-P1#4 | docs | `docs(git-workflow): add multi-subtask commit splitting via git add -p (A-P1#4)` |
| 5 | A-P1#6 | docs | `docs(techContext): document StatusOr.status() codebase audit (A-P1#6)` |
| 6 | A-P1#8 | docs | `docs(spec): add Appendix A on A14 link-closure vs size-diff semantics (A-P1#8)` |
| finalize | - | chore | `chore(build): finalize TASK-20260503-02 memory bank state` |

---

## §7 systemPatterns 协同度自我对照

本任务直接落实或加深的 systemPatterns 段：

| # | systemPattern 段 | 本任务关系 |
|:-:|---|---|
| 1 | 「Phase 0 投入越深 / build phase 越快」定律（triple-evidence）| §0.2 audit 预跑应用 |
| 2 | 「baseline 阻塞 hotfix 分离协议」段 | C-#4 任务 3 文档化 |
| 3 | 「R12 工具链版本激进升级风险」段 | C-#4 任务 3 文档化 |
| 4 | 「lazy-attach C ABI 容错模式」段 | C-#1 testability 检查清单（同源 API 设计模式）|
| 5 | 「子系统关闭守门 ctest smoke 范式 — 黑名单维护演进透明度」子段 | A-P1#8 任务 6 spec A14 双层语义补强 |
| 6 | 「Mixed TDD RED 反向探针实践」段 | 不适用（文档调整任务无 TDD 反向探针）|
| 7 | 「commit 颗粒度」段 | A-P1#4 任务 4 直接补强 |
| 8 | 「Status / StatusOr 使用规范」段（techContext）| A-P1#6 任务 5 audit 结果追加 |
| 9 | 「极窄档 plan ×0.6 矩阵」段 | §5.2 第 56+ 数据点假设入库 |

**协同度评分**：9/9 直接相关或加深，无新建 systemPattern 候选（本任务是规则落实任务而非范式创建任务）。
