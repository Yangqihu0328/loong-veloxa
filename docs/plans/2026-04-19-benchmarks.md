# Foundation Benchmarks 实现计划

**目标：** 集成 Google Benchmark v1.9.1 + 落地 Foundation 基础库（分配器/容器/字符串/HashMap）首版性能基线，建立 `benchmarks/` 目录脚手架供后续模块复用。

**架构：** 根 CMakeLists.txt 已就绪 `VX_BUILD_BENCHMARKS` option + FetchContent 拉取 v1.9.1；本计划新建 `benchmarks/` 子目录，按"一文件一可执行"组织 4 个 bench 目标，每个文件覆盖一个 Foundation 子模块（≈25 个 BM 用例）。结果留存策略：仅控制台 + 文档化 JSON 导出，不提交 baseline。

**技术栈：** C++17、CMake 3.20、Google Benchmark v1.9.1（FetchContent）、`vx_foundation` 静态库

**复杂度级别：** Level 2（简单增强）

---

## 设计文档

`docs/specs/2026-04-19-benchmarks-design.md`

## 关联任务

- 来源：TASK-20260405-01 Foundation 延期项 P1#1（"Benchmark 延期 — 需 google benchmark"）
- 后续：TASK-20260419-03（CSS 模块基准，未立项）、TASK-20260419-04（Layout/Render 基准，未立项）

---

## CMake 链接方向约束分析（决策已记录）

详见设计文档第 4 节。**结论：无需调整 vx_foundation 链接属性**，bench 目标只需 `PRIVATE` 链接 `vx_foundation` + `benchmark::benchmark`。

## FetchContent C 子项目编译选项审计（决策已记录）

详见设计文档第 5 节。**结论：google/benchmark 是纯 C++ 项目；项目根的 `-Werror`/`-Wpedantic` 已限定为 CXX**。Phase 1 步骤 4 验证 benchmark target 编译通过，若失败，方案是 `target_compile_options(benchmark PRIVATE -Wno-error)` 局部豁免，**不**全局放开。

---

## 文件结构

| 文件 | 职责 | 行数估计 |
|------|------|---------|
| `benchmarks/CMakeLists.txt` | `vx_add_benchmark()` 函数 + 4 个目标 | ~20 |
| `benchmarks/README.md` | 启用、运行、JSON 导出、proxy 说明 | ~80 |
| `benchmarks/bench_allocators.cc` | Malloc/Arena/Pool 三类分配器基准 | ~120 |
| `benchmarks/bench_containers.cc` | Vector/SmallVector/IntrusiveList | ~110 |
| `benchmarks/bench_hash_map.cc` | HashMap insert/lookup/rehash | ~90 |
| `benchmarks/bench_strings.cc` | BasicString SSO/heap、InternedString | ~110 |
| `memory-bank/techContext.md` | 追加 Benchmark 启用段；依赖表加 google/benchmark；技术债 #1 标 ✅ | +20 行 |

---

## Phase 概览

| Phase | 内容 | 预计 | 提交 |
|-------|------|------|------|
| 0 | 基线验证（890 测试全绿；FetchContent 通路验证） | 5 min | 0（验证不提交） |
| 1 | 创建分支 + `benchmarks/CMakeLists.txt` + smoke benchmark | 15 min | 1 |
| 2 | `bench_allocators.cc`（17 BM） | 25 min | 1 |
| 3 | `bench_containers.cc`（8 BM） | 20 min | 1 |
| 4 | `bench_hash_map.cc`（8 BM） | 15 min | 1 |
| 5 | `bench_strings.cc`（9 BM） | 25 min | 1 |
| 6 | `README.md` + `techContext.md` 更新 | 15 min | 1 |
| 7 | 收尾验证 + 依赖安全审计 + Memory Bank 收尾 | 15 min | 1（chore: finalize） |
| **合计** | | **~2h** | **7** |

测试模式说明：所有 phase 采用 `[覆盖补充]` 模式 —— 实现已存在于 `vx_foundation`，benchmark 文件本身的"测试"≡ 编译过 + 跑完无 abort + 至少打印数据行。

---

## Phase 0：基线验证

**目的：** 确认起点状态干净，FetchContent 通路就绪。

**前置：** 当前在 `main` 分支，工作树干净。

- [ ] **步骤 1：导出代理（如未导出）**

  ```bash
  export http_proxy=http://192.168.101.217:7890
  export https_proxy=$http_proxy
  ```

- [ ] **步骤 2：从干净状态构建并跑全测试**

  ```bash
  cd /home/qihooz/code/loong-veloxa
  rm -rf build
  cmake -B build -S .
  cmake --build build -j$(nproc) 2>&1 | tail -20
  ctest --test-dir build --output-on-failure 2>&1 | tail -30
  ```

  **预期：** 全部测试通过（声称数 890）。若失败，停止本任务，走 `/debug` 路径。

- [ ] **步骤 3：验证 FetchContent 能拉取 benchmark v1.9.1（带代理）**

  ```bash
  cmake -B build -DVX_BUILD_BENCHMARKS=ON 2>&1 | tail -30
  ```

  **预期：** 配置阶段输出 `Cloning into '_deps/benchmark-src'`，最终配置成功（即使后续因 `benchmarks/` 目录还不存在导致 `add_subdirectory(benchmarks)` 失败也无所谓——本步骤仅验证 FetchContent 能通）。**记录配置时间**到 progress.md。

  **如果 FetchContent 失败：** 检查代理设置 / 重新跑（GitHub 偶发抖动）；超过 3 次失败 → 升级到 `/debug`。

