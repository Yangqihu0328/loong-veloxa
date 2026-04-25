# 活跃上下文

## 当前阶段
构建中·R1 完成（#25 LayoutBox origin helpers — 19 helper + Point struct + 10 处替换 + ctest 960/960 + bench Flat/64 3715 ns +0.16%），等待用户确认进入 R2 #28

## 当前任务

**TASK-20260426-01：Layout 正确性消化（#25 + #28 + #20 + #21）** — Level 4

- **目标：** 消化 `techContext.md §技术债务清单` 4 项 Layout 正确性债（D1 全包策略，多轮次 Build 中间态）：
  - **#25** LayoutBox 缺 `border_box_origin()` / `padding_box_origin()` / `content_box_origin()` 辅助方法（坐标计算分散在 `layout_engine.cc` / `flex_layout.cc` 多处）
  - **#28** HTML 解析器 `parser.cc:95` 把 `style` 属性当作普通 attribute 处理，未调用已存在的 `CssParser::ParseDeclarationList`（JS 侧 `dom_bindings.cc:322` 已成熟使用此 API；缺口在 HTML 路径侧）
  - **#20** Block 布局完全无 margin collapsing（`layout_engine.cc:209-212` 直接 `y_offset += border_box + margin`）— CSS 2.1 §8.3.1 adjoining / nested / negative / collapse-through / clearance
  - **#21** LayoutInline 简化（`layout_engine.cc:260-303` 用 `line_height = max(child.height)`，**无 baseline / ascent / descent / vertical-align / line-height**；`TextShaper` 已有 `f32 baseline` 字段但未流入 layout）
- **复杂度：** Level 4（多子系统 + 架构决策 + #20/#21 各自单独够撑 Level 3）
- **来源：** `techContext.md §技术债务清单` + `tasks.md §待立项候选 包 D` + 本次 /van 用户决策 D1 全包
- **安全相关：** ✅ 标注（#28 接收 HTML `style="..."` 外部输入；`ParseDeclarationList` 已被 JS 路径用过相对成熟，但 spec 阶段需补轻量威胁建模 — DoS via 巨大 declaration / `url()` 引用 / 未实现 CSS var 退化）
- **分支：** `feature/TASK-20260426-01-layout-correctness`（基于 main `9f7f338`，已创建）
- **下一步：** `/build` R0 准备阶段（基线核验 951 PASS + wpt fixture 拉取 8 例 + grep 4 类 fingerprint）
- **设计 spec：** `docs/specs/2026-04-26-layout-correctness-design.md`（决策矩阵 6 维 / 17 验收标准 / 8 风险登记 / 安全威胁 7 类 / 文件清单 11 修改 + 9 新增）
- **实现 plan：** `docs/plans/2026-04-26-layout-correctness.md`（R0 准备 + R1-R4 四轮 + R5 finalize / 估时 plan 900 min × 0.6 = 540 min / 子代理 3 处 D3 任务）
- **代理实证：** `172.22.32.1:7890`（WSL2 host gateway）已于 plan 阶段实证可用，wpt 仓库 114 个 margin-collapse fixture 备选，build §0 阶段拉取 8 例
- **创意文档（已落盘 + 全 ACCEPT）：**
  - `memory-bank/creative/creative-margin-collapsing.md`（R3 #20）— D1 5 决策：方案 A MarginChain in-line 累积 + 内部 `Vector<MarginChain>` 栈式状态 + 仅 `overflow: hidden\|scroll\|auto` BFC root + `#if VX_DEBUG_LAYOUT` Trace + 先 layout child → 回填 child.y
  - `memory-bank/creative/creative-line-box-model.md`（R4 #21）— D2 5 决策：A.1 显式 LineBox + Vector + B.1 严格 2-pass vertical-align + C.1 TextMetrics 加字段不删 + `[[deprecated]]` + inline-block atomic + CSS 2.1 §10.8.1 line-height 默认语义

### VAN 阶段产出

| # | 维度 | 选择 | 理由 |
|:-:|---|---|---|
| V1 | 范围包 | **包 D（Layout 正确性）** | 用户从 5 包候选中选定（#46/#47/#49 等其他包属 P3 触发型；包 D 是规范级正确性硬骨头）|
| V2 | 拆分策略 | **D1 全包一次过 + 多轮次 Build 中间态** | 4 子任务依次 #25 → #28 → #20 → #21，每 Round 独立验收中间态（complexity-levels.mdc §多轮次 Build 中间态协议）|
| V3 | Git 分支 | **feature/TASK-20260426-01-layout-correctness** | 基于 main `9f7f338`，已创建 |
| V4 | 复杂度 | **Level 4** | 4 子系统 + 架构决策 + #20/#21 单独 Level 3 量级 |
| V5 | 安全标注 | **[安全相关]** | #28 路径接收 HTML 外部输入；spec §威胁建模轻量覆盖 |

### VAN 阶段代码实证（落实 P0「方案根因假设未先验证」+ TASK-13 反思 #2 基础假设核查）

| # | 假设/命题 | grep 实证 | 影响 |
|:-:|---|---|---|
| F1 | 「#28 LayoutEngine::BuildTree 不解析 inline style」 | ⚠️ **描述不精确**：`layout_engine.cc:35` 已传 `element->inline_declarations()` 给 StyleResolver；真实缺口在 `html/parser.cc:95` 把 `style` attr 当普通 attribute（未识别 + 未调 `ParseDeclarationList`）| spec/plan 阶段修正 #28 描述：缺口在 HTML 解析器侧，非 layout 侧；修复路径 = `html/parser.cc` 处理 attribute 时识别 `style` → 调 `CssParser::ParseDeclarationList` → 逐条 `SetInlineDeclaration` |
| F2 | 「`ParseDeclarationList` API 是否存在」 | ✅ `css/parser.h:12` + `css/parser.cc:202` 完整实现；`script/dom_bindings.cc:322` 已成熟使用（CSSText setter 路径）| #28 实施零 API 设计成本 |
| F3 | 「LayoutBox 当前 origin 计算分散位置」 | ✅ 实证 `layout_engine.cc` (5 处) + `flex_layout.cc` (15+ 处) 直接读 `child->x`/`child->y` + `margin[*]` 拼接，确实分散；helper 重构有意义 | #25 改造影响面：`layout_engine.cc` 5 处 + `flex_layout.cc` 15+ 处替换 |
| F4 | 「Block 布局当前是否有任何 margin collapsing」 | ✅ `layout_engine.cc:209-212` `y_offset = child->y + border_box + margin[Bottom]` 完全直加，零 collapsing 实现 | #20 是从零实施（不是补全） |
| F5 | 「LayoutInline 是否流入 TextShaper.baseline」 | ✅ `text_shaper.h:12` 有 `f32 baseline` 字段，但 `layout_engine.cc:266-280` 仅用 `metrics.width`/`.height`；baseline 字段未消费 | #21 第一步是接入 baseline + 引入 LineBox 抽象 |
| F6 | 「FetchContent 代理状态」 | ✅ 项目根 + `script/CMakeLists.txt` 含 `FetchContent_Declare`；`_deps/` 已离线预置 quickjs-ng + benchmark；本任务**不引入新依赖**（layout/html/css 全是已有代码改造）| 跳过 git proxy 守卫 |

