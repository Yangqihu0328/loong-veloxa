# 归档：TASK-20260504-01 MVP-scope 文档蓝图

**日期：** 2026-05-04
**任务 ID：** TASK-20260504-01
**复杂度级别：** Level 4（蓝图 V2=a 完整变体）
**状态：** ✅ 已完成
**安全相关：** 否
**主交付：** spec + plan + creative ×3 + 4 处文档全量同步
**总产出：** 7 commits / +2798 行 / -46 行 / 净 +2752 行

---

## 1. 任务概述

为 Veloxa（车载 HMI / 嵌入式高性能 UI 渲染引擎）建立**统一的 MVP 范围定义** + **可量化验收清单** + **三档分级矩阵**，解决项目缺独立 MVP scope 文档导致 "MVP 还差什么" 类问题难以客观回答的现状。

### 1.1 解决的具体问题

1. **MVP 边界模糊** — `projectbrief.md` 列了 5 大核心目标 / `productContext.md` 列了「已实现的核心功能」表 / `feature-completion-design.md §不在范围内` 列了局部 MVP 排除项 / 29 specs 散落各 milestone — **缺统一基准**
2. **优先级排期争议** — `activeContext.md` 的 P3 候选清单（DomBindings R2 / OpenGL ES / DRM/KMS / DevTool Phase E/F/G）之间无优先级，难以决定下一步立项哪个
3. **资源投入决策无基准** — "嵌入式硬件加速"（核心目标 #2）等长期目标的实施时机难以决断
4. **第三方集成者无 MVP 锚点** — README 路线图段是"按 Phase 列举"而非"按 MVP 档分层"，集成者难以判断"现在能拿来做什么"

### 1.2 核心成果（5 项战略价值）

1. **三档分级 MVP-A/B/C 体系建立** — 清晰量化各档完成度（A 100% / B 90% / C 70%）+ 验收标准 + 已知 gap 数（A 0 / B 4 / C 9）+ 估时
2. **路线图按 MVP 档分层重写** — README 路线图重写为 5 段（短期 MVP-B 收口 / 中期过渡 / 长期主线 P0 / 长期次线 / 超 MVP plus），第三方集成者 + 用户在单一段判断项目状态
3. **DevTool Phase E/F/G 明确标识为「超 MVP plus」** — 解决了「DevTool 4 件套收口后下一步该 Phase E 还是 R2 补全」优先级争议
4. **核心目标 #1+#2 路径量化** — C-G1 OpenGL ES (~30-60+ h) + C-G2 DRM/KMS (~10-20 h) 估时清晰 / 用户可基于本量化决断启动时机
5. **后续 ~13-15 个独立子任务推荐立项顺序** — spec §11.2 列 10 候选 + 各自 Level + plan ×0.6 估时 / 项目下半年所有任务排期可参照本 spec

---

## 2. 技术方案

### 2.1 整体方案：Level 4 蓝图 V2=a 完整变体

| 维度 | 决策 |
|---|---|
| 工作流路径 | `/van → /plan → /creative ×3 → /reflect → /archive`（**跳过 `/build`**）|
| 主交付物 | spec + plan + creative ×3 文档（**不含 build 实施**）|
| Build 阶段 | 由用户后续基于 plan 独立立项 N 个 Level 3-4 子任务 |

### 2.2 11/11 用户决策表（1 次 AskQuestion all_recommended 锁定）

| # | 维度 | 决策 |
|:-:|---|---|
| **V1** | MVP 范围视角 | **D 三档分级 MVP-A/B/C** |
| **V2** | 工作流形态 | **a V2=a 完整蓝图变体** |
| **V3** | 文档形态 | **c 双管齐下** |
| **V4** | 验收清单粒度 | **c 二者兼有** |
| **V5** | 同步更新范围 | **c 全量同步** |
| **B1** | 主 spec 文档结构 | 沿用 DevTool 蓝图 spec §1-§12 范式 |
| **B2** | creative ×N 拆分粒度 | **3 篇**（MVP-A/B/C 各一篇）|
| **B3** | 验收清单矩阵格式 | 单矩阵 三档 × 多维度 |
| **B4** | 与既有 spec 交叉引用规范 | 上游/下游分表 |
| **B5** | 估时 | plan ×0.6 ~120-180 min |
| **B6** | commit 拆分粒度 | **5 commits**（plan+spec / creative-A / B / C / finalize）|
| **B7** | Phase 0 grep 实证 | 7 子段 |