---

## Phase 1：分支 + CMake 脚手架 + smoke benchmark

**目的：** 建立 `benchmarks/` 目录，验证最小可运行 benchmark 端到端通路。

- [ ] **步骤 1：创建功能分支**

  ```bash
  git checkout -b feature/TASK-20260419-02-benchmarks main
  ```

- [ ] **步骤 2：创建 `benchmarks/CMakeLists.txt`**

  ```cmake
  # benchmarks/CMakeLists.txt
  function(vx_add_benchmark name)
    add_executable(${name} ${name}.cc)
    target_link_libraries(${name} PRIVATE vx_foundation benchmark::benchmark)
    set_target_properties(${name} PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/benchmarks")
  endfunction()

  vx_add_benchmark(bench_allocators)
  vx_add_benchmark(bench_containers)
  vx_add_benchmark(bench_hash_map)
  vx_add_benchmark(bench_strings)
  ```

- [ ] **步骤 3：创建 4 个 .cc 占位（仅含 `BENCHMARK_MAIN`）**

  每个文件先写最小内容确保 CMake 配置和编译通路：

  ```cpp
  // benchmarks/bench_allocators.cc
  #include <benchmark/benchmark.h>

  static void BM_Smoke(benchmark::State& state) {
    for (auto _ : state) {
      benchmark::DoNotOptimize(state.iterations());
    }
  }
  BENCHMARK(BM_Smoke);

  BENCHMARK_MAIN();
  ```

  对 `bench_containers.cc` / `bench_hash_map.cc` / `bench_strings.cc` 写相同内容。

- [ ] **步骤 4：配置并构建（验证 benchmark target 编译过）**

  ```bash
  rm -rf build
  cmake -B build -DVX_BUILD_BENCHMARKS=ON 2>&1 | tail -10
  cmake --build build --target benchmark -j$(nproc) 2>&1 | tail -20
  ```

  **预期：** `benchmark` library target 编译成功，无 `-Werror` 失败。
  **若失败：** `target_compile_options(benchmark PRIVATE -Wno-error)` 加在 `benchmarks/CMakeLists.txt` 顶部（在 `vx_add_benchmark` 函数定义之前），重新构建。记录到 progress.md。

- [ ] **步骤 5：构建所有 bench target**

  ```bash
  cmake --build build -j$(nproc) 2>&1 | tail -20
  ```

  **预期：** 4 个可执行文件生成于 `build/benchmarks/`，无错误。

- [ ] **步骤 6：跑 smoke**

  ```bash
  for b in bench_allocators bench_containers bench_hash_map bench_strings; do
    echo "=== $b ===" && ./build/benchmarks/$b --benchmark_min_time=0.01s
  done
  ```

  **预期：** 每个 exe 打印 `BM_Smoke` 行（time/CPU/iterations 三列均非零）。

- [ ] **步骤 7：现有测试回归验证**

  ```bash
  ctest --test-dir build --output-on-failure 2>&1 | tail -15
  ```

  **预期：** 890 测试全绿（启用 BENCHMARKS 不影响 GTest 套件）。

- [ ] **步骤 8：提交**

  ```bash
  git add benchmarks/CMakeLists.txt benchmarks/bench_*.cc
  git commit -m "build(benchmarks): scaffold benchmarks/ with vx_add_benchmark()

  - Wire VX_BUILD_BENCHMARKS=ON via FetchContent (google/benchmark v1.9.1)
  - 4 placeholder bench targets each containing BM_Smoke
  - Subsequent phases will fill in per-module benchmarks

  Refs TASK-20260419-02"
  ```

---

## Phase 2：bench_allocators.cc

**目的：** Malloc / Arena / Pool 三类分配器的核心吞吐基准。

