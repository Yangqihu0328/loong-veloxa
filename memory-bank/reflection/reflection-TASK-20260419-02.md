# 回顾：补充 Google Benchmark 集成与 Foundation 性能基准

**日期：** 2026-04-19
**任务 ID：** TASK-20260419-02
**复杂度级别：** Level 2（简单增强 / 单子系统 — Foundation 基线）
**工作流路径：** `/van` → `/plan`（4 轮 brainstorm，全 A）→ `/build`（7 phase）→ `/reflect`
**TDD 模式：** 覆盖补充（Foundation 实现已稳，benchmark 是工具性补充，每 phase 验收为「编译过 + 跑出非零数字」而非 RED→GREEN）

---

## 计划 vs 实际

| 维度 | 计划 | 实际 | 偏差原因 |
|------|------|------|---------|
| Phase 数 | 8（0-7） | 8 | 一致 |
| 提交数 | 7 | **8**（含 plan + 收尾） | 收尾合并到 7.5 步骤的 `chore(build): finalize ...`，符合 build 模板 |
| 预估 BM 用例 | ~25 设计 + Range 展开 ~42 | **40**（13+8+10+9） | 设计稿"~25"是用例条目数；Range 展开后 40 与 ~42 接近 |
| 预估时间 | ~2h | ~1.5h | 因 Foundation API 已熟、smoke 模板复用，节省了样板时间 |
| 文件新增 | 6 个（4 bench + CMakeLists + README） | **6**（与计划一致） | ✅ 100% 一致 |
| 文件修改 | 4 MB 文件（不含 docs/spec/plan 本身） | 4 | ✅ 一致 |
| 计划外编译选项工作 | 0 | **1 项**：`benchmark` ALIAS 不能直接 `set_target_properties` | 见挑战 #1 |
| 计划外环境工作 | 0 | **1 项**：`http_proxy` 环境变量未被 CMake 子进程继承，被迫 `git config --global` 临时打补丁 | 见挑战 #2 |
| 全测试回归 | 0 | **0**（890/890） | ✅ 一致 |
| CVE 审计 | 通过 | 通过（v1.9.1 无任何 GitHub Security Advisory） | ✅ 一致 |

### 提交序列（8 个）

```
c7bd859 docs(plan): TASK-20260419-02 design + implementation plan         [Plan]
b545839 feat(benchmarks): scaffold + 4 smoke targets                       [P1]
33441a4 feat(benchmarks): allocators (Malloc/Arena/Pool)                   [P2]
1ca4a19 feat(benchmarks): containers (Vector/SmallVector/IntrusiveList)    [P3]
5698951 feat(benchmarks): hash_map insert/lookup/rehash                    [P4]
5625f3c feat(benchmarks): strings (BasicString SSO/heap + InternedString)  [P5]
08daad9 docs(benchmarks): README + techContext update                      [P6]
75986f1 chore(build): finalize TASK-20260419-02 memory bank state          [P7 收尾]
```

每个 phase 一次提交，message 含 phase 标签 + 关键样本数据，方便回看。

---

## 回顾检查清单

**代码变更类任务：**

- [x] 计划精确度 — 文件清单 100% 一致，预估时间偏乐观（实际更快）
- [x] TDD 执行情况 — 显式声明「覆盖补充」模式（设计 spec §7 写明），benchmark 不写 GTest，每 phase 验收用 `./build/benchmarks/bench_xxx --benchmark_min_time=0.01s | grep -c '^BM_'`，符合 `writing-plans.mdc` 的"测试模式选择"条款
- [x] 子代理质量 — 本次未使用子代理（Level 2 + 文件少 + 无跨模块依赖，主代理直接执行更高效）
- [x] 测试隔离 — `BM_InternedString*` 用例在 SetUp/TearDown 显式 `ClearInternedStrings()` 防止全局表污染，3 个 intern BM 互不影响
- [x] 提交粒度 — 严格按 phase 提交，无大杂烩
- [x] 非默认路径 — `BM_VectorReservePushBackInt` 对照 `BM_VectorPushBackInt` 验证 grow 路径成本；`BM_HashMapLookupMissInt` 对照 hit；`BM_StringConstructHeap` 对照 SSO；`BM_SmallVectorOverflow` 对照 inline；都覆盖了

