# 回顾：TASK-20260430-01 first/last child margin collapse with parent

**日期：** 2026-04-30
**任务 ID：** TASK-20260430-01
**复杂度级别：** Level 3（单子系统 Layout + API 设计决策 + 跨函数 chain propagate）
**安全相关：** ❌ 否（纯 layout 算法，无外部输入 / 无认证 / 无新依赖）
**分支：** `feature/TASK-20260430-01-margin-collapse-parent`（基于 main `a84d30d`）

---

## 1. 计划 vs 实际

### 1.1 顶层维度

| 维度 | 计划 | 实际 | 偏差原因 |
|---|---|---|---|
| Phase 数 | 7（P0-P6）| 7 全部完成 | 0 |
| 估时 plan × 0.6 准确档 | 390 min × 0.6 = **234 min（~3.9h）** | 实测 BUILD 阶段约 **180 min**（约 3h，含 P0-P6.4） | **0.46× plan**（落「准确档 0.5-0.6×」与「中位档 0.37-0.50×」交界，第 14 数据点）|
| ctest 基线 → 终点 | 1029 → +10-15 = ≥1039 | 1029 → **1039/1039 PASS**（+10 cases） | ✅ 落点恰好为下限 +10 |
| Release `-O3 -Werror` | 0 warn | **0 warn**（Debug + Release 双通路） | ✅ |
| Bench `BM_LayoutBuildTreeFlat/64` ≤ +10% | A6 退出门 ≤ +10% | 同窗口 stash-swap **+7.55%**（mean）| ✅（A6 PASS）|
| Bench `BM_LayoutFlex<*>` ≤ +10% | A7 退出门 ≤ +10% | `<1,8> +5.84%` / `<8,8> +4.40%` / `<16,16> +4.94%` | ✅（A7 PASS）|
| W3C wpt 直接验证目标 | wpt-005 SKIP → PASS | **`Wpt005_NonSiblingAdjoiningMarginsCollapse` PASS** | ✅ |
| 单测 | 10 单测 + 3 反向探针 | **10 `A8_*` 单测 + 3 反向探针完整循环 + 1 wpt PASS** | ✅ 与 D4.B 完全对齐 |
| 文件清单 | 修 6 / 新 0 | 修 12 / 新 0 | +6（含 4 MB 文件 + spec/plan，spec §8.1 仅列代码侧）|
| 子代理 | 直执行（无 D3 标记）| 直执行 | ✅ |

### 1.2 实施时设计调整清单（plan vs build，本任务核心反思点）

| # | 微调点 | plan 原案 | 实证调整 | 严重度 |
|:-:|---|---|---|:-:|
| 1 | **D2 传递语义** | by-value in / by-value out（spec §5.4 伪码 `MarginChain incoming` 形参，by-value）| 改为 **in-out by-pointer in / by-value out**（`MarginChain* incoming`）— 多级 first-child margin propagate 需要 ancestor 在 child 完成后看到链路累积；by-value 在递归边界天然失效 | 🔴 高 |
| 2 | **隐式 BFC root** | spec §5.2.1 truth table 仅列 `padding-top != 0 / border-top != 0 / overflow != visible / first_child not in-flow`，未列 root / body 顶层 wrapper | 实施时为不退化 R3 既有 sibling 测试，必须把 `box->parent == nullptr`（document root）+ `box->parent->parent == nullptr`（html/body 顶层 wrapper）视为隐式 BFC root（与主流浏览器对齐）| 🟠 中 |
| 3 | **CSS parser `border-bottom` shorthand 缺失** | plan §1.2 测试 `BorderBottom_BlocksLastChildCollapse` 直接用 `border-bottom: 1px solid black`（VAN/plan 仅 grep 验证 `padding` 总体可解析）| build 时发现 css parser 不解析 `border-bottom` shorthand → 改测试名为 `PaddingBottom_BlocksLastChildCollapse` + 用 `padding-bottom: 1px` 等价物理分隔符；本任务 scope 内不修 parser bug | 🟡 低（独立 P3 候选）|
| 4 | **`MarginChain::MergeFrom`** | spec §5.4 算法伪码 `if (child->collapsed_through) cur_chain = child_outgoing` 直接覆盖语义 | build 时 A5 既有 sibling collapse-through 测试退化 → 加 `MarginChain::MergeFrom(other)` 用合并而非覆盖语义（max_positive / min_negative 双向取极值）| 🟠 中 |
| 5 | **性能优化 P6.2 hoisting** | plan 未规划专门优化 phase | bench `BM_LayoutBuildTreeFlat/8` 边缘超 +15%（nano 级 ~100ns）→ 把 end-of-loop O(N) `last_in_flow_block` scan 改 O(1) running pointer hoisting | 🟢 副产品收益 |
| 6 | **`collapsed_through` 判定不要求 `any_in_flow`** | spec §5.4 step 4 `IsCollapseThrough(box)` 隐含「需有 in-flow children」| build 时空 box（无 child）也应是 collapse-through（CSS 2.1 §8.3.1 R5 字面）→ 移除 `any_in_flow` 守卫 | 🟡 低 |

