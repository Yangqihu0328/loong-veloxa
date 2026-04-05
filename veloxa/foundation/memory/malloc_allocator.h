#ifndef VELOXA_FOUNDATION_MEMORY_MALLOC_ALLOCATOR_H_
#define VELOXA_FOUNDATION_MEMORY_MALLOC_ALLOCATOR_H_

#include <cstdlib>
#include <new>

#include "veloxa/foundation/base/assert.h"
#include "veloxa/foundation/base/types.h"
#include "veloxa/foundation/memory/memory_stats.h"

namespace vx {

class MallocAllocator {
 public:
  void* Allocate(usize size, usize align = alignof(std::max_align_t)) {
    VX_DCHECK(size > 0);
    VX_DCHECK((align & (align - 1)) == 0);  // power of two
    void* ptr = std::aligned_alloc(align < sizeof(void*) ? sizeof(void*) : align,
                                   (size + align - 1) & ~(align - 1));
    if (!ptr) {
      std::abort();
    }
    GlobalMemoryStats().RecordAlloc(size);
    return ptr;
  }

  void Deallocate(void* ptr, usize size) {
    if (ptr) {
      GlobalMemoryStats().RecordDealloc(size);
      std::free(ptr);
    }
  }
};

inline MallocAllocator& DefaultAllocator() {
  static MallocAllocator alloc;
  return alloc;
}

}  // namespace vx

#endif  // VELOXA_FOUNDATION_MEMORY_MALLOC_ALLOCATOR_H_
