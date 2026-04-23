# 归档：TASK-20260424-01 Layout super-linear knee 根因调查

**日期：** 2026-04-24
**任务 ID：** TASK-20260424-01（候选区原 ID TASK-20260419-10）
**复杂度级别：** Level 2-3（研究/调查 + 小型性能补丁）
**状态：** ✅ 已完成
**分支：** `feature/TASK-20260424-01-layout-knee-root-cause`（7 commits）

---

## 1. 任务概述

### 1.1 背景

TASK-05（Layout+Render Bench 入仓）与 TASK-09（Replay 深度 Bench）联动发现 K2/K3 问题：
- **K2：** `BM_LayoutBuildTreeFlat/128→256` 出现 **9.67× for 2×N** 超线性跳变，throughput 从 16.2M/s 骤降到 3.4M/s
- **K3：** `BM_LayoutFlex<16,16>` 同等规模出现 **16.49×** 跳变，更陡峭

Plan `knee` 性质不明，阻塞 Layout 后续优化（grid、multi-column 等新特性若建立在已 knee 的数据结构上会放大问题）。候选区已作为 **TASK-20260419-10**（P2 触发型）记录多轮。

### 1.2 目标

1. **根因定位**（主）：在 VAN 阶段 6 候选根因中实证定位 knee 归因
2. **修复交付**（条件）：若根因可单点修复（配置调整级别）则同任务落地
3. **回归保护**（必须）：新增 GTest 锁定修复不被回退

### 1.3 最终交付

- ✅ 根因：**(d) ArenaAllocator 4KB block malloc/free churn** 贡献 ~60% knee 强度
- ✅ 修复：`ArenaAllocator` 默认 block_size 4096 → **32768**（Phase 2 scan 定位的 sweet spot）
- ✅ 回归网：`DefaultBlockSizeFitsLargeAllocations` GTest + RED 反向探针验证
- ✅ 残余 ~40% knee 拆为 **TASK-20260424-02**（per-phase BM 调查，P3 触发型）

---

## 2. 技术方案

### 2.1 用户决策（Plan 头脑风暴产出）

| # | 维度 | 选择 | 理由 |
|:-:|---|---|---|
| D1 | 实验策略 | **A — 阶梯验证** | 先测最可能 (d) malloc churn（实验成本 < 5 min），命中即止步；否则升 Phase 1B per-phase 拆分 |
| D2 | 修复交付范围 | **A — 全局 ArenaAllocator 默认 bump** | grep 穷举 4 处使用点：仅 `Document::arena_` 使用默认构造受影响（+28KB/instance 可接受） |

**决策驱动**：D1 阶梯验证节省 ~50% 调查时间（Phase 1B 零触发）；D2 全局 bump 保持 API 最小接触面。

### 2.2 设计与计划文档

- **设计：** `docs/specs/2026-04-24-layout-knee-root-cause-design.md`（4 章节 / 273 行）
  - §1 现象与已有证据
  - §2 VAN 阶段推翻的候选
  - §3 研究策略（阶梯验证 + 升级路径）
  - §4 文件变更清单
- **计划：** `docs/plans/2026-04-24-layout-knee-root-cause.md`（6 Phase 骨架 / 613 行）
  - 115 min plan / plan × 0.6 预期 ~70 min / 5 commits 含 Phase 1B 升级分支

---

## 3. 实现摘要

### 3.1 根因定位过程

**Phase 1 单点探针**（block=65536 临时）：
- R256：9.42× → **3.61×**
- R_flex：16.49× → **8.36×**
- 落在 plan 2.5×-6× 中间区间 → **(d) malloc churn 部分确认**（贡献 ~60% 改善），进 Phase 2 精确扫描

**Phase 2 5 档扫描**（4K / 8K / 16K / 32K / 65K）：

| block_size | R256 | R_flex | 备注 |
|:-:|:-:|:-:|---|
| 4096 | 9.42× | 16.49× | 基线 |
| 8192 | 4.68× | 10.54× | |
| 16384 | 4.05× | 8.10× | |
| **32768** | **3.84×** | **7.40×** | ← Flex sweet spot |
| 65536 | 3.61× | 8.36× | **R_flex 回弹**（K8 新发现） |

