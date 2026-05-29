#ifndef VELOXA_GRAPHICS_GLES_SHADERS_H_
#define VELOXA_GRAPHICS_GLES_SHADERS_H_

// GLES shader sources — statically embedded as raw string literals (B6=A).
//
// Security contract (TASK-20260505-03 spec §6.2 / blueprint §9.1 #1):
//   These shader strings are COMPILE-TIME constants. They MUST NEVER be
//   concatenated with, replaced by, or augmented with caller-provided
//   data. The whole point of B6=A static embedding is to make GLSL source
//   injection impossible by construction. Verification:
//   `tests/graphics/gles/shader_injection_test.cc` reverse-probe.
//
// G1.4 skeleton phase ships only kPassthroughVert / kPassthroughFrag —
// the bare minimum that a future GLESCanvas shader pipeline can compile-
// link to prove the GLSL toolchain works under SDL_VIDEODRIVER=offscreen.
// G1.5+ adds kSolidVert / kSolidFrag / kRoundedRectFrag etc.

namespace vx::gfx::gles {

// Vertex shader: passthrough quad (position in clip space, no transform yet).
// G1.5+ will add a uniform mat3 u_transform — for skeleton this is a literal
// gl_Position assignment.
inline constexpr const char* kPassthroughVert = R"(#version 300 es
precision highp float;
in vec2 a_pos;
void main() {
  gl_Position = vec4(a_pos, 0.0, 1.0);
}
)";

// Fragment shader: solid white. G1.5+ swaps to per-brush color uniform.
// Skeleton just proves the pipeline links; actual draws use Clear (which
// bypasses fragment processing).
inline constexpr const char* kPassthroughFrag = R"(#version 300 es
precision mediump float;
out vec4 frag_color;
void main() {
  frag_color = vec4(1.0, 1.0, 1.0, 1.0);
}
)";

// -----------------------------------------------------------------------------
// G1.5: FillRect / FillRoundedRect shaders (B6=A static embedding continued).
//
// All three shaders take pixel-space inputs and produce NDC outputs in the
// vertex stage (Y-flipped to match Veloxa's top-left origin convention).
// Same security contract as kPassthroughVert/Frag: compile-time constant,
// never composed with user data. shader_injection_test.cc verifies all
// shader sources via kAllShaderSources[] (B8=A).
// -----------------------------------------------------------------------------

// Solid vertex shader: unit-quad [0,1]^2 -> pixel rect -> user xform -> NDC.
//
// Inputs:
//   * a_pos: unit quad vertex in [0,1]^2 (6 verts = 2 triangles).
//   * u_rect_px: target rect in pixel space (x, y, w, h).
//   * u_xform_px: user-supplied Matrix3x2 promoted to mat3 (last row = 0,0,1).
//   * u_viewport_px: surface size in pixels (w, h) for NDC mapping.
//
// Outputs:
//   * gl_Position: NDC coordinate with Y flipped (Veloxa: top-left origin /
//     OpenGL: bottom-left origin).
//   * v_local_px: local coordinate within the rect (0..u_rect_px.zw),
//     consumed by kRoundedRectFrag for SDF distance calculation.
inline constexpr const char* kSolidVert = R"(#version 300 es
precision highp float;
in vec2 a_pos;
uniform vec4 u_rect_px;
uniform mat3 u_xform_px;
uniform vec2 u_viewport_px;
out vec2 v_local_px;
void main() {
  vec2 px = u_rect_px.xy + a_pos * u_rect_px.zw;
  vec3 px3 = u_xform_px * vec3(px, 1.0);
  vec2 ndc = (px3.xy / u_viewport_px) * 2.0 - 1.0;
  gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);
  v_local_px = a_pos * u_rect_px.zw;
}
)";

// Path vertex shader: tessellated mesh vertices in document pixel space.
// a_pos comes from libtess2 output (2D pixel coordinates). Same NDC mapping
// and Y-flip as kSolidVert but without the unit-quad → rect indirection.
inline constexpr const char* kPathVert = R"(#version 300 es
precision highp float;
in vec2 a_pos;
uniform mat3 u_xform_px;
uniform vec2 u_viewport_px;
out vec2 v_local_px;
void main() {
  vec3 px3 = u_xform_px * vec3(a_pos, 1.0);
  vec2 ndc = (px3.xy / u_viewport_px) * 2.0 - 1.0;
  gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);
  v_local_px = px3.xy;
}
)";

// Solid fragment shader: emits u_color directly. Alpha blending is enabled
// in GLESCanvas::Begin (SRC_ALPHA, ONE_MINUS_SRC_ALPHA).
inline constexpr const char* kSolidFrag = R"(#version 300 es
precision mediump float;
in vec2 v_local_px;
uniform vec4 u_color;
out vec4 frag_color;
void main() {
  frag_color = u_color;
}
)";

