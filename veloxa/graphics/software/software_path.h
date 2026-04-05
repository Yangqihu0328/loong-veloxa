#ifndef VELOXA_GRAPHICS_SOFTWARE_SOFTWARE_PATH_H_
#define VELOXA_GRAPHICS_SOFTWARE_SOFTWARE_PATH_H_

#include "veloxa/foundation/containers/vector.h"
#include "veloxa/graphics/path.h"

namespace vx::gfx::sw {

class SoftwarePath : public Path {
 public:
  enum class CommandType : vx::u8 {
    kMoveTo,
    kLineTo,
    kQuadTo,
    kCubicTo,
    kArcTo,
    kClose
  };

  struct Command {
    CommandType type;
    Point p[3];
    vx::f32 f[3];
  };

  SoftwarePath() = default;

  void MoveTo(Point p) override;
  void LineTo(Point p) override;
  void QuadTo(Point control, Point end) override;
  void CubicTo(Point c1, Point c2, Point end) override;
  void ArcTo(Point center, vx::f32 radius, vx::f32 start_angle,
             vx::f32 sweep) override;
  void Close() override;
  void Reset() override;

  bool IsEmpty() const override;
  Rect Bounds() const override;
  bool Contains(Point p) const override;

  const vx::Vector<Command>& commands() const { return commands_; }

 private:
  vx::Vector<Command> commands_;
};

}  // namespace vx::gfx::sw

#endif  // VELOXA_GRAPHICS_SOFTWARE_SOFTWARE_PATH_H_
