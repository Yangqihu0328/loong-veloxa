# first/last child margin collapse with parent 实现计划

**目标：** 实施 W3C CSS 2.1 §8.3.1 中跨函数边界的 `first-child margin-top with parent margin-top` 与 `last-child margin-bottom with parent margin-bottom` 两条 adjoining 规则；wpt-005 SKIP → PASS。

**架构：** 引入 `LayoutBlockChild` 专用静态辅助函数（D1.A1）；by-value `MarginChain` in / out（D2.A）；在 `LayoutBlock` 主循环对 `kBlock` 子改为递归调用 `LayoutBlockChild` 透传 chain。LayoutInline / LayoutFlex / LayoutChild 行为与签名零变更（§9.1 跨 LayoutType 独立 box-model）。

**技术栈：** C++17 / CMake / GoogleTest / Google Benchmark / W3C wpt CSS2 fixture。

**复杂度级别：** Level 3（单子系统 Layout + API 设计决策 + 跨函数 chain propagate）

**任务 ID：** TASK-20260430-01
**分支：** `feature/TASK-20260430-01-margin-collapse-parent`（基于 main `a84d30d`）

**关联文档：**
- 设计规格：`docs/specs/2026-04-30-margin-collapse-with-parent-design.md`
- 上游归档：`memory-bank/archive/archive-TASK-20260426-01.md` §10 P3 占位
- 上游回顾：`memory-bank/reflection/reflection-TASK-20260426-01.md` §4.12

---

## Phase 0：准备阶段

### 任务 0.1：grep §8.3.1 当前实现状态 fingerprint

**文件：** 仅读

- [ ] **步骤 1：grep 验证 fingerprint**

  ```bash
  # F1: 当前 LayoutBlock 内 chain 处理位置
  rg "MarginChain|cur_chain|outgoing|incoming" veloxa/core/layout/layout_engine.cc -n
  # 期望：5 处 R3 实施点；零跨函数 propagate

  # F2: LayoutChild callers
  rg "LayoutChild\(" veloxa/core/layout/ -n
  # 期望：3 处（line 465 / 596 / 729）

  # F3: 既有 wpt-005 SKIP rationale
  rg "Wpt005|NonSiblingAdjoiningMargins" tests/integration/wpt_layout_test.cc -n -B2 -A20

  # F4: smoke 工具链可用性
  which jq bc python3 rg

  # F5: bench baseline 当前数值
  ls benchmarks/baseline/bench_layout_*.json
  ```

- [ ] **步骤 2：记录 fingerprint 命中数到 progress.md**

### 任务 0.2：手算 wpt-005 期望布局

**文件：** 仅笔算

- [ ] **步骤 1：拆解 fixture（`margin-collapse-005.xht`）**

  ```
  body
    p (text)
    div#div1 (height:4em, bg:url(2em-space))
      div (anonymous wrapper, no padding/border/height/min-height)
        div#div3 (margin-bottom:2em, height:1em, bg:green)
      div#div4 (margin-top:1em, height:1em, bg:green)
  ```

- [ ] **步骤 2：计算期望 y 坐标**

  - `font-size: 20px / 1em Ahem` → 1em = 20px
  - `#div3.margin-bottom: 2em = 40px`
  - 嵌套 `<div>` 无 padding/border/height/min-height → `#div3` 与外 div bottom adjoining → 渗出
  - `#div4.margin-top: 1em = 20px`
  - sibling collapse `max(40, 20) = 40px`（已 R3 完成）
  - **核心断言**：`div4.y - (div3.y + div3.border_box_height) == 40px`（而非 60px = 2em + 1em 未 collapse 的双计）

- [ ] **步骤 3：写入 progress.md 期望值表**

### 任务 0.3：基线核验 + commit spec/plan

- [ ] **步骤 1：跑 ctest 基线**

  ```bash
  cd build && ctest --output-on-failure 2>&1 | tail -5
  # 期望：1029/1029 PASS in ~2.5s（含 2 SKIP-w/-rationale）
  ```

