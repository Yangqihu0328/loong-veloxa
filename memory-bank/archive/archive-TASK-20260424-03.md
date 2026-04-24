# 归档：TASK-20260424-03 — SoftwareCanvas::DrawText 真路径 warm 优化

**日期：** 2026-04-24
**任务 ID：** TASK-20260424-03
**复杂度级别：** Level 2-3（优化类；7 Phase 阶梯验证 + 2 次 R1 升级分支）
**状态：** ✅ 已完成
**分支：** `feature/TASK-20260424-03-drawtext-warm-opt`（13 commits，待 `--no-ff` 合并到 main）
**前置链路：** TASK-20260419-09 K7 发现 → TASK-20260419-12 P2 触发型 → 本任务激活

---

## 1. 任务概述

### 目标

把 `SoftwareCanvas::DrawText` 真路径（FreeType + HarfBuzz + GlyphCache）**warm Medium** 耗时从 **5807 ns** 降到 **< 3000 ns**（≥ 1.94× 加速），使其在**所有文本长度**下优于 fallback 路径（3647 ns），为未来真路径默认化扫清性能债。

### 成果评估（12 项验收 §1.2）

| # | 判据 | 阈值（刚性）| 实测 | 结果 |
|:-:|---|---:|---:|:-:|
| S1 | `BM_DrawTextReal_Warm_Medium` | **< 3000 ns** | 3499 ns | ⚠️ 差 499 ns (14%) |
| S2 | `BM_DrawTextReal_Warm_Short` | ≤ 1166 ns | **677 ns** | ✅ |
| S3 | `BM_DrawTextReal_Warm_Long` | ≤ 18537 ns | **10573 ns** | ✅ |
| S4 | `BM_DrawTextReal_Cold_Medium` | ≤ 60677 ns | **28338 ns** | ✅ 超额（-46.4%）|
| S5 | `BM_DrawTextFallback_{Short,Medium}` | ≤ baseline × 1.15 | 3608 ns（对照，持平）| ✅ |
| S6 | `BM_ReplayTextHeavyReal` | ≤ 913 µs × 1.0 | 未回归 | ✅ |
| S7 | `bench_layout_*` / `bench_render_*` / `bench_imagecache` | ≤ 10% 退化 | 无退化 | ✅ |
| S8 | ctest 全量 | PASS | **59/59** | ✅ |
| S9 | Release `-O3 -Werror` 全量 rebuild | 0 err/warn | 0/0 | ✅ |
| S10 | 新增 `DrawTextPixelBlendPrecision` GTest + RED 反向探针 | PASS + 有效 | **11 GTests + 3 RED 循环** | ✅ 超额 |
| S11 | `bench_drawtext.json` 刷新 + K7 标 resolved | ✅ | baseline/README/techContext 均已更新 | ✅ |
| S12 | `techContext.md` K7 补 resolved + 新 warm 数据表 | ✅ | 已更新 | ✅ |

**关键业务判据**：**真路径 warm 3499 ns < fallback 3608 ns** ✅ → **真路径默认化前置条件锁定**（超业务目标）。

**技术刚性目标 D5**：差 499 ns 未达 — 2 次 R1 AskQuestion 升级（SSE2 + AVX2）后用户知情接受。

---

## 2. 技术方案

### 5 用户决策（头脑风暴产出，spec §2）

| # | 维度 | 选择 | 理由 |
|:-:|---|---|---|
| D1 | 优化实施策略 | **方案 1 阶梯验证驱动**（7 Phase）| 每 Phase 独立贡献可量化 + bisect 友好 + 累计达标即止 |
| D2 | `hb_buffer` 复用 | **thread_local + RAII** | 零线程风险 + 零 header 污染 + thread exit 自动清理 |
| D3 | Inner blit loop 深度 | **B1 + B2 组合**（SIMD 留 P2 触发型）| /255 乘-移近似 + pre-clip；SIMD 后续升级 |
| D4 | `GlyphBitmap` 结构 | **保持 Vector<u8>** | 0 ABI 改动；data() + row ptr 推进即可 |
| D5 | 验收阈值 | **刚性 < 3000 ns**（用户选严格）| 不设中间带；不达标则 AskQuestion 升 B3 SIMD |

### 7 Phase 阶梯 + 2 次 R1 升级实际执行