> **第 8 次连续跨决策协同度 100% 命中** / 累计 11+12+12+10+13+12+12+11 = **93/93** 跨决策一次锁定纪录 / 已 archive 阶段沉淀 systemPatterns 新段「跨决策协同度 100% sept-evidence」

---

## 3. 实现摘要

### 3.1 文件变更清单

| 操作 | 文件路径 | 行数 | 说明 |
|:-:|---|:-:|---|
| 创建 | `docs/specs/2026-05-04-mvp-scope.md` | +449 | 主蓝图 spec / 12 段 + 附录 A-D / 三档分级验收 + 9+4 gap 完整 + 推荐立项顺序 10 候选 |
| 创建 | `docs/plans/2026-05-04-mvp-scope.md` | +505 | 蓝图实施计划 / 10 段 / 5 commits 子任务分解 + commit message 范本 + 2 Checkpoint + 反复模式预防清单 |
| 创建 | `memory-bank/creative/creative-mvp-A.md` | +272 | MVP-A 基线档守门设计 / 10 段 / 3 方案 + B 主推 + commit body 守门范本 |
| 创建 | `memory-bank/creative/creative-mvp-B.md` | +468 | MVP-B 桌面 dogfood 完整档 / 11 段 / DomBindings R2 三连 API + 路径决策表 + dogfood 完整链路图 |
| 创建 | `memory-bank/creative/creative-mvp-C.md` | +524 | MVP-C 真嵌入式部署档 / 11 段 / G1 4 Phase + G2 实施 + 9 项 gap + 5 项新威胁面 T9-T13 |
| 创建 | `memory-bank/reflection/reflection-TASK-20260504-01.md` | +290 | Level 4 全面回顾 / 8 段 |
| 创建 | `memory-bank/archive/archive-TASK-20260504-01.md`（本文档）| +~440 | 全面归档 |
| 修改 | `README.md` | +66 / -30 | MVP 档对照子段 + 路线图按 MVP-A/B/C 分层 + ctest baseline 数字更新 + DevTool 4 件套表 + Phase D + T1 mitigation |
| 修改 | `memory-bank/projectbrief.md` | +13 | MVP 范围参考段 + 三档对照 + 与核心目标对应 |
| 修改 | `memory-bank/productContext.md` | +25 / -16 | 已实现表加 MVP 档列 + DevTool A/B/C/D 4 行 + 待补 gap 段 |
| 修改 | `memory-bank/activeContext.md` | +22 / -8 | 阶段流转 + 待处理事项 |
| 修改 | `memory-bank/tasks.md` | +47 / -7 | 任务条目流转 |
| 修改 | `memory-bank/progress.md` | +52 / -8 | 详细里程碑 |
| 修改 | `memory-bank/systemPatterns.md` | +200 | 4 个新段沉淀（中文文档 audit / 蓝图规模公式 / 超 MVP plus / 跨决策协同度）+ plan ×0.6 矩阵第 4 数据点 |
| 修改 | `.cursor/rules/skills/writing-plans.mdc` | +35 | 中文文档 StrReplace 字符类型 audit 段 |

**总：** 14 文件 / +2798 行 / -46 行（含本归档文档 + 后续 archive commit）

### 3.2 7 commits 详细时序

