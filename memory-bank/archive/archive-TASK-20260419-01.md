# 归档：流程规则沉淀 + P2 功能技术债收口

**日期：** 2026-04-19
**任务 ID：** TASK-20260419-01
**复杂度级别：** Level 3（中等功能 / 跨「.cursor 规则」+「Script / CSS / Event」3 个子系统）
**状态：** ✅ 已完成
**分支：** `feature/TASK-20260419-01-rules-and-debt`（基线 `main`）
**工作流路径：** `/van` → `/plan` → `/build` → `/reflect` → `/archive`（无 `/creative`）

## 任务概述

把 `activeContext.md` 累积的 22 条 P1/P2 待办按性质分两块一次性收口：

- **Part A — 流程规则沉淀（14 条 P1，纯 .mdc/MB 文档）**：把 TASK-20260413-01 ~ TASK-20260418-01 反思中沉淀的规则缺口固化到对应技能文件，新增 `integration-testing` 独立技能。
- **Part B — P2 功能技术债（3 条，含代码与 TDD 测试）**：补齐 TASK-20260418-01 遗留的 3 处功能债（CSS Enum 序列化 / `EventManager` 反向析构 / `removeEventListener` 精确移除）。

收口目标：`activeContext.md` 待办列表 ≤ 5 条；现有 856 测试零修改通过；编译零新警告。

## 技术方案

### 设计决策（头脑风暴阶段全部定案，无需 `/creative`）

| 子项 | 方案 | 模式来源 | 理由 |
|------|------|---------|------|
| **A3** 集成测试规范 | 新建独立技能 `.cursor/rules/skills/integration-testing.mdc` | — | 内容 6 条铁律 + API 备忘清单，与 `writing-plans.mdc` / `subagent-development.mdc` 同级，便于 main.mdc 表格化引用 |
| **B5** Enum 反查 | 完整反查表 `enum_serialization.{h,cc}`，覆盖 13 类 Enum | 枚举 + 元数据表驱动模式（`property.cc` 同构） | 表驱动 O(1) 查找；新增 enum 仅需追加 case + table；`StringView` 指向 `static constexpr const char*` 字面量 |
| **B6** 反向析构 | `EventManager::AddDestructionObserver(callback) → Token` | Push 回调失效触发模式（`SetInvalidationCallback` 同构） | 析构时快照 + 顺序触发；token 化注销避免 callback 死亡的 observer；DomBindings 在 `Bind/Unbind` 注册/注销 |
| **B7** 精确移除 | `EventDispatcher::AddEventListener` 返回 `u64 ListenerToken`；新增 `RemoveByToken` API | 不透明句柄模式（与 `JSValue` 句柄风格一致） | C++ "可丢弃返回值"特性保证旧调用零修改；DomBindings 维护 `(Element*, type, JSValuePtr)→Token` 映射实现 JS spec 三种重载 |

### 架构特点

- **Part A/B 严格分序**：Part A 先合（11 提交，纯文档零代码风险），Part B 才动代码（8 提交 TDD），保证流程规则即时约束 Part B 测试。
- **API 兼容设计**：`EventDispatcher::AddEventListener` 返回类型 `void → u64` 借 C++ 调用方"可丢弃非 void 返回值"特性零成本兼容所有既有调用点。
- **TDD 严格分提交**：B5/B6 每段都有独立的 RED 提交（`caa7c6d` / `b71589e`），任何人 `git checkout RED^..RED` 都能复现"实现前测试为何失败"。
- **跨模块生命周期解耦**：B6 的 `DestructionObserver` + B7 的 `ListenerToken` 联动，让 `DomBindings` 与 `EventManager` 析构顺序无强约束。

## 实现摘要

**变更规模：** 24 文件，+3100/-67 行，22 个提交（含 `/plan` × 1、`/build` × 21、`/reflect` × 1），**890/890 测试通过**（基线 856 + 新增 34 / 0 回归 / 零新警告）。

### 文件变更

