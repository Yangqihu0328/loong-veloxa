# 归档：CSS border shorthand 补全（4 方向 + 3 属性级）

**日期：** 2026-04-30
**任务 ID：** TASK-20260430-02
**复杂度级别：** Level 2
**安全相关：** ✅ 是
**状态：** ✅ 已完成

---

## 1. 任务概述

补全 W3C CSS 2.1 §8 / §16 标准的 7 个 border shorthand：

| 类型 | shorthand | 1-4 值规则 | 展开成 |
|---|---|---|---|
| 方向（R1，4 个）| `border-top` / `border-right` / `border-bottom` / `border-left` | 单边 width/style/color 任意顺序（仿 `border`）| 3 longhand × 1 边 |
| 属性级（R2，3 个）| `border-width` / `border-style` / `border-color` | 1-4 值（仿 `padding` / `margin`）| 4 longhand × 1 type |

全部展开为既有 12 longhand，零新 PropertyId / 零 enum 改动。

**双重价值：**
1. 直接消化 TASK-20260430-01 build 副发现 P3 候选（CSS parser `border-bottom` shorthand 缺失）
2. 自我应用 TASK-30-01 升级规则 §0「CSS shorthand 能力 grep 表」首次外部任务 ROI 验证

---

## 2. 技术方案

### 2.1 决策矩阵（D1-D5 全部 A/A/C/A/A 锁定）

| # | 维度 | 选择 | 理由 |
|:-:|---|---|---|
| D1 | 4 方向 shorthand 实现模式 | **A 复制粘贴** 既有 `border` 模板 | 与现有风格一致；reviewer 心智成本最低；helper 抽取的 30 LOC 收益不抵 template/lambda 引入新模式风险 |
| D2 | 3 属性级 shorthand 实现模式 | **A 复制粘贴** 既有 `padding/margin` 模板 | 3 个 value parser（Length/Enum/Color）类型异构，统一 helper 反而需要类型擦除 |
| D3 | 测试深度 | **C 完整档**（22 测试：10 R1 + 12 R2）| V5 安全 + §9.3 反向探针强制；双入口 + fingerprint 互斥不退化 |
| D4 | 安全护栏复用策略 | **A 完全复用** 既有 N-cap (3-iter / 4-iter) | 上游三件套 + 既有 N-cap 完整覆盖 T1-T8；零新护栏需求 |
| D5 | R1/R2 Phase 划分粒度 | **A 2 GREEN commits** | R1 4 shorthand 同模式 1 commit / R2 3 shorthand 同模式 1 commit |

### 2.2 实施模式（BUILD 阶段良性偏差 — 与 D1/D2=A 实质一致）

Spec §5.3/5.4 描述 7 个独立 if 分支，BUILD 阶段识别「与既有 margin/padding 单分支聚合模式一致」更优，自然推广为：
- **R1**：单分支 4-condition + name → PropertyId 三元组映射（96 LOC，预计 200 LOC，节省 52%）
- **R2**：单分支 3-condition + Mode enum 分派 value parser + Mode → PropertyId 三元组映射（116 LOC，预计 150 LOC，节省 23%）

总 LOC 212 vs 预计 350，节省 39%。**这不是 helper 抽取（D1/D2=A 决策不变），而是「if 多 condition 聚合」的既有惯用法推广**。

### 2.3 数据流（无改动）

```
CssParser::ParseDeclaration(out)
  ├─ name = NextNonWS()
  ├─ prop = PropertyIdFromName(name)            # longhand 直查
  ├─ if prop == kUnknown:
  │    ├─ if name == "margin" / "padding"       # 既有
  │    ├─ if name == "border"                   # 既有
  │    ├─ ★ if name == "border-{top,right,bottom,left}"  # 新增 R1
  │    ├─ ★ if name == "border-{color,style,width}"      # 新增 R2
  │    ├─ if name == "flex"                     # 既有
  │    └─ if name == "transition"               # 既有
  └─ 正常 longhand 路径（无改动）
```

---

## 3. 实现摘要

### 3.1 文件变更

