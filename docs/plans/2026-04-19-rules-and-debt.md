# 流程规则沉淀 + P2 功能技术债收口 实现计划

**目标：** 落实 14 条 P1 流程规则改进（Part A，纯 .mdc 文件修改），并消化 3 条 P2 功能技术债（Part B，含代码与测试），让 `activeContext.md` 待办列表收口至 ≤ 5 条。

**架构：** Part A 与 Part B 严格分阶段。先 A（无代码风险，可独立提交）→ 再 B（含代码，需 TDD 与逐 Phase 验证）。Part B 内部 B5（CSS Enum）独立于 B6/B7（Event 子系统），可在任何 A 段后独立切入；B6 与 B7 共享 `EventManager` API 扩展，B6 先（基础设施），B7 后（业务接入）。

**技术栈：** Markdown（Part A）+ C++17 / CMake / GoogleTest / QuickJS-NG v0.14.0（Part B）。

**复杂度级别：** Level 3（需要设计决策，跨「.cursor 规则」+「Script / CSS / Event」3 个子系统）。

**任务 ID：** TASK-20260419-01

**设计规格：** `docs/specs/2026-04-19-rules-and-debt-design.md`

**分支：** `feature/TASK-20260419-01-rules-and-debt`（已从 `main` 创建）

---

## 全局约定

- **构建目录**：`build/`（沿用既有配置）
- **构建命令**：`cmake --build build -j$(nproc) --target <target>`
- **完整回归**：`ctest --test-dir build --output-on-failure`
- **基线测试数**：856（来自 TASK-20260418-01 归档）；本任务结束后预期 ≥ 874
- **所有 `git commit` 采用 HEREDOC 形式**，commit 消息前缀按惯例：`docs(rules)` / `docs(memory-bank)` / `feat(css)` / `fix(event)` / `refactor(script)` / `test(...)`

---

## 文件结构

### Part A — 流程规则与文档

| 路径 | 职责 | 变更类型 |
|------|------|---------|
| `.cursor/rules/skills/writing-plans.mdc` | 计划模板增 5 段 | 修改 |
| `.cursor/rules/skills/subagent-development.mdc` | 子代理 prompt 增 3 段 | 修改 |
| `.cursor/rules/skills/integration-testing.mdc` | 6 条铁律 + API 备忘清单 | **新建** |
| `memory-bank/techContext.md` | 「FetchContent 与代理」段 | 修改（追加段） |

### Part B — 代码与测试

| 路径 | 职责 | 变更类型 |
|------|------|---------|
| `veloxa/core/css/enum_serialization.h` | `EnumValueToCssString(PropertyId, u16)` 接口 | **新建** |
| `veloxa/core/css/enum_serialization.cc` | 13 类 enum × 反查表 | **新建** |
| `veloxa/core/css/CMakeLists.txt` | 注册新源文件 | 修改（**[共享文件]**） |
| `tests/css/enum_serialization_test.cc` | 13 类 enum 边界测试 | **新建** |
| `tests/css/CMakeLists.txt` | 注册新测试目标 | 修改（**[共享文件]**） |
| `veloxa/core/event/event_dispatcher.h` | `AddEventListener` 返回 `u64`，新增 `RemoveEventListenerByToken` | 修改 |
| `veloxa/core/event/event_dispatcher.cc` | token 分配 + 按 token 删除 | 修改 |
| `veloxa/core/event/event_manager.h` | 镜像扩展 + `AddDestructionObserver` | 修改 |
| `veloxa/core/event/event_manager.cc` | observer 通知 + `~EventManager` 实现 | 修改 |
| `tests/event/event_manager_test.cc` | DestructionObserver + Token 测试 | 修改 |
| `tests/event/event_dispatcher_test.cc` | Token 测试（如已存在）| 修改或新建 |
| `veloxa/script/dom_bindings.cc` | 接入 `EnumValueToCssString` + DestructionObserver + 精确 remove | 修改 |
| `tests/script/dom_bindings_test.cc` | Display 读 + 反序销毁 + 精确移除 测试 | 修改 |

### [共享文件] 标注

- **修改**：`veloxa/core/css/CMakeLists.txt`、`tests/css/CMakeLists.txt`（B5 必须）
- **可能修改**：`tests/event/CMakeLists.txt`（若 `event_dispatcher_test` 不存在则新建）
- **不修改**：根 `CMakeLists.txt`、`vcpkg.json`、`.clang-format`、`.clang-tidy`

### [影响前序测试] 标注

- **B7（Token API 扩展）**：`EventManager::AddEventListener` 返回类型从 `void` → `u64`。所有现有调用方（`dom_bindings.cc`、`tests/event/*_test.cc`）若以 `void` 形式接收返回值是 OK 的（C++ 允许丢弃非 void 返回值）；但若某测试用 `expect_type<void>` 会失败 → Phase B7 步骤 1 先 grep 确认。
- **B6（~EventManager 加观察者通知）**：所有现有 `EventManager` 局部对象的析构都会触发观察者循环。若现有测试构造 EM 但不注册 observer，循环为空，零开销零行为差异。

---

## Phase 0 — 基线验证（5 分钟）

### 任务 0.1：确认基线测试全绿

- [ ] **步骤 1：构建**
  ```bash
  cmake --build build -j$(nproc) 2>&1 | tail -20
  ```
  预期：build 成功，无新警告

- [ ] **步骤 2：跑全量测试**
  ```bash
  ctest --test-dir build --output-on-failure 2>&1 | tail -10
  ```
  预期：`100% tests passed, 0 tests failed out of 856`（或当前实际数量；记入 progress.md）

- [ ] **步骤 3：确认分支与工作区**
  ```bash
  git status && git log --oneline -3
  ```
  预期：分支 `feature/TASK-20260419-01-rules-and-debt`，工作区干净

---

## Phase A1 — writing-plans.mdc 增 5 段（30 分钟，纯文档）

### 任务 A1.1：FetchContent C 子项目编译选项审计段

**文件：** `.cursor/rules/skills/writing-plans.mdc`

- [ ] **步骤 1：定位插入点**
  在「## CMake 链接方向约束分析」段（第 31 行起）的章节末尾之后、「## 小块任务粒度」段之前。

- [ ] **步骤 2：插入新段**
  内容（来自设计规格 §3.1.1）：
  ````markdown
  ## FetchContent C 子项目编译选项审计（涉及 FetchContent + 第三方源码时**必填**）

  > 反复出现（TASK-20260413-01）：根 `add_compile_options` 的严格告警标志会污染上游 C 源，触发 `-Werror=pedantic` 等失败。

  在任务分解**之前**回答：

  1. **第三方代码语言**：本次 `FetchContent_Declare` 拉取的代码是否包含 C 源（QuickJS、libuv、zlib 等）？
  2. **全局选项作用域**：根 `CMakeLists.txt` 的 `add_compile_options(...)` 是否对 C 源生效？特别检查 `-Werror`、`-Wpedantic`、`-Wpedantic`、`-Werror=pedantic` 等。
  3. **冲突预测**：第 1+2 都为是 → 计划必须包含**前置 Phase**，将严格告警限制为 C++：
     - 推荐：`add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:-Werror>")` 等
     - 备选：对自有目标用 `target_compile_options(my_target PRIVATE -Werror)`，避免全局注入
  4. **验证方式**：在前置 Phase 末尾 `cmake -B build && cmake --build build --target <第三方目标>` 验证无新告警。

  如本节得出「需要改根 CMakeLists」结论，把 CMake 变更作为独立 Phase 前置任务。
  ````

