# 活跃上下文

## 当前阶段
空闲

## 当前任务

_无活跃任务 — 等待 `/van` 接受新任务_

## 最近归档

- `memory-bank/archive/archive-TASK-20260419-05.md`（TASK-20260419-05 Layout + Render 性能基准 — 4 bench exe / 30 BMs / 4 baseline JSON 入仓；K1 实测 DrawText 是 Replay hot path（820× FillRect），ImageCache 不是；推 TASK-20260419-09 候选；落实 P0 #2 `writing-plans.mdc`「性能基准任务必检项」段）
- `memory-bank/archive/archive-TASK-20260419-07.md`（TASK-20260419-07 修复 main Release `-Werror` 编译失败 2 处 — fgets unused-result + BasicString copy ctor IPA array-bounds 误报；与 TASK-04 同元模式不同手法 noinline；已 `--no-ff` 合并到 main `206d227`）
- `memory-bank/archive/archive-TASK-20260419-03.md`（TASK-20260419-03 CSS 解析性能基准 — 30 BMs + 3 baseline JSON 入仓 + Cluster 度量证 PropertyMap 均匀 → TASK-06 P1→P3，已 `--no-ff` 合并到 main `660313a`）
- `memory-bank/archive/archive-TASK-20260419-04.md`（TASK-20260419-04 修复 `enum_serialization.cc` Release `-Warray-bounds` 误报，已合并到 main `a09ad1e`）
- `memory-bank/archive/archive-TASK-20260419-02.md`（TASK-20260419-02 Google Benchmark 集成 + Foundation 性能基线，已合并到 main）
- `memory-bank/archive/archive-TASK-20260419-01.md`（TASK-20260419-01 流程规则沉淀 + P2 功能技术债收口，已合并到 main）

## 待处理事项

### 后续任务候选

- ~~TASK-20260419-05：已归档（`archive-TASK-20260419-05.md`）~~
- ~~TASK-20260419-07：已归档（`archive-TASK-20260419-07.md`）~~
- **TASK-20260419-06（建议，P3 降级）：** HashMap Hash Mixing 优化（cluster 问题）— `BM_HashMapLookupHitInt/16384=9µs` vs n=64 时 69ns，根因 `H1=h>>7` + `std::hash<int>` 恒等映射。**降级理由（TASK-03 P4 实测）：** PropertyMap 60-entry HashMap<StringView, PropertyId> + djb2 hash 在最差 single key 下仅 2.75× HitHot5（远低于 5× cluster 阈值），证 cluster 问题主要见于 **int key + 大规模**场景。**触发条件**：「短字符串 ≠ 主用例 + 容器规模 > 1000 entry」的新场景出现时再立项
- **TASK-20260419-08（候选，P3 触发型）：** `string.h` 剩余 3 处 runtime-size memcpy（line 45 SSO ctor / 150 Append / 230 GrowAndCopy）防御性 noinline 化。**触发条件**：未来 GCC 升级回归同类 `-Warray-bounds` 误报；目前不主动改避免引入不必要内联开销（来源 TASK-20260419-07 副发现）
- **TASK-20260419-09（建议 P1，TASK-05 K1 + K5 触发）：** Replay hot path 深度基准 — 真实 ImageCache 通路（含解决 `DecodeFromFile` fixture 文件依赖：在 build-bench 期 `configure_file()` 复制 1×1 PNG 到 `${CMAKE_BINARY_DIR}/benchmarks/fixtures/`）+ `DrawText` 微基准（拆解 SimpleTextShaper / glyph cache lookup / SoftwareCanvas DrawTextFallback）。**目标量化**：「Text 是否真的是 820× FillRect 慢的根因」+「ImageCache 走真路径是否仍 < DrawText」+ Layout super-linear knee 根因（K2 + K3 同源调查）。来源：TASK-05 K1 hot path 实证 + K5 fixture 缺失工程问题 + K2/K3 super-linear

### 长期项（按优先级）

