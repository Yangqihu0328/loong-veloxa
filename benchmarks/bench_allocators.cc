// benchmarks/bench_allocators.cc
//
// Foundation memory allocator benchmarks.
// Run: ./build/benchmarks/bench_allocators
//
// Note: MallocAllocator includes GlobalMemoryStats counter overhead;
// numbers reflect that.

#include <benchmark/benchmark.h>

#include <cstddef>
#include <cstdint>

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
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(size));
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
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(size));
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
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(size));
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
