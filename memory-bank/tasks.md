# 任务跟踪

## 当前任务

### TASK-20260419-05：Layout + Render 性能基准（4 个 bench exe）

- **复杂度级别：** Level 2-3（4 文件新建 + CMakeLists 更新 + README + 4 baseline JSON 入仓）
- **状态：** 🟢 构建完成（待 `/reflect`）
- **设计文档：** `docs/specs/2026-04-19-layout-render-benchmarks-design.md`
- **实现计划：** `docs/plans/2026-04-19-layout-render-benchmarks.md`（7 phase / ~25 BMs / ~4.25h / 7 commits）
- **分支：** `feature/TASK-20260419-05-layout-render-benchmarks`（基于 main `2985220`）
- **创建日期：** 2026-04-19
- **来源：** TASK-20260419-02 归档 P1 后续任务 + TASK-20260419-03 baseline 模式延伸应用
- **Sticky ID 说明：** 候选区固定 ID TASK-05（自 TASK-02 归档以来），与当天序号 08 不同；按候选区一致性使用 sticky ID（参考 TASK-04 同样反向插入先例）

#### 目标 API（已验证）

| API | bench 用途 |
|---|---|
| `vx::layout::LayoutEngine::Layout(doc, ctx)` | `bench_layout_buildtree` — 构造 BlockBox / InlineBox 树 + 完整 layout（默认 block flow） |
| `vx::layout::LayoutEngine::Layout(doc, ctx)` （带 `display: flex` DOM） | `bench_layout_flex` — 触发 flex code path（行/列、子项数、尺寸约束）|
| `vx::render::Record(root)` → `DisplayList` | `bench_render_record` — 树→ DisplayList 转换 |
| `vx::render::Replay(list, canvas)` | `bench_render_replay` — DisplayList → Canvas 命令重放 |

#### 衔接 TASK-03 模式（可复用）

- ✅ `vx_add_benchmark()` CMake 函数已支持额外链接库（TASK-03 引入）
- ✅ baseline JSON 入仓 + `benchmarks/baseline/README.md` 4-piece 失真兜底协议（TASK-03 沉淀）
- ✅ RangeMultiplier 公式 `ceil(log_m(hi/lo))+1`（TASK-03 实证）
- ✅ 程序化 corpus 构造 + 静态 cache（TASK-03 `StylesheetCorpus` / `InlineStyleCorpus` 模式可复刻为 `LayoutCorpus`）
- ✅ 「带否定判据的发现型 phase」模板（TASK-03 cluster 度量；本任务可用于发现 layout 慢路径）

#### 前置验证（全部 ✅）

| 维度 | 结果 |
|------|------|
| 依赖可获取性 | ✅ 4 API 全在 main，`vx_core` 已链 |
| 环境就绪 | ✅ `build-bench/` 复用，**无需 FetchContent → P0 git proxy 不触发** |
| 已有 artifact | ✅ 7 现存 bench（4 Foundation + 3 CSS）；新增 4 bench 文件名不冲突 |
| 待处理事项关联 | ✅ 落实 `activeContext.md` 长期项 P1 「Layout + Render 基准」 |

#### 4 决策（头脑风暴产出）

| # | 维度 | 选择 |
|---|------|------|
| 1 | Corpus 构造 | **A** 纯程序化 DOM API（`CreateElement` + `AppendChild` + `SetInlineDeclaration`） |
| 2 | Flex 输入维度 | **B** 二维 `BENCHMARK_TEMPLATE(rows, cols)` 5 固定多维点 + 1 嵌套 flex |
| 3 | ImageCache 覆盖 | **B** Record / Replay 各加 1 个 img-only 对比 BM（含 ImgVsNoImg/Cache vs /NoCache 判定 hot path） |
| 4 | baseline 入仓 | **A** 4 个全入仓 + 复用 TASK-03 baseline/README 4-piece 失真兜底协议 |

#### Phase 划分（7 phase，详见 plan §1-7）