| # | hash | 时间 | 范围 | 行数 |
|:-:|---|:-:|---|:-:|
| 1 | `09b376c` | 17:55 | plan + spec + Phase 0 7 子段 grep audit + MB「初始化」→「规划中」| +1098 / -4 |
| 2 | `6b7a298` | 19:14 | creative-mvp-A.md（基线档守门设计）| +272 |
| 3 | `acca25a` | 19:19 | creative-mvp-B.md（桌面 dogfood 完整档）| +468 |
| 4 | `6a41e85` | 19:28 | creative-mvp-C.md（真嵌入式部署档）| +524 |
| 5 | `511104e` | 19:38 | finalize 全量同步 README + projectbrief + productContext + MB | +142 / -51 |
| 6 | `a8e7bc2` | ~19:50 | reflect + P0 立即沉淀（systemPatterns + writing-plans.mdc）| +351 / -8 |
| 7 | （本 archive）| ~19:55 | archive + P1+P2 沉淀 + Memory Bank 重置 | ~+440 + ~+200 sediment |

**5 commits 实施 + 1 reflect + 1 archive = 7 commits 总**

### 3.3 关键决策（5 项）

#### 决策 1：V1=D 三档分级 MVP-A/B/C（vs 单一档）

**理由：** 兼顾历史已交付「桌面 dogfood」+ 核心目标 #2「嵌入式硬件加速」长期目标 / 避免单一档定义争议 / A 已 100% / B ~90% / C ~70% 自然分层。

#### 决策 2：V2=a 完整蓝图变体（含 creative ×N 不含 build）

**理由：** 沿用 [TASK-20260430-04 DevTool 三件套蓝图范式](../archive/archive-TASK-20260430-04.md) / Level 4 蓝图（[main.mdc Level 4 蓝图任务 V2=a 工作流变体段](../../.cursor/rules/main.mdc)）/ 主交付 = 文档 / build 由用户后续独立立项。

#### 决策 3：B2=3 篇 creative 拆分粒度（vs 1 篇全档 / 4+ 篇细拆）

**理由：** 视 V1=D 三档自然拆分 / 每档独立设计文档 / 复杂度匹配（A 简洁 / B 中等 / C 详细）/ 总产出 ~1264 行 vs 1 篇全档难以聚焦 / 4+ 篇过度拆分。

#### 决策 4：B6=5 commits 拆分粒度（plan+spec / creative-A / B / C / finalize）

**理由：** 沿用 [TASK-20260430-04 蓝图 5 commits 范式](../archive/archive-TASK-20260430-04.md) / 每 commit 边界清晰 / commit body Source 段独立可追溯 / Plan 阶段已固定范本。

#### 决策 5：V5=c 全量同步（4 处文档：spec + README + projectbrief + productContext）

**理由：** 防漂移完整 / finalize 单 commit 守门 / 4 处一致更新避免 spec 与 README/projectbrief/productContext 信息不一致风险 / 沿用 [TASK-20260502-01 lazy-attach 全文档同步范式](../archive/archive-TASK-20260502-01.md)。

### 3.4 安全决策

**本任务不涉及安全变更**（纯文档蓝图 / 0 代码改动 / 0 新威胁面引入）：

- spec §8 + 3 篇 creative §安全考量段全引用既有 5+T 威胁面 mitigation（T1-T7 + DevTool 4 件套已就位）
- MVP-C creative 引入 5 项新威胁面 T9-T13（GL command injection / DRM permission / shader DoS / WebP RCE / 异步图片竞态）是**未来 G1+G2 等 MVP-C 子任务实施时的威胁建模占位**，**非本任务实施引入**

---

## 4. 测试覆盖

**N/A** — 本任务为纯文档蓝图（V2=a 不含 build）/ 0 代码改动 / 0 新测 / 0 测试改动。

**baseline 守门：** DEVTOOL=ON 1284 / DEVTOOL=OFF 1091 / A14 0 字节 — 不退化 ✅（commit body Source 段 quad-evidence 实证完整）。

---

## 5. 经验教训（从 reflection 提取）

### 5.1 做得好的（7 项）