| 操作 | 文件路径 | 说明 |
|------|---------|------|
| 创建 | `.cursor/rules/skills/integration-testing.mdc` | 6 条集成测试铁律 + API 备忘清单（独立新技能） |
| 创建 | `veloxa/core/css/enum_serialization.h` | `EnumValueToCssString(PropertyId, u16) → StringView` 接口 |
| 创建 | `veloxa/core/css/enum_serialization.cc` | 13 类 CSS Enum × 反查表（display/position/flex-direction/...） |
| 创建 | `tests/core/css/enum_serialization_test.cc` | 17 个测试（13 类规范值 + 边界 + 静态存储验证） |
| 创建 | `memory-bank/reflection/reflection-TASK-20260419-01.md` | 详细回顾文档（含 5 条改进建议） |
| 创建 | `memory-bank/archive/archive-TASK-20260419-01.md` | 本文档 |
| 修改 | `.cursor/rules/skills/writing-plans.mdc` | +7 段：FetchContent C 选项审计、测试基础设施审计、边界输入清单、调用链端到端验证、管线注入点可行性、**静态库循环依赖审计**、**Web 标准 API 多重载形态清单**（最后两段为反思追加） |
| 修改 | `.cursor/rules/skills/subagent-development.mdc` | +3 段：跨模块数据格式、LayoutBox 坐标语义、并行子代理前置条件 |
| 修改 | `.cursor/rules/main.mdc` | 注册 integration-testing 技能 |
| 修改 | `memory-bank/techContext.md` | +「FetchContent 与代理」段 + StatusOr 接口表面文档化 |
| 修改 | `memory-bank/systemPatterns.md` | + u64 不透明句柄模式 + 反向析构观察者模式 + CMake 循环依赖边界补充 |
| 修改 | `veloxa/core/CMakeLists.txt` | 注册 `enum_serialization.cc` |
| 修改 | `veloxa/core/event/event_dispatcher.{h,cc}` | `ListenerToken` (u64) 类型 + `AddEventListener` 返回 token + `RemoveEventListenerByToken` |
| 修改 | `veloxa/core/event/event_manager.{h,cc}` | 镜像 token API + `~EventManager` + `AddDestructionObserver/RemoveDestructionObserver` |
| 修改 | `veloxa/script/CMakeLists.txt` | 新增 `target_link_libraries(vx_script PRIVATE vx_core)`（解决循环依赖） |
| 修改 | `veloxa/script/dom_bindings.cc` | `SerializeCssValue` 增 `PropertyId` 参数接入 `EnumValueToCssString`；`InstanceData` 新增 `em_observer_token` + `js_listener_entries`；`Bind/Unbind` 注册/注销观察者；`ElementRemoveEventListener` 三种重载实现 + `DropJsEntry`/`MaybeForgetElement` helpers |
| 修改 | `tests/CMakeLists.txt` | 注册 `css_enum_serialization_test` |
| 修改 | `tests/core/event/event_dispatcher_test.cc` | +3 测试：token 唯一性 / 精确移除 / 未知 token no-op |
| 修改 | `tests/core/event/event_manager_test.cc` | +6 测试：4 个 DestructionObserver + 2 个 token 转发 |
| 修改 | `tests/script/dom_bindings_test.cc` | +9 测试：4 个 display 序列化 + 2 个析构顺序 + 3 个精确移除 |
| 修改 | `memory-bank/{tasks,activeContext,progress}.md` | 状态推进 + 待办清理 |

### 提交序列（22 个）

```
89cfae2 docs(plan): TASK-20260419-01 design spec and implementation plan
ea36625 docs(rules): add FetchContent C subproject compile options audit
e152597 docs(rules): add test infrastructure audit section
33692f0 docs(rules): add boundary input checklist section
3caa649 docs(rules): add end-to-end call chain verification section
951f057 docs(rules): add pipeline injection point feasibility section
dc18f23 docs(rules): require explicit cross-module data format
79bcd9d docs(rules): require LayoutBox coordinate semantics
9dfd38d docs(rules): add parallel subagent prerequisites checklist
0e50064 docs(rules): add integration-testing skill (6 ironclad rules)
95c8184 docs(rules): register integration-testing skill in main
e3229fa docs(memory-bank): add FetchContent and proxy notes to techContext
e650517 docs(memory-bank): mark Part A complete; cleanup todo list
caa7c6d test(css): add failing enum_serialization tests (RED)
f6e17d8 refactor(script): wire StyleGetProp through EnumValueToCssString (#46 followup)
b71589e test(event): add failing destruction observer tests (RED)
b353190 feat(event): add destruction observer to EventManager; relax DomBindings ordering
d55772e feat(event): EventDispatcher returns ListenerToken; precise removal API
63aaefe feat(event): EventManager forwards ListenerToken; add precise removal API
ed5d455 refactor(script): removeEventListener uses ListenerToken (#47)
6f36783 docs(memory-bank): mark TASK-20260419-01 build complete; ready for /reflect
f359156 docs(reflect): add reflection for TASK-20260419-01
```

### 关键决策

1. **跳过 /creative**：3 个设计点（B5/B6/B7）均能在项目内找到同构模式（`property.cc` 表驱动 / `SetInvalidationCallback` push 失效 / `JSValue` 句柄）作为论据，头脑风暴阶段直接锁定方案，实施零返工。
2. **Part A/B 严格分序**：避免流程规则与代码变更交错合并，且让 Part B 测试在新规则约束下书写（B6/B7 测试受 `integration-testing.mdc` 6 条铁律约束）。
3. **`AddEventListener` 返回值兼容性**：`void → u64` 利用 C++ 可丢弃返回值特性，保证 856 个旧测试零修改通过。
4. **`removeEventListener` 三种重载完整支持**：0 args（清空）/ 1 arg type（按类型批删）/ 2 args type+handler（按 token 精确移除），满足 WHATWG DOM spec。
5. **token 0 哨兵语义**：`kInvalidListenerToken = 0`；未知 token 静默 no-op（与 spec 一致）。
6. **静态库循环 link 显式化**：`vx_core` ↔ `vx_script` 双向 link，让 linker 在 archive 重复扫描中解析符号；CMake 对此场景天然支持。

### 安全决策

