# 进度记录

## 当前任务

### TASK-20260419-11：`ImageCache::Load` HashMap 化（K6 高 ROI 优化）

**当前阶段：** 回顾完成（待 `/archive`）

**里程碑：**
- ✅ VAN（2026-04-19）：复杂度评估 Level 2，5 处 grep 实证（落实 P0 #4 完整应用），双索引方案确定
- ✅ Plan（2026-04-19）：design + plan 双文档落地，4 决策（D1-D4）确认（owned String key / 默认 16 cap / +1 ClearAndReloadDeduplicates 用例 / 4 phase 细粒度），plan 补 grep `string.h:30-56` SSO 内联确认（D1 最终验证），无需 `/creative`
- ✅ **Build（2026-04-19）：4 phase 全部完成，10/10 验收 ✅；plan 估 55-80 min，实测 ~35-40 min（按 TASK-05/09 高估趋势继续，约 1.5-2.0×，protocol 首次实证生效）**
- ✅ **Reflect（2026-04-19）：reflection-TASK-20260419-11.md 落地，6 改进建议（3 P1 + 3 P2，0 P0），3 项关键沉淀已写入 systemPatterns + activeContext 待处理事项闭环**
- ⏳ Archive

**VAN 阶段关键发现：**
- F1：`gfx::ImageHandle = u32` 是 1-based vector 下标，`Get(handle) = images_[handle-1]` 是 ABI — 必须**双索引**（保 Vector + 加 HashMap）
- F2：`String` 无 `==(String, String)` 直接 operator → plan 阶段试编译 `std::equal_to<String>`，否则自定义 `StringEq`
- F3：djb2 hash 模板可机械复刻自 `property.cc:84 StringViewHash`（~5 行）
- F4：唯一调用方 `application.cc:67/118` 仅持有指针 → 回归风险接近零
- F5：`image_cache_test.cc:55-65 DeduplicateSamePath` 已覆盖 dedup 契约（K6 关键回归点）

**核心目标：** Hit<16> 50.87 ns → ~10-30 ns；Hit<256> 1151.77 ns → ~10-30 ns（38-115×↓）

**Build 实测结果（10/10 验收 ✅）：**
- ✅ Hit<16>: 50.87 → **44.05 ns**（1.16×↓，目标 < 50 ns ✓）
- ✅ Hit<256>: 1151.77 → **45.70 ns**（**25.2×↓**，目标 < 100 ns ✓）— K6 命题完全解
- ✅ Miss 3833 → 3344 ns 不退化；Get 0.94 → 1.16 ns 噪声级；ReplayImageReal<16/64> 持平
- ⚠️ 已知微回归：Hit<1> 10.35 → 43.27 ns（HashMap 固有 ~32ns djb2+probe，绝对量微小，被 256 路径 25× 净增益完全压倒，plan D2 已注明可接受）
- ✅ ctest **891/891** PASS（含新增 `ClearAndReloadDeduplicates`，D3 RED probe：临时注释 `path_to_handle_.clear()` 该测试立即失败，证回归网精准）
- ✅ Release `-O3 -DNDEBUG` build-bench 0 errors
- ✅ baseline JSON 重生成入仓（带 repetitions=3 + median/mean/stddev/cv，cv 多数 < 1%）；2 README + techContext K6 状态全部翻牌「已解决」
- ✅ 5 commits：VAN `e7a9162`、Plan `dbdcffc`、P1 `ae72800`（加字段）、P2 `47ecb1d`（切 Load + 测试）、P3 `1a1ceb5`（bench + baseline + README）— P4 收尾即将提交

**关键观察（待 /reflect 沉淀）：**
- D3 `ClearAndReloadDeduplicates` RED probe **真实捕获**了双索引漏 clear bug — 此为「写测试时主动反向验证回归网有效性」的成熟实践
- Hit<1> 微回归说明「HashMap 不是万能金科玉律」：极小 N（≤2）下线性扫的 cache locality + 单次 memcmp 仍快于 HashMap 固有路径；本任务因 N 分布偏向 16/256 端故净增益巨大，但若未来出现 N 永远 ≤ 4 的新 cache 场景应慎用同构方案
- bench 任务 plan 估时再次高估（连 3 次：TASK-05 3.4× / TASK-09 4.2× / TASK-11 ~1.5-2.0×；本次因加入 P3 RED probe 实跑 + smoke 三件套手工 jq→python 改写有少量补偿）— 与 P1 反复出现的「bench plan 估时模板」P1 行直接相关

## 已完成任务

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
