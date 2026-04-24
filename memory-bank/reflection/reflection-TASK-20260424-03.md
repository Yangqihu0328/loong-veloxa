# 回顾：TASK-20260424-03 — SoftwareCanvas::DrawText 真路径 warm 优化

**日期：** 2026-04-24
**任务 ID：** TASK-20260424-03
**复杂度级别：** Level 2-3（优化类；多候选路径 + 5 设计决策；修改 3-5 文件 + 2 次 R1 升级）
**分支：** `feature/TASK-20260424-03-drawtext-warm-opt`（12 commits，待合并）

---

## 1. 计划 vs 实际

| 维度 | 计划 | 实际 | 偏差原因 |
|------|------|------|---------|
| 任务 Phase 数 | 7 | **7 + 2 升级（7b AVX2 / 7.4 finalize）** | R1 AskQuestion 触发 2 次升级路径（SSE2 / AVX2） |
| 预估时间 | 130 min（plan × 0.6 = ~78 min） | ~78 min | ✅ 预估准确（尽管 scope 实际扩大到 ~230 min base） |
| 文件变更（核心代码）| 3-5 文件 | **7 文件 + 2 新 header + 1 新测试** | B3 SIMD 升级引入 `blit_sse2.h` / `blit_avx2.h` / `glyph_blend.h` |
| 设计变更 | D3 = B1+B2 组合（不含 SIMD） | **D3 失效 → 升 B3 SSE2 → 再升 AVX2 dispatch** | Phase 6 结束 warm=4689 ns 仍差 1689 ns；用户 2 次选升级 |
| Warm_Medium 目标 | < 3000 ns（刚性 D5） | **3499 ns**（差 499 ns / 14%）| API 层（Phase 1-3）累计仅 -0.5% 远低于 plan 预期 400-900 ns；Phase 5 /255 近似回退；累计 -38% 仍未穿透刚性目标 |
| 业务前置条件 | 真路径 < fallback | ✅ **达成**（3499 vs 3608 ns，快 3%） | Phase 7 SSE2 + AVX2 智能阈值 dispatch |
| Commits | 10 | **12** | 2 次 R1 升级各加 1 commit；Round 2/3 间有 1 chore commit 记录中间态 |
| 测试新增 | 1 GTest（Mixed TDD D3） | **11 GTests**（DivBy255 精度 / BlendGlyphPixel 契约 / SSE2 4 × / AVX2 dispatch 2 ×） | SIMD 升级要求覆盖 scalar / SSE2 / AVX2 三条路径 |

---

## 2. 回顾检查清单（代码变更类）

| 维度 | 结果 | 备注 |
|------|:-:|---|
| 计划精确度 — 文件清单 vs 实际 | ⚠️ 部分偏差 | 核心 3 文件（software_canvas / font_manager / glyph_cache）精准；但 plan 未预见 SIMD 升级带来的 2 新 SIMD header + 1 blend helper |
| TDD 执行情况 — RED/GREEN | ✅ 3 次完整 RED 循环 | Phase 5 /255 helper（5/5 测试 RED→GREEN）/ Phase 7 SSE2 channel order（3/4 RED→GREEN）/ Phase 7 AVX2 permute4x64 imm（2/2 RED→GREEN） |
| 子代理质量 | N/A | 本任务未启用子代理 |
| 测试隔离 | ✅ | 11 GTests 全部自包含，无外部字体依赖（`software_canvas_test` 真路径用系统 DejaVu，但 `pixel_blend_test` 全部纯数据） |
| 提交粒度 | ✅ 按 Phase 分 | 12 commits 中 9 个 Phase commits + 2 chore（round 间） + 1 finalize；无大杂烩 commit |
| 非默认路径验证 | ⚠️ 部分 | AVX2 only 路径需 `count >= 16` 才触发；测试覆盖 13 种 count 布局含 AVX2 + SSE2 + 标量 tail 混合；但**WSL2 冷启动瞬态首次未捕获**（下详 §4） |
| Release `-O3 -Werror` 通路 | ✅ | build-bench 多次全量 rebuild 0 err/warn |

---

## 3. 做得好的

### a. 阶梯验证策略完整跑通
每 Phase 独立 patch + 独立 bench 记录 + 独立 commit，Phase 5 负结果可单独 `git revert` 不影响其他 Phase；stash-swap 同窗口对比让每 Phase 独立贡献精确量化（见 progress.md Phase × warm_Medium 累积表）。