本任务不涉及外部输入/认证/存储/部署，但有两处内部内存安全改进：

- **B6 反向析构 UAF 修复**：原 `DomBindings` 持有 `EventManager*` 但要求 host 必须先析构 `DomBindings`；现通过 `DestructionObserver` 双向通知，反向场景下 `DomBindings` 可安全将 `em_` 置空，消除潜在 UAF。
- **B7 精确移除避免误删**：JS `removeEventListener(type, handler)` 现按 token 精确匹配，避免误删同 type 其它 handler 导致的状态泄漏。

依赖审计：本任务无新依赖。

## 测试覆盖

**总计：890/890 通过（基线 856 + 34 / 0 回归 / 零新警告）。**

| 测试文件 | 新增数 | 覆盖点 |
|---------|------:|--------|
| `tests/core/css/enum_serialization_test.cc` | **17** | 13 类 Enum 规范字符串 + 越界 / 非 Enum 属性 / 未知 PropertyId / `StringView` 静态存储验证 |
| `tests/core/event/event_dispatcher_test.cc` | **3** | token 唯一性 + 精确移除（保留兄弟）+ 未知 token no-op |
| `tests/core/event/event_manager_test.cc` | **6** | DestructionObserver 4 个（触发/顺序/移除后不触发/未知 token no-op）+ token 转发 2 个 |
| `tests/script/dom_bindings_test.cc` | **9** | display 序列化 4 个（block/flex/none/inline-block）+ 双向析构 2 个 + 精确移除 3 个（保留兄弟 / 未知 handler 不动 / 单 type 批删） |

测试隔离：全部独立构造 `EventManager` / `EventDispatcher` / `DomBindings` / `QuickjsEngine`，无共享状态，0 flaky；运行时间 1.56s（< 2s）。

## 经验教训

提取自 `memory-bank/reflection/reflection-TASK-20260419-01.md`：

### 流程层

1. **"21 个提交计划 → 21 个实际提交"是可达成目标**：核心是 /plan 阶段把每个 Phase 拆成单一职责的 commit，并明确 RED/GREEN 分提交。
2. **跳过 /creative 的判据要严格**：依据是"3 个设计点都能在已有代码中找到同构模式"。如果某设计点找不到先例，应触发 /creative。
3. **Part A 先于 Part B**：流程规则一旦合入，Part B TDD 即受规则约束（B6/B7 测试受 `integration-testing.mdc` 约束就是即时回报）。
4. **三个反复模式本次完全规避** ✅ — TDD 严格度、提交粒度、测试隔离三个历史高频问题首次清零，说明前期规则改进开始见效。

### 技术层

1. **静态库循环依赖在 C++ 子库化项目中是常态**：CMake 通过 archive 重复扫描可解，但需要双向 `target_link_libraries`。本次 B5 链接错误约 10 min 排查。
2. **DestructionObserver 模式适用于"非拥有跨模块引用"**：弱引用 + 析构通知是这种场景的标准答案；token 化 add/remove 保证 observer 注销精确。
3. **u64 token 模式可复用**：除 `ListenerToken`，未来 timer / animation 等持续型句柄都可复用同模式。已入 `systemPatterns.md`。
4. **Web 标准 API 多重载需 spec 表驱动**：`removeEventListener` 三种重载在头脑风暴未细化，B7 实施时需拆 helper 返工。已固化为 `writing-plans.mdc` 必填段。

### 改进建议落实清单（reflection 5 条全部已落实）

| # | 建议 | 优先级 | 落实状态 |
|---|------|--------|---------|
| 1 | 跨子库新增符号引用前 grep link graph | P1 | ✅ 已加段到 `writing-plans.mdc` 「静态库循环依赖审计」+ 已迁入 `activeContext.md` 待办 |
| 2 | 接入 Web 标准 API 必须列全 spec 重载 | **P0** | ✅ 已加段到 `writing-plans.mdc` 「Web 标准 API 多重载形态清单」 |
| 3 | u64 不透明句柄模式入系统模式 | P2 | ✅ 已入 `systemPatterns.md` |
| 4 | StatusOr API 表面文档化 | P2 | ✅ 已入 `techContext.md` Foundation 段 |
| 5 | DestructionObserver 反向析构模式入系统模式 | P2 | ✅ 已入 `systemPatterns.md` |

## 参考文档

- 设计规格：`docs/specs/2026-04-19-rules-and-debt-design.md`
- 实现计划：`docs/plans/2026-04-19-rules-and-debt.md`
- 创意设计：（无）
- 回顾文档：`memory-bank/reflection/reflection-TASK-20260419-01.md`
- 上游归档：`memory-bank/archive/archive-TASK-20260418-01.md`（#46 / #50 续作来源）

## 后续工作（已迁移到 activeContext.md）

- **P1**：补充 Benchmark（网络恢复后，来源 TASK-01）
- **P1**：跨子库新增符号引用前 grep link graph（来源本任务反思 #1，规则已固化）
- **P2**：将 `renderer_test` / `render_integration_test` 等剩余手写像素位移断言迁到 `tests/test_pixel_utils.h`（来源 TASK-20260413-02）
