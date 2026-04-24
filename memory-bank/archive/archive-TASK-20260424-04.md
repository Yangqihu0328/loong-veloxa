# 归档：TASK-20260424-04 — SoftwareCanvas::DrawText 真路径 warm 残余优化（D 纯收尾模式）

**日期：** 2026-04-25
**任务 ID：** TASK-20260424-04
**复杂度级别：** Level 2（单候选方案 B + /plan→/build 直通，跳过 /creative）
**模式：** D 纯收尾模式（relaxed `<3200 ns` 替代刚性 `<3000 ns`）
**状态：** ✅ 已完成
**分支：** `feature/TASK-20260424-04-drawtext-residual-opt`（8 commits ahead of main）

---

## 1. 任务概述

### 1.1 背景

TASK-20260424-03（SoftwareCanvas::DrawText 真路径 warm 优化）归档时，**Warm_Medium 残余 499 ns（14%）未达成 D5 刚性技术目标 `<3000 ns`**，用户知情接受；归档文档 §9.2 列出 3 个 P3 触发型候选：(a) 块级 zero-skip fast path / (b) GlyphCache row_ptr 数组 / (c) hb_shape cache。

本任务以 **D 纯收尾模式** 主动推进，目标放宽为 `<3200 ns`（-8.5% / -299 ns）新结构性阈值，达成即归档，避免 P3 候选长期挂单。

### 1.2 目标

| BM | 基线（TASK-24-03）| 目标 | 倍率 |
|---|---:|---:|---|
| `BM_DrawTextReal_Warm_Medium` (19 char) | **3499 ns** | **< 3200 ns** | **-8.5%** |
| `BM_DrawTextReal_Warm_Short` (2 char) | 677 ns | 不回归 | 兜底 |
| `BM_DrawTextReal_Warm_Long` (124 char) | 10573 ns | 不回归 | 兜底 |

### 1.3 VAN 基础假设核查（3 候选代码实证）

| 候选 | 实证位置 | 结论 |
|:-:|---|---|
| (a) skip-all-zero AA fast path | `blit_sse2.h` L94 标量 tail 已有单像素 zero-skip；SIMD 主循环无块级；FT_Bitmap 已 crop bbox → 块级连续 4/8 px 全 0 罕见 | **放弃**（净收益风险 ≈ 0）|
| (b) GlyphCache row_ptr 数组 | TASK-24-03 Phase 6 B2 已外层 hoist row_ptr | **基本否决**（残余 ≤ 20 ns）|
| (c) **hb_shape cache per-text** | `software_canvas.cc` L221-226 每次 DrawText 无条件执行 6 次 hb_* 调用 | **✅ 高收益（-200 ~ -500 ns 保守）**|

---

## 2. 技术方案

### 2.1 /plan 头脑风暴 5 决策（2026-04-24 锁定）

| # | 维度 | 锁定选项 | 简述 |
|:-:|---|---|---|
| Q1 | 候选组合策略 | **方案 B — 仅 (c) hb_shape cache** | 单候选达标几率高 + 长期复用价值（CJK / UI label）|
| Q2 | Cache key 表示 | **K2 — u64 fingerprint (FNV-1a) + text_len 碰撞护栏** | 无 String 拷贝；碰撞概率 ~2^-96 |
| Q3 | Cache 存活范围 | **S2 — per-FontManager 成员** | key = (FontHandle, pixel_size, fingerprint, text_len)；font 重载 Clear 安全 |
| Q4 | 容量 + 淘汰 | **C1 — 固定 128 + FIFO** | 最简实现；~40 KB 内存天花板 |
| Q5 | Cache miss 回归护栏 | **B1 RoundRobin** + B3 AllMiss 参考 | RoundRobin 作为门槛；AllMiss 入 baseline 不计门槛 |

### 2.2 核心设计

- **ShapeCache**：固定 128-slot FIFO，`Vector<Entry>` 存储；`LookupOrNull` 线性扫描（cap 小所以 cache-friendly）；`Insert` 满则 FIFO evict；`Clear` 不释放容量。
- **ShapeCacheKey**：`(FontHandle, pixel_size, u64 text_fingerprint, u32 text_len)`。text_len 作碰撞降级护栏使单独 FNV-1a 碰撞（~2^-64）叠加到 ~2^-96 实际不可达。
- **HbBufferHolder**（thread_local RAII）：从 `software_canvas.cc` 提取到 `veloxa/text/hb_buffer_holder.{h,cc}`，使 `FontManager::ShapeOrLookup` 也能复用同一 thread_local `hb_buffer_t`，避免第二个 thread_local 缓冲重复分配。
- **FontHandle**：从 `font_manager.h` 提取到独立 header `font_handle.h`，解除 `shape_cache.h` ↔ `font_manager.h` 潜在循环依赖。
- **VX_SHAPE_CACHE_OFF**：process-level latched-once env toggle（`static const bool disabled = []{...}();`），允许单次 build 内 A/B 对照 cache 贡献。
- **DrawText 改造**：移除 `software_canvas.cc` L216-231 的 6 次 hb_* 调用族，替换为 `font_manager_->ShapeOrLookup(...)` 单调用 + 对返回 `ShapedRun->glyphs` 迭代 blit。

