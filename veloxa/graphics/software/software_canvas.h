#ifndef VELOXA_GRAPHICS_SOFTWARE_SOFTWARE_CANVAS_H_
#define VELOXA_GRAPHICS_SOFTWARE_SOFTWARE_CANVAS_H_

#include <memory>

#include "veloxa/foundation/containers/vector.h"
#include "veloxa/graphics/canvas.h"
#include "veloxa/graphics/software/rasterizer.h"
#include "veloxa/graphics/software/software_path.h"

namespace vx::gfx::sw {

class SoftwareCanvas : public Canvas {
 public:
  SoftwareCanvas(vx::u32* pixels, vx::u32 width, vx::u32 height,
                 vx::u32 stride);

  void Begin() override;
  void End() override;
  void Clear(Color color) override;

  void FillRect(const Rect& rect, const Brush& brush) override;
  void FillRoundedRect(const Rect& rect, vx::f32 radius,
                       const Brush& brush) override;
  void FillPath(const Path& path, const Brush& brush) override;

  void StrokeRect(const Rect& rect, const Brush& brush,
                  vx::f32 width) override;
  void StrokeRoundedRect(const Rect& rect, vx::f32 radius,
                         const Brush& brush, vx::f32 width) override;
  void StrokePath(const Path& path, const Brush& brush,
                  vx::f32 width) override;
  void StrokeLine(Point a, Point b, const Brush& brush,
                  vx::f32 width) override;

  void PushClipRect(const Rect& rect) override;
  void PushClipPath(const Path& path) override;
  void PopClip() override;

  void PushLayer(const Rect& bounds, vx::f32 opacity) override;
  void PopLayer() override;

  void SetTransform(const Matrix3x2& m) override;
  Matrix3x2 GetTransform() const override;
  void PushState() override;
  void PopState() override;

  std::unique_ptr<Path> CreatePath() override;

 private:
  struct State {
    Matrix3x2 transform;
    vx::usize clip_stack_depth;
  };

  struct LayerInfo {
    vx::Vector<vx::u32> buffer;
    vx::u32 width, height, stride;
    vx::f32 opacity;
    vx::u32* parent_pixels;
  };

  Rect CurrentClip() const;

  vx::u32* pixels_;
  vx::u32 width_, height_, stride_;
  Matrix3x2 transform_;
  vx::Vector<State> state_stack_;
  vx::Vector<Rect> clip_stack_;
  vx::Vector<LayerInfo> layer_stack_;
  bool active_;
};

}  // namespace vx::gfx::sw

#endif  // VELOXA_GRAPHICS_SOFTWARE_SOFTWARE_CANVAS_H_
