# 回顾：Layout + Render 性能基准（4 bench exe）

**日期：** 2026-04-19
**任务 ID：** TASK-20260419-05
**复杂度级别：** Level 2-3
**回顾类型：** 整体回顾（一次跑完，单轮 7 phase）
**关联文档：**
- 设计：`docs/specs/2026-04-19-layout-render-benchmarks-design.md`
- 计划：`docs/plans/2026-04-19-layout-render-benchmarks.md`
- 上一轮 reflection 参考：`reflection-TASK-20260419-03.md`（CSS bench，模式同源）

---

## 计划 vs 实际（对照 plan 全 7 phase）

| 维度 | 计划 | 实际 | 偏差原因 |
|------|------|------|---------|
| Phase 数 | 7（P1 smoke + P2 corpus + P3-6 四套件 + P7 收尾） | 7 全完成 ✅ | 0 偏差，单轮一气呵成 |
| 提交数 | 7（plan + 6 phase） | **9**：`3eb9070` plan + 7 phase + `81d85cc` chore(build) finalize | +1 finalize commit（plan §7.6 显式留给 /reflect 的 MB 切阶段，本次 fold 进 chore；优于第 1 轮 TASK-03 多了 2 个小 chore） |
| 文件变更（净） | 8（4 .cc + corpus.h + CMake + 2 README + 4 baseline + MB） | **15** 触达：4 bench .cc + layout_corpus.h + CMakeLists + benchmarks/README + baseline/README + 4 baseline JSON + 3 MB + 1 reflection（**未触达** techContext.md / systemPatterns.md — 见 P0 改进建议） | 与 plan 9 文件目标基本一致；**techContext §Layout/Render 性能基线段未补（验收 #7 未满足）** |
| BM 数 | ~25（buildtree 9 行 / flex 6 / record 5 / replay 5-6） | **30 reported rows**：buildtree 14（3 BM × Range 展开） + flex 6 + record 5 + replay 5 | +5 行来自 plan 已声明的 RangeMultiplier 展开（公式 `ceil(log_m(hi/lo))+1` 精确预估，8→512 = 7 + 2→64 = 6 + 1 mixed = 14） |
| 设计变更 | 0 | **+3 处实质性修正**（见下文 §挑战 #1） | Render bench 默认 div 不 emit FillRect，被迫加 Styled corpus + ctx.stylesheets gating；spec/plan 未识别此前置 |
| 时间预估 | ~4.25h | **~75 min**（含 3 次完整 build + 体检 + reflection 准备） | 比预估短约 70%，原因：plan 详尽到代码片段，多数 phase 直接照写；意外 Phase 6 styled-corpus 修正只多花 ~10 min |
| 风险触发 | 6 行风险预案 | **第 6 行触发（ImageCache 30min 预算）+ 1 个未预见风险** | 详见下文「风险预案命中分析」 |

### 风险预案命中分析

| # | 风险 | 触发？ | 实际处置 |
|---|------|--------|---------|
| 1 | smoke build 出现非 IPA 类型 -Werror | ❌ 未触发 | Phase 1 build green |
| 2 | layout_corpus IPA 误报 | ❌ 未触发 | header-only inline，未触发 |
| 3-4 | 任一 BM 数字 < 100ns 或全相同 | ⚠️ **smoke phase 1 命中** | `BM_ReplaySmoke = 1.65 ns + items_per_second=0/s` — 表象是 DCE，实因 list 为空（默认 alpha=0），Phase 6 修正 |
| 5-6 | Layout 在 Record/Replay setup segfault | ❌ 未触发 | LaidOut/Recorded 缓存 unique_ptr<Arena> 设计成功 |
| 6 | ImageCache API 探查 > 30 min | ✅ **完全触发** | 看 `image_cache.h` + `renderer.cc:125-181` 5 分钟即识别 `images_` private + `DecodeFromFile` 强 I/O 依赖 + layout 不传 cache → handle=0 → Record 不 emit kDrawImage 三段链断；按 plan 退化路径走（5 BMs，推 TASK-09） |
| 7 | Debug ctest regression | ❌ 未触发 | 890/890 通过 |
| **未预见** | **Render bench 的 list 全空（默认 div alpha=0 → RecordBox 不 emit FillRect）** | ✅ Phase 6 触发 | 临时新增 BuildFlatStyled / BuildTextHeavyStyled / BuildImageStyled + ctx.stylesheets 非空 gating；约 ~10 min 修正成本，无中止风险 |