- [ ] **步骤 3：人眼校对** — 确保格式与上下文段一致（mdc frontmatter 不变，章节用 `##` 二级标题）

- [ ] **步骤 4：提交**
  ```bash
  git add .cursor/rules/skills/writing-plans.mdc
  git commit -m "$(cat <<'EOF'
  docs(rules): add FetchContent C subproject compile options audit (writing-plans)

  Refs TASK-20260419-01 Part A: 第 1/5 段（来源 TASK-20260413-01）
  EOF
  )"
  ```

### 任务 A1.2：测试基础设施审计段

**文件：** `.cursor/rules/skills/writing-plans.mdc`

- [ ] **步骤 1：在 A1.1 新段之后追加**
  内容（来自设计规格 §3.1.2）：
  ````markdown
  ## 测试基础设施审计（任何 Phase 包含新写测试时**必填**）

  > 反复出现（TASK-11 多次）：测试需要访问的内部状态没有访问路径，临时塞 friend 或 `#define private public`。

  对每个测试任务列出：

  1. **被测内部状态**：测试需要读哪些内部字段？（私有成员、内部容器大小、内部回调触发次数）
  2. **当前访问路径**：是否已存在公开 getter / 测试专用 friend / 间接观测点？
  3. **缺失补救**：若无访问路径 → 计划中加「补充访问路径」前置任务。优先级顺序：
     - **首选**：暴露公开只读 getter（`size()`、`count_xxx() const`）
     - **次选**：在被测类中显式 `friend class XxxTest;`（白名单 friend）
     - **禁止**：`#define private public`、`reinterpret_cast` 越权访问
  ````

- [ ] **步骤 2：提交**
  ```bash
  git add .cursor/rules/skills/writing-plans.mdc
  git commit -m "$(cat <<'EOF'
  docs(rules): add test infrastructure audit section (writing-plans)

  Refs TASK-20260419-01 Part A: 第 2/5 段（来源 TASK-11）
  EOF
  )"
  ```

### 任务 A1.3：边界输入清单段

**文件：** `.cursor/rules/skills/writing-plans.mdc`

- [ ] **步骤 1：追加新段**
  内容（来自设计规格 §3.1.3）：
  ````markdown
  ## 边界输入清单（涉及解析、查找、转换、边界判断时**必填**）

  > 反复出现（TASK-06）：only happy path 测试，遇到空/null/超长立刻崩。

  每个相关 Phase 必须列出：

  | 类别 | 示例 | 预期行为 |
  |------|------|---------|
  | 默认路径 | 正常字符串、合法 ID、典型尺寸 | 业务成功 |
  | 空 | `""`、`nullptr`、零长度数组 | 静默忽略 / 返回错误码（明示哪个）|
  | 超长 | 1MB 字符串、深度 10000 嵌套 | 拒绝 / 截断（明示）|
  | Unicode | 多字节 UTF-8、emoji、ZWJ | 正确处理（不截到字节中间）|
  | 边界值 | 0、-1、INT_MAX、FLT_MIN | clamp / 拒绝（明示）|
  | 未注册项 | 不存在的 ID、未定义的 enum | 返回 unknown / 默认值 |

  每条边界输入对应 ≥ 1 个测试。
  ````

- [ ] **步骤 2：提交**
  ```bash
  git add .cursor/rules/skills/writing-plans.mdc
  git commit -m "$(cat <<'EOF'
  docs(rules): add boundary input checklist section (writing-plans)

  Refs TASK-20260419-01 Part A: 第 3/5 段（来源 TASK-06）
  EOF
  )"
  ```

### 任务 A1.4：调用链端到端验证段

**文件：** `.cursor/rules/skills/writing-plans.mdc`

- [ ] **步骤 1：追加新段**
  内容（来自设计规格 §3.1.4）：
  ````markdown
  ## 调用链端到端验证（修改函数签名 / 默认值 / 返回类型时**必填**）

  > 反复出现（TASK-09）：SelectorMatcher 加了参数没透传到 StyleResolver，导致 restyle 阶段伪类永远 false。

  计划必须包含：

  1. **完整调用链清单**：从最上游入口（`Application::Update`、`vx_view_update`、用户回调）到最下游被改函数，列出每一层的文件 + 函数名。
  2. **透传决策**：每一层是否需要透传新参数？若是，标注「需透传」。
  3. **测试覆盖**：现有测试是否覆盖整条链？若否 → 加 ≥ 1 个端到端集成测试。
  4. **验证脚本**：在收尾 Phase 用 grep 确认所有调用点已更新：
     ```bash
     rg "OldFunc\(" --type cpp | grep -v "_test.cc"
     ```
  ````

- [ ] **步骤 2：提交**
  ```bash
  git add .cursor/rules/skills/writing-plans.mdc
  git commit -m "$(cat <<'EOF'
  docs(rules): add end-to-end call chain verification section (writing-plans)

  Refs TASK-20260419-01 Part A: 第 4/5 段（来源 TASK-09）
  EOF
  )"
  ```

### 任务 A1.5：管线注入点代码级可行性验证段

**文件：** `.cursor/rules/skills/writing-plans.mdc`

- [ ] **步骤 1：追加新段**
  内容（来自设计规格 §3.1.5）：
  ````markdown
  ## 管线注入点代码级可行性验证（设计文档提出"在 X 步骤后注入 Y"时**必填**）

  > 反复出现第 4 次（TASK-13）：设计文档说"在 Layout 后插入动画覆盖层"，实际代码里 LayoutBox 是 const，覆盖层无法工作。

  头脑风暴阶段必须：

  1. **定位注入点**：找到 X 步骤的实际代码（文件 + 函数 + 行号）。例如「在 LayoutEngine::Layout 末尾、return 之前」。
  2. **数据可访问性**：Y 所需的全部输入数据在该位置是否已可访问？是否需要新增参数透传 / 暴露内部状态？
  3. **可写性**：Y 是否需要修改 X 处的输出？该输出当前是 const / 私有？是否需要 const_cast / 接口扩展？
  4. **结论**：若任一项「不可」→ 设计文档**必须**显式标注「需要扩展 X 接口」并给出具体签名变更。

  此段产出物：「注入点核对表」一节进入设计文档。
  ````

- [ ] **步骤 2：提交**
  ```bash
  git add .cursor/rules/skills/writing-plans.mdc
  git commit -m "$(cat <<'EOF'
  docs(rules): add pipeline injection point feasibility section (writing-plans)

  Refs TASK-20260419-01 Part A: 第 5/5 段（来源 TASK-13，反复模式第 4 次）
  EOF
  )"
  ```

---

## Phase A2 — subagent-development.mdc 增 3 段（20 分钟，纯文档）

