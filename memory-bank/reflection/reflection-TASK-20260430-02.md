# 回顾：CSS border shorthand 补全（4 方向 + 3 属性级）

**日期：** 2026-04-30
**任务 ID：** TASK-20260430-02
**复杂度级别：** Level 2
**安全相关：** ✅ 是

---

## 1. 任务概览

补全 W3C CSS 2.1 §8 / §16 标准的 7 个 border shorthand：4 方向（`border-top` / `-right` / `-bottom` / `-left`）+ 3 属性级（`border-width` / `border-style` / `border-color`），全部展开为既有 12 longhand。零新 PropertyId / 零 enum 改动。

**双重价值：**
1. 直接消化 TASK-20260430-01 build 副发现 P3 候选（CSS parser `border-bottom` shorthand 缺失）
2. 自我应用 TASK-30-01 升级规则 §0「CSS shorthand 能力 grep 表」首次外部任务 ROI 验证

---

## 2. 计划 vs 实际

| 维度 | 计划 | 实际 | 偏差原因 |
|---|---|---|---|
| 任务数 | 8 Phase（P0 + R1.1/1.2/1.3 + R2.1/2.2/2.3 + P3）| 8 Phase 全过 | ✅ 一致 |
| 预估时间 | 170 min plan / 102 min plan×0.6 | **37 min**（21:48 → 22:25）| ⚠️ 实测 ÷ plan = **0.22×**，远低于 plan×0.6 准确档（连续第 15 数据点）|
| 测试数 | 25（12 R1 + 13 R2）| 22（10 R1 + 12 R2）| 删除冗余的 12 项「BorderTop_ReverseProbe TEST 占位」（实际反向探针是运行时三态切换程序，不是 TEST() 函数）+ 「ExistingBorderShorthand_NoDegradation」（与既有 BorderShorthand 测试重复）|
| 文件变更 | parser.cc + parser_test.cc + 4 MB + 2 docs | parser.cc（+212）+ parser_test.cc（+234）+ 4 MB + 2 docs | ✅ 一致 |
| 实施模式 | Spec §5.3/5.4：4+3 = 7 个独立 if 分支 | **2 个聚合分支**（4-direction 单分支 + 3-property 单分支 + Mode enum 分派）| ⚠️ 良性偏差 — 实施时识别「与既有 margin/padding 单分支聚合模式一致」更优；LOC 减半（212 vs 预计 350）+ 可读性更高；不悖 D1/D2=A「不引入 helper / template / function pointer」精神 |
| Commits | 5 GREEN 路径 + 1 finalize | 5 GREEN 路径 + 1 finalize | ✅ 一致（粒度 100% 匹配 plan §「估时与提交」表）|
| 反向探针 | ≥ 2 处（R1.3 + R2.3 各 1）| 2 处完整三态切换 + 实测 FAIL 证据 | ✅ 一致（D3=C 完整档 §9.3 强制） |
| ctest 期望 | 1064（基线 1039 + 25）| Debug 1061（1039 + 22）/ Release 1030 | ✅ 22 vs 25 的差为预期内（测试合并） |

---

## 3. 做得好的

### 3.1 D1+D2=A 复制粘贴决策极度有效

PLAN 阶段 5 决策矩阵全部锁定 A/A/C/A/A，BUILD 阶段 0 调整。"复制粘贴 + 既有范本 100% 同模式"在「Level 2 + 模式复用」场景胜过 helper 抽取的所有诱惑：
- 既有 `border` shorthand 范本（parser.cc:517-597）100% 复用到 4 方向
- 既有 `margin/padding` 1-4 值展开范本（parser.cc:454-514）100% 复用到 3 属性级
- 与既有 `if (margin || padding)` 单分支聚合模式自然推广为「单分支 + name/Mode 映射」

### 3.2 升级规则 §0「CSS shorthand 能力 grep 表」首次外部 ROI 验证 ✅ 高 ROI

TASK-30-01 沉淀的 §0 grep 表规则在本任务作为首次外部任务自我应用：
- P0 §0.2 G1-G6 grep 全部命中（既有 `border` / `padding/margin` 范本 + 12 longhand + 测试范本 + 上游护栏 + fingerprint 锁定）
- 设计阶段无任何「假设 vs 现实不符」意外 — 整个 PLAN/BUILD 路径 0 偏离
- 这是 TASK-30-01 build 副发现 P3 候选「border-bottom shorthand 缺失」的根因预防 — 规则有效阻断同类问题再次发生

