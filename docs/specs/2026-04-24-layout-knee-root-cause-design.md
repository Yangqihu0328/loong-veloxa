# Layout super-linear knee 根因调查 — 设计规格

**任务 ID：** TASK-20260424-01（对应候选区 TASK-20260419-10）
**复杂度级别：** Level 2-3（研究/调查 + 可能的小型性能补丁）
**创建日期：** 2026-04-24
**来源：** TASK-20260419-05 K2（BuildTreeFlat N=128→256 super-linear）+ K3（Flex 16x16 super-linear） + TASK-20260419-09 VAN 否定 ArenaAllocator chunk-grow 假设

---

## 1. 背景与问题陈述

### 1.1 现象

`bench_layout_buildtree` + `bench_layout_flex`（Release，build-bench/）的 `BM_LayoutBuildTreeFlat` 在 N=128→256 出现明显 super-linear knee：

| N | Time (ns) | 比值 vs 上一档 | 吞吐 (M items/s) |
|:-:|---:|:-:|---:|
| 8   |    534 | —     | 15.0 |
| 16  |    982 | 1.84× | 16.3 |
| 32  |   1871 | 1.91× | 17.1 |
| 64  |   3906 | 2.09× | 16.4 |
| 128 |   7901 | 2.02× | 16.2 |
| **256** | **76375** | **9.67×** ← knee | **3.4** |
| 512 | 208027 | 2.72× | 2.5 |

`BM_LayoutFlex`：`<8,8>` 5359 ns → `<16,16>` 92023 ns（4× cells → 17.2× time，同源 super-linear）。

### 1.2 目标

1. **根因定位**（研究产出）：给出经实验证实的 super-linear knee 根因；如无法定位，输出调查报告并升级到跟进任务
2. **性能修复**（如根因可修）：实施一次性修复，让 `BM_LayoutBuildTreeFlat/256` 从 76µs 回到 ≤ 20µs（linear ≤ 2.5× of /128）
3. **回归保护**：添加 bench 验证点 + 文档化，防止日后沉默回退

### 1.3 非目标

- 不做渲染或 CSS 相关 bench 优化（超出范围）
- 不重写 LayoutEngine 算法（若根因指向算法，产出调查报告 → 跟进任务）
- 不引入新第三方依赖

---

## 2. VAN 阶段已完成的根因推翻（落实 P0「方案根因假设未先验证」）

| # | 候选根因 | 推翻依据 | 备注 |
|:-:|---|---|---|
| a | `Vector<LayoutBox*> children` 扩容 | `veloxa/core/layout/layout_box.h:26-29` 用侵入式 `first_child/next_sibling/prev_sibling` 双向链表 | TASK-09 activeContext 候选列表 (a) 错误，此次 grep 纠正 |
| b | `ArenaAllocator` chunk 2× 增长 | `veloxa/foundation/memory/arena_allocator.h:13,39` 默认 4096 固定不变（仅当单次 alloc > default 时按 size 扩大） | TASK-09 VAN 已否定 |
| c | L2 cache 溢出 | N=256 工作集 142KB（= 257 element × 552 B） << L2 1280 KB；N=512 工作集 283KB << L2 | 本次 VAN 确认 |

### 2.1 VAN 保留的活跃候选根因

| # | 候选 | 证据强度 | 验证成本 |
|:-:|---|:-:|:-:|
| d | **Arena 4KB block malloc/free 高频 churn** | ⭐⭐⭐⭐ 直接可测 | 5 min（改一个数字 + rebuild + rerun） |
| e | L1D 抖动（单元素 552B） | ⭐⭐ 间接 | 需硬件计数器或 per-phase 拆分 |
| f | 算法内隐藏 O(N²) | ⭐ 低 | grep 未见，需 per-phase 拆分 |

---

## 3. 研究策略与决策

### 3.1 D1：实验策略 = A（阶梯验证，已用户批准）

**阶梯原则：** 先测最可能 + 验证成本最低的候选（d），若消解 knee 则止步；否则升级测 (e)/(f)。

