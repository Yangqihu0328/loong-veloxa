# 回顾：流程规则沉淀 + P2 功能技术债收口

**日期：** 2026-04-19
**任务 ID：** TASK-20260419-01
**复杂度级别：** Level 3（中等功能 / 跨「.cursor 规则」+ 「Script / CSS / Event」3 个子系统）
**工作流路径：** `/van` → `/plan` → `/build` → `/reflect`（无 `/creative`，头脑风暴阶段已固化全部 3 个设计点）

---

## 计划 vs 实际

| 维度 | 计划 | 实际 | 偏差原因 |
|------|------|------|---------|
| Phase 数 | 9（0 + A1-A4 + B5-B7 + 8） | 9 | 完全一致 |
| 预估时间 | ~4h | ~4h（含 B5 链接错误约 10 min） | 接近预估 |
| 提交数 | 21 | **21** | **100% 匹配**（含 plan 提交） |
| 文件变更（源） | 13 列出 | 13 + `veloxa/script/CMakeLists.txt`（1 个未预见）| 链接拓扑变更，见挑战 #1 |
| 新增测试数 | +22（基线 856 → ≥ 878） | **+34**（基线 856 → 890）| Part B 边界用例拆得更细（B5 13 类 + B7 3 种 remove 形式） |
| 编译警告 | 0 新增 | **0 新增** | ✅ 一致 |
| API 兼容 | 现有 856 测试零修改通过 | ✅ 零修改 | `EventDispatcher::AddEventListener` 由 `void` → `u64` 利用 C++ "可丢弃返回值"特性 |
| 设计变更 | — | B7 实现时拆分 `DropJsEntry` / `MaybeForgetElement` helpers | 头脑风暴未细化 JS removeEventListener 三种重载语义 |

### 提交序列（21 个）

```
89cfae2 docs(plan): TASK-20260419-01 design spec and implementation plan      [Phase 计划]
ea36625 docs(rules): add FetchContent C subproject compile options audit       [Phase A1.1]
e152597 docs(rules): add test infrastructure audit section                     [Phase A1.2]
33692f0 docs(rules): add boundary input checklist section                      [Phase A1.3]
3caa649 docs(rules): add end-to-end call chain verification section            [Phase A1.4]
951f057 docs(rules): add pipeline injection point feasibility section          [Phase A1.5]
dc18f23 docs(rules): require explicit cross-module data format                 [Phase A2.1]
79bcd9d docs(rules): require LayoutBox coordinate semantics                    [Phase A2.2]
9dfd38d docs(rules): add parallel subagent prerequisites checklist             [Phase A2.3]
0e50064 docs(rules): add integration-testing skill (6 ironclad rules)          [Phase A3.1]
95c8184 docs(rules): register integration-testing skill in main                [Phase A3.2]
e3229fa docs(memory-bank): add FetchContent and proxy notes to techContext     [Phase A4.1]
e650517 docs(memory-bank): mark Part A complete                                [Phase A4.2]
caa7c6d test(css): add failing enum_serialization tests (RED)                  [Phase B5.1 RED]
f6e17d8 refactor(script): wire StyleGetProp through EnumValueToCssString       [Phase B5.2 GREEN]
b71589e test(event): add failing destruction observer tests (RED)              [Phase B6.1 RED]
b353190 feat(event): add destruction observer to EventManager                  [Phase B6.2 GREEN]
d55772e feat(event): EventDispatcher returns ListenerToken; precise removal    [Phase B7.1]
63aaefe feat(event): EventManager forwards ListenerToken                       [Phase B7.2]
ed5d455 refactor(script): removeEventListener uses ListenerToken (#47)         [Phase B7.3]
6f36783 docs(memory-bank): mark TASK-20260419-01 build complete                [Phase 8 收尾]
```

测试：**890/890 passed**（基线 856 + 34，0 回归）。

---

## 回顾检查清单

本任务跨**配置/规则类**（Part A）+ **代码变更类**（Part B）两类。

**Part A（11 提交，纯 .mdc/MB 文档）：**
- [x] **文件位置验证** — A3 创建新 skill 前先 Glob 确认 `.cursor/rules/skills/` 目录结构；A4 追加 techContext 段前先 Read 现有结构
- [x] **交叉引用** — A3.2 明确把 `integration-testing.mdc` 注册到 `main.mdc` 技能表；保持与 `writing-plans.mdc` / `subagent-development.mdc` 同级地位

