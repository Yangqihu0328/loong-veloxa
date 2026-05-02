# DevTool Phase A · Inspector 实施计划（TASK-20260502-01）

**目标：** 在 Veloxa 引擎实现首套 DevTool 子系统 Inspector — DOM tree / Computed Style / LayoutBox panel 三件 + 鼠标 hover 元素 DisplayList overlay 红框高亮 + F12 toggle splitter dock，闭环历史技术债 #26（LayoutBox.Dump）+ #40（C API introspection）。

**架构：** 新建静态库 `vx_devtool`（`veloxa/devtool/inspector/`），DevTool UI 自身用 Veloxa HTML/CSS/JS 自渲染（V3=A dogfood）；与目标 View 共享单进程 Application，通过双 Document 槽（I1：`target_document_` + `devtool_document_`）+ DisplayList overlay 注入（I3）+ 双层 C API（D7=C 内部 C++ + 公开 thin wrapper）暴露 introspection；Inspector hover 选取走内部 C++ API 零拷贝 hot path，外部 CDP/IDE 接入走公开 C API JSON 序列化层；安全守卫 T3 redaction / T5 overlay 隔离 / T7 buffer overflow 双调用模式。

**技术栈：** C++17 / CMake / SDL2 2.0.20 / GTest / 既有 `vx_core` + `vx_script` + `vx_platform` + `vx_api`（零新 FetchContent 依赖）

**复杂度级别：** Level 3（中等功能 — 跨 4 子系统 core/devtool/api/tests，但蓝图 plan 已就绪 16 子任务，无新架构决策）

**任务 ID：** TASK-20260502-01

**分支：** `feature/TASK-20260502-01-devtool-inspector`（基于 main `679304e` — 含 TASK-30-04 蓝图全部归档 + TASK-30-03 codebase review R2 quick fix 6 项）

**关联文档：**

- spec：[`docs/specs/2026-04-30-devtool-design.md`](../specs/2026-04-30-devtool-design.md)（A1-A5, A13, A14 + T3/T5/T7/T8 + I1/I3/I4/I5/I6 + R1/R2/R5）
- 蓝图 plan：[`docs/plans/2026-04-30-devtool.md`](2026-04-30-devtool.md)（§Phase A 16 子任务清单）
- creative #1：[`memory-bank/creative/creative-devtool-screen-layout.md`](../../memory-bank/creative/creative-devtool-screen-layout.md)（splitter dock + HUD overlay 5 决策）

**plan ×0.6 估时（沿用蓝图基线，reflect 阶段实测沉淀第 18 数据点）：** 总 ~441 min plan ×0.6（7.35 h）

---

## Plan 阶段 build 级精化决策（B1-B8，2026-05-02 12:50 锁定）

| # | 决策 | 锁定值 | 理由 |
|:-:|---|---|---|
| **B1** | plan 文档定位 | **A** 新建独立 plan | 与 archive 边界清晰；蓝图 plan 不修改保归档完整 |
| **B2** | A.0 改造顺序 | **A** 严格串行 A.0.1 → 0.2 → 0.3 → 0.4 → 0.5 → 0.6 | commit 链清晰；reflect 易回溯；单 agent 无平行加速 |
| **B3** | 测试组织 | **B** 混合 — A.0 改造 → `tests/core/`，A.1+ DevTool 子系统 → 新建 `tests/devtool/inspector/` | A.0 仍属 core 子系统行为变化；A.1+ 才是 DevTool 子系统 |
| **B4** | DevTool UI 资源策略 | **B** 独立 `.html/.css/.js` 文件 + runtime 文件读取 | 与蓝图 §0.2 Q6 决策一致（一期不走 binary blob 工具链）；`vx_view_attach_devtool` API 透传 resource 路径 |
| **B5** | 编译开关 `VX_BUILD_DEVTOOL` | **OFF**（蓝图已锁） | A14 验收依赖：DevTool 关闭时 binary size diff = 0 |
| **B6** | 提交粒度 | **A** 每子任务 1 commit（~16 + reflect/archive ≈ 18 commits） | 与 fast-fix 12 min/项 colour ate 一致；reflect 易回溯 |
| **B7** | plan ×0.6 估时校准 | **A** 沿用蓝图估时 7.35 h plan ×0.6（baseline） | reflect 阶段实测沉淀第 18 数据点（review 类下限 vs 大件类 0.8-1.2× 验证） |
| **B8** | spec 复用 vs 新建 | **A** 复用 TASK-30-04 spec | 蓝图 spec 已涵盖 A1-A5/A13/A14 + T3/T5/T7/T8 + I1/I3/I4/I5/I6 + R1/R2/R5 |

---

## Phase 0 — 全局约束与基线核验（11 子段，2026-05-02 12:50 实测）

### 0.1 ctest 基线 reconfigure + 实测 1062 确认（关键 — 当前 build/ 过期）

**实测发现：** 当前 `build/` 目录 CMakeCache.txt mtime = 2026-04-26，**早于** TASK-30-03 R2.5 新增 `DecodeFromFileRejectsOversizedFile` 单测（2026-04-30 提交 `9c6ad5f`）；`ctest -N` 输出 1061，与 R2.5 落地后预期 1062 不符。

**Phase A 启动前必跑：**

```bash
# Step 1: reconfigure（保留 build-bench/）
cmake -S . -B build -DVX_BUILD_TESTS=ON
# Step 2: 全量构建
cmake --build build -j$(nproc) 2>&1 | tail -30
# Step 3: ctest 基线确认
cd build && ctest --output-on-failure 2>&1 | tail -5
# 预期：1062 tests, 1061 passed, 1 skipped (Wpt001)
```

**Acceptance：** ctest 报告 `100% tests passed, 0 tests failed out of 1062`（Wpt001 Skip 沿用 TASK-26-01 沉淀状态）

### 0.2 CMake 链接方向约束分析（writing-plans §CMake 链接方向约束分析必填）

复用蓝图 [`docs/plans/2026-04-30-devtool.md`](2026-04-30-devtool.md) §任务 0.2 Q1-Q6 全文，无修订。本任务 Phase A 仅引入新静态库 `vx_devtool`（`veloxa/devtool/inspector/` 子目录），上游链 `vx_core` + `vx_script` + `vx_platform` + `vx_api` 全 PUBLIC ✅。

**Phase A 实施样板**（写入 `veloxa/devtool/CMakeLists.txt`）：

```cmake
add_library(vx_devtool STATIC
  inspector/inspector_data.cc
  inspector/inspector_overlay.cc
)
target_include_directories(vx_devtool PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(vx_devtool
  PUBLIC vx_core vx_script vx_platform
)

# DevTool resource 一期 runtime 文件读取（B4=B），不走 binary blob
```

根 `CMakeLists.txt` 增量：

```cmake
option(VX_BUILD_DEVTOOL "Build DevTool subsystem (Inspector / Overlay / Hot Reload)" OFF)
if(VX_BUILD_DEVTOOL)
  add_subdirectory(veloxa/devtool)
endif()
```

**A14 守门：** `cmake -S . -B build` 默认 `VX_BUILD_DEVTOOL=OFF` → `veloxa/devtool/` 子目录不参与构建 → binary size 与 main `679304e` 完全一致。

### 0.3 静态库循环依赖审计（writing-plans §静态库循环依赖审计必填）

复用蓝图 §任务 0.3 全文。`vx_devtool → vx_core` 单向链（B 链 A 模式），✅ 无循环。

**Phase A 新符号归属**：

| 符号 | 归属库 | 调用方 |
|---|---|---|
| `vx::layout::LayoutBox::ToJson()` | `vx_core` | `vx_devtool/inspector_data.cc` |
| `vx::dom::Serializer::ToJson(Node*, RedactionPolicy)` | `vx_core` | `vx_devtool/inspector_data.cc` |
| `vx::render::PaintCommand::Type::kOverlayHighlight` | `vx_core` | `vx_devtool/inspector_overlay.cc` |
| `vx::devtool::*` 全部符号 | `vx_devtool`（新） | `tests/devtool/inspector/*_test.cc` + `examples/hello_devtool.cc` |
| `vx_view_serialize_dom_json` 等 C API | `vx_api` | 用户空间 C 调用 |

### 0.4 FetchContent 网络代理守卫

复用蓝图 §任务 0.4 — **跳过条件命中**（Phase A 零新 FetchContent 依赖；既有 quickjs / google-benchmark 已离线 `_deps/`）。

### 0.5 测试基础设施审计（writing-plans §测试基础设施审计必填）

