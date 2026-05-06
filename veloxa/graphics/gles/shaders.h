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

}  // namespace vx::gfx::gles

#endif  // VELOXA_GRAPHICS_GLES_SHADERS_H_
