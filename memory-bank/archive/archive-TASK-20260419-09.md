# 归档：Replay hot path 深度基准 + 真 ImageCache 通路（A+B 子集）

**日期：** 2026-04-19
**任务 ID：** TASK-20260419-09
**复杂度级别：** Level 2-3
**状态：** ✅ 已完成
**分支：** `feature/TASK-20260419-09-replay-deepbench`（6 commits + 1 reflect = 7 commits）

## 任务概述

继 TASK-20260419-05（Layout + Render 4 bench / 30 BMs）之后，针对 Replay 阶段已识别的 hot path 进行**深度基准 + 真路径通路覆盖**，并对 K1（DrawText）/ K5（ImageCache）两个候选命题给出明确量化判定。

**目标**（A+B 子集，C 拆为 TASK-10）：
- **A（DrawText 真路径）**：把 TASK-05 K1「DrawText 8200 ns/cmd 是 FreeType+HarfBuzz hot path」假设拿到 fallback / Real_Cold / Real_Warm 三种维度上做严格对照，给出真根因
- **B（ImageCache 真路径）**：解决 TASK-05 K5「ImageCache 真实例无法在 bench 内构造」工程问题，覆盖 Load hit/miss × {1, 16, 256} cache size + 端到端 Replay × 2 size + Get O(1)

**最终交付**：2 bench exe + `layout_corpus.h` 1 builder 扩展 + CMakeLists 注册 + 2 baseline JSON + 2 README 更新 + Memory Bank 4 项 K-finding（K1' 修正 / K1'' 真根因 / K6 新 / K7 新）+ 2 个 follow-up 候选任务（TASK-11/12）。

## 技术方案

### 整体架构

| 子目标 | bench 文件 | BM 数 | 关键覆盖 |
|--------|-----------|------|---------|
| A1 — `SoftwareCanvas::DrawText` 直接微基准 | `bench_drawtext.cc` | 6（Fallback × 2 + Real Cold × 1 + Real Warm × 3） | fallback vs FT+HB；cold vs warm；short/medium/long 摊还 |
| A2 — 端到端 Replay TextHeavy 真 vs fallback | `bench_drawtext.cc` | 2 | 验证 K1 在端到端层级的归因 |
| B1 — `ImageCache::Load` hit/miss × cache size | `bench_imagecache.cc` | 4（Miss × 1 + Hit<1/16/256> × 3） | O(N) 字符串扫描确认 |
| B2 — 端到端 Replay 真路径 | `bench_imagecache.cc` | 2（N=16, 64） | 三阶段管道（layout/Record/Replay）真测 |
| B3 — `ImageCache::Get` 单次 | `bench_imagecache.cc` | 1 | O(1) 数组下标确认 |

### 关键设计点

1. **fixture 极简策略**（VAN F1 决定）— ImageCache 用 libpng 程序化构造 1×1 RGBA PNG 写 `/tmp/vx_bench_<pid>_<i>.png`（仿 `image_decoder_test::CreateTestPng()`），析构 `std::remove` 清理；DrawText 用系统 `/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf` + fail-fast `std::exit(1)`（避免 K1 类静默 fallback 误报）。**0 个 `configure_file()`**，0 个 cmake 资源管理工程。
2. **真路径 vs fallback 严格分桶**（A1 D1 决定）— `CanvasFallback()` 不传 FontManager + GlyphCache → 走 `DrawTextFallback`；`CanvasReal()` 显式传 `&SharedFM() + &SharedGC()` → 走 FreeType+HarfBuzz 真路径。bench 名前缀 `Fallback_` / `Real_Cold_` / `Real_Warm_` 直接体现归因维度。
3. **cold/warm cache 控制**（A1 D2 决定）— `state.PauseTiming() → SharedGC().Clear() → state.ResumeTiming()` 强制每次 cold 测量；warm 由 `WarmUp()` 预热 3 次 + 持续命中。
4. **Multi-PNG distinct path fixture**（B D4 决定）— 256 张 distinct PNG 路径（含 `getpid()` 避免并行测试串扰），保证 cache hit/miss 行为完全可控（不依赖 path 字符串内容差异）。
5. **复用 layout_corpus.h 缓存模式**（B2）— 新增 `BuildImageWithSrcStyled` + `CachedImageWithSrcStyledDocument`，与现有 7 builder 同款 mutex-protected static map 缓存。
6. **「带否定判据的发现型 Phase」方法论 4/4 应用** — phase-2 设 hypothesis（Decode 是 ImageCache 瓶颈）+ phase-3 设 hypothesis（DrawText 8200 ns 是真路径），全部"否定原假设 + 意外定位真问题"。