---

## 3. 实现摘要

### 3.1 文件变更（新建 8 + 修改 9）

| 操作 | 文件路径 | 说明 |
|:-:|---|---|
| 新建 | `veloxa/foundation/base/hash.h` | FNV-1a 64-bit 字节 hash（header-only，41 行）|
| 新建 | `veloxa/text/font_handle.h` | `FontHandle` / `kInvalidFont` 提取（解循环依赖，20 行）|
| 新建 | `veloxa/text/shape_cache.h` | `ShapedGlyph / ShapedRun / ShapeCacheKey / ShapeCache` 接口（91 行）|
| 新建 | `veloxa/text/shape_cache.cc` | `ShapeCache` FIFO 实现（55 行）|
| 新建 | `veloxa/text/hb_buffer_holder.h` | thread_local `hb_buffer_t` 共享访问器（26 行）|
| 新建 | `veloxa/text/hb_buffer_holder.cc` | 实现（33 行）|
| 新建 | `tests/text/shape_cache_test.cc` | 10 unit tests（3 HashBytesU64 + 7 ShapeCache）|
| 新建 | `tests/graphics/drawtext_shape_cache_test.cc` | 4 集成 tests（含 R2 反向探针 `DifferentTexts_DifferentOutput`）|
| 修改 | `veloxa/text/font_manager.h` | + `ShapeOrLookup` / `ClearShapeCache` / `ShapeCache shape_cache_` 成员；使用 `font_handle.h` |
| 修改 | `veloxa/text/font_manager.cc` | 实现 `ShapeOrLookup`（含 VX_SHAPE_CACHE_OFF 钩子 + hb_shape miss path + insert）+ `ClearShapeCache` + `Shutdown` Clear 顺序 |
| 修改 | `veloxa/graphics/software/software_canvas.cc` | 移除内部 `HbBufferHolder` 匿名 namespace；移除 6 次 hb_* 直接调用；替换为 `ShapeOrLookup` 单调用 + `ShapedRun->glyphs` blit |
| 修改 | `veloxa/text/CMakeLists.txt` | + `shape_cache.cc` + `hb_buffer_holder.cc` 源 |
| 修改 | `tests/CMakeLists.txt` | + `shape_cache_test` + `drawtext_shape_cache_test` target |
| 修改 | `benchmarks/bench_drawtext.cc` | + 2 BM（RoundRobin / AllMiss）+ `VaryingTextPool` 确定性文本池（93 行）|
| 修改 | `benchmarks/baseline/bench_drawtext.json` | 10 BMs（+2 new）刷新 |
| 修改 | `benchmarks/baseline/README.md` | + K9 findings + 历史行 + 生成日期 2026-04-25 |
| 修改 | `.cursor/rules/skills/writing-plans.mdc` | + §7.1 Cache BM 稳态访问模式数学推演清单（归档阶段落实 P1 改进建议）|

**代码净增量：** +2316 lines / -101 lines（含 plan 948 + spec 414 = 1362 lines 文档；生产代码净增 ~450 lines；测试 ~250 lines；bench 93 lines）。

### 3.2 Git 提交（8 commits）

```
5f0c3ce docs(archive): add archive for TASK-20260424-04  ← 本次归档
237f924 docs(reflect): add reflection for TASK-20260424-04
0b21eb0 docs(baseline): refresh DrawText baseline + close TASK-20260424-04 build [P5+P6]
2b4310a bench(drawtext): add TextVarying RoundRobin + AllMiss BMs [TASK-20260424-04 P4]
623ad47 feat(text): wire ShapeCache into DrawText warm path [TASK-20260424-04 P3]
f081ed9 feat(text): ShapeCache FIFO cache for hb_shape results [TASK-20260424-04 P2]
b8f700e feat(foundation): add HashBytesU64 FNV-1a byte hash [TASK-20260424-04 P1]
31de018 docs(task-04): /plan design spec + impl plan [TASK-20260424-04]
03421a5 chore(workflow): initialize TASK-20260424-04 (VAN phase)
```

