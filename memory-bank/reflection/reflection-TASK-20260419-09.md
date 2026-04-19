# 回顾：TASK-20260419-09 — Replay hot path 深度基准 + 真 ImageCache 通路（A+B 子集）

**日期：** 2026-04-19
**任务 ID：** TASK-20260419-09
**复杂度级别：** Level 2-3
**分支：** `feature/TASK-20260419-09-replay-deepbench`
**TDD 模式：** 已豁免（bench 类任务，与 TASK-05 同例）— smoke 三件套替代

## 计划 vs 实际

| 维度 | 计划 | 实际 | 偏差原因 |
|------|------|------|---------|
| Phase 数 | 5 | 5 | ✅ 一致 |
| BM 数 | ~15（drawtext 6-8 + imagecache 7-8） | **15**（drawtext 8 + imagecache 7） | ✅ 一致 |
| 提交数 | 7（含 phase-1 wip + 4 phase + plan + finalize） | **6**（plan + 4 phase wip/feat/docs + finalize） | -1：phase-2/3 各按 1 commit 完成（无中间 wip） |
| 估时 | 3-3.5h | **~50 min** | **-4.2×**（系统性高估，与 TASK-05 同模式） |
| 文件变更 | 9（plan 列）| **15**（含 design + plan 文档 + 5 MB） | +6：plan 估时未计 design.md / plan.md / 5 MB；实际范围一致 |
| 工程意外 | 0 | **1**（PNG::PNG PRIVATE 不传递）| 1 处 grep 覆盖盲点；fix 1 行 ~1 min |
| 设计变更 | — | 0 | D1-D5 五决策全程不变 |
| K 命题数 | K1 + K5 判定（计划 2 项）| K1 修正归因 + K1' 真根因 + **K6 新** + **K7 新** = 4 项 | +2：B1 size 扫描 + warm vs fallback 对比直接产出意外发现 |

## 做得好的

### a. 「方案根因假设未先验证」P0 规则的全流程预防价值（最高价值）

本次是 TASK-05 archive 落实 `writing-plans.mdc §3 强制 grep` 规则后**第二个完整应用**，全流程量化预防价值：

| 阶段 | grep 次数 | 推翻假设 / 验证签名 | 预防的返工成本 |
|------|----------|-------------------|--------------|
| **VAN** | 4 处 | 推翻 K1（DrawText 是 FT+HB hot path）+ K5（需 fixture 文件复制）+ K2/K3（ArenaAllocator chunk grow）3 假设 | ~30 min cmake fixture 工程 + 整轮 fallback 误报 + 错误 root cause 追查 |
| **Plan** | 3 处 | 验证 `Element::SetAttribute` / `InternedString::Intern` / `Brush::Solid` API 签名 | ~10 min Phase 1 build 阶段 link error 返工 |
| **Build** | 0 处主动 | 仅 1 处工程意外（PNG::PNG 链接传递性，~1 min 修正） | — |

**结论**：P0 规则把不确定性前置到便宜的阶段（grep 几秒 vs build 失败几分钟到几十分钟），ROI 极高。本任务实际 build 时间 ~50 min vs plan 估 3.5h 的 4× 高估，主因是 P0 规则消除了通常 bench 任务 build 阶段的"试错-修正"循环。

### b. 「带否定判据的发现型 Phase」方法论 4/4 成功率

继 TASK-03（cluster 度量）+ TASK-05（DrawText hot path）之后，本次又有 2 次成功应用：

- **K1' 真根因**：`Real_Cold_Medium` 52763 ns vs `Real_Warm` 5807 ns = 9.1× → FT_Load + FT_Render 是冷路径主体（事先没有此 hypothesis，是 cold/warm 对比意外定位）
- **K6 ImageCache::Load O(N)**：`Hit/1` → `Hit/16` → `Hit/256` size 扫描发现 1162 ns 单次 Load 比整个 `ReplayImageReal<16>` 还慢 → HashMap 改造是高 ROI 候选（事先 hypothesis 是 Decode 是瓶颈）

**模式确认**：方法论 4/4 都是"否定原假设 + 意外定位真问题"。已写入 `systemPatterns.md` 验证次数表。

### c. 5 phase 划分稳定执行 + commit 粒度规整

每 phase 1 commit（plan / phase-1 wip / phase-2 feat / phase-3 feat / phase-4 docs / phase-5 chore-finalize），消息含数据要点 + K verdict + 工程注解。无大杂烩 commit，无主分支误提交。

### d. fixture 工程的极简实现

- ImageCache fixture：libpng 程序化 1×1 RGBA 写 `/tmp/vx_bench_<pid>_<i>.png`（VAN 阶段 grep 发现 image_decoder_test::CreateTestPng 模式，复用即可）+ `~PngFixturePool` 析构清理 — **0 个 `configure_file()`**，0 个 cmake 资源管理工程
- DrawText fixture：直接用系统 `/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf`（fail-fast `std::exit(1)` 而非静默 fallback，避免 K1 类误报再次发生）