## 实现摘要

### 文件变更

| 操作 | 文件路径 | 说明 |
|------|---------|------|
| 创建 | `benchmarks/bench_drawtext.cc` | 8 BMs（A1 6 + A2 2），含 SharedFM/SharedGC/CanvasReal/CanvasFallback 4 工具 + WarmUp |
| 创建 | `benchmarks/bench_imagecache.cc` | 7 BMs（B1 4 + B2 2 + B3 1），含 PngFixturePool + WriteOnePng + libpng 构造 |
| 创建 | `benchmarks/baseline/bench_drawtext.json` | Release 基线（library_build_type=release ✅） |
| 创建 | `benchmarks/baseline/bench_imagecache.json` | Release 基线（library_build_type=release ✅） |
| 修改 | `benchmarks/layout_corpus.h` | +`BuildImageWithSrcStyled` + `CachedImageWithSrcStyledDocument` + 3 新 include（assert / interned_string / `<string>` `<vector>`） |
| 修改 | `benchmarks/CMakeLists.txt` | +`find_package(PNG REQUIRED)`（vx_core PRIVATE PNG 不传递）+ 2 行 `vx_add_benchmark` 注册 |
| 修改 | `benchmarks/README.md` | exe 表 11→13；run-all 11→13；baseline 列表 7→9；K1' / K1'' / K6 / K7 量级行追加；image_cache + DrawText fallback gate 注意事项更新 |
| 修改 | `benchmarks/baseline/README.md` | 7→9 baseline 列表；key findings 拆 TASK-05（历史）+ TASK-09（本次）双段；环境表 + 命令模板 + 历史增 phase-4 行 |
| 修改 | `memory-bank/techContext.md` | +「Replay-Deepbench 性能基线」段（DrawText 8 BMs + ImageCache 7 BMs 数值表 + K6/K7 描述）；Render 性能基线段补 K1 修正归因 quote |
| 修改 | `memory-bank/systemPatterns.md` | Render Bench 前置清单 DrawText 行加 fallback gate 注解；隐式契约 2→3（新加 SoftwareCanvas ctor FontManager+GlyphCache 必传）；「带否定判据」表 2→4 行；新增「bench 估时校准」段 |
| 修改 | `.cursor/rules/skills/writing-plans.mdc` | §3 grep 增补「CMake 链接可见性 PUBLIC/PRIVATE/INTERFACE 必查」（落实 reflection P0 #1） |
| 创建 | `memory-bank/reflection/reflection-TASK-20260419-09.md` | 5 项改进建议（P0×1 / P1×2 / P2×2）+ 反复模式表 |
| 创建 | `docs/specs/2026-04-19-replay-deepbench-imagecache-design.md` | 设计文档（plan 阶段产出，含 D1-D5 五决策 + 10 验收 + 8 风险） |
| 创建 | `docs/plans/2026-04-19-replay-deepbench-imagecache.md` | 5-phase 实现计划（含完整代码片段 + smoke 三件套验收） |

**统计**：15 文件变更 / 净 +2087 / -25 行 / 7 commits（含 plan + 4 phase + finalize + reflect）。

### 关键决策

1. **范围拆分（VAN）**：原 TASK-09 候选含 3 子目标（A=DrawText / B=ImageCache / C=Layout super-linear），VAN 阶段范围检查后**拆为 A+B 本任务（bench 类）+ TASK-20260419-10 后续候选（C 研究类）**。理由：A+B 同型同工具链，C 是研究类需 perf/profiler 工具，混合会拖慢交付且互相干扰。
2. **「方案根因假设未先验证」P0 规则全流程应用**：
   - VAN 4 处 grep 推翻 K5 fixture 假设 + K1 真路径假设 + K2/K3 ArenaAllocator 假设
   - Plan 3 处 grep 验证 `Element::SetAttribute` / `InternedString::Intern` / `Brush::Solid` API 签名
   - **结果**：build 阶段仅 1 处工程意外（PNG::PNG PRIVATE 不传递），~1 min 修正
3. **D1-D5 五决策（plan 头脑风暴用户确认，全程不变）**：A1+A2 / cold+warm 维度 / B1×3 cache size + B2×2 + B3 / 多张 distinct PNG fixture / 5 phase
4. **fixture 用程序化构造 + fail-fast**：避免 TASK-05 K5 类「fixture 工程拖慢 plan」+ K1 类「静默 fallback 误报」两类问题同时发生
5. **K6 副产品发现优先于 K5 命题判定**：原计划用 B2 vs A2 vs FillRect 基线对比满足 K5 验收 #10；实际 phase-2 自然产出 K6 新发现（Load O(N) 字符串扫，size=256 时 1162 ns > ReplayImageReal<16> 595 ns），价值远超 K5 命题本身 → 验收 #10 改以 K6 量化 + B2 线性数据满足