**集中表征：** 微调点 #1-#4 共同根因 = **「设计阶段未做多级递归 mental walkthrough + 既有测试隐式假设 fingerprint」**。

---

## 2. 回顾检查清单

### 代码变更类（主调）

- [x] **计划精确度** — Phase 数与 commit 数 1:1 对齐；文件清单 spec §8.1 列 6 改 / 实际改 12（含 4 MB 文件 + 2 文档），代码侧准确；估行数 layout_engine.cc `+120/-5` 实测 `+218/-47`，主要因 D2 by-pointer 调整 + 隐式 BFC root + MergeFrom 新增（+~80 行）
- [x] **TDD 执行情况** — 严格 RED→GREEN→REFACTOR；P1 三段 RED commits（P1.1/P1.2/P1.3 分提交，先 main flow 再 blocking 再 recursion）；P5 反向探针完整循环 3/3（探针 1 weakened blocks_top → #3 PaddingTop FAIL；探针 2 weakened blocks_bottom → #4+#6+#7 三项 FAIL；探针 3 weakened BFC check → #5 BFCRoot FAIL）；恢复后 1039/1039 PASS
- [x] **子代理质量** — 不适用（plan 阶段直执行决策；本任务单子系统 + 单线程递归依赖不适合并行 D3 拆分）
- [x] **测试隔离** — 10 `A8_*` 单测各自独立 fixture（无 SetUp 共享状态）；wpt-005 fixture 独立；零 flaky；ctest 全局 1039/1039 in 2.22-2.42s 稳定
- [x] **提交粒度** — 9 commits（VAN/PLAN/P1.1/P1.2/P1.3/P2-P5.2 GREEN/P6.1/P6.2/P6.4），每 commit 锁定一个 phase 边界（P2/P3/P4/P5.2 因强 GREEN 一致性合并为 1 commit `feat(layout): P2-P5.2 GREEN`）；提交粒度比 plan §6.x 估更紧凑（plan 估 7 commits，实际 9 commits 含 VAN/PLAN/finalize）
- [x] **非默认路径** — 阻断条件 5 路径全部 explicit 测试（padding-top / padding-bottom / BFC root / explicit height / min-height）+ collapse-through 跨边界 + deep chain 3 层 + outgoing chain 含 parent.mb 全验证

### Layout 类任务必检项（writing-plans.mdc §9.1，TASK-26-01 升级规则）

- [x] **跨 LayoutType 独立 box-model** — 本任务仅改 `LayoutBlock` → `LayoutBlockChild`，LayoutInline/Flex 路径调用 LayoutChild 行为零变更（A8/A9 验收 PASS）
- [x] **默认值边界** — `LayoutBlockChild` incoming 默认 empty（root + body 顶层 wrapper 强制 empty）；outgoing 在 blocks_bottom=true 时默认 empty + box.margin_bottom 独立累积
- [x] **wpt fixture 数值化适配** — wpt-005 用数值化断言（`gap = 40px = 2em`），不直接对 png ref；继承 R3 wpt-002/003 同范式