### b. 2 次 R1 升级路径按 plan 预留流程严格走
Phase 6 末 `warm=4689 ns` 未达 → 首次 AskQuestion 用户选 (A) B3 SSE2；Phase 7 末 `warm=3354 ns` 仍未达 → 二次 AskQuestion 用户选 (C) AVX2 dispatch。**避免了 plan 阶段对 SIMD 一次性过度承诺**，也避免了不达标时 silent accept。

### c. RED 反向探针 3 次完整循环验证
Phase 5 (GCC 魔数乘法 vs 手写近似)、Phase 7 SSE2 (channel order)、Phase 7 AVX2 (permute4x64 imm) 都做了"破坏实现 → 确认 FAIL → 恢复 → 确认 PASS"的完整循环；**RED 探针模式第 3+ 次成熟应用**，可作为 Mixed TDD 标配。

### d. Phase 5 负结果转化为 SIMD 基础设施
即便 /255 乘-移近似在标量路径倒退 +1.1%，保留的 `glyph_blend.h`（DivBy255Approx + BlendGlyphPixel）+ `pixel_blend_test`（5 精度契约）直接成为 Phase 7 SSE2/AVX2 的参考实现与 fidelity 对照标尺。**负结果不等于浪费**。

### e. 智能阈值 SIMD dispatch 创新
AVX2 实测无净收益时，没有简单回退到 SSE2，而是引入 `count >= kAVX2MinPixelsPerRow(16)` 阈值 dispatch，保留 CJK 大字号 / 未来硬件进化 headroom。这是 **「异构工作负载 SIMD 尺寸阈值 dispatch」** 新模式的首次成熟落地。

### f. 业务目标超额达成
Warm 真路径 3499 ns 比 Fallback 3608 ns **快 3%**（Phase 7 SSE2 即时测量更达 7.8%），默认化前置条件锁定。Cold 路径副产品 -46.4% 大幅改善（default font + pixel size 缓存对 cold 路径同样生效），扩大了本次优化的实际影响面。

### g. Stash-swap warm-up + 10-reps 稳定测量协议
单次 WSL2 bench CV 可达 8%，通过「sleep 10s + 3-rep warm-up + 10-rep measure」可稳定在 CV ≤ 1%，首次 Phase 6 baseline 测得 6727 ns（43% 偏离真实 4689 ns）的错误结论被有效兜底。

---

## 4. 遇到的挑战

### a. Phase 1-3 累计收益远低于 plan 预期
**Plan 预期** hb_buffer 复用 + FT_Set_Pixel_Sizes 缓存 + FontHandle 缓存 = 400-900 ns（基于 hb_buffer_create / FT state set / FindFont 字符串扫描的 ns-per-call 估算）。
**实际** 累计仅 **-26 ns (-0.5%)**。
**根因** glibc tcmalloc 对同尺寸频繁 alloc/free 有 thread-cache 优化（hb_buffer 实际开销 < 50 ns）；FontManager::FindFont 对 font_count() <= 3 的小数组扫描极快；FT_Set_Pixel_Sizes 在 pixel_size 相同时 FT 内部本来就有 early-return。**现代运行时对 hot API 已做足优化，API 层优化空间常远低于 plan 估算**。

### b. Phase 5 /255 乘-移近似反而 +1.1% 倒退
**Plan 预期** -500 ns（基于 `(x*257+32768)>>16` 去 idiv 的微基准测试假设）。
**实际** +56 ns (+1.1%)。
**根因** GCC `-O3` 对常量除法 `/255` 已应用 **Granlund-Montgomery 魔数乘法**（imul + shr 2 instructions），比手写 add-shift 链（3 adds + 2 shifts = 5 instructions）更短；且手写版本引入 u8↔u32 扩展打包开销。**Plan 阶段没 godbolt 看 GCC `-O3` 原生输出**，导致优化方向选错。

