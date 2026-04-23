# Layout super-linear knee 根因调查 — 实现计划

**目标：** 定位并修复 `BM_LayoutBuildTreeFlat/256` 相对 `/128` 的 9.67× super-linear knee，让修复后 ≤ 2.5×；产出一次性可追溯的研究报告+补丁。

**架构：** 阶梯验证 — Phase 1 探针假设 `(d) Arena 4KB block malloc/free churn`；命中则 Phase 2 扫描最优值；Phase 3 全局 bump `ArenaAllocator::block_size` 默认（4096 → 最优值，预期 16384）；Phase 4 baseline 刷新；Phase 5 文档收尾。若 Phase 1 否定 (d) → 升级 Phase 1B per-phase 拆分 BM 定位。

**技术栈：** C++17 / CMake / google/benchmark v1.9.1（Release build-bench/）/ GoogleTest / Python3（json 兜底 jq MISS）

**复杂度级别：** Level 2-3（研究 + 可能的小补丁 / Phase 数 6，含 Phase 1B 分支）

---

## 计划前置分析（按 `writing-plans.mdc` 必检项）

### CMake 链接方向约束分析 — ❌ 不适用

本任务仅改头文件里的一个默认值，不涉及 CMake/link 变更。`vx_add_benchmark` / `vx_core` / `GTest::gtest` 等依赖图保持不变。

### 静态库循环依赖审计 — ❌ 不适用

无新符号导出；`ArenaAllocator` 定义保留在 `veloxa/foundation/memory/arena_allocator.h`，使用方 grep 已穷举（4 处），编译拓扑不变。

### Web 标准 API 多重载形态清单 — ❌ 不适用

无 Web 标准 API。

### FetchContent C 子项目编译选项审计 — ❌ 不适用

无新 `FetchContent_Declare`；`build-bench/_deps/` 已缓存 google/benchmark + quickjs-ng 源码。

### FetchContent 网络代理守卫 — ❌ 不触发

- grep 确认 `CMakeLists.txt` 含 `FetchContent_Declare`（根 + `veloxa/script/CMakeLists.txt`）
- 但 `build-bench/_deps/benchmark-src` + `quickjsng-src` 已完整缓存
- 「跳过条件」命中：已离线预置 `_deps/` → 不需要 `git config --global http.proxy` 检查
- 记录到 progress.md Phase 0 以便 `/reflect` 阶段回溯

### 测试基础设施审计 — ✅ 通过

- 被测内部状态：`ArenaAllocator::bytes_allocated()` **已是公开 const getter**（`arena_allocator.h:79`）；`block_size` 非公开但**可通过行为间接观测**（单次超过 default 的 alloc 会触发 oversized path）
- 新测试 `DefaultBlockSizeLocked` 通过 `bytes_allocated()` + 多次 `Allocate` 观察 block 边界位置间接验证默认
- 无需新增 friend / getter

### 边界输入清单 — ✅ 覆盖

`ArenaAllocator` 现有测试已覆盖：
| 类别 | 用例 | 状态 |
|---|---|:-:|
| 默认路径 | `BasicAllocation` / `MultipleAllocationsInSameBlock` | ✅ |
| 超长 | `OversizedAllocation`（alloc > block） | ✅ |
| 边界值（0 字节） | `VX_DCHECK(size > 0)` 兜底 | 契约内 |
| 重置 | `Reset` | ✅ |
| 对齐 | `Alignment` | ✅ |
| **新增：默认 block 大小契约** | `DefaultBlockSizeLocked`（本任务） | ⏳ |

### 调用链端到端验证 — ❌ 不适用

`ArenaAllocator` 签名不变，行为契约保持；仅默认参数值变化，调用方无需感知。

### 管线注入点代码级可行性验证 — ❌ 不适用

无管线注入。

### 性能基准任务必检项 — ✅ 全部通过（已在 VAN + Phase 0 设计中落实）