| 操作 | 文件路径 | 说明 |
|---|---|---|
| 修改 | `veloxa/core/css/parser.cc` | +212 LOC（R1 单分支聚合 96 + R2 单分支聚合 116），与既有 margin/padding/border 风格 100% 一致 |
| 修改 | `tests/core/css/parser_test.cc` | +234 LOC，22 新单测（10 R1 + 12 R2），覆盖 D3=C 完整档（happy / partial / out-of-order / important / dual-entry / NCapSecurity / sentinel）|
| 创建 | `docs/specs/2026-04-30-css-border-shorthand-design.md` | 设计规格（11 段，含 A1-A8 验收 / D1-D5 决策矩阵 / T1-T8 威胁建模 / 6 风险登记）|
| 创建 | `docs/plans/2026-04-30-css-border-shorthand.md` | 实现计划（8 Phase / 25 测试 / 5 commits） |
| 创建 | `memory-bank/reflection/reflection-TASK-20260430-02.md` | 反思文档（12 段）|
| 创建 | `memory-bank/archive/archive-TASK-20260430-02.md` | 本归档文档 |
| 修改 | `memory-bank/systemPatterns.md` | P1 #1：「最窄路径」表新增「极窄档 0.2-0.25×」分类 + P2 #2：新增「Spec 实施模式描述粒度准则」段 |
| 修改 | `memory-bank/{tasks,activeContext,progress}.md` | 任务追踪 / 阶段切换 / 进度记录 |

### 3.2 关键决策

1. **D1+D2=A 复制粘贴**：与既有 shorthand 风格一致；reviewer 心智成本最低；helper 抽取的所有诱惑应被否决。
2. **D3=C 完整档**：V5 安全相关任务必须双入口 + 反向探针 + 安全 N-cap 专测。
3. **D4=A 完全复用既有 N-cap**：威胁建模 T1-T8 论证「零新护栏需求」最终验证有效（T1-T5 上游三件套 / T6/T8 既有 N-cap / T7 EqualsIgnoreCase 严格相等）。
4. **D5=A 2 GREEN commits**：R1/R2 多轮次 Phase 划分与 commit 粒度对齐，避免大杂烩提交。
5. **BUILD 实施模式**：单分支聚合（与既有 margin/padding 同模式）替代 7 独立分支，节省 39% LOC，不悖 D1/D2=A 实质（未引入 helper / template / function pointer）。

### 3.3 安全决策

V5 安全相关任务的安全设计：

- **威胁建模 T1-T8**：覆盖 DoS via 海量 declaration / 单 value 巨长 / 历史攻击向量 / parser 内部 token 循环 / over-match 误绑 / 4-value 解析下界绕过
- **零新护栏需求**：上游 HTML inline style 三件套（`kInlineStyleMaxValueLength = 8KB` / `kInlineStyleMaxDeclarationCount = 1000` / `ContainsBlacklistKeyword`）覆盖 T1-T5；既有 parser 内部 N-cap（3-iter for 方向 shorthand / 4-iter for 4-side shorthand）复用零修改即覆盖 T6/T8；`EqualsIgnoreCase` 严格相等天然防御 T7
- **专测验证**：`BorderTopShorthand_NCapSecurity`（T6）+ `BorderWidthShorthand_NCapSecurity`（T8）+ `BorderTopWidthLonghand_NotShorthandPath`（T7 sentinel）+ `BorderStyleShorthand_InvalidIdentRejected`（input validation）

---

## 4. 测试覆盖

### 4.1 单元测试（22 个新测试，全部 PASS）

**R1 方向 shorthand（10 个）：**

| # | 测试名 | 覆盖维度 |
|:-:|---|---|
| 1-4 | `Border{Top,Right,Bottom,Left}Shorthand_FullValue` | 4 方向各 happy path |
| 5 | `BorderTopShorthand_PartialNoColor` | 缺 color（2 declaration）|
| 6 | `BorderTopShorthand_OutOfOrder` | color/width/style 任意顺序 |
| 7 | `BorderTopShorthand_Important` | !important 透传 3 declaration |
| 8 | `BorderTopShorthand_DualEntry` | ParseDeclarationList 路径覆盖 |
| 9 | `BorderTopShorthand_NCapSecurity` | T6 威胁专测 |
| 10 | `BorderTopWidthLonghand_NotShorthandPath` | T7 sentinel |

**R2 属性级 shorthand（12 个）：**

| # | 测试名 | 覆盖维度 |
|:-:|---|---|
| 1-4 | `BorderWidthShorthand_{One,Two,Three,Four}Values` | 1-4 值规则矩阵 |
| 5-6 | `BorderStyleShorthand_{One,Four}Values` | enum 单值/四值 |
| 7-8 | `BorderColorShorthand_{One,Four}Values` | color 单值/四值 |
| 9 | `BorderWidthShorthand_DualEntry` | ParseDeclarationList 路径 |
| 10 | `BorderWidthShorthand_NCapSecurity` | T8 威胁专测（7 length tokens, cap=4）|
| 11 | `BorderStyleShorthand_InvalidIdentRejected` | input validation |
| 12 | `BorderColorShorthand_Important` | !important 透传 4 declaration |