### 任务 A2.1：跨模块数据格式段

**文件：** `.cursor/rules/skills/subagent-development.mdc`

- [ ] **步骤 1：定位插入点**
  在「### Prompt 骨架示例」段之后、「## 与 Memory Bank 集成」之前。

- [ ] **步骤 2：插入新段**
  内容（来自设计规格 §3.2.1）：
  ````markdown
  ### 跨模块数据格式（涉及共享数据结构时**必填**）

  > 来源 TASK-02：缺少格式 prompt 触发 24 小时返工（像素格式假设矛盾）。

  当子代理任务读写跨模块共享数据时，prompt 必须包含：

  - **格式精确定义**：bit 布局、字节序、单位、范围
    - 例：`RGBA32 = R[0:7] | G[8:15] | B[16:23] | A[24:31]`
    - 例：`CSS color = u32 0xRRGGBBAA`（与上面**不同**！黑色 CSS=0x000000FF，gfx=0xFF000000）
  - **格式引用位置**：上游/下游模块在哪里使用？文件 + 函数
  - **已知不一致**：如有跨格式风险（CSS color vs gfx::Color），明示桥接函数（如 `CssColorToGfx`）

  反例（不要给子代理这种描述）：
  > "把 CSS background-color 应用到 PaintCommand"

  正例：
  > "把 ComputedStyle::background_color (u32 RRGGBBAA) 通过 render_utils.h::CssColorToGfx 转换为 gfx::Color，再写入 PaintCommand::FillRect{color, ...}"
  ````

- [ ] **步骤 3：提交**
  ```bash
  git add .cursor/rules/skills/subagent-development.mdc
  git commit -m "$(cat <<'EOF'
  docs(rules): add cross-module data format section (subagent-development)

  Refs TASK-20260419-01 Part A: 第 6/14 段（来源 TASK-02）
  EOF
  )"
  ```

### 任务 A2.2：LayoutBox 坐标语义段

**文件：** `.cursor/rules/skills/subagent-development.mdc`

- [ ] **步骤 1：在 A2.1 之后追加**
  内容（来自设计规格 §3.2.2）：
  ````markdown
  ### LayoutBox 坐标语义（涉及 hit-test / 渲染坐标 / padding-border 计算时**必填**）

  > 来源 TASK-07：子代理把 `LayoutBox::x/y` 当成 border box origin 用于 fill rect，导致背景色偏移。

  Prompt 必须明示：

  - `LayoutBox::x`, `LayoutBox::y` 是 **content area 原点**（绝对坐标，相对 viewport）
  - `padding box origin = (x - padding[left], y - padding[top])`
  - `border box origin = (x - padding[left] - border[left], y - padding[top] - border[top])`
  - **背景**绘制在 padding box；**边框**绘制在 border box；**内容**起点是 content origin
  - 子代理代码中所有坐标计算必须在注释中标注是哪一个 origin，例如：
    ```cpp
    // bg_rect: padding box（背景包含 padding 但不含 border）
    Rect bg_rect{box.x - box.padding_left, box.y - box.padding_top, ...};
    ```

  辅助方法（若已存在则要求子代理使用，禁止重复手写）：
  - `LayoutBox::padding_box_origin()`（如已实现）
  - `LayoutBox::border_box_origin()`（如已实现）
  ````

- [ ] **步骤 2：提交**
  ```bash
  git add .cursor/rules/skills/subagent-development.mdc
  git commit -m "$(cat <<'EOF'
  docs(rules): add LayoutBox coordinate semantics section (subagent-development)

  Refs TASK-20260419-01 Part A: 第 7/14 段（来源 TASK-07）
  EOF
  )"
  ```

### 任务 A2.3：并行子代理可行条件段

**文件：** `.cursor/rules/skills/subagent-development.mdc`

- [ ] **步骤 1：在 A2.2 之后追加**
  内容（来自设计规格 §3.2.3）：
  ````markdown
  ### 并行子代理可行条件（**主线程在分派 ≥ 2 个子代理并行执行前必须确认**）

  > 来源 TASK-08：首次成功并行 hit_test.cc + event_dispatcher.cc，零冲突。

  并行可行的**充分**条件（全部满足才能并行）：

  1. **不共享 .cc**：各子代理产出的 `.cc` 文件路径完全不重叠
  2. **共享 .h 已就绪**：子代理依赖的共享头文件（如类型定义、接口签名）已在前置 Phase 创建并完成
  3. **CMakeLists 已更新**：所有即将创建的 `.cc` 文件已在前置 Phase 加入对应 `CMakeLists.txt`
  4. **无运行时共享状态修改**：子代理不修改根 `CMakeLists.txt` / `vcpkg.json` / `memory-bank/` / `.cursor/` 等共享资源

  违反任一条件 → **必须串行**。

  典型反例（需串行的场景）：
  - Phase X 修改 layout_engine.cc 中的 LayoutChild，Phase Y 同时在 LayoutChild 增加新分支 → 同 .cc 冲突
  - Phase X 创建新接口 `IFoo`，Phase Y 实现 `FooImpl` 依赖该接口 → 共享 .h 未就绪
  ````

- [ ] **步骤 2：提交**
  ```bash
  git add .cursor/rules/skills/subagent-development.mdc
  git commit -m "$(cat <<'EOF'
  docs(rules): add parallel subagent prerequisites section (subagent-development)

  Refs TASK-20260419-01 Part A: 第 8/14 段（来源 TASK-08）
  EOF
  )"
  ```

---

## Phase A3 — 新建 integration-testing.mdc（20 分钟，纯文档）

### 任务 A3.1：创建集成测试技能文件

**文件：** `.cursor/rules/skills/integration-testing.mdc`（**新建**）

- [ ] **步骤 1：创建文件**
  完整内容（来自设计规格 §3.3，含 6 条铁律 + API 备忘清单）。

- [ ] **步骤 2：提交**
  ```bash
  git add .cursor/rules/skills/integration-testing.mdc
  git commit -m "$(cat <<'EOF'
  docs(rules): add integration-testing skill with 6 ironclad rules

  - 数据格式一致性优先（来源 TASK-02）
  - 必须使用真实 HTML/CSS 解析器（来源 TASK-06）
  - 像素验证优先 DisplayList + 区域扫描（来源 TASK-07）
  - CSS 颜色测试禁止与 gfx::Color 编程常量直接比较（来源 TASK-07）
  - 禁止使用 HTML inline style（来源 TASK-08）
  - API 备忘清单（来源 TASK-13）

  Refs TASK-20260419-01 Part A: 第 9-13/14 段
  EOF
  )"
  ```

### 任务 A3.2：在 main.mdc 注册新技能

**文件：** `.cursor/rules/main.mdc`

- [ ] **步骤 1：定位「### 可用技能」表**
  在表格末尾（"安全"行之后）追加：
  ```markdown
  | 集成测试 | 编写跨模块端到端测试时 | `.cursor/rules/skills/integration-testing.mdc` |
  ```

