#include "veloxa/graphics/software/rasterizer.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

struct Segment {
  vx::gfx::Point a, b;
};

void FlattenQuadCollect(vx::Vector<Segment>& segments, vx::gfx::Point p0,
                        vx::gfx::Point ctrl, vx::gfx::Point p2, int depth) {
  if (depth >= 16) {
    segments.push_back({p0, p2});
    return;
  }
  vx::gfx::Point mid = {(p0.x + p2.x) * 0.5f, (p0.y + p2.y) * 0.5f};
  vx::f32 dx = ctrl.x - mid.x;
  vx::f32 dy = ctrl.y - mid.y;
  if (dx * dx + dy * dy < 0.0625f) {
    segments.push_back({p0, p2});
    return;
  }
  vx::gfx::Point q0 = {(p0.x + ctrl.x) * 0.5f, (p0.y + ctrl.y) * 0.5f};
  vx::gfx::Point q1 = {(ctrl.x + p2.x) * 0.5f, (ctrl.y + p2.y) * 0.5f};
  vx::gfx::Point q2 = {(q0.x + q1.x) * 0.5f, (q0.y + q1.y) * 0.5f};
  FlattenQuadCollect(segments, p0, q0, q2, depth + 1);
  FlattenQuadCollect(segments, q2, q1, p2, depth + 1);
}

void FlattenCubicCollect(vx::Vector<Segment>& segments, vx::gfx::Point p0,
                         vx::gfx::Point c1, vx::gfx::Point c2,
                         vx::gfx::Point p3, int depth) {
  if (depth >= 16) {
    segments.push_back({p0, p3});
    return;
  }
  vx::f32 dx = p3.x - p0.x;
  vx::f32 dy = p3.y - p0.y;
  vx::f32 d1, d2;
  vx::f32 len_sq = dx * dx + dy * dy;
  if (len_sq > 1e-10f) {
    vx::f32 inv_len = 1.0f / std::sqrt(len_sq);
    vx::f32 nx = -dy * inv_len;
    vx::f32 ny = dx * inv_len;
    d1 = std::abs((c1.x - p0.x) * nx + (c1.y - p0.y) * ny);
    d2 = std::abs((c2.x - p0.x) * nx + (c2.y - p0.y) * ny);
  } else {
    d1 = std::sqrt((c1.x - p0.x) * (c1.x - p0.x) +
                   (c1.y - p0.y) * (c1.y - p0.y));
    d2 = std::sqrt((c2.x - p0.x) * (c2.x - p0.x) +
                   (c2.y - p0.y) * (c2.y - p0.y));
  }
  if (std::max(d1, d2) < 0.25f) {
    segments.push_back({p0, p3});
    return;
  }
  vx::gfx::Point p01 = {(p0.x + c1.x) * 0.5f, (p0.y + c1.y) * 0.5f};
  vx::gfx::Point p12 = {(c1.x + c2.x) * 0.5f, (c1.y + c2.y) * 0.5f};
  vx::gfx::Point p23 = {(c2.x + p3.x) * 0.5f, (c2.y + p3.y) * 0.5f};
  vx::gfx::Point p012 = {(p01.x + p12.x) * 0.5f, (p01.y + p12.y) * 0.5f};
  vx::gfx::Point p123 = {(p12.x + p23.x) * 0.5f, (p12.y + p23.y) * 0.5f};
  vx::gfx::Point pmid = {(p012.x + p123.x) * 0.5f,
                          (p012.y + p123.y) * 0.5f};
  FlattenCubicCollect(segments, p0, p01, p012, pmid, depth + 1);
  FlattenCubicCollect(segments, pmid, p123, p23, p3, depth + 1);
}

}  // namespace

