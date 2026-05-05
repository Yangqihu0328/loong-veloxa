# Plan: TASK-20260505-04 工作流元任务批量落地（P1×10 + P2×4 跨任务沉淀批量固化）

**任务 ID：** TASK-20260505-04
**复杂度：** Level 2-3 工作流元任务（沿用 [TASK-20260503-02 工作流元任务范式](../../memory-bank/archive/archive-TASK-20260503-02.md)）
**分支：** `feature/TASK-20260505-04-workflow-meta-batch`（基于 main `b085a85`）
**估时（plan ×0.6）：** ~130-180 min（14 项 / 平均 ~10-15 min/项）
**预期实测：** ~50-80 min（极速区 0.4-0.6× 系数）
**安全相关：** ❌ 否
**测试模式：** [文档调整模式]（沿用 TASK-03-02 / 无 ctest 验证 / 验证手段：grep audit + Read 结构 + ctest baseline 不变）

---

## 0. 任务定位与决策矩阵

### 0.1 任务定位

**工作流元任务**（vs 实施类任务 / 蓝图类任务）— 集中清零累积的 14 项跨任务 reflection §5/§6 P1+P2 沉淀，避免反复模式累积升级到 P0 紧急轨道，同时验证「P0 协议自吃狗粮」实践成效（D8=A）。

### 0.2 D1-D8 plan 阶段决策矩阵（8/8 1 次 AskQuestion all_recommended 锁定 ✅）

| # | 决策项 | 选择 | 理由 |
|:-:|---|---|---|
| **D1** | commit 拆分粒度 | **B 6 commit / 文件** | 同文件 batch 收益 + git bisect 精度足够 + 14 项规模适配最优 |
| **D2** | 实施顺序 | **A 文件聚合**（writing-plans 7 → systemPatterns 3 → git-workflow 2 → brainstorming 1 → main 1 → veloxa_api 1）| 与 D1=B 协同 ✅ / 沿用 TASK-03-02 范式 |
| **D3** | P1.5 P0 协议文本形态 | **B 完整段 ~80-120 行** | 沿用既有 writing-plans 段式范本（StrReplace audit 40 行 + ctest 矩阵 40 行）/ triple-evidence 应配完整规范 |
| **D4** | TDD 适用性 | **A 文档调整模式** | 沿用 TASK-03-02 工作流元任务范式 / 无 ctest 验证 / grep audit + Read 结构验证 |
| **D5** | P1.9 + P2.2 合并 | **B 分开** | 不同主题（粒度规格化 vs LOC 估算系数）/ DRY/单一职责 |
| **D6** | P1.7 veloxa_api.h 位置 | **A 顶部 doc 段**（第 1-7 行 `/* */` 内追加）| 与 P1.7 描述「头部 doc 段」一致 / 用户首次打开即见 |
| **D7** | 独立 spec | **B 仅 plan** | 沿用 TASK-03-02 范式 / 工作流元任务豁免 spec / P0 协议「plan/spec docs 落盘即 commit」对工作流元任务豁免 spec 子项 |
| **D8** | P0 协议自吃狗粮 | **A 自吃狗粮** | plan + activeContext + tasks + progress 单 commit 落盘 / triple → **quad-evidence** 候选升级 / 与 P1.5 主题完全一致 |

### 0.3 跨决策协同度（doudec-evidence 第 13 次连续命中）

- D1=B ↔ D2=A 自然协同 ✅
- D7=B ↔ D8=A 协同 ✅（plan + MB 单 commit / 无 spec）
- D3=B ↔ D4=A 协同 ✅（完整段 + 文档验证模式 / 无 ctest 干扰）
- D6=A ↔ P1.7 描述协同 ✅
- 全 ✅ / 累计 113 + 8 = **121/121**

---

## 1. 文件结构

### 1.1 修改文件清单（6 个 / 14.5 子项分布）

| # | 文件 | 子项数 | 净行数预估 | 性质 |
|:-:|---|:-:|:-:|---|
| 1 | `.cursor/rules/skills/writing-plans.mdc` | **7** | +~280-340 | 既有 1079 行 → ~1360-1419 行 / 7 个新段或子段 |
| 2 | `memory-bank/systemPatterns.md` | **3** | +~80-120 | 既有 3663 行 → ~3743-3783 行 / 2 顶级新段 + 1 子段 |
| 3 | `.cursor/rules/skills/git-workflow.mdc` | **2** | +~80-120 | 既有 228 行 → ~308-348 行 / 1 顶级新段 + 1 子段扩展 |
| 4 | `.cursor/rules/skills/brainstorming.mdc` | **1** | +~30-50 | 既有 161 行 → ~191-211 行 / 1 顶级新段 |
| 5 | `.cursor/rules/main.mdc` | **1** | +~25-35 | 既有 148 行 → ~173-183 行 / 既有 V2=a 段标注扩展 |
| 6 | `veloxa/api/veloxa_api.h` | **0.5**（与 writing-plans P1.7 配对）| +~15-25 | 既有 431 行 → ~446-456 行 / 头部 doc 段追加 |
| **总计** | — | **14.5** | **+~510-690** | 6 文件改动 / 0 新文件 |

