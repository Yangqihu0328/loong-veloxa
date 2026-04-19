# 活跃上下文

## 当前阶段
空闲（TASK-20260419-04 已归档并合并到 main；TASK-20260419-03 仍暂停于 Phase 1，但阻塞已解除，可随时续接）

## 当前任务
无活跃任务。

**优先建议下一步：** 续接 TASK-20260419-03 — `git checkout feature/TASK-20260419-03-css-benchmarks && git rebase main && /build`，先验证 3 个 CSS smoke bench 编译跑通后进入 Phase 2。

## 暂停中任务

**TASK-20260419-03** — CSS 解析性能基准（Tokenizer / Parser / Property Lookup）
- 分支：`feature/TASK-20260419-03-css-benchmarks`（ahead-of-old-main 3 commits：plan + WIP Phase 1 + chore；待 rebase main `a09ad1e` 拾取 TASK-04 修复）
- Phase 0 ✅ Phase 1 ⛔（vx_core Release `-Werror=array-bounds`，**已被 TASK-20260419-04 解锁**）
- 续接动作：`git checkout feature/TASK-20260419-03-css-benchmarks && git rebase main && cmake --build build-bench --target bench_css_{tokenizer,parser,property_lookup} -j` 验证 3 smoke 通过 → 进入 Phase 2
- 续接需重新 `/build` — `activeContext.md` 续接前需切回 `构建中` 阶段

## 最近归档

- `memory-bank/archive/archive-TASK-20260419-04.md`（TASK-20260419-04 修复 `enum_serialization.cc` Release `-Warray-bounds` 误报，已合并到 main `a09ad1e`）
- `memory-bank/archive/archive-TASK-20260419-02.md`（TASK-20260419-02 Google Benchmark 集成 + Foundation 性能基线，已合并到 main）
- `memory-bank/archive/archive-TASK-20260419-01.md`（TASK-20260419-01 流程规则沉淀 + P2 功能技术债收口，已合并到 main）

## 待处理事项

### 后续任务候选

- **TASK-20260419-03（暂停，阻塞已解除）：** CSS 解析基准 — 立即可续接，详见上方暂停中任务
- **TASK-20260419-05（建议）：** Layout + Render 基准 — `bench_layout_buildtree` / `bench_layout_flex` / `bench_render_record` / `bench_render_replay`（来源 TASK-20260419-02 归档）
- **TASK-20260419-06（建议）：** HashMap Hash Mixing 优化（cluster 问题，反思 #4） — `BM_HashMapLookupHitInt/16384=9µs` vs n=64 时 69ns，根因 `H1=h>>7` + `std::hash<int>` 恒等映射使所有起始探测位置压在 [0,127]（来源 TASK-20260419-02 BUILD 副发现）

### 长期项（按优先级）

- **P1（新升级，反复出现）：** 任何引入 GCC/Clang 模板特化技巧（`template<usize N>` 取数组引用、CRTP、SFINAE 分派）的 PR 必须在 PR 检查表中加一行「Release `-O2 -Werror` 通路验证」。来源：TASK-20260419-04 反思 — TASK-01 引入 `Lookup<N>` 时仅 Debug 验证，未验证 Release `-Werror=array-bounds`，导致 TASK-03 Phase 1 才暴露；与 TASK-02 反思 #1 P1「性能基准任务必须 Release」同源（同属"前置依赖/环境/API 能力未验证"反复模式，已 8+ 次出现），但泛化范围更广 — 不限基准任务。下次涉及模板/泛型代码引入的 PR 前固化到 `writing-plans.mdc` 「Release 通路验证」段
- **P1**：跨子库新增符号引用前 grep link graph，确认是否触发循环依赖（来源 TASK-20260419-01 反思 #1，规则已固化到 `writing-plans.mdc` 「静态库循环依赖审计」段；下次涉及静态库间符号引用时强制执行）
- **P1**：性能基准任务必须在 Plan 阶段就显式 `-DCMAKE_BUILD_TYPE=Release` + 独立 `build-bench/` 目录（来源 TASK-20260419-02 反思 #1；下次 CSS / Layout / Render bench 立项前固化到 `writing-plans.mdc` 「性能基准任务必检项」段）
- **P1**：CMake 操作第三方 target 前必须先用 `get_target_property(... ALIASED_TARGET)` 识别 ALIAS，避免 `set_target_properties` 报错（来源 TASK-20260419-02 反思 #2；下次 FetchContent 涉及第三方 target 改属性时固化到 `writing-plans.mdc` 「CMake 链接方向约束分析」段后追加「ALIAS 识别步骤」）
- **P1**：Cursor 沙箱内涉及 `FetchContent` 时，VAN 阶段必须验证「子进程能否真正用上自定义代理」而非仅父 shell 测试；推荐 `git config --global http.proxy` 并明确恢复时机（来源 TASK-20260419-02 反思 #3；属于"前置环境验证"反复模式的特化；下次任何 FetchContent 任务前固化到 `main.mdc` 或 `writing-plans.mdc` 「Cursor 沙箱代理验证」段）
- **P2**：将 `renderer_test` / `render_integration_test` 等剩余手写像素位移断言迁到 `tests/test_pixel_utils.h`，并在该头注释示例 hex→RGBA（来源 TASK-20260413-02）
- **P2**：google/benchmark `RangeMultiplier(m)->Range(lo,hi)` 的精确数量为 `ceil(log_m(hi/lo))+1`，写入 `writing-plans.mdc` 「Benchmark 用例估算」附录（来源 TASK-20260419-02 反思 #5）
