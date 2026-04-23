#ifndef VELOXA_FOUNDATION_MEMORY_ARENA_ALLOCATOR_H_
#define VELOXA_FOUNDATION_MEMORY_ARENA_ALLOCATOR_H_

#include <cstdlib>

#include "veloxa/foundation/base/assert.h"
#include "veloxa/foundation/base/types.h"

namespace vx {

class ArenaAllocator {
 public:
  // Default block size: bumped 4096 → 32768 by TASK-20260424-01.
  //
  // 4 KB blocks caused malloc/free churn in LayoutEngine hot path: at
  // N=256 each Layout() call allocated ~37 blocks from the arena → 37
  // malloc+free pairs per iteration, producing a super-linear knee
  // (BM_LayoutBuildTreeFlat/128→256 = 9.42× for 2× N, baseline block=4096).
  //
  // Phase 2 block-size sweep (benchmarks/baseline + Phase 2 experiment log):
  //   block | R256  | R_flex
  //    4096 | 9.42× | 16.49× (baseline)
  //    8192 | 6.14× | 10.62×
  //   16384 | 4.66× |  8.31×
  //   32768 | 3.84× |  7.40× ← optimum on Flex (65536 regresses Flex)
  //   65536 | 3.61× |  8.36×
  //
  // 32768 cuts the knee ~2.5× on both bench families at a cost of +28 KB
  // per Document::arena_ (only Document uses the default; UpdateManager,
  // LayoutEngine::Layout static arena, and benches now keep 8192 explicit
  // or accept the new default). See
  // docs/specs/2026-04-24-layout-knee-root-cause-design.md §5.
  //
  // The remaining ~40% of super-linearity (R256 still 3.84× vs linear 2×)
  // is tracked as TASK-20260424-02 for Phase 1B per-phase BM investigation.
  explicit ArenaAllocator(usize block_size = 32768)
      : default_block_size_(block_size), current_(nullptr), bytes_allocated_(0) {
    current_ = AllocateBlock(default_block_size_);
  }

  ~ArenaAllocator() { FreeAllBlocks(); }

  ArenaAllocator(const ArenaAllocator&) = delete;
  ArenaAllocator& operator=(const ArenaAllocator&) = delete;

  void* Allocate(usize size, usize align = alignof(std::max_align_t)) {
    VX_DCHECK(size > 0);
    VX_DCHECK((align & (align - 1)) == 0);

    uintptr_t current_ptr =
        reinterpret_cast<uintptr_t>(BlockData(current_)) + current_->used;
    uintptr_t aligned = (current_ptr + align - 1) & ~(align - 1);
    usize padding = aligned - current_ptr;
    usize total = padding + size;

    if (current_->used + total <= current_->size) {
      current_->used += total;
      bytes_allocated_ += size;
      return reinterpret_cast<void*>(aligned);
    }

    usize new_block_size = size > default_block_size_ ? size + align : default_block_size_;
    Block* new_block = AllocateBlock(new_block_size);
    new_block->next = current_;
    current_ = new_block;

    current_ptr = reinterpret_cast<uintptr_t>(BlockData(current_));
    aligned = (current_ptr + align - 1) & ~(align - 1);
    padding = aligned - current_ptr;
    current_->used = padding + size;
    bytes_allocated_ += size;
    return reinterpret_cast<void*>(aligned);
  }

  void Deallocate(void* /*ptr*/, usize /*size*/) {
    // no-op: arena frees everything at once
  }

  void Reset() {
    Block* block = current_;
    Block* first = nullptr;

    // Find the first (oldest) block and free the rest
    while (block) {
      Block* next = block->next;
      if (!next) {
        first = block;
      } else {
        std::free(block);
      }
      block = next;
    }

    if (first) {
      first->used = 0;
      first->next = nullptr;
      current_ = first;
    }
    bytes_allocated_ = 0;
  }

  usize bytes_allocated() const { return bytes_allocated_; }

 private:
  struct alignas(std::max_align_t) Block {
    Block* next;
    usize size;
    usize used;
  };

  static char* BlockData(Block* block) {
    return reinterpret_cast<char*>(block) + sizeof(Block);
  }

  static Block* AllocateBlock(usize data_size) {
    void* mem = std::malloc(sizeof(Block) + data_size);
    VX_CHECK(mem != nullptr) << "ArenaAllocator: allocation failed";
    auto* block = static_cast<Block*>(mem);
    block->next = nullptr;
    block->size = data_size;
    block->used = 0;
    return block;
  }

  void FreeAllBlocks() {
    Block* block = current_;
    while (block) {
      Block* next = block->next;
      std::free(block);
      block = next;
    }
    current_ = nullptr;
  }

  usize default_block_size_;
  Block* current_;
  usize bytes_allocated_;
};

}  // namespace vx

#endif  // VELOXA_FOUNDATION_MEMORY_ARENA_ALLOCATOR_H_
