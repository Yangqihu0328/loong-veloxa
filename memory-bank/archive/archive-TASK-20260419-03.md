# 归档：CSS 解析性能基准（Tokenizer / Parser / PropertyLookup）

**日期：** 2026-04-19
**任务 ID：** TASK-20260419-03
**复杂度级别：** Level 2
**状态：** ✅ 已完成
**分支：** `feature/TASK-20260419-03-css-benchmarks`（基线 `main` `9482fb5`）
**工作流路径：** `/van` → `/plan`（5 轮 brainstorm + 1 phase tweak）→ `/build` Round 1（P0+P1+P2，被 TASK-04 中断 + 续接）→ `/reflect` round 1 → `/build` Round 2（P3+P4+P5+P6） → `/reflect` 整体 → `/archive`

---

## 任务概述

接续 TASK-20260419-02 Foundation 基准脚手架，把性能基准能力扩展到 **核心 CSS 解析链**：

- 新增 3 个 Release-only benchmark 可执行文件：`bench_css_tokenizer` / `bench_css_parser` / `bench_css_property_lookup`
- 共 **30 BMs**（10 + 11 + 9）覆盖 tokenize / parse / property lookup 全主流路径 + cluster 探针
- 入仓 3 份 Release baseline JSON 作为「形态参考锚」，配套独立失真兜底 README
- 通过 cluster 度量为 TASK-06（HashMap Hash Mixing）候选项做实证降级

**非目标：**

- 不引入 CI 性能门禁（baseline JSON 仅作 PR 评审视觉锚，不卡点）
- 不做 SIMD / 多线程深度 profiling
- 不覆盖 Layout / Render（拆为 TASK-05 候选）

---

## 技术方案

### 设计决策（5 轮 brainstorm，全部 A/c 方案 + 1 phase tweak）

| ID | 维度 | 选择 | 理由 |
|----|------|------|------|
| Q1 | CSS 语料 | C 生成器（参数化合成 + 缓存） | 完全可控；缓存 IIFE 把生成成本逐到 BM 计时外；可复现 |
| Q2 | 解析深度 | B 中等（包含 `Parse` + `ParseDeclarationList` + selector 复合） | 第一版基线足够覆盖 Production 路径；Wide / Large 形态可见 quadratic 退化 |
| Q3 | Property 探针 | A 单独 exe + cluster 探针 | 隔离崩溃；5 个 single key 探针成本极低，能直接产出 cluster 实证 |
| Q4 | 结果留存 | B 入仓 baseline JSON（+ 失真兜底 README） | CSS 模块成熟稳定；3 份 JSON ~15 KB；强制配套独立 README 4 条失真警告 + 5 条更新硬规 |
| Phase tweak | Plan 拆分 | split baseline | 把 baseline JSON 生成 + 收尾从 P5 拆出独立 P6，便于轮次切分 |

### 架构特点

- **`vx_add_benchmark()` 升级为可变参数**：`vx_add_benchmark(name [extra_libs...])`，4 个 Foundation 旧调用 ARGN 为空 100% 向后兼容；3 个 CSS 调用追加 `vx_core` 链接 `CssTokenizer / CssParser / PropertyIdFromName` 符号
- **header-only `css_corpus.h`**：mutex-protected static map 缓存 `StylesheetCorpus(rules, decls)` / `InlineStyleCorpus(decls)`；3 份固定 property 名常量数组（All 60 / Hot 5 / Miss 5）— 不污染 4 个 Foundation bench
- **Cluster 探针设计**：`BENCHMARK_TEMPLATE(BM_PropertyLookupHitSingle, kKey, sizeof(kKey)-1)` × 5 固定 key（display / color / margin-top / border-radius / transition-timing-function），与 `BM_PropertyLookupHitHot5` 形成 N+1 数据点对比 — 任何 ≥ 5× 跨度即 cluster 信号
- **入仓 baseline 失真兜底**：`benchmarks/baseline/README.md` 强制 4 条失真警告 + 5 条更新硬规 + JSON `library_build_type=="release"` 体检 + `compare.py` 同机相对比较工作流；**禁止**绝对数字成 CI 卡点
- **TDD 模式：覆盖补充**：spec §7 + plan §TDD 模式声明显式延续 TASK-02 模式 — 既有 `tests/core/css/{tokenizer,parser,property}_test.cc` 作正确性回归（全任务期间 Debug 890/890 持续通过 0 回归），benchmark 不写新 GTest

---

## 实现摘要

**变更规模：** 19 文件，+3324/-42 行，11 个提交（plan × 1 + WIP P1 × 1 + chore mb resume × 1 + feat × 4 + chore finalize × 1 + docs × 2 + reflect × 2），**全测试 890/890 通过**（Debug 全绿 + Release 7 个 bench exe 全 clean，0 警告 0 错误于 bench 目标）。