- [ ] **步骤 2：抓取 bench baseline**

  ```bash
  cd build-bench && ./benchmarks/bench_layout_buildtree \
    --benchmark_filter="BM_LayoutBuildTreeFlat/64" \
    --benchmark_min_time=0.5s --benchmark_repetitions=3 \
    --benchmark_format=json > /tmp/bench_baseline_TASK-30-01.json
  jq '.benchmarks[] | {name, real_time}' /tmp/bench_baseline_TASK-30-01.json
  ```

- [ ] **步骤 3：commit spec + plan**

  ```bash
  git add docs/specs/2026-04-30-margin-collapse-with-parent-design.md \
          docs/plans/2026-04-30-margin-collapse-with-parent.md
  git commit -m "docs(plan): TASK-20260430-01 design + plan (D1-D5 locked)"
  ```

---

## Phase 1：RED 单测全套（D4.B 完整档）[TDD]

### 任务 1.1：扩展 block_layout_test.cc — 主流程 2 个 [TDD]

**文件：**
- 修改：`tests/core/layout/block_layout_test.cc`

- [ ] **步骤 1：写 RED 测试 #1 `FirstChildMarginCollapsesWithParent_Basic`**

  ```cpp
  TEST_F(BlockLayoutTest, FirstChildMarginCollapsesWithParent_Basic) {
    // <body><parent style="margin-top:30px"><child style="margin-top:20px;height:50px"/></parent></body>
    // R1: parent.margin_top + child.margin_top adjoining → max(30, 20) = 30px
    // 期望：parent.y == 30 (parent 自身 margin-top), child.y == 0 (相对 parent，因 chain 已被 parent 吃)
    // 验证渗出：parent 自身的 effective_margin_top == 30 (不是 30+20=50)
    auto* root = BuildBlockTree(R"HTML(
      <body>
        <div id="parent" style="margin-top:30px">
          <div id="child" style="margin-top:20px;height:50px"></div>
        </div>
      </body>
    )HTML");
    auto* parent = FindByID(root, "parent");
    auto* child = FindByID(root, "child");
    // 关键断言：child.y 应为 0（margin-top 已合入 parent 链）
    EXPECT_FLOAT_EQ(child->y, 0.0f);
    // parent 自身 margin-top 含 child 合入：仍是 max(30, 20) = 30
    EXPECT_FLOAT_EQ(parent->y, 30.0f);
  }
  ```

- [ ] **步骤 2：写 RED 测试 #2 `LastChildMarginCollapsesWithParent_Basic`**

  ```cpp
  TEST_F(BlockLayoutTest, LastChildMarginCollapsesWithParent_Basic) {
    // <parent (height:auto, no padding/border)><child (margin-bottom:20px, height:50px)/></parent>
    // <next-sibling style="margin-top:30px;height:30px"/>
    // R3: child.margin_bottom 渗出到 parent.margin_bottom
    // R2: parent.margin_bottom (= max(20, parent_own_mb)) 与 next.margin_top (30px) collapse
    // 期望：next.y - (parent.y + parent.border_box_height) == max(20, 30) = 30
    auto* root = BuildBlockTree(R"HTML(
      <body>
        <div id="parent" style="height:auto">
          <div id="child" style="margin-bottom:20px;height:50px"></div>
        </div>
        <div id="next" style="margin-top:30px;height:30px"></div>
      </body>
    )HTML");
    auto* parent = FindByID(root, "parent");
    auto* next = FindByID(root, "next");
    f32 gap = next->y - (parent->y + parent->border_box_height());
    EXPECT_FLOAT_EQ(gap, 30.0f);
  }
  ```

- [ ] **步骤 3：跑测试验证 FAIL**

  ```bash
  cd build && cmake --build . --target block_layout_test -j && \
    ./tests/block_layout_test --gtest_filter="*FirstChildMargin*:*LastChildMargin*"
  # 期望：2 FAIL（实现未完成；当前 child.y 含未 collapse 的 margin_top）
  ```