### c. AVX2 实测无净收益
**Plan R1 升级预期** -300~-800 ns（8 px/iter 翻倍）。
**实际** +13 ns（CV noise 内统计平手）。
**根因** ASCII glyph 宽度分布偏小（6-12 px），AVX2 8 px/iter 常常只跑 1-2 次后走 SSE2/scalar tail，无法摊销 `cvtepu8_epi16 + permute4x64_epi64` 每 iter 2-3 cycles setup 开销 + dispatch branch 开销。**一刀切 SIMD 宽度非最优，应按输入尺寸 dispatch**。

### d. WSL2 性能抖动 CV 高达 8%
**首次** Phase 6 stash baseline 测得 6727 ns（CV 7.92%），远高于真实值 4689 ns，险些得出「Phase 7 倒退」的错误结论。
**根因** WSL2 CPU governor 在不活跃时下调频率，冷启动瞬态 + load average 抖动；google/benchmark 自身的 `--benchmark_min_time=0.05s` 不足以让 CPU 进入稳态。
**缓解** sleep 10s + 3-rep warm-up + 10-rep measure + min_time=0.2s → CV 稳定 ≤ 1%。

### e. D5 刚性 < 3000 ns 未达标
2 次 R1 升级后累计 -38%（-2406 ns）仍差 499 ns (14%)。用户知情接受（业务前置条件已达成）。**根因** 剩余开销集中在 hb_shape 固定 cost（~600-800 ns per call，与 text 长度弱相关）+ FT glyph metrics lookup（~200 ns）+ glyph bitmap cache Get（~100 ns）— 这些是算法本质开销而非实现浪费，无明显 quick win。

---

## 5. 经验教训

### L1. Compiler 魔数乘法优化是手写位运算近似的一般性陷阱
**场景** 任何把 `/constant` 改为 `*magic >> shift` 的手写优化
**结论** GCC/Clang `-O3` 自 GCC 4.x 起对所有常量除法应用 Granlund-Montgomery 魔数乘法；**只有 SIMD 场景（编译器无法 auto-vectorize 到 pmulhuw）** 才值得手写，标量路径写手写近似几乎必然输给编译器。
**落实** 下次任何 "/常量 → 位运算" 的优化 plan 前必须 godbolt 确认 GCC `-O3` 原生输出。

### L2. SIMD 宽度不是线性越宽越快 —— 工作负载尺寸决定最优 SIMD 宽度
AVX2 8 px/iter 在 ASCII glyph (6-12 px) 下 amortisation 不足，甚至比 SSE2 4 px/iter 慢。**异构工作负载应按输入尺寸 dispatch**（本任务 `count >= 16` 阈值）；测量后不达预期不等于代码废弃，保留 AVX2 路径为工作负载未来变化（CJK 大字号 / 图形元素）提供 headroom。

### L3. WSL2 / 云机 bench 必须引入 warm-up 协议
google/benchmark 默认 `--benchmark_min_time=0.05s` 在 WSL2 不稳定。**稳定协议**：sleep 10s + 单 filter 3-rep warm-up + 全量 10-rep measure + min_time=0.2s + CV ≤ 2% 门槛；否则重测。

### L4. API 层优化空间常远低于 plan 估算
现代运行时（glibc malloc / FreeType / HarfBuzz 内部缓存）已对 hot API 做足优化。Plan 阶段遇到 "hot API 复用/缓存/幂等" 类优化候选，先**默认收益 ≤ 2%**，把预算集中到 **算法层（blit loop / data structure）**。本任务 Phase 6 B2 一个 pre-clip + row pointer 改动（-12.2%）收益超过 Phase 1-4 累计总和（-1.9%）6 倍。

### L5. Plan R1 预留升级路径 >> 一次性过度承诺
本任务 plan 初版 D3 锁定 "B1+B2 不含 SIMD"，在 Phase 6 末 AskQuestion 升 B3 SSE2；再在 Phase 7 末 AskQuestion 升 AVX2 dispatch。**两次升级都是用户基于实测数据做的决定**，而非 plan 阶段一次性承诺所有 SIMD 工作量。这避免了 plan 过度设计，也避免了不达标时 silent accept。

### L6. 负结果保留为未来基础设施
Phase 5 /255 近似回退，但保留的 `glyph_blend.h` + `pixel_blend_test` 精度契约测试直接支撑了 Phase 7 SSE2/AVX2 的开发。**负结果 != 浪费**，设计上保留核心 helper + 测试就能让下一个升级分支零成本接入。

---

## 6. 流程改进

