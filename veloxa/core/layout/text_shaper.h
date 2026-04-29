#ifndef VELOXA_CORE_LAYOUT_TEXT_SHAPER_H_
#define VELOXA_CORE_LAYOUT_TEXT_SHAPER_H_

#include "veloxa/foundation/base/types.h"
#include "veloxa/foundation/strings/string_view.h"

namespace vx::layout {

// CSS 2.1 §10.8.1 line-box 计算所需的字体度量。
// R4 #21（TASK-20260426-01）扩展：加 ascent/descent 拆分（FT face metrics
// 直接对应）。`baseline` 字段语义等价 `ascent`，标 `[[deprecated]]` 给未来
// 清理一个明确锚点；新代码请使用 `ascent`。
struct TextMetrics {
  f32 width;
  f32 height;
  // R4 #21 扩展字段（FT face->size->metrics.ascender/descender / 64.0）：
  //   ascent  = baseline 之上的字体上升（正数）
  //   descent = baseline 之下的字体下降（正数；FT descender 为负值，取绝对值）
  // CSS 2.1 §10.8.1 半-leading 公式依赖：line.height = max_ascent + max_descent
  f32 ascent;
  f32 descent;
  // DEPRECATED：与 `ascent` 同语义；保留仅为 ABI 兼容。新代码请用 `ascent`。
  [[deprecated("use ascent")]] f32 baseline;
};

class TextShaper {
 public:
  virtual ~TextShaper() = default;
  virtual TextMetrics Measure(StringView text, f32 font_size,
                              u16 font_weight) = 0;
};

class SimpleTextShaper : public TextShaper {
 public:
  TextMetrics Measure(StringView text, f32 font_size,
                      u16 font_weight) override;
};

}  // namespace vx::layout

#endif  // VELOXA_CORE_LAYOUT_TEXT_SHAPER_H_
