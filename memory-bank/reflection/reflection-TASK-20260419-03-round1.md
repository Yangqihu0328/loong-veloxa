# 回顾：CSS 解析性能基准 — 第 1 轮（Phase 0+1+2 of 6）

**日期：** 2026-04-19
**任务 ID：** TASK-20260419-03（**第 1 轮，未完成**；Phase 3-6 留待第 2 轮 `/build`）
**复杂度级别：** Level 2
**回顾类型：** 轮次回顾（task 整体未完成，**不可** `/archive`，需第 2 轮 `/build` 续接 Phase 3-6 后再做整体反思）

---

## 计划 vs 实际

| 维度 | 计划（全 6 phase） | 实际（本轮 = phase 0+1+2） | 偏差原因 |
|------|------|------|---------|
| Phase 数 | 6（P0-P6，含合并到 6） | 完成 3（P0+P1+P2），剩余 3（P3+P4+P5+P6 合 4 段未做） | 用户明确本轮范围 = "P1 验证 + P2 最直接收益"；其余下轮 |
| 提交数（本轮） | P1 plan + P2 feat + P3 chore_mb = 3 | 实际本轮 3：`dfdf556` chore(mb) resume / `4d014ec` feat tokenizer P2 / `fb9f8eb` chore(build) finalize | 0 偏差 |
| 提交数（rebase 前历史） | — | feature 分支 ahead 5 commits 共：`0a9c6fd` plan + `430a61e` WIP P1 + 上述 3 | rebase 把暂停期间的 plan / WIP 两 commit 保留下来；pause commit `679efaf` skip |
| 文件变更（本轮） | `bench_css_tokenizer.cc` 替换 + 3 个 MB | 实际：`bench_css_tokenizer.cc` (+105/-8) + 3 个 MB | 0 偏差 |
| 设计变更 | — | 0 | 计划在第 1 轮无需调整 |
| 时间预估 | 全 task ~2 小时；本轮 ~30 min（P1 验证 + P2 编写+验证） | 实际 ~25 min | 微短，因 Phase 1 文件早已 WIP 写好，本轮只跑编译 |
| 风险触发 | Plan 「风险预案」表第 1 行：「Phase 1 编译失败 vx_core 拉入未安装 PNG/JPEG」 | **未触发**（main 已能编 vx_core 进 examples） | 风险预案准确 |

---

## 回顾检查清单（代码变更类）

- [x] **计划精确度** — 文件清单 100% 一致；预估时间略短（~25 min < 30 min），合理
- [x] **TDD 执行情况** — 「覆盖补充」模式按 plan 声明，既有 GTest 890/890 作回归；本轮无新增 GTest（符合预期）
- [x] **子代理质量** — 本轮未使用子代理（小范围编辑 + 验证）
- [x] **测试隔离** — Tokenizer 套件中 3 个 `static const std::string css = []{...}();` 是 IIFE-init 的 BM-local 单例，不串扰；`StylesheetCorpus()` 缓存命中
- [x] **提交粒度** — 严格符合 plan 的「每 phase 1 commit」+ chore(mb) 续接 + chore(build) 收尾，无大杂烩
- [x] **非默认路径** — `BM_TokenizeAll/Range(64,4096)` 7 个规模点扫到边缘（最小 64 byte / 最大 4096 byte），WhitespaceHeavy 含注释路径，StringHeavy 含转义引号路径，覆盖了 tokenizer 状态机非主路径

---

## 做得好的

1. **TASK-04 解锁验证 = 实地 + 即时**
   - Phase 1 编译第一步即触达 `enum_serialization.cc` 重编：输出包含 `Building CXX object veloxa/core/CMakeFiles/vx_core.dir/css/enum_serialization.cc.o` → `Linking CXX static library libvx_core.a` → 3 个 CSS bench exe 全部链接成功
   - 0 `-Werror=array-bounds` 触发，证实 `LookupImpl + VX_LOOKUP` 修复在 Release `-O2` 下根因消除（不是 pragma 抑制）
   - 这是 TASK-04 选择 C 方案而非 A/B 的最强 ROI 证据

