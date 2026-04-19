# 活跃上下文

## 当前阶段
归档中

## 当前任务

**TASK-20260419-07：修复 main Release `-Werror` 编译失败（2 处）**

- 级别：Level 1
- 分支：`feature/TASK-20260419-07-release-werror-fixes`（基于 main `b321482`）
- 焦点：(a) `memory_surface_test.cc` `fgets -Wunused-result`（line 102/105/108） + (b) `string_test.cc` 触发 `string.h:69` `memcpy -Werror=array-bounds`（GCC IPA 误报）
- 已实地复现 ✅（`build-bench/` 复用，无需 FetchContent → P0 git proxy checklist 不触发）
- 来源：TASK-03 P6 fresh build 副发现 + 长期项 P1 #2 + 与 TASK-04 同源（Release 通路验证缺失反复模式）

**构建完成 ✅（2 commit）**

- (a) `8b57f8d` fix(tests/platform): A1 `ASSERT_NE(std::fgets(...), nullptr)` × 3 处
- (b) `51d6ff1` fix(foundation/strings): B2 `[[gnu::noinline]]` 拷贝构造（GCC 守卫）— **B3 `__builtin_memcpy` 假设失败**（IPA 在中端，先于 fortify 展开）
- 完成验证：Release 全量 build 0 errors（38.7s）+ Debug ctest 890/890（2.46s）+ 7 bench sanity exit 0

**回顾完成 ✅**

- 回顾文档：`memory-bank/reflection/reflection-TASK-20260419-07.md`
- 关键发现：B3 假设失败暴露方法论盲点（GCC IPA 在中端，先于 fortify）；TASK-04 ↔ TASK-07 是「阻断 IPA 关联」元模式两个实例；VAN「无需 FetchContent」预判完美绕开 P0 反复模式
- 改进建议：5 项（2 P1 / 1 P2 / 1 P3 + 1 P2 横向链接）
- 已沉淀：`systemPatterns.md` 新段「GCC IPA `-Warray-bounds` 3 阶段诊断与修复表 + 阻断关联方案族 + 候选方案根因验证要求」
- 下一步：`/archive` 归档

## 最近归档

- `memory-bank/archive/archive-TASK-20260419-03.md`（TASK-20260419-03 CSS 解析性能基准 — 30 BMs + 3 baseline JSON 入仓 + Cluster 度量证 PropertyMap 均匀 → TASK-06 P1→P3，已 `--no-ff` 合并到 main `660313a`）
- `memory-bank/archive/archive-TASK-20260419-04.md`（TASK-20260419-04 修复 `enum_serialization.cc` Release `-Warray-bounds` 误报，已合并到 main `a09ad1e`）
- `memory-bank/archive/archive-TASK-20260419-02.md`（TASK-20260419-02 Google Benchmark 集成 + Foundation 性能基线，已合并到 main）
- `memory-bank/archive/archive-TASK-20260419-01.md`（TASK-20260419-01 流程规则沉淀 + P2 功能技术债收口，已合并到 main）

## 待处理事项

### 后续任务候选

- **TASK-20260419-05（建议）：** Layout + Render 基准 — `bench_layout_buildtree` / `bench_layout_flex` / `bench_render_record` / `bench_render_replay`（来源 TASK-20260419-02 归档）
- **TASK-20260419-08（候选，P3 触发型）：** `string.h` 剩余 3 处 runtime-size memcpy（line 45 SSO ctor / 150 Append / 230 GrowAndCopy）防御性 noinline 化。**触发条件**：未来 GCC 升级回归同类 `-Warray-bounds` 误报；目前不主动改避免引入不必要内联开销（来源 TASK-20260419-07 副发现）
- **TASK-20260419-06（建议，**优先级降级 P1→P3**）：** HashMap Hash Mixing 优化（cluster 问题）— `BM_HashMapLookupHitInt/16384=9µs` vs n=64 时 69ns，根因 `H1=h>>7` + `std::hash<int>` 恒等映射使所有起始探测位置压在 [0,127]。**降级根据**（TASK-03 P4 实测）：PropertyMap 60-entry HashMap<StringView, PropertyId> + djb2 hash 在最差 single key 下仅 2.75× HitHot5（远低于 5× cluster 阈值），证明 cluster 问题主要见于 int key + 大规模场景，**短字符串 + 小规模场景免疫**；TASK-06 价值仍在但可推迟到大规模 int-key 容器场景出现时
- ~~TASK-20260419-07：已立项为当前任务，详见上方「当前任务」段~~