| Phase | 时间 | 文件 | BM 数 | 提交主题 |
|-------|------|------|-------|---------|
| 1 | 30min | CMakeLists + 4 smoke .cc | 4 | wip phase-1 |
| 2 | 30min | layout_corpus.h | 0 | wip phase-2 |
| 3 | 45min | bench_layout_buildtree.cc | 9 → 14 行 | feat phase-3 |
| 4 | 45min | bench_layout_flex.cc | 6 | feat phase-4 |
| 5 | 30min | bench_render_record.cc | 5 | feat phase-5 |
| 6 | 45min | bench_render_replay.cc | 5-6 | feat phase-6 |
| 7 | 30min | README + baseline + MB | 0 | docs phase-7 |

#### 验收标准（design §9 完整版 — 全部已验证 ✅）

1. ✅ 4 bench exe Release build 0 errors
2. ✅ 各 bench BM 数量符合设计：buildtree 14 行 / flex 6 / record 5 / replay 5
3. ✅ 4 bench 全 exit 0 + 数字非零（含 Replay 修正后 list 非空，~10 ns/cmd FillRect）
4. ✅ 全 7+4=11 bench targets 共存、零冲突
5. ✅ Debug ctest 890/890 不变（`build/` 重新 build 验证）
6. ✅ 4 baseline JSON 入仓（全 release 体检 ✅）+ baseline/README + benchmarks/README 更新
7. ⏸️ techContext.md「Layout / Render 性能基线」段 — **待 /reflect 阶段补**（已写入 baseline/README 与 benchmarks/README，techContext 整理推给 /reflect 一并）
8. ⚠️ ImageCache 对比 BM 给出明确判定 — **回答「不是 hot path」但**真测路径 layout→Record→Replay 三阶段都不传 cache → list 内 0 个 kDrawImage。改用 Replay TextHeavy 通路实测出真正 hot path = `DrawText`（820× FillRect）。ImageCache 真测推 TASK-09。验收意图（量化是否 hot path）已满足。
- 安全相关：否（性能测量任务，无外部输入/无认证/无新依赖）

#### 关键发现（5 项 → /reflect 输入）

| # | 发现 | 数值 | 后续 |
|---|------|------|------|
| K1 | Replay hot path = `DrawText` | 8200 ns/cmd vs FillRect 10 ns/cmd = 820× | 立 TASK-20260419-09（候选）DrawText / shaping micro-benches |
| K2 | Layout buildtree-flat super-linear knee | N=128→256 时 7.7→70 µs（10× for 2× N）| 调查 cache miss / arena grow |
| K3 | Layout flex 同源 super-linear | 8x8→16x16 时 4.9→73 µs（14.9× for 4× cells）| 同 K2 |
| K4 | Record 对 image 元素无额外开销 | image_handle=0 → RecordBox 跳过；ImgVsNoImg 16 ≈ Medium 64/4 | 设计正确 |
| K5 | ImageCache 真测 fixture 缺失 | DecodeFromFile I/O；layout 不传 cache → 三阶段链断 | 推 TASK-20260419-09 |

#### 提交清单（7 commits, on `feature/TASK-20260419-05-...`）

| # | SHA | 主题 |
|---|-----|------|
| 1 | `3eb9070` | docs(plan): TASK-20260419-05 layout/render benchmarks design + plan |
| 2 | (phase 1) | wip phase-1 register 4 smoke benches |
| 3 | (phase 2) | wip phase-2 add layout_corpus.h |
| 4 | (phase 3) | feat phase-3 bench_layout_buildtree full suite |
| 5 | (phase 4) | feat phase-4 bench_layout_flex 2D matrix |
| 6 | (phase 5) | feat phase-5 bench_render_record full suite |
| 7 | (phase 6) | feat phase-6 bench_render_replay + hot-path finding |
| 8 | (phase 7) | docs(bench): 4 layout/render baselines + README updates |

<details>
<summary>TASK-20260419-07：修复 main Release `-Werror` 编译失败（2 处） — ✅ 已归档（点开查看历史）</summary>

### TASK-20260419-07：修复 main Release `-Werror` 编译失败（2 处）

- **复杂度级别：** Level 1（2 文件，修复路径明确）
- **状态：** 🔵 回顾完成（待 `/archive`）
- **回顾文档：** `memory-bank/reflection/reflection-TASK-20260419-07.md`
- **分支：** `feature/TASK-20260419-07-release-werror-fixes`（基于 main `b321482`）
- **创建日期：** 2026-04-19
- **来源：** TASK-20260419-03 P6 Release fresh build 副发现 + 长期项 P1 #2
- **关联模式：** 与 TASK-20260419-04 同源（Release 通路验证缺失反复模式，第 2 次因此暴露）

