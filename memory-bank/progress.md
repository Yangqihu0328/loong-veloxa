# 进度记录

## 当前任务

**TASK-20260424-03：SoftwareCanvas::DrawText 真路径 warm 优化**（Level 2-3 优化类）

- ✅ VAN 初始化（2026-04-24）— 任务 ID 分配 / 分支 `feature/TASK-20260424-03-drawtext-warm-opt` 创建 / 基础假设核查（K7 候选 a/b 实证 + 3 副产品候选识别）
- ✅ /plan 规划完成（2026-04-24）— 头脑风暴 5 决策锁定（D1-D5） / 设计文档 `docs/specs/2026-04-24-drawtext-warm-opt-design.md` / 实现计划 `docs/plans/2026-04-24-drawtext-warm-opt.md`（7 Phase 阶梯骨架 + 130 min plan / ~78 min 预期 / 10 commits）/ 注入点核对表 6/6 可行
- ✅ /build Phase 0（2026-04-24）— 工具核查（rg/jq shell MISS 已知；python3/find/bc ✅；build-bench/_deps 已存在跳 FetchContent 守卫）；API grep 实证 HashMap `bool Insert` + `V& operator[]` / FontEntry 无 ft_pixel_size / GlyphCache::Put 5 处下游（software_canvas + 4 测试）；本机当日 baseline `BM_DrawTextReal_Warm_Medium_mean = 5412 ns`（JSON baseline 5807 ns -7% 抖动；CV 0.19% 稳定）
- ✅ /build Phase 1 A hb_buffer 复用（2026-04-24）— `HbBufferHolder` RAII + `thread_local` + `AcquireThreadLocalHbBuffer()`；`hb_buffer_create/destroy` per-call 消除；stash-swap 同窗口对比 Warm_Medium 5434→5397 ns（**-0.7%, -37 ns**），远低于预期 200-400 ns，**根因**：glibc tcmalloc 对同尺寸频繁 alloc/free 有 thread-cache 优化；Warm_Short 902→828 ns (-8.2%) 说明 fixed overhead 在短文本占比更大；ctest 26/26 PASS；D5 刚性目标 < 3000 ns 仍差 2397 ns，按 D1 阶梯进入 Phase 2
- ✅ /build Phase 2 C FT_Set_Pixel_Sizes 状态缓存（2026-04-24）— `FontEntry.ft_pixel_size` + `SetFacePixelSize(handle, size)` 幂等公开 API；DrawText 替换 `GetFace + FT_Set_Pixel_Sizes` 组合为单次 `SetFacePixelSize` 调用；stash-swap 同窗口 Warm_Medium 5323→5266 ns（**-1.07%, -57 ns**），Warm_Long 17063→16069 ns (-5.8%)，累计 Phase 0→2 Medium -146 ns (-2.7%)；ctest 26/26 PASS（现有契约测试 `GetHbFontHandlesSizeChange` 不受影响）；仍差 2266 ns，进入 Phase 3
- ✅ /build Phase 3 E 默认 FontHandle 缓存（2026-04-24）— `SoftwareCanvas.cached_default_font_` 成员（非 thread_local，避免跨 canvas 污染）；首次 DrawText 解析后缓存；stash-swap Warm_Medium 5403→5386 ns（**-0.3%, -17 ns**），Warm_Short 821→806 ns (-1.8%)，累计 Phase 0→3 Medium -26 ns (-0.5%)；ctest 24/24 PASS；**关键观察**：Phase 1-3 三候选累计改善远低于 plan 预期（400-900 ns），说明真瓶颈在内层 blit loop 或 GlyphCache 查找 — Phase 4-6 将直击核心
- ✅ /build Phase 4 D `GlyphCache::Put` 返回 `GlyphBitmap*`（2026-04-24）— Put 签名 `void`→`GlyphBitmap*`，实现改用 `entries_[key]` 操作单次查找 + 移动赋值；DrawText 用 Put 返回值，消掉紧跟的 Get；4 处测试忽略返回值向后兼容；stash-swap Warm_Medium 5378→5311 ns（**-1.25%, -67 ns**，CV 0.41% 可信）— 单 Phase 改善最大；累计 Phase 0→4 Medium -101 ns (-1.9%)，仍差 2311 ns；ctest 46/46 PASS（Renderer/RenderUtils 全路径绿）
- ⚠️ /build Phase 5 B1 `/255` 乘移位近似 — **实验回退**（2026-04-24）— 创建 `veloxa/graphics/software/glyph_blend.h` 内联 `DivBy255Approx` + `BlendGlyphPixel` + `tests/graphics/pixel_blend_test.cc` 含 5 个精度契约测试（range 扫描 + 参考实现比对 + 端点极值）+ RED 反向探针（临时 `n>>8` 错公式 → 5/5 FAIL → 恢复 5/5 PASS 完整循环）；但替换 DrawText 内层 blit 后 stash-swap Warm_Medium 5311→5367 ns（**+56 ns, +1.1% 倒退**，CV 0.65% 稳定）；**根因**：GCC `-O3` 对常量除法 `/255` 已应用 Granlund-Montgomery 魔数乘法（imul+shr）比手写 add-shift 链更优 + u8↔u32 扩展打包开销；已回退 blit 代码到 Phase 4 形态，但**保留 helper header + test + CMake 注册**作为未来 SSE2/NEON SIMD pass 的精度参考基础设施；ctest 51/51 PASS（回退后 28/28 验证）；**关键负面发现写入 code comment + progress** 作为 B1 候选的最终结论
- ✅ /build Phase 6 B2 预裁剪 + row pointer（2026-04-24）— 外层计算 `col_start/end`, `row_start/end` 预裁剪 glyph 与 canvas 交集；内层用 `dst_row` 指针 + `alpha_row` 指针递增，消除 `py*stride+px` 乘法 + 4 次边界比较 + 索引 mul；stash-swap **Warm_Medium 5340→4689 ns (-651 ns, -12.2%)**，Warm_Long 17007→11991 ns (**-5016 ns, -29.5%**) — 多字符场景巨大收益；CV 0.66% 稳定；累计 Phase 0→6 Medium **5412→4689 ns (-723 ns, -13.4%)**；ctest 53/53 PASS（广路径 render/integration/layout/paint_command 全绿）；**仍差 1689 ns (36%)** 未达 D5 刚性目标 → 按 plan R1 触发升级决策点 AskQuestion
- ✅ /build Phase 7 B3 SSE2 SIMD blit（2026-04-24）— 新建 `veloxa/graphics/software/blit_sse2.h` 含 `DivBy255ApproxU16` (per-lane pmulhuw-style 近似) + `BlendGlyphRowSSE2`（4 px/iter: 加载 4 alpha → 展开 u16 → DivBy255Approx(sa*alpha) 得 effective_a → unpack dst 到 2×8 u16 → 广播 ea/inv 到 per-channel u16 → split 两次 /255 避免 u16 溢出 → packus_epi16 clamp 到 u8 → storeu）+ 编译期 `#if defined(__SSE2__)` 守卫 + 标量 tail 0..3 px 回退；**src_vec 的 alpha 通道强制 255 让统一 blend 公式 `(src*ea + dst*inv)/255` 同时覆盖 RGBA 4 通道**（因 (255*ea)/255 = ea 精确）；扩展 `pixel_blend_test` 新增 4 个 SIMD 契约测试（7 种 count 布局含 4-iter + tail 混合 + 256 px stress + zero-alpha identity + full-opaque overwrite 4 组 + RED 反向探针破坏 channel order 确认 3/4 FAIL 再恢复 GREEN 完整循环）；stash-swap warm-up + 10 reps 稳定测量 **Warm_Medium 4700→3354 ns (-28.6%, -1346 ns)**，Warm_Short 739→644 ns (-12.9%)，**Warm_Long 12682→10382 ns (-18.1%, -2300 ns)**，CV 0.69% 非常稳；**累计 Phase 0→7 Medium 5412→3354 ns = -2058 ns (-38%)**；ctest 57/57 PASS（含 9 pixel_blend / 全 render/integration/layout/paint_command 路径）
- 🎯 /build Phase 7 业务/技术目标评估：**真路径 3354 ns < Fallback 3637 ns（快 7.8%）→ 真路径默认化业务前置条件超额达成**；技术刚性 D5 < 3000 ns 仍差 354 ns (10.6%) → 按 plan R1 二次 AskQuestion 决定是否 (A) 接受现状并推进 finalize / (B) 继续再压榨 (glyph cache row_ptr 数组 + skip-all-zero AA fast path) / (C) AVX2 8 px/iter
- ✅ /build Phase 7 B3 AVX2 升级（2026-04-24，用户选 (C)）— 新建 `veloxa/graphics/software/blit_avx2.h`：`__attribute__((target("avx2")))` 函数级编译 + `BlendGlyphRowAVX2` 8 px/iter（`_mm256_cvtepu8_epi16` + `_mm256_permute4x64_epi64(0xD8)` lane 对齐：lane_lo = pixels 0-3 alpha, lane_hi = pixels 4-7 alpha；AVX2 unpack_epi8 lane-local 产出 dst_lo = pixels {0,1,4,5}, dst_hi = pixels {2,3,6,7}；pack lane-local 还原成连续 8 pixel store）+ runtime dispatcher `BlendGlyphRow` 用 `__builtin_cpu_supports("avx2")` 静态缓存；扩展 `pixel_blend_test` 新增 2 个 AVX2 dispatch 契约测试（13 种 count 布局含 AVX2 fast path + SSE2 fast path + 标量 tail 混合、SSE2-parity 33 px 对照）+ RED 反向探针（破坏 permute imm `0xD8` → `0x00` → 2/2 dispatch test FAIL → 恢复完整循环）；**stash-swap 对比 AVX2 无统计显著收益**（Warm_Medium 3311 ns SSE2 vs 3324 ns AVX2 = +13 ns +0.4%，CV noise floor 内；Fallback 同期 3549 → 4184 ns +18% 证 run-to-run noise ~15%）；**根因**：ASCII glyph 宽度偏小（6-12 px），AVX2 8 px/iter 摊销不足 + cvtepu8_epi16+permute4x64 每 iter 多 2-3 cycles 开销 + dispatch static branch 开销；按用户选 (C) 智能阈值 `count >= kAVX2MinPixelsPerRow=16` 才走 AVX2，小 glyph 仍走 SSE2 直接路径；最终实测 Warm_Medium 3360 ns（与纯 SSE2 3311 统计等价 CV 内）；ctest **59/59 PASS**（含 11 pixel_blend GTests）；AVX2 代码作为 **CJK 大字号 / 未来硬件进化的 headroom** 保留
- ✅ /build Phase 7.4 finalize（2026-04-24）— 重采 `benchmarks/baseline/bench_drawtext.json`（--benchmark_min_time=1.0s Release 同机），新 baseline：Warm_Medium **5905→3499 ns (-40.7%)** / Warm_Short 975→677 ns (-30.6%) / Warm_Long 17456→10573 ns (-39.4%) / Cold_Medium 52873→28338 ns (-46.4%) / Fallback_Medium 3813→3608 ns 基本持平（对照组未参与优化）；`baseline/README.md` 更新 K7 ✅ resolved 条目 + 历史行 + DrawText 生成日期 2026-04-24；`tasks.md` TASK-20260424-03 归档折叠 + /build 成果段（7 Phase Before/After 表 + Phase × plan 0.6 第 6 数据点 0.42×）；`activeContext.md` 当前阶段 → 构建完成
- ✅ /reflect（2026-04-24）— 反思文档 `memory-bank/reflection/reflection-TASK-20260424-03.md`（13 段 / 7 改进建议 / 3 新模式候选 / plan × 0.6 第 6 数据点 0.34× 最窄档确认）；**3 大关键发现**：(F5) GCC `-O3` Granlund-Montgomery 魔数乘法已优于手写标量 /255 近似（通用编译器洞察）/ (F6) AVX2 8 px/iter 在 ASCII glyph 宽度无净收益，引入「异构工作负载 SIMD 尺寸阈值 dispatch」新模式 / (L4) API 层优化天花板远低于 plan 估算（Phase 6 B2 单 Phase 收益 = Phase 1-4 累计 6 倍）；**2 P1 建议** 追加到 `activeContext.md` 待处理事项：(#1) Godbolt 验证编译器魔数优化 / (#2) WSL2 warm-up 协议标准化，归档阶段落实到 `writing-plans.mdc` + `systemPatterns.md`
- ⏳ /archive（下一步）

### Phase 运行记录（当日同机可比）

| Phase | 候选 | warm_Medium (ns) | Δ vs baseline | 累计 | 达标? | Ctest |
|:-:|---|---:|---:|---:|:-:|:-:|
| 0 | baseline | **5412** | 0 | 0 | ❌ | n/a |
| 1 | A hb_buffer | **5397** | -37 (-0.7%) | -0.7% | ❌ | 26/26 ✅ |
| 2 | C FT_size | **5266** | -57 (-1.1%) | -2.7% (-146) | ❌ | 26/26 ✅ |
| 3 | E font cache | **5386** | -17 (-0.3%) | -0.5% (-26) | ❌ | 24/24 ✅ |
| 4 | D Put→ptr | **5311** | -67 (-1.25%) | -1.9% (-101) | ❌ | 46/46 ✅ |
| 5 | B1 /255 (**回退**) | 5367 | +56 (+1.1%) ❌ | -1.9% (-101) | ❌ | 51/51 ✅ (含 5 新) |
| 6 | B2 pre-clip+row ptr | **4689** | **-651 (-12.2%)** | **-13.4% (-723)** | ❌ (差 1689) | 53/53 ✅ |
| 7 | B3 SSE2 4px/iter | **3354** | **-1346 (-28.6%)** | **-38% (-2058)** | ❌ (差 354, 但 < Fallback) | 57/57 ✅ (含 9 blend) |
| 7b | + AVX2 dispatch (count≥16) | **3360** | +6 (statistical tie w/ SSE2) | -38% (-2052) | ❌ (差 360) | 59/59 ✅ (含 11 blend) |
| 7.4 finalize | baseline JSON 刷新 (min_time=1.0s) | **3499** | — (新 baseline 取代旧 5905) | -40.7% vs old baseline | ✅ < Fallback 3608 | 59/59 ✅ |

## 已完成任务

- TASK-20260424-01：Layout super-linear knee 根因调查（研究类）— 根因定位 (d) ArenaAllocator 4KB block malloc/free churn；默认 block 4096 → 32768；K2 R256 9.42×→4.18× / K3 R_flex 16.49×→6.40×；Phase 2 block-size 5 档扫描（4K/8K/16K/32K/65K），32K 为 Flex sweet spot；**K8 新发现**：65K block > L1D 触发抖动 → Arena 设计守则「block ≤ L1D」；`DefaultBlockSizeFitsLargeAllocations` GTest + RED 反向探针；ctest 892/892 PASS；**plan × 0.6 第 5 数据点 0.29×**（115 min plan / ~33 min 实测，历史最快，「最窄路径」子档样板）；3 新模式沉淀 `systemPatterns.md`（扫描型脚本化模板+双指标交叉 / 公开行为锚定内部约束 / 最窄路径子档）；残余 ~40% super-linear 拆出 TASK-20260424-02 → 归档 `memory-bank/archive/archive-TASK-20260424-01.md`
- TASK-20260419-13：流程规则 P0/P1 沉淀冲刺 — 3 条积压条目一次性闭环（P0 FetchContent proxy 守卫 / P1 smoke 工具链 grep / P1 多轮次 Build 中间态）；9 文件修改 / 8 commits / 反例追溯 7/7 通过（含 meta-dogfooding 实时自证）/ 10 验收 9 ✅ + 1 改进；跨类型估时收敛 plan × 0.6 通用协议（TASK-05/09/11/13 四数据点 3.4× → 4.2× → 1.5-2.0× → 1.67-1.86×）；5 新模式沉淀 `systemPatterns.md`（Meta-dogfooding Phase 0 / 基础假设核查 / 单一真相来源占位符 / 实证微调 spec / bench 估时校准扩展跨类型）；已 `--no-ff` 合并到 main `8a436ed` → 归档 `memory-bank/archive/archive-TASK-20260419-13.md`
- TASK-20260419-11：ImageCache::Load HashMap 化（K6 高 ROI 修复）— 双索引方案 (`Vector<Entry>` 保 ABI/Get O(1) + `HashMap<String, ImageHandle, StringHash, StringEq>`)；Hit<256> 1151.77 ns → 45.70 ns（25.2×↓），Hit<16> 50.87 → 44.05 ns；ctest 891/891 PASS（含 D3 `ClearAndReloadDeduplicates` 回归网，RED 反向探针验证有效）；Release `-O3` 0 errors；3 P1 + 3 P2 改进沉淀；plan 55-80 min vs 实测 ~35-40 min（估时校准协议首次实证 4.2×→2.0× 收敛）→ 归档 `memory-bank/archive/archive-TASK-20260419-11.md`
- TASK-20260419-09：Replay hot path 深度基准 + 真 ImageCache 通路（A+B 子集；2 bench exe / 15 BMs / 2 baseline JSON 入仓；K1 修正归因（fallback 非真路径）+ 真冷路径 14× 慢；K6 新发现 ImageCache::Load O(N) hit 路径 → 推 TASK-11 P1 高 ROI；K7 新发现 warm 真路径 1.6× 慢 fallback → 推 TASK-12 P2 触发型；落实「方案根因假设未先验证」P0 第 2 次完整应用 + grep 规则覆盖 CMake 链接可见性）→ 归档 `memory-bank/archive/archive-TASK-20260419-09.md`
- TASK-20260419-05：Layout + Render 性能基准（4 bench exe / 30 BMs / 4 baseline JSON 入仓 + K1 实测 DrawText 是 Replay hot path 820× FillRect / ImageCache 不是 → 推 TASK-09 候选；落实 P0 `writing-plans.mdc`「性能基准任务必检项」段）→ 归档 `memory-bank/archive/archive-TASK-20260419-05.md`
- TASK-20260419-07：修复 main Release `-Werror` 编译失败（fgets unused-result + BasicString copy ctor IPA array-bounds 误报；与 TASK-04 同元模式不同手法 noinline）→ 归档 `memory-bank/archive/archive-TASK-20260419-07.md`
- TASK-20260419-03：CSS 解析性能基准（Tokenizer 10 BM / Parser 11 BM / PropertyLookup 9 BM = 30 BMs + 3 baseline JSON 入仓 + Cluster 度量证 PropertyMap 均匀 → TASK-06 P1→P3 降级） → 归档 `memory-bank/archive/archive-TASK-20260419-03.md`
- TASK-20260419-04：修复 `enum_serialization.cc` Release `-Warray-bounds` 误报（去模板化 `Lookup<N>`，解锁 TASK-03 Phase 1） → 归档 `memory-bank/archive/archive-TASK-20260419-04.md`
- TASK-20260419-02：Google Benchmark 集成 + Foundation 性能基线（4 exe / 40 BM） → 归档 `memory-bank/archive/archive-TASK-20260419-02.md`
- TASK-20260419-01：流程规则沉淀 + P2 功能技术债收口 → 归档 `memory-bank/archive/archive-TASK-20260419-01.md`
- TASK-20260418-01：消化关键技术债务（#45/#46/#48/#50） → 归档 `memory-bank/archive/archive-TASK-20260418-01.md`
- TASK-20260414-01：功能补全 → 归档 `memory-bank/archive/archive-TASK-20260414-01.md`
- TASK-20260413-02：消化技术债务子集 → 归档 `memory-bank/archive/archive-TASK-20260413-02.md`
- TASK-20260413-01：QuickJS 脚本引擎集成 → 归档 `memory-bank/archive/archive-TASK-20260413-01.md`
- TASK-20260405-01：Foundation 基础库 → 归档 `memory-bank/archive/archive-TASK-20260405-01.md`
- TASK-20260405-02：Graphics HAL + Platform HAL → 归档 `memory-bank/archive/archive-TASK-20260405-02.md`
- TASK-20260405-03：DOM 树 + HTML 解析器 → 归档 `memory-bank/archive/archive-TASK-20260405-03.md`
- TASK-20260405-04：CSS 引擎 → 归档 `memory-bank/archive/archive-TASK-20260405-04.md`
- TASK-20260405-05：消化技术债务 → 归档 `memory-bank/archive/archive-TASK-20260405-05.md`
- TASK-20260405-06：Layout Engine 布局引擎 → 归档 `memory-bank/archive/archive-TASK-20260405-06.md`
- TASK-20260405-07：渲染管线（Render Pipeline） → 归档 `memory-bank/archive/archive-TASK-20260405-07.md`
- TASK-20260405-08：事件系统（Event System） → 归档 `memory-bank/archive/archive-TASK-20260405-08.md`
- TASK-20260405-09：脏区更新与重绘机制 → 归档 `memory-bank/archive/archive-TASK-20260405-09.md`
- TASK-20260405-10：事件循环与应用壳 → 归档 `memory-bank/archive/archive-TASK-20260405-10.md`
- TASK-20260405-11：C API 层 → 归档 `memory-bank/archive/archive-TASK-20260405-11.md`
- TASK-20260405-12：示例应用 → 归档 `memory-bank/archive/archive-TASK-20260405-12.md`
- TASK-20260405-13：CSS 动画系统（CSS Transitions） → 归档 `memory-bank/archive/archive-TASK-20260405-13.md`