### 安全决策

**本任务不涉及安全变更。**

性能基准任务全程：
- 无外部输入（fixture 全程序化生成；font 是系统已存在文件；纯 1×1 RGBA 红像素）
- 无认证路径
- 无敏感数据
- 无新依赖（PNG::PNG / Freetype 已是 vx_core 既有依赖；google/benchmark v1.9.1 已在 TASK-02 接入并审计；DejaVuSans 是 Ubuntu 系统字体）
- 错误信息无外部传播（仅 BM stderr 用于本地调试 + fail-fast `std::exit(1)`）

## 测试覆盖

- **回归测试**：Debug `ctest -j --output-on-failure` 全程 **890/890 通过**，零回归
- **bench 自验证**：13 bench targets（4 Foundation + 3 CSS + 4 Layout/Render + 2 Replay-deepbench）Release `--benchmark_min_time=0.01s` smoke 全部 exit 0
- **smoke 三件套全过**：每 BM 数字非零 ✅ + `SetItemsProcessed > 0` ✅ + JSON `items_per_second > 0` ✅（phase-1/2/3 各独立验证）
- **数据有效性**：2 baseline JSON 头部 `library_build_type=release` ✅；15 BMs items_per_second 全非零 ✅

**TDD 模式：** 已豁免（bench 类任务，与 TASK-05 同例；plan §0 显式声明）。替代验收 = smoke 三件套 + ctest 不引入回归 + lint 全清。

## 关键 K-finding（4 项）

| # | 命题 | 数值证据 | 后续动作 |
|---|------|---------|---------|
| **K1' 修正归因** | TASK-05 「DrawText 8200 ns/cmd 是 FT+HB 真路径」实为 fallback FillRect ×19/cmd | `BM_DrawTextFallback_Medium` 3647 ns / 19 char = 192 ns/char ≈ FillRect ×19；"820×"是 per-cmd 工作不可比性，非真路径慢源 | 已写入 `techContext.md` Render 性能基线段 quote + `systemPatterns.md` DrawText fallback gate 隐式契约 |
| **K1'' 真根因** | DrawText 真路径**冷路径**才是 hot；FT_Load + FT_Render 主导 | `BM_DrawTextReal_Cold_Medium` 52763 ns / 19 char = 2777 ns/char（**14× of fallback / 9.1× of warm**） | glyph_cache 是 ROI 极高存量优化（已存在）；冷启动场景可考虑 pre-warm |
| **K6 新（最高 ROI 候选）** | `ImageCache::Load` hit 路径 O(N) 字符串扫描；cache size = 256 时单次 1162 ns | hit/1 9.99 ns → hit/16 48.5 → hit/256 **1162 ns**（116× of hit/1）；**比 ReplayImageReal<16> 595 ns 还慢** | 立 **TASK-20260419-11**（候选，建议 P1）：HashMap<String, ImageHandle> 改造，工作量 ~1-2h |
| **K7 新** | warm 真路径 5807 ns > fallback 3647 ns（1.6×） | `hb_shape` 固定开销 + glyph bitmap memcpy > 19 个 FillRect | 立 **TASK-20260419-12**（候选，建议 P2）：`hb_buffer` 复用 + glyph 直接 raster；触发条件=真路径默认化提上日程 |

## 经验教训

提取自 `memory-bank/reflection/reflection-TASK-20260419-09.md` 的 5 项改进建议，全部已闭环：

| # | 教训 | 优先级 | 落实状态 |
|---|------|--------|---------|
| 1 | P0 grep 规则需覆盖 CMake 链接可见性（PUBLIC/PRIVATE/INTERFACE） | **P0 立即** | ✅ 已写入 `writing-plans.mdc` §3 grep 强制段（含 PNG::PNG 反例） |
| 2 | bench 类任务 plan 估时模板（含复用率 + 单 BM 3-5 min 经验值） | **P1 下次** | ✅ 已迁入 `activeContext.md` 待处理事项；连续 2 次 4× 高估（TASK-05 3.4× + TASK-09 4.2×）；如再次 > 3× 则升级到强制条目 |
| 3 | 「带否定判据的发现型 Phase」标记 4/4 成熟实践 | **P2 长期** | ✅ 已沉淀 `systemPatterns.md`（删除"实验性"措辞，更新表格） |
| 4 | 立 TASK-11（K6 高 ROI ImageCache HashMap）+ TASK-12（K7 真路径优化） | **P1 下次** | ✅ 已迁入 `tasks.md` 待立项候选 + `activeContext.md` 后续任务候选 |
| 5 | bench 估时校准段（双实证 4× 高估 + 升级阈值） | P2 长期 | ✅ 已沉淀 `systemPatterns.md` 新建段 |

