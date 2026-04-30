# 进度记录

## 当前任务

**TASK-20260430-02：CSS border shorthand 补全（4 方向 + 3 属性级）[安全相关]** — Level 2

- 阶段：🟢 构建完成（2026-04-30 22:25）— R1+R2 全 7 shorthand TDD 闭环 + 双反向探针 + Release `-O3 -Werror` 0 err/warn；Debug ctest 1061/1061 PASS（基线 1039 + R1 10 + R2 12）；Release ctest 1030/1030 PASS；5 commits（PLAN docs / R1 RED / R1 GREEN / R2 RED / R2 GREEN）+ 1 finalize；下一步 `/reflect`
- 范围（V1=A）：全量 7 shorthand — R1 = `border-top` / `-right` / `-bottom` / `-left` 4 方向；R2 = `border-width` / `border-style` / `border-color` 3 属性级
- 拆分（V2）：多轮次 Build R1 → R2
- 复杂度（V4）：Level 2
- 安全（V5）：✅ 是（CSS parser declaration 展开 N-cap 护栏 + 上游 HTML inline style 三件套护栏交互验证）
- 分支：`feature/TASK-20260430-02-css-border-shorthand`（基于 main `6b36c87`）
- 来源：TASK-20260430-01 archive §改进建议「副发现 P3 触发型候选 — CSS parser `border-bottom` shorthand 缺失」直接落实
- 双重价值：
  1. 直接消化 TASK-30-01 build 副发现 P3 候选
  2. 自我应用 TASK-30-01 升级规则 §0「CSS shorthand 能力 grep 表」（首次外部任务 ROI 验证样板）

### VAN 阶段 grep 实证摘要

- F1: `border` 总称 shorthand ✅ 已支持（`parser.cc:517-597`）
- F2: 4 方向 shorthand ❌ 完全无实现 — TASK-30-01 副发现完全确认
- F3: 3 属性级 shorthand ❌ 完全无实现 — 一并补全
- F4: PropertyId 枚举无需扩展 — 直接展开为现有 longhand
- F5: 测试 fingerprint 充足 — `ShorthandPadding` + `BorderShorthand` 双范本 100% 复用
- F6: FetchContent `_deps/` 已离线，跳过 proxy 守卫

### PLAN 阶段决策与威胁建模摘要

- D1 = A 复制粘贴既有 `border` 模板 4 次（R1 实施模式）
- D2 = A 复制粘贴既有 `padding/margin` 模板 3 次（R2 实施模式）
- D3 = C 完整档（25 测试：12 R1 + 13 R2，含双入口 + 反向探针 + 安全 N-cap）
- D4 = A 完全复用既有 N-cap（3-iter for 方向 / 4-iter for 属性级），上游三件套覆盖 T1-T5
- D5 = A 2 GREEN commits（R1 一起绿 / R2 一起绿）

威胁建模 T1-T8：上游 HTML inline style 三件套（kInlineStyleMaxValueLength=8KB / kInlineStyleMaxDeclarationCount=1000 / ContainsBlacklistKeyword）覆盖 T1-T5；既有 parser 内部 N-cap 复用覆盖 T6/T8；`EqualsIgnoreCase` 严格相等覆盖 T7。零新护栏需求。

### PLAN 阶段文档落盘

- 设计 spec：`docs/specs/2026-04-30-css-border-shorthand-design.md`
- 实现 plan：`docs/plans/2026-04-30-css-border-shorthand.md`
- Phase 划分：P0 + R1.1/1.2/1.3 + R2.1/2.2/2.3 + P3（plan ~170 min / plan×0.6 ~102 min 准确档预期）
- commits 预期：5（R1 RED + R1 GREEN + R2 RED + R2 GREEN + finalize）

### BUILD R1 完成（2026-04-30 22:05）

#### P0：准备

- §0 batch grep G1-G6 全部命中 ✅
- §0 fingerprint 锁定：parser_test.cc:321 `ASSERT_EQ(declarations.size(), 12u)`
- ctest 基线 1039/1039 PASS

#### R1.1 RED（commit `f8ed62f`）

10 个新测试落地 / 验证：
- 9 RED: `Border{Top,Right,Bottom,Left}Shorthand_FullValue` + `BorderTopShorthand_{PartialNoColor,OutOfOrder,Important,DualEntry,NCapSecurity}` 全 FAIL
- 1 T7 sentinel: `BorderTopWidthLonghand_NotShorthandPath` PASS（pre/post 不退化）
- 既有 `BorderShorthand` fingerprint PASS（12 declaration 不退化）

#### R1.2 GREEN（commit `5378e67`）

- parser.cc 插入 4 方向单分支聚合（与既有 margin/padding 模式一致），+96 LOC
- 解析循环 3-iter cap（T6 N-cap 复用既有 border 模式）
- 末尾 PropertyId 三元组按 name 映射
- 10 R1 测试全 PASS
- ctest 全量 **1049/1049 PASS**（基线 1039 + R1 新增 10）

