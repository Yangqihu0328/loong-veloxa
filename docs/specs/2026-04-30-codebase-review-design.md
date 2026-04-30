# 全代码库 Code Review 设计规格

- **任务 ID：** TASK-20260430-03
- **创建日期：** 2026-04-30
- **分支：** `feature/TASK-20260430-03-codebase-review`（基于 main `9411584`）
- **复杂度：** Level 4（多子系统横扫 + 6 维度全覆盖 + 多轮次 + 不可估时上限拆 R3+ 后续任务消化）
- **安全相关：** ✅ 是（V5 涵盖 security 维度 — HTML inline style 三件套 / CSS DoS / 第三方依赖 CVE 周期审计）

---

## §1 目的（Purpose）

对 Veloxa 全代码库（136 源 + 80 测试，7 子系统）做一次结构化 review，输出**可执行的不足清单 + 解决方案**，分类为 P0/P1/P2 优先级，作为后续若干独立修复任务的输入材料。

主要受益：
- 找出 30+ archive 任务沉淀的 systemPatterns ≥30 模式**未覆盖的盲点**
- 找出**外部 reviewer 视角下**的命名 / 反直觉 / 直观设计缺陷
- 量化测试覆盖率盲区（lcov）+ 第三方依赖 CVE 状态（GitHub Security Advisories）
- 把不可估的修复成本转为「按 ROI 拆出独立后续任务」的可控管理

---

## §2 不做（Out of Scope）

| # | 不做项 | 原因 |
|:-:|---|---|
| O1 | **不修改任何元规则文件**（`.cursor/rules/*.mdc` / `.cursor/commands/*.md`）— D7=C 决策 | scope 过宽；元规则修改需用户亲手决策，本任务仅记长期项 |
| O2 | **R3+ P1 大件修复不在本任务内** — 策略 X 决策 | 不可估成本必须封顶；R3+ 拆出独立后续 TASK-* |
| O3 | **不重新设计架构**（不动分层 / 抽象基类 / 数据流） | 30+ archive 任务已沉淀，本 review 仅找局部不足 |
| O4 | **不写新 benchmark**（除非现有 baseline 不足以判定 perf 维度） | bench 基础设施已成熟，复用现有 13 个 baseline |
| O5 | **不重写测试**（仅在 R2 P0 修复涉及单测时 +1）| R3+ 拆出 |
| O6 | **不动 TASK-26-01 R2 三件套护栏**（HTML inline style count cap 1000 / value cap 8KB / 黑名单） | 已被 TASK-26-01/30-01/30-02 多轮验证可靠 |
| O7 | **不评估 ABI 兼容性** / **不审 API 设计哲学** | 单独 spec 类任务的 scope，不混做 |

---

## §3 成功标准（Acceptance Criteria）

### R0 阶段验收

| # | 标准 | 必然 |
|:-:|---|:-:|
| A1 | 6 维度 grep fingerprint 完整产出（每维度 ≥ 5 反模式 grep 结果）| ✅ |
| A2 | 抽样深度策略明确（TOP-3 子系统全文件深扫名单 + 4 子系统 grep 模式名单） | ✅ |
| A3 | ctest 基线 1061/1061 PASS 核验（继承 TASK-30-02 终态） | ✅ |
| A4 | lcov 覆盖率报告生成（每子系统 line/branch/function 覆盖率三维度） | ✅ |
| A5 | CVE 审计完成（7 依赖各列出 GitHub Security Advisories 当前状态 + CVE 数量） | ✅ |

### R1 阶段验收（核心）

