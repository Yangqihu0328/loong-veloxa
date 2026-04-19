# 任务跟踪

## 当前任务

### TASK-20260419-11：ImageCache::Load HashMap 化（K6 高 ROI 优化）

- **复杂度级别：** Level 2（多文件：image_cache.{h,cc} + tests + bench 复跑验证；机械替换 + 数据双索引设计；无 UI/架构空白）
- **状态：** 🟢 规划完成（待 `/build`）
- **当前阶段：** 规划中（`activeContext.md`）
- **分支：** `feature/TASK-20260419-11-imagecache-hashmap`（已创建，含 1 commit `e7a9162`）
- **设计文档：** `docs/specs/2026-04-19-imagecache-hashmap-design.md` ✅
- **实现计划：** `docs/plans/2026-04-19-imagecache-hashmap.md` ✅（4 phase / 1 GTest 新增 / ~55-80 min plan 估时 / 5 commits 含 VAN）
- **创建日期：** 2026-04-19
- **来源：** TASK-20260419-09 K6 拆出（`ImageCache::Load` hit 路径 O(N) 字符串扫描；Hit<256> 1151.77 ns > `ReplayImageReal<16>` 595 ns）；候选区已立项 P1 高 ROI
- **预期工作量：** ~1-2h（含 baseline 同机对比 + 单测 + bench 复跑确认 K6 命题已解）

#### 4 决策（D1-D4，plan 阶段头脑风暴用户确认）

| # | 维度 | 选择 | 理由 |
|---|------|------|------|
| D1 | HashMap key 类型 | **owned `String`** + 自定义 `StringHash` + `StringEq` | VAN F2 + plan 补 grep（`string.h:30-56` SSO 内联确认）：StringView key 在 vector resize 时悬挂；`std::equal_to<String>` 类型推导不确定 → 自定义 functor 兜底 |
| D2 | HashMap 初始容量 | 沿用 `kDefaultCapacity = 16`，不 reserve | 生产场景不频繁增长；TASK-09 K6 baseline 极端 256 entry 仅 ~4 次 rehash，分摊代价远低于命中收益 |
| D3 | 边界测试 | 加 1 个 `ClearAndReloadDeduplicates` GTest（~5 行） | 双索引最易出 bug 点 = `Clear()` 漏清 HashMap → 此测试是「双索引同步」回归安全网 |
| D4 | Phase 划分 | **4 phase**（细粒度提交）：P1 仅加字段 / P2 切 Load 路径 + 新测试 / P3 bench + baseline / P4 文档 | Incremental 改造模式；P1 引入字段不影响行为，「数据结构改造与行为切换分离」便于独立 review/回滚 |

#### Phase 划分（4 phase，详见 plan §2）

| Phase | 估时 | 文件 | 提交主题 |
|-------|------|------|---------|
| 1 | 10-15 min | image_cache.h（加字段 + functor） | wip phase-1 |
| 2 | 20-30 min | image_cache.cc（切 Load + Clear）+ test（+1 用例） | feat phase-2 |
| 3 | 15-20 min | bench 同机复跑 + baseline JSON + 2 README | docs phase-3 |
| 4 | 10-15 min | techContext + MB 收尾 | chore phase-4 |

#### 核心目标

将 `vx::image::ImageCache` 的 `Load(path)` hit 路径从 O(N) 字符串线性扫升级为 O(1) HashMap 查找，**同时保持现有 ABI 不变**（`gfx::ImageHandle` 仍是 1-based vector 下标，`Get(handle)` 保持 O(1)）。

| 指标 | 当前（baseline） | 目标 |
|------|----------------|------|
| `BM_ImageCacheLoad_Hit<1>` | 10.35 ns | 保持 ~10-30 ns |
| `BM_ImageCacheLoad_Hit<16>` | 50.87 ns | **降到 ~10-30 ns**（5×↓） |
| `BM_ImageCacheLoad_Hit<256>` | 1151.77 ns | **降到 ~10-30 ns**（38-115×↓） |
| `BM_ImageCacheLoad_Miss` | （不变，由 DecodeFromFile 主导） | 不退化 |
| `BM_ImageCacheGet` | （不变） | 不退化 |

#### VAN 阶段重大发现（5 项 grep 实证，落实「方案根因假设未先验证」P0 #4 完整应用）

| # | 命题 | grep 实证 | 影响设计 |
|---|------|----------|---------|
| F1 | 「`gfx::ImageHandle` 可改为不透明 token」 | ❌ 错。`graphics/image.h:49` `using ImageHandle = u32;` + `image_cache.cc:30` `&images_[handle - 1].image` — handle **必须保持 1-based vector 下标**否则破坏 `Get` O(1) 语义 + ABI | 必须**双索引设计**：保留 `Vector<Entry>` 提供 handle→image O(1) + 新增 `HashMap<String, Handle>` 提供 path→handle O(1) |
| F2 | 「`std::equal_to<String>` 默认可用」 | ⚠️ 风险。`string.h:178/190` 仅有 `operator==(BasicString, StringView)` + 反向，**无** `operator==(BasicString, BasicString)` 直接版本（依赖隐式转换）— `std::equal_to<String>` 实例化是否成功需 plan 阶段试编译验证 | 设计 `StringEq` 自定义 functor（仿 `property.cc:94 StringViewEq`）作为兜底 |
| F3 | 「需要新写 djb2 hash」 | ❌ 错。`property.cc:84-92 StringViewHash` 已是现成 djb2，可机械复刻为 `StringHash`（仅参数类型 `StringView` → `const String&`） | hash functor 工作量 ≈ 5 行复刻 |
| F4 | 「修改可能影响多个调用方」 | ❌ 错。`application.cc:67/118` 是**唯一调用方**（仅作为指针字段 + `&image_cache_` 引用传递）— 无 handle 数值递增/连续性的外部依赖 | 回归风险接近零 |
| F5 | 「需新加 dedup 测试用例」 | ❌ 错。`image_cache_test.cc:55-65 DeduplicateSamePath` 已覆盖 — 改造后必须保持「同 path Load 返回相同 handle」契约即可 | 现有 3 个测试用例（LoadAndGet / DeduplicateSamePath / LoadInvalidPath）已充分覆盖正确性 |

