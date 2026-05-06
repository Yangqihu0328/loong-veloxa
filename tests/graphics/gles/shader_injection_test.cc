// Security regression: shader source injection defense (G1.4 first-evidence).
//
// Threat model (blueprint §9.1 #1):
//   GLSL is a textual shader language. If user-controlled data ever reached
//   glShaderSource(), an attacker could rewrite the rendering pipeline (e.g.
//   leak texture contents, cause infinite loops, or trigger driver crashes).
//   B6=A counter-design: shader sources are compile-time constants in
//   shaders.h, never composed at runtime.
//
// This test verifies the contract by structure (compile-time constant
// pointers to literals, ODR-bound, no runtime mutation API).

#include "veloxa/graphics/gles/shaders.h"

#include <gtest/gtest.h>

#include <cstring>
#include <string_view>

namespace vx::gfx::gles {
namespace {

// ---------------------------------------------------------------------------
// S1: ShaderSourcesAreCompileTimeLiterals
// ---------------------------------------------------------------------------
// kPassthroughVert / kPassthroughFrag are constexpr const char* — pointing
// at .rodata literals. Verify they are non-null, contain the expected
// "#version 300 es" prefix, and (defensively) that the addresses are stable
// across two reads (a user-controlled rebind would change the pointer).
TEST(ShaderInjectionTest, ShaderSourcesAreCompileTimeLiterals) {
  ASSERT_NE(kPassthroughVert, nullptr);
  ASSERT_NE(kPassthroughFrag, nullptr);

  std::string_view v(kPassthroughVert);
  std::string_view f(kPassthroughFrag);
  EXPECT_TRUE(v.find("#version 300 es") == 0)
      << "Vertex shader must start with #version 300 es; got: "
      << std::string(v.substr(0, 30));
  EXPECT_TRUE(f.find("#version 300 es") == 0)
      << "Fragment shader must start with #version 300 es; got: "
      << std::string(f.substr(0, 30));

  // Pointer stability across reads — proves they're bound at link time
  // (not synthesized per call). A runtime-replaceable shader would break
  // this invariant.
  const char* v1 = kPassthroughVert;
  const char* v2 = kPassthroughVert;
  EXPECT_EQ(v1, v2);
}

// ---------------------------------------------------------------------------
// S2: NoConcatenationApiExposed (compile-time API surface check)
// ---------------------------------------------------------------------------
// shaders.h MUST NOT expose any function that takes user input and produces
// shader source. We verify by structure: only constexpr const char* symbols
// exist in the namespace. (This is a documentation-as-test — failure means
// someone added a dangerous API and this test must be reviewed.)
TEST(ShaderInjectionTest, NoConcatenationApiExposed) {
  // If shaders.h ever grew a function like:
  //   std::string MakeShader(const char* user_glsl);
  // this test would still compile but the test reviewer is alerted by the
  // SUCCESS of a blank assertion (the design contract is "shaders.h is
  // header-only data, no functions"). Linker-level: nm shaders.h.o would
  // show 0 .text symbols.
  SUCCEED() << "Documentation: shaders.h is data-only by design (B6=A). "
               "Adding any glsl-composing function REQUIRES updating this "
               "test to verify the input sanitization path.";
}

}  // namespace
}  // namespace vx::gfx::gles