| 子任务 | 测试需访问的内部状态 | 当前访问路径 | 缺失补救 |
|---|---|---|---|
| **A.0.1** Application 双 Document 槽 | `target_document_` / `devtool_document_` 字段 | 当前 `document_` PRIVATE | 加 PUBLIC `target_document() / devtool_document()` getters（read-only 自然语义）|
| **A.0.2** LayoutBox.ToJson | LayoutBox 全字段 | 已 PUBLIC（struct）✓ | — |
| **A.0.3** DOM Serializer.ToJson redaction | Element.GetAttribute("type") | 已 PUBLIC ✓ | — |
| **A.0.4** PaintCommand kOverlayHighlight + ResetOverlayCommands | DisplayList vector | 已 PUBLIC（test 友好）✓ | — |
| **A.0.5** inspector_data 内部 API | DOM/Layout/Style 全部 PUBLIC | 已 ✓ | — |
| **A.0.6** C API thin wrapper | 通过 VxView 句柄 | 已 ✓ | — |
| **A.1.1** InspectorOverlay hover | EventManager.hover_target | 当前 PRIVATE | 加 `friend class InspectorOverlayTest;` 白名单 OR PUBLIC getter（hover_target 已是 read-only 状态）|
| **A.1.2-A.1.4** DevTool UI | DevTool Document.outerHTML | 已 PUBLIC ✓ | — |

**禁止项重申：** ❌ `#define private public` / ❌ `reinterpret_cast` 越权 / ❌ 在生产代码 PUBLIC 暴露内部 mutable 状态。

### 0.6 边界输入清单（writing-plans §边界输入清单必填，T3/T5/T7 强相关）

| 类别 | 示例 | 预期行为 | 对应威胁 / 子任务 |
|---|---|---|---|
| 默认路径 | `<div id=root>...</div>` 100 元素 DOM | 正常 JSON 序列化 | A.0.3 / A.0.5 |
| 空 buf | `vx_view_serialize_dom_json(view, NULL, NULL)` | 返回所需 size，不 segfault | T7 / A.0.6 / A.2.3 |
| 0 长度 / nullptr buf | `vx_view_serialize_dom_json(view, NULL, &len)` | 返回 len，不写 buf | T7 / A.0.6 / A.2.3 |
| `<input type="password">` | password input | value 字段 → `[REDACTED]` | T3 / A.0.3 / A.2.1 |
| `<input>` 无 type | type 默认 `text` | value 不 redact | T3 / A.2.1 |
| 极深 DOM | 10000 层嵌套 div | DOM JSON max_size 16 MiB → 拒绝大返回 `kOutOfMemory` | T7 / A.0.6 / A.2.3 |
| 极端 ComputedStyle | inline style 1000+ properties | JSON max_size 1 MiB → 拒绝大 | T7 / A.0.6 / A.2.3 |
| nullptr Application | `Inspector::OnHover(nullptr)` | 静默忽略 | A.1.1 |
| hover 同元素重复 | 100ms 内 5 次 hover 同 node | overlay 命令幂等（每帧开头 reset）| T5 / A.0.4 / A.2.2 |
| DevTool OFF 时 hover | `VX_BUILD_DEVTOOL=OFF` 编译 + 运行 | hover 无 overlay 注入；零行为变化 | A14 / A.2.4 |

**对应 ≥ 14 个边界测试**，分布在 A.0 改造测试（tests/core/）+ A.1+ DevTool 子系统测试（tests/devtool/inspector/）+ A.2 安全单测。

### 0.7 调用链端到端验证（writing-plans §调用链端到端验证必填，I1 改造关键 — 实测）

**实测（2026-05-02 12:30）：** `rg "->document\(\)|\.document\(\)" --type cpp` veloxa/ tests/ examples/ → **5 处命中**（4 处 `app.document()` in `tests/core/application_test.cc` lines 53/60/72/208 + 1 处 `bindings.document()` in `tests/script/dom_bindings_test.cc:508` 不相关）。

**真实 callsite = 4 处**（远低于蓝图 §9 R1 估的 10-15 处）→ R1 风险等级 🟡→🟢-🟡 进一步降级。

| 层 | 文件 + 函数 | 当前 | 改后 | 透传？ |
|:-:|---|---|---|:-:|
| 1 | `vx_view_create()` `veloxa_api.cc` | 创建单 Document → `Application` 内 `document_` | 改读用 `target_document()` getter | ✅ |
| 2 | `Application::ctor` `application.cc` | `document_ = ...` | `target_document_ = ...; devtool_document_ = nullptr;` | ✅ 重命名 |
| 3 | `Application::document()` getter `application.h:52` | `return *document_;` | **移除** + 加 `target_document() / devtool_document()` getters | ✅ 编译期暴露漏改 |
| 4 | `Application::Update()` / `OnFrame()` `application.cc` | 内部 `document_` 用法 | 改 `target_document_` 引用 | ✅ |
| 5 | `tests/core/application_test.cc` lines 53/60/72/208 | `app.document()` | `app.target_document()` | ✅ |

**A.0.1 收尾验证脚本：**

```bash
rg "->document\(\)" --type cpp veloxa/ examples/ | grep -v "_test\.cc" | grep -v "// OK:"
# 预期：0 命中（全部改名为 target_document() / devtool_document()）

rg "\.document\(\)" tests/ | grep -v "dom_bindings_test\.cc"
# 预期：0 命中（全部 application_test.cc 4 处改名为 .target_document()）
```

### 0.8 既有测试隐式契约 fingerprint（writing-plans §既有测试隐式契约必填，实测）

| 子系统 | 测试文件 | 关键字 | 实测命中 vs 期望 |
|---|---|---|:-:|
| event | `tests/core/event/*_test.cc` | `EXPECT_*hover\|propagated\|target` | 2 ≥ 3 ⚠️（`event_types_test.cc:2` 偏少）→ A.0/A.1 改造前补 grep 扩展关键字 `hovered_node` / `OnMouseMove` |
| layout | `tests/core/layout/*_test.cc` | `EXPECT_FLOAT_EQ\|content_height\|collapsed` | **65 ≥ 3 ✅** 充足 |
| dom serializer | `tests/core/dom/serializer_test.cc` | `EXPECT_EQ.*Serialize` | 已实证 ✓（grep `Serialize` ≥ 3 处）|
| render | `tests/core/render/renderer_test.cc` | `EXPECT_EQ.*PaintCommand\|command_count` | **17 ≥ 3 ✅** 充足 |
| application | `tests/core/application_test.cc` | `EXPECT_*document\(\)` | **1 ≥ 2 ⚠️**（仅 line 53 `EXPECT_EQ(app.document(), nullptr)`；其他 3 处是 `EXPECT_NE` / `auto* node = ...`）|

**判读：** event + application 子系统命中略低 → A.0.1 / A.1.1 实施前补 `tests/core/application_test.cc` + `tests/core/event/event_types_test.cc` 完整阅读，标注每条隐含的边界假设（如「`app.document()` 在 `LoadHTML` 后非空」「event_target 状态在 hover 切换时更新」）；不阻塞实施但避免无声退化。

### 0.9 CSS shorthand 能力 grep 表（writing-plans §CSS shorthand 能力 grep 表必填，DevTool UI 自渲染相关）

DevTool UI（`devtool/inspector/inspector_panel.{html,css}`）使用 shorthand：

| shorthand | 验证命令 | 实测命中 | 缺失兜底 |
|---|---|---|---|
| `flex` | `rg "kFlex\b" veloxa/core/css/` | 6 处 ✅ | — |
| `padding` | `rg "kPadding\b" veloxa/core/css/parser.cc` | TASK-30-02 已落地 ✓ | — |
| `border` | `rg "kBorder\b\|ParseBorderShorthand" veloxa/core/css/parser.cc` | TASK-30-02 已落地 ✓ | — |
| `background` | A.1.2 build 阶段 grep | 待 grep | longhand：`background-color` |
| `font` | A.1.2 build 阶段 grep | 待 grep | longhand：`font-size` + `font-family` + `font-weight` |
| `border-radius` | A.1.2 build 阶段 grep | 待 grep | 无圆角 fallback（DevTool UI 视觉略硬，可接受）|

**A.1.2 build 阶段先跑此 grep 表**，缺失项改用 longhand 或列入 P3 候选（不在本任务内扩展 parser）。

### 0.10 工具链可用性（writing-plans §smoke 工具链可用性检查必做，实测）

| 工具 | 实测 | 兜底 |
|---|:-:|---|
| `rg` | ✅ `/usr/bin/rg`（cursor-shipped）| — |
| `awk` | ✅ `/usr/bin/awk` | — |
| `python3` | ✅ `/usr/bin/python3` | — |
| `jq` | ❌ 缺失 | A.2.4 binary size diff 用 `python3 -c` 替代 jq JSON 解析 |
| `inotifywait` | — Phase A 不涉及（C 阶段才需） | — |

### 0.11 测试组织（B3 决策实施细节）

| 测试范围 | 目录 | 文件举例 |
|---|---|---|
| A.0 前置改造（core 子系统行为变化） | `tests/core/` 既有 | `tests/core/application_test.cc`（A.0.1 改名）/ `tests/core/layout/layout_box_test.cc`（A.0.2 ToJson）/ `tests/core/dom/serializer_test.cc`（A.0.3 ToJson + redaction）/ `tests/core/render/renderer_test.cc`（A.0.4 overlay tag）|
| A.0.5 / A.0.6 C API + 内部 C++ API | 新建 `tests/devtool/inspector/` + `tests/api/` 既有 | `tests/devtool/inspector/inspector_data_test.cc`（A.0.5）/ `tests/api/devtool_api_test.cc`（A.0.6 + A.2.3 T7 守卫）|
| A.1+ DevTool 子系统 | 新建 `tests/devtool/inspector/` | `tests/devtool/inspector/inspector_overlay_test.cc`（A.1.1）/ `tests/devtool/inspector/devtool_ui_test.cc`（A.1.2-A.1.4）|
| A.2 安全单测 | 与对应子任务测试合并 | T3 → `dom/serializer_test.cc` 内 / T5 → `tests/devtool/inspector/inspector_overlay_test.cc` 内 / T7 → `tests/api/devtool_api_test.cc` 内 |