- [ ] **步骤 1：替换占位文件内容**

  完整代码（直接写入 `benchmarks/bench_allocators.cc`，覆盖 Phase 1 的 smoke）：

  ```cpp
  // benchmarks/bench_allocators.cc
  //
  // Foundation memory allocator benchmarks.
  // Run: ./build/benchmarks/bench_allocators
  //
  // Note: MallocAllocator includes GlobalMemoryStats counter overhead;
  // numbers reflect that.

  #include <benchmark/benchmark.h>

  #include "veloxa/foundation/memory/arena_allocator.h"
  #include "veloxa/foundation/memory/malloc_allocator.h"
  #include "veloxa/foundation/memory/pool_allocator.h"

  namespace {

  // ---- MallocAllocator: alloc + immediate free ---------------------------------

  void BM_MallocAlloc(benchmark::State& state) {
    const std::size_t size = static_cast<std::size_t>(state.range(0));
    vx::MallocAllocator alloc;
    for (auto _ : state) {
      void* p = alloc.Allocate(size);
      benchmark::DoNotOptimize(p);
      alloc.Deallocate(p, size);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * size);
  }
  BENCHMARK(BM_MallocAlloc)->RangeMultiplier(8)->Range(8, 4096);

  // ---- ArenaAllocator: alloc only, periodic Reset ------------------------------

  void BM_ArenaAlloc(benchmark::State& state) {
    const std::size_t size = static_cast<std::size_t>(state.range(0));
    vx::ArenaAllocator arena(64 * 1024);
    std::size_t since_reset = 0;
    for (auto _ : state) {
      void* p = arena.Allocate(size);
      benchmark::DoNotOptimize(p);
      if (++since_reset == 256) {
        arena.Reset();
        since_reset = 0;
      }
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * size);
  }
  BENCHMARK(BM_ArenaAlloc)->RangeMultiplier(8)->Range(8, 4096);

  // ---- PoolAllocator: LIFO alloc/free at fixed block size ----------------------

  void BM_PoolAlloc(benchmark::State& state) {
    const std::size_t size = static_cast<std::size_t>(state.range(0));
    vx::PoolAllocator pool(size, 256);
    for (auto _ : state) {
      void* p = pool.Allocate(size);
      benchmark::DoNotOptimize(p);
      pool.Deallocate(p, size);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * size);
  }
  BENCHMARK(BM_PoolAlloc)->RangeMultiplier(8)->Range(8, 512);

  // ---- ArenaAllocator: Reset throughput on a populated arena -------------------

  void BM_ArenaReset(benchmark::State& state) {
    vx::ArenaAllocator arena(64 * 1024);
    for (auto _ : state) {
      state.PauseTiming();
      for (int i = 0; i < 100; ++i) {
        benchmark::DoNotOptimize(arena.Allocate(64));
      }
      state.ResumeTiming();
      arena.Reset();
    }
  }
  BENCHMARK(BM_ArenaReset);

  // ---- PoolAllocator: steady-state alloc/free churn ----------------------------

  void BM_PoolAllocFreeChurn(benchmark::State& state) {
    constexpr std::size_t kSize = 64;
    constexpr int kBatch = 100;
    vx::PoolAllocator pool(kSize, 256);
    void* slots[kBatch];
    for (auto _ : state) {
      for (int i = 0; i < kBatch; ++i) slots[i] = pool.Allocate(kSize);
      for (int i = 0; i < kBatch; ++i) pool.Deallocate(slots[i], kSize);
      benchmark::DoNotOptimize(slots);
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * kBatch);
  }
  BENCHMARK(BM_PoolAllocFreeChurn);

  }  // namespace

  BENCHMARK_MAIN();
  ```

- [ ] **步骤 2：构建**

  ```bash
  cmake --build build --target bench_allocators -j$(nproc) 2>&1 | tail -15
  ```

  **预期：** 编译通过（无 `-Werror` 失败）。
  **常见失败：** unused parameter / sign comparison —— 修复源码（变量带 `_`、`std::size_t` 强转）。

- [ ] **步骤 3：跑用例**

  ```bash
  ./build/benchmarks/bench_allocators --benchmark_min_time=0.05s 2>&1 | tail -25
  ```

  **预期输出：** 包含 `BM_MallocAlloc/8`、`BM_ArenaAlloc/64`、`BM_PoolAlloc/512`、`BM_ArenaReset`、`BM_PoolAllocFreeChurn` 等行；time 列均为正数 ns。

- [ ] **步骤 4：现有测试回归**

  ```bash
  ctest --test-dir build --output-on-failure 2>&1 | tail -10
  ```

  **预期：** 890 测试全绿。

- [ ] **步骤 5：提交**

  ```bash
  git add benchmarks/bench_allocators.cc
  git commit -m "bench(foundation): allocator throughput baselines

  Covers Malloc/Arena/Pool alloc+free, Arena Reset, and Pool steady-state
  churn. Sizes: 8/64/512/4096 (Pool capped at 512 due to fixed block).

  Refs TASK-20260419-02"
  ```

---

## Phase 3：bench_containers.cc

**目的：** Vector / SmallVector / IntrusiveList 操作基准。