### 1.2 新建文件清单（3 个 / 工作流文档）

| 操作 | 文件 | 行数 | 说明 |
|:-:|---|:-:|---|
| 创建 | `docs/plans/2026-05-05-workflow-meta-batch.md` | ~~~600 行~~ 当前文档 | 实施计划（plan 阶段产出）|
| 创建 | `memory-bank/reflection/reflection-TASK-20260505-04.md` | ~~~250-300 行~~ | Level 2 详细回顾（reflect 阶段产出）|
| 创建 | `memory-bank/archive/archive-TASK-20260505-04.md` | ~~~250-300 行~~ | 归档（archive 阶段产出）|

### 1.3 commit 时间线（D1=B 6 commit/文件 + D8=A 自吃狗粮 plan commit + finalize）

| # | commit type | 阶段 | 内容 |
|:-:|---|---|---|
| 0 | `chore(workflow)` | VAN（已 commit `3a1e610`）| initialize TASK-20260505-04（已完成）|
| 1 | `chore(workflow)` | **Plan（D8 自吃狗粮）** | **plan + activeContext + tasks + progress 单 commit 落盘**（实践 P1.5 P0 协议自身 / triple → quad-evidence 候选）|
| 2 | `docs(writing-plans)` | Build B.1 | writing-plans.mdc 7 子项（P1.1+P1.2+P1.5+P1.6+P1.7-half+P1.9+P2.2）|
| 3 | `docs(systemPatterns)` | Build B.2 | systemPatterns.md 3 子项（P1.4+P2.3+P2.4）|
| 4 | `docs(git-workflow)` | Build B.3 | git-workflow.mdc 2 子项（P1.10+P2.1）|
| 5 | `docs(brainstorming)` | Build B.4 | brainstorming.mdc 1 子项（P1.3）|
| 6 | `docs(main)` | Build B.5 | main.mdc 1 子项（P1.8）|
| 7 | `docs(api)` | Build B.6 | veloxa_api.h 1 子项（P1.7-half / lazy-attach contract 头部 doc）|
| 8 | `chore(build)` | Build finalize | finalize TASK-20260505-04 memory bank state |

**总计：** 8 commits（VAN 1 + Plan 1 + Build 6 + Finalize 1）

---

## 2. Phase 0 audit 实证结果（plan 阶段已完成 ✅）

### 2.1 文件位置 + 现有结构验证（10 项全 ✅）

| # | 维度 | 结果 |
|:-:|---|:-:|
| 1 | writing-plans.mdc 1079 行 + ~30 顶级段 | ✅ |
| 2 | brainstorming.mdc 161 行 + 6 顶级段（含「跨决策协同度」段已存在）| ✅ |
| 3 | git-workflow.mdc 228 行 + 11 顶级段（含「Multi-subtask commit 拆分」段）| ✅ |
| 4 | main.mdc 148 行 + 第 57-67 行「Level 4 蓝图任务 V2=a 工作流变体」段已存在 | ✅ |
| 5 | systemPatterns.md 3663 行 + 既有「中文文档编辑安全 audit」段（line 2961）| ✅ |
| 6 | veloxa_api.h 431 行 + 第 1-7 行 `/* */` 项目级文档块 | ✅ |
| 7 | TASK-03-02 工作流元任务范式参考（6 子项 / 6 commit / 0 独立 spec）| ✅ |
| 8 | doudec-evidence + V2=a triple-evidence 段（systemPatterns line 3538/3612）| ✅ |
| 9 | 既有 writing-plans 完整段范本（StrReplace audit 40 行 / ctest 矩阵 40 行）| ✅ |
| 10 | TASK-05-03 commit `1555cf4` 8 段 commit body 范本（实例引用源）| ✅ |

### 2.2 反复模式预防 audit（plan 阶段预审 0/8 命中）

- ✅ #1 前置依赖未验证：4 维度全通过（依赖 / 环境 / artifact / 待处理事项关联）
- ✅ #6 提交粒度偏离计划：D1=B + D2=A 已锁定 commit 拆分粒度
- ✅ #8 spec 数据回归 audit：本任务无 spec 数据回归
- ✅ 中文文档 StrReplace 字符类型 audit：6 文件编辑 / 严格执行 Read + StrReplace 协议
- ✅ #4 子代理产出返工：本任务单 agent 直跑（14 项规模适合）

