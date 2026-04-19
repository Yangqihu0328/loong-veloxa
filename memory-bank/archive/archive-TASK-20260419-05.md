# 归档：Layout + Render 性能基准（4 bench exe）

**日期：** 2026-04-19
**任务 ID：** TASK-20260419-05
**复杂度级别：** Level 2-3
**状态：** ✅ 已完成
**分支：** `feature/TASK-20260419-05-layout-render-benchmarks`（10 commits）

## 任务概述

在已有 Foundation（TASK-02, 4 bench / 40 BM）+ CSS（TASK-03, 3 bench / 30 BM）性能基线之上，新增 **Layout + Render 模块** 的 4 个 google/benchmark 可执行（共 30 BM 行），覆盖 LayoutEngine 的 block flow / flex / nested 三类构造路径，与 Render 管线的 Record / Replay 两阶段。

**目标**：
1. 让 Layout / Render 子系统拥有可量化对比的形态参考基线（baseline JSON 入仓）
2. 用「带否定判据」方法验证 ImageCache 是否为 Replay hot path
3. 为后续 Layout / Render 优化任务提供数据起点

**最终交付**：4 bench exe + 1 共享 corpus header + CMakeLists 注册 + 4 baseline JSON + 2 README + Memory Bank 5 项 K-finding。

## 技术方案

### 整体架构

| 阶段 | 输入 | 输出 | bench 文件 |
|------|------|------|----------|
| BuildTree（block flow） | `dom::Document` | `LayoutBox` 树 | `bench_layout_buildtree.cc` |
| Flex | `dom::Document`（display:flex） | `LayoutBox` 树（flex 排布完成） | `bench_layout_flex.cc` |
| Record | `LayoutBox*` 根 | `Vector<PaintCommand>` (DisplayList) | `bench_render_record.cc` |
| Replay | DisplayList + `Canvas*` | 像素写入 | `bench_render_replay.cc` |

### 关键设计点

1. **Corpus 程序化构造**（仿 TASK-03 css_corpus 模式）— `benchmarks/layout_corpus.h` 提供 7 种 builder（flat / nested / mixed / text-heavy / flex / nested-flex / image），各自配 mutex-protected static map 缓存，跨 BM 复用同一 Document 指针
2. **Styled corpus 子集**（Phase 6 新增）— Render bench 必需，`BuildFlatStyled` / `BuildTextHeavyStyled` / `BuildImageStyled` 通过 `Element::SetInlineDeclaration` 给元素加 `background-color`，触发 RecordBox 的 `bg.a > 0` emit gate
3. **2D Flex 矩阵**（Phase 4） — `BENCHMARK_TEMPLATE<Rows, Cols>` 5 固定点（1×8 / 1×32 / 1×128 / 8×8 / 16×16）+ 1 嵌套 flex
4. **三阶段缓存**（Phase 5/6）— `LaidOut` / `Recorded` 结构持有 `unique_ptr<ArenaAllocator>` + LayoutBox 指针 + DisplayList，确保 LayoutBox 寿命覆盖整个 BM 循环
5. **共享 SoftwareCanvas**（Phase 6） — `SharedCanvas()` 用 process-static 256×256 RGBA 像素 buffer，避免 BM 内层分配噪音
6. **「带否定判据的发现型 Phase」方法论复刻**（Phase 6） — 设 hypothesis（ImageCache 是 hot path）+ 阈值（5×）+ 对比组（ImgVsNoImg）+ 接受任意结果；最终否定原假设，意外定位 DrawText 才是 820× hot path

## 实现摘要

### 文件变更