- [ ] **步骤 1：替换占位文件内容**

  ```cpp
  // benchmarks/bench_containers.cc
  //
  // Foundation container benchmarks (Vector, SmallVector, IntrusiveList).
  // Run: ./build/benchmarks/bench_containers

  #include <benchmark/benchmark.h>

  #include "veloxa/foundation/containers/intrusive_list.h"
  #include "veloxa/foundation/containers/small_vector.h"
  #include "veloxa/foundation/containers/vector.h"

  namespace {

  // ---- Vector ------------------------------------------------------------------

  void BM_VectorPushBackInt(benchmark::State& state) {
    const std::size_t n = static_cast<std::size_t>(state.range(0));
    for (auto _ : state) {
      vx::Vector<int> v;
      for (std::size_t i = 0; i < n; ++i) v.push_back(static_cast<int>(i));
      benchmark::DoNotOptimize(v.data());
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * n);
  }
  BENCHMARK(BM_VectorPushBackInt)->RangeMultiplier(16)->Range(16, 4096);

  void BM_VectorReservePushBackInt(benchmark::State& state) {
    constexpr std::size_t kN = 4096;
    for (auto _ : state) {
      vx::Vector<int> v;
      v.reserve(kN);
      for (std::size_t i = 0; i < kN; ++i) v.push_back(static_cast<int>(i));
      benchmark::DoNotOptimize(v.data());
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * kN);
  }
  BENCHMARK(BM_VectorReservePushBackInt);

  // ---- SmallVector -------------------------------------------------------------

  void BM_SmallVectorInline(benchmark::State& state) {
    constexpr std::size_t kN = 4;  // inline cap = 8
    for (auto _ : state) {
      vx::SmallVector<int, 8> v;
      for (std::size_t i = 0; i < kN; ++i) v.push_back(static_cast<int>(i));
      benchmark::DoNotOptimize(v.data());
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * kN);
  }
  BENCHMARK(BM_SmallVectorInline);

  void BM_SmallVectorOverflow(benchmark::State& state) {
    constexpr std::size_t kN = 32;  // inline cap = 8 → spills to heap
    for (auto _ : state) {
      vx::SmallVector<int, 8> v;
      for (std::size_t i = 0; i < kN; ++i) v.push_back(static_cast<int>(i));
      benchmark::DoNotOptimize(v.data());
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * kN);
  }
  BENCHMARK(BM_SmallVectorOverflow);

  // ---- IntrusiveList -----------------------------------------------------------

  struct LinkedItem {
    int value;
    vx::IntrusiveListNode hook;
  };

  void BM_IntrusiveListPushFront(benchmark::State& state) {
    constexpr std::size_t kN = 256;
    std::vector<LinkedItem> items(kN);
    for (std::size_t i = 0; i < kN; ++i) items[i].value = static_cast<int>(i);
    for (auto _ : state) {
      vx::IntrusiveList<LinkedItem, &LinkedItem::hook> list;
      for (std::size_t i = 0; i < kN; ++i) list.push_front(&items[i]);
      benchmark::DoNotOptimize(list.size());
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * kN);
  }
  BENCHMARK(BM_IntrusiveListPushFront);

  void BM_IntrusiveListIterate(benchmark::State& state) {
    constexpr std::size_t kN = 256;
    std::vector<LinkedItem> items(kN);
    vx::IntrusiveList<LinkedItem, &LinkedItem::hook> list;
    for (std::size_t i = 0; i < kN; ++i) {
      items[i].value = static_cast<int>(i);
      list.push_back(&items[i]);
    }
    for (auto _ : state) {
      int sum = 0;
      for (auto it = list.begin(); it != list.end(); ++it) {
        sum += it->value;
      }
      benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * kN);
  }
  BENCHMARK(BM_IntrusiveListIterate);

  }  // namespace

  BENCHMARK_MAIN();
  ```

  > 注：`BM_IntrusiveListIterate` 假设 `IntrusiveList::begin()` / `end()` 与 `Iterator` 已实现 —— Phase 准备阶段已通过 Read 验证（intrusive_list.h L20-57 + L78 push_back）。如有 API 差异，调整字段名后再编译。

- [ ] **步骤 2：构建**

  ```bash
  cmake --build build --target bench_containers -j$(nproc) 2>&1 | tail -15
  ```

  **常见失败：** `IntrusiveList::begin()` 返回类型 / `size()` 不存在 → 查 `intrusive_list.h` 实际 API 调整。

- [ ] **步骤 3：跑用例**

  ```bash
  ./build/benchmarks/bench_containers --benchmark_min_time=0.05s 2>&1 | tail -20
  ```

  **预期：** 包含 `BM_VectorPushBackInt/256`、`BM_SmallVectorInline`、`BM_IntrusiveListIterate` 等行。

- [ ] **步骤 4：现有测试回归**

  ```bash
  ctest --test-dir build --output-on-failure 2>&1 | tail -10
  ```

- [ ] **步骤 5：提交**

  ```bash
  git add benchmarks/bench_containers.cc
  git commit -m "bench(foundation): container ops baselines

  Vector push_back (with/without reserve), SmallVector inline vs heap
  overflow, IntrusiveList push_front + iterate.

  Refs TASK-20260419-02"
  ```

---

## Phase 4：bench_hash_map.cc

**目的：** HashMap 插入 / 查询命中 / 查询未命中 / 重哈希基准。