**§1 Release + 独立 build 目录：** `build-bench/ -DCMAKE_BUILD_TYPE=Release -DVX_BUILD_BENCHMARKS=ON` 已就绪（TASK-02 建立）

**§2 CMake helper / 链接可见性：** 本任务不新增 bench exe，`bench_layout_buildtree` / `bench_layout_flex` 已注册于 `benchmarks/CMakeLists.txt`（TASK-05）；无链接可见性变更

**§3 目标 API 的发射/触发条件 grep：** 本任务测的是 `LayoutEngine::Layout` + `ArenaAllocator::Allocate`，均为无 gate 路径；已 grep 确认 `LayoutBox` 侵入式链表 + `StyleResolver` 在 bench 默认路径未触发（`ctx.stylesheets == nullptr` 的 buildtree bench）；**关键 API 触发条件全部已 VAN 实证**

**§4 Bench Smoke 验收三件套：**
- 数字非零 ✓
- `SetItemsProcessed(state.iterations() * n)` ✓（bench_layout_buildtree.cc:43 已写）
- `items_per_second > 0` ✓（现有 json 已有字段）

**§4.5 smoke 工具链可用性：**
```
jq:      MISS → 兜底 python3 -c 'import json'
rg:      MISS → 兜底 Grep 工具（Cursor 内置 ripgrep）
perf:    MISS → 兜底 /usr/bin/time -v（Phase 1B 才用）
python3: OK
bc:      OK
awk:     OK
```
Plan 内所有 json 解析脚本**必须**用 `python3 -c 'import json'` heredoc；**严禁** Build 阶段临时换栈。

**§5 RangeMultiplier 公式：** 本任务不新增 Range 型 BM；现有 `Range(8,512)` 公式已验证 7 case。

**§6 渲染/绘制 bench 前置清单：** 不适用（本任务测 layout，非 render）。

---

## Phase 0：基线核验 + 分支 + smoke 工具

**文件：**
- 读取：`benchmarks/baseline/bench_layout_buildtree.json`（已在仓）
- 创建：`feature/TASK-20260424-01-layout-knee-root-cause` 分支

### 任务 0.1：工作区 + 分支 [运维]

- [ ] **步骤 1：确认 main 干净**
  运行：`git status`
  预期：`nothing to commit, working tree clean`

- [ ] **步骤 2：确认基线 commit**
  运行：`git log --oneline -1`
  预期：`e3952dc chore(workflow): complete TASK-20260419-13 and reset to idle`

- [ ] **步骤 3：创建功能分支**
  运行：`git checkout -b feature/TASK-20260424-01-layout-knee-root-cause`

### 任务 0.2：smoke 工具矩阵快查 + 基线复跑 [运维]

- [ ] **步骤 1：记录工具矩阵**
  运行：`for t in jq python3 bc awk perf; do command -v $t >/dev/null && echo "$t: OK" || echo "$t: MISS"; done`
  预期：`jq: MISS, perf: MISS, 其余 OK`；输出追加到 `memory-bank/progress.md`

- [ ] **步骤 2：确认 build-bench 可用**
  运行：`ls build-bench/benchmarks/bench_layout_buildtree build-bench/benchmarks/bench_layout_flex`
  预期：两个 binary 存在

- [ ] **步骤 3：跑基线采 R256 起始值**
  运行：
  ```bash
  ./build-bench/benchmarks/bench_layout_buildtree \
    --benchmark_filter=Flat \
    --benchmark_min_time=0.5s \
    --benchmark_format=json \
    --benchmark_out=/tmp/vx_knee_baseline.json
  ```
  预期：exit 0；JSON 文件生成

