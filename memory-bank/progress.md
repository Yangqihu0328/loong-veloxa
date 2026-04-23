# 进度记录

## 当前任务

### TASK-20260424-01：Layout super-linear knee 根因调查（研究类）

- 🔵 **VAN 已完成**（2026-04-24）
  - 基线实测确认 knee：Flat/128→256 = 9.67× for 2×N（16.2M/s → 3.4M/s）
  - 关键 grep 实证：`LayoutBox` 用侵入式链表（非 Vector）→ 推翻候选根因 (a)
  - 关键数据：单元素 552B，N=256 工作集 142KB 远低于 L2 1280KB → 推翻候选根因 (c) cache 溢出
  - 保留候选根因：(d) malloc churn / (e) L1D 抖动 / (f) 算法内隐藏 O(N²)
- 🔵 **Plan 已完成**（2026-04-24）
  - 头脑风暴产出用户决策：**D1=A（阶梯验证）+ D2=A（全局 ArenaAllocator 默认 bump 4096 → Phase 2 扫描最优值）**
  - 设计文档：`docs/specs/2026-04-24-layout-knee-root-cause-design.md`（4 章节：现象/推翻证据/研究策略/文件变更清单）
  - 实现计划：`docs/plans/2026-04-24-layout-knee-root-cause.md`（6 Phase / 5 commits / plan 115 min / plan × 0.6 预期 ~70 min / 含 Phase 1B 升级分支）
  - smoke 工具矩阵（plan Phase 0 强制入清单）：`jq MISS → python3 兜底` / `rg MISS → Grep 工具` / `perf MISS → /usr/bin/time -v 兜底`；python3 / bc / awk OK
  - 前置守卫核对：**FetchContent 代理守卫 ❌ 不触发**（`build-bench/_deps/` 已离线缓存 benchmark + quickjs-ng 源码，命中「跳过条件」）；**CMake 链接方向 ❌ 不适用**（仅改头文件默认参数）；**测试基础设施 ✅ 通过**（`bytes_allocated()` 公开 getter 足够间接观测 block 边界）
  - 核心修复设计收敛为最小粒度：`arena_allocator.h:13` **1 行改动** + `arena_allocator_test.cc` **+1 GTest（`DefaultBlockSizeLocked` + RED 反向探针）** + **2 baseline JSON 刷新**
- 🟢 **Build 已完成**（2026-04-24，Phase 0-5，Phase 1B 未触发）
  - **Phase 0**（~3 min）：分支 `feature/TASK-20260424-01-layout-knee-root-cause` 建立；基线复现 R256=9.42× / R_flex=16.49× 与 VAN 数字匹配；smoke 工具 jq/rg/perf MISS 均命中预期兜底；commit `a46ff81`（docs(van+plan)）
  - **Phase 1**（~4 min）：临时改 arena 默认 4096→65536，rebuild bench，R256=3.61× / R_flex=8.36×；plan 判据落在 2.5×-6× 中间区间 → **(d) malloc churn 部分确认**，进 Phase 2
  - **Phase 2**（~5 min）：5 档扫描（4096/8192/16384/32768/65536）自动化 for 循环脚本一次跑完；**发现 K8：65K block 在 Flex 回弹（R_flex 7.40→8.36）**暗示 L1D 抖动（L1D 48KB 边界）；用户确认 **OPT_SIZE=32768**（Flex sweet spot），接受 plan 阈值 R256≤2.5/R_flex≤5 过严需实证调整；剩余 ~40% super-linear 立 **TASK-20260424-02**
  - **Phase 3**（~8 min）：RED → GREEN → 反向探针 TDD 完整闭环。`DefaultBlockSizeFitsLargeAllocations` 测试设计利用**指针连续性**（2× 16K 分配若 default≥32K 则连续，反之跨 malloc 块）；red 验证 p2-p1=16448（16K block header），green 验证 p2-p1=16384，revert 反向探针 FAIL，restore PASS；全量 ctest 892/892 PASS；commits `782d22d`（wip red）+ `0ad275d`（green fix）
  - **Phase 4**（~5 min）：build-bench Release 全量 rebuild 0 err/warn；bench_layout_{buildtree,flex} `--repetitions=3 --min_time=0.5s` 正式 baseline 入仓；3-reps 平均 R256=4.18× / R_flex=6.40×；README 更新「当前生成环境」+ K2/K3/K8 findings 段 + 2026-04-24 变更历史行；commit `102c7e5`（docs(bench)）
  - **Phase 5**（~8 min）：techContext Layout 性能基线段补 K2/K3 resolved + K8 新发现；activeContext → 构建完成；tasks.md 验收 10/10 + 候选区立 TASK-24-02；progress.md 里程碑（本段）；收尾 commit（进行中）
- **plan × 0.6 校准第 5 数据点**：115 min plan / ~33 min 实测 = **0.29×**（历史最快），研究型小补丁（脚本化实验 + 1 行改动 + 1 GTest）可作为「最窄路径任务」样板
- 🟣 **Reflect 已完成**（2026-04-24）
  - 回顾文档 `memory-bank/reflection/reflection-TASK-20260424-01.md`（10 段，Level 2-3 详细回顾）
  - **关键发现 5 项**：
    1. plan×0.6 第 5 数据点 0.29× 历史最快档 → 「最窄路径任务」子档（plan×0.3 预估基线）
    2. K8 新发现：ArenaAllocator block 过大触发 L1D 抖动（65K 让 Flex 回弹）→ Arena 设计守则「block ≤ L1D」
    3. RED 反向探针第 2 次成熟实践（TASK-11 首发）→ Mixed TDD 标配
    4. 阶梯验证 + 脚本化实验协同高效（Phase 1B 零触发，Phase 2 for 循环 40s 扫 3 档）
    5. 「公开行为锚定内部约束」测试哲学（指针连续性替代 getter/friend）
  - **改进建议**：2 P1（研究类任务阈值三段式 / test binary 路径矩阵）+ 3 P2（扫描型模板 / 路径宽度子档 / 测试设计模式）→ 追加 `activeContext.md` 待处理事项 + 沉淀 `systemPatterns.md` 3 段 + `techContext.md` K8（构建阶段已做）
  - **反复模式识别**：7 已知模式无本次重复（含TDD 严格度 11+ / 子代理返工 7+ / 计划一致性 9+ 等）；新变种「plan 验收阈值凭直觉凿实 + 部分达标无预设处理」记为观察项
- ⏳ **下一步：** `/archive` 归档 + merge 到 main（验收 10/10 已闭环）

## 已完成任务

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
