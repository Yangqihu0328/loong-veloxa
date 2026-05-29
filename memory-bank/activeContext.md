# 活跃上下文

## 当前阶段

**空闲** — 等待新任务。使用 `/van` 初始化下一任务。

**上一任务：** [TASK-20260529-01 G1.6 FillPath via libtess2](archive/archive-TASK-20260529-01.md) — ✅ 闭环归档（10/10 ctest / Matrix C 1385 +10）。

---

## 下一推荐任务

| 优先 | 候选任务 | MVP 档 | Level |
|:-:|---|:-:|:-:|
| **1** | **G1.7 Stroke*** | MVP-C 核心 | L3 |
| 2 | G1.8 GlyphAtlas + DrawText 部分 | MVP-C 核心 | L3 |
| 3 | R9 EventManager HitTest 改造 | MVP-C | L2-3 |
| **元** | 工作流元任务批量落地（P1×2 + P2×10）| 工作流 | L2 |

详见 [`docs/plans/2026-05-05-gles-renderer-blueprint.md`](../docs/plans/2026-05-05-gles-renderer-blueprint.md) §3.7 + [`docs/specs/2026-05-04-mvp-scope.md`](../docs/specs/2026-05-04-mvp-scope.md) §11.2。

---

## 待处理事项 — 跨任务沉淀（按优先级）

### P1 — 来自 TASK-20260529-01 Reflect

- **P1 #3** FetchContent C 依赖 checklist 补 `enable_language(C)` — `writing-plans.mdc` / ~10 min
- **P1 #4** GLES Bezier 像素测 plan 模板补解析采样坐标 — ~10 min

### 留下次工作流元任务批量落地（P1 / 累计 2 项）

- **P1 #1** `writing-plans.mdc` LOC 表格密度系数子条 — ~10 min
- **P1 #2** `writing-plans.mdc` Phase 0 ctest baseline fingerprint 协议 — ~15 min

### 长期沉淀（P2 — 累计 12 项）

- **P2 #11** libtess2 `_deps` 离线预置文档 — `techContext.md`
- **P2 #12** T10 winding rule 探针 deferred — G1.7 或 debug 任务

---

## 最近归档（速查）

- [`archive-TASK-20260529-01.md`](archive/archive-TASK-20260529-01.md) — G1.6 FillPath via libtess2（2026-05-29）
- [`archive-TASK-20260528-01.md`](archive/archive-TASK-20260528-01.md) — G1.5 FillRect + FillRoundedRect（2026-05-28）
- [`archive-TASK-20260507-01.md`](archive/archive-TASK-20260507-01.md) — G1.4 GLESCanvas 骨架（2026-05-08）