- [ ] **步骤 4：计算 R256（Python 兜底）**
  运行：
  ```bash
  python3 - <<'EOF'
  import json
  d = json.load(open('/tmp/vx_knee_baseline.json'))
  times = {b['name']: b['real_time'] for b in d['benchmarks']}
  r256 = times['BM_LayoutBuildTreeFlat/256'] / times['BM_LayoutBuildTreeFlat/128']
  r512 = times['BM_LayoutBuildTreeFlat/512'] / times['BM_LayoutBuildTreeFlat/256']
  print(f"R256 = {r256:.2f}x  (knee; baseline expected ~9.67x)")
  print(f"R512 = {r512:.2f}x  (post-knee; baseline expected ~2.72x)")
  EOF
  ```
  预期：`R256 ≈ 9.0-10.5x`（确认 knee 可复现）

- [ ] **步骤 5：commit VAN/Phase 0 产出**
  运行：
  ```bash
  git add memory-bank/ docs/specs/2026-04-24-layout-knee-root-cause-design.md docs/plans/2026-04-24-layout-knee-root-cause.md
  git commit -m "docs(van+plan): TASK-20260424-01 layout knee root-cause design + plan"
  ```

---

## Phase 1：假设 (d) 单点探针 — malloc churn

**文件：**
- 修改（**临时**，Phase 2 末尾会恢复）：`veloxa/foundation/memory/arena_allocator.h`

### 任务 1.1：单点 65536 实验 [实验]

- [ ] **步骤 1：临时改默认 block size**
  文件：`veloxa/foundation/memory/arena_allocator.h`
  行 13：
  ```cpp
  -  explicit ArenaAllocator(usize block_size = 4096)
  +  explicit ArenaAllocator(usize block_size = 65536)  // EXPERIMENT: Phase 1
  ```

- [ ] **步骤 2：重 build bench**
  运行：
  ```bash
  cmake --build build-bench --target bench_layout_buildtree bench_layout_flex -j
  ```
  预期：0 errors / 0 warnings

- [ ] **步骤 3：采样 R256**
  运行：
  ```bash
  ./build-bench/benchmarks/bench_layout_buildtree \
    --benchmark_filter=Flat \
    --benchmark_min_time=0.5s \
    --benchmark_format=json \
    --benchmark_out=/tmp/vx_knee_exp65536.json
  python3 - <<'EOF'
  import json
  d = json.load(open('/tmp/vx_knee_exp65536.json'))
  t = {b['name']: b['real_time'] for b in d['benchmarks']}
  r256 = t['BM_LayoutBuildTreeFlat/256'] / t['BM_LayoutBuildTreeFlat/128']
  print(f"R256 (block=65536) = {r256:.2f}x")
  print(f"N=128:{t['BM_LayoutBuildTreeFlat/128']:.0f} ns")
  print(f"N=256:{t['BM_LayoutBuildTreeFlat/256']:.0f} ns")
  EOF
  ```

- [ ] **步骤 4：判据判定**
  | R256 范围 | 结论 | 下一步 |
  |---|---|---|
  | ≤ 2.5× | 根因 (d) 强确认 | 进 Phase 2 扫描最优值 |
  | 2.5× - 6× | (d) 部分 | 进 Phase 2，若 16384/32768 达标用较小值 |
  | ≥ 6× | (d) 否定 | **revert** 此实验 → 启动 Phase 1B |

- [ ] **步骤 5：判据记录到 progress.md**
  把步骤 3 输出追加到 `memory-bank/progress.md` Phase 1 小节。

- [ ] **步骤 6：保持此实验状态进 Phase 2**（不 commit，不 revert）
  实验代码保留到 Phase 3 定版最优值后再统一替换。

---

## Phase 2：block size 扫描（最优值定位）

**前提：** Phase 1 判据命中 R256 < 6×。

**文件：**
- 反复修改（实验态）：`veloxa/foundation/memory/arena_allocator.h`（Phase 3 末尾定版）

### 任务 2.1：5 档扫描 [实验]

扫描值：**{4096, 8192, 16384, 32768, 65536}**（N=4096 与 baseline 一致，不重跑）

对每个档位重复：改默认值 → 重 build → 跑 bench_layout_buildtree + bench_layout_flex。

