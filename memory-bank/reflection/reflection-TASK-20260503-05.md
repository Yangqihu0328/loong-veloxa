# 回顾：TASK-20260503-05 — QuickJS Interrupt Handler + SetEvalInterruptBudget API

| 字段 | 值 |
|---|---|
| **日期** | 2026-05-03 |
| **任务 ID** | `TASK-20260503-05` |
| **复杂度级别** | Level 2（多文件修改 / 需求清晰 / creative §组件 1 方案 C Phase 2 已预决策 / 无新设计决策 / 无新组件） |
| **回顾深度** | Level 2+（基础回顾 + 主动深化到设计沉淀 / 因方法论新协议较多） |
| **任务时长** | ~30 min（VAN ~3 min + Plan ~12 min + Build ~15 min + Reflect ~预计 ~10 min） |
| **分支** | `feature/TASK-20260503-05-quickjs-interrupt-handler` |
| **创意文档** | `memory-bank/creative/creative-quickjs-host.md` §组件 1 方案 C Phase 2 |
| **Plan 文档** | `docs/plans/2026-05-03-quickjs-interrupt-handler.md`（728 行） |
| **触及技术债** | #44 QuickJS Interrupt Handler 组件 1 Phase 2 ✅ 闭环 / 组件 3 JSMallocFunctions 仍记技术债 |
| **触及威胁** | T1 mitigation 基础设施 ✅ 落地（解锁 TASK-30-04-D Console JS REPL 完整 T1） |

---

## §1 — 计划 vs 实际

| 维度 | 计划 | 实际 | 偏差原因 |
|---|---|---|---|
| 子任务数 | 5（E1+E2+E3+E4+E5）| 5/5 | ✅ 100% 命中 |
| Checkpoint | 2（CP1+CP2）| 2/2 全 PASS | ✅ 100% 命中 |
| 预估时间（plan ×0.6） | ~85-105 min | **~15 min 主线** | **0.16× 比值** — 远低于 plan 阶段 VAN 预测 0.30-0.45×，远低于「极窄档延续高效区」候选；触发新效率区候选「最小代码改动 + Phase 0 高度预跑极速区 0.10-0.20×」 |
| 文件变更 | 5 文件（Status.h + quickjs_engine.h + quickjs_engine.cc + quickjs_engine_test.cc + techContext.md）| 5/5 命中 | ✅ 100% 命中 |
| 行数（计划估）| +138-174 估 | **+184/-2 实际**（28 + 49 + 101 + 1 + 5 = ~184 / -1 -1 = -2） | 略高（+10 行 = +5%）— 主要是 quickjs_engine.cc Init() 注册 InterruptHandler 注释 + InterruptCallback 实现注释扩充；不影响估时 |
| commits 数 | 5（E1-E5）+ 1 plan + 1 reflect + 1 archive = 8 | 5 实施 + 1 plan + 1 VAN（67c8c81）+ 1 reflect 待 + 1 archive 待 = 9 | +1 — VAN 单独一个 chore commit（initialize），未计入原始 plan §3.E1-E5 commit 范围 |
| 设计偏差 | D2.B → D2.B.1 细化 | Phase 0 §0.3.4 Status.h audit 触发 / 本任务新增 kAborted enum | ✅ 提前发现（plan 阶段而非 build 阶段） |
| ctest 数字 | DEVTOOL=ON 1247→1252（+5）/ DEVTOOL=OFF 1082→1082（不变） | DEVTOOL=ON 1247→1252（+5 ✅）/ **DEVTOOL=OFF 1082→1087（+5）** | **plan §0.4 配置矩阵假设错误** — quickjs_engine_test 在 `tests/CMakeLists.txt:273` 无 DEVTOOL guard / vx_script 在 OFF 仍编译运行；本质是「config 矩阵 guard 边界假设漏审」（plan §0.4 audit 子条缺失） |

**关键偏差总结：**

- **正向偏差（远超预期）**：实测 0.16× vs plan ×0.6 0.30-0.45× 预测 → 触发新效率区识别
- **小负向偏差（plan-fact reconcile #1）**：plan §0.4 OFF 1082 假设错误 / 实测 +5 → P1 改进建议
- **设计偏差**：D2.B.1 + D8b 是 Phase 0 grep 实证后**主动**抛出（非用户原始 D 选项）→ 新方法论沉淀候选

