#include "veloxa/graphics/gles/gles_canvas.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "tesselator.h"
#include "veloxa/foundation/base/assert.h"
#include "veloxa/graphics/gles/shaders.h"
#include "veloxa/graphics/software/software_path.h"
#include "veloxa/platform/sdl2/sdl2_gl_window_surface.h"

namespace vx::gfx::gles {
namespace {

// CPU flatten helpers — algorithm mirrors software/rasterizer.cc but appends
// polyline vertices instead of scanline edges (G1.6 B1=A).

void FlattenQuadToContour(vx::Vector<Point>& contour, Point p0, Point ctrl,
                          Point p2, int depth) {
  if (depth >= 16) {
    contour.push_back(p2);
    return;
  }
  Point mid = {(p0.x + p2.x) * 0.5f, (p0.y + p2.y) * 0.5f};
  vx::f32 dx = ctrl.x - mid.x;
  vx::f32 dy = ctrl.y - mid.y;
  if (dx * dx + dy * dy < 0.0625f) {
    contour.push_back(p2);
    return;
  }
  Point q0 = {(p0.x + ctrl.x) * 0.5f, (p0.y + ctrl.y) * 0.5f};
  Point q1 = {(ctrl.x + p2.x) * 0.5f, (ctrl.y + p2.y) * 0.5f};
  Point q2 = {(q0.x + q1.x) * 0.5f, (q0.y + q1.y) * 0.5f};
  FlattenQuadToContour(contour, p0, q0, q2, depth + 1);
  FlattenQuadToContour(contour, q2, q1, p2, depth + 1);
}

void FlattenCubicToContour(vx::Vector<Point>& contour, Point p0, Point c1,
                           Point c2, Point p3, int depth) {
  if (depth >= 16) {
    contour.push_back(p3);
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
    contour.push_back(p3);
    return;
  }
  Point p01 = {(p0.x + c1.x) * 0.5f, (p0.y + c1.y) * 0.5f};
  Point p12 = {(c1.x + c2.x) * 0.5f, (c1.y + c2.y) * 0.5f};
  Point p23 = {(c2.x + p3.x) * 0.5f, (c2.y + p3.y) * 0.5f};
  Point p012 = {(p01.x + p12.x) * 0.5f, (p01.y + p12.y) * 0.5f};
  Point p123 = {(p12.x + p23.x) * 0.5f, (p12.y + p23.y) * 0.5f};
  Point pmid = {(p012.x + p123.x) * 0.5f, (p012.y + p123.y) * 0.5f};
  FlattenCubicToContour(contour, p0, p01, p012, pmid, depth + 1);
  FlattenCubicToContour(contour, pmid, p123, p23, p3, depth + 1);
}

vx::Vector<vx::Vector<Point>> BuildTessContours(
    const sw::SoftwarePath& path) {
  vx::Vector<vx::Vector<Point>> contours;
  vx::Vector<Point> current;
  Point current_point = {0.0f, 0.0f};
  Point subpath_start = {0.0f, 0.0f};

  auto flush = [&]() {
    if (current.size() >= 3) {
      contours.push_back(std::move(current));
      current = {};
    } else {
      current.clear();
    }
  };

  for (const auto& cmd : path.commands()) {
    switch (cmd.type) {
      case sw::SoftwarePath::CommandType::kMoveTo:
        flush();
        current_point = cmd.p[0];
        subpath_start = current_point;
        current.push_back(current_point);
        break;
      case sw::SoftwarePath::CommandType::kLineTo:
        current_point = cmd.p[0];
        current.push_back(current_point);
        break;
      case sw::SoftwarePath::CommandType::kQuadTo: {
        Point ctrl = cmd.p[0];
        Point end = cmd.p[1];
        FlattenQuadToContour(current, current_point, ctrl, end, 0);
        current_point = end;
        break;
      }
      case sw::SoftwarePath::CommandType::kCubicTo: {
        Point c1 = cmd.p[0];
        Point c2 = cmd.p[1];
        Point end = cmd.p[2];
        FlattenCubicToContour(current, current_point, c1, c2, end, 0);
        current_point = end;
        break;
      }
      case sw::SoftwarePath::CommandType::kArcTo: {
        Point center = cmd.p[0];
        vx::f32 radius = cmd.f[0];
        vx::f32 start_angle = cmd.f[1];
        vx::f32 sweep = cmd.f[2];
        if (std::abs(sweep) < 1e-6f) break;
        int n_segs =
            std::max(8, static_cast<int>(std::abs(sweep) * 4.0f));
        for (int j = 1; j <= n_segs; ++j) {
          vx::f32 angle =
              start_angle +
              sweep * static_cast<vx::f32>(j) / static_cast<vx::f32>(n_segs);
          Point p = {center.x + radius * std::cos(angle),
                     center.y + radius * std::sin(angle)};
          current.push_back(p);
        }
        current_point = current.back();
        break;
      }
      case sw::SoftwarePath::CommandType::kClose:
        current.push_back(subpath_start);
        current_point = subpath_start;
        break;
    }
  }
  flush();
  return contours;
}

void* TessAlloc(void* /*user_data*/, unsigned int size) {
  return std::malloc(size);
}

void TessFree(void* /*user_data*/, void* ptr) { std::free(ptr); }

}  // namespace

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
  glGenVertexArrays(1, &path_vao_);
  glGenBuffers(1, &path_vbo_);
  glGenBuffers(1, &path_ebo_);

