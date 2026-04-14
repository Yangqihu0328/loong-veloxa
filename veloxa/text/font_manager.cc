#include "veloxa/text/font_manager.h"

#include <cstring>

#include <ft2build.h>
#include FT_FREETYPE_H

namespace vx::text {

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
  for (usize i = 0; i < font_count_; ++i) {
    if (fonts_[i].face) {
      FT_Done_Face(fonts_[i].face);
      fonts_[i].face = nullptr;
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

}  // namespace vx::text