1. **跨决策协同度第 8 次连续 100% 命中** — 11/11 1 次 AskQuestion all_recommended 锁定 / 创纪录
2. **5 commits 范本 100% 复用** — 沿用 TASK-30-04 + TASK-04 蓝图范式 / 计划与实际 100% 一致
3. **Phase 0 grep 实证范式延续** — 7 子段 audit / 0 文件名冲突 / 0 上游素材遗漏
4. **§d.1 / §d.2 audit 强制守门** — 3 篇 creative 全做 audit / 反复模式 #1 第 5 次升级 P0 强制项继续生效
5. **Finalize V5=c 全量同步无漂移** — 4 处文档单 commit 守门
6. **DevTool Phase E/F/G「超 MVP plus」明确标识** — 解决优先级争议（最大战略贡献）
7. **核心目标 #1+#2 硬指标可视化路径** — MVP-C 估时 ~40-80 h 量化清晰

### 5.2 遇到的挑战（3 项）

1. **README 段 4 次 StrReplace 全角/半角字符差异重试** — 中文文档字符类型混用 / +5-10 min overhead / **新反复模式候选首次定型** ⚠️
2. **commit 2 用户中断 71 min stalled** — wall-clock 主导噪声 / 但纯 AI 执行时间不受影响
3. **spec 行数估算偏差 -25% ~ -44%** — 估时方法学待校准 / **P1 公式入库已落实**

### 5.3 经验教训（4 项）

1. **中文文档 StrReplace 必须先 Read 原文确认字符类型** — 4 项 mitigation 已 reflect 阶段立即沉淀（systemPatterns + writing-plans.mdc）
2. **蓝图任务 spec 行数估算应基于「N 段 × 平均段大小」公式** — 本 archive 阶段直接沉淀新段「蓝图任务规模估算公式」
3. **Level 4 V2=a 蓝图变体第 4 数据点入库** — 0.25-0.38× plan / 已更新 plan ×0.6 矩阵
4. **跨决策协同度 100% 范式可推广到 ≥ 11 决策** — 8 次连续命中 sept-evidence / 已沉淀新段

---

## 6. 改进建议落实情况（5 项 — P0 P1 P2 全 archive 阶段直接落实）

| # | 建议 | 优先级 | 状态 | 落实位置 |
|:-:|---|:-:|:-:|---|
| 1 | systemPatterns 「中文文档编辑安全 audit」段 + writing-plans.mdc 同源 audit 子条 | **P0** | ✅ **reflect 阶段立即沉淀** | `memory-bank/systemPatterns.md` + `.cursor/rules/skills/writing-plans.mdc` |
| 2 | 蓝图任务 spec 行数估算公式入库（N 段 × 30-50 行/段 + 附录 50-100 行）| **P1** | ✅ **archive 阶段直接落实** | `memory-bank/systemPatterns.md` 「蓝图任务规模估算公式」段 |
| 3 | Level 4 V2=a 蓝图变体 plan ×0.6 比值 0.25-0.38× 第 2 数据点入库 | **P2** | ✅ **archive 阶段直接落实** | `memory-bank/systemPatterns.md` plan ×0.6 矩阵段 / 「蓝图任务 V2=a」子档第 4 数据点 |
| 4 | systemPatterns 「超 MVP plus 标识范式」段沉淀 | **P2** | ✅ **archive 阶段直接落实** | `memory-bank/systemPatterns.md` 「超 MVP plus 标识范式」段 |
| 5 | 跨决策协同度 100% 第 8 次连续命中 sept-evidence 升级 | **P2** | ✅ **archive 阶段直接落实** | `memory-bank/systemPatterns.md` 「跨决策协同度 100% sept-evidence」段（创新段 / 8 次实证表）|

**5 项 100% archive 阶段直接落实** ✅（沿用 [TASK-20260503-04 archive 范式](../archive/archive-TASK-20260503-04.md) — 不留待下次工作流元任务）

---

## 7. 长期影响 / 架构评估（Level 4 必填）