**K8 新发现**：65K block > L1D (48KB) 触发 cache 抖动，Flex 布局（计算密集访问）放大该效应。Arena 设计守则：**block_size ≤ L1D cache size**。

**用户决策**：ship 32K 默认（Flex sweet spot），残余 ~40% super-linear 立 TASK-20260424-02 调查 (e)/(f)。

### 3.2 文件变更

| 操作 | 文件路径 | 说明 |
|------|---------|------|
| 修改 | `veloxa/foundation/memory/arena_allocator.h` | 默认 block_size 4096 → 32768（+20 行注释含完整扫描表 + 根因追溯 + K8 L1D 守则） |
| 修改 | `tests/foundation/memory/arena_allocator_test.cc` | +23 行：新增 `DefaultBlockSizeFitsLargeAllocations` GTest（指针连续性测试锁定默认 ≥ 32768） |
| 修改 | `benchmarks/baseline/bench_layout_buildtree.json` | 3-reps mean baseline 刷新（959 插 / 改） |
| 修改 | `benchmarks/baseline/bench_layout_flex.json` | 3-reps mean baseline 刷新（422 插 / 改） |
| 修改 | `benchmarks/baseline/README.md` | 环境表 + 变更历史行 + K2/K3 resolved 段 + K8 新发现段 |
| 修改 | `memory-bank/techContext.md` | Layout 性能基线段补 K2/K3 resolved + K8 新发现 |
| 创建 | `docs/specs/2026-04-24-layout-knee-root-cause-design.md` | VAN + Plan 设计文档（273 行） |
| 创建 | `docs/plans/2026-04-24-layout-knee-root-cause.md` | 6 Phase 实现计划（613 行） |
| 创建 | `memory-bank/reflection/reflection-TASK-20260424-01.md` | Level 2-3 详细回顾（287 行） |
| 修改 | `memory-bank/{tasks, activeContext, progress, systemPatterns}.md` | Memory Bank 状态跟踪 + 5 新模式沉淀 |

### 3.3 关键决策

1. **D1 阶梯验证优先于完整 per-phase BM**：Phase 1 小实验先行，若 (d) 主导则直接修复，否则才升级到 Phase 1B 的全新 bench binary。最终 Phase 1B 零触发，节省 ~50% 调查预算。

2. **D2 全局默认 bump 而非 LayoutEngine 专用 allocator**：grep 确认 4 处 `ArenaAllocator` 构造点（`Document`, `UpdateManager`, `LayoutEngine`, bench），仅 `Document::arena_` 使用默认构造。改默认值影响面可控，优于引入第二套 allocator 或专用路径。

3. **Phase 2 选 32K 而非 65K 虽然 R256 略优**：**双指标交叉验证**发现 65K R_flex 回弹（K8 L1D 抖动）。单看 R256 会误选 65K，单指标扫描易错 → 沉淀为扫描型研究任务新模式（`systemPatterns.md`）。

4. **测试设计「公开行为锚定内部约束」**：`DefaultBlockSizeFitsLargeAllocations` 用**指针连续性**（`p2 - p1 == 16384`）间接观测 block 容量，不扩 `block_size()` getter / 不加 `friend` / 不 `#define private public`。沉淀为新模式。

5. **Plan 阈值实证调整（非达标）**：原 plan R256≤2.5 / R_flex≤5 刚性阈值基于「(d) 是唯一根因」假设；实测证明 (d) 仅贡献 ~60%，plan 阈值过严。通过 Phase 2 AskQuestion 承接用户确认接受当前最优（4.18× / 6.40×），剩余 ~40% 立 TASK-24-02。

### 3.4 安全决策

**本任务不涉及安全变更。** 性能优化任务：无外部输入 / 无认证授权 / 无新依赖引入 / 无敏感数据处理 / 无错误响应变化。

---

## 4. 测试覆盖

### 4.1 新增测试

```
tests/foundation/memory/arena_allocator_test.cc:
  TEST(ArenaAllocatorTest, DefaultBlockSizeFitsLargeAllocations)
```