### Mixed TDD 反向探针（writing-plans.mdc §9.3，强制最小 ≥1 处）

- [x] **3/3 反向探针完整循环** — 超 §9.3 最小要求 3×；第 6 次实战覆盖三个独立 D3 维度（blocks_top / blocks_bottom / BFC root 三独立检测点）

---

## 3. 做得好的

### 3.1 同窗口 stash-swap bench 协议首次外部任务实战 ✅

TASK-26-01 R3 升级的「§7.0.1 同窗口对照」规则首次在外部任务命中验证：

- 操作：`git stash → 编 baseline.json → git stash pop → touch + 编 → 测 v1.json → diff`
- 实测全部 7 BM 一次过（median ~+5%，A6/A7 ≤ +10% 全 PASS），无跨时间窗误判
- 副产物：发现 `BM_LayoutBuildTreeFlat/8` 边缘 +15% 触发 P6.2 优化（O(N) → O(1) hoisting）
- ROI 验证：**规则有效，建议长期保留**；同窗口对照 CV 5-8% 区间为可信信号

### 3.2 TDD 严格度满分 + 3 反向探针完整循环

- P1 三段 RED 分提交（P1.1 主流程 / P1.2 阻断 / P1.3 递归），每段先验证 FAIL 再 commit；P1.2 5 个保护性不变量测试在 R3 baseline 已 PASS，GREEN 后继续 PASS，反向探针时 FAIL — 三态切换证实测试设计有效
- P5.3 反向探针 3/3 完整循环（破坏 → FAIL → 恢复 → PASS），覆盖 3 个独立 D3 维度（spec §7.3 完全实施）
- §9.3 强制规则在本任务做了 6× 多倍超额实战

### 3.3 决策矩阵 D1-D5 充分锁定，无需 /creative

PLAN 阶段头脑风暴 D1-D5 五个维度全部具体到选项级（D1.A1 / D2.A / D3.A / D4.B / D5.B），spec 给出算法伪码 + truth table + 状态字段，无 UI/算法/架构空白 → 直接 /build 不走 /creative，节省 ~30-45 min creative 阶段成本。

### 3.4 状态字段对称设计

`LayoutBox` 已有 R3 字段 `margin_top_collapsed_into_ancestor` + `collapsed_through`；本任务新增 `margin_bottom_collapsed_into_ancestor`，与 top 对称命名 + 同语义边界，零 ABI 风险，新代码自然消费。

### 3.5 wpt-005 SKIP→PASS 直接验证目标达成

第一首选验证目标（VAN F5 实证）一次过；fixture 几何完全按 plan §0.2 手算期望（`gap = 40px = 2em`）匹配。

### 3.6 性能优化副产品（P6.2 hoist last_in_flow_block）

bench 边缘超阈值是 trigger，但优化思路（end-of-loop O(N) → main loop O(1) running pointer）属于通用代码质量改进，对其他 layout 路径也有借鉴。Mixed bench 数据点 -9.84% 反向证明优化未引入 cache 抖动负面影响。

---

## 4. 遇到的挑战

### 4.1 [P0 教训] D2 by-value 决策在多级 propagate 场景天然失效

**现象：** plan §5.3 接口签名锁定 `MarginChain incoming`（by-value）；build P3 实施 first child collapse 写到第一层 grandparent → parent → child 三层递归时立即发现：grandchild 修改 chain 后，父层 LayoutBlockChild 拿不到累积值，物理化时 chain 已丢失。

**根因：** plan 阶段 mental walkthrough 仅推演了「单层 caller → callee」，未推演「Level N → Level N+1 → Level N+2」三层递归 + 「Level N+1 / N+2 修改 chain 后 Level N 物理化时是否需要看到」。

