#ifndef VELOXA_GRAPHICS_SOFTWARE_RASTERIZER_H_
#define VELOXA_GRAPHICS_SOFTWARE_RASTERIZER_H_

#include "veloxa/foundation/containers/vector.h"
#include "veloxa/graphics/brush.h"
#include "veloxa/graphics/software/software_path.h"
#include "veloxa/graphics/types.h"

namespace vx::gfx::sw {

struct Edge {
  vx::f32 x_top, y_top;
  vx::f32 x_bottom, y_bottom;
  vx::i8 direction;
};

class Rasterizer {
 public:
  Rasterizer(vx::u32* pixels, vx::u32 width, vx::u32 height, vx::u32 stride);

  void FillPath(const SoftwarePath& path, const Brush& brush,
                const Rect& clip, const Matrix3x2& transform);
  void StrokePath(const SoftwarePath& path, const Brush& brush,
                  vx::f32 stroke_width,
                  const Rect& clip, const Matrix3x2& transform);
  void FillRect(const Rect& rect, const Brush& brush,
                const Rect& clip, const Matrix3x2& transform);

  static vx::u32 BlendSrcOver(vx::u32 dst, vx::u32 src);

 private:
  void GenerateEdges(const SoftwarePath& path, const Matrix3x2& transform);
  void FlattenQuad(Point p0, Point ctrl, Point p2, int depth = 0);
  void FlattenCubic(Point p0, Point c1, Point c2, Point p3, int depth = 0);
  void AddLineEdge(Point from, Point to);
  void RenderScanlines(const Brush& brush, const Rect& clip);
  void ComputeEdgeCoverage(const Edge& edge, vx::f32 y, vx::f32 y_next);
  void AccumulateAndBlend(vx::i32 y, const Brush& brush, const Rect& clip);

  vx::u32* pixels_;
  vx::u32 width_, height_, stride_;

  vx::Vector<Edge> edges_;
  vx::Vector<Edge> active_edges_;
  vx::Vector<vx::f32> coverage_;
  Point current_point_;
  Point subpath_start_;
};

}  // namespace vx::gfx::sw

#endif  // VELOXA_GRAPHICS_SOFTWARE_RASTERIZER_H_
