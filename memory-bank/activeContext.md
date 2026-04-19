# 活跃上下文

## 当前阶段
构建中（TASK-20260419-03 第 2 轮 = Phase 3+4+5+6 全部完成 ✅；任务整体完成，待 `/reflect` 整体回顾 + `/archive`）

## 当前任务

**TASK-20260419-03** — CSS 解析性能基准（Tokenizer / Parser / Property Lookup）

- **复杂度：** Level 2
- **分支：** `feature/TASK-20260419-03-css-benchmarks`（基于 main `a09ad1e` rebase，feature ahead 10 commits 共 7 个 phase + 收尾）
- **TDD 模式：** 覆盖补充（既有 GTest 作正确性基线，本任务不新增 GTest）
- **整体进度：** Phase 0/1/2/3/4/5/6 全 ✅，30 BMs（10 tokenizer + 11 parser + 9 property lookup）入仓 + 3 baseline JSON 入仓
- **第 2 轮（本轮）范围与产出：**
  - Phase 3 — `bench_css_parser.cc` 11 BMs 完整套件（4 stylesheet template + 6 inline Range + 1 selector）
  - Phase 4 — `bench_css_property_lookup.cc` 9 BMs 完整套件（含 5 cluster probes）+ **关键产出**：cluster 判定 = **未触发**（最慢 single key 仅 2.75× HitHot5，远低于 5× 阈值），TASK-06 候选优先级 P1→P3
  - Phase 5 — `benchmarks/README.md` 扩展（CSS 章节 + 5 行已知量级 + 3 行注意事项）+ 新建 `benchmarks/baseline/README.md`（失真警告 + 5 条更新协议 + 命令模板）
  - Phase 6 — Release 全量 fresh rebuild build-bench/ + 3 baseline JSON（共 ~15 KB，全 release）入仓 + README/baseline TBD 数字回填
- **本轮验证证据：**
  - 7 bench exe Release 编译干净（含 fresh rm + reconfigure 路径）
  - 3 baseline JSON 体检：context.library_build_type=release ✅，BM count = 10 + 11 + 9 = 30 ✅
  - Debug 全测试 890/890 ✅（regression 通过）
  - 7 bench exe（4 foundation + 3 css）全 exit 0
  - lint 0
  - 关键数字（baseline JSON 实测）：BM_TokenizeAll/4096 ~10.8 µs (~265 MiB/s)、BM_ParseStylesheetMedium ~18.5 µs、BM_PropertyLookupHitHot5 ~13 ns、HitSingle/transition-timing-function ~33 ns
- **本轮提交（feature ahead 6→10）：**
  - `1e896d3` feat(benchmarks): CSS parser suite [P3]（含本轮启动 MB 阶段切换）
  - `66f0a10` feat(benchmarks): CSS property lookup + cluster probe [P4]
  - `36c7e9c` docs(benchmarks): document CSS suite + baseline retention policy [P5]
  - 待 commit：3 baseline JSON + README 数字回填 + MB 收尾（本步骤的 finalize commit）
- **关键发现 — Release 全量 build 副发现（main 已存在）：**
  - `tests/platform/memory_surface_test.cc:102/105/108` — `fgets` `-Werror=unused-result`
  - `tests/foundation/strings/string_test.cc` — `-Werror=array-bounds` GCC IPA 误报（与 TASK-04 同类，作用于 `BasicString` 而非 `Lookup<N>`）
  - 这些**不阻塞本任务**（bench 目标不依赖 tests）；建议立 TASK-07 修复（与 TASK-04 系列同源 — Release 通路验证缺失）
- **下一步：** `/reflect` 整体回顾（覆盖第 1 + 第 2 轮，对照原 plan 7 phase）→ `/archive`

## 最近归档

- `memory-bank/archive/archive-TASK-20260419-04.md`（TASK-20260419-04 修复 `enum_serialization.cc` Release `-Warray-bounds` 误报，已合并到 main `a09ad1e`）
- `memory-bank/archive/archive-TASK-20260419-02.md`（TASK-20260419-02 Google Benchmark 集成 + Foundation 性能基线，已合并到 main）
- `memory-bank/archive/archive-TASK-20260419-01.md`（TASK-20260419-01 流程规则沉淀 + P2 功能技术债收口，已合并到 main）

## 待处理事项

### 后续任务候选

- **TASK-20260419-05（建议）：** Layout + Render 基准 — `bench_layout_buildtree` / `bench_layout_flex` / `bench_render_record` / `bench_render_replay`（来源 TASK-20260419-02 归档）
- **TASK-20260419-06（建议，**优先级降级 P1→P3**）：** HashMap Hash Mixing 优化（cluster 问题）— `BM_HashMapLookupHitInt/16384=9µs` vs n=64 时 69ns，根因 `H1=h>>7` + `std::hash<int>` 恒等映射使所有起始探测位置压在 [0,127]。**降级根据**（TASK-03 P4 实测）：PropertyMap 60-entry HashMap<StringView, PropertyId> + djb2 hash 在最差 single key 下仅 2.75× HitHot5（远低于 5× cluster 阈值），证明 cluster 问题主要见于 int key + 大规模场景，**短字符串 + 小规模场景免疫**；TASK-06 价值仍在但可推迟到大规模 int-key 容器场景出现时
- **TASK-20260419-07（新建议）：** 修复 main 已存在的两个 Release `-Werror` 编译失败 — (1) `tests/platform/memory_surface_test.cc` `fgets -Wunused-result`（直接处理返回值）; (2) `tests/foundation/strings/string_test.cc` `-Werror=array-bounds`（GCC IPA 误报，参考 TASK-04 detemplatize / pragma 套路）。**触发：** TASK-03 P6 Release 全量 build。Level 1。**与 TASK-04 同源** — Release 通路验证缺失反复模式