### 前置验证清单

| 维度 | 结果 | 备注 |
|---|:-:|---|
| 依赖可获取性 | ✅ | 无新依赖（全部 vx_core/vx_text 已有模块）|
| 环境就绪 | ✅ | `build/` Debug + `build-bench/` Release 均可复用 |
| 已有 artifact | ✅ | `tests/integration/` + `tests/layout/` 已成熟（TASK-19-01 集成测试规范沉淀）；`bench_layout_buildtree` / `bench_layout_flex` 可作回归基线 |
| FetchContent 代理守卫 | ⊘ 跳过 | 不引入新依赖（F6）|
| 待处理事项关联 | ✅ 多条适用 | **plan × 0.6 第 10 数据点**（首个 Layout 正确性 Level 4 任务，预期 ≥ 1.0× 「准确档」非「最窄」）+ **多轮次 Build 中间态**（complexity-levels.mdc §轮次完成协议；TASK-19-13 P3 落实路径）+ **集成测试**（涉及跨 html/css/layout 模块）+ **Mixed TDD RED 反向探针**（每 Round 必标配）+ **安全**（#28 外部输入路径，spec §威胁建模轻量覆盖）+ **Bench 回归网**（TASK-24-01 baseline `bench_layout_*` 不退化超 10%）|

## 最近完成

**TASK-20260425-01：SDL2 窗口后端 + 输入事件桥接** — Level 3 ✅ 已归档

- 目标：为 Veloxa 引入第一个真实窗口后端（SDL2），打通 `vx_view_run()` WSLg / Linux Desktop 实时显示路径 + SDL 输入事件桥接 `VxInputEvent`
- 关键成果：13 新文件 + 16 修改文件 / 3113 行净增；ctest 951/951 PASS（Debug + Release `-O3 -Werror`）；plan ×0.6 第 9 数据点 0.22× 历史最快「最窄路径」第 4 次确认；5 新模式 + 2 新 plan §0 grep 子段沉淀长期知识库；P6.1 WSLg 真窗口手测标遗留（headless smoke 已自动覆盖渲染管道 + 输入分发 + 退出路径）
- 已 `--no-ff` 合并到 main `4a096ab`（17 commits）
- 归档：`memory-bank/archive/archive-TASK-20260425-01.md`

**TASK-20260424-04：SoftwareCanvas::DrawText 真路径 warm 残余优化（D 纯收尾模式）** — Level 2 ✅ 已归档

- 目标：warm Medium **3499 → < 3200 ns**（-299 ns / -8.5%，新结构性阈值；达成即归档）
- 来源：TASK-20260424-03 归档 §9.2 残余 499 ns P3 触发型候选
- **VAN 基础假设核查（已完成）：**
  - (a) skip-all-zero AA fast path — 条件命题，/plan 阶段进一步实证：SSE2 主循环无块级 zero-skip，但 FT_Bitmap 已 crop bbox → 净收益 ≈ 0 风险高 → **放弃**
  - (b) GlyphCache row_ptr 数组 — 基本否决（Phase 6 B2 已做大头，残余 ≤ 20 ns）
  - (c) hb_shape cache per-text — 高收益 -1000 ~ -2100 ns（hit 路径节省 hb_shape_full 全部成本），**/plan 选定主攻方向**
- **/plan 头脑风暴 5 决策（已锁定）：**
  - Q1 候选组合：**方案 B（仅 (c) hb_shape cache + 精简 FIFO LRU）**
  - Q2 Cache key：**K2（u64 xxhash fingerprint + text_len 碰撞降级护栏）**
  - Q3 存活范围：**S2（per-FontManager 成员）**，FontManager 生命周期对齐
  - Q4 容量策略：**C1（固定 128 entry + FIFO 淘汰）**，内存天花板 ~40 KB
  - Q5 回归护栏：**B1 RoundRobin 256 门槛** + B3 AllMiss 参考（入 baseline CSV 不计门槛）
- **设计文档：** `docs/specs/2026-04-24-drawtext-shape-cache-design.md`
- **实现计划：** `docs/plans/2026-04-24-drawtext-shape-cache.md`（6 Phase / 12 Task / 预估 ~2h 3m × 0.6 校准）
- **需要创意阶段：** ❌ 否（Level 2；所有设计决策已锁定）
- 分支：`feature/TASK-20260424-04-drawtext-residual-opt`（基于 main `78cabf4`，已创建）
- **构建结果（Phase 1-6 全部完成）：**
  - P1 `b8f700e`: HashBytesU64 FNV-1a + 3 tests PASS
  - P2 `f081ed9`: ShapeCache FIFO + FontHandle 提取 + 7 tests PASS（含 T6 碰撞降级）
  - P3 `623ad47`: FontManager::ShapeOrLookup + DrawText 接入 + HbBufferHolder 提取 + 4 集成 tests；917/917 ctest PASS；Release -O3 干净
  - P4 `2b4310a`: 2 新 BMs（RoundRobin / AllMiss）+ VX_SHAPE_CACHE_OFF env 开关 + pre-baseline 采集
  - P5 / P6（本 commit 合并）：WSL2 稳态协议 10 reps bench + baseline.json + README 刷新
- **最终门槛判决（全部 ✅）：**
  - Warm_Medium: 3499 → **2350 ns mean / 1877 ns single**（-32.8% / -46.4%；< 3200 ns 目标超额 850 ns；间接达成技术刚性 <3000 ns）
  - Warm_Short: 677 → 311 ns (-54%)；Warm_Long: 10573 → 4333 ns (-59%)
  - TextVarying_RoundRobin (hit=100%): 2676 ns；AllMiss (miss=100%): 4711 ns
  - Cache ON vs VX_SHAPE_CACHE_OFF=1: Warm_Medium 1788 vs 3542 ns (env toggle 精度验证)
  - Fallback / ReplayTextHeavy* 无回归（-0.1% ~ +2% 噪音区间，ReplayReal 反降 -9.8%）
