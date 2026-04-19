# 进度记录

## 当前任务

### TASK-20260419-03 — CSS 解析性能基准（续接）

- **2026-04-19 续接 BUILD 启动：** TASK-04 已归档合并到 main `a09ad1e`；`feature/TASK-20260419-03-css-benchmarks` rebase 到 main，3 处 MB 冲突均接受 main 端版本（TASK-04 idle 状态为更新真相），pause commit `679efaf` skip（已无意义）；feature 分支 ahead 2 commits（plan + WIP Phase 1）。本轮范围：Phase 1 验证（编译/运行 3 smoke bench）+ Phase 2（Tokenizer 完整 ~10 BM 套件）。Phase 3-6 留待下轮。
- **2026-04-19 Phase 1 验证 ✅：** Release `build-bench/` reconfigure（CMake 拾取 `vx_add_benchmark` 扩展 + 3 个新 exe 注册）；`cmake --build build-bench --target bench_css_{tokenizer,parser,property_lookup} -j` 完全干净 — `enum_serialization.cc` 重编后 vx_core 链接通过，TASK-04 LookupImpl 修复在 Release `-O2 -Werror=array-bounds` 下实地生效；3 exe 各 1 BM 行（`BM_TokenizeAllSmoke` 635 ns / `BM_ParseStylesheetSmoke` 1821 ns / `BM_PropertyLookupSmoke` 10.4 ns）+ exit 0。Phase 1 无文件改动，无新 commit。
- **2026-04-19 Phase 2 ✅：** 替换 `bench_css_tokenizer.cc` smoke 为完整 4 BM 套件（10 行 BM = 7 Range + 3 single）。验证：`BM_TokenizeAll/Range(64,4096)` 7 个规模点吞吐 297-340 MiB/s 稳定（确认 tokenizer 无 quadratic 退化）；`StringHeavy` 603 / `WhitespaceHeavy` 614 / `NumericHeavy` 372 MiB/s。提交 `feat(benchmarks): CSS tokenizer suite [P2]` (`4d014ec`)。
- **2026-04-19 完成验证 ✅：** Debug `cmake --build build && ctest` 890/890 通过，0 回归；3 CSS bench Release exit 0 + BM 行数符合本轮预期（tokenizer 10 / parser 1 / property_lookup 1）；3 个 .cc + css_corpus.h + CMakeLists.txt 全 lint 干净；本任务无新增依赖故跳过 CVE 审计（google/benchmark v1.9.1 在 TASK-02 已审）。
- **2026-04-19 第 1 轮 REFLECT ✅：** Level 2 轮次回顾，独立文档 `memory-bank/reflection/reflection-TASK-20260419-03-round1.md`。**计划 vs 实际**：本轮 3/6 phase 完成（P0+P1+P2），3 commit 与 plan 0 偏差，~25 min < 30 min 预估。**做得好**：(1) TASK-04 修复实地生效（vx_core Release 链接 0 `-Werror=array-bounds`）= TASK-04 选 C 方案的最强 ROI 证据；(2) Rebase 冲突 3 处 MB 全 `--ours`（main 端 = 更新真相）+ pause commit `--skip` = 决策清晰可解释；(3) `RangeMultiplier(2)->Range(64,4096)` 用例数实测 7 行精确匹配 `ceil(log_2(4096/64))+1` 公式（TASK-02 反思 #5 P2 已落实）；(4) Tokenizer 吞吐 297-340 MiB/s 跨 64-4096 byte 规模稳定，证 tokenizer 摊还成本恒定。**挑战**：(1) WIP commit `c73d112 wip(TASK-03): Phase 1 CSS bench scaffolding (BLOCKED on TASK-04)` 在 TASK-04 解锁后 subject 字面失效但 reword 风险大于收益，决策保留+MB 注释；(2) 本轮范围决策需用户明确划线（"Phase 2 即止"），否则容易"最大化覆盖"诱惑；(3) `/reflect` 切「回顾中」对多 phase 任务的多轮工作流是死端，下轮 `/build` 又要切回，违反隐含 invariant。**反复模式识别**：7 类已知模式全部 ❌（无重复）；**新增首次发现**：「WIP commit subject 含外部状态过期」模式 → 建议 #1 P1 优先级；如再次出现立即升级 P0 + 写入 `git-workflow.mdc`。**改进建议**：5 项（P1×2 / P2×3）— P1 #1 WIP commit subject 中性化、P1 #3 多 phase 任务多轮 build 工作流，已迁入 `activeContext.md` 长期项；P2 已沉淀到 `systemPatterns.md`「来自 TASK-20260419-03 Round 1」段（rebase 处理标准法 + 多轮工作流临时缓解 + Phase 表「轮次切分建议」列模板）。
- **2026-04-19 第 2 轮 BUILD 启动：** MB 阶段「回顾中」→「构建中」，本轮范围 = Phase 3+4+5+6 一气呵成（用户指令"继续"）。本次启动**不**单独 chore commit，阶段切换 1 行 MB 改动 fold 进 P3 commit，降低噪音（修正第 1 轮 chore(mb) resume 单独 commit 的过度切片）。
- **2026-04-19 Phase 3 ✅：** 替换 `bench_css_parser.cc` smoke 为完整 11 BM 套件（4 stylesheet template = Small 2×5 / Medium 20×5 / Large 200×5 / Wide 20×20，6 inline RangeMultiplier(2)->Range(1,32)，1 selector 50×compound+combinator）。验证：编译干净、11 BM 全非零、ParseStylesheet 102-136 MiB/s（约 tokenizer 1/3，AST 构造主导）、DeclarationListInline 每倍 decl ~2×（线性）、SelectorListMixed 593 ns/selector（vs declaration ~6×）。提交 `feat(benchmarks): CSS parser suite [P3]` (`1e896d3`)。
- **2026-04-19 Phase 4 ✅（关键产出 — cluster 度量）：** 替换 `bench_css_property_lookup.cc` smoke 为完整 9 BM 套件（HitAll / HitHot5 / HitSingle×5 / Miss / BuildPropertyMapInit）。**Cluster 判定 = 未触发**：最慢 single key (`transition-timing-function`, 26 chars) 28.6 ns，HitHot5 10.4 ns，比例 **2.75×** — 远低于 plan §3.4 阈值 5×；HitSingle 5 个 key 7.26-28.6 ns 的 ~3.6× 跨度由 key 长度（hash O(len) + memcmp O(len)）主导，**不是** probe 距离不均。**结论**：60-entry HashMap<StringView, PropertyId> + djb2 + H1=h>>7 在短字串 + 小规模场景下 cluster 问题 = 伪命题；TASK-06 候选优先级 P1→P3。Warmup() 模式（首次 lookup 触发 lazy build 在第一行 BM 之前）保证稳态测量。提交 `feat(benchmarks): CSS property lookup + cluster probe [P4]` (`66f0a10`)。
- **2026-04-19 Phase 5 ✅：** `benchmarks/README.md` 编辑 4 段（标题 'Foundation' → 'Foundation + Core CSS'；可执行表追加 3 行 CSS；删除 '本仓库不提交基线 JSON' 一行 + 替换为 '基线 JSON（已入仓）' 段指向 baseline/；已知量级表追加 5 行 CSS TBD；注意事项追加 3 条 CSS-specific）。新建 `benchmarks/baseline/README.md`（4 段：失真警告 4 条 + 当前生成环境 8 行表 4 行 TBD + 更新协议 5 条硬规 + 命令模板含 JSON 体检 + compare.py 同机对比 + 历史表 1 行）。lint 0。提交 `docs(benchmarks): document CSS suite + baseline retention policy [P5]` (`36c7e9c`)。
- **2026-04-19 Phase 6 ✅：** 子任务 1 — `rm -rf build-bench` 触发只读 `_deps/*-src/.git`（沙箱限制），用 `chmod -R u+w` + `all` 权限解决；reconfigure 时 quickjsng FetchContent DNS 失败 → 已知 P1 reproducible — 重设 `git config --global http.proxy http://192.168.101.217:7890`（fetch 完成后留待任务收尾 unset）；reconfigure 成功 ~7.5 min（含 quickjsng + benchmark fresh clone）。**副发现**：`cmake --build build-bench -j` 整体失败 — main 已存在 2 个 Release `-Werror` 编译失败（`tests/platform/memory_surface_test.cc:102/105/108 fgets -Wunused-result`、`tests/foundation/strings/string_test.cc -Werror=array-bounds` GCC IPA 误报），与 TASK-04 同源。**不阻塞本任务**：bench 目标不依赖 tests，改用 `cmake --build build-bench --target bench_allocators bench_containers bench_hash_map bench_strings bench_css_tokenizer bench_css_parser bench_css_property_lookup -j` 7 个目标全 clean。建议立 TASK-07 修复（已记入 `activeContext.md` 长期项）。子任务 2 — 3 个 baseline JSON 用 `--benchmark_min_time=0.5s` 生成（共 ~15 KB）；体检通过 `library_build_type=release` ✅、benchmark_count = 10/11/9 ✅。子任务 3 — README 5 行 TBD 回填（关键数字：BM_TokenizeAll/4096 ~10.8 µs ~265 MiB/s、ParseStylesheetMedium ~18.5 µs、DeclListInline/8 ~1.14 µs、HitHot5 ~13 ns、HitSingle/transition-timing-function ~33 ns）；baseline/README 当前生成环境 8 行表 4 行 TBD 回填（qihooz / WSL2 6.6.87 / x86_64 8 logical 2918 MHz / gcc 11.4.0 / 2026-04-19）。子任务 4 — 全量回归：Debug `cmake --build build && ctest` 890/890 ✅；7 bench exe 全 exit 0 ✅。子任务 5 — `techContext.md` 「结果留存」段重写（区分 Foundation 4 不入仓 + CSS 3 入仓 + 4 条更新硬规），`activeContext.md` 切「构建中」并整体进度 ✅，`tasks.md` 状态切 BUILD 完成 ✅。提交（待）：`feat(benchmarks): CSS baselines + finalize [P6]`。
- **下一步：** `/reflect` 整体回顾（覆盖第 1 + 第 2 轮，对照原 plan 全 7 phase）→ `/archive`

## 已完成任务

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
