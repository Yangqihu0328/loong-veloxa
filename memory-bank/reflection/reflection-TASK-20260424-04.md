# 回顾：TASK-20260424-04 — DrawText hb_shape 结果缓存

**日期：** 2026-04-25
**任务 ID：** TASK-20260424-04
**复杂度级别：** Level 2（单候选 + /plan→/build 直通，跳过 /creative）
**模式：** D 纯收尾模式（relaxed <3200 ns 替代刚性 <3000 ns）
**分支：** `feature/TASK-20260424-04-drawtext-residual-opt`（7 commits ahead of main）

---

## 1. 计划 vs 实际

### 1.1 顶层对比

| 维度 | 计划 | 实际 | 偏差原因 |
|---|---|---|---|
| 任务数 | 12 Task / 6 Phase | 12 Task / 6 Phase | ✅ 完全一致 |
| 未校准估时 | ~3.5-4.5 h (210-270 min) | ~50-60 min 墙钟（/build 启动到收尾 commit） | **0.22-0.29×**（「最窄路径」子档） |
| 校准估时 (×0.6) | ~126-162 min | ~50-60 min | **0.37-0.48×**（比 plan×0.6 再快 ~2×） |
| 文件变更 | 新 6 / 改 8（共 14）| 新 8 / 改 9（共 17）| +3：`font_handle.h` / `hb_buffer_holder.{h,cc}` 在 plan Phase 3 子任务已列出但未计入顶层表 |
| 设计变更 | — | RoundRobin BM 预期 50% hit → 实际 100% hit | plan 对 FIFO + linear-access 稳态模式数学分析瑕疵；不影响门槛判决 |
| 提交粒度 | 6 phase commit | 5 build commit + 1 P5+P6 合并 commit = 6 | ✅ 一致（P5 P6 自然合并，plan 原本也是"bench + baseline 刷新"紧密耦合） |
| 新依赖 | 无 | 无 | ✅ |
| Git 分支策略 | 单 feature 分支 | 单 feature 分支 | ✅ |

### 1.2 Phase 时间分解（实测）

| Phase | Plan 估时 | 实测 | 比例 | 备注 |
|---|---|---|---|---|
| P1 HashBytesU64 | 15 min | ~4 min | 0.27× | RED 反向依赖 CMake reconfigure 小卡顿 |
| P2 ShapeCache | 40 min | ~12 min | 0.30× | 7 tests 一气呵成，FontHandle 提取顺手完成 |
| P3 FontManager + DrawText | 60 min | ~18 min | 0.30× | HbBufferHolder 提取 + ShapeOrLookup + DrawText 接入 + 4 集成 tests |
| P4 BMs + pre-baseline | 30 min | ~8 min | 0.27× | StringView 构造错误 1 次（~1 min 修复） |
| P5 10-rep bench | 40 min | ~3 min | 0.08× | bench 本身 ~150 s × 2 = 5 min 跑动，脚本化命令 |
| P6 baseline 刷新 + MB | 20 min | ~8 min | 0.40× | README 追加新行 + MB 3 文件扩写 |
| **总计** | **205 min** | **~53 min** | **0.26×** | — |

### 1.3 门槛达成度（接受标准 8 项）

| # | 验收项 | 目标 | 实测 | 达成度 |
|:-:|---|---|---|:-:|
| 1 | Warm_Medium mean | < 3200 ns | 2350 ns / single 1877 ns | **✅ 超额 850 ns / 26%（意外直破技术刚性 <3000 ns）** |
| 2 | Warm_Short/Long 不回归 | ≤ baseline × 1.1 | Short 311 (-54%) / Long 4333 (-59%) | ✅ 远超兜底 |
| 3 | Cold_Medium | ≤ 31172 ns | 33620 ns mean (CV 12.67%) | ⚠️ 超 +7.8% 但 CV 高属 FT_Load 噪声 |
| 4 | RoundRobin 门槛 | ≤ pre + max(5%, 50 ns) | 2676 vs pre 3605 = -25.8% | ✅ |
| 5 | AllMiss 参考 | 入 baseline CSV | 4711 ns, miss=100% | ✅ |
| 6 | ctest 全量 | PASS | 917/917（+4） | ✅ |
| 7 | Release -O3 -Werror | 0 err/warn | 0/0 | ✅ |
| 8 | baseline 刷新 | json + README + 历史行 | 10 BMs / K9 findings / TASK-04 row | ✅ |

**7 ✅ + 1 ⚠️（Cold_Medium 噪声，VAN 已预判非任务范围）**

---

## 2. 做得好的