| # | 标准 | 必然 |
|:-:|---|:-:|
| A6 | spec 主报告落盘 `docs/specs/2026-04-30-codebase-review-report.md`，含 ≥ N 项不足（N 由 R0 fingerprint 估算，预期 20-50 项）| ✅ |
| A7 | 每项不足必含：定位（文件:行）/ 维度归类（perf/correct/maint/security/test/consistency）/ 反模式描述 / 影响评估 / 解决方案（结构化 D3=B：问题 + 方向 + 工时 + 代码量，不含具体代码示例） / 优先级（P0/P1/P2） | ✅ |
| A8 | 不足清单分类汇总表 + ROI 矩阵（按「修复成本 × 影响范围」二维分类） | ✅ |
| A9 | 与既有 systemPatterns ≥30 模式对照表（每项标注「已规则化 / 部分规则化 / 未规则化」） | ✅ |
| A10 | 元规则潜在问题独立附录（D7=C：仅记长期项，不修改元规则文件） | ✅ |
| A11 | 第三方依赖 CVE 长期审计追踪表（D8=C：本任务跑结果 + 周期性 audit 长期项 P3 候选） | ✅ |
| A12 | 测试覆盖率盲区报告（D9=B：lcov 实测 + 未覆盖热点 TOP-N） | ✅ |

### R2 阶段验收（条件触发）

| # | 标准 | 必然 |
|:-:|---|:-:|
| A13 | P0 修复 5-15 个，每个独立 commit + 单测覆盖（如适用）+ RED 反向探针（§9.3 强制）| ⚠️ 条件 |
| A14 | ctest 1061+ → 1061+N PASS（继承基线不退化）| ⚠️ 条件 |
| A15 | Release `-O3 -Werror` 0 err/warn | ⚠️ 条件 |
| A16 | `bench_*` baseline 不退化超 +5%（继承基线 — R2 不应触发 hot path 性能改动）| ⚠️ 条件 |

---

## §4 决策矩阵（Decision Matrix，已锁定）

### V 决策（VAN 阶段已锁，详见 `tasks.md`）

| # | 维度 | 选择 |
|:-:|---|---|
| V1 | 范围 | A 全代码库（7 子系统）|
| V2 | 维度 | all 6 维（perf / correct / maint / security / test / consistency）|
| V3 | 输出 | C 完整实施 → 多轮次 + Checkpoint |
| V4 | 视角 | D 混合（外部 reviewer + 内部 systemPatterns 验证）|
| V5 | 安全 | ✅ 是 |
| 策略 X | 多轮次 | R0+R1 必然 / R2 条件 / R3+ 拆出 |

### D 决策（PLAN 阶段头脑风暴产出）

| # | 维度 | 选择 | 理由 |
|:-:|---|---|---|
| D1 | R0 抽样深度策略 | **B 优先深扫 TOP-3** — foundation/{containers,strings} + script/ + core/{dom,html} 全文件深扫；其他 4 子系统 grep 模式 | 30+ archive 任务后多数子系统已经过深度沉淀；TOP-3 选项是「最久未深度审视」的，深扫 ROI 最高 |
| D2 | R1 报告章节组织 | **C 矩阵 + 优先级前置** | 先列 P0/P1/P2 总表方便用户审 Checkpoint 1 决策 R2 范围；按子系统的支持详情作附录便于深读 |
| D3 | 不足项「方案」深度 | **B 结构化描述**（问题 + 方向 + 工时 + 代码量，不含具体代码示例） | 报告中量级可控；R3+ 拆出独立 task 时再写完整 spec 即可 |
| D4 | 与既有 systemPatterns 对照 | **B 强制每项标注 3 状态**（已规则化 / 部分规则化 / 未规则化） | 避免与已沉淀规则重叠；显化「需新增规则」候选 |
| D5 | P0/P1/P2 分类标准 | 见 §6 单独段定义 | 必须显式定义避免歧义 |
| D6 | R2 上限 | **15 个 P0 上限** | Checkpoint 1 用户决定实际启动数量 |
| D7 | 元规则同步 review | **C 仅记长期项** | scope 收紧；元规则修改需用户亲自决策不能 AI 包办 |
| D8 | CVE 审计 | **C 跑 + 列长期项** | 本任务一次性给出当前状态 + 长期 audit 节奏 |
| D9 | 测试覆盖率深度 | **B lcov 实测**（line / branch / function 三维度） | 真实数据 vs grep 启发；lcov 1.14 已验证可用 |
| D10 | 是否用子代理 | **❌ 不用**（独 agent 跑） | 单 reviewer 任务 prompt 上下文传递成本 > 并行收益 |

