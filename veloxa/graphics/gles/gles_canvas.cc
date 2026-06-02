#include "veloxa/graphics/gles/gles_canvas.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "tesselator.h"
#include "veloxa/foundation/base/assert.h"
#include "veloxa/graphics/gles/glyph_atlas.h"
#include "veloxa/graphics/gles/image_texture_pool.h"
#include "veloxa/graphics/gles/shaders.h"
#include "veloxa/graphics/image.h"
#include "veloxa/graphics/software/software_path.h"
#include "veloxa/platform/sdl2/sdl2_gl_window_surface.h"
#include "veloxa/text/font_manager.h"

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

  // G1.8: glyph program + dynamic VBO, and the GL_R8 atlas (only when a
  // FontManager + GlyphCache are available — otherwise DrawText no-ops).
  InitGlyphResources();
  if (font_manager_ != nullptr && glyph_cache_ != nullptr) {
    glyph_atlas_ =
        std::make_unique<GlyphAtlas>(font_manager_, glyph_cache_);
  }

  // G1.9: image program + dynamic VBO + RGBA8 texture cache. Unlike the glyph
  // atlas this needs no fonts, so it is always created.
  InitImageResources();
  image_pool_ = std::make_unique<ImageTexturePool>();
}

GLESCanvas::~GLESCanvas() {
  image_pool_.reset();   // delete cached textures before context-bound teardown
  DestroyImageResources();
  glyph_atlas_.reset();  // delete atlas texture before context-bound teardown
  DestroyGlyphResources();
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
  // in UploadUnitQuad. Must be set BEFORE glLinkProgram. a_uv at location 1
  // is used only by kGlyphVert (G1.8); binding it for programs that lack the
  // attribute is harmless (the GL ignores unused bindings).
  glBindAttribLocation(program, 0, "a_pos");
  glBindAttribLocation(program, 1, "a_uv");
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

void GLESCanvas::InitGlyphResources() {
  // Program: kGlyphVert + kGlyphFrag.
  GLuint gv = CompileShader(GL_VERTEX_SHADER, kGlyphVert);
  GLuint gf = CompileShader(GL_FRAGMENT_SHADER, kGlyphFrag);
  glyph_program_ = LinkProgram(gv, gf);
  glDeleteShader(gv);
  glDeleteShader(gf);
  if (glyph_program_ != 0) {
    glyph_uniforms_[kGlyphUXformPx] =
        glGetUniformLocation(glyph_program_, "u_xform_px");
    glyph_uniforms_[kGlyphUViewportPx] =
        glGetUniformLocation(glyph_program_, "u_viewport_px");
    glyph_uniforms_[kGlyphUColor] =
        glGetUniformLocation(glyph_program_, "u_color");
    glyph_uniforms_[kGlyphUAtlas] =
        glGetUniformLocation(glyph_program_, "u_atlas");
  }

  // Dynamic per-glyph quad: interleaved [pos.xy, uv.xy] (4 floats / vertex).
  glGenVertexArrays(1, &glyph_vao_);
  glGenBuffers(1, &glyph_vbo_);
  glBindVertexArray(glyph_vao_);
  glBindBuffer(GL_ARRAY_BUFFER, glyph_vbo_);
  glEnableVertexAttribArray(0);  // a_pos
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat),
                        reinterpret_cast<const void*>(0));
  glEnableVertexAttribArray(1);  // a_uv
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat),
                        reinterpret_cast<const void*>(2 * sizeof(GLfloat)));
  glBindVertexArray(0);
}

void GLESCanvas::DestroyGlyphResources() {
  if (glyph_vbo_ != 0) {
    glDeleteBuffers(1, &glyph_vbo_);
    glyph_vbo_ = 0;
  }
  if (glyph_vao_ != 0) {
    glDeleteVertexArrays(1, &glyph_vao_);
    glyph_vao_ = 0;
  }
  if (glyph_program_ != 0) {
    glDeleteProgram(glyph_program_);
    glyph_program_ = 0;
  }
}