### 4.2 §9.3 反向探针（双探针完整三态切换）

| Round | mis-route | 实测 FAIL 证据 | 恢复后 PASS |
|:-:|---|---|:-:|
| R1.3 | `border-top` 分支 `pid_w = kBorderBottomWidth` | `BorderTopShorthand_FullValue` `property = 26 ≠ 期望 24` | ✅ |
| R2.3 | `border-width` 2-value 展开 `top=values[1] / right=values[0]` | `BorderWidthShorthand_TwoValues` top != 1.0f | ✅ |

### 4.3 ctest 全量

- **Debug ctest 1061/1061 PASS**（基线 1039 + R1 10 + R2 12）
- **Release ctest 1030/1030 PASS** + Release `-O3 -Werror` **0 err / 0 warn**
- 既有 `BorderShorthand` fingerprint 不退化（parser_test.cc:321 `ASSERT_EQ(declarations.size(), 12u)` 严格通过）

### 4.4 A1-A8 验收

| # | 项 | 结果 |
|:-:|---|---|
| A1 | 4 方向 shorthand 双入口解析 | ✅ R1 10 测试（含 DualEntry）|
| A2 | 3 属性级 shorthand 1-4 值规则 | ✅ R2 12 测试 |
| A3 | 既有 `BorderShorthand` fingerprint 不退化 | ✅ ctest 1061+ |
| A4 | DoS 护栏 T6/T8 | ✅ NCapSecurity 双测试 |
| A5 | §9.3 反向探针 ≥ 2 处 | ✅ R1.3 + R2.3 完整三态 |
| A6 | Release `-O3 -Werror` 0 err/warn | ✅ |
| A7 | ctest 全量 PASS | ✅ Debug 1061 + Release 1030 |
| A8 | TASK-30-01 §0 升级规则首次外部 ROI | ✅ 2/4 触发 + 触发部分均高/中 ROI |

---

## 5. 实测耗时（plan × 0.6 第 15 数据点）

- Plan 估时：170 min（plan）/ 102 min（plan×0.6 准确档预期）
- 实测：~37 min（21:48 BUILD 启动 → 22:25 finalize 完成）
- **比例：实测 ÷ plan = 0.22×** — 与 TASK-30-01 P6（0.21×）一同**定型「极窄档 0.2-0.25×」分类**
- 推测原因：D1+D2=A 复制粘贴决策 + 既有 border / padding-margin 范本 100% 同模式 + 单分支聚合（仿 margin/padding 既有模式）大幅减少代码 + R2 全程 0 意外

---

## 6. 经验教训

### 6.1 关键发现 4 项

1. **plan × 0.6 第 15 数据点 0.22× 与 TASK-30-01 P6（0.21×）双数据点定型「极窄档 0.2-0.25×」分类**：识别清单 = 既有 same-subsystem 范本 100% 模式复用 + Level 2 单子系统 + 0 创意决策 + 0 新护栏需求（V5 安全相关时护栏复用既有）→ plan 估时按「中等」档给出时直接下调 50-78%
2. **D1+D2=A 复制粘贴决策极致有效**：Level 2 + 模式复用场景下 helper 抽取的所有诱惑都应被否决；「单分支聚合 + name/Mode 映射」是既有代码惯用法的自然推广，不是 helper 抽取
3. **TASK-30-01 升级规则首次外部 ROI ✅**：§0「CSS shorthand 能力 grep 表」+「既有测试隐式契约 fingerprint」高/中 ROI 验证（2/4 触发，§9.4 + creative.md §d.2 因任务类型不匹配不可证伪）
4. **spec 描述粒度反思**：§5.3/5.4 过度具体化分支结构，应改为「契约 + 约束」（已落实「Spec 实施模式描述粒度准则」段到 systemPatterns.md），让 BUILD 阶段「写最少代码」原则自然驱动最优实施

### 6.2 反复模式识别（vs 27 份历史回顾基准）