---

## 3. 详细子任务（按文件聚合 / D2=A 顺序）

### Phase B.1 — `.cursor/rules/skills/writing-plans.mdc`（7 子项 / 1 commit）

#### B.1.1 — P1.1 Phase 0 段「JS context 归属与 host binding 注册 ctx 一致性 audit」子条 [文档调整]

**来源：** TASK-20260503-04 reflection §5 #1（反复模式 #1 第 4 个新形式 / panel JS / 用户脚本 ctx 归属未实证）

**目标位置：** writing-plans.mdc Phase 0 audit 系列段中（line ~169 既有「测试基础设施审计」段附近 / 或单独新段）

**内容要点：** ~10 行子条 — 触发条件 + 「ctx 归属反向 grep」操作步骤 + 反模式（pannel JS 假设 ctx 归属未实证）

**验证：** Grep 「JS context 归属」字符串确认入库 / Read 验证段落结构

#### B.1.2 — P1.2「资源类反向探针 SOP」新子段 [文档调整]

**来源：** TASK-20260503-04 reflection §5 #2（D.3 console_panel.html 注释里 `<input` 字面量触发反向探针 false positive）

**目标位置：** writing-plans.mdc 既有 Phase 0 / 反向探针类段附近（line ~340 「管线注入点代码级可行性验证」段附近）

**内容要点：** ~15-20 行子段 — 资源反向探针应限定到非注释区域 + comment policy 推荐 + 实证

**验证：** Grep 「资源类反向探针」+ Read 验证段落

#### B.1.3 — **⭐ P1.5「plan/spec docs 落盘即 commit」P0 协议段（完整段 ~80-120 行）** [P0 升级 / 文档调整]

**来源：** TASK-20260505-01 提议 → TASK-20260505-02 部分实施 → TASK-20260505-03 完整实施 → **triple-evidence 已达固化阈值**

**目标位置：** writing-plans.mdc 顶部「文件结构」段附近（line ~21）/ 或独立新段（推荐 `## plan/spec docs 落盘即 commit P0 协议（plan 阶段产出物 commit 协议）`）

**内容要点（D3=B 完整段）：**

```markdown
## plan/spec docs 落盘即 commit P0 协议（plan 阶段产出物 commit 协议）

> **TASK-20260505-01 提议 → TASK-20260505-02 部分实施 → TASK-20260505-03 首次完整实施 → triple-evidence 已达固化阈值（P0 立即固化）**

### 触发条件

**plan 阶段产出物必须单 commit 落盘**：

- 涉及 spec 文档创建或修改（蓝图任务 / 实施类任务）
- 涉及 plan 文档创建
- 涉及 creative ×N 文档创建（V2=a 蓝图 / 蓝图 V2=完整变体）
- 涉及 Memory Bank 三件套同步更新（activeContext + tasks + progress）

### 强制操作

1. **plan + spec + creative ×N + Memory Bank 单 commit 落盘** — 全部产出物作为单 atomic commit / 0 collateral commit
2. **build 阶段（如有）零 collateral commit** — 不得有「补 plan 段」/「补 creative 文档」类碎片 commit
3. **commit body 含完整 8 段范本**（详见下方 §commit body 8 段范本）
4. **工作流元任务豁免 spec 子项** — 工作流元任务（如 TASK-20260503-02 + TASK-20260505-04）无独立 spec / 仅 plan + Memory Bank 单 commit

### commit body 8 段范本

1. 任务定位（任务 ID + 复杂度 + 工作流变体 + 主交付概要 + 不含 build 标注[V2=a]）
2. 决策矩阵（V/B/D 决策矩阵完整列表）
3. 主交付清单（spec/plan/creative 文档 + 行数）
4. 后续实施（实施子任务 + 估时 + +30% buffer）
5. 协议元数据（plan/spec docs 落盘即 commit P0 协议执行状态）
6. plan ×0.6 实测系数（实测 + 子档归属）
7. Source 溯源（双源：上游 spec / archive / reflection 引用）
8. 下一步（/reflect 或 /build）

### 反模式

- ❌ 「先 commit plan，等 build 阶段补 spec」分两 commit（破坏原子性）
- ❌ 「creative 文档放到 build 阶段第 1 commit」（违反「plan 阶段产出」边界）
- ❌ 「Memory Bank 三件套等 build 完再更新」（导致 reflect 阶段 MB drift）
- ❌ 工作流元任务强制独立 spec（沿用 TASK-03-02 范式 / 工作流元任务豁免）

### triple-evidence 实证表

| # | 任务 | 实施程度 | commit |
|:-:|---|---|---|
| 1 | TASK-20260505-01 | 提议 P1 #6 改进建议 | 反思阶段提案 |
| 2 | TASK-20260505-02 | 部分实施（plan + spec + MB 1 commit / build 阶段 0 collateral）| `4feda52` |
| 3 | TASK-20260505-03 | **首次完整实施 ✅**（plan + spec + creative ×3 + MB ×3 = 8 files 单 commit / 0 collateral）| `1555cf4` |
| 4 | TASK-20260505-04（本任务）| **自吃狗粮**（plan + MB 单 commit / quad-evidence 候选升级）| 本任务 commit 1 |

### 交叉引用

- `memory-bank/systemPatterns.md` 「跨决策协同度 100% doudec-evidence」段（决策矩阵协同范式）
- `memory-bank/systemPatterns.md` 「V2=a 蓝图任务范式 triple-evidence」段（V2=a 范式协议）
- `.cursor/rules/skills/git-workflow.mdc` 「蓝图任务 commit body 范本」段（commit body 8 段范本同源）
```