#### R1.3 §9.3 反向探针（无 commit，三态切换）

- 破坏：`border-top` 分支 `pid_w = kBorderBottomWidth` mis-route → `BorderTopShorthand_FullValue` FAIL ✅
  实测证据：`declarations[0].property = kBorderBottomWidth(26) ≠ kBorderTopWidth(24) 期望`
- 恢复：还原映射 → `BorderTopShorthand_FullValue` + `BorderBottomShorthand_FullValue` 双 PASS ✅
- 工作树干净，证明探针完整三态切换、测试有真实判别能力

### BUILD R2 完成（2026-04-30 22:20）

#### R2.1 RED（commit `e2688a7`）

12 个新测试落地：
- 11 RED: border-{width,style,color} × {1,2,3,4,InvalidIdent,Important,DualEntry,NCapSecurity} 矩阵
- 1 sentinel: `BorderStyleShorthand_InvalidIdentRejected` (kUnknown 路径返回 0 decl 偶合期望)
- 既有 BorderShorthand fingerprint PASS 不退化

#### R2.2 GREEN（commit `1761b4f`）

- parser.cc 插入 3 属性级单分支聚合（仿既有 padding/margin 单分支聚合 + 新 Mode enum 分派 value parser），+116 LOC
- 解析循环 4-iter cap（T8 N-cap 复用既有 padding/margin 模式）
- 1-4 值展开规则与 padding/margin 完全一致
- !important 透传 4 declaration
- 12 R2 测试全 PASS
- ctest 全量 **1061/1061 PASS**（基线 1039 + R1 10 + R2 12）

#### R2.3 §9.3 反向探针（无 commit，三态切换）

- 破坏：`border-width` 2-value 展开规则 `top=values[1]; right=left=values[0]` 语义错位 → `BorderWidthShorthand_TwoValues` FAIL ✅
- 恢复：还原 `top=values[0]; right=values[1]` → R2 4 个 1-4 值测试全 PASS ✅
- 工作树干净

### P3 finalize（2026-04-30 22:25）

- Release `-O3 -Werror` 全量构建：0 err / 0 warn ✅（A6 通过）
- Release ctest **1030/1030 PASS**（既有 build matrix 与 Debug 1061 差异为既有非本任务相关）
- A1-A8 验收全部通过：

| # | 验收项 | 结果 |
|:-:|---|---|
| A1 | 4 方向 shorthand 双入口解析正确 | ✅ R1 10 测试全 PASS（含 DualEntry）|
| A2 | 3 属性级 shorthand 1-4 值规则同 padding/margin | ✅ R2 12 测试全 PASS |
| A3 | 既有 BorderShorthand 测试不退化 | ✅ ctest 1061+ 全 PASS |
| A4 | DoS / 病态输入护栏 T6/T8 | ✅ NCapSecurity 双测试 PASS |
| A5 | §9.3 反向探针 ≥ 2 处 | ✅ R1.3 + R2.3 双探针完整三态 |
| A6 | Release -O3 -Werror 0 err/warn | ✅ |
| A7 | ctest 全量 PASS | ✅ Debug 1061/1061 + Release 1030/1030 |
| A8 | TASK-30-01 §0 升级规则首次外部 ROI | 待 reflection 评估 |

### 实测耗时（plan × 0.6 第 15 数据点）

- Plan 估时：170 min（plan）/ 102 min（×0.6）
- 实测：~37 min（21:48 BUILD 启动 → 22:25 finalize 完成）
- 比例：实测 ÷ plan = 0.22×（远低于 plan×0.6 准确档预期 0.6×；接近 TASK-30-01 P6 的 0.21× 极速档）
- 推测原因：D1+D2=A 复制粘贴决策 + 既有 border / padding-margin 范本 100% 同模式 + 单分支聚合（仿 margin/padding 既有模式）大幅减少代码 + R2 全程 0 意外

## 最近归档完成

- **TASK-20260430-01：first/last child margin collapse with parent（CSS 2.1 §8.3.1）** — Level 3 ✅
  - 归档文档：`memory-bank/archive/archive-TASK-20260430-01.md`
  - 4 改进建议全部落实（P0 → `writing-plans.mdc` §9.4；P1×3 → `writing-plans.mdc` §0 + `creative.md` §d.2；P2 → `systemPatterns.md` 5 段）
- **TASK-20260426-01：Layout 正确性消化（#25 + #28 + #20 + #21）** — Level 4 ✅
- **TASK-20260425-01：SDL2 窗口后端 + 输入事件桥接** — Level 3 ✅

## 说明

详细实现过程、测试细节与经验教训均已沉淀至各任务的 `archive-`* / `reflection-*` 文档。
