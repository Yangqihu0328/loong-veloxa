#include "veloxa/text/freetype_shaper.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <hb.h>
#include <hb-ft.h>

namespace vx::text {

FreeTypeTextShaper::FreeTypeTextShaper(FontManager* fm)
    : font_manager_(fm) {}

layout::TextMetrics FreeTypeTextShaper::Measure(StringView text,
                                                 f32 font_size,
                                                 u16 font_weight) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  // R4 #21（TASK-20260426-01）TextMetrics 已扩展 ascent/descent；baseline 标
  // [[deprecated]] 但仍需写入以保 ABI 兼容。下方所有 return 通过本 push/pop
  // 段抑制 deprecated-declarations warning。
  if (text.empty()) {
    return {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
  }

  FontHandle fh = default_font_;
  if (fh == kInvalidFont) {
    fh = font_manager_->FindFont("", font_weight);
  }

  FT_Face face = (fh != kInvalidFont) ? font_manager_->GetFace(fh) : nullptr;
  if (!face) {
    // Fallback: simple estimation matching SimpleTextShaper（同 ascent/descent 比例）
    f32 char_width = font_size * 0.6f;
    f32 width = static_cast<f32>(text.size()) * char_width;
    f32 ascent = font_size * 0.8f;
    f32 descent = font_size * 0.2f;
    f32 height = ascent + descent;
    return {width, height, ascent, descent, ascent};
  }

  u32 pixel_size = static_cast<u32>(font_size + 0.5f);
  if (pixel_size == 0) pixel_size = 1;
  FT_Set_Pixel_Sizes(face, 0, pixel_size);

  hb_font_t* hb_font = hb_ft_font_create_referenced(face);
  hb_buffer_t* buf = hb_buffer_create();
  hb_buffer_add_utf8(buf, text.data(), static_cast<int>(text.size()), 0,
                     static_cast<int>(text.size()));
  hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
  hb_buffer_set_script(buf, HB_SCRIPT_LATIN);
  hb_buffer_set_language(buf, hb_language_from_string("en", -1));
  hb_buffer_guess_segment_properties(buf);

  hb_shape(hb_font, buf, nullptr, 0);

  unsigned int glyph_count = 0;
  hb_glyph_position_t* glyph_pos =
      hb_buffer_get_glyph_positions(buf, &glyph_count);

  f32 total_advance = 0.0f;
  for (unsigned int i = 0; i < glyph_count; ++i) {
    total_advance += static_cast<f32>(glyph_pos[i].x_advance);
  }

  f32 width = total_advance / 64.0f;

  FT_Size_Metrics& m = face->size->metrics;
  // FT ascender 为正值（baseline 之上），descender 为负值（baseline 之下）。
  // CSS 2.1 §10.8.1 约定 descent 为正数 → 取绝对值 / 取负号。
  f32 ascent = static_cast<f32>(m.ascender) / 64.0f;
  f32 descent = -static_cast<f32>(m.descender) / 64.0f;
  f32 height = ascent + descent;  // 等价旧式 ascender - descender

  hb_buffer_destroy(buf);
  hb_font_destroy(hb_font);

  return {width, height, ascent, descent, ascent};
#pragma GCC diagnostic pop
}

}  // namespace vx::text
