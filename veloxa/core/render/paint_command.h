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
    kFillRoundedRect,
    kDrawText,
    kPushClipRect,
    kPopClip,
    kPushLayer,
    kPopLayer,
    kStrokeRoundedRect,
    kDrawImage,
    // DevTool Inspector hover highlight (TASK-20260502-01 A.0.4 + A5).
    // 业务 Renderer 不会发出此命令；DevTool 子系统专用。
    // T5 mitigation：每帧通过 ResetOverlayCommands() 清除，防止跨帧累积污染
    // 或被业务 DOM/JS 通过 PaintCommand 反向构造。
    kOverlayHighlight,
  };

  Type type;
  gfx::Rect rect;
  gfx::Color color;
  f32 param = 0;
  f32 param2 = 0;
  StringView text;
  u32 image_handle = 0;

  static PaintCommand FillRect(const gfx::Rect& r, gfx::Color c) {
    return {Type::kFillRect, r, c, 0, 0, {}, 0};
  }

  static PaintCommand FillRoundedRect(const gfx::Rect& r, f32 radius,
                                      gfx::Color c) {
    return {Type::kFillRoundedRect, r, c, radius, 0, {}, 0};
  }

  static PaintCommand StrokeRoundedRect(const gfx::Rect& r, f32 radius,
                                        gfx::Color c, f32 stroke_width) {
    return {Type::kStrokeRoundedRect, r, c, radius, stroke_width, {}, 0};
  }

  static PaintCommand DrawText(StringView t, const gfx::Rect& r,
                                f32 font_size, gfx::Color c) {
    return {Type::kDrawText, r, c, font_size, 0, t, 0};
  }

  static PaintCommand PushClipRect(const gfx::Rect& r) {
    return {Type::kPushClipRect, r, {}, 0, 0, {}, 0};
  }

  static PaintCommand PopClip() {
    return {Type::kPopClip, {}, {}, 0, 0, {}, 0};
  }

  static PaintCommand PushLayer(const gfx::Rect& r, f32 opacity) {
    return {Type::kPushLayer, r, {}, opacity, 0, {}, 0};
  }

  static PaintCommand PopLayer() {
    return {Type::kPopLayer, {}, {}, 0, 0, {}, 0};
  }

  static PaintCommand DrawImage(u32 img_handle, const gfx::Rect& dst) {
    PaintCommand cmd{};
    cmd.type = Type::kDrawImage;
    cmd.rect = dst;
    cmd.image_handle = img_handle;
    return cmd;
  }

  // DevTool Inspector hover highlight: stroke a rect at `r` with `c` and
  // `stroke_width`. See Type::kOverlayHighlight comment for T5 lifecycle.
  static PaintCommand OverlayHighlight(const gfx::Rect& r, gfx::Color c,
                                        f32 stroke_width) {
    return {Type::kOverlayHighlight, r, c, stroke_width, 0, {}, 0};
  }

  // TASK-20260502-02 B.2.3 — DevTool Performance Overlay dirty rect 边框.
  // 复用 kOverlayHighlight type (T5 mitigation 协议复用 — ResetOverlayCommands
  // 同时清 hover + dirty rect overlays). 默认黄绿色区分 hover red, stroke 1px
  // 比 hover 2px 细避免视觉冲突 (creative #1 决策 4).
  static PaintCommand OverlayDirtyRect(
      const gfx::Rect& r,
      gfx::Color c = gfx::Color::FromRGBA(255, 255, 0, 200),
      f32 stroke_width = 1.0f) {
    return {Type::kOverlayHighlight, r, c, stroke_width, 0, {}, 0};
  }

  bool operator==(const PaintCommand& other) const {
    return type == other.type && rect == other.rect &&
           color == other.color && param == other.param &&
           param2 == other.param2 && text == other.text &&
           image_handle == other.image_handle;
  }
  bool operator!=(const PaintCommand& other) const {
    return !(*this == other);
  }
};

using DisplayList = Vector<PaintCommand>;

}  // namespace vx::render

#endif  // VELOXA_CORE_RENDER_PAINT_COMMAND_H_