### 3.3 §9.3 反向探针执行模板已形成肌肉记忆

R1.3 + R2.3 双探针严格按「破坏 → 编译 → ctest FAIL → 实测证据 → 恢复 → ctest PASS → 工作树干净」六步执行：
- R1.3：mis-route `pid_w = kBorderBottomWidth` → `BorderTopShorthand_FullValue` 实测 `property = 26 ≠ 期望 24`
- R2.3：mis-route 2-value 展开 `top=values[1] / right=values[0]` → `BorderWidthShorthand_TwoValues` 实测 `top != 1.0f`

证据全部用 ctest 输出原文记录，零拍脑袋。

### 3.4 单分支聚合发明（实施模式良性偏差）

Spec §5.3/5.4 描述 7 个独立分支，BUILD 时识别「既有 margin/padding 也是单分支聚合 + name 区分」的天然模式，自然推广为：
- R1：单分支 4-condition + name → PropertyId 三元组映射（96 LOC，预计 200 LOC，节省 52%）
- R2：单分支 3-condition + Mode enum 分派 value parser + Mode → PropertyId 三元组映射（116 LOC，预计 150 LOC，节省 23%）

总 LOC 212 vs 预计 350，节省 39%。**这不是 helper 抽取（D1/D2=A 决策不变），而是「if 多 condition 聚合」的既有惯用法推广**。

### 3.5 威胁建模 T1-T8 完整覆盖 + 零新护栏需求

PLAN 阶段威胁建模产出明确「上游 HTML inline style 三件套 + 既有 parser 内部 N-cap 复用」即覆盖全部威胁面，BUILD 阶段印证：
- T6（3-iter cap）/ T8（4-iter cap）通过 `BorderTopShorthand_NCapSecurity` + `BorderWidthShorthand_NCapSecurity` 双专测验证
- T7（over-match 误绑）通过 `EqualsIgnoreCase` 严格相等天然防御 + `BorderTopWidthLonghand_NotShorthandPath` sentinel 防御
- 零新护栏需求 ✅

---

## 4. 遇到的挑战

### 4.1 无重大挑战

整个 BUILD 全程 0 调试 / 0 意外 / 0 RED→GREEN 反复。这是 plan × 0.6 0.22× 极速档的根因。

### 4.2 唯一小决策点：单分支聚合 vs 7 独立分支

BUILD 阶段 R1.2 实施时 ~30 秒识别「与既有 margin/padding 同模式 → 单分支聚合」，未与 spec 偏差冲突，因为：
- D1/D2=A 决策的实质是「不引入 helper / template / function pointer」（避免类型擦除复杂度）
- 「if 多 condition 聚合」是既有代码惯用法，不是 helper 抽取
- 所以这是 spec 形式 vs 实质的良性偏差，不需要回到 PLAN

未来同类任务可在 PLAN 阶段 spec §5 直接写「仿既有 X 同模式实施」而非具体分支结构。

---

## 5. 经验教训

### 5.1 plan × 0.6 第 15 数据点：极速档 0.2-0.3× 频率定型契机

历史 14 数据点统计：

| 任务 | plan ÷ 实测 | 类型 |
|---|:-:|---|
| TASK-30-01 整体 | 0.46× | Level 3 复杂 |
| TASK-30-01 P6（最简一阶段）| 0.21× | Level 3 极速 |
| TASK-26-01 R2 | 0.43× | Level 4 复杂 |
| TASK-30-02（本任务）| **0.22×** | **Level 2 + 100% 模式复用** |

**新模式定型候选**：「Level 2 单子系统 + 模式 100% 复用 + 0 创意决策 + 0 新护栏」场景的 plan × 0.6 准确档应为 **0.2-0.3× 极速档**（而非 0.4-0.6× 准确档）。

这是连续第 N 次此类场景被 plan 估时高估，已经构成统计反复模式，可固化到规则。

### 5.2 spec 描述粒度应聚焦「效果」而非「形式」

Spec §5.3/5.4 描述「7 个独立 if 分支」是过度具体化的实施形式，BUILD 时识别更优实施（单分支聚合）后产生形式偏差。改进方向：

```diff
- spec §5.3：「按字母序插入 4 个独立分支：border-bottom → border-left → border-right → border-top，每分支 ~50 LOC，与既有 border 分支同模式」
+ spec §5.3：「实施 4 方向 shorthand 解析逻辑，与既有 border 分支同模式（width/style/color 任意顺序，3-iter cap），最终 push 3 longhand declaration（按 name 决定方向）。具体分支结构（4 独立 vs 1 聚合）由 BUILD 阶段决定，要求：(a) 不引入 helper/template/function pointer；(b) 与既有 shorthand 风格一致」
```

