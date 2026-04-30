# CSS border shorthand 补全 实现计划

**目标：** 补全 4 方向 + 3 属性级共 7 个 border shorthand，全部展开为既有 12 longhand。

**架构：** parser.cc `ParseDeclaration` 内 `if (prop == kUnknown)` 块新增 7 个 shorthand 分支（按字母序）；零新 PropertyId / 零 enum 改动。

**技术栈：** C++ / `vx::css::CssParser` / 既有 `ParseLengthOrPercent` / `ParseColor` / `BorderStyle` enum

**复杂度级别：** Level 2

**任务 ID：** TASK-20260430-02

**安全相关：** ✅ 是（CSS parser 内部 N-cap 复用 + 上游 HTML inline style 三件套护栏交互验证）

**关联：** [设计 spec](../specs/2026-04-30-css-border-shorthand-design.md)

---

## §0 全局约束

### 0.1 基线核验

```bash
# Debug ctest 基线
cmake --build build -j --target all 2>&1 | tail -5
cd build && ctest -j --output-on-failure | tail -5
# 期望：1039/1039 PASS（TASK-30-01 终态继承）
```

### 0.2 §0 batch grep 验证（升级规则 §0「CSS shorthand 能力 grep 表」自我应用）

| # | 验证目标 | 命令 | 期望命中 |
|:-:|---|---|---|
| G1 | 既有 `border` shorthand 范本 | `rg "EqualsIgnoreCase\(name, \"border\"\)" veloxa/core/css/parser.cc -n` | parser.cc:517 |
| G2 | 既有 `padding`/`margin` 范本 | `rg "EqualsIgnoreCase\(name, \"margin\"\)" veloxa/core/css/parser.cc -n` | parser.cc:454 |
| G3 | 12 个 longhand `kBorder{Top,Right,Bottom,Left}{Width,Style,Color}` 全部存在 | `rg "kBorder(Top\|Right\|Bottom\|Left)(Width\|Style\|Color)" veloxa/core/css/property.h -n` | 12 处 |
| G4 | 既有 `BorderShorthand` 测试范本 | `rg "TEST.*BorderShorthand\\b" tests/core/css/parser_test.cc -n` | parser_test.cc:319 |
| G5 | 既有 `ShorthandPadding` 测试范本 | `rg "TEST.*ShorthandPadding\\b" tests/core/css/parser_test.cc -n` | parser_test.cc 含 |
| G6 | DoS 上游护栏 `kInlineStyleMaxValueLength` / `kInlineStyleMaxDeclarationCount` | `rg "kInlineStyle" veloxa/core/html -n` | parser.h:13-14 |

### 0.3 §0 既有测试隐式契约 fingerprint 反向 grep（升级规则 §0 自我应用）

```bash
# 既有 BorderShorthand 测试隐含「12 declaration 严格相等」契约
rg "ASSERT_EQ.*sheet\\.rules\\[0\\]\\.declarations\\.size.*12u" tests/core/css/parser_test.cc -n
# 期望命中 1 处（parser_test.cc:321）
# → 新 shorthand 实施不能影响这条契约
```

### 0.4 工具链 grep（smoke toolchain 检查）

```bash
command -v rg && command -v cmake && command -v ctest
# 期望全部命中（基础工具）
```

### 0.5 反向探针位置预约（§9.3 强制）

| Round | 反向探针位置 | mis-route 方式 | 预期 FAIL |
|:-:|---|---|---|
| R1 | `BorderTopShorthand_FullValue` | 临时把 `border-top` 分支末尾 `kBorderTopWidth` 改成 `kBorderBottomWidth`（mis-route 到错误方向）| `BorderTopShorthand_FullValue` FAIL（width 跑到 bottom 边）|
| R2 | `BorderWidthShorthand_TwoValues` | 临时把 4-side 展开规则 size==2 改为 `top=values[1], right=left=values[0]`（语义错位）| `BorderWidthShorthand_TwoValues` FAIL |

---

## P0：准备（10 min, plan × 0.6 = 6 min）

- [ ] **步骤 1**：执行 §0 batch grep 验证 G1-G6，记录命中到 `progress.md`
- [ ] **步骤 2**：执行 §0.4 工具链 grep（rg/cmake/ctest）
- [ ] **步骤 3**：执行 ctest 基线 1039/1039 PASS 确认
- [ ] **步骤 4**：grep 既有 BorderShorthand 测试 fingerprint（§0.3）
- [ ] **步骤 5**：本任务 R1 反向探针 mis-route 位置注释到 spec §7.1（已含）

**P0 出口判据**：6 项 grep 全命中 + ctest 1039/1039 PASS + fingerprint 记录 → 进入 R1.1。

