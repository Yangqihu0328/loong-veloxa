#ifndef VELOXA_CORE_RENDER_PAINT_COMMAND_H_
#define VELOXA_CORE_RENDER_PAINT_COMMAND_H_

#include "veloxa/foundation/base/types.h"
#include "veloxa/foundation/containers/vector.h"
#include "veloxa/foundation/strings/string_view.h"
#include "veloxa/graphics/types.h"

namespace vx::render {

struct PaintCommand {
  enum class Type : u8 {
    kFillRect,
    kDrawText,
    kPushClipRect,
    kPopClip,
    kPushLayer,
    kPopLayer,
  };

  Type type;
  gfx::Rect rect;
  gfx::Color color;
  f32 param = 0;
  StringView text;

  static PaintCommand FillRect(const gfx::Rect& r, gfx::Color c) {
    return {Type::kFillRect, r, c, 0, {}};
  }

  static PaintCommand DrawText(StringView t, const gfx::Rect& r,
                                f32 font_size, gfx::Color c) {
    return {Type::kDrawText, r, c, font_size, t};
  }

  static PaintCommand PushClipRect(const gfx::Rect& r) {
    return {Type::kPushClipRect, r, {}, 0, {}};
  }

  static PaintCommand PopClip() {
    return {Type::kPopClip, {}, {}, 0, {}};
  }

  static PaintCommand PushLayer(const gfx::Rect& r, f32 opacity) {
    return {Type::kPushLayer, r, {}, opacity, {}};
  }

  static PaintCommand PopLayer() {
    return {Type::kPopLayer, {}, {}, 0, {}};
  }

  bool operator==(const PaintCommand& other) const {
    return type == other.type && rect == other.rect &&
           color == other.color && param == other.param && text == other.text;
  }
  bool operator!=(const PaintCommand& other) const {
    return !(*this == other);
  }
};

using DisplayList = Vector<PaintCommand>;

}  // namespace vx::render

#endif  // VELOXA_CORE_RENDER_PAINT_COMMAND_H_