**新建 `tests/devtool/CMakeLists.txt` + `tests/devtool/inspector/CMakeLists.txt`** 注册新单测目标。

---

## Phase A.0 — 前置改造（6 子任务，串行，~189 min plan ×0.6）

### 任务 A.0.1：I1 Application 双 Document 槽改造 + 全 callsite 重命名 [TDD]

**文件：**

- 修改：`veloxa/core/application.h`（字段重命名 + getter 拆分）
- 修改：`veloxa/core/application.cc`（构造 / OnFrame / Update 内部用法替换）
- 修改：`tests/core/application_test.cc`（4 处 `app.document()` → `app.target_document()`）
- 测试：复用既有 `tests/core/application_test.cc`（覆盖 RED → GREEN）

**估时：** 90 min plan / 54 min plan ×0.6

- [ ] **步骤 1：编写失败测试（RED）**

  在 `tests/core/application_test.cc` 末尾新增：

  ```cpp
  TEST(ApplicationTest, DevtoolDocumentSlotDefaultsToNull) {
    platform::HeadlessEventLoop loop;
    auto config = MakeConfig(&loop);
    Application app(config);
    EXPECT_NE(app.target_document(), nullptr);
    EXPECT_EQ(app.devtool_document(), nullptr);
  }
  ```

- [ ] **步骤 2：运行测试验证失败**

  ```bash
  cmake --build build -j --target application_test 2>&1 | tail -5
  ```

  预期：编译失败（`target_document` / `devtool_document` 未定义）

- [ ] **步骤 3：实施 GREEN（重命名 + 加槽）**

  `veloxa/core/application.h` line 52-72 修改：

  ```cpp
   public:
    // ... existing methods ...

    // 双 Document 槽（DevTool 三件套 I1 改造，TASK-20260502-01 A.0.1）：
    // - target_document_：业务页面 DOM 树（原 document_ 重命名）
    // - devtool_document_：DevTool UI DOM 树（仅 VX_BUILD_DEVTOOL=ON + 用户调
    //   vx_view_attach_devtool 后非空）；nullable
    dom::Document* target_document() const { return target_document_; }
    dom::Document* devtool_document() const { return devtool_document_; }

   private:
    Config config_;
    dom::Document* target_document_ = nullptr;     // 重命名 from document_
    dom::Document* devtool_document_ = nullptr;    // 新增槽
    // ... rest unchanged ...
  ```

  `veloxa/core/application.cc` 全文 `document_` → `target_document_` 替换（grep + sed 批量；保留析构释放仅 target_document_，devtool_document_ 由 DevTool 子系统拥有）

- [ ] **步骤 4：修改全 callsite（4 处）**

  ```bash
  # 4 处 app.document() → app.target_document()
  sed -i 's/app\.document()/app.target_document()/g' tests/core/application_test.cc
  ```

  注：dom_bindings_test.cc:508 `bindings.document()` 是 DomBindings 自有 method，**不动**。

- [ ] **步骤 5：运行全测试验证通过**

  ```bash
  cmake --build build -j 2>&1 | tail -10
  cd build && ctest -R "application_test|dom_bindings_test" --output-on-failure
  ```

  预期：全 PASS；ctest 总数仍 1062（新增 1 测 + 隐式 1 测 NoOp，待 Step 7 确认）

  实际新增 1 个 test → ctest 总数 = 1063（如果 GoogleTest 自动发现）

- [ ] **步骤 6：A.0.1 收尾验证脚本**

  ```bash
  rg "->document\(\)" --type cpp veloxa/ examples/ | grep -v "_test\.cc" | grep -v "// OK:"
  rg "\.document\(\)" tests/ | grep -v "dom_bindings_test\.cc"
  # 双脚本预期：0 命中
  ```

- [ ] **步骤 7：提交**

  ```bash
  git add veloxa/core/application.{h,cc} tests/core/application_test.cc
  git commit -m "refactor(application): I1 dual Document slot — target_document_ + devtool_document_

  Phase A.0.1 of TASK-20260502-01 (DevTool Inspector). Renames Application::document_
  to target_document_ and adds nullable devtool_document_ slot. Removes the legacy
  document() getter; all callsites must use target_document() / devtool_document()
  to make missed updates fail at compile time (R1 mitigation).

  Real callsite count: 4 (in tests/core/application_test.cc), well below blueprint
  estimate of 10-15. ctest 1063/1063 PASS (+1 new test for slot defaults).
  "
  ```

---

### 任务 A.0.2：I4 LayoutBox::ToJson() — 闭环技术债 #26 [TDD]

**文件：**

- 修改：`veloxa/core/layout/layout_box.h`（加 `BasicString ToJson() const` 方法）
- 修改：`veloxa/core/layout/CMakeLists.txt`（如新增 `layout_box.cc`，本任务也可全 inline 在头）
- 测试：`tests/core/layout/layout_box_test.cc`（新增 ≥ 3 测：基本几何 / margin collapse 状态 / 子树递归）

**估时：** 30 min plan / 18 min plan ×0.6

- [ ] **步骤 1：编写失败测试（RED）**

  在 `tests/core/layout/layout_box_test.cc` 末尾新增：

  ```cpp
  TEST(LayoutBoxTest, ToJsonBasicGeometry) {
    LayoutBox box;
    box.x = 10.5f;
    box.y = 20.0f;
    box.content_width = 100.0f;
    box.content_height = 50.0f;
    box.padding[LayoutBox::kTop] = 4.0f;
    box.border[LayoutBox::kLeft] = 1.0f;
    box.margin[LayoutBox::kBottom] = 8.0f;

    String json = box.ToJson();
    EXPECT_TRUE(json.contains("\"x\":10.5"));
    EXPECT_TRUE(json.contains("\"content_width\":100"));
    EXPECT_TRUE(json.contains("\"padding\":[4,0,0,0]"));
    EXPECT_TRUE(json.contains("\"border\":[0,0,0,1]"));
    EXPECT_TRUE(json.contains("\"margin\":[0,0,8,0]"));
  }

  TEST(LayoutBoxTest, ToJsonMarginCollapseState) {
    LayoutBox box;
    box.collapsed_through = true;
    box.margin_top_collapsed_into_ancestor = true;
    String json = box.ToJson();
    EXPECT_TRUE(json.contains("\"collapsed_through\":true"));
    EXPECT_TRUE(json.contains("\"margin_top_collapsed_into_ancestor\":true"));
  }
  ```

- [ ] **步骤 2：运行测试验证失败**

  ```bash
  cmake --build build -j --target layout_box_test 2>&1 | tail -5
  ```

  预期：编译失败（`LayoutBox::ToJson` 未定义）

- [ ] **步骤 3：实施 GREEN（plan 阶段完整代码）**

  `veloxa/core/layout/layout_box.h` 末尾（`}; // struct LayoutBox` 前）新增：

  ```cpp
    // Serialize layout box to JSON (DevTool Inspector A4 acceptance, TASK-20260502-01).
    // Closes tech debt #26 (LayoutBox.Dump). Output schema:
    // { "type": "block", "x": ..., "y": ..., "content_width": ..., ...,
    //   "padding":[t,r,b,l], "border":[t,r,b,l], "margin":[t,r,b,l],
    //   "collapsed_through": bool, ... }
    String ToJson() const;
  ```

  新建 `veloxa/core/layout/layout_box.cc`（plan 阶段提供完整实现）：

  ```cpp
  #include "veloxa/core/layout/layout_box.h"

  #include "veloxa/foundation/strings/string_format.h"

  namespace vx::layout {

  namespace {

  StringView LayoutTypeName(LayoutType t) {
    switch (t) {
      case LayoutType::kBlock: return "block";
      case LayoutType::kInline: return "inline";
      case LayoutType::kFlex: return "flex";
      case LayoutType::kText: return "text";
      case LayoutType::kReplaced: return "replaced";
    }
    return "unknown";
  }

  }  // namespace

  String LayoutBox::ToJson() const {
    String out;
    out.Reserve(512);
    StringFormat(&out,
        "{\"type\":\"%s\",\"x\":%g,\"y\":%g,"
        "\"content_width\":%g,\"content_height\":%g,"
        "\"padding\":[%g,%g,%g,%g],"
        "\"border\":[%g,%g,%g,%g],"
        "\"margin\":[%g,%g,%g,%g],"
        "\"collapsed_through\":%s,"
        "\"margin_top_collapsed_into_ancestor\":%s,"
        "\"margin_bottom_collapsed_into_ancestor\":%s,"
        "\"child_count\":%u}",
        LayoutTypeName(type), x, y, content_width, content_height,
        padding[kTop], padding[kRight], padding[kBottom], padding[kLeft],
        border[kTop], border[kRight], border[kBottom], border[kLeft],
        margin[kTop], margin[kRight], margin[kBottom], margin[kLeft],
        collapsed_through ? "true" : "false",
        margin_top_collapsed_into_ancestor ? "true" : "false",
        margin_bottom_collapsed_into_ancestor ? "true" : "false",
        child_count());
    return out;
  }

  }  // namespace vx::layout
  ```

  `veloxa/core/layout/CMakeLists.txt` 加 `layout_box.cc` 源文件（如尚未列入）。