| 操作 | 文件路径 | 说明 |
|------|---------|------|
| 创建 | `benchmarks/bench_layout_buildtree.cc` | 3 BM × 14 行（Flat / Nested / Mixed） |
| 创建 | `benchmarks/bench_layout_flex.cc` | 6 BMs（5 BENCHMARK_TEMPLATE + 1 NestedFlex） |
| 创建 | `benchmarks/bench_render_record.cc` | 5 BMs（Small/Medium/Large/TextHeavy/ImgVsNoImg） |
| 创建 | `benchmarks/bench_render_replay.cc` | 5 BMs（同上 shape，Replay 到 SoftwareCanvas 256×256） |
| 创建 | `benchmarks/layout_corpus.h` | 7 builder + 3 Styled getter + mutex-protected static cache |
| 创建 | `benchmarks/baseline/bench_layout_buildtree.json` | Release 基线 |
| 创建 | `benchmarks/baseline/bench_layout_flex.json` | Release 基线 |
| 创建 | `benchmarks/baseline/bench_render_record.json` | Release 基线 |
| 创建 | `benchmarks/baseline/bench_render_replay.json` | Release 基线 |
| 修改 | `benchmarks/CMakeLists.txt` | 4 行 `vx_add_benchmark` 注册（replay 额外链 vx_graphics + vx_platform） |
| 修改 | `benchmarks/README.md` | exe 表 7→11；增 Layout/Render 数量级摘要 + 工程注意事项 |
| 修改 | `benchmarks/baseline/README.md` | CSS 章节后追加 Layout/Render；新增「Key findings」段（K1-K5）|
| 修改 | `memory-bank/techContext.md` | 新增「Layout 性能基线」+「Render 性能基线」段（与 CSS 性能基线段并列）|
| 修改 | `memory-bank/systemPatterns.md` | 新增 4 段：「带否定判据的发现型 Phase」方法论 / 「Render Bench 前置清单」/「跨阶段管道型 API default-nullptr 反模式」/「Bench Smoke 自检模式」|
| 修改 | `.cursor/rules/skills/writing-plans.mdc` | 新增「性能基准任务必检项」段（6 子项，落实 reflection P0 #2） |
| 创建 | `memory-bank/reflection/reflection-TASK-20260419-05.md` | 8 项改进建议 + 反复模式表 |
| 创建 | `docs/specs/2026-04-19-layout-render-benchmarks-design.md` | 设计文档（本任务 plan 阶段产出） |
| 创建 | `docs/plans/2026-04-19-layout-render-benchmarks.md` | 7-phase 实现计划 |

### 关键决策

1. **Corpus = 纯程序化 DOM API**（不读文件、不依赖 css 解析）— 仿 TASK-03 `css_corpus`，零外部 fixture 依赖；后期意外发现 default div alpha=0 → 必须新增 Styled 子集（4 个 getter）
2. **Flex bench = 二维 BENCHMARK_TEMPLATE**（不用 Range）— 5 固定点直观可读，避免 Range 隐含的 RangeMultiplier 案例数预估错误
3. **ImageCache 真测延后** — Phase 6 探查 5 分钟即识别 `DecodeFromFile` 强 I/O 依赖 + 三阶段链（layout/Record/Replay）任一不传 cache 即断 → 按 plan 退化路径走（5 BMs 用 `cache=nullptr`），把工程问题（fixture 文件复制 + 三阶段同传）整体推 TASK-20260419-09 候选
4. **4 baseline JSON 全入仓 + 复用 TASK-03 4-piece 失真兜底协议** — 入仓视为「形态参考」非 SLA，README 显式说明数字会随机器/编译器漂移；与 CSS baseline 同级管理
5. **「带否定判据的发现型 Phase」方法论第 2 次复刻**（最高价值决策） — 设 5× 阈值 + 对比组 + 接受任意结果；意外发现 DrawText 才是真 hot path（820× FillRect）

### 安全决策

**本任务不涉及安全变更。**

性能基准任务全程：
- 无外部输入（corpus 全程序化生成）
- 无认证路径
- 无敏感数据
- 无新依赖（google/benchmark v1.9.1 已在 TASK-02 接入并审计）
- 错误信息无外部传播（仅 BM stderr 用于本地调试）

## 测试覆盖

- **回归测试**：Debug `ctest -j --output-on-failure` 全程 890/890 通过，零回归（layout / render / dom / css 既有 unit test 已充分覆盖正确性）
- **bench 自验证**：11 bench targets（4 Foundation + 3 CSS + 4 Layout/Render）Release `--benchmark_min_time=0.001s` smoke 全部 exit 0
- **数据有效性**：4 baseline JSON 头部 `library_build_type=release` ✅；所有 BM `items_per_second` 非零 ✅（Phase 6 styled-corpus 修正后）