- [ ] **步骤 2：提交**
  ```bash
  git add .cursor/rules/main.mdc
  git commit -m "$(cat <<'EOF'
  docs(rules): register integration-testing skill in main.mdc skill table

  Refs TASK-20260419-01 Part A
  EOF
  )"
  ```

---

## Phase A4 — techContext.md 代理段（10 分钟，纯文档）

### 任务 A4.1：追加 FetchContent 与代理段

**文件：** `memory-bank/techContext.md`

- [ ] **步骤 1：定位插入点**
  在「### 第三方依赖」段表格之后、「## Sciter 架构分析摘要」段之前。

- [ ] **步骤 2：插入新段**
  内容（来自设计规格 §3.4，含 `http_proxy` 设置 + 离线场景 + 已知失败模式表）。

- [ ] **步骤 3：提交**
  ```bash
  git add memory-bank/techContext.md
  git commit -m "$(cat <<'EOF'
  docs(memory-bank): add FetchContent and proxy notes to techContext

  Refs TASK-20260419-01 Part A: 第 14/14 段（来源 TASK-20260413-01）
  EOF
  )"
  ```

### 任务 A4.2：Part A 完成里程碑提交

- [ ] **步骤 1：更新 progress.md**
  在「TASK-20260419-01」段加一行：
  ```markdown
  - 2026-04-19 Part A 完成：14 条 P1 流程规则已固化到 .mdc / techContext。共 8 个提交。
  ```

- [ ] **步骤 2：从 activeContext.md 待办清单移除已落实的 14 条 P1**
  保留 P2 三条 + Benchmark + 像素测试迁移；其余 P1 全部删除。

- [ ] **步骤 3：提交**
  ```bash
  git add memory-bank/progress.md memory-bank/activeContext.md
  git commit -m "$(cat <<'EOF'
  docs(memory-bank): mark TASK-20260419-01 Part A complete; cleanup todo list

  - progress.md：记录 Part A 完成
  - activeContext.md：移除已落实的 14 条 P1（保留 P2 + 长期项）
  EOF
  )"
  ```

---

## Phase B5 — Enum 反查表（45 分钟，TDD）

### 任务 B5.1：创建 enum_serialization 头文件 [TDD]

**文件：**
- 创建：`veloxa/core/css/enum_serialization.h`

- [ ] **步骤 1：编写头文件**
  内容（来自设计规格 §4.1.2 头部分）：
  ```cpp
  #ifndef VELOXA_CORE_CSS_ENUM_SERIALIZATION_H_
  #define VELOXA_CORE_CSS_ENUM_SERIALIZATION_H_

  #include "veloxa/core/css/property.h"
  #include "veloxa/foundation/strings/string_view.h"

  namespace vx::css {

  // Returns the CSS-canonical string for an enum value of the given property,
  // or an empty StringView if (property, enum_value) is not a registered enum.
  //
  // Examples:
  //   EnumValueToCssString(PropertyId::kDisplay, 1) == "block"
  //   EnumValueToCssString(PropertyId::kPosition, 2) == "absolute"
  //   EnumValueToCssString(PropertyId::kBackgroundColor, 0) == ""  // not enum
  StringView EnumValueToCssString(PropertyId property, u16 enum_value);

  }  // namespace vx::css
  #endif  // VELOXA_CORE_CSS_ENUM_SERIALIZATION_H_
  ```

- [ ] **步骤 2：人眼校对** — 命名空间 `vx::css`、include 路径正确

### 任务 B5.2：编写失败测试 [TDD]

**文件：**
- 创建：`tests/css/enum_serialization_test.cc`
- 修改：`tests/css/CMakeLists.txt`（**[共享文件]**）

- [ ] **步骤 1：编写测试文件**
  ```cpp
  #include "veloxa/core/css/enum_serialization.h"

  #include <gtest/gtest.h>

  namespace vx::css {

  TEST(EnumSerialization, DisplayCanonical) {
    EXPECT_EQ(EnumValueToCssString(PropertyId::kDisplay, 0), StringView("none"));
    EXPECT_EQ(EnumValueToCssString(PropertyId::kDisplay, 1), StringView("block"));
    EXPECT_EQ(EnumValueToCssString(PropertyId::kDisplay, 2), StringView("inline"));
    EXPECT_EQ(EnumValueToCssString(PropertyId::kDisplay, 3), StringView("inline-block"));
    EXPECT_EQ(EnumValueToCssString(PropertyId::kDisplay, 4), StringView("flex"));
  }

  TEST(EnumSerialization, PositionCanonical) {
    EXPECT_EQ(EnumValueToCssString(PropertyId::kPosition, 0), StringView("static"));
    EXPECT_EQ(EnumValueToCssString(PropertyId::kPosition, 1), StringView("relative"));
    EXPECT_EQ(EnumValueToCssString(PropertyId::kPosition, 2), StringView("absolute"));
    EXPECT_EQ(EnumValueToCssString(PropertyId::kPosition, 3), StringView("fixed"));
  }

  TEST(EnumSerialization, FlexDirectionCanonical) {
    EXPECT_EQ(EnumValueToCssString(PropertyId::kFlexDirection, 0), StringView("row"));
    EXPECT_EQ(EnumValueToCssString(PropertyId::kFlexDirection, 1), StringView("row-reverse"));
    EXPECT_EQ(EnumValueToCssString(PropertyId::kFlexDirection, 2), StringView("column"));
    EXPECT_EQ(EnumValueToCssString(PropertyId::kFlexDirection, 3), StringView("column-reverse"));
  }

  TEST(EnumSerialization, JustifyContentCanonical) {
    EXPECT_EQ(EnumValueToCssString(PropertyId::kJustifyContent, 0), StringView("flex-start"));
    EXPECT_EQ(EnumValueToCssString(PropertyId::kJustifyContent, 4), StringView("space-around"));
  }

  TEST(EnumSerialization, AlignItemsCanonical) {
    EXPECT_EQ(EnumValueToCssString(PropertyId::kAlignItems, 0), StringView("auto"));
    EXPECT_EQ(EnumValueToCssString(PropertyId::kAlignItems, 4), StringView("stretch"));
    EXPECT_EQ(EnumValueToCssString(PropertyId::kAlignItems, 5), StringView("baseline"));
    // align-self 共用同一表
    EXPECT_EQ(EnumValueToCssString(PropertyId::kAlignSelf, 0), StringView("auto"));
  }

  TEST(EnumSerialization, BorderStyleSharedAcrossSides) {
    for (auto p : {PropertyId::kBorderTopStyle, PropertyId::kBorderRightStyle,
                   PropertyId::kBorderBottomStyle, PropertyId::kBorderLeftStyle}) {
      EXPECT_EQ(EnumValueToCssString(p, 0), StringView("none"));
      EXPECT_EQ(EnumValueToCssString(p, 1), StringView("solid"));
      EXPECT_EQ(EnumValueToCssString(p, 2), StringView("dashed"));
      EXPECT_EQ(EnumValueToCssString(p, 3), StringView("dotted"));
    }
  }

  TEST(EnumSerialization, NonEnumPropertyReturnsEmpty) {
    EXPECT_TRUE(EnumValueToCssString(PropertyId::kBackgroundColor, 0).empty());
    EXPECT_TRUE(EnumValueToCssString(PropertyId::kWidth, 100).empty());
    EXPECT_TRUE(EnumValueToCssString(PropertyId::kOpacity, 1).empty());
  }

  TEST(EnumSerialization, OutOfRangeEnumValueReturnsEmpty) {
    EXPECT_TRUE(EnumValueToCssString(PropertyId::kDisplay, 99).empty());
    EXPECT_TRUE(EnumValueToCssString(PropertyId::kPosition, 4).empty());
    EXPECT_TRUE(EnumValueToCssString(PropertyId::kVisibility, 2).empty());
  }

  TEST(EnumSerialization, UnknownPropertyReturnsEmpty) {
    EXPECT_TRUE(EnumValueToCssString(PropertyId::kUnknown, 0).empty());
  }

  }  // namespace vx::css
  ```

