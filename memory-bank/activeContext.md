# 活跃上下文

## 当前阶段
回顾中

**TDD 模式：** 已豁免（bench 任务，与 TASK-05 同例）— smoke 三件套全过：15 BMs（drawtext 8 + imagecache 7） / 全 exit 0 / items_processed > 0 / JSON items_per_second > 0

## 当前任务

**TASK-20260419-09：Replay hot path 深度基准 + 真 ImageCache 通路（A+B 子集）**

- **复杂度级别：** Level 2-3（2 bench exe + 复用 layout_corpus.h + 2 baseline JSON）
- **分支：** `feature/TASK-20260419-09-replay-deepbench`（基于 main `bfe44ae`）
- **设计文档：** `docs/specs/2026-04-19-replay-deepbench-imagecache-design.md` ✅
- **实现计划：** `docs/plans/2026-04-19-replay-deepbench-imagecache.md` ✅（5 phase / ~15 BMs / ~3.5h / 7 commits）
- **范围决策（VAN）：** 原候选含 3 子目标，VAN 范围检查后**拆为 A+B 本任务（bench 类）+ TASK-10 后续（C 研究类 layout super-linear 调查）**
- **VAN 阶段重大发现（推翻 TASK-05 K1/K5 假设）：**
  - F1：K5「ImageCache 需 fixture 文件复制」❌ — `image_decoder_test.cc::CreateTestPng()` 已有程序化 PNG 构造写 `/tmp`，复用即可（节省 ~30 min cmake 工程）
  - F2：K1「DrawText 8200 ns/cmd 是 FreeType+HarfBuzz 真路径」⚠️ — 实际走 fallback（line 145 `font_manager==nullptr` gate）→ A 子目标核心命题改为「fallback vs 真路径谁是 820× 根因」
  - F3：K2/K3「ArenaAllocator chunk grow 是 super-linear 根因」❌ — 默认 4096 不 grow，量级不符 → C 拆为 TASK-10
- **Plan 阶段补 grep 验证（第二轮 P0 规则预防）：** `Element::SetAttribute(InternedString, String)` ✅ + `InternedString::Intern(StringView)` ✅ + `Brush::Solid(Color)` ✅ + `SoftwareCanvas` ctor 后两参可选 ✅ → design §5.3 代码可直接照写，build 阶段无需返工
- **5 决策（D1-D5，用户确认）：** A1+A2 / cold+warm 维度 / B1×3 cache size + B2×2 + B3 / 多张 distinct PNG fixture / 5 phase
- **意义：** 「方案根因假设未先验证」P0 在 TASK-05 /archive 落实新规（`writing-plans.mdc` §3 强制 grep）后**第一+第二次产生预防价值** — VAN+Plan 双阶段共 grep 7 处，一次性发现 3 个原假设全错 + 4 个 API 签名验证通过
- **构建完成（5 phase / 5 commits + finalize）：**
  - Phase 1 ✅ `f767231` corpus 扩 + 2 smoke + CMake 注册
  - Phase 2 ✅ `c19dc97` bench_imagecache 全套 7 BMs（K6 新发现）
  - Phase 3 ✅ `8e55337` bench_drawtext 全套 8 BMs（K1 修正归因 + K7 新发现）
  - Phase 4 ✅ `913bf01` 2 baseline JSON + 2 README
  - Phase 5 ⏳ techContext + systemPatterns + MB 收尾（本 commit）
- **核心数据成果：**
  - **K1 修正归因**：TASK-05 8200 ns/cmd 实为 fallback 路径（FillRect ×19/cmd），"820×"是 per-cmd 工作不可比性，非真路径慢源
  - **K1 真根因**：`Real_Cold_Medium` 52763 ns / 19 char = FT_Load_Glyph + FT_Render_Glyph 是冷路径主体（vs warm 9.1×）
  - **K6 新发现（最高 ROI 候选）**：`ImageCache::Load` hit 路径 O(N) 字符串扫，size=256 时 1162 ns > ReplayImageReal<16> 595 ns → HashMap 改造极高价值
  - **K7 新发现**：当前 warm 真路径 5807 ns > fallback 3647 ns（1.6×），如默认开真路径需先优化 hb_buffer 复用 + 直接 raster
