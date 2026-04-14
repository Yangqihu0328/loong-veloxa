#ifndef VELOXA_TEXT_FONT_MANAGER_H_
#define VELOXA_TEXT_FONT_MANAGER_H_

#include "veloxa/foundation/base/status.h"
#include "veloxa/foundation/base/types.h"
#include "veloxa/foundation/strings/string_view.h"

struct FT_LibraryRec_;
struct FT_FaceRec_;

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
  usize font_count() const;

 private:
  struct FontEntry {
    FT_FaceRec_* face = nullptr;
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