**估时：** ~15-20 min（最大子项）

**验证：** Grep 「plan/spec docs 落盘即 commit」字符串入库 + Read 验证 8 段 + 协议元数据完整

#### B.1.4 — P1.6 Phase 0 audit 段「spec vs code 一致性 audit」+「能力假设 audit」双子条 [文档调整]

**来源：** TASK-20260505-02 reflection §5 #4（反复模式 #8 spec 数据回归 triple-evidence / TASK-03-02-04 + 05-01 + 05-02 三次累计）

**目标位置：** writing-plans.mdc Phase 0 audit 段中（line ~169 「测试基础设施审计」段附近）

**内容要点：** ~25-35 行 — 两个子条
- 子条 A:「spec vs code 一致性 audit」（spec 标记功能"缺失"但实际已实现 / VAN/plan 阶段必须 grep 验证）
- 子条 B:「能力假设 audit」（spec 推荐「路径 X 可行」但底层引擎不支持 / VAN/plan 阶段必须实证验证）

**验证：** Grep 「spec vs code 一致性」+「能力假设 audit」双字符串入库

#### B.1.5 — P1.7-half C ABI 设计模式段「lazy-attach 默认契约」子条 [文档调整]

**来源：** TASK-20260505-02 reflection §5 #5（lazy-attach C ABI 容错模式 quad-evidence / 已成 Veloxa 默认范式）

**目标位置：** writing-plans.mdc 「公开 API testability 检查清单」段附近（line ~31）/ 单独子段「C ABI 设计模式」

**内容要点：** ~20-30 行 — lazy-attach 默认契约：当 C ABI 函数依赖的 sub-system（如 update_manager_ / script_engine_）可能未初始化时，必须返回 INVALID_STATE 而非 crash / 引用 4 次实证累计 / 引用 veloxa_api.h 顶部 doc 段（B.6.1 配对）

**验证：** Grep 「lazy-attach 默认契约」+ Read 验证 4 实证表

#### B.1.6 — P1.9「蓝图任务子任务规格化深浅梯度」段（🟢 完整 / 🔵 概要） [文档调整]

**来源：** TASK-20260505-03 reflection §3.d #3 + §4 #5（18 子任务规格化深浅梯度首次实证）

**目标位置：** writing-plans.mdc 「TDD 模式任务模板」段附近（line ~968）/ 或独立段「蓝图任务子任务规格化深浅梯度」

**内容要点：** ~30-40 行 — 🟢 完整规格化（首批 1-3 子任务 / 直接基于本规格立项）+ 🔵 概要规格化（其余子任务 / 仍需独立立项 + Phase 0 audit + brainstorm）+ plan §8 边界明示模板

**验证：** Grep 「🟢 完整规格化」+「🔵 概要规格化」入库

#### B.1.7 — P2.2 LOC 估算附录「隐性附加工作类型清单」+ ×1.3-1.5 buffer [文档调整]

**来源：** TASK-20260503-02 reflection §6 #4 + TASK-03-04 P2 #1（同源合并）

**目标位置：** writing-plans.mdc「计划文档头部」段附近（line ~939）/ 或附录段

**内容要点：** ~20-25 行 — 隐性附加工作类型清单（5-7 项 / 含 Doxygen 文档 + commit-friendly 注释扩展 + test fixture + atomic counter + 反向探针注释）+ ×1.3-1.5 buffer 范本

**验证：** Grep 「隐性附加工作类型」+ Read 验证清单结构

#### Phase B.1 commit