- **下一步：** `/reflect` 进行回顾

## 最近归档

- `memory-bank/archive/archive-TASK-20260419-05.md`（TASK-20260419-05 Layout + Render 性能基准 — 4 bench exe / 30 BMs / 4 baseline JSON 入仓；K1 实测 DrawText 是 Replay hot path（820× FillRect），ImageCache 不是；推 TASK-20260419-09 候选；落实 P0 #2 `writing-plans.mdc`「性能基准任务必检项」段；已 `--no-ff` 合并到 main `d0999e8`）
- `memory-bank/archive/archive-TASK-20260419-07.md`（TASK-20260419-07 修复 main Release `-Werror` 编译失败 2 处 — fgets unused-result + BasicString copy ctor IPA array-bounds 误报；与 TASK-04 同元模式不同手法 noinline；已 `--no-ff` 合并到 main `206d227`）
- `memory-bank/archive/archive-TASK-20260419-03.md`（TASK-20260419-03 CSS 解析性能基准 — 30 BMs + 3 baseline JSON 入仓 + Cluster 度量证 PropertyMap 均匀 → TASK-06 P1→P3，已 `--no-ff` 合并到 main `660313a`）
- `memory-bank/archive/archive-TASK-20260419-04.md`（TASK-20260419-04 修复 `enum_serialization.cc` Release `-Warray-bounds` 误报，已合并到 main `a09ad1e`）
- `memory-bank/archive/archive-TASK-20260419-02.md`（TASK-20260419-02 Google Benchmark 集成 + Foundation 性能基线，已合并到 main）
- `memory-bank/archive/archive-TASK-20260419-01.md`（TASK-20260419-01 流程规则沉淀 + P2 功能技术债收口，已合并到 main）

## 待处理事项

### 后续任务候选

- ~~TASK-20260419-05：已归档（`archive-TASK-20260419-05.md`）~~
- ~~TASK-20260419-07：已归档（`archive-TASK-20260419-07.md`）~~
- **TASK-20260419-06（建议，P3 降级）：** HashMap Hash Mixing 优化（cluster 问题）— `BM_HashMapLookupHitInt/16384=9µs` vs n=64 时 69ns，根因 `H1=h>>7` + `std::hash<int>` 恒等映射。**降级理由（TASK-03 P4 实测）：** PropertyMap 60-entry HashMap<StringView, PropertyId> + djb2 hash 在最差 single key 下仅 2.75× HitHot5（远低于 5× cluster 阈值），证 cluster 问题主要见于 **int key + 大规模**场景。**触发条件**：「短字符串 ≠ 主用例 + 容器规模 > 1000 entry」的新场景出现时再立项
- **TASK-20260419-08（候选，P3 触发型）：** `string.h` 剩余 3 处 runtime-size memcpy（line 45 SSO ctor / 150 Append / 230 GrowAndCopy）防御性 noinline 化。**触发条件**：未来 GCC 升级回归同类 `-Warray-bounds` 误报；目前不主动改避免引入不必要内联开销（来源 TASK-20260419-07 副发现）
- ~~TASK-20260419-09：已立项为当前任务（A+B 子集，分支 `feature/TASK-20260419-09-replay-deepbench`）；C 拆出为 TASK-10~~
- **TASK-20260419-10（新增，TASK-05 K2/K3 + TASK-09 VAN 拆出，建议 P2 触发型）：** Layout super-linear knee 根因调查（**研究类**）— buildtree N=128→256 / flex 8x8→16x16 同源 super-linear。**VAN 阶段已否定 ArenaAllocator chunk grow 候选根因**（默认 4096 不 grow，量级不符）；剩余候选：(a) `LayoutBox` `Vector<LayoutBox*> children` 扩容序列；(b) layout 算法本身 O(N²) 路径（margin collapsing / line box reflow）；(c) 数据局部性 / prefetch break。**预期产出**：调查报告 +（可能）layout 算法重构 PR。**触发条件**：TASK-09 完成后
- **TASK-20260419-11（新增，TASK-09 K6 拆出，建议 P1 高 ROI）：** `ImageCache::Load` HashMap 化（**优化类**，机械替换）— 现状 hit 路径 O(N) 字符串扫，size=256 时 1162 ns > `ReplayImageReal<16>` 595 ns（K6 量化数据）。改 `HashMap<String, ImageHandle>` 后 hit 路径稳定 ~10-30 ns 不论 size。**预期工作量**：~1-2h（含 baseline 同机对比 + 单测 + bench 复跑确认 K6 命题已解）。**触发条件**：可即立项；与 TASK-10 顺序不强制（K6 修复独立 + 工作量小，建议在 TASK-10 之前先做）
- **TASK-20260419-12（新增，TASK-09 K7 拆出，建议 P2 触发型）：** `SoftwareCanvas::DrawText` 真路径优化（**优化类**）— 当前 warm 真路径 5807 ns > fallback 3647 ns（1.6×），阻碍未来默认开真路径。候选优化：(a) `hb_buffer` 复用（避免 hb_buffer_create/destroy 每帧分配）；(b) glyph bitmap 直接 raster 到 canvas（避免 GlyphCache → 中间 buffer → blit 的两次拷贝）。**预期产出**：warm 真路径 < 3000 ns（小于 fallback）后默认真路径。**触发条件**：当真路径默认化提上日程时