### e. K6 / K7 新发现的副产品价值

两个新发现都不是计划目标，但都是高 ROI 优化候选：
- **K6**：ImageCache::Load 改 HashMap → 任何 > 30 张图的真实页面直接受益
- **K7**：当前 warm 真路径反而比 fallback 慢 1.6× → 揭示 hb_buffer 复用 + glyph 直接 raster 是真路径默认化的前置条件

两项已写入 baseline/README.md key findings + techContext.md 性能基线段，自然作为 follow-up 候选触发。

## 遇到的挑战

### a. 1 处 grep 覆盖盲点 — `vx_core PRIVATE PNG::PNG` 的链接传递性

**现象**：Phase 1 cmake reconfigure 报 `Target "bench_imagecache" links to target "PNG::PNG" but the target was not found`。

**根因**：VAN/Plan grep 检查覆盖了"PNG::PNG 是否存在 + vx_core 是否链接它"，但**没有检查链接可见性是 PRIVATE 还是 PUBLIC**。`vx_core PUBLIC vx_foundation vx_graphics PRIVATE vx_text vx_script PNG::PNG JPEG::JPEG` — PRIVATE 不向 consumer 传递。

**修正**：在 `benchmarks/CMakeLists.txt` 顶部加 `find_package(PNG REQUIRED)`，consumer 直接拿；1 行修复 ~1 min。

**教训**：P0 grep 清单应明确包含**链接可见性**（PUBLIC/PRIVATE/INTERFACE）确认，不仅仅是"该 target 在不在某处被链接"。

### b. Plan 估时 4× 高估（与 TASK-05 同模式）

| 任务 | Plan 估 | 实际 | 比率 |
|------|--------|------|------|
| TASK-05 | 4.25h | 75 min | **3.4×** |
| TASK-09 | 3.5h | ~50 min | **4.2×** |

**根因猜想**：bench 类任务的"已知模式高度可复用"特性（layout_corpus.h / vx_add_benchmark / smoke 三件套 / baseline JSON 协议都已成熟）使得每个 phase 的实际工作量远低于"从零写一个 bench exe"的 mental model。

**改进**：下次 bench 类任务，plan 估时应基于"复用率 = 80%+ 时单 BM 实际编写时间 ~3-5 min"的经验值校准，而非"从零编写 ~10-15 min/BM"。

### c. K5 命题判定方式改变 — 由 K6 取代

**计划目标**：B2 vs A2 vs FillRect 基线对比给出 K5 判定。
**实际**：K5 命题（"ImageCache 真实例无法在 bench 内构造"）已在 VAN 阶段证伪并解决（程序化 PNG），phase-2 的 7 BMs 自然产出 K6 新发现取代 K5 命题。验收 #10 改为以 K6 + B2 线性数据满足。

**评价**：偏离是计划本身的失效（K5 命题在 VAN 后已不成立），不是执行偏离；但 plan 验收应在 VAN 后同步修正，避免 verdict 字段对原命题的误指代。

## 经验教训

### 1. P0 grep 规则的覆盖范围应升级到包含「链接传递性」

本次 PNG::PNG 工程意外的根因是 grep 只看了"链接关系"，没看"传递可见性"。改进规则：

> **P0 grep 增补项（CMake target 链接类）**：当下游 target 链接上游 target 时，grep 必须确认链接关键字（`PUBLIC`/`PRIVATE`/`INTERFACE`），而非仅确认链接存在。`PRIVATE` 链接的传递依赖必须在下游 target 处独立 `find_package` / 重新链接。

将写入 `writing-plans.mdc` 「§3 强制 grep」段。

### 2. bench 类任务 plan 估时校准（成熟期 → 工作量大幅下降）

bench 类任务到了 TASK-09 已经是第 4 个（02 / 03 / 05 / 09），所有基础设施（layout_corpus.h / vx_add_benchmark / google/benchmark FetchContent / smoke 三件套 / baseline JSON 协议 / library_build_type=release 体检）都成熟。继续按"从零写"的 mental model 估时会系统性高估 3-4×。

下次 bench 任务 plan 阶段应在 §0 估时表中明确写：
> **复用率假设**：layout_corpus.h X% 复用 / vx_add_benchmark 100% 复用 / smoke 三件套 100% 复用 → 单 BM 实际编写时间 3-5 min（成熟期）

### 3. 「带否定判据」方法论已可视为成熟实践

4/4 成功率 + 都意外定位真问题（不是仅否定）→ 这不再是"实验性方法"。建议升级至 `systemPatterns.md` 的「成熟方法论」段（与「Render Bench 前置清单」并列）。

### 4. K6（ImageCache::Load O(N)）应作为高优先级 follow-up 立项