```
docs(writing-plans): batch land 7 P1+P2 tech debt items [TASK-20260505-04]

子项汇总（7 项 / +~280-340 行）：
- P1.1 Phase 0「JS context 归属」子条
- P1.2「资源类反向探针 SOP」段
- ⭐ P1.5「plan/spec docs 落盘即 commit」P0 协议段（完整段 / triple-evidence 升级 P0）
- P1.6 Phase 0「spec vs code 一致性 + 能力假设」双子条
- P1.7-half C ABI 设计模式段「lazy-attach 默认契约」（与 veloxa_api.h B.6.1 配对）
- P1.9「蓝图任务子任务规格化深浅梯度」段（🟢/🔵）
- P2.2 LOC 估算附录「隐性附加工作类型」+ ×1.3-1.5 buffer

Source: TASK-20260503-04/05-01/05-02/05-03 reflection §5 累计 / triple → quad-evidence 升级
```

---

### Phase B.2 — `memory-bank/systemPatterns.md`（3 子项 / 1 commit）

#### B.2.1 — P1.4「视觉链路三件齐识别协议」段 [文档调整]

**来源：** TASK-20260505-01 reflection §5 #5（dogfood UI 行为依赖 ≥3 个独立缺陷修复 / 必须单任务集中闭环）

**目标位置：** systemPatterns.md 末尾「待定架构决策」段之前（line ~3655 / 紧邻 V2=a triple-evidence 段后）

**内容要点：** ~30-40 行 — 视觉链路三件齐识别协议（当 dogfood UI 行为依赖 ≥ 3 个独立缺陷修复才能完整工作时，必须单任务集中闭环）+ plan §UI 行为验收表是识别工具 + TASK-05-01 plan §0.11「视觉恢复链路」表是范式 + 实证 + 反模式

**验证：** Grep 「视觉链路三件齐」入库

#### B.2.2 — P2.3「activeContext.md 重复 anchor 检测协议」子段 [文档调整]

**来源：** TASK-20260505-03 reflection §4 #7（VAN 阶段 StrReplace 前 Grep `^## 上次任务` 类标题段）

**目标位置：** systemPatterns.md 「中文文档编辑安全 audit」段中（line ~2961）/ 加子段「重复 anchor 检测协议」

**内容要点：** ~15-20 行子段 — VAN/archive 阶段 StrReplace 前必须 Grep `^## 上次任务|^## 当前阶段` 类标题段检测重复 / TASK-05-03 实证（VAN commit `8ba512f` 后出现 2 个「上次任务」段）+ TASK-05-04 plan 实证（progress.md 出现 2 个「上次任务」立即清理）

**验证：** Grep 「重复 anchor 检测」入库

#### B.2.3 — P2.4「V2=a 蓝图任务文档密度系数 1.0-1.4×」段 [文档调整]

**来源：** TASK-20260505-03 reflection §4 #8（5 文档全偏正向 +27% ~ +139% / 既有 0.7-1.0× 系数偏低）

**目标位置：** systemPatterns.md 末尾 V2=a triple-evidence 段后（紧邻 B.2.1 的 P1.4 段后）

**内容要点：** ~30-40 行 — V2=a 蓝图任务文档密度系数 1.0-1.4× 升级（既有 0.7-1.0× 系数偏低 / 实证 3 任务对照表 / TASK-20260430-04 + 20260504-01 + 20260505-03）+ 适用前置 + 反模式

**验证：** Grep 「V2=a 蓝图任务文档密度系数」入库

#### Phase B.2 commit

```
docs(systemPatterns): batch land P1.4 + P2.3 + P2.4 visual chain + anchor + density [TASK-20260505-04]

子项汇总（3 项 / +~80-120 行）：
- P1.4「视觉链路三件齐识别协议」段（来源 TASK-05-01 reflect §5 #5）
- P2.3「activeContext.md 重复 anchor 检测协议」子段（来源 TASK-05-03 reflect §4 #7）
- P2.4「V2=a 蓝图任务文档密度系数 1.0-1.4×」段（来源 TASK-05-03 reflect §4 #8）

Source: TASK-20260505-01 + TASK-20260505-03 reflection 累计沉淀
```

---

### Phase B.3 — `.cursor/rules/skills/git-workflow.mdc`（2 子项 / 1 commit）

#### B.3.1 — P1.10「蓝图任务 commit body 范本」段（8 段固化） [文档调整]

**来源：** TASK-20260505-03 reflection §3.c #5 + §4 #6（commit `1555cf4` 8 段范本实例）

**目标位置：** git-workflow.mdc「提交规范」段后（line ~27）/ 或「Multi-subtask commit 拆分」段后（line ~157）

**内容要点：** ~30-40 行 — 蓝图任务 commit body 8 段固化（任务定位 / 决策矩阵 / 主交付清单 / 后续实施 / 协议元数据 / plan ×0.6 / Source 溯源 / 下一步）+ 实例引用 commit `1555cf4`（TASK-05-03）+ 与 P0 协议「plan/spec docs 落盘即 commit」段交叉引用

