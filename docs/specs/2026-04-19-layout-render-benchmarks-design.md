# 设计规格：Layout + Render 性能基准（TASK-20260419-05）

**日期：** 2026-04-19
**任务 ID：** TASK-20260419-05
**复杂度级别：** Level 2-3
**关联：** TASK-20260419-02（Foundation 基准框架）+ TASK-20260419-03（CSS 基准 + baseline 协议）

---

## 1. 设计目标

为 vx 引擎的两大核心子系统建立可重复的性能基线：

- **Layout**：树构建（block flow）、flex 行/列双维布局
- **Render**：DisplayList 录制（树→命令）、命令重放（命令→ Canvas 像素）

**完整目标**（参照 TASK-02 归档 P1）：
1. 形成 4 个独立 bench 可执行（一文件一可执行，类 TASK-03）
2. 形成可重复 baseline JSON 入仓（参照 TASK-03 4-piece 失真兜底协议）
3. 暴露 layout/render 的「输入规模 → 时间复杂度」曲线（log-scale 直观判断）
4. 验证 ImageCache 是否是 Replay 关键路径（侧确认 TASK-05 性能争议）

**非目标**：
- 不优化任何 layout / render 代码（仅测量）
- 不引入新依赖（沿用 vx_core + benchmark::benchmark）
- 不构造像素级 corpus（流式合成 corpus，参照 TASK-03 `StylesheetCorpus`）
- 不测试 Layout/Render 的正确性（既有 GTest 已完全覆盖，本次仅性能补充）

---

## 2. 4 决策（来自头脑风暴）

| # | 维度 | 选择 | 理由 |
|---|------|-----|------|
| 1 | Corpus 构造 | **A** 纯程序化 DOM API | 隔离 HTML 解析开销；与 TASK-03 程序化 corpus 模式同源；最轻量 |
| 2 | Flex 输入维度 | **B** 二维 `BENCHMARK_TEMPLATE(rows, cols)` × 4-5 固定多维点 | 单行扫描掩盖二维交互；三维 over-engineer |
| 3 | ImageCache 覆盖 | **B** Record + Replay 各加 1 个 img-only BM 作对比项 | 验证 ImageCache 是否热点（侧确认 §1 目标 4）|
| 4 | baseline 入仓 | **A** 4 个全入仓 + 复用 TASK-03 baseline/README 协议 | 协议已验证、占用小（~4×10KB JSON）、未来回归证据完整 |

---

## 3. 4 个 bench 详细设计

### 3.1 `bench_layout_buildtree`（块流 + 嵌套，~9 BMs）

**目标**：测量 `LayoutEngine::Layout(doc, ctx)` 在不同 DOM 形态下的耗时。

| BM | 输入 | RangeMultiplier 估算 |
|---|------|-----|
| `BM_LayoutBuildTreeFlat/Range(8, 512)` | 平铺 N 个 `<div>` 子项（无嵌套） | `RangeMultiplier(2)` → 7 case |
| `BM_LayoutBuildTreeNested/Range(2, 64)` | 单链嵌套 N 层 div | `RangeMultiplier(2)` → 6 case |
| `BM_LayoutBuildTreeMixed` | 固定混合：3 层嵌套 × 4 子项/层 + text node | 1 case |

**工具**：纯 `dom::Document::CreateElement` + `Element::AppendChild`；不带 stylesheets（默认 ComputedStyle，触发 block flow 主路径）。

**预期复杂度**：`O(N)` 线性（树构建 + 单遍布局），baseline 给出 ns/box 单位。

### 3.2 `bench_layout_flex`（二维矩阵，~6 BMs）

**目标**：测量 flex 子项布局在 row × col 双维下的耗时。

| BM | row | col | 含义 |
|---|---|---|---|
| `BM_LayoutFlex/1x8`     | 1 | 8 | 单行少子项（baseline）|
| `BM_LayoutFlex/1x32`    | 1 | 32 | 单行中等子项 |
| `BM_LayoutFlex/1x128`   | 1 | 128 | 单行大量子项（探 row 维度极限）|
| `BM_LayoutFlex/8x8`     | 8 | 8 | 方阵中等 |
| `BM_LayoutFlex/16x16`   | 16 | 16 | 方阵大（探 row × col 交互）|
| `BM_LayoutFlexNestedFlex` | — | — | flex 内嵌 flex（嵌套主轴交叉轴影响）|

**工具**：DOM API 构造 `<body>` 外层 `display: flex; flex-direction: column` + 内层 `<div>` row × `<div>` col；inline declarations 通过 `Element::SetInlineDeclaration` 注入（非 stylesheet 路径，零解析）。