- **回顾完成（2026-04-25）：** `memory-bank/reflection/reflection-TASK-20260424-04.md`
  - 关键发现：(1) plan × 0.6 第 7 数据点 **0.26×**（「最窄路径」子档第 3 次确认，继 TASK-24-01 0.29× / TASK-24-03 0.34×）；(2) D 纯收尾模式门槛 <3200 ns 实测 2350 ns mean / 1877 ns single，**意外直破技术刚性 <3000 ns**，根因：hb_shape API 族（6 次连续调用）消除收益远超经验常数估算；(3) `VX_SHAPE_CACHE_OFF` env toggle 实现 A/B 对照，Cache OFF 3542 ns 与 TASK-24-03 baseline 3499 ns 吻合（1.2% 噪音）；(4) Cold_Medium CV 12.67% 属 FT_Load 噪声非任务范围
  - 3 新模式沉淀 `systemPatterns.md`（Env Toggle A/B 对照 / 预提取依赖 Header 原则 / 第三方 API 消除型优化估时下限公式 `N × single_cost × (1-miss_rate)`）
  - 改进建议：5 条（P1 × 1 Cache BM 稳态数学推演清单 → ✅ 已入 `writing-plans.mdc` §7.1；P2 × 4 ✅ 已入 systemPatterns）
- **归档完成（2026-04-25）：** `memory-bank/archive/archive-TASK-20260424-04.md`（Level 2 全维度归档）

## 下一步

执行 `/build` R0 准备阶段：
1. **基线核验**：`ctest --test-dir build -j 8` 应 951 PASS / 0 FAIL；记录 `bench_layout_buildtree` baseline mean
2. **W3C wpt fixture 拉取**：通过代理 `172.22.32.1:7890` 拉 8 例（4 margin-collapse + 2 IFC + 2 vertical-align baseline）入仓 `tests/fixtures/wpt/`
3. **grep 影响面 fingerprint**：4 类 grep（#25 替换点 / #28 inline_declarations 消费点 / #21 vertical-align 现有 hint / enum_serialization 全链路 fingerprint），输出快照写入 progress.md
4. **R0 退出门**：ctest 951 PASS + wpt 8/8 200 OK + grep #25 ≥ 12 处命中

R0 完成后依次进 R1（#25 helper）→ R2（#28 inline style + 安全护栏）→ R3（#20 margin collapsing）→ R4（#21 LineBox + vertical-align）→ R5 finalize → /reflect。

## 未合并分支

- `feature/TASK-20260426-01-layout-correctness` — TASK-20260426-01 CREATIVE 完成（spec + plan + 2 创意文档全落盘 + D1/D2 全 ACCEPT），等待 /build R0（基于 main `9f7f338`）

## 最近归档

