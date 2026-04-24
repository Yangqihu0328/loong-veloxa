# 活跃上下文

## 当前阶段
回顾中 — 等待 /archive

## 当前任务

**TASK-20260424-03：SoftwareCanvas::DrawText 真路径 warm 优化** — /build 阶段完成

- 复杂度：Level 2-3（优化类，多候选路径 + 5 设计决策）
- 来源：`activeContext.md` 后续任务候选 TASK-20260419-12（TASK-09 K7 拆出，P2 触发型）
- 目标：warm 真路径 5807 ns → < 3000 ns，使真路径默认化具备前置条件
- **/plan 产出：** `docs/specs/2026-04-24-drawtext-warm-opt-design.md` + `docs/plans/2026-04-24-drawtext-warm-opt.md`
- **5 决策：** D1 阶梯验证 / D2 thread_local + RAII / D3 B1+B2 组合 / D4 保持 Vector<u8> / D5 刚性 <3000 ns

### /build 最终成果汇总（3 轮次 / 7 Phase / 9 commits）

| Phase | 优化 | Warm_Medium | 累计 | 命中 | 测试 |
|:-:|---|---:|---:|:-:|:-:|
| 0 | baseline anchor | 5412 ns | — | — | 46 ✅ |
| 1 | A hb_buffer thread_local 复用 | 5397 ns | -0.3% | ❌ | 46 |
| 2 | C FT_Set_Pixel_Sizes 状态缓存 | 5266 ns | -0.9% | ❌ | 46 |
| 3 | E 默认 FontHandle 缓存 | 5386 ns | -0.3% | ❌ | 46 |
| 4 | D GlyphCache::Put → GlyphBitmap* | 5311 ns | -1.9% | ❌ | 46 |
| 5 | B1 /255 乘移位近似（**实验回退**）| — | — | — | 51 (5 新 infra 保留) |
| 6 | B2 pre-clip + row ptr | **4689 ns** | **-13.4%** | ❌ | 53 |
| 7 | B3 SSE2 4 px/iter + AVX2 8 px/iter `count≥16` 智能 dispatch | **3354 → 3499 ns** | **-38%** | ⚠️ 差 499 ns | **59** ✅ |

**关键结论：**
- **业务目标达成** ✅：Warm_Medium **3499 ns < Fallback 3608 ns (-3%)**，真路径默认化前置条件满足
- **技术刚性目标**：D5 `< 3000 ns` 差 499 ns (14%)，2 次 R1 AskQuestion 升级后仍差，用户知情接受
- **Cold 路径副产品**：Real_Cold_Medium 52873 → 28338 ns **(-46.4%)**（default font + pixel size 缓存对 cold 也生效）
- **Phase 5 负结果**：GCC -O3 Granlund-Montgomery 魔数乘法已优于手写 /255 近似 → 保留 `glyph_blend.h` + `pixel_blend_test` 作 SIMD 精度基础设施
- **AVX2 实验教训**：glyph 宽度分布偏小（ASCII 6-12 px）使 8 px/iter 摊销不足 → 实施 `count >= 16` 智能阈值为 CJK / 大字号保留 headroom
- **SIMD 精度契约**：11 GTests（含 RED 反向探针完整循环验证）保证 SSE2/AVX2 与 scalar ±1 LSB/channel

### 交付清单

- 7 修改文件（software_canvas / font_manager / glyph_cache 核心 3 对 + 2 新 SIMD header + 1 blend helper + 1 test）
- baseline JSON 刷新：`benchmarks/baseline/bench_drawtext.json`
- `benchmarks/baseline/README.md`：K7 → resolved + 历史行 TASK-20260424-03
- `memory-bank/progress.md`：6 phase 条目 + 7 表行
- `memory-bank/tasks.md`：TASK-20260424-03 归档折叠 + /build 成果段

### /reflect 阶段成果（2026-04-24）

