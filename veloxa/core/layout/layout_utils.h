#ifndef VELOXA_CORE_LAYOUT_LAYOUT_UTILS_H_
#define VELOXA_CORE_LAYOUT_LAYOUT_UTILS_H_

#include "veloxa/core/css/computed_style.h"
#include "veloxa/core/css/css_value.h"
#include "veloxa/core/css/selector.h"
#include "veloxa/foundation/base/types.h"

namespace vx::layout {

class TextShaper;

struct LayoutContext {
  TextShaper* text_shaper = nullptr;
  const Vector<css::Stylesheet>* stylesheets = nullptr;
  f32 viewport_width = 0;
  f32 viewport_height = 0;
  f32 root_font_size = 16.0f;
};

f32 ResolveLength(const css::LengthValue& len, f32 containing_size,
                  f32 font_size, const LayoutContext& ctx);

void ResolveBoxModel(const css::ComputedStyle& style,
                     f32 containing_width, f32 font_size,
                     const LayoutContext& ctx,
                     f32 padding[4], f32 border_widths[4],
                     f32 margins[4]);

}  // namespace vx::layout

#endif  // VELOXA_CORE_LAYOUT_LAYOUT_UTILS_H_