- [ ] **步骤 1：4096 / 65536 已采（Phase 0 + Phase 1），只需补 8192 / 16384 / 32768**
  脚本化单 for 循环：
  ```bash
  for SIZE in 8192 16384 32768; do
    # 改默认值
    sed -i "s/block_size = [0-9]\+)/block_size = ${SIZE})/" \
      veloxa/foundation/memory/arena_allocator.h

    # 重 build
    cmake --build build-bench --target bench_layout_buildtree bench_layout_flex -j 2>&1 | tail -3

    # 采样
    ./build-bench/benchmarks/bench_layout_buildtree \
      --benchmark_filter=Flat \
      --benchmark_min_time=0.5s \
      --benchmark_format=json \
      --benchmark_out=/tmp/vx_knee_exp${SIZE}.json

    ./build-bench/benchmarks/bench_layout_flex \
      --benchmark_filter="16, 16|8, 8" \
      --benchmark_min_time=0.5s \
      --benchmark_format=json \
      --benchmark_out=/tmp/vx_knee_flex_exp${SIZE}.json
  done
  ```
  （`sed` pattern 对当前 4096 / 65536 / 其它都能匹配；执行后 grep 验证）

- [ ] **步骤 2：聚合输出扫描表**
  运行：
  ```bash
  python3 - <<'EOF'
  import json
  sizes = [4096, 8192, 16384, 32768, 65536]
  print(f"{'block':>8} | {'N=128':>8} | {'N=256':>8} | {'R256':>6} | {'Flex16':>8} | {'R_flex':>6}")
  print("-" * 62)
  for s in sizes:
    bt_path = f"/tmp/vx_knee_{'baseline' if s == 4096 else f'exp{s}'}.json"
    flex_path = f"/tmp/vx_knee_flex_{'baseline' if s == 4096 else f'exp{s}'}.json"
    try:
      bt = json.load(open(bt_path))
      flex = json.load(open(flex_path)) if s != 4096 else None
      t = {b['name']: b['real_time'] for b in bt['benchmarks']}
      r256 = t['BM_LayoutBuildTreeFlat/256'] / t['BM_LayoutBuildTreeFlat/128']
      if flex:
        ft = {b['name']: b['real_time'] for b in flex['benchmarks']}
        r_flex = ft.get('BM_LayoutFlex<16, 16>',0) / ft.get('BM_LayoutFlex<8, 8>',1)
        print(f"{s:>8} | {t['BM_LayoutBuildTreeFlat/128']:>8.0f} | {t['BM_LayoutBuildTreeFlat/256']:>8.0f} | {r256:>5.2f}x | {ft.get('BM_LayoutFlex<16, 16>',0):>8.0f} | {r_flex:>5.2f}x")
      else:
        print(f"{s:>8} | {t['BM_LayoutBuildTreeFlat/128']:>8.0f} | {t['BM_LayoutBuildTreeFlat/256']:>8.0f} | {r256:>5.2f}x | {'--':>8} | {'--':>6}")
    except FileNotFoundError:
      print(f"{s:>8} | MISSING")
  EOF
  ```
  （注：Phase 0 的 baseline 没跑 flex；在步骤 1 的 for 之前补跑一次 flex baseline `block=4096`，命名为 `/tmp/vx_knee_flex_baseline.json`）

- [ ] **步骤 3：挑选最优值**
  **主判据：** 最小 SIZE 使 R256 ≤ 2.5
  **次判据：** 同 SIZE 下 R_flex ≤ 5
  若两判据共同命中 → 记为 **OPT_SIZE**
  否则 → 记录最接近达标的 SIZE，标注为「部分达标」，写进 progress.md 讨论是否升级到 Phase 1B

- [ ] **步骤 4：扫描表落盘**
  把表格 + 选中的 OPT_SIZE 写入 `memory-bank/progress.md` Phase 2 小节。