- [ ] **步骤 4：commit**

  ```bash
  git add tests/core/layout/block_layout_test.cc
  git commit -m "test(layout): TASK-20260430-01 P1.1 RED tests for first/last child main flow"
  ```

### 任务 1.2：阻断条件 5 个 [TDD]

**文件：** `tests/core/layout/block_layout_test.cc`

- [ ] **步骤 1：写 RED 测试 #3-#7（5 个阻断场景）**

  - `PaddingTop_BlocksFirstChildCollapse`：`parent style="padding-top:1px;margin-top:30px"` + child margin-top:20px → child.y == 1+20 = 21（无 collapse），parent.y == 30
  - `BorderBottom_BlocksLastChildCollapse`：`parent style="border-bottom:1px solid;height:auto"` → child.margin-bottom 不渗出
  - `BFCRoot_BlocksBothCollapse`：`parent style="overflow:hidden"` → 上下都阻断
  - `ExplicitHeight_BlocksLastChildCollapse`：`parent style="height:80px"` → last child margin-bottom 不渗出
  - `MinHeightNonZero_BlocksLastChildCollapse`：`parent style="min-height:50px;height:auto"` → 不渗出

- [ ] **步骤 2：跑测试验证 5 FAIL**

- [ ] **步骤 3：commit**

  ```bash
  git commit -am "test(layout): TASK-20260430-01 P1.2 RED tests for 5 blocking conditions"
  ```

### 任务 1.3：递归 + collapse-through 3 个 [TDD]

- [ ] **步骤 1：写 RED 测试 #8-#10**

  - `DeepCollapseChain_NestedFirstChildren`：`grandparent > parent > child` 三层全无 padding/border，三方 margin-top 通过 chain 合并到 grandparent.margin-top
  - `CollapseThrough_BoxPropagatesAcrossLevels`：collapse-through box 把 chain 跨边界传递（含 collapsed_through 状态字段验证）
  - `LastChild_OutgoingChainContainsParentMarginBottom`：验证 outgoing chain 含 parent.margin-bottom（用 sibling 做断言基准）

- [ ] **步骤 2：跑测试验证 3 FAIL**

- [ ] **步骤 3：commit**

  ```bash
  git commit -am "test(layout): TASK-20260430-01 P1.3 RED tests for deep chain + collapse-through"
  ```

---

## Phase 2：API 改造 + dispatch [TDD]

### 任务 2.1：加 LayoutBlockChild 签名 + 状态字段

**文件：**
- 修改：`veloxa/core/layout/layout_engine.h`
- 修改：`veloxa/core/layout/layout_box.h`
- 修改：`veloxa/core/layout/layout_engine.cc`

- [ ] **步骤 1：加 layout_engine.h 私有静态方法签名**

  ```cpp
  // 在 LayoutEngine 类的 private 段
  static MarginChain LayoutBlockChild(LayoutBox* box, f32 containing_width,
                                       const LayoutContext& ctx,
                                       MarginChain incoming);
  ```

- [ ] **步骤 2：加 layout_box.h 状态字段**

  ```cpp
  // 在 LayoutBox struct（已有 collapsed_through 与 margin_top_collapsed_into_ancestor）
  bool margin_bottom_collapsed_into_ancestor = false;
  ```

- [ ] **步骤 3：layout_engine.cc 加 stub 实现（先调通编译）**

  ```cpp
  MarginChain LayoutEngine::LayoutBlockChild(LayoutBox* box, f32 containing_width,
                                              const LayoutContext& ctx,
                                              MarginChain incoming) {
    (void)incoming;  // 暂不使用，P3-P4 实施
    LayoutBlock(box, containing_width, ctx);
    return MarginChain{};  // stub：暂返回 empty
  }
  ```