**Part B（8 提交，含 TDD）：**
- [x] **计划精确度** — 源文件清单 13/13 匹配；唯一未预见：`veloxa/script/CMakeLists.txt` 需新增 `target_link_libraries(vx_script PRIVATE vx_core)`（链接拓扑变更，见挑战 #1）
- [x] **TDD 执行情况** — 严格 RED→GREEN：B5（`caa7c6d` RED → `f6e17d8` GREEN）、B6（`b71589e` RED → `b353190` GREEN）、B7（impl 与测试同提交，token 路径演进式 RED 因 API 不存在则编译失败即 RED）
- [x] **测试隔离** — 全部新增测试独立构造 `EventManager` / `EventDispatcher` / `DomBindings` / `QuickjsEngine`，无共享状态，0 flaky
- [x] **提交粒度** — 每个子 Phase 独立 commit，message 阐明 why（"#46 followup" / "#47" / "relax DomBindings ordering"），无大杂烩
- [x] **非默认路径** —
  - B5：`OutOfRangeReturnsEmpty` / `NonEnumPropertyReturnsEmpty` / `UnknownPropertyReturnsEmpty` / `ReturnedStringHasStaticStorage`（4 类边界）
  - B6：双向析构（`EventManagerDestroyedBeforeDomBindings` + `DomBindingsDestroyedBeforeEventManagerStillWorks`）+ token 注销验证
  - B7：3 种 remove 形式全覆盖（0 args / 1 arg type / 2 args type+handler）+ 未知 token no-op + 同 type 不同 handler 互不影响

---

## 做得好的

1. **/plan 阶段 3 个设计点全部固化，跳过 /creative**：B5（完整反查表 vs 懒加载）、B6（DestructionObserver vs 弱引用计数）、B7（u64 token vs handler 字符串哈希）三处都在头脑风暴阶段直接锁定，引用项目内已有的同构模式作为论据（property.cc / SetInvalidationCallback / JSValue 句柄风格），实施零返工。
2. **Part A / Part B 严格分序**：Part A 11 个文档提交先合，Part B 才开始动代码，保证流程规则被引入 commit 历史时无任何 C++ 编译风险。Part B 内部 B5 → B6 → B7 串行也按"独立 → 基础设施 → 业务接入"严格分层。
3. **TDD 提交清晰可溯源**：B5 / B6 都有独立的 RED 提交（`caa7c6d` / `b71589e`），任何人 `git checkout caa7c6d^..caa7c6d` 都能复现"实现前测试为何失败"。
4. **API 兼容设计**：`EventDispatcher::AddEventListener` 返回类型由 `void` → `u64`，借 C++ "调用方可丢弃非 void 返回值" 的特性零成本兼容所有既有调用点；856 个旧测试 0 修改通过。
5. **集成测试规范实战验证**：`integration-testing.mdc` 6 条铁律在 A3 写出后，B6/B7 的测试就直接受其约束（双向析构 + JS callback 三个生命周期点 + 真实组件无 mock + < 100ms），形成"规则即写即用"的闭环。
6. **基线测试快照立刻入 progress.md**：Phase 0 跑出 856/856 后立即记录，结束时 890/890 才能形成精准 delta（+34）。

---

## 遇到的挑战

### 1. 静态库循环依赖未在 /plan 阶段识别（链接错误约 10 min 排查）

- **现象**：B5 实现 `EnumValueToCssString` 后，把它接入 `dom_bindings.cc`（属于 `vx_script`），构建时报 `undefined reference to vx::css::EnumValueToCssString`。
- **根因**：`vx_core` 已经 `target_link_libraries(... vx_script)`（DOM 触发 JS 回调），形成循环依赖。CMake 对静态库循环可处理，但需要双向 link：必须显式 `target_link_libraries(vx_script PRIVATE vx_core)` 才能让 linker 在 archive 重复扫描中解析符号。
- **修复**：1 行 CMake 修改即解决。
- **教训**：跨子库新增符号引用时，应在 /plan 阶段就 grep 当前 link graph（`grep -r "target_link_libraries.*vx_core" CMakeLists.txt`）确认拓扑。

### 2. JS `removeEventListener` 三种重载语义未在头脑风暴细化（B7 实现时拆分 helper）