- [ ] **步骤 5：revert 当前实验值**（准备 Phase 3 用 OPT_SIZE 定版）
  运行：`git checkout -- veloxa/foundation/memory/arena_allocator.h`
  预期：文件回到 `4096`

---

## Phase 3：实施修复 + RED 反向探针测试

**文件：**
- 修改：`veloxa/foundation/memory/arena_allocator.h`（最终定版）
- 修改：`tests/foundation/memory/arena_allocator_test.cc`（+1 GTest）

### 任务 3.1：新增 `DefaultBlockSizeLocked` 测试 [TDD]

- [ ] **步骤 1：编写失败测试**
  文件：`tests/foundation/memory/arena_allocator_test.cc`
  在末尾（第 70 行 `Alignment` 之后）追加：
  ```cpp
  TEST(ArenaAllocatorTest, DefaultBlockSizeLocked) {
    // TASK-20260424-01: default block_size locked (was 4096 pre-TASK-01).
    // 4 KB caused malloc/free churn in LayoutEngine, producing a
    // super-linear knee at ~N=200. Any change must come with a bench
    // re-run + baseline update. See techContext.md Layout performance section.
    ArenaAllocator arena;
    constexpr usize kExpected = {OPT_SIZE};  // replaced by Phase 3.2
    // 分配大小 (kExpected - 128) 的单块 — 预期 fits in one block.
    (void)arena.Allocate(kExpected - 128);
    // bytes_allocated 应准确反映此分配（oversized path 会有不同语义）
    EXPECT_EQ(arena.bytes_allocated(), kExpected - 128);
    // 再 alloc (kExpected) 应触发 **oversized** path（size > default_block_size_）
    // per arena_allocator.h:39 new_block_size = size + align
    (void)arena.Allocate(kExpected);
    EXPECT_EQ(arena.bytes_allocated(), (kExpected - 128) + kExpected);
  }
  ```
  **说明**：{OPT_SIZE} 占位，步骤 3.2 根据 Phase 2 结论替换为实际数字字面量。

- [ ] **步骤 2：运行测试验证失败**
  运行：
  ```bash
  cmake --build build --target arena_allocator_test -j
  ./build/tests/foundation/memory/arena_allocator_test --gtest_filter=ArenaAllocatorTest.DefaultBlockSizeLocked
  ```
  预期：**FAIL**（当前默认仍为 4096，与 kExpected 不符）

- [ ] **步骤 3：wip commit 红状态**
  运行：
  ```bash
  git add tests/foundation/memory/arena_allocator_test.cc
  git commit -m "wip(TASK-20260424-01): red — lock default block size expected = {OPT_SIZE}"
  ```

### 任务 3.2：实施最终默认值 bump [TDD GREEN]

- [ ] **步骤 1：改 `arena_allocator.h` 默认值**
  文件：`veloxa/foundation/memory/arena_allocator.h`
  行 13：
  ```cpp
  -  explicit ArenaAllocator(usize block_size = 4096)
  +  // Default block size: bumped 4096 → {OPT_SIZE} by TASK-20260424-01.
  +  //
  +  // 4 KB blocks caused malloc/free churn in LayoutEngine hot path:
  +  // at N=256 each iteration allocated ~37 blocks → 37 malloc+free pairs
  +  // per Layout() call, producing a super-linear knee
  +  // (BM_LayoutBuildTreeFlat/128→256 = 9.67× for 2× N).
  +  // {OPT_SIZE} removes the knee (R256 ≤ 2.5×) at the cost of +12 KB
  +  // per Document::arena_. Callers can still pass an explicit block_size.
  +  // See docs/specs/2026-04-24-layout-knee-root-cause-design.md §5.1
  +  explicit ArenaAllocator(usize block_size = {OPT_SIZE})
  ```

- [ ] **步骤 2：同步把测试中占位符 `{OPT_SIZE}` 替换成实际值**
  （在 3.1 步骤 1 放的是占位符；此步骤将其与 Phase 3.2 步骤 1 同时定版）