void GLESCanvas::InitImageResources() {
  // Program: kImageVert + kImageFrag.
  GLuint iv = CompileShader(GL_VERTEX_SHADER, kImageVert);
  GLuint ifrag = CompileShader(GL_FRAGMENT_SHADER, kImageFrag);
  image_program_ = LinkProgram(iv, ifrag);
  glDeleteShader(iv);
  glDeleteShader(ifrag);
  if (image_program_ != 0) {
    image_uniforms_[kImageUXformPx] =
        glGetUniformLocation(image_program_, "u_xform_px");
    image_uniforms_[kImageUViewportPx] =
        glGetUniformLocation(image_program_, "u_viewport_px");
    image_uniforms_[kImageUTex] =
        glGetUniformLocation(image_program_, "u_tex");
  }

  // Dynamic quad: interleaved [pos.xy, uv.xy] (4 floats / vertex), same layout
  // as glyph_vbo_ (a_pos at loc 0, a_uv at loc 1).
  glGenVertexArrays(1, &image_vao_);
  glGenBuffers(1, &image_vbo_);
  glBindVertexArray(image_vao_);
  glBindBuffer(GL_ARRAY_BUFFER, image_vbo_);
  glEnableVertexAttribArray(0);  // a_pos
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat),
                        reinterpret_cast<const void*>(0));
  glEnableVertexAttribArray(1);  // a_uv
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat),
                        reinterpret_cast<const void*>(2 * sizeof(GLfloat)));
  glBindVertexArray(0);
}