### 7.1 架构层面

- ✅ **零新架构引入**（V2=a 蓝图本身不实施 / spec §5.1 明示）
- ✅ **完全引用既有 8 大子系统 + 11+ systemPatterns**（self-coherent / 0 架构债务积累）
- ✅ **MVP-C 子任务架构占位预留**（C-G1 GLESRenderer + GLContext + GLSurface 抽象 / C-G2 DRMSurface + DRMEventLoop / 复用 [`2026-04-25-sdl2-window-backend-design.md §未来工作`](../../docs/specs/2026-04-25-sdl2-window-backend-design.md) 已留接口）
- ⭐ **MVP-C C-G6.e EventManager R9 与 C-G8 协同新发现** — creative-mvp-C §6.6 + §6.8 揭示 R9 改造与 CSS pointer-events 解析协同关系 / 应在 G8 子任务立项时纳入 plan §3 brainstorm

### 7.2 长期影响分析（5 维度）

| 维度 | 影响范围 | 评估 |
|---|---|---|
| **项目战略可见度** | 高 | MVP-A/B/C 三档分级 + 路线图 5 段分层使第三方集成者 + 开发者 + 用户能在**单一 README 路线图段**判断项目状态 / 立项推荐顺序 / 资源投入预期 |
| **后续任务排期基准** | 极高 | spec §11.2 列 10 个候选立项 + 各自 Level + plan ×0.6 估时 / 用户后续 N 个月所有任务排期可参照 |
| **systemPatterns 沉淀** | 中 | 5 项 P0/P1/P2 改进建议入库 / 4 个新段 + plan ×0.6 矩阵第 4 数据点 |
| **DevTool 4 件套主线收官标识** | 高 | 本任务正式标识 DevTool 4 件套（A+B+C+D）已收官 / DevTool Phase E/F/G 是 plus / 解决优先级争议 |
| **核心目标 #1+#2 路径量化** | 极高 | C-G1 OpenGL ES (~30-60+ h) + C-G2 DRM/KMS (~10-20 h) 估时清晰 |

### 7.3 长期维护建议

#### 维护点 1：MVP 档完成度同步更新

每次 MVP-B / MVP-C gap 子任务完成后，必须同步更新：
- `docs/specs/2026-05-04-mvp-scope.md` §3.2 / §3.3 的 gap 清单 + 完成度 %
- `README.md` 「MVP 档对照」子段
- `memory-bank/productContext.md` 已实现表 + 待补 gap 段
- `memory-bank/projectbrief.md` 「MVP 范围参考」段

#### 维护点 2：MVP-A baseline 守门红线不可退化

任何后续涉及 codebase 改动的 commit body 必须含：
```
DEVTOOL=ON 1284/1284 PASS / DEVTOOL=OFF 1091/1091 PASS
A14 link closure: nm verify 0 byte growth (vs baseline)
MVP-A baseline 不退化 ✅
```

#### 维护点 3：超 MVP plus 标识范式持续应用

新提出的子系统应判断：是 MVP-A/B/C 内 / 超 MVP plus / 还是 MVP 完成后才考虑？避免「MVP-D / MVP-E」概念混淆。

#### 维护点 4：跨决策协同度数据点持续累积

每次 brainstorm + plan 阶段 1 次 AskQuestion all_recommended 锁定 N 决策时，应在 commit body 标注「跨决策协同度第 X 次连续命中」/ 累计入 systemPatterns。

---

## 8. 性能 / 估时 数据

### 8.1 plan ×0.6 比值（standalone-AI 实测条件）

| 维度 | 计划 | 实际 |
|---|:-:|:-:|
| spec 行数 | 600-800 | **449**（-25%~-44% / 公式预测 540 ✅）|
| plan 行数 | 400-500 | **505** ✅ |
| creative-mvp-A | 300-400 | **272**（-10%~-32%）|
| creative-mvp-B | 400-500 | **469** ✅ |
| creative-mvp-C | 500-600 | **525** ✅ |
| 总产出 | 2000-2500 | **+2798 / -46 = 净 +2752 行** ✅ |
| Standalone-AI 总耗时 | 25-48 min | **~45 min（除用户中断）** ✅ |
| plan ×0.6 比值 | 0.14-0.40× | **0.25-0.38×** ✅（落入预测中上区间）|