- [ ] **步骤 3：构建并跑该测试**
  运行：
  ```bash
  cmake --build build --target arena_allocator_test -j
  ./build/tests/foundation/memory/arena_allocator_test --gtest_filter=ArenaAllocatorTest.DefaultBlockSizeLocked
  ```
  预期：**PASS**

- [ ] **步骤 4：RED 反向探针验证（per systemPatterns「Mixed TDD RED 反向探针」）**
  临时注释掉默认 bump 一行（或改回 4096），重 build，跑测试 → 应 FAIL（捕获回归能力确认）
  然后立即恢复默认值 bump。

- [ ] **步骤 5：跑全量 ctest 回归**
  运行：
  ```bash
  cmake --build build -j
  cd build && ctest -j --output-on-failure | tail -10; cd ..
  ```
  预期：**891/891 PASS**（或新增 1 测试后 892/892 PASS）

- [ ] **步骤 6：commit**
  运行：
  ```bash
  git add veloxa/foundation/memory/arena_allocator.h tests/foundation/memory/arena_allocator_test.cc
  git commit -m "fix(foundation/memory): bump ArenaAllocator default block 4096 → {OPT_SIZE} (TASK-20260424-01)"
  ```

---

## Phase 4：ctest 全量 + Bench baseline 刷新

**文件：**
- 覆盖：`benchmarks/baseline/bench_layout_buildtree.json`
- 覆盖：`benchmarks/baseline/bench_layout_flex.json`
- 修改：`benchmarks/baseline/README.md`（环境表 + 变更历史行）

### 任务 4.1：Release 全量 rebuild + bench 全绿 [验收]

- [ ] **步骤 1：build-bench 全量重 build**
  运行：
  ```bash
  cmake --build build-bench -j 2>&1 | tail -5
  ```
  预期：0 errors / 0 warnings

- [ ] **步骤 2：跑 bench_layout_buildtree 全 BM**
  运行：
  ```bash
  ./build-bench/benchmarks/bench_layout_buildtree \
    --benchmark_repetitions=3 \
    --benchmark_min_time=0.5s \
    --benchmark_format=json \
    --benchmark_out=benchmarks/baseline/bench_layout_buildtree.json
  ```
  预期：exit 0；14 rows 全数字非零；`R256 ≤ 2.5×`（用 python3 再校验一次）

- [ ] **步骤 3：跑 bench_layout_flex 全 BM**
  运行：
  ```bash
  ./build-bench/benchmarks/bench_layout_flex \
    --benchmark_repetitions=3 \
    --benchmark_min_time=0.5s \
    --benchmark_format=json \
    --benchmark_out=benchmarks/baseline/bench_layout_flex.json
  ```
  预期：exit 0；6 rows 全数字非零；R_flex ≤ 5

- [ ] **步骤 4：JSON 体检**
  运行：
  ```bash
  python3 - <<'EOF'
  import json
  for name in ['bench_layout_buildtree', 'bench_layout_flex']:
    d = json.load(open(f'benchmarks/baseline/{name}.json'))
    assert d['context']['library_build_type'] == 'release', f"{name} not release!"
    print(f"{name}: release ✓ / {len(d['benchmarks'])} benchmarks")
  EOF
  ```
  预期：两行 release ✓

### 任务 4.2：baseline README 更新 [docs]

- [ ] **步骤 1：读 README 当前内容**
  （使用 Read 工具查看 `benchmarks/baseline/README.md`）

- [ ] **步骤 2：更新「当前生成环境」表 4 行**
  - CPU model / 时钟
  - 编译器版本
  - build_type = release（不变）
  - min_time = 0.5s（不变）
  - **新增行：** `ArenaAllocator default block = {OPT_SIZE}`