- `memory-bank/archive/archive-TASK-20260425-01.md`（TASK-20260425-01 SDL2 窗口后端 + 输入事件桥接 — Level 3 中等功能 / 6 AskQuestion 决策锁定 / 直通 /build；已 `--no-ff` 合并到 main `4a096ab`；**第一个有可见窗口的平台后端**，解锁 hot reload / inspector / FPS overlay 等 DevTool 主线；`Sdl2WindowSurface` + `Sdl2EventLoop`（**Composition over Inheritance** 内组合 `HeadlessEventLoop` 复用 task/timer，避免继承式 Run/Quit 状态机死结）；`Surface::Present()` virtual no-op + `Application::Update()` 末尾自动调用；`vx_event_loop_create_sdl2()` + `vx_surface_create_window(VxWindowOptions*)` + `vx_event_loop_pump_input()` 三 C API；`vx_view_run()` 自动 `dynamic_cast<Sdl2EventLoop*>` wire input callback；**隐含技术债清理**：`destroy/save_ppm` 改基类指针 + 虚析构（清掉新后端 UB 隐患）；CMake 双轨 SDL2 lookup（`find_package CONFIG` → `pkg_check_modules`）+ `VX_PLATFORM_SDL2=OFF` 默认；`examples/hello_sdl2.cc` 含 `:hover` CSS 规则（A2 验收依赖）+ `VX_HELLO_SDL2_AUTOQUIT_MS` env hook + `SDL_AddTimer` push `SDL_QUIT` 自终止 ctest；`hello_sdl2_smoke` headless ctest (`SDL_VIDEODRIVER=dummy`) 0.22s；ctest **951/951 PASS**（Debug + Release `-O3 -Werror`，+34 cases 含 `SDL_RenderReadPixels` 像素探针 + `SdlQuitTriggersLoopQuit` 修复实证）；**plan × 0.6 第 9 数据点 0.22× 历史最快**「最窄路径」第 4 次确认（180 min plan / ~40 min 实测；条件：6 AskQuestion 提前锁定决策 + 平台抽象成熟 + TDD 节奏稳）；**反复模式 2 新变体**：「计划清单不一致」第 10 次（plan A2 :hover vs example 漂移 UI 行为维度）+「测试隔离问题」第 8 次（`<SDL2/SDL.h>` 误置 anon namespace 污染 `std::abs`）；**5 新模式沉淀 systemPatterns.md**：(1) Composition over Inheritance — Platform Backend 复用模式 + 同名方法歧义防护配套准则（`inner_->Quit()` 误调挂死实证）/ (2) GUI/主循环型程序 ctest 自终止模式（`VX_<APP>_AUTOQUIT_MS` env hook）/ (3) 测试文件 include 卫生模式 / (4) Plan 验收用例与 example 实现一致性检查模式 / (5) 复用 + 配套准则模式；**2 新 plan §0 grep 子段沉淀 writing-plans.mdc**：测试文件 include 卫生 grep + 验收用例与 example 一致性 3 选 1 强制；**P6.1 WSLg 真窗口手测**用户决议标遗留，触发条件 = 下次 GUI 环境可用；后续 TASK-20260425-02 占位（SDL2 二期 + DevTool 主线起点））
- `memory-bank/archive/archive-TASK-20260424-04.md`（TASK-20260424-04 SoftwareCanvas::DrawText 真路径 warm 残余优化 D 纯收尾模式 — Level 2 单候选 /plan→/build 直通；已 `--no-ff` 合并到 main `253fab7`；Warm_Medium **3499→2350 ns mean / 1877 ns single (-32.8% / -46.4%)**，超额门槛 <3200 ns 850 ns **意外直破技术刚性 <3000 ns**；Warm_Short -54% / Warm_Long -59%；**VX_SHAPE_CACHE_OFF** env toggle A/B 精确剥离 cache 贡献（OFF 3542 ≈ TASK-24-03 baseline 3499，1.2% 噪音带）；FNV-1a 64-bit + text_len 双重 key 碰撞概率 2^-96；固定 128 FIFO + per-FontManager scope + 预提取 FontHandle/HbBufferHolder 零循环依赖；ctest 917/917 PASS（+14 cases 含 R2 反向探针 `DifferentTexts_DifferentOutput`）/ Release -O3 -Werror 零警告；**plan × 0.6 第 7 数据点 0.26×**「最窄路径」子档第 3 次确认（继 TASK-24-01 0.29× / TASK-24-03 0.34×）；**归档阶段落实 1 P1 规则**：`writing-plans.mdc` §7.1 Cache BM 稳态访问模式数学推演清单（推演模板 + 速查表 + 反模式）；**3 新模式沉淀 `systemPatterns.md`**（Env Toggle A/B 对照 / 预提取依赖 Header 原则 / 第三方 API 消除型优化估时下限公式 `N × single_cost × (1-miss_rate)`））
- `memory-bank/archive/archive-TASK-20260424-03.md`（TASK-20260424-03 SoftwareCanvas::DrawText 真路径 warm 优化 — **K7 Resolved**；已 `--no-ff` 合并到 main `e6fef0b`；7 Phase 阶梯 + 2 次 R1 AskQuestion 升级（SSE2 4 px/iter → AVX2 8 px/iter `count≥16` 智能阈值 dispatch）；Warm_Medium **5905→3499 ns (-40.7%)**，Warm_Long -39.4%，Cold_Medium 副产品 -46.4%；**业务目标达成**（真路径 3499 ns < Fallback 3608 ns）；技术刚性 D5 `<3000 ns` 差 499 ns (14%) 用户知情接受；**11 pixel_blend GTests + 3 次 RED 反向探针完整循环** (Phase 5 / SSE2 / AVX2)；ctest 59/59 PASS / Release `-O3 -Werror` 0 err/warn；**Phase 5 /255 乘-移位近似试验回退**实证 GCC `-O3` Granlund-Montgomery 覆盖手写（通用编译器洞察）；Phase 6 B2 单 Phase -12.2% 是最大单点收益（6× Phase 1-4 API 层累计）；Phase 7 SSE2 -28.6% 第二大；AVX2 `kAVX2MinPixelsPerRow=16` 保留 CJK/大字号 headroom；**plan × 0.6 第 6 数据点 0.34× 最窄档确认**；**归档阶段落实 2 P1 规则**：`writing-plans.mdc` §7 WSL2 稳态协议 + §8 编译器已做优化识别反模式；**4 新模式沉淀 `systemPatterns.md`**（异构工作负载 SIMD 尺寸阈值 dispatch / 负结果资产化 / 刚性目标+R1 升级路径 plan 模式 / 编译器已做优化识别反模式））
- `memory-bank/archive/archive-TASK-20260424-01.md`（TASK-20260424-01 Layout super-linear knee 根因调查 — 根因 (d) ArenaAllocator 4KB block malloc/free churn 定位；默认 block 4096 → 32768；K2 R256 9.42×→4.18× / K3 R_flex 16.49×→6.40× 均 ~2.5× 改善；Phase 2 block-size 5 档扫描，32K 为 Flex sweet spot；**K8 新发现**：65K block > L1D 触发抖动 → Arena 设计守则「block ≤ L1D」；`DefaultBlockSizeFitsLargeAllocations` GTest + RED 反向探针；ctest 892/892 PASS；**plan × 0.6 第 5 数据点 0.29×**（115 min plan / ~33 min 实测，历史最快，「最窄路径」子档样板）；3 新模式沉淀 `systemPatterns.md`（扫描型脚本化模板+双指标交叉 / 公开行为锚定内部约束 / 最窄路径子档）；残余 ~40% super-linear 拆出 TASK-20260424-02；已 `--no-ff` 合并到 main `0882d0c`）
- `memory-bank/archive/archive-TASK-20260419-13.md`（TASK-20260419-13 流程规则 P0/P1 沉淀冲刺 — 3 条积压条目一次性闭环：P0 FetchContent proxy 守卫（反复 9+ 次痛点终结）/ P1 smoke 工具链可用性 grep（TASK-11 #2）/ P1 多轮次 Build 中间态（TASK-03 Round 1 首发）；9 文件修改（6 规则/命令 + 3 MB）/ 4 phase commits / 反例追溯 7/7 通过（含 meta-dogfooding 实时自证）/ 10 验收 9 ✅ + 1 改进；跨类型估时收敛 plan × 0.6 通用协议（TASK-05/09/11/13 四数据点）；5 新模式沉淀 `systemPatterns.md`（Meta-dogfooding Phase 0 / 基础假设核查 / 单一真相来源占位符 / 实证微调 spec / bench 估时校准扩展跨类型）；已 `--no-ff` 合并到 main `8a436ed`）
- `memory-bank/archive/archive-TASK-20260419-11.md`（TASK-20260419-11 ImageCache::Load HashMap 化 — K6 高 ROI 修复 — 双索引方案（`Vector<Entry>` 保 ABI/Get O(1) + `HashMap<String, ImageHandle, StringHash, StringEq>` 提供 O(1) path 查询）；Hit<256> 1151.77 ns → 45.70 ns（**25.2×↓**），Hit<16> 50.87 → 44.05 ns；ctest 891/891 PASS（含 `ClearAndReloadDeduplicates` D3 回归网，RED 反向探针验证有效）；Release `-O3` 0 errors；3 P1 沉淀：bench 阈值表绝对增量兜底 / Plan grep `which <tool>` / Mixed TDD RED 反向探针；3 P2 沉淀：P1+P2 拆分模式 / HashMap 不是金科玉律 / bench 估时校准 4.2×→2.0× 收敛；已 `--no-ff` 合并到 main `8515c25`）
- `memory-bank/archive/archive-TASK-20260419-09.md`（TASK-20260419-09 Replay 深度基准 + 真 ImageCache 通路 — 2 bench exe / 15 BMs / 2 baseline JSON 入仓；K1 修正归因（fallback 非真路径）+ 真冷路径 14× 慢；K6 新发现 ImageCache::Load O(N) hit 路径（size=256 时 1162 ns）→ 推 TASK-11 P1 高 ROI；K7 新发现 warm 真路径 1.6× 慢 fallback → 推 TASK-12 P2 触发型；落实「方案根因假设未先验证」P0 第 2 次完整应用 + 升级 grep 规则覆盖 CMake 链接可见性）
- `memory-bank/archive/archive-TASK-20260419-05.md`（TASK-20260419-05 Layout + Render 性能基准 — 4 bench exe / 30 BMs / 4 baseline JSON 入仓；K1 实测 DrawText 是 Replay hot path（820× FillRect），ImageCache 不是；推 TASK-20260419-09 候选；落实 P0 #2 `writing-plans.mdc`「性能基准任务必检项」段；已 `--no-ff` 合并到 main `d0999e8`）
- `memory-bank/archive/archive-TASK-20260419-07.md`（TASK-20260419-07 修复 main Release `-Werror` 编译失败 2 处 — fgets unused-result + BasicString copy ctor IPA array-bounds 误报；与 TASK-04 同元模式不同手法 noinline；已 `--no-ff` 合并到 main `206d227`）
- `memory-bank/archive/archive-TASK-20260419-03.md`（TASK-20260419-03 CSS 解析性能基准 — 30 BMs + 3 baseline JSON 入仓 + Cluster 度量证 PropertyMap 均匀 → TASK-06 P1→P3，已 `--no-ff` 合并到 main `660313a`）
- `memory-bank/archive/archive-TASK-20260419-04.md`（TASK-20260419-04 修复 `enum_serialization.cc` Release `-Warray-bounds` 误报，已合并到 main `a09ad1e`）
- `memory-bank/archive/archive-TASK-20260419-02.md`（TASK-20260419-02 Google Benchmark 集成 + Foundation 性能基线，已合并到 main）
- `memory-bank/archive/archive-TASK-20260419-01.md`（TASK-20260419-01 流程规则沉淀 + P2 功能技术债收口，已合并到 main）

