#include "veloxa/core/layout/text_shaper.h"

namespace vx::layout {

TextMetrics SimpleTextShaper::Measure(StringView text, f32 font_size,
                                      u16 /*font_weight*/) {
  f32 char_width = font_size * 0.6f;
  f32 width = static_cast<f32>(text.size()) * char_width;
  f32 height = font_size * 1.2f;
  f32 baseline = font_size;
  return {width, height, baseline};
}

}  // namespace vx::layout