---

## §5 抽样深度策略（D1=B 落地）

### TOP-3 深扫子系统选择依据

按「最近 6 任务（TASK-20260424-* 至 TASK-20260430-02）未深度涉及」排序：

| 子系统 | 最近 6 任务的涉及度 | 上次深度审视 | 选 TOP-3? |
|---|---|---|:-:|
| foundation/containers | ❌ 完全未涉及 | TASK-20260405-01（Foundation 初版） | ✅ |
| foundation/strings | ⚠️ TASK-19-04/07 仅修 -Werror | TASK-20260405-01（含 BasicString SSO 设计） | ✅ |
| script/（QuickJS） | ⚠️ TASK-18-01 修反向析构 / TASK-19-01 P2 收口 | TASK-20260413-01（首次集成）+ TASK-20260414-01（DOM 绑定） | ✅ |
| core/dom + core/html | ⚠️ TASK-26-01 R2 仅加 inline style 护栏 | TASK-20260405-03（DOM 树初版）+ TASK-20260405-04（HTML parser） | ✅ |
| core/css | ✅ TASK-30-02 刚动 | 不深扫（grep 即可） | ❌ |
| core/layout + core/render | ✅ TASK-30-01/26-01 刚动 | 不深扫 | ❌ |
| text + graphics/software | ✅ TASK-24-03/04 刚动 | 不深扫 | ❌ |
| platform | ✅ TASK-25-01 刚动 | 不深扫 | ❌ |
| api | ⚠️ TASK-25-01 加 SDL2 入口 | 不深扫（grep 即可） | ❌ |

**最终 TOP-3 深扫名单**（合并 dom + html 为 1 单元）：
1. `veloxa/foundation/containers/` + `veloxa/foundation/strings/`（合并：基础设施 13 文件）
2. `veloxa/script/`（QuickJS 集成 8 文件）
3. `veloxa/core/dom/` + `veloxa/core/html/`（合并：DOM/HTML parser 14 文件）

**4 个 grep 子系统**：
- `veloxa/foundation/{base,memory,log}/`（基础设施剩余）
- `veloxa/core/{css,layout,render,event,image}/` + `veloxa/text/` + `veloxa/graphics/`（最近活跃）
- `veloxa/platform/` + `veloxa/api/`（最近活跃）
- `tests/`（测试本身的盲区）

### 6 维度 fingerprint grep 模式（每维度 ≥ 5 关键字）

| 维度 | grep 模式（部分举例，R0 阶段精化扩展） |
|---|---|
| 性能 | `O\(N\^2\)\|TODO.*perf\|FIXME.*slow\|memcpy.*[0-9]+\|Vector\<.*\>.*push_back.*for\|find.*linear` |
| 正确性 | `TODO.*bug\|FIXME\|XXX\|HACK\|UB\|undefined.*behavior\|narrowing\|signed.*unsigned\|abs.*size_t` |
| 可维护性 | `magic.*number\|hard.*coded\|copy.*paste\|duplicate\|long.*function\|deeply.*nested` |
| 安全 | `gets\|strcpy\|sprintf\|system\(\|exec\|getenv\(\|atoi\|sscanf\|hardcoded.*proxy\|hardcoded.*token` |
| 测试 | `DISABLED_\|GTEST_SKIP\|skip\|disabled.*test\|TODO.*test\|FIXME.*test` |
| 一致性 | `inconsistent\|different.*style\|legacy\|obsolete\|deprecated.*not.*marked` |

**注：** 上表是初版示例，R0 阶段会扩展为 ≥ 5 × 6 = 30 关键字的完整 fingerprint。

---

## §6 P0/P1/P2 分类标准（D5 显式定义）

### 分类判定矩阵

