#include "veloxa/text/glyph_cache.h"

namespace vx::text {

const GlyphBitmap* GlyphCache::Get(FontHandle font, u32 glyph_id,
                                    u32 pixel_size) const {
  Key key{font, glyph_id, pixel_size};
  auto* ptr = entries_.Find(key);
  return ptr;
}

void GlyphCache::Put(FontHandle font, u32 glyph_id, u32 pixel_size,
                      GlyphBitmap bitmap) {
  Key key{font, glyph_id, pixel_size};
  entries_.Insert(key, static_cast<GlyphBitmap&&>(bitmap));
}

}  // namespace vx::text