- [ ] **步骤 2：在 `tests/css/CMakeLists.txt` 注册新测试目标**
  仿照 `selector_matcher_test` 等现有目标格式添加 `add_executable(enum_serialization_test ...)` + `target_link_libraries` + `add_test(...)`。
  **先看一眼现有写法再加**：`Read tests/css/CMakeLists.txt`

- [ ] **步骤 3：构建并运行测试，确认失败**
  ```bash
  cmake --build build --target enum_serialization_test 2>&1 | tail -20
  ```
  预期：链接失败（`undefined reference to vx::css::EnumValueToCssString`）

### 任务 B5.3：实现 enum_serialization.cc [TDD]

**文件：**
- 创建：`veloxa/core/css/enum_serialization.cc`
- 修改：`veloxa/core/css/CMakeLists.txt`（**[共享文件]**）

- [ ] **步骤 1：编写实现**
  完整 13 类 enum 反查表（参见设计规格 §4.1.2）。每个表 `constexpr const char*` 数组 + `EnumTable GetEnumTable(PropertyId)` switch + `EnumValueToCssString` 单一入口。

- [ ] **步骤 2：在 `veloxa/core/css/CMakeLists.txt` 注册新源文件**
  在 `vx_css` 目标的 `target_sources(vx_css PRIVATE ...)` 列表追加 `enum_serialization.cc`，把头文件加入对应 `target_sources(... PUBLIC FILE_SET HEADERS ...)` 区段（如已有此模式）。
  **先看一眼现有写法再加**。

- [ ] **步骤 3：构建并运行测试，确认通过**
  ```bash
  cmake --build build --target enum_serialization_test && \
  ctest --test-dir build --output-on-failure -R enum_serialization
  ```
  预期：9 个测试全部 PASS

- [ ] **步骤 4：提交**
  ```bash
  git add veloxa/core/css/enum_serialization.{h,cc} veloxa/core/css/CMakeLists.txt \
          tests/css/enum_serialization_test.cc tests/css/CMakeLists.txt
  git commit -m "$(cat <<'EOF'
  feat(css): add EnumValueToCssString for PropertyId-aware enum reverse-lookup

  - 新建 enum_serialization.{h,cc}：13 类 CSS enum 反查表
  - 9 个单元测试覆盖：合法值、边界值、非 enum 属性、未知 property
  - 设计模式：枚举 + 元数据表驱动（与 property.cc 同构）
  - 用途：dom_bindings.cc StyleGetProp 的 Enum 读路径（#46 续作）

  Refs TASK-20260419-01 Part B5
  EOF
  )"
  ```

### 任务 B5.4：dom_bindings.cc 接入 EnumValueToCssString [TDD]

**文件：**
- 修改：`veloxa/script/dom_bindings.cc`
- 修改：`tests/script/dom_bindings_test.cc`

- [ ] **步骤 1：写失败测试**
  在 `tests/script/dom_bindings_test.cc` 现有的 `StyleGetDisplayEnumCurrentlyEmpty` 测试旁追加：
  ```cpp
  TEST_F(DomBindingsTest, StyleGetDisplayBlock) {
    SetInlineDisplay("block");
    JSValue v = EvalJs("document.getElementById('t').style.display");
    EXPECT_EQ(JsToString(v), "block");
    JS_FreeValue(ctx_, v);
  }

  TEST_F(DomBindingsTest, StyleGetDisplayFlex) {
    SetInlineDisplay("flex");
    JSValue v = EvalJs("document.getElementById('t').style.display");
    EXPECT_EQ(JsToString(v), "flex");
    JS_FreeValue(ctx_, v);
  }

  TEST_F(DomBindingsTest, StyleGetDisplayNone) {
    SetInlineDisplay("none");
    JSValue v = EvalJs("document.getElementById('t').style.display");
    EXPECT_EQ(JsToString(v), "none");
    JS_FreeValue(ctx_, v);
  }
  ```
  辅助方法 `SetInlineDisplay`、`EvalJs`、`JsToString` 若现有 fixture 没有，则按现有 test 文件里的模式补充（`StyleGetWidthPx` 等已有相似辅助）。

- [ ] **步骤 2：删除旧的 `StyleGetDisplayEnumCurrentlyEmpty` 测试**
  该测试断言「Enum 属性读返回 ""」，本任务正是要打破此约束。删除整个 TEST_F。

- [ ] **步骤 3：构建并运行，确认失败**
  ```bash
  cmake --build build --target dom_bindings_test && \
  ctest --test-dir build --output-on-failure -R dom_bindings_test
  ```
  预期：3 个新测试 FAIL（StyleGetProp 还没接入反查表）

- [ ] **步骤 4：修改 dom_bindings.cc 的 SerializeCssValue**
  - `SerializeCssValue` 签名加 `css::PropertyId prop` 参数
  - `kEnum` 分支调 `out->append(css::EnumValueToCssString(prop, v.enum_value))`
  - `StyleGetProp` 内调用处传 `kStyleMappings[magic].prop`
  - `#include "veloxa/core/css/enum_serialization.h"`

- [ ] **步骤 5：构建并运行，确认通过**
  ```bash
  cmake --build build --target dom_bindings_test && \
  ctest --test-dir build --output-on-failure -R dom_bindings
  ```
  预期：所有 dom_bindings 测试 PASS

- [ ] **步骤 6：跑全量回归**
  ```bash
  ctest --test-dir build --output-on-failure 2>&1 | tail -5
  ```
  预期：≥ 856 + 9（新单元）+ 3（新集成）− 1（删除旧）= 867 通过

- [ ] **步骤 7：提交**
  ```bash
  git add veloxa/script/dom_bindings.cc tests/script/dom_bindings_test.cc
  git commit -m "$(cat <<'EOF'
  refactor(script): wire StyleGetProp through EnumValueToCssString (#46 followup)

  - SerializeCssValue 增 PropertyId 参数；kEnum 分支走 EnumValueToCssString
  - 新增 3 个集成测试：StyleGetDisplayBlock/Flex/None
  - 移除占位测试 StyleGetDisplayEnumCurrentlyEmpty（约束已打破）

  Refs TASK-20260419-01 Part B5
  EOF
  )"
  ```

---

## Phase B6 — EventManager DestructionObserver（30 分钟，TDD）

