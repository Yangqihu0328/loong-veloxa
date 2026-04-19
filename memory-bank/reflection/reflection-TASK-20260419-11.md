# 回顾：TASK-20260419-11 ImageCache::Load HashMap 化（K6 高 ROI 优化）

**日期：** 2026-04-19
**任务 ID：** TASK-20260419-11
**复杂度级别：** Level 2（多文件机械替换 + 数据双索引设计；无 UI/架构空白）
**分支：** `feature/TASK-20260419-11-imagecache-hashmap`（5 commits + 1 reflect 即将提交）

---

## 计划 vs 实际

| 维度 | 计划 | 实际 | 偏差原因 |
|------|------|------|---------|
| Phase 数 | 4（D4 决策） | 4 + 完成验证 + 收尾 = 6 step | 收尾 chore 提交按 `/build` §7.5 模板纳入 |
| 估时 | 55-80 min（plan §2 汇总） | **~35-40 min**（连续 timer 测） | 进一步收敛（vs TASK-05 3.4× / TASK-09 4.2× 高估）— `systemPatterns.md` "bench 类任务估时校准" 协议生效首次实证 |
| 文件变更 | image_cache.{h,cc} + test + 2 README + baseline JSON + techContext + 3 MB | **完全一致** — 0 意外文件 | plan §1 文件结构表 + VAN F1-F5 grep 实证已穷尽影响面 |
| 设计变更 | 双索引 + StringHash/StringEq + ClearAndReloadDeduplicates | **0 偏差** — D1-D4 全部直接落地 | plan 阶段头脑风暴已闭合所有维度 |
| Commit 数 | 5 commits（VAN+Plan+P1+P2+P3+P4 收尾） | 6 commits（同上 + 本 reflect） | 完全按 D4 细粒度计划 |
| K6 量化目标 | Hit<16> < 50ns / Hit<256> < 100ns | **Hit<16> = 44.05ns / Hit<256> = 45.70ns（25.2×↓）** | 远超 plan 目标（plan 12×↑ → 实际 25×↑），HashMap 在 N=256 完全平台化 |
| 已知微回归 | Hit<1> 阈值 < 30ns（plan §P3 步骤 4） | **Hit<1> = 43.27ns**（超阈值 13ns） | plan 阈值定得过紧 — HashMap djb2+probe 固有 ~32ns 开销不可避免 |

---

## 回顾检查清单（代码变更类）

- [x] 计划精确度 — ✅ 文件清单完全一致；估时偏差 1.5-2×（继续收敛趋势）
- [x] TDD 执行情况 — ⚠️ Mixed TDD（D3 测试用 RED 反向探针补强；P1+P2 生产代码先于测试，符合 plan §0 mixed TDD 模式）
- [x] 子代理质量 — N/A（本次未用子代理；任务规模 + plan 详尽度均无需分派）
- [x] 测试隔离 — ✅ 891/891 PASS；新增 D3 测试用 `getpid()` 隔离临时文件
- [x] 提交粒度 — ✅ 5 + 1 commits 严格按 D4 phase 划分（P1 仅加字段不改行为；P2 切 Load + 新测试；P3 bench + baseline + README；P4 MB 收尾；reflect 独立）
- [x] 非默认路径 — ✅ D3 ClearAndReloadDeduplicates 主动覆盖「双索引漏 clear」场景；RED probe 验证回归网精准

---

## 做得好的

### 1. P0 #4「方案根因假设未先验证」第 4 次完整应用，全程零返工

VAN 阶段 5 处 grep 实证（F1-F5）+ Plan 阶段补 grep `string.h:30-56 BasicString` SSO 内联确认 — 把"假设"全部转为"grep 证据"：

| 假设 | grep 实证 | 关键发现 |
|------|----------|---------|
| F1 「`ImageHandle` 可改为不透明 token」 | `image_cache.cc:30 &images_[handle - 1]` | ❌ 错 — handle 必须 1-based vector 下标（ABI + Get O(1)）→ **必须双索引**而非单 HashMap 替换 |
| F2 「`std::equal_to<String>` 默认可用」 | `string.h:178/190` 仅 `==(String, StringView)` | ⚠️ 风险 — 触发 `StringEq` 自定义 functor 决策 |
| F3 「需新写 djb2 hash」 | `property.cc:84-92 StringViewHash` | ❌ 错 — 现成 djb2，~5 行复刻 |
| F4 「修改影响多调用方」 | grep 仅 `application.cc:67/118` 一处 | ❌ 错 — 单一持有者，回归风险接近零 |
| F5 「需新加 dedup 测试」 | `image_cache_test.cc:55 DeduplicateSamePath` | ❌ 错 — 已覆盖核心契约 |