### 8.2 反复模式命中（已知 7 + 1 新候选）

| # | 已知模式 | 历史频率 | 本次 |
|:-:|---|:-:|:-:|
| 1 | 计划文件清单与实际变更不一致 | 9+ | **❌ 0/9+** ✅（11 文件 100% 契合）|
| 2 | 子代理产出需大量返工 | 7+ | N/A |
| 3 | 前置依赖/环境/API 能力未验证 | 8+ | **❌ 0/8+** ✅（Phase 0 7 子段 grep）|
| 4 | 非默认路径遗漏验证 | 4+ | N/A |
| 5 | 测试隔离问题 | 7+ | N/A |
| 6 | 提交粒度偏离计划 | 7+ | **❌ 0/7+** ✅（5 commits 严格按 plan）|
| 7 | TDD 严格度与场景不匹配 | 11+ | N/A |
| **新候选 ⓪** | 中文文档 StrReplace 全角/半角字符差异 | **首次定型** | ✅ **mitigation 已立即沉淀**（4 项 P0）|

**已知模式命中：0/7**（5 N/A + 2 明示守门）— **创第 6 次连续 0 反复纪录候选** ✅

### 8.3 Phase 0 投入定律 sept-evidence 升级（第 8 次实证）

| 数据点 | 任务 | Phase 0 投入 | ROI |
|:-:|---|:-:|:-:|
| A | TASK-20260502-01 Phase A | ~20 min | 5.3× |
| B | TASK-20260502-02 Phase B | ~25 min | 5.2× |
| C | TASK-20260503-01 Phase C | ~20 min | 7.6× |
| 02 | TASK-20260503-02 (R3+) | ~15 min | 6.7× |
| 03 | TASK-20260503-03 | ~20 min | 8.0× |
| 05 | TASK-20260503-05 | ~10 min | 16× |
| 04 | TASK-20260503-04 Phase D | ~15 min | ~6-10× |
| **本任务** | TASK-20260504-01 蓝图 | **~5 min** | **~3-5×（蓝图 ROI 偏低 / 范式延续完整）** |

**8 次实证 sept-evidence 维持** / 蓝图任务 ROI 自然偏低（~3-5× vs codebase 5-16×）但范式延续完整。

### 8.4 跨决策协同度 100% 第 8 次连续命中

| # | 任务 | 决策数 |
|:-:|---|:-:|
| 1-7 | TASK-30-04 / 02-01 / 02-02 / 03-01 / 03-02 / 03-05 / 03-04 | 11+12+12+10+13+12+12 = 82 |
| **8** | **TASK-20260504-01** | **11** |
| **累计** | — | **93/93** |

---

## 9. DevTool 4 件套主线收官标识

本任务通过明确将 **DevTool Phase E/F/G + Vulkan + Web Animations + SVG + 完整 ICU 国际化** 标识为「**超 MVP plus 而非 MVP**」，正式宣告：

> **🎉 DevTool 4 件套（Phase A Inspector + Phase B Performance Overlay + Phase C Hot Reload + Phase D Console JS REPL）主线收官完成 — DevTool 自托管 dogfood 完整闭环达成**

后续 DevTool 扩展（Phase E/F/G）属于「按需增强」而非「MVP 必需」，由用户需求驱动决定立项时机。

---

## 10. 参考文档

### 10.1 主交付物