#### 前置验证（全部 ✅）

| 维度 | 结果 |
|---|---|
| 依赖可获取性 | ✅ `HashMap<K,V,Hash,Eq>` API 完整（`hash_map.h` Insert/Find/Erase/clear/reserve）；djb2 hash 模板（`property.cc:84`）已现成 |
| 环境就绪 | ✅ `build/` + `build-bench/` 复用，无 FetchContent → P0 git proxy 不触发 |
| 已有 artifact | ✅ `bench_imagecache.json` baseline 已入仓（TASK-09），改造后可直接同机对比 |
| 调用方影响 | ✅ 仅 `application.cc` 1 处持有指针，无 handle 数值依赖 |
| 测试覆盖 | ✅ 3 个 GTest 已覆盖核心契约（dedup 是 K6 改造关键回归点，已覆盖） |
| 待处理事项关联 | ✅ 落实 candidate TASK-11（候选区 P1 高 ROI）；与 TASK-10/12 不冲突 |
| Sticky ID 一致性 | ✅ 候选区 ID = TASK-20260419-11，本任务沿用 |

#### 验收标准（初拟，待 /plan 细化）

1. ✅ 双索引数据结构（`Vector<Entry>` + `HashMap<String, ImageHandle>`）实现，handle 保持 1-based vector 下标
2. ✅ `ImageCache::Load` hit 路径全部走 HashMap O(1)，O(N) 字符串扫被消除
3. ✅ 现有 3 个 GTest 全部通过（含 `DeduplicateSamePath` dedup 契约）
4. ✅ ctest 全量回归（890/890）零退化
5. ✅ Release `-Werror` 通路零编译失败
6. ✅ `bench_imagecache` 同机复跑：Hit<16> < 50 ns 且 Hit<256> < 100 ns（即 K6 量化命题"size=256 时 1162 ns"已解）
7. ✅ Miss 与 Get 性能不退化（Miss 由 DecodeFromFile 主导；Get 仍用 `images_[handle-1]`）
8. ✅ baseline JSON 重生成入仓（同名覆盖），README "Key findings" 段加入 K6 修复证据行
9. ✅ techContext.md「Replay-Deepbench 性能基线」段更新（K6 状态从"待优化"改为"已解决"）
10. ✅ Memory Bank 更新（K6 候选标记为已完成，并在反思中评估 1-2h 工作量是否准确）

#### 安全相关

否（性能优化任务，无外部输入新增/无认证路径/无新依赖；`HashMap` 已是 vx_core 内部成熟容器）。

<details>
<summary>TASK-20260419-09：Replay hot path 深度基准 + 真 ImageCache 通路（A+B 子集） — ✅ 已归档（点开查看历史）</summary>

### TASK-20260419-09：Replay hot path 深度基准 + 真 ImageCache 通路（A+B 子集）

- **复杂度级别：** Level 2-3（2 个新 bench exe + 复用 layout_corpus.h + 2 baseline JSON 入仓 + README 更新）
- **状态：** ✅ 已完成（已合并到 main，详见 `archive-TASK-20260419-09.md`）
- **回顾文档：** `memory-bank/reflection/reflection-TASK-20260419-09.md`
- **归档文档：** `memory-bank/archive/archive-TASK-20260419-09.md`
- **设计文档：** `docs/specs/2026-04-19-replay-deepbench-imagecache-design.md`
- **实现计划：** `docs/plans/2026-04-19-replay-deepbench-imagecache.md`（5 phase / 15 BMs / **plan 估 3.5h / 实际 ~50 min** / 7 commits）

#### 5 决策（D1-D5，plan 阶段头脑风暴用户确认）

| # | 维度 | 选择 |
|---|------|------|
| D1 | DrawText 层次 | **A1+A2**：A1 直接 `SoftwareCanvas::DrawText` 微基准（fallback / Real_Cold / Real_Warm）+ A2 端到端 Replay TextHeavy 真 vs fallback 对比 |
| D2 | 文本维度 | 短(2)/中(19)/长(124) char + glyph cache cold/warm 分组（量化 cache 收益） |
| D3 | ImageCache 维度 | B1 Load hit/miss × cache size {1, 16, 256}（验 O(N) 扫）+ B2 端到端真路径 N={16, 64} + B3 Get 单次 |
| D4 | fixture 策略 | 多张 distinct PNG（setup 写 256 张 1×1 RGBA 到 `/tmp/vx_bench_<pid>_<i>.png`，path 唯一避免 hit 污染） |
| D5 | Phase 划分 | 5 phase（P1 corpus 扩 + 2 smoke / P2 bench_imagecache / P3 bench_drawtext / P4 baseline JSON + README / P5 techContext + MB 收尾） |

#### Phase 划分（5 phase，详见 plan §1-5）

| Phase | 时间 | 文件 | BM 数 | 提交主题 |
|-------|------|------|-------|---------|
| 1 | 30 min | layout_corpus.h 扩展 + 2 smoke .cc + CMakeLists | 2 smoke | wip phase-1 |
| 2 | 50 min | bench_imagecache.cc 全套 | 7 BMs | feat phase-2 |
| 3 | 60 min | bench_drawtext.cc 全套 | 8 BMs | feat phase-3 |
| 4 | 30 min | 2 baseline JSON + 2 README | 0 | docs phase-4 |
| 5 | 30 min | techContext + systemPatterns + MB 收尾 | 0 | docs + chore phase-5 |