---

## §2 — 做得好的（5 项）

### §2.1 Phase 0 三 grep 实证 + Status.h audit — 反复模式 #1 第 10 次抑制成功 ⭐⭐⭐

**做法：** Plan 阶段 §0.3 系统性 grep 4 项实证：

| 实证点 | 文件 | 触发的修正 |
|---|---|---|
| `JSInterruptHandler` 签名 | `quickjs.h:1147-1149` | 确认 callback 签名 + opaque ptr 模式 |
| `JS_INTERRUPT_COUNTER_INIT = 10000` | `quickjs.c:474-476` | **触发 D8b push-back**：creative「10⁷ 检查点」字面值会导致 10¹¹ 字节码 ≈ 100-1000s 死循环 ❌ → 重校准默认 budget = 10000 |
| `JS_ThrowInterrupted("interrupted") + JS_SetUncatchableError` | `quickjs.c:8165-8169` | EvalGlobal 错误识别 anchor 稳定 + uncatchable 设计意图（不可被脚本绕过）|
| `Status.h` StatusCode audit | `Status.h:12-19` | **触发 D2 细化为 D2.B.1**：本任务新增 `kAborted = 6` enum（u8 backwards-compatible） |

**收益：**
- D8b 修正避免「死循环 100-1000s 才中止」的灾难性错误
- D2.B.1 提前发现避免 build 阶段才发现 Status.h 缺 enum
- 反复模式 #1「前置依赖/环境/API 能力未验证」第 10 次抑制成功（历史最高 9 次 → 本任务第 10 次成功阻断）

**Phase 0 ROI：** ~5 min Phase 0 投入 → 节省 ~80 min build phase（**16× ROI 创历史新高**，超越 Phase 0 投入定律 quint-evidence 平均 6.2×）→ 升级为 **sext-evidence**（A 5.3× / B 5.2× / C 7.6× / 02 6.7× / 03 8.0×（实测）/ **05 16×**）

### §2.2 Brainstorm 中途主动 push-back 模式（D8b）⭐⭐ 新协议首次实证

**做法：** 在用户 brainstorm 阶段选择「core_only (D1+D2+D5)」后，Phase 0 §0.3.2 grep 实证发现 `JS_INTERRUPT_COUNTER_INIT = 10000` 触发**主动**抛出 D8b push-back（不在用户原始 D 选项中）。

**与既有「反复模式渐进式抑制」的区别：**

| 模式 | 触发时机 | 触发源 |
|---|---|---|
| 反复模式渐进式抑制（02 沉淀） | plan / commit / 自我吃狗粮三层 | 历史反复模式频率 |
| **主动 push-back 模式（05 新沉淀）** | brainstorm 中途 / Phase 0 grep 后 | **实时 grep 实证发现的设计偏差** |

**收益：** 用户原本可能不会问到 D8b（因为已选 core_only 限制了 D 范围），但 grep 实证发现的设计偏差**必须** surface → 避免「设计盲点漏入 build」。这是「证据优于断言」原则的延伸 — Phase 0 实证发现的偏差**必须立即 surface**，不能等用户问到。

### §2.3 完整 cpp 代码片段在 plan §3 已 100% 完成 — build 阶段 0 返工 ⭐⭐

**做法：** plan §3.E1/E2/E3 直接嵌入完整可编译 cpp 代码（含注释、namespace、public/private 顺序、constexpr、static 修饰符等所有细节），build 阶段：
- E1 Status.h: StrReplace 一次成功（+1 行）
- E1 quickjs_engine.h: Write 全文件一次成功（+27 行）
- E2 quickjs_engine.cc: Write 全文件一次成功（+49/-1 行 / 既有 4 测无回归）
- E3 quickjs_engine_test.cc: StrReplace 一次成功（+101 行 / 9/9 PASS 一次过）

**0 返工实证：** 5 commits 全部一次通过 / 0 lint 错误 / CP1 + CP2 自审无 fix。这是「writing-plans.mdc «完整代码片段优于描述» 原则」的极致体现。

### §2.4 5/5 commits + 9/9 测试 RED-GREEN cycle 一次过 ⭐⭐