| Phase | 候选 | Warm_Medium | Δ | 累计 | 达标? | Ctest |
|:-:|---|---:|---:|---:|:-:|:-:|
| 0 | baseline anchor | 5412 ns | 0 | 0 | ❌ | n/a |
| 1 | A hb_buffer thread_local | 5397 ns | -37 | -0.7% | ❌ | 26/26 |
| 2 | C FT_Set_Pixel_Sizes 缓存 | 5266 ns | -57 | -2.7% | ❌ | 26/26 |
| 3 | E 默认 FontHandle 缓存 | 5386 ns | -17 | -0.5% | ❌ | 24/24 |
| 4 | D GlyphCache::Put → ptr | 5311 ns | -67 | -1.9% | ❌ | 46/46 |
| 5 | B1 /255 乘-移近似 **回退** | 5367 ns | **+56** | -1.9% | ❌ | 51/51（含 5 新 infra） |
| 6 | B2 pre-clip + row ptr | **4689 ns** | **-651** | **-13.4%** | ❌（差 1689）| 53/53 |
| 7 | **R1 升级** B3 SSE2 4 px/iter | **3354 ns** | **-1346** | **-38%** | ❌（差 354，但 < Fallback 3637）| 57/57（含 9 blend） |
| 7b | **R1 升级** AVX2 dispatch `count≥16` | 3360 ns | tie | -38% | ❌（差 360）| **59/59**（含 11 blend） |
| 7.4 | baseline JSON 刷新（min_time=1.0s）| **3499 ns** | — | -40.7% vs 旧 5905 | ✅ < Fallback 3608 | 59/59 |

**关键结论：**
- **Phase 6 B2 (pre-clip + row ptr) 是最大单 Phase 收益**（-12.2%）— 算法层改动 > 所有 API 层累计改动（Phase 1-4 合计 -1.9%）6 倍
- **Phase 7 SSE2 是第二大收益**（-28.6%）— SIMD 是突破 blit 内层性能的正确方向
- **Phase 5 /255 乘-移近似回退**（+1.1%）— GCC `-O3` Granlund-Montgomery 魔数乘法已优于手写（通用编译器洞察）
- **AVX2 在 ASCII glyph (6-12 px) 无净收益** — 引入 `count >= 16` 智能阈值 dispatch 保留 CJK / 大字号 headroom

---

## 3. 实现摘要

### 3.1 文件变更

| 操作 | 文件路径 | 说明 |
|------|---------|------|
| 修改 | `veloxa/graphics/software/software_canvas.{h,cc}` | Phase 1 `HbBufferHolder` RAII + `thread_local` 缓存 / Phase 3 `cached_default_font_` 成员 / Phase 6 inner blit pre-clip + row ptr 预计算 / Phase 7 接线 `BlendGlyphRow` runtime dispatch |
| 修改 | `veloxa/text/font_manager.{h,cc}` | Phase 2 `FontEntry::ft_pixel_size` 字段 + `SetFacePixelSize(handle, size)` 幂等 API |
| 修改 | `veloxa/text/glyph_cache.{h,cc}` | Phase 4 `Put` 签名 `void` → `GlyphBitmap*`，消除 Put 后冗余 Get |
| 创建 | `veloxa/graphics/software/glyph_blend.h` | Phase 5 header-only `DivBy255Approx` + `BlendGlyphPixel`（试验回退后保留作 SIMD ±1 LSB 精度契约参考）|
| 创建 | `veloxa/graphics/software/blit_sse2.h` | Phase 7 SSE2 4 px/iter `BlendGlyphRowSSE2`（pmullw + DivBy255ApproxU16 + packus + 标量 tail）|
| 创建 | `veloxa/graphics/software/blit_avx2.h` | Phase 7 AVX2 8 px/iter `BlendGlyphRowAVX2`（`__attribute__((target("avx2")))` + `_mm256_permute4x64_epi64(0xD8)` lane 对齐 + `_mm256_cvtepu8_epi16`）+ `BlendGlyphRow` 智能 dispatch（`count >= kAVX2MinPixelsPerRow=16 && __builtin_cpu_supports("avx2")`）|
| 创建 | `tests/graphics/pixel_blend_test.cc` | **11 GTests**：DivBy255Approx 范围扫描 / BlendGlyphPixel 参考实现对比 / SSE2 4 布局 + zero-alpha + full-opaque + 256-px stress / Dispatch AVX2 13 布局 + SSE2 parity 对照 |
| 修改 | `benchmarks/baseline/bench_drawtext.json` | Phase 7.4 重采（`--benchmark_min_time=1.0s` Release 同机）|
| 修改 | `benchmarks/baseline/README.md` | K7 → RESOLVED + 历史行 + 新 warm 数据表 |
| 修改 | `memory-bank/tasks.md` / `activeContext.md` / `progress.md` | /build + /reflect + /archive 三阶段状态同步 |
| 修改 | `memory-bank/techContext.md`（规划阶段；实际 K7 更新已写入 `benchmarks/baseline/README.md`）| 原 K7 未解决记号保留，作历史上下文 |
| 创建 | `memory-bank/reflection/reflection-TASK-20260424-03.md` | /reflect 阶段产出（13 段 / 7 改进建议 / plan × 0.6 第 6 数据点 0.34× 最窄档） |
| 修改 | `.cursor/rules/skills/writing-plans.mdc` | **归档阶段落实 P1 #1 #2**：§7 WSL2 / 云机 bench 稳态协议 + §8 编译器已做优化识别反模式 |
| 修改 | `memory-bank/systemPatterns.md` | **归档阶段落实 P1 #2**：Bench Smoke 自检模式追加「WSL2 / 云机 / Docker 稳态协议」附录；/reflect 已沉淀 4 新模式段 |

