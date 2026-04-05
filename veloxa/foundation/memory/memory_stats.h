#ifndef VELOXA_FOUNDATION_MEMORY_MEMORY_STATS_H_
#define VELOXA_FOUNDATION_MEMORY_MEMORY_STATS_H_

#include <atomic>

#include "veloxa/foundation/base/types.h"

namespace vx {

struct MemoryStats {
  std::atomic<i64> current_bytes{0};
  std::atomic<i64> peak_bytes{0};
  std::atomic<i64> total_allocations{0};

  void RecordAlloc(usize bytes) {
    i64 current = current_bytes.fetch_add(static_cast<i64>(bytes),
                                          std::memory_order_relaxed) +
                  static_cast<i64>(bytes);
    i64 peak = peak_bytes.load(std::memory_order_relaxed);
    while (current > peak &&
           !peak_bytes.compare_exchange_weak(peak, current,
                                             std::memory_order_relaxed)) {
    }
    total_allocations.fetch_add(1, std::memory_order_relaxed);
  }

  void RecordDealloc(usize bytes) {
    current_bytes.fetch_sub(static_cast<i64>(bytes), std::memory_order_relaxed);
  }

  void Reset() {
    current_bytes.store(0, std::memory_order_relaxed);
    peak_bytes.store(0, std::memory_order_relaxed);
    total_allocations.store(0, std::memory_order_relaxed);
  }
};

inline MemoryStats& GlobalMemoryStats() {
  static MemoryStats stats;
  return stats;
}

}  // namespace vx

#endif  // VELOXA_FOUNDATION_MEMORY_MEMORY_STATS_H_