### 文件变更

| 操作 | 文件路径 | 说明 |
|------|---------|------|
| 创建 | `docs/specs/2026-04-19-css-benchmarks-design.md` | 设计规格（4 决策 + cluster 阈值 5× + 失真兜底原则） |
| 创建 | `docs/plans/2026-04-19-css-benchmarks.md` | 7 phase 实现计划（含风险预案 4 行 / 完成标准 8 项） |
| 创建 | `benchmarks/css_corpus.h` | header-only 语料生成器（mutex-static cache + 3 个 property 名数组） |
| 创建 | `benchmarks/bench_css_tokenizer.cc` | 10 BMs：BM_TokenizeAll/Range(64,4096) 7 + 3 heavy 形态 |
| 创建 | `benchmarks/bench_css_parser.cc` | 11 BMs：4 stylesheet (Small/Medium/Large/Wide) + 6 DeclList Range + 1 SelectorMixed |
| 创建 | `benchmarks/bench_css_property_lookup.cc` | 9 BMs：HitAll + HitHot5 + 5 HitSingle + Miss + BuildInit（含 cluster 探针） |
| 创建 | `benchmarks/baseline/README.md` | 失真兜底文档（4 条警告 + 5 条更新硬规 + 命令模板 + 历史表） |
| 创建 | `benchmarks/baseline/bench_css_tokenizer.json` | Release baseline JSON（context 含 release 标识） |
| 创建 | `benchmarks/baseline/bench_css_parser.json` | Release baseline JSON |
| 创建 | `benchmarks/baseline/bench_css_property_lookup.json` | Release baseline JSON |
| 修改 | `benchmarks/CMakeLists.txt` | `vx_add_benchmark()` 升级为可变参数 + 注册 3 个 CSS exe |
| 修改 | `benchmarks/README.md` | CSS 章节 + baseline 工作流 + 已知量级表新增 5 行 + 注意事项追加 |
| 创建 | `memory-bank/reflection/reflection-TASK-20260419-03.md` | 整体回顾（plan 全 7 phase） |
| 创建 | `memory-bank/reflection/reflection-TASK-20260419-03-round1.md` | Round 1 轮次回顾（P0+P1+P2） |
| 修改 | `memory-bank/systemPatterns.md` | +2 已验证模式（失真兜底设计 + 发现型 phase plan 模板） |
| 修改 | `memory-bank/techContext.md` | +CSS 性能基线段（30 BMs 数量级 + cluster 判定）+ 沙箱 _deps 只读失败模式 |
| 修改 | `memory-bank/activeContext.md` | 长期项升 P0 #1 (proxy) + P1 #2 (TASK-07) + P3 #3 (TASK-06 降级) |
| 修改 | `memory-bank/tasks.md` | 任务历史 + 整体产出 + 改进项落实状态 |
| 修改 | `memory-bank/progress.md` | 整体 REFLECT 摘要 |

### 提交清单（feature 分支 ahead-of-main 11 commits）

| Commit | 说明 |
|--------|------|
| `0a9c6fd` docs(plan) | TASK-20260419-03 design + implementation plan |
| `430a61e` wip(TASK-03) | Phase 1 CSS bench scaffolding (BLOCKED on TASK-04) — 历史保留 subject 字面已过期 |
| `dfdf556` chore(mb) | resume TASK-20260419-03 — phase to BUILD after TASK-04 unblock |
| `4d014ec` feat(benchmarks) | CSS tokenizer suite [P2] |
| `fb9f8eb` chore(build) | finalize TASK-20260419-03 round 1 (P1 verify + P2) memory bank state |
| `86b1495` docs(reflect) | add round-1 reflection for TASK-20260419-03 |
| `1e896d3` feat(benchmarks) | CSS parser suite [P3] |
| `66f0a10` feat(benchmarks) | CSS property lookup + cluster probe [P4] |
| `36c7e9c` docs(benchmarks) | document CSS suite + baseline retention policy [P5] |
| `65aa75f` feat(benchmarks) | CSS baselines + finalize [P6] |
| `0ed0391` docs(reflect) | add overall reflection for TASK-20260419-03 |

### 关键决策

