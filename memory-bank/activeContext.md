# 活跃上下文

## 当前阶段
回顾中

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

**焦点：** 进入 `/reflect`，从决策准确率、TDD 适配（Benchmark 非 GTest 走"覆盖补充"模式）、SYSTEM include 屏蔽 -Werror 技巧、git 临时代理等维度沉淀经验。

**后续任务（待 02 归档后立项）：**
- TASK-20260419-03（建议）：CSS 解析基准（bench_css_tokenizer / bench_css_parser / bench_property_lookup）
- TASK-20260419-04（建议）：Layout + Render 基准（bench_layout_buildtree / bench_render_record / bench_render_replay）
- 候选债：HashMap 大容量 cluster 问题（独立优化任务）

**下一步：** `/reflect`

## 最近归档

- `memory-bank/archive/archive-TASK-20260419-01.md`（TASK-20260419-01 流程规则沉淀 + P2 功能技术债收口，已合并到 main）
- `memory-bank/archive/archive-TASK-20260418-01.md`(TASK-20260418-01 消化关键技术债务，已合并到 main)

## 待处理事项

### 长期项（按优先级）

- **P1**：~~补充 Benchmark（网络恢复后，来源 TASK-01）~~ → 已立项为 TASK-20260419-02（处理中）
- **P1**：跨子库新增符号引用前 grep link graph，确认是否触发循环依赖（来源 TASK-20260419-01 反思 #1，规则已固化到 `writing-plans.mdc` 「静态库循环依赖审计」段；下次涉及静态库间符号引用时强制执行）
- **P2**：将 `renderer_test` / `render_integration_test` 等剩余手写像素位移断言迁到 `tests/test_pixel_utils.h`，并在该头注释示例 hex→RGBA（来源 TASK-20260413-02）
