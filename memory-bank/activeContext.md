# 活跃上下文

## 当前阶段
回顾中

## 当前任务

**TASK-20260419-05：Layout + Render 性能基准（4 bench exe）**

- 级别：Level 2-3（4 bench .cc + 1 共享 corpus header + CMakeLists + 2 README + 4 baseline JSON）
- 分支：`feature/TASK-20260419-05-layout-render-benchmarks`（基于 main `2985220`）
- **设计文档：** `docs/specs/2026-04-19-layout-render-benchmarks-design.md`
- **实现计划：** `docs/plans/2026-04-19-layout-render-benchmarks.md`
- **构建结果（7 phase 全完成 / 7 commits）：**
  - Phase 1：CMakeLists `vx_add_benchmark` 注册 4 + 4 smoke .cc，11 bench 全 build green
  - Phase 2：`benchmarks/layout_corpus.h` 头文件 inline 7 种 builder（含 Styled 子集）+ mutex-protected static cache
  - Phase 3：`bench_layout_buildtree` 3 BM × 14 rows（Flat/Nested/Mixed）
  - Phase 4：`bench_layout_flex` 6 BMs（5 fixed `BENCHMARK_TEMPLATE<rows,cols>` + 1 NestedFlex）
  - Phase 5：`bench_render_record` 5 BMs（Small/Medium/Large/TextHeavy/ImgVsNoImg）
  - Phase 6：`bench_render_replay` 5 BMs；揭示 hot path = **DrawText**（820× FillRect），非 ImageCache
  - Phase 7：4 baseline JSON 入仓（全 release 体检通过）+ baseline/README key findings + benchmarks/README 升级到 11 exe
- **关键发现（5 项 → /reflect 输入）：**
  - K1 — Replay hot path = `DrawText` ~8200 ns/cmd vs FillRect ~10 ns/cmd（820×，远超 5× 阈值）→ 立 TASK-09 候选
  - K2 — Layout buildtree-flat N=128→256 super-linear knee（7.7→70 µs，10× for 2× N）
  - K3 — Layout flex 8x8→16x16 同源 super-linear（4.9→73 µs，14.9× for 4× cells）
  - K4 — Record 对 image 元素无额外开销（image_handle=0 时 RecordBox 直接跳过）
  - K5 — ImageCache 真测无法在 bench 内构造（DecodeFromFile 需 I/O fixture）→ 推 TASK-09
- **完成验证（证据）：**
  - Release build：11/11 bench 全 build green，全 exit 0
  - Debug ctest：890/890 通过
  - baseline JSON：4 份均 `library_build_type=release`
  - baseline 协议：完全复刻 TASK-03 4-piece 失真兜底
- **回顾文档：** `memory-bank/reflection/reflection-TASK-20260419-05.md`（5 项关键发现 + 8 项改进建议；P0 #1 techContext 段已落实，P0 #2 待 /archive 同步到 writing-plans.mdc）
- **下一步：** `/archive` 归档任务（合并到 main，把 P0 #2「render bench 必须 grep 发射条件」固化到规则）

## 最近归档

- `memory-bank/archive/archive-TASK-20260419-07.md`（TASK-20260419-07 修复 main Release `-Werror` 编译失败 2 处 — fgets unused-result + BasicString copy ctor IPA array-bounds 误报；与 TASK-04 同元模式不同手法 noinline；已 `--no-ff` 合并到 main `206d227`）
- `memory-bank/archive/archive-TASK-20260419-03.md`（TASK-20260419-03 CSS 解析性能基准 — 30 BMs + 3 baseline JSON 入仓 + Cluster 度量证 PropertyMap 均匀 → TASK-06 P1→P3，已 `--no-ff` 合并到 main `660313a`）
- `memory-bank/archive/archive-TASK-20260419-04.md`（TASK-20260419-04 修复 `enum_serialization.cc` Release `-Warray-bounds` 误报，已合并到 main `a09ad1e`）
- `memory-bank/archive/archive-TASK-20260419-02.md`（TASK-20260419-02 Google Benchmark 集成 + Foundation 性能基线，已合并到 main）
- `memory-bank/archive/archive-TASK-20260419-01.md`（TASK-20260419-01 流程规则沉淀 + P2 功能技术债收口，已合并到 main）

## 待处理事项

### 后续任务候选