- 反思文档：`memory-bank/reflection/reflection-TASK-20260424-03.md`（13 段 / 11 GTests 测试契约对照 / plan × 0.6 第 6 数据点 0.34× 「最窄」档确认）
- **3 大关键发现**：
  - F5 **GCC `-O3` Granlund-Montgomery 魔数乘法 > 手写 /255 近似**（通用编译器洞察，适用所有标量 `/常量` 场景）
  - F6 **AVX2 8 px/iter 在 ASCII glyph (6-12 px) 无净收益** — 引出「**异构工作负载 SIMD 尺寸阈值 dispatch**」新模式
  - L4 **API 层优化天花板远低于 plan 估算** — 现代 runtime (glibc tcmalloc / FT / hb) 已做足，Phase 6 B2 单 Phase 收益是 Phase 1-4 累计 6 倍
- **7 改进建议** 含 2 项 P1（godbolt 编译器优化识别 / WSL2 warm-up 协议标准化） + 5 项 P2（SIMD 阈值 dispatch 模式 / 负结果资产化 / API 天花板识别 / R1 升级路径 / 智能阈值不退化）
- 反复模式识别：「前置依赖/环境/API 能力未验证」1 次轻重复（编译器优化未 godbolt 确认），已纳入 P1 建议 #1

### 下一步

1. `/archive` — 生成 `memory-bank/archive/archive-TASK-20260424-03.md` + merge 到 main；落实 P1 建议 #1 (godbolt 检查) + #2 (WSL2 warm-up 协议) 到 `writing-plans.mdc`/`systemPatterns.md`

## 未合并分支

- `feature/TASK-20260424-03-drawtext-warm-opt`（/build 完成 8 commits，等待 /reflect + /archive + `--no-ff` 合并到 main）

## 最近归档

