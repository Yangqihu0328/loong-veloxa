#include "veloxa/graphics/software/software_canvas.h"

#include <cstring>

#include "veloxa/foundation/base/assert.h"

namespace vx::gfx::sw {

SoftwareCanvas::SoftwareCanvas(vx::u32* pixels, vx::u32 width, vx::u32 height,
                               vx::u32 stride)
    : pixels_(pixels),
      width_(width),
      height_(height),
      stride_(stride),
      transform_(Matrix3x2::Identity()),
      active_(false) {}

void SoftwareCanvas::Begin() { active_ = true; }

void SoftwareCanvas::End() { active_ = false; }

void SoftwareCanvas::Clear(Color color) {
  vx::u32 packed = color.ToRGBA32();
  vx::u32 stride_px = stride_ / 4;
  for (vx::u32 y = 0; y < height_; ++y) {
    vx::u32 row = y * stride_px;
    for (vx::u32 x = 0; x < width_; ++x) {
      pixels_[row + x] = packed;
    }
  }
}

void SoftwareCanvas::FillRect(const Rect& rect, const Brush& brush) {
  Rasterizer rasterizer(pixels_, width_, height_, stride_);
  rasterizer.FillRect(rect, brush, CurrentClip(), transform_);
}

void SoftwareCanvas::FillRoundedRect(const Rect& rect, vx::f32 radius,
                                     const Brush& brush) {
  SoftwarePath path;
  path.MoveTo({rect.x + radius, rect.y});
  path.LineTo({rect.right() - radius, rect.y});
  path.QuadTo({rect.right(), rect.y}, {rect.right(), rect.y + radius});
  path.LineTo({rect.right(), rect.bottom() - radius});
  path.QuadTo({rect.right(), rect.bottom()},
              {rect.right() - radius, rect.bottom()});
  path.LineTo({rect.x + radius, rect.bottom()});
  path.QuadTo({rect.x, rect.bottom()}, {rect.x, rect.bottom() - radius});
  path.LineTo({rect.x, rect.y + radius});
  path.QuadTo({rect.x, rect.y}, {rect.x + radius, rect.y});
  path.Close();

  Rasterizer rasterizer(pixels_, width_, height_, stride_);
  rasterizer.FillPath(path, brush, CurrentClip(), transform_);
}

void SoftwareCanvas::FillPath(const Path& path, const Brush& brush) {
  const auto* sw_path = dynamic_cast<const SoftwarePath*>(&path);
  VX_DCHECK(sw_path != nullptr);
  Rasterizer rasterizer(pixels_, width_, height_, stride_);
  rasterizer.FillPath(*sw_path, brush, CurrentClip(), transform_);
}

void SoftwareCanvas::StrokeRect(const Rect& rect, const Brush& brush,
                                vx::f32 width) {
  SoftwarePath path;
  path.MoveTo({rect.x, rect.y});
  path.LineTo({rect.right(), rect.y});
  path.LineTo({rect.right(), rect.bottom()});
  path.LineTo({rect.x, rect.bottom()});
  path.Close();

  Rasterizer rasterizer(pixels_, width_, height_, stride_);
  rasterizer.StrokePath(path, brush, width, CurrentClip(), transform_);
}

void SoftwareCanvas::StrokeRoundedRect(const Rect& rect, vx::f32 radius,
                                       const Brush& brush, vx::f32 width) {
  SoftwarePath path;
  path.MoveTo({rect.x + radius, rect.y});
  path.LineTo({rect.right() - radius, rect.y});
  path.QuadTo({rect.right(), rect.y}, {rect.right(), rect.y + radius});
  path.LineTo({rect.right(), rect.bottom() - radius});
  path.QuadTo({rect.right(), rect.bottom()},
              {rect.right() - radius, rect.bottom()});
  path.LineTo({rect.x + radius, rect.bottom()});
  path.QuadTo({rect.x, rect.bottom()}, {rect.x, rect.bottom() - radius});
  path.LineTo({rect.x, rect.y + radius});
  path.QuadTo({rect.x, rect.y}, {rect.x + radius, rect.y});
  path.Close();

  Rasterizer rasterizer(pixels_, width_, height_, stride_);
  rasterizer.StrokePath(path, brush, width, CurrentClip(), transform_);
}

void SoftwareCanvas::StrokePath(const Path& path, const Brush& brush,
                                vx::f32 width) {
  const auto* sw_path = dynamic_cast<const SoftwarePath*>(&path);
  VX_DCHECK(sw_path != nullptr);
  Rasterizer rasterizer(pixels_, width_, height_, stride_);
  rasterizer.StrokePath(*sw_path, brush, width, CurrentClip(), transform_);
}

void SoftwareCanvas::StrokeLine(Point a, Point b, const Brush& brush,
                                vx::f32 width) {
  SoftwarePath path;
  path.MoveTo(a);
  path.LineTo(b);

  Rasterizer rasterizer(pixels_, width_, height_, stride_);
  rasterizer.StrokePath(path, brush, width, CurrentClip(), transform_);
}

void SoftwareCanvas::DrawText(vx::StringView text, const Rect& bounds,
                              vx::f32 font_size, const Brush& brush) {
  f32 char_width = font_size * 0.6f;
  f32 char_height = font_size;
  f32 x = bounds.x;
  f32 y = bounds.y;
  for (usize i = 0; i < text.size(); ++i) {
    if (text[i] == ' ') {
      x += char_width;
      continue;
    }
    if (x + char_width > bounds.right()) break;
    if (y + char_height > bounds.bottom()) break;
    FillRect({x, y, char_width - 1.0f, char_height}, brush);
    x += char_width;
  }
}

void SoftwareCanvas::PushClipRect(const Rect& rect) {
  Rect current = CurrentClip();
  clip_stack_.push_back(current.Intersect(rect));
}

void SoftwareCanvas::PushClipPath(const Path& path) {
  PushClipRect(path.Bounds());
}

void SoftwareCanvas::PopClip() {
  if (!clip_stack_.empty()) {
    clip_stack_.pop_back();
  }
}

void SoftwareCanvas::PushLayer(const Rect& bounds, vx::f32 opacity) {
  (void)bounds;
  LayerInfo layer;
  layer.width = width_;
  layer.height = height_;
  layer.stride = stride_;
  layer.opacity = opacity;
  layer.parent_pixels = pixels_;

  vx::u32 stride_px = stride_ / 4;
  vx::u32 buf_size = stride_px * height_;
  layer.buffer.resize(buf_size, 0u);

  layer_stack_.push_back(std::move(layer));
  pixels_ = layer_stack_.back().buffer.data();
}

void SoftwareCanvas::PopLayer() {
  if (layer_stack_.empty()) return;

  LayerInfo layer = std::move(layer_stack_.back());
  layer_stack_.pop_back();

  vx::u32* layer_pixels = layer.buffer.data();
  vx::f32 opacity = layer.opacity;
  pixels_ = layer.parent_pixels;

  vx::u32 stride_px = stride_ / 4;
  for (vx::u32 y = 0; y < height_; ++y) {
    for (vx::u32 x = 0; x < width_; ++x) {
      vx::u32 idx = y * stride_px + x;
      vx::u32 src = layer_pixels[idx];
      vx::u32 sa = (src >> 24) & 0xFF;
      if (sa == 0) continue;

      vx::u8 new_a =
          static_cast<vx::u8>(static_cast<vx::f32>(sa) * opacity);
      src = (src & 0x00FFFFFF) | (static_cast<vx::u32>(new_a) << 24);
      pixels_[idx] = Rasterizer::BlendSrcOver(pixels_[idx], src);
    }
  }
}

void SoftwareCanvas::SetTransform(const Matrix3x2& m) { transform_ = m; }

Matrix3x2 SoftwareCanvas::GetTransform() const { return transform_; }

void SoftwareCanvas::PushState() {
  state_stack_.push_back({transform_, clip_stack_.size()});
}

void SoftwareCanvas::PopState() {
  if (state_stack_.empty()) return;
  State state = state_stack_.back();
  state_stack_.pop_back();
  transform_ = state.transform;
  while (clip_stack_.size() > state.clip_stack_depth) {
    clip_stack_.pop_back();
  }
}

std::unique_ptr<Path> SoftwareCanvas::CreatePath() {
  return std::make_unique<SoftwarePath>();
}

Rect SoftwareCanvas::CurrentClip() const {
  if (clip_stack_.empty()) {
    return {0, 0, static_cast<vx::f32>(width_),
            static_cast<vx::f32>(height_)};
  }
  return clip_stack_.back();
}

}  // namespace vx::gfx::sw