- ~~TASK-20260419-05：已立项为当前任务，详见上方「当前任务」段~~
- **TASK-20260419-08（候选，P3 触发型）：** `string.h` 剩余 3 处 runtime-size memcpy（line 45 SSO ctor / 150 Append / 230 GrowAndCopy）防御性 noinline 化。**触发条件**：未来 GCC 升级回归同类 `-Warray-bounds` 误报；目前不主动改避免引入不必要内联开销（来源 TASK-20260419-07 副发现）
- **TASK-20260419-06（建议，**优先级降级 P1→P3**）：** HashMap Hash Mixing 优化（cluster 问题）— `BM_HashMapLookupHitInt/16384=9µs` vs n=64 时 69ns，根因 `H1=h>>7` + `std::hash<int>` 恒等映射使所有起始探测位置压在 [0,127]。**降级根据**（TASK-03 P4 实测）：PropertyMap 60-entry HashMap<StringView, PropertyId> + djb2 hash 在最差 single key 下仅 2.75× HitHot5（远低于 5× cluster 阈值），证明 cluster 问题主要见于 int key + 大规模场景，**短字符串 + 小规模场景免疫**；TASK-06 价值仍在但可推迟到大规模 int-key 容器场景出现时
- ~~TASK-20260419-07：已归档（`archive-TASK-20260419-07.md`），合并到 main `206d227`~~

### 长期项（按优先级）