- **现象**：B7 的 prompt 中只说"按 token 精确移除"，实施时发现 JS 端 `removeEventListener()` / `removeEventListener(type)` / `removeEventListener(type, handler)` 三种调用都需要支持，零参/单参为兜底（清空 / 按类型批删），双参才走 token 路径。最终拆出 `DropJsEntry` / `MaybeForgetElement` 两个 helper 才让 `js_listener_entries` / `listener_elements` 两个数据结构同步收口。
- **根因**：头脑风暴只关注了 token API 的 C++ 形态，没把 JS 端 Web API 标准的多重载语义列全。
- **教训**：接入 WHATWG/DOM/MDN 标准 API 时，头脑风暴必须列出 spec 全部 overload 形态及其对应 C++ 路径。这是反复出现的"非默认路径遗漏"模式的子类。

### 3. `StatusOr<T>::error_message()` 接口缺失（测试小返工）

- **现象**：`tests/script/dom_bindings_test.cc` 写 `ASSERT_TRUE(r.ok()) << r.error_message();` 编译失败。
- **根因**：项目自实现 `StatusOr` 不带 `error_message()`，与常见 absl::StatusOr 不同。
- **修复**：移除 `<< r.error_message()`。
- **教训**：使用未读过 header 的 API 前应 Grep 接口签名确认（虽然这次只是测试断言，影响可控）。

---

## 经验教训

### 流程层

1. **"21 个提交计划 → 21 个实际提交"是可达成目标**：核心方法论是 /plan 阶段就把每个 Phase 拆成单一职责的 commit，并明确 RED/GREEN 分提交。
2. **跳过 /creative 的判据要严格**：本次跳过的依据是"3 个设计点都能在已有代码中找到同构模式"。如果某个设计点找不到先例，应立即触发 /creative 而不是依赖头脑风暴的"看起来可行"。
3. **Part A（流程规则）应先于 Part B（业务代码）**：流程规则一旦合入，紧接着的 Part B TDD 就能受规则约束（B6/B7 测试受 `integration-testing.mdc` 约束就是即时回报）。

### 技术层

1. **静态库循环依赖在 C++ 子库化项目中是常态**：CMake 通过 archive 重复扫描可解，但需要双向 `target_link_libraries`。
2. **DestructionObserver 模式适用于"非拥有跨模块引用"**：`DomBindings` 持有 `EventManager*` 但不拥有，弱引用 + 析构通知是这种场景的标准答案；token 化 add/remove 保证 observer 注销精确（避免析构时回调已死亡的 observer）。
3. **u64 token 模式的扩展点**：除 `ListenerToken`，本项目其它持续型句柄（如未来的 timer / animation）都可复用同模式。建议固化到 `systemPatterns.md`。

---

## 改进建议（附优先级与落实方式）

| # | 建议 | 优先级 | 落实方式 | 目标 |
|---|------|--------|---------|------|
| 1 | **跨子库新增符号引用前必须 grep link graph**：`/plan` 阶段对每个跨库新符号验证 `target_link_libraries` 双向闭合 | **P1** ⚠️ 反复模式（"前置依赖未验证"第 9 次） | 追加段到 `.cursor/rules/skills/writing-plans.mdc`：「跨静态库符号引用拓扑审计」 | `writing-plans.mdc` |
| 2 | **接入 Web 标准 API 必须列全 spec 重载形态**：WHATWG/DOM/MDN/Fetch 等标准接入前，头脑风暴必须把所有 overload 形态、参数组合、错误路径列成表 | **P0** ⚠️ 反复模式（"非默认路径遗漏"第 5 次，且本次直接造成 B7 返工） | 追加段到 `.cursor/rules/skills/writing-plans.mdc`：「Web 标准 API 重载形态清单」 | `writing-plans.mdc` |
| 3 | **u64 不透明句柄模式提升为系统模式**：DestructionObserverToken / ListenerToken 已成第 2/3 次复用，应正式入 `systemPatterns.md` | **P2** | 追加到 `memory-bank/systemPatterns.md` 句柄模式段 | `systemPatterns.md` |
| 4 | **StatusOr API 表面文档化**：在 `techContext.md` Foundation 段标注本项目 `StatusOr` 不含 `error_message()`，避免下次同坑 | **P2** | 追加到 `memory-bank/techContext.md` Foundation 段 | `techContext.md` |
| 5 | **集成测试技能的"API 备忘清单"应持续扩展**：本次新增了 `DispatchPointerDown` 等模板，应定期回收到 `integration-testing.mdc` 的「API Cheat Sheet」段 | P2 | 下次涉及新跨模块测试时回填 | `integration-testing.mdc` |