- [ ] **步骤 1：替换占位文件内容**

  ```cpp
  // benchmarks/bench_hash_map.cc
  //
  // Foundation HashMap benchmarks.
  // Run: ./build/benchmarks/bench_hash_map

  #include <benchmark/benchmark.h>

  #include <vector>

  #include "veloxa/foundation/containers/hash_map.h"

  namespace {

  void BM_HashMapInsertInt(benchmark::State& state) {
    const std::size_t n = static_cast<std::size_t>(state.range(0));
    for (auto _ : state) {
      vx::HashMap<int, int> m;
      for (std::size_t i = 0; i < n; ++i) {
        m.Insert(static_cast<int>(i), static_cast<int>(i * 2));
      }
      benchmark::DoNotOptimize(m.size());
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * n);
  }
  BENCHMARK(BM_HashMapInsertInt)->RangeMultiplier(16)->Range(64, 16384);

  void BM_HashMapLookupHitInt(benchmark::State& state) {
    const std::size_t n = static_cast<std::size_t>(state.range(0));
    vx::HashMap<int, int> m;
    for (std::size_t i = 0; i < n; ++i) {
      m.Insert(static_cast<int>(i), static_cast<int>(i * 2));
    }
    std::size_t i = 0;
    for (auto _ : state) {
      int* v = m.Find(static_cast<int>(i % n));
      benchmark::DoNotOptimize(v);
      ++i;
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
  }
  BENCHMARK(BM_HashMapLookupHitInt)->RangeMultiplier(16)->Range(64, 16384);

  void BM_HashMapLookupMissInt(benchmark::State& state) {
    constexpr std::size_t kN = 1024;
    vx::HashMap<int, int> m;
    for (std::size_t i = 0; i < kN; ++i) {
      m.Insert(static_cast<int>(i), static_cast<int>(i * 2));
    }
    std::size_t i = 0;
    for (auto _ : state) {
      int* v = m.Find(static_cast<int>(kN + (i % kN)));  // guaranteed miss
      benchmark::DoNotOptimize(v);
      ++i;
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
  }
  BENCHMARK(BM_HashMapLookupMissInt);

  void BM_HashMapRehash(benchmark::State& state) {
    constexpr std::size_t kTarget = 1024;
    for (auto _ : state) {
      vx::HashMap<int, int> m;  // default capacity = 16 → multiple rehashes
      for (std::size_t i = 0; i < kTarget; ++i) {
        m.Insert(static_cast<int>(i), 0);
      }
      benchmark::DoNotOptimize(m.size());
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * kTarget);
  }
  BENCHMARK(BM_HashMapRehash);

  }  // namespace

  BENCHMARK_MAIN();
  ```

- [ ] **步骤 2：构建**

  ```bash
  cmake --build build --target bench_hash_map -j$(nproc) 2>&1 | tail -10
  ```

- [ ] **步骤 3：跑用例**

  ```bash
  ./build/benchmarks/bench_hash_map --benchmark_min_time=0.05s 2>&1 | tail -20
  ```

  **预期：** 包含 `BM_HashMapInsertInt/16384`、`BM_HashMapLookupHitInt/1024`、`BM_HashMapLookupMissInt`、`BM_HashMapRehash` 等行。

- [ ] **步骤 4：现有测试回归**

  ```bash
  ctest --test-dir build --output-on-failure 2>&1 | tail -10
  ```

- [ ] **步骤 5：提交**

  ```bash
  git add benchmarks/bench_hash_map.cc
  git commit -m "bench(foundation): HashMap insert/lookup/rehash baselines

  Insert n=64..16384, hit/miss lookup, multi-rehash from default
  capacity (16) to 1024 entries.

  Refs TASK-20260419-02"
  ```

---

## Phase 5：bench_strings.cc

**目的：** BasicString SSO/heap 路径 + InternedString intern/lookup 基准。

- [ ] **步骤 1：替换占位文件内容**

  ```cpp
  // benchmarks/bench_strings.cc
  //
  // Foundation string benchmarks (BasicString, InternedString).
  // Run: ./build/benchmarks/bench_strings

  #include <benchmark/benchmark.h>

  #include <string>
  #include <vector>

  #include "veloxa/foundation/strings/interned_string.h"
  #include "veloxa/foundation/strings/string.h"
  #include "veloxa/foundation/strings/string_view.h"

  namespace {

  // ---- BasicString construction (SSO vs heap) ----------------------------------
  // Note: SSO capacity = 22 bytes per string.h L15.

  void BM_StringConstructSso(benchmark::State& state) {
    const std::size_t len = static_cast<std::size_t>(state.range(0));
    std::string source(len, 'a');
    vx::StringView sv(source.data(), source.size());
    for (auto _ : state) {
      vx::String s(sv);
      benchmark::DoNotOptimize(s.data());
    }
  }
  BENCHMARK(BM_StringConstructSso)->Arg(8)->Arg(22);

  void BM_StringConstructHeap(benchmark::State& state) {
    const std::size_t len = static_cast<std::size_t>(state.range(0));
    std::string source(len, 'a');
    vx::StringView sv(source.data(), source.size());
    for (auto _ : state) {
      vx::String s(sv);
      benchmark::DoNotOptimize(s.data());
    }
  }
  BENCHMARK(BM_StringConstructHeap)->Arg(64)->Arg(256);

  // ---- BasicString append ------------------------------------------------------

  void BM_StringAppendSso(benchmark::State& state) {
    constexpr std::size_t kTotal = 16;  // stays inside SSO
    for (auto _ : state) {
      vx::String s;
      for (std::size_t i = 0; i < kTotal; ++i) s.append('x');
      benchmark::DoNotOptimize(s.data());
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * kTotal);
  }
  BENCHMARK(BM_StringAppendSso);

  void BM_StringAppendHeap(benchmark::State& state) {
    constexpr std::size_t kTotal = 256;  // forces SSO→heap promotion + grows
    for (auto _ : state) {
      vx::String s;
      for (std::size_t i = 0; i < kTotal; ++i) s.append('x');
      benchmark::DoNotOptimize(s.data());
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * kTotal);
  }
  BENCHMARK(BM_StringAppendHeap);

  // ---- InternedString ----------------------------------------------------------
  // Note: global table is process-wide; ClearInternedStrings() is called between
  // setup and timed loops to keep results deterministic.

  void BM_InternedStringInternUnique(benchmark::State& state) {
    constexpr int kN = 100;
    std::vector<std::string> sources;
    sources.reserve(kN);
    for (int i = 0; i < kN; ++i) {
      sources.emplace_back("vx_bench_unique_key_" + std::to_string(i));
    }
    int idx = 0;
    for (auto _ : state) {
      state.PauseTiming();
      vx::InternedString::ClearInternedStrings();
      state.ResumeTiming();
      for (int i = 0; i < kN; ++i) {
        auto s = vx::InternedString::Intern(
            vx::StringView(sources[i].data(), sources[i].size()));
        benchmark::DoNotOptimize(s);
      }
      ++idx;
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * kN);
  }
  BENCHMARK(BM_InternedStringInternUnique);

  void BM_InternedStringInternDup(benchmark::State& state) {
    constexpr int kN = 100;
    const std::string source = "vx_bench_dup_key_repeated";
    vx::InternedString::ClearInternedStrings();
    // Pre-intern once so all subsequent calls hit the cache.
    vx::InternedString::Intern(
        vx::StringView(source.data(), source.size()));
    for (auto _ : state) {
      for (int i = 0; i < kN; ++i) {
        auto s = vx::InternedString::Intern(
            vx::StringView(source.data(), source.size()));
        benchmark::DoNotOptimize(s);
      }
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * kN);
  }
  BENCHMARK(BM_InternedStringInternDup);

  void BM_InternedStringEquality(benchmark::State& state) {
    vx::InternedString::ClearInternedStrings();
    const std::string source = "vx_bench_eq_key";
    auto a = vx::InternedString::Intern(
        vx::StringView(source.data(), source.size()));
    auto b = vx::InternedString::Intern(
        vx::StringView(source.data(), source.size()));
    for (auto _ : state) {
      bool eq = (a == b);
      benchmark::DoNotOptimize(eq);
    }
  }
  BENCHMARK(BM_InternedStringEquality);

  }  // namespace

  BENCHMARK_MAIN();
  ```