- **🔴 P0（紧急升级，TASK-07 + TASK-05 第 2 次实证）：** `writing-plans.mdc` 增加段「目标 API 的发射/触发条件 grep」— 性能基准任务的 plan 阶段必须 grep 目标 hot path 的实际触发条件（如 `RecordBox` 的 `if (bg.a > 0)` gating），否则 smoke 数字无意义。**升级路径：** TASK-07 reflection §改进建议 #1 「候选方案附根因验证步骤」原 P1 → 本次 TASK-05 Phase 6 styled-corpus 假阳性即同型问题（plan 假设 div→Record 有命令但没 grep emit gate），第 2 次出现 → **升 P0**。**反复模式：** 「前置依赖/环境/API 能力未验证」（8+ 次 → 9+ 次）+ 「方案根因假设未先验证」（1 次 → 2 次）双重命中。**落实命令：** `/plan` 命令文档 + `writing-plans.mdc`「性能基准任务必检项」段
- **🟠 P1（已升级 P0 见上，原 TASK-07 关键教训）：** ~~`writing-plans.mdc` 增加段「候选方案表必须附根因验证步骤」~~ → 已升级 P0 + 扩展为「目标 API 的发射/触发条件 grep」段
- **🟠 P2（新增，反复模式表更新）：** `/reflect` 命令 §3.5 反复模式表加新行「方案根因假设未先验证」频率 1 次（来源 TASK-07）。**落实：** `.cursor/rules/skills/...` 或 `/reflect` 命令文档
- **🔴 P0（紧急升级，反复 9+ 次，TASK-07 已验有效预防）：** Cursor 沙箱内任何 FetchContent 任务的 VAN 阶段**必须**强制重设 git 全局代理（`git config --global http.proxy http://...`），并在 `/archive` 阶段决定是否 unset；任务 hand-off（如 TASK-04 archive → TASK-03 resume → TASK-03 fresh rebuild）必须在新任务 VAN 检查表加一行「proxy 状态确认」。**根因：** TASK-02/04 反思都识别了此问题但只升 P1，每次都是「单次 5-10 分钟可解决 + 可绕行」导致**累计成本 ≥ 1 小时却始终未升 P0**；本次 TASK-03 Round 2 Phase 6 fresh build 第 9+ 次出现，破例升 P0。**下次落实动作：** (a) 写入 `main.mdc` 或 `writing-plans.mdc`「FetchContent 任务前置 checklist」强制条目；(b) `/van` 命令文档加阶段守卫「检测到 plan 含 FetchContent → 自动提醒 proxy 状态」。**已知代理地址：** `http://192.168.101.217:7890`（开发环境特定，写入规则时用占位符）
- ~~🟠 P1（已归档为 TASK-20260419-07，合并到 main `206d227`）~~
- **🟠 P3（实测降级，原 TASK-20260419-06，原 P1）：** HashMap Hash Mixing 优化 — `BM_HashMapLookupHitInt/16384=9µs` vs n=64 时 69ns，根因 `H1=h>>7` + `std::hash<int>` 恒等映射使所有起始探测位置压在 [0,127]（来源 TASK-20260419-02 BUILD 副发现）。**降级理由（TASK-20260419-03 P4 实测）：** PropertyMap 60-entry HashMap<StringView, PropertyId> + djb2 hash 在最差 single key 下仅 2.75× HitHot5（远低于 5× cluster 阈值），证 cluster 问题主要见于 **int key + 大规模**场景，**短字串 + 小规模**场景免疫。**触发条件**变为：当出现「短字符串 ≠ 主用例 + 容器规模 > 1000 entry」的新场景时再立项；不主动追求优化 PropertyMap 类场景
- **P1（已确认，本任务整体回顾再次复确）：** WIP / 中间 commit 的 subject 严禁包含外部任务状态字样（`BLOCKED on TASK-X` / `WAITING for Y` / `PENDING dep`），改用中性 `(unverified)` / `(compile-pending)` 后缀；外部依赖关系写到 commit body。**根因：** WIP commit 在 rebase / 续接 / 后续阅读时，subject 的 git log oneline 长期可见而 body 易被忽略，"过期外部状态"会持续误导。**首发：** TASK-20260419-03 Round 1 — `wip(TASK-03): Phase 1 CSS bench scaffolding (BLOCKED on TASK-04)` commit 在 TASK-04 解锁后 subject 字面失效但难以清理；**Round 2 完成后 git log 阅读体验仍受损**，确认 P1 不升级也不降级。**落实：** 下次 wip commit 前对照；如再次出现立即升级 P0 + 写入 `git-workflow.mdc`「Commit Subject 规范」段
- **P1（已确认，本任务整体回顾再次复确）：** Level 2+ 多 phase 任务（phase 数 ≥ 5）需要支持「轮次完成」中间态 — `/reflect` 不强制切死端「回顾中」，允许多次 `/build` 续接同一 task 的不同 phase 区间。**首发：** TASK-20260419-03 Round 1 — 当前阶段流转 `构建中 → 回顾中 → 归档中` 假设「一轮即完成」，本任务 Phase 3-6 还要做但已切到「回顾中」，下轮 `/build` 又要切回，违反隐含 invariant。**Round 2 实际操作：** 手动 StrReplace 把阶段改回构建中，无强制守卫干预。**落实：** 修改 `complexity-levels.mdc` 增加「多轮次 Build 工作流」段；或调整 `/reflect`、`/build`、`/archive` 命令的阶段守卫
- **P1（反复出现）：** 任何引入 GCC/Clang 模板特化技巧（`template<usize N>` 取数组引用、CRTP、SFINAE 分派）的 PR 必须在 PR 检查表中加一行「Release `-O2 -Werror` 通路验证」。来源：TASK-20260419-04 反思 — TASK-01 引入 `Lookup<N>` 时仅 Debug 验证，未验证 Release `-Werror=array-bounds`，导致 TASK-03 Phase 1 才暴露；与 TASK-02 反思 #1 P1「性能基准任务必须 Release」同源；**TASK-03 P6 进一步泛化：** 任何新增 .cc / 测试代码合入 main 前都需要 Release 通路验证（不限模板/泛型）— 本任务 P6 fresh build 第 2 次因此暴露问题（`memory_surface_test.cc fgets` + `string_test.cc array-bounds`）。下次涉及代码合入主分支的 PR 前固化到 `writing-plans.mdc`「Release 通路验证」段
- **P1**：跨子库新增符号引用前 grep link graph，确认是否触发循环依赖（来源 TASK-20260419-01 反思 #1，规则已固化到 `writing-plans.mdc`「静态库循环依赖审计」段；下次涉及静态库间符号引用时强制执行）
- **P1**：性能基准任务必须在 Plan 阶段就显式 `-DCMAKE_BUILD_TYPE=Release` + 独立 `build-bench/` 目录（来源 TASK-20260419-02 反思 #1；下次 CSS / Layout / Render bench 立项前固化到 `writing-plans.mdc`「性能基准任务必检项」段）
- **P1**：CMake 操作第三方 target 前必须先用 `get_target_property(... ALIASED_TARGET)` 识别 ALIAS，避免 `set_target_properties` 报错（来源 TASK-20260419-02 反思 #2）
- **P2**：将 `renderer_test` / `render_integration_test` 等剩余手写像素位移断言迁到 `tests/test_pixel_utils.h`，并在该头注释示例 hex→RGBA（来源 TASK-20260413-02）
- **P2**：google/benchmark `RangeMultiplier(m)->Range(lo,hi)` 的精确数量为 `ceil(log_m(hi/lo))+1`，写入 `writing-plans.mdc`「Benchmark 用例估算」附录（来源 TASK-20260419-02 反思 #5；TASK-20260419-03 整体 13 个 Range case 0 偏差实证）
- **P1（新增, TASK-05）：** Bench smoke 验收三件套（数字非零 + SetItemsProcessed > 0 + JSON 校验 items_per_second > 0）— 写入 `writing-plans.mdc`「性能基准任务必检项」段。**根因：** TASK-05 Phase 1 smoke `BM_ReplaySmoke=1.65 ns + items_per_second=0/s` 满足「数字非零」但实际是空 list 假阳性，假阳性持续到 Phase 6 才被 styled-corpus 修正暴露。**已沉淀到 systemPatterns.md「Bench Smoke 自检模式」段**
- **P2（新增, TASK-05）：** 「Render bench 前置清单」+「带否定判据的发现型 Phase」+「跨阶段管道型 API default-nullptr 反模式」3 段已沉淀到 `systemPatterns.md`（来源 TASK-05 K1/K4/K5 + TASK-03 cluster 同源方法论）— 下次 render 系列 bench plan / API 设计 review 必查
- **P2（新增, TASK-05）：** Layout super-linear knee 调查（K2 + K3，buildtree N=128→256 + flex 8x8→16x16 同源 super-linear）— 候选根因：(a) ArenaAllocator chunk grow（4096 byte 边界）；(b) SmallVector 阈值；(c) cache miss。已写入 `techContext.md`「Layout 性能基线」段；建议归入 TASK-20260419-09 候选一并研究
