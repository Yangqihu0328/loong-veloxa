# 进度记录

## 当前任务

**TASK-20260425-01：SDL2 窗口后端 + 输入事件桥接** — Level 3 / 🟢 规划完成

- 阶段：VAN ✅ + /plan ✅ → 等待用户审查 → /build
- 分支：`feature/TASK-20260425-01-sdl2-backend`（基于 main `e52868b`）
- 决策（Q1-Q6 锁定）：(B) PumpInputEvents callback / (B) Surface::Present virtual / (C) CMake 双轨 / (C) `VX_PLATFORM_SDL2=OFF` 默认 / (B) 新增 hello_sdl2.cc / (C) options struct
- 隐含范围：veloxa_api.cc destroy/save_ppm 基类化（修复硬编码派生类指针 UB 隐患）
- 设计：`docs/specs/2026-04-25-sdl2-window-backend-design.md`
- 计划：`docs/plans/2026-04-25-sdl2-window-backend.md`（6 Phase / 12 任务 / 14 GTests + smoke）
- 估时：300 min × 0.6 = ~180 min（plan × 0.6 第 8 数据点，首个新模块类任务）
- 环境：libsdl2-dev 2.0.20 ✅ / WSLg ✅ / FetchContent 不引入

### 构建里程碑

| Phase | 任务 | 状态 | 备注 |
|---|---|:-:|---|
| P0.1 | 基线核验 | ✅ | SDL2 2.0.20 / 工具链全 / proxy 空 / **ctest 917/917 PASS 1.00s** / branch on feature |
| P0.2 | Surface::Present virtual no-op | ✅ | TDD |
| P0.3 | destroy/save_ppm 基类化 | ✅ | 覆盖补充 |
| P1.1 | CMake 骨架 + VX_PLATFORM_SDL2 选项 | ✅ | TDD |
| P1.2 | TranslateSdlEvent 鼠标事件 | ✅ | TDD |
| P1.3 | TranslateSdlEvent 键盘事件 + 修饰位 + Quit | ✅ | TDD |
| P2.1 | Sdl2WindowSurface RAII ctor/dtor + getter | ✅ | TDD |
| P2.2 | Sdl2WindowSurface Lock/Unlock/Present 实现 | ✅ | TDD |
| P3.1 | Sdl2EventLoop 基础接口（Run/Quit/PostTask/SetTimer） | ✅ | TDD |
| P3.2 | Sdl2EventLoop PumpInputEvents + SetInputCallback | ✅ | TDD |
| P4.1 | VxWindowOptions + create_sdl2 / create_window C API | ✅ | TDD |
| P4.2 | vx_event_loop_pump_input helper | ✅ | TDD |
| P5.1 | examples/hello_sdl2.cc | ✅ | vx_view_run 自动 wire SDL2 callback；composition over inheritance |
| P5.2 | ctest hello_sdl2_smoke (dummy driver) | ✅ | VX_HELLO_SDL2_AUTOQUIT_MS hook；**ctest 951/951 PASS 1.07s** |
| P6.1 | WSLg 手工验证 A1/A2/A3（用户协助） | 🟡 遗留 | 用户当前环境不便实测；**触发条件**：下次有 GUI/WSLg 桌面环境时复跑 `./build/examples/hello_sdl2` 验证 A1（窗口 + 三色块）/A2（鼠标无崩溃）/A3（× 关闭后输出 `Done.`）；headless smoke (`SDL_VIDEODRIVER=dummy`) 已在 P5.2 自动覆盖 |
| P6.2 | Release -O3 -Werror 通路验证 | ✅ | `build-release/`（-DVX_PLATFORM_SDL2=ON）`-O3 -Werror` 0 警告；ctest **951/951 PASS 1.24s** |
| P6.3 | techContext + productContext 文档更新 | ✅ | techContext 新增 SDL2 行 + Platform Backends 段；productContext 加 SDL2 ✅ 行 |

## 已完成任务