**配置/规则类任务：**

- [x] 文件位置验证 — 修改 `CMakeLists.txt` 前先 Read 确认 `VX_BUILD_BENCHMARKS` 选项位置；写 bench .cc 前先 Read 4 个 Foundation 头文件确认 API
- [x] 交叉引用 — `techContext.md` 同时更新「依赖表」「Benchmark 启用段」「技术债 #1」三处，保持一致

**安全相关任务：** 见 §安全评估

---

## 做得好的

1. **设计 spec 已含 CMake 链接方向 + FetchContent C 子项目编译选项审计**（设计稿 §4 §5 主动套用 TASK-20260419-01 沉淀的两个 checklist），实际构建时果然命中 § 5.3 预测的 `-Werror` 冲突 — 设计已留好"对 benchmark target 单独豁免"的预案。
2. **决策栈一次锁定**（4 轮 brainstorm 全 A），后续无任何方向反悔，VAN/PLAN 阶段总耗时不到 build 耗时的 1/3。
3. **Foundation API 先 Read 再写 bench**：Vector/SmallVector/IntrusiveList/HashMap/String/InternedString 头文件全部前置阅读，避免了「猜 API 后编译失败」的反复模式（这是 19-01 反思 #5 教训的外推）。
4. **SYSTEM include 隔离 -Werror**：`set_target_properties(benchmark PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES ...)` 一次性解决第三方头文件 warning 污染下游，不需要在每个 bench target 上 `target_compile_options(... -Wno-error)` 散落写法。这是本次新发现的 CMake 模式，已写入 techContext。
5. **副发现可视化**：`BM_HashMapLookupHitInt/16384 = 9µs` 这类异常立刻被识别（数量级偏离 100×），且追到根因（`H1=h>>7` + `std::hash<int>` 让所有起始探测位置压在 [0,127]），写入 README 留作独立任务候选 — 体现"基准价值不止于数字，还包括异常诊断"。
6. **本机网络问题诊断闭环**：发现 cmake 子进程不继承 `http_proxy` → 立刻切换到 `git config --global http.proxy`，并明确告知用户「Phase 7 会 `--unset` 恢复」，最终验证恢复成功，遵循"修改 git config 必须有恢复闭环"的稳定操作。

## 遇到的挑战

1. **CMake ALIAS target 不可写属性**：第一次写 `set_target_properties(benchmark::benchmark ...)` 失败，错误信息 `set_target_properties can not be used on an ALIAS target`。需要用底层真实 target 名 `benchmark`。这是设计 spec §5 没预见的细节（spec 只说"对 benchmark target 单独豁免"，没说要绕开 ALIAS）。
2. **CMake FetchContent 子进程不继承 shell 代理变量**：`export http_proxy=...` 在父 shell 里 `git --version` 能识别代理，但 `cmake -B build` 内部启动的 `git clone` 失败 `Could not resolve host: github.com`。Cursor 沙箱的 `__CURSOR_SANDBOX_ENV_RESTORE` 机制把 `HTTP_PROXY/HTTPS_PROXY` 等变量替换成了 `127.0.0.1:40601`（沙箱的本地 SOCKS 跳板），覆盖了用户设的 `192.168.101.217:7890`。被迫退化为 `git config --global http.proxy` 全局补丁。
3. **Build type 默认 Debug**：`cmake -B build` 不显式指定 `-DCMAKE_BUILD_TYPE=Release`，google/benchmark 启动时打印 `***WARNING*** Library was built as DEBUG`，所有数字失真 10-100×。设计稿和计划没强制 Release，README 已补救，但下次类似任务应在 Plan 阶段就强制写入。
4. **Plan 中 BM 数量估算偏差**：spec §6.1 写 "≈ 4×3 + 3×3 + 3 + 1 + 1 = **17 用例**"，实际按 `RangeMultiplier(8)->Range(8,4096)` 展开是 4 个值，所以 MallocAlloc/ArenaAlloc 各 4 个 + PoolAlloc 3 个 + 2 个独立 = **13** 而非 17。错在我把 RangeMultiplier 当成了"每量级 3 步"。**对量级影响为零**，但暴露了"用 google/benchmark Range 公式估算精确数量"的一处算错。
5. **HashMap 用例无意中暴露被测库自身的性能 bug**：本意是测吞吐，意外发现 cluster 问题。这其实是好事（Bonus 发现），但说明"基线建立 ≠ 性能 OK"，写入文档时要明确区分"基线"和"已优化"。