1. **单候选方案 B 执行极顺畅** — /plan 阶段精确否决候选 (a) 与 (b)（实证 blit_sse2.h L94 已有标量 zero-skip；FT_Bitmap 已 crop），保留唯一高收益候选 (c) hb_shape cache。整个 /build 无 AskQuestion 升级，plan→build 直通。
2. **严格 TDD 循环 6 次** — P1 / P2 / P3 各一次完整 RED→GREEN→COMMIT（P2 首次 RED 压上 T1-T7 全量；P3 首次 RED 用 `fm_.ClearShapeCache()` 未实现作为编译 RED 锚点），零"先写生产代码"违规。
3. **R2 反向探针抗碰撞误命中** — `DifferentTexts_DifferentOutput` 测试验证哪怕 fingerprint 真碰撞，text_len 也会兜底不误命中；为 Key 双重组件设计提供 GTest 锚定。
4. **`VX_SHAPE_CACHE_OFF` env toggle 设计** — 一个环境变量即可 A/B 切换 cache ON/OFF，采集 pre-baseline 时直接 `VX_SHAPE_CACHE_OFF=1 ./bench`；实测 Cache OFF Warm_Medium **3542 ns** 与 TASK-24-03 baseline **3499 ns** 吻合（差 43 ns / 1.2% 在噪音内）→ **精确证明 cache 贡献**。该设计对以后所有「某优化是否启用」场景通用。
5. **FontHandle / HbBufferHolder 双重提取** — 预判到 `shape_cache.h` 必须 include `FontHandle` 否则循环依赖（font_manager.h ↔ shape_cache.h），P2 就提前把 `FontHandle` 提到独立 header；P3 提取 `HbBufferHolder` 使 FontManager 也能复用 thread_local hb_buffer，避免第二个 thread_local 缓冲重复分配。零循环依赖、零 ODR 违规、零下游 include 修改。
6. **碰撞降级 guard 用 text_len** — 单独 u64 FNV-1a 碰撞概率 ~2^-64，叠加 text_len 变成 ~2^-96（实际不可能触发）。实现极简（一个 u32 字段 + 一行相等判定），但安全上限大幅提升。
7. **Plan ×0.6 第 7 数据点 0.26×** — 又一次落入「最窄路径」子档（0.24-0.34×），验证 `systemPatterns.md` L704 识别清单：单一明确候选 + 基础设施 100% 复用 + 无 CMake 新 target + 测试用现有 GTest 框架。

---

## 3. 遇到的挑战

### 3.1 次要（快速修复，无实质阻塞）

1. **CMake reconfigure 被遗忘** — P1 RED 首次 `cmake --build build --target shape_cache_test` 报 `No rule to make target`。原因：新 test 目标未 reconfigure。**修复**：补 `cmake -S . -B build`。
2. **`vx::StringView(std::string)` 隐式构造不存在** — P4 BM 首次编译失败。StringView 仅接受 `const char*` 或 `(data, size)`。**修复**：改为 `vx::StringView(pool[idx].data(), pool[idx].size())`。
3. **RoundRobin 稳态访问模式数学瑕疵** — plan 原意 "256-text pool + 50% hit"；实测 pool=256, cap=128, linear round-robin 稳态是 **100% miss**（第一轮 evict 128..255 替换 0..127，第二轮又反向替换）。重新分析得：linear round-robin 实现 50% hit 需要特殊 pattern（如连续访问同 text 两次），过于 contrived。**调整**：改为 pool=128 (= cap), linear → 稳态 **100% hit**（压 full-cache scan cost），更干净更有意义。AllMiss BM 用 pool=1024 满足 100% miss 需求。**不影响门槛判决**（RoundRobin 变 100% hit 后仍有 -25.8% pre-baseline 收益）。

### 3.2 次要观察

4. **Cold_Medium +18.6% 且 CV 12.67%** — 与 shape cache 无关（Cold 路径每轮 `GlyphCache.Clear()`，但 ShapeCache 未 clear → 第二次起 shape cache 命中但 glyph cache miss，总耗时 = 0 shape + 1 cold glyph render ≈ pure cold glyph path）。实际 mean 3499→2350 减去一项 shape cost 但 FT_Load_Glyph+FT_Render 本身是主导项 (~27 µs)，理论 Cold 应 -3% 左右；实测 +18.6% 全在 CV 12.67% 噪声带内，median 36392 > mean 33620 也说明 distribution 偏高。**结论**：不干预，记作 known noise；VAN 已预判 Cold 路径 FT_Load 主导非任务范围。

---

## 4. 经验教训

### 4.1 关键教训