**修复：** D2 改为「in-out by-pointer in（`MarginChain* incoming`）/ by-value out（return MarginChain）」；调用站点 `LayoutBlockChild(child, w, ctx, &cur_chain)` + 子函数对 `*incoming` 做 Add；ancestor 物理化时直接读 `*incoming` 拿到完整链路累积。零 ABI 风险（POD 12B），编译器仍可 SROA 优化。

**反复模式匹配：** ⚠️ 已知模式 #3「前置依赖/环境/API 能力未验证」+「设计未充分代码级演练」（reflection-TASK-20260413-01 #2 / TASK-26-01 R4.6 反直觉 layout 默认值同源）— **第 9+ 次反复**。

**升级行动：** writing-plans.mdc §9 增补「API 传递语义决策必须做多级 mental trace」P0 强制条目（详见 §6 改进建议 #1）。

### 4.2 [P1 教训] spec truth table 未覆盖既有测试隐式假设的边界

**现象：** spec §5.2.1 blocking conditions truth table 列了 4 类阻断（padding/border/BFC root/in-flow），未列 `root` 与 `body/html 顶层 wrapper` 这两个边界；build P3 GREEN 实施完后 R3 既有 sibling collapse 测试退化（root 节点 propagate 进了不该 propagate 的位置）。

**根因：** plan §0 grep 只查了「目标功能 fingerprint F1-F5」，未对 R3 既有测试（`block_layout_test.cc` `MarginCollapseLayoutTest` 系列）做「边界假设反向 fingerprint」。R3 测试隐含「`Layout(root, ctx)` 之后 root 自身 margin-top 不会 propagate 到 viewport」的假设。

**修复：** `ComputeBlocksTop` 增加 root + body 顶层 wrapper 隐式 BFC root 识别（`box->parent == nullptr || box->parent->parent == nullptr`），与主流浏览器对齐。

**反复模式匹配：** ⚠️ 已知模式 #4「设计文档管线注入点须附代码级可行性验证」+「既有测试隐式假设未 grep」— 与 TASK-26-01 R4 反直觉 layout 默认值同源（第 4-5 次相关）。

**升级行动：** writing-plans.mdc §9 增补「Layout 任务 plan §0 grep 必须含既有测试边界假设反向 fingerprint」P1 条目（详见 §6 改进建议 #2）。

### 4.3 [P2 教训] CSS parser shorthand 能力 plan 阶段未逐项验证

**现象：** plan §1.2 RED 测试 `BorderBottom_BlocksLastChildCollapse` 用 `border-bottom: 1px solid black`，build 时发现 css parser 不解析 `border-bottom` shorthand → child 没有 border-bottom 计算值 → blocks_bottom=false → 测试反而 PASS（虚假通过）。

**根因：** VAN/plan 阶段仅 grep 验证 `css/parser.h` 总体可解析 `padding`，未对每个 shorthand（`border-bottom` / `font` / `background` 等）单独验证。

**修复：** 改测试名为 `PaddingBottom_BlocksLastChildCollapse` + 用 `padding-bottom: 1px` 等价物理分隔符；border-bottom shorthand 缺失列入独立 P3 候选技术债。

**反复模式匹配：** ⚠️ 已知模式 #3「前置依赖/环境/API 能力未验证」第 9+ 次。

**升级行动：** writing-plans.mdc §0 增补「CSS 测试用到的 shorthand 必须 plan 阶段逐项 grep 验证可解析」P1 条目（详见 §6 改进建议 #3）。

### 4.4 [P1 教训] 算法伪码「赋值 vs 合并」语义未在 spec 阶段明确

**现象：** spec §5.4 算法 step 4 `if (child->collapsed_through) cur_chain = child_outgoing` 是赋值（覆盖）语义；build P3 GREEN 实施后 A5 既有 sibling collapse-through 测试退化（cur_chain 累积值被 child_outgoing 覆盖丢失）。

**根因：** spec 算法草拟时未演练「父循环已累积 sibling chain → 遇到 collapse-through child → 应合并而非覆盖」场景。

**修复：** `MarginChain` 类增 `MergeFrom(const MarginChain& other)` 合并语义（`max_positive` 取大 / `min_negative` 取小）；调用站点改为 `cur_chain.MergeFrom(child_outgoing)`。

