#include "veloxa/graphics/gles/gles_canvas.h"

#include <algorithm>

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

  // G1.5 (B2=A): upload unit-quad triangle list (6 verts) to quad_vbo_.
  // B5=A: build the two shader programs while the GL context is current.
  UploadUnitQuad();
  InitShaderPrograms();
}

GLESCanvas::~GLESCanvas() {
  if (quad_vao_ != 0) glDeleteVertexArrays(1, &quad_vao_);
  if (quad_vbo_ != 0) glDeleteBuffers(1, &quad_vbo_);
  DestroyShaderPrograms();  // G1.5 (B5=A) program teardown
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

// -----------------------------------------------------------------------------
// G1.5: Shader compilation / linking helpers (private static).
// -----------------------------------------------------------------------------

GLuint GLESCanvas::CompileShader(GLenum type, const char* source) {
  GLuint shader = glCreateShader(type);
  if (shader == 0) return 0;
  glShaderSource(shader, 1, &source, nullptr);
  glCompileShader(shader);
  GLint status = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
  if (status != GL_TRUE) {
    char log[1024] = {0};
    GLsizei len = 0;
    glGetShaderInfoLog(shader, sizeof(log) - 1, &len, log);
    VX_DCHECK(false && "GLES shader compile failed; see infoLog");
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

GLuint GLESCanvas::LinkProgram(GLuint vert, GLuint frag) {
  GLuint program = glCreateProgram();
  if (program == 0) return 0;
  glAttachShader(program, vert);
  glAttachShader(program, frag);
  // a_pos at attribute location 0 — matches glVertexAttribPointer(0, ...)
  // in UploadUnitQuad. Must be set BEFORE glLinkProgram.
  glBindAttribLocation(program, 0, "a_pos");
  glLinkProgram(program);
  GLint status = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &status);
  if (status != GL_TRUE) {
    char log[1024] = {0};
    GLsizei len = 0;
    glGetProgramInfoLog(program, sizeof(log) - 1, &len, log);
    VX_DCHECK(false && "GLES program link failed; see infoLog");
    glDeleteProgram(program);
    return 0;
  }
  // Detach (shaders are reference-counted; the glDeleteShader call sites
  // above retain the shaders until the program is deleted).
  glDetachShader(program, vert);
  glDetachShader(program, frag);
  return program;
}

void GLESCanvas::InitShaderPrograms() {
  // Solid program: kSolidVert + kSolidFrag.
  GLuint sv = CompileShader(GL_VERTEX_SHADER, kSolidVert);
  GLuint sf = CompileShader(GL_FRAGMENT_SHADER, kSolidFrag);
  solid_program_ = LinkProgram(sv, sf);
  glDeleteShader(sv);
  glDeleteShader(sf);
  if (solid_program_ != 0) {
    solid_uniforms_[kSolidURectPx] =
        glGetUniformLocation(solid_program_, "u_rect_px");
    solid_uniforms_[kSolidUXformPx] =
        glGetUniformLocation(solid_program_, "u_xform_px");
    solid_uniforms_[kSolidUViewportPx] =
        glGetUniformLocation(solid_program_, "u_viewport_px");
    solid_uniforms_[kSolidUColor] =
        glGetUniformLocation(solid_program_, "u_color");
  }

  // Rounded program: kSolidVert (shared) + kRoundedRectFrag.
  GLuint rv = CompileShader(GL_VERTEX_SHADER, kSolidVert);
  GLuint rf = CompileShader(GL_FRAGMENT_SHADER, kRoundedRectFrag);
  rounded_program_ = LinkProgram(rv, rf);
  glDeleteShader(rv);
  glDeleteShader(rf);
  if (rounded_program_ != 0) {
    rounded_uniforms_[kRoundedURectPx] =
        glGetUniformLocation(rounded_program_, "u_rect_px");
    rounded_uniforms_[kRoundedUXformPx] =
        glGetUniformLocation(rounded_program_, "u_xform_px");
    rounded_uniforms_[kRoundedUViewportPx] =
        glGetUniformLocation(rounded_program_, "u_viewport_px");
    rounded_uniforms_[kRoundedUColor] =
        glGetUniformLocation(rounded_program_, "u_color");
    rounded_uniforms_[kRoundedUHalfPx] =
        glGetUniformLocation(rounded_program_, "u_half_px");
    rounded_uniforms_[kRoundedURadiusPx] =
        glGetUniformLocation(rounded_program_, "u_radius_px");
  }
}

void GLESCanvas::DestroyShaderPrograms() {
  if (solid_program_ != 0) {
    glDeleteProgram(solid_program_);
    solid_program_ = 0;
  }
  if (rounded_program_ != 0) {
    glDeleteProgram(rounded_program_);
    rounded_program_ = 0;
  }
}

void GLESCanvas::UploadUnitQuad() {
  // 6 verts (2 triangles) over unit square [0,1]^2.
  static constexpr GLfloat kQuad[12] = {
      0.0f, 0.0f,
      1.0f, 0.0f,
      0.0f, 1.0f,
      0.0f, 1.0f,
      1.0f, 0.0f,
      1.0f, 1.0f,
  };
  glBindVertexArray(quad_vao_);
  glBindBuffer(GL_ARRAY_BUFFER, quad_vbo_);
  glBufferData(GL_ARRAY_BUFFER, sizeof(kQuad), kQuad, GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat),
                        reinterpret_cast<const void*>(0));
  glBindVertexArray(0);
}

Color GLESCanvas::BrushSolidColor(const Brush& brush) {
  // B7=A: kSolid passes through; kLinearGradient falls back to color_start
  // so callers don't crash. G2 will add a real gradient shader path.
  if (brush.type == Brush::Type::kSolid) return brush.solid;
  return brush.linear.color_start;
}

void GLESCanvas::Matrix3x2ToMat3(const Matrix3x2& src, GLfloat dst[9]) {
  // GLES uniformMatrix3fv is column-major. Matrix3x2 layout:
  //   m[0] m[1] | m[2] m[3] | m[4] m[5]
  //     a    b      c    d      tx   ty
  // -> mat3 cols: (a, b, 0), (c, d, 0), (tx, ty, 1).
  dst[0] = src.m[0]; dst[1] = src.m[1]; dst[2] = 0.0f;
  dst[3] = src.m[2]; dst[4] = src.m[3]; dst[5] = 0.0f;
  dst[6] = src.m[4]; dst[7] = src.m[5]; dst[8] = 1.0f;
}

// -----------------------------------------------------------------------------
// G1.5: FillRect / FillRoundedRect.
// -----------------------------------------------------------------------------

void GLESCanvas::FillRect(const Rect& rect, const Brush& brush) {
  if (solid_program_ == 0 || rect.IsEmpty()) return;
  Color c = BrushSolidColor(brush);
  GLfloat mat[9];
  Matrix3x2ToMat3(transform_, mat);

  glUseProgram(solid_program_);
  glUniform4f(solid_uniforms_[kSolidURectPx], rect.x, rect.y, rect.w, rect.h);
  glUniformMatrix3fv(solid_uniforms_[kSolidUXformPx], 1, GL_FALSE, mat);
  glUniform2f(solid_uniforms_[kSolidUViewportPx],
              static_cast<GLfloat>(width_), static_cast<GLfloat>(height_));
  glUniform4f(solid_uniforms_[kSolidUColor], c.r / 255.0f, c.g / 255.0f,
              c.b / 255.0f, c.a / 255.0f);

  glBindVertexArray(quad_vao_);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  glBindVertexArray(0);
}

void GLESCanvas::FillRoundedRect(const Rect& rect, vx::f32 radius,
                                 const Brush& brush) {
  if (rounded_program_ == 0 || rect.IsEmpty()) return;
  // Clamp radius to half the shortest dimension (matches CSS / SoftwareCanvas).
  vx::f32 r = std::min(radius, std::min(rect.w, rect.h) * 0.5f);
  if (r <= 0.0f) {
    // Degenerate to FillRect (no rounding → SDF would still work but slower
    // and would gate a 1-pixel AA edge unnecessarily).
    FillRect(rect, brush);
    return;
  }
  Color c = BrushSolidColor(brush);
  GLfloat mat[9];
  Matrix3x2ToMat3(transform_, mat);

  glUseProgram(rounded_program_);
  glUniform4f(rounded_uniforms_[kRoundedURectPx], rect.x, rect.y, rect.w, rect.h);
  glUniformMatrix3fv(rounded_uniforms_[kRoundedUXformPx], 1, GL_FALSE, mat);
  glUniform2f(rounded_uniforms_[kRoundedUViewportPx],
              static_cast<GLfloat>(width_), static_cast<GLfloat>(height_));
  glUniform4f(rounded_uniforms_[kRoundedUColor], c.r / 255.0f, c.g / 255.0f,
              c.b / 255.0f, c.a / 255.0f);
  glUniform2f(rounded_uniforms_[kRoundedUHalfPx], rect.w * 0.5f, rect.h * 0.5f);
  glUniform1f(rounded_uniforms_[kRoundedURadiusPx], r);

  glBindVertexArray(quad_vao_);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  glBindVertexArray(0);
}

}  // namespace vx::gfx::gles