| 维度 | 评估 | 改进方向 |
|------|:-:|---|
| 头脑风暴 5 决策充分性 | ✅ | D1-D5 全程无返工；D3 失效由 R1 升级承接属正常流程 |
| Plan 详细度 | ⚠️ | Plan Phase 1-3 预期收益高估（根因 L4）；应追加「API 层天花板识别」子段 |
| TDD 遵守度 | ✅ | Mixed TDD D3 + 3 次 RED 反向探针完整循环 |
| R1 升级流程 | ✅ | 2 次 AskQuestion 严格按 plan §7 触发条件执行，无 silent accept |
| Commit 粒度 | ✅ | 12 commits 按 Phase 分；round 间 chore commit 明确记录中间态 |
| 文档更新 | ✅ | baseline JSON / README / techContext / tasks / progress / activeContext 全链路同步 |
| Stash-swap 测量协议 | ✅ | 每 Phase 严格同窗口对比，避免硬件/load 漂移污染数据 |

---

## 7. 技术改进

### a. 代码质量
- **SIMD header 分层**：`glyph_blend.h`（scalar helper）→ `blit_sse2.h`（SSE2 4 px）→ `blit_avx2.h`（AVX2 8 px + dispatcher）清晰层次；每层仅 include 前一层，避免菱形依赖
- **`__attribute__((target("avx2")))` 函数级编译**：保持二进制 ABI 为 baseline x86-64 不变，单函数启用 AVX2 + 运行时 `__builtin_cpu_supports` 判定，生产环境安全
- **智能阈值 `kAVX2MinPixelsPerRow=16`** 经验常量有 comment 解释 rationale（ASCII 6-12 px / CJK 20+ px）

### b. 测试覆盖
- **11 GTests 覆盖 3 路径 × 多 count 布局**：DivBy255 range scan / scalar BlendGlyphPixel parity / SSE2 {0,1,3,4,7,16,19} layouts / AVX2 {0,1,3,4,7,8,9,15,16,17,23,31,64} layouts / 256-px stress / zero-alpha identity / full-opaque overwrite
- **RED 反向探针** 在 3 处作为 guardrail 留存（测试 case 本身会在实现被破坏时 FAIL）

### c. 技术债
- **D5 刚性 < 3000 ns 未达** 留 354-499 ns 缺口 — 后续候选：skip-all-zero AA fast path（AA 边缘之外纯 0 alpha 像素跳过，预期 -100-200 ns）/ glyph cache row_ptr 数组预算（-50-100 ns）/ hb_shape cache per-text（-200-500 ns，但空间代价大） — **建议作 P3 触发型任务**，不主动排期
- **AVX2 路径在当前硬件无可见收益** — 保留代码为 workload evolution 预留；下次字体/语言环境变化（CJK 大字号）需重测验证阈值 `kAVX2MinPixelsPerRow`

---

## 8. 安全评估

| 维度 | 状态 | 备注 |
|------|:-:|---|
| 输入验证 | N/A | 本任务优化内部 API，无新外部输入面 |
| 认证/授权 | N/A | 无权限模型变更 |
| 数据保护 | N/A | 无敏感数据 |
| 依赖审计 | ✅ | 无新第三方依赖（FreeType/HarfBuzz/google-benchmark 已在仓） |
| 错误信息 | N/A | 无错误路径变更 |
| 敏感数据处理 | N/A | 无 |
| **内存安全（新增维度）** | ✅ | SSE2/AVX2 路径加载都用 `_mm_loadu_si128` / `_mm_loadl_epi64` 允许 unaligned 访问；AVX2 加载 32B 前经 `count + 8 <= n` 守卫；标量 tail 单像素 bounds 由外层 pre-clip 保证；**未引入新的缓冲区越界风险** |
| **数值正确性（新增维度）** | ✅ | 11 GTests ±1 LSB 精度契约；RED 反向探针验证测试能捕获实现 bug |

**本任务不涉及显式安全变更，但 SIMD 代码引入了隐式的内存访问与数值精度风险，已由 11 GTests + pre-clip 守卫完整覆盖。**

---

## 9. 反复模式识别