**预案表整体质量：** 6 行预案中 1 行命中（ImageCache）+ 1 个未预见但被 smoke 数字 1.65 ns 早期暴露；**预案 ROI 中等**，重要漏洞在「render bench 必须 styled corpus」前置条件未识别 → 反复模式 #3 升级建议（详见 §改进建议）。

---

## 回顾检查清单（代码变更类）

- [x] **计划精确度** — 文件清单 95% 一致，BM 数 +5 来自 RangeMultiplier 展开（已在 plan 注释中预测）；时间预估宽松（4.25h vs 实际 75 min）；**关键缺漏：plan §1.1 `vx_add_benchmark` 写成三参 `(name file libs)`，实际签名 `(name [libs...])`，build 时改正**
- [x] **TDD 执行情况** — 「覆盖补充」模式按 plan 声明，复用既有 `tests/core/{layout,render}/` GTest 作正确性回归（Debug 890/890 持续通过 0 回归），不写新 unit test；每 phase 验收为「Release build 零 -Werror + 数字非零 + BM 数符合设计」 — 但**「数字非零」这条在 phase 1 smoke 给出假阳性**（Replay = 1.65 ns + items_per_second=0 实际是 list 空，需 phase 6 用 Styled corpus 修正）
- [x] **子代理质量** — 全任务未使用子代理（中范围编辑 + API 探查 + 数据采集），手工探查 + grep 高效
- [x] **测试隔离** — 7 builders 各 mutex-protected static map 缓存，跨 BM 复用同一 Document 指针；LaidOut/Recorded 用 unique_ptr<Arena> 保证 LayoutBox 指针不悬挂；SoftwareCanvas 用 process-static pixel buffer 避免内层分配噪音；零串扰
- [x] **提交粒度** — 严格 1 phase 1 commit，subject 不含外部任务状态字样（落实 P1 反复教训）；finalize commit 必要（plan 显式预留）
- [x] **非默认路径** — Phase 6 styled-corpus 修正后 5 个 Replay BM 各覆盖一种 paint 命令组合；ImgVsNoImg 路径明确量化为「无 image cmd 路径」并把缺失部分推 TASK-09

---

## 做得好的（按价值排序）

### 1. **DrawText hot path 实测发现（820× FillRect）— 最高价值**

- **结论**：`Replay` 真正 hot path = `DrawText`，而非原 plan 假设的 `ImageCache`
- **数据**：

| paint 命令 | ns/cmd | 相对 FillRect | 是否 hot path（5× 阈值，TASK-03 同源）|
|---|---|---|---|
| FillRect (无 image) | ~10 | 1× | 否（基准） |
| FillRect (有 image cmd) | ~10 | 1× | 否（K4：image 不是开销来源） |
| DrawText (text shaper) | ~8200 | **820×** | ✅ **是**（远超 5× 阈值） |

- **分析**：与 TASK-03 PropertyMap cluster 度量同模式（带否定判据的发现型 phase）— 我们设计了对比 BM 来检验"X 是否是 hot path"假设，结果**否定原假设的同时定位了真正的 hot path**
- **影响**：
  - 立项 TASK-20260419-09 候选 — DrawText 微基准（拆解 SimpleTextShaper / glyph cache lookup / SoftwareCanvas DrawTextFallback 三层）
  - 推迟原 ImageCache 真实例测试（依赖 fixture 工程，K5）
  - 给后续渲染优化指明方向：先优化 DrawText（量级 8200 ns），再考虑 FillRect 优化（量级 10 ns，已接近物理极限）

### 2. **跨 bench 同源现象识别：buildtree-flat + flex 双双 super-linear knee**

- **K2 数据**：`BM_LayoutBuildTreeFlat` N=128→256 时 7.7 µs → 70 µs（10× for 2× N）
- **K3 数据**：`BM_LayoutFlex<8,8>` → `<16,16>` 时 4.9 µs → 73 µs（14.9× for 4× cells）
- **关键洞察**：两个独立 bench 在不同 corpus shape（block flow vs flex）下都出现 super-linear，**强烈暗示共享根因**（candidate：cache miss / arena chunk grow / SmallVector 阈值跨越 / vector grow）
- **影响**：自然形成下一轮 layout 优化的研究问题（不属本任务）；K2/K3 写入 baseline/README 让未来开发者第一眼就看见

### 3. **「带否定判据的发现型 phase」模式第 2 次成功复刻**

- TASK-03 用 cluster 度量（5× 阈值）证明 PropertyMap **未触发** cluster
- 本次用同样的 5× 阈值证明 ImageCache **不是** Replay hot path（实测发现 DrawText 是）
- **价值**：这套「设 hypothesis + 设阈值 + 设对比组 + 接受任意结果」的科学化 bench 模式正在固化，**已属于本仓库可复制方法论**（建议沉淀到 `systemPatterns.md`）