- `memory-bank/archive/archive-TASK-20260424-01.md`（TASK-20260424-01 Layout super-linear knee 根因调查 — 根因 (d) ArenaAllocator 4KB block malloc/free churn 定位；默认 block 4096 → 32768；K2 R256 9.42×→4.18× / K3 R_flex 16.49×→6.40× 均 ~2.5× 改善；Phase 2 block-size 5 档扫描，32K 为 Flex sweet spot；**K8 新发现**：65K block > L1D 触发抖动 → Arena 设计守则「block ≤ L1D」；`DefaultBlockSizeFitsLargeAllocations` GTest + RED 反向探针；ctest 892/892 PASS；**plan × 0.6 第 5 数据点 0.29×**（115 min plan / ~33 min 实测，历史最快，「最窄路径」子档样板）；3 新模式沉淀 `systemPatterns.md`（扫描型脚本化模板+双指标交叉 / 公开行为锚定内部约束 / 最窄路径子档）；残余 ~40% super-linear 拆出 TASK-20260424-02；已 `--no-ff` 合并到 main `0882d0c`）
- `memory-bank/archive/archive-TASK-20260419-13.md`（TASK-20260419-13 流程规则 P0/P1 沉淀冲刺 — 3 条积压条目一次性闭环：P0 FetchContent proxy 守卫（反复 9+ 次痛点终结）/ P1 smoke 工具链可用性 grep（TASK-11 #2）/ P1 多轮次 Build 中间态（TASK-03 Round 1 首发）；9 文件修改（6 规则/命令 + 3 MB）/ 4 phase commits / 反例追溯 7/7 通过（含 meta-dogfooding 实时自证）/ 10 验收 9 ✅ + 1 改进；跨类型估时收敛 plan × 0.6 通用协议（TASK-05/09/11/13 四数据点）；5 新模式沉淀 `systemPatterns.md`（Meta-dogfooding Phase 0 / 基础假设核查 / 单一真相来源占位符 / 实证微调 spec / bench 估时校准扩展跨类型）；已 `--no-ff` 合并到 main `8a436ed`）
- `memory-bank/archive/archive-TASK-20260419-11.md`（TASK-20260419-11 ImageCache::Load HashMap 化 — K6 高 ROI 修复 — 双索引方案（`Vector<Entry>` 保 ABI/Get O(1) + `HashMap<String, ImageHandle, StringHash, StringEq>` 提供 O(1) path 查询）；Hit<256> 1151.77 ns → 45.70 ns（**25.2×↓**），Hit<16> 50.87 → 44.05 ns；ctest 891/891 PASS（含 `ClearAndReloadDeduplicates` D3 回归网，RED 反向探针验证有效）；Release `-O3` 0 errors；3 P1 沉淀：bench 阈值表绝对增量兜底 / Plan grep `which <tool>` / Mixed TDD RED 反向探针；3 P2 沉淀：P1+P2 拆分模式 / HashMap 不是金科玉律 / bench 估时校准 4.2×→2.0× 收敛；已 `--no-ff` 合并到 main `8515c25`）
- `memory-bank/archive/archive-TASK-20260419-09.md`（TASK-20260419-09 Replay 深度基准 + 真 ImageCache 通路 — 2 bench exe / 15 BMs / 2 baseline JSON 入仓；K1 修正归因（fallback 非真路径）+ 真冷路径 14× 慢；K6 新发现 ImageCache::Load O(N) hit 路径（size=256 时 1162 ns）→ 推 TASK-11 P1 高 ROI；K7 新发现 warm 真路径 1.6× 慢 fallback → 推 TASK-12 P2 触发型；落实「方案根因假设未先验证」P0 第 2 次完整应用 + 升级 grep 规则覆盖 CMake 链接可见性）
- `memory-bank/archive/archive-TASK-20260419-05.md`（TASK-20260419-05 Layout + Render 性能基准 — 4 bench exe / 30 BMs / 4 baseline JSON 入仓；K1 实测 DrawText 是 Replay hot path（820× FillRect），ImageCache 不是；推 TASK-20260419-09 候选；落实 P0 #2 `writing-plans.mdc`「性能基准任务必检项」段；已 `--no-ff` 合并到 main `d0999e8`）
- `memory-bank/archive/archive-TASK-20260419-07.md`（TASK-20260419-07 修复 main Release `-Werror` 编译失败 2 处 — fgets unused-result + BasicString copy ctor IPA array-bounds 误报；与 TASK-04 同元模式不同手法 noinline；已 `--no-ff` 合并到 main `206d227`）
- `memory-bank/archive/archive-TASK-20260419-03.md`（TASK-20260419-03 CSS 解析性能基准 — 30 BMs + 3 baseline JSON 入仓 + Cluster 度量证 PropertyMap 均匀 → TASK-06 P1→P3，已 `--no-ff` 合并到 main `660313a`）
- `memory-bank/archive/archive-TASK-20260419-04.md`（TASK-20260419-04 修复 `enum_serialization.cc` Release `-Warray-bounds` 误报，已合并到 main `a09ad1e`）
- `memory-bank/archive/archive-TASK-20260419-02.md`（TASK-20260419-02 Google Benchmark 集成 + Foundation 性能基线，已合并到 main）
- `memory-bank/archive/archive-TASK-20260419-01.md`（TASK-20260419-01 流程规则沉淀 + P2 功能技术债收口，已合并到 main）

## 待处理事项

### 后续任务候选