**触发升级条件：**
- 若 Phase 1 实验（block-size 65536）后 `BM_LayoutBuildTreeFlat/256` ÷ `/128` > **2.5×** → 升级到 Phase 1B（per-phase 拆分 BM 调查）
- 若 ≤ 2.5× → 根因 (d) 确认，进入 Phase 2（最优 block 扫描）

### 3.2 D2：修复交付范围 = A（全局 bump，已用户批准）

**方案：** `ArenaAllocator` 默认 block_size 从 `4096` bump 到实验证实的最优值（Phase 2 输出，预期 16384 或 32768）。

**影响面（grep 实证）：**

| 使用点 | 当前 block size | bump 后（假设 16384） | 内存影响（单实例） |
|---|:-:|:-:|:-:|
| `dom::Document::arena_`（`document.h:32`）默认 | 4096 | 16384 | +12 KB |
| `UpdateManager::arena_`（`update_manager.h:43`）显式 `8192` | 8192 | 8192（不变，显式传参） | 0 |
| `LayoutEngine::Layout()` 静态 `layout_arena(8192)`（`layout_engine.cc:109`）显式 | 8192 | 8192（不变） | 0 |
| `bench_layout_buildtree/flex` 默认 `vx::ArenaAllocator arena;` | 4096 | 16384 | +12 KB |
| `arena_allocator_test` 显式传 64 / 128 / 1024 | unchanged | unchanged | 0 |

**结论：** 实际受影响的**生产路径**只有 `Document::arena_`；其余都是显式 block_size。内存成本可接受（+12 KB × Document 实例数；嵌入式场景通常 1 Document/view，总量 µKB 量级）。

### 3.3 修复不适用场景（若 Phase 1 否定 D）

若 (d) 被否定：
- 输出 `docs/specs/...-findings.md` 调查报告
- 标记 `tasks.md` 为「研究完成，修复跟进」
- 立 **TASK-20260424-02（后续候选）**：承接 (e) 或 (f) 方向的修复

---

## 4. 实验设计

### 4.1 Phase 1：假设 D 验证（单点探针）

**操作：** 修改 `veloxa/foundation/memory/arena_allocator.h:13` 默认 `block_size = 4096` → `65536`（临时，**不 commit**）。

**重 build：** `cmake --build build-bench --target bench_layout_buildtree bench_layout_flex -j`

**重跑：** `./build-bench/benchmarks/bench_layout_buildtree --benchmark_filter=Flat --benchmark_min_time=0.3s --benchmark_format=json`

**判定公式：**
```
R256 = BM_LayoutBuildTreeFlat/256.real_time / BM_LayoutBuildTreeFlat/128.real_time
```

| 判据 | 结论 | 动作 |
|---|---|---|
| R256 ≤ 2.5× | 根因 (d) 确认（malloc churn） | 进 Phase 2 扫描最优值 |
| 2.5× < R256 < 6× | (d) 部分贡献 | 进 Phase 2 扫描；如无法降到 ≤ 2.5× 则启动 Phase 1B |
| R256 ≥ 6× | (d) 否定 | 回滚实验 → 启动 Phase 1B 升级路径 |

### 4.2 Phase 2：block size 扫描（最优值定位）

**前提：** Phase 1 判据命中 `R256 ≤ 2.5×` 或 `< 6×`。

**扫描值：** {4096 (baseline), 8192, 16384, 32768, 65536}

**采集矩阵：** 每个 block_size 取 `Flat/{128, 256, 512}` 三点 + `Flex/<16,16>` 一点 = 4 数据。

**最优选择规则：**
1. **主判据：** 最小 block_size 使得 `Flat/256` 与 `Flat/128` 的比值 ≤ 2.5×（定义「knee 消失」）
2. **次判据：** 同时 `Flex<16,16>` 相对 `<8,8>` 比值 ≤ 5×（定义「Flex 同源问题也缓解」）
3. **备选：** 如两项都未满足，记录最佳近似值，启动 Phase 1B

### 4.3 Phase 1B：升级路径（仅当 Phase 1 否定 D）

**per-phase 拆分 BM：** 给 `LayoutEngine::Layout` 三个内部阶段（BuildTree / LayoutBlock / ApplyPositioning）各加一个调试用 BM（仅 Phase 1B 临时启用，不入 main）：