| 已知模式 | 频率 | 本次重复? |
|---|:-:|:-:|
| 计划文件清单与实际变更不一致 | 9+ | ❌ |
| 前置依赖/环境/API 能力未验证 | 8+ | ❌（升级规则 §0 全程触发，0 假设漏洞）|
| 提交粒度偏离计划 | 7+ | ❌（5+1 commits 与 plan 100% 匹配）|
| TDD 严格度与场景不匹配 | 11+ | ❌（严格 RED→GREEN→反向探针）|
| 测试隔离问题 | 7+ | ❌（0 flaky）|
| **plan × 0.6 极速档定型（新候选）** | 14+（含本次 0.22×）| ✅ **首次定型** → 已升级 systemPatterns.md |

### 6.3 改进建议落实状态（reflection §11）

| # | 建议 | 优先级 | 落实状态 | 目标 |
|:-:|---|:-:|:-:|---|
| 1 | systemPatterns.md「最窄路径」表新增「极窄档 0.2-0.25×」分类 + 识别清单 | P1 | ✅ 已落实 | systemPatterns.md |
| 2 | systemPatterns.md 新增「Spec 实施模式描述粒度准则」段 | P2 | ✅ 已落实 | systemPatterns.md |
| 3 | A8 ROI 评估记录（保留 + 推广同类 CSS / 解析器任务）| — | ✅ 仅评估 | reflection §A8 |

---

## 7. ROI 评估：TASK-30-01 升级规则首次外部任务

| 规则 | 触发场景 | 本任务 ROI | 备注 |
|---|---|:-:|---|
| `writing-plans.mdc` §0「CSS shorthand 能力 grep 表」 | 自我应用 | ✅ 高 | P0 §0.2 G1-G6 grep 全命中，PLAN 阶段 0 假设漏洞 |
| `writing-plans.mdc` §0「既有测试隐式契约 fingerprint」 | 既有 BorderShorthand 12-decl 期望 | ✅ 中 | parser_test.cc:321 fingerprint 锁定，BUILD 阶段不退化预防 |
| `writing-plans.mdc` §9.4「递归算法 API 决策必检项」 | 不触发（CSS parser 非递归）| ⊘ N/A | 适用边界外 |
| `creative.md` §d.2「算法伪码累积语义 explicit method」 | 不触发（无算法伪码）| ⊘ N/A | 适用边界外 |

**结论**：2/4 触发 + 触发部分均高/中 ROI，规则有效，建议保留 + 推广到同类 CSS / 解析器任务。

---

## 8. Commit 历史

```
1f4f89f docs(reflect): add reflection for TASK-20260430-02
bf89c34 chore(build): finalize TASK-20260430-02 memory bank state
1761b4f feat(css): TASK-20260430-02 R2 GREEN border-{width,style,color} shorthand
e2688a7 test(css): TASK-20260430-02 R2 RED tests for border-{width,style,color} shorthand
5378e67 feat(css): TASK-20260430-02 R1 GREEN border-{top,right,bottom,left} shorthand
f8ed62f test(css): TASK-20260430-02 R1 RED tests for border-{top,right,bottom,left} shorthand
18aa8c3 docs(plan): TASK-20260430-02 add design spec + impl plan + MB updates
```

7 commits（PLAN docs + R1 RED + R1 GREEN + R2 RED + R2 GREEN + finalize + reflect），与 plan §「估时与提交」表 5+1 GREEN 路径 + reflect 完全一致。

---

## 9. 参考文档

- 设计规格：`docs/specs/2026-04-30-css-border-shorthand-design.md`
- 实现计划：`docs/plans/2026-04-30-css-border-shorthand.md`
- 反思文档：`memory-bank/reflection/reflection-TASK-20260430-02.md`
- systemPatterns.md 长期沉淀：
  - 「最窄路径」表「极窄档 0.2-0.25×」分类（P1 #1 落实）
  - 「Spec 实施模式描述粒度准则」段（P2 #2 落实）

---

## 10. 长期维护建议

1. **同类任务参考**：未来 CSS shorthand 扩展任务（如 `background` 多值 / `font` shorthand）可直接参考本任务模式：grep 表 G1-G6 + 既有范本 100% 复用 + 单分支聚合 + 双反向探针。
2. **plan × 0.6 估时**：识别「极窄档」特征（既有 same-subsystem 范本 100% 复用 + 0 创意 + 0 新护栏）后按 plan × 0.22 预估，避免持续高估。
3. **威胁建模复用**：CSS parser 类任务的 T1-T8 威胁清单可作为模板，每次新任务只需验证「上游护栏 + 既有 N-cap」是否覆盖即可，零新护栏是常态。

---

**归档人：** AI Agent
**归档日期：** 2026-04-30