### 长期项（按优先级）

- **P1（新模式，首次出现）：** WIP / 中间 commit 的 subject 严禁包含外部任务状态字样（`BLOCKED on TASK-X` / `WAITING for Y` / `PENDING dep`），改用中性 `(unverified)` / `(compile-pending)` 后缀；外部依赖关系写到 commit body。**根因：** WIP commit 在 rebase / 续接 / 后续阅读时，subject 的 git log oneline 长期可见而 body 易被忽略，"过期外部状态"会持续误导。**首发：** TASK-20260419-03 Round 1 — `wip(TASK-03): Phase 1 CSS bench scaffolding (BLOCKED on TASK-04)` commit 在 TASK-04 解锁后 subject 字面失效但难以清理。**落实：** 下次 wip commit 前对照；如再次出现立即升级 P0 + 写入 `git-workflow.mdc` 「Commit Subject 规范」段
- **P1（新模式）：** Level 2+ 多 phase 任务（phase 数 ≥ 5）需要支持「轮次完成」中间态 — `/reflect` 不强制切死端「回顾中」，允许多次 `/build` 续接同一 task 的不同 phase 区间。**首发：** TASK-20260419-03 Round 1 — 当前阶段流转 `构建中 → 回顾中 → 归档中` 假设「一轮即完成」，本任务 Phase 3-6 还要做但已切到「回顾中」，下轮 `/build` 又要切回，违反隐含 invariant。**落实：** 修改 `complexity-levels.mdc` 增加「多轮次 Build 工作流」段；或调整 `/reflect`、`/build`、`/archive` 命令的阶段守卫
- **P1（新升级，反复出现）：** 任何引入 GCC/Clang 模板特化技巧（`template<usize N>` 取数组引用、CRTP、SFINAE 分派）的 PR 必须在 PR 检查表中加一行「Release `-O2 -Werror` 通路验证」。来源：TASK-20260419-04 反思 — TASK-01 引入 `Lookup<N>` 时仅 Debug 验证，未验证 Release `-Werror=array-bounds`，导致 TASK-03 Phase 1 才暴露；与 TASK-02 反思 #1 P1「性能基准任务必须 Release」同源（同属"前置依赖/环境/API 能力未验证"反复模式，已 8+ 次出现），但泛化范围更广 — 不限基准任务。下次涉及模板/泛型代码引入的 PR 前固化到 `writing-plans.mdc` 「Release 通路验证」段
- **P1**：跨子库新增符号引用前 grep link graph，确认是否触发循环依赖（来源 TASK-20260419-01 反思 #1，规则已固化到 `writing-plans.mdc` 「静态库循环依赖审计」段；下次涉及静态库间符号引用时强制执行）
- **P1**：性能基准任务必须在 Plan 阶段就显式 `-DCMAKE_BUILD_TYPE=Release` + 独立 `build-bench/` 目录（来源 TASK-20260419-02 反思 #1；下次 CSS / Layout / Render bench 立项前固化到 `writing-plans.mdc` 「性能基准任务必检项」段）
- **P1**：CMake 操作第三方 target 前必须先用 `get_target_property(... ALIASED_TARGET)` 识别 ALIAS，避免 `set_target_properties` 报错（来源 TASK-20260419-02 反思 #2；下次 FetchContent 涉及第三方 target 改属性时固化到 `writing-plans.mdc` 「CMake 链接方向约束分析」段后追加「ALIAS 识别步骤」）
- **P1**：Cursor 沙箱内涉及 `FetchContent` 时，VAN 阶段必须验证「子进程能否真正用上自定义代理」而非仅父 shell 测试；推荐 `git config --global http.proxy` 并明确恢复时机（来源 TASK-20260419-02 反思 #3；属于"前置环境验证"反复模式的特化；下次任何 FetchContent 任务前固化到 `main.mdc` 或 `writing-plans.mdc` 「Cursor 沙箱代理验证」段）
- **P2**：将 `renderer_test` / `render_integration_test` 等剩余手写像素位移断言迁到 `tests/test_pixel_utils.h`，并在该头注释示例 hex→RGBA（来源 TASK-20260413-02）
- **P2**：google/benchmark `RangeMultiplier(m)->Range(lo,hi)` 的精确数量为 `ceil(log_m(hi/lo))+1`，写入 `writing-plans.mdc` 「Benchmark 用例估算」附录（来源 TASK-20260419-02 反思 #5）
