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
- ⏳ /build Phase 5-6 核心 blit loop 优化（B1 /255 近似 + B2 预裁剪）
- ⏳ /build Phase 7 baseline 刷新 + MB 收尾
- ⏳ /reflect + /archive

### Phase 运行记录（当日同机可比）

| Phase | 候选 | warm_Medium (ns) | Δ vs baseline | 累计 | 达标? | Ctest |
|:-:|---|---:|---:|---:|:-:|:-:|
| 0 | baseline | **5412** | 0 | 0 | ❌ | n/a |
| 1 | A hb_buffer | **5397** | -37 (-0.7%) | -0.7% | ❌ | 26/26 ✅ |
| 2 | C FT_size | **5266** | -57 (-1.1%) | -2.7% (-146) | ❌ | 26/26 ✅ |
| 3 | E font cache | **5386** | -17 (-0.3%) | -0.5% (-26) | ❌ | 24/24 ✅ |
| 4 | D Put→ptr | **5311** | -67 (-1.25%) | -1.9% (-101) | ❌ | 46/46 ✅ |
| 5 | B1 /255 | _待_ | _待_ | _待_ | _待_ | _待_ |
| 6 | B2 pre-clip | _待_ | _待_ | _待_ | _待_ | _待_ |

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