- [ ] **步骤 4：运行测试验证通过**

  ```bash
  cmake --build build -j --target layout_box_test
  cd build && ctest -R layout_box_test --output-on-failure
  ```

  预期：3 新测 PASS + 既有 65 测保持 PASS

- [ ] **步骤 5：反向探针验证（writing-plans §9.3 D3 类必加）**

  临时改 `LayoutBox::ToJson` 为 `return "{}";` → 跑测试 → 确认 3 新测 FAIL → 恢复实现 → 全 PASS。记录到 `progress.md`：「ToJson 反向探针通过 — 实现正确时也能精准 FAIL」。

- [ ] **步骤 6：提交**

  ```bash
  git add veloxa/core/layout/layout_box.{h,cc} veloxa/core/layout/CMakeLists.txt tests/core/layout/layout_box_test.cc
  git commit -m "feat(layout): LayoutBox::ToJson() — close tech debt #26 (DevTool Inspector A4)

  Phase A.0.2 of TASK-20260502-01. Adds LayoutBox::ToJson() serializing geometry,
  box-model arrays (padding/border/margin), and margin-collapse flags to JSON
  for DevTool Inspector Layout panel. Closes tech debt #26 (LayoutBox.Dump).

  ctest 1066/1066 PASS (+3 new tests). Reverse probe verified."
  ```

---

### 任务 A.0.3：I5 DOM Serializer::ToJson(node, RedactionPolicy) [TDD + 安全 T3]

**文件：**

- 修改：`veloxa/core/dom/serializer.h`（加 `ToJson` 重载 + `RedactionPolicy` enum）
- 新建：`veloxa/core/dom/serializer.cc` 增加 ToJson 实现段（如尚未存在 .cc，否则修改）
- 测试：`tests/core/dom/serializer_test.cc`（新增 ≥ 4 测：基本 / nested / password redact / text node）

**估时：** 45 min plan / 27 min plan ×0.6

- [ ] **步骤 1：编写失败测试（RED）**

  在 `tests/core/dom/serializer_test.cc` 末尾新增：

  ```cpp
  TEST(SerializerTest, ToJsonBasicElement) {
    Document doc;
    auto* div = doc.CreateElement("div");
    div->SetAttribute("id", "root");
    doc.AppendChild(div);
    String json = ToJson(div, RedactionPolicy::kDefault);
    EXPECT_TRUE(json.contains("\"tag\":\"div\""));
    EXPECT_TRUE(json.contains("\"attrs\":{\"id\":\"root\"}"));
  }

  TEST(SerializerTest, ToJsonNestedTree) {
    Document doc;
    auto* parent = doc.CreateElement("section");
    auto* child = doc.CreateElement("p");
    parent->AppendChild(child);
    doc.AppendChild(parent);
    String json = ToJson(parent, RedactionPolicy::kDefault);
    EXPECT_TRUE(json.contains("\"children\":[{\"tag\":\"p\""));
  }

  // T3 安全：password input 默认 redact
  TEST(SerializerTest, ToJsonRedactsPasswordInputValue) {
    Document doc;
    auto* input = doc.CreateElement("input");
    input->SetAttribute("type", "password");
    input->SetAttribute("value", "mySecret123");
    doc.AppendChild(input);
    String json = ToJson(input, RedactionPolicy::kDefault);
    EXPECT_TRUE(json.contains("\"value\":\"[REDACTED]\""));
    EXPECT_FALSE(json.contains("mySecret123"));
  }

  // T3 安全：非 password input 不 redact
  TEST(SerializerTest, ToJsonDoesNotRedactNonPasswordInput) {
    Document doc;
    auto* input = doc.CreateElement("input");
    input->SetAttribute("type", "text");
    input->SetAttribute("value", "publicValue");
    doc.AppendChild(input);
    String json = ToJson(input, RedactionPolicy::kDefault);
    EXPECT_TRUE(json.contains("\"value\":\"publicValue\""));
  }
  ```

- [ ] **步骤 2：运行测试验证失败**

  ```bash
  cmake --build build -j --target serializer_test 2>&1 | tail -5
  ```

  预期：编译失败（`ToJson` / `RedactionPolicy` 未定义）

- [ ] **步骤 3：实施 GREEN**

  `veloxa/core/dom/serializer.h` 末尾（`} // namespace vx::dom` 前）新增：

  ```cpp
  // RedactionPolicy controls whether sensitive data in the DOM is redacted before
  // JSON serialization for DevTool Inspector. T3 mitigation (TASK-20260502-01 A.0.3).
  enum class RedactionPolicy : u8 {
    kDefault,   // Redacts <input type="password"> value to "[REDACTED]"
    kNone,      // No redaction (caller must ensure local-only display)
  };

  // Serialize a DOM tree (or subtree) to a JSON string for DevTool Inspector.
  // Schema: { "tag": "...", "attrs": {...}, "children": [...] } for elements;
  //         { "text": "..." } for text nodes.
  // T3 (Inspector sensitive data redaction) — see RedactionPolicy.
  String ToJson(const Node* node, RedactionPolicy policy);
  ```

  `veloxa/core/dom/serializer.cc` 增量（plan 阶段完整实现 ~80 行）：

  ```cpp
  // 复用现有 String 流构造模式 — JsonEscape 字符串转义 + 递归 children。
  // 参考 Serializer::Serialize() 的 HTML 输出，改为 JSON。
  String ToJson(const Node* node, RedactionPolicy policy) {
    String out;
    out.Reserve(1024);
    if (!node) {
      out.Append("null");
      return out;
    }
    if (node->IsText()) {
      out.Append("{\"text\":\"");
      JsonEscape(node->AsText()->data(), &out);
      out.Append("\"}");
      return out;
    }
    if (node->IsElement()) {
      const auto* elem = node->AsElement();
      out.Append("{\"tag\":\"");
      out.Append(elem->tag_name());
      out.Append("\",\"attrs\":{");
      bool first = true;
      bool is_password = false;
      for (const auto& [name, value] : elem->attributes()) {
        if (!first) out.Append(",");
        first = false;
        out.Append("\"");
        JsonEscape(name, &out);
        out.Append("\":\"");
        if (name == "type" && value == "password") is_password = true;
        // Apply T3 redaction: password input value → [REDACTED]
        if (policy == RedactionPolicy::kDefault &&
            elem->tag_name() == "input" && name == "value" &&
            elem->GetAttribute("type") == "password") {
          out.Append("[REDACTED]");
        } else {
          JsonEscape(value, &out);
        }
        out.Append("\"");
      }
      out.Append("}");
      // children
      if (elem->first_child()) {
        out.Append(",\"children\":[");
        bool first_child = true;
        for (const auto* child = elem->first_child(); child; child = child->next_sibling()) {
          if (!first_child) out.Append(",");
          first_child = false;
          out.Append(ToJson(child, policy));
        }
        out.Append("]");
      }
      out.Append("}");
      return out;
    }
    out.Append("null");
    return out;
  }

  // Helper: escape JSON string special chars
  static void JsonEscape(StringView in, String* out) {
    for (char c : in) {
      switch (c) {
        case '"':  out->Append("\\\""); break;
        case '\\': out->Append("\\\\"); break;
        case '\n': out->Append("\\n"); break;
        case '\r': out->Append("\\r"); break;
        case '\t': out->Append("\\t"); break;
        default:
          if (static_cast<u8>(c) < 0x20) {
            // Control chars → \u00XX
            char buf[8];
            std::snprintf(buf, sizeof(buf), "\\u%04x", c);
            out->Append(buf);
          } else {
            out->Append(StringView(&c, 1));
          }
      }
    }
  }
  ```

- [ ] **步骤 4：运行测试验证通过**

  ```bash
  cmake --build build -j --target serializer_test
  cd build && ctest -R serializer_test --output-on-failure
  ```

  预期：4 新测 PASS

- [ ] **步骤 5：反向探针验证（T3 redact 必检）**

  临时改 redact 条件为 `if (false)` → 跑 `ToJsonRedactsPasswordInputValue` 测 → 确认 FAIL → 恢复 → PASS。记录 progress.md。

