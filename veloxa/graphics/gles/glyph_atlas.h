#ifndef VELOXA_GRAPHICS_GLES_GLYPH_ATLAS_H_
#define VELOXA_GRAPHICS_GLES_GLYPH_ATLAS_H_

#include <GLES3/gl3.h>

#include "veloxa/foundation/base/types.h"
#include "veloxa/foundation/containers/hash_map.h"
#include "veloxa/text/font_handle.h"

namespace vx::text {
class FontManager;
class GlyphCache;
}  // namespace vx::text

namespace vx::gfx::gles {

// Per-glyph metadata returned by GlyphAtlas::GetOrUpload. UVs index into the
// GL_R8 atlas texture; bearing/advance mirror FreeType metrics (1/64 px units
// already divided out). `valid == false` means the glyph could not be
// rasterized (missing / FT error) and callers should skip drawing it.
struct GlyphAtlasInfo {
  vx::f32 u0 = 0, v0 = 0, u1 = 0, v1 = 0;
  vx::i32 bearing_x = 0, bearing_y = 0;
  vx::i32 width = 0, height = 0;
  vx::f32 advance = 0;
  bool valid = false;
};

// CPU FreeType rasterization → GL_R8 GPU texture atlas with simple row-pack
// (first-fit shelf). Reuses GlyphCache for the CPU bitmap layer and rasterizes
// on miss (mirrors software_canvas.cc DrawText). Does not own font_manager /
// glyph_cache. Caller must hold the GL context current.
class GlyphAtlas {
 public:
  GlyphAtlas(vx::text::FontManager* font_manager,
             vx::text::GlyphCache* glyph_cache, vx::u32 atlas_width = 1024,
             vx::u32 atlas_height = 1024);
  ~GlyphAtlas();

  GlyphAtlas(const GlyphAtlas&) = delete;
  GlyphAtlas& operator=(const GlyphAtlas&) = delete;

  // Returns cached info or rasterizes+uploads on miss. `glyph_id` is the
  // post-shaping FreeType glyph index (NOT a Unicode codepoint); `pixel_size`
  // is the integer FT pixel size.
  GlyphAtlasInfo GetOrUpload(vx::text::FontHandle font, vx::u32 glyph_id,
                             vx::u32 pixel_size);

  GLuint texture_id() const { return texture_; }
  vx::u32 atlas_width() const { return atlas_width_; }
  vx::u32 atlas_height() const { return atlas_height_; }

  void OnContextLost();
  void OnContextRestored();

  vx::u64 cache_hits() const { return cache_hits_; }
  vx::u64 cache_misses() const { return cache_misses_; }
  vx::u64 uploads_this_frame() const { return uploads_this_frame_; }
  void ResetFrameCounters() { uploads_this_frame_ = 0; }

 private:
  bool PackGlyph(vx::u32 width, vx::u32 height, vx::u32* out_x, vx::u32* out_y);
  void CreateTexture();
  static vx::u64 MakeKey(vx::text::FontHandle font, vx::u32 glyph_id,
                         vx::u32 pixel_size);

  vx::text::FontManager* font_manager_ = nullptr;
  vx::text::GlyphCache* glyph_cache_ = nullptr;
  GLuint texture_ = 0;
  vx::u32 atlas_width_;
  vx::u32 atlas_height_;
  vx::HashMap<vx::u64, GlyphAtlasInfo> cache_;
  vx::u32 cursor_x_ = 0;
  vx::u32 cursor_y_ = 0;
  vx::u32 row_height_ = 0;
  vx::u64 cache_hits_ = 0;
  vx::u64 cache_misses_ = 0;
  vx::u64 uploads_this_frame_ = 0;
};

}  // namespace vx::gfx::gles

#endif  // VELOXA_GRAPHICS_GLES_GLYPH_ATLAS_H_