**升级行动：** creative.md / writing-plans.mdc 增补「算法伪码涉及 chain/list/state 累积时必须明确赋值 vs 合并语义」P1 条目（详见 §6 改进建议 #4）。

---

## 5. 经验教训

### 5.1 「多级递归 mental walkthrough」是 API 传递语义决策的硬性前置（P0 升级）

任何递归算法（layout / parser / event bubble / clean-up cascade）的 API 传递语义决策（by-value / by-ref / by-pointer / in-out / 返回）必须在 plan 阶段做以下 mental trace：

1. **3 层最小递归**（Level N caller → Level N+1 → Level N+2）
2. **每层是否修改共享状态**？（chain / accumulator / cursor）
3. **caller 在 callee 完成后是否需要看到 callee 的修改**？

如果答案是「需要看到」，by-value 必然失效。本任务 D2 决策因仅推演单层（Level N → N+1）漏掉 N+2 层 propagate 需求；build 阶段付出修改签名 + 修改全部 callsite 的成本。

**反向自证：** R3 任务（TASK-26-01）实施 sibling collapse 时用 `MarginChain cur_chain` 作为 LayoutBlock 局部变量，未跨函数边界，by-value 等价 by-ref（同函数内变量）— 当时未暴露此风险，是本任务把它从「函数内」推到「跨函数」时才显化。

### 5.2 既有测试是「隐式 spec」必须 grep fingerprint（P1 升级）

R3 既有 sibling collapse 测试隐含的「root 不向上 propagate」假设是 vx layout 引擎的隐式行为契约。plan §0 grep fingerprint 不仅要查目标功能，还要查既有测试的边界期望（特别是 layout / event / parser 类有大量边界 case 的子系统）。

**操作化：** plan §0 增加「既有测试边界期望反向 fingerprint」步骤，对当前任务即将 touch 的代码路径相关的所有 `*test*.cc` 做关键字 grep（如 layout 任务必 grep `EXPECT_FLOAT_EQ.*y` / `EXPECT_FLOAT_EQ.*content_height` / `EXPECT_TRUE.*collapsed`）。

### 5.3 CSS parser 能力 grep 必须按 shorthand 逐项验证（P1 升级）

vx CSS parser 对各 shorthand 的支持参差不齐（`padding` ✅ / `margin` ✅ / `border` ⚠️ / `border-bottom` ❌ / `font` ?）。plan / RED 阶段使用任何 shorthand 前必须单独 grep 验证：

```bash
# 通用模板
rg "ParseBorderShorthand|ParseFontShorthand|ParseBackgroundShorthand" \
   veloxa/core/css/parser.cc -n
# 或对具体 shorthand
rg '"border-bottom"|kBorderBottom' veloxa/core/css/ -n
```

如未命中，改用语义等价的 longhand 或 padding 替代物。

### 5.4 算法伪码「累积合并」vs「覆盖」语义必须明确（P1 升级）

任何涉及 chain / accumulator / list / state 的算法伪码，赋值符 `=` 是高歧义符号。spec 阶段必须用 explicit method name 替代赋值符：

- ❌ `cur_chain = child_outgoing`（歧义：覆盖？合并？swap？）
- ✅ `cur_chain.MergeFrom(child_outgoing)` / `cur_chain.Replace(child_outgoing)` / `swap(cur_chain, child_outgoing)`

### 5.5 性能优化作为副产品 vs 主线任务的判定

本任务 P6.2 hoist `last_in_flow_block` 是「测试驱动发现 + scope 内 ≤ 5 行修改」典型 — 符合「副产品修 pre-existing bug 3 标准」（已写入 systemPatterns）。但本次属于「副产品**优化**」而非「副产品**修复**」，证明 3 标准对优化也适用：

1. 由本 Round bench 触发（A6 边缘）✅
2. 修复 ≤ 5 行（实际 +7/-12）✅
3. 不引入新结构性改动（仅 hoist 局部变量）✅