  // G1.5 (B2=A): upload unit-quad triangle list (6 verts) to quad_vbo_.
  // G1.6 (B3=A): allocate path VAO/VBO/EBO for tessellated mesh draws.
  // B5=A: build shader programs while the GL context is current.
  UploadUnitQuad();
  InitPathGeometry();
  InitShaderPrograms();
}

GLESCanvas::~GLESCanvas() {
  if (path_vao_ != 0) glDeleteVertexArrays(1, &path_vao_);
  if (path_vbo_ != 0) glDeleteBuffers(1, &path_vbo_);
  if (path_ebo_ != 0) glDeleteBuffers(1, &path_ebo_);
  if (quad_vao_ != 0) glDeleteVertexArrays(1, &quad_vao_);
  if (quad_vbo_ != 0) glDeleteBuffers(1, &quad_vbo_);
  DestroyShaderPrograms();  // G1.5 (B5=A) + G1.6 path program teardown
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

  // Path program: kPathVert + kSolidFrag (G1.6 B2=A).
  GLuint pv = CompileShader(GL_VERTEX_SHADER, kPathVert);
  GLuint pf = CompileShader(GL_FRAGMENT_SHADER, kSolidFrag);
  path_program_ = LinkProgram(pv, pf);
  glDeleteShader(pv);
  glDeleteShader(pf);
  if (path_program_ != 0) {
    path_uniforms_[kPathUXformPx] =
        glGetUniformLocation(path_program_, "u_xform_px");
    path_uniforms_[kPathUViewportPx] =
        glGetUniformLocation(path_program_, "u_viewport_px");
    path_uniforms_[kPathUColor] =
        glGetUniformLocation(path_program_, "u_color");
  }
}

void GLESCanvas::DestroyShaderPrograms() {
  if (path_program_ != 0) {
    glDeleteProgram(path_program_);
    path_program_ = 0;
  }
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

void GLESCanvas::InitPathGeometry() {
  glBindVertexArray(path_vao_);
  glBindBuffer(GL_ARRAY_BUFFER, path_vbo_);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat),
                        reinterpret_cast<const void*>(0));
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, path_ebo_);
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

void GLESCanvas::FillPath(const Path& path, const Brush& brush) {
  if (path_program_ == 0) return;
  const auto* sw_path = dynamic_cast<const sw::SoftwarePath*>(&path);
  VX_DCHECK(sw_path != nullptr);
  if (sw_path->IsEmpty()) return;

  Color c = BrushSolidColor(brush);
  if (c.a == 0) return;

  vx::Vector<vx::Vector<Point>> contours = BuildTessContours(*sw_path);
  if (contours.empty()) return;

  TESSalloc alloc;
  std::memset(&alloc, 0, sizeof(alloc));
  alloc.memalloc = TessAlloc;
  alloc.memfree = TessFree;

  TESStesselator* tess = tessNewTess(&alloc);
  if (tess == nullptr) return;

  for (const auto& contour : contours) {
    if (contour.size() < 3) continue;
    tessAddContour(tess, 2, contour.data(),
                   static_cast<int>(sizeof(Point)),
                   static_cast<int>(contour.size()));
  }

  if (!tessTesselate(tess, TESS_WINDING_NONZERO, TESS_POLYGONS, 3, 2, 0)) {
    tessDeleteTess(tess);
    return;
  }

  const int nverts = tessGetVertexCount(tess);
  const int nelems = tessGetElementCount(tess);
  const TESSreal* vertices = tessGetVertices(tess);
  const TESSindex* elements = tessGetElements(tess);
  if (nverts <= 0 || nelems <= 0 || vertices == nullptr ||
      elements == nullptr) {
    tessDeleteTess(tess);
    return;
  }

  GLfloat mat[9];
  Matrix3x2ToMat3(transform_, mat);

  glUseProgram(path_program_);
  glUniformMatrix3fv(path_uniforms_[kPathUXformPx], 1, GL_FALSE, mat);
  glUniform2f(path_uniforms_[kPathUViewportPx],
              static_cast<GLfloat>(width_), static_cast<GLfloat>(height_));
  glUniform4f(path_uniforms_[kPathUColor], c.r / 255.0f, c.g / 255.0f,
              c.b / 255.0f, c.a / 255.0f);

  glBindVertexArray(path_vao_);
  glBindBuffer(GL_ARRAY_BUFFER, path_vbo_);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(nverts) * 2 * sizeof(TESSreal),
               vertices, GL_STREAM_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, path_ebo_);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(nelems) * 3 * sizeof(TESSindex),
               elements, GL_STREAM_DRAW);
  glDrawElements(GL_TRIANGLES, nelems * 3, GL_UNSIGNED_INT, nullptr);
  glBindVertexArray(0);

  tessDeleteTess(tess);
}

}  // namespace vx::gfx::gles