1. **被 TASK-04 中断时选择「暂停 + 单独修复 + rebase 续接」而非「inline pragma 抑制」** — 经 TASK-04 选 C 方案 detemplatize 根因消除，本任务 Round 1 + Round 2 fresh build 两次实地确认 0 `-Werror=array-bounds`，验证此决策正确
2. **入仓 3 份 baseline JSON 而非完全不入仓**（与 TASK-02 Foundation 决策不同）— CSS 模块成熟、change frequency 低、需要给后续 Layout/Render bench 提供形态对比锚；强制配套失真兜底 README 防止数字被滥用为 CI 卡点
3. **Round 1 完成后自我修正提交粒度纪律** — Round 1 出现 2 个独立 chore commit（resume + finalize），Round 2 主动把 MB 阶段切换 fold 进 P3 commit 实现零冗余；这是同一任务内 reflection 直接生效的少见样本
4. **Cluster 探针采用 5 固定 key + `BENCHMARK_TEMPLATE` 模式而非动态参数** — 编译期常量保证零编译期开销、key 长度信息可见、N 个独立 BM 行便于直接读取均匀性；该模式可复用到任何 HashMap-based 查表场景
5. **bench 目标在 main 上有 Release `-Werror` 失败时绕行而非阻塞** — 发现 `tests/platform/memory_surface_test.cc fgets` + `tests/foundation/strings/string_test.cc array-bounds` 失败后，改用 `cmake --build build-bench --target <仅 7 个 bench exe> -j`，bench 目标干净；同时立 TASK-07 作为后续修复

### 安全决策

**本任务不涉及安全变更：**
- 无外部输入（CSS 语料完全由 `css_corpus.h` 内部生成，60 个 property 名为固定常量数组）
- 无新依赖（google/benchmark v1.9.1 已在 TASK-20260419-02 完成 CVE 审计）
- 无认证 / 数据保护 / 错误响应

---

## 测试覆盖

**TDD 模式：覆盖补充（与 TASK-02 一致）**

- 既有正确性测试：`tests/core/css/tokenizer_test.cc` / `parser_test.cc` / `property_test.cc` — 全任务期间 Debug 890/890 持续通过，**0 回归**
- 本任务新增：0 GTest（基准不写正确性测试，符合 plan 模式声明）
- 验收方式：每 phase 编译过 + 跑出非零数字 + BM 行计数 ≥ 设计值
- 验收结果：30/30 BM 行完全匹配设计（10 + 11 + 9）；3 份 baseline JSON 全部 `context.library_build_type == "release"` 通过体检

**完整验证矩阵（plan 完成标准 8/8 全绿）：**

- ✅ 7 个 phase 全绿（P0-P6）
- ✅ 全测试 890/890 通过（无回归）
- ✅ 全量构建 0 警告 0 错误（Debug 与 Release bench 目标各一次）
  - 附注：Release 全量 build 时发现 main 已存在 2 个测试文件 `-Werror` 失败，绕行用 target 列表 build；建议立 TASK-07 修复
- ✅ 3 个 CSS bench exe 编译通过且各打印 ≥ 设计值的 BM 行（10 / 11 / 9 = 30）
- ✅ 3 个 baseline JSON 入仓且 `context.library_build_type == "release"`
- ✅ `benchmarks/README.md` CSS 章节完整 + baseline 工作流可独立复现
- ✅ cluster 度量数据写入回顾，作为 TASK-05/06 立项判据 — 实际产出 = TASK-06 P1→P3 降级 + 8 个均匀数据点

**性能基线数量级（Release / WSL2 8 核 ~2.92 GHz / gcc 11.4 / `--benchmark_min_time=0.5s`）：**

| 模块 | 关键 BM | 数量级 |
|------|---------|--------|
| Tokenizer | `BM_TokenizeAll/Range(64,4096)` 7 case | 297-340 MiB/s（变异 ±7%） |
| Tokenizer | StringHeavy / WhitespaceHeavy / NumericHeavy | 614 / 603 / 372 MiB/s |
| Parser | `BM_ParseStylesheet*` 4 形态 | 102-136 MiB/s（约 Tokenizer 1/3） |
| PropertyLookup | HitHot5 / HitAll / 最慢 single key | 10.4 / 14.9 / 28.6 ns |
| PropertyLookup | Cluster 度量 | 最慢 2.75× HitHot5 < 5× 阈值 → **未触发 cluster** |

---

## 经验教训

### 关键技术教训

1. **Cluster 度量产出的实证降级机制** — 立项时基于 TASK-02 std::hash<int> identity-mapping cluster 发现，假设 PropertyMap 的 djb2 + StringView 也可能 cluster；plan 风险预案 row 2 预设了「实测均匀 → 降级」动作，实测确实未触发，预案直接照搬出 TASK-06 P1→P3 降级。教训：**任何「立项假设新问题存在 → 用基准证伪/证实」型 phase，plan 阶段必须预设两路结果对应的下一步动作**，否则实测否定预期时容易陷入决策犹豫。已沉淀到 `systemPatterns.md`「带否定判据的发现型 phase 的 plan 模板」段
2. **fresh Release build 的全栈通路副发现高价值** — 在「日常 Debug 优先 + 增量 Release」开发模式下，main 上的 Release `-Werror` 失败可隐藏数月（TASK-04 `Lookup<N>` 隐藏 ~2 周；本次 `memory_surface_test.cc fgets` + `string_test.cc array-bounds` 不知隐藏了多久）；单任务 fresh `rm -rf build-bench && cmake --build -j` 是唯一会触达全栈 Release 通路的高频时机。教训：**fresh build 失败必须强制记录到反思**，即使本任务可绕行
3. **入仓基线数字必须配套失真兜底** — 入仓 baseline JSON 时强制写独立 README 含 4 条失真警告 + 5 条更新硬规 + JSON `library_build_type` 体检 + `compare.py` 同机对比工作流；此设计成本（1 个 README）远小于「未来用户拿这些数字做 ±10% CI 卡点」的预期纠错成本。已沉淀到 `systemPatterns.md`「入仓基线数字的失真兜底设计」段