#### 验收标准（design §7 完整版，10 项）

1. ✅ 2 bench exe Release build 0 errors（`build-bench/`，phase-1 + phase-2 + phase-3 各确认）
2. ✅ bench_drawtext 8 BMs + bench_imagecache 7 BMs，全 exit 0
3. ✅ smoke 三件套全过（每 BM 数字非零 + `SetItemsProcessed > 0` + JSON `items_per_second > 0`，phase-1/2/3 全验证）
4. ✅ 11 现存 + 2 新 = 13 bench targets 共存零冲突（CMake 重 configure 全过）
5. ✅ 2 baseline JSON 入仓（`benchmarks/baseline/bench_drawtext.json` + `bench_imagecache.json`，default `--benchmark_min_time` ~0.5s，library_build_type=release）
6. ✅ `benchmarks/README.md` exe 表 11→13；新增 K1' / K1'' / K6 / K7 量级行；run-all 11→13；baseline 列表 7→9
7. ✅ `memory-bank/techContext.md` 新增「Replay-Deepbench 性能基线」段 + Render 基线 K1 修正归因；`systemPatterns.md` Render Bench 前置清单加 DrawText fallback gate（隐式契约 2→3）
8. ✅ Debug ctest 不引入回归（仅新增 bench exe，不触动核心源码 / 库 / 测试）
9. ✅ **K1 命题判定**（fallback 192 ns/char / cold real 2777 ns/char / warm real 305 ns/char — 完整三路证据）
10. ✅ **K5 命题判定**（K6 新发现取代：B2 ReplayImageReal 37 ns/cmd 线性 vs B1 Load hit/256 1162 ns 是真瓶颈）

#### 不需要 `/creative` 阶段

理由：所有设计决策已在 plan 阶段头脑风暴 D1-D5 完成；无 UI/算法/架构空白。

#### 元数据

- **分支：** `feature/TASK-20260419-09-replay-deepbench`（基于 main `bfe44ae`）
- **创建日期：** 2026-04-19
- **归档日期：** 2026-04-19
- **来源：** TASK-20260419-05 K1（DrawText 真路径未验证）+ K5（ImageCache 三阶段链断）触发；候选区已立项 P1

#### 范围决策（VAN /van 阶段拆分，2026-04-19）

原候选 TASK-09 包含 3 个独立子目标，VAN 范围检查后**拆为 2 任务**：

| 子目标 | 本次范围 | 原因 |
|---|---|---|
| A. DrawText 真路径微基准（FreeType + HarfBuzz + glyph_cache 拆解；fallback vs 真路径对比） | ✅ 本次做 | bench 类，与 TASK-05 同模式可复用 layout_corpus.h |
| B. 真 ImageCache 通路基准（Load + Get + cache hit/miss + nullptr 对比） | ✅ 本次做 | bench 类，同 A |
| C. Layout super-linear knee 根因（K2 + K3） | ❌ 拆为 TASK-20260419-10 | 异质（研究/调查类，可能产出 layout 算法重构 PR），混做会拉长任务且模式不统一 |

#### VAN 阶段重大发现（推翻 K1/K5 假设，落实「方案根因假设未先验证」P0 #3 提前止损）

| # | 原假设（TASK-05 K1/K5） | VAN grep 实证 | 影响 |
|---|---|---|---|
| F1 | K5：「ImageCache 真路径需 fixture 文件复制（`configure_file()` 拷 PNG）」 | ❌ 错。`tests/core/image/image_decoder_test.cc:12-41` 已有 `CreateTestPng()` 用 libpng 程序化构造 PNG 写 `/tmp/vx_test_<pid>.png` | B 子目标实现简化 ~1h（不用 cmake fixture 工程） |
| F2 | K1：「DrawText 8200 ns/cmd 是 FreeType+HarfBuzz 真路径」 | ⚠️ 可能误判。`SoftwareCanvas::DrawText` line 145 在 `font_manager==nullptr \|\| glyph_cache==nullptr` 时 fallback 到 `DrawTextFallback`（逐字符 FillRect）；当前 bench 不传 font_manager → **测的实际是 fallback 路径** | A 子目标核心命题改为「fallback vs 真路径谁是 820× 根因」，拆解维度更细 |
| F3 | K2/K3：「ArenaAllocator chunk grow 是 super-linear 根因」 | ❌ 错。`ArenaAllocator` 默认 4096 byte/block 不 grow（新 block 仍 4096）；malloc 4096 块 ~µs 级，无法解释 7.7→70 µs 的 10× 跳变 | C 子目标已拆为 TASK-10，候选根因表需重写 |
| F4 | TTF 字体 fixture 来源 | ✅ `/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf` 已被 font_manager_test 验证可用 | A 子目标无需打包字体 |
| F5 | 依赖可获取性 | ✅ FreeType + HarfBuzz + libpng 已在 vx_text/vx_core 链接 | 不触发 FetchContent → P0 git proxy 不触发 |

#### 前置验证（全部 ✅）

| 维度 | 结果 |
|---|---|
| 依赖可获取性 | ✅ FreeType / HarfBuzz / libpng 已在 vx_text / vx_core；DejaVuSans.ttf 系统已装 |
| 环境就绪 | ✅ `build-bench/` 复用，无 FetchContent → P0 git proxy 不触发 |
| 已有 artifact | ✅ TASK-05 4 bench 已在 main；新增 2 bench 文件名（`bench_drawtext.cc` / `bench_imagecache.cc` 或同等命名）不冲突 |
| 待处理事项关联 | ✅ 落实 candidate TASK-09（候选区 P1）；推 TASK-10（候选） |
| Sticky ID 一致性 | ✅ 候选区 ID = TASK-20260419-09，本任务沿用 |

#### 关键发现（4 项 → /reflect 输入）