| 优先级 | 修复成本 | 影响范围 | R2 是否启动 | 典型示例 |
|:-:|---|---|:-:|---|
| **P0** | ≤ 5 行 / ≤ 15 min | 单文件 / 不改公共 API / 不加测试也安全 | ✅ R2 内修 | 笔误 / 死代码 / TODO 已完成未删 / 命名不一致 / 注释错误 / `-Wxxx` 抑制可去 / magic number 提常量 |
| **P1** | 5-50 行 / 15-90 min | 单子系统 / 可能改公共 API / 必须加测试 | ❌ 拆 R3+ | 数据结构改造 / 算法优化 / API 重构 / 安全护栏增强 / 缺失边界条件处理 |
| **P2** | 50+ 行 / 90+ min / 跨子系统 | 多子系统 / 改架构边界 / 需独立 spec | ❌ 拆 R3+ | 跨子系统重构 / 新抽象层 / 依赖升级 / 大幅性能改造 |

### 分类边界规则

- **「修复成本」与「影响范围」二者取其大**：5 行修复但跨 3 子系统 → P2，不是 P0
- **安全相关项至少 P1**：即使 1 行修复涉及外部输入护栏，提升至 P1（强制独立验证）
- **测试覆盖率盲区**：默认 P1（添加测试本身可能 50+ 行）；除非是 1 个 trivial unit test 可补 → P0
- **CVE 审计发现**：依赖版本有明确 CVE 公告 → 至少 P1（视严重性可至 P0）；当前最新版本无 CVE → P3（长期 audit）

### R2 上限计算

- 总上限 15 个 P0
- 实际数量由 Checkpoint 1 用户决定（可能跳过 R2 直接 R3+ 拆出，可能限定 5-10 个）
- 单 P0 修复时间预算：5-15 min（实际不应超过此区间）

---

## §7 R1 报告章节框架（D2=C 落地）

### 主报告结构（`docs/specs/2026-04-30-codebase-review-report.md`）

```
## §0 执行摘要（1 页）
  - 总览：N 项不足 / P0=X / P1=Y / P2=Z / 元规则长期项=W
  - ROI 矩阵图（P0 列表前置，方便用户决策 R2）
  - 最重要发现 TOP 5（reviewer judgment 推荐优先关注）

## §1 P0 不足清单（按维度组织，用户审 Checkpoint 1 直接对照）
  | # | 文件:行 | 维度 | 描述 | 方案 | 工时 | 代码量 |
  |---|---------|------|------|------|------|--------|
  ...

## §2 P1 不足清单（按子系统组织）
  ...

## §3 P2 不足清单（按主题组织 — 跨子系统重构 / 依赖升级等）
  ...

## §4 与 systemPatterns 对照（A9 验收）
  | 不足项 ID | 涉及规则 | 状态 |
  |-----------|----------|------|
  | ...     | ...    | 已规则化 / 部分规则化 / 未规则化 |

## §5 测试覆盖率盲区（D9 输出）
  - 总体：line X% / branch Y% / function Z%
  - 各子系统覆盖率分布表
  - 未覆盖热点 TOP 10（按 LOC 加权）
  - 建议补测 P1 候选

## §6 第三方依赖 CVE 长期审计追踪表（A11 输出）
  - 7 依赖当前版本 + GitHub Security Advisories 数量 + 严重性分布
  - 长期 audit 节奏建议（季度 / 年度）
  - 升级候选清单

## §7 元规则潜在问题（D7=C 长期项，仅记不动文件）
  - 矛盾点 / 过时段 / 冗余规则
  - systemPatterns 中规则间的潜在冲突

## §8 ROI 矩阵 + Checkpoint 1 决策建议
  - 推荐 R2 启动 P0 子集（建议 5-10 个最高 ROI）
  - 建议拆出 R3+ 独立任务的 P1 排序（按 ROI 降序）

## 附录 A：抽样深度详情（哪些子系统全扫 / 抽样方式）
## 附录 B：grep fingerprint 完整结果（30+ 关键字 × 7 子系统）
## 附录 C：lcov 详细报告链接（HTML 报告生成在 build/coverage/）
```

---