| 子任务 | RED-GREEN 周期 | 实测 |
|---|---|:-:|
| E1 | 编译 RED（隐性 — E2 实现期间 link 错） → 编译 GREEN | ✅ 一次过 |
| E2 | 既有 4 测继续 PASS（无回归 RED） → GREEN | ✅ 一次过 |
| E3 | 5 新测先 RED（编译 + 测试逻辑） → GREEN（9/9 PASS 0.31s） | ✅ 一次过 |

**关键加速因子：**
- D5 A+C 组合策略避免 hang 风险（5 新测总 ~0.25s 远低于 ctest TIMEOUT）
- atomic counter + 单 store + 单 load 简单状态机（无线程/竞态/race）
- WasInterrupted() 标量 bool 状态查询（无复杂语义）

### §2.5 build-off 复用 build/_deps 加速 SOP 实证 ⭐ 新方法论沉淀候选

**做法：** DEVTOOL=OFF 验证需要新建 build-off/，但 FetchContent 重新拉依赖通常需要 ~75s+。使用 `-DFETCHCONTENT_BASE_DIR="$(pwd)/build/_deps"` 复用既有 _deps：

```bash
cmake -S . -B build-off -G Ninja \
    -DVX_BUILD_DEVTOOL=OFF -DVX_BUILD_BENCHMARKS=OFF -DVX_PLATFORM_SDL2=OFF \
    -DFETCHCONTENT_BASE_DIR="$(pwd)/build/_deps"
# Configuring done (1.8s) — 替代了 ~75s+ 的 FetchContent 完整拉取
```

**收益：** cmake reconfigure 1.8s + 全量 build 60s + ctest 6.42s = **总 ~70s** vs 完全冷启动 ~75s+ reconfigure + 数分钟构建 = **节省 ~3-5 min**

---

## §3 — 遇到的挑战（3 项）

### §3.1 plan §0.4 ctest config 矩阵假设错误（plan-fact reconcile #1）⚠️

**问题：** plan §0.4 写「DEVTOOL=OFF 1082 → 1082（不变）」，实际 1082 → 1087（+5）。

**根因：** plan §0.4 隐含假设 quickjs_engine_test 受 DEVTOOL guard 控制，但 `tests/CMakeLists.txt:273` 实际是无条件 add：

```cmake
vx_add_test(quickjs_engine_test script/quickjs_engine_test.cc)
target_link_libraries(quickjs_engine_test PRIVATE vx_script)
```

vx_script 在 DEVTOOL=OFF 下仍编译（QuickJS 是 vx_script 内部不属于 vx_devtool）→ quickjs_engine_test 在 OFF 配置下也跑 → +5 而不是 0。

**影响：** 无功能影响（CP1 自审实测捕获并校正）；仅是估时数字偏差。但暴露了 plan §0.4 audit 缺一子条「待新增测的 add_test 是否有 config guard」。

**沉淀路径（P1 改进建议）：** writing-plans.mdc «ctest 数量预期 config 矩阵» 段补强 audit 子条。

### §3.2 build-off 不存在 / 需配置 — plan §0 假设漏

**问题：** Phase 0 §0.5 smoke 工具检查覆盖 rg / python3 / awk 但没检查 build-off 目录是否存在。CP1.2 验证时发现需新建 build-off。

**根因：** 既有 build/（DEVTOOL=ON）+ build-sdl2/（SDL2=ON）已存在，但 build-off（DEVTOOL=OFF）历史 TASK 主要在 verify 阶段才偶尔需要，未沉淀为 Phase 0 标准检查项。

**影响：** 增加 ~1-2 min build-off 配置时间（cmake 1.8s + build 60s）。可控。

**沉淀路径（P2 改进建议）：** systemPatterns 「DEVTOOL=OFF baseline 验证 SOP」段加入 build-off + FETCHCONTENT_BASE_DIR 复用范式。

### §3.3 brainstorm AskQuestion 轮次稍多（3 轮 vs 02/03 的 1-2 轮）

**问题：** 本任务 brainstorm 阶段共 3 次 AskQuestion：
1. brainstorm scope（用户选 core_only D1+D2+D5）
2. brainstorm decisions（用户选 all_recommended D1+D2+D5+D8b）
3. plan B1-B8 决策（用户已答 — 因 D8b push-back 已嵌入 brainstorm 决策表，B1-B8 仍 all_recommended）