| # | 发现 | 数值 | 后续 |
|---|------|------|------|
| K1' | TASK-05 「DrawText 8200 ns/cmd 是 FT+HB 真路径」**修正归因** | fallback 192 ns/char ≈ FillRect ×19；"820×"是 per-cmd 工作不可比性 | 已写入 techContext + systemPatterns DrawText fallback gate 隐式契约 |
| K1'' | DrawText 真路径**冷路径**才是 hot | `Real_Cold_Medium` 52763 ns / 19 char（**14× of fallback / 9.1× of warm**） | glyph_cache 已是 ROI 极高存量优化 |
| K6 | `ImageCache::Load` hit 路径 O(N) 字符串扫；size=256 时 1162 ns | hit/1 9.99 ns → hit/256 1162 ns（116×）；**比 ReplayImageReal<16> 595 ns 还慢** | 立 **TASK-20260419-11**（候选 P1 高 ROI）：HashMap 改造 |
| K7 | warm 真路径 5807 ns > fallback 3647 ns（1.6×） | hb_shape 固定开销 + glyph bitmap memcpy > 19 个 FillRect | 立 **TASK-20260419-12**（候选 P2 触发型）：hb_buffer 复用 + 直接 raster |

#### 提交清单（7 commits, on `feature/TASK-20260419-09-replay-deepbench`）

| # | SHA | 主题 |
|---|-----|------|
| 1 | `71e01ab` | docs(plan): TASK-20260419-09 replay deepbench design + plan |
| 2 | `f767231` | wip(TASK-20260419-09): phase-1 register bench_drawtext + bench_imagecache smoke |
| 3 | `c19dc97` | feat(bench): add bench_imagecache full suite (TASK-20260419-09 phase-2) |
| 4 | `8e55337` | feat(bench): add bench_drawtext full suite + K1 verdict (TASK-20260419-09 phase-3) |
| 5 | `913bf01` | docs(bench): TASK-20260419-09 phase-4 baselines + READMEs |
| 6 | `8cdb36c` | chore(build): finalize TASK-20260419-09 memory bank state (phase-5) |
| 7 | `6e0f775` | docs(reflect): add reflection for TASK-20260419-09 |

#### 安全相关

否（性能测量任务，无外部输入/无认证/无新依赖）。

</details>

<details>
<summary>TASK-20260419-05：Layout + Render 性能基准（4 个 bench exe） — ✅ 已归档（点开查看历史）</summary>

### TASK-20260419-05：Layout + Render 性能基准（4 个 bench exe）

- **复杂度级别：** Level 2-3（4 文件新建 + CMakeLists 更新 + README + 4 baseline JSON 入仓）
- **状态：** ✅ 已完成（已合并到 main，详见 `archive-TASK-20260419-05.md`）
- **回顾文档：** `memory-bank/reflection/reflection-TASK-20260419-05.md`
- **归档文档：** `memory-bank/archive/archive-TASK-20260419-05.md`
- **设计文档：** `docs/specs/2026-04-19-layout-render-benchmarks-design.md`
- **实现计划：** `docs/plans/2026-04-19-layout-render-benchmarks.md`（7 phase / ~25 BMs / ~4.25h / 7 commits）
- **分支：** `feature/TASK-20260419-05-layout-render-benchmarks`（基于 main `2985220`）
- **创建日期：** 2026-04-19
- **来源：** TASK-20260419-02 归档 P1 后续任务 + TASK-20260419-03 baseline 模式延伸应用
- **Sticky ID 说明：** 候选区固定 ID TASK-05（自 TASK-02 归档以来），与当天序号 08 不同；按候选区一致性使用 sticky ID（参考 TASK-04 同样反向插入先例）

#### 目标 API（已验证）

| API | bench 用途 |
|---|---|
| `vx::layout::LayoutEngine::Layout(doc, ctx)` | `bench_layout_buildtree` — 构造 BlockBox / InlineBox 树 + 完整 layout（默认 block flow） |
| `vx::layout::LayoutEngine::Layout(doc, ctx)` （带 `display: flex` DOM） | `bench_layout_flex` — 触发 flex code path（行/列、子项数、尺寸约束）|
| `vx::render::Record(root)` → `DisplayList` | `bench_render_record` — 树→ DisplayList 转换 |
| `vx::render::Replay(list, canvas)` | `bench_render_replay` — DisplayList → Canvas 命令重放 |

#### 衔接 TASK-03 模式（可复用）

- ✅ `vx_add_benchmark()` CMake 函数已支持额外链接库（TASK-03 引入）
- ✅ baseline JSON 入仓 + `benchmarks/baseline/README.md` 4-piece 失真兜底协议（TASK-03 沉淀）
- ✅ RangeMultiplier 公式 `ceil(log_m(hi/lo))+1`（TASK-03 实证）
- ✅ 程序化 corpus 构造 + 静态 cache（TASK-03 `StylesheetCorpus` / `InlineStyleCorpus` 模式可复刻为 `LayoutCorpus`）
- ✅ 「带否定判据的发现型 phase」模板（TASK-03 cluster 度量；本任务可用于发现 layout 慢路径）

#### 前置验证（全部 ✅）

| 维度 | 结果 |
|------|------|
| 依赖可获取性 | ✅ 4 API 全在 main，`vx_core` 已链 |
| 环境就绪 | ✅ `build-bench/` 复用，**无需 FetchContent → P0 git proxy 不触发** |
| 已有 artifact | ✅ 7 现存 bench（4 Foundation + 3 CSS）；新增 4 bench 文件名不冲突 |
| 待处理事项关联 | ✅ 落实 `activeContext.md` 长期项 P1 「Layout + Render 基准」 |

#### 4 决策（头脑风暴产出）

