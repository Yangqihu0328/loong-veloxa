# 进度记录

## 当前任务

**TASK-20260430-01：first/last child margin collapse with parent（CSS 2.1 §8.3.1 嵌套规则）** — Level 3

- 阶段：🟡 规划中（PLAN 完成 2026-04-30 19:50）
- 范围：A 子项 only（first/last child collapse with parent；不动 clearance/float）
- 直接验证：wpt-005 SKIP → PASS
- 分支：`feature/TASK-20260430-01-margin-collapse-parent`（基于 main `a84d30d`）
- 设计 spec：`docs/specs/2026-04-30-margin-collapse-with-parent-design.md`
- 实现 plan：`docs/plans/2026-04-30-margin-collapse-with-parent.md`
- 决策：D1=A1 (LayoutBlockChild 专用辅助) / D2=A (by-value POD) / D3=A (完整规范子集) / D4=B (10 单测 + 3 反向探针) / D5=B (7 Phase)
- 估时：~6.5h plan / ~3.9h plan×0.6（第 14 数据点）
- 下一步：`/build`（不需要 /creative）

## 最近归档完成

- **TASK-20260426-01：Layout 正确性消化（#25 + #28 + #20 + #21）** — Level 4 ✅
  - 归档文档：`memory-bank/archive/archive-TASK-20260426-01.md`
  - 回顾文档：`memory-bank/reflection/reflection-TASK-20260426-01.md`
  - 分支：`feature/TASK-20260426-01-layout-correctness` 已合并到 `main`
  - 关键验证：ctest `1029/1029 PASS`；同窗口对照 bench `mean -3.6% / median +2.65%`
  - 安全结果：#28 inline style 路径三件套护栏落地
- **TASK-20260425-01：SDL2 窗口后端 + 输入事件桥接** — Level 3 ✅
  - 归档文档：`memory-bank/archive/archive-TASK-20260425-01.md`
- **TASK-20260424-04：SoftwareCanvas::DrawText 真路径 warm 残余优化** — Level 2 ✅
  - 归档文档：`memory-bank/archive/archive-TASK-20260424-04.md`

## 说明

详细实现过程、测试细节与经验教训均已沉淀至各任务的 `archive-`* / `reflection-*` 文档。