**核心元教训（覆盖性观察）**：
- 「方案根因假设未先验证」P0 规则（TASK-05 archive 落实）**全流程量化预防价值再次实证**：VAN 4 处 + Plan 3 处共 7 处 grep，build 阶段仅 1 处工程意外（链接可见性盲点，1 min 修正）。这是规则被引入后的第 2 个完整应用，与 TASK-07 一起形成 2/2 高效预防记录
- **plan 估时校准是连续问题**：bench 任务到 TASK-09 已是第 4 个（02/03/05/09），基础设施完全成熟，但 plan 仍按"从零写"心智模型估时 → 系统性 4× 高估。已追踪到 `activeContext.md` P1 + `systemPatterns.md` 校准段，下次必须应用
- **「带否定判据」方法论已成熟可复用**：4/4 全部"否定原假设 + 意外定位真问题"（TASK-03 cluster / TASK-05 DrawText / TASK-09 K1' real cold / TASK-09 K6 Load O(N)），认知跳跃价值无法用 grep + 推理替代

## 反复模式抑制效果（27 任务历史基线）

7 大已知反复模式中，本次仅命中 1 个变体：

| 模式 | 历史频率 | 本次状态 |
|------|---------|---------|
| 计划文件清单与实际变更不一致 | 9+ 次 | ❌ 不重复 |
| 子代理产出需大量返工 | 7+ 次 | N/A（未用子代理） |
| **前置依赖/环境/API 能力未验证** | 8+ 次 | ⚠️ **半重复** — VAN/Plan grep 7 处覆盖了 API + 文件存在性，**漏 CMake 链接可见性** → 已通过 reflection 建议 #1 升级 P0 grep 规则 |
| 非默认路径（流式/错误/缓存）遗漏验证 | 4+ 次 | ❌ 不重复（B1 hit/miss / DrawText cold/warm / fallback vs real 全部覆盖） |
| 测试隔离问题 | 7+ 次 | ❌ 不重复（fixture 用 `getpid()` 隔离 + 析构清理） |
| 提交粒度偏离计划 | 7+ 次 | ❌ 不重复（5 phase 5 commit 完美对齐） |
| TDD 严格度与场景不匹配 | 11+ 次 | ❌ 不重复（plan §0 显式声明 bench 类豁免 + smoke 三件套替代） |

**结论**：成熟的 P0 grep 规则 + plan 模板对反复模式的抑制效果显著（6/7 规避，1/7 升级）。

## 参考文档

- 设计规格：`docs/specs/2026-04-19-replay-deepbench-imagecache-design.md`
- 实现计划：`docs/plans/2026-04-19-replay-deepbench-imagecache.md`
- 创意设计：无（plan 阶段头脑风暴 D1-D5 五决策已覆盖所有设计空白）
- 回顾文档：`memory-bank/reflection/reflection-TASK-20260419-09.md`
- 长期知识库更新：
  - `memory-bank/techContext.md`「Replay-Deepbench 性能基线」段
  - `memory-bank/systemPatterns.md`「带否定判据」(4/4 成熟) + 「Render Bench 前置清单」DrawText fallback gate + 「bench 估时校准」段
  - `.cursor/rules/skills/writing-plans.mdc` §3 grep CMake 链接可见性增补
- 后续任务候选：
  - **TASK-20260419-11**（K6 拆出，建议 P1 高 ROI 优化）：`ImageCache::Load` HashMap 化
  - **TASK-20260419-12**（K7 拆出，建议 P2 触发型）：`SoftwareCanvas::DrawText` 真路径优化
  - **TASK-20260419-10**（TASK-05 K2/K3 + TASK-09 VAN 拆出，建议 P2 触发型）：Layout super-linear knee 根因调查
- 相关历史归档：
  - `memory-bank/archive/archive-TASK-20260419-05.md`（前置任务，本任务的 K1/K5 命题来源）
  - `memory-bank/archive/archive-TASK-20260419-03.md`（同模式 bench 任务，「带否定判据」方法论首次应用）