- [ ] **步骤 4：编译验证（不应破坏既有测试）**

  ```bash
  cd build && cmake --build . -j 2>&1 | tail -10
  ./tests/block_layout_test --gtest_filter="*Wpt001*:-*FirstChildMargin*:-*LastChildMargin*:-*Padding*:-*Border*:-*BFC*:-*Explicit*:-*MinHeight*:-*Deep*:-*CollapseThrough*:-*LastChild_*"
  # 期望：既有 R3 测试全 PASS（10 个新 RED 仍 FAIL）
  ```

- [ ] **步骤 5：commit**

  ```bash
  git commit -am "feat(layout): TASK-20260430-01 P2 LayoutBlockChild API skeleton"
  ```

---

## Phase 3：GREEN first child collapse [TDD]

### 任务 3.1：实现 first child margin-top collapse with parent

**文件：** `veloxa/core/layout/layout_engine.cc`

- [ ] **步骤 1：抽离 LayoutBlock 主体到 LayoutBlockChild + 接入 incoming chain**

  关键改动：

  1. `LayoutBlock(box, w, ctx)` 转调 `LayoutBlockChild(box, w, ctx, MarginChain{})` 后丢弃返回值
  2. `LayoutBlockChild` 主体接收原 LayoutBlock 全部逻辑
  3. 在 box-model 解析后计算 `blocks_top`：

     ```cpp
     const bool blocks_top = box->padding[LayoutBox::kTop] != 0.0f
                          || box->border[LayoutBox::kTop] != 0.0f
                          || CreatesBlockFormattingContext(box);

     MarginChain cur_chain;
     if (!blocks_top) {
       cur_chain = incoming;
       cur_chain.Add(box->margin[LayoutBox::kTop]);
       box->margin_top_collapsed_into_ancestor = true;
     }
     // 否则 cur_chain = empty；box.margin[Top] 由 caller 写 box->y
     ```

  4. children 循环里对 kBlock 子分支：

     ```cpp
     if (child->type == LayoutType::kBlock) {
       // 计算 child.x（margin-left 已在 LayoutChild 解析）
       child->x = child->margin[LayoutBox::kLeft];
       // child.y 必须基于当前 chain（含 incoming）— 但 child 内部的 chain 处理由 LayoutBlockChild 自己负责
       // tentative：先把 chain 累积进 cur_chain，再让 LayoutBlockChild 用此 chain 计算 child.y
       MarginChain child_outgoing = LayoutBlockChild(
           child, box->content_width, ctx, cur_chain);

       // 写 child.y：基于 collapsed chain 和 y_cursor
       // ...（关键：与 R3 sibling collapse 协议保持一致）
     } else {
       LayoutChild(child, box->content_width, ctx);
       // R3 老路径
     }
     ```

  > **注意**：算法的关键复杂度在于 child.y 的写入时机。R3 中 child.y 是在 sibling collapse 路径里 `child->y = y_cursor + cur_chain.Collapsed()` 写入。本任务把 kBlock 子的 y 写入下沉到 `LayoutBlockChild` 内部（LayoutBlockChild 接 incoming chain 后用 chain.Collapsed() 写自己的 y 偏移）。

- [ ] **步骤 2：跑测试 #1**

  ```bash
  cd build && cmake --build . --target block_layout_test -j && \
    ./tests/block_layout_test --gtest_filter="*FirstChildMarginCollapsesWithParent_Basic*"
  # 期望：PASS
  ```

- [ ] **步骤 3：跑全部 R3 既有测试不退化**

  ```bash
  ./tests/block_layout_test
  # 期望：R3 既有 PASS / 新单测中 #1 PASS / 其他 9 个仍 FAIL
  ```

- [ ] **步骤 4：commit**

  ```bash
  git commit -am "feat(layout): TASK-20260430-01 P3 first child margin-top collapse with parent"
  ```

---

## Phase 4：GREEN last child collapse + 4 阻断条件 [TDD]

### 任务 4.1：last child margin-bottom outgoing 路径 + height 阻断

**文件：** `veloxa/core/layout/layout_engine.cc`