**验证：** Grep 「蓝图任务 commit body 范本」入库

#### B.3.2 — P2.1 commit body Source 溯源 + 实测数据格式固化 [文档调整]

**来源：** TASK-20260503-03 reflection §6 #4（quad-evidence ~39 commits / 已成默认协议）

**目标位置：** git-workflow.mdc「提交规范」段中（line ~27）/ 加子段「commit body Source 溯源 + 实测数据格式」

**内容要点：** ~20-30 行 — Source 溯源 4 类（spec / plan §X / archive / reflection §X）+ 实测数据格式（plan ×0.6 / commit hash / 度量数据）+ ~39 commits quad-evidence 实证 + 反模式

**验证：** Grep 「Source 溯源」入库

#### Phase B.3 commit

```
docs(git-workflow): batch land P1.10 + P2.1 commit body templates [TASK-20260505-04]

子项汇总（2 项 / +~80-120 行）：
- P1.10「蓝图任务 commit body 范本」段（8 段固化 / 实例 commit 1555cf4）
- P2.1 commit body Source 溯源 + 实测数据格式（quad-evidence ~39 commits）

Source: TASK-20260503-03 + TASK-20260505-03 reflection 累计沉淀
```

---

### Phase B.4 — `.cursor/rules/skills/brainstorming.mdc`（1 子项 / 1 commit）

#### B.4.1 — P1.3「Phase 0 grep 实证驱动的主动 push-back 模式」段 [文档调整]

**来源：** TASK-20260503-05 reflection §5 #2（D8b 实证 / brainstorm scope 已被 core_only 限定后 Phase 0 grep 发现 creative 文档 10⁷ 检查点字面值会导致 100-1000s 死循环灾难 → 必须主动抛出）

**目标位置：** brainstorming.mdc「跨决策协同度」段后（line ~131）/ 独立新段

**内容要点：** ~30-40 行 — Phase 0 grep 实证驱动主动 push-back 模式 / 触发条件清单（3 条件全成立时强制 push-back）：(1) brainstorm scope 已被用户限定 + (2) Phase 0 grep / audit 阶段发现偏差 + (3) 偏差显著（默认值差 10³+ 倍）+ 实证 + 反模式

**验证：** Grep 「Phase 0 grep 实证驱动」入库

#### Phase B.4 commit

```
docs(brainstorming): add P1.3 Phase 0 grep evidence-driven push-back pattern [TASK-20260505-04]

子项汇总（1 项 / +~30-50 行）：
- P1.3「Phase 0 grep 实证驱动的主动 push-back 模式」段（D8b 实证 / 3 触发条件）

Source: TASK-20260503-05 reflection §5 #2
```

---

### Phase B.5 — `.cursor/rules/main.mdc`（1 子项 / 1 commit）

#### B.5.1 — P1.8 Level 4 V2=a 工作流变体段升级为「稳定范式 / triple-evidence」标注 [文档调整]

**来源：** TASK-20260505-03 reflection §4 #4（3 任务实证累计 / 范式参数已稳定）

**目标位置：** main.mdc 第 57-67 行「Level 4 蓝图任务 V2=a 工作流变体」段中

**内容要点：** ~20-30 行扩展（基于既有 11 行段）— 加 triple-evidence 标注 + 3 任务对照表（TASK-20260430-04 + TASK-20260504-01 + TASK-20260505-03）+ 范式稳定参数（平均 12.3 决策 / ~3030 行 / ~38 min）+ 适用任务类型 + 与 systemPatterns 「V2=a triple-evidence」段交叉引用

**验证：** Grep 「triple-evidence 稳定范式」入库

#### Phase B.5 commit

```
docs(main): upgrade Level 4 V2=a blueprint variant to triple-evidence stable [TASK-20260505-04]

子项汇总（1 项 / +~25-35 行）：
- P1.8 Level 4 V2=a 工作流变体段升级为「稳定范式 / triple-evidence」标注
  - 3 任务对照表（TASK-20260430-04 + TASK-20260504-01 + TASK-20260505-03）
  - 范式稳定参数（平均 12.3 决策 / ~3030 行 / ~38 min）

Source: TASK-20260505-03 reflection §4 #4 / triple-evidence 已达稳定范式标注阈值
```

---

### Phase B.6 — `veloxa/api/veloxa_api.h`（0.5 子项 / 1 commit）

#### B.6.1 — P1.7-half veloxa_api.h 顶部 doc 段「lazy-attach contract」节 [文档调整]

**来源：** TASK-20260505-02 reflection §5 #5（quad-evidence 已成 Veloxa C ABI 默认范式 / 与 B.1.5 writing-plans P1.7-half 配对）

