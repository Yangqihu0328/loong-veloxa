# =============================================================================
# VxRenderer.cmake — TASK-20260505-05 G1.1 [VX_RENDERER 渲染后端 flag]
#
# Spec §3.5 V5=A: "VX_RENDERER=software|gles CMake flag / SoftwareCanvas 作 fallback"
# Plan B5=A:      "software default / 不退化既有 ctest baseline"
#
# Single-source-of-truth for the VX_RENDERER option:
#   - declares the cache STRING with allowed values {software, gles}
#   - validates user-supplied value (FATAL_ERROR on unknown)
#   - emits VX_RENDERER_SOFTWARE=1 OR VX_RENDERER_GLES=1 compile definition
#   - prints a STATUS line with the active backend
#
# Included by:
#   - top-level CMakeLists.txt (real build)
#   - tests/smoke/vx_renderer_flag_check.cmake stub probe (4-scenario smoke)
#
# G1.1 explicitly does NOT introduce OpenGLES/EGL deps (D1=A YAGNI). The
# `gles` branch only emits a compile definition; actual GPU dep + canvas
# implementation arrive with G1.2 GLESDisplay + Sdl2EGLDisplay.
# =============================================================================

set(VX_RENDERER "software" CACHE STRING "Renderer backend (software|gles)")
set_property(CACHE VX_RENDERER PROPERTY STRINGS software gles)

if(NOT VX_RENDERER MATCHES "^(software|gles)$")
  message(FATAL_ERROR
    "VX_RENDERER must be 'software' or 'gles', got: '${VX_RENDERER}'")
endif()

if(VX_RENDERER STREQUAL "gles")
  add_compile_definitions(VX_RENDERER_GLES=1)
  message(STATUS "VX_RENDERER = gles [VX_RENDERER_GLES=1] (OpenGL ES 3.0+ canvas — G1.2+ implementation)")
else()
  add_compile_definitions(VX_RENDERER_SOFTWARE=1)
  message(STATUS "VX_RENDERER = software [VX_RENDERER_SOFTWARE=1] (CPU rasterizer canvas)")
endif()