- [ ] **步骤 2：构建**

  ```bash
  cmake --build build --target bench_strings -j$(nproc) 2>&1 | tail -10
  ```

  **常见失败：** `String::append(char)` 与 `append(StringView)` 重载二义性 → 在调用处显式 `s.append('x')` 已正确（h L155 单字符重载存在）。

- [ ] **步骤 3：跑用例**

  ```bash
  ./build/benchmarks/bench_strings --benchmark_min_time=0.05s 2>&1 | tail -20
  ```

  **预期：** 包含 `BM_StringConstructSso/22`、`BM_StringConstructHeap/256`、`BM_StringAppendHeap`、`BM_InternedStringInternUnique`、`BM_InternedStringInternDup`、`BM_InternedStringEquality` 等行。`BM_InternedStringInternDup` 应显著快于 `BM_InternedStringInternUnique`（缓存命中）。

- [ ] **步骤 4：现有测试回归**

  ```bash
  ctest --test-dir build --output-on-failure 2>&1 | tail -10
  ```

- [ ] **步骤 5：提交**

  ```bash
  git add benchmarks/bench_strings.cc
  git commit -m "bench(foundation): String/InternedString baselines

  BasicString SSO vs heap construction + append, InternedString
  unique/duplicate intern + O(1) pointer equality.

  Refs TASK-20260419-02"
  ```

---

## Phase 6：文档（README + techContext）

- [ ] **步骤 1：创建 `benchmarks/README.md`**

  ```markdown
  # Veloxa Benchmarks

  Performance baselines built on [Google Benchmark v1.9.1](https://github.com/google/benchmark).

  ## Enable

  Benchmarks are off by default. Configure with:

  ```bash
  cmake -B build -DVX_BUILD_BENCHMARKS=ON
  cmake --build build -j$(nproc)
  ```

  The first `cmake -B build -DVX_BUILD_BENCHMARKS=ON` triggers `FetchContent`
  to clone `google/benchmark` into `build/_deps/benchmark-src/`. Behind a
  corporate / WSL proxy, set:

  ```bash
  export http_proxy=http://your-proxy:port
  export https_proxy=$http_proxy
  ```

  See `memory-bank/techContext.md` § "FetchContent 与代理" for offline
  workarounds.

  ## Targets

  | Executable | Coverage |
  |-----------|----------|
  | `bench_allocators` | `MallocAllocator`, `ArenaAllocator`, `PoolAllocator` |
  | `bench_containers` | `Vector`, `SmallVector`, `IntrusiveList` |
  | `bench_hash_map`   | `HashMap` insert / lookup hit & miss / rehash |
  | `bench_strings`    | `BasicString` (SSO+heap), `InternedString` |

  Each builds to `build/benchmarks/bench_<name>`.

  ## Run

  Single suite:

  ```bash
  ./build/benchmarks/bench_allocators
  ```

  Filter by regex:

  ```bash
  ./build/benchmarks/bench_allocators --benchmark_filter=BM_ArenaAlloc
  ```

  All suites in sequence:

  ```bash
  for b in bench_allocators bench_containers bench_hash_map bench_strings; do
    echo "=== $b ===" && ./build/benchmarks/$b
  done
  ```

  ## Export results to JSON

  For programmatic comparison between runs (e.g. before/after an optimization):

  ```bash
  ./build/benchmarks/bench_allocators \
    --benchmark_format=json \
    --benchmark_out=/tmp/before.json

  # ... apply changes, rebuild ...

  ./build/benchmarks/bench_allocators \
    --benchmark_format=json \
    --benchmark_out=/tmp/after.json
  ```

  Compare with the upstream `compare.py` tool:

  ```bash
  python3 build/_deps/benchmark-src/tools/compare.py \
    benchmarks /tmp/before.json /tmp/after.json
  ```

  ## Caveats

  - **No baseline files are committed** — hardware / compiler / thermal state
    differences make absolute numbers transient. Use the JSON export workflow
    above for local before/after comparisons during optimization work.
  - `MallocAllocator` includes `GlobalMemoryStats` counter overhead; the
    numbers reflect that, not raw `malloc` cost.
  - `BM_InternedString*` benchmarks call `ClearInternedStrings()` internally;
    do not run multiple instances of `bench_strings` in parallel.
  - For target embedded platforms (ARM / RTOS), re-run on the actual hardware
    — the dev-machine numbers are ballpark only.

  ## Adding a new benchmark file

  1. Create `benchmarks/bench_<module>.cc` containing benchmarks + `BENCHMARK_MAIN();`
  2. Append `vx_add_benchmark(bench_<module>)` to `benchmarks/CMakeLists.txt`
  3. Link extra static libs by extending the function or using
     `target_link_libraries(bench_<module> PRIVATE <extra>)` after the call.
  ```