**预期复杂度**：约 `O(rows × cols)` + flex 主轴/交叉轴双遍。

### 3.3 `bench_render_record`（DisplayList 录制，~5 BMs）

**目标**：测量 `vx::render::Record(layout_root)` 把布局树转为 `DisplayList` 的耗时。

| BM | 输入 layout | image_cache |
|---|------|---|
| `BM_RecordSmallTree`     | 8 子项平铺 | `nullptr` |
| `BM_RecordMediumTree`    | 64 子项（嵌套 4×4）| `nullptr` |
| `BM_RecordLargeTree`     | 512 子项 8×4×16 | `nullptr` |
| `BM_RecordTextHeavy`     | 32 子项 + 每项 1 text | `nullptr` |
| `BM_RecordImgVsNoImg/Img` | 16 子项含 8 img | `nullptr`（仅录命令）|

**工具**：在 setup 调 LayoutEngine::Layout，hot loop 仅 `Record`。

**对比项**：`BM_RecordImgVsNoImg/Img` 与 `BM_RecordSmallTree`（8 div）规模相当但含 8 `<img>`，对比看 `kDrawImage` 命令生成是否有显著开销（理论上无 — Record 只读 layout box `image_handle`）。

### 3.4 `bench_render_replay`（命令重放，~5 BMs）

**目标**：测量 `vx::render::Replay(list, canvas)` 把命令序列回放到 SoftwareCanvas 的耗时。

| BM | 输入 DisplayList | image_cache |
|---|------|---|
| `BM_ReplaySmallList`     | 8 cmds（FillRect 主导） | `nullptr` |
| `BM_ReplayMediumList`    | ~70 cmds（mixed） | `nullptr` |
| `BM_ReplayLargeList`     | ~600 cmds（mixed） | `nullptr` |
| `BM_ReplayTextHeavy`     | ~50 cmds（DrawText 主导） | `nullptr` |
| `BM_ReplayImgVsNoImg/NoCache` | 16 cmds（8 DrawImage） | `nullptr`（DrawImage 走快路径跳过）|
| `BM_ReplayImgVsNoImg/Cache`   | 同上 | 真 ImageCache（DrawImage 真画）|

**工具**：setup 跑完整 layout + Record 拿 DisplayList；hot loop 仅 `Replay`。SoftwareCanvas 用预分配 800×600 缓冲（参照 `tests/core/render/integration_test.cc:41-45`）。

**核心对比项**：`ImgVsNoImg/NoCache` vs `ImgVsNoImg/Cache` — 直接量化 ImageCache 是否是 Replay 关键路径（**带否定判据的发现型 BM**，沿用 TASK-03 cluster 度量模式）。

---

## 4. 共享 Corpus 设计

仿 TASK-03 `benchmarks/css_corpus.h`，新建 **`benchmarks/layout_corpus.h`**（header-only inline，零额外 .cc）。

```cpp
namespace vx::bench {

// 按规模程序化构造 DOM；返回 owned Document*（生命周期由 caller 管理）
inline dom::Document* MakeFlatDocument(int n_children);
inline dom::Document* MakeNestedDocument(int depth);
inline dom::Document* MakeMixedDocument();
inline dom::Document* MakeFlexDocument(int rows, int cols);
inline dom::Document* MakeNestedFlexDocument();
inline dom::Document* MakeImageDocument(int n_images);

// 缓存复用（避免每次迭代重建 — 参照 TASK-03 StylesheetCorpus mutex-protected static map）
inline dom::Document& CachedFlatDocument(int n_children);
// ... 同上 6 个 getter

}  // namespace vx::bench
```

**生命周期**：`Document` 构造一次缓存到静态 map，整个 benchmark 进程生命周期内不释放（与 TASK-03 corpus 同设计；进程退出时 OS 回收）。

**为什么 inline header-only**：
- 与 TASK-03 css_corpus.h 一致（无 .cc 维护成本）
- 单元测试不需要（仅 bench 内部使用，正确性由现有 layout/render 测试覆盖）
- 模板 + inline 优化下零 ABI 风险

---

## 5. 文件结构

```
benchmarks/
├── CMakeLists.txt              [修改] +4 vx_add_benchmark(...) 行
├── README.md                   [修改] 增 Layout + Render 章节
├── layout_corpus.h             [新建] DOM 程序化构造 + 静态缓存
├── bench_layout_buildtree.cc   [新建] ~9 BMs
├── bench_layout_flex.cc        [新建] ~6 BMs
├── bench_render_record.cc      [新建] ~5 BMs
├── bench_render_replay.cc      [新建] ~6 BMs（含 ImgVsNoImg 对比项）
└── baseline/
    ├── README.md                       [修改] +4 baseline JSON 引用
    ├── bench_layout_buildtree.json     [新建] Phase 6 生成
    ├── bench_layout_flex.json          [新建] Phase 6 生成
    ├── bench_render_record.json        [新建] Phase 6 生成
    └── bench_render_replay.json        [新建] Phase 6 生成
```