- [ ] **步骤 6：提交**

  ```bash
  git add veloxa/core/dom/serializer.{h,cc} tests/core/dom/serializer_test.cc
  git commit -m "feat(dom): DOM Serializer::ToJson(node, RedactionPolicy) — T3 mitigation

  Phase A.0.3 of TASK-20260502-01. Adds ToJson() overload producing JSON tree for
  DevTool Inspector DOM panel (A1 acceptance). Implements T3 (Inspector sensitive
  data redaction): RedactionPolicy::kDefault redacts <input type=password> value
  to [REDACTED] (A5 acceptance). Closes tech debt #40 (C API introspection foundation).

  ctest 1070/1070 PASS (+4 new tests). T3 reverse probe verified."
  ```

---

### 任务 A.0.4：I3 PaintCommand kOverlayHighlight + ResetOverlayCommands [TDD + 安全 T5]

**文件：**

- 修改：`veloxa/core/render/paint_command.h`（加 `kOverlayHighlight` enum 值 + 工厂方法）
- 修改：`veloxa/core/render/renderer.h/.cc`（加 `ResetOverlayCommands()` API + 渲染顺序固定末位）
- 测试：`tests/core/render/renderer_test.cc`（新增 ≥ 3 测：tag 注册 / 渲染顺序末位 / Reset 清除）

**估时：** 30 min plan / 18 min plan ×0.6

- [ ] **步骤 1：编写失败测试（RED）**

  在 `tests/core/render/renderer_test.cc` 末尾新增：

  ```cpp
  TEST(RendererTest, OverlayHighlightCommandRendersAfterTargetCommands) {
    // 设构造 displaylist：[FillRect(target), OverlayHighlight(devtool)]
    // 验证渲染顺序：target FillRect 先于 OverlayHighlight 落到 canvas
    Renderer renderer;
    SoftwareCanvas canvas(100, 100);
    DisplayList list = {
      PaintCommand::FillRect({0, 0, 100, 100}, gfx::Color::White()),
      PaintCommand::OverlayHighlight({10, 10, 30, 30}, gfx::Color::Red(), 2.0f),
    };
    renderer.Render(list, &canvas);
    // 验证 (10,10) 是红色 outline
    EXPECT_EQ(canvas.PixelAt(10, 10), gfx::Color::Red());
    // 验证 outline 内部 (15, 15) 仍是白色（不填充）
    EXPECT_EQ(canvas.PixelAt(15, 15), gfx::Color::White());
  }

  TEST(RendererTest, ResetOverlayCommandsClearsAndKeepsTargetCommands) {
    DisplayList list = {
      PaintCommand::FillRect({0, 0, 100, 100}, gfx::Color::White()),
      PaintCommand::OverlayHighlight({10, 10, 30, 30}, gfx::Color::Red(), 2.0f),
    };
    Renderer::ResetOverlayCommands(&list);
    EXPECT_EQ(list.size(), 1u);
    EXPECT_EQ(list[0].type, PaintCommand::Type::kFillRect);
  }
  ```

- [ ] **步骤 2：运行测试验证失败** → 编译失败（`kOverlayHighlight` / `ResetOverlayCommands` 未定义）

- [ ] **步骤 3：实施 GREEN**

  `veloxa/core/render/paint_command.h` enum class Type 末尾加 `kOverlayHighlight,`，并新增工厂方法：

  ```cpp
  // DevTool overlay highlight (red outline) — T5 mitigation (TASK-20260502-01 A.0.4).
  // Rendered after all target Document commands; ResetOverlayCommands() clears
  // these tags each frame to prevent stale overlay leakage.
  static PaintCommand OverlayHighlight(const gfx::Rect& r, gfx::Color c,
                                        f32 stroke_width) {
    return {Type::kOverlayHighlight, r, c, stroke_width, 0, {}, 0};
  }
  ```

  `veloxa/core/render/renderer.h` 增量：

  ```cpp
  // T5 mitigation: clear all kOverlayHighlight commands from the list (called
  // each frame start by DevTool to prevent overlay propagation across frames).
  static void ResetOverlayCommands(DisplayList* list);
  ```

  `veloxa/core/render/renderer.cc` 增量：

  ```cpp
  void Renderer::ResetOverlayCommands(DisplayList* list) {
    list->RemoveIf([](const PaintCommand& cmd) {
      return cmd.type == PaintCommand::Type::kOverlayHighlight;
    });
  }
  ```

  在 `Renderer::Render()` switch 中加 `case PaintCommand::Type::kOverlayHighlight:` 分支：调 `canvas->StrokeRect(cmd.rect, cmd.color, cmd.param /* stroke_width */)`（既有 API）。

- [ ] **步骤 4：运行测试验证通过**

  ```bash
  cmake --build build -j --target renderer_test
  cd build && ctest -R renderer_test --output-on-failure
  ```

  预期：2 新测 PASS + 既有 17 测保持 PASS

- [ ] **步骤 5：反向探针验证（T5 隔离必检）**

  临时改 `ResetOverlayCommands` 为空函数 → 跑 `ResetOverlayCommandsClearsAndKeepsTargetCommands` 测 → 确认 FAIL → 恢复 → PASS。

- [ ] **步骤 6：提交**

  ```bash
  git commit -m "feat(render): PaintCommand kOverlayHighlight + ResetOverlayCommands — T5 mitigation

  Phase A.0.4 of TASK-20260502-01. Adds new PaintCommand::Type::kOverlayHighlight
  for DevTool Inspector hover element highlighting (A2 acceptance). T5: per-frame
  ResetOverlayCommands clears stale overlay tags to prevent propagation; rendering
  order fixed at end-of-list to overlay all target Document commands.

  ctest 1072/1072 PASS (+2 new tests). T5 reverse probe verified."
  ```

---

### 任务 A.0.5：C API 内部 C++ 层 `inspector_data.h` [TDD]

**文件：**

- 新建：`veloxa/devtool/CMakeLists.txt`（注册 `vx_devtool` 静态库）
- 新建：`veloxa/devtool/inspector/inspector_data.h`（内部 C++ API — DevTool 直用零拷贝）
- 新建：`veloxa/devtool/inspector/inspector_data.cc`
- 新建：`tests/devtool/CMakeLists.txt` + `tests/devtool/inspector/CMakeLists.txt`（B3 决策实施）
- 新建：`tests/devtool/inspector/inspector_data_test.cc`

**估时：** 60 min plan / 36 min plan ×0.6

- [ ] **步骤 1：编写失败测试（RED）**

  ```cpp
  // tests/devtool/inspector/inspector_data_test.cc
  #include "veloxa/devtool/inspector/inspector_data.h"
  #include <gtest/gtest.h>

  using namespace vx;
  using namespace vx::devtool;

  TEST(InspectorDataTest, SerializeDocumentReturnsJsonTree) {
    dom::Document doc;
    auto* root = doc.CreateElement("html");
    doc.AppendChild(root);
    String json = SerializeDocument(&doc, dom::RedactionPolicy::kDefault);
    EXPECT_TRUE(json.contains("\"tag\":\"html\""));
  }

  TEST(InspectorDataTest, SerializeNodeLayoutBoxAttachedReturnsBoxJson) {
    dom::Document doc;
    auto* div = doc.CreateElement("div");
    doc.AppendChild(div);
    layout::LayoutBox box;
    box.element = div;
    box.x = 10;
    box.content_width = 100;
    String json = SerializeLayoutBox(&box);
    EXPECT_TRUE(json.contains("\"x\":10"));
  }

  TEST(InspectorDataTest, SerializeComputedStyleReturnsPropertiesJson) {
    css::ComputedStyle style;
    style.SetProperty(css::PropertyId::kColor, css::Value::Color({255, 0, 0, 255}));
    String json = SerializeComputedStyle(&style);
    EXPECT_TRUE(json.contains("\"color\":\"rgba(255,0,0,1)\""));
  }
  ```

- [ ] **步骤 2：运行测试验证失败** → 编译失败（库 / 头未存在）

