# 任务跟踪

## 当前任务

### TASK-20260419-04 — 修复 `enum_serialization.cc` Release `-Warray-bounds` 误报

- **复杂度级别：** Level 1（小修小补 / 单文件 / 修复路径明确）
- **状态：** VAN 完成（待 `/build` 确认 A/B/C 方向后实施）
- **创建日期：** 2026-04-19
- **基线分支：** `main`（commit `861070e`）
- **功能分支：** `feature/TASK-20260419-04-array-bounds-fix`
- **来源：** TASK-20260419-03 Phase 1 BUILD 副发现
- **预估提交：** 2 个（fix + chore(mb)）
- **预估改动：** ≤ 5 行（A 方案）/ ≤ 3 行（B 方案）/ ~30 行（C 方案）
- **TDD 模式：** 现有覆盖（166 行 / 60 处断言的 `enum_serialization_test.cc` 即回归基线 — 不新增测试）
- **安全相关：** 否
- **解锁目标：** TASK-20260419-03 Phase 1（CSS bench 链接 vx_core 的 Release 通路）

#### 错误现场（VAN 已复现，TASK-03 Phase 1 100% 必现）

```
veloxa/core/css/enum_serialization.cc:63:15: error: array subscript ‘const char* const [5][0]’
is partly outside array bounds of ‘const char* const [2]’ [-Werror=array-bounds]
   63 |   const char* s = table[v];
veloxa/core/css/enum_serialization.cc:63:15: error: array subscript ‘const char* const [5][0]’
is partly outside array bounds of ‘const char* const [4]’ [-Werror=array-bounds]
```

触发命令：`cmake --build build-bench --target <任意依赖 vx_core 的 target> -j`（Release `-O2`，`-Werror` 由根 CMakeLists.txt 设置）

#### 根因分析

`template<usize N> Lookup(const char* const (&table)[N], u16 v)` 在 13 个 `case` 中按不同 `N` 参数实例化 + 内联出 5+ 个 clone（编译器为各 `[5]/[4]/[2]/[6]` 等独立特化）。GCC IPA 优化阶段做跨函数值域传播时，将「某 case 实际访问的 `[5]` 数组类型」错误地匹配到「另一 case 中类型为 `[2]/[4]` 的 table 元数据」，未识别 `if (v >= N) return` 已先行守住。属 GCC 已知误报模式。Debug `-O0` 不做该层优化故不触发。

#### 候选方案（待 `/build` 确认 1 句话方向后实施）

| 方案 | 改动 | 优势 | 劣势 |
|------|------|------|------|
| **A) 文件局部 pragma**（推荐） | `enum_serialization.cc` 顶部 `#pragma GCC diagnostic push` + `#pragma GCC diagnostic ignored "-Warray-bounds"` 包裹 `Lookup<N>` + `pop`；附详尽注释 | 源文件级 — 代码债位置 = 抑制位置；新人读到 `Lookup<N>` 立刻看到 why；Clang 静默忽略无副作用 | 抑制了该 .cc 的整段 array-bounds（实际全部都是 Lookup 调用，影响域已自然限定） |
| **B) CMake 单文件豁免** | `veloxa/core/CMakeLists.txt` 加 `set_source_files_properties(css/enum_serialization.cc PROPERTIES COMPILE_OPTIONS "-Wno-array-bounds")` | 源文件保持完全干净；CMake 显式记录例外 | 信息散到构建系统层；新人不易看到，需 grep 才能找到 |
| **C) 去模板化** | `Lookup<N>` → `LookupImpl(const char* const* table, std::size_t n, u16 v)`；13 处调用点显式传 `std::size(arr)` | 根因消除；不再有 IPA clone 触发条件；未来 GCC/Clang 都不会报 | 工作量超 Level 1 边界；模板原本就是为了避免显式传长度的人工失配；维护风险升高 |

#### 前置验证

| 维度 | 结果 |
|------|------|
| 错误复现 | ✅ TASK-03 Phase 1 已 100% 必现 |
| 影响文件 | ✅ 单文件 `veloxa/core/css/enum_serialization.cc`（107 行） |
| 现有测试覆盖 | ✅ `tests/core/css/enum_serialization_test.cc`（166 行 / 60 处断言） |
| GCC pragma / `set_source_files_properties COMPILE_OPTIONS` 可用性 | ✅ GCC 9+ / CMake 3.x 早已支持 |
| Clang 兼容 | ✅ A 方案 pragma 被 Clang 静默忽略；B 方案 `-Wno-array-bounds` Clang 也接受 |
| 解锁验证路径 | ✅ 修复后切到 `feature/TASK-20260419-03-css-benchmarks` rebase main → `cmake --build build-bench --target bench_css_*` → 3 smoke 跑通 |