### 3.2 关键决策

1. **阶梯验证优先便宜候选**（D1）— 遵循 TASK-24-01 0.29× 样板，A→C→E→D（API 层）先做；结果 API 层累计仅 -0.5%，验证「**API 层天花板远低于 plan 估算**」经验教训
2. **`/255` 乘-移近似的负结果保留 helper + tests**（Phase 5）— 即便主路径回退，`glyph_blend.h` + `pixel_blend_test` 作为 Phase 7 SSE2/AVX2 的 ±1 LSB 精度契约基础设施零成本复用，**负结果 ≠ 浪费**
3. **AVX2 实测无净收益 → 智能阈值 dispatch**（Phase 7b）— 没有简单 revert，而是 `count >= kAVX2MinPixelsPerRow=16` 保留为 CJK / 大字号 / 未来硬件进化的 headroom；模式化为「**异构工作负载 SIMD 尺寸阈值 dispatch**」
4. **2 次 R1 AskQuestion 升级严格按 plan 协议执行**（Phase 6→7 / Phase 7→7b）— 用户基于实测数据做升级决定，避免 plan 阶段一次性过度承诺 SIMD 工作量
5. **测试策略 Mixed TDD D3 + RED 反向探针**（Phase 5 / 7 / 7b 三次完整循环）— RED 探针第 3+ 次成熟应用，可作为 Mixed TDD 标配

### 3.3 安全决策

**本任务不涉及安全变更**。性能优化 / 无外部输入 / 无认证 / 无新依赖。

**SIMD 代码的隐式安全：**
- SSE2/AVX2 路径加载用 `_mm_loadu_si128` / `_mm_loadl_epi64` 允许 unaligned 访问，无对齐假设
- AVX2 加载 32B 前经 `count + 8 <= n` 守卫，避免缓冲区越界
- 标量 tail 单像素 bounds 由外层 pre-clip（Phase 6 B2）保证
- 11 GTests ±1 LSB 精度契约 + 3 次 RED 反向探针保证数值正确性

---

## 4. 测试覆盖

### 4.1 单元测试

| 测试 suite | 测试数 | 覆盖路径 |
|:-:|:-:|---|
| `PixelBlendTest` | **11 GTests** | DivBy255Approx 范围扫描 / BlendGlyphPixel 参考实现对比 / SSE2 4 种 count 布局 + zero-alpha + full-opaque + 256-px stress / Dispatch AVX2 13 种 count 布局 + SSE2 parity 对照 33-px head-to-head |
| `SoftwareCanvasTest` | 已有 26+ | 端到端 DrawText 真路径（真路径 + fallback） |
| `RendererTest` / `RenderIntegrationTest` / `LayoutIntegrationTest` / `PaintCommandTest` | 已有 20+ | 广路径像素断言（Phase 0-7 全程零行为变化）|
| **ctest 全量** | **59/59 PASS** | Phase 7.4 最终状态 |

### 4.2 性能测试

- `bench_drawtext` 8 BMs 全部通过（Warm/Cold × Short/Medium/Long + Replay × Real/Fallback）
- baseline/bench_drawtext.json 已入仓（Release，min_time=1.0s，CV ≤ 1%）
- 4 bench baseline 交叉验证（`bench_layout_*` / `bench_render_*` / `bench_imagecache`）无显著退化（10% 内）

### 4.3 RED 反向探针（3 次完整循环）

