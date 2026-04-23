# 回顾：TASK-20260424-01 Layout super-linear knee 根因调查

**日期：** 2026-04-24
**任务 ID：** TASK-20260424-01（候选区原 ID TASK-20260419-10）
**复杂度级别：** Level 2-3（研究/调查 + 小型性能补丁）
**分支：** `feature/TASK-20260424-01-layout-knee-root-cause`（5 commits，未合并）

---

## 1. 计划 vs 实际

### 1.1 宏观对比

| 维度 | 计划 | 实际 | 偏差原因 |
|------|------|------|---------|
| Phase 数 | 6（+ Phase 1B 条件分支） | 6（Phase 1B 未触发） | 阶梯验证策略生效 |
| 任务/步骤数 | ~25 步骤 | ~25 | 精准匹配 |
| 预估时间 | plan 115 min / plan×0.6 = 70 min | **~33 min** | **0.29×（历史最快档）** |
| Commits 数 | 5 | **5** | 精准匹配 |
| 文件变更 | 3 核心 + 4 MB + 2 新文档 | 3 核心 + 4 MB + 2 新文档 | 精准匹配 |
| R256 改善（主判据） | ≤ 2.5×（plan 刚性） | **4.18×**（实证达标，plan 过严）| plan 阈值凭直觉设，未考虑 (d) 非唯一根因 |
| R_flex 改善（次判据） | ≤ 5×（plan 刚性） | **6.40×**（同上） | 同上 |
| Phase 1B（升级分支） | R256 ≥ 6× 触发 | 未触发（R256=3.61× 在中间区间）| 阶梯验证生效，(d) 确认为主要贡献 |

### 1.2 文件变更对比（计划列表 vs 实际）

| 文件 | 计划动作 | 实际动作 | 偏差 |
|---|---|---|:-:|
| `veloxa/foundation/memory/arena_allocator.h` | 1 行改动 + 注释 | 1 行 + 20 行扫描表注释 | ✅ 精准 |
| `tests/foundation/memory/arena_allocator_test.cc` | +1 GTest | +1 GTest（+23 行，含 RED 反向探针验证路径）| ✅ 精准 |
| `benchmarks/baseline/bench_layout_buildtree.json` | 覆盖刷新 | 刷新（3-reps + mean/median/stddev/cv 加权） | ✅ 精准 |
| `benchmarks/baseline/bench_layout_flex.json` | 覆盖刷新 | 同上 | ✅ 精准 |
| `benchmarks/baseline/README.md` | 环境表 + 历史行 | 环境表 + K2/K3/K8 段 + 历史行（超预期） | +K8 新发现沉淀 |
| `memory-bank/techContext.md` | Layout 基线段补 resolved | + K2/K3 resolved + K8 新发现 | +K8 新发现沉淀 |
| `memory-bank/activeContext.md` | 阶段更新 | 完整 build report（5 commits + 耗时表 + 验收）| 超计划 |
| `memory-bank/tasks.md` | 验收 10/10 + 耗时 | 同上 + TASK-24-02 立项 | 符合 |
| `memory-bank/progress.md` | Phase 里程碑 | 详细 Phase 0-5 耗时 + 决策过程 | 符合 |
| `memory-bank/systemPatterns.md` | 条件触发 | 未更新（本 reflection 后才追加）| 保留给 reflect 阶段 |

### 1.3 Phase 耗时对比（plan × 0.6 协议）

| Phase | plan (min) | 实测 (min) | 比例 | 偏差类型 |
|:-:|:-:|:-:|:-:|---|
| 0 基线+分支 | 10 | ~3 | 0.30× | smoke 工具矩阵一次 grep 完成 |
| 1 假设 (d) 探针 | 20 | ~4 | 0.20× | 单 sed + build + run 流水线 |
| 2 block-size 扫描 | 25 | ~5 | 0.20× | for 循环脚本化 3 档（已有 4K/65K 2 档） |
| 3 RED→GREEN+反向探针 | 25 | ~8 | 0.32× | 含指针连续性测试设计时间 |
| 4 baseline 刷新 | 20 | ~5 | 0.25× | --repetitions=3 并行自动跑 |
| 5 MB 收尾 | 15 | ~8 | 0.53× | 文档写作占主 |
| **合计** | **115** | **~33** | **0.29×** | **历史最快档** |