```cpp
// benchmarks/bench_layout_phases.cc（Phase 1B 创建，不一定合并）
static void BM_BuildTreeOnly(State& state) { ... }
static void BM_BuildTreePlusLayoutBlock(State& state) { ... }
static void BM_FullLayout(State& state) { ... }  // 与原 BM_LayoutBuildTreeFlat 一致
```

**分解公式：**
- LayoutBlock cost = BuildTreePlusLayoutBlock - BuildTreeOnly
- ApplyPositioning cost = FullLayout - BuildTreePlusLayoutBlock

用此三值对 N=128/256 重复采样，定位 super-linear 在哪一阶段。

**硬件兜底：** `perf stat -e cache-misses,page-faults` MISS（WSL2 沙箱）→ 退化用 `/usr/bin/time -v` 看 major page faults 指标代替。

---

## 5. 修复交付设计（假设 Phase 2 确定 16384 为最优）

### 5.1 代码变更

**文件：** `veloxa/foundation/memory/arena_allocator.h`

```cpp
-  explicit ArenaAllocator(usize block_size = 4096)
+  // Default block size bumped 4096 → 16384 (TASK-20260424-01):
+  // 4 KB blocks caused malloc/free churn in hot paths like LayoutEngine,
+  // producing a super-linear knee at N ≈ 200 elements (bench_layout_buildtree).
+  // 16 KB defaults cost +12 KB per Document but remove the knee.
+  explicit ArenaAllocator(usize block_size = 16384)
```

**一行改动**（+ 注释），无其他代码路径修改。

### 5.2 回归保护测试（新增）

**文件：** `tests/foundation/memory/arena_allocator_test.cc`

新增一个 GTest 锁定默认值，避免日后 silent 回退：

```cpp
TEST(ArenaAllocatorTest, DefaultBlockSizeLocked) {
  // TASK-20260424-01: locked to 16384 to avoid layout malloc/free churn.
  // Any change must be coupled with a bench re-run and baseline update.
  ArenaAllocator arena;
  // 分配 1 字节，然后继续分配直到跨 block（间接观测 block 大小）
  arena.Allocate(1);
  // 1 字节 + 对齐 padding ≤ ~16；再分配 (kExpected - 32) 字节应仍在同一块
  constexpr usize kExpected = 16384;
  void* p1 = arena.Allocate(kExpected - 64);
  // 此时 block 应已满，再分配触发新 block（可观测 bytes_allocated 递增但 p2 可能跨块）
  void* p2 = arena.Allocate(1);
  EXPECT_NE(p1, p2);
  // 关键断言：bytes_allocated 反映已分配，间接验证 block 容纳了大请求
  EXPECT_GE(arena.bytes_allocated(), kExpected - 64);
}
```

**RED 反向探针**（per TASK-11 反思 #3 & systemPatterns「Mixed TDD RED 反向探针实践」）：
实施后先把默认改回 4096 → 跑测试应 FAIL（失败原因：单次大分配超过 block size 触发 oversized path，bytes_allocated 语义不同）→ 恢复 16384 → PASS。

### 5.3 Bench baseline 刷新

按 `benchmarks/baseline/README.md`「4-piece 失真兜底协议」：
1. 真实算法/数据结构变更触发 ✅（default block size 属于数据结构层配置）
2. Release 独立 build-bench + `--benchmark_min_time=0.5s` ✅
3. 更新 `benchmarks/baseline/README.md`「当前生成环境」表 4 行
4. JSON 体检 `context.library_build_type == "release"`

更新 2 个 baseline：`bench_layout_buildtree.json` + `bench_layout_flex.json`（knee 消失后的新基线）。

---

## 6. 文件变更清单

