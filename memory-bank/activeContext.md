# 活跃上下文

## 当前阶段
空闲（TASK-20260419-03 已开工但暂停于 Phase 1，等待 TASK-20260419-04 解锁）

## 当前任务
无活跃任务。建议下一步：`/van TASK-04` 处理 `enum_serialization.cc` Release `-Warray-bounds` 误报，解锁 TASK-03 续接。

## 暂停中任务

**TASK-20260419-03** — CSS 解析性能基准
- 分支：`feature/TASK-20260419-03-css-benchmarks`（已 ahead-of-main 4 commits：plan + WIP Phase 1 + 2 chore）
- Phase 0 ✅ Phase 1 ⛔（vx_core Release `-Werror=array-bounds` 阻塞）
- 续接：TASK-04 合并后 `git checkout feature/TASK-20260419-03-css-benchmarks` → `/build`

## 最近归档

- `memory-bank/archive/archive-TASK-20260419-02.md`（TASK-20260419-02 Google Benchmark 集成 + Foundation 性能基线，已合并到 main）
- `memory-bank/archive/archive-TASK-20260419-01.md`（TASK-20260419-01 流程规则沉淀 + P2 功能技术债收口，已合并到 main）
- `memory-bank/archive/archive-TASK-20260418-01.md`（TASK-20260418-01 消化关键技术债务，已合并到 main）

## 待处理事项

### 后续任务候选

- **TASK-20260419-03（暂停）：** CSS 解析基准 — `bench_css_tokenizer` / `bench_css_parser` / `bench_css_property_lookup`；Phase 1 阻塞，详见上方暂停中任务
- **TASK-20260419-04（待 `/van`，最高优先级）：** 修复 `veloxa/core/css/enum_serialization.cc` Release `-O2 -Werror=array-bounds` 误报（GCC IPA inline 模板 `Lookup<N>` 跨 5+ clone 值域分析失误）；候选方案：A) 文件局部 `#pragma GCC diagnostic push/ignored "-Warray-bounds"` / B) CMake `set_source_files_properties(... PROPERTIES COMPILE_OPTIONS "-Wno-array-bounds")` / C) 重构 `Lookup<N>` 去模板化；Level 1（小修小补 / 单文件）；解锁目标 = TASK-03 Phase 1 续接
- **TASK-20260419-05（建议）：** Layout + Render 基准 — `bench_layout_buildtree` / `bench_layout_flex` / `bench_render_record` / `bench_render_replay`（原 04，编号顺延；来源 TASK-20260419-02 归档）
- **TASK-20260419-06（建议）：** HashMap Hash Mixing 优化（cluster 问题，反思 #4） — `BM_HashMapLookupHitInt/16384=9µs` vs n=64 时 69ns，根因 `H1=h>>7` + `std::hash<int>` 恒等映射使所有起始探测位置压在 [0,127]（原 05，编号顺延；来源 TASK-20260419-02 BUILD 副发现）

### 长期项（按优先级）

- **P1**：跨子库新增符号引用前 grep link graph，确认是否触发循环依赖（来源 TASK-20260419-01 反思 #1，规则已固化到 `writing-plans.mdc` 「静态库循环依赖审计」段；下次涉及静态库间符号引用时强制执行）
- **P1**：性能基准任务必须在 Plan 阶段就显式 `-DCMAKE_BUILD_TYPE=Release` + 独立 `build-bench/` 目录（来源 TASK-20260419-02 反思 #1；下次 CSS / Layout / Render bench 立项前固化到 `writing-plans.mdc` 「性能基准任务必检项」段）
- **P1**：CMake 操作第三方 target 前必须先用 `get_target_property(... ALIASED_TARGET)` 识别 ALIAS，避免 `set_target_properties` 报错（来源 TASK-20260419-02 反思 #2；下次 FetchContent 涉及第三方 target 改属性时固化到 `writing-plans.mdc` 「CMake 链接方向约束分析」段后追加「ALIAS 识别步骤」）
- **P1**：Cursor 沙箱内涉及 `FetchContent` 时，VAN 阶段必须验证「子进程能否真正用上自定义代理」而非仅父 shell 测试；推荐 `git config --global http.proxy` 并明确恢复时机（来源 TASK-20260419-02 反思 #3；属于"前置环境验证"反复模式的特化；下次任何 FetchContent 任务前固化到 `main.mdc` 或 `writing-plans.mdc` 「Cursor 沙箱代理验证」段）
- **P2**：将 `renderer_test` / `render_integration_test` 等剩余手写像素位移断言迁到 `tests/test_pixel_utils.h`，并在该头注释示例 hex→RGBA（来源 TASK-20260413-02）
- **P2**：google/benchmark `RangeMultiplier(m)->Range(lo,hi)` 的精确数量为 `ceil(log_m(hi/lo))+1`，写入 `writing-plans.mdc` 「Benchmark 用例估算」附录（来源 TASK-20260419-02 反思 #5）