- [ ] **步骤 1：在 LayoutBlockChild 末尾加 outgoing chain 计算**

  ```cpp
  // 计算 blocks_bottom
  const bool blocks_bottom = box->padding[LayoutBox::kBottom] != 0.0f
                          || box->border[LayoutBox::kBottom] != 0.0f
                          || (style && !style->height.is_auto() && !style->height.is_none())
                          || HasNonZeroMinHeight(style, ctx)
                          || CreatesBlockFormattingContext(box);

  if (!blocks_bottom && any_in_flow) {
    // last child margin-bottom 已合入 cur_chain（步骤 2 完成）
    MarginChain outgoing = cur_chain;
    outgoing.Add(box->margin[LayoutBox::kBottom]);
    box->margin_bottom_collapsed_into_ancestor = true;
    return outgoing;
  }
  return MarginChain{};
  ```

- [ ] **步骤 2：实现 `HasNonZeroMinHeight` helper**

  ```cpp
  inline bool HasNonZeroMinHeight(const css::ComputedStyle* style,
                                   const LayoutContext& ctx) {
    if (!style) return false;
    if (style->min_height.is_none() || style->min_height.is_auto()) return false;
    const f32 fs = ResolveFontSize(style, ctx);
    return ResolveLength(style->min_height, 0.0f, fs, ctx) > 0.0f;
  }
  ```

- [ ] **步骤 3：跑测试 #2 + #6 + #7**

  ```bash
  ./tests/block_layout_test --gtest_filter="*LastChildMargin*:*ExplicitHeight*:*MinHeight*"
  # 期望：3 PASS
  ```

### 任务 4.2：4 阻断条件细化（padding-top / border-top 等）

- [ ] **步骤 1：测试 #3-#5 应在 P3+P4.1 中已 PASS**（因 blocks_top / blocks_bottom 实施时已含全部条件）

- [ ] **步骤 2：跑全部阻断条件测试**

  ```bash
  ./tests/block_layout_test --gtest_filter="*PaddingTop*:*BorderBottom*:*BFCRoot*"
  # 期望：3 PASS
  ```

- [ ] **步骤 3：commit**

  ```bash
  git commit -am "feat(layout): TASK-20260430-01 P4 last child collapse + 5 blocking conditions"
  ```

---

## Phase 5：反向探针 + deep chain + collapse-through 递归 [TDD]

### 任务 5.1：deep chain 递归（测试 #8）

**文件：** 实施已自然涵盖（递归调 `LayoutBlockChild` 的 chain 透传）；本任务仅验证。

- [ ] **步骤 1：跑测试 #8**

  ```bash
  ./tests/block_layout_test --gtest_filter="*DeepCollapseChain*"
  # 期望：PASS（递归调用透传 chain，无需新代码）
  ```

- [ ] **步骤 2：若 FAIL，调试 chain 在递归边界传递的 sign / order 错误**

### 任务 5.2：collapse-through 跨边界（测试 #9）

- [ ] **步骤 1：在 LayoutBlockChild 中处理 collapse-through 的 incoming + outgoing 同链**

  ```cpp
  // 若 box 自身是 collapse-through，则 incoming + 自身 top + 自身 bottom 全合入 outgoing
  // R3 已有 IsCollapseThrough 判定逻辑，本任务在 box layout 完成后回填判断
  if (IsCollapseThrough(box)) {
    box->collapsed_through = true;
    MarginChain through_chain = incoming;
    through_chain.Add(box->margin[LayoutBox::kTop]);
    through_chain.Add(box->margin[LayoutBox::kBottom]);
    return through_chain;
  }
  ```

- [ ] **步骤 2：跑测试 #9**

- [ ] **步骤 3：跑测试 #10 `LastChild_OutgoingChainContainsParentMarginBottom`**

### 任务 5.3：3 反向探针验证（§9.3 强制）

