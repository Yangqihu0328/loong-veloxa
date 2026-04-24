// veloxa/text/shape_cache.h
//
// TASK-20260424-04: fixed-capacity FIFO cache for hb_shape results.
//
// Owned by FontManager (single-threaded by techContext convention); hot-path
// API is LookupOrNull + Insert. Linear scan of up to 128 entries; prefetcher-
// friendly; faster than a HashMap at this small N.
//
// Stability contract (critical — see design §6.2):
//   entries_ is pre-allocated to kCapacity at construction and never resized.
//   Returned ShapedRun* remains valid until the next FIFO eviction that
//   overwrites the same slot, or Clear(), or cache destruction. Callers must
//   not persist pointers across DrawText invocations.
//
// Key schema:
//   { FontHandle, pixel_size, u64 text_fingerprint, u32 text_len }
//   Both fingerprint and text_len must match to count as a hit. The text_len
//   component guards against fingerprint collisions (combined collision
//   probability ~ 2^-96).

#ifndef VELOXA_TEXT_SHAPE_CACHE_H_
#define VELOXA_TEXT_SHAPE_CACHE_H_

#include "veloxa/foundation/base/types.h"
#include "veloxa/foundation/containers/vector.h"
#include "veloxa/text/font_handle.h"

namespace vx::text {

struct ShapedGlyph {
  u32 glyph_id;
  f32 x_offset;
  f32 y_offset;
  f32 x_advance;
};

struct ShapedRun {
  Vector<ShapedGlyph> glyphs;
};

struct ShapeCacheKey {
  FontHandle font;
  u32 pixel_size;
  u64 text_fingerprint;
  u32 text_len;
};

inline bool operator==(const ShapeCacheKey& a, const ShapeCacheKey& b) {
  return a.font == b.font && a.pixel_size == b.pixel_size &&
         a.text_fingerprint == b.text_fingerprint && a.text_len == b.text_len;
}

class ShapeCache {
 public:
  static constexpr usize kCapacity = 128;

  ShapeCache();
  ShapeCache(const ShapeCache&) = delete;
  ShapeCache& operator=(const ShapeCache&) = delete;

  // Returns pointer to stored ShapedRun if key matches an occupied slot,
  // else nullptr. O(kCapacity) linear scan.
  const ShapedRun* LookupOrNull(const ShapeCacheKey& key) const;

  // Inserts (run) under (key); if cache is full, FIFO-evicts the oldest
  // slot. Returns pointer to the newly stored ShapedRun, valid per the
  // stability contract above.
  ShapedRun* Insert(ShapeCacheKey key, ShapedRun run);

  // Marks all slots unoccupied and clears their glyph vectors. Preserves
  // the pre-allocated entries_ capacity (no realloc). Call on font reload
  // or unload.
  void Clear();

  usize size() const { return count_; }

 private:
  struct Entry {
    ShapeCacheKey key{};
    ShapedRun run{};
    bool occupied = false;
  };

  Vector<Entry> entries_;  // size() == kCapacity after construction
  usize head_ = 0;          // index of next slot to overwrite when full
  usize count_ = 0;         // number of occupied slots
};

}  // namespace vx::text

#endif  // VELOXA_TEXT_SHAPE_CACHE_H_