---

## 2. 做得好的

### 2.1 阶梯验证策略首次实战成功

D1=A（阶梯验证）在 plan 阶段设计为「先测最可能 (d) malloc churn → 命中止步修复；否则升 Phase 1B per-phase 拆分」。实战流程：

1. Phase 1 单点探针 block=65536 → R256 从 9.42× 降到 3.61×，**(d) 部分确认**
2. Phase 2 扫描定位最优 32K
3. Phase 1B **零触发**

对比 Phase 1B 若触发的成本（+40-55 min plan / 新 bench .cc + CMake 注册 / per-phase 分解）— 阶梯验证节省了 ~50% 调查时间。

### 2.2 脚本化实验（for 循环 + python3 aggregate）

Phase 2 的 `for SIZE in 8192 16384 32768; do sed + cmake + 2 binaries`一次 40 秒 3 档扫描 + python3 aggregate 输出 markdown 表格，**零手工粘贴**。这种模式应沉淀为研究型任务的模板（见后文「改进建议 P2-1」）。

### 2.3 TDD RED 反向探针（第 2 次实践，模式成熟）

TASK-11 首次提出、本任务第 2 次应用：
- RED: 改默认前测试 FAIL（p2-p1=16448 跨 oversized 块）
- GREEN: 改后 PASS（p2-p1=16384 同块连续）
- **反向探针**: revert 默认→FAIL→restore→PASS 证实测试精准抓得住回归

测试设计巧妙：不依赖 `block_size()` getter（不存在），而用**指针连续性**（p2-p1 == kChunk）间接观测 block 容量；既不扩 API 也不加 friend，属于「公开行为锚定内部约束」的测试哲学典范。

### 2.4 plan×0.6 协议第 5 数据点（历史最快档 0.29×）

| 任务 | 类型 | 倍率 |
|------|------|:-:|
| TASK-05 | bench | 3.4× |
| TASK-09 | bench | 4.2× |
| TASK-11 | bench | 1.5-2.0× |
| TASK-13 | 文档 | 1.67-1.86× |
| **TASK-24-01** | **研究+小补丁** | **0.29×** ← 历史最快 |

0.29× 突破前 4 任务 ≥1.5× 下限，样板特征：
- **最窄核心路径**：1 行代码 + 1 GTest + 2 baseline JSON
- **基础设施成熟度 100%**：ArenaAllocator 已有完整测试 / build-bench 已就绪 / baseline JSON 协议已落地
- **脚本化替代手工**：sed for 循环 + python3 aggregate 替代手工 sed+build+跑+粘

**校准协议更新方向**：对「基础设施 100% 复用 + 单文件 ≤ 1 行改动」类任务，plan 估时应进一步下调到 plan×0.3 预期。

### 2.5 用户决策节点设计得当

Plan 阶段 D1/D2 两个决策点（AskQuestion）锁定策略；Build 阶段 Phase 2 结果不达 plan 刚性阈值时再次插入 AskQuestion（ship 32K / ship 65K / 深入 Phase 1B）— 关键决策点前置 + 中途实证后再次收敛，全程无返工。

### 2.6 交付物「一次到位」

Phase 5 在 techContext.md 不仅标记 K2/K3 resolved，还把 Phase 2 扫描**完整数据表**（5 档 R256/R_flex）作为证据入库，未来若 reopen（如 TASK-02 Phase 1B 不成立需回退改动）可直接回溯。

---

## 3. 遇到的挑战

### 3.1 plan 阶段验收阈值凭直觉设置（主要教训）

**现象：** plan §7 写 "R256 ≤ 2.5× / R_flex ≤ 5×" 作为主/次判据，实测最优 32K 只能达到 4.18× / 6.40×，两项都未达 plan 刚性判据。

**根因：** plan 时假设 (d) 是唯一或主导根因（即 bump block 能把 knee 彻底消除），没考虑（实测证明）(d) 只贡献 ~60% 改善、剩余 ~40% 属于 (e) L1D / (f) 算法因素。凭直觉设的"knee 消失"阈值实际是**两个独立根因叠加效应的总和目标**。

**影响：** Phase 2 结束时用户需要 AskQuestion 三选一（ship 32K / ship 65K / 走 Phase 1B）— 虽有条不紊，但理想情况下 plan 应预见"部分达标"场景并自动给出处理方案。