- [ ] **步骤 2：更新 `memory-bank/techContext.md`**

  在「第三方依赖」表追加一行：

  ```
  | google/benchmark | 性能基准 | **已接入** v1.9.1（仅 VX_BUILD_BENCHMARKS=ON），FetchContent |
  ```

  在「FetchContent 与代理」段后追加新段：

  ```markdown
  ## Benchmarks 启用与运行

  Benchmark 默认关闭。启用：

  ```bash
  cmake -B build -DVX_BUILD_BENCHMARKS=ON
  cmake --build build -j$(nproc)
  ./build/benchmarks/bench_allocators  # 或 bench_containers/bench_hash_map/bench_strings
  ```

  详见 `benchmarks/README.md`。基线策略：**不**提交 baseline JSON，
  使用 `compare.py` 在本地做 before/after 对比（README 有命令）。
  ```

  在「技术债务清单」中将 #1 改为：

  ```
  1. ~~Benchmark 延期（需 google benchmark）~~ ✅ 已接入 google/benchmark v1.9.1（TASK-20260419-02），覆盖 Foundation 子模块；CSS/Layout/Render 模块基准留作 TASK-20260419-03/04
  ```

- [ ] **步骤 3：构建并运行 4 个 bench 验证文档命令准确**

  ```bash
  cmake --build build -j$(nproc) 2>&1 | tail -5
  for b in bench_allocators bench_containers bench_hash_map bench_strings; do
    echo "=== $b ===" && ./build/benchmarks/$b --benchmark_min_time=0.01s 2>&1 | head -10
  done
  ```

  **预期：** 4 个 exe 都能正常输出表头 + BM 行。

- [ ] **步骤 4：JSON 导出 smoke**

  ```bash
  ./build/benchmarks/bench_allocators \
    --benchmark_format=json \
    --benchmark_out=/tmp/vx_bench_smoke.json \
    --benchmark_min_time=0.01s 2>&1 | tail -3
  test -s /tmp/vx_bench_smoke.json && echo "JSON OK ($(wc -c < /tmp/vx_bench_smoke.json) bytes)"
  rm /tmp/vx_bench_smoke.json
  ```

  **预期：** 输出 `JSON OK (xxx bytes)`，文件存在且非空。

- [ ] **步骤 5：提交**

  ```bash
  git add benchmarks/README.md memory-bank/techContext.md
  git commit -m "docs(benchmarks): document enable/run/JSON export workflow

  - benchmarks/README.md: per-target coverage table, run commands,
    JSON export + compare.py workflow, caveats (no baseline files).
  - techContext.md: dependency table adds google/benchmark v1.9.1;
    new 'Benchmarks 启用与运行' section; tech-debt #1 marked resolved.

  Refs TASK-20260419-02"
  ```

---

## Phase 7：完成验证 + 依赖安全审计 + Memory Bank 收尾

按 `verification.mdc` 完成验证清单。

- [ ] **步骤 1：清空构建目录后从零构建（含 benchmarks）**

  ```bash
  rm -rf build
  cmake -B build -DVX_BUILD_BENCHMARKS=ON 2>&1 | tail -10
  cmake --build build -j$(nproc) 2>&1 | tail -10
  ```

  **预期：** 配置 + 构建均成功，无 `-Werror` / `-Wpedantic` 失败。

- [ ] **步骤 2：完整测试套件**

  ```bash
  ctest --test-dir build --output-on-failure 2>&1 | tail -15
  ```

  **预期：** 890 测试 100% 通过。