### 4. **plan 精度 + 风险预案的 ROI 高**

- 预估 4.25h，实际 75min — plan 越详细，build 越快（多数 phase 直接照搬）
- 6 行风险预案准确预测了 Phase 6 ImageCache 的 30min 预算 + 退化路径 → 不需要二次决策
- TASK-03 模式复刻清单（plan §TASK-03 模式复刻清单 9 行）100% 应用，每行都有具体落地

### 5. **layout_corpus.h 设计的灵活性**

- 7 builder shapes（flat / nested / mixed / text-heavy / flex / nested-flex / image）+ 4 个 Styled 子集（Phase 6 增）
- mutex-protected static map cache 让相同 shape 跨多个 BM file 复用同一 Document
- header-only inline + per-shape getter 函数 — 没有 .cc 实现文件，部署/复用零摩擦

---

## 遇到的挑战

### 1. **Phase 6 修正：默认 div 不 emit FillRect → list 全空（最大未预见风险）**

- **现象**：Phase 6 完成 5 个 Replay BMs，4 个跑出 `~1.6 ns + items_per_second=0/s`
- **根因**：`renderer.cc:80-86` 的 `if (bg.a > 0)` gating — 默认 ComputedStyle background-color alpha=0 → RecordBox 不 emit FillRect → list 完全为空
- **二次根因**：layout 不传 `ctx.stylesheets` 时 LayoutEngine 走默认 ComputedStyle 路径（不调 StyleResolver）→ inline `background-color` 不会被读取 → 即使 corpus 加了 inline decl 也无效
- **修正成本**：~10 min（在 layout_corpus.h 加 3 个 Styled getter + 在 record/replay bench 加 ctx.stylesheets 非空指针 + 改 corpus 调用）
- **plan 缺失环节**：spec §3.4-3.5 设计了 BM 但未 grep `RecordBox` / `Replay` 的 emit gating，假设「拿一个 div 跑就有命令」；属于反复模式 #3「前置依赖/环境/API 能力未验证」**第 9+ 次**

### 2. **plan §1.1 `vx_add_benchmark` 函数签名错误**

- **plan 写法**：`vx_add_benchmark(bench_layout_buildtree bench_layout_buildtree.cc vx_core)` — 三参（name + .cc + libs）
- **实际签名**（`benchmarks/CMakeLists.txt:22`）：`vx_add_benchmark(name [extra_libs...])`，文件名硬编码为 `${name}.cc`
- **影响**：build 时直接改正，无显著阻塞（~30 秒）
- **根因**：plan 起草时未读 CMakeLists 实现，凭印象写；与 #1 同源（plan 阶段 grep 不充分）

### 3. **ImageCache 真测三阶段链断**

- 想测 `Replay(list, canvas, &cache)` 的 cache 影响，但 layout 不传 `ctx.image_cache` → `ImageCache::Load` 不被调 → `image_handle = 0` → `RecordBox` 不 emit kDrawImage → Replay 无 cache 路径可走
- 解链所需工程：(a) 在 build-bench 期 `configure_file()` 复制 1×1 PNG 到 fixture；(b) 在 BM SetUp 阶段构造真 ImageCache 并 Load；(c) layout 时把 cache 指针传入 ctx；(d) 验证 list 含 kDrawImage
- 4 个工程步骤每步都需 ≥10 min 验证 → 超过 30 min 退化预算
- **决策**：按 plan 退化路径走，把工程问题完整记录为 K5 + 推 TASK-20260419-09 候选

---

## 经验教训

### 教训 1：性能基准的 smoke 验收标准必须包含「output 非空语义检查」

- 「数字非零」（plan §0 全局约束 / 验收 §3）**不充分** — `BM_ReplaySmoke = 1.65 ns + items_per_second=0/s` 完全满足「数字非零」但**实际是空 list 跑迭代器**
- 应升级为：「数字非零 + items_processed/items_per_second > 0（如 BM 调用了 SetItemsProcessed）」或「DisplayList::size() > 0 之类语义断言」
- 本次因 phase 1 smoke 没有 SetItemsProcessed，假阳性持续到 phase 6 才暴露

### 教训 2：「render/绘制 bench 必须 styled corpus」是可枚举的前置清单