| 文件 | 动作 | 职责 |
|---|:-:|---|
| `veloxa/foundation/memory/arena_allocator.h` | 修改 | 默认 block size 4096 → 16384（1 行 + 注释） |
| `tests/foundation/memory/arena_allocator_test.cc` | 追加 | +1 GTest 锁定默认（+ RED 反向探针） |
| `benchmarks/baseline/bench_layout_buildtree.json` | 覆盖 | 新基线，knee 已消 |
| `benchmarks/baseline/bench_layout_flex.json` | 覆盖 | 新基线 |
| `benchmarks/baseline/README.md` | 修改 | 更新「当前生成环境」+ 加本次变更历史行 |
| `memory-bank/techContext.md` | 修改 | 更新「Layout 性能基线」段，补 knee 已解记录；「FetchContent 与代理」段不动 |
| `memory-bank/systemPatterns.md` | **可选** | 如 Phase 1B 被触发，沉淀「Arena 默认 block size 决策模式」段 |
| `docs/specs/2026-04-24-layout-knee-root-cause-design.md` | 本文件 | 设计规格 |
| `docs/plans/2026-04-24-layout-knee-root-cause.md` | 创建 | 实现计划 |

---

## 7. 验收标准

| # | 判据 | 方法 |
|:-:|---|---|
| 1 | Phase 1 判据命中（R256 ≤ 6×），根因 (d) 至少部分成立 | `python3 -c 'import json; ...'` 解析两个 json 文件 |
| 2 | Phase 2 定位到最优 block size（≤ 65536） | 扫描表写入 progress.md |
| 3 | `BM_LayoutBuildTreeFlat/256` 修复后 ≤ `/128` × 2.5 | json 对比 |
| 4 | `BM_LayoutFlex<16,16>` 修复后 ≤ `<8,8>` × 5 | json 对比 |
| 5 | `ctest -j` 全量回归 891/891 PASS | 直接跑 |
| 6 | Release `build-bench/` 全量重 build 0 errors / 0 warnings | `cmake --build build-bench -j` |
| 7 | 新增 `DefaultBlockSizeLocked` GTest 通过 + RED 探针验证有效 | 临时把默认改回验证 FAIL |
| 8 | 2 个 bench baseline JSON 同步刷新（`library_build_type=release`） | jq/python3 校验 |
| 9 | `benchmarks/baseline/README.md` 环境表 + 变更历史更新 | grep 验证 |
| 10 | `techContext.md`「Layout 性能基线」段补 knee-resolved 记录 | grep 验证 |

**附加（若 Phase 1B 触发）：**
- 11'. 调查报告 `docs/specs/...-findings.md` 产出
- 12'. 跟进任务 TASK-20260424-02 在 `tasks.md` 候选区立项

---

## 8. Phase 划分预览（细节见 plan 文档）

| Phase | 名称 | 时间（plan） | 产出 |
|:-:|---|:-:|---|
| 0 | 基线核验 + smoke 工具 + 分支 | 10 min | 分支建立；json MISS 工具记录 |
| 1 | 假设 D 单点探针 | 20 min | R256 数字 + 判据判定 |
| 2 | block size 扫描 + 最优选 | 25 min | 扫描表 + 最优值 |
| 3 | 实施 fix + RED 探针测试 | 25 min | 1 行改动 + 1 GTest |
| 4 | ctest 全量 + Bench baseline 刷新 | 20 min | 891/891 PASS + 2 json + README |
| 5 | techContext + MB 收尾 | 15 min | 文档对齐 |
| **合计 plan** | — | **115 min** | — |
| **plan × 0.6 预期** | — | **~70 min** | — |

**Phase 1B（升级路径）** 若触发：+60-90 min（plan×0.6 ≈ 40-55 min）

---

## 9. 安全相关

❌ 否。性能优化任务，无外部输入、无认证路径、无新依赖、无敏感数据处理。

---

## 10. 风险与缓解

| 风险 | 概率 | 影响 | 缓解 |
|---|:-:|:-:|---|
| Phase 1 否定 (d) | 中 | 高 | 升级 Phase 1B，预算 +40-55 min；若仍无法定位 → 输出调查报告 + 立 TASK-24-02 |
| 16384 默认导致 Document 内存回归 | 低 | 中 | 只有 `Document::arena_` 受影响，+12 KB/instance，嵌入式场景总量 < 100 KB |
| 其他 arena 使用点 silently 受影响 | 极低 | 低 | grep 已穷举 4 处；显式传参的 3 处不受影响 |
| Bench 数据跨机器 noise | 中 | 低 | 用 `--benchmark_min_time=0.5s` + 4-piece 失真协议，只对比同机数据 |