## §8 安全维度威胁建模（V5 安全相关，T1-T7）

| # | 威胁面 | review 关注点 | 既有护栏（不动）| 新发现可能性 |
|:-:|---|---|---|:-:|
| T1 | DoS via 海量 declaration | HTML inline style count cap 1000 | TASK-26-01 R2 已加 | ⚠️ 低（已成熟）|
| T2 | DoS via 单 value 巨长 | HTML inline style value cap 8KB | TASK-26-01 R2 已加 | ⚠️ 低 |
| T3-T5 | 历史攻击向量（expression/behavior/javascript:）| `ContainsBlacklistKeyword` | TASK-26-01 R2 已加 | ⚠️ 低 |
| T6 | parser N-cap 缺失 | CSS shorthand / property 解析循环界 | TASK-30-02 已批量 review | ⚠️ 低 |
| **T7** | **新威胁面：QuickJS 集成** | JS callback 寿命 / opaque pointer 类型混淆 / `JS_NewClassID` 进程级状态泄漏 | TASK-18-01 + TASK-19-01 部分覆盖 | 🔴 **重点 review**（D1 TOP-3 含 script/） |
| **T8** | **新威胁面：FreeType / HarfBuzz 集成** | 字体输入未验证 / 大字体 file DoS / glyph index 越界 | 部分隐式护栏 | 🟡 **二级关注**（不在 TOP-3，但 D1=B 抽样深扫候选）|
| **T9** | **新威胁面：图片解码（libpng / libjpeg）** | 解压炸弹 / 损坏文件触发 UB | 当前未审计 | 🟡 **二级关注** |
| T10 | 第三方依赖 CVE | 7 依赖周期审计 | 当前未跑 | 🟡 D8=C 本任务跑 |

**结论**：T7-T10 是本 review 的安全维度新发现重点；T1-T6 已被既有护栏覆盖，仅做合规性二次确认，不重新设计。

---

## §9 工具链 / 测试策略 / 风险登记

### 9.1 工具链（D8/D9 落地）

| 工具 | 版本 | 用途 | 验证 |
|---|---|---|---|
| `lcov` | 1.14 | 覆盖率报告 | ✅ 已装 |
| `gcov` | 11.4 | 覆盖率原始数据 | ✅ 已装（gcc 11.4 自带）|
| `genhtml` | 1.14 | HTML 报告生成 | ✅ 已装 |
| `rg`(ripgrep) | — | grep fingerprint | 待 R0 验证（fallback 至 `grep -r`）|
| `gh`(GitHub CLI) | — | CVE 审计 GitHub Security Advisories API | 待 R0 验证（需 full_network 权限触发 + 仅 read API）|

### 9.2 测试策略

- **R0**：仅核验 ctest 1061/1061 基线 PASS（无新测试）
- **R1**：报告产出阶段无代码改动，零新测试
- **R2 P0 修复**：每 P0 必含 RED 反向探针（§9.3 强制）；trivial 改动（如笔误 / 注释）可豁免单测但必须 ctest 全过

### 9.3 风险登记

| # | 风险 | 概率 | 影响 | 应对 |
|:-:|---|:-:|:-:|---|
| R1 | R0/R1 时间膨胀（review 发现量超预期）| 中 | 中 | spec §3 上限封顶 R0+R1+R2；R3+ 强制拆出，本任务超时即止步 |
| R2 | 「样样不深」陷阱 | 中 | 高 | D1=B TOP-3 深扫策略 + 6 维度 fingerprint 双轨 |
| R3 | Spec 主报告超长（违反 TASK-30-02 描述粒度准则）| 低 | 中 | D2=C 矩阵 + 优先级前置；详细数据进附录 |
| R4 | CVE 审计需 full_network 权限失败 | 中 | 低 | fallback：手工查 GitHub Security Advisories 网页 + 记录截图 |
| R5 | lcov 报告生成失败（gcov 版本不兼容）| 低 | 中 | gcc 11.4 + lcov 1.14 验证可用；fallback 用 gcovr 替代 |
| R6 | TOP-3 深扫漏掉真正问题最多的子系统 | 中 | 中 | 4 grep 子系统的 fingerprint 应能捕获大部分严重问题；R0 末根据 grep 数量分布有权调整 TOP-3 名单 |
| R7 | 元规则发现需要主观判断（D7=C）| 中 | 低 | spec 仅列「候选讨论项」交用户决策；不擅自打分 |

