#ifndef VELOXA_GRAPHICS_GLES_GLES_CANVAS_H_
#define VELOXA_GRAPHICS_GLES_GLES_CANVAS_H_

#include <GLES3/gl3.h>

#include <memory>

#include "veloxa/foundation/containers/vector.h"
#include "veloxa/graphics/canvas.h"

namespace vx::platform { class Sdl2GLWindowSurface; }
namespace vx::text {
class FontManager;
class GlyphCache;
}

namespace vx::gfx::gles {

// GLES-backed Canvas implementation (G1.4 skeleton phase).
//
// Skeleton scope (TASK-20260507-01):
//   * Begin / End / Clear / SetTransform / GetTransform / PushState /
//     PopState — fully implemented.
//   * The remaining 15 Canvas methods (FillRect / FillRoundedRect /
//     FillPath / Stroke* / DrawText / DrawImage / Push*Clip / PopClip /
//     PushLayer / PopLayer / CreatePath) are no-op stubs that get
//     replaced by G1.5 (FillRect+FillRoundedRect), G1.6 (FillPath via
//     libtess2), G1.7 (Stroke*), G1.8 (DrawText), G1.9 (DrawImage),
//     G1.10 (Clip), G1.11 (Layer), G1.12 (CreatePath).
//
// Ownership:
//   * surface_ — borrowed (caller retains ownership).
//   * quad_vao_ / quad_vbo_ — owned, glDelete in dtor.
//   * No GL context is created here — caller must have made the context
//     current via `surface->gles_display()->MakeCurrent()` before
//     constructing this canvas (test fixtures do this explicitly;
//     Application G1.13 will plumb it).
//
// Threading:
//   * GL is single-threaded by design; all GLESCanvas methods MUST be
//     called from the thread that holds the GL context current. A real
//     DCHECK arrives with G1.13/G1.14 once Application owns the
//     lifecycle. T8 reverse probe documents the contract today.
class GLESCanvas final : public Canvas {
 public:
  explicit GLESCanvas(vx::platform::Sdl2GLWindowSurface* surface,
                      vx::text::FontManager* font_manager = nullptr,
                      vx::text::GlyphCache* glyph_cache = nullptr);
  ~GLESCanvas() override;

  GLESCanvas(const GLESCanvas&) = delete;
  GLESCanvas& operator=(const GLESCanvas&) = delete;

  // ---- Real implementations (G1.4 skeleton scope) ----
  void Begin() override;
  void End() override;
  void Clear(Color color) override;
  void SetTransform(const Matrix3x2& m) override;
  Matrix3x2 GetTransform() const override;
  void PushState() override;
  void PopState() override;

  // ---- Real implementations (G1.5 fill scope) ----
  void FillRect(const Rect& rect, const Brush& brush) override;
  void FillRoundedRect(const Rect& rect, vx::f32 radius,
                       const Brush& brush) override;

  // ---- Real implementations (G1.6 path fill scope) ----
  void FillPath(const Path& path, const Brush& brush) override;

  // ---- No-op stubs (G1.7+ implementation scope) ----
  void StrokeRect(const Rect&, const Brush&, vx::f32) override {}
  void StrokeRoundedRect(const Rect&, vx::f32, const Brush&,
                         vx::f32) override {}
  void StrokePath(const Path&, const Brush&, vx::f32) override {}
  void StrokeLine(Point, Point, const Brush&, vx::f32) override {}
  void DrawText(vx::StringView, const Rect&, vx::f32,
                const Brush&) override {}
  void DrawImage(const Image&, const Rect&, const Rect&) override {}
  void PushClipRect(const Rect&) override {}
  void PushClipPath(const Path&) override {}
  void PopClip() override {}
  void PushLayer(const Rect&, vx::f32) override {}
  void PopLayer() override {}
  std::unique_ptr<Path> CreatePath() override;  // returns nullptr (G1.12)

  // ---- GLES-specific accessors for tests / G1.5+ implementations ----
  bool active() const { return active_; }
  GLuint quad_vao() const { return quad_vao_; }
  GLuint quad_vbo() const { return quad_vbo_; }
  GLuint path_vao() const { return path_vao_; }

 private:
  struct State {
    Matrix3x2 transform;
    vx::usize clip_stack_depth;  // reserved for G1.10 PushClip*
  };

  vx::platform::Sdl2GLWindowSurface* surface_;
  vx::u32 width_ = 0;
  vx::u32 height_ = 0;
  Matrix3x2 transform_;
  vx::Vector<State> state_stack_;
  bool active_ = false;
  GLuint quad_vao_ = 0;
  GLuint quad_vbo_ = 0;
  GLuint path_vao_ = 0;
  GLuint path_vbo_ = 0;
  GLuint path_ebo_ = 0;
  vx::text::FontManager* font_manager_ = nullptr;
  vx::text::GlyphCache* glyph_cache_ = nullptr;

  // ---- G1.5 shader program resources (B5=A ctor init / dtor delete) ----
  // solid_program_ + rounded_program_ are GL program handles. uniform_loc_
  // arrays cache glGetUniformLocation results to avoid per-FillRect lookups.
  GLuint solid_program_ = 0;
  GLuint rounded_program_ = 0;

  // Uniform indices into per-program location caches.
  enum SolidUniform {
    kSolidURectPx = 0,
    kSolidUXformPx,
    kSolidUViewportPx,
    kSolidUColor,
    kSolidUniformCount
  };
  enum RoundedUniform {
    kRoundedURectPx = 0,
    kRoundedUXformPx,
    kRoundedUViewportPx,
    kRoundedUColor,
    kRoundedUHalfPx,
    kRoundedURadiusPx,
    kRoundedUniformCount
  };
  GLint solid_uniforms_[kSolidUniformCount] = {-1, -1, -1, -1};
  GLint rounded_uniforms_[kRoundedUniformCount] = {-1, -1, -1, -1, -1, -1};

  // ---- G1.6 path fill program (kPathVert + kSolidFrag) ----
  GLuint path_program_ = 0;
  enum PathUniform {
    kPathUXformPx = 0,
    kPathUViewportPx,
    kPathUColor,
    kPathUniformCount
  };
  GLint path_uniforms_[kPathUniformCount] = {-1, -1, -1};

  // ---- G1.5 helpers (private) ----
  // CompileShader / LinkProgram return 0 on failure (with infoLog assertion
  // in debug builds via VX_DCHECK).
  static GLuint CompileShader(GLenum type, const char* source);
  static GLuint LinkProgram(GLuint vert, GLuint frag);
  void InitShaderPrograms();      // ctor helper
  void DestroyShaderPrograms();   // dtor helper
  void InitPathGeometry();        // G1.6 ctor — path VAO/VBO/EBO layout
  void UploadUnitQuad();          // ctor helper — 6 verts to quad_vbo_
  // Extract solid Color from a Brush. For kLinearGradient, returns
  // brush.linear.color_start (B7=A fallback) so callers don't crash.
  static Color BrushSolidColor(const Brush& brush);
  // Convert Matrix3x2 (6 floats) to GLES mat3 column-major (9 floats).
  static void Matrix3x2ToMat3(const Matrix3x2& src, GLfloat dst[9]);
};

}  // namespace vx::gfx::gles

#endif  // VELOXA_GRAPHICS_GLES_GLES_CANVAS_H_