2. **Rebase 冲突处理决策清晰且可解释**
   - 3 处 MB 文件冲突来源：plan/WIP/pause 三个 commit 各自摸了 `activeContext.md`/`tasks.md`/`progress.md`，与 main 上的 TASK-04 归档冲突 — 这是结构性冲突，不是失误
   - 决策：plan + WIP 两 commit 全部 `git checkout --ours`（接受 main 端 = 更新真相 = TASK-04 已归档 + idle），pause commit `git rebase --skip`（已无意义）
   - 这套规则可写入 `git-workflow.mdc` 「长任务暂停后 rebase MB 冲突」段（见改进建议 #2）

3. **Plan 「Range 用例数公式」直接命中**
   - Plan 注释：`RangeMultiplier(2)->Range(64, 4096)` → `ceil(log_2(4096/64))+1 = 7`
   - 实测 7 行 BM_TokenizeAll/{64,128,256,512,1024,2048,4096}，零差异 — 验证 TASK-02 反思 #5 P2 公式（已沉淀到本任务 plan）
   - 后续 Phase 3 `BM_ParseDeclarationListInline/Range(1,32)` `ceil(log_2(32/1))+1=6`、Foundation `RangeMultiplier(8)->Range(64,16384)` `ceil(log_8(16384/64))+1=4` 都已用同一公式预估，全对

4. **吞吐数据一致性意外发现**
   - `BM_TokenizeAll` 在 64..4096 byte 全规模吞吐稳在 297-340 MiB/s（变异 ±7%），无 quadratic 退化
   - `StringHeavy` 603 / `WhitespaceHeavy` 614 MiB/s 显著高于 `NumericHeavy` 372 MiB/s
   - 假设：StringHeavy 主要是字符 copy 状态保持（少分支），WhitespaceHeavy 是 skip 路径短，NumericHeavy 涉及数字解析的多分支判定 — 这是 tokenizer 内部分支预测/码路径长度的有效信号，可作为 Phase 3 parser 性能与 tokenizer 占比对比的基准

---

## 遇到的挑战

1. **WIP commit subject 含「BLOCKED on TASK-04」字样在续接后过期**
   - 原 commit `c73d112` （rebase 后变 `430a61e`）的 subject 是 `wip(TASK-03): Phase 1 CSS bench scaffolding (BLOCKED on TASK-04)`
   - rebase 后 TASK-04 已合并 → "BLOCKED" 字面失效，但要清理需要 `git rebase -i HEAD~5 reword`，操作风险大于收益
   - **决策：保留不动**，在 `activeContext.md` 「WIP commit subject 注」段说明字面过期事实
   - 后果：未来 `git log` 阅读者看到 "BLOCKED on TASK-04" 会困惑，需要看 commit body 或 MB 才能厘清；这是历史真相但 UX 差

2. **本轮范围决策需用户明确，否则容易被「最大化覆盖」诱惑**
   - 本轮目标是 6 phase 中的 0+1+2，自然倾向是「既然在 BUILD，索性把 Phase 3 parser 套件也写了 — 同样小工作量」
   - 但用户原始指示是 "验证 3 smoke bench → Phase 2（最直接收益）"，明确划线在 Phase 2
   - **挑战：** Level 2 任务的多 phase 「轮次拆分」是用户决策，不是算法决策；BUILD 模式无机制提醒「到此为止」，依赖人脑

3. **Reflect 阶段切到「回顾中」会让人误以为任务可归档**
   - 当前 MB 阶段流转设计：构建中 → 回顾中 → 归档中 → 空闲
   - 但本任务 Phase 3-6 还要做，归档不行；下轮 `/build` 又要把阶段切回「构建中」 — 这违反了「回顾中后只能归档」的隐含 invariant
   - **症状：** 现有命令文档的阶段守卫不识别「轮次完成 ≠ 任务完成」

---

## 经验教训

### a. 关于多 phase 任务的轮次工作流

- Level 2+ 任务（phase 数 ≥ 5）天然适合「分轮次 build」：每 1-2 phase 一轮，期间可被其他高优先级任务打断（如本任务被 TASK-04 中断 → 回归 → 续接）
- 当前 `/build` → `/reflect` → `/archive` 命令链假设「一轮即完成」；遇到多轮工作流需要：
  - `/reflect` 后**不切**「回顾中」死端，允许再次 `/build` 续接
  - 或引入「轮次完成」中间态（`build-paused-pending-next-round`）