| # | 维度 | 选择 |
|---|------|------|
| 1 | Corpus 构造 | **A** 纯程序化 DOM API（`CreateElement` + `AppendChild` + `SetInlineDeclaration`） |
| 2 | Flex 输入维度 | **B** 二维 `BENCHMARK_TEMPLATE(rows, cols)` 5 固定多维点 + 1 嵌套 flex |
| 3 | ImageCache 覆盖 | **B** Record / Replay 各加 1 个 img-only 对比 BM（含 ImgVsNoImg/Cache vs /NoCache 判定 hot path） |
| 4 | baseline 入仓 | **A** 4 个全入仓 + 复用 TASK-03 baseline/README 4-piece 失真兜底协议 |

#### Phase 划分（7 phase，详见 plan §1-7）

| Phase | 时间 | 文件 | BM 数 | 提交主题 |
|-------|------|------|-------|---------|
| 1 | 30min | CMakeLists + 4 smoke .cc | 4 | wip phase-1 |
| 2 | 30min | layout_corpus.h | 0 | wip phase-2 |
| 3 | 45min | bench_layout_buildtree.cc | 9 → 14 行 | feat phase-3 |
| 4 | 45min | bench_layout_flex.cc | 6 | feat phase-4 |
| 5 | 30min | bench_render_record.cc | 5 | feat phase-5 |
| 6 | 45min | bench_render_replay.cc | 5-6 | feat phase-6 |
| 7 | 30min | README + baseline + MB | 0 | docs phase-7 |

#### 验收标准（design §9 完整版 — 全部已验证 ✅）

1. ✅ 4 bench exe Release build 0 errors
2. ✅ 各 bench BM 数量符合设计：buildtree 14 行 / flex 6 / record 5 / replay 5
3. ✅ 4 bench 全 exit 0 + 数字非零（含 Replay 修正后 list 非空，~10 ns/cmd FillRect）
4. ✅ 全 7+4=11 bench targets 共存、零冲突
5. ✅ Debug ctest 890/890 不变（`build/` 重新 build 验证）
6. ✅ 4 baseline JSON 入仓（全 release 体检 ✅）+ baseline/README + benchmarks/README 更新
7. ✅ techContext.md「Layout 性能基线」+「Render 性能基线」段已补（/reflect 阶段落实改进建议 #1，与 CSS 性能基线段并列）
8. ⚠️ ImageCache 对比 BM 给出明确判定 — **回答「不是 hot path」但**真测路径 layout→Record→Replay 三阶段都不传 cache → list 内 0 个 kDrawImage。改用 Replay TextHeavy 通路实测出真正 hot path = `DrawText`（820× FillRect）。ImageCache 真测推 TASK-09。验收意图（量化是否 hot path）已满足。
- 安全相关：否（性能测量任务，无外部输入/无认证/无新依赖）

#### 关键发现（5 项 → /reflect 输入）

| # | 发现 | 数值 | 后续 |
|---|------|------|------|
| K1 | Replay hot path = `DrawText` | 8200 ns/cmd vs FillRect 10 ns/cmd = 820× | 立 TASK-20260419-09（候选）DrawText / shaping micro-benches |
| K2 | Layout buildtree-flat super-linear knee | N=128→256 时 7.7→70 µs（10× for 2× N）| 调查 cache miss / arena grow |
| K3 | Layout flex 同源 super-linear | 8x8→16x16 时 4.9→73 µs（14.9× for 4× cells）| 同 K2 |
| K4 | Record 对 image 元素无额外开销 | image_handle=0 → RecordBox 跳过；ImgVsNoImg 16 ≈ Medium 64/4 | 设计正确 |
| K5 | ImageCache 真测 fixture 缺失 | DecodeFromFile I/O；layout 不传 cache → 三阶段链断 | 推 TASK-20260419-09 |

#### 提交清单（7 commits, on `feature/TASK-20260419-05-...`）

| # | SHA | 主题 |
|---|-----|------|
| 1 | `3eb9070` | docs(plan): TASK-20260419-05 layout/render benchmarks design + plan |
| 2 | (phase 1) | wip phase-1 register 4 smoke benches |
| 3 | (phase 2) | wip phase-2 add layout_corpus.h |
| 4 | (phase 3) | feat phase-3 bench_layout_buildtree full suite |
| 5 | (phase 4) | feat phase-4 bench_layout_flex 2D matrix |
| 6 | (phase 5) | feat phase-5 bench_render_record full suite |
| 7 | (phase 6) | feat phase-6 bench_render_replay + hot-path finding |
| 8 | (phase 7) | docs(bench): 4 layout/render baselines + README updates |
| 9 | `81d85cc` | chore(build): finalize TASK-20260419-05 memory bank state |
| 10 | `01833c6` | docs(reflect): add reflection for TASK-20260419-05 |

#### 安全相关

否（性能测量任务，无外部输入/无认证/无新依赖）。

</details>

<details>
<summary>TASK-20260419-07：修复 main Release `-Werror` 编译失败（2 处） — ✅ 已归档（点开查看历史）</summary>

### TASK-20260419-07：修复 main Release `-Werror` 编译失败（2 处）

- **复杂度级别：** Level 1（2 文件，修复路径明确）
- **状态：** 🔵 回顾完成（待 `/archive`）
- **回顾文档：** `memory-bank/reflection/reflection-TASK-20260419-07.md`
- **分支：** `feature/TASK-20260419-07-release-werror-fixes`（基于 main `b321482`）
- **创建日期：** 2026-04-19
- **来源：** TASK-20260419-03 P6 Release fresh build 副发现 + 长期项 P1 #2
- **关联模式：** 与 TASK-20260419-04 同源（Release 通路验证缺失反复模式，第 2 次因此暴露）

#### 修复目标（已实地复现 ✅）