→ 直接列记 progress.md「实证微调」，不拆任务。

---

## 6. 改进建议（附优先级与落实方式）

| # | 建议 | 优先级 | 落实方式 | 目标 |
|---|------|--------|---------|------|
| 1 | API 传递语义决策必须做「多级递归 mental walkthrough」（≥3 层 + 是否需要 caller 看到 callee 修改）| **P0** | 改规则：`writing-plans.mdc` §9 增加「递归算法 API 决策必检项」段；creative.md「6.algorithm」段同步 | 长期防御「by-value 多级失效」反复模式（第 9+ 次） |
| 2 | Layout / parser / event 类任务 plan §0 grep 必须含「既有测试边界期望反向 fingerprint」 | **P1** | 改规则：`writing-plans.mdc` §0 增加「既有测试隐式契约 fingerprint 必检表」 | 防御「隐式假设未 grep → 实施退化」反复模式 |
| 3 | CSS / 解析器类任务 plan / RED 用到的所有 shorthand 必须单独 grep 验证 parser 能力 | **P1** | 改规则：`writing-plans.mdc` §0 增加「CSS shorthand 能力 grep 表」；`activeContext.md` 待处理事项 | 防御「shorthand 能力未验证 → 测试虚假通过」反复模式 |
| 4 | 算法伪码涉及 chain / accumulator / state 累积时禁止用 `=` 赋值符，必须用 explicit method（`MergeFrom` / `Replace` / `swap`） | **P1** | 改规则：`creative.md` 命令模板「6.algorithm」段；伪码语法约定 | 防御「赋值 vs 合并语义歧义 → 实施退化」反复模式 |
| 5 | 「副产品优化」3 标准（与「副产品修复」3 标准对称） | **P2** | 写入 `systemPatterns.md`「scope 边界 — 副产品优化 3 标准」段（与既有「副产品修复」段并列） | 长期实践引用 |
| 6 | CSS parser `border-bottom` shorthand 缺失（独立 P3 候选）| **P2** | 写入 `tasks.md` §待立项候选 + `activeContext.md` 待处理事项 | 触发条件：下次 layout / 视觉测试用到 border shorthand 时 |

**P0 必须在归档前落实**（建议 #1）；**P1 在 `activeContext.md` 待处理事项追加**（建议 #2-#4）；**P2 写入长期知识库**（建议 #5-#6）。

---

## 7. 反复模式识别

| 已知模式 | 出现频率 | 本次是否重复？ | 处置 |
|---------|---------|-------------|------|
| 计划文件清单与实际变更不一致 | 9+ 次 | ⚠️ 部分（layout_engine.cc 估 +120 实测 +218）| 已记录 §1.2 微调点 1+4，根因都在「设计未代码级演练」 |
| 子代理产出需大量返工（CMake/编译/上下文不足）| 7+ 次 | ❌ 不适用（直执行）| — |
| 前置依赖/环境/API 能力未验证 | 8+ 次 | ✅ **第 9+ 次**（CSS parser border-bottom + R3 测试边界假设）| 升级 §6 建议 #2-#3 P1 条目 |
| 非默认路径（流式/错误/缓存）遗漏验证 | 4+ 次 | ❌ 不重复（5 阻断条件 + 反向探针完整覆盖）| — |
| 测试隔离问题（flaky / 并行冲突 / 环境依赖）| 7+ 次 | ❌ 不重复（10 单测独立 fixture）| — |
| 提交粒度偏离计划（大杂烩提交）| 7+ 次 | ⚠️ 部分（plan 估 7 commits，实际 9，P2-P5.2 GREEN 合并为 1 大 commit `+252/-47`）| 接受 — GREEN 一致性合并合理；不算反复 |
| TDD 严格度与场景不匹配 | 11+ 次 | ❌ 不重复（严格 RED→GREEN + 3 反向探针）| — |
| **API 传递语义决策未做多级 mental trace（新候选模式）** | 1（首次显化）| ✅ 本任务首次定型 | 升级 §6 建议 #1 P0 条目 |
| **算法伪码赋值符歧义（新候选模式）** | 1（首次显化）| ✅ 本任务首次定型 | 升级 §6 建议 #4 P1 条目 |