### 任务 B6.1：写失败测试 [TDD]

**文件：**
- 修改：`tests/event/event_manager_test.cc`

- [ ] **步骤 1：检查现有测试文件结构**
  ```bash
  rg "TEST" tests/event/event_manager_test.cc | head -5
  ```

- [ ] **步骤 2：追加新测试**
  ```cpp
  TEST(EventManagerDestruction, ObserverFiredOnDestroy) {
    bool fired = false;
    {
      event::EventManager em;
      em.AddDestructionObserver([&fired]() { fired = true; });
      EXPECT_FALSE(fired);
    }
    EXPECT_TRUE(fired);
  }

  TEST(EventManagerDestruction, MultipleObserversFiredInRegistrationOrder) {
    Vector<int> order;
    {
      event::EventManager em;
      em.AddDestructionObserver([&order]() { order.push_back(1); });
      em.AddDestructionObserver([&order]() { order.push_back(2); });
      em.AddDestructionObserver([&order]() { order.push_back(3); });
    }
    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
    EXPECT_EQ(order[2], 3);
  }

  TEST(EventManagerDestruction, NoObserverNoCrash) {
    event::EventManager em;
    // empty destruction observer list — should not crash on ~em
    SUCCEED();
  }
  ```

- [ ] **步骤 3：构建并运行，确认失败**
  ```bash
  cmake --build build --target event_manager_test 2>&1 | tail -10
  ```
  预期：编译失败（`AddDestructionObserver` 未声明）

### 任务 B6.2：实现 EventManager 析构观察者 [TDD]

**文件：**
- 修改：`veloxa/core/event/event_manager.h`
- 修改：`veloxa/core/event/event_manager.cc`

- [ ] **步骤 1：修改 event_manager.h**
  - `class EventManager` 添加 `using DestructionObserver = std::function<void()>;`
  - 添加 public 方法：`void AddDestructionObserver(DestructionObserver cb);`
  - 添加显式析构函数声明：`~EventManager();`（**必须** non-default，否则向量销毁顺序不确定）
  - 添加 private 字段：`Vector<DestructionObserver> destruction_observers_;`

- [ ] **步骤 2：修改 event_manager.cc**
  - 实现 `AddDestructionObserver`：`destruction_observers_.push_back(std::move(cb));`
  - 实现 `~EventManager()`：遍历 `destruction_observers_` 调用每个回调（带 nullptr 检查）

- [ ] **步骤 3：构建并运行，确认通过**
  ```bash
  cmake --build build --target event_manager_test && \
  ctest --test-dir build --output-on-failure -R event_manager
  ```
  预期：3 个新测试 PASS，旧测试零回归

- [ ] **步骤 4：提交**
  ```bash
  git add veloxa/core/event/event_manager.{h,cc} tests/event/event_manager_test.cc
  git commit -m "$(cat <<'EOF'
  feat(event): add EventManager::AddDestructionObserver (#50 followup infra)

  - Push-style destruction notification (与 SetInvalidationCallback 同构)
  - 3 个单元测试：单一/多个/空 observer 场景
  - 用途：让 DomBindings 在反向析构时清空 em_ 指针，防 UAF

  Refs TASK-20260419-01 Part B6 infrastructure
  EOF
  )"
  ```

### 任务 B6.3：DomBindings 注册 DestructionObserver [TDD]

**文件：**
- 修改：`veloxa/script/dom_bindings.cc`
- 修改：`tests/script/dom_bindings_test.cc`

- [ ] **步骤 1：写失败集成测试**
  ```cpp
  TEST_F(DomBindingsTest, EventManagerDestroyedFirstThenUnbindSafe) {
    auto em = std::make_unique<event::EventManager>();
    DomBindings db;
    db.Bind(ctx_, doc_.get(), em.get());
    // Register a listener so DomBindings actually holds the EM pointer
    EvalJs("document.getElementById('t').addEventListener('click', function() {})");
    // Destroy EM first (反序)
    em.reset();
    // Now Unbind must NOT crash (em_ should have been nulled by observer)
    db.Unbind();
    SUCCEED();
  }
  ```

- [ ] **步骤 2：构建并运行，确认失败（或 ASAN 报 UAF）**
  ```bash
  cmake --build build --target dom_bindings_test && \
  ctest --test-dir build --output-on-failure -R EventManagerDestroyedFirstThenUnbindSafe
  ```
  预期：崩溃 / UAF 报告 / 段错误

- [ ] **步骤 3：修改 DomBindings::Bind，注册 destruction observer**
  在 `Bind` 末尾：
  ```cpp
  if (data_->em) {
    data_->em->AddDestructionObserver([data_ptr = data_.get()]() {
      data_ptr->em = nullptr;
    });
  }
  ```

- [ ] **步骤 4：构建并运行，确认通过**
  ```bash
  cmake --build build --target dom_bindings_test && \
  ctest --test-dir build --output-on-failure -R dom_bindings
  ```
  预期：新测试 PASS，旧测试零回归

- [ ] **步骤 5：提交**
  ```bash
  git add veloxa/script/dom_bindings.cc tests/script/dom_bindings_test.cc
  git commit -m "$(cat <<'EOF'
  fix(script): register DestructionObserver to null em on reverse destroy (#50)

  - DomBindings::Bind 注册 observer：em 析构时把 data_->em 置 nullptr
  - Unbind 已有 if (data_->em) 检查，新增的 nullptr 路径自动安全
  - 集成测试 EventManagerDestroyedFirstThenUnbindSafe 验证反向析构无崩溃

  Refs TASK-20260419-01 Part B6
  EOF
  )"
  ```

---

## Phase B7 — Listener Token + 精确移除（60 分钟，TDD）

### 任务 B7.1：EventDispatcher Token 化 [TDD]

**文件：**
- 修改：`veloxa/core/event/event_dispatcher.h`
- 修改：`veloxa/core/event/event_dispatcher.cc`
- 修改/新建：`tests/event/event_dispatcher_test.cc`

- [ ] **步骤 1：检查 event_dispatcher_test.cc 是否存在**
  ```bash
  ls tests/event/event_dispatcher_test.cc 2>/dev/null && echo "exists" || echo "missing"
  ```
  若不存在，本步骤合并到 `tests/event/event_manager_test.cc`（无需新增 CMakeLists 注册）。

- [ ] **步骤 2：写失败测试**
  ```cpp
  TEST(EventDispatcherToken, AddReturnsUniqueToken) {
    event::EventDispatcher d;
    auto* el = MakeFakeElement();  // 现有测试 fixture 风格
    u64 t1 = d.AddEventListener(el, EventType::kClick, [](DOMEvent&){});
    u64 t2 = d.AddEventListener(el, EventType::kClick, [](DOMEvent&){});
    EXPECT_GE(t1, 1u);
    EXPECT_NE(t1, t2);
  }

  TEST(EventDispatcherToken, RemoveByTokenRemovesOnly) {
    event::EventDispatcher d;
    auto* el = MakeFakeElement();
    int call1 = 0, call2 = 0;
    u64 t1 = d.AddEventListener(el, EventType::kClick, [&](DOMEvent&){ ++call1; });
    u64 t2 = d.AddEventListener(el, EventType::kClick, [&](DOMEvent&){ ++call2; });
    EXPECT_TRUE(d.RemoveEventListenerByToken(el, t1));
    DOMEvent ev{...};  // click event
    d.Dispatch(ev);
    EXPECT_EQ(call1, 0);
    EXPECT_EQ(call2, 1);
  }

  TEST(EventDispatcherToken, RemoveUnknownTokenReturnsFalse) {
    event::EventDispatcher d;
    auto* el = MakeFakeElement();
    EXPECT_FALSE(d.RemoveEventListenerByToken(el, 999));
  }
  ```