- **TASK-20260419-06（建议，P3 降级）：** HashMap Hash Mixing 优化（cluster 问题）— `BM_HashMapLookupHitInt/16384=9µs` vs n=64 时 69ns，根因 `H1=h>>7` + `std::hash<int>` 恒等映射。**降级理由（TASK-03 P4 实测）：** PropertyMap 60-entry HashMap<StringView, PropertyId> + djb2 hash 在最差 single key 下仅 2.75× HitHot5（远低于 5× cluster 阈值），证 cluster 问题主要见于 **int key + 大规模**场景。**触发条件**：「短字符串 ≠ 主用例 + 容器规模 > 1000 entry」的新场景出现时再立项
- **TASK-20260419-08（候选，P3 触发型）：** `string.h` 剩余 3 处 runtime-size memcpy（line 45 SSO ctor / 150 Append / 230 GrowAndCopy）防御性 noinline 化。**触发条件**：未来 GCC 升级回归同类 `-Warray-bounds` 误报；目前不主动改避免引入不必要内联开销（来源 TASK-20260419-07 副发现）
- ~~TASK-20260419-10：已立项为 TASK-20260424-01 并完成归档。根因 (d) ArenaAllocator malloc churn 定位 + 32K 落地，knee 收敛 ~60%，详见 `archive-TASK-20260424-01.md`~~
- **TASK-20260424-02（新增，TASK-24-01 Phase 1B 升级路径拆出，建议 P3 触发型）：** Layout 残余 super-linear 调查（per-phase 拆分 BM 定位 (e) L1D 抖动 / (f) 隐藏算法因素）— 承接 TASK-24-01 解决后剩余 ~40% super-linear（R256 仍 4.18× / R_flex 仍 6.40×）+ Phase 2 K8 新发现（65K block Flex 回弹暗示 L1D 抖动）。**触发条件**：下次 layout 性能需求（grid / multi-column）或主动预算
- ~~TASK-20260419-11：已完成并合并到 main `8515c25`，详见 `archive-TASK-20260419-11.md~~`
- **TASK-20260419-12（TASK-09 K7 拆出，建议 P2 触发型）：** `SoftwareCanvas::DrawText` 真路径优化（**优化类**）— 当前 warm 真路径 5807 ns > fallback 3647 ns（1.6×），阻碍未来默认开真路径。候选优化：(a) `hb_buffer` 复用（避免 hb_buffer_create/destroy 每帧分配）；(b) glyph bitmap 直接 raster 到 canvas（避免 GlyphCache → 中间 buffer → blit 的两次拷贝）。**预期产出**：warm 真路径 < 3000 ns（小于 fallback）后默认真路径。**触发条件**：当真路径默认化提上日程时

### 长期项（按优先级）

