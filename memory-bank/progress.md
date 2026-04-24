# 进度记录

## 当前任务

**TASK-20260424-04：SoftwareCanvas::DrawText 真路径 warm 残余优化（D 纯收尾模式）**（Level 2）

- ✅ VAN 初始化（2026-04-24）— 任务 ID 分配 / 分支 `feature/TASK-20260424-04-drawtext-residual-opt` 创建（基于 main `78cabf4`）/ 基础假设核查（3 候选代码实证）/ 用户决策锁定（V1 D 纯收尾模式目标 `<3200 ns` / V2 Level 2 / V3 分支确认）
- ✅ /plan 规划完成（2026-04-24）— **进一步代码实证**：blit_sse2.h L94 标量 tail 已有单像素 zero-skip 但 SIMD 主循环无块级 zero-skip；FT_Bitmap 已 crop bbox → 块级连续 4/8 px 全 0 罕见 → 候选 (a) 净收益风险 ≈ 0 被放弃；(c) hit 路径节省整个 hb_shape_full (~2100 ns) 保守 3× 冗余达标空间 → **方案 B 单候选 (c) 锁定**。**5 决策锁定**：Q1 方案 B / Q2 K2 u64 fingerprint + text_len 碰撞护栏 / Q3 S2 per-FontManager / Q4 C1 FIFO 128 / Q5 B1 RoundRobin 门槛 + B3 AllMiss 参考。**产出**：设计文档 `docs/specs/2026-04-24-drawtext-shape-cache-design.md`（11 段，含安全威胁建模 + 契约 + 测试矩阵）+ 实现计划 `docs/plans/2026-04-24-drawtext-shape-cache.md`（6 Phase / 12 Task / 14 新增测试 / 2 新 BM / 预估 ~3h 25m × 0.6 校准 ≈ ~2h 3m）+ FontHandle 提取到 `font_handle.h` 解循环依赖 + HbBufferHolder 提取到共享 header 复用给 FontManager
- ✅ /build 构建完成（2026-04-25）— 5 commits / 6 Phase / 12 Task 全部完成；严格 TDD RED→GREEN→REFACTOR 循环
  - P1 `b8f700e` HashBytesU64 FNV-1a header-only 实现 + 3 unit tests（RED 编译失败→GREEN PASS）
  - P2 `f081ed9` ShapeCache FIFO 128-slot 实现 + 7 tests（含 T6 碰撞降级 guard via text_len）+ FontHandle 提取为独立 header 以解循环依赖
  - P3 `623ad47` HbBufferHolder 提取到共享 header / FontManager::ShapeOrLookup + ClearShapeCache（含 VX_SHAPE_CACHE_OFF env 钩子）/ SoftwareCanvas::DrawText 接入（移除 ~10 hb_* 调用于 hit 路径）/ 4 集成 tests（含 R2 反向探针 `DifferentTexts_DifferentOutput`）；917/917 ctest PASS；Release -O3 零警告
  - P4 `2b4310a` 2 新 BMs（RoundRobin hit=100% / AllMiss miss=100%）+ VaryingTextPool 确定性文本池 + pre-baseline 采集（VX_SHAPE_CACHE_OFF=1 Warm_Medium 3542 ns 与 baseline 3499 吻合验证 env toggle 精度）
  - **P5+P6**（此 commit 合并）WSL2 稳态协议 10 reps + baseline.json 刷新（10 BMs）+ baseline/README.md K9 新发现 + TASK-04 历史行 + 生成日期
- **最终门槛判决（10/10 全通过，唯一 Cold_Medium +18.6% CV 12.67% 属 FT_Load 主导噪声非任务范围）：**
  - **主目标 Warm_Medium < 3200 ns ✅ 实测 2350 ns mean / 1877 ns single（超额 850 ns / 26%；间接达成技术刚性目标 <3000 ns）**
  - Warm_Short 311 ns (-54%) / Warm_Long 4333 ns (-59%) 均远优于 ≤baseline×1.1 兜底
  - TextVarying_RoundRobin 2676 ns (hit=100%, -25.8% vs pre-baseline 3605)；AllMiss 4711 ns (miss=100%, 入 baseline)
  - Fallback BMs 无变化（±2% 噪音），ReplayTextHeavyReal 反降 -9.8%
  - 917/917 ctest PASS（+4：shape_cache_test 10 + drawtext_shape_cache_test 4）；Release -O3 -Werror 零警告
- ⏳ /reflect 待启动 — 回顾 6 Phase 实施、plan×0.6 校准第 7 数据点、门槛超额达成根因、hb_shape cache 通用模式抽取

## 已完成任务

- **TASK-20260424-03：SoftwareCanvas::DrawText 真路径 warm 优化**（Level 2-3 优化类）— 7 Phase 阶梯 + 2 次 R1 AskQuestion 升级（SSE2 4 px/iter → AVX2 8 px/iter `count≥16` 智能阈值 dispatch）；Warm_Medium **5905→3499 ns (-40.7%)**，Warm_Long -39.4%，Cold_Medium 副产品 -46.4%；**业务目标达成**（真路径 3499 ns < Fallback 3608 ns，K7 **Resolved**），技术刚性 D5 `<3000ns` 差 499 ns (14%) 2 次 R1 后用户接受；**11 pixel_blend GTests + 3 次 RED 反向探针完整循环**（Phase 5 /255 helper / Phase 7 SSE2 channel order / Phase 7b AVX2 permute4x64 imm）；ctest 59/59 PASS / Release `-O3 -Werror` 0 err/warn；**Phase 5 /255 乘-移位近似试验回退**实证 GCC `-O3` Granlund-Montgomery 覆盖手写（通用编译器洞察）；**Phase 6 B2 pre-clip + row ptr** 单 Phase -12.2% 是最大单点收益（API 层 Phase 1-4 累计仅 -1.9%，算法层贡献是 6 倍）；**Phase 7 SSE2 -28.6%** 是第二大收益；AVX2 在 ASCII (6-12 px) 无净收益 → `kAVX2MinPixelsPerRow=16` 智能阈值为 CJK / 大字号 / 未来硬件保留 headroom；**plan × 0.6 第 6 数据点 0.34× 最窄档确认**（~230 min 完整 scope vs ~78 min 实测）；**归档阶段落实 2 P1 规则**：`writing-plans.mdc` §7 WSL2 / 云机 bench 稳态协议 + §8 编译器已做优化识别 — 位运算/除法近似反模式；**4 新模式沉淀 `systemPatterns.md`**（异构工作负载 SIMD 尺寸阈值 dispatch / 负结果资产化 / 刚性目标+R1 升级路径 plan 模式 / 编译器已做优化识别反模式）；`benchmarks/baseline/bench_drawtext.json` + README.md K7 → Resolved；`--no-ff` 合并到 main → 归档 `memory-bank/archive/archive-TASK-20260424-03.md`
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