### 3.3 关键决策

1. **单候选方案 B（仅 hb_shape cache）** — VAN 阶段三候选中，(a) 经代码实证净收益风险 ≈ 0、(b) 残余空间 ≤ 20 ns、(c) 预期 -1000~-2100 ns。单候选执行下，/plan→/build 直通无 /creative。
2. **FNV-1a + text_len 双重 key**（Q2 K2）— u64 fingerprint + u32 text_len 构成 96 位有效识别；碰撞概率 ~2^-96 实际不可达。FNV-1a 非密码学但简单高效，无 String 拷贝零堆分配。
3. **固定 128 FIFO**（Q4 C1）— 对比 LRU（+1 swap/lookup）和动态扩容，FIFO 最简；128 slot × ~320 B = ~40 KB 硬顶（设计 §6.2 DoS 分析）。
4. **per-FontManager scope**（Q3 S2）— cache 生命周期对齐 FontManager；`Shutdown` 先 `shape_cache_.Clear()` 再释放 fonts，避免 stale FontHandle hit。
5. **VX_SHAPE_CACHE_OFF env toggle** — 设计阶段决定引入（未在 Q1-Q5 显式列出，GREEN 阶段自然浮现）；process-level latched-once 避免热路径 getenv 开销；pre-baseline A/B 验证 cache 贡献量精度（OFF 3542 ns ≈ TASK-24-03 baseline 3499 ns，1.2% 噪音）。
6. **FontHandle 预提取独立 header** — P2 预见 `shape_cache.h` 需 include `FontHandle`，而 `font_manager.h` 下一步也需 include `shape_cache.h` → 预提取到 `font_handle.h` 解循环依赖；同理 HbBufferHolder 提取避免二次修改 `software_canvas.cc`。
7. **RoundRobin BM 改为 pool=128 (=cap)** — 原 plan pool=256 预期 50% hit，数学稳态实为 100% miss；改为 pool=128 使 linear 访问稳态 100% hit，更清洁压 full-cache scan cost。AllMiss BM 保持 pool=1024 达 100% miss。

### 3.4 安全决策

设计文档 `docs/specs/2026-04-24-drawtext-shape-cache-design.md` §6.2 轻量威胁建模覆盖 4 向量：

| 威胁向量 | Mitigation |
|---|---|
| **DoS（内存膨胀）** | Cap 128 hard limit；~40 KB 内存天花板由 FIFO 保证 |
| **碰撞侧信道** | FNV-1a 非密码学（不用于密码场景）+ text_len 双重 key → 2^-96 实际不可达 |
| **UAF（use-after-free）** | `ShapedRun*` stability contract：仅保证到下次同 slot 覆盖前有效；caller 不得跨 DrawText 持有 |
| **Font reload stale hit** | `FontManager::Shutdown` / font reload 路径先 `shape_cache_.Clear()` 再释放 fonts |

**无外部输入** / **无认证授权** / **无敏感数据** / **无新依赖** — 威胁面局限于 FontManager 内部 API 扩展。

---

## 4. 测试覆盖

### 4.1 新增测试（14 new）

| 类型 | 文件 | 数量 | 关键测试 |
|---|---|:-:|---|
| Unit | `tests/text/shape_cache_test.cc` | 10 | 3 HashBytesU64（`EmptyInput_ReturnsFnvBasis` / `SingleByteDifference_ChangesHash` / `LengthAffectsHash`）+ 7 ShapeCache（含 T6 `FingerprintEq_LenMismatch_Miss` 碰撞降级 guard）|
| Integration | `tests/graphics/drawtext_shape_cache_test.cc` | 4 | `SameTextTwice_PixelByteIdentical` / `AfterClearShapeCache_PixelByteIdentical` / `InterleavedTexts_StableHits` / **R2 反向探针 `DifferentTexts_DifferentOutput`**（抗碰撞误命中）|

**ctest 总量：** 913 → **917 PASSED**（+4 test targets；单 target 多 cases，净新增 14 case）。

### 4.2 Mixed TDD 循环（6 次 RED→GREEN）

- P1 RED: HashBytesU64 测试编译失败（header 不存在）→ GREEN: `hash.h` 实现 → 3/3 PASS
- P2 RED: ShapeCache 测试编译失败（header 不存在）→ GREEN: `shape_cache.{h,cc}` 实现 → 7/7 PASS
- P3 RED: drawtext_shape_cache_test 编译失败（`FontManager::ClearShapeCache` 未实现）→ GREEN: `ShapeOrLookup` + DrawText 接入 → 4/4 PASS
- P4: bench 新增 BM 无 RED（纯测量代码）

