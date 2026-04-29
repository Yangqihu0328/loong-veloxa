# 回顾：TASK-20260426-01 Layout 正确性消化（#25 + #28 + #20 + #21）

**日期：** 2026-04-29
**任务 ID：** TASK-20260426-01
**复杂度级别：** Level 4（多子系统 + 架构决策 + #20/#21 各自单独 Level 3 量级）
**安全相关：** ✅（#28 接收 HTML `style="..."` 外部输入）
**分支：** `feature/TASK-20260426-01-layout-correctness`（基于 main `9f7f338`）

---

## 1. 计划 vs 实际

### 1.1 顶层维度

| 维度 | 计划 | 实际 | 偏差原因 |
|---|---|---|---|
| 子任务数 | 4 (#25/#28/#20/#21) + R0 准备 + R5 finalize = 6 阶段 | 6 阶段全部完成 | 0 |
| 估时 plan × 0.6 准确档 | 900 min × 0.6 = **540 min** | 实测 ~**400 min**（R0 ~25 + R1 ~30 + R2 ~50 + R3 ~110 + R4 ~180 + R5 ~5）| **0.44× plan**（介于「准确 0.6×」与「最窄 0.3×」之间，Level 4 首数据点） |
| ctest 基线 → 终点 | 951 → ≥ 1010 | 951 → **1029/1029 PASS**（+78 cases，9 R1 + 24 R2 + 26 R3 + 19 R4） | +18 cases 高于估，威胁建模/反向探针完整覆盖驱动 |
| Release `-O3 -Werror` | 0 warn | **0 warn** | ✅ |
| Bench `BM_LayoutBuildTreeFlat/64` ≤ +10% | R3 退出门 ≤ +5%；R4 ≤ +10% | R3 同窗口对照 mean +3.2% / median +3.4%；R4 同窗口 mean **-3.6%** / median +2.65% | ✅ 远低退出门 |
| W3C wpt fixture | ≥ 6 例集成测试金标准 | 7 fixture 入仓（2 PASS margin + 2 SKIP-w/-rationale margin + 2 PASS linebox + 1 ref）| 落点合理 |
| 文件清单 plan 11 改 + 9 新 | — | 实际 ≈ 11 改 + 6 新（CSS 全链路 6 文件 + line_box.h + margin_collapse.h + 2 测试 + wpt fixture）| -3 新文件，部分 plan 拟新增 helper 合并入主体头文件 |
| 子代理 D3 任务 | 3 处（R3.3 / R4.3 / R4.6）| **全部直执行** | 用户上下文充分 + 算法 spec 已 creative 锁定，直执行更省两阶段审查成本（详见 §3.b 挑战 #1）|

### 1.2 多轮次 Build 中间态执行

| Round | 子任务 | plan (min) | 实测 (min) | ratio | 主要风险事件 |
|:-:|:-:|---:|---:|---:|---|
| R0 | 准备 | 30 | ~25 | 0.83× | wpt fixture 091 不存在替换 005（实证微调 #1）|
| R1 | #25 origin helpers | 60 | ~30 | 0.50× | WSL2 高 Load 假退化（稳态协议第 1 次自证）|
| R2 | #28 inline style + 三件套 | 90 | ~50 | 0.56× | upsert 破坏 cap 可观测性 + e2e over-match（反思 §3.b 挑战 #3）|
| R3 | #20 margin collapsing | 300 | ~110 | 0.37× | bench 边缘超 +10% → V1 优化 + **同窗口对照范式确立**（反思 §3.c 教训 #1，新 P0）|
| R4 | #21 LineBox + vertical-align | 360 | ~180 | 0.50× | sign error 系统性 + fill-available 反直觉 + inline-block 高度失效 + ninja 时间戳偏差（反思 §3.c 教训 #2-#5）|
| R5 | finalize | 60 | ~5 | 0.08× | 大部分由 reflect 阶段一并消化（实证微调 #2）|
| **合计** | — | **900** | **~400** | **0.44×** | — |

### 1.3 实证微调清单（plan vs build）

| # | 微调点 | plan 原案 | 实证调整 | 标注 |
|:-:|---|---|---|---|
| 1 | wpt margin-collapse-091 | plan §0 引用 091 | wpt master 不存在（088-095 全 404）→ 替代 005 | progress.md L21 |
| 2 | inline style 安全常量可见性 | hidden in anon namespace | 暴露到 namespace + 测试可断言数字守恒 | progress.md R2.1 |
| 3 | ContainsBlacklistKeyword 测试入口 | 仅 e2e | 提取 `internal::` namespace 暴露 unit test 入口（e2e over-match） | progress.md R2.3 |
| 4 | overflow:auto 启用 enum | 不在 R3 scope | R3.4 测试 `A7_OverflowAutoBlocksMarginCollapse` 触发 → 顺手补 1 行 fix | progress.md R3.3.x |
| 5 | bench 同窗口对照范式 | plan §3 仅声明 ≤+10% | R3 实施时确立「stash-swap 同窗口对照」防 stale baseline | progress.md R3.8 / 新 P0 |
| 6 | LayoutInline 默认 width 语义 | plan 未规定 | 反直觉发现：默认应 fit-content（max line.end_x）而非 fill-available | progress.md R4.6 |
| 7 | inline-block atomic explicit height | plan 未规定 | 必须显式读 style.height（与 LayoutBlock 路径对称）否则 vertical-align 物理失效 | progress.md R4.6 |
| 8 | wpt linebox fixture #id 选择器 | plan 直接拷贝 wpt 标记 | vx StyleResolver 子集 ID 支持有限 → 改 class | progress.md R4.8 |
| 9 | wpt linebox container `display: inline` | plan 未规定 | vx LayoutBlock 不创建匿名 IFC → 测试容器须显式 inline | progress.md R4.8 |
| 10 | R5 finalize 内嵌 reflect | plan 独立 60 min | 大部分（techContext 状态变更 / proxy unset 验证）由 reflect 阶段一并消化 | 本文 §5 |

---

## 2. 回顾检查清单

### 代码变更类（主调）

- [x] **计划精确度** — 文件清单基本一致；helper 数量微膨胀（R1 估 5-7 处替换实测 9 处 / R2 估 +8 case 实测 +24 case）；属「实证微调」非「计划失真」
- [x] **TDD 执行情况** — 严格 RED→GREEN，3 次完整反向探针循环（R1 OriginHelpers / R2 ContainsBlacklistKeyword / R3 MarginChain Negative+Mixed / R4 line_box RED1+RED2）；本任务把反向探针从「TDD 标配」升级到「Mixed TDD 标配」
- [x] **子代理质量（如使用）** — 全部直执行，N/A；但 plan 阶段已标记 3 处 D3，build 阶段判断 creative 锁定后直执行更经济（成本敏感性反思见 §3.c 教训 #6）
- [x] **测试隔离** — 无串扰、无 flaky；唯一一次「假退化」是 WSL2 Load 4.97 噪声（已被 TASK-19-13 P1 协议捕获）
- [x] **提交粒度** — Q5A 每 Round 1 commit + 中间态报告 + 用户确认 6 commits（R0~R4 + V1 优化）；零大杂烩
- [x] **非默认路径** — ⚠️ **3 项遗漏被 build 阶段捕获**：(a) vertical-align 坐标系 sign error、(b) LayoutInline 默认 width fill-available 反 IFC 直觉、(c) inline-block atomic explicit height 缺失。creative 阶段未画好坐标约定 + 默认值边界（反思 §3.c 教训 #2 / #3）

### 安全相关（#28）

- [x] **输入验证** — 三件套：count cap 1000 + value len cap 8 KB + 黑名单 (`expression(`/`behavior:`/`javascript:`，大小写不敏感)
- [x] **认证/授权** — N/A（解析层无 user identity）
- [x] **数据保护（加密/脱敏）** — N/A（解析数据，非持久化敏感）
- [x] **依赖审计** — 0 新依赖（spec §6 协议确认；F6 实证）
- [x] **错误信息** — Parser 错误静默 discard，不向上抛带原文的异常信息；reject 路径仅返回 nullptr / drop declaration，无内容回显
- [x] **敏感数据处理** — 代理地址 `172.22.32.1:7890` 单一真相来源 `techContext.md`，本任务规则文件 0 硬编码

---

## 3. 结构化回顾

### a. 做得好的

1. **多轮次 Build 中间态协议成熟落地（Q5A 全程）** — 6 commits 干净、每 Round 末用户确认门、`构建中·轮次 N 完成` 子状态正确传递；TASK-19-13 P3 协议第 2 次完整运行（首发 TASK-03 Round 1，本次 R0-R4 5 轮次稳定执行）。
2. **VAN 阶段 6 项 grep 实证完整覆盖**（F1 修正 #28 真实缺口在 HTML parser / F2 ParseDeclarationList 已存在 / F3 origin 分散 20+ 处 / F4 Block 零 collapsing / F5 baseline 字段未消费 / F6 不引入新依赖）—「方案根因假设未先验证」P0 第 4 次零盲点应用，spec 阶段直接修正了 #28 描述（避免在错的 layout/ 文件徘徊）。
3. **同窗口对照 bench 范式确立** — R3 一次实战把「跨时间窗 stale baseline +10.2% 假退化」纠正为「真实增量 +3.2%」（差异分解：~70% 时窗校准 + ~30% 真实优化），R4 复用范式得 mean -3.6% / median +2.65%；这是 TASK-24-03 「WSL2 稳态协议」的高阶进化。
4. **TDD 反向探针 4 次完整循环成熟应用** — R1 OriginHelpers + R2 ContainsBlacklistKeyword + R3 MarginChain Negative/Mixed + R4 LineBox RED1/RED2，每次验证「实现已正确」时也能精准 FAIL，反向探针从「P1 建议」彻底定型为「Mixed TDD 标配」。
5. **Creative 阶段「显式画范围边界」节省 build 后期成本** — D1.2 锁定「保持 LayoutChild API 不动 + 内部栈式状态」直接显化为 wpt-005 SKIP-w/-rationale + 留 P3 TASK-26-02；D2.D 锁定 inline-block atomic 不递归 IFC 让 R4 范围清晰；R3.3 实施时 wpt-005 不需要解释「为什么不实施」就能通过审查（R3 教训 #3）。
6. **TextMetrics ABI 兼容渐进式扩展** — `[[deprecated("use ascent")]]` 给未来清理明确锚点，本次零 caller 改造（R1/R2/R3 同步推进零冲突）；本范式可写入 `systemPatterns.md` 作为「跨 Round 共享数据结构 ABI 渐进扩展」标准做法。
7. **副产品修 1 pre-existing bug**（`overflow: auto` 经 ParseDeclaration 走 `ValueType::kAuto` 全局快路径不进 ParseEnumValue）— 由 R3.4 `A7_OverflowAutoBlocksMarginCollapse` 测试触发发现，「测试 driving bug discovery」生动样板；scope 内顺手 1 行 fix 不算超范围。
8. **每 Round 末用户决策门保住范围** — R3 末 V1 优化前用户选 B（不立即进 R4），R4 末 reflect 触发前用户接受 ctest 1029 + 同窗口 -3.6%；2 次门控避免了「快速堆叠致末段质量下滑」反模式。

### b. 遇到的挑战

1. **R3 子代理 vs 直执行决策反复（plan 标 D3 实际全直执行）** — plan §6 子代理协议（强制规格输入 + 单一文件 patch + 两阶段审查）成本对 creative 已完整锁定的算法实施反而是 overhead；本任务连续 3 处 D3 任务（R3.3 LayoutBlock 算法 / R4.3 FT metrics / R4.6 LayoutInline）直执行均成功且更快，提示「算法已 creative 锁定 + 上下文连续 + 单文件主修改」场景子代理收益接近 0。但 plan 阶段倾向标 D3 是合理保险，build 阶段重评估机制需更明确触发条件。
2. **R3 bench 跨时间窗误判** — 初版「+10.2%」用一周前 baseline（3709 ns）vs 新窗口测量（4087 ns），系统抖动 ±3-7% 让真实 +3.2% 增量被放大成 +10.2% 假退化。如未做同窗口对照会导致：(a) 用户被误导选 V1 优化（实际不必）、(b) 后续任务 baseline 数字不可比、(c) bench 退出门信号噪声比恶化。**根因**：`writing-plans.mdc` §7 WSL2 协议虽规定 sleep 30s + CV ≤ 2%，但未规定「同窗口对照」。
3. **R2 SetInlineDeclaration upsert 语义破坏 cap test 可观测性** — 1500 个相同 prop dedup 至 1 → cap 测试只能证「不崩溃」+「常量数字守恒」，真 cap 实证依赖 spec §6 review。「测试该测什么」的边界发现不止于此：R2.5 黑名单 e2e 4 case 因 CssParser 自然丢弃 unknown property 已 trivially PASS（over-match），最后通过提取 `internal::ContainsBlacklistKeyword` 直接 unit test 8 case 才证「实现真存在」。
4. **R4 vertical-align 系统性 sign error** — 多个公式（ComputeNonExtremeAlign 6 关键字 + Phase 1/2 max_ascent/max_descent 维护 + kTop/kBottom offset）在「item.y = baseline_y - ascent + offset」坐标约定下符号反复出错；初版 RED 阶段 3 个测试 FAIL，调试用 ~30 min 系统性诊断后采用统一坐标约定 + 每条公式注释引用约定。**根因**：creative-line-box-model.md 锁定了 2-pass 算法但未画「单一坐标系 + 公式表」单一图。
5. **R4 LayoutInline 默认 fill-available 反 IFC 直觉** — 原默认 `content_width = containing_width` 致 nested inline child 永远撑满父宽度，相邻 child 必然换行（`A11_4_VerticalAlignLengthOffsetMovesUp` 触发：up_span->y 反超 base_span->y）；正确语义是 fit-content（max line.end_x）。**根因**：plan 阶段未规定 LayoutInline 默认 width 语义，spec §10.8 IFC 隐含约束未显式抽出。
6. **R4 inline-block atomic + LayoutInline 路径 height 失效** — vx BuildTree 把 display:inline-block 映射成 LayoutType::kInline → 走 LayoutInline 路径不读 style.height → atomic ascent = border_box_height = 0 → vertical-align 关键字物理位置失效（RED2 触发：span_box->content_height 19.2 vs 期望 ≥ 40.0）。**根因**：跨 LayoutType 共用样式属性（width/height）单一路径假设。
7. **R4 ninja 时间戳偏差致代码改动未生效** — git stash + 构建产物 `.o` 时间戳 > stash 后还原的源码 → ninja 跳过重编 → 测试始终用旧 binary，输出迷惑性 0 改动报告。`touch <src>` 强制 mtime 或 `cmake --build --target vx_core` 全图重建可绕开。**根因**：stash-swap 协议未涵盖时间戳问题。
8. **R4.8 wpt linebox fixture 多重适配** — wpt master 标记直接拷贝失败（#id 选择器 vx 子集支持有限 + `<div>` 容器 vx 不创建匿名 IFC），需把 ID 改 class + 容器显式 `display: inline`；这暗示 vx CSS 选择器 + IFC 边界 sub-spec 与 W3C 完整集合的差距，未来更多 wpt 适配会同模式撞上。
9. **WSL2 高 Load 假退化（首次反模式）** — R1 编译刚完成 Load 4.97 → bench Flat/64 +26%；TASK-19-13 P1 协议生效 sleep 30s 重测 → 真值 +0.16%。本任务 R1/R2 各 1 次自证，证 WSL2 协议有效；但「真实退化 vs 高 Load 假退化」初判依然耗 ~3 min 操作员注意力。

### c. 经验教训

1. **【P0 升级】Bench 退出门验证默认 stash-swap 同窗口对照** — `writing-plans.mdc` §7 WSL2 协议追加：performance regression 验证必须在同时间窗内对照（stash 改动测 baseline → 还原测 V1 → diff），不允许跨日比对绝对数字。延伸：bench baseline `.json` 仅作长期趋势监控，单次退化判定一律同窗口。**根因**：跨时间窗 ±3-7% 系统抖动在 ±10% 退出门下严重放大假信号。
2. **【P0 新增】跨 LayoutType 共用样式属性必须每路径独立 box-model 解析** — 写入 `systemPatterns.md`：当 `display: X` 在 BuildTree 阶段映射成 `LayoutType::Y`（如 inline-block → kInline）时，Layout::Y 必须独立完成 box-model（width/height/padding/border/margin）解析，不能假设单一 layout 路径已处理。**根因**：本任务 inline-block atomic + LayoutInline 路径 height 失效根因模式。
3. **【P0 新增】Creative 阶段「单一坐标约定 + 公式表」一图** — 凡涉及 ≥2 个坐标系/方向（baseline / ascent / descent / offset / top-bottom）的算法 creative 必产出统一坐标约定声明 + 全部公式按约定列出 + build 阶段每条公式注释引用约定。**根因**：本任务 vertical-align sign error 系统性出现，creative 锁定了算法但未锁定坐标约定。
4. **【P1 新增】stash-swap 协议补丁：包含「stash pop 后 touch 关键 src + 全图重建」步骤** — 写入 `writing-plans.mdc` §7「WSL2 / 云机 bench 稳态协议」附录。**根因**：本任务 R4 ninja 时间戳偏差致同窗口对照用旧 binary 比新 binary。
5. **【P1 新增】Plan 阶段必须明确 LayoutInline / LayoutBlock 默认 width / height 语义** — 写入 `writing-plans.mdc` Layout 类任务必检项：默认值边界（fit-content vs fill-available / explicit vs zero-fallback）必须在 plan §0 grep fingerprint + §3 设计决策一并锁定，不允许 build 阶段实证发现。**根因**：本任务 R4 默认 fill-available 反 IFC 直觉 + inline-block 显式 height 缺失，根因均是 plan 未规定默认值。
6. **【P1 新增】子代理 vs 直执行决策 build 阶段重评估机制** — `subagent-development.mdc` 加：「creative 完整锁定算法（含全部公式 + 坐标 + 边界 case）+ 单文件主修改 + plan/上下文连续」3 条同时满足时，build 阶段应重评估子代理收益，倾向直执行；反之依然 D3。**根因**：本任务连续 3 处 D3 子代理任务直执行均成功且更快。
7. **【P1 新增】反向探针从「Mixed TDD 标配」彻底定型** — 累计 5 次实战（TASK-11 D3 + TASK-24-01 default block + TASK-24-03 三层 SIMD + 本任务 4 次），可写入 `writing-plans.mdc` 强制条目「Mixed TDD 模式 D3 类回归测试默认含反向探针」。**根因**：每次反向探针都精准捕获了「测试因实现恰巧正确成为永远不报警死代码」最大风险，零误报。
8. **【P2 新增】wpt fixture 适配 vx 子集模式** — 拷贝 W3C wpt 标记前必查 (a) ID 选择器 vx 支持范围 / (b) container display 与 vx anonymous IFC 行为 / (c) 像素级 ref 不可比则改数值化断言；写入 `systemPatterns.md`「wpt fixture 数值化适配模式」。**根因**：R3 wpt 005 SKIP + R4 wpt 006/007 ID→class + container→display:inline 共 3 次同模式适配。
9. **【P2 新增】「测试该测什么」的边界发现** — 当上层组件（如 CssParser）有自然错误恢复路径，e2e 测试可能 trivially PASS（over-match），需提取内部 helper（`internal::` namespace）直接 unit test 才能真证「实现存在」。本次 ContainsBlacklistKeyword 是范例。
10. **【P2 新增】「副产品修 pre-existing bug」scope 边界标准** — 本任务 R3 顺手修 `overflow: auto` 走 `ValueType::kAuto` 不进 ParseEnumValue 是「scope 内顺手 1 行 fix」典型；判定标准：(a) 由本 Round 测试触发 / (b) 修复 ≤ 5 行 / (c) 不引入新结构性改动 → 可在 progress.md「实证微调」列记，不必拆任务。
11. **【P3 触发型】wpt 拉取的 commit hash 因 GitHub API rate-limit 暂记 'unknown'** — 本任务 R0.2 实际值缺失但不阻塞；累计 3+ 任务再出现即立项升级（如离线缓存 / 本地 git submodule 镜像）。
12. **【正面教训】反复模式 0 重复触发** — 见 §3.5；本任务无任何已知反复模式重现，VAN 阶段 6 项 grep 实证 + creative 完整锁定 + 多轮次中间态协议成熟应用三联合保住了流程质量。

### d. 流程改进

- **头脑风暴阶段** — VAN 6 grep 实证 + 5 包候选用户决策 + V1-V5 5 维矩阵锁定，全部充分。✅
- **计划详尽度** — spec 17 验收标准 + 8 风险登记 + 6 决策矩阵 + 11 改 + 9 新文件清单非常详尽，但**未规定 LayoutInline 默认 width 语义 + inline-block atomic 高度边界 + vertical-align 单一坐标约定**（教训 #3 / #5）。
- **TDD 流程** — 严格遵守，4 次反向探针完整循环。✅
- **代码审查** — 跨 Round commit 用户确认门相当于轻量审查，未发现遗漏；建议未来 Level 4 任务在 R3/R4 末加 ReadLints + bench 全场（非仅热点）作为 finalize 退出门。

### e. 技术改进

- **代码质量** — Helper / POD / inline 风格一致，arena 友好；新增模块 (margin_collapse.h / line_box.h) 都是 header-only inline，零 vtable，遵守现有约定。✅
- **测试覆盖度** — +78 cases 覆盖 4 个子任务全部正向 + 反向探针 + e2e + wpt 数值化集成；2 SKIP-w/-rationale 留 P3 边界。✅
- **技术债务** — 本任务消化 4 项，新增 2 项 P3 触发型：
  - **TASK-26-02**（P3，clearance + first/last child with parent margin collapse 完整实施 + 跨函数 chain 传播）— 受 D1.2 LayoutChild API 边界限留
  - **TASK-26-03**（P3，LayoutInline 内部 IFC 递归 + bidi LTR 假设破除 + block 含 inline 匿名 IFC）— 受 D2.D inline-block atomic 决策限留
- **性能考虑** — R3 V1 优化 3 项（IsCollapseThrough sum-merge + Add 严格零跳过 + inline overflow）一次拉回到 +3.2%；R4 同窗口 mean -3.6% / median +2.65%。同窗口对照范式贡献巨大。✅

### f. 安全评估

| 维度 | 状态 | 备注 |
|---|---|---|
| 输入验证 | ✅ | 三件套：count cap 1000 + value len cap 8 KB + 黑名单（expression/behavior/javascript:） |
| 认证/授权 | N/A | 解析层无 user identity |
| 数据保护（加密/脱敏） | N/A | 解析数据，非持久化敏感 |
| 依赖审计 | ✅ | 0 新依赖（spec §6 + F6 实证） |
| 错误信息脱敏 | ✅ | Parser 错误静默 discard，无原文回显 |
| 敏感数据处理 | ✅ | 代理地址单一真相来源 `techContext.md` 占位符，本任务零硬编码 |

---

## 3.5 反复模式识别

| 已知模式 | 历史频率 | 本次是否重复？ | 备注 |
|---|---|---|---|
| 计划文件清单与实际变更不一致 | 9+ 次 | ⚠️ 部分（实证微调 10 项，但属「精化」非「失真」）| 不计入反复，因每项都在 progress.md 标注 + plan 阶段已隐含「实证微调允许」原则（TASK-13 反思 #5 已沉淀） |
| 子代理产出需大量返工（CMake/编译/上下文不足） | 7+ 次 | ❌ 否（全部直执行）| 反而暴露新模式：creative 锁定后子代理 overhead > 收益（教训 #6 P1 新规则） |
| 前置依赖/环境/API 能力未验证 | 8+ 次 | ❌ 否 | VAN 6 grep 实证 + F6 不引入新依赖完整守护 |
| 非默认路径（流式/错误/缓存）遗漏验证 | 4+ 次 | ⚠️ **部分** | LayoutInline 默认 width / inline-block 高度 / vertical-align 坐标约定 3 项 build 阶段才发现，**第 5 次出现** → 升级 P0（教训 #2 / #3） |
| 测试隔离问题（flaky/并行冲突/环境依赖） | 7+ 次 | ⚠️ 部分（ninja 时间戳偏差致 stash-swap 用旧 binary）| 第 9 次新变体「构建系统时间戳 vs git stash」→ 升级 P1（教训 #4） |
| 提交粒度偏离计划（大杂烩提交） | 7+ 次 | ❌ 否 | Q5A 每 Round 1 commit 协议执行 100% |
| TDD 严格度与场景不匹配 | 11+ 次 | ❌ 否 | 反向探针 4 次完整循环；反而新模式「`internal::` 提取直接测」补足边界 |

**反复模式触发的规则升级路径：**

1. **「非默认路径遗漏验证」第 5 次** → 教训 #2 / #3 / #5 升 P0，写入 `writing-plans.mdc`「Layout 类任务必检项」+ `systemPatterns.md`「跨 LayoutType 共用样式属性必须每路径独立 box-model 解析」。
2. **「测试隔离问题」第 9 次新变体** → 教训 #4 升 P1，写入 `writing-plans.mdc` §7 WSL2 稳态协议附录「stash-swap 必含 touch + 全图重建」。

---

## 4. 改进建议（附优先级与落实方式）

| # | 建议 | 优先级 | 落实方式 | 目标文件 / 流程 |
|---|---|---|---|---|
| 1 | Bench 退出门验证默认 stash-swap 同窗口对照 | **P0** | 升 `writing-plans.mdc` §7 WSL2 协议；不允许跨日比对绝对数字 | `writing-plans.mdc` |
| 2 | 跨 LayoutType 共用样式属性必须每路径独立 box-model 解析 | **P0** | 写入 `systemPatterns.md` 新模式段；下次 Layout 任务 plan §0 必检项 | `systemPatterns.md` + `writing-plans.mdc` |
| 3 | Creative 阶段「单一坐标约定 + 公式表」一图（≥2 坐标系/方向算法）| **P0** | 升 `creative.md` 命令模板；当涉及 baseline/ascent/descent/offset 等概念时强制要求 | `creative.md` 命令 |
| 4 | stash-swap 协议补丁：stash pop 后 touch 关键 src + 全图重建 | **P1** | 升 `writing-plans.mdc` §7 附录 | `writing-plans.mdc` |
| 5 | Plan 阶段必须明确 LayoutInline/LayoutBlock 默认 width/height 语义 | **P1** | 升 `writing-plans.mdc` Layout 类任务必检项 | `writing-plans.mdc` |
| 6 | 子代理 vs 直执行 build 阶段重评估机制（creative 锁定 + 单文件 + 上下文连续 → 倾向直执行） | **P1** | 升 `subagent-development.mdc` D2/D3 判定流程图 | `subagent-development.mdc` |
| 7 | 反向探针从「Mixed TDD 标配」升强制条目 | **P1** | 升 `writing-plans.mdc`「Mixed TDD 模式 D3 类回归测试默认含反向探针」 | `writing-plans.mdc` |
| 8 | wpt fixture 数值化适配模式 | **P2** | 写入 `systemPatterns.md`：拷贝 wpt 前必查 (a) ID 选择器范围 / (b) container display 与 vx 匿名 IFC / (c) ref 像素 → 数值化 | `systemPatterns.md` |
| 9 | 「测试该测什么」边界发现：上层错误恢复时提取 `internal::` helper 直接 unit test | **P2** | 写入 `systemPatterns.md`「测试边界发现 — internal helper 提取模式」 | `systemPatterns.md` |
| 10 | 「副产品修 pre-existing bug」scope 边界判定 3 标准 | **P2** | 写入 `systemPatterns.md`「scope 边界 — 副产品修复 3 标准」 | `systemPatterns.md` |
| 11 | wpt commit hash 离线缓存 / 本地 submodule 镜像 | **P3 触发型** | 累计 3+ 任务 GitHub API rate-limit 失败再立项 | — |
| 12 | TASK-26-02 占位（clearance + first/last child margin collapse + 跨函数 chain）| **P3 触发型** | 受 D1.2 LayoutChild API 边界限；下次 Layout 性能/正确性 workload 驱动立项 | `tasks.md` 候选 |
| 13 | TASK-26-03 占位（LayoutInline 内部 IFC 递归 + bidi LTR 破除 + block 含 inline 匿名 IFC） | **P3 触发型** | 受 D2.D inline-block atomic 决策限；下次国际化/RTL/复杂 IFC 驱动立项 | `tasks.md` 候选 |

---

## 5. 技术改进建议

1. **跨 Round 共享数据结构 ABI 渐进扩展** — 本任务 TextMetrics ABI 兼容拆 ascent/descent + `[[deprecated]]` 是范例；可定型为 `systemPatterns.md`「ABI 兼容渐进扩展模式」。
2. **CSS 全链路 6/7 文件 fingerprint** — 本任务 R0.3 F4 grep 提供了 kLineHeight 模板；下次新增 enum CSS property（如 R4 VerticalAlign 模仿 LineHeight 失败一次因 transition 不参与）应升级 fingerprint 区分「参与 transition」vs「不参与」两种模板。
3. **Layout 引擎 helper inline 常态化** — `IsInFlow` / `CreatesBlockFormattingContext` / `IsCollapseThrough` / `ResolveLineHeightPx` / `ComputeInlineMetrics` / `ComputeNonExtremeAlign` / `FinalizeLine` 全部是 anonymous namespace inline；本风格在 V1 优化时 inline 调用站可直接做调用消除（合 sum 等），效率高。
4. **POD 头文件作为 layout 子模块入口约定** — `margin_collapse.h` 与 `line_box.h` 都是 POD + inline + 零 vtable + arena 友好；建议下次新 layout 子模块直接套此约定。

---

## 6. plan × 0.6 第 13 数据点

| 任务 | plan (min) | 实测 (min) | ratio | 类型 |
|---|---:|---:|---:|---|
| TASK-20260426-01 | 900 | ~400 | **0.44×** | Level 4 多轮次 |

**跨数据点收敛**：

| # | 任务 | ratio | 子档 |
|:-:|---|---:|---|
| 5 | TASK-24-01 | 0.29× | 最窄路径（研究类） |
| 6 | TASK-24-03 | 0.34× | 最窄路径带 R1 升级（优化类） |
| 7 | TASK-24-04 | 0.26× | 最窄路径（D 纯收尾优化类） |
| 9 | TASK-25-01 | 0.22× | 最窄路径（platform backend 增量） |
| 10 | R1 子任务 | 0.50× | 准确档（单文件 D2）|
| 11 | R2 子任务 | 0.56× | 准确档（单文件 D2 含 8 直接测试）|
| 12 | R3 子任务 | 0.37× | 中位（D3 算法 creative 锁定 + V1 优化）|
| 13 | R4 子任务 | 0.50× | 中位（D3 算法 creative 锁定 + 边界 case 撞回 ~30 min）|
| 13 | **整体 Level 4** | **0.44×** | **Level 4 首数据点：介于「最窄 0.3×」与「准确 0.6×」** |

**结论**：

- Level 4 多轮次任务首数据点 0.44× 落在「准确档（0.6×）下方 + 最窄档（0.3×）上方」的中段区间
- 与跨子任务平均 0.43×（R1-R4 平均）一致
- 写入 `systemPatterns.md`「bench 类任务估时校准」段：**Level 4 多轮次任务在「creative 完整锁定 + 多轮次协议成熟 + VAN grep 实证」三联合下应按 plan × 0.4-0.5× 预估**，不能简单按子任务相加（子任务平均会受最慢的 R4 拉高，整体含 R0/R5 meta 反而拉低）

---

## 7. 落实状态（截至本回顾完成）

| # | 落实项 | 状态 |
|---|---|---|
| F1 | Release 全量 ctest 1029/1029 PASS in 2.21s（`-O3 -Werror` 0 warn） | ✅ |
| F2 | 同窗口对照 bench Flat/64 mean -3.6% / median +2.65% ≪ ±10% | ✅ |
| F3 | techContext.md #20/#21/#25/#28 状态全部 ⏳→✅ | ✅（本 reflect 阶段一并消化） |
| F4 | activeContext.md / progress.md / tasks.md 同步 | ⏳（本 reflect 阶段第 5 步同步） |
| F5 | git config 代理 unset（`git config --global --get http.proxy` 为空 + `https.proxy` 为空） | ✅ |
| F6 | git rebase main + push origin → 进 /reflect | ⏸（push 由用户 /archive 阶段决策） |
| F7 | reflection-TASK-20260426-01.md 落盘 | ✅（本文件） |

---

## 8. 下一步

- **本 reflect 完成后** → 提交 reflection + 同步 Memory Bank → 等待用户 `/archive`
- **/archive 阶段** → `--no-ff` 合并 main + 落实 P0/P1 改进（教训 #1 / #2 / #3 / #4 / #5 / #6 / #7 → 7 条规则升级）+ 归档 `archive-TASK-20260426-01.md`

---

**回顾人：** AI Agent
**完成时间：** 2026-04-29
**关联文档：**
- `docs/specs/2026-04-26-layout-correctness-design.md`
- `docs/plans/2026-04-26-layout-correctness.md`
- `memory-bank/creative/creative-margin-collapsing.md`
- `memory-bank/creative/creative-line-box-model.md`
- `memory-bank/progress.md`
- `memory-bank/tasks.md`
- `memory-bank/activeContext.md`
