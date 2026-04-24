#ifndef VELOXA_TEXT_FONT_MANAGER_H_
#define VELOXA_TEXT_FONT_MANAGER_H_

#include "veloxa/foundation/base/status.h"
#include "veloxa/foundation/base/types.h"
#include "veloxa/foundation/strings/string_view.h"
#include "veloxa/text/font_handle.h"  // FontHandle, kInvalidFont
#include "veloxa/text/shape_cache.h"  // ShapeCache, ShapedRun

struct FT_LibraryRec_;
struct FT_FaceRec_;
struct hb_font_t;

namespace vx::text {

class FontManager {
 public:
  FontManager();
  ~FontManager();

  FontManager(const FontManager&) = delete;
  FontManager& operator=(const FontManager&) = delete;

  Status Init();
  void Shutdown();
  bool initialized() const { return ft_library_ != nullptr; }

  StatusOr<FontHandle> LoadFont(StringView path, StringView family);
  FontHandle FindFont(StringView family, u16 weight = 400) const;

  FT_FaceRec_* GetFace(FontHandle handle) const;

  // Idempotent FT_Set_Pixel_Sizes with per-handle size cache. Only invokes
  // the FreeType call when the requested pixel_size differs from the cached
  // value. Returns the FT_Face pointer for chained use, or nullptr if
  // handle is invalid / face missing / FT_Set_Pixel_Sizes fails.
  //
  // Intended to be called BEFORE GetHbFont so that HarfBuzz observes
  // consistent metrics via hb_ft_font_changed on actual size change.
  FT_FaceRec_* SetFacePixelSize(FontHandle handle, u32 pixel_size);

  // Returns (and lazily creates) an hb_font_t* bound to this handle's
  // FT_Face. The same pointer is returned for repeated calls with the
  // same handle; on size change, the cached hb_font_t is reconfigured
  // via hb_ft_font_changed so HarfBuzz re-reads the FT_Face metrics.
  //
  // Caller contract: invoke FT_Set_Pixel_Sizes(face, 0, pixel_size)
  // BEFORE calling this function so HarfBuzz observes the new metrics.
  // Returns nullptr for invalid handle or if the handle has no FT_Face.
  hb_font_t* GetHbFont(FontHandle handle, u32 pixel_size);

  // TASK-20260424-04: shape-or-lookup hot entry point for DrawText. Returns
  // a pointer to a ShapedRun whose lifetime matches the underlying cache
  // slot (see ShapeCache stability contract). Never nullptr on success.
  //
  // On miss: invokes hb_shape via the thread-local hb_buffer, copies glyph
  // infos+positions into a new ShapedRun, inserts it, returns pointer.
  // On hit: direct pointer return, no hb_shape invocation.
  //
  // Behavior is controlled at runtime by the VX_SHAPE_CACHE_OFF environment
  // variable (if set to any non-empty value at process start, lookups are
  // forced miss and inserts are skipped — used for A/B benchmarking the
  // pre-cache baseline).
  //
  // Returns nullptr only if handle is invalid or hb_font acquisition fails;
  // callers must fall back to the legacy path in that case.
  const ShapedRun* ShapeOrLookup(FontHandle handle, u32 pixel_size,
                                  StringView text);

  // Drops all cached shape results. Intended for font reload / unload
  // paths so a stale ShapedRun cannot be returned after a font changes.
  void ClearShapeCache();

  usize font_count() const;

 private:
  struct FontEntry {
    FT_FaceRec_* face = nullptr;
    hb_font_t* hb_font = nullptr;
    u32 hb_pixel_size = 0;
    u32 ft_pixel_size = 0;
    FontHandle handle = kInvalidFont;
    char family[64] = {};
    u16 weight = 400;
  };

  FT_LibraryRec_* ft_library_ = nullptr;
  static constexpr usize kMaxFonts = 32;
  FontEntry fonts_[kMaxFonts] = {};
  usize font_count_ = 0;
  FontHandle next_handle_ = 1;

  // TASK-20260424-04: per-FontManager hb_shape result cache.
  ShapeCache shape_cache_;
};

}  // namespace vx::text

#endif  // VELOXA_TEXT_FONT_MANAGER_H_