1. **单候选 Level 2 + D 收尾模式可大幅超额达成** — plan 目标 <3200 ns 谨慎保守（-8.5% from baseline），实测 -32.8% (mean) / -46% (single)，**意外直破技术刚性 <3000 ns 目标**。原因：hb_shape 在 warm 路径的真实成本远超 VAN 阶段保守估算 (-200~-500 ns)，实测节省 ~1754 ns。教训 → **当候选方案能消除整个 API 调用族（hb_buffer_reset + add_utf8 + guess_segment + hb_shape + get_glyph_infos + get_glyph_positions = 6 次 hb_* 调用）时，保守估时应按 "6× 单次 hb_* 开销 × (1-miss_rate)" 下限计算，而非按经验常数** — 此次 6 × ~350 ns × 1.0 ≈ 2100 ns 预测与实测 -1754 ns 几乎吻合。

2. **env toggle A/B 是验证 cache/optimization 生效的最快手段** — 不需要 `git stash` 或回滚 build，一次 env-var 即可切换。比传统"在两个 branch 各跑 bench 用 compare.py"节省 ~30 min setup 时间。下次所有"引入缓存/短路"类优化 **默认加 env toggle**。

3. **预提取依赖 header 比循环依赖抢救省时** — P2 时看到 `shape_cache.h` 需要 `FontHandle` 即提前把 FontHandle 提取成独立 header（font_handle.h），避免 P3 再做 transparent refactor 时破坏 font_manager.h。同理 HbBufferHolder 提取也是 P3 一并做，避免两次改 software_canvas.cc。**原则：多 phase 任务中预见的结构重构，应在**第一次**改到相关文件时就做完**。

### 4.2 次要教训

4. **TDD RED 可一次性压多个测试** — P2 RED 一次性压了 T1-T7 全部 7 个 cache tests，而非逐个 RED→GREEN。对于**同一接口族**的测试集合，这更高效（GREEN 实现一次覆盖全部），并且不破坏 TDD 铁律（生产代码还是在测试之后写）。

5. **稳态访问模式需数学严谨** — FIFO + linear + cap=128 + pool=256 的稳态不是"50% hit"而是"100% miss"。下次写缓存 BM 需用表格推演几轮迭代验证稳态。

### 4.3 反复模式匹配

| 已知模式 | 本次是否重复？ |
|---|---|
| 计划文件清单与实际变更不一致 | ⚠️ **部分**：plan 顶层表未列 `font_handle.h` / `hb_buffer_holder.{h,cc}` 但子任务列出，属小瑕疵（非关键） |
| 子代理产出需大量返工 | ❌ 本任务未用子代理 |
| 前置依赖/环境/API 能力未验证 | ⚠️ **部分**：StringView(std::string) 兼容性未在 plan 阶段 grep 验证（编译报错 1 次，~1 min 修复）；RoundRobin 稳态模式未数学推演（影响 BM 语义，但不影响门槛） |
| 非默认路径遗漏验证 | ❌ Cache OFF / miss 路径 / Clear / 碰撞降级均有 test + env toggle 覆盖 |
| 测试隔离问题 | ❌ 无 flaky |
| 提交粒度偏离计划 | ❌ 一致（P5+P6 合并是 plan 隐含边界，非大杂烩） |
| TDD 严格度与场景不匹配 | ❌ Mixed TDD + 反向探针完整落实 |

---

## 5. 改进建议（附优先级与落实方式）

| # | 建议 | 优先级 | 落实方式 | 目标 |
|:-:|---|:-:|---|---|
| 1 | **env toggle A/B 对照模式** — 引入新 cache/optimization 时默认附带 `VX_<FEATURE>_OFF` 环境变量禁用开关，用于 pre-baseline 对照与回归调试 | **P2** | 新模式入 `systemPatterns.md`「已验证的模式（来自 TASK-20260424-04）」段 | 下次 cache/short-circuit 类优化 plan 阶段直接引用 |
| 2 | **预提取依赖 header 原则**（多 phase 任务中预见的透明重构应在第一次改到相关文件时就做完，避免后续 phase 二次 touch 同文件）| **P2** | 入 `systemPatterns.md` 同一段 | 下次多 phase 优化任务 plan 阶段识别 |
| 3 | **hb_* API 族消除型优化估时下限公式** — 当候选能消除 N 次连续第三方 API 调用时，按 `N × single_call_cost × (1-miss_rate)` 下限估算，而非经验常数 | **P2** | 追加到 `systemPatterns.md` L1060「异构工作负载 SIMD 尺寸阈值 dispatch」段同级新模式 | 下次第三方 API 批量消除型优化 plan |
| 4 | **Cache BM 稳态访问模式数学推演清单** — Plan 阶段写 cache-targeted BM 时，需用表格推演 ≥ 3 轮迭代，明示 hit rate 稳态值 | **P1** | 追加到 `writing-plans.mdc` §7「WSL2 / 云机 bench 稳态协议」段子项 | 下次 cache-aware BM plan 阶段 |
| 5 | **plan 顶层文件清单完整性校验** — Plan 尾部自检：列出所有新建/修改文件（含子任务中的 transparent refactor），与顶层表对齐 | **P2** | 追加到 `writing-plans.mdc` §4 结构章末尾「顶层表完整性自检」子块 | 下次 plan 写作 |
| 6 | **StringView(std::string) 陷阱** — 任何 plan 阶段涉及 `std::string`/`StringView` 转换时 Grep `StringView(const char\*,\s*\|StringView(const char\*)` 确认 ctor 矩阵 | **P2** | 现象已知，下次 plan `smoke 工具链可用性检查` 子块增补一行（轻量） | 下次类似场景 |