### b. 关于 WIP commit subject 的纪律

- WIP / 中间 commit 的 subject 应**不依赖外部任务状态**（避免 "BLOCKED on TASK-X" / "WAITING for Y" / "PENDING dep"）
- 外部依赖关系写在 commit body 而不是 subject — 因为 subject 在 git log oneline 中长期可见，body 容易被忽略
- 推荐 subject 模板：
  - `wip(scope): <feature> scaffolding (compile-pending verification)` ← 中性
  - `wip(scope): <feature> scaffolding [BLOCKED on TASK-X]` ← 反例（本任务）

### c. 关于 rebase 冲突的结构化处理

- 长任务暂停期间，main 的归档活动会触摸 MB 三件套（`activeContext.md`/`tasks.md`/`progress.md`）→ rebase 必现 MB 冲突
- 处理通用法：MB 三件套**总是接受 main 端**（`git checkout --ours` 在 rebase 上下文 = main side），因为归档后的 idle 状态总是更新真相；feature 端的 plan/WIP/pause MB 信息已通过 commit 自身 + plan/spec 文档保留
- pause commit（`chore(mb): pause TASK-X pending TASK-Y`）在 TASK-Y 解锁后失去意义，可 `git rebase --skip`

---

## 改进建议（附优先级与落实方式）

| # | 建议 | 优先级 | 落实方式 | 目标 |
|---|------|--------|---------|------|
| 1 | WIP / 中间 commit subject 禁止包含外部任务状态字样（"BLOCKED on X" / "WAITING for Y"），改用中性 "(unverified)" / "(compile-pending)" 或仅 `wip:` 前缀；外部依赖写 commit body | **P1**（**新模式首次出现**，未匹配已知 7 类反复模式） | 追加到 `git-workflow.mdc` 「Commit Subject 规范」段（如不存在则新建）；下次 wip commit 前对照 | `git-workflow.mdc` |
| 2 | 长任务暂停后 rebase main 时，MB 三件套（`activeContext.md`/`tasks.md`/`progress.md`）冲突一律接受 main 端（`git checkout --ours` in rebase context）；pause commit 一律 skip | **P2** | 追加到 `git-workflow.mdc` 「长任务 rebase MB 冲突处理」段 | `git-workflow.mdc` |
| 3 | Level 2+ 多 phase 任务（phase 数 ≥ 5）支持「轮次完成」中间态，`/reflect` 不强制切回顾中死端，允许再次 `/build` 续接；或在 `activeContext.md` 阶段字段允许 `构建中（轮次N完成，待续）` 形态 | **P1** | 修改 `/reflect`、`/build`、`/archive` 命令文档的阶段守卫；或在 `complexity-levels.mdc` 增加「多轮次 Build 工作流」段 | `.cursor/rules/workflow/complexity-levels.mdc` 或对应命令 .mdc |
| 4 | Plan 阶段在 phase 表追加「轮次切分建议」列（标注哪几 phase 适合一轮做完、哪些适合分轮，便于用户决策 BUILD 范围） | **P2** | 追加到 `writing-plans.mdc` 「Phase 表必填列」段 | `writing-plans.mdc` |
| 5 | Tokenizer 不同形态吞吐差异（StringHeavy 603 / WhitespaceHeavy 614 / NumericHeavy 372 MiB/s）作为 Phase 3 Parser 性能解读的对比基准，沉淀到 `techContext.md` 性能基线段 | **P2** | 在第 2 轮 build 完成 Phase 3 后回填 | `techContext.md` |

**落实状态：**
- P0 建议：无（无破坏性问题）
- P1 建议（#1, #3）：迁移到 `activeContext.md` 「待处理事项 → 长期项」（见步骤 5）
- P2 建议（#2, #4, #5）：记录到 `systemPatterns.md` 或 `techContext.md`（见步骤 5）

---

## 反复模式识别

