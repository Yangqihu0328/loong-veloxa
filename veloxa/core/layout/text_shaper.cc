#include "veloxa/core/layout/text_shaper.h"

namespace vx::layout {

TextMetrics SimpleTextShaper::Measure(StringView text, f32 font_size,
                                      u16 /*font_weight*/) {
  // R4 #21 扩展：拆分 ascent/descent（CSS 2.1 §10.8.1）。
  // SimpleTextShaper 无真实字体 → 用经验比例（与 SDL2/Web typical 字体一致）：
  //   ascent  ≈ font_size × 0.8（baseline 之上）
  //   descent ≈ font_size × 0.2（baseline 之下，正数）
  // height = ascent + descent = font_size × 1.0；line-box 半-leading 仍能
  // 在外层 line-height 大于 height 时撑高，与 FT 路径一致。
  const f32 char_width = font_size * 0.6f;
  const f32 width = static_cast<f32>(text.size()) * char_width;
  const f32 ascent = font_size * 0.8f;
  const f32 descent = font_size * 0.2f;
  const f32 height = ascent + descent;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  // baseline 是 deprecated 兼容字段（== ascent）；构造期写入需局部抑制 warning。
  return TextMetrics{width, height, ascent, descent, ascent};
#pragma GCC diagnostic pop
}

}  // namespace vx::layout