| 已知模式 | 频率 | 本次是否重复 |
|---------|:-:|---|
| 计划文件清单与实际变更不一致 | 9+ 次 | ⚠️ **部分重复** — plan 3-5 文件 vs 实际 7 文件 + 3 新 header；根因是 R1 升级带来的 SIMD 扩展，属 plan 预留升级分支的正常扩张而非 plan 错误 |
| 子代理产出需大量返工 | 7+ 次 | N/A（未启用子代理） |
| 前置依赖/环境/API 能力未验证 | 8+ 次 | ⚠️ **重复** — Phase 5 /255 近似前未 godbolt 确认 GCC 已做 Granlund-Montgomery 优化（新变体：**编译器已做的优化未 godbolt 确认**） |
| 非默认路径遗漏验证 | 4+ 次 | ⚠️ **轻度重复** — 首次 Phase 6 stash baseline 测冷启动瞬态误判（未 warm-up），但当次就识别并补 warm-up 协议；属实时修正 |
| 测试隔离问题 | 7+ 次 | ✅ 无 |
| 提交粒度偏离计划 | 7+ 次 | ✅ 无 |
| TDD 严格度与场景不匹配 | 11+ 次 | ✅ 无 — Mixed TDD D3 + RED 反向探针 3 次应用均匹配场景 |

**本次两个重复模式均在单次任务内实时修正，但沉淀到规则可预防下次：**
- 「编译器已做的优化未 godbolt 确认」→ 升级规则（下 §10.P1）
- 「云机 / WSL2 bench 冷启动瞬态」→ 升级规则（下 §10.P1）

---

## 10. 改进建议（附优先级与落实方式）

| # | 建议 | 优先级 | 落实方式 | 目标 |
|:-:|---|:-:|---|---|
| 1 | **Godbolt 验证编译器魔数优化** — 任何 "/常量 → 位运算" 或 "常量模/乘替换" 类优化 plan 前必须 godbolt 看 GCC `-O3` 原生输出 | **P1** | 追加到 `writing-plans.mdc`「API grep + 代码级可行性验证」段新增子项；在 plan Phase 0 工具核查矩阵加 `godbolt-cli` 或 `gcc -S` 手动确认 | 反复出现（同类：手写位运算 vs 编译器优化），本次 Phase 5 实证 +1.1% 倒退。Granlund-Montgomery / Montgomery reduction 等编译器已自动应用的优化不可手写再次替换 |
| 2 | **WSL2 / 云机 bench warm-up 协议标准化** — plan §0 工具核查加「bench 稳态检查」3 步协议：sleep 10s → 3-rep warm-up (min_time=0.2s) → 10-rep measure → CV ≤ 2% 否则重测 | **P1** | 追加到 `systemPatterns.md`「Bench Smoke 自检模式」+ `writing-plans.mdc`「性能基准任务必检项」§N | 反复出现（WSL2 本机抖动是常态），本任务 CV 从 8% 收敛到 1% 的实际经验 |
| 3 | **SIMD 工作负载尺寸阈值 dispatch 模式沉淀** — 异构尺寸工作负载应按输入尺寸阈值 dispatch 不同 SIMD 宽度，不应一刀切 | **P2** | 追加到 `systemPatterns.md` 新模式段「异构工作负载 SIMD 尺寸阈值 dispatch」；引用本任务 `kAVX2MinPixelsPerRow=16` 为样板 | 首次模式化，下次 SIMD 类任务（NEON / AVX-512）可复用 |
| 4 | **负结果保留为基础设施模式沉淀** — Mixed TDD 中的 D3 测试 + helper 即便在主路径回退时也应保留，作为未来升级分支的 fidelity 对照 | **P2** | 追加到 `systemPatterns.md`「Mixed TDD RED 反向探针实践」段追加子项「负结果资产化」；引用本任务 Phase 5 → Phase 7 的价值链 | 新模式，避免 revert 误伤；本任务 `glyph_blend.h` + `pixel_blend_test` 直接支撑 SIMD 升级 |
| 5 | **优化类任务 Plan 阶段加「API 层天花板识别」子段** — 候选是 hot API 的复用/缓存/幂等改造时，默认收益 ≤ 2%，主预算集中到算法层 | **P2** | 追加到 `writing-plans.mdc` 或 `systemPatterns.md`「优化类任务候选排序」新段 | 本任务 Phase 1-3 vs Phase 6 的 6 倍落差实证 |
| 6 | **R1 AskQuestion 升级路径的价值正向沉淀** — 多候选优化类任务 plan 主线刚性目标未必达成时，应显式预留升级路径而非一次性承诺全部工作 | **P2** | 追加到 `systemPatterns.md` 新模式段「刚性目标 + R1 升级路径 plan 模式」；引用 TASK-24-01 / TASK-24-03 两次 R1 实证 | 本任务 2 次 R1 升级都按协议执行无偏差，模式可固化 |
| 7 | **智能阈值 dispatch 不退化为简单回退** — SIMD 路径实测无净收益时，应增加尺寸阈值 dispatch 保留代码为 workload evolution headroom，而非直接 revert | **P2** | 引用本任务 AVX2 `count >= 16` 实例作为 `systemPatterns.md`「异构工作负载 SIMD 尺寸阈值 dispatch」段样板 | 与 #3 配对落实 |