void GLESCanvas::DestroyImageResources() {
  if (image_vbo_ != 0) {
    glDeleteBuffers(1, &image_vbo_);
    image_vbo_ = 0;
  }
  if (image_vao_ != 0) {
    glDeleteVertexArrays(1, &image_vao_);
    image_vao_ = 0;
  }
  if (image_program_ != 0) {
    glDeleteProgram(image_program_);
    image_program_ = 0;
  }
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

// -----------------------------------------------------------------------------
// G1.7: Stroke* via Fill conversion (Stroke = Fill).
// -----------------------------------------------------------------------------

void GLESCanvas::StrokeRect(const Rect& rect, const Brush& brush,
                            vx::f32 width) {
  if (width <= 0.0f || rect.IsEmpty()) return;
  if (BrushSolidColor(brush).a == 0) return;
  // B1=A: four FillRect bars centered on the rect's edges (matches the
  // SoftwareCanvas StrokePath-of-rect convention). Top/bottom bars span the
  // corners so the ring is seam-free; left/right fill only the gap between.
  const vx::f32 hw = width * 0.5f;
  FillRect({rect.x - hw, rect.y - hw, rect.w + width, width}, brush);
  FillRect({rect.x - hw, rect.y + rect.h - hw, rect.w + width, width}, brush);
  FillRect({rect.x - hw, rect.y + hw, width, rect.h - width}, brush);
  FillRect({rect.x + rect.w - hw, rect.y + hw, width, rect.h - width}, brush);
}

void GLESCanvas::StrokeLine(Point a, Point b, const Brush& brush,
                            vx::f32 width) {
  if (width <= 0.0f) return;
  if (BrushSolidColor(brush).a == 0) return;
  const vx::f32 dx = b.x - a.x;
  const vx::f32 dy = b.y - a.y;
  const vx::f32 len = std::sqrt(dx * dx + dy * dy);
  if (len < 1e-6f) return;  // B5=A: zero-length line is a no-op.

  // B2=A: build a length×width rect centered at the origin, rotate to the
  // line's angle, translate to the midpoint, then prepend the active
  // transform. Matrix3x2 exposes only static Translation/Rotation + Multiply
  // (no fluent chaining), so compose explicitly.
  const vx::f32 angle = std::atan2(dy, dx);
  const Point mid = {(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f};
  const Matrix3x2 line_xform = transform_.Multiply(
      Matrix3x2::Translation(mid.x, mid.y).Multiply(Matrix3x2::Rotation(angle)));

  PushState();
  SetTransform(line_xform);
  FillRect({-len * 0.5f, -width * 0.5f, len, width}, brush);
  PopState();
}

void GLESCanvas::StrokeRoundedRect(const Rect& rect, vx::f32 radius,
                                   const Brush& brush, vx::f32 width) {
  if (rounded_program_ == 0 || width <= 0.0f || rect.IsEmpty()) return;
  if (BrushSolidColor(brush).a == 0) return;

  const Rect inner = {rect.x + width, rect.y + width, rect.w - 2.0f * width,
                      rect.h - 2.0f * width};
  const vx::f32 inner_r = std::max(0.0f, radius - width);

  // B3=A: carve an inside-stroke ring with the stencil buffer. SDL requests an
  // 8-bit stencil (sdl2_egl_display.cc), but Mesa swrast / strict drivers may
  // not honor it — query and fall back to a rectangular bar ring if absent.
  GLint stencil_bits = 0;
  glGetIntegerv(GL_STENCIL_BITS, &stencil_bits);
  while (glGetError() != GL_NO_ERROR) {
  }  // tolerate strict GLES3 drivers that reject GL_STENCIL_BITS.

  if (stencil_bits <= 0 || inner.IsEmpty()) {
    // Fallback: inside-stroke rectangular ring (rounded corners degrade to
    // square). Four FillRect bars hugging the inner edge of the rect.
    FillRect({rect.x, rect.y, rect.w, width}, brush);
    FillRect({rect.x, rect.y + rect.h - width, rect.w, width}, brush);
    FillRect({rect.x, rect.y + width, width, rect.h - 2.0f * width}, brush);
    FillRect({rect.x + rect.w - width, rect.y + width, width,
              rect.h - 2.0f * width},
             brush);
    return;
  }

  glEnable(GL_STENCIL_TEST);
  glStencilMask(0xFF);
  glClearStencil(0);
  glClear(GL_STENCIL_BUFFER_BIT);
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

  // Mark outer region = 1.
  glStencilFunc(GL_ALWAYS, 1, 0xFF);
  glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
  FillRoundedRect(rect, radius, brush);

  // Punch inner region back to 0 (the hole).
  glStencilFunc(GL_ALWAYS, 0, 0xFF);
  FillRoundedRect(inner, inner_r, brush);

  // Draw color only where stencil == 1 (the ring).
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glStencilFunc(GL_EQUAL, 1, 0xFF);
  glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
  FillRoundedRect(rect, radius, brush);

  glDisable(GL_STENCIL_TEST);
  glStencilMask(0xFF);
}

void GLESCanvas::StrokeSegmentQuad(Point a, Point b, vx::f32 half_width,
                                   const Brush& brush) {
  const vx::f32 dx = b.x - a.x;
  const vx::f32 dy = b.y - a.y;
  const vx::f32 len = std::sqrt(dx * dx + dy * dy);
  if (len < 1e-6f) return;
  const vx::f32 nx = -dy / len * half_width;
  const vx::f32 ny = dx / len * half_width;

  sw::SoftwarePath quad;
  quad.MoveTo({a.x + nx, a.y + ny});
  quad.LineTo({b.x + nx, b.y + ny});
  quad.LineTo({b.x - nx, b.y - ny});
  quad.LineTo({a.x - nx, a.y - ny});
  quad.Close();
  // FillPath applies transform_ via its uniform, so quads are built in
  // document (local) space — mirroring rasterizer.cc's StrokePath.
  FillPath(quad, brush);
}

void GLESCanvas::StrokePath(const Path& path, const Brush& brush,
                            vx::f32 width) {
  if (path_program_ == 0 || width <= 0.0f) return;
  const auto* sw_path = dynamic_cast<const sw::SoftwarePath*>(&path);
  VX_DCHECK(sw_path != nullptr);
  if (sw_path == nullptr || sw_path->IsEmpty()) return;
  if (BrushSolidColor(brush).a == 0) return;

  // B4=A: segment-quad stroke (rasterizer.cc algorithm). Flatten curves to a
  // polyline in local space and stroke each segment as a quad via FillPath.
  const vx::f32 hw = width * 0.5f;
  Point current = {0.0f, 0.0f};
  Point sub_start = {0.0f, 0.0f};
  vx::Vector<Point> poly;

  for (const auto& cmd : sw_path->commands()) {
    switch (cmd.type) {
      case sw::SoftwarePath::CommandType::kMoveTo:
        current = cmd.p[0];
        sub_start = current;
        break;
      case sw::SoftwarePath::CommandType::kLineTo:
        StrokeSegmentQuad(current, cmd.p[0], hw, brush);
        current = cmd.p[0];
        break;
      case sw::SoftwarePath::CommandType::kQuadTo: {
        poly.clear();
        poly.push_back(current);
        FlattenQuadToContour(poly, current, cmd.p[0], cmd.p[1], 0);
        for (vx::usize i = 1; i < poly.size(); ++i)
          StrokeSegmentQuad(poly[i - 1], poly[i], hw, brush);
        current = cmd.p[1];
        break;
      }
      case sw::SoftwarePath::CommandType::kCubicTo: {
        poly.clear();
        poly.push_back(current);
        FlattenCubicToContour(poly, current, cmd.p[0], cmd.p[1], cmd.p[2], 0);
        for (vx::usize i = 1; i < poly.size(); ++i)
          StrokeSegmentQuad(poly[i - 1], poly[i], hw, brush);
        current = cmd.p[2];
        break;
      }
      case sw::SoftwarePath::CommandType::kArcTo: {
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
          Point next = {center.x + radius * std::cos(angle),
                        center.y + radius * std::sin(angle)};
          StrokeSegmentQuad(prev, next, hw, brush);
          prev = next;
        }
        current = prev;
        break;
      }
      case sw::SoftwarePath::CommandType::kClose:
        StrokeSegmentQuad(current, sub_start, hw, brush);
        current = sub_start;
        break;
    }
  }
}

// -----------------------------------------------------------------------------
// G1.8: DrawText — HarfBuzz shaping + GL_R8 glyph atlas, per-glyph quad draw.
// Flow mirrors software_canvas.cc DrawText (font resolution → SetFacePixelSize
// → ShapeOrLookup → pen walk) but uploads glyph coverage to the atlas and
// draws textured quads instead of CPU blending.
// -----------------------------------------------------------------------------
void GLESCanvas::DrawText(vx::StringView text, const Rect& bounds,
                          vx::f32 font_size, const Brush& brush) {
  if (glyph_program_ == 0 || glyph_atlas_ == nullptr ||
      font_manager_ == nullptr) {
    return;
  }
  if (text.empty()) return;
  Color c = BrushSolidColor(brush);
  if (c.a == 0) return;

  // Resolve a default font (mirrors software path's "" / handle-1 fallback).
  vx::text::FontHandle font = vx::text::kInvalidFont;
  if (font_manager_->font_count() > 0) {
    font = font_manager_->FindFont(vx::StringView(""), 400);
    if (font == vx::text::kInvalidFont) font = 1;
  }
  if (font == vx::text::kInvalidFont) return;

  vx::u32 pixel_size = static_cast<vx::u32>(font_size);
  if (pixel_size == 0) pixel_size = 1;

  FT_FaceRec_* face = font_manager_->SetFacePixelSize(font, pixel_size);
  if (face == nullptr) return;

  const vx::text::ShapedRun* shaped =
      font_manager_->ShapeOrLookup(font, pixel_size, text);
  if (shaped == nullptr) return;

  const vx::f32 ascender =
      static_cast<vx::f32>(face->size->metrics.ascender >> 6);
  vx::f32 pen_x = bounds.x;
  const vx::f32 pen_y = bounds.y + ascender;

  GLfloat mat[9];
  Matrix3x2ToMat3(transform_, mat);

  glUseProgram(glyph_program_);
  glUniformMatrix3fv(glyph_uniforms_[kGlyphUXformPx], 1, GL_FALSE, mat);
  glUniform2f(glyph_uniforms_[kGlyphUViewportPx],
              static_cast<GLfloat>(width_), static_cast<GLfloat>(height_));
  glUniform4f(glyph_uniforms_[kGlyphUColor], c.r / 255.0f, c.g / 255.0f,
              c.b / 255.0f, c.a / 255.0f);
  glActiveTexture(GL_TEXTURE0);
  glUniform1i(glyph_uniforms_[kGlyphUAtlas], 0);

  glBindVertexArray(glyph_vao_);
  glBindBuffer(GL_ARRAY_BUFFER, glyph_vbo_);

  const vx::usize glyph_count = shaped->glyphs.size();
  for (vx::usize i = 0; i < glyph_count; ++i) {
    const vx::text::ShapedGlyph& g = shaped->glyphs[i];
    // NB: GetOrUpload may bind/unbind GL_TEXTURE_2D when uploading a glyph on
    // a cache miss, so the atlas texture must be (re)bound AFTER this call and
    // before the draw, not once outside the loop.
    GlyphAtlasInfo info =
        glyph_atlas_->GetOrUpload(font, g.glyph_id, pixel_size);
    if (info.valid && info.width > 0 && info.height > 0) {
      // Pixel-space glyph quad (top-left origin). bearing_y is the distance
      // from baseline to the glyph's top edge.
      const vx::f32 x0 = pen_x + g.x_offset + static_cast<vx::f32>(info.bearing_x);
      const vx::f32 y0 = pen_y - g.y_offset - static_cast<vx::f32>(info.bearing_y);
      const vx::f32 x1 = x0 + static_cast<vx::f32>(info.width);
      const vx::f32 y1 = y0 + static_cast<vx::f32>(info.height);
      // The atlas stores the glyph top row at v0, so the quad's top edge maps
      // to v0 (consistent with the vertex shader's Y-flip to framebuffer).
      const GLfloat verts[24] = {
          x0, y0, info.u0, info.v0,
          x1, y0, info.u1, info.v0,
          x0, y1, info.u0, info.v1,
          x0, y1, info.u0, info.v1,
          x1, y0, info.u1, info.v0,
          x1, y1, info.u1, info.v1,
      };
      glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STREAM_DRAW);
      glBindTexture(GL_TEXTURE_2D, glyph_atlas_->texture_id());
      glDrawArrays(GL_TRIANGLES, 0, 6);
    }
    pen_x += g.x_advance;
  }

  glBindVertexArray(0);
  glBindTexture(GL_TEXTURE_2D, 0);
}