## 暂停中任务

### TASK-20260419-03 — CSS 解析性能基准（Tokenizer / Parser / Property Lookup）

- **复杂度级别：** Level 2
- **状态：** 暂停（Phase 1 WIP 已 commit，等待 TASK-04 解锁）
- **分支：** `feature/TASK-20260419-03-css-benchmarks`（ahead-of-main 4 commits：plan + WIP Phase 1 + 2 chore；MB 暂停说明）
- **进度：** Phase 0 ✅（GTest 890/890 + Release `build-bench/` + bench_allocators 13 BM 验证 Release 通路 OK）；Phase 1 ⛔（CMake 扩展 + css_corpus.h + 3 smoke .cc 已写入但未编译验证）
- **阻塞：** Release `-O2 -Werror=array-bounds` 拒绝 `vx_core/css/enum_serialization.cc:63` — GCC IPA inline 模板 `Lookup<N>` 跨 5+ clone 值域分析误报；Debug 不触发；TASK-02 仅链 vx_foundation 故未暴露
- **续接动作：** TASK-04 修复合并到 main 后 → `git checkout feature/TASK-20260419-03-css-benchmarks` → `git rebase main` 或 merge → `/build` → `cmake --build build-bench --target bench_css_{tokenizer,parser,property_lookup} -j` 验证 3 smoke 跑通 → 进入 Phase 2

> **注：** TASK-20260419-04 已上移至「当前任务」，详情见上方完整入口。

<details>
<summary>已归档：TASK-20260419-02 — 补充 Google Benchmark 集成与 Foundation 性能基准（点开查看历史细节）</summary>

- **复杂度级别：** Level 2（简单增强）
- **状态：** ✅ 已完成（已合并到 main）
- **分支：** `feature/TASK-20260419-02-benchmarks`（已删除）
- **创建日期：** 2026-04-19
- **归档日期：** 2026-04-19
- **来源：** TASK-20260405-01 Foundation 延期项 P1#1（"Benchmark 延期 — 需 google benchmark，网络恢复后补充"）
- **设计规格：** `docs/specs/2026-04-19-benchmarks-design.md`
- **实现计划：** `docs/plans/2026-04-19-benchmarks.md`
- **回顾文档：** `memory-bank/reflection/reflection-TASK-20260419-02.md`
- **归档文档：** `memory-bank/archive/archive-TASK-20260419-02.md`
- **实际提交：** 10 个（plan + 7 build + reflect + archive）
- **实际新增 BM 用例：** 40（allocators 13 + containers 8 + hash_map 10 + strings 9）；GTest 基线 890/890 不变

#### 关键决策（头脑风暴 4 轮，全部 A 方案）

| 维度 | 决策 |
|------|------|
| 范围 | 仅 Foundation（allocators / containers / strings / hash_map），不含 CSS/Layout/Render |
| Executable 粒度 | 一文件一 exe（4 个） |
| 用例深度 | 中等（每组件核心操作 + 1-2 个尺寸/容量梯度） |
| 结果留存 | 仅控制台 + README 给出 JSON 导出（不提交 baseline 文件） |

#### Phase 概览

| Phase | 内容 | 提交 |
|-------|------|------|
| 0 | 基线验证（890 测试全绿；FetchContent 通路验证） | 0 |
| 1 | 创建分支 + `benchmarks/CMakeLists.txt` + 4 个 smoke .cc | 1 |
| 2 | `bench_allocators.cc`（Malloc/Arena/Pool，13 BM） | 1 |
| 3 | `bench_containers.cc`（Vector/SmallVector/IntrusiveList，8 BM） | 1 |
| 4 | `bench_hash_map.cc`（insert/lookup hit&miss/rehash，10 BM） | 1 |
| 5 | `bench_strings.cc`（BasicString SSO+heap、InternedString，9 BM） | 1 |
| 6 | `README.md` + `techContext.md` 更新（依赖表 + Benchmark 启用段） | 1 |
| 7 | 收尾验证 + 依赖安全审计（google/benchmark v1.9.1 CVE） + MB 收尾 | 1 |