---

## R1：4 方向 shorthand（border-top / -right / -bottom / -left）

### R1.1：RED 测试 [TDD]

**文件：**
- 修改：`tests/core/css/parser_test.cc`（追加 12 个 R1 测试，详见 spec §7.1）

- [ ] **步骤 1：编写 12 R1 测试**

每测试遵循模板：

```cpp
TEST(CssParserTest, BorderTopShorthand_FullValue) {
  auto sheet = CssParser::Parse("div { border-top: 1px solid red; }");
  ASSERT_EQ(sheet.rules.size(), 1u);
  ASSERT_EQ(sheet.rules[0].declarations.size(), 3u);
  EXPECT_EQ(sheet.rules[0].declarations[0].property, PropertyId::kBorderTopWidth);
  EXPECT_FLOAT_EQ(sheet.rules[0].declarations[0].value.number, 1.0f);
  EXPECT_EQ(sheet.rules[0].declarations[1].property, PropertyId::kBorderTopStyle);
  EXPECT_EQ(sheet.rules[0].declarations[1].value.enum_value, static_cast<u16>(BorderStyle::kSolid));
  EXPECT_EQ(sheet.rules[0].declarations[2].property, PropertyId::kBorderTopColor);
  EXPECT_EQ(sheet.rules[0].declarations[2].value.color, 0xFF0000FFu);
}
```

完整 12 测试遵循 spec §7.1 列表（FullValue / PartialValue / OutOfOrder / Important / DualEntry / NCapSecurity / LonghandFallthrough / NoDegradation / ReverseProbe + 3 边其他方向 FullValue）。

- [ ] **步骤 2：运行测试验证 FAIL**
  运行：`cd build && ctest -R "BorderTopShorthand|BorderRightShorthand|BorderBottomShorthand|BorderLeftShorthand|ExistingBorderShorthand_NoDegradation" --output-on-failure`
  预期：12 测试全 FAIL（除 `ExistingBorderShorthand_NoDegradation` 应 PASS — 既有功能不退化）

- [ ] **步骤 3：提交 RED**
  ```bash
  git add tests/core/css/parser_test.cc
  git commit -m "test(css): TASK-20260430-02 R1 RED tests for border-{top,right,bottom,left} shorthand"
  ```

### R1.2：GREEN 实现 [TDD]

**文件：**
- 修改：`veloxa/core/css/parser.cc`（4 个新分支按字母序插入）

- [ ] **步骤 1**：在 `ParseDeclaration` 内 `if (EqualsIgnoreCase(name, "border")) { ... }` 分支之后、`if (EqualsIgnoreCase(name, "flex"))` 分支之前，按字母序插入 4 个新分支

  插入顺序（字母序）：`border-bottom` → `border-left` → `border-right` → `border-top`

  每分支实施伪码见 spec §5.3（与既有 `border` 完全同模式，仅 push 的 PropertyId 不同）。

  关键差异点逐分支表：

  | 分支 | push 的 PropertyId（width/style/color）|
  |---|---|
  | `border-top` | kBorderTopWidth / kBorderTopStyle / kBorderTopColor |
  | `border-right` | kBorderRightWidth / kBorderRightStyle / kBorderRightColor |
  | `border-bottom` | kBorderBottomWidth / kBorderBottomStyle / kBorderBottomColor |
  | `border-left` | kBorderLeftWidth / kBorderLeftStyle / kBorderLeftColor |

- [ ] **步骤 2：编译 + 运行测试验证 PASS**
  ```bash
  cmake --build build -j
  cd build && ctest -R "BorderTopShorthand|BorderRightShorthand|BorderBottomShorthand|BorderLeftShorthand|ExistingBorderShorthand" --output-on-failure
  ```
  预期：12 测试全 PASS

- [ ] **步骤 3：全量 ctest**
  ```bash
  cd build && ctest --output-on-failure | tail -10
  ```
  预期：1051/1051 PASS（基线 1039 + R1 新增 12）

- [ ] **步骤 4：提交 GREEN**
  ```bash
  git add veloxa/core/css/parser.cc
  git commit -m "feat(css): TASK-20260430-02 R1 GREEN border-{top,right,bottom,left} shorthand"
  ```

### R1.3：§9.3 反向探针验证

- [ ] **步骤 1：临时破坏 border-top 实施**
  在 `parser.cc` `border-top` 分支末尾把 `PropertyId::kBorderTopWidth` 改为 `PropertyId::kBorderBottomWidth`（mis-route）

- [ ] **步骤 2：跑测试 → 确认 FAIL**
  ```bash
  cmake --build build -j && cd build && ctest -R "BorderTopShorthand_FullValue" --output-on-failure
  ```
  预期：`BorderTopShorthand_FullValue` FAIL（看到 `kBorderBottomWidth` 而非期望的 `kBorderTopWidth`）

