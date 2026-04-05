#ifndef VELOXA_CORE_LAYOUT_TEXT_SHAPER_H_
#define VELOXA_CORE_LAYOUT_TEXT_SHAPER_H_

#include "veloxa/foundation/base/types.h"
#include "veloxa/foundation/strings/string_view.h"

namespace vx::layout {

struct TextMetrics {
  f32 width;
  f32 height;
  f32 baseline;
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
