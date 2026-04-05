#ifndef VELOXA_GRAPHICS_BRUSH_H_
#define VELOXA_GRAPHICS_BRUSH_H_

#include <algorithm>
#include <cmath>

#include "veloxa/graphics/types.h"

namespace vx::gfx {

struct Brush {
  enum class Type : vx::u8 { kSolid, kLinearGradient };

  Type type;

  struct LinearGradient {
    Point start, end;
    Color color_start, color_end;
  };

  union {
    Color solid;
    LinearGradient linear;
  };

  static Brush Solid(Color c) {
    Brush b;
    b.type = Type::kSolid;
    b.solid = c;
    return b;
  }

  static Brush Linear(Point start, Point end, Color c0, Color c1) {
    Brush b;
    b.type = Type::kLinearGradient;
    b.linear = {start, end, c0, c1};
    return b;
  }

  Color Sample(Point p) const {
    if (type == Type::kSolid) {
      return solid;
    }

    vx::f32 dx = linear.end.x - linear.start.x;
    vx::f32 dy = linear.end.y - linear.start.y;
    vx::f32 len_sq = dx * dx + dy * dy;
    if (len_sq < 1e-12f) {
      return linear.color_start;
    }

    vx::f32 px = p.x - linear.start.x;
    vx::f32 py = p.y - linear.start.y;
    vx::f32 t = (px * dx + py * dy) / len_sq;
    t = std::max(0.0f, std::min(1.0f, t));

    auto lerp = [](vx::u8 a, vx::u8 b, vx::f32 t) -> vx::u8 {
      return static_cast<vx::u8>(a + (b - a) * t + 0.5f);
    };

    return {lerp(linear.color_start.r, linear.color_end.r, t),
            lerp(linear.color_start.g, linear.color_end.g, t),
            lerp(linear.color_start.b, linear.color_end.b, t),
            lerp(linear.color_start.a, linear.color_end.a, t)};
  }
};

}  // namespace vx::gfx

#endif  // VELOXA_GRAPHICS_BRUSH_H_