| # | 文件 | 行 | 错误 | 修复方向 |
|---|------|----|----|---------|
| (a) | `tests/platform/memory_surface_test.cc` | 102 / 105 / 108 | `fgets -Werror=unused-result` | ✅ A1 `ASSERT_NE(std::fgets(...), nullptr)` 包裹（commit `8b57f8d`） |
| (b) | `veloxa/foundation/strings/string.h` 拷贝构造 | 61-74 | `memcpy` `-Werror=array-bounds` GCC IPA 误报 | ✅ B2 `[[gnu::noinline]]` 拷贝构造（GCC-only 守卫），commit `51d6ff1`。**B3 `__builtin_memcpy` 试验失败** — 诊断在 IPA 中端阶段，先于 fortify 展开 |

#### 验收标准（全部 ✅）

- ✅ `cmake --build build-bench -j` 全量构建零 `-Werror` 失败（38.7s）
- ✅ `ctest` Debug 全量回归 890/890 不变（2.46s）
- ✅ bench 7 目标继续 exit 0（sanity 跑 1ms 全 BM 数字正常）

#### 提交清单

| # | SHA | 描述 |
|---|------|------|
| 1 | `8b57f8d` | fix(tests/platform): assert fgets return value |
| 2 | `51d6ff1` | fix(foundation/strings): mark BasicString copy ctor noinline |
| 3 | `6fca7cb` | chore(build): finalize TASK-20260419-07 memory bank state |
| 4 | `0d749c1` | docs(reflect): add reflection for TASK-20260419-07 |
| 5 | `466ba01` | chore: ignore build-*/ directories |

#### 安全相关

否。

</details>

### 待立项候选

- ~~TASK-20260419-05：已立项为当前任务，详见上方「当前任务」段~~
- **TASK-20260419-06（建议，**P3 降级**）：** HashMap Hash Mixing 优化 — 触发条件改为「短字符串 ≠ 主用例 + 容器规模 > 1000 entry」的新场景出现时再立项（来源 TASK-03 P4 实测均匀降级）
- **TASK-20260419-08（候选，P3 触发型）：** `string.h` 剩余 3 处 runtime-size memcpy（line 45 SSO ctor / 150 Append / 230 GrowAndCopy）防御性 noinline 化。**触发条件**：未来 GCC 升级回归同类 `-Warray-bounds` 误报（来源 TASK-07 副发现）
- ~~TASK-20260419-09：已立项为当前任务（A+B 子集），详见上方「当前任务」段。VAN 阶段 grep 推翻 K5「需 fixture 文件复制」假设（复用 `image_decoder_test.cc::CreateTestPng()` 程序化构造写 /tmp）+ K1「DrawText 真路径」假设（实际走 fallback FillRect）~~
- **TASK-20260419-10（新增，TASK-05 K2/K3 + TASK-09 VAN 拆出，建议 P2 触发型）：** Layout super-linear knee 根因调查（**研究类**，非 bench 类）— buildtree N=128→256 / flex 8x8→16x16 同源 super-linear（10×～15×）。**VAN 阶段已否定 ArenaAllocator chunk grow 候选根因**（默认 4096 不 grow，量级不符）；剩余候选：(a) `LayoutBox` 内 `Vector<LayoutBox*> children` 扩容序列（首发→第 N 次扩容产生连续 reallocate）；(b) layout 算法本身 O(N²) 路径（margin collapsing / line box reflow）；(c) 数据局部性 cache miss（256 box × ~100 byte = 25.6 KB ≤ L1d 32KB，但 prefetch pattern 可能 break）。**预期产出**：调查报告 + （可能）layout 算法重构 PR。**触发条件**：TASK-09 完成后立项；如新增 layout 性能问题先于 TASK-09 完成出现可优先此项
- ~~TASK-20260419-11：已立项为当前任务（分支 `feature/TASK-20260419-11-imagecache-hashmap`），详见上方「当前任务」段。**VAN 阶段重大发现**：handle 必须保持 1-based vector 下标（保 Get O(1) + ABI），改造路径定为**双索引**（Vector + HashMap）；djb2 模板可复刻自 `property.cc:84 StringViewHash`~~
- **TASK-20260419-12（新增，TASK-09 K7 拆出，建议 P2 触发型）：** `SoftwareCanvas::DrawText` 真路径优化（**优化类**）— 当前 warm 真路径 5807 ns > fallback 3647 ns（1.6×），阻碍未来默认开真路径。候选：(a) `hb_buffer` 复用避免每次 alloc/free；(b) glyph bitmap 直接 raster 到 canvas 避免 GlyphCache → 中间 buffer → blit 两次拷贝。**预期产出**：warm 真路径 < 3000 ns 后默认真路径开关。**触发条件**：当真路径默认化提上日程时

## 任务历史

| 任务 ID | 描述 | 状态 | 完成日期 | 归档文档 |
|---------|------|------|---------|---------|
| TASK-20260419-09 | Replay 深度基准（2 bench exe / 15 BMs / 2 baseline JSON）；修正 K1 归因（fallback 非真路径），定位真冷路径 14× 慢；新发现 K6 ImageCache::Load O(N) + K7 warm 真路径 1.6× 慢；推 TASK-11/12 | ✅ 已完成 | 2026-04-19 | `archive-TASK-20260419-09.md` |
| TASK-20260419-05 | Layout + Render 性能基准（4 bench exe / 30 BMs / 4 baseline JSON）；K1 实测 DrawText 是 Replay hot path（820× FillRect），ImageCache 不是；推 TASK-09 | ✅ 已完成 | 2026-04-19 | `archive-TASK-20260419-05.md` |
| TASK-20260419-07 | 修复 main Release `-Werror` 编译失败（fgets unused-result + BasicString copy ctor IPA array-bounds 误报）— 与 TASK-04 同元模式不同手法 | ✅ 已完成 | 2026-04-19 | `archive-TASK-20260419-07.md` |
| TASK-20260419-03 | CSS 解析性能基准（Tokenizer / Parser / PropertyLookup）— 30 BMs + 3 baseline JSON + Cluster 度量证 PropertyMap 均匀 | ✅ 已完成 | 2026-04-19 | `archive-TASK-20260419-03.md` |
| TASK-20260419-04 | 修复 `enum_serialization.cc` Release `-Warray-bounds` 误报（解锁 TASK-03 Phase 1） | ✅ 已完成 | 2026-04-19 | `archive-TASK-20260419-04.md` |