**目标位置：** veloxa_api.h 第 1-7 行 `/* */` 项目级文档块内（在 `* All types and functions use C99-compatible declarations.` 后追加）

**内容要点：** ~15-20 行 doc 段 — lazy-attach contract（C ABI 函数在依赖 sub-system 未初始化时返回 VX_ERROR_INVALID_STATE 而非 crash / 4 次实证 ABI 列表：vx_view_set_pipeline_hooks / vx_view_attach_devtool / vx_devtool_get_console_output / vx_view_invalidate）+ 与 systemPatterns「lazy-attach C ABI 容错模式 quad-evidence」段交叉引用

**验证：** ctest baseline 不变 1302/1109（仅 doc 改动 / 不影响编译）+ Grep 「lazy-attach contract」入库

#### Phase B.6 commit

```
docs(api): add lazy-attach contract documentation to veloxa_api.h header [TASK-20260505-04]

子项汇总（1 项 / +~15-25 行）：
- P1.7-half veloxa_api.h 顶部 doc 段「lazy-attach contract」节
  - 4 次实证 ABI 列表（vx_view_set_pipeline_hooks / vx_view_attach_devtool / vx_devtool_get_console_output / vx_view_invalidate）
  - 与 P1.7-half writing-plans「lazy-attach 默认契约」段配对

Source: TASK-20260505-02 reflection §5 #5 / quad-evidence 已成 Veloxa C ABI 默认范式

ctest impact: 0（仅 doc 改动 / DEVTOOL=ON 1302/1302 + DEVTOOL=OFF 1109/1109 baseline 保持）
```

---

### Phase B.7 — finalize commit

#### B.7.1 — finalize Memory Bank state [chore]

**目标：** 更新 Memory Bank 三件套（activeContext / tasks / progress）状态从「构建中」到「待 reflect」+ 记录 build 阶段实测数据

**内容要点：**
- activeContext.md 阶段更新为 `构建完成` / 待 `/reflect`
- tasks.md 当前任务段加 Build 阶段产出
- progress.md 加 build 阶段时间线 + 实测系数

#### Phase B.7 commit

```
chore(build): finalize TASK-20260505-04 memory bank state

- Build 阶段 6 commits 完成（B.1-B.6 / 14.5 子项 / 6 文件 / +~510-690 行）
- ctest baseline 保持 DEVTOOL=ON 1302 + DEVTOOL=OFF 1109 ✅（仅文档/规则改动）
- 反复模式 0/8 抑制（VAN + plan + build 三阶段全程保持）
- plan ×0.6 实测系数：~?? min vs plan ×0.6 130-180 min = 待 reflect 阶段统计
- 跨决策协同度：8/8 D 决策 1 次 AskQuestion 锁定 / 第 13 次连续命中 / 累计 121/121

下一步：/reflect 进入回顾阶段
```

---

## 4. ctest 数量预期 config 矩阵

**本任务无 ctest 影响**（沿用 TASK-03-02 工作流元任务范式 / 仅文档/规则改动）

| ctest config | baseline（VAN 阶段）| 期望（archive 阶段）| 增量 |
|---|:-:|:-:|:-:|
| DEVTOOL=ON | 1302 | **1302** | **0**（保持 ✅）|
| DEVTOOL=OFF | 1109 | **1109** | **0**（保持 ✅）|

**验收门槛：** 任一 phase 后 ctest 数字漂移 → 立即停 + 排查（应为 0 漂移）

---

## 5. 安全任务

**N/A — 本任务不涉及安全变更**（仅文档/规则改动 / 0 代码逻辑改动 / 0 新威胁面 / 0 输入处理 / 0 认证/授权 / 0 敏感数据）

---

## 6. 反复模式预防清单（plan 阶段预审）

| # | 反复模式 | 命中风险 | mitigation |
|:-:|---|:-:|---|
| 1 | 前置依赖未验证 | ✅ 0 | Phase 0 audit 10/10 实证 |
| 3 | 子代理产出返工 | ✅ 0 | 单 agent 直跑 / 14 项规模适合 |
| 4 | 测试隔离 | N/A | 无 ctest 改动 |
| 5 | 提交粒度偏离计划 | ✅ 0 | D1=B + D2=A 锁死 |
| 6 | 非默认路径遗漏 | N/A | 无代码改动 |
| 7 | TDD 严格度不匹配 | ✅ 0 | D4=A 文档调整模式锁定 |
| 8 | spec 数据回归 audit | ✅ 0 | 本任务无 spec 数据回归 |
| 中文 StrReplace audit | ⚠️ 6 文件改动 | strict | Read 准确范围 + StrReplace ≤ 10 行 + 4 mitigation 全启用 |

---

## 7. Checkpoint（CP1+CP2）

### CP1 — 任务 B.1.3 「P1.5 P0 协议完整段」落盘后