效果：spec 锁定「契约 + 约束」，让 BUILD 阶段「写最少代码」原则自然驱动最优实施。

### 5.3 「升级规则首次外部任务 ROI」是高价值评估

TASK-30-01 沉淀的 4 条规则中，§0「CSS shorthand 能力 grep 表」+ §0「既有测试隐式契约 fingerprint」在本任务都触发并产生显著价值（P0 阶段 0 意外 + fingerprint 锁定预防退化）。这印证 reflection→规则升级闭环的有效性。

未触发的 §9.4 递归算法决策 + creative.md §d.2 累积语义因任务类型不匹配而不可证伪 — 不是规则失效，是适用边界外。

---

## 6. 流程改进

| 维度 | 评估 | 备注 |
|---|---|---|
| 头脑风暴是否充分 | ✅ 充分 | D1-D5 5 决策矩阵全锁定，全部 A/A/C/A/A，BUILD 阶段 0 调整 |
| 计划是否足够详细 | ⚠️ 过度详细 | spec §5.3/5.4 形式过具体，应改为「契约 + 约束」（见 §5.2 改进建议）|
| TDD 流程被遵守 | ✅ 严格 | 每 Phase RED→GREEN→反向探针，commit 粒度与 plan 100% 匹配 |
| 代码审查捕获问题 | ✅ 自查 | ReadLints 全程 0 警告；commit 历史清晰 |
| §9.3 反向探针执行 | ✅ 模板化 | 双探针严格按六步执行，证据 ctest 原文记录 |

---

## 7. 技术改进

### 7.1 代码质量

- parser.cc +212 LOC，与既有风格 100% 一致（单分支聚合 + name 映射 + 既有 N-cap 模式 + 既有 ParseLengthOrPercent / ParseColor 复用）
- parser_test.cc +234 LOC，22 新测试覆盖 D3=C 完整档（happy / partial / out-of-order / important / dual-entry / NCapSecurity / sentinel）
- 0 lint 警告 / 0 Release `-O3 -Werror` 警告

### 7.2 测试覆盖度

- ctest Debug 1061/1061 PASS（基线 1039 + R1 10 + R2 12）
- ctest Release 1030/1030 PASS
- 双反向探针完整三态切换证据
- 既有 BorderShorthand fingerprint 不退化

### 7.3 技术债务

❌ 无新技术债务。

但记录 P3 触发型独立候选（不是债务，是后续机会）：
- CSS 4 标准 `border-block` / `border-inline` 逻辑属性 shorthand（独立任务）
- `border-image` / `border-radius` 简写（独立任务）
- W3C `border-bottom: 1px solid black` 等等历史副发现已通过本任务消化

---

## 8. 安全评估

| 维度 | 状态 | 备注 |
|---|:-:|---|
| 输入验证 | ✅ | T6 (3-iter cap) + T8 (4-iter cap) + InvalidIdent 拒绝（border-style: foobar → 0 decl）|
| 认证/授权 | N/A | CSS parser 无认证语义 |
| 数据保护 | N/A | 无敏感数据 |
| 依赖审计 | N/A | 零新依赖 |
| 错误信息脱敏 | N/A | 解析失败静默丢弃，无错误信息泄露 |
| 敏感数据处理 | N/A | 无敏感数据 |

威胁建模 T1-T8 完整覆盖：
- T1-T5：✅ 上游 HTML inline style 三件套（kInlineStyleMaxValueLength 8KB / kInlineStyleMaxDeclarationCount 1000 / ContainsBlacklistKeyword）
- T6/T8：✅ 既有 parser 内部 N-cap 复用 + 双 NCapSecurity 专测验证
- T7：✅ `EqualsIgnoreCase` 严格相等天然防御 + sentinel 验证

**结论**：零新护栏需求（D4=A 决策最终验证有效）。

---

## 9. A8 评估：TASK-30-01 升级规则首次外部任务 ROI