**变更影响**：
- `CMakeLists.txt`：[共享文件] 增 4 行 `vx_add_benchmark(... vx_core)`，零冲突（参照 TASK-03 已沉淀模式）
- `README.md` / `baseline/README.md`：纯文档增量
- 不修改 `vx_core` / `vx_foundation` 任何源代码（验证 §1 非目标 1）
- 不修改任何现有 GTest（验证 §1 非目标 4）

---

## 6. 关键技术约束

1. **Build mode 强制 Release**：`-DCMAKE_BUILD_TYPE=Release` + 独立 `build-bench/` 目录（来源 TASK-02 反思 P1，TASK-03 复用，TASK-05 继续）
2. **TASK-04 detemplatize 已在 main**：本任务无需重做 GCC IPA 修复
3. **TASK-07 noinline 已在 main**：`BasicString` copy ctor 被 noinline，对 String 拷贝路径有 ~1-2ns 影响（layout/render 几乎不拷贝 String → 可忽略）
4. **`build-bench/` 复用 → 无 FetchContent → P0 git proxy checklist 不触发** ✅
5. **ImageCache 真实例**：Replay 对比 BM 需要构造一个 minimal ImageCache（含 1 个 dummy image），具体 API 在 Phase 5 验证
6. **SoftwareCanvas 已存在** (`veloxa/graphics/software/software_canvas.h:18`)，800×600 RGBA 缓冲 = 1.92MB（栈上不行，benchmark 帧用 static 一次性分配）

---

## 7. 风险评估

| 风险 | 概率 | 影响 | 缓解 |
|------|-----|-----|-----|
| Flex 路径布局算法 bug 导致基准结果异常波动 | 低 | 中 | Phase 3 完成后 sanity 跑现有 layout test 全绿 |
| ImageCache API 比预期复杂（dummy image 难构造） | 中 | 低 | Phase 5 BUILD 时若 30 分钟内构不成 dummy → 退化 NoImg-only Replay BM，把 ImgVsNoImg 拆到 TASK-09 |
| 800×600 SoftwareCanvas 缓冲对小 BM 引入测量噪音 | 低 | 低 | 用 `state.SetItemsProcessed(cmd_count)` 让数字按命令数归一化 |
| 程序化 DOM corpus 与真实使用差距大 | 中 | 中 | baseline/README 注明「仅作回归参考，不代表用户场景吞吐」（沿用 TASK-03 4-piece 失真兜底） |
| 7+4=11 bench targets 编译时间显著增长 | 低 | 低 | 一文件一可执行 + 并行 build；TASK-03 加 3 后增量 ~3s/bench，11 总计 ~30s 可接受 |

---

## 8. 安全分析

**N/A — 性能测量任务**。无外部输入（corpus 全程序化）/ 无认证 / 无数据存储 / 无新依赖 / 无网络。Layout 与 Render 输入均 in-process 构造。

---

## 9. 验收标准

| # | 标准 | 验证 |
|---|------|------|
| 1 | 4 bench exe Release build 0 errors | `cmake --build build-bench --target bench_layout_buildtree bench_layout_flex bench_render_record bench_render_replay -j` |
| 2 | 各 bench BM 数量符合 §3 设计 | bench `--benchmark_list_tests` 输出行数核对 |
| 3 | 4 bench 全 exit 0 + 数字非零 | `./build-bench/benchmarks/bench_layout_*` + `bench_render_*` 各跑 1ms |
| 4 | 7+4=11 bench 共存零冲突 | 全 11 bench 全跑 1ms 全 exit 0 |
| 5 | Debug ctest 890/890 不变 | `cd build && ctest -j` |
| 6 | 4 baseline JSON 入仓 + README 更新 | `git ls-files benchmarks/baseline/bench_layout_*.json bench_render_*.json` |
| 7 | techContext.md 「CSS 性能基线」段后追加「Layout 性能基线」+「Render 性能基线」 | grep "Layout 性能基线" / "Render 性能基线" |
| 8 | ImageCache 对比 BM 给出明确判定（hot/not-hot ≥/< 5x 阈值，参照 TASK-03 cluster） | Phase 5 reflection 段记录 |
