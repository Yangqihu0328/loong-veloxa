#ifndef VELOXA_GRAPHICS_TYPES_H_
#define VELOXA_GRAPHICS_TYPES_H_

#include <algorithm>
#include <cmath>

#include "veloxa/foundation/base/types.h"

namespace vx::gfx {

struct Color {
  vx::u8 r, g, b, a;

  static constexpr Color FromRGBA(vx::u8 r, vx::u8 g, vx::u8 b,
                                  vx::u8 a = 255) {
    return {r, g, b, a};
  }

  static constexpr Color Transparent() { return {0, 0, 0, 0}; }
  static constexpr Color Black() { return {0, 0, 0, 255}; }
  static constexpr Color White() { return {255, 255, 255, 255}; }
  static constexpr Color Red() { return {255, 0, 0, 255}; }
  static constexpr Color Green() { return {0, 255, 0, 255}; }
  static constexpr Color Blue() { return {0, 0, 255, 255}; }

  constexpr bool operator==(Color other) const {
    return r == other.r && g == other.g && b == other.b && a == other.a;
  }

  constexpr bool operator!=(Color other) const { return !(*this == other); }

  // Pixel memory layout: R in bits[0:7], G in bits[8:15], B in bits[16:23], A in bits[24:31].
  // This is the canonical pixel format for all Veloxa rendering and storage.
  constexpr vx::u32 ToRGBA32() const {
    return static_cast<vx::u32>(r) | (static_cast<vx::u32>(g) << 8) |
           (static_cast<vx::u32>(b) << 16) | (static_cast<vx::u32>(a) << 24);
  }

  static constexpr Color FromRGBA32(vx::u32 packed) {
    return {static_cast<vx::u8>(packed & 0xFF),
            static_cast<vx::u8>((packed >> 8) & 0xFF),
            static_cast<vx::u8>((packed >> 16) & 0xFF),
            static_cast<vx::u8>((packed >> 24) & 0xFF)};
  }
};

struct Point {
  vx::f32 x, y;

  constexpr Point operator+(Point other) const {
    return {x + other.x, y + other.y};
  }

  constexpr Point operator-(Point other) const {
    return {x - other.x, y - other.y};
  }

  constexpr Point operator*(vx::f32 s) const { return {x * s, y * s}; }

  constexpr bool operator==(Point other) const {
    return x == other.x && y == other.y;
  }

  constexpr bool operator!=(Point other) const { return !(*this == other); }
};

struct Rect {
  vx::f32 x, y, w, h;

  constexpr vx::f32 right() const { return x + w; }
  constexpr vx::f32 bottom() const { return y + h; }

  constexpr bool Contains(Point p) const {
    return p.x >= x && p.x < right() && p.y >= y && p.y < bottom();
  }

  constexpr bool IsEmpty() const { return w <= 0 || h <= 0; }

  Rect Intersect(const Rect& other) const {
    vx::f32 ix = std::max(x, other.x);
    vx::f32 iy = std::max(y, other.y);
    vx::f32 ir = std::min(right(), other.right());
    vx::f32 ib = std::min(bottom(), other.bottom());
    vx::f32 iw = ir - ix;
    vx::f32 ih = ib - iy;
    if (iw <= 0 || ih <= 0) {
      return {0, 0, 0, 0};
    }
    return {ix, iy, iw, ih};
  }

  constexpr bool operator==(const Rect& other) const {
    return x == other.x && y == other.y && w == other.w && h == other.h;
  }

  constexpr bool operator!=(const Rect& other) const {
    return !(*this == other);
  }
};

struct Matrix3x2 {
  vx::f32 m[6];

  static Matrix3x2 Identity() { return {{1, 0, 0, 1, 0, 0}}; }

  static Matrix3x2 Translation(vx::f32 tx, vx::f32 ty) {
    return {{1, 0, 0, 1, tx, ty}};
  }

  static Matrix3x2 Scale(vx::f32 sx, vx::f32 sy) {
    return {{sx, 0, 0, sy, 0, 0}};
  }

  static Matrix3x2 Rotation(vx::f32 radians) {
    vx::f32 c = std::cos(radians);
    vx::f32 s = std::sin(radians);
    return {{c, s, -s, c, 0, 0}};
  }

  Point Apply(Point p) const {
    return {m[0] * p.x + m[2] * p.y + m[4],
            m[1] * p.x + m[3] * p.y + m[5]};
  }

  Matrix3x2 Multiply(const Matrix3x2& o) const {
    return {{m[0] * o.m[0] + m[2] * o.m[1], m[1] * o.m[0] + m[3] * o.m[1],
             m[0] * o.m[2] + m[2] * o.m[3], m[1] * o.m[2] + m[3] * o.m[3],
             m[0] * o.m[4] + m[2] * o.m[5] + m[4],
             m[1] * o.m[4] + m[3] * o.m[5] + m[5]}};
  }
};

}  // namespace vx::gfx

#endif  // VELOXA_GRAPHICS_TYPES_H_