| 已知模式 | 出现频率 | 本轮是否重复？ |
|---------|---------|-------------|
| 计划文件清单与实际变更不一致 | 9+ 次 | ❌ 0 偏差 |
| 子代理产出需大量返工 | 7+ 次 | N/A（本轮无子代理） |
| 前置依赖/环境/API 能力未验证 | 8+ 次 | ❌ TASK-04 已先行解锁，前置全绿 |
| 非默认路径遗漏验证 | 4+ 次 | ❌ Range 边缘 + 注释/转义/数字路径都覆盖 |
| 测试隔离问题 | 7+ 次 | ❌ IIFE-init static 单例 + Stylesheet cache 隔离 |
| 提交粒度偏离计划 | 7+ 次 | ❌ 严格符合 plan |
| TDD 严格度与场景不匹配 | 11+ 次 | ❌ 「覆盖补充」模式按 plan 声明 |

**新增模式（首次发现）：**
- **「WIP commit subject 含临时外部状态在 rebase / 续接后过期」** — 本轮首次出现；建议 #1 P1 优先级；如未来再次出现立即升级 P0 + 修改 git-workflow.mdc 强制规则

---

## 技术改进建议

1. **Tokenizer 吞吐稳定性给后续优化提供 baseline 基线**
   - 当前 297-340 MiB/s（Release WSL 8 核 2.92GHz）— 任何后续 tokenizer 优化（SIMD scan / SOA token / state machine 重构）必须以此为对照
   - Phase 6 入仓 baseline JSON 时，本轮 10 个 BM 数据点会冻结成可机读对比锚点

2. **Phase 4 cluster 度量的预期信号**
   - Phase 1 smoke 测得 `BM_PropertyLookupSmoke` 10.4 ns（5 个 hot key 轮转，平均探测代价）
   - Phase 4 `BM_PropertyLookupHitSingle/transition-timing-function` 等长 key（26 字符）若与 hot5（5-15 字符）量级差 ≥ 5×，证实 PropertyMap djb2 hash 在 60-entry 规模有 cluster；< 2× 则 PropertyMap 实测均匀，TASK-05 候选项可降级 P3
   - 这是 plan 风险预案表第 2 行的判据，已就位

3. **续接成本 vs 一气呵成成本**
   - 本任务被 TASK-04 中断了一次，续接成本约 5-10 min（rebase + 冲突解决 + MB 续接 commit）
   - 如果再被中断 1 次，累计续接成本 ~20 min ≈ 单 phase 工作量；说明 phase 拆分到 ~30 min 粒度时，单次中断成本已可观
   - 启示：未来类似多 phase 任务，**plan 阶段应把「中断恢复成本」纳入 phase 拆分粒度决策**

---

## 安全评估

| 维度 | 状态 | 备注 |
|------|------|------|
| 输入验证 | N/A | 本轮无外部输入；CSS 语料由 `css_corpus.h` 内部生成 |
| 认证/授权 | N/A | 无认证 |
| 数据保护 | N/A | 无敏感数据 |
| 依赖审计 | N/A | 本轮**未引入新依赖**（google/benchmark v1.9.1 已在 TASK-02 审计） |
| 错误信息脱敏 | N/A | 无错误响应 |
| 敏感数据处理 | N/A | 无敏感数据 |

**结论：本轮不涉及安全变更。**

---

## 下轮 `/build` 范围（Phase 3-6）

| Phase | 内容 | 预估提交 |
|-------|------|---------|
| Phase 3 | `bench_css_parser.cc` 完整 11 BM 套件（Small/Medium/Large/Wide + DeclList Range + SelectorListMixed） | 1 commit |
| Phase 4 | `bench_css_property_lookup.cc` 完整 9 BM 套件（HitAll/HitHot5/HitSingle×5/Miss/BuildInit）+ cluster 度量分析 | 1 commit |
| Phase 5 | README.md（CSS 章节扩展）+ baseline/README.md（失真警告 + 更新协议） | 1 commit |
| Phase 6 | 3 个 baseline JSON 入仓 + README 数字回填 + 全量回归验证 + 收尾 | 1 commit |

**第 2 轮启动条件：** 当前 `/reflect` 完成后，用户决定是否立即续接还是隔开做其他任务都可以；续接时直接 `/build`（feature 分支已在 `feature/TASK-20260419-03-css-benchmarks`）。