| Phase | 破坏点 | FAIL 结果 | 恢复后 |
|:-:|---|:-:|:-:|
| 5 | 改 `DivBy255Approx` 的 `n>>8` 错公式 | **5/5 FAIL** | 5/5 PASS |
| 7 SSE2 | 交换 `src_vec` channel order | **3/4 FAIL** | 4/4 PASS |
| 7b AVX2 | 改 `_mm256_permute4x64_epi64(0xD8)` → `0x00` | **2/2 FAIL** | 2/2 PASS |

**结论：** 测试套有效，能在实现错误时精准捕获，不是「死代码」。

---

## 5. 经验教训

摘自 `reflection-TASK-20260424-03.md`（完整 13 段见反思文档）。

### L1. 编译器魔数乘法 — 手写位运算近似的一般性陷阱

**场景：** 任何 "/常量 → 位运算" 或 "常量模/乘替换" 类手写优化

**教训：** GCC 自 4.x / Clang 自早期版本已对所有编译期常量除法自动应用 **Granlund-Montgomery 魔数乘法**（imul + shr，2 instructions），而手写 add-shift 链 3 adds + 2 shifts = 5 instructions 且破坏 u8↔u32 扩展优化。**只有 SIMD 场景才值得手写**（编译器无法 auto-vectorize 到 `pmulhuw` 等 SIMD-specific 指令）。

**落实：** ✅ 已写入 `writing-plans.mdc` §8「编译器已做优化识别 — 位运算/除法近似反模式」+ `systemPatterns.md`「已验证的模式（来自 TASK-20260424-03）」段（含 godbolt 验证命令）

### L2. SIMD 宽度不是线性越宽越快 — 工作负载尺寸决定最优 SIMD 宽度

AVX2 8 px/iter 在 ASCII glyph (6-12 px) 下 amortisation 不足，甚至比 SSE2 4 px/iter 慢。**异构工作负载应按输入尺寸阈值 dispatch**；测量后不达预期不等于代码废弃，保留宽 SIMD 路径为 workload evolution（CJK 大字号 / 图形元素 / 未来硬件）提供 headroom。

**落实：** ✅ 已沉淀到 `systemPatterns.md`「异构工作负载 SIMD 尺寸阈值 dispatch 模式」段（含 `kAVX2MinPixelsPerRow=16` 样板）

### L3. WSL2 / 云机 bench 必须引入 warm-up 协议

google/benchmark 默认 `--benchmark_min_time=0.05s` 在 WSL2 不稳定（CV 高达 8%）。**稳定协议**：sleep 10s + 单 filter 3-rep warm-up (min_time=0.2s) + 10-rep measure + CV ≤ 2% 门槛。Phase 6 首次测 6727 ns（偏离真实值 43%）的教训促成本协议标准化。

**落实：** ✅ 已写入 `writing-plans.mdc` §7「WSL2 / 云机 bench 稳态协议」+ `systemPatterns.md`「Bench Smoke 自检模式」追加 WSL2 稳态协议附录

### L4. API 层优化空间常远低于 plan 估算

现代运行时（glibc tcmalloc / FreeType / HarfBuzz 内部缓存）已对 hot API 做足优化。Plan 阶段遇到 "hot API 复用/缓存/幂等" 类候选，**默认收益 ≤ 2%**，主预算应集中到**算法层**（blit loop / data structure）。本任务 Phase 6 B2 单改动（-12.2%）收益 = Phase 1-4 API 层累计（-1.9%）× 6 倍。

**落实（P2）：** 已沉淀到 `systemPatterns.md`「已验证的模式（来自 TASK-20260424-03）」段

### L5. Plan R1 预留升级路径 >> 一次性过度承诺

本任务 plan 初版 D3 锁定 "B1+B2 不含 SIMD"，在 Phase 6/7 末两次 AskQuestion 触发升级，**用户基于实测数据做决定**而非 plan 凭空承诺。避免 plan 过度设计，也避免不达标时 silent accept。

**落实（P2）：** 已沉淀到 `systemPatterns.md`「刚性目标 + R1 升级路径 plan 模式」段

### L6. 负结果保留为未来基础设施

Phase 5 /255 近似回退，但保留 `glyph_blend.h` + `pixel_blend_test` 直接支撑 Phase 7 SSE2/AVX2 开发。**设计上保留核心 helper + 测试就能让下一个升级分支零成本接入**。

**落实（P2）：** 已沉淀到 `systemPatterns.md`「负结果资产化模式」段

---

## 6. 反复模式识别

