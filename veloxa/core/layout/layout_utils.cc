#include "veloxa/core/layout/layout_utils.h"

namespace vx::layout {

f32 ResolveLength(const css::LengthValue& len, f32 containing_size,
                  f32 font_size, const LayoutContext& ctx) {
  switch (len.unit) {
    case css::Unit::kPx:
      return len.value;
    case css::Unit::kPercent:
      return len.value / 100.0f * containing_size;
    case css::Unit::kEm:
      return len.value * font_size;
    case css::Unit::kRem:
      return len.value * ctx.root_font_size;
    case css::Unit::kVw:
      return len.value / 100.0f * ctx.viewport_width;
    case css::Unit::kVh:
      return len.value / 100.0f * ctx.viewport_height;
    case css::Unit::kAuto:
    case css::Unit::kNone:
    case css::Unit::kNumber:
      return 0.0f;
  }
  return 0.0f;
}

void ResolveBoxModel(const css::ComputedStyle& style,
                     f32 containing_width, f32 font_size,
                     const LayoutContext& ctx,
                     f32 padding[4], f32 border_widths[4],
                     f32 margins[4]) {
  padding[0] = ResolveLength(style.padding_top, containing_width,
                              font_size, ctx);
  padding[1] = ResolveLength(style.padding_right, containing_width,
                              font_size, ctx);
  padding[2] = ResolveLength(style.padding_bottom, containing_width,
                              font_size, ctx);
  padding[3] = ResolveLength(style.padding_left, containing_width,
                              font_size, ctx);

  border_widths[0] =
      style.border_style[0] != css::BorderStyle::kNone
          ? ResolveLength(style.border_width[0], containing_width,
                          font_size, ctx)
          : 0.0f;
  border_widths[1] =
      style.border_style[1] != css::BorderStyle::kNone
          ? ResolveLength(style.border_width[1], containing_width,
                          font_size, ctx)
          : 0.0f;
  border_widths[2] =
      style.border_style[2] != css::BorderStyle::kNone
          ? ResolveLength(style.border_width[2], containing_width,
                          font_size, ctx)
          : 0.0f;
  border_widths[3] =
      style.border_style[3] != css::BorderStyle::kNone
          ? ResolveLength(style.border_width[3], containing_width,
                          font_size, ctx)
          : 0.0f;

  margins[0] = ResolveLength(style.margin_top, containing_width,
                              font_size, ctx);
  margins[1] = ResolveLength(style.margin_right, containing_width,
                              font_size, ctx);
  margins[2] = ResolveLength(style.margin_bottom, containing_width,
                              font_size, ctx);
  margins[3] = ResolveLength(style.margin_left, containing_width,
                              font_size, ctx);
}

}  // namespace vx::layout