### 长期项（按优先级）

- **🔴 P0（紧急升级，反复 9+ 次，TASK-07 已验有效预防）：** Cursor 沙箱内任何 FetchContent 任务的 VAN 阶段**必须**强制重设 git 全局代理（`git config --global http.proxy http://...`），并在 `/archive` 阶段决定是否 unset；任务 hand-off 必须在新任务 VAN 检查表加一行「proxy 状态确认」。**根因：** TASK-02/04 反思都识别此问题但只升 P1，每次「单次 5-10 分钟可解决」导致**累计成本 ≥ 1 小时**；TASK-03 Round 2 第 9+ 次出现破例升 P0。**待落实动作：** (a) 写入 `main.mdc` 或 `writing-plans.mdc`「FetchContent 任务前置 checklist」强制条目；(b) `/van` 命令文档加阶段守卫「检测到 plan 含 FetchContent → 自动提醒 proxy 状态」。**已知代理地址：** `http://192.168.101.217:7890`（开发环境特定，写规则时用占位符）
- ~~**🔴 P0（TASK-07 + TASK-05 第 2 次实证）：** `writing-plans.mdc` 「目标 API 的发射/触发条件 grep」段~~ → ✅ **已于 TASK-20260419-05 /archive 阶段落实**：`writing-plans.mdc` 新增「性能基准任务必检项」段（6 子项，含发射条件 grep / smoke 三件套 / RangeMultiplier 公式 / Render 前置清单交叉引用）
- **🟠 P2（频率升级，TASK-07 + TASK-05 + TASK-09 = 3 次）：** `/reflect` 命令 §3.5 反复模式表「方案根因假设未先验证」频率升至 3 次（已被 P0 grep 规则有效抑制；TASK-09 全程 7 处 grep 仅遗漏 1 处链接可见性盲点 → 已通过本任务 reflection 建议 #1 把链接 PUBLIC/PRIVATE/INTERFACE 检查写入 P0 规则）
- **P1（新增，TASK-09 反思建议 #2）：** bench 类任务 plan 估时模板 — 含「复用率假设 + 单 BM 3-5 min 经验值」段。TASK-05 plan 4.25h vs 实际 75 min（3.4×）；TASK-09 plan 3.5h vs 实际 50 min（4.2×）；连续两次 4× 高估。**落实方式**：下次 bench 任务 plan §0 全局约束段写明复用率 + 单 BM 时间估算；如再次 > 3× 高估则升级到 `writing-plans.mdc` 强制条目
- **P1（已确认，本任务整体回顾再次复确）：** WIP / 中间 commit 的 subject 严禁包含外部任务状态字样（`BLOCKED on TASK-X` / `WAITING for Y` / `PENDING dep`）。**首发：** TASK-20260419-03 Round 1。**落实：** 下次 wip commit 前对照；如再次出现立即升级 P0 + 写入 `git-workflow.mdc`「Commit Subject 规范」段
- **P1（已确认，本任务整体回顾再次复确）：** Level 2+ 多 phase 任务（phase 数 ≥ 5）需要支持「轮次完成」中间态 — `/reflect` 不强制切死端「回顾中」。**首发：** TASK-20260419-03 Round 1。**落实：** 修改 `complexity-levels.mdc` 增加「多轮次 Build 工作流」段；或调整 `/reflect`、`/build`、`/archive` 命令的阶段守卫
- **P1（反复出现）：** 任何引入 GCC/Clang 模板特化技巧（`template<usize N>` 取数组引用、CRTP、SFINAE 分派）的 PR 必须在 PR 检查表中加一行「Release `-O2 -Werror` 通路验证」。来源：TASK-04 + TASK-03 P6 + TASK-07 实证。固化到 `writing-plans.mdc`「Release 通路验证」段
- **P1**：跨子库新增符号引用前 grep link graph，确认是否触发循环依赖（来源 TASK-20260419-01 反思 #1，规则已固化到 `writing-plans.mdc`「静态库循环依赖审计」段）
- ~~**P1**：性能基准任务必须在 Plan 阶段就显式 `-DCMAKE_BUILD_TYPE=Release` + 独立 `build-bench/` 目录~~ → ✅ **已于 TASK-05 /archive 阶段落实**：`writing-plans.mdc`「性能基准任务必检项」§1 强制条目
- **P1**：CMake 操作第三方 target 前必须先用 `get_target_property(... ALIASED_TARGET)` 识别 ALIAS，避免 `set_target_properties` 报错（来源 TASK-20260419-02 反思 #2）
- ~~**P1（新增, TASK-05）：** Bench smoke 验收三件套~~ → ✅ **已于 TASK-05 /archive 阶段落实**：`writing-plans.mdc`「性能基准任务必检项」§4 强制条目 + `systemPatterns.md`「Bench Smoke 自检模式」段
- **P2**：将 `renderer_test` / `render_integration_test` 等剩余手写像素位移断言迁到 `tests/test_pixel_utils.h`（来源 TASK-20260413-02）
- ~~**P2**：google/benchmark `RangeMultiplier(m)->Range(lo,hi)` 的精确数量为 `ceil(log_m(hi/lo))+1`~~ → ✅ **已于 TASK-05 /archive 阶段落实**：`writing-plans.mdc`「性能基准任务必检项」§5 强制条目
- ~~**P2（新增, TASK-05）：** 「Render bench 前置清单」+「带否定判据的发现型 Phase」+「跨阶段管道型 API default-nullptr 反模式」~~ → ✅ **已于 TASK-05 /reflect 阶段沉淀到 `systemPatterns.md`**（4 段）+ /archive 阶段交叉引用入 `writing-plans.mdc`「性能基准任务必检项」§6
- **P2（TASK-05 K2/K3 待研究）：** Layout super-linear knee 调查 — buildtree N=128→256 + flex 8x8→16x16 同源 super-linear（10×～15× for 2×～4× 输入）。候选根因：(a) ArenaAllocator chunk grow（4096 byte 边界）；(b) SmallVector 阈值；(c) cache miss。已写入 `techContext.md`「Layout 性能基线」段；建议归入 TASK-20260419-09 候选一并研究
