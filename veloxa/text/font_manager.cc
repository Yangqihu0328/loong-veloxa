#include "veloxa/text/font_manager.h"

#include <cstdlib>
#include <cstring>

#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#include <hb-ft.h>

#include "veloxa/foundation/base/hash.h"
#include "veloxa/text/hb_buffer_holder.h"

namespace vx::text {

namespace {

// Latched at first call: reads VX_SHAPE_CACHE_OFF once and caches the
// result. Ensures the flag cannot flip mid-run and that the env lookup
// cost is paid exactly once.
bool ShapeCacheDisabled() {
  static const bool disabled = [] {
    const char* v = std::getenv("VX_SHAPE_CACHE_OFF");
    return v != nullptr && v[0] != '\0';
  }();
  return disabled;
}

}  // namespace

FontManager::FontManager() = default;

FontManager::~FontManager() { Shutdown(); }

Status FontManager::Init() {
  if (ft_library_) {
    return Status::Ok();
  }
  FT_Error err = FT_Init_FreeType(&ft_library_);
  if (err != 0) {
    ft_library_ = nullptr;
    return Status(StatusCode::kInternal, "FT_Init_FreeType failed");
  }
  return Status::Ok();
}

void FontManager::Shutdown() {
  // TASK-20260424-04: invalidate shape results before releasing the
  // underlying fonts, so a subsequent Init can't see a stale ShapedRun
  // if a future FontHandle value collides with a previous one.
  shape_cache_.Clear();
  for (usize i = 0; i < font_count_; ++i) {
    // Destroy hb_font_t before FT_Face — hb_ft_font_create_referenced
    // holds a reference to the FT_Face but freeing the face first is
    // undefined on some HarfBuzz versions.
    if (fonts_[i].hb_font) {
      hb_font_destroy(fonts_[i].hb_font);
      fonts_[i].hb_font = nullptr;
      fonts_[i].hb_pixel_size = 0;
    }
    if (fonts_[i].face) {
      FT_Done_Face(fonts_[i].face);
      fonts_[i].face = nullptr;
      fonts_[i].ft_pixel_size = 0;
    }
  }
  font_count_ = 0;
  if (ft_library_) {
    FT_Done_FreeType(ft_library_);
    ft_library_ = nullptr;
  }
}

StatusOr<FontHandle> FontManager::LoadFont(StringView path,
                                           StringView family) {
  if (!ft_library_) {
    return Status(StatusCode::kInternal, "FontManager not initialized");
  }
  if (font_count_ >= kMaxFonts) {
    return Status(StatusCode::kInternal, "Maximum font count reached");
  }

  // Build null-terminated path string on stack
  char path_buf[512] = {};
  usize len = path.size() < sizeof(path_buf) - 1 ? path.size()
                                                   : sizeof(path_buf) - 1;
  std::memcpy(path_buf, path.data(), len);
  path_buf[len] = '\0';

  FT_Face face = nullptr;
  FT_Error err = FT_New_Face(ft_library_, path_buf, 0, &face);
  if (err != 0) {
    return Status(StatusCode::kNotFound, "Failed to load font");
  }

  FontEntry& entry = fonts_[font_count_];
  entry.face = face;
  entry.handle = next_handle_++;
  entry.weight = 400;

  usize fam_len = family.size() < sizeof(entry.family) - 1
                      ? family.size()
                      : sizeof(entry.family) - 1;
  std::memcpy(entry.family, family.data(), fam_len);
  entry.family[fam_len] = '\0';

  ++font_count_;
  return entry.handle;
}

FontHandle FontManager::FindFont(StringView family, u16 /*weight*/) const {
  for (usize i = 0; i < font_count_; ++i) {
    if (family == StringView(fonts_[i].family)) {
      return fonts_[i].handle;
    }
  }
  return kInvalidFont;
}

FT_FaceRec_* FontManager::GetFace(FontHandle handle) const {
  for (usize i = 0; i < font_count_; ++i) {
    if (fonts_[i].handle == handle) {
      return fonts_[i].face;
    }
  }
  return nullptr;
}

usize FontManager::font_count() const { return font_count_; }

FT_FaceRec_* FontManager::SetFacePixelSize(FontHandle handle, u32 pixel_size) {
  if (handle == kInvalidFont) return nullptr;
  for (usize i = 0; i < font_count_; ++i) {
    if (fonts_[i].handle != handle) continue;
    FontEntry& entry = fonts_[i];
    if (!entry.face) return nullptr;
    if (entry.ft_pixel_size != pixel_size) {
      FT_Error err = FT_Set_Pixel_Sizes(entry.face, 0, pixel_size);
      if (err != 0) return nullptr;
      entry.ft_pixel_size = pixel_size;
    }
    return entry.face;
  }
  return nullptr;
}

void FontManager::ClearShapeCache() { shape_cache_.Clear(); }

const ShapedRun* FontManager::ShapeOrLookup(FontHandle handle, u32 pixel_size,
                                             StringView text) {
  // Cache lookup (skipped when VX_SHAPE_CACHE_OFF=1).
  ShapeCacheKey key{handle, pixel_size,
                    HashBytesU64(reinterpret_cast<const u8*>(text.data()),
                                 text.size()),
                    static_cast<u32>(text.size())};
  if (!ShapeCacheDisabled()) {
    const ShapedRun* hit = shape_cache_.LookupOrNull(key);
    if (hit) return hit;
  }

  // Miss path: run hb_shape on the thread-local buffer, copy results into
  // a new ShapedRun, and insert.
  hb_font_t* hb_font = GetHbFont(handle, pixel_size);
  if (!hb_font) return nullptr;

  hb_buffer_t* buf = AcquireThreadLocalHbBuffer();
  if (!buf) return nullptr;
  hb_buffer_reset(buf);
  hb_buffer_add_utf8(buf, text.data(), static_cast<int>(text.size()), 0, -1);
  hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
  hb_buffer_set_script(buf, HB_SCRIPT_LATIN);
  hb_buffer_guess_segment_properties(buf);
  hb_shape(hb_font, buf, nullptr, 0);

  u32 glyph_count = 0;
  const hb_glyph_info_t* glyph_info =
      hb_buffer_get_glyph_infos(buf, &glyph_count);
  const hb_glyph_position_t* glyph_pos =
      hb_buffer_get_glyph_positions(buf, &glyph_count);

  ShapedRun run;
  run.glyphs.reserve(glyph_count);
  for (u32 i = 0; i < glyph_count; ++i) {
    run.glyphs.push_back(
        ShapedGlyph{glyph_info[i].codepoint,
                    static_cast<f32>(glyph_pos[i].x_offset) / 64.0f,
                    static_cast<f32>(glyph_pos[i].y_offset) / 64.0f,
                    static_cast<f32>(glyph_pos[i].x_advance) / 64.0f});
  }

  if (ShapeCacheDisabled()) {
    // Cache is disabled — return a thread_local copy so the pointer stays
    // valid for the immediate caller but nothing is retained across calls.
    thread_local ShapedRun uncached_buffer;
    uncached_buffer = std::move(run);
    return &uncached_buffer;
  }
  return shape_cache_.Insert(key, std::move(run));
}

hb_font_t* FontManager::GetHbFont(FontHandle handle, u32 pixel_size) {
  if (handle == kInvalidFont) return nullptr;
  for (usize i = 0; i < font_count_; ++i) {
    if (fonts_[i].handle != handle) continue;
    FontEntry& entry = fonts_[i];
    if (!entry.face) return nullptr;

    if (!entry.hb_font) {
      entry.hb_font = hb_ft_font_create_referenced(entry.face);
      entry.hb_pixel_size = pixel_size;
    } else if (entry.hb_pixel_size != pixel_size) {
      // FT_Face has been reconfigured (precondition); tell HarfBuzz to
      // re-read metrics so subsequent hb_shape sees the new size.
      hb_ft_font_changed(entry.hb_font);
      entry.hb_pixel_size = pixel_size;
    }
    return entry.hb_font;
  }
  return nullptr;
}

}  // namespace vx::text
