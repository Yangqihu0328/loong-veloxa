#ifndef VELOXA_FOUNDATION_MEMORY_POOL_ALLOCATOR_H_
#define VELOXA_FOUNDATION_MEMORY_POOL_ALLOCATOR_H_

#include <cstdlib>
#include <cstring>

#include "veloxa/foundation/base/assert.h"
#include "veloxa/foundation/base/types.h"

namespace vx {

class PoolAllocator {
 public:
  explicit PoolAllocator(usize block_size, usize blocks_per_chunk = 64)
      : block_size_(block_size < sizeof(FreeNode) ? sizeof(FreeNode) : block_size),
        blocks_per_chunk_(blocks_per_chunk),
        free_list_(nullptr),
        chunks_(nullptr) {
    VX_DCHECK(block_size > 0);
    VX_DCHECK(blocks_per_chunk > 0);
    AllocateChunk();
  }

  ~PoolAllocator() {
    Chunk* chunk = chunks_;
    while (chunk) {
      Chunk* next = chunk->next;
      std::free(chunk);
      chunk = next;
    }
  }

  PoolAllocator(const PoolAllocator&) = delete;
  PoolAllocator& operator=(const PoolAllocator&) = delete;

  void* Allocate(usize size, usize /*align*/ = alignof(std::max_align_t)) {
    VX_DCHECK(size <= block_size_);
    if (!free_list_) {
      AllocateChunk();
    }
    FreeNode* node = free_list_;
    free_list_ = node->next;
    return static_cast<void*>(node);
  }

  void Deallocate(void* ptr, usize /*size*/) {
    if (!ptr) return;
    auto* node = static_cast<FreeNode*>(ptr);
    node->next = free_list_;
    free_list_ = node;
  }

  usize block_size() const { return block_size_; }

 private:
  struct FreeNode {
    FreeNode* next;
  };

  struct Chunk {
    Chunk* next;
  };

  void AllocateChunk() {
    usize chunk_data_size = block_size_ * blocks_per_chunk_;
    void* mem = std::malloc(sizeof(Chunk) + chunk_data_size);
    VX_CHECK(mem != nullptr) << "PoolAllocator: allocation failed";

    auto* chunk = static_cast<Chunk*>(mem);
    chunk->next = chunks_;
    chunks_ = chunk;

    char* data = reinterpret_cast<char*>(chunk) + sizeof(Chunk);
    for (usize i = 0; i < blocks_per_chunk_; ++i) {
      auto* node = reinterpret_cast<FreeNode*>(data + i * block_size_);
      node->next = free_list_;
      free_list_ = node;
    }
  }

  usize block_size_;
  usize blocks_per_chunk_;
  FreeNode* free_list_;
  Chunk* chunks_;
};

}  // namespace vx

#endif  // VELOXA_FOUNDATION_MEMORY_POOL_ALLOCATOR_H_
