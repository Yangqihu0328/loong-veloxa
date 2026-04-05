#ifndef VELOXA_GRAPHICS_PATH_H_
#define VELOXA_GRAPHICS_PATH_H_

#include "veloxa/graphics/types.h"

namespace vx::gfx {

class Path {
 public:
  virtual ~Path() = default;

  virtual void MoveTo(Point p) = 0;
  virtual void LineTo(Point p) = 0;
  virtual void QuadTo(Point control, Point end) = 0;
  virtual void CubicTo(Point c1, Point c2, Point end) = 0;
  virtual void ArcTo(Point center, vx::f32 radius, vx::f32 start_angle,
                     vx::f32 sweep) = 0;
  virtual void Close() = 0;
  virtual void Reset() = 0;

  virtual bool IsEmpty() const = 0;
  virtual Rect Bounds() const = 0;
  virtual bool Contains(Point p) const = 0;
};

}  // namespace vx::gfx

#endif  // VELOXA_GRAPHICS_PATH_H_