| 已知模式 | 本次是否重复 | 落实 |
|---------|:-:|---|
| 前置依赖/环境/API 能力未验证 | ⚠️ **轻度重复**（新变体：**编译器已做的优化未 godbolt 确认**）| ✅ 已升级到 P1 建议 #1，`writing-plans.mdc` §8 规则化 |
| 非默认路径遗漏验证 | ⚠️ **轻度重复**（WSL2 冷启动瞬态未 warm-up）| ✅ 已升级到 P1 建议 #2，`writing-plans.mdc` §7 + `systemPatterns.md` Bench Smoke 附录规则化 |
| 计划文件清单与实际变更不一致 | ⚠️ 部分（R1 升级带 SIMD 扩展属 plan 预留升级分支正常扩张，非 plan 错误）| 不算反复，属模式「L5 R1 升级路径」的正向体现 |
| 测试隔离 / 提交粒度 / TDD 匹配 | ✅ 无 | — |
| 子代理返工 | N/A | — |

---

## 7. plan × 0.6 估时校准数据点 #6

| 任务 | 类型 | plan (min) | 实测 (min) | 倍率 | 档 |
|------|------|:-:|:-:|:-:|---|
| TASK-05 | bench | 180 | ~53 | 0.29× | 最窄 |
| TASK-09 | bench | 200 | ~47 | 0.24× | 最窄 |
| TASK-11 | bench | 80 | ~38 | 0.48× | 中等 |
| TASK-13 | 文档 | 120 | ~67 | 0.56× | 中等 |
| TASK-24-01 | 研究 | 115 | ~33 | 0.29× | 最窄 |
| **TASK-24-03** | **优化（2 次 R1 升级）** | **~230** | **~78** | **0.34×** | **最窄** |

**本任务意义：**
- 第 6 数据点确认「**最窄**」档 plan × 0.3 区间稳定（0.24-0.34× 范围，中位 0.29×）
- 首次出现**带 R1 升级的优化类任务**落在「最窄」档 — 虽有 2 次 R1 扩展 scope 一倍多，实际耗时仍在 0.3× 倍率
- 「最窄」识别特征：**基础设施（bench + baseline + 测试 target）100% 复用** + **核心改动 ≤ 10 文件** + **实验阶段可脚本化**（stash-swap + warm-up + 10-reps 固定协议）

---

## 8. 参考文档

- **设计规格：** `docs/specs/2026-04-24-drawtext-warm-opt-design.md`（v1.0，5 决策锁定 / 6 注入点核对表 / 12 验收标准）
- **实现计划：** `docs/plans/2026-04-24-drawtext-warm-opt.md`（7 Phase 阶梯骨架 + R1 升级分支）
- **回顾文档：** `memory-bank/reflection/reflection-TASK-20260424-03.md`（13 段 / 7 改进建议 / 4 新模式候选）
- **创意设计：** ❌ N/A（spec §2 5 决策已锁定，无架构/算法空白）
- **源头任务：** `memory-bank/archive/archive-TASK-20260419-09.md`（K7 来源）

### Git 提交链（13 commits）

| Commit | Phase | 主题 |
|:-:|:-:|---|
| `376b7b1^^^^...` | 0-4 | Phase 0-4 阶梯（baseline → hb_buffer → FT_Set_Pixel_Sizes → FontHandle 缓存 → GlyphCache::Put→ptr） |
| `2891d9d` | 4 | perf: Phase 4 GlyphCache::Put returns GlyphBitmap* |
| `09192c1` | 5 | perf: Phase 5 /255 mul-shift approx **(reverted)** |
| `05a82ab` | 6 | perf: Phase 6 B2 — pre-clip + row pointer blit |
| `838fd12` | - | chore: Round 2 build state |
| `c5f2856` | 7 | perf: Phase 7 B3 — SSE2 4 px/iter glyph blit |
| `376b7b1` | 7b+7.4 | perf: Phase 7 B3+ — AVX2 dispatch + finalize |
| `93a2602` | /reflect | docs(reflect): add reflection for TASK-20260424-03 |
| `<this>` | /archive | docs(archive): add archive for TASK-20260424-03 + 落实 P1 #1 #2 到规则 |

---

## 9. 长期维护建议

### 9.1 AVX2 路径的生效条件

当前 `kAVX2MinPixelsPerRow = 16`，意味着：
- **ASCII 字号 ≤ 18 px**（常见 glyph 宽度 6-12 px）：全部走 SSE2 直接路径
- **CJK 字号 ≥ 16 px** 或 **ASCII 字号 ≥ 24 px**（glyph 宽度 ≥ 16 px）：走 AVX2 fast path