---

## 11. plan × 0.6 估时校准数据点 #6

| 任务 | 类型 | plan (min) | 实测 (min) | 倍率 | 档 |
|------|------|:-:|:-:|:-:|---|
| TASK-05 | bench | 180 | ~53 | 0.29× | 最窄 |
| TASK-09 | bench | 200 | ~47 | 0.24× | 最窄 |
| TASK-11 | bench | 80 | ~38 | 0.48× | 中等 |
| TASK-13 | 文档 | 120 | ~67 | 0.56× | 中等 |
| TASK-24-01 | 研究 | 115 | ~33 | 0.29× | 最窄 |
| **TASK-24-03** | **优化（带 2 次 R1 升级）** | **~230**（130 base + 40-60 B3 + 40-60 AVX2 用户加码） | **~78** | **0.34×** | **最窄** |

**本任务数据点意义：**
- **第 6 个数据点确认「最窄」档 plan × 0.3 区间稳定**（0.24-0.34× 范围）
- **带 R1 升级的优化类任务 scope 扩张可纳入「最窄」档**——虽有 2 次 R1 扩展 scope 一倍多，实际耗时仍在 0.3× 倍率
- **识别特征**：本任务满足「基础设施（bench + baseline + 测试 target）100% 复用」+ 「核心改动 ≤ 10 文件」+ 「实验阶段可脚本化（stash-swap + warm-up + 10-reps 固定协议）」 3 条「最窄」识别条件

---

## 12. 关键发现清单

| # | 发现 | 类型 |
|:-:|---|---|
| F1 | **K7 解决**：真路径 warm 3499 ns < fallback 3608 ns（快 3%），真路径默认化业务前置条件锁定 | 业务成果 |
| F2 | **Phase 6 B2 pre-clip + row ptr 是 blit 最大单 Phase 收益**（-12.2% / -651 ns Medium；-29.5% / -5016 ns Long）| 算法层发现 |
| F3 | **Phase 7 SSE2 SIMD 是第二大收益**（-28.6% / -1346 ns Medium）| SIMD 层发现 |
| F4 | **Cold 路径副产品 -46.4%** 大幅改善（default font + pixel size 缓存对 cold 同样生效）| 额外收益 |
| F5 | **GCC `-O3` Granlund-Montgomery 魔数乘法优于手写 /255 近似**（Phase 5 +1.1% 倒退实证）| 编译器洞察 |
| F6 | **AVX2 8 px/iter 在 ASCII glyph (6-12 px) 下无净收益** — amortisation 不足；用 `count >= 16` 阈值 dispatch | SIMD 分发洞察 |
| F7 | **D5 刚性 < 3000 ns 未达标 499 ns**（14%）— 累计 -38% 后仍差；业务目标已达成，用户接受 | 目标评估 |

---

## 13. 下一步

1. `/archive` — 生成 `memory-bank/archive/archive-TASK-20260424-03.md` 合并本回顾 + /build 成果 + 落实 P1 建议 1-2（godbolt + WSL2 warm-up 协议）到规则文件
2. 合并 `feature/TASK-20260424-03-drawtext-warm-opt` → `main`（`--no-ff`）
3. 建议追加候选任务 **TASK-20260424-04 (P3 触发型)**：skip-all-zero AA fast path + glyph cache row_ptr 数组 预算 -150-300 ns（若未来 D5 刚性目标重启或有 CJK 大字号 workload 需求）
