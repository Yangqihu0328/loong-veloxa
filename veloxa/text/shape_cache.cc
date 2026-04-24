// veloxa/text/shape_cache.cc
//
// TASK-20260424-04 Phase 2 implementation. See shape_cache.h for the
// stability + collision-guard contract.

#include "veloxa/text/shape_cache.h"

#include <utility>

namespace vx::text {

ShapeCache::ShapeCache() {
  entries_.reserve(kCapacity);
  for (usize i = 0; i < kCapacity; ++i) {
    entries_.push_back(Entry{});
  }
}

const ShapedRun* ShapeCache::LookupOrNull(const ShapeCacheKey& key) const {
  // Compare fingerprint first as the most discriminating field; short-circuit
  // on the hot-miss path.
  for (usize i = 0; i < kCapacity; ++i) {
    const Entry& e = entries_[i];
    if (!e.occupied) continue;
    if (e.key.text_fingerprint == key.text_fingerprint &&
        e.key.text_len == key.text_len && e.key.font == key.font &&
        e.key.pixel_size == key.pixel_size) {
      return &e.run;
    }
  }
  return nullptr;
}

ShapedRun* ShapeCache::Insert(ShapeCacheKey key, ShapedRun run) {
  Entry& slot = entries_[head_];
  if (!slot.occupied) {
    ++count_;
  }
  slot.key = key;
  slot.run = std::move(run);
  slot.occupied = true;
  head_ = (head_ + 1) % kCapacity;
  return &slot.run;
}

void ShapeCache::Clear() {
  for (usize i = 0; i < kCapacity; ++i) {
    entries_[i].occupied = false;
    entries_[i].run.glyphs.clear();
  }
  head_ = 0;
  count_ = 0;
}

}  // namespace vx::text