- [ ] **步骤 3：恢复实施 → 跑测试 → 确认 PASS**

- [ ] **步骤 4：记录反向探针结果**
  把「破坏 → FAIL → 恢复 → PASS」三态切换写到 `progress.md` R1.3 段。

### R1 中间态报告

完成 R1.3 后，给用户中间态报告：

```markdown
## R1 中间态（border 4 方向 shorthand）

- ✅ R1.1 RED: 12 测试落地 + 验证 FAIL
- ✅ R1.2 GREEN: 4 分支实施 + ctest 1051/1051 PASS
- ✅ R1.3 反向探针: 完整循环（破坏 FAIL → 恢复 PASS）
- 📊 实测耗时: ~XX min（plan 估 80 min, plan×0.6 = 48 min）
- 🔄 下一步: R2 3 属性级 shorthand
```

征求用户确认是否继续 R2。

---

## R2：3 属性级 shorthand（border-width / -style / -color）

### R2.1：RED 测试 [TDD]

**文件：**
- 修改：`tests/core/css/parser_test.cc`（追加 13 个 R2 测试，详见 spec §7.2）

- [ ] **步骤 1：编写 13 R2 测试**

`border-width` 1-4 值 OneValue / TwoValues / ThreeValues / FourValues：

```cpp
TEST(CssParserTest, BorderWidthShorthand_OneValue) {
  auto sheet = CssParser::Parse("div { border-width: 5px; }");
  ASSERT_EQ(sheet.rules[0].declarations.size(), 4u);
  EXPECT_EQ(sheet.rules[0].declarations[0].property, PropertyId::kBorderTopWidth);
  EXPECT_FLOAT_EQ(sheet.rules[0].declarations[0].value.number, 5.0f);
  EXPECT_EQ(sheet.rules[0].declarations[1].property, PropertyId::kBorderRightWidth);
  EXPECT_FLOAT_EQ(sheet.rules[0].declarations[1].value.number, 5.0f);
  EXPECT_EQ(sheet.rules[0].declarations[2].property, PropertyId::kBorderBottomWidth);
  EXPECT_FLOAT_EQ(sheet.rules[0].declarations[2].value.number, 5.0f);
  EXPECT_EQ(sheet.rules[0].declarations[3].property, PropertyId::kBorderLeftWidth);
  EXPECT_FLOAT_EQ(sheet.rules[0].declarations[3].value.number, 5.0f);
}

TEST(CssParserTest, BorderWidthShorthand_TwoValues) {
  auto sheet = CssParser::Parse("div { border-width: 1px 2px; }");
  ASSERT_EQ(sheet.rules[0].declarations.size(), 4u);
  EXPECT_FLOAT_EQ(sheet.rules[0].declarations[0].value.number, 1.0f);  // top
  EXPECT_FLOAT_EQ(sheet.rules[0].declarations[1].value.number, 2.0f);  // right
  EXPECT_FLOAT_EQ(sheet.rules[0].declarations[2].value.number, 1.0f);  // bottom
  EXPECT_FLOAT_EQ(sheet.rules[0].declarations[3].value.number, 2.0f);  // left
}
```

`border-style` / `border-color` 同模式。

完整 13 测试遵循 spec §7.2 列表。

- [ ] **步骤 2：运行测试验证 FAIL**
  ```bash
  cd build && ctest -R "BorderWidthShorthand|BorderStyleShorthand|BorderColorShorthand" --output-on-failure
  ```
  预期：13 测试全 FAIL

- [ ] **步骤 3：提交 RED**
  ```bash
  git add tests/core/css/parser_test.cc
  git commit -m "test(css): TASK-20260430-02 R2 RED tests for border-{width,style,color} shorthand"
  ```

### R2.2：GREEN 实现 [TDD]

**文件：**
- 修改：`veloxa/core/css/parser.cc`（3 个新分支按字母序插入）

- [ ] **步骤 1**：在 R1 4 分支后按字母序插入 3 个新分支：`border-color` → `border-style` → `border-width`

  每分支实施伪码见 spec §5.4（与既有 `padding/margin` 1-4 值规则同模式；value parser 异构：Length / Enum / Color）。

- [ ] **步骤 2：编译 + 运行测试验证 PASS**
  ```bash
  cmake --build build -j
  cd build && ctest -R "BorderWidthShorthand|BorderStyleShorthand|BorderColorShorthand" --output-on-failure
  ```
  预期：13 测试全 PASS

- [ ] **步骤 3：全量 ctest**
  预期：1064/1064 PASS（基线 1039 + R1 12 + R2 13）