// Rounded-rect SDF fragment shader: uses fwidth() for screen-space AA edge.
//
// Distance function (Inigo Quilez rounded-box SDF):
//   d = |p - center| - (half - radius)
//   dist = length(max(d, 0)) + min(max(d.x, d.y), 0) - radius
//
// alpha = 1 - smoothstep(-fwidth(dist), +fwidth(dist), dist)
// fwidth gives the per-pixel rate of change so AA stays 1px wide regardless
// of user transform (zoom / rotation).
inline constexpr const char* kRoundedRectFrag = R"(#version 300 es
precision highp float;
in vec2 v_local_px;
uniform vec4 u_color;
uniform vec2 u_half_px;
uniform float u_radius_px;
out vec4 frag_color;
void main() {
  vec2 d = abs(v_local_px - u_half_px) - (u_half_px - vec2(u_radius_px));
  float dist = length(max(d, 0.0))
             + min(max(d.x, d.y), 0.0)
             - u_radius_px;
  float aa = fwidth(dist);
  float alpha = 1.0 - smoothstep(-aa, aa, dist);
  frag_color = vec4(u_color.rgb, u_color.a * alpha);
}
)";

// -----------------------------------------------------------------------------
// G1.8: Glyph (text) shaders. Same pixel-space → NDC + Y-flip convention as
// kPathVert, plus a per-vertex UV into the GL_R8 coverage atlas. The fragment
// shader samples the single red channel as coverage alpha and tints with
// u_color. Same security contract: compile-time constant, never composed with
// caller data. a_uv binds to attribute location 1 (see LinkProgram).
// -----------------------------------------------------------------------------
inline constexpr const char* kGlyphVert = R"(#version 300 es
precision highp float;
in vec2 a_pos;
in vec2 a_uv;
uniform mat3 u_xform_px;
uniform vec2 u_viewport_px;
out vec2 v_uv;
void main() {
  vec3 px3 = u_xform_px * vec3(a_pos, 1.0);
  vec2 ndc = (px3.xy / u_viewport_px) * 2.0 - 1.0;
  gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);
  v_uv = a_uv;
}
)";

inline constexpr const char* kGlyphFrag = R"(#version 300 es
precision mediump float;
in vec2 v_uv;
uniform vec4 u_color;
uniform sampler2D u_atlas;
out vec4 frag_color;
void main() {
  float coverage = texture(u_atlas, v_uv).r;
  frag_color = vec4(u_color.rgb, u_color.a * coverage);
}
)";

// -----------------------------------------------------------------------------
// G1.9: Image (DrawImage) shaders. Same pixel-space → NDC + Y-flip convention
// as kGlyphVert, with a per-vertex UV into an RGBA8 texture. The fragment
// shader samples the texture directly (premultiply / blending handled by the
// fixed-function GL_BLEND state, D4=A). Same security contract: compile-time
// constant, never composed with caller data. a_uv binds to location 1.
// -----------------------------------------------------------------------------
inline constexpr const char* kImageVert = R"(#version 300 es
precision highp float;
in vec2 a_pos;
in vec2 a_uv;
uniform mat3 u_xform_px;
uniform vec2 u_viewport_px;
out vec2 v_uv;
void main() {
  vec3 px3 = u_xform_px * vec3(a_pos, 1.0);
  vec2 ndc = (px3.xy / u_viewport_px) * 2.0 - 1.0;
  gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);
  v_uv = a_uv;
}
)";

inline constexpr const char* kImageFrag = R"(#version 300 es
precision mediump float;
in vec2 v_uv;
uniform sampler2D u_tex;
out vec4 frag_color;
void main() {
  frag_color = texture(u_tex, v_uv);
}
)";

// -----------------------------------------------------------------------------
// kAllShaderSources[] — B8=A: enumerate every shader for the security
// regression test (shader_injection_test.cc S1 ShaderSourcesAreCompileTimeLiterals).
// Adding a new shader requires only adding an entry here + a comment in the
// shader's docstring. Failure to register triggers no test (silent gap) — the
// test reviewer must visually confirm new shaders are listed.
// -----------------------------------------------------------------------------
inline constexpr const char* kAllShaderSources[] = {
    kPassthroughVert,
    kPassthroughFrag,
    kSolidVert,
    kSolidFrag,
    kRoundedRectFrag,
    kPathVert,
    kGlyphVert,
    kGlyphFrag,
    kImageVert,
    kImageFrag,
};
inline constexpr int kAllShaderSourceCount =
    sizeof(kAllShaderSources) / sizeof(kAllShaderSources[0]);

}  // namespace vx::gfx::gles

#endif  // VELOXA_GRAPHICS_GLES_SHADERS_H_