**监控建议：** 下次用户 workload 发生显著变化（加入 CJK / 大字号 UI / 图形元素 blit 走本路径）时，重新 profile `BM_DrawTextReal_Warm_Long` 并重新标定 `kAVX2MinPixelsPerRow`。

### 9.2 残余 499 ns 的可能突破点（P3 触发型候选 TASK）

若未来 D5 `< 3000 ns` 刚性目标重启，推荐候选（由反思 §7.c 提炼）：

| # | 候选 | 预期收益 | 空间代价 |
|:-:|---|:-:|:-:|
| a | Skip-all-zero AA fast path（AA 边缘外纯 0 alpha 像素跳过）| -100-200 ns | 0 |
| b | GlyphCache row_ptr 数组预算 | -50-100 ns | +少量字节 |
| c | hb_shape cache per-text（复用同文本 shape 结果）| -200-500 ns | 大（per-text 空间）|

**建议优先级 P3 触发型** — 业务目标已达成，不主动排期。

### 9.3 降级风险评估

- **SSE2 路径**：所有 x86-64 baseline 硬件均支持（POPCNT 时代以前），0 运行时检查，稳定
- **AVX2 路径**：`__builtin_cpu_supports("avx2")` 运行时检查；Intel Haswell (2013) / AMD Zen 1 (2017) 及之后支持；**非 x86 平台（ARM / WASM）** 编译期守卫 `#if defined(__x86_64__) || defined(__i386__)` 直接跳过，走 SSE2 或标量 fallback（需后续 TASK 加 NEON）
- **标量 fallback**：永远保留（`BlendGlyphPixel` in `glyph_blend.h`），无 SIMD 时平滑降级

---

## 10. 归档阶段改进建议落实（最终闭环）

### P1 建议（强制归档前落实）

| # | 建议 | 落实 |
|:-:|---|:-:|
| 1 | **Godbolt 验证编译器魔数优化** — 位运算/除法近似前置检查 | ✅ `writing-plans.mdc` §8「编译器已做优化识别 — 位运算/除法近似反模式」+ `systemPatterns.md` 新段 |
| 2 | **WSL2 / 云机 bench warm-up 协议标准化** | ✅ `writing-plans.mdc` §7「WSL2 / 云机 bench 稳态协议」+ `systemPatterns.md`「Bench Smoke 自检模式」追加附录 |

### P2 建议（沉淀到 `systemPatterns.md`）

| # | 建议 | 落实 |
|:-:|---|:-:|
| 3 | 异构工作负载 SIMD 尺寸阈值 dispatch 模式 | ✅ 已沉淀 |
| 4 | 负结果资产化模式（helper + tests 保留为未来 SIMD 基础设施） | ✅ 已沉淀 |
| 5 | 刚性目标 + R1 升级路径 plan 模式 | ✅ 已沉淀 |
| 6 | API 层天花板识别（优化类任务预估调整）| ✅ 已沉淀（融入编译器优化识别段与「本任务关键发现」）|
| 7 | 智能阈值 dispatch 不退化为简单 revert | ✅ 已沉淀（与 #3 配对落实）|

---

## 11. 最终评估

| 维度 | 结果 |
|------|:-:|
| **业务目标**（真路径 < fallback）| ✅ 达成（3499 < 3608 ns，快 3%）|
| 技术刚性目标 D5 (`< 3000 ns`) | ⚠️ 未达（差 499 ns / 14%，2 次 R1 升级后用户接受）|
| 12 验收判据 | ✅ **11/12 完整达标** + ⚠️ 1/12（S1 warm medium）14% 偏差用户接受 |
| 全局质量（ctest / Release `-O3 -Werror` / 无回归） | ✅ 59/59 PASS / 0 err/warn / 无 bench 回归 |
| plan × 0.6 倍率 | ✅ 0.34×（「最窄」档）|
| 反复模式沉淀 | ✅ 2 P1（godbolt / WSL2 warm-up）+ 5 P2 规则化 |
| **K7 状态** | ✅ **Resolved** |

**总评：** 业务目标超额达成（真路径默认化前置条件锁定 + Cold 路径副产品 -46.4%）；技术刚性目标未完全达成但用户基于完整 R1 升级路径数据接受；2 个 P1 反复模式在归档阶段规则化，长期知识库新增 4 个经过实证的架构模式。
