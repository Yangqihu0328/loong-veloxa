# 技术上下文

## 技术栈

### 核心语言
- **C++17** — 引擎核心实现
- **C** — 对外 API 层（ABI 稳定性）

### 代码规范
- **Google C++ Style Guide** — 命名、格式、异常（禁用）等遵循 Google 规范
- `.clang-format`：BasedOnStyle: Google
- `.clang-tidy`：启用 bugprone/google/modernize/performance/readability 检查

### 构建系统
- **CMake** — 主构建系统（跨平台，嵌入式工具链友好）
- **vcpkg** — 第三方依赖管理（可选）

### 图形后端（按优先级）
- **OpenGL ES 2.0/3.0** — 嵌入式主力后端
- **Vulkan** — 高性能可选后端
- **Skia（精简）** — 可选 2D 渲染路径
- **Software Renderer** — 无 GPU 回退

### 脚本引擎
- **QuickJS（quickjs-ng）** — 轻量 ECMAScript 引擎；**已集成**（TASK-20260413-01）
  - CMake：`veloxa/script/CMakeLists.txt` 使用 `FetchContent` 固定 **`v0.14.0`**，目标 `qjs` + `qjs-libc`
  - 模块：静态库 **`vx_script`**，`vx::script::QuickjsEngine`（`Init` / `Shutdown` / `EvalGlobal`）
  - 策略：`JS_SetMemoryLimit` 默认 **32MiB**/Runtime；**不**调用 `js_std_add_helpers`；源码 **256KiB** 上限；异常路径：`JS_FreeValue` → `JS_GetException`（见 `api-test.c`）
  - 构建：根目录全局 `-Werror`/`-Wpedantic` 已限制为 **`$<COMPILE_LANGUAGE:CXX>`**，避免污染上游 C 源码；WSL 无 DNS 时需 **`http_proxy`/`https_proxy`**（或离线预置 `_deps`）以便首次 `FetchContent`

### 第三方依赖
| 库 | 用途 | 备注 |
|---|---|---|
| quickjs-ng (QuickJS) | JavaScript 引擎 | **已接入** v0.14.0，FetchContent；周期性 CVE/发行说明对照（自动化工具缺位） |
| FreeType | 字体光栅化 | **已接入**（TASK-20260414-01），系统包 find_package(Freetype) |
| HarfBuzz | 文本整形 | **已接入**（TASK-20260414-01），系统包 pkg_check_modules(HARFBUZZ) |
| libpng | PNG 图片解码 | **已接入**（TASK-20260414-01），系统包 find_package(PNG) |
| libjpeg-turbo | JPEG 图片解码 | **已接入**（TASK-20260414-01），系统包 find_package(JPEG) |
| libwebp | WebP 图片解码 | 未接入（系统缺少 -dev 包），延期 |
| zlib | 压缩 | 待接入（资源打包） |
| libuv | 异步 I/O | 待接入（事件循环） |
| google/benchmark | 性能基准 | **已接入** v1.9.1（TASK-20260419-02），`FetchContent`，仅在 `-DVX_BUILD_BENCHMARKS=ON` 时拉取 |
| SDL2 | 窗口/输入后端 | **已接入**（TASK-20260425-01），系统包双轨查找（`find_package(SDL2 CONFIG)` 优先，`pkg_check_modules` 兜底），WSL2 包名 `libsdl2-dev`（最低 2.0.12，实测 2.0.20）；仅在 `-DVX_PLATFORM_SDL2=ON` 时构建 `vx_platform_sdl2` |

## 平台后端（Platform Backends）

`veloxa/platform/` 按 `Surface` + `EventLoop` 抽象提供可替换的窗口/事件子系统。

| 后端 | 头文件目录 | 状态 | 用途 |
|---|---|---|---|
| Headless / Memory Surface | `veloxa/platform/{memory_surface,headless_event_loop}.h` | ✅ 默认始终编译 | 单元测试、图片导出（`SavePPM`）、CI |
| **SDL2** | `veloxa/platform/sdl2/` | ✅ 可选 (`VX_PLATFORM_SDL2=ON`) | 桌面/WSLg 实时窗口；`Sdl2WindowSurface` + `Sdl2EventLoop`（内部组合 `HeadlessEventLoop` 复用 task/timer） |

### Surface 抽象增量

- `Surface::Present()` `virtual void` 默认 no-op（headless 后端继续行内为空），SDL2 后端重写为 `SDL_UpdateTexture + RenderClear + RenderCopy + RenderPresent`；`Application::Update()` 末尾自动调用
- `vx_event_loop_destroy` / `vx_surface_destroy` / `vx_surface_save_ppm` 在 TASK-20260425-01 改为基类指针 + 虚析构，避免新后端 UB

### SDL2 接入要点（构建/运行）

- 配置：`cmake -B build -DVX_PLATFORM_SDL2=ON`
- WSL 安装：`sudo apt install libsdl2-dev`
- 运行：`./build/examples/hello_sdl2`（WSLg 自动开窗）
- 无显示环境：`SDL_VIDEODRIVER=dummy ./build/examples/hello_sdl2`（headless smoke）
- 测试 hook：`VX_HELLO_SDL2_AUTOQUIT_MS=200`（hello_sdl2_smoke ctest 用，定时 push SDL_QUIT 自终止）
- C API：`vx_event_loop_create_sdl2()` + `vx_surface_create_window(VxWindowOptions*)` + `vx_event_loop_pump_input(loop, view)` 手工泵；或 `vx_view_run()` 自动 wire SDL2 callback 并由 `Sdl2EventLoop::Run()` 驱动

## FetchContent 与代理（开发环境注意）

### http_proxy / HTTPS_PROXY 设置

首次 `cmake -B build` 触发 `FetchContent` 拉取 quickjs-ng（v0.14.0）等依赖。WSL2 / 内网环境若无系统 DNS，必须导出代理：

```bash
export http_proxy=http://172.22.32.1:7890   # WSL2 host gateway (Windows clash)
export https_proxy=$http_proxy
cmake -B build
```

> **代理地址来源（单一真相）：** `172.22.32.1:7890` 为 WSL2 → Windows host 网络网关（`172.22.x.1` 系 WSL2 NAT gateway 默认 IP），由 Windows 侧 Clash / V2Ray 等本地代理在 7890 端口暴露。任务收尾必须 `git config --global --unset http.proxy` + `--unset https.proxy` 恢复（参考 TASK-20260419-13 P1 守卫规则）。规则文件中**统一占位符** `<开发环境代理地址>`，本段为唯一可写入实际值的位置。

### 离线场景

若需完全离线构建，预先把 `_deps` 目录从可联网机器拷贝到 `build/_deps`，再运行 `cmake -B build`，FetchContent 会跳过下载。