**目的：** P0 协议自身段是最大子项（最复杂 ~80-120 行 / triple-evidence 实证 / 8 段范本）— 落盘后立即审查文本质量 + 是否覆盖完整 8 段 commit body 范本 + 与既有 writing-plans 段式协同度。

**审查项：**
- ✅ 8 段 commit body 范本完整列出
- ✅ 触发条件清晰（4 项）
- ✅ 反模式 4 项
- ✅ triple-evidence 实证表（含本任务 quad-evidence 候选行）
- ✅ 交叉引用 ≥ 3 处

### CP2 — Phase B.6 finalize 前

**目的：** 6 个 commit + 1 plan commit 完成 / 全文件改动累计后 / 最终质量审查 + 反复模式 0/8 全程抑制确认。

**审查项：**
- ✅ 6 commit body 各含 Source 溯源 + 实测数据
- ✅ ctest baseline 0 漂移
- ✅ 14.5 子项 100% 命中（grep 验证每子项关键字符串入库）
- ✅ 反向 grep 验证旧反模式描述未漏入新规则

---

## 8. 验收要点（reflect 阶段重审）

| # | 验收项 | 期望值 |
|:-:|---|---|
| 1 | 14.5 子项 100% 命中 | grep 验证全 ✅ |
| 2 | 6 文件改动 | 全部正确 |
| 3 | 8 commits（VAN 1 + Plan 1 + Build 6 + Finalize 1）| 全部正确 |
| 4 | 跨决策协同度 100% | 8/8 D 决策 1 次 AskQuestion 锁定 / 第 13 次 |
| 5 | 反复模式 0/8 命中 | 全程抑制 |
| 6 | ctest baseline 0 漂移 | 1302/1109 保持 |
| 7 | plan ×0.6 实测系数 | 落预期 ~50-80 min（极速区 0.4-0.6×）|
| 8 | P0 协议自吃狗粮 ✅ | plan + MB 单 commit / quad-evidence 升级 |

---

## 9. 后续路径

### 9.1 reflect 阶段建议沉淀

- **plan ×0.6 实测系数 sept → oct-evidence 候选**（第 8 数据点 / 工作流元任务子档）
- **跨决策协同度 doudec → tridec-evidence 候选**（第 13 次连续命中 / 累计 121/121）
- **P0 协议 quad-evidence 升级**（triple → quad / 已成 Veloxa 默认协议）
- **工作流元任务范式 dual-evidence**（TASK-03-02 + 本任务 / 第 2 实证）

### 9.2 archive 阶段路径

沿用 TASK-03-02 archive 范式：
- archive 文档（~250-300 行 / Level 2 详细归档）
- 分支处理（建议本地合并 main + 删除 / 沿用 ~24 commits 已合并范式）
- Memory Bank 三件套重置为空闲态

### 9.3 下一推荐任务

按 [activeContext.md](../../memory-bank/activeContext.md) 「下一推荐任务」段优先级（本任务完成后）：

1. G1.1 CMake VX_RENDERER flag（首批实施 / Level 2 / 闭环 GLES 蓝图首步）
2. R9 EventManager HitTest 改造（HUD pointer-events 真支持 / Level 2-3）
3. G2 DRM/KMS 嵌入式后端蓝图（Level 3-4）

---

## 10. 引用

- 上游 reflection 5 个：
  - [reflection-TASK-20260503-04.md §5](../../memory-bank/reflection/reflection-TASK-20260503-04.md)（P1.1 + P1.2）
  - [reflection-TASK-20260503-05.md §5](../../memory-bank/reflection/reflection-TASK-20260503-05.md)（P1.3）
  - [reflection-TASK-20260505-01.md §5](../../memory-bank/reflection/reflection-TASK-20260505-01.md)（P1.4 + P1.5 提议）
  - [reflection-TASK-20260505-02.md §5](../../memory-bank/reflection/reflection-TASK-20260505-02.md)（P1.5 部分实施 + P1.6 + P1.7）
  - [reflection-TASK-20260505-03.md §3.c+§4](../../memory-bank/reflection/reflection-TASK-20260505-03.md)（P1.5 完整实施 + P1.8 + P1.9 + P1.10 + P2.3 + P2.4）
- 上游 archive 工作流元任务范式：
  - [archive-TASK-20260503-02.md](../../memory-bank/archive/archive-TASK-20260503-02.md)（first-evidence / 6 子项 / 6 commit / 0 spec）
- systemPatterns 沉淀目标：
  - [systemPatterns.md doudec-evidence](../../memory-bank/systemPatterns.md)（line 3538）
  - [systemPatterns.md V2=a triple-evidence](../../memory-bank/systemPatterns.md)（line 3612）

---

**END OF PLAN**