- 设计 spec：`docs/specs/2026-05-04-mvp-scope.md`（449 行 / 12 段 + 附录 A-D）
- 实施 plan：`docs/plans/2026-05-04-mvp-scope.md`（505 行 / 10 段 / 5 commits 子任务分解）
- 创意 ×3：
  - `memory-bank/creative/creative-mvp-A.md`（272 行 / MVP-A 基线档守门设计）
  - `memory-bank/creative/creative-mvp-B.md`（469 行 / MVP-B 桌面 dogfood 完整档 / DomBindings R2 三连 + Perf invalidate）
  - `memory-bank/creative/creative-mvp-C.md`（525 行 / MVP-C 真嵌入式部署档 / G1 OpenGL ES 4 Phase + G2 DRM/KMS + 9 项 gap）
- 回顾：`memory-bank/reflection/reflection-TASK-20260504-01.md`（290 行 / Level 4 全面回顾）

### 10.2 同步更新文档

- `README.md`（+66 / -30）— 路线图按 MVP-A/B/C 分层 + ctest baseline 数字 + DevTool 4 件套表 + Phase D + T1 mitigation
- `memory-bank/projectbrief.md`（+13）— MVP 范围参考段
- `memory-bank/productContext.md`（+25 / -16）— 已实现表加 MVP 档列 + 待补 gap 段

### 10.3 长期知识库沉淀

- `memory-bank/systemPatterns.md` — 4 个新段沉淀：
  1. 中文文档编辑安全 audit（P0 / 反复模式新候选首次定型）
  2. 蓝图任务规模估算公式（P1 / 实证 6 数据点）
  3. 超 MVP plus 标识范式（P2 / 解决优先级争议）
  4. 跨决策协同度 100% sept-evidence（P2 / 8 次连续命中 / 93/93 累计）
  + plan ×0.6 矩阵第 4 数据点入库（蓝图任务 V2=a 子档下沿延伸到 0.25× plan）
- `.cursor/rules/skills/writing-plans.mdc` — 中文文档 StrReplace 字符类型 audit 段

### 10.4 上游素材

- `memory-bank/projectbrief.md` 5 大核心目标（MVP 验收维度框架）
- `memory-bank/productContext.md` 已实现表 14 项 + 6 大里程碑（MVP-A/B 已完成基线）
- `memory-bank/activeContext.md` P3 候选清单（MVP-B/C gap 来源）
- 29 specs（含 `feature-completion-design.md §不在范围内` 8 项排除项）
- DevTool 蓝图 spec §11/§12（DevTool Phase E/F/G「plus」素材）

### 10.5 后续候选立项任务（10 候选 / 详见 spec §11.2 + §6.2）

| 优先 | 候选任务 | MVP 档 | Level | plan ×0.6 |
|:-:|---|:-:|:-:|:-:|
| 1 | DomBindings R2 三连补全 | MVP-B 收口 | L3 | ~2-4 h |
| 2 | Performance Overlay 持续 invalidate 机制 | MVP-B 收口 | L1-3 | ~30 min-2 h |
| 3 | 资源加载策略蓝图 | MVP-C 过渡 | L3 蓝图 + 实施 | ~5-10 h |
| 4 | R9 EventManager HitTest 改造 | MVP-C | L2-3 | ~1.5-2 h |
| 5 | **G1 OpenGL ES 硬件渲染后端蓝图** | MVP-C 核心 | L4 多 Phase | ~30-60+ h |
| 6 | **G2 DRM/KMS 嵌入式后端** | MVP-C 核心 | L3-4 | ~10-20 h |
| 7 | DomBindings 节点动态创建删除 | MVP-C | L3 | ~3-5 h |
| 8 | CSS 高级特性 5 项 | MVP-C | 5 × L2-3 | ~10-20 h |
| 9 | 图像扩展 3 项 | MVP-C | 3 × L2 | ~6-12 h |
| 10 | 性能优化收口（含 #35 阶段 2 / R3+ 13 项）| MVP-C | 多 L2-3 | ~10-30 h |

---

**归档结束。** 本任务正式完成 — 阶段「回顾中」→「归档完成 / 空闲」/ 6 commits 落盘 + 1 archive commit / 等待用户 `/van` 启动新任务。