- **TASK-20260424-04：SoftwareCanvas::DrawText 真路径 warm 残余优化（D 纯收尾模式）**（Level 2 单候选 /plan→/build 直通）— Warm_Medium **3499→2350 ns mean / 1877 ns single (-32.8% / -46.4%)** 超额门槛 <3200 ns 850 ns **意外直破技术刚性 <3000 ns**；Warm_Short -54% / Warm_Long -59%；**VX_SHAPE_CACHE_OFF** env toggle A/B 精确剥离 cache 贡献（OFF 3542 ≈ TASK-24-03 baseline 3499，1.2% 噪音）；FNV-1a 64-bit + text_len 双重 key 碰撞 2^-96；固定 128 FIFO + per-FontManager scope + 预提取 FontHandle/HbBufferHolder 零循环依赖；ctest 917/917 PASS（+14 cases 含 R2 反向探针）/ Release -O3 -Werror 零警告；**plan × 0.6 第 7 数据点 0.26×「最窄路径」第 3 次确认**（继 TASK-24-01 0.29× / TASK-24-03 0.34×）；**归档阶段落实 1 P1 规则**：`writing-plans.mdc` §7.1 Cache BM 稳态访问模式数学推演清单；**3 新模式沉淀 `systemPatterns.md`**（Env Toggle A/B / 预提取 Header / 第三方 API 消除公式）；9 commits；详见 `memory-bank/archive/archive-TASK-20260424-04.md`
- **TASK-20260424-03：SoftwareCanvas::DrawText 真路径 warm 优化**（Level 2-3 优化类）— 7 Phase 阶梯 + 2 次 R1 AskQuestion 升级（SSE2 4 px/iter → AVX2 8 px/iter `count≥16` 智能阈值 dispatch）；Warm_Medium **5905→3499 ns (-40.7%)**，Warm_Long -39.4%，Cold_Medium 副产品 -46.4%；**业务目标达成**（真路径 3499 ns < Fallback 3608 ns，K7 **Resolved**），技术刚性 D5 `<3000ns` 差 499 ns (14%) 2 次 R1 后用户接受；**11 pixel_blend GTests + 3 次 RED 反向探针完整循环**（Phase 5 /255 helper / Phase 7 SSE2 channel order / Phase 7b AVX2 permute4x64 imm）；ctest 59/59 PASS / Release `-O3 -Werror` 0 err/warn；**Phase 5 /255 乘-移位近似试验回退**实证 GCC `-O3` Granlund-Montgomery 覆盖手写（通用编译器洞察）；**Phase 6 B2 pre-clip + row ptr** 单 Phase -12.2% 是最大单点收益（API 层 Phase 1-4 累计仅 -1.9%，算法层贡献是 6 倍）；**Phase 7 SSE2 -28.6%** 是第二大收益；AVX2 在 ASCII (6-12 px) 无净收益 → `kAVX2MinPixelsPerRow=16` 智能阈值为 CJK / 大字号 / 未来硬件保留 headroom；**plan × 0.6 第 6 数据点 0.34× 最窄档确认**（~230 min 完整 scope vs ~78 min 实测）；**归档阶段落实 2 P1 规则**：`writing-plans.mdc` §7 WSL2 / 云机 bench 稳态协议 + §8 编译器已做优化识别 — 位运算/除法近似反模式；**4 新模式沉淀 `systemPatterns.md`**（异构工作负载 SIMD 尺寸阈值 dispatch / 负结果资产化 / 刚性目标+R1 升级路径 plan 模式 / 编译器已做优化识别反模式）；`benchmarks/baseline/bench_drawtext.json` + README.md K7 → Resolved；`--no-ff` 合并到 main → 归档 `memory-bank/archive/archive-TASK-20260424-03.md`
- TASK-20260424-01：Layout super-linear knee 根因调查（研究类）— 根因定位 (d) ArenaAllocator 4KB block malloc/free churn；默认 block 4096 → 32768；K2 R256 9.42×→4.18× / K3 R_flex 16.49×→6.40×；Phase 2 block-size 5 档扫描（4K/8K/16K/32K/65K），32K 为 Flex sweet spot；**K8 新发现**：65K block > L1D 触发抖动 → Arena 设计守则「block ≤ L1D」；`DefaultBlockSizeFitsLargeAllocations` GTest + RED 反向探针；ctest 892/892 PASS；**plan × 0.6 第 5 数据点 0.29×**（115 min plan / ~33 min 实测，历史最快，「最窄路径」子档样板）；3 新模式沉淀 `systemPatterns.md`（扫描型脚本化模板+双指标交叉 / 公开行为锚定内部约束 / 最窄路径子档）；残余 ~40% super-linear 拆出 TASK-20260424-02 → 归档 `memory-bank/archive/archive-TASK-20260424-01.md`
- TASK-20260419-13：流程规则 P0/P1 沉淀冲刺 — 3 条积压条目一次性闭环（P0 FetchContent proxy 守卫 / P1 smoke 工具链 grep / P1 多轮次 Build 中间态）；9 文件修改 / 8 commits / 反例追溯 7/7 通过（含 meta-dogfooding 实时自证）/ 10 验收 9 ✅ + 1 改进；跨类型估时收敛 plan × 0.6 通用协议（TASK-05/09/11/13 四数据点 3.4× → 4.2× → 1.5-2.0× → 1.67-1.86×）；5 新模式沉淀 `systemPatterns.md`（Meta-dogfooding Phase 0 / 基础假设核查 / 单一真相来源占位符 / 实证微调 spec / bench 估时校准扩展跨类型）；已 `--no-ff` 合并到 main `8a436ed` → 归档 `memory-bank/archive/archive-TASK-20260419-13.md`
- TASK-20260419-11：ImageCache::Load HashMap 化（K6 高 ROI 修复）— 双索引方案 (`Vector<Entry>` 保 ABI/Get O(1) + `HashMap<String, ImageHandle, StringHash, StringEq>`)；Hit<256> 1151.77 ns → 45.70 ns（25.2×↓），Hit<16> 50.87 → 44.05 ns；ctest 891/891 PASS（含 D3 `ClearAndReloadDeduplicates` 回归网，RED 反向探针验证有效）；Release `-O3` 0 errors；3 P1 + 3 P2 改进沉淀；plan 55-80 min vs 实测 ~35-40 min（估时校准协议首次实证 4.2×→2.0× 收敛）→ 归档 `memory-bank/archive/archive-TASK-20260419-11.md`
- TASK-20260419-09：Replay hot path 深度基准 + 真 ImageCache 通路（A+B 子集；2 bench exe / 15 BMs / 2 baseline JSON 入仓；K1 修正归因（fallback 非真路径）+ 真冷路径 14× 慢；K6 新发现 ImageCache::Load O(N) hit 路径 → 推 TASK-11 P1 高 ROI；K7 新发现 warm 真路径 1.6× 慢 fallback → 推 TASK-12 P2 触发型；落实「方案根因假设未先验证」P0 第 2 次完整应用 + grep 规则覆盖 CMake 链接可见性）→ 归档 `memory-bank/archive/archive-TASK-20260419-09.md`
- TASK-20260419-05：Layout + Render 性能基准（4 bench exe / 30 BMs / 4 baseline JSON 入仓 + K1 实测 DrawText 是 Replay hot path 820× FillRect / ImageCache 不是 → 推 TASK-09 候选；落实 P0 `writing-plans.mdc`「性能基准任务必检项」段）→ 归档 `memory-bank/archive/archive-TASK-20260419-05.md`
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