- [ ] **步骤 3：构建并运行，确认失败**

- [ ] **步骤 4：修改 event_dispatcher.h**
  - `EventListener` 加 `u64 token = 0;`
  - `AddEventListener` 返回 `u64`
  - 新增 `bool RemoveEventListenerByToken(dom::Element*, u64);`
  - 新增 private 成员 `u64 next_token_ = 1;`

- [ ] **步骤 5：修改 event_dispatcher.cc**
  - `AddEventListener` 内 `u64 t = next_token_++; listeners_[el].push_back({type, std::move(handler), use_capture, t}); return t;`
  - `RemoveEventListenerByToken` 在该 element 的 vector 内 swap-and-pop 匹配 token 的 listener

- [ ] **步骤 6：构建并运行，确认通过**
  ```bash
  cmake --build build --target event_dispatcher_test 2>&1 | tail -10
  ctest --test-dir build --output-on-failure -R event_dispatcher
  ```
  预期：3 个新测试 PASS

- [ ] **步骤 7：提交**
  ```bash
  git add veloxa/core/event/event_dispatcher.{h,cc} tests/event/event_dispatcher_test.cc
  git commit -m "$(cat <<'EOF'
  feat(event): EventDispatcher token-based listener identity (#47 followup)

  - AddEventListener 返回 u64 token（旧 void 返回值 → 兼容）
  - 新增 RemoveEventListenerByToken(el, token)
  - EventListener struct 增 token 字段
  - 3 个单元测试：唯一性、按 token 移除、未知 token

  Refs TASK-20260419-01 Part B7 infrastructure
  EOF
  )"
  ```

### 任务 B7.2：EventManager 镜像扩展 [TDD]

**文件：**
- 修改：`veloxa/core/event/event_manager.h`
- 修改：`veloxa/core/event/event_manager.cc`
- 修改：`tests/event/event_manager_test.cc`

- [ ] **步骤 1：写失败测试**
  ```cpp
  TEST(EventManagerToken, AddReturnsToken) {
    event::EventManager em;
    auto* el = MakeFakeElement();
    u64 t = em.AddEventListener(el, EventType::kClick, [](DOMEvent&){});
    EXPECT_GE(t, 1u);
  }

  TEST(EventManagerToken, RemoveByTokenRemovesOnly) {
    event::EventManager em;
    auto* el = MakeFakeElement();
    int call1 = 0, call2 = 0;
    u64 t1 = em.AddEventListener(el, EventType::kClick, [&](DOMEvent&){ ++call1; });
    u64 t2 = em.AddEventListener(el, EventType::kClick, [&](DOMEvent&){ ++call2; });
    EXPECT_TRUE(em.RemoveEventListenerByToken(el, t1));
    // 触发 click（用 InputEvent 通过 HandleInput 或直接调内部 dispatcher）
    // 简化：通过 EventManager 的现有 API 触发
    InputEvent input{...};
    em.HandleInput(input, ...);
    EXPECT_EQ(call1, 0);
    EXPECT_EQ(call2, 1);
  }
  ```

- [ ] **步骤 2：修改 event_manager.h**
  - `AddEventListener` 返回 `u64`
  - 新增 `bool RemoveEventListenerByToken(dom::Element*, u64);`

- [ ] **步骤 3：修改 event_manager.cc**
  - 转发到 dispatcher：`return dispatcher_.AddEventListener(...)`、`return dispatcher_.RemoveEventListenerByToken(...)`

- [ ] **步骤 4：构建运行，确认通过**

- [ ] **步骤 5：提交**
  ```bash
  git add veloxa/core/event/event_manager.{h,cc} tests/event/event_manager_test.cc
  git commit -m "$(cat <<'EOF'
  feat(event): EventManager mirrors token-based listener API (#47)

  - AddEventListener 返回 u64 token，转发 dispatcher_
  - RemoveEventListenerByToken 转发
  - 2 个单元测试

  Refs TASK-20260419-01 Part B7
  EOF
  )"
  ```

### 任务 B7.3：dom_bindings.cc 接入精确移除 [TDD]

**文件：**
- 修改：`veloxa/script/dom_bindings.cc`
- 修改：`tests/script/dom_bindings_test.cc`

- [ ] **步骤 1：写失败集成测试**
  ```cpp
  TEST_F(DomBindingsTest, RemoveEventListenerOnlyRemovesMatching) {
    EvalJs(R"(
      var el = document.getElementById('t');
      window.calls = 0;
      var fnA = function() { window.calls += 1; };
      var fnB = function() { window.calls += 10; };
      el.addEventListener('click', fnA);
      el.addEventListener('click', fnB);
      el.removeEventListener('click', fnA);
    )");
    // 触发一次 click
    InjectClick(GetElementById("t"));
    JSValue calls = EvalJs("window.calls");
    EXPECT_EQ(JsToInt(calls), 10);
    JS_FreeValue(ctx_, calls);
  }

  TEST_F(DomBindingsTest, RemoveEventListenerWrongTypeIsNoOp) {
    EvalJs(R"(
      var el = document.getElementById('t');
      window.calls = 0;
      var fn = function() { window.calls += 1; };
      el.addEventListener('click', fn);
      el.removeEventListener('mousemove', fn);  // 错误 type
    )");
    InjectClick(GetElementById("t"));
    JSValue calls = EvalJs("window.calls");
    EXPECT_EQ(JsToInt(calls), 1);
    JS_FreeValue(ctx_, calls);
  }
  ```

- [ ] **步骤 2：构建运行，确认失败**

- [ ] **步骤 3：修改 dom_bindings.cc**
  - 在 `InstanceData` 增 `HashMap<ListenerKey, u64> listener_tokens;` 字段；定义 `ListenerKey` + `ListenerKeyHash`（位置：匿名 namespace 顶部，与现有 helper struct 一致）
  - 修改 `ElementAddEventListener`：调 `data->em->AddEventListener(...)` 接收返回的 token；`data->listener_tokens.Insert({el, event_type, JS_VALUE_GET_PTR(callback)}, token);`
  - 重写 `ElementRemoveEventListener`：解析 `argv[0]` 为 EventType，按 `(el, type, JS_VALUE_GET_PTR(argv[1]))` 在 `listener_tokens` 查 token；找到 → `em->RemoveEventListenerByToken(el, token)` + `listener_tokens.Erase(key)`；找不到 → 静默
  - **保留** `listener_elements` 用于 `Unbind`（批量清理逻辑不变）
  - 移除原 `ElementRemoveEventListener` 中的 swap-and-pop `listener_elements` 逻辑（因为同元素上其他 listener 仍在）