> 反向探针验证：临时改实现为 placeholder → 确认 D3 类测试 FAIL → 恢复实现 → 确认全 PASS

- [ ] **探针 1：blocks_top 检测**

  - 临时改：`const bool blocks_top = false;`
  - 跑：`./tests/block_layout_test --gtest_filter="*PaddingTop_BlocksFirstChildCollapse*"`
  - 期望：FAIL（first child 错误地 collapse 进了不该 collapse 的 parent）
  - 恢复 + 全 PASS 复查
  - 记录到 progress.md

- [ ] **探针 2：blocks_bottom 检测**

  - 临时改：`const bool blocks_bottom = false;`
  - 跑：`./tests/block_layout_test --gtest_filter="*ExplicitHeight_BlocksLastChildCollapse*"`
  - 期望：FAIL
  - 恢复 + 全 PASS 复查

- [ ] **探针 3：outgoing chain 传播**

  - 临时改：LayoutBlockChild 末尾改为 `return MarginChain{};`（永不渗出）
  - 跑：`./tests/block_layout_test --gtest_filter="*CollapseThrough*"`
  - 期望：FAIL
  - 恢复 + 全 PASS 复查

- [ ] **步骤 4：commit**

  ```bash
  git commit -am "feat(layout): TASK-20260430-01 P5 deep chain + collapse-through + 3 reverse probes"
  ```

---

## Phase 6：wpt-005 + bench 同窗口 + 收尾

### 任务 6.1：wpt-005 SKIP → PASS

**文件：** `tests/integration/wpt_layout_test.cc`

- [ ] **步骤 1：定位 Wpt005 测试**

  ```bash
  rg "Wpt005|NonSiblingAdjoining" tests/integration/wpt_layout_test.cc -n -B2 -A20
  ```

- [ ] **步骤 2：把 SKIP-w/-rationale 改为数值化断言**

  关键断言：`div4.y - (div3.y + div3.border_box_height) == 40px`（2em，单 collapse）。具体测试代码模板与既有 Wpt002/003 一致。

- [ ] **步骤 3：跑测试**

  ```bash
  ./tests/wpt_layout_test --gtest_filter="*Wpt005*"
  # 期望：PASS
  ```

- [ ] **步骤 4：跑 ctest 全量**

  ```bash
  cd build && ctest --output-on-failure 2>&1 | tail -10
  # 期望：1039+/1039+ PASS（+10 单测 + 1 wpt SKIP→PASS = +11）
  ```

- [ ] **步骤 5：commit**

  ```bash
  git commit -am "test(layout): TASK-20260430-01 P6.1 wpt-005 SKIP -> PASS"
  ```

### 任务 6.2：bench 同窗口 stash-swap 对照（§7.0.1 强制）

- [ ] **步骤 1：执行同窗口对照协议**

  ```bash
  # Step 1: stash 改动测原 baseline（用 a84d30d 上的状态）
  git stash push -m "TASK-30-01 stash for bench compare" \
    veloxa/core/layout/layout_engine.{h,cc} veloxa/core/layout/layout_box.h \
    tests/core/layout/block_layout_test.cc tests/integration/wpt_layout_test.cc

  cd build-bench && cmake --build . --target bench_layout_buildtree -j
  sleep 30  # WSL2 稳态等待
  ./benchmarks/bench_layout_buildtree \
    --benchmark_filter="BM_LayoutBuildTreeFlat/64" \
    --benchmark_min_time=0.5s --benchmark_repetitions=10 \
    --benchmark_format=json > /tmp/bench_baseline_TASK-30-01.json

  # Step 2: stash pop + 强制重编（防 ninja 时间戳跳过）
  cd .. && git stash pop
  touch veloxa/core/layout/layout_engine.cc veloxa/core/layout/layout_engine.h
  cd build-bench && cmake --build . --target bench_layout_buildtree -j
  sleep 30
  ./benchmarks/bench_layout_buildtree \
    --benchmark_filter="BM_LayoutBuildTreeFlat/64" \
    --benchmark_min_time=0.5s --benchmark_repetitions=10 \
    --benchmark_format=json > /tmp/bench_v1_TASK-30-01.json
  ```