- [ ] **步骤 3：4 个 bench exe 各跑一次完整 BM 集**

  ```bash
  for b in bench_allocators bench_containers bench_hash_map bench_strings; do
    echo "=== $b ===" && ./build/benchmarks/$b --benchmark_min_time=0.05s 2>&1
  done | tee /tmp/vx_bench_full.txt | tail -100
  echo "---"
  echo "Total BM lines: $(grep -cE '^BM_' /tmp/vx_bench_full.txt)"
  ```

  **预期：** "Total BM lines" ≥ 30（4 个文件展开后的实际数）；无 abort / signal。

- [ ] **步骤 4：依赖安全审计 — google/benchmark v1.9.1 CVE 检索**

  ```bash
  echo "Checking google/benchmark v1.9.x CVE history..."
  curl -sS --max-time 15 -x http://192.168.101.217:7890 \
    "https://services.nvd.nist.gov/rest/json/cves/2.0?keywordSearch=google+benchmark" \
    | python3 -c "import sys, json; d=json.load(sys.stdin); print('Total matches:', d.get('totalResults', 'N/A'))" \
    || echo "(NVD API unavailable; manual check required)"
  ```

  备用：访问 https://github.com/google/benchmark/security/advisories ，确认无 CRITICAL/HIGH advisory 影响 v1.9.1。

  **预期：** 无 CRITICAL/HIGH 漏洞影响 v1.9.1。结果记录到 progress.md。

  > 备注：google/benchmark 是开发期/测试期依赖，不进入生产二进制（`VX_BUILD_BENCHMARKS=OFF` 默认），CVE 影响面比生产依赖更小，但仍按规范走一次审计。

- [ ] **步骤 5：重读设计文档逐项确认**

  逐项核对 `docs/specs/2026-04-19-benchmarks-design.md` § 11「完成标准」：

  ```bash
  echo "完成标准核对："
  echo "[1] cmake DVX_BUILD_BENCHMARKS=ON 配置成功 → 步骤 1 已验证"
  echo "[2] cmake --build 全量成功 → 步骤 1 已验证"
  echo "[3] 890 GTest 测试 100% → 步骤 2 已验证"
  echo "[4] 4 个 bench exe 独立可跑 + 各 ≥ 5 行 BM → 步骤 3 已验证"
  echo "[5] benchmarks/README.md 文档完备 → Phase 6 已完成"
  echo "[6] techContext.md 更新 → Phase 6 已完成"
  echo "[7] CVE 审计通过 → 步骤 4 已完成"
  ```

- [ ] **步骤 6：更新 Memory Bank**

  - `memory-bank/tasks.md`：将 TASK-20260419-02 标记为「已完成（待 reflect）」，写入实际提交数 / 实际新增 BM 数 / 关键决策摘要
  - `memory-bank/progress.md`：追加 Phase 0-7 完成时间线 + Phase 4 步骤 4 安全审计结论
  - `memory-bank/activeContext.md`：阶段保持 `构建中`（reflect 阶段才转 `回顾中`），更新焦点为「待 /reflect」

- [ ] **步骤 7：收尾提交**

  ```bash
  git add memory-bank/tasks.md memory-bank/progress.md memory-bank/activeContext.md
  git status
  git commit -m "chore(build): finalize TASK-20260419-02 memory bank state

  Build complete: 6 functional commits + scaffolding, 4 bench targets,
  ~30+ BM cases. Foundation P1#1 tech-debt cleared. Dependency security
  audit (google/benchmark v1.9.1) clean.

  Refs TASK-20260419-02"
  git status  # verify clean tree
  ```

- [ ] **步骤 8：输出构建完成报告**

  ```markdown
  ## 🔨 构建完成

  **已完成任务：** 7/7
  **测试状态：** 890/890 通过（验证命令见 Phase 7 步骤 2）
  **新增 BM 用例：** ~42（按 Range 展开）
  **提交记录：** 7 个（脚手架 + 4 个 bench 文件 + 文档 + 收尾）
  **分支：** feature/TASK-20260419-02-benchmarks
  **依赖审计：** google/benchmark v1.9.1 无 CRITICAL/HIGH CVE

  **下一步：** 使用 `/reflect` 进行回顾
  ```

---

## 风险与备用路径

| 触发条件 | 处理 |
|---------|------|
| FetchContent 拉取失败 | 检查代理；3 次失败后 → `/debug` |
| benchmark target `-Werror` 编译失败 | `target_compile_options(benchmark PRIVATE -Wno-error)` 局部豁免 |
| Phase 3-5 中某个 BM 因 API 假设错调用失败 | 立即 Read 对应 `*.h`，调整调用，**不**修改 vx_foundation 实现（基线测量目的） |
| 实际跑出来某 BM 时间 ≈ 0（被优化掉） | 加 `benchmark::DoNotOptimize(...)` 或 `benchmark::ClobberMemory()` |
| 计划比预期复杂 | 升级 Level 3，回 `/plan`（按 complexity-levels.mdc 升级流程） |

---

## 执行交接

**计划完成并保存到 `docs/plans/2026-04-19-benchmarks.md`。准备执行？**

- 子代理可用 → 可考虑用 1 个子代理跑完 Phase 2-5 的 4 个独立 bench 文件（共享 `*.h` 已存在、无相互依赖、CMakeLists.txt Phase 1 已落地）
- 当前会话执行 → `/build` 命令进入 TDD 循环