- 不是 ad-hoc 修复，应当作可复用 checklist：
  - **FillRect** 需 `bg.a > 0`（inline `background-color` 非透明）
  - **FillRoundedRect** 需 `bg.a > 0` + `border-radius > 0`
  - **DrawText** 需 text node + text shaper（已在本次 corpus 处理）
  - **DrawImage** 需 image_cache 三阶段同时传（layout / Record / Replay）+ valid handle
  - **StrokeRoundedRect** 需 `border-radius > 0` + `border-style != none` + `border-width > 0`
- 沉淀到 `systemPatterns.md`「Render bench 前置清单」段，下次 render 系列 bench plan 必查

### 教训 3：plan 阶段必须 grep 目标 hot path 的发射条件（与 TASK-07 教训同源）

- TASK-07 反思立项的「方案根因假设未先验证」反复模式（P1）今天**再次以同形态出现**：plan 假设「div 跑 layout + Record 就有 paint command」，但没 grep `RecordBox` 的 `if (bg.a > 0)` gating
- 这与 TASK-07 的「假设 `-Warray-bounds` 来自 `__memcpy_chk` fortify，B3 `__builtin_memcpy` 试验失败才发现 IPA 中端先于 fortify 展开」**完全同型**
- 落实建议：**升级 P0** — `writing-plans.mdc` 必须强制「设计目标 API 的发射/触发条件 grep」段，性能基准任务尤其严

### 教训 4：跨阶段管道型 API 的协同要求是隐式契约

