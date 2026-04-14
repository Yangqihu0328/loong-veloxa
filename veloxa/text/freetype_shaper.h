#ifndef VELOXA_TEXT_FREETYPE_SHAPER_H_
#define VELOXA_TEXT_FREETYPE_SHAPER_H_

#include "veloxa/core/layout/text_shaper.h"
#include "veloxa/text/font_manager.h"

namespace vx::text {

class FreeTypeTextShaper : public layout::TextShaper {
 public:
  explicit FreeTypeTextShaper(FontManager* fm);

  layout::TextMetrics Measure(StringView text, f32 font_size,
                               u16 font_weight) override;

  void set_default_font(FontHandle font) { default_font_ = font; }

 private:
  FontManager* font_manager_;
  FontHandle default_font_ = kInvalidFont;
};

}  // namespace vx::text

#endif  // VELOXA_TEXT_FREETYPE_SHAPER_H_