数据：cache size = 256 时单次 Load 1162 ns，**比整个 ReplayImageReal<16>（595 ns）还慢**。任何真实页面有数十张图就会被命中。改 `HashMap<String, ImageHandle>` 是机械替换，工作量小，ROI 极高。

建议：立 **TASK-20260419-11**（候选）或并入 TASK-10 之前优先处理。

## 改进建议（附优先级与落实方式）

| # | 建议 | 优先级 | 落实方式 | 目标文件 |
|---|------|--------|---------|---------|
| 1 | P0 grep 规则增补：CMake 链接关键字（PUBLIC/PRIVATE/INTERFACE）必查 | **P0 立即** | 写入 `writing-plans.mdc` §3 强制 grep 段 | `.cursor/rules/skills/writing-plans.mdc` |
| 2 | bench 类任务 plan 估时模板（含复用率假设 + 单 BM 3-5 min 经验值） | **P1 下次** | 追加 `activeContext.md` 待处理事项；下次 bench plan 应用 | `memory-bank/activeContext.md` |
| 3 | 「带否定判据的发现型 Phase」升级为成熟方法论 | **P2 长期** | 在 `systemPatterns.md` 该段标记"已 4/4 成熟"+ 删除"实验性"措辞 | `memory-bank/systemPatterns.md` |
| 4 | 立 TASK-20260419-11（候选）：ImageCache::Load 改 HashMap（K6 高 ROI 优化） | **P1 下次** | 追加 `tasks.md` 待立项候选 | `memory-bank/tasks.md` |
| 5 | bench 任务的 plan 验收 #10「K 命题判定」字段在 VAN 推翻后须同步更新命题指代 | P2 长期 | 写入 `writing-plans.mdc` 「VAN→Plan 同步」段 | `.cursor/rules/skills/writing-plans.mdc` |

## 技术改进建议

- **`SoftwareCanvas::DrawText` 真路径优化**（K7 follow-up）：hb_buffer 复用（避免每次 `hb_buffer_create` + `hb_buffer_destroy`）+ glyph bitmap 直接 raster 到 canvas（避免 GlyphCache → 中间 buffer → blit 两次内存拷贝）。预期 warm 真路径 5807 ns → < 3000 ns（小于 fallback）后才能默认开真路径。
- **`ImageCache::Load` 改 `HashMap<String, ImageHandle>`**（K6 follow-up）：现状 O(N) 字符串扫描。改为 HashMap 后 hit 路径稳定 ~10-30 ns（与现 hit/1 持平），不论 cache size。Get 已是 O(1) 不动。

## 反复模式识别

| 已知模式 | 历史频率 | 本次是否重复？ |
|---------|---------|-------------|
| 计划文件清单与实际变更不一致 | 9+ 次 | ❌ 不重复（本次 plan 9 vs 实际 15 的差异是 plan 没列 design.md + 5 MB，属"低估文档面"非"清单错"） |
| 子代理产出需大量返工 | 7+ 次 | N/A 本次未用子代理 |
| **前置依赖/环境/API 能力未验证** | 8+ 次 | ⚠️ **半重复** — VAN/Plan grep 7 处覆盖了 API + 文件存在性，但**漏了 CMake 链接可见性**（PNG::PNG 1 处工程意外）。**升级为 P0 规则增补**（建议 #1） |
| 非默认路径（流式/错误/缓存）遗漏验证 | 4+ 次 | ❌ 不重复（B1 hit/miss / DrawText cold/warm / fallback vs real 全部分支都覆盖） |
| 测试隔离问题 | 7+ 次 | ❌ 不重复（fixture 用 `getpid()` 隔离，自动清理） |
| 提交粒度偏离计划 | 7+ 次 | ❌ 不重复（5 phase 5 commit 完美对齐） |
| TDD 严格度与场景不匹配 | 11+ 次 | ❌ 不重复（plan §0 显式声明 bench 类豁免 + smoke 三件套替代） |

**核心结论**：本次最显著的反复模式是「前置依赖未验证」的**变体**（链接可见性盲点），其余 6 项反复模式全部规避。说明 P0 grep 规则 + plan 模板成熟度对反复模式的抑制效果显著（27 份历史回顾的 7 大模式中本次仅命中 1 项变体）。

## 安全评估

| 维度 | 状态 | 备注 |
|------|------|------|
| 输入验证 | N/A | bench 任务无外部输入 |
| 认证/授权 | N/A | 同上 |
| 数据保护（加密/脱敏） | N/A | 同上 |
| 依赖审计 | ✅ | 未引入新依赖（PNG::PNG 已是 vx_core 既有依赖；google/benchmark 已是 bench 既有依赖；DejaVuSans 是系统字体） |
| 错误信息脱敏 | N/A | bench 不对外输出 |
| 敏感数据处理 | N/A | fixture 是 1×1 RGBA 红像素，不含任何敏感数据；写到 `/tmp/vx_bench_<pid>_<i>.png` 析构清理 |

**结论**：本任务不涉及安全变更。