- [ ] **步骤 3：实施 GREEN — 文件结构**

  `veloxa/devtool/CMakeLists.txt`：

  ```cmake
  add_library(vx_devtool STATIC
    inspector/inspector_data.cc
  )
  target_include_directories(vx_devtool PUBLIC ${PROJECT_SOURCE_DIR})
  target_link_libraries(vx_devtool
    PUBLIC vx_core
  )
  ```

  根 `CMakeLists.txt` 增加（line 36 `add_subdirectory(veloxa/api)` 后）：

  ```cmake
  option(VX_BUILD_DEVTOOL "Build DevTool subsystem (Inspector / Overlay / Hot Reload)" OFF)
  if(VX_BUILD_DEVTOOL)
    add_subdirectory(veloxa/devtool)
  endif()
  ```

  `tests/CMakeLists.txt` 加：

  ```cmake
  if(VX_BUILD_DEVTOOL)
    add_subdirectory(devtool)
  endif()
  ```

  `tests/devtool/CMakeLists.txt` + `tests/devtool/inspector/CMakeLists.txt`：

  ```cmake
  # tests/devtool/CMakeLists.txt
  add_subdirectory(inspector)

  # tests/devtool/inspector/CMakeLists.txt
  add_executable(inspector_data_test inspector_data_test.cc)
  target_link_libraries(inspector_data_test PRIVATE vx_devtool vx_core GTest::gtest_main)
  add_test(NAME inspector_data_test COMMAND inspector_data_test)
  ```

  `veloxa/devtool/inspector/inspector_data.h`：

  ```cpp
  #ifndef VELOXA_DEVTOOL_INSPECTOR_INSPECTOR_DATA_H_
  #define VELOXA_DEVTOOL_INSPECTOR_INSPECTOR_DATA_H_

  #include "veloxa/core/dom/document.h"
  #include "veloxa/core/dom/serializer.h"
  #include "veloxa/core/layout/layout_box.h"
  #include "veloxa/core/css/computed_style.h"
  #include "veloxa/foundation/strings/string.h"

  namespace vx::devtool {

  // Internal C++ API for DevTool Inspector (D7=C 第一层 — 零拷贝 hot path).
  // Unlike the public C API thin wrapper (vx_view_serialize_*, A.0.6), these
  // return BasicString directly and avoid double-call protocol overhead.

  // SerializeDocument — DOM tree JSON for Inspector DOM panel (A1 acceptance).
  String SerializeDocument(const dom::Document* doc, dom::RedactionPolicy policy);

  // SerializeLayoutBox — single LayoutBox JSON for Inspector Layout panel (A4).
  // Returns "null" if box is nullptr.
  String SerializeLayoutBox(const layout::LayoutBox* box);

  // SerializeComputedStyle — ComputedStyle JSON for Inspector Style panel (A3).
  String SerializeComputedStyle(const css::ComputedStyle* style);

  }  // namespace vx::devtool

  #endif
  ```

  `veloxa/devtool/inspector/inspector_data.cc`：

  ```cpp
  #include "veloxa/devtool/inspector/inspector_data.h"

  namespace vx::devtool {

  String SerializeDocument(const dom::Document* doc, dom::RedactionPolicy policy) {
    if (!doc) return String("null");
    String out;
    out.Reserve(2048);
    out.Append("{\"document\":[");
    bool first = true;
    for (const auto* child = doc->first_child(); child; child = child->next_sibling()) {
      if (!first) out.Append(",");
      first = false;
      out.Append(dom::ToJson(child, policy));
    }
    out.Append("]}");
    return out;
  }

  String SerializeLayoutBox(const layout::LayoutBox* box) {
    if (!box) return String("null");
    return box->ToJson();
  }

  String SerializeComputedStyle(const css::ComputedStyle* style) {
    if (!style) return String("null");
    String out;
    out.Reserve(1024);
    out.Append("{");
    // 遍历所有 properties，输出 "key":"value" 对（plan 阶段简化版本）
    bool first = true;
    style->ForEachProperty([&](css::PropertyId id, const css::Value& value) {
      if (!first) out.Append(",");
      first = false;
      out.Append("\"");
      out.Append(css::PropertyIdToString(id));
      out.Append("\":\"");
      out.Append(value.ToCssString());
      out.Append("\"");
    });
    out.Append("}");
    return out;
  }

  }  // namespace vx::devtool
  ```

  注：`ComputedStyle::ForEachProperty` 与 `Value::ToCssString()` 是 build 阶段需 grep 验证的既有 API；如不存在则补加（A.0.5 步骤 3 内）。

- [ ] **步骤 4：运行测试验证通过**

  ```bash
  cmake -S . -B build -DVX_BUILD_DEVTOOL=ON
  cmake --build build -j --target inspector_data_test
  cd build && ctest -R inspector_data_test --output-on-failure
  ```

  预期：3 新测 PASS

- [ ] **步骤 5：A14 守门验证（DevTool OFF 时回归）**

  ```bash
  cmake -S . -B build-noffi -DVX_BUILD_DEVTOOL=OFF
  cmake --build build-noffi -j 2>&1 | tail -5
  cd build-noffi && ctest --output-on-failure 2>&1 | tail -3
  # 预期：1072/1072 PASS（Phase A.0.4 后基线）+ A14 ✅
  ```

- [ ] **步骤 6：提交**

  ```bash
  git add veloxa/devtool/ tests/devtool/ CMakeLists.txt tests/CMakeLists.txt
  git commit -m "feat(devtool): vx_devtool static lib + Inspector internal C++ API

  Phase A.0.5 of TASK-20260502-01. Creates new veloxa/devtool/ subdirectory with
  vx_devtool static library (linked PUBLIC vx_core). Adds inspector_data.h with
  internal C++ API (D7=C 第一层 — zero-copy hot path): SerializeDocument /
  SerializeLayoutBox / SerializeComputedStyle. Behind VX_BUILD_DEVTOOL=OFF default.

  ctest VX_BUILD_DEVTOOL=ON: 1075/1075 PASS (+3 new tests).
  ctest VX_BUILD_DEVTOOL=OFF (A14): 1072/1072 PASS (no regression)."
  ```

---

### 任务 A.0.6：C API 公开 thin wrapper `vx_view_serialize_*` [TDD + 安全 T7]

**文件：**

- 修改：`veloxa/api/veloxa_api.h`（加 3 C API + double-call 协议文档）
- 修改：`veloxa/api/veloxa_api.cc`（实现 thin wrapper + max_size 守卫）
- 新建：`tests/api/devtool_api_test.cc`（T7 双调用模式 + max_size 守卫）

**估时：** 60 min plan / 36 min plan ×0.6

- [ ] **步骤 1：编写失败测试（RED）**

  ```cpp
  // tests/api/devtool_api_test.cc
  #include "veloxa/api/veloxa_api.h"
  #include <gtest/gtest.h>
  #include <vector>

  TEST(DevtoolApiTest, SerializeDomDoubleCallProtocolFirstReturnsLen) {
    auto* loop = vx_event_loop_create_headless();
    auto* surface = vx_surface_create_memory(100, 100);
    VxViewConfig config{loop, surface, 60, 0xFFFFFFFF};
    auto* view = vx_view_create(&config);
    vx_view_load_html(view, "<html><body><div id=root></div></body></html>", 44);

    // First call: out_buf=NULL → returns required size
    uint32_t needed = 0;
    VxResult r = vx_view_serialize_dom_json(view, nullptr, &needed, 16 * 1024 * 1024);
    EXPECT_EQ(r, VX_OK);
    EXPECT_GT(needed, 0u);

    // Second call: caller alloc + receive
    std::vector<char> buf(needed);
    uint32_t written = needed;
    r = vx_view_serialize_dom_json(view, buf.data(), &written, 16 * 1024 * 1024);
    EXPECT_EQ(r, VX_OK);
    EXPECT_GT(written, 0u);
    EXPECT_NE(std::string(buf.data(), written).find("div"), std::string::npos);

    vx_view_destroy(view);
    vx_surface_destroy(surface);
    vx_event_loop_destroy(loop);
  }

  TEST(DevtoolApiTest, SerializeDomNullViewReturnsError) {
    uint32_t needed = 0;
    VxResult r = vx_view_serialize_dom_json(nullptr, nullptr, &needed, 16 * 1024 * 1024);
    EXPECT_EQ(r, VX_ERROR_NULL_PARAM);
  }

  // T7 安全：max_size 超限拒绝
  TEST(DevtoolApiTest, SerializeDomMaxSizeExceededReturnsOutOfMemory) {
    auto* loop = vx_event_loop_create_headless();
    auto* surface = vx_surface_create_memory(100, 100);
    VxViewConfig config{loop, surface, 60, 0xFFFFFFFF};
    auto* view = vx_view_create(&config);

    // 构造大 DOM（10000 div，>1 MiB JSON）
    std::string html = "<html><body>";
    for (int i = 0; i < 10000; ++i) html += "<div id=\"d" + std::to_string(i) + "\"></div>";
    html += "</body></html>";
    vx_view_load_html(view, html.c_str(), html.size());

    uint32_t needed = 0;
    // 设 max_size = 1 MiB（小于实际所需）
    VxResult r = vx_view_serialize_dom_json(view, nullptr, &needed, 1024 * 1024);
    EXPECT_EQ(r, VX_ERROR_OUT_OF_MEMORY);

    vx_view_destroy(view);
    vx_surface_destroy(surface);
    vx_event_loop_destroy(loop);
  }
  ```

- [ ] **步骤 2：运行测试验证失败** → 编译失败（API 未定义）