### Benchmark 启用（TASK-20260419-02）

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DVX_BUILD_BENCHMARKS=ON
cmake --build build -j
./build/benchmarks/bench_allocators   # 同理 bench_containers / bench_hash_map / bench_strings
```

启用 `VX_BUILD_BENCHMARKS=ON` 时根 `CMakeLists.txt` 通过 `FetchContent` 拉取 `google/benchmark v1.9.1`。注意点：

- **代理**：与 quickjs-ng 同步，需 `http_proxy`/`https_proxy` 或 `git config --global http.proxy ...`
- **`-Werror -Wpedantic` 隔离**：`benchmarks/CMakeLists.txt` 通过把 `benchmark` 目标的 include 路径标记为 `INTERFACE_SYSTEM_INCLUDE_DIRECTORIES` 屏蔽第三方头文件 warning（`benchmark::benchmark` 是 ALIAS，需操作底层 `benchmark` target）
- **DEBUG 警告**：默认配置不指定 `CMAKE_BUILD_TYPE`，运行时 google/benchmark 会输出 `***WARNING*** Library was built as DEBUG`，数据失真——基线测量必须显式 `-DCMAKE_BUILD_TYPE=Release`
- **结果留存**（已修订，TASK-20260419-03）：
  - **Foundation 4 个 bench**（`bench_allocators` / `bench_containers` / `bench_hash_map` / `bench_strings`）：仍**不**入仓 baseline；本地按需用 `--benchmark_format=json --benchmark_out=...` + `tools/compare.py`
  - **CSS 3 个 bench**（`bench_css_tokenizer` / `bench_css_parser` / `bench_css_property_lookup`）：入仓 `benchmarks/baseline/*.json` 共 3 份（TASK-03 P6 入仓，~15 KB），仅作「形态参考」**不**作 CI 卡点。任何 baseline 更新必须：
    1. 由真实算法 / 数据结构 / hash 函数变更触发（不接受跨机数字漂移更新）
    2. 走 Release 独立 `build-bench/` + `--benchmark_min_time=0.5s`
    3. 同步刷新 `benchmarks/baseline/README.md` 「当前生成环境」表 4 行
    4. JSON 体检确认 `context.library_build_type == "release"`
  - 详细更新协议见 `benchmarks/baseline/README.md`

### 已知首次配置失败模式

| 现象 | 原因 | 解决 |
|------|------|------|
| `Could not resolve host: github.com` | 无代理；或父 shell `export http_proxy` 未传到 cmake 子进程（Cursor 沙箱会把 `HTTP_PROXY/HTTPS_PROXY` 强制覆盖为本地 SOCKS 跳板 `127.0.0.1:40601`，覆盖用户值） | **首选** `git config --global http.proxy ...` + `https.proxy`；任务收尾必须 `--unset` 恢复（参考 TASK-20260419-02 反思 #3） |
| `error: '#pragma' is not allowed here` (QuickJS) | 全局 `-Werror=pedantic` 污染 C 源 | 用 `add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:-Wpedantic>")` 限制为仅 C++ |
| `Unknown arguments specified` (FindPkgConfig) | 子目录重复 `pkg_check_modules` | 提供方一次声明 + `target_link_libraries(... PUBLIC ...)` 传播 |
| 第三方库 include 路径在测试目标找不到 | 通过 `INTERFACE` 链接但下游 target 没传播 | 改用 `PUBLIC` 链接（参考 TASK-20260418-01 P1#1）|
| `set_target_properties can not be used on an ALIAS target` | 第三方 target 用 `::` 形式（如 `benchmark::benchmark`、`Freetype::Freetype`、`GTest::gtest`）多为 ALIAS | 先 `get_target_property(_real <alias> ALIASED_TARGET)` 取底层名，对真实 target 操作（参考 TASK-20260419-02 反思 #2） |
| 第三方头文件触发本项目 `-Werror -Wpedantic` | 第三方头通过 `INTERFACE_INCLUDE_DIRECTORIES` 传播但未标 SYSTEM | `set_target_properties(<real-tgt> PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${INTERFACE_INCLUDE_DIRECTORIES}")` 一次性把头标为 SYSTEM，下游所有 target 自动屏蔽 warning（参考 TASK-20260419-02 P1） |
| google/benchmark 输出 `***WARNING*** Library was built as DEBUG` | `cmake -B build` 不指定 `CMAKE_BUILD_TYPE` 默认 Debug | 性能基准必须 `cmake -B build-bench -DCMAKE_BUILD_TYPE=Release -DVX_BUILD_BENCHMARKS=ON`，且独立 `build-bench/` 目录避免污染 Debug 测试 build（参考 TASK-20260419-02 反思 #1） |
| `rm -rf build-bench` 报 `Read-only file system` / `Device or resource busy` 错误 | Cursor 沙箱把 FetchContent clone 进来的 `build-bench/_deps/<lib>-src/.git` 目录设为只读 | `chmod -R u+w build-bench/_deps && rm -rf build-bench`（需要 `all` 权限）（参考 TASK-20260419-03 P6） |
| `cmake --build build-bench -j` 整体失败但单个 bench target 干净 | main 上某些 .cc / 测试代码隐藏了 Release `-Werror` 失败（仅 Debug 通过），fresh Release build 暴露 | (a) 临时绕行：`cmake --build build-bench --target <仅需要的目标> -j`；(b) 中长期：立 fix 任务（参考 TASK-20260419-03 P6 → TASK-20260419-07 候选）|

### Plan/VAN 阶段守卫（来源 TASK-20260419-13）

规则文件 `.cursor/rules/skills/writing-plans.mdc` 的「FetchContent 网络代理守卫」段要求每个 FetchContent 任务在 VAN / Plan Phase 0 检查并（必要时）补设 git 全局代理；`.cursor/commands/van.md` 步骤 1 已有对应自动检查子项。**本段是代理地址的单一真相来源**（实际值见上文 §FetchContent 与代理 / `172.22.32.1:7890`），规则文件中**严禁硬编码** IP 地址，统一用占位符 `<开发环境代理地址>`。

**TASK-20260426-01 PLAN 阶段实证：** 通过 `http_proxy=http://172.22.32.1:7890 curl ... raw.githubusercontent.com/web-platform-tests/wpt/...` 200 OK + GitHub API 列出 114 个 margin-collapse fixture，确认代理可拉取 W3C wpt 仓库。Build §0 阶段将拉取 8 例（4 margin-collapse + 2 IFC + 2 vertical-align baseline）入仓 `tests/fixtures/wpt/`。

## CSS 性能基线（来源 TASK-20260419-03，2026-04-19）

入仓 `benchmarks/baseline/bench_css_*.json`，**形态参考**而非 SLA。基础数量级（Release / WSL2 8 核 ~2.92 GHz / gcc 11.4 / `--benchmark_min_time=0.5s`）：

### Tokenizer（`bench_css_tokenizer`，10 BMs）

| 形态 | 吞吐 |
|------|------|
| `BM_TokenizeAll/Range(64,4096)` 7 case 全规模 | **297-340 MiB/s**（变异 ±7%，无 quadratic 退化） |
| `BM_TokenizeNumericHeavy` | **372 MiB/s**（数字解析多分支） |
| `BM_TokenizeStringHeavy` | **603 MiB/s**（字符 copy 状态保持） |
| `BM_TokenizeWhitespaceHeavy` | **614 MiB/s**（skip 路径短） |

> Tokenizer 摊还成本恒定；StringHeavy / WhitespaceHeavy 比 NumericHeavy 高约 1.6×，反映分支预测与码路径长度差异。

### Parser（`bench_css_parser`，11 BMs）

| 形态 | 吞吐 |
|------|------|
| `BM_ParseStylesheet` Small/Medium/Large/Wide | **102-136 MiB/s** |
| `BM_ParseDeclarationListInline/Range(1,32)` 6 case | inline 单 decl 解析 ~µs 级 |
| `BM_ParseSelectorListMixed` | compound + descendant combinator 形态 |

> Parser ≈ Tokenizer 1/3 吞吐，**AST 构造主导时间**（含 selector 解析 + decl 解析 + AST 节点分配）。

### PropertyLookup（`bench_css_property_lookup`，9 BMs）

| 场景 | ns | 相对 HitHot5 |
|------|----|----|
| HitAll（60 keys 轮转） | 14.9 | 1.43× |
| HitHot5（5 keys 轮转） | 10.4 | 1.00×（基准） |
| HitSingle/display | 7.97 | 0.77× |
| HitSingle/color | 7.26 | 0.70× |
| HitSingle/margin-top | 9.51 | 0.91× |
| HitSingle/border-radius | 13.5 | 1.30× |
| HitSingle/transition-timing-function | **28.6** | **2.75×**（最慢） |
| Miss | 10.1 | 0.97× |

**Cluster 判定（cluster 阈值 5×）：** 最慢 single key 仅 2.75× HitHot5，**远低于阈值** → PropertyMap (60-entry HashMap<StringView, PropertyId> + djb2 hash + H1=h>>7 probing) **未触发 cluster 问题**；~3.6× single key 跨度由 key 长度（djb2 O(len) + StringView 比较 O(len)）主导。

**对 TASK-20260419-06（HashMap Hash Mixing）影响：** 优先级 **P1→P3 降级**；触发条件改为「短字符串 ≠ 主用例 + 容器规模 > 1000 entry」的新场景出现时再立项。TASK-20260419-02 测得的 std::hash<int> identity-mapping cluster 问题对 **int key + n=16384** 真实存在（n=16384=9µs vs n=64=69ns），但**对短字串 + 60 entry 场景免疫**。

### HashMap 不是金科玉律：极小 N 下线性扫的 cache locality 仍胜（TASK-11 反思 #5）

**TASK-11 实证**：`ImageCache::Load` Hit<1>（cache 仅 1 entry）改造前后：
- 旧：`for (i: 0..size) if images_[i].path.view() == path` — N=1 时仅 1 次 view()==path 比较 + cache-line 命中 = **10.35 ns**
- 新：`String key(path); path_to_handle_.Find(key)` — 必须 djb2(O(strlen)) + H1 probing + Slot 间接 = **43.27 ns（4×↑）**

**结论**：HashMap 在 N=1 上的 ~32 ns 固有开销（djb2 hash + probe + Slot 间接）**永远大于**单元素线性扫的一次 memcmp。本任务因 N 分布偏向 16/256 端（生产场景：> 30 张图片的真实页面），Hit<256> 25.2× 净增益完全压倒 Hit<1> 32 ns 微回归 → 整体 ROI 极高。

**未来决策提示**：若引入新的 cache 场景且**N 永远 ≤ 4**（如「最近 N 调用 token cache」、「父链短路缓存」、「fallback registry」等），**不应套用同构 HashMap 方案**；考虑：
- (a) 保留 `Vector<Entry>` 线性扫
- (b) 用 fixed-size `array<pair, N>` + 分支预测友好的 unrolled 比较
- (c) 若 key 是数值（`u32` / `Handle`），直接 `array<V, N>` 下标查找

**判据**：N 中位数 ≤ 4 且 95th percentile ≤ 8 时，用线性扫；否则 HashMap 化。

### 用途

- 任何后续 CSS 模块优化（SIMD scan / SOA token / ParserPool / hash 函数替换）必须以这些数字为对照
- TASK-20260419-05（候选）Layout/Render bench 立项时，可用这些数据反推 CSS 解析占整体 layout pipeline 时间预算的 ~5-10%；若 layout 时间 << CSS 解析时间，需怀疑 layout 本身有问题

## Layout 性能基线（来源 TASK-20260419-05，2026-04-19）

入仓 `benchmarks/baseline/bench_layout_*.json`，**形态参考**而非 SLA。同环境与 CSS 基线（Release / WSL2 8 核 ~2.92 GHz / gcc 11.4），生成参数为 `--benchmark_repetitions=3 --benchmark_report_aggregates_only --benchmark_min_time=0.05s`（3 次 median 后稳态）。

### Buildtree（`bench_layout_buildtree`，3 BM × 14 行）

| 形态 | 时间 / 单位 |
|------|------------|
| `BM_LayoutBuildTreeFlat/8` | 589 ns（73 ns/box） |
| `BM_LayoutBuildTreeFlat/64` | 3.9 µs（61 ns/box） |
| `BM_LayoutBuildTreeFlat/128` | **7.7 µs**（60 ns/box）— 线性段终点 |
| `BM_LayoutBuildTreeFlat/256` | **70 µs**（274 ns/box）— ⚠️ super-linear knee |
| `BM_LayoutBuildTreeFlat/512` | 196 µs（383 ns/box）|
| `BM_LayoutBuildTreeNested/Range(2,64)` | 205 ns → 5.1 µs（每 depth ~80 ns 稳定） |
| `BM_LayoutBuildTreeMixed`（3-level × 4-fanout + text） | 6.2 µs（53 box，~117 ns/box）|

> **K2 发现：N=128→256 出现 super-linear knee（10× for 2× N）**；候选根因：(a) ArenaAllocator chunk grow（默认 4096 byte 边界）；(b) SmallVector<LayoutBox*, 16> 阈值降级；(c) cache miss。待 TASK-09 调查。
>
> **K2 大幅解决（TASK-20260424-01，2026-04-24）**：根因 **(d) ArenaAllocator 4KB block malloc/free churn**（非 (a) chunk grow、非 (b) Vector 扩容——`LayoutBox` 实为侵入式链表，见 `layout_box.h:26-29`）；Phase 2 block-size 扫描 {4K,8K,16K,32K,65K} 实证 32K 为 Flex 最优。`ArenaAllocator` 默认 block_size **4096 → 32768**。新基线（同机、同参数）：
>
> | 形态 | 修复前 | 修复后 | 改善 |
> |---|---:|---:|---|
> | `BM_LayoutBuildTreeFlat/128` | 7.7 µs | 10.1 µs | +31%（稍慢；大 block 首次 malloc 稍贵） |
> | `BM_LayoutBuildTreeFlat/256` | **70 µs** | **42.3 µs** | **1.66× 快** |
> | `BM_LayoutBuildTreeFlat/512` | 196 µs | 140 µs | 1.40× 快 |
> | **R256（knee 强度）** | **9.42×** | **4.18×** | **2.25× 降** |
>
> 剩余 super-linear（R256=4.18× 仍 > 理想 2×）由 **TASK-20260424-02** per-phase BM 调查承接（定位 BuildTree / LayoutBlock / ApplyPositioning 哪一阶段残留 (e) L1D 抖动 / (f) 隐藏算法因素）。

### Flex（`bench_layout_flex`，6 BMs，BENCHMARK_TEMPLATE<rows, cols>）

| 形态 | 时间 |
|------|------|
| `<1, 8>` | 762 ns（10.5 M cell/s） |
| `<1, 32>` | 2.2 µs |
| `<1, 128>` | 8.0 µs（线性段末） |
| `<8, 8>` | **4.9 µs**（64 cell，线性段终点） |
| `<16, 16>` | **73 µs**（256 cell，14.9× for 4× cells）— ⚠️ super-linear |
| `BM_LayoutFlexNested`（3-level × 4-fanout） | 5.4 µs |

> **K3 发现：8×8→16×16 同源 super-linear**，与 K2 形态一致 → 强烈暗示共享根因。
>
> **K3 大幅解决（TASK-20260424-01，2026-04-24）**：共享根因 (d) 同 K2。同一 fix（`ArenaAllocator` 默认 4096→32768）作用于 Flex 路径：`BM_LayoutFlex<16,16>` **82.5 µs → 44.2 µs**（1.87× 快）；R_flex（`<16,16>÷<8,8>`）**16.49× → 6.40×**（2.58× 降）。剩余超线性同 K2 交 TASK-24-02。另有 **K8 新发现**：Phase 2 扫描中 65K block 让 R_flex 回弹（7.40→8.36），暗示 block 过大触发 **L1D 抖动**（L1D 48 KB 边界）→ 32K 是 layout 路径的 sweet spot，未来若需更大 arena 应用显式 per-use-case block_size（如 `LayoutEngine::Layout` 静态 arena 已显式 8192）。

### 用途

- Layout 改动（margin collapsing / line box 模型 / inline-flow 重构）必须以这些数字为对照
- super-linear knee 的主要部分已由 TASK-20260424-01 解决（K2/K3 收敛 2.25-2.58×）；剩余 ~40% 由 TASK-20260424-02 per-phase 拆分 BM 承接
- 若引入新 layout 路径（grid / multi-column）需在该 .json 后追加新 bench 并入仓
- **ArenaAllocator 默认 block_size = 32768**（生产配置）；改动需：(1) 跑 layout bench 确认 R256 < 5 + R_flex < 8；(2) 同步刷新 baseline README「当前生成环境」的 block_size 行

## Render 性能基线（来源 TASK-20260419-05，2026-04-19）

入仓 `benchmarks/baseline/bench_render_*.json`，参数同 Layout 基线。

### Record（`bench_render_record`，5 BMs；layout 一次缓存，仅计 Record）

| 形态 | 时间 / 单位 |
|------|------------|
| `BM_RecordSmallTree`（8 div） | 274 ns（34 ns/box） |
| `BM_RecordMediumTree`（64 div） | 1.86 µs（29 ns/box） |
| `BM_RecordLargeTree`（512 div） | 16 µs（31 ns/box）— **线性 O(N)，64.7× for 64× N** |
| `BM_RecordTextHeavy`（32 div + text） | 2.45 µs |
| `BM_RecordImgVsNoImg`（16 img，cache=nullptr） | 544 ns（≈ Medium / 4，验证 K4） |

> **K4 发现：Record 对 image 元素无额外开销**（image_handle=0 时 RecordBox 直接跳过 kDrawImage emit）；Record 阶段瓶颈不在 image。

### Replay（`bench_render_replay`，5 BMs；layout+Record 缓存，仅计 Replay 到 SoftwareCanvas 256×256）

| 形态 | 时间 / 单位 |
|------|------------|
| `BM_ReplaySmallList`（~8 cmd） | 83 ns（96 M cmd/s） |
| `BM_ReplayMediumList`（~64 cmd） | 630 ns（102 M cmd/s）|
| `BM_ReplayLargeList`（~512 cmd） | 5.0 µs（102 M cmd/s）— **完美线性** |
| **`BM_ReplayTextHeavy`**（~96 cmd，含 DrawText） | **784 µs（122 k cmd/s）— ⚠️ DrawText 平均 8200 ns/cmd** |
| `BM_ReplayImgVsNoImg`（~16 cmd，cache=nullptr） | 166 ns（96 M cmd/s） |

> **K1 发现（最高价值）：Replay 真正 hot path = `DrawText`，不是 ImageCache**
> - DrawText 8200 ns/cmd vs FillRect 10 ns/cmd = **820×**，远超 5× 阈值（与 TASK-03 cluster 同源「带否定判据」方法）
> - 渲染优化方向应优先聚焦 SoftwareCanvas::DrawText 路径（候选：FreeType+HarfBuzz 集成 / glyph cache / SOA glyph layout）
> - FillRect 已接近物理极限（10 ns/cmd ≈ 2.5 cycles per pixel base + memset 摊还），优化 ROI 极低
>
> **K1 修正归因（来源 TASK-20260419-09 phase-3，2026-04-19）**：
> - TASK-05 BM_ReplayTextHeavy 测的是 **fallback 路径**（`SoftwareCanvas::DrawText` 在 `font_manager == nullptr || glyph_cache == nullptr` 时走 `DrawTextFallback` — 每字符 1 个 FillRect）
> - "8200 ns/cmd" ≈ `BM_DrawTextFallback_Medium` 3647 ns / 19 char × 含 PushClip + 多字符 painting 的合计；**"820×"实为「1 cmd 含 N 字符 painting」vs「1 cmd 单 FillRect」的 per-cmd 工作不可比性**，并非真路径慢源
> - **真正贵的是真路径冷路径**：`BM_DrawTextReal_Cold_Medium` 52763 ns / 19 char = 2777 ns/char，是 fallback 的 14×，是 warm 的 9.1× → FT_Load_Glyph + FT_Render_Glyph 是冷路径成本主体，glyph_cache 是 ROI 极高的存量优化

## Replay-Deepbench 性能基线（来源 TASK-20260419-09，2026-04-19）

入仓 `benchmarks/baseline/bench_drawtext.json` + `bench_imagecache.json`（Release，`build-bench/`，default `--benchmark_min_time` ~0.5s）。

### DrawText（`bench_drawtext`，8 BMs）

| 形态 | 时间 / 单位 |
|------|------------|
| `BM_DrawTextFallback_Short`（"hi"，2 char） | 460 ns（230 ns/char） |
| `BM_DrawTextFallback_Medium`（19 char） | 3647 ns（**192 ns/char** ≈ FillRect ×19） |
| `BM_DrawTextReal_Cold_Medium`（19 char） | **52763 ns（2777 ns/char）— FT_Load+FT_Render** |
| `BM_DrawTextReal_Warm_Medium`（19 char） | 5807 ns（305 ns/char） |
| `BM_DrawTextReal_Warm_Short`（2 char） | 968 ns（hb_shape 固定开销显化） |
| `BM_DrawTextReal_Warm_Long`（124 char） | 16852 ns（136 ns/char，最佳摊还） |
| `BM_ReplayTextHeavyFallback`（96 cmd） | 804 µs / 119k cmd/s |
| `BM_ReplayTextHeavyReal`（96 cmd） | 913 µs / 105k cmd/s（**仅 13% overhead vs fallback**） |

> **K7 发现（新，TASK-09 phase-3）**：当前 warm 真路径（5807 ns）> fallback（3647 ns），1.6×。`hb_shape` 固定开销 + glyph bitmap memcpy 当前 > 19 个 FillRect。如未来默认开真路径，需先做：(a) `hb_buffer` 复用避免每次 alloc / (b) glyph bitmap 直接 raster 到 canvas 避免中间 memcpy。

### ImageCache（`bench_imagecache`，7 BMs；libpng 程序化 fixture 写 `/tmp/vx_bench_<pid>_<i>.png`）

| 形态 | 时间 / 单位 |
|------|------------|
| `BM_ImageCacheLoad_Miss` | 3344 ns（含 1×1 RGBA decode + Vector push） |
| `BM_ImageCacheLoad_Hit<1>` | 43.27 ns（HashMap djb2 + probe 固有开销，绝对量微小）|
| `BM_ImageCacheLoad_Hit<16>` | 44.05 ns（O(1) 起效）|
| `BM_ImageCacheLoad_Hit<256>` | **45.70 ns（O(1) 平台化，原 1162 ns 25.2×↓）— K6 已解决** |
| `BM_ReplayImageReal<16>`（end-to-end） | 597.86 ns（37 ns/cmd） |
| `BM_ReplayImageReal<64>`（end-to-end） | 2398.12 ns（37 ns/cmd，完美线性） |
| `BM_ImageCacheGet` | 1.16 ns（O(1) 数组下标，未改）|

> **K6（TASK-20260419-11 已解决）**：`ImageCache::Load` hit 路径已升级为双索引 `Vector<Entry>`（保留 ABI / Get O(1)）+ `HashMap<String, gfx::ImageHandle, StringHash, StringEq>`（O(1) path lookup）。Hit<256> 1151.77 ns → 45.70 ns（**25.2×↓**），Hit<16> 50.87 ns → 44.05 ns（**1.16×↓**），anomaly「size=256 cache hit 慢于 ReplayImageReal<16>」消失。Hit<1> 10.35 ns → 43.27 ns 是 HashMap 固有 ~32 ns 开销（djb2 + probe），绝对量微小，被 256 路径净增益完全压倒。详见 `archive-TASK-20260419-11.md`。

### 用途

- DrawText 路径修改（FreeType / HarfBuzz / glyph cache 重构）以 K1 修正归因 + K7 为基线
- ImageCache 改造（HashMap 化）已由 TASK-20260419-11 完成：双索引 + custom StringHash/StringEq，未来可对照 `BM_ImageCacheGet` 1.16 ns 评估进一步优化空间
- 跨硬件比较见 `benchmarks/baseline/README.md` 失真警告

### 用途

- 任何 SoftwareCanvas 优化必须以 ReplayLargeList = 5 µs（FillRect 路径）+ ReplayTextHeavy = 784 µs（DrawText 路径）为对照
- 若新增 GPU 后端（OpenGL / Vulkan），可对比 Replay 时间证明加速比（预期 DrawText 加速比远高于 FillRect）
- DrawText 微基准（TASK-09 候选）应在该 baseline 之后再入仓

### 已知 bench 工程约束

- **Render bench 必须用 Styled corpus**（`CachedFlatStyledDocument` / `CachedTextHeavyStyledDocument` / `CachedImageStyledDocument`）— 默认 ComputedStyle background-color alpha=0 → `RecordBox` 不 emit FillRect → DisplayList 为空
- **Render bench 的 layout 阶段必须传 `ctx.stylesheets = &<empty Vector>`**（非 nullptr）— 否则 LayoutEngine 走默认 ComputedStyle 路径，inline `background-color` 不会被 StyleResolver 处理
- **ImageCache 真路径需 layout/Record/Replay 三阶段同传 cache 指针**（任一不传 → image_handle=0 → 链断）；当前 4 baseline 未走此路径，推 TASK-20260419-09 候选解决 fixture 工程

## Sciter 架构分析摘要

### 分层架构（参考）
```
┌─────────────────────────────────────┐
│          宿主应用 (C API)            │
├─────────────────────────────────────┤
│  脚本绑定 (xdomjs + QuickJS)         │
├─────────────────────────────────────┤
│  HTML 引擎 (DOM + CSS + Layout)      │
├─────────────────────────────────────┤
│  图形对象层 GOOL (抽象绘制接口)       │
├─────────────────────────────────────┤
│  平台后端 (D2D / GDI+ / Cairo / CGX) │
├─────────────────────────────────────┤
│  基础工具 (tool: 容器/字符串/IO)      │
└─────────────────────────────────────┘
```

### Sciter 关键设计点
1. **GOOL 抽象**：`gool::graphics` 纯虚接口，后端通过 `app_factory()` 选择
2. **DOM 类层次**：`node → element → block → block_vertical/horizontal/grid/stack`
3. **CSS 管线**：词法(`css_istream`) → 规则(`style_def`) → 匹配/层叠(`style_bag`) → 计算(`style::resolve`)
4. **布局引擎**：block/inline/flex(spring)/grid(kiwi约束)/table 多种流
5. **事件模型**：捕获(sinking) → 目标 → 冒泡，支持委托与选择器匹配
6. **视图更新**：`update_queue` 脏区管理，按 `STYLE_CHANGE_TYPE` 排队

### Veloxa 差异化方向
- 去除桌面重型依赖（D2D/GDI+），聚焦 OpenGL ES / Vulkan
- CSS 子集裁剪（保留 flex/grid，去除极少使用的属性）
- 内存池化与零拷贝设计
- 帧缓冲直出（适配 DRM/KMS）
- 车载特有：DPI 适配、多屏、触摸优先、旋钮/HUD 交互

## Foundation 实现经验（2026-04-05）

### StatusOr 接口表面（项目自实现，非 absl::StatusOr）
- 本项目 `vx::StatusOr<T>` 提供 `ok()` / `value()` / 错误状态构造
- **不提供** `error_message()` / `status()` / `code()` 等 absl 风格接口
- 测试中 `ASSERT_TRUE(r.ok()) << r.error_message();` 写法会编译失败
- 推荐写法：`ASSERT_TRUE(r.ok());` 或仅在 `ok()` 为 true 时取 `value()`
- 来源：TASK-20260419-01 测试编译失败教训

### 编译器约束
- `-Wpedantic` 禁止 C99 柔性数组成员（`char data[]`），需用偏移计算替代
- `-Wtype-limits` 与编译时常量比较冲突，需 `-Wno-type-limits` 豁免
- GCC 11 对 C++17 支持完整，`if constexpr`、结构化绑定均可用

### 依赖管理
- FetchContent 依赖网络可用性，离线环境应优先使用系统包
- 系统 GTest v1.11.0 功能足够（`EXPECT_DEATH`、`TEST_F` 均可用），但缺少 v1.14+ 的 `EXPECT_THAT`

## Graphics/Platform HAL 实现经验（2026-04-05）

### 像素格式
- RGBA32 格式统一为 R[0:7] | G[8:15] | B[16:23] | A[24:31]
- 跨模块操作（渲染 → 存储 → 导出）必须引用同一格式定义
- SavePPM 导出需按 R, G, B 顺序提取字节（bits 0-7, 8-15, 16-23）

### 子代理协作约束
- 涉及多模块数据交互的子代理 prompt 必须包含精确的数据格式规范
- 不同子代理的像素格式假设可能矛盾，集成测试是唯一防线

### 软件渲染器实现
- 覆盖率光栅化使用中点近似（非解析面积），正确但 AA 质量有限
- StrokePath 逐段独立渲染，无 join/cap 处理
- PushClipPath 使用 bounds 近似替代真正的路径裁剪
- `std::function<void()>` 用于 EventLoop::Task，嵌入式场景可能需要替换为轻量回调

## DOM/HTML Parser 实现经验（2026-04-05）

### DOM 架构
- 精简四类节点（Element/Text/Comment/Document），与 Layout 完全分离
- Document 使用 ArenaAllocator 分配节点（placement new），owned_nodes_ 追踪析构
- 子节点双向链表，属性 SmallVector<Attribute, 4>

### HTML 解析管线
- Tokenizer（混合状态机）→ Parser（DOM Builder）→ Document
- Tokenizer 零拷贝：Token 的 name/value 是 StringView 指向原始输入
- 实体解码延迟到 Parser 层（Token 的 has_entities 标志）
- 隐式关闭使用 20 条数据驱动规则表

## CSS Engine 实现经验（2026-04-05）

### CSS 解析管线
- CssTokenizer：22 种 token 类型，主分支 + 子扫描器模式，零拷贝 StringView
- CssParser：选择器解析（6 种简单选择器 + 2 种组合子）+ 声明解析 + 值类型分发
- 颜色解析：#hex（3/6/8 位）、rgb()/rgba()、18 种命名颜色（排序数组 + 二分查找）
- 简写展开：margin/padding（1-4 值）、border（width+style+color）

### 选择器匹配
- 选择器解析时从左到右，存储时反转为从右到左
- 匹配时从 compounds[0]（最右端，目标元素）开始，根据 combinator 向上遍历
- compounds[i].combinator 描述 compounds[i] 与 compounds[i-1] 之间的关系

### 层叠与继承
- 应用顺序：非 important 样式表 → 非 important 内联 → important 样式表 → important 内联
- 10 个可继承属性：color, visibility, font-family/size/weight/style, line-height, text-align, white-space, letter-spacing
- ComputedStyle ~200 字节，直存所有计算后的属性值

## Layout Engine 实现经验（2026-04-05）

### 布局架构
- LayoutBox 扁平 struct + LayoutType 枚举，arena 分配（ArenaAllocator）
- 独立布局树，通过 BuildTree 递归 DOM 树构建
- ComputedStyle 在 BuildTree 阶段通过 StyleResolver::Resolve 即时计算，结果 arena 分配存入 LayoutBox
- Block/Inline/Flex 三种布局模式 + relative/absolute 定位

### 布局算法覆盖
- **Block**：垂直堆叠、auto width/height、border-box、min/max 约束、auto margin 居中（无 margin collapsing）
- **Flex**：CSS Flexbox Level 1 §9——grow/shrink/basis、justify-content 5 种、align-items/self、gap、wrap、reverse
- **Inline**：简化文本行排列（水平堆叠+换行），无真正 line box 模型
- **Positioning**：relative 偏移、absolute 定位+递归布局

### 关键实现细节
- `LayoutEngine::Layout` 使用 `static ArenaAllocator`（线程不安全，需重构）
- BuildTree 过滤纯空白文本节点（缩进/换行），避免布局干扰
- LayoutChild 对 kText 节点调用 TextShaper::Measure 测量尺寸
- LayoutChild 分发：Block→LayoutBlock, Flex→LayoutFlex(自由函数), Inline→LayoutInline, Text→测量

## Render Pipeline 实现经验（2026-04-05）

### Display List 架构
- PaintCommand 6 种类型（FillRect/DrawText/PushClipRect/PopClip/PushLayer/PopLayer）
- Record 递归遍历 LayoutBox 树，生成有序 DisplayList
- Replay 遍历 DisplayList，调用 Canvas 方法
- Paint = Record + Replay 便捷入口

### 跨模块颜色格式
- CSS ComputedStyle: RRGGBBAA — `0xFF0000FF` = red opaque
- gfx::Color::ToRGBA32(): R[0:7]G[8:15]B[16:23]A[24:31] — `0xFF0000FF` = red opaque（巧合）
- **不可混用**：black 在 CSS 是 `0x000000FF`，在 gfx 是 `0xFF000000`
- CssColorToGfx() 转换函数位于 render_utils.h
- CSS 命名颜色 `green` = #008000 ≠ gfx::Color::Green() = #00FF00

### LayoutBox 坐标语义
- `x`, `y`：content area 原点（绝对坐标）
- padding box 原点：`(x - padding[left], y - padding[top])`
- border box 原点：`(x - padding[left] - border[left], y - padding[top] - border[top])`
- 此语义对渲染、hit-testing 等所有坐标计算至关重要

### Canvas::DrawText
- 纯虚接口，参数：text (StringView), bounds (Rect), font_size (f32), brush (Brush)
- SoftwareCanvas 实现：有 FontManager* 时使用 FreeType+HarfBuzz 做真实字形渲染（HarfBuzz 塑形 → FreeType 光栅化 → GlyphCache 缓存 → SrcOver 混合）
- 无 FontManager* 时退化为旧存根（逐字符 FillRect）

### Canvas::DrawImage（TASK-20260414-01 新增）
- 纯虚接口，参数：image (const Image&), src_rect (Rect), dst_rect (Rect)
- SoftwareCanvas 实现：最近邻缩放 + SrcOver alpha 混合，裁剪到 canvas 边界

### Image 解码管线（TASK-20260414-01 新增）
- ImageDecoder：DecodeFromFile/DecodeFromMemory，magic bytes 自动检测 PNG/JPEG
- PNG：libpng，png_set_expand + gray_to_rgb + add_alpha 确保 RGBA 输出
- JPEG：libjpeg-turbo，RGB→RGBA（alpha=255）
- ImageCache：路径去重缓存，handle-based 查找

### QuickJS DOM 绑定（TASK-20260414-01 新增）
- DomBindings::Bind(ctx, doc, em) 注册 document.getElementById + Element 类 + Style proxy
- Element 属性：tagName（只读）、id（只读）、textContent（读写）、style（getter 返回 proxy）
- Element 方法：getAttribute、setAttribute、addEventListener、removeEventListener
- Style proxy：7 个 CSS 属性的 camelCase setter，通过 CssParser::ParseDeclarationList 解析
- addEventListener 通过 JS_DupValue 保持 callback 引用，TrackedCallbacks 在 Unbind 时释放

## Event System 实现经验（2026-04-05）

### 事件系统架构
- 两层事件模型：InputEvent（平台层）+ DOMEvent（DOM 分发上下文）
- W3C DOM Events 三阶段分发：Capture → Target → Bubble
- 中央 EventManager 管理 hover/active/focus 状态（3 指针，不侵入 Element）
- HitTest 使用 LayoutBox 树后序遍历，共享渲染管线的 z-index 排序逻辑

### CSS 伪类连接
- SelectorMatcher::Matches 添加可选 `const EventManager* em = nullptr` 参数
- :hover → em->IsHovered(el)（祖先链遍历）
- :active → em->IsActive(el)（祖先链遍历）
- :focus → em->IsFocused(el)（精确匹配）
- em == nullptr 时保持原有行为（返回 false）

### 集成测试约束（重要）
- **LayoutEngine::BuildTree 不解析 HTML inline style**
- BuildTree 调用 StyleResolver::Resolve 时 inline_decls 参数默认为 nullptr
- 所有集成测试必须使用外部 CSS 选择器（`#id { ... }`），不可使用 `style="..."` 属性
- 这是 API 能力假设错误第三次出现（TASK-02 像素格式、TASK-07 border box 坐标、TASK-08 inline style）

### CSS 伪类透传链完整路径（TASK-09 修复）
- 完整链路：LayoutContext.event_manager → BuildTree → StyleResolver::Resolve(em) → CollectMatchingRules(em) → SelectorMatcher::Matches(sel, el, em)
- TASK-08 只修改了 SelectorMatcher，未更新上游 StyleResolver 调用链，导致 restyle 阶段伪类永远不匹配
- 教训：跨模块参数透传修改必须端到端验证整条调用链

## Update Manager 实现经验（2026-04-05）

### 更新管线架构
- UpdateManager 编排 Invalidate → Restyle → Relayout → DirtyRect → Repaint
- 全量 restyle + relayout（HMI 树小，< 1ms），仅重绘阶段做脏区优化
- 拥有 ArenaAllocator，每帧 Reset 释放旧 LayoutBox 树
- LayoutEngine::Layout 新增 ArenaAllocator& 重载，旧签名（static arena）仍保留

### 脏区计算
- DisplayList 逐项对比（PaintCommand::operator==），差异项 rect 并集 = 脏区
- 列表长度不同时回退全屏重绘
- Canvas::PushClipRect 裁剪 + Clear + Replay 实现脏区重绘

### SoftwareCanvas 构造 API
- SoftwareCanvas 接受 `(u32* pixels, u32 width, u32 height, u32 stride)`
- 不接受 Surface* 指针，需手动 Lock/Unlock Surface 获取 pixels
- 写测试时应参考现有测试的构造方式，避免 API 假设错误

## DomBindings pimpl 与 QuickJS 集成经验（TASK-20260418-01）

### pimpl 化 header 零 quickjs 依赖
- `dom_bindings.h` 仅前向声明 `struct JSContext;`，所有 `JSClassID`/`JSValue`/`JS_*` 都进 `.cc`
- **不完整类型坑**：`std::unique_ptr<InstanceData>` 在 header 中只有前向声明时，`~DomBindings()` 必须在 `.cc` 定义（non-default），否则 delete 时类型不完整
- 匿名 namespace 中的 C 风格回调无法访问 `private` 嵌套类型——把 `struct InstanceData;` 前向声明放 `public:`，定义和 `data_` 保持 `private`；用 `friend struct DomBindingsInternal;` 提供 `.cc` 内桥接
- `JS_SetContextOpaque(ctx, this)` + `JS_GetContextOpaque(ctx)` 是 ctx→C++ 实例的规范通道

### JSClassID 是进程级常量，不是实例状态
- `JSClassID` 由 `JS_NewClassID(rt, &id)` 全局分配，即使多个 `JSRuntime` 也共享；必须当作 file-scope static 处理
- 幂等注册模板（量产代码必用）：
  ```cpp
  if (s_class_id == 0) JS_NewClassID(rt, &s_class_id);
  if (!JS_IsRegisteredClass(rt, s_class_id)) JS_NewClass(rt, s_class_id, &def);
  ```
- 陷阱：把 `JSClassID` 作为实例成员每次 `Bind` 重新 `JS_NewClassID`，会在同 runtime 上创建重复 class 导致 `JS_NewObjectClass` 失败
- quickjs-ng v0.14.0 已支持 `JS_IsRegisteredClass`（`build/_deps/quickjsng-src/quickjs.h` 可核实）

### JS 回调生命周期 UAF 防护
- `addEventListener` 注册 C++ lambda 捕获了 `JSValue callback` —— lambda 与 `JSValue` **必须同寿命**
- `Unbind` 顺序硬约束：先 `EventManager::RemoveEventListeners(el)`（销毁 lambda 拷贝）**再** `JS_FreeValue(callback)`
- 维护 `InstanceData::listener_elements: Vector<Element*>` 去重记录已绑定元素，`Unbind` 遍历清理；`removeEventListener` 也必须同步移除
- 剩余风险：`DomBindings` 析构必须先于 `EventManager`；相反则 `RemoveEventListeners` 作用于悬垂 em 指针。当前由宿主手动保证，弱引用/观察者模式留待后续

## HarfBuzz + FreeType 生命周期与 CMake 传播（TASK-20260418-01）

### hb_font_t 缓存模式
- `hb_ft_font_create_referenced(FT_Face)` 是重操作（解析 charmap/metric），per-DrawText 调用是显著性能债
- 缓存粒度：每 `FontHandle` 一个 `hb_font_t`，跟 `FT_Face` 同寿命
- `FT_Face` 尺寸变化时 HarfBuzz 需显式通知：**先** `FT_Set_Pixel_Sizes(face, 0, new_size)`（调用方契约）**再** `hb_ft_font_changed(hb_font)`；否则 `hb_shape` 会用旧 metrics
- 释放顺序：`Shutdown` 中先 `hb_font_destroy(hb)` 再 `FT_Done_Face(face)` —— 某些 HarfBuzz 版本在 face 已释放后析构 hb_font 是 UB

### CMake 静态库依赖传播
- 核心原则：静态库 `A` 的 `.a` 内包含 `A.o` 但**不**捆绑其 `PRIVATE` 依赖 `B.a`；链接 `A` 的目标 `T` 不会自动得到 `B` 的头路径和符号
- 当 `T` 的源文件**直接** `#include <b_header.h>` 或**间接**依赖通过 `A` 内联/模板暴露的 `B` 符号时，`B` 必须在 `A` 中 `PUBLIC` 链接
- 本次反面教训：先尝试在 `tests/CMakeLists.txt` 添加 `pkg_check_modules(HARFBUZZ …)` 通过 `font_manager_test` 补链接，触发 `FindPkgConfig.cmake` "Unknown arguments specified"（父 scope 已调用导致参数状态冲突）
- 正确解法：在 `veloxa/text/CMakeLists.txt` 把 `Freetype::Freetype`、`${HARFBUZZ_LIBRARIES}` 从 `PRIVATE` 改 `PUBLIC`，同时 `${HARFBUZZ_INCLUDE_DIRS}` 也改 `PUBLIC`；所有下游（含 tests）自动继承
- 决策准则：**公开 header 中前向声明的类型**（`struct hb_font_t;`）即使在 .h 里不 include，只要 **测试或下游需要定义并调用这些类型的 API**，依赖就必须 `PUBLIC`

### 技术债务清单
1. ~~Benchmark 延期（需 google benchmark）~~ ✅ 已接入 v1.9.1（TASK-20260419-02），覆盖 Foundation 4 子模块共 40 个 BM 用例
2. HashMap SIMD Group 探测未实现（当前标量线性探测）
3. InternedString 全局表非线程安全
4. BasicString 含 Alloc* 指针，sizeof 为 32 而非纯 24
5. Status::message 使用 std::string，与自有 String 存在循环依赖
6. Rasterizer 覆盖率算法待升级为解析面积计算（AA 质量）
7. PushClipPath 仅用 bounds 近似，待实现真正路径裁剪
8. StrokePath 无 join/cap 处理（当前 butt 端帽、无连接）
9. ~~PPM 测试使用硬编码 /tmp 路径~~ ✅ 已修复（TASK-05，PID 隔离）
10. CMake: vx_graphics 链接 vx_platform 可能引入不必要耦合
11. ~~TagIdFromName/PropertyIdFromName 均为 O(N) 线性扫描~~ ✅ 已优化为 HashMap O(1)（TASK-05）
12. ~~Document 节点管理用 Vector<Node*> + delete~~ ✅ 已集成 ArenaAllocator（TASK-05）
13. ~~Parser 静默忽略 kError token~~ ✅ 已支持错误收集（TASK-05）
14. Serializer 不做空白规范化
15. parser.cc 过大（1035 行），考虑拆分为 parser_selector.cc + parser_value.cc
16. ApplyDeclaration switch 规模大（~55 case），可用宏或代码生成简化
17. 选择器匹配 O(rules × elements) 全量遍历，大页面需哈希索引优化
18. ~~:hover/:active/:focus 伪类当前返回 false（stub）~~ ✅ 已回填（TASK-08 SelectorMatcher + TASK-09 StyleResolver 透传）
19. LayoutEngine::Layout 的 static ArenaAllocator 线程不安全（旧签名仍存在，新签名接受外部 arena）
20. ~~Block 布局缺少 margin collapsing（CSS 规范要求，影响渲染正确性）~~ — ✅ **TASK-20260426-01 R3 + TASK-20260430-01 已闭环**（W3C CSS 2.1 §8.3.1 全部 5 类 adjoining 规则完整实施）：
    - **R3 (TASK-26-01)：** sibling collapse / collapse-through / negative max(pos)+min(neg) / BFC root 阻断（仅 overflow trigger）；新文件 `margin_collapse.h` POD `MarginChain` header-only + `Add/Collapsed/IsEmpty/ApplyClearance/Trace`；`layout_engine.cc::LayoutBlock` 重写 + helpers；`layout_box.h` 增 `collapsed_through` + `margin_top_collapsed_into_ancestor`；first/last child 与 parent collapse 受 D1.2 LayoutChild API 边界约束当时 stub-w/-rationale 留 P3 TASK-26-02；ctest +26 = 1010/1010；同窗口对照 mean +3.2% / median +3.4%
    - **TASK-26-02 升级（TASK-20260430-01，2026-04-30）：** first/last child 与 parent 的 margin-top/margin-bottom collapse + 完整阻断条件（padding-top / border-top / overflow != visible BFC root / height != auto/none / min-height > 0）。新增 `LayoutBlockChild(box, w, ctx, MarginChain* incoming) → MarginChain outgoing` 专用辅助（D1.A1 决策；D2 实施时调整为 in-out by-pointer 以支持多级跨函数 propagate 而非 plan 阶段 by-value）；`LayoutBlock` 改为 wrapper 调 `LayoutBlockChild` 并物理化 root chain；隐式 BFC root 识别（document root + body/html 顶层 wrapper：`box->parent->parent == nullptr`）；`MarginChain::MergeFrom` 用于 collapse-through 跨层累积；`layout_box.h` 增 `margin_bottom_collapsed_into_ancestor` 状态字段。ctest +10 = 1039/1039 PASS（Debug + Release `-O3 -Werror`）；wpt-005 `Wpt005_NonSiblingAdjoiningMarginsCollapse` SKIP→PASS（直接验证目标）；同窗口 stash-swap bench median ~+5%（A6/A7 ≤ +10% 全 PASS，§7.0.1 规则首次外部任务实战）；性能优化：O(N) end-of-loop scan → O(1) `last_in_flow_block` running pointer hoisting；W3C `clearance` 仍为 stub（`ApplyClearance` 仅置标志位），完整版需 float/clear CSS 属性，依赖独立 Level 4 任务（VAN F2 实证 CSS layer 零实现）；CSS parser 不处理 `border-bottom` shorthand 是新发现技术债（独立 P3 候选）
21. ~~LayoutInline 是简化实现，缺少真正的 line box 模型（ascent+descent 计算）~~ — ✅ **TASK-20260426-01 R4 已修复**（CSS 2.1 §10.8 / §10.8.1 完整实施；新文件 `veloxa/core/layout/line_box.h` POD `LineBox`+`LineBoxItem` Vector；`enums.h::VerticalAlign` 9 关键字（含 `kLength` sentinel）+ `computed_style.h` `vertical_align`+`vertical_align_offset` 双字段联动 + CSS 全链路 6 文件（property/parser/style_resolver/enum_serialization）+ `TextShaper` ABI 兼容拆 ascent/descent + `[[deprecated]] baseline` + FT 路径取真实 face metrics；`LayoutEngine::LayoutInline` 严格 2-pass vertical-align 算法 + 半-leading + 隐式 strut + LineBox Vector + fit-content width / explicit height 修正 + inline-block atomic 路径；ctest +19 = 1029/1029 PASS（11 line_box + 5 text_shaper + 3 parser）+ 2 wpt linebox PASS（Wpt006 inline-formatting / Wpt007 va: 0% ≡ baseline）；同窗口对照 bench Flat/64 mean -3.6% / median +2.65% 远低 ±10%；不在边界：bidi LTR 假设 / inline-block 内部 IFC 当 atomic / block 含 inline 匿名 IFC 留 P3）
22. Arena 分配的 ComputedStyle 含 InternedString 成员，arena 释放不调用析构函数，可能泄露引用计数
23. SoftwareCanvas::DrawText 是存根（逐字符 FillRect），需集成 FreeType+HarfBuzz
24. border-radius 渲染未实现（ComputedStyle 有值但 renderer 忽略）
25. ~~LayoutBox 缺少 border_box_origin()/padding_box_origin() 辅助方法，坐标计算分散在多处~~ — ✅ **TASK-20260426-01 R1 已修复**（`layout_box.h` 增 `Point` POD + `border_box_origin()`/`padding_box_origin()`/`content_box_origin()` 三族 + 12 个 *_top/right/bottom/left 边缘 helpers，全部 constexpr inline；`layout_engine.cc` 5 处 + `flex_layout.cc` 12 处 重复表达式替换；ctest 不变 PASS；bench 0 退化）
26. DisplayList 无 Dump() 调试方法
27. vx_core 新增 vx_graphics 依赖，所有 core 代码（包括不需要 graphics 的 HTML/CSS/Layout）都链接了 vx_graphics
28. ~~LayoutEngine::BuildTree 不解析 inline style（inline_decls 始终为 nullptr）~~ — ✅ **TASK-20260426-01 R2 已修复**（`html/parser.cc` 增 `ApplyInlineStyleAttribute` 私有方法，调 `CssParser::ParseDeclarationList` + 完整三件套安全护栏 [安全相关]：T1 count cap 1000 / T2 value len cap 8 KB / T3-T5 黑名单 expression(/behavior:/javascript: 大小写不敏感）；常量 `kInlineStyleMaxDeclarationCount` + `kInlineStyleMaxValueLength` 暴露 namespace 可测；`internal::ContainsBlacklistKeyword(StringView) → bool` 提取为 unit test 入口；ctest +24 cases（14 inline_style + 1 const guard + 8 ContainsBlacklist + 1 layout e2e）= 984/984 PASS；与 JS dom_bindings.cc:322 路径行为完全等价（A2 e2e 验证 `<div style='padding:10px'>` ≡ stylesheet）
29. ~~EventManager 无「状态变更→样式失效→重绘」触发机制~~ ✅ 已实现（TASK-09 UpdateManager + InvalidationCallback）
30. EventDispatcher::listeners_ 元素销毁时需手动 RemoveEventListeners（无自动清理）
31. HitTest z-index 排序每次调用重新排序（可缓存）
32. EventDispatcher 使用 std::function 作为 handler（嵌入式场景可能需轻量替代）
33. UpdateManager::Update 中 canvas 操作（PushClipRect+Clear+Replay+PopClip）可提取为 RepaintDirtyRegion 辅助方法
34. ComputeDirtyRect 仅支持 PaintCommand 逐项对比，不处理命令重排序（假设绘制顺序不变）
35. UpdateManager 缺少 OnBeforeUpdate/OnAfterUpdate 钩子（宿主应用无法监听帧生命周期）
36. Application 缺少 Resize 支持（需重建 Canvas + 更新 LayoutContext viewport）
37. Application 缺少 HTML/CSS 解析错误回调
38. Application::LoadHTML 使用裸 delete 管理 Document，可改用 unique_ptr
39. ~~像素测试缺少标准化工具库（PixelR/G/B/A 提取函数），同类代码分散在多个测试文件中~~ ✅ 已提供 `tests/test_pixel_utils.h`（TASK-20260413-02，部分测试已迁移）
40. C API 缺少 vx_view_resize、vx_view_get_document、错误回调等扩展 API
41. ~~HashMap 缺少 const_iterator / cbegin / cend，const 方法无法遍历~~ ✅ 已实现（TASK-20260413-02）；`TransitionManager::HasActive` 已改为扫描
42. transition shorthand 仅支持单条声明，不支持逗号分隔多条（如 `transition: bg 300ms, opacity 200ms`）
43. LayoutBox.style 是 const 指针，动画覆盖需 const_cast，应考虑引入可写样式覆盖层或改为 non-const
44. `vx_script`：`JS_SetInterruptHandler` / 执行预算未实现（见 `creative-quickjs-host.md` Phase 2）；`JSMallocFunctions` 与 Foundation 分配器未对齐
45. ~~dom_bindings.cc 使用全局变量（g_bound_doc、g_element_class_id、g_tracked_callbacks）~~ ✅ 已迁移到 `DomBindings::InstanceData`（pimpl，`JS_SetContextOpaque` 桥接）；JSClassID 保留为文件级 `s_` 静态 + `JS_IsRegisteredClass` 幂等注册（TASK-20260418-01）
46. ~~StyleGetProp getter 始终返回空字符串~~ ✅ 已实现 length/color/auto/number/inherit/initial 序列化，遍历 `inline_declarations()`（TASK-20260418-01）；Enum（display 等）读路径仍返回 `""`，留作后续
47. removeEventListener 简化为移除元素所有监听器，未实现按 type+handler 精确移除
48. ~~SoftwareCanvas::DrawText 每次调用创建 hb_font_t~~ ✅ 已缓存到 `FontManager::FontEntry`，`GetHbFont(handle, pixel_size)` 按需创建 + `hb_ft_font_changed` 响应 size 变化（TASK-20260418-01）
49. textContent setter 不处理无 Text 子节点的情况（不创建新 Text 节点，静默返回）
50. ~~addEventListener lambda 捕获 JSContext*~~ ✅ `DomBindings::Unbind` 先 `em->RemoveEventListeners(el)` 再 `FreeAll callbacks`，消除 UAF（TASK-20260418-01）；遗留约束：宿主必须确保 `DomBindings` 先于 `EventManager` 析构

### TASK-20260430-03 全代码库 Code Review 新增条目（2026-04-30）

**详细清单见 `docs/reports/2026-04-30-codebase-review.md`**（55 项 findings 跨 6 维度，28 P1 + 19 P2 + 8 P3）。

**R2 已修复 6 项**（commit 链 `3b4b2e7` / `1467207` / `ddea78d` / `95ae814` / `9c6ad5f` / `668a9fe`，ctest 1062/1062 PASS）：
- ✅ F-020 SelectorMatcher 末尾 dead `return false` 替换为 `VX_DCHECK(false); return false;` + 解释性注释
- ✅ F-026 LayoutEngine 一参重载 arena `static` → `thread_local`（多线程并发 Layout 不再撞 arena）
- ✅ F-033 ProcessEndTag 移除 isize/usize 重复转换 + early-return 防 underflow
- ✅ F-040 Rasterizer FlattenQuad/Cubic 0.0625（quad 平方）vs 0.25（cubic 距离）阈值数学等价注释
- ✅ F-053 image_decoder.cc DecodeFromFile 加 `usize max_size = 256 MiB` 默认参数 + 守卫 + 单测（防 Vector OOM abort）
- ✅ F-055 vx_version() 改 CMake `configure_file` 从 `project(VERSION X.Y.Z)` 生成 `VELOXA_VERSION_STRING`（消除硬编码漂移）

**R3+ 推荐拆分 13 项独立任务**（按 ROI 排序，完整列表见 R1 报告 §8 / reflection §6.1）：
- 🔴 #1 image_decoder 安全三件套（F-049 PNG alpha 丢失 / F-050 width×height 溢出 / F-051 JPEG error_exit 杀进程）— P1 安全 / 4-6 h / Level 3
- 🟡 #2 EventDispatcher snapshot 防 listener mutation UAF（F-046）— P1 正确 / 2-3 h / Level 2
- 🟡 #3 LoadHTML 重置 dom_bindings_ 防 UAF（F-025）— P1 正确 / 1-2 h / Level 2
- 🔴 #4 CSS 属性元数据表（F-022）— P1 维护 / 1-2 周 / Level 4 大件 / 解 6 处 shotgun surgery
- #5-13（DOM lifecycle / 现代选择器 / Layout 边角 / HTML5 实体表 / Rasterizer 性能 / 12 模块测试 / libpng 升级 / 8 项 P3 候选 等）

**新增 systemPattern**（来自 reflection §5）：
- Background agent 双轨模式 + worktree 隔离协议 — 见 `systemPatterns.md` §「TASK-30-03 已验证模式」
- plan ×0.6 估时按任务类型分桶系数矩阵 — 同上
- Quick fix 颗粒度估时基准 12 min/项（含上下文 + 验证）— 同上
- Checkpoint 推荐默认 + 隐式批准协议 — 同上
- Review 类任务 6 维度 × 抽样深度矩阵 spec 模板 — 同上

### CMake 配置矩阵（basic vs full）

ctest 数量随 CMake 选项变化（worktree 重 configure 时需注意基线匹配）：

| 配置 | `VX_BUILD_BENCHMARKS` | `VX_PLATFORM_SDL2` | ctest 总数 | 备注 |
|---|---|---|---|---|
| basic（默认）| OFF | OFF | **1031** | 不含 SDL2 backend / benchmark smoke |
| full（开发者基线）| ON | ON | **1062** | TASK-30-03 R0/R2 基线（1062 含 R2.5 新单测；之前 1061）|

**用法**：
- 主 worktree 默认 full（已有 CMakeCache）
- 新建 worktree 第一次 cmake configure 需显式 `-DVX_BUILD_BENCHMARKS=ON -DVX_PLATFORM_SDL2=ON`
- 否则 ctest 1031 会被误判为「30 测试丢失 / 回归」

### Background Agent 双轨模式 worktree 隔离工程指引（来自 TASK-30-03 R2 实战）

**触发场景**：用户在主 worktree 切到任务 B（如 04 DevTool 规划），agent 需在任务 A（如 03 codebase review）feature 分支推进。

**避坑步骤**：
```bash
# 1. agent 启动时立即建立独立 worktree（沙箱限制 workspace 内路径）
git worktree add .worktree-<task_short>/ feature/TASK-<id>-<slug>

# 2. CMake 配置同步（默认 OFF → 显式开启）
cd .worktree-<task_short>
cmake -S . -B build -DVX_BUILD_BENCHMARKS=ON -DVX_PLATFORM_SDL2=ON

# 3. 每 commit 前断言守门
[ "$(git symbolic-ref --short HEAD)" = "feature/TASK-<id>-<slug>" ] || exit 1

# 4. 完成后清理
cd ..
rm -rf .worktree-<task_short>/build  # 先清 cmake FetchContent generated
git worktree remove --force .worktree-<task_short>/
```

**工程债注解**：cmake FetchContent 拉的 `google/benchmark` `_deps/benchmark-src/.git/hooks/*.sample` 是只读，WSL2 NTFS 锁可能导致 `git worktree remove` 报 `Device or resource busy` — 需先手动 rm `<wt>/build`。

## TASK-20260430-04 蓝图主交付摘要（DevTool 三件套）

### 任务概述

**TASK-20260430-04** — 规划 UI 编辑器与调试器（DevTool 三件套蓝图设计）[安全相关]
**复杂度：** Level 4（V2=a 纯蓝图主交付：spec + plan + creative ×2，build 由用户独立立项）
**完成日期：** 2026-05-01

### 4 篇产出文档（蓝图基线）

| 类型 | 路径 | 行数 | 核心内容 |
|---|---|---:|---|
| spec | `docs/specs/2026-04-30-devtool-design.md` | ~600 | 12 段式样：目的 / 不做 / 14 验收 / D1-D8 决策矩阵 / I1-I8 注入点 / 数据流 / Phase 划分 / T1-T8 威胁建模 / 测试策略 / R1-R6 风险 / 30+ systemPatterns 自我对照 / 与未来任务关系 |
| plan | `docs/plans/2026-04-30-devtool.md` | ~700 | Phase 0 全局约束 + CMake 链接审计 + 静态库循环审计 + 边界输入清单 16 项 + 测试隐式契约 + CSS shorthand 能力 grep 表 / Phase A/B/C/D 子任务 ~40 项 + plan ×0.6 估时矩阵 |
| creative #1 | `memory-bank/creative/creative-devtool-screen-layout.md` | ~280 | 5 决策：splitter dock + HUD overlay 双层结构 / dock-to-right vs dock-to-bottom 切换协议 / HUD 透明合成 / overlay 渲染顺序双线宽 / F12-F11 toggle |
| creative #2 | `memory-bank/creative/creative-devtool-hot-reload.md` | ~300 | 5 决策：FileWatcher 跨平台抽象 / Linux inotify CSS-only 增量 / DOM 状态保留协议 / watcher root 边界 + T2 8 步守卫 / CSS 解析失败错误恢复 |

### D1-D8 决策矩阵（详见 spec §4）

| # | 维度 | **锁定值** |
|:-:|---|---|
| D1 | 三件套实施优先级 | **B** Inspector → Overlay → Hot Reload |
| D2 | Inspector 数据采集协议 | **B** 半结构化（JSON tree + DisplayList overlay + C API JSON）|
| D3 | DevTool UI 主屏布局 | **B** 同窗口 splitter dock + Overlay HUD 子模式 |
| D4 | DevTool 隔离边界 | **B** 单进程共享容器（双 Document + 共享 EventLoop / Application / ImageCache）|
| D5 | Hot Reload file watcher + 增量策略 | **A** 嵌入式专注（Linux inotify + CSS-only 增量重载）|
| D6 | Performance Overlay 数据采集点 | **B** Chrome DevTools 风格（五钩子 + 滑动 60 帧 + dirty rect 边框高亮）|
| D7 | C API 扩展边界 | **C** 双层 API（内部 C++ 核心 + 公开 C API 薄封装）|
| D8 | 安全威胁建模 | **A** T2/T3/T5/T6/T7/T8 完整 + T1/T4 扩展段占位 |

### 7 项独立立项候选（用户后续基于 plan 拆出，不在 04 任务范围内）

**主线 3 项：**
- **TASK-30-04-A**：DevTool Phase A — Inspector 实施（Level 3，~12.25 h plan / ~7.35 h plan ×0.6）
- **TASK-30-04-B**：DevTool Phase B — Performance Overlay 实施（Level 2-3，~7.25 h / ~4.35 h）
- **TASK-30-04-C**：DevTool Phase C — Hot Reload 实施（Linux only，Level 3，~10 h / ~6 h）

**扩展段 4 项**（spec §11 占位，按用户优先级排期）：
- TASK-30-04-D Console JS REPL（威胁 T1 任意 eval mitigation）
- TASK-30-04-E JS Debugger backend（QuickJS Debug API + 威胁 T6 callback budget）
- TASK-30-04-F CDP 远程调试 port（威胁 T4 HMAC token + nonce + loopback only + default off）
- TASK-30-04-G 完整 UI 编辑器（dogfood 完整闭环，长期愿景）

### 触及技术债 4 项闭环 ROI 路径

| # | 技术债 | DevTool 子系统 | 闭环节奏 |
|:-:|---|---|---|
| #26 | LayoutBox.Dump 调试方法缺失 | Inspector Layout 面板 | TASK-30-04-A Phase A.0.2 实施 |
| #35 | UpdateManager 未暴露 frame hook | Performance Overlay 五钩子 | TASK-30-04-B Phase B.0.1 实施 |
| #40 | C API 缺 DOM/Style/Layout introspection | Inspector 全子系统 | TASK-30-04-A Phase A.0.5/A.0.6 实施 |
| #4 | ImageCache 命名空间隔离 | DevTool icon 防污染 | TASK-30-04-A Phase A.0.1 配套 |

### 强依赖关系（立项前置守卫）

- **TASK-30-04-A 必须先于 TASK-30-04-B**：B 依赖 A 的 Inspector 渲染管线 + UI 框架基础
- **TASK-30-04-A 第一步 = I1 Application 双 Document 槽改造**（重命名 `document_` → `target_document_` 让 callsite 漏改在编译期暴露 — R1 风险 mitigation）
- **TASK-30-04-C Hot Reload Phase C.2 增量重载 = CSS-only**（不触发 LoadHTML）→ **不踩 R3+ #3 F-025 LoadHTML use-after-free**
- **如未来扩展 HTML 增量重载** → 必须先做 R3+ #3 修复（强依赖，spec §12.3 + R3+ #3 已交叉记录）
- **TASK-30-04-C 可与 A 并行**（无强依赖）；扩展段 4 项独立可立（无相互强依赖）

### V2=a 蓝图任务工作流变体

本任务首次实践 Level 4 V2=a 工作流变体 `/van → /plan → /reflect → /archive`（跳过 `/build` / `/creative` 内联到 `/plan`）。详细沉淀见 `systemPatterns.md` § Level 4 蓝图任务 V2=a 工作流变体 + `.cursor/rules/main.mdc` § Level 4 蓝图任务 V2=a 工作流变体。

### plan ×0.6 第 17 数据点

主线（VAN + Plan）实测 ~74 min vs plan 估时 210-270 min → **0.27-0.35× plan / 0.46-0.59× plan ×0.6**（落「极窄档 + review 类下限交界」）。详见 `systemPatterns.md` § plan ×0.6 任务类型分桶系数矩阵 § 蓝图任务 V2=a + 多决策连续跳过 + 批量文档段。

---

## TASK-20260502-01 DevTool Phase A · Inspector 实施摘要

### 任务概述

DevTool Phase A · Inspector 主线落地 — TASK-30-04 蓝图主交付的第一项 (subtask_a) 实施版，Level 4 实施类大件任务，16 子任务跨 4 Phase 8 轮 build 完成。

### 主要交付

- **公开 C API**（`veloxa/api/veloxa_api.h`）新增：
  - `vx_view_attach_devtool / detach_devtool / devtool_loaded`（DevTool lifecycle）
  - `vx_view_serialize_dom_json`（T7 双调用 + max_size 守卫）
  - `vx_inspector_set_redaction_policy`（T3 mitigation policy switch）
  - `VxRedactionPolicy { REDACT_SENSITIVE, NONE }` enum
  - `VxDevtoolOptions { devtool_width, enable_f12_hotkey }` struct
  - `VX_KEY_F12 = 0x40000045u` 常量
- **内部 C++ API**（`veloxa/devtool/inspector/inspector_data.h`）— `SerializeDocument / SerializeLayoutBox / SerializeComputedStyle`（D7=C 第一层零拷贝）
- **JS native binding** — `vx_devtool_get_dom_json()` 全局函数（DomBindings::SetDevtoolTargetDocument 跨 Document inspection）
- **编译期嵌入资源** — `veloxa/devtool/resources/inspector_panel.{html,css,js}` 通过 Python codegen + CMake `add_custom_command` 嵌入 binary（B-A1.1=b T2 路径穿越威胁面消除）
- **Hello example** — `examples/hello_devtool.cc`（SDL2 真窗口 + F12 toggle + T3 redaction 运行时验证）

### 技术债闭环

- **#26 LayoutBox.Dump 缺失** ✅ 闭环（A.0.2 LayoutBox::ToJson() + 4 单测）
- **#40 C API 缺 DOM/Style/Layout introspection** ✅ 闭环（A.0.5 + A.0.6 双层 API + by-node API 待 P3 node_id 系统建立后补）

### 安全威胁 mitigation

- **T2 路径穿越** ✅ 完全消除（编译期嵌入路径推翻 B4=B 改 B-A1.1=b）
- **T3 Inspector 敏感数据 redact** ✅ 完整 mitigation（C-API + JS binding 路径统一安全策略）
- **T5 DisplayList overlay 隔离** ✅ 完整 mitigation（kOverlayHighlight + ResetOverlayCommands + 多帧隔离测）
- **T7 C API buffer overflow** ✅ 完整 mitigation（双调用模式 + max_size + 4 边界测）
- **A14 链接闭包零** ✅ 自动化守门（A.2.4 ctest smoke 每次 ctest 跑 nm 验证 8 符号黑名单零命中）

### R2 缺陷暴露 → P3 候选 3 项

- **DomBindings.Element.children** collection getter 缺失（HTMLCollection 风格）
- **DomBindings.element.addEventListener** 缺失（已有 EventManager API，需 JS binding 暴露）
- **DomBindings.element.innerHTML setter** 缺失（HTML parser 重新解析子树）

### plan ×0.6 第 18-37 数据点入库

20 个新数据点群组，~281 min 主线 vs 441 min plan ×0.6 = **0.64×（落「大件实现」桶下沿外，触发桶系数下调 0.8-1.2× → 0.6-1.1×）**。详见 `systemPatterns.md` § plan ×0.6 任务类型分桶系数矩阵实证段。

### 4 大跨阶段教训沉淀

1. plan-fact reconcile 是 Level 4 大件任务的常态（11 处修正）
2. 多层 A14 zero-byte guard 范式锁定（5 子任务复用）
3. C ABI stub 公开表面 vs DevTool 闭包精确区分（3 处出现）
4. dogfood = R2 缺陷暴露清单产出（A.1.8 集中产出 3 P3 候选）

### 新模式入库

- `systemPatterns.md` § plan escalation 中途触发（首次完整实证 — Phase A.1 escalation 后 0.99×）
- `systemPatterns.md` § 反向探针有效性陷阱清单（4 类陷阱）
- `systemPatterns.md` § 子系统关闭守门 ctest smoke 范式（pure CMake script + nm 自动化）

---

## Status / StatusOr 使用规范（TASK-20260502-01 A.1.8 教训沉淀）

### 背景

`StatusOr<T>` 在 `has_value=true` 时调 `.status()` 会 `VX_DCHECK(!has_value_)` abort（设计上 status() 仅在错误状态时返回 Status，OK 状态由 `T` 持有）。直接写 `Status s = result.status()` 而 result 已成功会触发 abort。

### 规范

```cpp
// ✅ 正确：三元守卫
StatusOr<MyType> result = ComputeSomething();
Status s = result.ok() ? Status::Ok() : result.status();

// ❌ 错误：has_value=true 时 status() abort
StatusOr<MyType> result = ComputeSomething();
Status s = result.status();  // DCHECK abort if result.ok()

// ✅ 正确：直接消费 value 或处理错误
StatusOr<MyType> result = ComputeSomething();
if (!result.ok()) {
  return result.status();  // 仅在错误时调 .status()
}
auto value = std::move(result).value();
```

### 实证

- TASK-20260502-01 A.1.8 GREEN 期间踩坑：`devtool_script_status_ = eval.status()` 当 `eval.ok() == true` 时 abort
- 修正：`devtool_script_status_ = eval.ok() ? Status::Ok() : eval.status()`

### TASK-20260503-02 StatusOr<T>::status() codebase audit 结果（2026-05-03）

**Audit 命令**（Grep tool 替代 `rg`，POSIX `grep` 兜底亦可）：

```bash
# 等价 rg "\.status\(\)" 双重 sweep（veloxa/ + tests/ + examples/ + benchmarks/）
grep -rn "\.status()" veloxa/ tests/ examples/ benchmarks/ \
  --include="*.cc" --include="*.h"
```

**结果**（14/14 ✅ 全部正确，零误用）：

| 范围 | 命中 | 评估 |
|---|:-:|:-:|
| `veloxa/` | 6 处（5 文件） | 6/6 ✅ |
| `tests/` | 8 处（5 文件） | 8/8 ✅ |
| `examples/` | 0 处 | — |
| `benchmarks/` | 0 处 | — |

**veloxa/ 详表**：

| # | 文件:行 | 守卫模式 | 评估 |
|:-:|---|---|:-:|
| 1 | `veloxa/core/application.cc:113` | `if (!result.ok()) return result.status();` | ✅ |
| 2 | `veloxa/core/application.cc:135` | 同上 | ✅ |
| 3 | `veloxa/core/application.cc:348` | `eval.ok() ? Status::Ok() : eval.status();` | ✅ 三元守卫范本 |
| 4 | `veloxa/core/image/image_cache.cc:16` | `if (!result.ok()) { return result.status(); }` | ✅ 多行守卫 |
| 5 | `veloxa/devtool/hot_reload/file_watcher_inotify.cc:239` | `if (!resolved.ok()) { ... .status().code() ... }` | ✅ 守卫块内合法 |
| 6 | `veloxa/devtool/hot_reload/file_watcher_inotify.cc:242` | 同 #5 块内 `.status().message()` | ✅ |

**tests/ 详表**：

| # | 文件:行 | 守卫模式 | 评估 |
|:-:|---|---|:-:|
| 7 | `tests/devtool/hot_reload/file_watcher_test.cc:314` | `ASSERT_TRUE(resolved.ok()) << ... resolved.status().message();` | ✅ GoogleTest 短路评估（cond=true 时 stream 不 evaluate） |
| 8 | `tests/devtool/hot_reload/file_watcher_test.cc:350` | `EXPECT_EQ(resolved.status().code(), kInvalidArgument);` | ✅ 测试上下文已知 error |
| 9 | `tests/script/quickjs_engine_test.cc:18` | `ASSERT_TRUE(r.ok()) << (!r.ok() ? r.status().message() : "");` | ✅ 显式三元守卫（冗余但 100% 安全） |
| 10 | `tests/script/quickjs_engine_test.cc:27/35/46` | `EXPECT_FALSE/NE/EQ(r.status().*)` | ✅ 测试期望 error |
| 11 | `tests/integration/devtool_dogfood_smoke_test.cc:106` | `ASSERT_TRUE(json.ok()) << ... json.status().message().data();` | ✅ GoogleTest 短路评估 |
| 12 | `tests/foundation/base/status_test.cc:47` | `EXPECT_EQ(result.status().code(), kNotFound);` | ✅ 测试期望 error |
| 13 | `tests/core/image/image_decoder_test.cc:75` | `EXPECT_EQ(result.status().code(), kOutOfMemory);` | ✅ 测试期望 error |

**结论**：

- 0 fix 必要 — A.1.8 修复后的三元守卫范本（#3）+ 既有 `if (!ok())` 前置守卫（#1/#2/#4）+ 守卫块内合法引用（#5/#6）+ tests/ GoogleTest 短路评估模式（#7/#11）+ tests/ 期望 error 模式（#8-#10/#12/#13）已覆盖 codebase 全部 `.status()` 用例
- 无新引入误用风险点
- **P3 候选保留**：clang-tidy custom check enforce（编译时强制守卫规范）— 估时 ~1-2 h，待 codebase 整体 clang-tidy 化任务时合并执行（与 TASK-30-03-style review round 类型任务合流）
- **P3 候选 (新)**：`ASSERT_TRUE(x.ok()) << x.status().message()` 模式依赖 GoogleTest 短路评估，是易错模式（独立语句 `auto err = x.status();` 在 OK 时 abort）— 可考虑 codebase guideline「测试中也用三元守卫显式化」（如 #9 范本）

**audit 范式可复用**：本任务首次实证「plan Phase 0 §0.2 阶段预跑 audit」模式 — audit 在 plan 阶段完成而非 build 阶段，避免「audit 发现需 fix → plan 不准 → 重新 plan」的低效循环。Build 阶段 CP2 自审进一步扩展到 tests/ + examples/ + benchmarks/ 完整范围，零新增 fix 候选。

---

## 安全测设计 — 边界场景显式语义状态优于数值阈值（TASK-20260502-02 B.1.2 教训）

### 背景

T6 callback budget guard 测设计原意：`budget_us_ = 0` 表示「禁用 budget」（任何 callback 时长都触发 abort），但原实施 `if (frame_total_us_ > budget_us_)` 在某些机器上不触发 — 因 sub-µs 硬件计时器精度下，capture-less / nullptr-body callback 的实际耗时 < 1 µs → cast 后归零 → `0 > 0` = false。

### 规范

```cpp
// ❌ 错误：纯数值阈值依赖（受运行环境精度影响）
if (frame_total_us_ > budget_us_) {
  AbortBudget();
}

// ✅ 正确：显式语义状态短路 + 数值阈值（双重保护）
if (budget_us_ == 0 || frame_total_us_ > budget_us_) {
  AbortBudget();  // budget=0 表示「禁用」语义，任何 callback 都 abort
}
```

### 启动条件

T6 / 性能 / 边界场景测时，如设计意图含「特殊值 = 特殊语义」（e.g. 0 = disabled / -1 = unlimited / max = no-op），必须显式短路语义状态，不能依赖数值比较结果。

### 反模式

❌ **「特殊值 = 边界数值，依赖比较结果」** — 浮点 / 时间 / 转换精度受硬件影响，可能漏报
❌ **「假设 cast 结果与原值同符号同零性」** — `static_cast<u64>(0.5e-6)` = 0，与原值 0.5e-6 不同语义

### 实证

- TASK-20260502-02 B.1.2 BudgetGuardZeroAlwaysAborts 测在某些机器上 FAIL（false negative）
- 修正：`budget_us_ == 0 || frame_total_us_ > budget_us_` 显式短路 → ON+OFF 双绿

### 后续 Action（P1）

- 全 codebase grep `StatusOr.*\.status\(\)` audit 现有用法
- 考虑加 clang-tidy custom check 强制规范

---

## TASK-20260503-01 DevTool Phase C · Hot Reload 实施摘要

**完成时间**：2026-05-03 16:28（build commit `6247626`）/ 16:35（reflect 文档落盘）

**核心交付**：
- 新子系统 `vx::devtool::hot_reload::{FileWatcher, InotifyFileWatcher, HotReloadManager}` — Linux inotify 自实现 + CSS-only 增量重载（严格不踩 F-025 LoadHTML use-after-free）
- 2 公开 C API 新增：`vx_view_attach_devtool` 加 `hot_reload_dir` 参数 + `vx_view_hot_reload_tracked_count`
- 1 ABI 扩展：`VxDevtoolOptions.hot_reload_dir` 字段 + `VxResult` 新增 `VX_WARNING_HOT_RELOAD_FAILED = 1`（warning 语义层）
- 1 example 第三次扩展：hello_devtool.cc + hello_devtool_hot_reload_smoke ctest 端到端 0.87s 验证
- 1 DevTool UI 状态指示器：inspector_panel.html/css/js + `vx_devtool_get_hot_reload_status` JS native binding
- T2 路径穿越 8 步守卫 dual-probe 16 测全覆盖（forward × 8 + reverse × 8）— absolute root / locked inotify mask / realpath canonicalization / canonical path boundary / max_file_size / .css extension / WARN logging / 50ms debounce
- A14 链接闭包黑名单 +3 项（FileWatcher / InotifyFileWatcher / HotReloadManager）

**关键技术决策（5 项实测驱动）**：
1. 错误码沿用 `VX_ERROR_INVALID_STATE` 而非 plan 字面 `VX_ERROR_UNSUPPORTED`（codebase 一致性，与 A.1.7 / B.0.1 一致）
2. CSS 解析失败检测改用 brace imbalance 启发式（实测发现 CssParser 过宽容 — 缺 `}` 仍能解析出 0 declarations 的 rule）
3. application.h `unique_ptr<HotReloadManager>` 字段 + getter + ~Application reset 三处 #ifdef 包围（OFF 路径 incomplete type 修复）
4. C.4.1 引入 vx_core PRIVATE link vx_devtool 形成第二循环依赖叠加 — binutils 2.46+ hotfix `--start-group/--end-group` 包围方案无需任何额外 CMake 改动即解决（实证 hotfix 设计的「叠加场景覆盖性」）
5. C.4.2 dogfood smoke 需新增 `vx_view_hot_reload_tracked_count` testability 接口（plan 阶段未识别 — P1 反馈到 writing-plans skill）

**实测耗时与比值**：
- 11 子任务 + CP1/CP2/CP3 + 双绿 verify finalize = ~104 min build
- plan ×0.6 333 min → 比值 **0.31×** — 触发「极窄档延续高效区下沿挤压」新数据点群组（候选新子档「极窄档加速衰减区 0.20-0.30×」）
- 三任务连续递降趋势：A 0.64× → B 0.40× → **C 0.31×**

**Phase 0 投入定律 triple-evidence 升级**：
- A 5.3× / B 5.2× / **C 7.6× ROI**（30 min Phase 0 投入 → 节省 ~229 min build phase）
- ROI 公式入库：`ROI = (plan ×0.6 总分钟数 - 实测 build 分钟数) / Phase 0 投入分钟数`

**额外事件（已 fast-forward main 零业务范围污染）**：
- build §0.1 baseline 二次验证遭遇 binutils 2.46+ ld 单次扫描静态归档严格化阻塞 → `hotfix/binutils-2.46-link-group` 单独分支修复 → 根 `CMakeLists.txt` +10 行 `-Wl,--start-group <LINK_LIBRARIES> -Wl,--end-group`（仅 GNU/Clang + Linux 生效）→ 271/271 link OK + 1195/1195 ctest PASS → fast-forward to main `ddc1e3c` → feature 分支 rebase 上 main
- 「baseline 阻塞 hotfix 分离协议」首次实证 + R12「工具链版本激进升级」风险登记（已沉淀到 systemPatterns.md）

**ctest 双绿 verify 终局（三路径）**：
- DEVTOOL=ON：1247 PASS（baseline 1231 + 16 SecurityT2）
- DEVTOOL=OFF：1082 PASS + A14 link-closure 零 DevTool 符号 ✅
- SDL2=ON：1265 PASS（含 hello_devtool_hot_reload_smoke 0.87s 端到端 ✅ 验证 `HOT RELOAD: triggered count=1`）

**反复模式：1/7 部分命中**（前置依赖/环境/API 能力未验证 — CssParser 严格性假设 + 工具链 binutils 2.46+ 行为变化 2 项）— 比 Phase A/B 0/7 小幅回升；P1 #2 + P1 #4 改进建议已迁移 activeContext 待处理事项段闭环。

**DevTool 三件套主线收官** — Inspector + Performance Overlay + Hot Reload 完整闭环 ✅；后续候选见 activeContext.md 待处理事项 §「Phase C 完成后 P3 候选」段。

