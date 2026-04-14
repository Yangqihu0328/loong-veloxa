#ifndef VELOXA_GRAPHICS_CANVAS_H_
#define VELOXA_GRAPHICS_CANVAS_H_

#include <memory>

#include "veloxa/foundation/strings/string_view.h"
#include "veloxa/graphics/brush.h"
#include "veloxa/graphics/image.h"
#include "veloxa/graphics/path.h"
#include "veloxa/graphics/types.h"

namespace vx::gfx {

class Canvas {
 public:
  virtual ~Canvas() = default;

  virtual void Begin() = 0;
  virtual void End() = 0;
  virtual void Clear(Color color) = 0;

  virtual void FillRect(const Rect& rect, const Brush& brush) = 0;
  virtual void FillRoundedRect(const Rect& rect, vx::f32 radius,
                               const Brush& brush) = 0;
  virtual void FillPath(const Path& path, const Brush& brush) = 0;

  virtual void StrokeRect(const Rect& rect, const Brush& brush,
                          vx::f32 width) = 0;
  virtual void StrokeRoundedRect(const Rect& rect, vx::f32 radius,
                                 const Brush& brush, vx::f32 width) = 0;
  virtual void StrokePath(const Path& path, const Brush& brush,
                          vx::f32 width) = 0;
  virtual void StrokeLine(Point a, Point b, const Brush& brush,
                          vx::f32 width) = 0;

  virtual void DrawText(vx::StringView text, const Rect& bounds,
                        vx::f32 font_size, const Brush& brush) = 0;

  virtual void DrawImage(const Image& image, const Rect& src_rect,
                         const Rect& dst_rect) = 0;

  virtual void PushClipRect(const Rect& rect) = 0;
  virtual void PushClipPath(const Path& path) = 0;
  virtual void PopClip() = 0;

  virtual void PushLayer(const Rect& bounds, vx::f32 opacity) = 0;
  virtual void PopLayer() = 0;

  virtual void SetTransform(const Matrix3x2& m) = 0;
  virtual Matrix3x2 GetTransform() const = 0;
  virtual void PushState() = 0;
  virtual void PopState() = 0;

  virtual std::unique_ptr<Path> CreatePath() = 0;
};

}  // namespace vx::gfx

#endif  // VELOXA_GRAPHICS_CANVAS_H_
