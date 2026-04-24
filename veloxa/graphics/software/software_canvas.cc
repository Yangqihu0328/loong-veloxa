#include "veloxa/graphics/software/software_canvas.h"

#include <cstring>

#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#include <hb-ft.h>

#include "veloxa/foundation/base/assert.h"
#include "veloxa/text/font_manager.h"
#include "veloxa/text/glyph_cache.h"

namespace vx::gfx::sw {

namespace {

// TASK-20260424-03 Phase 1 (D2): hb_buffer reuse via thread_local RAII holder.
// Eliminates per-DrawText hb_buffer_create/destroy (≈ glibc arena malloc +
// structure init). Thread-safe by construction (each thread owns its own
// buffer); leak-safe via dtor running at thread exit.
struct HbBufferHolder {
  hb_buffer_t* buf = nullptr;
  HbBufferHolder() : buf(hb_buffer_create()) {}
  ~HbBufferHolder() {
    if (buf) hb_buffer_destroy(buf);
  }
  HbBufferHolder(const HbBufferHolder&) = delete;
  HbBufferHolder& operator=(const HbBufferHolder&) = delete;
};

hb_buffer_t* AcquireThreadLocalHbBuffer() {
  thread_local HbBufferHolder holder;
  return holder.buf;
}

}  // namespace

SoftwareCanvas::SoftwareCanvas(vx::u32* pixels, vx::u32 width, vx::u32 height,
                               vx::u32 stride,
                               text::FontManager* font_manager,
                               text::GlyphCache* glyph_cache)
    : pixels_(pixels),
      width_(width),
      height_(height),
      stride_(stride),
      transform_(Matrix3x2::Identity()),
      active_(false),
      font_manager_(font_manager),
      glyph_cache_(glyph_cache) {}

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

void SoftwareCanvas::DrawTextFallback(vx::StringView text, const Rect& bounds,
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

void SoftwareCanvas::DrawText(vx::StringView text, const Rect& bounds,
                              vx::f32 font_size, const Brush& brush) {
  if (!font_manager_ || !glyph_cache_) {
    DrawTextFallback(text, bounds, font_size, brush);
    return;
  }

  using namespace vx::text;

  // TASK-03 Phase 3 (E): cache default FontHandle per canvas instance.
  // FindFont("", 400) performs an O(N) string compare against each loaded
  // family on every warm DrawText call; on this canvas the handle is stable
  // so a single resolution suffices.
  FontHandle font = kInvalidFont;
  if (cached_default_font_ != 0) {
    font = cached_default_font_;
  } else if (font_manager_->font_count() > 0) {
    font = font_manager_->FindFont("", 400);
    if (font == kInvalidFont) {
      font = 1;
    }
    cached_default_font_ = font;
  }
  if (font == kInvalidFont) {
    DrawTextFallback(text, bounds, font_size, brush);
    return;
  }

  u32 pixel_size = static_cast<u32>(font_size);
  if (pixel_size == 0) pixel_size = 1;

  // TASK-03 Phase 2 (C): idempotent FT size set — skips the underlying
  // FT_Set_Pixel_Sizes call on warm paths where the cached ft_pixel_size
  // already matches, saving an internal FT_Request_Metrics recompute.
  auto* face = font_manager_->SetFacePixelSize(font, pixel_size);
  if (!face) {
    DrawTextFallback(text, bounds, font_size, brush);
    return;
  }

  // #48: obtain a cached hb_font_t for this handle instead of the
  // previous per-DrawText hb_ft_font_create_referenced/hb_font_destroy.
  // FontManager handles reconfiguration when pixel_size differs.
  hb_font_t* hb_font = font_manager_->GetHbFont(font, pixel_size);
  if (!hb_font) {
    DrawTextFallback(text, bounds, font_size, brush);
    return;
  }

  hb_buffer_t* buf = AcquireThreadLocalHbBuffer();
  if (!buf) {
    DrawTextFallback(text, bounds, font_size, brush);
    return;
  }
  hb_buffer_reset(buf);
  hb_buffer_add_utf8(buf, text.data(), static_cast<int>(text.size()), 0, -1);
  hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
  hb_buffer_set_script(buf, HB_SCRIPT_LATIN);
  hb_buffer_guess_segment_properties(buf);
  hb_shape(hb_font, buf, nullptr, 0);

  u32 glyph_count = 0;
  hb_glyph_info_t* glyph_info = hb_buffer_get_glyph_infos(buf, &glyph_count);
  hb_glyph_position_t* glyph_pos =
      hb_buffer_get_glyph_positions(buf, &glyph_count);

  f32 pen_x = bounds.x;
  f32 pen_y = bounds.y + static_cast<f32>(face->size->metrics.ascender >> 6);
  Color text_color = brush.solid;

  for (u32 i = 0; i < glyph_count; ++i) {
    u32 glyph_id = glyph_info[i].codepoint;
    f32 x_offset = glyph_pos[i].x_offset / 64.0f;
    f32 y_offset = glyph_pos[i].y_offset / 64.0f;
    f32 x_advance = glyph_pos[i].x_advance / 64.0f;

    const GlyphBitmap* cached =
        glyph_cache_->Get(font, glyph_id, pixel_size);
    if (!cached) {
      FT_Load_Glyph(face, glyph_id, FT_LOAD_DEFAULT);
      FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
      FT_Bitmap& bmp = face->glyph->bitmap;

      GlyphBitmap gbmp;
      gbmp.width = bmp.width;
      gbmp.height = bmp.rows;
      gbmp.bearing_x = face->glyph->bitmap_left;
      gbmp.bearing_y = face->glyph->bitmap_top;
      gbmp.advance = x_advance;
      usize bmp_size = static_cast<usize>(bmp.width) * bmp.rows;
      gbmp.alpha.reserve(bmp_size);
      for (usize j = 0; j < bmp.rows; ++j) {
        for (usize k = 0; k < bmp.width; ++k) {
          gbmp.alpha.push_back(bmp.buffer[j * bmp.pitch + k]);
        }
      }
      glyph_cache_->Put(font, glyph_id, pixel_size,
                         static_cast<GlyphBitmap&&>(gbmp));
      cached = glyph_cache_->Get(font, glyph_id, pixel_size);
    }

    if (cached && cached->width > 0 && cached->height > 0) {
      i32 gx = static_cast<i32>(pen_x + x_offset) + cached->bearing_x;
      i32 gy = static_cast<i32>(pen_y - y_offset) - cached->bearing_y;

      for (u32 row = 0; row < cached->height; ++row) {
        for (u32 col = 0; col < cached->width; ++col) {
          i32 px = gx + static_cast<i32>(col);
          i32 py = gy + static_cast<i32>(row);
          if (px < 0 || py < 0 || px >= static_cast<i32>(width_) ||
              py >= static_cast<i32>(height_))
            continue;

          u8 alpha = cached->alpha[row * cached->width + col];
          if (alpha == 0) continue;

          u32 dst_idx =
              static_cast<u32>(py) * (stride_ / 4) + static_cast<u32>(px);
          u32 dst_pixel = pixels_[dst_idx];
          u8 dr = static_cast<u8>(dst_pixel & 0xFF);
          u8 dg = static_cast<u8>((dst_pixel >> 8) & 0xFF);
          u8 db = static_cast<u8>((dst_pixel >> 16) & 0xFF);
          u8 da = static_cast<u8>((dst_pixel >> 24) & 0xFF);

          u8 sa = static_cast<u8>(
              (static_cast<u32>(text_color.a) * alpha) / 255);
          u8 inv_sa = static_cast<u8>(255 - sa);
          u8 or_ = static_cast<u8>(
              (text_color.r * sa + dr * inv_sa) / 255);
          u8 og = static_cast<u8>(
              (text_color.g * sa + dg * inv_sa) / 255);
          u8 ob = static_cast<u8>(
              (text_color.b * sa + db * inv_sa) / 255);
          u8 oa = static_cast<u8>(sa + (da * inv_sa) / 255);

          pixels_[dst_idx] = static_cast<u32>(or_) |
                             (static_cast<u32>(og) << 8) |
                             (static_cast<u32>(ob) << 16) |
                             (static_cast<u32>(oa) << 24);
        }
      }
    }
    pen_x += x_advance;
  }

  // TASK-03 Phase 1: buf is owned by thread_local HbBufferHolder; do not
  // destroy here. Contents will be cleared via hb_buffer_reset on next call.
  // hb_font is owned by FontManager; do not destroy here.
}

void SoftwareCanvas::DrawImage(const Image& image, const Rect& src_rect,
                               const Rect& dst_rect) {
  if (!image.valid() || src_rect.IsEmpty() || dst_rect.IsEmpty()) return;

  Rect clip = CurrentClip();
  Rect visible = clip.Intersect(dst_rect);
  if (visible.IsEmpty()) return;

  vx::u32 stride_px = stride_ / 4;
  i32 dx0 = static_cast<i32>(visible.x);
  i32 dy0 = static_cast<i32>(visible.y);
  i32 dx1 = static_cast<i32>(visible.right());
  i32 dy1 = static_cast<i32>(visible.bottom());

  if (dx0 < 0) dx0 = 0;
  if (dy0 < 0) dy0 = 0;
  if (dx1 > static_cast<i32>(width_)) dx1 = static_cast<i32>(width_);
  if (dy1 > static_cast<i32>(height_)) dy1 = static_cast<i32>(height_);

  const u32* src_pixels = image.pixels();
  u32 img_w = image.width();
  u32 img_h = image.height();

  for (i32 py = dy0; py < dy1; ++py) {
    f32 ty = (static_cast<f32>(py) + 0.5f - dst_rect.y) / dst_rect.h;
    i32 sy = static_cast<i32>(src_rect.y + ty * src_rect.h);
    if (sy < 0 || sy >= static_cast<i32>(img_h)) continue;

    for (i32 px = dx0; px < dx1; ++px) {
      f32 tx = (static_cast<f32>(px) + 0.5f - dst_rect.x) / dst_rect.w;
      i32 sx = static_cast<i32>(src_rect.x + tx * src_rect.w);
      if (sx < 0 || sx >= static_cast<i32>(img_w)) continue;

      u32 src = src_pixels[sy * img_w + sx];
      u32 dst_idx = static_cast<u32>(py) * stride_px + static_cast<u32>(px);
      pixels_[dst_idx] = Rasterizer::BlendSrcOver(pixels_[dst_idx], src);
    }
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