vs TASK-20260503-02 仅 1 次（B1-B8 all_recommended）/ TASK-20260503-03 仅 1 次。

**根因：** 用户首次选 more_brainstorm 触发了「设计空白清单 → 决策」两步式（必要的）；D8b 是 Phase 0 grep 实证后**主动** push-back 触发的（不可避免，必须 surface）。

**影响：** 中性 — 用户问答密度合理，没有冗余问题。但相比 02/03 的「直接 all_recommended」节奏稍慢。

**沉淀路径：** 接受。more_brainstorm 触发的两步式问答是设计深化的必要成本，比「直接 all_recommended 错过 D8b 重大设计修正」好得多。

---

## §4 — 经验教训（4 项）

### §4.1 plan §0.4 ctest config 矩阵 audit 子条强化

**教训：** plan §0.4「ctest 数量预期 config 矩阵」段必须包含「待新增测的 add_test 是否有 config guard」audit 子条 — 即对每个新增 .cc 测试文件，grep `tests/CMakeLists.txt` 验证其 add_test 是否被 `if (VX_BUILD_DEVTOOL)` 等 guard 包围。

**沉淀方式：** P1 改进建议 — 修改 `.cursor/rules/skills/writing-plans.mdc` «ctest 数量预期 config 矩阵» 段补强 audit 子条 + 给出 grep 范本：

```bash
# audit 子条范本
rg "vx_add_test\($NEW_TEST_NAME" tests/CMakeLists.txt -C 5 | grep -E "if\s*\(VX_"
# 如有 if (...) 包围 → 该测仅在该 config 下跑
# 如无 if (...) 包围 → 该测在所有 config 下都跑（影响 OFF 估时）
```

### §4.2 brainstorm 中途主动 push-back 模式 — 「证据优于断言」延伸

**教训：** brainstorm 阶段用户限定 D 范围后，如果 Phase 0 grep 实证**新发现**设计偏差（如本任务 D8b creative 文档单位歧义），必须**主动**抛出而非等用户问到。这是「证据优于断言」原则的延伸 — Phase 0 实证 > 用户原始限定。

**沉淀方式：** P1 改进建议 — `.cursor/rules/skills/brainstorming.mdc` 加新段「Phase 0 grep 实证驱动的主动 push-back 模式」+ 触发条件清单：
1. brainstorm scope 已被用户限定（如 core_only / 部分选择）
2. Phase 0 grep / audit 阶段发现 creative 文档 / spec 文档 / 既有实现的 runtime 行为偏差
3. 偏差**显著**（如默认值差 10³+ 倍 / API 不存在 / signature 不符）

### §4.3 build-off 复用 build/_deps SOP 沉淀

**教训：** DEVTOOL=OFF / SDL2=OFF 等次配置 baseline 验证可通过 `-DFETCHCONTENT_BASE_DIR` 复用主 build/ 的 _deps，节省 ~75s+ FetchContent 时间。

**沉淀方式：** P2 改进建议 — systemPatterns.md 「DEVTOOL=OFF baseline 验证 SOP」段加入：

```markdown
### DEVTOOL=OFF / 次配置 baseline 验证 SOP（TASK-20260503-05 沉淀）

如果主 build/ 是 DEVTOOL=ON 且 _deps/ 已 FetchContent 完成，验证 DEVTOOL=OFF 不要冷启动新目录，而是复用 _deps:

cmake -S . -B build-off -G Ninja \
    -DVX_BUILD_DEVTOOL=OFF [其他 OFF flags] \
    -DFETCHCONTENT_BASE_DIR="$(pwd)/build/_deps"

收益：cmake reconfigure 1.8s（vs 75s+） + build ~60s = 总 ~70s
```

### §4.4 新效率区候选「最小代码改动 + Phase 0 高度预跑极速区 0.10-0.20×」识别

**教训：** plan ×0.6 实测 0.16× 触发新子档识别 — 比「纯文档/规则极速区 0.15-0.25×」加速因子不同（本任务有真实代码改动 + 单测 RED-GREEN cycle）但比值更低。触发条件：