| 规则 | 触发场景 | 本任务 ROI | 备注 |
|---|---|:-:|---|
| `writing-plans.mdc` §0「CSS shorthand 能力 grep 表」 | 自我应用（本任务自身是该规则应用样板）| ✅ **高** | P0 §0.2 G1-G6 grep 全命中，PLAN 阶段 0 假设漏洞 |
| `writing-plans.mdc` §0「既有测试隐式契约 fingerprint」 | 既有 `BorderShorthand` 12-decl 期望反向 grep | ✅ **中** | parser_test.cc:321 fingerprint 锁定，BUILD 阶段不退化预防 |
| `writing-plans.mdc` §9.4「递归算法 API 决策必检项」 | 不触发（CSS parser 非递归）| ⊘ N/A | 适用边界外，不可证伪 |
| `creative.md` §d.2「算法伪码累积语义 explicit method」| 不触发（本任务无算法伪码）| ⊘ N/A | 适用边界外，不可证伪 |

**A8 结论**：✅ **2/4 触发 + 触发部分均高/中 ROI**，规则有效，建议保留 + 推广到同类 CSS / 解析器任务。

---

## 10. 反复模式识别

| 已知模式 | 频率 | 本次重复? | 备注 |
|---|:-:|:-:|---|
| 计划文件清单与实际变更不一致 | 9+ | ❌ | 文件清单 100% 一致 |
| 子代理产出需大量返工 | 7+ | N/A | 未用子代理 |
| 前置依赖/环境/API 能力未验证 | 8+ | ❌ | TASK-30-01 §0 升级规则全程触发，0 假设漏洞 |
| 非默认路径遗漏验证 | 4+ | ❌ | dual-entry 双入口 + sentinel 双验证已覆盖 |
| 测试隔离问题 | 7+ | ❌ | 0 flaky |
| 提交粒度偏离计划 | 7+ | ❌ | 5+1 commits 与 plan 100% 匹配 |
| TDD 严格度与场景不匹配 | 11+ | ❌ | 严格 RED→GREEN→反向探针 |

### 10.1 新发现的反复模式：plan × 0.6 极速档定型

| 模式 | 频率 | 本次定型? | 候选规则升级 |
|---|:-:|:-:|---|
| **plan × 0.6 在「Level 2 + 模式 100% 复用 + 0 创意决策」场景持续高估**| 14+（含 TASK-30-01 P6 0.21× / 本任务 0.22×）| ✅ **首次定型** | 新增「极速档 0.2-0.3×」分类 |

**判据**：连续 14+ 数据点中，至少有 4 个落入 0.2-0.3× 区间，且全部满足同一类型条件。这构成统计反复模式，应固化到 `.cursor/rules/skills/writing-plans.mdc` 的 plan×0.6 估时模型。

---

## 11. 改进建议（附优先级与落实方式）

| # | 建议 | 优先级 | 落实方式 | 目标 |
|:-:|---|:-:|---|---|
| 1 | plan × 0.6 估时模型新增「极速档 0.2-0.3×」分类（适用 Level 2 单子系统 + 模式 100% 复用 + 0 创意决策 + 0 新护栏场景）| P1 | 修改 `.cursor/rules/skills/writing-plans.mdc` plan×0.6 估时段，加入极速档判定条件 | writing-plans.mdc |
| 2 | spec 实施模式描述粒度准则：聚焦「契约 + 约束」而非「具体分支结构」，让 BUILD 阶段「写最少代码」原则自然驱动最优实施 | P2 | 记入 `memory-bank/systemPatterns.md`「Spec 描述粒度准则」段；为 future spec 模板提供示范 | systemPatterns.md |
| 3 | TASK-30-01 §0「CSS shorthand 能力 grep 表」+ §0「既有测试隐式契约 fingerprint」首次外部 ROI 验证为 ✅ 高/中 ROI，建议保留并推广到同类 CSS / 解析器任务 | — | 仅评估记录，不需新落实 | reflection §A8 |

---

## 12. 关键发现摘要

1. **plan × 0.6 第 15 数据点 0.22×** — 与 TASK-30-01 P6（0.21×）一同定型「极速档 0.2-0.3×」分类，应升级到 `writing-plans.mdc`
2. **D1+D2=A 复制粘贴决策极致有效** — Level 2 + 模式复用场景下 helper 抽取的所有诱惑都应被否决；「单分支聚合 + name/Mode 映射」是既有代码惯用法的自然推广
3. **TASK-30-01 升级规则首次外部 ROI 验证 ✅** — §0「CSS shorthand 能力 grep 表」+「既有测试隐式契约 fingerprint」高/中 ROI，规则有效
4. **spec 描述粒度反思** — §5.3/5.4 过度具体化分支结构，应改为「契约 + 约束」，避免 BUILD 阶段良性偏差与 spec 形式失配

---

**回顾人：** AI Agent
**回顾日期：** 2026-04-30
**下一步：** `/archive`（落实 P1 建议 #1 后归档）