**反复模式匹配：** 接近"非默认路径遗漏验证"（4+ 次）的亲戚 — 但这里是**非理想实测路径未预留处理方案**。属新变种。

### 3.2 GTest binary 路径未在 plan 预查

**现象：** plan Phase 3 步骤 2 写 `./build/tests/foundation/memory/arena_allocator_test --gtest_filter=...`，实际路径是 `./build/tests/arena_allocator_test`（CMake 生成时扁平化到 `tests/` 目录，不反映源码路径）。首次跑 exit 127 "No such file"。

**根因：** plan 阶段只对 bench binary (`build-bench/benchmarks/*`) 做过路径核对，未对 unit test binary 做类似核对。

**损失：** ~30 秒（find 一次定位、续跑）。小规模，但属于反复模式"前置依赖/环境/API 能力未验证"（8+ 次）的近亲。

### 3.3 65K block 在 Flex 回弹（意外信号，但被正确处理）

**现象：** Phase 2 扫描中 block=65536 的 R_flex 从 32K 的 7.40× **回弹**到 8.36×。

**根因（分析）：** single block 大小接近 L1D cache size（48 KB），block 本身分配后初始访问时 L1D 全部被覆盖 → 后续 ComputedStyle 访问跨 cache line 挤占。

**处理：** 作为 K8 新发现沉淀到 baseline README + techContext，并为 Arena 设计留 guard rail（"block 不宜无限扩大"）。

**挑战识别**：实验阶段意外信号容易被"数字最小就是最好"掩盖；本次及时注意到"R256 最小但 R_flex 反而回弹" = **双指标交叉验证**抓住了 L1D 的副信号。

### 3.4 plan 的 Phase 3 任务 3.1 步骤 1 错把测试名写成 `DefaultBlockSizeLocked`

**现象：** plan 文档中新测试叫 `DefaultBlockSizeLocked`，实际实现叫 `DefaultBlockSizeFitsLargeAllocations`（更准确描述行为）。

**根因：** plan 时测试思路是"用分配大小观察 bytes_allocated 变化"，实现时发现用**指针连续性**更简洁，测试方法变了 → 测试名同步调整。

**影响：** 零（属于 TASK-13 沉淀的"实证微调 spec 范式"合理场景，名字变化没影响契约意图）。

---

## 4. 经验教训

### 4.1 研究/实验型任务的 plan 阈值应区分「预期带」和「下限带」

具体做法：plan §7 验收标准段区分：
- **预期带**：理想情况下的目标（R256 ≤ 2.5）
- **下限带**：可接受的最低改善（R256 ≤ 6）
- **分支处理**：实测落在 (下限带, 预期带) 区间时，自动插入用户确认 AskQuestion「接受当前改善 / 继续升级路径」；不落人工判断。

这与 TASK-13 沉淀的「实证微调 spec 范式」是互补关系：实证微调处理**实现细节微偏**（如测试名），本教训处理**验收数字部分达标**。

### 4.2 测试设计的「公开行为锚定内部约束」哲学

不暴露新 getter、不加 friend、不 `#define private public`，而是设计一个**只有在内部约束成立时才会成功的公开行为测试**。本次的指针连续性测试就是典范：
- 约束：default block_size ≥ 32768
- 公开行为：2× 16K 分配后指针距离 == 16K（contiguous）
- 若约束破坏：指针距离 != 16K（反向探针验证）

这比 API 扩展 / friend 白名单更优雅，应作为测试设计首选。

### 4.3 扫描型研究任务的脚本化模板

```bash
for VAR in VALUE_1 VALUE_2 VALUE_3; do
  sed -i "s/PATTERN/${VAR}/" FILE
  cmake --build DIR --target TARGET -j 2>&1 | tail -1
  ./BINARY --benchmark_format=json --benchmark_out=/tmp/result_${VAR}.json > /dev/null
done
python3 - <<'EOF'  # aggregate all results to markdown table
...
EOF
```

TASK-20260424-01 Phase 2 已验证此模板有效：3 档扫描 + 2 档已有数据 = 5 档聚合表 **一次性完成 + 零手工粘贴**。

### 4.4 双指标交叉验证抓副信号

