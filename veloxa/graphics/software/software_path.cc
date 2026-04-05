#include "veloxa/graphics/software/software_path.h"

#include <algorithm>
#include <limits>

namespace vx::gfx::sw {

void SoftwarePath::MoveTo(Point p) {
  commands_.push_back({CommandType::kMoveTo, {p, {0, 0}, {0, 0}}, {0, 0, 0}});
}

void SoftwarePath::LineTo(Point p) {
  commands_.push_back({CommandType::kLineTo, {p, {0, 0}, {0, 0}}, {0, 0, 0}});
}

void SoftwarePath::QuadTo(Point control, Point end) {
  commands_.push_back(
      {CommandType::kQuadTo, {control, end, {0, 0}}, {0, 0, 0}});
}

void SoftwarePath::CubicTo(Point c1, Point c2, Point end) {
  commands_.push_back({CommandType::kCubicTo, {c1, c2, end}, {0, 0, 0}});
}

void SoftwarePath::ArcTo(Point center, vx::f32 radius, vx::f32 start_angle,
                         vx::f32 sweep) {
  commands_.push_back(
      {CommandType::kArcTo, {center, {0, 0}, {0, 0}}, {radius, start_angle, sweep}});
}

void SoftwarePath::Close() {
  commands_.push_back(
      {CommandType::kClose, {{0, 0}, {0, 0}, {0, 0}}, {0, 0, 0}});
}

void SoftwarePath::Reset() { commands_.clear(); }

bool SoftwarePath::IsEmpty() const { return commands_.empty(); }

Rect SoftwarePath::Bounds() const {
  if (commands_.empty()) {
    return {0, 0, 0, 0};
  }

  vx::f32 min_x = std::numeric_limits<vx::f32>::max();
  vx::f32 min_y = std::numeric_limits<vx::f32>::max();
  vx::f32 max_x = std::numeric_limits<vx::f32>::lowest();
  vx::f32 max_y = std::numeric_limits<vx::f32>::lowest();

  auto expand = [&](Point pt) {
    min_x = std::min(min_x, pt.x);
    min_y = std::min(min_y, pt.y);
    max_x = std::max(max_x, pt.x);
    max_y = std::max(max_y, pt.y);
  };

  for (vx::usize i = 0; i < commands_.size(); ++i) {
    const auto& cmd = commands_[i];
    switch (cmd.type) {
      case CommandType::kMoveTo:
      case CommandType::kLineTo:
        expand(cmd.p[0]);
        break;
      case CommandType::kQuadTo:
        expand(cmd.p[0]);
        expand(cmd.p[1]);
        break;
      case CommandType::kCubicTo:
        expand(cmd.p[0]);
        expand(cmd.p[1]);
        expand(cmd.p[2]);
        break;
      case CommandType::kArcTo: {
        vx::f32 r = cmd.f[0];
        expand({cmd.p[0].x - r, cmd.p[0].y - r});
        expand({cmd.p[0].x + r, cmd.p[0].y + r});
        break;
      }
      case CommandType::kClose:
        break;
    }
  }

  return {min_x, min_y, max_x - min_x, max_y - min_y};
}

bool SoftwarePath::Contains(Point p) const {
  vx::Vector<Point> vertices;
  vx::Vector<vx::usize> subpath_starts;

  for (vx::usize i = 0; i < commands_.size(); ++i) {
    const auto& cmd = commands_[i];
    switch (cmd.type) {
      case CommandType::kMoveTo:
        subpath_starts.push_back(vertices.size());
        vertices.push_back(cmd.p[0]);
        break;
      case CommandType::kLineTo:
        vertices.push_back(cmd.p[0]);
        break;
      case CommandType::kQuadTo:
        vertices.push_back(cmd.p[0]);
        vertices.push_back(cmd.p[1]);
        break;
      case CommandType::kCubicTo:
        vertices.push_back(cmd.p[0]);
        vertices.push_back(cmd.p[1]);
        vertices.push_back(cmd.p[2]);
        break;
      case CommandType::kArcTo:
        vertices.push_back(cmd.p[0]);
        break;
      case CommandType::kClose:
        break;
    }
  }

  if (vertices.empty()) {
    return false;
  }

  subpath_starts.push_back(vertices.size());

  vx::i32 count = 0;

  for (vx::usize s = 0; s + 1 < subpath_starts.size(); ++s) {
    vx::usize start = subpath_starts[s];
    vx::usize end = subpath_starts[s + 1];
    vx::usize n = end - start;
    if (n < 2) continue;

    for (vx::usize i = 0; i < n; ++i) {
      const Point& v0 = vertices[start + i];
      const Point& v1 = vertices[start + (i + 1) % n];

      vx::f32 y_min = std::min(v0.y, v1.y);
      vx::f32 y_max = std::max(v0.y, v1.y);

      if (p.y < y_min || p.y >= y_max) continue;

      vx::f32 x_intersect =
          v0.x + (p.y - v0.y) / (v1.y - v0.y) * (v1.x - v0.x);
      if (p.x < x_intersect) {
        ++count;
      }
    }
  }

  return (count % 2) != 0;
}

}  // namespace vx::gfx::sw