- [ ] **步骤 2：用 python3 计算差异**

  ```bash
  python3 - <<'EOF'
  import json
  base = json.load(open('/tmp/bench_baseline_TASK-30-01.json'))
  v1 = json.load(open('/tmp/bench_v1_TASK-30-01.json'))
  for b, v in zip(base['benchmarks'], v1['benchmarks']):
      if b['name'] != v['name']: continue
      if 'aggregate_name' not in b: continue
      if b['aggregate_name'] in ('mean', 'median'):
          delta = (v['real_time'] - b['real_time']) / b['real_time'] * 100
          print(f"{b['name']:50s} {b['aggregate_name']:8s} {b['real_time']:.0f} -> {v['real_time']:.0f} ({delta:+.2f}%)")
  EOF
  ```

- [ ] **步骤 3：验收 ≤ +10%**

  - mean Δ% ≤ +10% AND median Δ% ≤ +10% → ✅
  - 否则 AskQuestion 决策（接受 / 优化 / 回退）

- [ ] **步骤 4：跑额外维度 bench 兜底**

  ```bash
  ./benchmarks/bench_layout_buildtree --benchmark_filter="BM_LayoutBuildTreeNested/16" --benchmark_repetitions=3
  ./benchmarks/bench_layout_flex --benchmark_filter="BM_LayoutFlex<8,8>" --benchmark_repetitions=3
  # 兜底：mean ≤ +10%
  ```

### 任务 6.3：Release `-O3 -Werror` 全量 rebuild 验证

- [ ] **步骤 1：清掉 build-bench artifact 强制 fresh build**

  ```bash
  cd build-bench && cmake --build . -j 2>&1 | tee /tmp/release_build.log | tail -20
  # 期望：0 err / 0 warn
  ```

- [ ] **步骤 2：跑 Release ctest**

  ```bash
  ctest --output-on-failure 2>&1 | tail -5
  # 期望：与 Debug 同 PASS 数
  ```

### 任务 6.4：Memory Bank 同步

- [ ] **步骤 1：更新 `memory-bank/techContext.md` 技术债 #20**

  在 #20 状态注释中追加：「TASK-20260430-01 完成 first/last child margin collapse with parent + 6 阻断条件；clearance 完整版仍依赖 float 实施（独立 P3 任务）」。

- [ ] **步骤 2：更新 `memory-bank/progress.md`**

- [ ] **步骤 3：更新 `memory-bank/activeContext.md`**

  阶段：`构建中` → `构建完成（待 /reflect）`

- [ ] **步骤 4：更新 `memory-bank/tasks.md`**

  状态：`🟡 初始化中` → `🟢 构建完成`；任务历史表新增 P0-P6 时间点行。

- [ ] **步骤 5：commit**

  ```bash
  git add memory-bank/ tests/integration/wpt_layout_test.cc
  git commit -m "docs(layout): TASK-20260430-01 P6 wpt-005 PASS + bench + finalize"
  ```

---

## 完成定义

- [ ] 全部 A1-A17 验收通过（spec §3）
- [ ] ctest 1039+ PASS（基线 1029 + 10 新单测 + wpt-005 PASS）
- [ ] Release `-O3 -Werror` 0 err / 0 warn
- [ ] bench 同窗口对照 mean ≤ +10% / median ≤ +10%
- [ ] 3 反向探针完整循环记录到 progress.md
- [ ] `memory-bank/techContext.md` #20 状态注释更新
- [ ] git log 显示 7 个 phase commits（可在 P5 末选择性 squash 部分）
- [ ] **下一步**：用户确认 → `/reflect` 启动回顾

---

**计划人：** AI Agent
**创建日期：** 2026-04-30
**预估总工时（plan）：** ~6.5h（plan × 0.6 ≈ 3.9h 准确档预期）
