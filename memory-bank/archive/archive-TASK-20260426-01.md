# 归档：TASK-20260426-01 Layout 正确性消化（#25 + #28 + #20 + #21）

**日期：** 2026-04-30
**任务 ID：** TASK-20260426-01
**复杂度级别：** Level 4（多子系统 + 架构决策 + 多轮次 Build）
**状态：** ✅ 已完成
**分支：** `feature/TASK-20260426-01-layout-correctness`

---

## 1. 任务概述

本任务按 D1 全包 + 多轮次 Build 中间态，集中消化 4 项 Layout 正确性技术债：

1. #25 `LayoutBox` origin helper 缺失
2. #28 HTML parser 未打通 inline style 解析
3. #20 Block margin collapsing 缺失（CSS 2.1 §8.3.1）
4. #21 LayoutInline line box 模型缺失（CSS 2.1 §10.8 / §10.8.1）

目标是将布局正确性从“可运行”提升到“规范一致 + 可验证 + 可维护”，并把性能回归控制在 ±10% 退出门内。

---

## 2. 技术方案

### 2.1 方案选择

- **执行策略：** D1 全包 + R0~R5 多轮次中间态，每轮单独验收并提交
- **R3 关键算法：** `MarginChain` in-line 单 pass 累积法（避免 pre-pass 双遍开销）
- **R4 关键算法：** 显式 `LineBox` + strict 2-pass vertical-align
- **安全策略（#28）：** 三件套护栏（count cap 1000 / value length cap 8KB / 黑名单关键字）
- **性能策略：** 同窗口 stash-swap 对照，防跨时间窗 baseline 误判

### 2.2 关键实现摘要

- **R1（#25）：**
  - `layout_box.h` 增 `Point` + origin/helpers 体系
  - `layout_engine.cc` / `flex_layout.cc` 10 处坐标表达式替换
- **R2（#28）：**
  - `html/parser.cc` 增 `ApplyInlineStyleAttribute`
  - 打通 `CssParser::ParseDeclarationList` 路径并加入三件套安全护栏
  - 新增 `internal::ContainsBlacklistKeyword()` 直接可测入口
- **R3（#20）：**
  - 新建 `margin_collapse.h`（`MarginChain` POD）
  - `LayoutEngine::LayoutBlock` 重写，覆盖 sibling / collapse-through / negative / BFC root
  - 修复副产品 bug：`overflow:auto` enum 路径丢失
- **R4（#21）：**
  - 新建 `line_box.h`（`LineBox`/`LineBoxItem` POD）
  - `LayoutInline` 重写：2-pass vertical-align + half-leading + implicit strut
  - `TextMetrics` ABI 兼容扩展（ascent/descent + deprecated baseline）
  - 修正 inline 默认 width 语义（fit-content）与 inline-block explicit height 失效问题

---

## 3. 文件变更（代表性）

| 操作 | 文件路径 | 说明 |
|---|---|---|
| 创建 | `veloxa/core/layout/margin_collapse.h` | R3 `MarginChain` 协议实现 |
| 创建 | `veloxa/core/layout/line_box.h` | R4 LineBox POD 抽象 |
| 修改 | `veloxa/core/layout/layout_engine.cc` | R3/R4 核心算法重写 |
| 修改 | `veloxa/core/layout/layout_box.h` | origin helpers + collapse 状态字段 |
| 修改 | `veloxa/core/html/parser.cc` | inline style 解析接入 + 安全护栏 |
| 修改 | `veloxa/core/css/parser.cc` | `vertical-align` enum/length 双路径解析 |
| 修改 | `veloxa/core/css/style_resolver.cc` | `vertical-align` 应用 + `overflow:auto` 修复 |
| 修改 | `veloxa/core/layout/text_shaper.h` | TextMetrics ABI 兼容扩展 |
| 创建 | `tests/core/html/inline_style_test.cc` | #28 单测与反向探针 |
| 创建 | `tests/core/layout/margin_collapse_test.cc` | #20 协议单测 |
| 创建 | `tests/core/layout/line_box_test.cc` | #21 单测 + RED 探针 |
| 修改 | `tests/integration/wpt_layout_test.cc` | margin/linebox WPT 数值化集成 |
| 创建 | `memory-bank/reflection/reflection-TASK-20260426-01.md` | Level 4 全维度回顾 |