- [ ] **步骤 3：实施 GREEN**

  `veloxa/api/veloxa_api.h` 末尾（`#endif` 前）增加：

  ```c
  /* ── DevTool Inspector C API (TASK-20260502-01 A.0.6) ──────────── */
  /* These functions follow the double-call protocol (T7 mitigation):
   *  1. First call with out_buf=NULL → returns needed size in *out_len
   *  2. Caller allocates buf of size *out_len
   *  3. Second call writes JSON to out_buf
   *
   * max_size: hard limit on serialization size (default 16 MiB DOM, 1 MiB style/box).
   * Returns VX_ERROR_OUT_OF_MEMORY if needed > max_size (no allocation performed).
   *
   * Available only when built with -DVX_BUILD_DEVTOOL=ON; otherwise returns
   * VX_ERROR_INVALID_STATE.
   */
  VxResult vx_view_serialize_dom_json(VxView* view, char* out_buf,
                                       uint32_t* out_len, uint32_t max_size);

  /* node_id is a stable handle obtained from a previous DOM JSON tree's
   * "_node_id" attribute (assigned during serialization). */
  VxResult vx_node_get_computed_style_json(VxView* view, uint64_t node_id,
                                            char* out_buf, uint32_t* out_len,
                                            uint32_t max_size);

  VxResult vx_node_get_layout_box_json(VxView* view, uint64_t node_id,
                                        char* out_buf, uint32_t* out_len,
                                        uint32_t max_size);

  /* Result codes — extension */
  /* (Add to existing VxResult enum:)
   *   VX_ERROR_OUT_OF_MEMORY = -3
   *   VX_ERROR_NOT_FOUND = -4
   */
  ```

  `veloxa/api/veloxa_api.cc` 增加 thin wrapper 实现（plan 阶段约 ~50 行代码 + max_size 守卫）：

  ```cpp
  VxResult vx_view_serialize_dom_json(VxView* view, char* out_buf,
                                       uint32_t* out_len, uint32_t max_size) {
    if (!view || !out_len) return VX_ERROR_NULL_PARAM;
  #ifndef VX_BUILD_DEVTOOL
    return VX_ERROR_INVALID_STATE;
  #else
    auto& app = view->app();
    auto* doc = app.target_document();
    if (!doc) {
      *out_len = 0;
      return VX_OK;
    }
    String json = vx::devtool::SerializeDocument(doc, dom::RedactionPolicy::kDefault);
    if (json.size() > max_size) {
      return VX_ERROR_OUT_OF_MEMORY;
    }
    if (!out_buf) {
      *out_len = static_cast<uint32_t>(json.size());
      return VX_OK;
    }
    if (*out_len < json.size()) {
      return VX_ERROR_OUT_OF_MEMORY;  // caller buf too small
    }
    std::memcpy(out_buf, json.data(), json.size());
    *out_len = static_cast<uint32_t>(json.size());
    return VX_OK;
  #endif
  }
  // vx_node_get_computed_style_json / vx_node_get_layout_box_json 同模式
  ```

  `VxResult` enum 扩展（`veloxa_api.h`）：

  ```c
  typedef enum {
    VX_OK = 0,
    VX_ERROR_NULL_PARAM = -1,
    VX_ERROR_INVALID_STATE = -2,
    VX_ERROR_OUT_OF_MEMORY = -3,  // (new) T7 mitigation — DOM/style/box JSON > max_size
    VX_ERROR_NOT_FOUND = -4,      // (new) node_id not found in current document
  } VxResult;
  ```

- [ ] **步骤 4：运行测试验证通过**

  ```bash
  cmake --build build -j --target devtool_api_test
  cd build && ctest -R devtool_api_test --output-on-failure
  ```

  预期：3 新测 PASS

- [ ] **步骤 5：T7 反向探针验证**

  临时去掉 `max_size` 检查 → 跑 `SerializeDomMaxSizeExceededReturnsOutOfMemory` 测 → 确认 FAIL → 恢复 → PASS。

- [ ] **步骤 6：A14 守门验证 + 提交**

  ```bash
  # A14 OFF 测试：确认 stub 路径不增加 binary size 显著
  cmake -S . -B build-noffi -DVX_BUILD_DEVTOOL=OFF
  cmake --build build-noffi -j --target veloxa
  ls -la build/libvx_api.a build-noffi/libvx_api.a
  # 预期：size diff < 1 KiB（仅 stub 路径返回 INVALID_STATE）

  git commit -m "feat(api): vx_view_serialize_*_json — DevTool C API thin wrapper + T7

  Phase A.0.6 of TASK-20260502-01. Adds 3 public C APIs (vx_view_serialize_dom_json,
  vx_node_get_computed_style_json, vx_node_get_layout_box_json) as thin wrappers
  over devtool::Serialize*. T7 mitigation: double-call protocol (out_buf=NULL → size,
  alloc, second call → write) + max_size hard limit (default 16 MiB DOM, 1 MiB box).

  Closes tech debt #40 (C API DOM/Style/Layout introspection).

  ctest VX_BUILD_DEVTOOL=ON: 1078/1078 PASS (+3 new tests). A14 OFF: 1072 stable."
  ```

---

## Phase A.1 — DevTool UI 实施（4 子任务，~144 min plan ×0.6）

### 任务 A.1.1：InspectorOverlay hover highlight 注入 [TDD]

**文件：**

- 新建：`veloxa/devtool/inspector/inspector_overlay.{h,cc}`
- 新建：`tests/devtool/inspector/inspector_overlay_test.cc`

**估时：** 45 min plan / 27 min plan ×0.6

**关键 API（plan 阶段锁定签名）：**

```cpp
namespace vx::devtool {
class InspectorOverlay {
 public:
  // 注入 hover 元素的 highlight 命令到 target Document 的 DisplayList
  // T5 mitigation: 仅追加 kOverlayHighlight tag；调用前不 reset（reset 在帧首端做）
  static void InjectHoverHighlight(dom::Document* target_doc,
                                    const dom::Element* hovered_element,
                                    gfx::Color highlight_color = gfx::Color::Red(),
                                    f32 stroke_width = 2.0f);
};
}
```

**5 步流程同 A.0.x 模板**（RED test → GREEN impl → 反向探针 → A14 守门 → commit）。RED 测验证 `dirty_paint_commands_` 末尾追加 1 条 `kOverlayHighlight` 命令；反向探针把工厂改返回 `kFillRect` → 测 FAIL。

### 任务 A.1.2：DevTool UI HTML/CSS/JS 资源（splitter dock + DOM tree view）[TDD]

**文件（B4=B 决策：runtime 文件读取）：**

- 新建：`veloxa/devtool/resources/inspector_panel.html`
- 新建：`veloxa/devtool/resources/inspector_panel.css`
- 新建：`veloxa/devtool/resources/inspector_panel.js`
- 新建：`veloxa/devtool/inspector/inspector_panel_loader.{h,cc}`（runtime 路径加载封装）
- 测试：`tests/devtool/inspector/devtool_ui_test.cc`

**估时：** 90 min plan / 54 min plan ×0.6

**A.1.2 步骤 0 同步检查（writing-plans §验收用例与 example 实现一致性检查必做）：**

```bash
# 验证 HTML 含 splitter dock 结构
rg "splitter\|devtool-dom-tree" veloxa/devtool/resources/inspector_panel.html
# 验证 CSS 用支持的 shorthand
rg "background:|font:|border-radius:" veloxa/devtool/resources/inspector_panel.css
# 缺失的 shorthand 改 longhand
```

**HTML 骨架（plan 阶段锁定）：**

```html
<!-- veloxa/devtool/resources/inspector_panel.html -->
<html>
<head><link rel="stylesheet" href="inspector_panel.css"></head>
<body>
  <div id="devtool-root">
    <div id="devtool-tabs">
      <button data-tab="dom">DOM</button>
      <button data-tab="style">Computed Style</button>
      <button data-tab="layout">LayoutBox</button>
    </div>
    <div id="devtool-content">
      <div id="dom-tree-panel" class="active"></div>
      <div id="style-panel"></div>
      <div id="layout-panel"></div>
    </div>
  </div>
  <script src="inspector_panel.js"></script>
</body>
</html>
```

**CSS 骨架（仅用已验证 shorthand — R2 mitigation）：**

```css
/* veloxa/devtool/resources/inspector_panel.css */
#devtool-root {
  flex: 1 1 auto;
  background-color: #1e1e1e;
  color: #d4d4d4;
  padding: 8px;
}
#devtool-tabs button {
  padding: 4px 8px;
  border: 1px solid #444;
  background-color: #333;
}
/* avoid: flex 边角 / bidi / transform / border-radius (待 grep 确认) */
```

**JS 骨架（DOM tree view + tab toggle）：**

```js
// veloxa/devtool/resources/inspector_panel.js
function renderDomTree() {
  const json = vx_devtool_get_dom_json();  // 注入到 JS 的 native binding
  const panel = document.getElementById("dom-tree-panel");
  panel.innerHTML = renderTreeNode(JSON.parse(json));
}

function renderTreeNode(node) {
  if (node.text) return `<div class="text">${node.text}</div>`;
  let html = `<div class="elem">&lt;${node.tag}&gt;`;
  if (node.children) {
    for (const c of node.children) html += renderTreeNode(c);
  }
  html += `&lt;/${node.tag}&gt;</div>`;
  return html;
}
```

**5 步流程**（RED 验证 panel 加载 + DOM 树渲染 → GREEN 实现 + native binding 桩 → 反向探针 → A14 守门 → commit）。

### 任务 A.1.3：Style panel + Layout panel 数据展示 [TDD]

**文件：**

- 修改：`veloxa/devtool/resources/inspector_panel.js`（加 Style panel + Layout panel 渲染）
- 测试：`tests/devtool/inspector/devtool_ui_test.cc`（扩展）

**估时：** 60 min plan / 36 min plan ×0.6