- ~~**🔴 P0（紧急升级，反复 9+ 次，TASK-07 已验有效预防）：** Cursor 沙箱内任何 FetchContent 任务的 VAN 阶段**必须**强制重设 git 全局代理~~ → ✅ **已于 TASK-20260419-13 P1 落实**：`writing-plans.mdc` L96「FetchContent 网络代理守卫」段（6 小节）+ `van.md` §1 子项「FetchContent 代理状态检查」+ `techContext.md` L98「Plan/VAN 阶段守卫」交叉引用；代理地址单一真相来源为 `techContext.md`，规则零硬编码 IP，统一占位符 `<开发环境代理地址>`
- ~~**🔴 P0（TASK-07 + TASK-05 第 2 次实证）：** `writing-plans.mdc` 「目标 API 的发射/触发条件 grep」段~~ → ✅ **已于 TASK-20260419-05 /archive 落实**；TASK-09 /reflect 二次升级覆盖 **CMake 链接可见性**（PUBLIC/PRIVATE/INTERFACE，含 PNG::PNG 反例）
- **🟠 P2（频率升级，TASK-07 + TASK-05 + TASK-09 = 3 次）：** `/reflect` 命令 §3.5 反复模式表「方案根因假设未先验证」频率升至 3 次（已被 P0 grep 规则有效抑制；TASK-09 全程 7 处 grep 仅遗漏 1 处链接可见性盲点 → 已通过 TASK-09 reflection 建议 #1 把链接 PUBLIC/PRIVATE/INTERFACE 检查写入 P0 规则）
- **P1（TASK-09 反思建议 #2 + TASK-11/13 实证，已跨类型扩展）：** **通用 plan × 0.6 目标倍率协议**（原 bench 类任务专属已泛化）— TASK-05 3.4× / TASK-09 4.2× / TASK-11 1.5-2.0× / **TASK-13 文档类 1.67-1.86×** / **TASK-24-01 研究类 0.29×** 跨 5 任务；TASK-24-01 首次突破 ≥1.5× 下限 → 触发「路径宽度」子档分化（全新 / 中等 / **最窄**）。**落实方式**：✅ 已于 TASK-13 /reflect 扩写 `systemPatterns.md`「bench 估时校准」段为跨类型统一；TASK-24-01 /reflect 追加「最窄路径」子档；下次任一类型任务 ≤ 1.5× 即视为「准确档」，写入 `writing-plans.mdc` 强制条目。
- **P1（新增, TASK-11 反思 #1）：** bench plan 阈值表对**超低 ns BM**（baseline < 50ns）应用「绝对增量兜底」，避免噪声误警。建议公式：`判定阈值 = max(baseline × 1.2, baseline + 0.5ns for <5ns / +5ns for [5,50)ns)`。**落实方式**：追加附录到 `systemPatterns.md` "bench 类任务估时校准" 段（已沉淀 ✅）+ 下次 bench plan 阈值表必引。**根因**：TASK-11 P3 实测 Get 0.94→1.16 ns（1.23×，超 1.2× 阈值）但实现完全没改动，纯属测量噪声；Hit<1> 10.35→43.27 ns 是 HashMap 固有 ~32ns 开销，绝对量微小但乘法判定显示「4×」。
- ~~**P1（新增, TASK-11 反思 #2）：** Plan 阶段需 grep `which <tool>` 验证 smoke 工具链可用性~~ → ✅ **已于 TASK-20260419-13 P2 落实**：`writing-plans.mdc` §4 末尾新增 `#### smoke 工具链可用性检查` 子块（jq/bc/valgrind/awk/xmllint/rg 6 工具兜底矩阵 + Plan Phase 0 一次性 batch grep + Build 严禁临时换栈 + 与 `verification.mdc` 协同定位）
- **P1（新增, TASK-11 反思 #3 + TASK-24-01 第 2 次实证）：** Mixed TDD 模式下「为预防特定 bug 而新增的回归测试」（D3 类）**标配 RED 反向探针验证**（临时破坏实现确认 FAIL → 恢复确认 PASS）。**落实方式**：沉淀到 `systemPatterns.md` 新模式段「Mixed TDD RED 反向探针实践」（已沉淀 ✅）；TASK-24-01 `DefaultBlockSizeFitsLargeAllocations` 第 2 次成熟应用 → 模式可作为 Mixed TDD 标配。**根因**：TASK-11 D3 `ClearAndReloadDeduplicates` + TASK-24-01 default block 均通过反向探针证实测试在「实现已正确」时也能精准 FAIL，耗时 < 3 min 但避免「测试因实现恰巧正确而成为永远不报警的死代码」最大风险。
- **P1（已确认，本任务整体回顾再次复确）：** WIP / 中间 commit 的 subject 严禁包含外部任务状态字样（`BLOCKED on TASK-X` / `WAITING for Y` / `PENDING dep`）。**首发：** TASK-20260419-03 Round 1。**落实：** 下次 wip commit 前对照；如再次出现立即升级 P0 + 写入 `git-workflow.mdc`「Commit Subject 规范」段
- ~~**P1（已确认）：** Level 2+ 多 phase 任务（phase 数 ≥ 5）需要支持「轮次完成」中间态~~ → ✅ **已于 TASK-20260419-13 P3 落实**：`complexity-levels.mdc` L68 新段「多轮次 Build 中间态」（跨 Level 2-4 通用扩展 / 触发条件 / 子状态协议 `构建中·轮次 N 完成` / 向前兼容 / 恢复路径 / git 关系 5 小节）+ `build.md` §6.5「轮次完成判断」含轮次完成报告模板 + `reflect.md` §0 守卫放宽识别子状态标签立即返回
- **P1（反复出现）：** 任何引入 GCC/Clang 模板特化技巧（`template<usize N>` 取数组引用、CRTP、SFINAE 分派）的 PR 必须在 PR 检查表中加一行「Release `-O2 -Werror` 通路验证」。来源：TASK-04 + TASK-03 P6 + TASK-07 实证。固化到 `writing-plans.mdc`「Release 通路验证」段
- **P1**：跨子库新增符号引用前 grep link graph，确认是否触发循环依赖（来源 TASK-20260419-01 反思 #1，规则已固化到 `writing-plans.mdc`「静态库循环依赖审计」段）
- ~~**P1**：性能基准任务必须在 Plan 阶段就显式 `-DCMAKE_BUILD_TYPE=Release` + 独立 `build-bench/` 目录~~ → ✅ **已于 TASK-05 /archive 阶段落实**：`writing-plans.mdc`「性能基准任务必检项」§1 强制条目
- **P1**：CMake 操作第三方 target 前必须先用 `get_target_property(... ALIASED_TARGET)` 识别 ALIAS，避免 `set_target_properties` 报错（来源 TASK-20260419-02 反思 #2）
- ~~**P1（新增, TASK-05）：** Bench smoke 验收三件套~~ → ✅ **已于 TASK-05 /archive 阶段落实**：`writing-plans.mdc`「性能基准任务必检项」§4 强制条目 + `systemPatterns.md`「Bench Smoke 自检模式」段
- **P2**：将 `renderer_test` / `render_integration_test` 等剩余手写像素位移断言迁到 `tests/test_pixel_utils.h`（来源 TASK-20260413-02）
- ~~**P2**：google/benchmark `RangeMultiplier(m)->Range(lo,hi)` 的精确数量为 `ceil(log_m(hi/lo))+1~~` → ✅ **已于 TASK-05 /archive 阶段落实**：`writing-plans.mdc`「性能基准任务必检项」§5 强制条目
- ~~**P2（新增, TASK-05）：** 「Render bench 前置清单」+「带否定判据的发现型 Phase」+「跨阶段管道型 API default-nullptr 反模式」~~ → ✅ **已于 TASK-05 /reflect 阶段沉淀到 `systemPatterns.md`**（4 段）+ /archive 阶段交叉引用入 `writing-plans.mdc`「性能基准任务必检项」§6；TASK-09 /reflect 升级「带否定判据」标签为 4/4 成熟实践
- **P1（新增, TASK-13 反思 #1）：** **Meta-dogfooding 作为规则沉淀类任务的 Phase 0 标配动作** — 基线验证时刻意检验新规则触发路径，命中即 T-0 实时自证。**落实方式**：✅ 已沉淀 `systemPatterns.md`「规则沉淀类任务的 Meta-dogfooding Phase 0 模式」段（本任务自证，TASK-13 `rg`/`jq` MISS 触发条目 2 规则）；下次流程规则沉淀类任务 plan Phase 0 直接引用。
- **P1（新增, TASK-13 反思 #2）：** **基础假设核查 — VAN/Plan 前置清单**：对非标准目录/约定（如 `.cursor/commands/*`）做 Glob/Read 实证，避免 plan 建立在错误假设上。**落实方式**：✅ 已沉淀 `systemPatterns.md`「基础假设核查 — VAN/Plan 前置清单」段；下次涉及非 `src/`/`tests/`/`build*/` 目录的任务 VAN 阶段必做。
- **P2（新增, TASK-13 反思 #4）：** **单一真相来源占位符模式**：环境敏感/跨项目复用/可变配置值用占位符，实际值集中 `techContext.md` 等单一真相来源。**落实方式**：✅ 已沉淀 `systemPatterns.md`「配置管理 — 单一真相来源占位符模式」段；下次涉及代理/端口/token 等配置的任务参照。
- **P2（新增, TASK-13 反思 #5）：** **实证微调 spec 范式**：plan 执行时允许基于实证数据微调 spec 细节（非核心意图），在 progress/reflection 标注依据。**落实方式**：✅ 已沉淀 `systemPatterns.md`「计划执行 — 实证微调 spec 范式」段；下次 build 阶段遇实证偏差直接用 "9 ✅ + N 改进" 格式标记。
- **P2（新增, TASK-13 反思 #6）：** **`.editorconfig` / prettier 统一 markdown 表格格式**：避免编辑器 auto-reformat 表格列宽污染 diff。**触发条件**：累计 3+ 任务再出现即立项；当前仅 TASK-13 spec §1 表格 auto-reformat 1 次，暂记观察。
- **P1（新增, TASK-24-01 反思 #1）：** **研究/实验型任务 plan §验收标准段应区分「预期带 / 下限带 / 中间区间处理方案」**（避免刚性阈值卡住流程）。**落实方式**：追加到 `writing-plans.mdc`「研究/调查类任务验收阈值三段式」子段。**根因**：本任务 plan 凭直觉设 R256≤2.5 刚性阈值，实测 4.18× 属于「(d) 贡献 60% + (e)/(f) 承担 40%」的 partial-root-cause 场景，plan 未预见需插入 AskQuestion 承接。
- **P1（新增, TASK-24-01 反思 #2）：** **plan §0 smoke/工具矩阵扩展到 unit test binary 路径**（现仅覆盖 bench binary）。**落实方式**：`writing-plans.mdc`「smoke 工具链可用性检查」子块追加「test binary 路径矩阵 `find build -name '*_test' -type f`」条目。**根因**：本任务 Phase 3 首次跑 GTest 时路径写成 `build/tests/foundation/memory/arena_allocator_test`（源码路径），实际是 `build/tests/arena_allocator_test`（扁平化输出），exit 127。
- **P2（新增, TASK-24-01 反思 #3）：** **扫描型研究任务的 for 循环 + python3 聚合模板 + 双指标交叉验证**沉淀。**落实方式**：✅ 已沉淀到 `systemPatterns.md`「扫描型研究任务脚本化模板 + 双指标交叉验证模式」段（本次 Phase 2 blocked-size 5 档扫描为范例）。
- **P2（新增, TASK-24-01 反思 #4）：** **「最窄路径任务」plan×0.3 子档 + TASK-24-01 0.29× 历史最快数据点**。**落实方式**：✅ 已沉淀到 `systemPatterns.md`「bench 类任务估时校准」段追加子档。下次单文件 1 行改动 + 基础设施 100% 复用的任务直接按 plan×0.3 预估。
- **P2（新增, TASK-24-01 反思 #5）：** **「公开行为锚定内部约束」测试设计哲学**。**落实方式**：✅ 已沉淀到 `systemPatterns.md`「测试设计 — 公开行为锚定内部约束模式」段。本任务 `DefaultBlockSizeFitsLargeAllocations` 用指针连续性间接观测 block 容量，不扩 API 不加 friend 为样板。
- **P1（新增, TASK-24-03 反思 #1，反复模式「前置依赖未验证」新变体）：** **Godbolt 验证编译器魔数优化 — 位运算/除法近似前置检查**。任何 "/常量 → 位运算" 或 "常量模/乘替换" 类手写优化 plan 前必须 godbolt 看 GCC `-O3` 原生输出；**Granlund-Montgomery 魔数乘法自 GCC 4.x 已自动应用**，标量路径手写近似几乎必然输给编译器；**只有 SIMD 场景才值得手写**。**落实方式**：归档阶段追加到 `writing-plans.mdc`「管线注入点代码级可行性验证」段新增子项「编译器已做优化识别」；Plan Phase 0 工具核查矩阵加 `gcc -S -O3` 或 godbolt-cli 手动确认。**根因**：TASK-24-03 Phase 5 实证 `/255 → (x*257+32768)>>16` 倒退 +1.1%（来源 reflection §4.b / §10.1）。
- **P1（新增, TASK-24-03 反思 #2）：** **WSL2 / 云机 bench warm-up 协议标准化**。`google/benchmark` 默认 `--benchmark_min_time=0.05s` 在 WSL2 冷启动瞬态下 CV 可达 8%（本任务实证）。**稳定协议**：sleep 10s → 单 filter 3-rep warm-up (min_time=0.2s) → 全量 10-rep measure → CV ≤ 2% 门槛，否则重测。**落实方式**：归档阶段追加到 `systemPatterns.md`「Bench Smoke 自检模式」+ `writing-plans.mdc`「性能基准任务必检项」新 §N 「WSL2/云机稳态协议」。**根因**：TASK-24-03 Phase 6 首次 stash baseline 测 6727 ns（43% 偏离真实 4689 ns），险些得出「Phase 7 倒退」的错误结论，warm-up 协议让 CV 从 8% 收敛到 1%。

