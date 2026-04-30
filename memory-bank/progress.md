# 进度记录

## 当前任务

**TASK-20260430-01：first/last child margin collapse with parent（CSS 2.1 §8.3.1 嵌套规则）** — Level 3

- 阶段：🔨 构建完成（2026-04-30 21:00）— P0-P6 全 7 Phase ✅
- 范围：A 子项 only（first/last child collapse with parent；不动 clearance/float）
- 直接验证：✅ wpt-005 SKIP → PASS（`Wpt005_NonSiblingAdjoiningMarginsCollapse`）
- 分支：`feature/TASK-20260430-01-margin-collapse-parent`（基于 main `a84d30d`）
- 设计 spec：`docs/specs/2026-04-30-margin-collapse-with-parent-design.md`
- 实现 plan：`docs/plans/2026-04-30-margin-collapse-with-parent.md`
- 决策：D1=A1 (LayoutBlockChild 专用辅助) / D2=A→实施时调整为 in-out by-pointer (跨函数多级 propagate 必需) / D3=A (完整规范子集) / D4=B (10 单测 + 3 反向探针) / D5=B (7 Phase)
- 估时：~6.5h plan / ~3.9h plan×0.6（第 14 数据点）
- 下一步：`/reflect`（回顾本任务，沉淀经验教训；首次外部任务验证 TASK-26-01 P0/P1 升级规则 ROI）

### Build 阶段成果（截至 2026-04-30 21:00）

| Phase | 状态 | 核心产出 |
|:-:|:-:|---|
| P0 | ✅ | 基线 ctest 1029/1029；wpt-005 期望布局数值手算 |
| P1 | ✅ | RED 10 单测 + `MarginCollapseLayoutTest::A8_*` 全套（包含 5 阻断条件 + 3 递归/collapse-through） |
| P2 | ✅ | API 改造：`LayoutBlockChild` 专用辅助 skeleton + `margin_bottom_collapsed_into_ancestor` 状态字段 |
| P3 | ✅ | GREEN first child collapse with parent（`LayoutBlock` → wrapper + `LayoutBlockChild` 主逻辑） |
| P4 | ✅ | GREEN last child collapse + 4 阻断条件（padding-top/border-top/BFC root/height/min-height）|
| P5 | ✅ | deep chain 递归 + collapse-through 跨边界 + §9.3 反向探针 3/3 验证 |
| P6 | ✅ | wpt-005 SKIP→PASS + 同窗口 stash-swap bench A6/A7 全 PASS + Release `-O3 -Werror` 全量 rebuild + MB 同步 |

**关键测试结果：**
- ctest **1039/1039** PASS（+10 cases vs baseline 1029；Debug + Release `-O3 -Werror`）
- 仅 `Wpt001_HorizontalMarginNotCollapse` SKIP（已知未实现 horizontal margin，与本任务无关）
- 同窗口 stash-swap bench（a84d30d → HEAD）：median ~+5%，A6/A7 ≤ +10% 全 PASS
- Bench 优化：O(N) end-of-loop scan → O(1) running pointer (`last_in_flow_block` hoisting)
- 实施时设计调整：D2 「by-value in/by-value out」→ 「by-pointer in (in-out) / by-value out」以支持多级跨函数 propagate
- 实施时新增：隐式 BFC root 识别（document root + html/body 顶层 wrapper）
- 副发现：CSS parser 不处理 `border-bottom` shorthand → 等价测试改用 `padding-bottom`，主任务范围外（独立技术债）

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