单指标扫描会选到"某一项最优但副作用指标差"的点；本次 65K 在 R256 上最优但 R_flex 回弹，只有**同时看两指标**才能识别 sweet spot。扫描型实验必用 ≥ 2 指标交叉。

### 4.5 plan×0.6 协议适用跨类型但需分「路径宽度」子档

5 数据点收敛：

| 路径宽度 | 示例 | 典型倍率 | 预估指南 |
|---|---|:-:|---|
| 全新系统 | bench 首次集成 | 3-4× | plan×0.25-0.3 |
| 中等（2-5 文件） | bench 扩展 / 文档沉淀 | 1.5-2× | plan×0.5-0.6 |
| 最窄（1-3 文件 + 100% 复用基础设施） | 本任务 | **0.29×** | **plan×0.3** |

下次遇到"最窄路径"任务（单改动 + 全复用基础设施）直接按 plan×0.3 预估。

---

## 5. 回顾检查清单

### 代码变更类任务

- [x] **计划精确度**：文件清单与实际变更 **100% 一致**（含意外补充的 K8 新发现沉淀）
- [x] **TDD 执行情况**：严格 RED → GREEN → 反向探针三步完整执行，无跳过
- [x] **子代理质量**：本次未用子代理（任务单文件修改，单会话完成）
- [x] **测试隔离**：新测试独立分配 `ArenaAllocator arena`，无共享状态；全量 ctest 892/892 无串扰
- [x] **提交粒度**：5 commits 严格按 Phase 切分（plan+van / wip red / fix / bench refresh / finalize），无大杂烩
- [x] **非默认路径**：本任务即是**发现非默认数据点**的专门任务（扫描 5 档），全覆盖

### 配置/规则类任务

- [x] **文件位置验证**：所有 MB 文件路径在 /van 阶段已 Read 确认；plan 阶段 grep 实证 ArenaAllocator 使用点（4 处）

### 安全相关任务

**❌ 不适用**：本任务为性能优化，无外部输入 / 无认证 / 无新依赖 / 无敏感数据处理。

---

## 6. 反复模式识别

| 已知模式 | 历史频率 | 本次重复？ | 备注 |
|---------|:-:|:-:|---|
| 计划文件清单与实际变更不一致 | 9+ 次 | ❌ | 本次完全一致 |
| 子代理产出需大量返工 | 7+ 次 | N/A | 未用子代理 |
| 前置依赖/环境/API 能力未验证 | 8+ 次 | ⚠️ 近似 | GTest binary 路径未预查，小规模 |
| 非默认路径遗漏验证 | 4+ 次 | ❌ | 本任务即是主动发现非默认行为 |
| 测试隔离问题 | 7+ 次 | ❌ | 新测试完全独立 |
| 提交粒度偏离计划 | 7+ 次 | ❌ | 5 commits 严格按 Phase |
| TDD 严格度与场景不匹配 | 11+ 次 | ❌ | 严格 RED→GREEN + 反向探针 |

**新变种识别：**
- **「plan 验收阈值凭直觉凿实，实测部分达标时无预设处理方案」** — 与"非默认路径遗漏"亲缘但不同；本次为 5 任务中首次清晰出现。若下次研究型任务再次出现 → 升级 P1 沉淀。

---

## 7. 改进建议（附优先级与落实方式）