#### 修复目标（已实地复现 ✅）

| # | 文件 | 行 | 错误 | 修复方向 |
|---|------|----|----|---------|
| (a) | `tests/platform/memory_surface_test.cc` | 102 / 105 / 108 | `fgets -Werror=unused-result` | ✅ A1 `ASSERT_NE(std::fgets(...), nullptr)` 包裹（commit `8b57f8d`） |
| (b) | `veloxa/foundation/strings/string.h` 拷贝构造 | 61-74 | `memcpy` `-Werror=array-bounds` GCC IPA 误报 | ✅ B2 `[[gnu::noinline]]` 拷贝构造（GCC-only 守卫），commit `51d6ff1`。**B3 `__builtin_memcpy` 试验失败** — 诊断在 IPA 中端阶段，先于 fortify 展开 |

#### 验收标准（全部 ✅）

- ✅ `cmake --build build-bench -j` 全量构建零 `-Werror` 失败（38.7s）
- ✅ `ctest` Debug 全量回归 890/890 不变（2.46s）
- ✅ bench 7 目标继续 exit 0（sanity 跑 1ms 全 BM 数字正常）

#### 提交清单

| # | SHA | 描述 |
|---|------|------|
| 1 | `8b57f8d` | fix(tests/platform): assert fgets return value |
| 2 | `51d6ff1` | fix(foundation/strings): mark BasicString copy ctor noinline |
| 3 | `6fca7cb` | chore(build): finalize TASK-20260419-07 memory bank state |
| 4 | `0d749c1` | docs(reflect): add reflection for TASK-20260419-07 |
| 5 | `466ba01` | chore: ignore build-*/ directories |

#### 安全相关

否。

</details>

### 待立项候选

- ~~TASK-20260419-05：已立项为当前任务，详见上方「当前任务」段~~
- **TASK-20260419-06（建议，**P3 降级**）：** HashMap Hash Mixing 优化 — 触发条件改为「短字符串 ≠ 主用例 + 容器规模 > 1000 entry」的新场景出现时再立项（来源 TASK-03 P4 实测均匀降级）
- **TASK-20260419-08（候选，P3 触发型）：** `string.h` 剩余 3 处 runtime-size memcpy（line 45 SSO ctor / 150 Append / 230 GrowAndCopy）防御性 noinline 化。**触发条件**：未来 GCC 升级回归同类 `-Warray-bounds` 误报（来源 TASK-07 副发现）
- **TASK-20260419-09（新增，TASK-05 K1 + K5 触发，建议 P1）：** Replay hot path 深度基准 — 真实 ImageCache 通路（含解决 DecodeFromFile fixture 文件依赖：在 build-bench 期 `configure_file()` 复制 1×1 PNG 到 `${CMAKE_BINARY_DIR}/benchmarks/fixtures/`）+ `DrawText` 微基准（拆解 SimpleTextShaper / glyph cache lookup / SoftwareCanvas DrawTextFallback）。目标量化「Text 是否真的是 820× FillRect 慢的根因」+ 「ImageCache 是否走真路径仍 < DrawText」。来源：TASK-20260419-05 K1 hot path 实证 + K5 fixture 缺失工程问题

## 任务历史

| 任务 ID | 描述 | 状态 | 完成日期 | 归档文档 |
|---------|------|------|---------|---------|
| TASK-20260419-07 | 修复 main Release `-Werror` 编译失败（fgets unused-result + BasicString copy ctor IPA array-bounds 误报）— 与 TASK-04 同元模式不同手法 | ✅ 已完成 | 2026-04-19 | `archive-TASK-20260419-07.md` |
| TASK-20260419-03 | CSS 解析性能基准（Tokenizer / Parser / PropertyLookup）— 30 BMs + 3 baseline JSON + Cluster 度量证 PropertyMap 均匀 | ✅ 已完成 | 2026-04-19 | `archive-TASK-20260419-03.md` |
| TASK-20260419-04 | 修复 `enum_serialization.cc` Release `-Warray-bounds` 误报（解锁 TASK-03 Phase 1） | ✅ 已完成 | 2026-04-19 | `archive-TASK-20260419-04.md` |

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