5 步流程模板。

### 任务 A.1.4：F12 toggle DevTool splitter（`vx_view_attach_devtool` 入口） [TDD]

**文件：**

- 修改：`veloxa/api/veloxa_api.{h,cc}`（加 `vx_view_attach_devtool` API + F12 hotkey）
- 修改：`examples/`（一期 example smoke 直接使用，留 A.3.1 实施）

**估时：** 45 min plan / 27 min plan ×0.6

**API 锁定签名：**

```c
typedef struct {
  const char* resources_dir;     /* 路径到 inspector_panel.html 等资源 (B4=B) */
  uint32_t devtool_width;        /* DevTool splitter 宽度（px），默认 400 */
} VxDevtoolOptions;

VxResult vx_view_attach_devtool(VxView* view, const VxDevtoolOptions* opts);
VxResult vx_view_detach_devtool(VxView* view);
```

5 步流程模板。

---

## Phase A.2 — 安全单测（4 子任务，~63 min plan ×0.6）

### 任务 A.2.1：T3 redaction policy 单测 + `vx_inspector_set_redaction_policy` API [安全]

**文件：**

- 测试：扩展 `tests/core/dom/serializer_test.cc` 已在 A.0.3 覆盖 T3 主路径（password redact / 非 password 不 redact）
- 新增：`vx_inspector_set_redaction_policy()` C API + 1 单测验证 policy 切换

**估时：** 30 min plan / 18 min plan ×0.6

- [ ] **步骤 1：编写安全测试**

  ```cpp
  TEST(DevtoolApiTest, SetRedactionPolicyNoneEmitsPasswordValue) {
    // ... setup view + load HTML with <input type=password value=secret> ...
    vx_inspector_set_redaction_policy(view, VX_REDACTION_NONE);
    char buf[1024];
    uint32_t len = sizeof(buf);
    vx_view_serialize_dom_json(view, buf, &len, sizeof(buf));
    std::string json(buf, len);
    EXPECT_NE(json.find("secret"), std::string::npos);  // policy=none → 暴露 value
  }
  ```

- [ ] **步骤 2-6：标准安全模板**（RED → GREEN → 反向探针 → 依赖审计 → 提交）

### 任务 A.2.2：T5 overlay 隔离 + 每帧复位单测 [安全]

A.0.4 + A.1.1 已覆盖 T5 主路径，A.2.2 加 1 集成测：连续 3 帧不同 hover → 每帧 overlay 命令数恒定（不累积）。**估时：** 30 min plan / 18 min plan ×0.6

### 任务 A.2.3：T7 buffer overflow 守卫单测（双调用模式 + max_size） [安全]

A.0.6 已覆盖 T7 主路径，A.2.3 加边界测：4 GB max_size 上限拒绝 / 0 max_size 拒绝 / nullptr out_len 返回 NULL_PARAM。**估时：** 30 min plan / 18 min plan ×0.6

### 任务 A.2.4：A14 binary size diff 验证（DevTool OFF 编译） [Smoke]

**文件：** 新建 `tests/smoke/devtool_a14_size_smoke.sh`（CMake test 注册）

**估时：** 15 min plan / 9 min plan ×0.6

```bash
#!/usr/bin/env bash
# A14 acceptance: VX_BUILD_DEVTOOL=OFF binary size = main `679304e` baseline
set -e
cmake -S . -B build-a14-off -DVX_BUILD_DEVTOOL=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-a14-off -j --target veloxa 2>&1 | tail -5
SIZE_OFF=$(stat --format=%s build-a14-off/libvx_api.a)
echo "VX_BUILD_DEVTOOL=OFF libvx_api.a size: $SIZE_OFF bytes"
# Phase A 启动前记 baseline，Phase A 完成后对比
# diff 应为 0（vx_devtool 子目录不参与 build）
```

---

## Phase A.3 — example + reflect prep（2 子任务，~45 min plan ×0.6）

### 任务 A.3.1：`examples/hello_devtool.cc` — Inspector smoke

**文件：**

- 新建：`examples/hello_devtool.cc`（基于 `hello_sdl2.cc` 加 `vx_view_attach_devtool`）
- 新建：`examples/CMakeLists.txt` 增加 `if(VX_BUILD_DEVTOOL AND VX_PLATFORM_SDL2)` 块
- 新建：`tests/integration/hello_devtool_smoke.cc`（headless smoke，env hook auto-quit）

**估时：** 45 min plan / 27 min plan ×0.6

### 任务 A.3.2：Phase A integration test + reflect prep

**步骤：**

- 全 Phase A 16 子任务 ctest 全 PASS 验证
- A14 `VX_BUILD_DEVTOOL=OFF` ctest 1062/1062 + binary size diff = 0 守门
- progress.md 更新「Phase A 完成快照」段
- 待 `/reflect` 命令进入

**估时：** 30 min plan / 18 min plan ×0.6

---

## 总估时矩阵 + plan ×0.6 第 18 数据点候选

| Phase | 任务数 | plan 估时 | plan ×0.6 估时 |
|:-:|:-:|---:|---:|
| Phase 0（0.1 reconfigure + 其他）| 11 子段 | 已包含在 A.0/A.1 内 | — |
| **A.0** 前置改造 | 6 | 315 min | **189 min（3.15 h）** |
| **A.1** DevTool UI | 4 | 240 min | **144 min（2.40 h）** |
| **A.2** 安全单测 | 4 | 105 min | **63 min（1.05 h）** |
| **A.3** example + reflect prep | 2 | 75 min | **45 min（0.75 h）** |
| **合计** | **16** | **735 min（12.25 h）** | **441 min（7.35 h）** |

**plan ×0.6 第 18 数据点假设：**

- 沿用蓝图基线 7.35 h plan ×0.6（B7=A 决策）
- 实测落 0.85-1.15× plan ×0.6 区间 → 「大件类 0.8-1.2×」桶（与 systemPatterns 矩阵一致）
- 实测落 0.46-0.59× plan ×0.6（极窄档群组延续）— 仅 plan 蓝图全照搬 + 决策跳过率高时可能
- reflect 阶段对照 sytemPatterns 任务类型分桶矩阵决定数据点归入哪一桶

---

## 风险登记 + Checkpoint

| # | 风险 | 概率 | 影响 | 缓解 |
|:-:|---|:-:|:-:|---|
| **R1** | I1 callsite 漏改 | 🟢 低（实证 4 处）| 🟡 中 | A.0.1 重命名 + grep 收尾验证脚本 ✅ |
| **R2** | DevTool UI 自渲染暴露引擎缺陷 | 🟡 中 | 🟡 中 | A.1.2 用「已验证 OK」CSS 子集（avoid flex 边角 / bidi / transform / border-radius 待 grep）；缺陷暴露立即列入 R3+ 不阻塞主线 |
| **R5** | ComputedStyle JSON hover 性能 | 🟡 中 | 🟡 中 | hover hot path 走 D7=C 第一层零拷贝；C API JSON 仅 Style panel 点击时序列化（lazy）|
| **R7**（新） | 蓝图 plan 写于 4-30，main 改动可能小步漂移 | 🟢 低 | 🟢 低 | Phase 0.1 ctest 1062 reconfigure + 实测确认；A.0/A.1 实施前 grep 验证既有 API 签名仍在 |
| **R8**（新） | DevTool resource 路径透传 / runtime 加载失败 | 🟡 中 | 🟢 低 | A.1.4 `vx_view_attach_devtool` API 接受绝对路径 + Phase A.3.1 example smoke 用 `__FILE__` 推算路径 |

### Checkpoint 1：A.0 完成（6 commits 落地后）

- 用户审 A.0 前置改造 6 commits + ctest 1078/1078 + A14 OFF 1072 PASS
- 选项：A 继续 A.1 / B 暂停审 commit 链 / C 提前 reflect（如发现 R1 callsite 实测远超 4 处）

### Checkpoint 2：A.1 完成（4 commits 落地后）

- 用户审 DevTool UI dogfood smoke + R2 引擎缺陷暴露清单
- 选项：A 继续 A.2/A.3 / B 拆出 R2 缺陷为独立 P3 候选 / C 提前 reflect

### 默认协议

无明确选择 → 按 systemPatterns「Checkpoint 推荐默认 + 隐式批准协议」走 A 选项继续。

---

## 执行交接

**计划完成并保存到 `docs/plans/2026-05-02-devtool-inspector.md`。准备执行？**

**下一步：** 使用 `/build` 命令执行 — 按 B6=A 决策每子任务 1 commit（~16 commits + reflect/archive ≈ 18 commits）。

**首个执行任务：** Phase 0.1 ctest reconfigure + 1062 基线确认（关键前置）+ Phase A.0.1 I1 改造。

---

## 与 Memory Bank 集成

- 计划保存后更新 `memory-bank/tasks.md`（在 TASK-20260502-01 任务卡片增「计划完成」段）
- 更新 `memory-bank/activeContext.md`（阶段 → `规划中` → `已规划`）
- 更新 `memory-bank/progress.md`（记录 plan 完成里程碑 + Phase 0 11 子段实测结果）