**TDD 模式：** 「覆盖补充」— 既有 layout/render GTest 已保证正确性，本任务全程未新增 GTest；每 phase 验收标准 = Release build 零 -Werror + 数字非零 + BM 数符合设计 + DisplayList 非空（Phase 6 后增此条）

## 关键发现（K1-K5，详见 baseline/README + techContext）

| # | 发现 | 数值 | 影响 |
|---|------|------|------|
| **K1** | **Replay hot path = `DrawText`，不是 ImageCache** | DrawText 8200 ns/cmd vs FillRect 10 ns/cmd = **820×** | 立 TASK-09 候选；渲染优化方向定位 DrawText |
| K2 | Layout buildtree-flat super-linear knee | N=128→256 时 7.7→70 µs（10× for 2× N）| 同源根因待 TASK-09 调查 |
| K3 | Layout flex 同源 super-linear | 8×8→16×16 时 4.9→73 µs（14.9× for 4× cells）| 候选根因：cache miss / arena chunk grow / SmallVector 阈值 |
| K4 | Record 对 image 元素无额外开销 | image_handle=0 → RecordBox 跳过；ImgVsNoImg 16 ≈ Medium 64/4 | 设计正确，Record 阶段瓶颈不在 image |
| K5 | ImageCache 真测 fixture 缺失 | `DecodeFromFile` 需 I/O；layout 不传 cache → 三阶段链断 | 推 TASK-09（需 `configure_file()` 复制 PNG fixture） |

## 经验教训

### 高价值正面教训

1. **plan 详尽度 vs build 速度强正相关（再次实证）** — 预估 4.25h 实际 75 min（提速 70%）；TASK-03 同模式 25% 加速规律在本次放大到 70%，规律稳定
2. **「带否定判据的发现型 Phase」是可复制方法论** — 4 要素（hypothesis / threshold / 对比组 / 接受任意结果），TASK-03 (cluster) + TASK-05 (hot path) 2 次成功，现已沉淀到 `systemPatterns.md` 作为正式模式
3. **跨 bench 同源现象识别**（K2 + K3）—  buildtree-flat 与 flex 双双 super-linear knee 强烈暗示共享根因（不属本任务，但下一轮研究问题已成型）

### 反复模式触发与升级

| 模式 | 历史频率 | 本次 | 处理 |
|------|---------|------|------|
| **API 能力未验证（render bench 默认 div 不 emit FillRect）** | 8+ 次 | ✅ 第 9+ 次 | **升 P0**：写入 `writing-plans.mdc`「性能基准任务必检项」§3 |
| **方案根因假设未验证**（TASK-07 同模式重现） | 1 次 | ✅ 第 2 次 | **升 P0**：同上段 |
| 计划文件清单与实际变更不一致 | 9+ 次 | ✅ 是（+3 Styled getter / techContext 段未补） | 影响轻微 |
| 提交粒度偏离计划 | 7+ 次 | ❌ 否（严格 1 phase 1 commit） | 良好 |
| TDD 严格度不匹配 | 11+ 次 | ❌ 否（覆盖补充按 plan 声明） | 良好 |

### 沉淀到长期知识库的内容

- `techContext.md` — 「Layout 性能基线」+「Render 性能基线」2 段（K1-K5 完整数值表 + bench 工程约束）
- `systemPatterns.md` — 4 段：
  1. 「带否定判据的发现型 Phase」方法论（4 要素 + 2 次实证表）
  2. 「Render Bench 前置清单」（FillRect / FillRoundedRect / StrokeRoundedRect / DrawText / DrawImage 各自的 emit 条件 + 2 个隐式契约）
  3. 「跨阶段管道型 API default-nullptr 反模式」（ImageCache 三阶段链断警示）
  4. 「Bench Smoke 自检模式」（数字非零 + SetItemsProcessed > 0 + JSON 校验三件套）