- **🔴 P0（紧急升级，反复 9+ 次，TASK-07 已验有效预防）：** Cursor 沙箱内任何 FetchContent 任务的 VAN 阶段**必须**强制重设 git 全局代理（`git config --global http.proxy http://...`），并在 `/archive` 阶段决定是否 unset；任务 hand-off 必须在新任务 VAN 检查表加一行「proxy 状态确认」。**根因：** TASK-02/04 反思都识别此问题但只升 P1，每次「单次 5-10 分钟可解决」导致**累计成本 ≥ 1 小时**；TASK-03 Round 2 第 9+ 次出现破例升 P0。**待落实动作：** (a) 写入 `main.mdc` 或 `writing-plans.mdc`「FetchContent 任务前置 checklist」强制条目；(b) `/van` 命令文档加阶段守卫「检测到 plan 含 FetchContent → 自动提醒 proxy 状态」。**已知代理地址：** `http://192.168.101.217:7890`（开发环境特定，写规则时用占位符）
- ~~**🔴 P0（TASK-07 + TASK-05 第 2 次实证）：** `writing-plans.mdc` 「目标 API 的发射/触发条件 grep」段~~ → ✅ **已于 TASK-20260419-05 /archive 阶段落实**：`writing-plans.mdc` 新增「性能基准任务必检项」段（6 子项，含发射条件 grep / smoke 三件套 / RangeMultiplier 公式 / Render 前置清单交叉引用）
- **🟠 P2（新增，反复模式表更新）：** `/reflect` 命令 §3.5 反复模式表加新行「方案根因假设未先验证」频率提到 2 次（来源 TASK-07 + TASK-05）
- **P1（已确认，本任务整体回顾再次复确）：** WIP / 中间 commit 的 subject 严禁包含外部任务状态字样（`BLOCKED on TASK-X` / `WAITING for Y` / `PENDING dep`）。**首发：** TASK-20260419-03 Round 1。**落实：** 下次 wip commit 前对照；如再次出现立即升级 P0 + 写入 `git-workflow.mdc`「Commit Subject 规范」段
- **P1（已确认，本任务整体回顾再次复确）：** Level 2+ 多 phase 任务（phase 数 ≥ 5）需要支持「轮次完成」中间态 — `/reflect` 不强制切死端「回顾中」。**首发：** TASK-20260419-03 Round 1。**落实：** 修改 `complexity-levels.mdc` 增加「多轮次 Build 工作流」段；或调整 `/reflect`、`/build`、`/archive` 命令的阶段守卫
- **P1（反复出现）：** 任何引入 GCC/Clang 模板特化技巧（`template<usize N>` 取数组引用、CRTP、SFINAE 分派）的 PR 必须在 PR 检查表中加一行「Release `-O2 -Werror` 通路验证」。来源：TASK-04 + TASK-03 P6 + TASK-07 实证。固化到 `writing-plans.mdc`「Release 通路验证」段
- **P1**：跨子库新增符号引用前 grep link graph，确认是否触发循环依赖（来源 TASK-20260419-01 反思 #1，规则已固化到 `writing-plans.mdc`「静态库循环依赖审计」段）
- ~~**P1**：性能基准任务必须在 Plan 阶段就显式 `-DCMAKE_BUILD_TYPE=Release` + 独立 `build-bench/` 目录~~ → ✅ **已于 TASK-05 /archive 阶段落实**：`writing-plans.mdc`「性能基准任务必检项」§1 强制条目
- **P1**：CMake 操作第三方 target 前必须先用 `get_target_property(... ALIASED_TARGET)` 识别 ALIAS，避免 `set_target_properties` 报错（来源 TASK-20260419-02 反思 #2）
- ~~**P1（新增, TASK-05）：** Bench smoke 验收三件套~~ → ✅ **已于 TASK-05 /archive 阶段落实**：`writing-plans.mdc`「性能基准任务必检项」§4 强制条目 + `systemPatterns.md`「Bench Smoke 自检模式」段
- **P2**：将 `renderer_test` / `render_integration_test` 等剩余手写像素位移断言迁到 `tests/test_pixel_utils.h`（来源 TASK-20260413-02）
- ~~**P2**：google/benchmark `RangeMultiplier(m)->Range(lo,hi)` 的精确数量为 `ceil(log_m(hi/lo))+1`~~ → ✅ **已于 TASK-05 /archive 阶段落实**：`writing-plans.mdc`「性能基准任务必检项」§5 强制条目
- ~~**P2（新增, TASK-05）：** 「Render bench 前置清单」+「带否定判据的发现型 Phase」+「跨阶段管道型 API default-nullptr 反模式」~~ → ✅ **已于 TASK-05 /reflect 阶段沉淀到 `systemPatterns.md`**（4 段）+ /archive 阶段交叉引用入 `writing-plans.mdc`「性能基准任务必检项」§6
- **P2（TASK-05 K2/K3 待研究）：** Layout super-linear knee 调查 — buildtree N=128→256 + flex 8x8→16x16 同源 super-linear（10×～15× for 2×～4× 输入）。候选根因：(a) ArenaAllocator chunk grow（4096 byte 边界）；(b) SmallVector 阈值；(c) cache miss。已写入 `techContext.md`「Layout 性能基线」段；建议归入 TASK-20260419-09 候选一并研究