---

## §10 R0/R1/R2 估时（plan 阶段精化）

| 阶段 | 内容 | plan 估时 | plan × 0.6 |
|:-:|---|---:|---:|
| R0 | 准备：fingerprint + lcov + CVE + 抽样名单 | ~80-100 min | ~48-60 min |
| R1 | 全代码库 review 报告 | ~240-300 min | ~144-180 min |
| Checkpoint 1 | 用户审 + 决定 R2 范围 | — | — |
| R2 | P0 quick fix（条件触发，5-15 个）| ~60-180 min | ~36-108 min |
| Checkpoint 2 | 用户审 + 决定 R3+ 拆分 | — | — |
| **合计上限** | R0+R1+R2 | **~6-10h** | **~3.5-6h** |

**估时档位判定**（按 systemPatterns「Level 4 多轮次」沉淀）：
- Level 4 多轮次目标 plan × 0.4-0.5×
- 本任务 review 类与 TASK-26-01 不完全可比（R 数较少 + 无创意阶段 + 无大子代理）
- 预期落 plan × 0.5-0.6×（中位档），属合理区间

---

## §11 与既有任务关系

| 关系 | 任务 ID | 描述 |
|---|---|---|
| 依赖 | 无 | review 任务无前置依赖 |
| 被依赖 | R3+ 拆出的多个独立后续任务 | 每个 P1 大件可能拆出独立 TASK-* |
| 关联 | TASK-26-01 / TASK-30-01 / TASK-30-02 | 既有护栏 / 沉淀规则的 review 对照基准 |
| 关联 | 8 项 P3 触发型候选 | 作为 R1 输入材料被首先审视；review 中可能确认 / 调整 / 合并 / 移除 |

### 8 P3 候选的 review 处理策略

| 候选 ID | review 处理 |
|---|---|
| TASK-26-02-full（clearance）| 列入 R1 报告 P2（已立项 + 设计已知，仅记录优先级 + 触发条件）|
| TASK-26-03（IFC 递归）| 列入 R1 报告 P2 |
| TASK-20260424-02（残余 super-linear）| 列入 R1 报告 P2 |
| CSS 4 逻辑属性 / `border-image` / `border-radius`| 列入 R1 报告 P2（CSS 子集扩展）|
| TASK-20260419-06（HashMap Hash Mixing）| 列入 R1 报告 P3（已 P3 降级，仅确认）|
| TASK-20260419-08（string memcpy noinline）| 列入 R1 报告 P3 |
| TASK-20260419-12（DrawText 真路径）| **重新评估**（K7 已被 TASK-24-04 隐式闭环？R1 报告显式确认或反驳）|

---

## §12 验收清单（一页速查）

R0 完成 → A1-A5 全过 → 产出抽样名单 + fingerprint + 覆盖率 + CVE 数据
R1 完成 → A6-A12 全过 → 产出主报告（含 P0/P1/P2 分类 + ROI 矩阵）
Checkpoint 1 → 用户审报告 + 决定 R2 启动数量（0-15 个 P0）
R2 完成（条件）→ A13-A16 全过 → P0 修复合并到 main 前
Checkpoint 2 → 用户决定 R3+ 拆分后续任务 ID（每个独立 Level 1-2）

---

> **「Spec 描述粒度准则」自我应用**（TASK-30-02 沉淀）
>
> 本 spec 描述「契约 + 约束」（review 范围 / 输出形态 / 优先级标准 / 工具链 / Checkpoint 协议），不描述具体实施细节（如「先 grep foundation/containers 再 grep script」）。R0/R1 阶段实施由 plan 阶段精化 + BUILD 阶段实证微调。