### 长期项（按优先级）

- **🟠 P1（新立项，TASK-20260419-07 关键教训）：** `writing-plans.mdc` 增加段「候选方案表必须附根因验证步骤」— 候选方案 ≥ 3 项时强制要求每方案附 ≤ 30 秒根因验证操作（grep / 读 1 个头 / 跑 1 个 build）。**根因：** TASK-07 VAN 假设「`-Warray-bounds` 来自 `__memcpy_chk` fortify」推荐 B3 `__builtin_memcpy`，BUILD 试验失败才发现 IPA 在中端先于 fortify 展开。**反复模式新子类：** 「方案-根因绑定假设未验证」首次明确识别（已在反复模式表加新行）。**落实命令：** `/build` 步骤 2 + `/plan` 候选方案表段
- **🟠 P2（新增，反复模式表更新）：** `/reflect` 命令 §3.5 反复模式表加新行「方案根因假设未先验证」频率 1 次（来源 TASK-07）。**落实：** `.cursor/rules/skills/...` 或 `/reflect` 命令文档
- **🔴 P0（紧急升级，反复 9+ 次，TASK-07 已验有效预防）：** Cursor 沙箱内任何 FetchContent 任务的 VAN 阶段**必须**强制重设 git 全局代理（`git config --global http.proxy http://...`），并在 `/archive` 阶段决定是否 unset；任务 hand-off（如 TASK-04 archive → TASK-03 resume → TASK-03 fresh rebuild）必须在新任务 VAN 检查表加一行「proxy 状态确认」。**根因：** TASK-02/04 反思都识别了此问题但只升 P1，每次都是「单次 5-10 分钟可解决 + 可绕行」导致**累计成本 ≥ 1 小时却始终未升 P0**；本次 TASK-03 Round 2 Phase 6 fresh build 第 9+ 次出现，破例升 P0。**下次落实动作：** (a) 写入 `main.mdc` 或 `writing-plans.mdc`「FetchContent 任务前置 checklist」强制条目；(b) `/van` 命令文档加阶段守卫「检测到 plan 含 FetchContent → 自动提醒 proxy 状态」。**已知代理地址：** `http://192.168.101.217:7890`（开发环境特定，写入规则时用占位符）
- ~~🟠 P1（已立项为 TASK-20260419-07，移至上方「当前任务」段）~~
- **🟠 P3（实测降级，原 TASK-20260419-06，原 P1）：** HashMap Hash Mixing 优化 — `BM_HashMapLookupHitInt/16384=9µs` vs n=64 时 69ns，根因 `H1=h>>7` + `std::hash<int>` 恒等映射使所有起始探测位置压在 [0,127]（来源 TASK-20260419-02 BUILD 副发现）。**降级理由（TASK-20260419-03 P4 实测）：** PropertyMap 60-entry HashMap<StringView, PropertyId> + djb2 hash 在最差 single key 下仅 2.75× HitHot5（远低于 5× cluster 阈值），证 cluster 问题主要见于 **int key + 大规模**场景，**短字串 + 小规模**场景免疫。**触发条件**变为：当出现「短字符串 ≠ 主用例 + 容器规模 > 1000 entry」的新场景时再立项；不主动追求优化 PropertyMap 类场景
- **P1（已确认，本任务整体回顾再次复确）：** WIP / 中间 commit 的 subject 严禁包含外部任务状态字样（`BLOCKED on TASK-X` / `WAITING for Y` / `PENDING dep`），改用中性 `(unverified)` / `(compile-pending)` 后缀；外部依赖关系写到 commit body。**根因：** WIP commit 在 rebase / 续接 / 后续阅读时，subject 的 git log oneline 长期可见而 body 易被忽略，"过期外部状态"会持续误导。**首发：** TASK-20260419-03 Round 1 — `wip(TASK-03): Phase 1 CSS bench scaffolding (BLOCKED on TASK-04)` commit 在 TASK-04 解锁后 subject 字面失效但难以清理；**Round 2 完成后 git log 阅读体验仍受损**，确认 P1 不升级也不降级。**落实：** 下次 wip commit 前对照；如再次出现立即升级 P0 + 写入 `git-workflow.mdc`「Commit Subject 规范」段
- **P1（已确认，本任务整体回顾再次复确）：** Level 2+ 多 phase 任务（phase 数 ≥ 5）需要支持「轮次完成」中间态 — `/reflect` 不强制切死端「回顾中」，允许多次 `/build` 续接同一 task 的不同 phase 区间。**首发：** TASK-20260419-03 Round 1 — 当前阶段流转 `构建中 → 回顾中 → 归档中` 假设「一轮即完成」，本任务 Phase 3-6 还要做但已切到「回顾中」，下轮 `/build` 又要切回，违反隐含 invariant。**Round 2 实际操作：** 手动 StrReplace 把阶段改回构建中，无强制守卫干预。**落实：** 修改 `complexity-levels.mdc` 增加「多轮次 Build 工作流」段；或调整 `/reflect`、`/build`、`/archive` 命令的阶段守卫
- **P1（反复出现）：** 任何引入 GCC/Clang 模板特化技巧（`template<usize N>` 取数组引用、CRTP、SFINAE 分派）的 PR 必须在 PR 检查表中加一行「Release `-O2 -Werror` 通路验证」。来源：TASK-20260419-04 反思 — TASK-01 引入 `Lookup<N>` 时仅 Debug 验证，未验证 Release `-Werror=array-bounds`，导致 TASK-03 Phase 1 才暴露；与 TASK-02 反思 #1 P1「性能基准任务必须 Release」同源；**TASK-03 P6 进一步泛化：** 任何新增 .cc / 测试代码合入 main 前都需要 Release 通路验证（不限模板/泛型）— 本任务 P6 fresh build 第 2 次因此暴露问题（`memory_surface_test.cc fgets` + `string_test.cc array-bounds`）。下次涉及代码合入主分支的 PR 前固化到 `writing-plans.mdc`「Release 通路验证」段
- **P1**：跨子库新增符号引用前 grep link graph，确认是否触发循环依赖（来源 TASK-20260419-01 反思 #1，规则已固化到 `writing-plans.mdc`「静态库循环依赖审计」段；下次涉及静态库间符号引用时强制执行）
- **P1**：性能基准任务必须在 Plan 阶段就显式 `-DCMAKE_BUILD_TYPE=Release` + 独立 `build-bench/` 目录（来源 TASK-20260419-02 反思 #1；下次 CSS / Layout / Render bench 立项前固化到 `writing-plans.mdc`「性能基准任务必检项」段）
- **P1**：CMake 操作第三方 target 前必须先用 `get_target_property(... ALIASED_TARGET)` 识别 ALIAS，避免 `set_target_properties` 报错（来源 TASK-20260419-02 反思 #2）
- **P2**：将 `renderer_test` / `render_integration_test` 等剩余手写像素位移断言迁到 `tests/test_pixel_utils.h`，并在该头注释示例 hex→RGBA（来源 TASK-20260413-02）
- **P2**：google/benchmark `RangeMultiplier(m)->Range(lo,hi)` 的精确数量为 `ceil(log_m(hi/lo))+1`，写入 `writing-plans.mdc`「Benchmark 用例估算」附录（来源 TASK-20260419-02 反思 #5；TASK-20260419-03 整体 13 个 Range case 0 偏差实证）