## 待处理事项

### 后续任务候选

- **TASK-20260419-06（建议，P3 降级）：** HashMap Hash Mixing 优化（cluster 问题）— `BM_HashMapLookupHitInt/16384=9µs` vs n=64 时 69ns，根因 `H1=h>>7` + `std::hash<int>` 恒等映射。**降级理由（TASK-03 P4 实测）：** PropertyMap 60-entry HashMap<StringView, PropertyId> + djb2 hash 在最差 single key 下仅 2.75× HitHot5（远低于 5× cluster 阈值），证 cluster 问题主要见于 **int key + 大规模**场景。**触发条件**：「短字符串 ≠ 主用例 + 容器规模 > 1000 entry」的新场景出现时再立项
- **TASK-20260419-08（候选，P3 触发型）：** `string.h` 剩余 3 处 runtime-size memcpy（line 45 SSO ctor / 150 Append / 230 GrowAndCopy）防御性 noinline 化。**触发条件**：未来 GCC 升级回归同类 `-Warray-bounds` 误报；目前不主动改避免引入不必要内联开销（来源 TASK-20260419-07 副发现）
- ~~TASK-20260419-10：已立项为 TASK-20260424-01 并完成归档。根因 (d) ArenaAllocator malloc churn 定位 + 32K 落地，knee 收敛 ~60%，详见 `archive-TASK-20260424-01.md`~~
- **TASK-20260424-02（新增，TASK-24-01 Phase 1B 升级路径拆出，建议 P3 触发型）：** Layout 残余 super-linear 调查（per-phase 拆分 BM 定位 (e) L1D 抖动 / (f) 隐藏算法因素）— 承接 TASK-24-01 解决后剩余 ~40% super-linear（R256 仍 4.18× / R_flex 仍 6.40×）+ Phase 2 K8 新发现（65K block Flex 回弹暗示 L1D 抖动）。**触发条件**：下次 layout 性能需求（grid / multi-column）或主动预算
- ~~TASK-20260419-11：已完成并合并到 main `8515c25`，详见 `archive-TASK-20260419-11.md~~`
- ~~TASK-20260419-12：已立项为 TASK-20260424-03 并完成归档，K7 Resolved，详见 `archive-TASK-20260424-03.md`~~
- ~~TASK-20260424-04：已立项并完成归档，详见 `archive-TASK-20260424-04.md`；D 纯收尾模式 Warm_Medium -32.8% 意外直破 <3000 ns 技术刚性目标~~
- **TASK-20260424-05（新增，TASK-24-04 后续候选，P3 触发型）：** DrawText 真路径 ShapeCache 进一步优化 — (a) LRU 替代 FIFO（cap 降到 32 或 workload 偏斜场景）/ (b) Shape result interning（相同 glyph sequence 共享 ShapedRun，CJK 场景节省内存）/ (c) Layout `freetype_shaper::Measure()` 也用 `ShapeOrLookup`（layout pass 10-30% 潜在加速）。**触发条件**：Layout 性能 workload 驱动 / 内存敏感设备暴露 cap 不足 / CJK UI 大量文本节点
- **TASK-20260425-02（新增，TASK-25-01 后续候选，P3 触发型）：** SDL2 backend 二期 + interactive_sdl2 example — (a) `EventLoop::SetInputCallback` 上提到抽象基类（默认 no-op）替代 `dynamic_cast<Sdl2EventLoop>`（TASK-25-01 reflection §8 改进 #3）/ (b) `examples/interactive_sdl2.cc` 含 hover/click/keystroke 三类反馈用例作为 SDL2 backend 长期 reference / (c) `Surface::Present()` 调用约定 contract test（mock surface 验 Update 调用次数）/ (d) `vx_add_sdl2_test` CMake helper 自动加 `SDL_VIDEODRIVER=dummy` env。**触发条件**：第二个有 input 的 platform backend (Win32/Wayland) 立项 / FPS overlay / DevTool 启动 / 累计 5+ 处 SDL_VIDEODRIVER=dummy 重复字符串

### 长期项（按优先级）