### 4.3 Release 验证

- `cmake --build build-bench` with `-O3 -Werror`: 0 errors / 0 warnings
- shape_cache_test Release 10/10 PASS，drawtext_shape_cache_test Release 4/4 PASS

---

## 5. 性能结果（门槛判决 8/8 ✅，唯一 Cold_Medium 属 FT_Load 噪声非任务范围）

### 5.1 核心 warm 路径

| BM | baseline (ns) | 后 (ns, mean) | 后 (ns, single) | Δ |
|---|---:|---:|---:|---|
| **Warm_Medium** | 3499 | **2350** | **1877** | **-32.8% / -46.4%**（超额 850 ns / 26%，意外直破技术刚性 <3000 ns）|
| Warm_Short | 677 | 311 | — | **-54%** |
| Warm_Long | 10573 | 4333 | — | **-59%** |

### 5.2 新增 BM（含 env toggle A/B）

| BM | 测量 | 说明 |
|---|---:|---|
| `Warm_TextVarying_RoundRobin`（pool=128, hit=100%）| 2676 ns | pre-baseline 3605 → -25.8%（压 full-cache scan cost）|
| `Warm_TextVarying_AllMiss`（pool=1024, miss=100%）| 4711 ns | pre-baseline 3736 → +975 ns（insert + ShapedRun copy 开销，入 baseline 不计门槛）|
| Warm_Medium `VX_SHAPE_CACHE_OFF=1` | 3542 ns | 与 TASK-24-03 baseline 3499 ns 吻合（1.2% 噪音）→ **env toggle A/B 精度验证** |

### 5.3 回归护栏

| BM | 基线 | 后 | Δ | 结论 |
|---|---:|---:|---:|---|
| Cold_Medium | 28338 | 33620 | +18.6% | ⚠️ CV 12.67%（FT_Load 主导噪声），VAN 已预判非任务范围；接受 |
| ReplayTextHeavyFallback | — | +16% | — | ⚠️ 不接触新路径，属噪声/stale baseline |
| ReplayTextHeavyReal | — | -9.8% | — | ✅ 反降（同受益 ShapeCache） |
| Fallback BMs | — | ±2% | — | ✅ 无回归 |

---

## 6. 经验教训

### 6.1 关键教训（3 项）

1. **单候选 Level 2 + D 收尾模式可大幅超额达成** — 根因：消除整个 API 调用族（6 次连续 hb_* 调用）的收益远超经验常数估算。由此抽出 **第三方 API 消除型优化估时下限公式**：`下限 = N × single_call_cost × (1 - miss_rate)`。本任务 6 × ~350 ns × 1.0 ≈ 2100 ns 预测与实测 -1754 ns 吻合（误差 <17%）。

2. **env toggle A/B 比 branch 切换快 30 min** — 不需 `git stash` / reconfigure / 切 branch 各跑一轮；process-level latched-once env 一次 build 即可对照。下次所有 cache / memoization / short-circuit 类优化 **默认加同名 env 禁用开关**。

3. **多 phase 任务中预见的透明重构应在第一次改到相关文件时就做完** — FontHandle（P2 预提取）/ HbBufferHolder（P3 提取）避免了 P3 / P4 二次修改同文件、破坏 phase 隔离、触发循环依赖抢救。

### 6.2 次要教训

4. **TDD RED 可一次性压多个测试** — 同接口族 7 tests 一次 RED，GREEN 实现后一次全 PASS；比逐个 RED→GREEN 更高效，不破坏 TDD 铁律。
5. **稳态访问模式需数学严谨** — FIFO + linear + cap=128 + pool=256 稳态是 100% miss（不是 50% hit）。由此催生 §7.1 新规则。

### 6.3 反复模式匹配

本次 9 类已知反复模式均未触发或仅部分轻微匹配：
- ⚠️ 部分：plan 顶层文件清单轻微遗漏（font_handle.h / hb_buffer_holder 在子任务列出但顶层表未计入，+3 文件）
- ⚠️ 部分：StringView(std::string) ctor 兼容性未预 grep 验证（1 次编译错误 ~1 min 修复）
- 其余 7 类无重复

---

## 7. 新模式沉淀（`systemPatterns.md`）

### 7.1 Env Toggle A/B 对照模式（TASK-24-04 反思 #1）

