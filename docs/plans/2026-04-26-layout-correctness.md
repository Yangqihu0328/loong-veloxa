# 实现计划：Layout 正确性消化（TASK-20260426-01）

- **关联 spec：** [docs/specs/2026-04-26-layout-correctness-design.md](../specs/2026-04-26-layout-correctness-design.md)
- **复杂度：** Level 4
- **拆分策略：** D1 全包 + 多轮次 Build 中间态（4 Round 串行 + 用户每 Round 末确认）
- **安全相关：** ✅（Round 2 #28）
- **预测：** plan ~15h → 实测 ~9h（0.6× 准确档）
- **基线 PASS：** 951 ctest（截至 TASK-25-01 归档）

---

## 目录

- [§0 准备阶段](#0-准备阶段--r0--约-30-min--实测-18-min)
- [§1 Round 1 — #25 LayoutBox origin helpers](#1-round-1--25-layoutbox-origin-helpers--约-60-min--实测-36-min)
- [§2 Round 2 — #28 HTML inline style + 安全护栏](#2-round-2--28-html-inline-style--安全护栏--约-90-min--实测-54-min)
- [§3 Round 3 — #20 Margin collapsing 全实施](#3-round-3--20-margin-collapsing-全实施--约-300-min--实测-180-min)
- [§4 Round 4 — #21 LineBox + vertical-align + line-height](#4-round-4--21-linebox--vertical-align--line-height--约-360-min--实测-216-min)
- [§5 Finalize](#5-finalize--约-60-min--实测-36-min)
- [§6 子代理 / 直执行决策](#6-子代理--直执行决策)
- [§7 风险监控触发器](#7-风险监控触发器)

---

## §0 准备阶段 — R0（约 30 min / 实测 18 min）

> **进入条件：** spec 已落盘，用户已确认进入 build 阶段。

### P0.1 基线核验（5 min）

| 步骤 | 命令 | 期望 |
|---|---|---|
| (1) git status 干净 | `git status` | 当前分支 `task/20260426-01-layout-correctness`，无 modified |
| (2) ctest baseline | `ctest --test-dir build -j 8` | 951 PASS / 0 FAIL |
| (3) bench baseline | `./build/bench/bench_layout_buildtree --benchmark_repetitions=3 --benchmark_min_time=2s` | 记录 `BM_BuildTree_32K_blocks` mean，写入 progress.md |

### P0.2 W3C wpt fixture 拉取（15 min）

**前置：** 代理 `172.22.32.1:7890` 已实证可用（plan 阶段确认）。

```bash
# 1. 设置 git 全局代理（任务收尾必 unset，TASK-19-13 P1 守卫）
git config --global http.proxy http://172.22.32.1:7890
git config --global https.proxy http://172.22.32.1:7890
export http_proxy=http://172.22.32.1:7890 https_proxy=http://172.22.32.1:7890

# 2. 拉取 6 例 margin-collapse + 2 例 line-box（直接 raw GitHub）
mkdir -p tests/fixtures/wpt/css/CSS2/margin-padding-clear
mkdir -p tests/fixtures/wpt/css/CSS2/linebox

# margin-collapse 选 4 例（覆盖 case 1/2/3/negative）
for n in 001 002 003 091; do
  curl -fsS "https://raw.githubusercontent.com/web-platform-tests/wpt/master/css/CSS2/margin-padding-clear/margin-collapse-${n}.xht" \
    -o "tests/fixtures/wpt/css/CSS2/margin-padding-clear/margin-collapse-${n}.xht"
  curl -fsS "https://raw.githubusercontent.com/web-platform-tests/wpt/master/css/CSS2/margin-padding-clear/margin-collapse-${n}-ref.xht" \
    -o "tests/fixtures/wpt/css/CSS2/margin-padding-clear/margin-collapse-${n}-ref.xht" 2>/dev/null || true
done

# line-box 选 2 例（vertical-align + line-height）
curl -fsS "https://raw.githubusercontent.com/web-platform-tests/wpt/master/css/CSS2/linebox/inline-formatting-context-001.xht" \
  -o "tests/fixtures/wpt/css/CSS2/linebox/inline-formatting-context-001.xht"
curl -fsS "https://raw.githubusercontent.com/web-platform-tests/wpt/master/css/CSS2/linebox/vertical-align-baseline-001.xht" \
  -o "tests/fixtures/wpt/css/CSS2/linebox/vertical-align-baseline-001.xht"

# 3. 记录来源 + commit hash
cat > tests/fixtures/wpt/README.md <<'EOF'
# W3C web-platform-tests Fixtures

来源：https://github.com/web-platform-tests/wpt
许可证：BSD-3-Clause（详见 LICENSE）
拉取时间：2026-04-26
拉取分支：master
拉取 commit：（执行时填入）

## 文件清单
- css/CSS2/margin-padding-clear/margin-collapse-{001,002,003,091}.xht — covers CSS 2.1 §8.3.1 case 1/2/3/negative
- css/CSS2/linebox/{inline-formatting-context-001,vertical-align-baseline-001}.xht — IFC + vertical-align baseline

## 拉取脚本
见 docs/plans/2026-04-26-layout-correctness.md §0.P0.2
EOF

# 4. 任务收尾必 unset（reflection 阶段最后一步）
# git config --global --unset http.proxy
# git config --global --unset https.proxy
```

### P0.3 grep 影响面 fingerprint（10 min）

```bash
# #25 替换点全量（必须 ≥ 12 处命中才达 A1 验收）
rg -n 'border\[k(Top|Left)\]\s*\+\s*padding\[k(Top|Left)\]' veloxa/core/layout/

# #28 现有 inline_declarations 消费点（不应有遗漏）
rg -n 'inline_declarations\(\)' veloxa/core/

# #21 vertical-align 现有 hint（应为空 → 确认从零实现）
rg -n 'vertical[_-]align|vertical_align' veloxa/core/

# enum_serialization 全链路 fingerprint 复用（line-height 模式）
rg -nA 2 'kLineHeight' veloxa/core/css/ | head -40
```

### P0.4 输出物

- ✅ ctest 基线（写入 progress.md）
- ✅ bench 基线（同上）
- ✅ 8 个 wpt fixture 入仓（git add 但不 commit，待 R3/R4 真正消费时 commit）
- ✅ grep 4 类 fingerprint 输出快照（写入 progress.md）

### P0.5 退出门

进入 R1 前必须满足：
- [ ] ctest 基线 951 PASS（不达标 → 暂停 / `/reflect` 排查）
- [ ] wpt fixture 8 个全部 200 OK（不达标 → 降级 Q3_B 手工 fixture）
- [ ] grep #25 替换点 ≥ 12 处（不达标 → 调整 spec A1 验收 / 与用户确认）

---

## §1 Round 1 — #25 LayoutBox origin helpers（约 60 min / 实测 36 min）

> **进入条件：** §0 完成。
> **类型：** D2 单一注入点的 helper 提炼 + 调用点替换 → 可直执行（不上子代理）。

### 任务清单

| ID | 描述 | 路径 | 类型 | 估时 (min) |
|:-:|---|---|:-:|---:|
| R1.1 | 在 `layout_box.h` 追加 `Point` struct + 6 helper（border/padding/content × origin/4-side） | veloxa/core/layout/layout_box.h | impl | 10 |
| R1.2 | 单元测试：`tests/core/layout/layout_box_test.cc` +6 case（含 RED 反向探针：临时把 `padding_box_origin` 改为 `+border[kTop]+border[kTop]` 故意错位 → FAIL → 恢复）| tests/core/layout/layout_box_test.cc | test | 10 |
| R1.3 | `layout_engine.cc:209-212` y_offset 替换为 `child->margin_box_bottom()` | veloxa/core/layout/layout_engine.cc | refactor | 8 |
| R1.4 | `flex_layout.cc` 5-7 处分散计算替换 | veloxa/core/layout/flex_layout.cc | refactor | 12 |
| R1.5 | 全量 ctest（957 应 PASS） | — | verify | 5 |
| R1.6 | bench `bench_layout_buildtree` + `bench_layout_flex` 跑 3 reps，记录 ≤ baseline×1.10 | — | verify | 10 |
| R1.7 | git commit `feat(layout): add LayoutBox origin helpers (#25)` + 中间态报告 + 用户确认 | — | meta | 5 |

### 退出门

- ctest 951 + 6 = 957 PASS
- bench buildtree mean ≤ baseline × 1.10
- 用户确认 → 进 R2

---

## §2 Round 2 — #28 HTML inline style + 安全护栏（约 90 min / 实测 54 min）

> **进入条件：** R1 用户确认 + git commit 完成。
> **类型：** D2 单一注入点（html/parser.cc:92-95）+ 三件套安全护栏 → 直执行。
> **安全相关：** ✅

### 任务清单

| ID | 描述 | 路径 | 类型 | 估时 (min) |
|:-:|---|---|:-:|---:|
| R2.1 | `html/parser.cc` 文件级常量 `kMaxInlineDeclarationCount=1000` / `kMaxInlineValueLength=8192` + 黑名单数组 + `ContainsBlacklistKeyword(StringView)` | veloxa/core/html/parser.cc | impl | 15 |
| R2.2 | `Parser::ApplyInlineStyleAttribute(Element*, StringView)` 私有方法 | veloxa/core/html/parser.cc + .h | impl | 10 |
| R2.3 | `Parser::ProcessAttributes` 改造：style 名走分支、其余走 SetAttribute | veloxa/core/html/parser.cc | impl | 8 |
| R2.4 | 单元测试 `tests/core/html/inline_style_test.cc`（**新增文件**）：A2 行为一致性 + A3 三件套（T1 count cap / T2 value cap / T3-T5 黑名单 × 3）| tests/core/html/inline_style_test.cc | test | 25 |
| R2.5 | RED 反向探针：临时移除 count cap → T1 fixture FAIL（含 1500 项 declaration）→ 恢复 | 同上 | test/RED | 5 |
| R2.6 | 端到端集成：`tests/integration/html_layout_e2e_test.cc` +1 case，`<div style="padding:10px;color:red">` 解析后 layout 中 padding 应生效 | tests/integration/* | integration | 10 |
| R2.7 | 全量 ctest（957 + 8 = 965 PASS） | — | verify | 5 |
| R2.8 | git commit `feat(html): parse inline style attribute via CssParser, with safety guards (#28)` + 中间态报告 + 用户确认 | — | meta | 5 |

### 安全合规检查（spec §6 落地）

- [ ] T1 declaration count cap 1000 实证：1500 项 fixture → 仅前 1000 应用 + 无 abort
- [ ] T2 single value len cap 8 KB 实证：16 KB fixture → 整 attribute 跳过 + element 仍正常渲染
- [ ] T3-T5 黑名单 3 例：`expression(` / `behavior:` / `javascript:` 各一例 → inline_declarations 为空 + log warn 1 行
- [ ] **未硬编码秘密**：黑名单关键字本身不是秘密，可入仓（与敏感数据保护规则不冲突）

### 退出门

- ctest 965 PASS
- bench 同 R1（不预期波动 — 仅修改 HTML parser 路径）
- 用户确认 → 进 R3

---

## §3 Round 3 — #20 Margin collapsing 全实施（约 300 min / 实测 180 min）

> **进入条件：** R2 用户确认 + git commit 完成。
> **类型：** D3（多文件 + 算法 + W3C 规范一致性）→ 推荐 `/creative` 子阶段先做算法决策，再回 `/build` 实施。

### 子阶段触发：`/creative` 介入

> 因 collapsing 算法存在多种实现路径（pre-pass 分析 vs in-line 累积 chain），且 collapse-through 与 first/last child 嵌套规则交互复杂，需先在 `memory-bank/creative/` 落设计文档再实施。

详见 spec §13 引用，`/creative` 阶段产出：
- `memory-bank/creative/creative-margin-collapsing.md`：MarginChain in-line 累积算法 vs pre-pass 分析法 trade-off + 选择落地

### 任务清单

| ID | 描述 | 路径 | 类型 | 估时 (min) |
|:-:|---|---|:-:|---:|
| R3.1 | 创建 `core/layout/margin_collapse.h` — `MarginChain` 数据结构（spec §5.3） | veloxa/core/layout/margin_collapse.h | impl | 15 |
| R3.2 | 单元测试 `margin_collapse_test.cc` — MarginChain Add/Collapsed/IsEmpty + 4 case（正/负/零/空）+ 1 RED（故意把 `Collapsed()` 改回 `max_positive` 不含 negative → FAIL） | tests/core/layout/margin_collapse_test.cc | test/RED | 30 |
| R3.3 | 子代理 A：实现 `LayoutBlock` 算法重写（spec §5.3 伪码 → C++） | veloxa/core/layout/layout_engine.cc | subagent | 90 |
| R3.4 | 单元测试 `block_layout_test.cc` +12 case：A4 sibling / A5 first/last child / A6 collapse-through / A7 negative / A8 clearance stub + 2 RED 反向探针 | tests/core/layout/block_layout_test.cc | test/RED | 60 |
| R3.5 | W3C wpt 集成：`tests/integration/wpt_layout_test.cc` 跑 4 例 margin-collapse fixture（XHTML 头预处理 → HTML5）| tests/integration/wpt_layout_test.cc | integration | 45 |
| R3.6 | 全量 ctest（965 + 19 = 984 PASS） | — | verify | 10 |
| R3.7 | bench 必跑：`bench_layout_buildtree` + `bench_layout_flex`（margin collapsing 改动 hot path，必须实证 ≤ ×1.10） | — | verify | 15 |
| R3.8 | git commit `feat(layout): implement CSS 2.1 §8.3.1 margin collapsing (#20)` + 中间态报告 + 用户确认 | — | meta | 5 |

### 退出门

- ctest 984 PASS
- bench buildtree mean ≤ baseline × 1.10（**关键** — 此 round 触及 hot path）
- 4 例 wpt margin-collapse fixture PASS
- 用户确认 → 进 R4

### R3 风险触发器

- 若 R3.7 bench 退化 > 10% → **不进 R4**，进 P1 升级排查（profile + flamegraph + 优化或回滚 collapse-through 路径）
- 若 R3.5 任一 wpt fixture FAIL → **不进 R4**，先看是 spec 漏 case 还是实现 bug：
  - spec 漏 case → 升级到 P2（创意决策回炉）
  - 实现 bug → 直接 fix（D1）

---

## §4 Round 4 — #21 LineBox + vertical-align + line-height（约 360 min / 实测 216 min）

> **进入条件：** R3 用户确认 + git commit 完成。
> **类型：** D3（最复杂）→ 必经 `/creative` 子阶段。

### 子阶段触发：`/creative` 介入

> LineBox 是新抽象，vertical-align 6 关键字 + length + 2-pass 算法是新算法，必先在 creative 落决策：

产出：`memory-bank/creative/creative-line-box-model.md`
- LineBox 数据结构 vs 内联式算法（无显式 LineBox）trade-off
- 2-pass vertical-align 算法 vs N-pass 迭代算法
- TextShaper.TextMetrics 字段扩展 ABI 影响（已用 PIMPL，不影响外部用户）

### 任务清单

| ID | 描述 | 路径 | 类型 | 估时 (min) |
|:-:|---|---|:-:|---:|
| R4.1 | CSS 全链路：`enums.h` `VerticalAlign` enum + `computed_style.h` 字段 + `property.{h,cc}` PropertyId + `style_resolver.cc` parser + `transition.cc` discrete + `enum_serialization.{h,cc}` 反查表 | 6 文件 | impl | 60 |
| R4.2 | 单元测试 CSS 路径 `tests/core/css/parser_test.cc` +3 case（baseline / sub / 50% length）+ 1 RED（解析器把 `kSub` 算成 `kSuper`） | tests/core/css/parser_test.cc | test/RED | 20 |
| R4.3 | 子代理 B：`text_shaper.{h,cc}` `TextMetrics` 加 ascent/descent + FT `face->size->metrics` 路径 | veloxa/core/layout/text_shaper.cc + .h | subagent | 60 |
| R4.4 | 单元测试 `text_shaper_test.cc` +3 case（ascent > 0 / descent > 0 / 兼容 baseline ≈ ascent） | tests/core/layout/text_shaper_test.cc | test | 15 |
| R4.5 | 创建 `core/layout/line_box.h` — `LineBox` + `LineBoxItem` 数据结构 | veloxa/core/layout/line_box.h | impl | 15 |
| R4.6 | 子代理 C：`LayoutInline` 算法重写（spec §5.4 伪码 → C++ 含 2-pass vertical-align 解析） | veloxa/core/layout/layout_engine.cc | subagent | 100 |
| R4.7 | 单元测试 `line_box_test.cc` +9 case：A9 LineBox 结构 / A10 ascent/descent / A11 vertical-align 6 关键字 / A12 半-leading + 2 RED 反向探针 | tests/core/layout/line_box_test.cc | test/RED | 60 |
| R4.8 | W3C wpt 集成：`wpt_layout_test.cc` +2 例 linebox fixture | tests/integration/wpt_layout_test.cc | integration | 30 |
| R4.9 | 全量 ctest（984 + 16 = 1000 PASS） | — | verify | 10 |
| R4.10 | bench：`bench_layout_buildtree` + `bench_layout_flex` + `bench_text_shaping`（如有）| — | verify | 15 |
| R4.11 | git commit `feat(layout): implement line box model with vertical-align and line-height (#21)` + 中间态报告 + 用户确认 | — | meta | 5 |

### 退出门

- ctest 1000 PASS（**里程碑：突破千测**）
- bench 全部 ≤ baseline × 1.10
- 2 例 wpt linebox fixture PASS
- 用户确认 → 进 §5 finalize

---

## §5 Finalize（约 60 min / 实测 36 min）

| ID | 描述 | 路径 | 类型 | 估时 (min) |
|:-:|---|---|:-:|---:|
| F1 | 全量 ctest Release `-O3 -Werror` 模式 1000 PASS | — | verify | 5 |
| F2 | 全量 bench 三机轮转（如有 CI hook）/ 单机 5 reps mean | — | verify | 10 |
| F3 | techContext.md：#20/#21/#25/#28 状态 ⏳ → ✅ + 实证表 + 代理地址单一真相来源补全（`<开发环境代理地址>` → `172.22.32.1:7890` + WSL2 host gateway 注解） | memory-bank/techContext.md | doc | 15 |
| F4 | activeContext.md / progress.md / tasks.md 同步至「构建完成-待 reflect」 | memory-bank/* | doc | 10 |
| F5 | git config 代理 unset（**TASK-19-13 P1 守卫强制**）+ 验证 `git config --global --get http.proxy` 为空 | — | meta | 3 |
| F6 | git rebase main + push origin → 进 `/reflect` | — | meta | 12 |
| F7 | 触发 `/reflect` 命令 → reflection-TASK-20260426-01.md | memory-bank/reflection/* | doc | 5 |

### 退出门

- ctest 1000 PASS（Debug + Release）
- bench 全部 ≤ baseline × 1.10
- techContext.md / activeContext.md / progress.md / tasks.md 全部同步
- git config 代理已 unset 实证
- 进 `/reflect`

---

## §6 子代理 / 直执行决策

按 `.cursor/rules/skills/subagent-development.mdc` D1-D3 分级：

| Round | 任务 | 决策 | 理由 |
|:-:|---|:-:|---|
| R1 | #25 helper 注入 + 替换 | **直执行** | D2，单文件抽象 + 调用点替换，无算法决策 |
| R2 | #28 HTML parser 接入 + 三件套安全 | **直执行** | D2，单注入点 + 常量化护栏 |
| R3 | #20 MarginChain | **直执行** | D2，新数据结构 |
| R3 | #20 LayoutBlock 算法重写（R3.3） | **子代理 A**（generalPurpose） | D3，CSS 2.1 §8.3.1 多 case 算法实现 ≥ 90 min |
| R3 | #20 单元测试 + W3C 集成（R3.4-R3.5） | **直执行** | D2，按 spec §5.3 验收标准对应实施 |
| R4 | #21 CSS 全链路（R4.1） | **直执行** | D2，参考 line-height 模式 7 文件复制 |
| R4 | #21 TextShaper FT metrics（R4.3） | **子代理 B**（generalPurpose） | D3，FreeType API 探索 + 兼容字段维护 ≥ 60 min |
| R4 | #21 LayoutInline 算法重写（R4.6） | **子代理 C**（generalPurpose） | D3，IFC 2-pass 算法 ≥ 100 min |
| R4 | #21 单元测试 + W3C 集成（R4.7-R4.8） | **直执行** | 同 R3 模式 |

子代理协议（强制）：
1. 完整规格输入（spec §5 对应小节 + 验收标准 A_n）
2. 输出预期：单一文件 patch + change summary + 自跑 ctest 数字
3. 两阶段审查：(a) 规格合规（API 签名 / 验收点） (b) 代码质量（命名 / 注释 / 错误处理）
4. 失败处理：审查 FAIL → 不 commit，提供具体反馈让子代理 1 次修复；2 次失败转直执行

---

## §7 风险监控触发器

| 触发条件 | 应对动作 | 升级路径 |
|---|---|---|
| §0 任一 wpt fixture 拉取失败 | 降级 Q3_B 手工 fixture（写 6 例对应 case 1/2/3/negative/IFC/vertical-align）| `/plan` 调整 → 用户确认 |
| §0 ctest 基线 < 951 PASS | 暂停，先排查回归来源（git log + reset HEAD~N 二分） | `/debug` 介入 |
| Rn 任一 ctest 退化 | 不 commit，先排查；3 次失败 → 子代理转直执行 | `/debug` 介入 |
| Rn bench 退化 > 10% | 不进下 Round；profile + flamegraph 排查；可选回滚 | `/debug` 介入 |
| R3 wpt fixture FAIL | 区分 spec 漏 case vs 实现 bug；前者 → `/creative` 回炉 | `/creative` 二轮 |
| 估时累计偏差 > +50% | 暂停，重新评估剩余 Round 是否拆分 | 用户决策 |
| 安全测试 (R2 T1-T5) 任一漏报 | 不 commit，必须先达 spec §6 完整覆盖 | 强制 |

---

## §8 估时数据点登记（plan × 0.6 第 10 数据点）

落地实测时填表（每 Round 末更新 progress.md）：

| Round | plan (min) | actual (min) | ratio | 备注 |
|:-:|---:|---:|---:|---|
| R0 | 30 | TBD | TBD | — |
| R1 | 60 | TBD | TBD | — |
| R2 | 90 | TBD | TBD | — |
| R3 | 300 | TBD | TBD | — |
| R4 | 360 | TBD | TBD | — |
| R5 | 60 | TBD | TBD | — |
| **合计** | **900** | TBD | TBD | 第 10 数据点 / Level 4 首个 |

数据点累积 → reflection 阶段更新 `memory-bank/systemPatterns.md` 估时模型（当前模型 plan × 0.6，置信区间会因 Level 4 数据点扩张）。

---

## §9 退出标准（任务收尾）

进入 `/reflect` 前必须满足：

- [ ] ctest 1000 PASS（Debug + Release）
- [ ] 8 例 W3C wpt fixture PASS
- [ ] bench 所有项 ≤ baseline × 1.10
- [ ] git config 代理已 unset 实证
- [ ] techContext.md 4 项技术债 ✅ 闭环
- [ ] activeContext.md / progress.md / tasks.md 同步「构建完成」
- [ ] 4 个 commit 已推 origin
- [ ] 用户已确认 R4 完成