- `writing-plans.mdc` — 「性能基准任务必检项」段（6 子项：Release + helper 签名 + 发射条件 grep + Smoke 三件套 + RangeMultiplier 公式 + Render 前置清单交叉引用）

### 改进建议落实状态（reflection 8 项）

| # | 建议 | 优先级 | 落实状态 |
|---|------|--------|---------|
| 1 | techContext.md 补 Layout/Render 性能基线段 | P0 | ✅ /reflect 阶段已落实 |
| 2 | writing-plans.mdc 「目标 API 发射条件 grep」段 | P0 | ✅ **本次 /archive 阶段已落实**（新增「性能基准任务必检项」§3） |
| 3 | bench smoke 验收升级（SetItemsProcessed > 0） | P1 | ✅ 沉淀到 systemPatterns.md + writing-plans.mdc §4 |
| 4 | 「Render bench 前置清单」沉淀 | P1 | ✅ 沉淀到 systemPatterns.md + writing-plans.mdc §6 引用 |
| 5 | 「跨阶段协同要求」反模式沉淀 | P2 | ✅ 沉淀到 systemPatterns.md |
| 6 | 「带否定判据的发现型 phase」方法论沉淀 | P2 | ✅ 沉淀到 systemPatterns.md |
| 7 | Layout super-linear knee 调查（K2 + K3） | P2 | ⏸ 推 TASK-20260419-09 候选（已立项） |
| 8 | plan 起草前 grep CMake helper 签名 | P2 | ✅ 写入 writing-plans.mdc §2 |

**8/8 已闭环**（5 项已落实 + 1 项已立候选 + 2 项已写入规则段）。

## 提交记录（10 commits on `feature/TASK-20260419-05-layout-render-benchmarks`）

| # | SHA | 主题 | 阶段 |
|---|-----|------|------|
| 1 | `3eb9070` | docs(plan): TASK-20260419-05 layout/render benchmarks design + plan | /plan |
| 2 | `361cf22` | wip(TASK-20260419-05): phase-1 register 4 smoke benches (layout/render) | /build P1 |
| 3 | `54a63f5` | wip(TASK-20260419-05): phase-2 add layout_corpus.h (DOM builder + cache) | /build P2 |
| 4 | `931fe6c` | feat(bench): add bench_layout_buildtree full suite (TASK-20260419-05 phase-3) | /build P3 |
| 5 | `6be2439` | feat(bench): add bench_layout_flex 2D matrix suite (TASK-20260419-05 phase-4) | /build P4 |
| 6 | `d4a1cb3` | feat(bench): add bench_render_record full suite (TASK-20260419-05 phase-5) | /build P5 |
| 7 | `704daf3` | feat(bench): add bench_render_replay full suite + hot-path finding | /build P6 |
| 8 | `6484025` | docs(bench): add 4 layout/render baselines + README updates | /build P7 |
| 9 | `81d85cc` | chore(build): finalize TASK-20260419-05 memory bank state | /build §7.5 |
| 10 | `01833c6` | docs(reflect): add reflection for TASK-20260419-05 | /reflect |
| 11 | (本次) | docs(archive): add archive for TASK-20260419-05 | /archive |

## 参考文档

- 设计规格：`docs/specs/2026-04-19-layout-render-benchmarks-design.md`
- 实现计划：`docs/plans/2026-04-19-layout-render-benchmarks.md`
- 回顾文档：`memory-bank/reflection/reflection-TASK-20260419-05.md`
- 上一轮 bench 任务（同模式参考）：`memory-bank/archive/archive-TASK-20260419-03.md`
- 长期知识库更新：
  - `memory-bank/techContext.md`「Layout 性能基线」+「Render 性能基线」段
  - `memory-bank/systemPatterns.md` 末尾 4 段（方法论 + 清单 + 反模式）
  - `.cursor/rules/skills/writing-plans.mdc`「性能基准任务必检项」段
- 后续任务候选：TASK-20260419-09（DrawText 微基准 + 真 ImageCache 通路 + super-linear 根因）