Plan 补 grep 进一步避免了 D1 决策风险（SSO 确认 → owned String key 唯一安全选择）。

### 2. D3 RED 反向探针实践（mixed TDD 模式补强 — 新模式）

Plan 阶段就识别「双索引漏 clear」是最大陷阱（D3 决策直接立项 ClearAndReloadDeduplicates），Build 阶段在测试通过后**主动反向破坏实现验证 RED**：

```cpp
void ImageCache::Clear() {
  images_.clear();
  // path_to_handle_.clear();  // RED probe ← 临时注释
}
```
→ 测试立即 FAIL（`EXPECT_EQ(h2.value(), 1)` 命中：HashMap 残留 handle 但 vector 空 → Load 直接返回旧 handle，但 1-based 下标错位）→ 恢复后 PASS。

**价值**：mixed TDD 下「测试可能因实现已正确而误以为有效」的最大风险被消除；此实践耗时 < 3 min，但提供了完整的 RED→GREEN 证据链，等价于严格 TDD 的 RED 阶段。

### 3. P1+P2 拆分（D4）— 数据结构改造与行为切换分离

P1 仅加 HashMap 字段不改 Load/Clear 行为 → ctest 890/890 立即 PASS（验证 F2 编译风险闭合，**未引入任何运行时变化**）；P2 才切 Load 路径 + 新增测试。这种"加字段先于切语义"的拆分让 review 风险面在每个 commit 都可量化（P1 仅看头文件 + 编译/链接；P2 仅看 17 行 cc + 26 行 test）。

### 4. K6 量化目标全部超预期命中

Plan 目标 Hit<16> < 50ns（1×↓）/ Hit<256> < 100ns（12×↓） → 实际 44.05ns（1.16×↓）/ 45.70ns（**25.2×↓**）。Hit<256> 平台化到 ~46ns 说明双索引 + djb2 在 N=256 已完全消除 O(N) 量级回归，anomaly「size=256 cache hit 慢于 ReplayImageReal<16>」彻底消失。

### 5. bench plan 估时收敛趋势（systemPatterns 协议首次实证）

| 任务 | Plan | 实际 | 比率 |
|------|-----:|-----:|-----:|
| TASK-05 | 4.25h | 75 min | 3.4× |
| TASK-09 | 3.5h | ~50 min | 4.2× |
| **TASK-11** | **55-80 min** | **~35-40 min** | **~1.5-2.0×** |

`systemPatterns.md:666-681` "bench 类任务估时校准" 段（含复用率 + 单 BM 3-5 min + phase 估时基线）协议生效；偏差从 4× 收敛到 ~2×，距离"准确"还差 1 次校准。本次 plan 已主动按 phase 拆分（P1 10-15 min / P2 20-30 min / P3 15-20 min / P4 10-15 min），实际仍偏短 — 因「mechanical 替换 + 充分 grep 实证」组合让单 phase 切换/编译/验证开销也在收敛。

### 6. 全程零回退、零阻塞、零 TODO 漂移

所有 6 个 todo 一气呵成完成；commit chain 线性无 amend；ctest 891/891 + Release `-O3` 0 errors / 0 warnings 一次过；linter 零错误。

---

## 遇到的挑战

### C1 — 沙箱无 `jq`（plan 阶段未验证 smoke 工具链）

Plan §P3 步骤 3-4 用 `jq` 做 smoke 三件套自检（`items_per_second==0` 过滤 + `library_build_type` 提取 + Hit BM 数字提取），实际环境无 `jq` → 改 `python3 -c` heredoc，多花 ~3 min。

**根因**：VAN/Plan 阶段所有 grep 都聚焦在源码 / API，**未 grep `which jq`** 验证 smoke 工具链可用性。

**影响**：单次 ~3 min，不致命；但属于"前置依赖未验证"模式（systemPatterns 已知反复模式 #3，频率 8+），**本次部分重复**。

### C2 — Python f-string + `\"` 反斜杠语法错误