- [ ] **步骤 3：追加变更历史行**
  在 README 变更历史段追加：
  ```markdown
  - 2026-04-24 TASK-20260424-01：bench_layout_buildtree + bench_layout_flex baseline 刷新，
    根因 (d) malloc churn 确认；`ArenaAllocator` 默认 block_size 4096 → {OPT_SIZE}；
    `BM_LayoutBuildTreeFlat/128→256` 从 9.67× 收敛到 ≤ 2.5×；
    `BM_LayoutFlex<16,16>` 从 17.2× 收敛到 ≤ 5×。
  ```

- [ ] **步骤 4：commit Phase 4**
  运行：
  ```bash
  git add benchmarks/baseline/bench_layout_buildtree.json benchmarks/baseline/bench_layout_flex.json benchmarks/baseline/README.md
  git commit -m "docs(bench): refresh layout baselines after TASK-20260424-01 knee fix"
  ```

---

## Phase 5：techContext + Memory Bank 收尾

**文件：**
- 修改：`memory-bank/techContext.md`（「Layout 性能基线」段）
- 修改：`memory-bank/activeContext.md`（阶段变 `构建完成`）
- 修改：`memory-bank/tasks.md`（验收 10/10）
- 修改：`memory-bank/progress.md`（里程碑清单）

### 任务 5.1：techContext Layout 性能基线段刷新 [docs]

- [ ] **步骤 1：定位「Layout 性能基线」段**
  （Read `memory-bank/techContext.md`）

- [ ] **步骤 2：追加 K2 resolved 子段**
  ```markdown
  #### TASK-20260424-01 — Layout super-linear knee 已解决

  根因 **(d) ArenaAllocator 4KB block malloc/free churn** 经 Phase 1-2 block-size 扫描确认：
  `ArenaAllocator` 默认 block_size 4096 → {OPT_SIZE}。

  **前后对比（build-bench Release / WSL2 8核 2.92GHz / gcc 11.4 / min_time=0.5s）：**

  | BM | before | after | 改善 |
  |---|---:|---:|---:|
  | `BM_LayoutBuildTreeFlat/128` | {T128_BEFORE} ns | {T128_AFTER} ns | {R128_IMPR} |
  | `BM_LayoutBuildTreeFlat/256` | 76375 ns | {T256_AFTER} ns | **{R256_IMPR}** |
  | `BM_LayoutBuildTreeFlat/512` | 208027 ns | {T512_AFTER} ns | {R512_IMPR} |
  | `BM_LayoutFlex<16,16>` | 92023 ns | {TFLEX_AFTER} ns | {RFLEX_IMPR} |

  KNEE 消除标准：`R256 / R128 ≤ 2.5×`（实际达到 {R256_FINAL}）。

  内存成本：`Document::arena_` +（{OPT_SIZE} - 4096）字节 / instance ≈ +{MEM_DELTA} KB，嵌入式单 Document/view 可接受。
  ```
  所有 `{…}` 占位符由本阶段运行时数值替换。

### 任务 5.2：activeContext 更新到「构建完成」 [docs]

- [ ] **步骤 1：改 `memory-bank/activeContext.md` 当前阶段**
  从「规划中」→「构建完成」

- [ ] **步骤 2：更新「待处理事项」**
  - 划除候选区 TASK-20260419-10（标为「已立项为 TASK-20260424-01 并完成」）
  - 若 Phase 1B 未触发：`(e)/(f)` 候选根因标为「本任务 (d) 已解，未再测」，收纳到候选区 P3 触发型
  - 若 Phase 1B 触发：按实际结论更新

### 任务 5.3：tasks.md 验收勾选 [docs]

- [ ] **步骤 1：在任务段追加「验收标准 10/10 ✅」表**

- [ ] **步骤 2：记录最终实绩耗时（用于 plan × 0.6 协议校准）**

### 任务 5.4：progress.md 更新 [docs]

- [ ] **步骤 1：追加 Phase 1-5 的里程碑**

- [ ] **步骤 2：commit Phase 5**
  运行：
  ```bash
  git add memory-bank/
  git commit -m "chore(build): finalize TASK-20260424-01 memory bank state"
  ```