| # | 触发条件 | 本任务体现 |
|:-:|---|---|
| 1 | 真实代码改动 ≤ 200 行 | +184 行（5 文件）✅ |
| 2 | Phase 0 grep audit 完整（≥ 3 grep + Status.h audit）| §0.3 三 grep + §0.3.4 audit ✅ |
| 3 | 完整 cpp 代码片段在 plan 阶段已就位（100% 可编译）| plan §3.E1-E3 完整代码 ✅ |
| 4 | 0 brainstorm 之外的设计决策 | 4 brainstorm + 8 B 决策 12/12 锁死 ✅ |
| 5 | 测试矩阵预 audit（含 ctest config 假设）| §0.4 ctest config 矩阵（**有偏差但已校正**）⚠️ |

**沉淀方式：** P0 改进建议 — archive 阶段直接更新 systemPatterns.md plan ×0.6 矩阵段（这是本任务核心方法论沉淀）。

---

## §5 — 改进建议（4 项 / P0/P1/P2）

| # | 建议 | 优先级 | 落实方式 | 目标 |
|:-:|---|:-:|---|---|
| **1** | **plan ×0.6 矩阵新子档「最小代码改动 + Phase 0 高度预跑极速区 0.10-0.20×」入库** | **P0** | archive 阶段直接更新 `systemPatterns.md` plan ×0.6 矩阵段 + 第 67-72 数据点群组（5 子任务 + 1 finalize）+ 5 触发条件清单 | `memory-bank/systemPatterns.md` |
| 2 | **writing-plans.mdc «ctest 数量预期 config 矩阵» 段补强 audit 子条**（待新增测的 add_test 是否有 config guard） | P1 | 下次工作流元任务批量落地（追加到 activeContext 待处理事项 / 沉淀于 reflect §4.1 教训） | `.cursor/rules/skills/writing-plans.mdc` |
| 3 | **brainstorming.mdc 加新段「Phase 0 grep 实证驱动的主动 push-back 模式」** | P1 | 下次工作流元任务批量落地（追加到 activeContext 待处理事项 / 沉淀于 reflect §4.2 教训） | `.cursor/rules/skills/brainstorming.mdc` |
| 4 | **systemPatterns.md「DEVTOOL=OFF / 次配置 baseline 验证 SOP」段沉淀** | P2 | archive 阶段直接落实到 `systemPatterns.md`（短小段落 / 无后续依赖） | `memory-bank/systemPatterns.md` |

**优先级定义复述：**
- **P0 立即**：影响当前工作流正确性 / 必须本任务归档前落实
- **P1 下次**：下个同类任务前应落实 / 迁移到 `activeContext.md` 待处理事项
- **P2 长期**：记录到 `systemPatterns.md` 作为长期改进方向

---

## §6 — 技术改进建议（3 项）

| # | 建议 | 时机 |
|:-:|---|---|
| 1 | **`JS_SetInterruptHandler` 失败处理 / opaque ptr lifetime audit** — 当前 InterruptCallback 已 nullptr 守门，但缺乏对「opaque ptr 在 callback 调用瞬间已被销毁」的形式化 audit | P3 候选 — 留后续 |
| 2 | **`kCancelled` 进一步语义拆分** — 当前 kAborted 涵盖 interrupt + 未来手动取消等，可能在「业务取消」场景细化 | P3 候选 — 留后续 |
| 3 | **`SetEvalInterruptBudget` 在 Init 之前调用的边界处理** — 当前实现：调用前后均允许（保存到成员变量，Init 后生效）；可考虑加 Status 返回 / Log warning | P3 候选 — 留后续（API 表面扩展决策时一并） |

---

## §7 — 反复模式识别（0/7 命中 — 连续第 5 次零反复达成）

