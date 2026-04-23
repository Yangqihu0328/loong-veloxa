# 活跃上下文

## 当前阶段

回顾中

**TDD 模式：** 文档类任务无 TDD — 以「反例追溯表」替代（已实证 7/7 通过，作为新范式沉淀）

**构建产出：** 3 条规则落地到 6 个规则/命令文件 + 3 MB 文件 = 9 文件修改 / 4 独立 commit（P1→P4）/ 反例追溯 7/7 通过 / 10 验收 9✅+1 改进

**Reflection 产出：** `memory-bank/reflection/reflection-TASK-20260419-13.md`
- **5 新模式沉淀** `systemPatterns.md`：Meta-dogfooding Phase 0 / 基础假设核查 VAN/Plan 前置清单 / 单一真相来源占位符 / 实证微调 spec / bench 估时校准扩展为跨类型（plan × 0.6 通用目标）
- **6 改进建议：** P1×3（#1 Meta-dogfooding 标配 / #2 跨类型估时校准 / #3 基础假设核查清单）+ P2×3（#4 单一真相来源通用 / #5 实证微调 spec / #6 .editorconfig 观察）
- **反复模式扫描：** "前置依赖/环境/API 能力未验证"（频率 8+）本次 meta-dogfooding 自证触发 + 沉淀的条目 2 规则直接针对，**自闭环**；新记录模式「.cursor/commands/* 可编辑性假设错误」(本次 1 次，纳入 L3 基础假设核查)

## 当前任务

**TASK-20260419-13：流程规则 P0/P1 沉淀冲刺（3 条积压条目一次性闭环）**

- **复杂度级别：** Level 2（6 个规则/命令文件 + 2 MB 文件 = 8 文件修改；遵循 `writing-plans.mdc` 已有段式样；纯文档无代码）
- **基线分支：** `main`（6 个目标规则/命令文件全在 main）
- **分支：** `feature/TASK-20260419-13-process-rules-sunk-in`（已创建 VAN `ec78f1c`）
- **安全相关：** ❌ 否（纯流程规则文档）
- **设计文档：** `docs/specs/2026-04-19-process-rules-sunk-in-design.md` ✅（用户已批准）
- **实现计划：** `docs/plans/2026-04-19-process-rules-sunk-in.md` ✅（5 phase / 85-95 min 预估）

### 待沉淀 3 条


| #   | 优先级       | 来源                              | 目标文件                                                       | 内容                                                                                                                                             |
| --- | --------- | ------------------------------- | ---------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------- |
| 1   | 🔴 **P0** | 反复 9+ 次（TASK-02/03/04 反思）       | `.cursor/rules/skills/writing-plans.mdc` + `main.mdc` 可能联动 | FetchContent 任务 VAN 阶段**强制重设 git 全局代理**；需 (a) 写入 `writing-plans.mdc`「FetchContent 任务前置 checklist」强制条目；(b) `main.mdc` 会话启动或 VAN 命令守卫提醒 proxy 状态 |
| 2   | 🟠 **P1** | TASK-11 反思 #2                   | `.cursor/rules/skills/writing-plans.mdc` §5.4 邻段新增 §5.7    | Plan 阶段**必须 grep `which <tool>`** 验证 smoke 工具链可用性（jq / bc / valgrind / awk / xmllint 等）                                                        |
| 3   | 🟠 **P1** | TASK-03 Round 1 首发 + TASK-11 复确 | `.cursor/rules/workflow/complexity-levels.mdc`             | Level 2+ 多 phase 任务（≥ 5 phase）支持**「轮次完成」中间态** — `/reflect` / `/build` / `/archive` 阶段守卫调整说明                                                    |


### VAN 阶段关键核查（已完成）

- ✅ 3 条目插入点已精确定位且不存在重复（grep 实证）
- ✅ `writing-plans.mdc` 已有 6 个「XX 前置条件必填」段成熟模板可直接复刻（覆盖 CMake 链接 / 循环依赖 / Web API 多重载 / FetchContent C 编译选项 / 测试基础设施 / 边界输入 / 调用链 / 管线注入 / 性能基准，第 7 段为 proxy 守卫同构插入）
- ✅ `complexity-levels.mdc` Level 4 已有「迭代机制」段可参考（L56/L66）；新增「轮次完成中间态」作为对 `/reflect` `/build` `/archive` 阶段守卫的补充，而非新级别
- ✅ `.cursor/commands/*.md` 经 Plan 阶段二次核查发现**是 workspace 可编辑文件**（非只读系统提示），扩大了条目 1 和条目 3 的实施范围到真正的「可执行守卫」
- ✅ 纯文档类任务，无 TDD 要求 — 规则文案写完后用「反例追溯表」验证有效性（每条规则 ≥ 3 历史任务 case 追溯）

### Plan 阶段设计结论（3 决策 + 10 验收标准）


| 决策                | 选择                                | 理由                                                   |
| ----------------- | --------------------------------- | ---------------------------------------------------- |
| **D1** 代理地址处理     | **占位符 `<开发环境代理地址>`**              | 用户 Plan 阶段直接拍板；规则零硬编码 IP，地址在 `techContext.md` 单一真相来源 |
| **D2** 条目 3 子状态实现 | **子状态标签 `构建中·轮次 N 完成`**           | 保持 5 主阶段骨架不变；对未感知命令视作 `构建中` 扩展，向后兼容                  |
| **D3** Phase 提交粒度 | **每 Phase 1 commit**（5-6 commits） | 与 TASK-01/11 同模式；每条目可独立 review/回滚                    |


**Phase 拆分：** P0 基线 5min → P1 proxy 25-30min → P2 smoke 15min → P3 多轮次 30-35min → P4 收尾 10min = **85-95 min 合计**

### 参考长期待办

- 已并入本任务的 3 条来自 `activeContext.md` 下方「长期项」段：P0 proxy / P1 smoke 工具链 / P1 多轮次 Build — 本任务完成后应从待处理事项中标记「已落实」

**下一步：** 使用 `/build` 按计划执行 5 个 Phase

## 最近归档

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
- **TASK-20260419-10（TASK-05 K2/K3 + TASK-09 VAN 拆出，建议 P2 触发型）：** Layout super-linear knee 根因调查（**研究类**）— buildtree N=128→256 / flex 8x8→16x16 同源 super-linear。**TASK-09 VAN 阶段已否定 ArenaAllocator chunk grow 候选根因**（默认 4096 不 grow，量级不符）；剩余候选：(a) `LayoutBox` `Vector<LayoutBox*> children` 扩容序列；(b) layout 算法本身 O(N²) 路径（margin collapsing / line box reflow）；(c) 数据局部性 / prefetch break。**预期产出**：调查报告 +（可能）layout 算法重构 PR。**触发条件**：建议在 TASK-11 之后立项（K6 修复独立且小，先做）
- ~~TASK-20260419-11：已完成并合并到 main `8515c25`，详见 `archive-TASK-20260419-11.md~~`
- **TASK-20260419-12（TASK-09 K7 拆出，建议 P2 触发型）：** `SoftwareCanvas::DrawText` 真路径优化（**优化类**）— 当前 warm 真路径 5807 ns > fallback 3647 ns（1.6×），阻碍未来默认开真路径。候选优化：(a) `hb_buffer` 复用（避免 hb_buffer_create/destroy 每帧分配）；(b) glyph bitmap 直接 raster 到 canvas（避免 GlyphCache → 中间 buffer → blit 的两次拷贝）。**预期产出**：warm 真路径 < 3000 ns（小于 fallback）后默认真路径。**触发条件**：当真路径默认化提上日程时

### 长期项（按优先级）

- ~~**🔴 P0（紧急升级，反复 9+ 次，TASK-07 已验有效预防）：** Cursor 沙箱内任何 FetchContent 任务的 VAN 阶段**必须**强制重设 git 全局代理~~ → ✅ **已于 TASK-20260419-13 P1 落实**：`writing-plans.mdc` L96「FetchContent 网络代理守卫」段（6 小节）+ `van.md` §1 子项「FetchContent 代理状态检查」+ `techContext.md` L98「Plan/VAN 阶段守卫」交叉引用；代理地址单一真相来源为 `techContext.md`，规则零硬编码 IP，统一占位符 `<开发环境代理地址>`
- ~~**🔴 P0（TASK-07 + TASK-05 第 2 次实证）：** `writing-plans.mdc` 「目标 API 的发射/触发条件 grep」段~~ → ✅ **已于 TASK-20260419-05 /archive 落实**；TASK-09 /reflect 二次升级覆盖 **CMake 链接可见性**（PUBLIC/PRIVATE/INTERFACE，含 PNG::PNG 反例）
- **🟠 P2（频率升级，TASK-07 + TASK-05 + TASK-09 = 3 次）：** `/reflect` 命令 §3.5 反复模式表「方案根因假设未先验证」频率升至 3 次（已被 P0 grep 规则有效抑制；TASK-09 全程 7 处 grep 仅遗漏 1 处链接可见性盲点 → 已通过 TASK-09 reflection 建议 #1 把链接 PUBLIC/PRIVATE/INTERFACE 检查写入 P0 规则）
- **P1（TASK-09 反思建议 #2 + TASK-11/13 实证，已跨类型扩展）：** **通用 plan × 0.6 目标倍率协议**（原 bench 类任务专属已泛化）— TASK-05 3.4× / TASK-09 4.2× / TASK-11 1.5-2.0× / **TASK-13 文档类 1.67-1.86×** 跨 4 任务稳定收敛 ~2×。**落实方式**：✅ 已于 TASK-13 /reflect 扩写 `systemPatterns.md`「bench 估时校准」段为跨类型统一；下次任一类型任务 ≤ 1.5× 即视为「准确档」，写入 `writing-plans.mdc` 强制条目。
- **P1（新增, TASK-11 反思 #1）：** bench plan 阈值表对**超低 ns BM**（baseline < 50ns）应用「绝对增量兜底」，避免噪声误警。建议公式：`判定阈值 = max(baseline × 1.2, baseline + 0.5ns for <5ns / +5ns for [5,50)ns)`。**落实方式**：追加附录到 `systemPatterns.md` "bench 类任务估时校准" 段（已沉淀 ✅）+ 下次 bench plan 阈值表必引。**根因**：TASK-11 P3 实测 Get 0.94→1.16 ns（1.23×，超 1.2× 阈值）但实现完全没改动，纯属测量噪声；Hit<1> 10.35→43.27 ns 是 HashMap 固有 ~32ns 开销，绝对量微小但乘法判定显示「4×」。
- ~~**P1（新增, TASK-11 反思 #2）：** Plan 阶段需 grep `which <tool>` 验证 smoke 工具链可用性~~ → ✅ **已于 TASK-20260419-13 P2 落实**：`writing-plans.mdc` §4 末尾新增 `#### smoke 工具链可用性检查` 子块（jq/bc/valgrind/awk/xmllint/rg 6 工具兜底矩阵 + Plan Phase 0 一次性 batch grep + Build 严禁临时换栈 + 与 `verification.mdc` 协同定位）
- **P1（新增, TASK-11 反思 #3）：** Mixed TDD 模式下「为预防特定 bug 而新增的回归测试」（D3 类）**标配 RED 反向探针验证**（临时破坏实现确认 FAIL → 恢复确认 PASS）。**落实方式**：沉淀到 `systemPatterns.md` 新模式段「Mixed TDD RED 反向探针实践」（已沉淀 ✅）。**根因**：TASK-11 D3 `ClearAndReloadDeduplicates` 通过反向探针证实测试在「实现已正确」时也能精准 FAIL（注释 `path_to_handle_.clear()` → 立即 FAIL → 恢复 → PASS），耗时 < 3 min 但避免「测试因实现恰巧正确而成为永远不报警的死代码」最大风险。
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