**关键发现：** 「**前置依赖/环境/API 能力未验证**」反复模式已突破 9 次（升级 P0），但本任务仍在 CSS parser shorthand 维度命中 — 说明现有规则对「能力 grep 颗粒度」要求不够细，需明确到 shorthand / function / API method 单点级别。

---

## 8. 验证 TASK-20260426-01 升级规则 ROI（首次外部任务）

| 规则段 | 触发场景 | 实战表现 | ROI 评估 |
|---|---|---|---|
| `writing-plans.mdc` §7.0.1 同窗口 stash-swap | P6.2 bench 验证 | ✅ **首次外部任务实战完整执行**；7 BM 一次过；副产物发现 P6.2 优化点；CV 5-8% 区间为可信信号 | **🟢 高 ROI 验证**：协议有效，必须长期保留；无误判 |
| `writing-plans.mdc` §9.1 跨 LayoutType 独立 box-model | LayoutBlockChild 改造（仅触 kBlock 路径）| ⊘ 不触发（本任务零改 LayoutInline/Flex）| 🟡 不可证伪 |
| `writing-plans.mdc` §9.2 默认值边界 | `incoming` 默认 empty / `outgoing` blocks_bottom 默认 empty | ✅ plan 阶段 spec §5.4 / §5.5 已明确锁定；零反直觉默认值 | 🟢 中 ROI 验证 |
| `writing-plans.mdc` §9.3 D3 反向探针强制 | P1 / P5 测试设计 | ✅ **3/3 反向探针完整循环**（第 6 次实战）；超 §9.3 最小要求 3× | 🟢 高 ROI 验证 |
| `subagent-development.mdc` D3 重评估 | plan 阶段未标 D3 子代理 | ⊘ 不触发（单线程递归依赖不适合并行）| 🟡 不可证伪 |
| `creative.md` d.1 多坐标系约定 | 不触发（无多坐标算法）| ⊘ 不触发 | 🟡 不可证伪 |

**整体结论：** TASK-26-01 升级规则 5 条中 3 条触发，全部高 / 中 ROI 验证（§7.0.1 / §9.2 / §9.3）；2 条不触发但留存合理（§9.1 / subagent-D3）。规则有效，建议长期保留。

---

## 9. 技术改进建议

### 9.1 LayoutBlockChild 算法可读性

当前 `LayoutBlockChild` 函数 ~250 行（含注释），逻辑路径分支密集（`blocks_top` / `blocks_bottom` / `collapsed_through` / `kBlock vs non-kBlock` / `first-in-flow propagate` 五维交叉）。可拆分为：

- `ComputeBlockBoxModel(box, w, ctx)` — box-model + width 解析（~30 行）
- `LayoutBlockChildren(box, cur_chain, &any_in_flow, &last_in_flow_block)` — children 循环主体（~80 行）
- `ComputeOutgoingChain(box, cur_chain, blocks_bottom)` — outgoing 计算（~20 行）

**优先级：** P3（不影响功能，仅可读性）；触发条件：下次 layout 算法扩展（grid / multi-column / float）时拆分。

### 9.2 `MarginChain::MergeFrom` 命名澄清

`MergeFrom` 与 `Add(f32)` 语义差异需文档化：`Add(f32 m)` 是单值累积（max_pos / min_neg 取极值）；`MergeFrom(other)` 是 chain 累积（双向取极值）。当前注释已说明，但可考虑改名为 `MergeChain` 以与 `Add` 语义对偶。

**优先级：** P3（命名洁癖）；不影响行为。

### 9.3 LayoutBox `margin_top_collapsed_into_ancestor` / `margin_bottom_collapsed_into_ancestor` 双字段对称