<details>
<summary>已归档：TASK-20260419-02 — 补充 Google Benchmark 集成与 Foundation 性能基准（点开查看历史细节）</summary>

- **复杂度级别：** Level 2（简单增强）
- **状态：** ✅ 已完成（已合并到 main）
- **分支：** `feature/TASK-20260419-02-benchmarks`（已删除）
- **创建日期：** 2026-04-19
- **归档日期：** 2026-04-19
- **来源：** TASK-20260405-01 Foundation 延期项 P1#1（"Benchmark 延期 — 需 google benchmark，网络恢复后补充"）
- **设计规格：** `docs/specs/2026-04-19-benchmarks-design.md`
- **实现计划：** `docs/plans/2026-04-19-benchmarks.md`
- **回顾文档：** `memory-bank/reflection/reflection-TASK-20260419-02.md`
- **归档文档：** `memory-bank/archive/archive-TASK-20260419-02.md`
- **实际提交：** 10 个（plan + 7 build + reflect + archive）
- **实际新增 BM 用例：** 40（allocators 13 + containers 8 + hash_map 10 + strings 9）；GTest 基线 890/890 不变

#### 关键决策（头脑风暴 4 轮，全部 A 方案）

| 维度 | 决策 |
|------|------|
| 范围 | 仅 Foundation（allocators / containers / strings / hash_map），不含 CSS/Layout/Render |
| Executable 粒度 | 一文件一 exe（4 个） |
| 用例深度 | 中等（每组件核心操作 + 1-2 个尺寸/容量梯度） |
| 结果留存 | 仅控制台 + README 给出 JSON 导出（不提交 baseline 文件） |

#### Phase 概览

| Phase | 内容 | 提交 |
|-------|------|------|
| 0 | 基线验证（890 测试全绿；FetchContent 通路验证） | 0 |
| 1 | 创建分支 + `benchmarks/CMakeLists.txt` + 4 个 smoke .cc | 1 |
| 2 | `bench_allocators.cc`（Malloc/Arena/Pool，13 BM） | 1 |
| 3 | `bench_containers.cc`（Vector/SmallVector/IntrusiveList，8 BM） | 1 |
| 4 | `bench_hash_map.cc`（insert/lookup hit&miss/rehash，10 BM） | 1 |
| 5 | `bench_strings.cc`（BasicString SSO+heap、InternedString，9 BM） | 1 |
| 6 | `README.md` + `techContext.md` 更新（依赖表 + Benchmark 启用段） | 1 |
| 7 | 收尾验证 + 依赖安全审计（google/benchmark v1.9.1 CVE） + MB 收尾 | 1 |

#### 安全相关

否（纯内部性能测量；唯一新依赖 google/benchmark v1.9.1 经 CVE 审计：GitHub Security Advisories 0 条）

#### 副发现

`HashMap::Find` 对小整数 key 在 cap≥1024 时严重 cluster（LookupHit/16384=9µs vs n=64=69ns）。根因 `H1=h>>7` + `std::hash<int>` 恒等映射使所有起始探测位置压在 [0,127]。已记入 `benchmarks/README.md` ⚠️ 量级表 + activeContext 待处理事项 P2，建议立项 TASK-20260419-05。

</details>

<details>
<summary>已归档：TASK-20260419-01 — 流程规则沉淀 + P2 功能技术债收口（点开查看历史细节）</summary>

- **复杂度级别：** Level 3（中等功能；跨「.cursor 规则」+ 「Script/CSS/Event」子系统）
- **状态：** ✅ 已完成（已合并到 main）
- **分支：** `feature/TASK-20260419-01-rules-and-debt`（已删除）
- **创建日期：** 2026-04-19
- **归档日期：** 2026-04-19
- **设计规格：** `docs/specs/2026-04-19-rules-and-debt-design.md`
- **实现计划：** `docs/plans/2026-04-19-rules-and-debt.md`
- **回顾文档：** `memory-bank/reflection/reflection-TASK-20260419-01.md`
- **归档文档：** `memory-bank/archive/archive-TASK-20260419-01.md`
- **预估提交：** 21 个；**实际：** 23 个（含 reflect + archive）
- **预估新增测试：** +22；**实际：** +34（基线 856 → 890）

#### 需要创意阶段的组件

**无。** 头脑风暴阶段已为 Part B 三个设计点（B5 Enum 反查表 / B6 反向析构机制 / B7 精确移除 API）固化方案：

| 子项 | 决策 | 模式来源 |
|------|------|---------|
| B5 | 完整反查表 `enum_serialization.{h,cc}` | 枚举 + 元数据表驱动模式（property.cc 同构） |
| B6 | EventManager `AddDestructionObserver` 回调 | Push 回调失效触发模式（SetInvalidationCallback 同构） |
| B7 | u64 ListenerToken + `RemoveByToken` API | 不透明句柄模式（与现有 `JSValue` 句柄风格一致） |

无需进一步探索 → 跳过 `/creative`，直接 `/build`。

#### Phase 概览