## 经验教训

1. **L2 任务不必启动子代理**：本次 8 个提交、6 个新文件、~600 行代码，主代理直接做更快、心智负担更低。子代理适合（≥3 个高自由度并行子任务） + （主代理上下文紧张）的情况，本任务都不满足。
2. **CMake ALIAS 是项目的常见绊脚石**：`benchmark::benchmark` / `Freetype::Freetype` / `GTest::gtest` 等"::"形式 95% 是 ALIAS。下次涉及对第三方 target 改属性时，第一动作是 `get_target_property(... ALIASED_TARGET)` 拿底层名。
3. **Cursor 沙箱代理变量优先级**：在 Cursor 内执行 `cmake/git/curl` 等需联网命令时，`HTTPS_PROXY` 大小写变体、`NO_PROXY` 范围、`SOCKS_PROXY` 都被沙箱默认设了 `127.0.0.1`。要改用项目代理，**最稳妥的做法**是：
   - 走 `git config --global http.proxy` 走 git 配置层，绕开环境变量栈
   - **务必**在任务收尾 `--unset` 恢复
   - 这条本质上是「修改 git config 必须有恢复闭环」的特化
4. **Benchmark 任务必须 Plan 阶段就强制 Release**：下次扩 CSS/Layout/Render benchmark 时，命令必须写成 `cmake -B build-bench -DCMAKE_BUILD_TYPE=Release -DVX_BUILD_BENCHMARKS=ON`（独立 build dir 避免污染 Debug 测试 build）。
5. **Range 估算经验值**：Google Benchmark 的 `RangeMultiplier(m)->Range(lo,hi)` 实际生成 `ceil(log_m(hi/lo)) + 1` 个值，不是「lo, hi 之间任意梯度」。下次估算 BM 数量时按这个公式算。
6. **「基线」与「优化结论」的区别**：写 README 时把 `BM_HashMapLookupHitInt/16384 = 9µs` 标为 ⚠️ 而非平铺数字，避免下次读者误以为这是 HashMap 的"正常水平"。

---

## 改进建议（附优先级与落实方式）

| # | 建议 | 优先级 | 落实方式 | 目标 |
|---|------|--------|---------|------|
| 1 | Plan 阶段「Benchmark 启用」必须显式写 `-DCMAKE_BUILD_TYPE=Release` 命令，且要求新建独立 `build-bench/` 目录避免与 Debug 测试 build 串扰 | P1（下次 CSS/Layout bench 前） | `writing-plans.mdc` 增加段「性能基准任务必检项」 | `.cursor/rules/skills/writing-plans.mdc` |
| 2 | CMake 操作第三方 target 前必须先识别是否 ALIAS（`get_target_property(... ALIASED_TARGET)`），若是则切换到底层 target 名 | P1（下次 FetchContent 涉及第三方 target 改属性时） | `writing-plans.mdc` 「CMake 链接方向约束分析」段后追加「ALIAS 识别步骤」 | `.cursor/rules/skills/writing-plans.mdc` |
| 3 | Cursor 沙箱内涉及 `FetchContent` 时，VAN 阶段必须验证「子进程能否真正用上自定义代理」，而不只是验证「父 shell 的 `curl` 能用」；推荐方案是直接写到 `git config --global` 并明确恢复时机 | P1（下次任何 FetchContent 任务） | `main.mdc` 或 `writing-plans.mdc` 添加「Cursor 沙箱代理验证」段 | 待选：放 `main.mdc`「网络与代理」新章 |
| 4 | HashMap cluster 问题（`H1=h>>7` 对小整数 key 退化）应作为独立 P2 任务立项 | P2 | 立项 `TASK-20260419-05`（建议名：HashMap Hash Mixing 优化） | 任务库 |
| 5 | google/benchmark 的 `RangeMultiplier`/`Range` 估算公式 `ceil(log_m(hi/lo))+1` 写入 spec 模板供下次参考 | P2 | `writing-plans.mdc` 「Benchmark 用例估算」附录 | `.cursor/rules/skills/writing-plans.mdc` |

