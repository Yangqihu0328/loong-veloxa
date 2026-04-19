# 进度记录

## 当前任务

### TASK-20260419-09：Replay hot path 深度基准 + 真 ImageCache 通路（A+B 子集）

**当前阶段：** 构建完成（待 `/reflect`）

**里程碑：**
- ✅ VAN（2026-04-19）：分支创建（基于 main `bfe44ae`），范围拆分（A+B 本任务 / C 拆 TASK-10 候选），4 处 grep 推翻 K1/K5 三个原假设
- ✅ Plan（2026-04-19）：design + plan 双文档落地，5 决策（D1-D5）确认，3 处补 grep 验证 API 签名（`Element::SetAttribute` / `InternedString::Intern` / `Brush::Solid`），无需 `/creative`
- ✅ Build（2026-04-19，5 phase / 15 BMs / 5 commits + finalize；估时 3.5h，实际 ~50 min）：
  - phase-1 `f767231` corpus 扩 + 2 smoke + CMake 注册（PNG::PNG find_package 1 处工程修正）
  - phase-2 `c19dc97` bench_imagecache 全套 7 BMs（K6 新发现）
  - phase-3 `8e55337` bench_drawtext 全套 8 BMs（K1 修正归因 + K7 新发现）
  - phase-4 `913bf01` 2 baseline JSON + 2 README
  - phase-5 (本 commit) techContext + systemPatterns + MB 收尾
- ⏳ Reflect / Archive

**核心数据成果（K1/K6/K7 三大判定）：**
- **K1 修正归因**：TASK-05 8200 ns/cmd 实为 fallback FillRect ×19/cmd；"820×"是 per-cmd 工作不可比，非真路径慢源
- **K1 真根因**：Real_Cold_Medium 52763 ns / 19 char = FT_Load+FT_Render（vs warm 9.1×、vs fallback 14×）
- **K6 新发现（最高 ROI 候选）**：ImageCache::Load hit 路径 O(N) 字符串扫，size=256 时 1162 ns > ReplayImageReal<16> 595 ns
- **K7 新发现**：当前 warm 真路径 5807 ns > fallback 3647 ns（1.6×），未来若默认开真路径需先优化 hb_buffer 复用 + 直接 raster

**关键观察：**
- 「方案根因假设未先验证」P0 规则（TASK-05 /archive 落实）**全流程产生量化预防价值**：
  - VAN 阶段 grep 4 处推翻 K1/K5 三个假设（节省 ~30 min cmake fixture 工程 + 整轮 fallback 误报）
  - Plan 阶段补 grep 3 处验证 API 签名（避免 ~10 min build 阶段 link error 返工）
  - **结果**：Build 阶段实际 ~50 min（plan 估 3.5h），其中工程意外仅 1 处（PNG::PNG `find_package` 需在 benchmarks/CMakeLists.txt 顶部 — 因 vx_core PRIVATE 链接不传递）
- 「带否定判据的发现型 Phase」方法论 4 次应用全部"否定原假设 + 意外定位真问题"（TASK-03 cluster / TASK-05 DrawText / TASK-09 K1' real cold / TASK-09 K6 Load O(N)）— 已写入 systemPatterns 表

## 已完成任务

- TASK-20260419-05：Layout + Render 性能基准（4 bench exe / 30 BMs / 4 baseline JSON 入仓 + K1 实测 DrawText 是 Replay hot path 820× FillRect / ImageCache 不是 → 推 TASK-09 候选；落实 P0 `writing-plans.mdc`「性能基准任务必检项」段）→ 归档 `memory-bank/archive/archive-TASK-20260419-05.md`
- TASK-20260419-07：修复 main Release `-Werror` 编译失败（fgets unused-result + BasicString copy ctor IPA array-bounds 误报；与 TASK-04 同元模式不同手法 noinline）→ 归档 `memory-bank/archive/archive-TASK-20260419-07.md`
- TASK-20260419-03：CSS 解析性能基准（Tokenizer 10 BM / Parser 11 BM / PropertyLookup 9 BM = 30 BMs + 3 baseline JSON 入仓 + Cluster 度量证 PropertyMap 均匀 → TASK-06 P1→P3 降级） → 归档 `memory-bank/archive/archive-TASK-20260419-03.md`
- TASK-20260419-04：修复 `enum_serialization.cc` Release `-Warray-bounds` 误报（去模板化 `Lookup<N>`，解锁 TASK-03 Phase 1） → 归档 `memory-bank/archive/archive-TASK-20260419-04.md`
- TASK-20260419-02：Google Benchmark 集成 + Foundation 性能基线（4 exe / 40 BM） → 归档 `memory-bank/archive/archive-TASK-20260419-02.md`
- TASK-20260419-01：流程规则沉淀 + P2 功能技术债收口 → 归档 `memory-bank/archive/archive-TASK-20260419-01.md`
- TASK-20260418-01：消化关键技术债务（#45/#46/#48/#50） → 归档 `memory-bank/archive/archive-TASK-20260418-01.md`
- TASK-20260414-01：功能补全 → 归档 `memory-bank/archive/archive-TASK-20260414-01.md`
- TASK-20260413-02：消化技术债务子集 → 归档 `memory-bank/archive/archive-TASK-20260413-02.md`
- TASK-20260413-01：QuickJS 脚本引擎集成 → 归档 `memory-bank/archive/archive-TASK-20260413-01.md`
- TASK-20260405-01：Foundation 基础库 → 归档 `memory-bank/archive/archive-TASK-20260405-01.md`
- TASK-20260405-02：Graphics HAL + Platform HAL → 归档 `memory-bank/archive/archive-TASK-20260405-02.md`
- TASK-20260405-03：DOM 树 + HTML 解析器 → 归档 `memory-bank/archive/archive-TASK-20260405-03.md`
- TASK-20260405-04：CSS 引擎 → 归档 `memory-bank/archive/archive-TASK-20260405-04.md`
- TASK-20260405-05：消化技术债务 → 归档 `memory-bank/archive/archive-TASK-20260405-05.md`
- TASK-20260405-06：Layout Engine 布局引擎 → 归档 `memory-bank/archive/archive-TASK-20260405-06.md`
- TASK-20260405-07：渲染管线（Render Pipeline） → 归档 `memory-bank/archive/archive-TASK-20260405-07.md`
- TASK-20260405-08：事件系统（Event System） → 归档 `memory-bank/archive/archive-TASK-20260405-08.md`
- TASK-20260405-09：脏区更新与重绘机制 → 归档 `memory-bank/archive/archive-TASK-20260405-09.md`
- TASK-20260405-10：事件循环与应用壳 → 归档 `memory-bank/archive/archive-TASK-20260405-10.md`
- TASK-20260405-11：C API 层 → 归档 `memory-bank/archive/archive-TASK-20260405-11.md`
- TASK-20260405-12：示例应用 → 归档 `memory-bank/archive/archive-TASK-20260405-12.md`
- TASK-20260405-13：CSS 动画系统（CSS Transitions） → 归档 `memory-bank/archive/archive-TASK-20260405-13.md`
