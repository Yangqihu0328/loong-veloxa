#include "veloxa/graphics/gles/glyph_atlas.h"

namespace vx::gfx::gles {

// RED-phase stub. Real implementation lands in Phase 1B GREEN.

GlyphAtlas::GlyphAtlas(vx::text::FontManager* font_manager,
                       vx::text::GlyphCache* glyph_cache, vx::u32 atlas_width,
                       vx::u32 atlas_height)
    : font_manager_(font_manager),
      glyph_cache_(glyph_cache),
      atlas_width_(atlas_width),
      atlas_height_(atlas_height) {}

GlyphAtlas::~GlyphAtlas() = default;

GlyphAtlasInfo GlyphAtlas::GetOrUpload(vx::text::FontHandle, vx::u32, vx::u32) {
  return {};
}

bool GlyphAtlas::PackGlyph(vx::u32, vx::u32, vx::u32*, vx::u32*) { return false; }

void GlyphAtlas::CreateTexture() {}

vx::u64 GlyphAtlas::MakeKey(vx::text::FontHandle, vx::u32, vx::u32) { return 0; }

void GlyphAtlas::OnContextLost() {}

void GlyphAtlas::OnContextRestored() {}

}  // namespace vx::gfx::gles