优先级定义：
- **P0 立即**：本任务归档前落实（建议 #2）
- **P1 下次**：下个同类任务前落实，迁移到 `activeContext.md` 待处理事项（建议 #1）
- **P2 长期**：记录到 `systemPatterns.md` / `techContext.md`（建议 #3 / #4 / #5）

---

## 反复模式识别

本次教训对照已知反复模式（基于 16 份历史回顾）：

| 已知模式 | 历史频率 | 本次是否重复？ | 处理 |
|---------|---------|-------------|------|
| 计划文件清单与实际变更不一致 | 9+ 次 | **轻微**（漏 1 处 CMake link） | 升级建议 #1 至 P1 |
| 子代理产出需大量返工 | 7+ 次 | **未触发**（本任务未用子代理） | — |
| 前置依赖/环境/API 能力未验证 | 8+ 次 | **是**（StatusOr API + 链接拓扑） | 升级建议 #1/#4 |
| 非默认路径（流式/错误/缓存）遗漏验证 | 4+ 次 | **是**（removeEventListener 三种重载） | 升级建议 #2 至 P0 |
| 测试隔离问题 | 7+ 次 | **未出现** ✅ | — |
| 提交粒度偏离计划 | 7+ 次 | **未出现** ✅（21/21 完美匹配） | — |
| TDD 严格度与场景不匹配 | 11+ 次 | **未出现** ✅（B5/B6 严格 RED 提交，Part A 文档无 TDD 合理） | — |

**正面信号**：测试隔离 / 提交粒度 / TDD 严格度 三个反复模式本次完全规避，说明 `writing-plans.mdc` / `integration-testing.mdc` 的近期改进开始见效。

**警示**：「前置依赖未验证」+ 「非默认路径遗漏」仍出现，需要本任务新加的两段规则（建议 #1 / #2）继续收口。

---

## 技术改进建议

1. **EventManager 析构通知顺序**：当前实现是「快照 → 按注册顺序触发」，已通过测试 `DestructionObserversFireInRegistrationOrder` 验证。如未来 observer 数量变多（>10），考虑 RAII handle 包装，避免手动 `RemoveDestructionObserver` 的遗漏。
2. **EnumValueToCssString 反查表的扩展性**：当前用 `constexpr const char* table[]`，13 类 enum 共约 80 条字符串。如新增 enum 类型（如未来的 `cursor` / `text-decoration`），应在同 `.cc` 内追加 case 与 table，并补一组测试。维护成本可控。
3. **DomBindings 的 listener 索引数据结构**：当前 `Vector<JsListenerEntry>` 线性查找，对每元素 listener 数量小（< 10）场景 OK；如未来支持大量 listener，可引入 `HashMap<(Element*, type, JSValuePtr), Token>`。

---

## 安全评估

| 维度 | 状态 | 备注 |
|------|------|------|
| 输入验证 | ✅ | `EnumValueToCssString` 越界 / 未知 PropertyId 返回空 `StringView`，已测试 |
| 认证/授权 | N/A | — |
| 数据保护 | N/A | — |
| 依赖审计 | N/A | 本任务无新依赖 |
| 错误信息脱敏 | N/A | — |
| 敏感数据处理 | N/A | — |
| **内存安全（额外）** | ✅ | B6 解决了 `EventManager` 先于 `DomBindings` 析构的 UAF 风险（双向析构通知 + token 化注销） |
| **API 误用防护（额外）** | ✅ | B7 `RemoveEventListenerByToken` 处理 `kInvalidListenerToken` 无操作，未知 token no-op |

**结论**：本任务无敏感数据/认证场景，但 B6 修复了一类潜在内存安全问题（反向析构 UAF），属于安全改进。

---

## 总结

TASK-20260419-01 是一次"流程沉淀 + 技术债收口"的教科书式 Level 3 任务：

- **Part A**（11 提交）把 14 条 P1 散落待办收敛到 4 个 .mdc/MB 文件，新增 `integration-testing.mdc` 独立技能。
- **Part B**（8 提交）严格 TDD 修复 3 条 P2 功能债（CSS Enum 序列化 + EventManager 反向析构安全 + 精确 removeEventListener），全部 API 兼容、零警告、零回归。
- **Phase 8**（2 提交）完整最终验证 890/890 + Memory Bank 收尾，待办列表从 22 条收敛至 2 条长期项。

21 个计划提交 / 21 个实际提交、+22 测试预估 / +34 实际、~4h 预估 / ~4h 实际，三项核心指标均落在或优于预估区间。两个 P0/P1 反复模式（前置依赖、非默认路径）通过本次新增规则继续收口。