**已落实状态：**
- 建议 #1/#2/#3：本次 README 内已说明（`benchmarks/README.md` 第 1 段强调 Release 必选；techContext 「Benchmark 启用段」记录了 SYSTEM include + ALIAS target 经验）；规则文件层面的固化迁移到 `activeContext.md` 待处理事项
- 建议 #4：本次写入 `benchmarks/README.md` 已知量级表的 ⚠️ 行；正式立项待用户决定
- 建议 #5：本次仅写到本反思中，规则层面留待 #2 同批迁移

---

## 反复模式识别

| 已知模式 | 出现频率 | 本次是否重复？ |
|---------|---------|-------------|
| 计划文件清单与实际变更不一致 | 9+ 次 | ❌ **未重复**（100% 匹配） |
| 子代理产出需大量返工 | 7+ 次 | N/A（未使用子代理） |
| 前置依赖/环境/API 能力未验证 | 8+ 次 | ⚠️ **部分重复** — VAN 阶段验证了「父 shell `git clone` 能用代理」，但没验证「CMake 子进程能用代理」；这是 #3 改进建议要解决的；按规则升至 P1 |
| 非默认路径遗漏验证 | 4+ 次 | ❌ **未重复**（reserve/grow、SSO/heap、hit/miss、inline/overflow 都对照） |
| 测试隔离问题 | 7+ 次 | ❌ **未重复**（InternedString 显式 ClearInternedStrings） |
| 提交粒度偏离计划 | 7+ 次 | ❌ **未重复**（每 phase 一提交，message 含 phase 标签） |
| TDD 严格度与场景不匹配 | 11+ 次 | ❌ **未重复**（spec §7 显式声明「覆盖补充」模式，每 phase 有量化验收） |

**净结论：** 1 项部分重复（环境验证未到子进程粒度），已升 P1 进入 activeContext 待处理；其余 6 项历史反复模式本次全数避免。这是 19-01 改进措施开始见效的信号。

---

## 技术改进建议

1. **HashMap Hash Mixing**：当前 `H1=h>>7` 对低位熵高、高位为 0 的 key（小整数、对齐指针）严重退化。可选方案：
   - SplittableRandom 风格 `h ^ (h >> 32)` 后 mod cap
   - FxHash / xxhash 替换 `std::hash<int>` 的恒等
   - 至少：`H1=mix(h)` 后再 `& (cap-1)`
2. **Benchmark 独立 build dir**：`build-bench/` 与 `build/` 分离，前者强制 Release，后者保留 Debug 用于 GTest，互不污染编译选项与缓存。
3. **MallocAllocator 测量基线注解**：`BM_MallocAlloc` 含 `GlobalMemoryStats::RecordAlloc` 计数器开销，README 已注明，但代码层面可考虑提供一个「无统计」编译期开关（`#ifdef VX_DISABLE_MEMORY_STATS`）专给 benchmark 用，让对比更纯。

---

## 安全评估

| 维度 | 状态 | 备注 |
|------|------|------|
| 输入验证 | N/A | 无外部输入（benchmark 只读自有内存对象） |
| 认证/授权 | N/A | 无认证概念 |
| 数据保护（加密/脱敏） | N/A | 无敏感数据 |
| 依赖审计 | ✅ | google/benchmark v1.9.1：GitHub Security Advisories 0 条；WebSearch 确认 v1.9.1 (2024-11-28) 无 CVE；项目 ~10.1k star、Apache-2.0、Google 官方维护，供应链来源可信 |
| 错误信息脱敏 | N/A | benchmark 仅打印自带统计 |
| 敏感数据处理 | N/A | 无 |

**净结论：** 本任务为纯内部性能测量基础设施，唯一新依赖经审计无 CVE。