- ~~**🔴 P0（紧急升级，反复 9+ 次，TASK-07 已验有效预防）：** Cursor 沙箱内任何 FetchContent 任务的 VAN 阶段**必须**强制重设 git 全局代理~~ → ✅ **已于 TASK-20260419-13 P1 落实**：`writing-plans.mdc` L96「FetchContent 网络代理守卫」段（6 小节）+ `van.md` §1 子项「FetchContent 代理状态检查」+ `techContext.md` L98「Plan/VAN 阶段守卫」交叉引用；代理地址单一真相来源为 `techContext.md`，规则零硬编码 IP，统一占位符 `<开发环境代理地址>`
- ~~**🔴 P0（TASK-07 + TASK-05 第 2 次实证）：** `writing-plans.mdc` 「目标 API 的发射/触发条件 grep」段~~ → ✅ **已于 TASK-20260419-05 /archive 落实**；TASK-09 /reflect 二次升级覆盖 **CMake 链接可见性**（PUBLIC/PRIVATE/INTERFACE，含 PNG::PNG 反例）
- **🟠 P2（频率升级，TASK-07 + TASK-05 + TASK-09 = 3 次）：** `/reflect` 命令 §3.5 反复模式表「方案根因假设未先验证」频率升至 3 次（已被 P0 grep 规则有效抑制；TASK-09 全程 7 处 grep 仅遗漏 1 处链接可见性盲点 → 已通过 TASK-09 reflection 建议 #1 把链接 PUBLIC/PRIVATE/INTERFACE 检查写入 P0 规则）
- **P1（TASK-09 反思建议 #2 + TASK-11/13 实证，已跨类型扩展）：** **通用 plan × 0.6 目标倍率协议**（原 bench 类任务专属已泛化）— TASK-05 3.4× / TASK-09 4.2× / TASK-11 1.5-2.0× / **TASK-13 文档类 1.67-1.86×** / **TASK-24-01 研究类 0.29×** / **TASK-24-03 优化类带 2 R1 升级 0.34×** 跨 6 任务；「最窄路径」子档（plan×0.3）已获 3 个数据点稳定确认（0.24-0.34× 中位 0.29×）。**落实方式**：✅ 已于 TASK-13 /reflect 扩写 `systemPatterns.md`「bench 估时校准」段为跨类型统一；TASK-24-01 /reflect 追加「最窄路径」子档；**TASK-24-03 第 6 数据点确认带 R1 升级的优化类任务可落「最窄」档**（基础设施 100% 复用 + 核心改动 ≤ 10 文件 + 实验脚本化）；下次任一类型任务 ≤ 1.5× 即视为「准确档」，写入 `writing-plans.mdc` 强制条目
- **P1（新增, TASK-11 反思 #1）：** bench plan 阈值表对**超低 ns BM**（baseline < 50ns）应用「绝对增量兜底」，避免噪声误警。建议公式：`判定阈值 = max(baseline × 1.2, baseline + 0.5ns for <5ns / +5ns for [5,50)ns)`。**落实方式**：追加附录到 `systemPatterns.md` "bench 类任务估时校准" 段（已沉淀 ✅）+ 下次 bench plan 阈值表必引。**根因**：TASK-11 P3 实测 Get 0.94→1.16 ns（1.23×，超 1.2× 阈值）但实现完全没改动，纯属测量噪声；Hit<1> 10.35→43.27 ns 是 HashMap 固有 ~32ns 开销，绝对量微小但乘法判定显示「4×」。
- ~~**P1（新增, TASK-11 反思 #2）：** Plan 阶段需 grep `which <tool>` 验证 smoke 工具链可用性~~ → ✅ **已于 TASK-20260419-13 P2 落实**：`writing-plans.mdc` §4 末尾新增 `#### smoke 工具链可用性检查` 子块（jq/bc/valgrind/awk/xmllint/rg 6 工具兜底矩阵 + Plan Phase 0 一次性 batch grep + Build 严禁临时换栈 + 与 `verification.mdc` 协同定位）
- **P1（新增, TASK-11 反思 #3 + TASK-24-01 第 2 次实证 + TASK-24-03 第 3 次成熟应用）：** Mixed TDD 模式下「为预防特定 bug 而新增的回归测试」（D3 类）**标配 RED 反向探针验证**（临时破坏实现确认 FAIL → 恢复确认 PASS）。**落实方式**：沉淀到 `systemPatterns.md` 新模式段「Mixed TDD RED 反向探针实践」（已沉淀 ✅）；**TASK-24-03 单任务内 3 次完整循环（Phase 5 /255 helper / Phase 7 SSE2 channel order / Phase 7b AVX2 permute4x64 imm）→ 模式升级为 Mixed TDD 标配，可写入 `writing-plans.mdc` 强制条目**。**根因**：TASK-11 D3 + TASK-24-01 default block + TASK-24-03 三层 SIMD 均通过反向探针证实测试在「实现已正确」时也能精准 FAIL，耗时 < 3 min 但避免「测试因实现恰巧正确而成为永远不报警的死代码」最大风险
- **P1（已确认，本任务整体回顾再次复确）：** WIP / 中间 commit 的 subject 严禁包含外部任务状态字样（`BLOCKED on TASK-X` / `WAITING for Y` / `PENDING dep`）。**首发：** TASK-20260419-03 Round 1。**落实：** 下次 wip commit 前对照；如再次出现立即升级 P0 + 写入 `git-workflow.mdc`「Commit Subject 规范」段
- ~~**P1（已确认）：** Level 2+ 多 phase 任务（phase 数 ≥ 5）需要支持「轮次完成」中间态~~ → ✅ **已于 TASK-20260419-13 P3 落实**：`complexity-levels.mdc` L68 新段「多轮次 Build 中间态」（跨 Level 2-4 通用扩展 / 触发条件 / 子状态协议 `构建中·轮次 N 完成` / 向前兼容 / 恢复路径 / git 关系 5 小节）+ `build.md` §6.5「轮次完成判断」含轮次完成报告模板 + `reflect.md` §0 守卫放宽识别子状态标签立即返回
- **P1（反复出现）：** 任何引入 GCC/Clang 模板特化技巧（`template<usize N>` 取数组引用、CRTP、SFINAE 分派）的 PR 必须在 PR 检查表中加一行「Release `-O2 -Werror` 通路验证」。来源：TASK-04 + TASK-03 P6 + TASK-07 实证。固化到 `writing-plans.mdc`「Release 通路验证」段
- **P1**：跨子库新增符号引用前 grep link graph，确认是否触发循环依赖（来源 TASK-20260419-01 反思 #1，规则已固化到 `writing-plans.mdc`「静态库循环依赖审计」段）
- ~~**P1**：性能基准任务必须在 Plan 阶段就显式 `-DCMAKE_BUILD_TYPE=Release` + 独立 `build-bench/` 目录~~ → ✅ **已于 TASK-05 /archive 阶段落实**：`writing-plans.mdc`「性能基准任务必检项」§1 强制条目
- **P1**：CMake 操作第三方 target 前必须先用 `get_target_property(... ALIASED_TARGET)` 识别 ALIAS，避免 `set_target_properties` 报错（来源 TASK-20260419-02 反思 #2）
- ~~**P1（新增, TASK-05）：** Bench smoke 验收三件套~~ → ✅ **已于 TASK-05 /archive 阶段落实**：`writing-plans.mdc`「性能基准任务必检项」§4 强制条目 + `systemPatterns.md`「Bench Smoke 自检模式」段
- **P2**：将 `renderer_test` / `render_integration_test` 等剩余手写像素位移断言迁到 `tests/test_pixel_utils.h`（来源 TASK-20260413-02）
- ~~**P2**：google/benchmark `RangeMultiplier(m)->Range(lo,hi)` 的精确数量为 `ceil(log_m(hi/lo))+1~~` → ✅ **已于 TASK-05 /archive 阶段落实**：`writing-plans.mdc`「性能基准任务必检项」§5 强制条目
- ~~**P2（新增, TASK-05）：** 「Render bench 前置清单」+「带否定判据的发现型 Phase」+「跨阶段管道型 API default-nullptr 反模式」~~ → ✅ **已于 TASK-05 /reflect 阶段沉淀到 `systemPatterns.md`**（4 段）+ /archive 阶段交叉引用入 `writing-plans.mdc`「性能基准任务必检项」§6；TASK-09 /reflect 升级「带否定判据」标签为 4/4 成熟实践
- **P1（新增, TASK-13 反思 #1）：** **Meta-dogfooding 作为规则沉淀类任务的 Phase 0 标配动作** — 基线验证时刻意检验新规则触发路径，命中即 T-0 实时自证。**落实方式**：✅ 已沉淀 `systemPatterns.md`「规则沉淀类任务的 Meta-dogfooding Phase 0 模式」段（本任务自证，TASK-13 `rg`/`jq` MISS 触发条目 2 规则）；下次流程规则沉淀类任务 plan Phase 0 直接引用。
- **P1（新增, TASK-13 反思 #2）：** **基础假设核查 — VAN/Plan 前置清单**：对非标准目录/约定（如 `.cursor/commands/*`）做 Glob/Read 实证，避免 plan 建立在错误假设上。**落实方式**：✅ 已沉淀 `systemPatterns.md`「基础假设核查 — VAN/Plan 前置清单」段；下次涉及非 `src/`/`tests/`/`build*/` 目录的任务 VAN 阶段必做。
- **P2（新增, TASK-13 反思 #4）：** **单一真相来源占位符模式**：环境敏感/跨项目复用/可变配置值用占位符，实际值集中 `techContext.md` 等单一真相来源。**落实方式**：✅ 已沉淀 `systemPatterns.md`「配置管理 — 单一真相来源占位符模式」段；下次涉及代理/端口/token 等配置的任务参照。
- **P2（新增, TASK-13 反思 #5）：** **实证微调 spec 范式**：plan 执行时允许基于实证数据微调 spec 细节（非核心意图），在 progress/reflection 标注依据。**落实方式**：✅ 已沉淀 `systemPatterns.md`「计划执行 — 实证微调 spec 范式」段；下次 build 阶段遇实证偏差直接用 "9 ✅ + N 改进" 格式标记。
- **P2（新增, TASK-13 反思 #6）：** **`.editorconfig` / prettier 统一 markdown 表格格式**：避免编辑器 auto-reformat 表格列宽污染 diff。**触发条件**：累计 3+ 任务再出现即立项；当前仅 TASK-13 spec §1 表格 auto-reformat 1 次，暂记观察。
- **P1（新增, TASK-24-01 反思 #1）：** **研究/实验型任务 plan §验收标准段应区分「预期带 / 下限带 / 中间区间处理方案」**（避免刚性阈值卡住流程）。**落实方式**：追加到 `writing-plans.mdc`「研究/调查类任务验收阈值三段式」子段。**根因**：本任务 plan 凭直觉设 R256≤2.5 刚性阈值，实测 4.18× 属于「(d) 贡献 60% + (e)/(f) 承担 40%」的 partial-root-cause 场景，plan 未预见需插入 AskQuestion 承接。
- **P1（新增, TASK-24-01 反思 #2）：** **plan §0 smoke/工具矩阵扩展到 unit test binary 路径**（现仅覆盖 bench binary）。**落实方式**：`writing-plans.mdc`「smoke 工具链可用性检查」子块追加「test binary 路径矩阵 `find build -name '*_test' -type f`」条目。**根因**：本任务 Phase 3 首次跑 GTest 时路径写成 `build/tests/foundation/memory/arena_allocator_test`（源码路径），实际是 `build/tests/arena_allocator_test`（扁平化输出），exit 127。
- **P2（新增, TASK-24-01 反思 #3）：** **扫描型研究任务的 for 循环 + python3 聚合模板 + 双指标交叉验证**沉淀。**落实方式**：✅ 已沉淀到 `systemPatterns.md`「扫描型研究任务脚本化模板 + 双指标交叉验证模式」段（本次 Phase 2 blocked-size 5 档扫描为范例）。
- **P2（新增, TASK-24-01 反思 #4）：** **「最窄路径任务」plan×0.3 子档 + TASK-24-01 0.29× 历史最快数据点**。**落实方式**：✅ 已沉淀到 `systemPatterns.md`「bench 类任务估时校准」段追加子档。下次单文件 1 行改动 + 基础设施 100% 复用的任务直接按 plan×0.3 预估。
- **P2（新增, TASK-24-01 反思 #5）：** **「公开行为锚定内部约束」测试设计哲学**。**落实方式**：✅ 已沉淀到 `systemPatterns.md`「测试设计 — 公开行为锚定内部约束模式」段。本任务 `DefaultBlockSizeFitsLargeAllocations` 用指针连续性间接观测 block 容量，不扩 API 不加 friend 为样板。
- ~~**P1（新增, TASK-24-03 反思 #1，反复模式「前置依赖未验证」新变体）：** **Godbolt 验证编译器魔数优化 — 位运算/除法近似前置检查**~~ → ✅ **已于 TASK-20260424-03 /archive 阶段落实**：`writing-plans.mdc` §8「编译器已做优化识别 — 位运算/除法近似反模式」+ `systemPatterns.md`「已验证的模式（来自 TASK-20260424-03）」段「编译器已做优化识别 — 位运算近似反模式」（含 godbolt 验证命令 + SIMD 例外说明）
- ~~**P1（新增, TASK-24-03 反思 #2）：** **WSL2 / 云机 bench warm-up 协议标准化**~~ → ✅ **已于 TASK-20260424-03 /archive 阶段落实**：`writing-plans.mdc` §7「WSL2 / 云机 bench 稳态协议」（4 步固定模板 + stash-swap 样板）+ `systemPatterns.md`「Bench Smoke 自检模式」追加「WSL2 / 云机 / Docker 稳态协议」附录（CV ≤ 2% 门槛 + 连续 2 次失败记为环境不可信）
- **P2（新增, TASK-24-03 反思 #3-#7）：** 4 新模式 — **异构工作负载 SIMD 尺寸阈值 dispatch** / **负结果资产化** / **刚性目标+R1 升级路径 plan 模式** / **编译器已做优化识别反模式**。**落实方式**：✅ 已沉淀到 `systemPatterns.md`「已验证的模式（来自 TASK-20260424-03 DrawText 真路径 warm 优化）」段；下次 SIMD / Mixed TDD D3 / 多候选优化类任务直接引用
- ~~**P1（新增, TASK-24-04 反思 #4）：** Cache BM 稳态访问模式数学推演清单~~ → ✅ **已于 TASK-20260424-04 /archive 阶段落实**：`writing-plans.mdc` §7.1（推演模板 + FIFO/linear 5 场景速查表 + 反模式 + 交叉引用 env toggle 事后验证）。**根因**：TASK-24-04 P4 `RoundRobin` pool=256 plan 声明 "50% hit" 实际稳态 100% miss（FIFO+linear+cap=128），改为 pool=128 后实际 100% hit；Phase 4 临时调整虽未影响门槛但削弱规划精度
- **P2（新增, TASK-24-04 反思 #1-#3）：** 3 新模式 — **Env Toggle A/B 对照模式**（process-level latched-once 环境变量 `VX_<FEATURE>_OFF` 单 build A/B 对照）/ **预提取依赖 Header 原则**（多 phase 任务预见透明重构第一次就完成，避免循环依赖 & 同文件二次修改）/ **第三方 API 消除型优化估时下限公式** `N × single_cost × (1-miss_rate)`（适用于消除一族连续 API 调用的 warm 路径 hit 优化）。**落实方式**：✅ 已沉淀到 `systemPatterns.md`「已验证的模式（来自 TASK-20260424-04 DrawText hb_shape 结果缓存）」段；下次 cache/memoization/short-circuit 类优化直接引用
- **P2（新增, TASK-24-04 反思 #1）：** `StringView(std::string)` ctor 兼容性与 plan 顶层表完整性自检 — 下次 plan smoke check 既有 grep 条目可覆盖 API ctor 矩阵；plan 尾部自检顶层文件清单（含 transparent refactor）与子任务一致。**触发条件**：累计 3+ 任务再出现即立项升级到规则；当前 TASK-24-04 各 1 次小瑕疵记观察
- ~~**P1（新增, TASK-25-01 反思 #1）：** Composition pattern 内 outer 类自方法显式 `this->Method()` 防与 inner 同名方法混淆~~ → ✅ **已于 TASK-20260425-01 /reflect 阶段落实**：`systemPatterns.md`「已验证的模式（来自 TASK-20260425-01）」段「Composition over Inheritance — Platform Backend 复用模式」+「同名方法歧义防护」配套准则（含 TASK-25-01 `Sdl2EventLoop` `inner_->Quit()` 误调实证）
- ~~**P1（新增, TASK-25-01 反思 #2，反复模式「测试隔离问题」第 8 次新变体）：** 测试文件 include 卫生 grep（new test files / 引入新第三方 C header 时 plan §0 batch grep 必做）~~ → ✅ **已于 TASK-20260425-01 /reflect 阶段落实**：`writing-plans.mdc`「smoke 工具链可用性检查」段后追加新子段「测试文件 include 卫生 grep」（含 TASK-25-01 `<SDL2/SDL.h>` 误置 anon namespace 实证 + 标志性 fingerprint「`'<symbol>' has not been declared in '{anonymous}::std'`」+ rg 一行命令）
- ~~**P1（新增, TASK-25-01 反思 #3，反复模式「计划清单不一致」第 10 次新维度）：** Plan 验收用例与 example 实现一致性检查（plan 引用 hover/click/keystroke→样式变化等 UI 行为时必做）~~ → ✅ **已于 TASK-20260425-01 /reflect 阶段落实**：`writing-plans.mdc`「smoke 工具链可用性检查」段后追加新子段「验收用例与 example 一致性检查」（3 选 1 强制：plan example 段附 CSS/HTML 片段 / §X 步骤 0 同步检查项 / example 与 plan 同 PR）+ TASK-25-01 P6.1 A2 :hover plan-vs-example 漂移实证 + 反模式
- **P2（新增, TASK-25-01 反思 #5-#6）：** 2 新模式 — **GUI/主循环型程序 ctest 自终止协议**（`VX_<APP>_AUTOQUIT_MS` env hook + `SDL_AddTimer` push platform quit event；prod 永不触发；TIMEOUT N 兜底；前缀 `VX_*` 防污染）/ **Platform Backend Composition 复用模式**（每个 backend 内部组合 `HeadlessEventLoop` 复用 task/timer，避免 EventLoop 抽象基类硬塞容器违反单一职责）。**落实方式**：✅ 已沉淀到 `systemPatterns.md`「已验证的模式（来自 TASK-20260425-01）」段；下次 GUI smoke / 新 platform backend 直接引用
- **P1（新增, TASK-25-01 数据点）：** **plan × 0.6 第 9 数据点「最窄路径」第 4 次确认 0.22×**（180 min 估算 / ~40 min 实测，是历史最快记录）。条件：基础设施 100% 复用（platform 抽象成熟）+ 6 个 AskQuestion 提前锁定全部决策 + 无性能/微优化 phase + TDD 节奏稳定。**落实方式**：下次类似条件任务（新 backend / 现有抽象增量 / 决策已锁定）直接按 plan ×0.22-0.30 预估「最窄路径」子档，跨 4 数据点稳定区间 0.22-0.34×（中位 0.28×）；如可写入 `writing-plans.mdc` 强制条目作为 4 数据点稳定门槛
- **P2 / P3 触发型（新增, TASK-25-01 反思 #4 / 技术改进 #1-#5）：** TASK-25-01 后续候选见上文「后续任务候选」TASK-20260425-02 占位（含 EventLoop::SetInputCallback 上提抽象 / interactive_sdl2 example / Surface::Present contract test / vx_add_sdl2_test helper）

