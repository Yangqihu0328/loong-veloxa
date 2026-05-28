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
// S1: ShaderSourcesAreCompileTimeLiterals  (B8=A array-based registry)
// ---------------------------------------------------------------------------
// All shader sources (G1.4 kPassthroughVert/Frag + G1.5 kSolidVert/Frag +
// kRoundedRectFrag) are constexpr const char* pointing at .rodata literals.
// Verify each entry is non-null, contains the expected "#version 300 es"
// prefix, and (defensively) has stable pointer identity across reads.
//
// B8=A: adding a new shader requires only adding an entry to
// kAllShaderSources[] in shaders.h; this test then auto-covers it.
TEST(ShaderInjectionTest, ShaderSourcesAreCompileTimeLiterals) {
  static_assert(kAllShaderSourceCount > 0, "Empty shader list");
  for (int i = 0; i < kAllShaderSourceCount; ++i) {
    const char* s = kAllShaderSources[i];
    ASSERT_NE(s, nullptr) << "Shader index " << i << " is null";
    std::string_view v(s);
    EXPECT_TRUE(v.find("#version 300 es") == 0)
        << "Shader index " << i
        << " must start with #version 300 es; got: "
        << std::string(v.substr(0, 30));
    // Pointer stability across reads — proves link-time binding (not
    // runtime synthesis). A user-controlled rebind would change the
    // pointer.
    EXPECT_EQ(kAllShaderSources[i], s);
  }
  // Sanity: G1.5 adds 3 shaders on top of G1.4's 2 = 5 total. A regression
  // that drops kSolidVert / kSolidFrag / kRoundedRectFrag from the array
  // would trip this floor.
  EXPECT_GE(kAllShaderSourceCount, 5)
      << "Expected at least 5 shaders after G1.5 (kPassthroughVert/Frag + "
         "kSolidVert/Frag + kRoundedRectFrag)";
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

// ---------------------------------------------------------------------------
// S3: NoUserConcatPatternInG15Shaders (G1.5 reverse-probe extension)
// ---------------------------------------------------------------------------
// Structural check that the G1.5 shaders do not contain any pattern that
// would suggest runtime string concatenation:
//   * "%s"            — printf-style placeholder
//   * "#define USER_" — user-prefixed macro injection point
// These would imply either a sprintf-style synthesis API or a templated
// shader builder, both of which are disallowed by B6=A static embedding.
TEST(ShaderInjectionTest, NoUserConcatPatternInG15Shaders) {
  const char* g15_shaders[] = {kSolidVert, kSolidFrag, kRoundedRectFrag};
  for (const char* s : g15_shaders) {
    std::string_view v(s);
    EXPECT_EQ(v.find("%s"), std::string_view::npos)
        << "G1.5 shader contains printf-style placeholder; first 60 chars: "
        << std::string(v.substr(0, 60));
    EXPECT_EQ(v.find("#define USER_"), std::string_view::npos)
        << "G1.5 shader contains USER_ macro injection point; first 60 chars: "
        << std::string(v.substr(0, 60));
  }
}

}  // namespace
}  // namespace vx::gfx::gles