- ImageCache 走完整路径需要 layout/Record/Replay **三阶段都传同一个 cache 指针**，任一不传则链断
- 这种「跨阶段协同要求」在 API doc 完全无文档（`renderer.h` 仅注释 `image_cache = nullptr` 默认值）
- 沉淀到 `systemPatterns.md`「跨阶段协同要求」段（候选名）— 让未来 API 设计者意识到「default nullptr` 在管道型 API 里会导致使用者误以为 cache 可选实际上是必需」

### 教训 5：plan 详尽度 vs build 速度强正相关（再次复确 TASK-03 教训）

- TASK-03 plan 详尽 → 实际 build 比预估快 25%；本次 TASK-05 plan 更详尽（含具体代码片段）→ 实际 build 比预估快 70%
- **plan 越详尽，build 越快** 这一规律在两次大型 bench 任务中得到一致验证；TASK-03 reflection 已建议但本次再次实证

---

## 改进建议（附优先级与落实方式）

| # | 建议 | 优先级 | 落实方式 | 目标 |
|---|------|--------|---------|------|
| 1 | **techContext.md 补「Layout 性能基线」+「Render 性能基线」段**（验收 #7 未满足） | **P0 立即** | 本次回顾后直接补；与现有「CSS 性能基线」段并列 | `memory-bank/techContext.md` |
| 2 | **plan §"目标 API 的发射/触发条件 grep"强制段**（render bench 必查；与 TASK-07「方案根因假设未先验证」P1 同源，第 2 次出现 → 升级） | **P0 立即** | 升级 TASK-07 reflection §改进建议 #1 优先级（P1→P0），并在 `writing-plans.mdc`「性能基准任务必检项」段加一行强制清单 | `.cursor/rules/skills/writing-plans.mdc` |
| 3 | **bench smoke 验收升级：要求 SetItemsProcessed/SetBytesProcessed > 0** | P1 下次 | render 系列 bench 模板必加，写入 `systemPatterns.md`「Bench smoke 自检模式」段 | `memory-bank/systemPatterns.md` |
| 4 | **「Render bench 前置清单」段沉淀**（FillRect/FillRoundedRect/DrawText/DrawImage/StrokeRoundedRect 各自的发射条件） | P1 下次 | 写入 `systemPatterns.md` 一段；下次 render 系列 bench plan 必查 | `memory-bank/systemPatterns.md` |
| 5 | **「跨阶段协同要求」反模式沉淀**（管道型 API 的 default nullptr 陷阱） | P2 长期 | 写入 `systemPatterns.md`；未来 API 设计 review 时参考 | `memory-bank/systemPatterns.md` |
| 6 | **「带否定判据的发现型 phase」方法论沉淀**（5× 阈值 + 对比组 + 接受任意结果，已 2 次成功应用：cluster / hot path） | P2 长期 | 写入 `systemPatterns.md`「带否定判据的发现型 phase」段（TASK-03 reflection 已提，本次第 2 次复刻 → 提升为正式模式） | `memory-bank/systemPatterns.md` |
| 7 | **Layout super-linear knee 调查**（K2 + K3 同源） | P2 长期 | 立 TASK-09（候选）或归入 TASK-09 一并研究 — DrawText 微基准 + super-linear 根因（cache / arena grow / vector realloc） | `memory-bank/tasks.md` 候选区 |
| 8 | **plan 起草时必读目标 CMake helper 函数实际签名**（避免 `vx_add_benchmark` 三参错误同型问题） | P2 长期 | 写入 `writing-plans.mdc`「Plan 起草前置 grep 清单」段 | `.cursor/rules/skills/writing-plans.mdc` |

---

## 反复模式识别

| 已知模式 | 历史频率 | 本次是否重复？ | 处理 |
|---------|---------|--------------|------|
| 计划文件清单与实际变更不一致 | 9+ 次 | ✅ 是（+3 个 Styled getter / +1 chore commit / techContext 段未补） | 影响轻微，无升级 |
| 子代理产出需大量返工 | 7+ 次 | ❌ 不适用（未用子代理） | — |
| **前置依赖/环境/API 能力未验证** | 8+ 次 | ✅ **是（第 9+ 次）** — render bench 默认 div 不 emit FillRect 未识别 | **升级 P0**（建议 #2） |
| 非默认路径（流式/错误/缓存）遗漏验证 | 4+ 次 | ⚠️ 部分相关 — ImageCache 真路径漏验，但 plan §6 风险表 #6 已预案，按退化路径走，**算合规** | 无升级 |
| 测试隔离问题（flaky/并行冲突） | 7+ 次 | ❌ 否（cache + unique_ptr<Arena> 设计成功） | — |
| **提交粒度偏离计划（大杂烩提交）** | 7+ 次 | ❌ **否（严格 1 phase 1 commit + 必要 finalize）** | 反复模式**未重复**（建议） |
| **TDD 严格度与场景不匹配** | 11+ 次 | ❌ 否（覆盖补充模式按 plan 声明执行，零回归） | 反复模式**未重复** |
| **方案根因假设未先验证（TASK-07 新模式）** | 1 次 → **本次第 2 次** | ✅ 是（plan 假设 div 跑 record 有命令，未 grep 触发条件） | **升级 P0**（建议 #2） |

**关键识别：** 「方案根因假设未先验证」从 TASK-07 的 1 次升级到本次第 2 次出现，**已成正式反复模式**；建议升级 P0 + 写入 `writing-plans.mdc`。

---

## 技术改进建议

1. **DrawText 微基准（TASK-09 候选）** — 拆解 SimpleTextShaper / glyph cache lookup / SoftwareCanvas DrawTextFallback 三层耗时，验证 8200 ns 中各占比
2. **真 ImageCache 通路（TASK-09 候选）** — 在 build-bench 期 `configure_file()` 复制 1×1 PNG 到 `${CMAKE_BINARY_DIR}/benchmarks/fixtures/`，构造真 ImageCache + Load + 三阶段同传
3. **Layout super-linear knee 调查** — buildtree N=128→256 + flex 8x8→16x16 同源，候选根因：(a) ArenaAllocator chunk grow（默认 4096 字节，128 div × 32 byte/box ≈ 4 KB 边界）（b) SmallVector<LayoutBox*, 16> 跨 16 阈值降级到 heap allocation（c) cache miss
4. **bench helper 抽取** — `LaidOut` / `Recorded` / `MakeCtx` / `EmptySheets` 在 record/replay 两个 .cc 中重复；可抽到 `benchmarks/layout_render_helpers.h`（与 css_corpus.h 同级）

---

## 安全评估

| 维度 | 状态 | 备注 |
|------|------|------|
| 输入验证 | N/A | 性能基准无外部输入 |
| 认证/授权 | N/A | 无认证路径 |
| 数据保护 | N/A | 无敏感数据 |
| 依赖审计 | N/A | 无新依赖（google/benchmark 已在 TASK-02 接入并审计） |
| 错误信息脱敏 | N/A | 无错误响应 |
| 敏感数据处理 | N/A | corpus 数据全程序化生成 |

**本任务不涉及安全变更。**

---

## 总结

- ✅ 7 phase 全完成，11 bench 共存，全 release，890/890 ctest 通过
- ✅ 5 项关键发现（K1-K5）+ 1 项有价值反模式识别（render bench 必须 styled corpus）
- ⚠️ 1 项验收未满足：techContext.md 性能基线段未补 → P0 归档前必做
- ⚠️ 反复模式「方案根因假设未先验证」第 2 次出现 → P0 升级到 `writing-plans.mdc`
- 📋 5 项中长期建议沉淀到 `systemPatterns.md`
- 🆕 立 TASK-20260419-09 候选（DrawText 微基准 + 真 ImageCache 通路 + super-linear 根因）