#### 安全相关

否（纯内部性能测量；唯一新依赖 google/benchmark v1.9.1 经 CVE 审计：GitHub Security Advisories 0 条）

#### 副发现

`HashMap::Find` 对小整数 key 在 cap≥1024 时严重 cluster（LookupHit/16384=9µs vs n=64=69ns）。根因 `H1=h>>7` + `std::hash<int>` 恒等映射使所有起始探测位置压在 [0,127]。已记入 `benchmarks/README.md` ⚠️ 量级表 + activeContext 待处理事项 P2，建议立项 TASK-20260419-05。

</details>

<details>
<summary>已归档：TASK-20260419-01 — 流程规则沉淀 + P2 功能技术债收口（点开查看历史细节）</summary>

- **复杂度级别：** Level 3（中等功能；跨「.cursor 规则」+ 「Script/CSS/Event」子系统）
- **状态：** ✅ 已完成（已合并到 main）
- **分支：** `feature/TASK-20260419-01-rules-and-debt`（已删除）
- **创建日期：** 2026-04-19
- **归档日期：** 2026-04-19
- **设计规格：** `docs/specs/2026-04-19-rules-and-debt-design.md`
- **实现计划：** `docs/plans/2026-04-19-rules-and-debt.md`
- **回顾文档：** `memory-bank/reflection/reflection-TASK-20260419-01.md`
- **归档文档：** `memory-bank/archive/archive-TASK-20260419-01.md`
- **预估提交：** 21 个；**实际：** 23 个（含 reflect + archive）
- **预估新增测试：** +22；**实际：** +34（基线 856 → 890）

#### 需要创意阶段的组件

**无。** 头脑风暴阶段已为 Part B 三个设计点（B5 Enum 反查表 / B6 反向析构机制 / B7 精确移除 API）固化方案：

| 子项 | 决策 | 模式来源 |
|------|------|---------|
| B5 | 完整反查表 `enum_serialization.{h,cc}` | 枚举 + 元数据表驱动模式（property.cc 同构） |
| B6 | EventManager `AddDestructionObserver` 回调 | Push 回调失效触发模式（SetInvalidationCallback 同构） |
| B7 | u64 ListenerToken + `RemoveByToken` API | 不透明句柄模式（与现有 `JSValue` 句柄风格一致） |

无需进一步探索 → 跳过 `/creative`，直接 `/build`。

#### Phase 概览

| Phase | 内容 | 预计耗时 |
|-------|------|---------|
| 0 | 基线验证（856 测试全绿） | 5 min |
| A1 | writing-plans.mdc 增 5 段 | 30 min |
| A2 | subagent-development.mdc 增 3 段 | 20 min |
| A3 | 新建 integration-testing.mdc + 注册 | 20 min |
| A4 | techContext.md「FetchContent 与代理」段 | 10 min |
| B5 | enum_serialization.{h,cc} + dom_bindings 接入 + 测试 | 45 min |
| B6 | EventManager DestructionObserver + DomBindings 注册 | 30 min |
| B7 | Listener Token API + 精确 removeEventListener | 60 min |
| 8 | 收尾验证 + Memory Bank 更新 | 20 min |
| **合计** | | **~4 h** |

#### 任务范围

**Part A — 流程规则沉淀（14 条 P1 待办，纯 .mdc 文件修改）**

1. 计划模板（`.cursor/rules/skills/writing-plans.mdc`）增段：
   - FetchContent 引入第三方编译时校验根 `add_compile_options(-Werror...)` 是否仅限 `$<COMPILE_LANGUAGE:CXX>` 或目标级（来源 TASK-20260413-01）
   - 测试基础设施审计——列出测试需访问的内部状态及访问路径（来源 TASK-11）
   - 边界输入清单——每个 Phase 列出非默认路径（来源 TASK-06）
   - 跨模块参数透传修改时调用链端到端验证（来源 TASK-09）
   - 设计文档管线注入点须附代码级可行性验证（来源 TASK-13）
