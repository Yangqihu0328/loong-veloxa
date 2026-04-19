# 进度记录

## 当前任务

### TASK-20260419-05：Layout + Render 性能基准（4 bench exe）

#### 里程碑

- 2026-04-19 — `/van`：API 验证 ✅、分支创建 ✅、前置验证 ✅（无 FetchContent）
- 2026-04-19 — `/plan`：4 决策头脑风暴 ✅、design 文档落盘 ✅、7-phase 实现计划落盘 ✅、Memory Bank 同步 ✅
- 2026-04-19 — `/build`：7 phase / 8 commits 全完成 ✅
  - Phase 1：CMakeLists 注册 + 4 smoke .cc，11/11 build green，4 smoke 数字非零（156 / 321 / 91 / 1.65 ns）
  - Phase 2：`benchmarks/layout_corpus.h`（7 builders + Styled 子集 + mutex-protected static cache），smoke 透传验证
  - Phase 3：`bench_layout_buildtree` 3 BM × 14 行；意外发现 Flat N=128→256 super-linear knee（K2）
  - Phase 4：`bench_layout_flex` 6 BMs；Flex 8x8→16x16 同源 super-linear（K3，14.9× for 4× cells）
  - Phase 5：`bench_render_record` 5 BMs；Record/512 ≈ 31 ns/box 完美线性（64.7×）
  - Phase 6：`bench_render_replay` 5 BMs；**修正 styled corpus + ctx.stylesheets 非空 gating**，揭示 hot path = `DrawText`（820× FillRect，K1），ImageCache 真路径不在 hot path（K4 + K5）
  - Phase 7：4 baseline JSON 入仓（全 release ✅）+ baseline/README key findings + benchmarks/README 升级到 11 exe
- **完成验证：** Release build 11/11 green，全 exit 0；Debug ctest 890/890 通过；4 baseline JSON `library_build_type=release`

#### 关键决策（已生效，括号内为构建结果）

1. Corpus = 纯程序化 DOM API（仿 TASK-03 css_corpus）→ ✅ 落地，并新增 Styled 子集（render bench 必需）
2. Flex bench = 二维 BENCHMARK_TEMPLATE 5 固定点 + 1 嵌套 flex → ✅ 6 BMs 全跑通
3. ImageCache 在 Record/Replay 各加 1 个 img-only 对比 → ⚠️ 已加但发现真测路径需 fixture（推 TASK-09），用 TextHeavy 通路替代实测出真 hot path
4. 4 baseline JSON 全入仓 + 复用 TASK-03 4-piece 失真兜底协议 → ✅ 落地 + key findings 表追加 5 项

#### 关键发现（5 项）

| # | 发现 | 数值 | 后续 |
|---|------|------|------|
| K1 | Replay hot path = DrawText | 8200 ns/cmd vs FillRect 10 ns/cmd = 820× | 立 TASK-20260419-09 |
| K2 | Layout buildtree-flat super-linear knee | N=128→256 时 7.7→70 µs（10× for 2× N）| /reflect 调查 |
| K3 | Layout flex 同源 super-linear | 8x8→16x16 时 4.9→73 µs（14.9× for 4× cells）| 同 K2 |
| K4 | Record 对 image 元素无额外开销 | image_handle=0 → 跳过；ImgVsNoImg 16 ≈ Medium 64/4 | 设计正确 |
| K5 | ImageCache 真测 fixture 缺失 | DecodeFromFile I/O；layout 不传 cache → 三阶段链断 | 推 TASK-20260419-09 |

#### 下一步

`/reflect` 进入回顾 — 重点处理 K1~K5 + 沉淀 3 条 lessons：
1. Render bench 需 Styled corpus（默认 alpha=0 → FillRect 不 emit）
2. Render bench layout 阶段需 `ctx.stylesheets` 非空（StyleResolver gate）
3. ImageCache 三阶段协同要求（layout/Record/Replay 任一不传 cache → 链断）

## 已完成任务

- TASK-20260419-07：修复 main Release `-Werror` 编译失败（fgets unused-result + BasicString copy ctor IPA array-bounds 误报；与 TASK-04 同元模式不同手法 noinline）→ 归档 `memory-bank/archive/archive-TASK-20260419-07.md`
- TASK-20260419-03：CSS 解析性能基准（Tokenizer 10 BM / Parser 11 BM / PropertyLookup 9 BM = 30 BMs + 3 baseline JSON 入仓 + Cluster 度量证 PropertyMap 均匀 → TASK-06 P1→P3 降级） → 归档 `memory-bank/archive/archive-TASK-20260419-03.md`
- TASK-20260419-04：修复 `enum_serialization.cc` Release `-Warray-bounds` 误报（去模板化 `Lookup<N>`，解锁 TASK-03 Phase 1） → 归档 `memory-bank/archive/archive-TASK-20260419-04.md`
- TASK-20260419-02：Google Benchmark 集成 + Foundation 性能基线（4 exe / 40 BM） → 归档 `memory-bank/archive/archive-TASK-20260419-02.md`
- TASK-20260419-01：流程规则沉淀 + P2 功能技术债收口 → 归档 `memory-bank/archive/archive-TASK-20260419-01.md`
- TASK-20260418-01：消化关键技术债务（#45/#46/#48/#50） → 归档 `memory-bank/archive/archive-TASK-20260418-01.md`
- TASK-20260414-01：功能补全 → 归档 `memory-bank/archive/archive-TASK-20260414-01.md`
- TASK-20260413-02：消化技术债务子集 → 归档 `memory-bank/archive/archive-TASK-20260413-02.md`
- TASK-20260413-01：QuickJS 脚本引擎集成 → 归档 `memory-bank/archive/archive-TASK-20260413-01.md`
- TASK-20260405-01：Foundation 基础库 → 归档 `memory-bank/archive/archive-TASK-20260405-01.md`
- TASK-20260405-02：Graphics HAL + Platform HAL → 归档 `memory-bank/archive/archive-TASK-20260405-02.md`
- TASK-20260405-03：DOM 树 + HTML 解析器 → 归档 `memory-bank/archive/archive-TASK-20260405-03.md`
- TASK-20260405-04：CSS 引擎 → 归档 `memory-bank/archive/archive-TASK-20260405-04.md`
- TASK-20260405-05：消化技术债务 → 归档 `memory-bank/archive/archive-TASK-20260405-05.md`
- TASK-20260405-06：Layout Engine 布局引擎 → 归档 `memory-bank/archive/archive-TASK-20260405-06.md`
- TASK-20260405-07：渲染管线（Render Pipeline） → 归档 `memory-bank/archive/archive-TASK-20260405-07.md`
- TASK-20260405-08：事件系统（Event System） → 归档 `memory-bank/archive/archive-TASK-20260405-08.md`
- TASK-20260405-09：脏区更新与重绘机制 → 归档 `memory-bank/archive/archive-TASK-20260405-09.md`
- TASK-20260405-10：事件循环与应用壳 → 归档 `memory-bank/archive/archive-TASK-20260405-10.md`
- TASK-20260405-11：C API 层 → 归档 `memory-bank/archive/archive-TASK-20260405-11.md`
- TASK-20260405-12：示例应用 → 归档 `memory-bank/archive/archive-TASK-20260405-12.md`
- TASK-20260405-13：CSS 动画系统（CSS Transitions） → 归档 `memory-bank/archive/archive-TASK-20260405-13.md`
