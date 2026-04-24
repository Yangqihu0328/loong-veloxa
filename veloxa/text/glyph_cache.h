#ifndef VELOXA_TEXT_GLYPH_CACHE_H_
#define VELOXA_TEXT_GLYPH_CACHE_H_

#include "veloxa/foundation/base/types.h"
#include "veloxa/foundation/containers/hash_map.h"
#include "veloxa/foundation/containers/vector.h"
#include "veloxa/text/font_manager.h"

namespace vx::text {

struct GlyphBitmap {
  Vector<u8> alpha;
  u32 width = 0;
  u32 height = 0;
  i32 bearing_x = 0;
  i32 bearing_y = 0;
  f32 advance = 0;
};

class GlyphCache {
 public:
  const GlyphBitmap* Get(FontHandle font, u32 glyph_id, u32 pixel_size) const;
  // Inserts (or overwrites) a cache entry and returns a pointer to the stored
  // GlyphBitmap. This removes the need for callers to perform a follow-up
  // Get() lookup after population — saving one hash map query per glyph on
  // first-miss paths. Returns nullptr only on allocation failure (not
  // currently produced by the underlying HashMap).
  GlyphBitmap* Put(FontHandle font, u32 glyph_id, u32 pixel_size,
                   GlyphBitmap bitmap);
  usize size() const { return entries_.size(); }
  void Clear() { entries_.clear(); }

 private:
  struct Key {
    FontHandle font;
    u32 glyph_id;
    u32 pixel_size;

    bool operator==(const Key& other) const {
      return font == other.font && glyph_id == other.glyph_id &&
             pixel_size == other.pixel_size;
    }
  };

  struct KeyHash {
    usize operator()(const Key& k) const {
      return static_cast<usize>(k.font) * 2654435761u ^
             static_cast<usize>(k.glyph_id) * 40503u ^
             static_cast<usize>(k.pixel_size);
    }
  };

  HashMap<Key, GlyphBitmap, KeyHash> entries_;
};

}  // namespace vx::text

#endif  // VELOXA_TEXT_GLYPH_CACHE_H_
