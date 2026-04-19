# 活跃上下文

## 当前阶段
构建中

## 当前任务

**TASK-20260419-01** — 流程规则沉淀 + P2 功能技术债收口

- **复杂度：** Level 3
- **分支：** `feature/TASK-20260419-01-rules-and-debt`（基线 `main`）
- **创建日期：** 2026-04-19
- **设计规格：** `docs/specs/2026-04-19-rules-and-debt-design.md` ✅
- **实现计划：** `docs/plans/2026-04-19-rules-and-debt.md` ✅
- **范围：** Part A 落实 14 条 P1 流程规则改进；Part B 修复 P2 三条功能债

## 关键决策（已固化）

| 子项 | 决策 |
|------|------|
| Part A/B 顺序 | A 先 B 后；Part A 内部 A1→A2→A3→A4 顺序，Part B 内部 B5（独立）→ B6（基础设施）→ B7（业务接入） |
| 集成测试规范位置 | 新建 `.cursor/rules/skills/integration-testing.mdc`（独立技能） |
| B5 Enum 反查 | 新建 `veloxa/core/css/enum_serialization.{h,cc}`，覆盖全部 13 个 Enum |
| B6 反向析构 | EventManager `AddDestructionObserver` 回调（push 失效模式同构） |
| B7 精确移除 | `EventDispatcher::AddEventListener` 返回 u64 token；新增 `RemoveByToken` API |
| 子代理使用 | 不使用（Part A 主线程更可靠；Part B 单 Phase 代码量小） |

## 是否需要 /creative

**否。** 头脑风暴阶段已固化全部 3 个设计点的方案。直接 `/build`。

## 验收标准

- 全量测试 ≥ 878 通过（基线 856 + 新增 22）
- `activeContext.md` 待办列表 ≤ 5 条
- API 兼容：现有 856 测试零修改通过
- 编译零新警告

## 最近归档

`memory-bank/archive/archive-TASK-20260418-01.md`（TASK-20260418-01 消化关键技术债务，已合并到 main）

## 待处理事项

### 本任务进行中（Part B，TASK-20260419-01）
- **P2**：`StyleGetProp` Enum 读路径（display 等）——需要 `PropertyId→enum string` 反查表（来源 TASK-20260418-01 #46，本任务 B5）
- **P2**：`DomBindings`/`EventManager` 析构顺序硬约束——反向场景需弱引用机制（来源 TASK-20260418-01 #50，本任务 B6）
- **P2**：`removeEventListener` 按 `(type, handler)` 精确移除，需 `EventManager` 扩展 API（原 #47，本任务 B7）

### 长期项（不在本任务范围）
- **P1**：补充 Benchmark（网络恢复后，来源 TASK-01）
- **P2**：将 `renderer_test` / `render_integration_test` 等剩余手写像素位移断言迁到 `tests/test_pixel_utils.h`，并在该头注释示例 hex→RGBA（来源 TASK-20260413-02）

### 已落实（本任务 Part A 完成）
- ~~14 条 P1 流程规则~~ ✅ 已固化到 `writing-plans.mdc` / `subagent-development.mdc` / 新建 `integration-testing.mdc` / `techContext.md`
