#ifndef VELOXA_TEXT_FONT_MANAGER_H_
#define VELOXA_TEXT_FONT_MANAGER_H_

#include "veloxa/foundation/base/status.h"
#include "veloxa/foundation/base/types.h"
#include "veloxa/foundation/strings/string_view.h"

struct FT_LibraryRec_;
struct FT_FaceRec_;
struct hb_font_t;

namespace vx::text {

using FontHandle = u32;
static constexpr FontHandle kInvalidFont = 0;

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

  // Returns (and lazily creates) an hb_font_t* bound to this handle's
  // FT_Face. The same pointer is returned for repeated calls with the
  // same handle; on size change, the cached hb_font_t is reconfigured
  // via hb_ft_font_changed so HarfBuzz re-reads the FT_Face metrics.
  //
  // Caller contract: invoke FT_Set_Pixel_Sizes(face, 0, pixel_size)
  // BEFORE calling this function so HarfBuzz observes the new metrics.
  // Returns nullptr for invalid handle or if the handle has no FT_Face.
  hb_font_t* GetHbFont(FontHandle handle, u32 pixel_size);

  usize font_count() const;

 private:
  struct FontEntry {
    FT_FaceRec_* face = nullptr;
    hb_font_t* hb_font = nullptr;
    u32 hb_pixel_size = 0;
    FontHandle handle = kInvalidFont;
    char family[64] = {};
    u16 weight = 400;
  };

  FT_LibraryRec_* ft_library_ = nullptr;
  static constexpr usize kMaxFonts = 32;
  FontEntry fonts_[kMaxFonts] = {};
  usize font_count_ = 0;
  FontHandle next_handle_ = 1;
};

}  // namespace vx::text

#endif  // VELOXA_TEXT_FONT_MANAGER_H_