---

## Phase 1B（升级路径，仅当 Phase 1 判据 R256 ≥ 6×）

> **触发条件：** Phase 1 步骤 4 判据落在 `R256 ≥ 6×` 行，即根因 (d) 被否定或贡献极小。

### 任务 1B.1：per-phase 拆分 BM [实验]

**文件：** `benchmarks/bench_layout_phases.cc`（新建，临时，不注册到 main 合并前可选删除）

实现 3 个 BM：
- `BM_BuildTreeOnly/N`：仅跑 `LayoutEngine::BuildTree(...)`
- `BM_BuildTreePlusLayoutBlock/N`：BuildTree + LayoutBlock（跳过 ApplyPositioning）
- `BM_FullLayout/N`：完整 Layout（对照）

注册到 `benchmarks/CMakeLists.txt`（参考现有 bench 模式）。

### 任务 1B.2：采样 + 分解 [实验]

- [ ] 跑三个 BM 于 N ∈ {128, 256, 512}，采 json
- [ ] 按分解公式计算 LayoutBlock cost / ApplyPositioning cost
- [ ] 对 N=128 vs 256 各阶段独立算比值
- [ ] 定位 super-linear 所属阶段

### 任务 1B.3：根据定位决定后续 [决策]

- 若在 BuildTree → 非 (d) 就是 (e) L1D / 或 arena 的其它行为，产出调查报告
- 若在 LayoutBlock → (f) 算法内 O(N²) 嫌疑，需 perf 或 code review
- 若在 ApplyPositioning → 检查递归 + box tree 深度乘法效应

此 Phase 产出为**调查报告**（`docs/specs/2026-04-24-layout-knee-findings.md`）而非修复。

### 任务 1B.4：立跟进任务 [立项]

- [ ] 在 `memory-bank/tasks.md` 候选区立 **TASK-20260424-02**（承接具体修复方向）
- [ ] 本任务 TASK-01 状态收为「研究完成，修复跟进 TASK-02」

---

## 提交粒度 + 时间预估总表

| Phase | 提交数 | plan (min) | plan × 0.6 预期 (min) |
|:-:|:-:|:-:|:-:|
| 0 | 1（plan + van） | 10 | 6 |
| 1 | 0（实验，不 commit） | 20 | 12 |
| 2 | 0（实验，不 commit） | 25 | 15 |
| 3 | 2（red wip + green fix） | 25 | 15 |
| 4 | 1（baseline refresh） | 20 | 12 |
| 5 | 1（MB 收尾） | 15 | 9 |
| **合计** | **5** | **115** | **~70** |

**若 Phase 1B 触发：** +60-90 min plan / +40-55 min 预期；+1-2 commit（phase bench + findings）。

---

## 验收清单（摘自 design §7）

- [ ] 1. Phase 1 判据命中（R256 < 6×）
- [ ] 2. Phase 2 定位最优 OPT_SIZE
- [ ] 3. `BM_LayoutBuildTreeFlat/256 ≤ /128 × 2.5`
- [ ] 4. `BM_LayoutFlex<16,16> ≤ <8,8> × 5`
- [ ] 5. `ctest -j` 891/891 → 892/892 PASS
- [ ] 6. build-bench Release 全量 rebuild 0 errors / 0 warnings
- [ ] 7. `DefaultBlockSizeLocked` GTest PASS + RED 反向探针验证
- [ ] 8. 2 bench baseline JSON 刷新（`library_build_type=release`）
- [ ] 9. `benchmarks/baseline/README.md` 环境表 + 变更历史更新
- [ ] 10. `techContext.md` Layout 性能基线段补 knee-resolved 记录

**若 Phase 1B 触发补充：**
- [ ] 11'. `docs/specs/2026-04-24-layout-knee-findings.md` 产出
- [ ] 12'. TASK-20260424-02 在候选区立项

---

## 开始执行

计划完成并保存到本文件。准备执行 `/build` 阶段。