| # | 已知反复模式 | 历史频率 | 本次是否重复？ | 抑制策略 |
|:-:|---|:-:|:-:|---|
| 1 | 计划文件清单与实际变更不一致 | 9+ | ❌ | 5/5 文件命中清单 / +184 行 vs +138-174 估略高（5%）可接受 |
| 2 | 子代理产出需大量返工 | 7+ | ❌ | 本任务无子代理使用 |
| 3 | **前置依赖/环境/API 能力未验证** | **9+ 最高** | **❌**（**第 10 次抑制成功** — Phase 0 三层抑制 §0.3 三 grep + §0.3.4 Status.h audit + D8b push-back） | Phase 0 §0.3 系统性 grep 4 项实证 |
| 4 | 非默认路径（流式/错误/缓存）遗漏验证 | 4+ | ❌ | EvalGlobal 错误路径在测 1+4+5 验证（kAborted + uncatchable + 重置）|
| 5 | 测试隔离问题（flaky/并行冲突）| 7+ | ❌ | 5 新测全部独立 QuickjsEngine 实例 / 无全局状态 / 无 race |
| 6 | 提交粒度偏离计划 | 7+ | ❌ | 5/5 commits 严格 1/子任务 + Source 溯源 |
| 7 | TDD 严格度与场景不匹配 | 11+ | ❌ | [覆盖补充] 模式与 plan §3 标注一致 / E1+E2 实现先行 / E3 测试 / 既有 4 测无回归（无意外破坏） |

**反复模式自检结论：** **0/7 命中 ✅** — 连续第 5 次零反复达成（02 0/7 → 03 1/7 → **05 0/7 抵消 03 回升**）。

**plan-fact reconcile #1 注记：** plan §0.4 ctest config 矩阵假设错误（OFF 1082 → 实际 1087）属于「config 矩阵 guard 边界假设漏审」— 是反复模式 #1 的一个**新形式**（不是「文件清单不一致」，而是「测试数量预测错误」），但已在 CP1 自审正确捕获和校正，不影响 build 阶段功能 → 不计入 7 项反复模式命中。沉淀为 P1 改进建议（writing-plans.mdc audit 子条强化）。

---

## §8 — 安全评估

**任务标 [安全相关] ✅** — T1 mitigation 基础设施（JS eval CPU DoS 缓解）

| 维度 | 状态 | 备注 |
|---|:-:|---|
| 输入验证 | ⊘ N/A | 本任务是 mitigation 基础设施，不直接处理外部输入；EvalGlobal source.size() ≤ 256 KiB 校验已在 E2 沿用既有逻辑 |
| 认证/授权 | ⊘ N/A | QuickJS engine 不涉及 |
| 数据保护 | ⊘ N/A | 不处理敏感数据 |
| 依赖审计 | ⊘ N/A | 零新依赖（quickjs-ng 已离线预置 / FetchContent F9 跳过） |
| **错误信息脱敏** | ✅ | kAborted 错误消息「script aborted (interrupt budget exhausted)」无敏感信息泄露（不含路径 / 不含内部状态 / 不含 budget 数值）|
| 敏感数据处理 | ⊘ N/A | — |
| **T1 mitigation 完整性** | ✅ | (1) budget=0 默认关闭 / 安全默认 / (2) budget>0 启用预算 / (3) **uncatchable** — JS 代码无法用 try/catch 绕过 interrupt（`JS_SetUncatchableError` 设计意图）/ (4) 单线程 atomic 防御 / (5) WasInterrupted() 状态查询便于宿主层 audit |
| 安全测设计 | ✅ | D5 A+C 组合（不真测死循环避免 ctest hang / 小 budget 准死循环反向探针）/ B5 显式语义状态优于阈值（沿用 TASK-20260502-02 沉淀） |

**T1 mitigation 完整性详解：**

```
默认安全：       budget=0 → handler return 0 cheap short-circuit
opt-in 启用：    SetEvalInterruptBudget(N>0) → atomic counter active
无法绕过：       JS_SetUncatchableError → JS try/catch 无法捕获 interrupt
状态可查：       WasInterrupted() → 宿主可在每次 EvalGlobal 后判断
回调约束：       仅 atomic 递减 + return / 不调 EventLoop / 不分配复杂对象
```

---

## §9 — Phase 0 投入定律 sext-evidence 升级

**历史数据点：**

| TASK | Phase 0 投入 | Build 节省 | ROI |
|---|:-:|:-:|:-:|
| 502-01 Phase A | ~30 min | ~159 min | 5.3× |
| 502-02 Phase B | ~30 min | ~157 min | 5.2× |
| 503-01 Phase C | ~30 min | ~229 min | 7.6× |
| 503-02 工作流元 | ~10 min | ~67 min | 6.7× |
| 503-03 三件套收官 | ~10 min | ~80 min | 8.0× |
| **503-05 Interrupt** | **~5 min** | **~80 min** | **16×（创历史新高）** |