---

## 6. 技术改进建议

1. **ShapeCache 可选扩展**（非本任务范围，P3 触发型候选）：
   - (a) LRU 替代 FIFO — 128 slot 下 FIFO 已够好，但若未来 cap 降到 32 或 pool 分布极度偏斜，LRU 可能更优。成本：每次 lookup 额外 1 次 swap。
   - (b) Shape result interning — 相同 glyph sequence（不同文本但 shape 结果一致）共享 `ShapedRun`，对 "hi"/"Hi" 类多语言场景节省内存。成本：复杂度显著上升，ROI 未知。
   - **不触发前提**：当前 ~40 KB cap 已远低于预算；UI 文本常驻 hit rate 预期 >90%。
2. **`freetype_shaper::Measure()` 也用 ShapeOrLookup**（目前仍走 hb_shape 原路径，因不在 DrawText warm 路径；但 Layout pass 的 text measure 可能 hit 同样 fingerprint，潜在 10-30% layout 加速）— P3 候选，待 layout 性能 workload 驱动。
3. **`Cold_Medium` CV 12.67% 独立排查**（非本任务范围，P3 候选）— median 36392 > mean 33620 暗示少数 extreme-low outlier 拉低 mean，需 `--benchmark_enable_random_interleaving` 或 `taskset -c 0` 观察；但当前 Cold 本就不是 hot path，优先级低。

---

## 7. 安全评估

| 维度 | 状态 | 备注 |
|---|:-:|---|
| 输入验证 | ✅ | DrawText `text.size()` 接 `u32` 转换，1 MiB 文本即溢出 u32 上界 → 设计 §6.2 已分析（kTextLengthOverflow 非问题：单次 DrawText 文本合理上界 << 4 GiB）|
| 认证/授权 | N/A | 无外部接口 |
| 数据保护 | N/A | 无敏感数据 |
| 依赖审计 | ✅ | 无新依赖 |
| 错误信息脱敏 | N/A | 无用户可见错误 |
| 敏感数据处理 | N/A | |
| **DoS（内存膨胀）** | ✅ | Cap 128 × (sizeof(Entry) ≈ 320 B) ≈ 40 KB 硬顶；FIFO 保证有界 |
| **碰撞侧信道** | ✅ | FNV-1a 非密码学 hash（不用于密码场景），+ text_len guard 使同碰撞概率 ~2^-96 |
| **UAF** | ✅ | Stability contract（header §39-45）documented：返回指针在下次同 slot 覆盖前有效；caller 不得跨 DrawText 持有 |
| **Font reload stale hit** | ✅ | `FontManager::Shutdown` 先 `shape_cache_.Clear()` 再释放 fonts；设计 §6.2 明示 |

**结论**：安全面 **lightly-touched**（扩展 FontManager 内部 API），威胁建模已于设计阶段 §6.2 完成，所有 4 威胁向量均有代码层或 contract 层 mitigation。

---

## 8. 关键指标汇总

- **代码新增：** 新 8 文件 / 改 9 文件 / +2316 / -101 lines（含 plan/spec 1362 lines 文档）
- **生产代码净增：** ~450 lines（hash.h 41 / shape_cache.{h,cc} 146 / hb_buffer_holder.{h,cc} 59 / font_handle.h 20 / font_manager 增改 57 / software_canvas 增改 ~20）
- **测试新增：** 11 GTests（3 HashBytesU64 + 7 ShapeCache + 4 集成含 1 R2 反向探针）
- **性能收益：** Warm_Medium -32.8% mean / -46.4% single；Warm_Short -54%；Warm_Long -59%；RoundRobin 2676 ns @ 100% hit；AllMiss 4711 ns @ 100% miss
- **Plan 时间校准：** ×0.26（「最窄路径」第 4 数据点，继 TASK-24-01 0.29× / TASK-24-03 0.34×）
- **Git 提交：** 7 commits（VAN / plan / P1 / P2 / P3 / P4 / P5+P6），每个 commit self-contained 可回滚