> 全分支对 main 总计：**51 files changed, 5137 insertions, 95 deletions**。

---

## 4. 关键决策

1. **R3 使用 in-line MarginChain 而非 pre-pass 双遍**
   - 保持 O(N) 主路径，性能回归可控，易插入现有 `LayoutBlock` 流程。
2. **R4 使用 strict 2-pass vertical-align**
   - 在复杂基线计算下比单 pass 更可验证，便于写 RED 探针。
3. **同窗口对照作为性能门禁唯一可靠范式**
   - 解决跨时间窗 baseline 假退化问题（+10.2% 误判纠正到 +3.2%）。
4. **TextMetrics 采取 ABI 渐进策略**
   - 保留 `baseline`，新增 `ascent/descent`，避免全链路一次性破坏。
5. **子代理 D3 在 build 阶段重评估后降级直执行**
   - creative 决策已锁定 + 单文件主修改 + 上下文连续，直执行更高效。

---

## 5. 安全决策

本任务涉及安全变更（#28）：

- 输入边界控制：
  - declaration count cap = 1000
  - 单 value 长度 cap = 8KB
- 危险关键字黑名单（大小写不敏感）：
  - `expression(`
  - `behavior:`
  - `javascript:`
- 风险处置：
  - reject 路径静默丢弃，不暴露原始输入细节
  - 无新增依赖、无认证/授权模型变更、无敏感数据落库

---

## 6. 测试覆盖

- **全量回归：** `ctest` 1029/1029 PASS
- **新增用例总计：** +78
  - R1 +9
  - R2 +24
  - R3 +26
  - R4 +19
- **WPT 数值化：**
  - margin-collapse：2 PASS + 2 SKIP-w/-rationale
  - linebox：2 PASS（Wpt006 / Wpt007）
- **反向探针：**
  - R1/R2/R3/R4 多轮 RED 反向探针，验证测试非“死代码”

---

## 7. 性能结果

- **R3：** 同窗口对照 `BM_LayoutBuildTreeFlat/64`
  - mean +3.2% / median +3.4%（远低于 ±10%）
- **R4：** 同窗口对照 `BM_LayoutBuildTreeFlat/64`
  - mean -3.6% / median +2.65%（远低于 ±10%）
- **结论：** 正确性改造没有引入不可接受性能退化。

---

## 8. 经验教训（归档提炼）

1. Bench 退化判定必须同窗口 stash-swap，对比绝对数字不可跨时间窗。
2. Layout 任务必须在 plan 阶段锁定默认值语义（fit-content vs fill-available）。
3. 涉及多坐标系算法时，creative 必须产出“单一坐标约定 + 公式表”。
4. `display -> LayoutType` 映射后的 box-model 解析必须“每路径独立执行”。
5. Mixed TDD 的 D3 回归测试应默认带反向探针。

---

## 9. 改进建议闭环（P0/P1）

| 建议 | 级别 | 归档时状态 |
|---|---|---|
| 同窗口对照 bench 规则化（含 touch + 全图重建） | P0/P1 | ✅ 已落实到 `writing-plans.mdc` |
| Layout 必检项：默认值语义 + 独立 box-model | P0/P1 | ✅ 已落实到 `writing-plans.mdc` |
| Creative 坐标约定一图 | P0 | ✅ 已落实到 `.cursor/commands/creative.md` |
| 子代理 D3 build 阶段重评估机制 | P1 | ✅ 已落实到 `subagent-development.mdc` |
| 反向探针强制条目 | P1 | ✅ 已落实到 `writing-plans.mdc` |

---

## 10. 参考文档

- 设计规格：`docs/specs/2026-04-26-layout-correctness-design.md`
- 实现计划：`docs/plans/2026-04-26-layout-correctness.md`
- 创意设计：
  - `memory-bank/creative/creative-margin-collapsing.md`
  - `memory-bank/creative/creative-line-box-model.md`
- 回顾文档：`memory-bank/reflection/reflection-TASK-20260426-01.md`