// -----------------------------------------------------------------------------
// G1.9: DrawImage. Uploads (or reuses) an RGBA8 texture via ImageTexturePool
// and blits a textured quad. src_rect selects a sub-region of the image (→ UV);
// dst_rect is the pixel-space destination. Mirrors software_canvas.cc's
// src→dst mapping; GL_BLEND (set in Begin) does the src-over compositing.
// -----------------------------------------------------------------------------
void GLESCanvas::DrawImage(const Image& image, const Rect& src_rect,
                           const Rect& dst_rect) {
  if (image_program_ == 0 || image_pool_ == nullptr) return;
  if (!image.valid() || src_rect.IsEmpty() || dst_rect.IsEmpty()) return;

  // NB: GetOrUpload mutates the GL_TEXTURE_2D binding + GL_UNPACK_ALIGNMENT on
  // a cache miss (P1#A side-effect contract), so the texture must be bound
  // AFTER this call and right before the draw.
  GLuint tex = image_pool_->GetOrUpload(image);
  if (tex == 0) return;

  // src_rect sub-region → normalized UV (D5). Image origin is top-left and the
  // vertex shader Y-flips to the framebuffer, so v grows downward like y.
  const vx::f32 iw = static_cast<vx::f32>(image.width());
  const vx::f32 ih = static_cast<vx::f32>(image.height());
  const vx::f32 u0 = src_rect.x / iw;
  const vx::f32 v0 = src_rect.y / ih;
  const vx::f32 u1 = src_rect.right() / iw;
  const vx::f32 v1 = src_rect.bottom() / ih;
  const vx::f32 x0 = dst_rect.x;
  const vx::f32 y0 = dst_rect.y;
  const vx::f32 x1 = dst_rect.right();
  const vx::f32 y1 = dst_rect.bottom();

  GLfloat mat[9];
  Matrix3x2ToMat3(transform_, mat);

  glUseProgram(image_program_);
  glUniformMatrix3fv(image_uniforms_[kImageUXformPx], 1, GL_FALSE, mat);
  glUniform2f(image_uniforms_[kImageUViewportPx],
              static_cast<GLfloat>(width_), static_cast<GLfloat>(height_));
  glActiveTexture(GL_TEXTURE0);
  glUniform1i(image_uniforms_[kImageUTex], 0);

  const GLfloat verts[24] = {
      x0, y0, u0, v0,
      x1, y0, u1, v0,
      x0, y1, u0, v1,
      x0, y1, u0, v1,
      x1, y0, u1, v0,
      x1, y1, u1, v1,
  };
  glBindVertexArray(image_vao_);
  glBindBuffer(GL_ARRAY_BUFFER, image_vbo_);
  glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STREAM_DRAW);
  glBindTexture(GL_TEXTURE_2D, tex);  // bind AFTER GetOrUpload (P1#A)
  // Authoritatively set the sampling filter each draw (overrides the pool's
  // upload-time LINEAR default), so one cached texture switches correctly
  // between NEAREST/LINEAR (D2: filter is sampler state, not a cache key).
  const GLint gl_filter =
      (image_filter_ == SamplingFilter::kNearest) ? GL_NEAREST : GL_LINEAR;
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter);
  glDrawArrays(GL_TRIANGLES, 0, 6);

  glBindVertexArray(0);
  glBindTexture(GL_TEXTURE_2D, 0);
}

}  // namespace vx::gfx::gles