- **约束**：default block_size ≥ 32768
- **观测**：2× 16KB 分配的指针距离（`p2 - p1 == 16384`）
- **RED 反向探针**：revert 默认到 4096 → FAIL（`p2 - p1 = 16448 ≠ 16384`，因为第 2 次分配跨到新 malloc 块）；restore → PASS
- **运行验证**：`ctest -R DefaultBlockSizeFitsLargeAllocations` ✅ PASS

### 4.2 全量回归

- `ctest -j` **892/892 PASS**（新增 1 测试后总数 +1）
- `cmake --build build-bench` Release `-Werror` 全量 rebuild：**0 errors / 0 warnings**
- `bench_layout_{buildtree,flex}` 3-reps 输出稳定（cv < 5%），baseline JSON 入仓

---

## 5. 关键 Before/After 数据

**正式 baseline（3-reps mean，`library_build_type=release`，同机）：**

| BM | 修复前 | 修复后 | 改善 |
|---|---:|---:|---|
| `BM_LayoutBuildTreeFlat/128` | 7.7 µs | 10.1 µs | +31%（稍慢，不影响 knee 判据）|
| `BM_LayoutBuildTreeFlat/256` | 70 µs | **42.3 µs** | **1.66×** |
| `BM_LayoutBuildTreeFlat/512` | 196 µs | 140 µs | 1.40× |
| **R256**（knee 强度，越接近 2 越好）| **9.42×** | **4.18×** | **2.25× ↓** |
| `BM_LayoutFlex<16,16>` | 82.5 µs | **44.2 µs** | **1.87×** |
| **R_flex** | **16.49×** | **6.40×** | **2.58× ↓** |

**内存成本：** `Document::arena_` 每 instance +28 KB（默认 block 4096 → 32768）。嵌入式场景每 view 一个 Document 可接受；TASK-24-02 若观察到多 Document 并存（iframe）放大，届时评估。

---

## 6. 经验教训（从回顾提取）

### 6.1 做得好

1. **阶梯验证策略首次实战成功**：D1 设计正确、Phase 1B 零触发、节省 ~50% 调查时间
2. **脚本化实验**：Phase 2 `for SIZE in ...; do sed + build + run + aggregate` 40 秒扫 3 档 + python3 聚合 markdown 表，零手工粘贴
3. **RED 反向探针第 2 次成熟实践**（TASK-11 首发）：模式已稳定可作为 Mixed TDD 标配
4. **plan × 0.6 第 5 数据点 0.29× 历史最快档**：「最窄路径任务」新子档建立
5. **双指标交叉验证**：抓住 K8 L1D 抖动副信号（若只看 R256 会误选 65K 漏掉 L1D 约束）

### 6.2 挑战

1. **plan 阶段验收阈值凭直觉设**：R256≤2.5 / R_flex≤5 过严，实测 (d) 仅贡献 ~60% 无法单点达标。需引入 AskQuestion 承接
2. **GTest binary 路径未预查**：plan Phase 3 写成 `build/tests/foundation/memory/arena_allocator_test`（源码路径），实际 `build/tests/arena_allocator_test`（扁平化输出）；exit 127 后 find 定位 30 秒
3. **65K block Flex 回弹**意外信号 — 通过双指标交叉验证正确识别为 L1D 约束而非噪声

### 6.3 经验沉淀（改进建议）

| # | 建议 | 优先级 | 落实 |
|:-:|---|:-:|---|
| 1 | 研究/实验型任务 plan §验收标准段区分「预期带 / 下限带 / 中间区间 AskQuestion」| **P1** | 已入 `activeContext.md` 待处理事项，下次类似任务前落实到 `writing-plans.mdc` |
| 2 | plan §0 smoke 矩阵扩展到 unit test binary 路径 | **P1** | 已入 `activeContext.md` 待处理事项 |
| 3 | 扫描型研究任务脚本化模板 + 双指标交叉验证 | **P2** | ✅ 已沉淀 `systemPatterns.md` |
| 4 | 「最窄路径」plan×0.3 子档 + TASK-24-01 0.29× 数据点 | **P2** | ✅ 已沉淀 `systemPatterns.md` |
| 5 | 公开行为锚定内部约束测试设计模式 | **P2** | ✅ 已沉淀 `systemPatterns.md` |

**0 条 P0 立即项**（无影响当前工作流正确性的问题）。