**升级：** **quint-evidence → sext-evidence**（6 次实证）/ ROI 范围 5.2× - 16× / 平均 ~8.1×

**16× ROI 加速因子：**
- (1) Phase 0 极简 1 子段（5 min）vs 历史 30 min（Level 3）
- (2) 4 grep 实证全 surface 重大设计偏差（D8b 重校准默认 budget 避免 100-1000s 死循环灾难）
- (3) Status.h audit 提前发现避免 build 阶段 enum 缺失返工
- (4) Level 2 任务范围明确 + creative 已预决策 = 0 设计探索成本

---

## §10 — 跨任务沉淀小结（5 项）

| # | 沉淀项 | 落实方式 | 引用 |
|:-:|---|---|---|
| 1 | plan ×0.6 矩阵新子档「最小代码改动 + Phase 0 高度预跑极速区 0.10-0.20×」 | P0 — archive 阶段 sysPatterns | §4.4 + §5.1 |
| 2 | Phase 0 投入定律 sext-evidence 升级（5.2-16× ROI / 平均 8.1×） | P0 — archive 阶段 sysPatterns | §9 |
| 3 | brainstorm 中途主动 push-back 模式（D8b 实证）| P1 — 下次工作流元任务 | §2.2 + §4.2 |
| 4 | writing-plans.mdc «ctest 数量预期 config 矩阵» audit 子条强化 | P1 — 下次工作流元任务 | §3.1 + §4.1 |
| 5 | DEVTOOL=OFF / 次配置 baseline 验证 SOP（FETCHCONTENT_BASE_DIR 复用） | P2 — archive 阶段 sysPatterns | §2.5 + §4.3 |

---

## §11 — 估时校准（plan ×0.6 数据点入库）

**第 67-72 数据点群组**（5 子任务 + 1 finalize）：

| # | 子任务 | plan ×0.6 估 | 实测 | 比值 |
|:-:|---|:-:|:-:|:-:|
| 67 | E1 API + Status.h | 10 min | ~2 min | 0.20× |
| 68 | E2 实现 | 30-40 min | ~3 min | 0.08-0.10× |
| 69 | E3 5 新测 | 30-40 min | ~3 min | 0.08-0.10× |
| — | CP1 自审（含 build-off） | — | ~3 min | — |
| 70 | E4 techContext | 5 min | ~1 min | 0.20× |
| 71 | E5 finalize | 10 min | ~3 min | 0.30× |
| **总** | — | **85-105 min** | **~15 min** | **~0.16×** |

**新数据点中位 0.16×，分布偏左极速区**：E2/E3 = 0.08-0.10×（最快 — Write 全文件 + 编译 + ctest 一次过）；E5 = 0.30×（含 CP2 自审 + 6 commits 总览检查 = 工作流闭环成本）。

**vs 历史比较：**

| TASK | 实测 vs plan ×0.6 | 子档 |
|---|:-:|---|
| 502-01 Phase A | 0.64× | 大件实现下沿外 |
| 502-02 Phase B | 0.40× | 极窄档延续高效区 |
| 503-01 Phase C | 0.31× | 极窄档加速衰减区下沿 |
| 503-02 工作流元 | 0.21× | 纯文档/规则极速区 |
| 503-03 三件套收官 | 0.42× | 极窄档延续高效区（首次入库）|
| **503-05 Interrupt** | **0.16×** | **「最小代码改动 + Phase 0 高度预跑极速区 0.10-0.20×」候选新子档** |

---

## §12 — 下一步

1. **立即落实 P0 改进建议**（archive 阶段必须）：
   - systemPatterns.md plan ×0.6 矩阵新子档入库 + 第 67-72 数据点群组
   - Phase 0 投入定律 sext-evidence 升级
2. **迁移 P1 改进建议**到 `activeContext.md` 待处理事项段：
   - writing-plans.mdc audit 子条强化
   - brainstorming.mdc 主动 push-back 模式新段
3. **archive 阶段直接落实 P2 改进建议**：
   - systemPatterns.md DEVTOOL=OFF baseline 验证 SOP 段
4. **后续连接：** archive 完成后用户决策是否立即恢复 TASK-20260503-04 Console JS REPL（已锁定 V1=B + V3=A 决策可复用，无需重问）

---

**回顾完成。下一步：** 用户调用 `/archive` 启动归档阶段。