2. 子代理 prompt（`.cursor/rules/skills/subagent-development.mdc`）增段：
   - 跨模块数据格式段（来源 TASK-02）
   - LayoutBox 坐标计算时须包含 x/y 语义定义 content origin vs border box origin（来源 TASK-07）
   - 并行子代理可行条件：无共享 .cc + 共享 .h 已创建 + CMakeLists.txt 已更新（来源 TASK-08）
   - 存根文件预创建策略（来源 TASK-04）
   - 合并 Phase 给子代理的策略（来源 TASK-04）
3. 集成测试规范——新建 `.cursor/rules/skills/integration-testing.mdc`：
   - 集成测试优先验证数据格式一致性（来源 TASK-02）
   - 必须使用真实 HTML/CSS 解析器，禁止仅用手动 DOM 构建（来源 TASK-06）
   - 像素验证优先用 DisplayList 检查和区域扫描，避免硬编码坐标（来源 TASK-07）
   - CSS 颜色测试禁止与 gfx::Color 编程常量直接比较，必须通过 CssColorToGfx 转换（来源 TASK-07）
   - 禁止使用 HTML inline style（BuildTree 不解析），必须用外部 CSS 选择器（来源 TASK-08）
   - 集成测试模板增加 API 备忘清单：html::Parser / FindElement / HandleInput（来源 TASK-13）
4. 文档：含 Git 拉取依赖的文档（`techContext.md` 或 README）写明代理 `http_proxy`/`HTTPS_PROXY` 与首次 `cmake` 注意点（来源 TASK-20260413-01）

**Part B — P2 功能技术债（含代码与测试）**

5. **#46 续作**：`StyleGetProp` Enum 读路径（display 等）——构建 `PropertyId→enum string` 反查表，覆盖 display/position/visibility 等 Enum 类型 CSS 属性
6. **#50 续作**：`DomBindings`/`EventManager` 析构顺序硬约束——目前仅保证 `DomBindings` 先析构；反向场景（`EventManager` 先销毁）需弱引用机制
7. **#47 续作**：`removeEventListener` 按 `(type, handler)` 精确移除——扩展 `EventManager::RemoveEventListenersByHandler` API 并在 `dom_bindings` 调用

#### 安全相关

否（纯内部技术债 + 流程规则；不涉及外部输入/认证/存储/部署）

</details>

## 任务历史

| ID | 描述 | 最终状态 | 归档日期 |
|----|------|---------|---------|
| TASK-20260419-02 | 补充 Google Benchmark 集成与 Foundation 性能基准（4 个 bench exe / 40 BM 用例） | ✅ 已完成 | 2026-04-19 |
| TASK-20260419-01 | 流程规则沉淀 + P2 功能技术债收口（14 条 P1 流程规则 + CSS Enum 序列化 / EventManager 反向析构 / removeEventListener 精确移除） | ✅ 已完成 | 2026-04-19 |
| TASK-20260418-01 | 消化关键技术债务（#45 dom_bindings 全局状态 / #46 StyleGetProp 读路径 / #48 hb_font 缓存 / #50 addEventListener 生命周期） | ✅ 已完成 | 2026-04-18 |
| TASK-20260414-01 | 功能补全（border-radius / 字体渲染 / 图片支持 / JS-DOM 绑定） | ✅ 已完成 | 2026-04-14 |
| TASK-20260413-02 | 消化技术债务子集（#41 `HashMap` const 迭代、#39 测试像素头、`active_count_` 移除） | ✅ 已完成 | 2026-04-13 |
| TASK-20260413-01 | QuickJS 脚本引擎集成（quickjs-ng、`vx_script`、`QuickjsEngine`） | ✅ 已完成 | 2026-04-13 |
| TASK-20260405-01 | 构建 Foundation 基础库（内存管理/容器/字符串/日志） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-02 | 构建 Graphics HAL 图形抽象层与 Platform HAL 平台抽象层 | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-03 | 构建 DOM 树 + HTML 解析器 | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-04 | 构建 CSS 引擎（Tokenizer/Parser/选择器/属性/层叠） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-05 | 消化技术债务（Arena/HashMap/PPM/Parser Error） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-06 | 构建 Layout Engine 布局引擎 | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-07 | 构建渲染管线（Render Pipeline） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-08 | 构建事件系统（Event System） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-09 | 脏区更新与重绘机制 | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-10 | 事件循环与应用壳（EventLoop / Application Shell） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-11 | C API 层（对外嵌入接口） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-12 | 示例应用（examples/hello） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-13 | CSS 动画系统（CSS Transitions） | ✅ 已完成 | 2026-04-05 |
