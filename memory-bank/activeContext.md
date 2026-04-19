# 活跃上下文

## 当前阶段
构建中

## 当前任务

**TASK-20260419-02** — 补充 Google Benchmark 集成与 Foundation 基础库性能基准（原 TASK-20260405-01 延期项 P1#1）

- **复杂度：** Level 2（简单增强）
- **基线分支：** `main`
- **功能分支（待创建）：** `feature/TASK-20260419-02-benchmarks`
- **代理：** `192.168.101.217:7890`（已验证 git clone google/benchmark v1.9.1 8.3s 成功）
- **设计规格：** `docs/specs/2026-04-19-benchmarks-design.md`
- **实现计划：** `docs/plans/2026-04-19-benchmarks.md`

**关键决策（4×A，已锁定）：**
- 范围：仅 Foundation（allocators / containers / strings / hash_map）
- 结构：一文件一可执行（4 个 exe）
- 深度：每组件覆盖核心操作 + 1-2 个梯度（~25 BM，按 Range 展开 ~42）
- 留存：仅控制台 + README 给出 JSON 导出方法（不提交 baseline）

**焦点：** 等待用户审查 `docs/specs/...` + `docs/plans/...`，确认后执行 `/build` 进入 Phase 0 基线验证。

**后续任务（待 02 归档后立项）：**
- TASK-20260419-03（建议）：CSS 解析基准（bench_css_tokenizer / bench_css_parser / bench_property_lookup）
- TASK-20260419-04（建议）：Layout + Render 基准（bench_layout_buildtree / bench_render_record / bench_render_replay）

**下一步：** `/build`（按 `docs/plans/2026-04-19-benchmarks.md` 7 个 phase 顺序执行）

## 最近归档

- `memory-bank/archive/archive-TASK-20260419-01.md`（TASK-20260419-01 流程规则沉淀 + P2 功能技术债收口，已合并到 main）
- `memory-bank/archive/archive-TASK-20260418-01.md`(TASK-20260418-01 消化关键技术债务，已合并到 main)

## 待处理事项

### 长期项（按优先级）

- **P1**：~~补充 Benchmark（网络恢复后，来源 TASK-01）~~ → 已立项为 TASK-20260419-02（处理中）
- **P1**：跨子库新增符号引用前 grep link graph，确认是否触发循环依赖（来源 TASK-20260419-01 反思 #1，规则已固化到 `writing-plans.mdc` 「静态库循环依赖审计」段；下次涉及静态库间符号引用时强制执行）
- **P2**：将 `renderer_test` / `render_integration_test` 等剩余手写像素位移断言迁到 `tests/test_pixel_utils.h`，并在该头注释示例 hex→RGBA（来源 TASK-20260413-02）