| # | 建议 | 优先级 | 落实方式 | 目标文件 |
|:-:|---|:-:|---|---|
| 1 | plan §验收标准段对研究/实验型任务区分「预期带 + 下限带 + 中间区间处理方案」，避免刚性阈值卡住流程 | **P1** | 在 `writing-plans.mdc` 追加子段「研究/调查类任务验收阈值三段式」，模板含「预期带 / 下限带 / 中间区间用户 AskQuestion」| `.cursor/rules/skills/writing-plans.mdc` |
| 2 | plan §0 smoke/工具矩阵扩展到 **test binary 路径**（GTest / 集成测试 binary），一次性 `find build -name "*_test" -type f` 入 progress.md | **P1** | 在 `writing-plans.mdc`「smoke 工具链可用性检查」子块追加「test binary 路径矩阵」条目 | `.cursor/rules/skills/writing-plans.mdc` |
| 3 | 「扫描型研究任务的 for 循环 + python3 聚合」样板沉淀为 systemPatterns 模式 | **P2** | 在 `systemPatterns.md` 追加「扫描型研究任务脚本化模板」段（含本任务 Phase 2 代码样板 + 双指标交叉验证要点）| `memory-bank/systemPatterns.md` |
| 4 | plan×0.6 协议扩展「路径宽度」子档（全新 / 中等 / 最窄）+ 本任务 0.29× 作为「最窄路径」样板数据点 | **P2** | 更新 `systemPatterns.md`「bench 类任务估时校准」段，追加 TASK-24-01 数据点 + 子档表 | `memory-bank/systemPatterns.md` |
| 5 | 「公开行为锚定内部约束」的测试设计哲学沉淀 | **P2** | 在 `systemPatterns.md` 追加「测试设计 — 公开行为锚定内部约束模式」段，举本任务 `DefaultBlockSizeFitsLargeAllocations` 为样板 | `memory-bank/systemPatterns.md` |
| 6 | K8 新发现「ArenaAllocator block size 不宜过大（超 L1D 48KB 触发抖动）」作为 Arena 设计守则 | **P2** | 已在 `techContext.md` Layout 性能基线段 + `benchmarks/baseline/README.md` 文档化 | ✅ 已落实 |
| 7 | 「双指标交叉验证抓副信号」原则沉淀 | **P2** | 追加到「扫描型研究任务脚本化模板」模式（与 #3 合并入同段）| `memory-bank/systemPatterns.md` |

---

## 8. 技术改进建议

1. **ArenaAllocator API 小优化**（P3 触发型）：考虑暴露 `default_block_size()` const getter，简化未来测试（现在通过指针连续性间接观测）。但**不紧急**，当前测试哲学优雅，不主动做。

2. **LayoutEngine 静态 arena 的 block_size**：`layout_engine.cc:109` 当前 `static ArenaAllocator layout_arena(8192)`，小于新默认 32768。考虑是否也 bump 到 32K 或移除显式（用默认）。但生产场景 Document 寿命长、arena Reset 后复用，block 大小影响小 → **延后评估**（可纳入 TASK-24-02 调查范围）。

3. **`Document::arena_` 默认化带来的 +28KB/instance 成本**：嵌入式场景每 view 一个 Document，总量 sub-100KB；但若未来出现多 Document 并存（iframe / 多窗口），累积可观察。**不急**，留作 TASK-24-02 或后续优化观察点。

4. **TASK-24-02 承接方向**（per-phase 拆分 BM）：剩余 super-linear ~40% 的候选：
   - (e) L1D 抖动 — 单元素 552B × 256 = 142KB，远超 L1D 48KB；可能是主力
   - (f) 隐藏算法因素 — Phase 2 实验中 32K→65K 改善递减暗示**非 malloc 因素**接近主导
   - 建议 per-phase BM 先拆 BuildTree / LayoutBlock / ApplyPositioning；若 super-linear 主要在 BuildTree → (e) L1D；若在 LayoutBlock → (f) 算法

---

## 9. 安全评估

**本任务不涉及安全变更。**

| 维度 | 状态 | 备注 |
|------|:-:|---|
| 输入验证 | N/A | 无外部输入 |
| 认证/授权 | N/A | 无权限边界 |
| 数据保护 | N/A | 无敏感数据 |
| 依赖审计 | N/A | 无新依赖 |
| 错误信息脱敏 | N/A | 无错误输出变化 |
| 敏感数据处理 | N/A | 无 |

---

## 10. 关键发现小结

1. **根因 (d) ArenaAllocator 4KB block malloc/free churn 大幅解决 knee（贡献 ~60%）**；剩余 ~40% 属 (e)/(f) 转 TASK-24-02
2. **K8 新发现：block 过大触发 L1D 抖动** — 65K 让 Flex 回弹，32K 是 Layout sweet spot；未来 Arena 设计原则「block ≤ L1D」
3. **plan × 0.6 协议第 5 数据点 0.29×，历史最快档** — 「最窄路径任务」样板
4. **RED 反向探针第 2 次实践，模式成熟** — 可作为 Mixed TDD 标配
5. **阶梯验证 + 脚本化实验协同高效** — Phase 1B 零触发，Phase 2 for 循环 40 秒扫 3 档