| Phase | 内容 | 预计耗时 |
|-------|------|---------|
| 0 | 基线验证（856 测试全绿） | 5 min |
| A1 | writing-plans.mdc 增 5 段 | 30 min |
| A2 | subagent-development.mdc 增 3 段 | 20 min |
| A3 | 新建 integration-testing.mdc + 注册 | 20 min |
| A4 | techContext.md「FetchContent 与代理」段 | 10 min |
| B5 | enum_serialization.{h,cc} + dom_bindings 接入 + 测试 | 45 min |
| B6 | EventManager DestructionObserver + DomBindings 注册 | 30 min |
| B7 | Listener Token API + 精确 removeEventListener | 60 min |
| 8 | 收尾验证 + Memory Bank 更新 | 20 min |
| **合计** | | **~4 h** |

#### 任务范围

**Part A — 流程规则沉淀（14 条 P1 待办，纯 .mdc 文件修改）**

1. 计划模板（`.cursor/rules/skills/writing-plans.mdc`）增段：
   - FetchContent 引入第三方编译时校验根 `add_compile_options(-Werror...)` 是否仅限 `$<COMPILE_LANGUAGE:CXX>` 或目标级（来源 TASK-20260413-01）
   - 测试基础设施审计——列出测试需访问的内部状态及访问路径（来源 TASK-11）
   - 边界输入清单——每个 Phase 列出非默认路径（来源 TASK-06）
   - 跨模块参数透传修改时调用链端到端验证（来源 TASK-09）
   - 设计文档管线注入点须附代码级可行性验证（来源 TASK-13）
2. 子代理 prompt（`.cursor/rules/skills/subagent-development.mdc`）增段：
   - 跨模块数据格式段（来源 TASK-02）
   - LayoutBox 坐标计算时须包含 x/y 语义定义 content origin vs border box origin（来源 TASK-07）
   - 并行子代理可行条件：无共享 .cc + 共享 .h 已创建 + CMakeLists.txt 已更新（来源 TASK-08）
   - 存根文件预创建策略（来源 TASK-04）
   - 合并 Phase 给子代理的策略（来源 TASK-04）
3. 集成测试规范——新建 `.cursor/rules/skills/integration-testing.mdc`：
   - 集成测试优先验证数据格式一致性（来源 TASK-02）
   - 必须使用真实 HTML/CSS 解析器，禁止仅用手动 DOM 构建（来源 TASK-06）
   - 像素验证优先用 DisplayList 检查和区域扫描，避免硬编码坐标（来源 TASK-07）
   - CSS 颜色测试禁止与 gfx::Color 编程常量直接比较，必须通过 CssColorToGfx 转换（来源 TASK-07）
   - 禁止使用 HTML inline style（BuildTree 不解析），必须用外部 CSS 选择器（来源 TASK-08）
   - 集成测试模板增加 API 备忘清单：html::Parser / FindElement / HandleInput（来源 TASK-13）
4. 文档：含 Git 拉取依赖的文档（`techContext.md` 或 README）写明代理 `http_proxy`/`HTTPS_PROXY` 与首次 `cmake` 注意点（来源 TASK-20260413-01）

**Part B — P2 功能技术债（含代码与测试）**

5. **#46 续作**：`StyleGetProp` Enum 读路径（display 等）——构建 `PropertyId→enum string` 反查表，覆盖 display/position/visibility 等 Enum 类型 CSS 属性
6. **#50 续作**：`DomBindings`/`EventManager` 析构顺序硬约束——目前仅保证 `DomBindings` 先析构；反向场景（`EventManager` 先销毁）需弱引用机制
7. **#47 续作**：`removeEventListener` 按 `(type, handler)` 精确移除——扩展 `EventManager::RemoveEventListenersByHandler` API 并在 `dom_bindings` 调用

#### 安全相关

否（纯内部技术债 + 流程规则；不涉及外部输入/认证/存储/部署）

</details>

## 任务历史

| ID | 描述 | 最终状态 | 归档日期 |
|----|------|---------|---------|
| TASK-20260419-02 | 补充 Google Benchmark 集成与 Foundation 性能基准（4 个 bench exe / 40 BM 用例） | ✅ 已完成 | 2026-04-19 |
| TASK-20260419-01 | 流程规则沉淀 + P2 功能技术债收口（14 条 P1 流程规则 + CSS Enum 序列化 / EventManager 反向析构 / removeEventListener 精确移除） | ✅ 已完成 | 2026-04-19 |
| TASK-20260418-01 | 消化关键技术债务（#45 dom_bindings 全局状态 / #46 StyleGetProp 读路径 / #48 hb_font 缓存 / #50 addEventListener 生命周期） | ✅ 已完成 | 2026-04-18 |
| TASK-20260414-01 | 功能补全（border-radius / 字体渲染 / 图片支持 / JS-DOM 绑定） | ✅ 已完成 | 2026-04-14 |
| TASK-20260413-02 | 消化技术债务子集（#41 `HashMap` const 迭代、#39 测试像素头、`active_count_` 移除） | ✅ 已完成 | 2026-04-13 |
| TASK-20260413-01 | QuickJS 脚本引擎集成（quickjs-ng、`vx_script`、`QuickjsEngine`） | ✅ 已完成 | 2026-04-13 |
| TASK-20260405-01 | 构建 Foundation 基础库（内存管理/容器/字符串/日志） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-02 | 构建 Graphics HAL 图形抽象层与 Platform HAL 平台抽象层 | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-03 | 构建 DOM 树 + HTML 解析器 | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-04 | 构建 CSS 引擎（Tokenizer/Parser/选择器/属性/层叠） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-05 | 消化技术债务（Arena/HashMap/PPM/Parser Error） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-06 | 构建 Layout Engine 布局引擎 | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-07 | 构建渲染管线（Render Pipeline） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-08 | 构建事件系统（Event System） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-09 | 脏区更新与重绘机制 | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-10 | 事件循环与应用壳（EventLoop / Application Shell） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-11 | C API 层（对外嵌入接口） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-12 | 示例应用（examples/hello） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-13 | CSS 动画系统（CSS Transitions） | ✅ 已完成 | 2026-04-05 |