- [ ] **步骤 4：构建运行，确认通过**
  ```bash
  ctest --test-dir build --output-on-failure -R dom_bindings
  ```
  预期：所有 dom_bindings 测试 PASS（含新增 2 个）

- [ ] **步骤 5：跑全量回归**
  ```bash
  ctest --test-dir build --output-on-failure 2>&1 | tail -5
  ```
  预期：全绿，约 ≥ 874 测试

- [ ] **步骤 6：提交**
  ```bash
  git add veloxa/script/dom_bindings.cc tests/script/dom_bindings_test.cc
  git commit -m "$(cat <<'EOF'
  fix(script): removeEventListener now removes only matching (type, handler) (#47)

  - InstanceData 增 listener_tokens: HashMap<(Element*, type, JS ptr), token>
  - ElementAddEventListener 记录 token
  - ElementRemoveEventListener 按 (type, callback) 查 token + ByToken 删除
  - 集成测试 2 个：精确移除、错误 type no-op

  Refs TASK-20260419-01 Part B7
  EOF
  )"
  ```

---

## Phase 8 — 收尾验证与 Memory Bank 更新（20 分钟）

### 任务 8.1：完整回归 + lint

- [ ] **步骤 1：清理重建**
  ```bash
  cmake --build build -j$(nproc) 2>&1 | tail -5
  ```
  预期：无新警告

- [ ] **步骤 2：完整测试**
  ```bash
  ctest --test-dir build --output-on-failure 2>&1 | tail -10
  ```
  预期：100% PASS，新增 ≥ 18 测试

- [ ] **步骤 3：手动验收（按设计规格 §6 验收标准）**
  - `activeContext.md` 待办列表 ≤ 5 条 ✓
  - `StyleGetProp(display)` 返回 `"block"` 等真实值 ✓
  - EM 反序销毁不崩溃 ✓
  - `removeEventListener(type, fn)` 仅移除匹配项 ✓

### 任务 8.2：更新 systemPatterns + techContext

**文件：**
- 修改：`memory-bank/systemPatterns.md`
- 修改：`memory-bank/techContext.md`

- [ ] **步骤 1：systemPatterns 追加新段**
  ```markdown
  ## 已验证的模式（来自 TASK-20260419-01 流程规则沉淀 + P2 收口）

  ### 析构观察者模式（EventManager DestructionObserver）
  - `AddDestructionObserver(cb)` 注册回调，`~EventManager` 遍历触发
  - 与 `SetInvalidationCallback` 同构（push 失效触发）
  - 用途：长寿命外部持有者（DomBindings）安全清空指针，防反向析构 UAF
  - 约束：observer 回调禁止回调 EventManager 任何方法（mid-destruction）

  ### Listener Token 模式（EventDispatcher）
  - AddEventListener 返回单调递增 u64 token（>= 1，0 保留为「未找到」）
  - 跨 API 边界（C++/JS）传递时只暴露 token，不暴露内部 listener struct
  - 调用方维护 (上下文 key) → token 索引，按 key 查 token 再调 RemoveByToken
  - 适用：handler 不可比较（std::function）、需要按 (type, callback) 精确移除的场景

  ### CSS Enum 反查模式（enum_serialization.cc）
  - PropertyId → const char* const[] 二维查表（与 property.cc 同构）
  - 共享枚举（AlignItems / AlignSelf；BorderTopStyle 等 4 边）共用同一表
  - 边界检查：表为空 / 索引越界 → 返回空 StringView
  - 适用：所有 ValueType::kEnum 的 CSS 序列化（StyleGetProp、潜在的 CSSOM 调试输出）
  ```

- [ ] **步骤 2：techContext 技术债清单更新**
  - #46 标 ✅（Enum 读路径已实现）
  - #47 标 ✅（精确移除已实现）
  - #50 标 ✅（反向析构观察者已实现）
  - 30 EventDispatcher::listeners_ 元素销毁需手动 RemoveEventListeners → 标注「现有约束未变；可考虑后续加 Element 析构钩子」

- [ ] **步骤 3：提交**
  ```bash
  git add memory-bank/systemPatterns.md memory-bank/techContext.md
  git commit -m "$(cat <<'EOF'
  docs(memory-bank): record patterns and tech debt closures from TASK-20260419-01

  - systemPatterns：析构观察者 / Listener Token / CSS Enum 反查 三模式
  - techContext：#46 #47 #50 标记 ✅

  Refs TASK-20260419-01
  EOF
  )"
  ```

### 任务 8.3：交接到 /reflect

- [ ] **步骤 1：更新 progress.md**
  追加：
  ```markdown
  - 2026-04-19 Phase B5/B6/B7 完成：3 条 P2 技术债已收口，新增 ~18 测试，全量 ≥ 874 PASS。
  - 准备进入 /reflect。
  ```

- [ ] **步骤 2：更新 activeContext.md 当前阶段为「构建中」→ 准备 `/reflect`**
  保留任务焦点信息，待 `/reflect` 切换至 `回顾中`。

- [ ] **步骤 3：提交**
  ```bash
  git add memory-bank/progress.md memory-bank/activeContext.md
  git commit -m "$(cat <<'EOF'
  docs(memory-bank): mark TASK-20260419-01 build complete; ready for /reflect

  Refs TASK-20260419-01
  EOF
  )"
  ```

---

## 提交计数预期

| Phase | 提交数 |
|-------|-------|
| A1 | 5 |
| A2 | 3 |
| A3 | 2 |
| A4 | 2 |
| B5 | 2 |
| B6 | 2 |
| B7 | 3 |
| 8 | 2 |
| **合计** | **21** |

## 测试增量预期

| Phase | 新增测试 |
|-------|---------|
| B5 | 9（enum_serialization）+ 3（dom_bindings StyleGetDisplay）− 1（删除 EnumCurrentlyEmpty）= **+11** |
| B6 | 3（EventManagerDestruction）+ 1（DomBindings 反序销毁）= **+4** |
| B7 | 3（EventDispatcher Token）+ 2（EventManager Token）+ 2（DomBindings 精确移除）= **+7** |
| **合计** | **+22**（基线 856 → 预期 ≥ 878） |

## 子代理使用建议

本任务**不推荐使用子代理**：
- Part A 全部为 .mdc 文件追加段，主线程直接执行更可靠（避免子代理重写整个 mdc）
- Part B 每个 Phase 涉及的代码量小（< 100 行），主线程 TDD 循环效率更高
- 跨 Phase 状态（`InstanceData` 增字段、`AddEventListener` 返回类型变化）由主线程统一掌握更安全

如需子代理（仅作备选），按 Phase 切分（每个 B5/B6/B7 一个子代理），prompt 必须包含本计划对应 Phase 的全部步骤 + 设计规格 §4 对应小节。

## 执行交接

计划保存到 `docs/plans/2026-04-19-rules-and-debt.md`。下一步：

- 设计已无需 `/creative` 阶段（头脑风暴已固化方案）
- 直接 `/build` 进入实现
