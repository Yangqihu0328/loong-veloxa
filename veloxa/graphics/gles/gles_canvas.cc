#include "veloxa/graphics/gles/gles_canvas.h"

#include "veloxa/foundation/base/assert.h"
#include "veloxa/graphics/gles/shaders.h"
#include "veloxa/platform/sdl2/sdl2_gl_window_surface.h"

namespace vx::gfx::gles {

GLESCanvas::GLESCanvas(vx::platform::Sdl2GLWindowSurface* surface,
                       vx::text::FontManager* font_manager,
                       vx::text::GlyphCache* glyph_cache)
    : surface_(surface),
      transform_(Matrix3x2::Identity()),
      font_manager_(font_manager),
      glyph_cache_(glyph_cache) {
  VX_DCHECK(surface_ != nullptr && surface_->valid());
  width_ = surface_->width();
  height_ = surface_->height();

  // D5=A: one-shot VAO/VBO allocation. Symmetric with dtor's glDelete.
  // Caller must have made the GL context current; we don't MakeCurrent
  // here because (a) the surface owns the display, (b) Application
  // (G1.13) already does the lifecycle dance, and (c) test fixtures
  // explicitly call MakeCurrent before constructing the canvas.
  glGenVertexArrays(1, &quad_vao_);
  glGenBuffers(1, &quad_vbo_);
}

GLESCanvas::~GLESCanvas() {
  if (quad_vao_ != 0) glDeleteVertexArrays(1, &quad_vao_);
  if (quad_vbo_ != 0) glDeleteBuffers(1, &quad_vbo_);
}

void GLESCanvas::Begin() {
  // Refresh viewport + alpha blending state every Begin so a Resize()
  // between frames takes effect immediately. width_/height_ track the
  // surface's logical size; G1.13 will plumb resize events.
  width_ = surface_->width();
  height_ = surface_->height();
  glViewport(0, 0, static_cast<GLsizei>(width_),
             static_cast<GLsizei>(height_));
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  active_ = true;
}

void GLESCanvas::End() {
  // Flush GPU command queue so SwapBuffers (called by Surface::Present)
  // sees a consistent state. We do NOT call SwapBuffers here — that's
  // the surface's responsibility (Sdl2GLWindowSurface::Present →
  // SDL_GL_SwapWindow).
  glFlush();
  active_ = false;
}

void GLESCanvas::Clear(Color color) {
  const float r = color.r / 255.0f;
  const float g = color.g / 255.0f;
  const float b = color.b / 255.0f;
  const float a = color.a / 255.0f;
  glClearColor(r, g, b, a);
  glClear(GL_COLOR_BUFFER_BIT);
}

void GLESCanvas::SetTransform(const Matrix3x2& m) { transform_ = m; }

Matrix3x2 GLESCanvas::GetTransform() const { return transform_; }

void GLESCanvas::PushState() {
  // D6=A: clip_stack_depth reserved for G1.10 — currently 0.
  state_stack_.push_back({transform_, 0});
}

void GLESCanvas::PopState() {
  if (state_stack_.empty()) return;
  State s = state_stack_.back();
  state_stack_.pop_back();
  transform_ = s.transform;
  // G1.10 will pop clip_stack_ down to s.clip_stack_depth here.
}

std::unique_ptr<Path> GLESCanvas::CreatePath() {
  // G1.12 will return a GLESPath (vertex-buffer-friendly variant);
  // for skeleton we return nullptr so callers can detect "not yet
  // implemented" without crashing.
  return nullptr;
}

}  // namespace vx::gfx::gles
