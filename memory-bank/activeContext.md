# 活跃上下文

## 当前阶段
回顾中（reflection 已落地）

## 当前任务

**TASK-20260419-02** — 补充 Google Benchmark 集成与 Foundation 基础库性能基准（原 TASK-20260405-01 延期项 P1#1）

- **复杂度：** Level 2（简单增强）
- **基线分支：** `main`
- **功能分支：** `feature/TASK-20260419-02-benchmarks`
- **代理：** `192.168.101.217:7890`（验证有效；本次构建期间临时 `git config --global http.proxy`，Phase 7 已 unset）
- **设计规格：** `docs/specs/2026-04-19-benchmarks-design.md`
- **实现计划：** `docs/plans/2026-04-19-benchmarks.md`

**构建结果（Phase 0–7 全绿）：**
- 7 个提交：plan + scaffold(P1) + 4×bench(P2-5) + docs(P6) — Phase 7 仅验证未产生新代码
- 4 个 exe：bench_allocators(13 BM) / bench_containers(8) / bench_hash_map(10) / bench_strings(9) = **40 BM**
- 全测试 890/890 通过；全量构建 0 警告 0 错误（Debug）
- CVE 审计：google/benchmark v1.9.1 GitHub Security Advisories 0 条
- Bonus 发现（DEBUG WSL 数据）：`HashMap::Find` 对小整数 key + `H1=h>>7` 在 cap≥1024 时所有起始探测位置挤在 [0,127]，LookupHit/16384 = 9µs（vs 64 = 69ns），已记入 `benchmarks/README.md` 留待后续优化

**焦点：** 回顾已完成（`memory-bank/reflection/reflection-TASK-20260419-02.md`）。关键沉淀：CMake ALIAS target 不可写属性、Cursor 沙箱代理需走 git config 而非 env、benchmark Range 估算公式、HashMap cluster 副发现。下一步进入 `/archive`。

**后续任务（待 02 归档后立项）：**
- TASK-20260419-03（建议）：CSS 解析基准（bench_css_tokenizer / bench_css_parser / bench_property_lookup）
- TASK-20260419-04（建议）：Layout + Render 基准（bench_layout_buildtree / bench_render_record / bench_render_replay）
- TASK-20260419-05（建议）：HashMap Hash Mixing 优化（cluster 问题，反思 #4）

**下一步：** `/archive`

## 最近归档

- `memory-bank/archive/archive-TASK-20260419-01.md`（TASK-20260419-01 流程规则沉淀 + P2 功能技术债收口，已合并到 main）
- `memory-bank/archive/archive-TASK-20260418-01.md`(TASK-20260418-01 消化关键技术债务，已合并到 main)

## 待处理事项

### 长期项（按优先级）

- **P1**：~~补充 Benchmark（网络恢复后，来源 TASK-01）~~ → ✅ 已完成 TASK-20260419-02（待归档）
- **P1**：跨子库新增符号引用前 grep link graph，确认是否触发循环依赖（来源 TASK-20260419-01 反思 #1，规则已固化到 `writing-plans.mdc` 「静态库循环依赖审计」段；下次涉及静态库间符号引用时强制执行）
- **P1**：性能基准任务必须在 Plan 阶段就显式 `-DCMAKE_BUILD_TYPE=Release` + 独立 `build-bench/` 目录（来源 TASK-20260419-02 反思 #1；下次 CSS / Layout / Render bench 立项前固化到 `writing-plans.mdc` 「性能基准任务必检项」段）
- **P1**：CMake 操作第三方 target 前必须先用 `get_target_property(... ALIASED_TARGET)` 识别 ALIAS，避免 `set_target_properties` 报错（来源 TASK-20260419-02 反思 #2；下次 FetchContent 涉及第三方 target 改属性时固化到 `writing-plans.mdc` 「CMake 链接方向约束分析」段后追加「ALIAS 识别步骤」）
- **P1**：Cursor 沙箱内涉及 `FetchContent` 时，VAN 阶段必须验证「子进程能否真正用上自定义代理」而非仅父 shell 测试；推荐 `git config --global http.proxy` 并明确恢复时机（来源 TASK-20260419-02 反思 #3；属于"前置环境验证"反复模式的特化；下次任何 FetchContent 任务前固化到 `main.mdc` 或 `writing-plans.mdc` 「Cursor 沙箱代理验证」段）
- **P2**：将 `renderer_test` / `render_integration_test` 等剩余手写像素位移断言迁到 `tests/test_pixel_utils.h`，并在该头注释示例 hex→RGBA（来源 TASK-20260413-02）
- **P2**：HashMap `H1=h>>7` 对小整数 / 对齐指针等低位高熵 key 严重 cluster（`BM_HashMapLookupHitInt/16384=9µs` vs n=64 时 69ns），建议立项 TASK-20260419-05 引入 hash mixing（来源 TASK-20260419-02 BUILD 副发现）
- **P2**：google/benchmark `RangeMultiplier(m)->Range(lo,hi)` 的精确数量为 `ceil(log_m(hi/lo))+1`，写入 `writing-plans.mdc` 「Benchmark 用例估算」附录（来源 TASK-20260419-02 反思 #5）
