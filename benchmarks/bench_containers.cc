// benchmarks/bench_containers.cc
//
// Foundation container benchmarks.
// Run: ./build/benchmarks/bench_containers

#include <benchmark/benchmark.h>

#include <cstdint>
#include <vector>

#include "veloxa/foundation/containers/intrusive_list.h"
#include "veloxa/foundation/containers/small_vector.h"
#include "veloxa/foundation/containers/vector.h"

namespace {

// ---- Vector<int>: push_back from empty (grows) -------------------------------

void BM_VectorPushBackInt(benchmark::State& state) {
  const int n = static_cast<int>(state.range(0));
  for (auto _ : state) {
    vx::Vector<int> v;
    for (int i = 0; i < n; ++i) v.push_back(i);
    benchmark::DoNotOptimize(v.data());
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * n);
}
BENCHMARK(BM_VectorPushBackInt)->RangeMultiplier(16)->Range(16, 4096);

// ---- Vector<int>: push_back with reserve (no grow) ---------------------------

void BM_VectorReservePushBackInt(benchmark::State& state) {
  const int n = static_cast<int>(state.range(0));
  for (auto _ : state) {
    vx::Vector<int> v;
    v.reserve(static_cast<vx::usize>(n));
    for (int i = 0; i < n; ++i) v.push_back(i);
    benchmark::DoNotOptimize(v.data());
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * n);
}
BENCHMARK(BM_VectorReservePushBackInt)->Arg(4096);

// ---- SmallVector<int, 8>: stays inline ---------------------------------------

void BM_SmallVectorInline(benchmark::State& state) {
  for (auto _ : state) {
    vx::SmallVector<int, 8> v;
    for (int i = 0; i < 4; ++i) v.push_back(i);
    benchmark::DoNotOptimize(v.data());
  }
}
BENCHMARK(BM_SmallVectorInline);

// ---- SmallVector<int, 8>: overflows to heap ----------------------------------

void BM_SmallVectorOverflow(benchmark::State& state) {
  for (auto _ : state) {
    vx::SmallVector<int, 8> v;
    for (int i = 0; i < 32; ++i) v.push_back(i);
    benchmark::DoNotOptimize(v.data());
  }
}
BENCHMARK(BM_SmallVectorOverflow);

// ---- IntrusiveList: push_front into preallocated nodes -----------------------

namespace il {
struct Node {
  int value;
  vx::IntrusiveListNode link;
};
}  // namespace il

void BM_IntrusiveListPushFront(benchmark::State& state) {
  constexpr int kN = 256;
  std::vector<il::Node> nodes(kN);
  for (auto _ : state) {
    vx::IntrusiveList<il::Node, &il::Node::link> list;
    for (int i = 0; i < kN; ++i) {
      nodes[i].link = vx::IntrusiveListNode{};
      list.push_front(&nodes[i]);
    }
    benchmark::DoNotOptimize(list.size());
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * kN);
}
BENCHMARK(BM_IntrusiveListPushFront);

// ---- IntrusiveList: full traversal -------------------------------------------

void BM_IntrusiveListIterate(benchmark::State& state) {
  constexpr int kN = 256;
  std::vector<il::Node> nodes(kN);
  vx::IntrusiveList<il::Node, &il::Node::link> list;
  for (int i = 0; i < kN; ++i) {
    nodes[i].value = i;
    list.push_back(&nodes[i]);
  }
  for (auto _ : state) {
    int sum = 0;
    for (auto& n : list) sum += n.value;
    benchmark::DoNotOptimize(sum);
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * kN);
}
BENCHMARK(BM_IntrusiveListIterate);

}  // namespace

BENCHMARK_MAIN();