namespace vx::gfx::sw {

Rasterizer::Rasterizer(vx::u32* pixels, vx::u32 width, vx::u32 height,
                       vx::u32 stride)
    : pixels_(pixels),
      width_(width),
      height_(height),
      stride_(stride),
      current_point_{0, 0},
      subpath_start_{0, 0} {}

vx::u32 Rasterizer::BlendSrcOver(vx::u32 dst, vx::u32 src) {
  vx::u32 sa = (src >> 24) & 0xFF;
  if (sa == 0) return dst;
  if (sa == 255) return src;
  vx::u32 inv_sa = 255 - sa;
  vx::u32 sr = src & 0xFF;
  vx::u32 sg = (src >> 8) & 0xFF;
  vx::u32 sb = (src >> 16) & 0xFF;
  vx::u32 dr = dst & 0xFF;
  vx::u32 dg = (dst >> 8) & 0xFF;
  vx::u32 db = (dst >> 16) & 0xFF;
  vx::u32 da = (dst >> 24) & 0xFF;
  vx::u32 out_r = (sr * sa + dr * inv_sa + 127) / 255;
  vx::u32 out_g = (sg * sa + dg * inv_sa + 127) / 255;
  vx::u32 out_b = (sb * sa + db * inv_sa + 127) / 255;
  vx::u32 out_a = sa + (da * inv_sa + 127) / 255;
  if (out_a > 255) out_a = 255;
  return out_r | (out_g << 8) | (out_b << 16) | (out_a << 24);
}

void Rasterizer::FillRect(const Rect& rect, const Brush& brush,
                          const Rect& clip, const Matrix3x2& transform) {
  bool axis_aligned = (transform.m[1] == 0.0f && transform.m[2] == 0.0f);

  if (axis_aligned) {
    Point tl = transform.Apply({rect.x, rect.y});
    Point br = transform.Apply({rect.right(), rect.bottom()});
    Rect transformed = {std::min(tl.x, br.x), std::min(tl.y, br.y),
                        std::abs(br.x - tl.x), std::abs(br.y - tl.y)};
    Rect clipped = transformed.Intersect(clip);
    clipped = clipped.Intersect(
        {0, 0, static_cast<vx::f32>(width_), static_cast<vx::f32>(height_)});
    if (clipped.IsEmpty()) return;

    vx::i32 x0 = static_cast<vx::i32>(std::floor(clipped.x));
    vx::i32 y0 = static_cast<vx::i32>(std::floor(clipped.y));
    vx::i32 x1 = static_cast<vx::i32>(std::ceil(clipped.right()));
    vx::i32 y1 = static_cast<vx::i32>(std::ceil(clipped.bottom()));
    x0 = std::max(x0, static_cast<vx::i32>(0));
    y0 = std::max(y0, static_cast<vx::i32>(0));
    x1 = std::min(x1, static_cast<vx::i32>(width_));
    y1 = std::min(y1, static_cast<vx::i32>(height_));

    vx::u32 stride_px = stride_ / 4;
    for (vx::i32 y = y0; y < y1; ++y) {
      for (vx::i32 x = x0; x < x1; ++x) {
        Color c = brush.Sample(
            {static_cast<vx::f32>(x) + 0.5f, static_cast<vx::f32>(y) + 0.5f});
        vx::u32 src = c.ToRGBA32();
        vx::u32 idx =
            static_cast<vx::u32>(y) * stride_px + static_cast<vx::u32>(x);
        pixels_[idx] = BlendSrcOver(pixels_[idx], src);
      }
    }
  } else {
    SoftwarePath path;
    path.MoveTo({rect.x, rect.y});
    path.LineTo({rect.right(), rect.y});
    path.LineTo({rect.right(), rect.bottom()});
    path.LineTo({rect.x, rect.bottom()});
    path.Close();
    FillPath(path, brush, clip, transform);
  }
}

void Rasterizer::GenerateEdges(const SoftwarePath& path,
                               const Matrix3x2& transform) {
  edges_.clear();
  current_point_ = {0, 0};
  subpath_start_ = {0, 0};

  for (vx::usize i = 0; i < path.commands().size(); ++i) {
    const auto& cmd = path.commands()[i];
    switch (cmd.type) {
      case SoftwarePath::CommandType::kMoveTo:
        current_point_ = transform.Apply(cmd.p[0]);
        subpath_start_ = current_point_;
        break;
      case SoftwarePath::CommandType::kLineTo: {
        Point next = transform.Apply(cmd.p[0]);
        AddLineEdge(current_point_, next);
        current_point_ = next;
        break;
      }
      case SoftwarePath::CommandType::kQuadTo: {
        Point ctrl = transform.Apply(cmd.p[0]);
        Point end = transform.Apply(cmd.p[1]);
        FlattenQuad(current_point_, ctrl, end, 0);
        current_point_ = end;
        break;
      }
      case SoftwarePath::CommandType::kCubicTo: {
        Point c1 = transform.Apply(cmd.p[0]);
        Point c2 = transform.Apply(cmd.p[1]);
        Point end = transform.Apply(cmd.p[2]);
        FlattenCubic(current_point_, c1, c2, end, 0);
        current_point_ = end;
        break;
      }
      case SoftwarePath::CommandType::kArcTo: {
        Point center = cmd.p[0];
        vx::f32 radius = cmd.f[0];
        vx::f32 start_angle = cmd.f[1];
        vx::f32 sweep = cmd.f[2];
        if (std::abs(sweep) < 1e-6f) break;
        int n_segs = std::max(8, static_cast<int>(std::abs(sweep) * 4.0f));
        Point prev = current_point_;
        for (int j = 1; j <= n_segs; ++j) {
          vx::f32 angle =
              start_angle +
              sweep * static_cast<vx::f32>(j) / static_cast<vx::f32>(n_segs);
          Point local = {center.x + radius * std::cos(angle),
                         center.y + radius * std::sin(angle)};
          Point transformed = transform.Apply(local);
          AddLineEdge(prev, transformed);
          prev = transformed;
        }
        current_point_ = prev;
        break;
      }
      case SoftwarePath::CommandType::kClose:
        AddLineEdge(current_point_, subpath_start_);
        current_point_ = subpath_start_;
        break;
    }
  }
}

void Rasterizer::AddLineEdge(Point from, Point to) {
  if (std::abs(from.y - to.y) < 1e-6f) return;
  if (from.y < to.y) {
    edges_.push_back({from.x, from.y, to.x, to.y, 1});
  } else {
    edges_.push_back({to.x, to.y, from.x, from.y, -1});
  }
}

void Rasterizer::FlattenQuad(Point p0, Point ctrl, Point p2, int depth) {
  if (depth >= 16) {
    AddLineEdge(p0, p2);
    return;
  }
  Point mid = {(p0.x + p2.x) * 0.5f, (p0.y + p2.y) * 0.5f};
  vx::f32 dx = ctrl.x - mid.x;
  vx::f32 dy = ctrl.y - mid.y;
  if (dx * dx + dy * dy < 0.0625f) {
    AddLineEdge(p0, p2);
    return;
  }
  Point q0 = {(p0.x + ctrl.x) * 0.5f, (p0.y + ctrl.y) * 0.5f};
  Point q1 = {(ctrl.x + p2.x) * 0.5f, (ctrl.y + p2.y) * 0.5f};
  Point q2 = {(q0.x + q1.x) * 0.5f, (q0.y + q1.y) * 0.5f};
  FlattenQuad(p0, q0, q2, depth + 1);
  FlattenQuad(q2, q1, p2, depth + 1);
}

void Rasterizer::FlattenCubic(Point p0, Point c1, Point c2, Point p3,
                              int depth) {
  if (depth >= 16) {
    AddLineEdge(p0, p3);
    return;
  }
  vx::f32 dx = p3.x - p0.x;
  vx::f32 dy = p3.y - p0.y;
  vx::f32 d1, d2;
  vx::f32 len_sq = dx * dx + dy * dy;
  if (len_sq > 1e-10f) {
    vx::f32 inv_len = 1.0f / std::sqrt(len_sq);
    vx::f32 nx = -dy * inv_len;
    vx::f32 ny = dx * inv_len;
    d1 = std::abs((c1.x - p0.x) * nx + (c1.y - p0.y) * ny);
    d2 = std::abs((c2.x - p0.x) * nx + (c2.y - p0.y) * ny);
  } else {
    d1 = std::sqrt((c1.x - p0.x) * (c1.x - p0.x) +
                   (c1.y - p0.y) * (c1.y - p0.y));
    d2 = std::sqrt((c2.x - p0.x) * (c2.x - p0.x) +
                   (c2.y - p0.y) * (c2.y - p0.y));
  }
  if (std::max(d1, d2) < 0.25f) {
    AddLineEdge(p0, p3);
    return;
  }
  Point p01 = {(p0.x + c1.x) * 0.5f, (p0.y + c1.y) * 0.5f};
  Point p12 = {(c1.x + c2.x) * 0.5f, (c1.y + c2.y) * 0.5f};
  Point p23 = {(c2.x + p3.x) * 0.5f, (c2.y + p3.y) * 0.5f};
  Point p012 = {(p01.x + p12.x) * 0.5f, (p01.y + p12.y) * 0.5f};
  Point p123 = {(p12.x + p23.x) * 0.5f, (p12.y + p23.y) * 0.5f};
  Point pmid = {(p012.x + p123.x) * 0.5f, (p012.y + p123.y) * 0.5f};
  FlattenCubic(p0, p01, p012, pmid, depth + 1);
  FlattenCubic(pmid, p123, p23, p3, depth + 1);
}

void Rasterizer::FillPath(const SoftwarePath& path, const Brush& brush,
                          const Rect& clip, const Matrix3x2& transform) {
  GenerateEdges(path, transform);
  if (edges_.empty()) return;
  RenderScanlines(brush, clip);
}

void Rasterizer::RenderScanlines(const Brush& brush, const Rect& clip) {
  if (edges_.empty()) return;

  vx::f32 y_min_f = edges_[0].y_top;
  vx::f32 y_max_f = edges_[0].y_bottom;
  for (vx::usize i = 1; i < edges_.size(); ++i) {
    y_min_f = std::min(y_min_f, edges_[i].y_top);
    y_max_f = std::max(y_max_f, edges_[i].y_bottom);
  }
  y_min_f = std::max(y_min_f, clip.y);
  y_max_f = std::min(y_max_f, clip.bottom());
  y_min_f = std::max(y_min_f, 0.0f);
  y_max_f = std::min(y_max_f, static_cast<vx::f32>(height_));

  vx::i32 y_start = static_cast<vx::i32>(std::floor(y_min_f));
  vx::i32 y_end = static_cast<vx::i32>(std::ceil(y_max_f));
  y_start = std::max(y_start, static_cast<vx::i32>(0));
  y_end = std::min(y_end, static_cast<vx::i32>(height_));
  if (y_start >= y_end) return;

  std::sort(edges_.begin(), edges_.end(),
            [](const Edge& a, const Edge& b) { return a.y_top < b.y_top; });

  coverage_.resize(width_);
  active_edges_.clear();
  vx::usize edge_idx = 0;

  for (vx::i32 y = y_start; y < y_end; ++y) {
    std::memset(coverage_.data(), 0, width_ * sizeof(vx::f32));

    vx::f32 yf = static_cast<vx::f32>(y);
    while (edge_idx < edges_.size() &&
           std::floor(edges_[edge_idx].y_top) <= yf) {
      active_edges_.push_back(edges_[edge_idx]);
      ++edge_idx;
    }

    vx::f32 y_next = yf + 1.0f;
    for (vx::usize i = 0; i < active_edges_.size(); ++i) {
      ComputeEdgeCoverage(active_edges_[i], yf, y_next);
    }

    AccumulateAndBlend(y, brush, clip);

    vx::usize write = 0;
    for (vx::usize i = 0; i < active_edges_.size(); ++i) {
      if (active_edges_[i].y_bottom > y_next) {
        if (write != i) {
          active_edges_[write] = active_edges_[i];
        }
        ++write;
      }
    }
    active_edges_.resize(write);
  }
}

void Rasterizer::ComputeEdgeCoverage(const Edge& edge, vx::f32 y,
                                     vx::f32 y_next) {
  vx::f32 y_start = std::max(edge.y_top, y);
  vx::f32 y_end = std::min(edge.y_bottom, y_next);
  if (y_end <= y_start) return;

  vx::f32 height = y_end - y_start;
  vx::f32 total_dy = edge.y_bottom - edge.y_top;
  vx::f32 x_start =
      edge.x_top + (y_start - edge.y_top) / total_dy *
                        (edge.x_bottom - edge.x_top);
  vx::f32 x_end =
      edge.x_top +
      (y_end - edge.y_top) / total_dy * (edge.x_bottom - edge.x_top);

  vx::f32 x_mid = (x_start + x_end) * 0.5f;
  vx::i32 ix = static_cast<vx::i32>(std::floor(x_mid));
  if (ix >= 0 && ix < static_cast<vx::i32>(width_)) {
    coverage_[static_cast<vx::usize>(ix)] += height * edge.direction;
  }
}

void Rasterizer::AccumulateAndBlend(vx::i32 y, const Brush& brush,
                                    const Rect& clip) {
  if (y < 0 || y >= static_cast<vx::i32>(height_)) return;

  vx::i32 x_min = std::max(static_cast<vx::i32>(0),
                            static_cast<vx::i32>(clip.x));
  vx::i32 x_max = std::min(static_cast<vx::i32>(width_),
                            static_cast<vx::i32>(std::ceil(clip.right())));

  vx::f32 running = 0.0f;
  vx::u32 stride_px = stride_ / 4;

  for (vx::i32 x = 0; x < static_cast<vx::i32>(width_); ++x) {
    running += coverage_[static_cast<vx::usize>(x)];
    if (x < x_min || x >= x_max) continue;

    vx::f32 alpha = std::min(std::abs(running), 1.0f);
    if (alpha < 1.0f / 256.0f) continue;

    Color brush_color = brush.Sample(
        {static_cast<vx::f32>(x) + 0.5f, static_cast<vx::f32>(y) + 0.5f});
    vx::u8 a = static_cast<vx::u8>(alpha * brush_color.a);
    if (a == 0) continue;

    vx::u32 src = static_cast<vx::u32>(brush_color.r) |
                  (static_cast<vx::u32>(brush_color.g) << 8) |
                  (static_cast<vx::u32>(brush_color.b) << 16) |
                  (static_cast<vx::u32>(a) << 24);
    vx::u32 dst_idx =
        static_cast<vx::u32>(y) * stride_px + static_cast<vx::u32>(x);
    pixels_[dst_idx] = BlendSrcOver(pixels_[dst_idx], src);
  }
}

void Rasterizer::StrokePath(const SoftwarePath& path, const Brush& brush,
                            vx::f32 stroke_width, const Rect& clip,
                            const Matrix3x2& transform) {
  vx::f32 half_w = stroke_width * 0.5f;
  Point current = {0, 0};
  Point sub_start = {0, 0};

  vx::Vector<Segment> segments;

  for (vx::usize i = 0; i < path.commands().size(); ++i) {
    const auto& cmd = path.commands()[i];
    switch (cmd.type) {
      case SoftwarePath::CommandType::kMoveTo:
        current = transform.Apply(cmd.p[0]);
        sub_start = current;
        break;
      case SoftwarePath::CommandType::kLineTo: {
        Point next = transform.Apply(cmd.p[0]);
        segments.push_back({current, next});
        current = next;
        break;
      }
      case SoftwarePath::CommandType::kQuadTo: {
        Point ctrl = transform.Apply(cmd.p[0]);
        Point end = transform.Apply(cmd.p[1]);
        FlattenQuadCollect(segments, current, ctrl, end, 0);
        current = end;
        break;
      }
      case SoftwarePath::CommandType::kCubicTo: {
        Point c1 = transform.Apply(cmd.p[0]);
        Point c2 = transform.Apply(cmd.p[1]);
        Point end = transform.Apply(cmd.p[2]);
        FlattenCubicCollect(segments, current, c1, c2, end, 0);
        current = end;
        break;
      }
      case SoftwarePath::CommandType::kArcTo: {
        Point center = cmd.p[0];
        vx::f32 radius = cmd.f[0];
        vx::f32 start_angle = cmd.f[1];
        vx::f32 sweep = cmd.f[2];
        if (std::abs(sweep) < 1e-6f) break;
        int n_segs = std::max(8, static_cast<int>(std::abs(sweep) * 4.0f));
        Point prev = current;
        for (int j = 1; j <= n_segs; ++j) {
          vx::f32 angle =
              start_angle +
              sweep * static_cast<vx::f32>(j) / static_cast<vx::f32>(n_segs);
          Point local = {center.x + radius * std::cos(angle),
                         center.y + radius * std::sin(angle)};
          Point next = transform.Apply(local);
          segments.push_back({prev, next});
          prev = next;
        }
        current = prev;
        break;
      }
      case SoftwarePath::CommandType::kClose:
        segments.push_back({current, sub_start});
        current = sub_start;
        break;
    }
  }

  for (vx::usize i = 0; i < segments.size(); ++i) {
    Point a = segments[i].a;
    Point b = segments[i].b;
    vx::f32 dx = b.x - a.x;
    vx::f32 dy = b.y - a.y;
    vx::f32 len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-6f) continue;

    vx::f32 nx = -dy / len * half_w;
    vx::f32 ny = dx / len * half_w;

    SoftwarePath quad;
    quad.MoveTo({a.x + nx, a.y + ny});
    quad.LineTo({b.x + nx, b.y + ny});
    quad.LineTo({b.x - nx, b.y - ny});
    quad.LineTo({a.x - nx, a.y - ny});
    quad.Close();

    FillPath(quad, brush, clip, Matrix3x2::Identity());
  }
}

}  // namespace vx::gfx::sw
