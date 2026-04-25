# 活跃上下文

## 当前阶段
构建中 — Phase 0 进行中（基线核验 + Surface::Present + destroy/save_ppm 基类化）

## 当前任务

**TASK-20260425-01：SDL2 窗口后端 + 输入事件桥接** — Level 3（中等功能）

- 目标：为 Veloxa 引入第一个真实窗口后端（SDL2），让 `vx_view_run()` 在 WSLg / Linux Desktop 上能开窗实时显示渲染结果，并把 SDL 输入事件桥接到现有 `VxInputEvent` / 事件系统三阶段
- 来源：项目主体功能已完成（30 任务归档），实时调试 UI 的前置依赖；解锁后续 DevTool（hot reload / overlay / Inspector）
- 复杂度：**Level 3** — 引入新模块 `veloxa/platform/sdl2/`、需要 surface/event_loop 设计决策、CMake 模式选型、跨现有 `headless` 后端的抽象统一
- 分支：`feature/TASK-20260425-01-sdl2-backend`（已基于 main `e52868b` 创建并切换）
- **环境就绪：** ✅
  - libsdl2-dev 2.0.20 已安装（`pkg-config sdl2` ✅ + `/usr/include/SDL2/SDL.h` ✅ + `/usr/lib/x86_64-linux-gnu/cmake/SDL2/sdl2-config.cmake` ✅）
  - WSLg `DISPLAY=:0` + `WAYLAND_DISPLAY=wayland-0` + `/mnt/wslg`
  - FetchContent `_deps/` 离线预置，本任务不引入新 FetchContent
- **/plan 头脑风暴 6 决策（已锁定）：**
  - Q1 输入事件投递路径：**(B) `Sdl2EventLoop::PumpInputEvents(callback)` 裸函数** — EventLoop 不反向持有 View；保持平台抽象单向
  - Q2 Surface 像素呈现时机：**(B) `Surface` 抽象增加 `virtual void Present() {}` 默认 no-op** — SDL2 重写为 `SDL_UpdateTexture + RenderPresent`
  - Q3 CMake SDL2 查找：**(C) 双轨兜底** — `find_package(SDL2 CONFIG QUIET)` 优先，失败 fallback `pkg_check_modules`
  - Q4 默认构建可选性：**(C) 默认 OFF，需 `-DVX_PLATFORM_SDL2=ON`** — 与 `VX_BUILD_BENCHMARKS` 风格一致
  - Q5 examples 形态：**(B) 保留 `hello.cc` 不动 + 新增 `examples/hello_sdl2.cc`** — 仅 SDL2 ON 时构建；含 ctest smoke (`SDL_VIDEODRIVER=dummy`)
  - Q6 C API 命名：**(C) `vx_event_loop_create_sdl2()` + `vx_surface_create_window(const VxWindowOptions*)`** — options struct 与现有 `VxViewConfig` 风格一致
- **隐含范围（必须做）：** 修复 `vx_event_loop_destroy` / `vx_surface_destroy` / `vx_surface_save_ppm` 在 `veloxa_api.cc` 硬编码 `HeadlessEventLoop*` / `MemorySurface*` 的技术债（加 SDL2 后端时 `delete` 错的派生类指针会 UB）— 改为基类指针调用
- **设计规格：** `docs/specs/2026-04-25-sdl2-window-backend-design.md`（13 段，含安全威胁建模 + 注入点核对表 + R1-R6 风险登记）
- **实现计划：** `docs/plans/2026-04-25-sdl2-window-backend.md`（6 Phase / 12 任务 / 13 新增测试 + ctest smoke）
- **需要创意阶段：** ❌ 否（Level 3 但所有设计决策已通过头脑风暴 Q1-Q6 锁定 + 接口签名已具体化；可直接 /build）
- **估时：** plan 300 min × 0.6 = ~180 min 实测预期（第 8 数据点，首个新模块类任务）
- **下一步：** 用户审查 spec/plan → `/build` 进入 Phase 0 基线核验

## 最近完成

**TASK-20260424-04：SoftwareCanvas::DrawText 真路径 warm 残余优化（D 纯收尾模式）** — Level 2 ✅ 已归档

## 最近完成

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

使用 `/van` 启动新任务。候选任务列表见「待处理事项 → 后续任务候选」段。

## 未合并分支

（所有待归档分支均已合并到 main；新任务启动时再创建 feature 分支）

## 最近归档

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