两字段对称命名 + 同语义边界，但分散在 `LayoutBlockChild` 的不同位置赋值（top 在 first-child propagate 路径 / bottom 在 last-in-flow-block 末尾扫描）。可考虑封装为 `MarginCollapseStateOps::SetTopCollapsed()` / `SetBottomCollapsed()` 一致接口。

**优先级：** P3（封装收益小）；当前直接字段访问足够清晰。

---

## 10. 安全评估

| 维度 | 状态 | 备注 |
|------|------|------|
| 输入验证 | N/A | 纯内部 layout 算法，无外部输入 |
| 认证/授权 | N/A | — |
| 数据保护（加密/脱敏）| N/A | — |
| 依赖审计 | N/A | 零新依赖（VAN F6 实证） |
| 错误信息脱敏 | N/A | — |
| 敏感数据处理 | N/A | — |

**本任务不涉及安全变更**（VAN V5 锁定）。

---

## 11. plan × 0.6 估时数据点

**第 14 数据点（Level 3 单 Round 单子系统）：**

| 维度 | 值 |
|---|---:|
| plan 估时 | 390 min（~6.5h）|
| plan × 0.6 准确档 | 234 min（~3.9h）|
| 实测 | ~180 min（~3h）|
| 比例 | **0.46×** |

**落点分析：** 介于「准确档 0.5-0.6×」（R1/R2 子任务 0.50/0.56）与「中位档 0.37-0.50×」（R3/R4 子任务 0.37/0.50）之间偏中。Level 3 单子系统类典型表现。

**对比基线（性能优化 / 单子系统类历史）：**

| 任务 ID | 类型 | 比例 |
|---|---|---:|
| TASK-20260424-01 | research/小补丁 | 0.29× |
| TASK-20260424-03 | DrawText warm 优化 | 0.42× |
| TASK-20260424-04 | DrawText 残余 | 0.26× |
| TASK-20260425-01 | SDL2 后端 | 0.22× |
| TASK-20260426-01 R1 | LayoutBox helpers | 0.50× |
| TASK-20260426-01 R2 | inline style + 安全护栏 | 0.56× |
| TASK-20260426-01 R3 | margin collapse R3 | 0.37× |
| TASK-20260426-01 R4 | LineBox 模型 | 0.50× |
| **TASK-20260430-01** | **first/last child margin collapse** | **0.46×** |

**模式确认：** Level 3 单子系统 + 决策已 plan 阶段锁定 + VAN grep 实证三联合 → 0.40-0.50× 区间稳定；本数据点居中。

---

## 12. 总结

### 12.1 核心成就

1. ✅ W3C CSS 2.1 §8.3.1 nested margin collapse R1 + R3 + R5 完整实施
2. ✅ wpt-005 SKIP→PASS（直接验证目标达成）
3. ✅ ctest 1029 → 1039（+10），Release `-O3 -Werror` 0 warn
4. ✅ §7.0.1 同窗口 stash-swap 协议首次外部任务实战 — 7 BM 一次过
5. ✅ §9.3 反向探针 3/3 完整循环（第 6 次实战）
6. ✅ TASK-26-01 升级规则 ROI 验证：3/5 触发，全部高/中 ROI

### 12.2 核心待改进

1. 🔴 **P0**：API 传递语义决策必须做多级递归 mental walkthrough（D2 by-value 失效）
2. 🟠 **P1**：plan §0 grep 必含既有测试隐式契约 fingerprint（隐式 BFC root 边界）
3. 🟠 **P1**：CSS parser shorthand 能力按用到的逐项 grep（border-bottom 缺失）
4. 🟠 **P1**：算法伪码禁用赋值符 `=` 对 chain/state 累积，必须 explicit method（MergeFrom）

### 12.3 下一步

- 用户确认 → `/archive` 启动归档
- 归档前必须落实 P0 建议 #1（升级 `writing-plans.mdc` §9 递归算法 API 决策必检项）
- P1 建议 #2-#4 落实路径在归档时同步执行（与归档 finalize 一并提交）

---

**回顾人：** AI Agent
**回顾日期：** 2026-04-30
**预计归档日期：** 用户确认后立即
