#include "veloxa/graphics/gles/glyph_atlas.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include "veloxa/text/font_manager.h"
#include "veloxa/text/glyph_cache.h"

namespace vx::gfx::gles {

namespace {
// 1px gutter between packed glyphs avoids linear-filter bleed between
// neighbours sharing a texel row.
constexpr vx::u32 kPad = 1;
}  // namespace

GlyphAtlas::GlyphAtlas(vx::text::FontManager* font_manager,
                       vx::text::GlyphCache* glyph_cache, vx::u32 atlas_width,
                       vx::u32 atlas_height)
    : font_manager_(font_manager),
      glyph_cache_(glyph_cache),
      atlas_width_(atlas_width),
      atlas_height_(atlas_height) {
  CreateTexture();
}

GlyphAtlas::~GlyphAtlas() {
  if (texture_ != 0) {
    glDeleteTextures(1, &texture_);
    texture_ = 0;
  }
}

vx::u64 GlyphAtlas::MakeKey(vx::text::FontHandle font, vx::u32 glyph_id,
                            vx::u32 pixel_size) {
  // font (top bits) | pixel_size (16 bits) | glyph_id (24 bits). glyph_id and
  // pixel_size fit comfortably for any real font/size.
  return (static_cast<vx::u64>(font) << 40) |
         (static_cast<vx::u64>(pixel_size & 0xFFFFu) << 24) |
         static_cast<vx::u64>(glyph_id & 0xFFFFFFu);
}

void GlyphAtlas::CreateTexture() {
  glGenTextures(1, &texture_);
  glBindTexture(GL_TEXTURE_2D, texture_);
  // Single-channel coverage atlas. Initialize to zero so unwritten regions
  // never bleed stale coverage into a glyph quad via linear filtering.
  vx::Vector<vx::u8> zero;
  zero.resize(static_cast<vx::usize>(atlas_width_) * atlas_height_, 0);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, static_cast<GLsizei>(atlas_width_),
               static_cast<GLsizei>(atlas_height_), 0, GL_RED,
               GL_UNSIGNED_BYTE, zero.data());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_2D, 0);
  cursor_x_ = 0;
  cursor_y_ = 0;
  row_height_ = 0;
}

bool GlyphAtlas::PackGlyph(vx::u32 width, vx::u32 height, vx::u32* out_x,
                           vx::u32* out_y) {
  const vx::u32 w = width + kPad;
  const vx::u32 h = height + kPad;
  if (width > atlas_width_ || height > atlas_height_) return false;
  // Advance to a new shelf if the current row cannot fit the glyph.
  if (cursor_x_ + w > atlas_width_) {
    cursor_x_ = 0;
    cursor_y_ += row_height_;
    row_height_ = 0;
  }
  if (cursor_y_ + h > atlas_height_) return false;  // atlas full
  *out_x = cursor_x_;
  *out_y = cursor_y_;
  cursor_x_ += w;
  if (h > row_height_) row_height_ = h;
  return true;
}

GlyphAtlasInfo GlyphAtlas::GetOrUpload(vx::text::FontHandle font,
                                       vx::u32 glyph_id, vx::u32 pixel_size) {
  const vx::u64 key = MakeKey(font, glyph_id, pixel_size);
  if (auto* hit = cache_.Find(key)) {
    ++cache_hits_;
    return *hit;
  }
  ++cache_misses_;

  // CPU bitmap layer: reuse GlyphCache, rasterize via FreeType on miss
  // (mirrors software_canvas.cc DrawText).
  const vx::text::GlyphBitmap* bmp =
      glyph_cache_->Get(font, glyph_id, pixel_size);
  if (bmp == nullptr) {
    FT_FaceRec_* face = font_manager_->SetFacePixelSize(font, pixel_size);
    if (face == nullptr) return {};
    if (FT_Load_Glyph(face, glyph_id, FT_LOAD_DEFAULT) != 0) return {};
    if (FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL) != 0) return {};
    const FT_Bitmap& fb = face->glyph->bitmap;

    vx::text::GlyphBitmap g;
    g.width = fb.width;
    g.height = fb.rows;
    g.bearing_x = face->glyph->bitmap_left;
    g.bearing_y = face->glyph->bitmap_top;
    g.advance = static_cast<vx::f32>(face->glyph->advance.x) / 64.0f;
    g.alpha.reserve(static_cast<vx::usize>(fb.width) * fb.rows);
    for (vx::u32 row = 0; row < fb.rows; ++row) {
      for (vx::u32 col = 0; col < fb.width; ++col) {
        g.alpha.push_back(fb.buffer[row * fb.pitch + col]);
      }
    }
    bmp = glyph_cache_->Put(font, glyph_id, pixel_size,
                            static_cast<vx::text::GlyphBitmap&&>(g));
    if (bmp == nullptr) return {};
  }

  GlyphAtlasInfo info;
  info.bearing_x = bmp->bearing_x;
  info.bearing_y = bmp->bearing_y;
  info.width = static_cast<vx::i32>(bmp->width);
  info.height = static_cast<vx::i32>(bmp->height);
  info.advance = bmp->advance;
  info.valid = true;

  // Whitespace glyphs (e.g. space) carry advance but no coverage — record
  // metrics, skip upload, leave UVs at zero.
  if (bmp->width == 0 || bmp->height == 0) {
    cache_.Insert(key, info);
    return info;
  }

  vx::u32 px = 0, py = 0;
  if (!PackGlyph(bmp->width, bmp->height, &px, &py)) {
    // MVP eviction: clear the whole atlas and retry once. Stale cache entries
    // referencing freed regions are dropped along with it.
    cache_.clear();
    cursor_x_ = cursor_y_ = row_height_ = 0;
    if (!PackGlyph(bmp->width, bmp->height, &px, &py)) {
      info.valid = false;
      return info;
    }
  }

  glBindTexture(GL_TEXTURE_2D, texture_);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexSubImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(px),
                  static_cast<GLint>(py), static_cast<GLsizei>(bmp->width),
                  static_cast<GLsizei>(bmp->height), GL_RED, GL_UNSIGNED_BYTE,
                  bmp->alpha.data());
  glBindTexture(GL_TEXTURE_2D, 0);
  ++uploads_this_frame_;

  const vx::f32 fw = static_cast<vx::f32>(atlas_width_);
  const vx::f32 fh = static_cast<vx::f32>(atlas_height_);
  info.u0 = static_cast<vx::f32>(px) / fw;
  info.v0 = static_cast<vx::f32>(py) / fh;
  info.u1 = static_cast<vx::f32>(px + bmp->width) / fw;
  info.v1 = static_cast<vx::f32>(py + bmp->height) / fh;

  cache_.Insert(key, info);
  return info;
}

void GlyphAtlas::OnContextLost() {
  // GL objects are gone after context loss; do not call glDelete (invalid).
  texture_ = 0;
  cache_.clear();
  cursor_x_ = cursor_y_ = row_height_ = 0;
}

void GlyphAtlas::OnContextRestored() {
  cache_.clear();
  CreateTexture();
}

}  // namespace vx::gfx::gles