### 6.4 反复模式检查

7 已知模式**无本次重复**（TDD 严格度 11+ / 子代理返工 7+ / 计划一致性 9+ / 提交粒度 7+ 等全部避开）。新变种「plan 验收阈值凭直觉凿实 + 部分达标无预设处理方案」记为观察项，若再现即升 P1。

---

## 7. Phase 耗时对比（plan × 0.6 协议）

| Phase | plan (min) | 实测 (min) | 比例 | 备注 |
|:-:|:-:|:-:|:-:|---|
| 0 基线+分支 | 10 | ~3 | 0.30× | smoke 工具矩阵一次 grep 完成 |
| 1 假设 (d) 探针 | 20 | ~4 | 0.20× | 单 sed + build + run 流水线 |
| 2 block-size 扫描 | 25 | ~5 | 0.20× | for 循环脚本化 3 档 |
| 3 RED→GREEN+反向探针 | 25 | ~8 | 0.32× | 含指针连续性测试设计时间 |
| 4 baseline 刷新 | 20 | ~5 | 0.25× | --repetitions=3 自动跑 |
| 5 MB 收尾 | 15 | ~8 | 0.53× | 文档写作占主 |
| **合计** | **115** | **~33** | **0.29×** | **历史最快档** |

**跨 5 任务 plan × 0.6 校准：**

| 任务 | 类型 | 倍率 | 路径宽度 |
|------|------|:-:|---|
| TASK-05 | bench | 3.4× | 全新 |
| TASK-09 | bench | 4.2× | 全新 |
| TASK-11 | bench | 1.5-2.0× | 中等 |
| TASK-13 | 文档 | 1.67-1.86× | 中等 |
| **TASK-24-01** | 研究+小补丁 | **0.29×** | **最窄** |

---

## 8. Commits（7 个）

| # | SHA | Subject |
|:-:|---|---|
| 1 | `a46ff81` | `docs(van+plan): TASK-20260424-01 layout knee root-cause design + plan` |
| 2 | `782d22d` | `wip(TASK-20260424-01): red — lock ArenaAllocator default block >= 32768` |
| 3 | `0ad275d` | `fix(foundation/memory): bump ArenaAllocator default block 4096 → 32768 (TASK-20260424-01)` |
| 4 | `102c7e5` | `docs(bench): refresh layout baselines after TASK-20260424-01 knee fix` |
| 5 | `d93ef2a` | `chore(build): finalize TASK-20260424-01 memory bank state` |
| 6 | `7dbe08b` | `docs(reflect): add reflection for TASK-20260424-01` |
| 7 | _(本归档) `docs(archive): add archive for TASK-20260424-01`_ |

---

## 9. 后续任务

**TASK-20260424-02（Phase 1B 升级路径正式立项）**：Layout 残余 super-linear 调查（per-phase 拆分 BM）
- **触发条件**：下次 layout 性能需求（grid / multi-column）或主动预算
- **优先级**：P3 触发型
- **方向**：创建 `benchmarks/bench_layout_phases.cc`，将 `LayoutEngine::Layout` 拆成 BuildTree / LayoutBlock / ApplyPositioning 三阶段独立 bench
- **目标**：定位剩余 ~40% super-linear 所在阶段 → 指向 (e) L1D 抖动或 (f) 隐藏算法因素

---

## 10. 参考文档

- **设计规格：** `docs/specs/2026-04-24-layout-knee-root-cause-design.md`
- **实现计划：** `docs/plans/2026-04-24-layout-knee-root-cause.md`
- **回顾文档：** `memory-bank/reflection/reflection-TASK-20260424-01.md`
- **新模式沉淀：** `memory-bank/systemPatterns.md`
  - 「扫描型研究任务脚本化模板 + 双指标交叉验证模式」
  - 「测试设计 — 公开行为锚定内部约束模式」
  - 「bench 估时校准」段 TASK-24-01 最窄路径子档补充
- **技术上下文：** `memory-bank/techContext.md` Layout 性能基线段（K2/K3 resolved + K8 新发现）
- **Baseline 文档：** `benchmarks/baseline/README.md` 2026-04-24 变更历史 + K2/K3/K8 段