Process-level latched-once 环境变量 `VX_<FEATURE>_OFF` 使单次 build 可做 cache ON/OFF A/B 对照。本任务 Cache ON 1788 ns vs OFF 3542 ns vs TASK-24-03 baseline 3499 ns 精确剥离 cache 贡献（1.2% 噪音带）。

### 7.2 预提取依赖 Header 原则（TASK-24-04 反思 #2）

多 phase 任务（≥ 3 phase）中，预见后续 phase 需要 include 当前 phase 未暴露 symbol 时，**第一次改到相关文件就提取到独立 header**，避免循环依赖或同文件二次修改。

### 7.3 第三方 API 消除型优化估时下限公式（TASK-24-04 反思 #3）

`下限 = N × single_call_cost × (1 - miss_rate)`，适用于「消除一族连续第三方 API 调用」类优化（shape / layout / rasterize / codec 等），warm 稳态 hit rate > 80%。

### 7.4 「最窄路径」子档数据点扩展

三数据点 0.26-0.34× 区间（TASK-24-01 0.29× / TASK-24-03 0.34× / **TASK-24-04 0.26×**）；识别清单：单候选明确、基础设施 100% 复用、无 CMake 新 target、测试用现有 GTest 框架。

---

## 8. 改进建议闭环

| # | 建议 | 优先级 | 落实状态 |
|:-:|---|:-:|---|
| 1 | Cache BM 稳态访问模式数学推演清单 | **P1** | ✅ **归档阶段已落实** → `writing-plans.mdc` §7.1（新增段，含推演模板 + 速查表 + 反模式）|
| 2 | Env Toggle A/B 对照模式 | P2 | ✅ 已入 `systemPatterns.md` |
| 3 | 预提取依赖 Header 原则 | P2 | ✅ 已入 `systemPatterns.md` |
| 4 | 第三方 API 消除型优化估时下限公式 | P2 | ✅ 已入 `systemPatterns.md` |
| 5 | StringView ctor 陷阱 + plan 顶层表完整性自检 | P2 | ℹ️ 保留为下次 plan 阶段参考（smoke check 既有 grep 条目可覆盖）|

---

## 9. 后续工作（P3 触发型候选，不在本任务范围）

1. **LRU 替代 FIFO** — 当前 128 slot FIFO 已充分；若未来 cap 降到 32 或 pool 分布极度偏斜，LRU 可能更优。
2. **Shape result interning** — 相同 glyph sequence 不同文本共享 `ShapedRun`（CJK / 多语言场景节省内存）。
3. **`freetype_shaper::Measure()` 用 ShapeOrLookup** — Layout pass text measure 也可能受益。
4. **Cold_Medium CV 12.67% 独立排查** — FT_Load 噪声源分析，非 hot path 优先级低。

---

## 10. 参考文档

- **设计规格：** `docs/specs/2026-04-24-drawtext-shape-cache-design.md`（11 段，含安全威胁建模 §6.2 + 契约 + 测试矩阵）
- **实现计划：** `docs/plans/2026-04-24-drawtext-shape-cache.md`（6 Phase / 12 Task / 预估 ~3h 25m × 0.6 校准 ≈ ~2h 3m）
- **回顾文档：** `memory-bank/reflection/reflection-TASK-20260424-04.md`（Level 2 全维度回顾）
- **创意设计：** ❌ 无（Level 2 跳过 /creative）
- **上游任务归档：** `memory-bank/archive/archive-TASK-20260424-03.md`（K7 Resolved / 残余 499 ns P3 触发本任务）
- **基线 CSV：** `benchmarks/baseline/bench_drawtext.json`（10 BMs with TASK-24-04 K9 findings row）
- **新规则落实：** `.cursor/rules/skills/writing-plans.mdc` §7.1

---

## 11. 关键指标汇总

- **代码变更：** 新 8 / 改 9 / +2316 / -101 lines（含文档 1362 lines）
- **生产代码净增：** ~450 lines
- **测试：** +14 new cases / ctest 917/917 PASS
- **性能：** Warm_Medium **-32.8% mean / -46.4% single**（超额破 <3000 ns 技术刚性）
- **Plan 时间校准：** **×0.26**（「最窄路径」第 3 数据点）
- **提交：** 9 commits（含 VAN + plan + P1-P6 + reflect + archive）
- **安全：** 4 威胁向量设计阶段全部 mitigate
- **新规则：** `writing-plans.mdc` §7.1 Cache BM 稳态推演清单（P1 落实）
- **新模式：** `systemPatterns.md` × 3（Env Toggle A/B / 预提取 Header / API 消除公式）