- [ ] **步骤 4：提交 GREEN**
  ```bash
  git add veloxa/core/css/parser.cc
  git commit -m "feat(css): TASK-20260430-02 R2 GREEN border-{width,style,color} shorthand"
  ```

### R2.3：§9.3 反向探针验证

- [ ] **步骤 1：临时破坏 border-width 4-side 展开**
  在 `parser.cc` `border-width` 分支 size==2 展开规则改为 `top=values[1]; right=left=values[0]`（语义错位）

- [ ] **步骤 2：跑测试 → 确认 FAIL**
  ```bash
  cmake --build build -j && cd build && ctest -R "BorderWidthShorthand_TwoValues" --output-on-failure
  ```
  预期：`BorderWidthShorthand_TwoValues` FAIL（top 期望 1.0f 实测 2.0f）

- [ ] **步骤 3：恢复实施 → 跑测试 → 确认 PASS**

- [ ] **步骤 4：记录反向探针结果**

### R2 中间态报告

完成 R2.3 后，给用户中间态报告。

---

## P3：finalize（10 min, plan × 0.6 = 6 min）

- [ ] **步骤 1：Release `-O3 -Werror` 全量构建**
  ```bash
  cmake --build build-bench -j 2>&1 | tail -5
  ```
  预期：0 err / 0 warn

- [ ] **步骤 2：Release ctest（如适用）**
  ```bash
  cd build-bench && ctest -j --output-on-failure | tail -5
  ```
  预期：1064/1064 PASS（含本任务新增）

- [ ] **步骤 3：Memory Bank 同步**
  - 更新 `memory-bank/tasks.md` 任务历史表（标 BUILD 完成）
  - 更新 `memory-bank/activeContext.md` 阶段 → `构建完成`
  - 更新 `memory-bank/progress.md` 完整 R1+R2 中间态报告

- [ ] **步骤 4：提交 finalize**
  ```bash
  git add memory-bank/
  git commit -m "chore(build): TASK-20260430-02 finalize MB state — 7 border shorthand complete"
  ```

---

## 估时与提交

| Phase | 内容 | plan (min) | × 0.6 (min) | commits |
|:-:|---|---:|---:|:-:|
| P0 | 准备 + grep 验证 | 10 | 6 | 0 |
| R1.1 | RED 12 R1 测试 | 30 | 18 | 1 |
| R1.2 | GREEN 4 方向实施 | 40 | 24 | 1 |
| R1.3 | 反向探针验证 | 10 | 6 | 0 |
| R2.1 | RED 13 R2 测试 | 30 | 18 | 1 |
| R2.2 | GREEN 3 属性级实施 | 30 | 18 | 1 |
| R2.3 | 反向探针验证 | 10 | 6 | 0 |
| P3 | finalize | 10 | 6 | 1 |
| **合计** | — | **170 (~2.83h)** | **~102 (~1.7h)** | **5 commits** |

**plan × 0.6 第 15 数据点预期**：Level 2 单子系统 + 模式 100% 复用 + V5 安全 + 多轮次 → 落「准确档 0.5-0.6×」（~85-100 min 实测预期）。

---

## 验收标准（A1-A8 完整版，与 spec §3 一致）

| # | 判据 | 验证方式 |
|:-:|---|---|
| A1 | 4 方向 shorthand 双入口（`Parse` + `ParseDeclarationList`）解析正确 | R1 12 单测 PASS |
| A2 | 3 属性级 shorthand 1-4 值规则与 padding/margin 同模式 | R2 13 单测 PASS |
| A3 | 既有 `BorderShorthand` 测试不退化 | ctest 1039+ 全 PASS（含 `ExistingBorderShorthand_NoDegradation`）|
| A4 | DoS / 病态输入护栏：T6（per-shorthand 内部 token 上限）+ T8（4-value 上限） | 安全 N-cap 测试 PASS（`BorderTopShorthand_NCapSecurity` + `BorderWidthShorthand_NCapSecurity`）|
| A5 | §9.3 反向探针 ≥ 2 处（R1 + R2 各 1）| R1.3 + R2.3 反向探针完整循环 |
| A6 | Release `-O3 -Werror` 0 err/warn | `cmake --build build-bench -j` |
| A7 | ctest 全量 PASS（基线 1039 → 1064）| `ctest --output-on-failure` |
| A8 | TASK-30-01 升级规则 §0「CSS shorthand 能力 grep 表」自我应用首次外部任务 ROI 验证 | reflection §8 评估 |

---

## 不需要 `/creative` 阶段

5 决策 D1-D5 已锁定（全部 A/A/C/A/A），无 UI/算法/架构空白；保存计划后直接 `/build`。