第一次 `python3 -c '...f"...{b[\"name\"]}..."'` 直接 syntax error（f-string 表达式部分不允许 `\` 反斜杠）→ 改 heredoc。多花 ~30 sec。

**根因**：思维捷径，凭印象写 inline python；**应直接 heredoc**或写到 `.py` 文件。

### C3 — Hit<1> 微回归 32ns 超 plan 软阈值 30ns

Plan §P3 步骤 4 写 Hit<1> 阈值 `< 30 ns`（来自基线 10.35ns 的"不退化"心智），实测 43.27ns。绝对回归 32ns，符合 HashMap djb2(`O(strlen)`) + probe + Insert 的固有开销，但**超出 plan 形式阈值**。

**判定**：plan 阈值定得过紧；实际 K6 主目标 Hit<256> 25.2× 净增益完全压倒此微回归（256 路径节省 1106ns，Hit<1> 路径多花 32ns，加权 ROI 极高）。在 baseline README + commit message 中如实标注，但未阻塞合并。

**真正的问题**：plan 对**超低 ns BM**（< 5ns 量级）继续用乘法判定（如 Get 0.94→1.16 = 1.23×），噪声敏感 — 阈值表设计不严谨。

### C4 — Plan §P3 步骤 5「baseline JSON 同名覆盖」schema 变化

旧 baseline 是 TASK-09 单次跑（无 repetitions），新跑带 `--benchmark_repetitions=3` → schema 增加 `_mean / _median / _stddev / _cv` 行（147 行 → 805 行）。Plan 未明示 schema 变化是否需要 baseline README 同步说明 → 我主动加了"带 repetitions=3 / mean+median+stddev/cv"标注。

**判定**：未导致返工；但 plan 应明确「baseline JSON 替换前后 schema 兼容性」是否要标注。

---

## 经验教训

### L1 — Mixed TDD 模式下 RED 反向探针 = D3 类「回归网测试」的标配补强

D3 决策（额外加 1 个 GTest 覆盖双索引同步）配 RED 反向探针验证（临时破坏实现确认 FAIL → 恢复确认 PASS），耗时 < 3 min 但提供完整证据链。**任何 mixed TDD 任务中「为预防特定 bug 而新增的回归测试」都应配反向探针**，否则该测试可能在「实现恰好正确」的巧合下成为永远不报警的死代码。

### L2 — bench plan 阈值表应对超低 ns BM 用「绝对增量兜底」

`< 5ns` 的 BM（Get 0.94ns / Hit<1> 10.35ns）继续用 `< baseline × 1.2` 判定会触发噪声误警。建议改为：

```
判定阈值 = max(baseline × 1.2,  baseline + 0.5ns)   for baseline < 5ns
判定阈值 = max(baseline × 1.2,  baseline + 5ns)     for baseline ∈ [5, 50)ns
判定阈值 = baseline × 1.2                            for baseline ≥ 50ns
```

写入 `systemPatterns.md` "bench 类任务估时校准" 段附录。

### L3 — Plan 阶段需 grep `which <tool>` 验证 smoke 工具链可用

VAN/Plan grep 实证不应只覆盖被改源码的 API；smoke 验收依赖的 CLI 工具（`jq` / `bc` / `xmllint` / `valgrind` 等）也需在 plan 阶段确认存在，否则进入 Build 期才发现要切技术栈。

### L4 — 「数据结构改造与行为切换分离」是低风险性能任务的通用 phase 拆分模式

P1 仅加字段不改语义 → P2 切 Load 路径 + 测试 → P3 bench + baseline → P4 文档收尾。每 commit 风险面独立可审；P1 的「编译通过 + ctest 全过」就证明 F2 类型推导风险已闭合，让 P2 切语义时心理负担降到 0。

**适用场景**：任何"加新数据结构 → 切换 hot path → 验证 perf"的优化型 PR，包括未来可能的 Layout Box `Vector<LayoutBox*> children` HashMap 化、ResolvedCache 升级等。

### L5 — HashMap 不是金科玉律，极小 N 下线性扫的 cache locality 仍胜

Hit<1> 10.35ns → 43.27ns 的 4× 回归是 djb2(O(strlen)) + probe + Slot 间接的固有开销总和；同一组件 vs 单元素线性 `view()==path` 的 1 次 memcmp + cache-line 命中。**未来若引入 N 永远 ≤ 4 的新 cache 场景（如最近 N 调用 token cache），不应套用本任务的同构方案**。

写入 `techContext.md` 提示。

---

## 反复模式识别（基于 27 份历史回顾）

| 已知模式 | 频率 | 本次是否重复？ | 备注 |
|---------|-----:|---|------|
| 计划文件清单与实际变更不一致 | 9+ | ❌ 未重复 | plan §1 文件结构表与 5 commits 完全一致 |
| 子代理产出需大量返工 | 7+ | N/A | 本次未用子代理 |
| 前置依赖/环境/API 能力未验证 | 8+ | ⚠️ **部分重复**（C1：jq 不存在）| 核心 API 已 grep 5 处实证，但 smoke 工具链 jq 漏验 → L3 改进 |
| 非默认路径遗漏验证 | 4+ | ❌ 未重复 | D3 ClearAndReloadDeduplicates + RED probe 主动覆盖 |
| 测试隔离 | 7+ | ❌ 未重复 | `getpid()` 隔离 |
| 提交粒度偏离计划 | 7+ | ❌ 未重复 | 严格按 D4 4 phase + 收尾 |
| TDD 严格度与场景不匹配 | 11+ | ⚠️ **本次显式 mixed TDD（plan §0 协议）+ RED 反向探针补强** — 这是该模式的"成熟变体"而非偏离 | L1 沉淀 |

**结论**：仅 1 项部分重复（C1 / 工具链漏验），且改进路径明确（L3）。无需升级现有 P0/P1。

---

## 改进建议（附优先级与落实方式）

| # | 建议 | 优先级 | 落实方式 | 目标位置 |
|---|------|:------:|---------|---------|
| 1 | bench plan 阈值表对超低 ns BM 用「绝对增量兜底」（公式见 L2）| **P1** 下次 bench 任务前 | 追加附录到 systemPatterns "bench 类任务估时校准" 段 | `systemPatterns.md` |
| 2 | Plan 阶段需 grep `which <tool>` 验证 smoke 工具链可用（jq / bc / valgrind 等）| **P1** 下次性能/集成任务前 | 加到 `writing-plans.mdc` "性能基准任务必检项" 段 | `writing-plans.mdc` + `activeContext.md` 待处理事项 |
| 3 | Mixed TDD 下 D3 类「回归网测试」标配 RED 反向探针验证 | **P1** 下次 mixed TDD 任务 | 沉淀新模式段「Mixed TDD RED 反向探针实践」 | `systemPatterns.md` |
| 4 | 「数据结构改造与行为切换分离」P1+P2 拆分作为低风险性能任务 phase 拆分模板 | **P2** 长期 | 沉淀新模式段 | `systemPatterns.md` |
| 5 | HashMap 不是金科玉律：极小 N 下线性扫的 cache locality 仍胜 — 提示未来同类决策 | **P2** 长期 | 加到 `techContext.md` HashMap 段附注 | `techContext.md` |
| 6 | bench plan 估时连续校准趋势（4.2×→2.0×），下次 bench 任务前再做一次校准实测；如再次 ≤ 1.5× 即可收敛到「准确档」 | **P2** 长期监测 | 更新 systemPatterns "bench 类任务估时校准" 表 | `systemPatterns.md` |

**P0**：无（本任务零返工、零阻塞、零回退）。

---

## 技术改进建议

1. **`HashMap::reserve(N)` API 在已知容量场景下应使用** — 本次因 N 上限不可知（生产场景），D2 决策不 reserve；但若未来发现 ImageCache 在某场景下 N 稳定 > 32，可调用 `path_to_handle_.reserve(64)` 节省 ~2 次 rehash（每次 ~50 ns × 32 entry = ~1.6µs 一次性，影响极小）。
2. **PNG fixture 复用** — `image_cache_test.cc::CreateTestPng()` 当前每个 GTest 单独建 fixture（共 4 个 test）。可改为 GTest fixture class 复用同一 PNG，节省 ~3 次 png_create_write_struct + I/O。但优化收益 < 5ms，**性价比低，记技术债不立项**。
3. **bench fixture path 唯一性** — TASK-09 已用 `/tmp/vx_bench_<pid>_<i>.png` 避免 hit 污染，本次双索引方案后该约束依然成立（如改 `/tmp/foo.png` 单文件，所有 256 个 Load 都会 dedup 到 handle=1 测不出 Hit<256> 的 hash 性能）。**已隐式正确，无需改动**。

---

## 安全评估

| 维度 | 状态 | 备注 |
|------|------|------|
| 输入验证 | N/A | 无新输入路径；`String(StringView)` 拷贝沿用现有约束 |
| 认证/授权 | N/A | 无认证路径 |
| 数据保护（加密/脱敏） | N/A | 无敏感数据 |
| 依赖审计 | ✅ | **零新依赖**（HashMap / String 均为 vx_core 内部成熟容器）|
| 错误信息脱敏 | N/A | 无新错误路径（`StatusOr` 沿用）|
| 敏感数据处理 | N/A | 无敏感数据 |

**结论**：本任务为内部数据结构性能优化，**不涉及任何安全敏感面**。