### 流程教训

1. **多 phase 任务被中断后续接成本可观** — 本任务被 TASK-04 中断，续接成本 ~10 min（rebase + 冲突解决 + MB 续接 commit）；但意外提供了 round 1 / round 2 之间的反思纠偏机会（提交粒度纪律改进、cluster 数据反过来佐证 Tokenizer 性能基线）。**轮次切分窗口** = 强制反思机会，可作为 plan 阶段切分粒度的考量因素
2. **「轮次完成」中间态在现有命令链里缺乏支持** — `/reflect` → `/archive` 假设「一轮即完成」，多 phase 任务（phase ≥ 5）需要支持多次 `/build` 续接同一 task；本任务 Round 1 后手动 StrReplace 把阶段从「回顾中」改回「构建中」，无强制守卫干预 — 走通了但是因为人脑清楚 invariant，不是流程支持。改进项 P1 #4 已落到 activeContext 长期项
3. **Cursor 沙箱 git proxy 反复 9+ 次** — 单次低成本可绕行（5-10 分钟）但累计成本 ≥ 1 小时却始终未升 P0；本次破例升 P0，下次 FetchContent 任务前必须固化到 `main.mdc` 强制 checklist
4. **WIP commit subject 含外部任务状态会过期** — `wip(TASK-03): Phase 1 CSS bench scaffolding (BLOCKED on TASK-04)` 在 TASK-04 解锁后字面失效，git log oneline 长期可见误导阅读者；改进项 P1 #8 已落到 activeContext 长期项

### 改进项汇总（10 项 → activeContext / systemPatterns / techContext 已落）

- 🔴 **P0 ×1**：Cursor 沙箱 FetchContent 任务前置 proxy checklist
- 🟠 **P1 ×4**：TASK-07 立项（main 2 个 Release `-Werror` 失败修复）/ TASK-06 P1→P3 降级 / 多轮工作流守卫 / WIP commit subject 中性化
- **P2 ×4**：发现型 phase plan 模板 / fresh build 副发现强制记录 / 入仓基线失真兜底设计 / CSS 性能基线沉淀
- **关闭 ×1**：Rebase MB 冲突标准处理（Round 1 已沉淀，本次二次验证）

---

## 后续候选任务（来自本任务分析）

| 任务 ID | 复杂度 | 优先级 | 说明 |
|---------|--------|--------|------|
| TASK-20260419-05 | Level 2-3 | P1 | Layout + Render 基准（`bench_layout_buildtree` / `bench_layout_flex` / `bench_render_record` / `bench_render_replay`），可用本任务 baseline 作性能预算反推参考锚 |
| TASK-20260419-06 | Level 2 | **P3 (降级)** | HashMap Hash Mixing 优化 — 触发条件改为「短字符串 ≠ 主用例 + 容器规模 > 1000 entry」的新场景出现时再立项；不主动追求 PropertyMap 类场景优化 |
| TASK-20260419-07 | Level 1 | P1 | 修复 main 已存在 2 个 Release `-Werror` 失败：(a) `tests/platform/memory_surface_test.cc:102/105/108` `fgets -Wunused-result`；(b) `tests/foundation/strings/string_test.cc` `-Werror=array-bounds`（GCC IPA 误报，参考 TASK-04 detemplatize / pragma 套路） |

---

## 参考文档

- 设计规格：`docs/specs/2026-04-19-css-benchmarks-design.md`
- 实现计划：`docs/plans/2026-04-19-css-benchmarks.md`
- 创意设计：N/A（Level 2，无 `/creative` 阶段）
- 整体回顾：`memory-bank/reflection/reflection-TASK-20260419-03.md`
- Round 1 轮次回顾：`memory-bank/reflection/reflection-TASK-20260419-03-round1.md`
- 失真兜底协议：`benchmarks/baseline/README.md`
- 模块入口：`benchmarks/README.md`「Foundation + Core CSS Benchmarks」
- 关联归档：`memory-bank/archive/archive-TASK-20260419-02.md`（前置 Foundation 基准脚手架）/ `memory-bank/archive/archive-TASK-20260419-04.md`（中途解锁 `Lookup<N>` 修复）
