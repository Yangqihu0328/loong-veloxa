# =============================================================================
# vx_renderer_flag_check.cmake — TASK-20260505-05 G1.1 [VX_RENDERER 守门]
#
# Spec §3.5 V5=A: "VX_RENDERER=software|gles CMake flag / SoftwareCanvas 作 fallback"
# Plan B5=A:      "software default / 不退化既有 ctest baseline"
#
# 4 scenarios validated against cmake/VxRenderer.cmake (single source of
# truth, included from both the real top-level CMakeLists.txt and the
# stub probe project below):
#   1. default   (no -DVX_RENDERER)        → configure success + VX_RENDERER_SOFTWARE
#   2. software  (-DVX_RENDERER=software)  → configure success + VX_RENDERER_SOFTWARE
#   3. gles      (-DVX_RENDERER=gles)      → configure success + VX_RENDERER_GLES
#                                             (G1.1 does NOT link GLES yet — D1=A YAGNI)
#   4. invalid   (-DVX_RENDERER=invalid)   → configure FAILS (FATAL_ERROR / D5=A negative probe)
#
# Each scenario instantiates a 5-line stub project that ONLY includes
# cmake/VxRenderer.cmake — sub-second per scenario, ~5s total smoke runtime.
#
# Required CMake-defined variables (-D on the cmake -P invocation):
#   SOURCE_DIR       — absolute path to project root
#   BUILD_DIR_BASE   — absolute path to a writable scratch dir for sub-configs
# =============================================================================

if(NOT DEFINED SOURCE_DIR)
  message(FATAL_ERROR "VX_RENDERER smoke: SOURCE_DIR not provided")
endif()
if(NOT DEFINED BUILD_DIR_BASE)
  message(FATAL_ERROR "VX_RENDERER smoke: BUILD_DIR_BASE not provided")
endif()
if(NOT EXISTS "${SOURCE_DIR}/cmake/VxRenderer.cmake")
  message(FATAL_ERROR "VX_RENDERER smoke: ${SOURCE_DIR}/cmake/VxRenderer.cmake missing")
endif()

# Drift guard: the top-level CMakeLists.txt MUST include VxRenderer.cmake,
# otherwise the option is well-defined but not actually consumed by the real
# build. The 4-scenario sub-probes below exercise VxRenderer.cmake in
# isolation; this static grep ensures the integration is wired up.
file(READ "${SOURCE_DIR}/CMakeLists.txt" _top_cml_content)
if(NOT _top_cml_content MATCHES "include.*VxRenderer\\.cmake")
  message(FATAL_ERROR
    "VX_RENDERER smoke: top-level CMakeLists.txt does NOT include "
    "cmake/VxRenderer.cmake — VX_RENDERER flag will silently default. "
    "Add `include(\${CMAKE_SOURCE_DIR}/cmake/VxRenderer.cmake)` near the "
    "other option(...) declarations.")
endif()

set(_scratch "${BUILD_DIR_BASE}/_vx_renderer_smoke")
file(REMOVE_RECURSE "${_scratch}")
file(MAKE_DIRECTORY "${_scratch}")

# Stub probe: minimal CMakeLists.txt that only exercises VxRenderer.cmake.
file(WRITE "${_scratch}/CMakeLists.txt" "\
cmake_minimum_required(VERSION 3.20)
project(vx_renderer_probe LANGUAGES NONE)
include(${SOURCE_DIR}/cmake/VxRenderer.cmake)
")

function(_vx_run_scenario scenario flag_arg expected_def expect_fail)
  set(probe_dir "${_scratch}/${scenario}")
  file(MAKE_DIRECTORY "${probe_dir}")

  if("${flag_arg}" STREQUAL "")
    execute_process(
      COMMAND ${CMAKE_COMMAND} -B "${probe_dir}" -S "${_scratch}"
      RESULT_VARIABLE rc
      OUTPUT_VARIABLE out
      ERROR_VARIABLE err)
  else()
    execute_process(
      COMMAND ${CMAKE_COMMAND} ${flag_arg} -B "${probe_dir}" -S "${_scratch}"
      RESULT_VARIABLE rc
      OUTPUT_VARIABLE out
      ERROR_VARIABLE err)
  endif()

  if(expect_fail)
    if(rc EQUAL 0)
      message(FATAL_ERROR
        "VX_RENDERER smoke: scenario=${scenario} expected FAIL but got rc=0\n"
        "stdout:\n${out}\nstderr:\n${err}")
    endif()
    if(NOT err MATCHES "VX_RENDERER must be")
      message(FATAL_ERROR
        "VX_RENDERER smoke: scenario=${scenario} failed but error message did NOT "
        "contain 'VX_RENDERER must be' — guard removed?\nstderr:\n${err}")
    endif()
    message(STATUS "VX_RENDERER smoke: scenario=${scenario} ✓ (rejected with rc=${rc})")
  else()
    if(NOT rc EQUAL 0)
      message(FATAL_ERROR
        "VX_RENDERER smoke: scenario=${scenario} expected PASS but got rc=${rc}\n"
        "stderr (last lines):\n${err}")
    endif()
    if(NOT out MATCHES "${expected_def}")
      message(FATAL_ERROR
        "VX_RENDERER smoke: scenario=${scenario} expected compile def "
        "'${expected_def}' to appear in cmake STATUS output, but it didn't.\n"
        "stdout (tail):\n${out}")
    endif()
    message(STATUS "VX_RENDERER smoke: scenario=${scenario} ✓ (${expected_def} active)")
  endif()
endfunction()

_vx_run_scenario(default  ""                       "VX_RENDERER_SOFTWARE" FALSE)
_vx_run_scenario(software "-DVX_RENDERER=software" "VX_RENDERER_SOFTWARE" FALSE)
_vx_run_scenario(gles     "-DVX_RENDERER=gles"     "VX_RENDERER_GLES"     FALSE)
_vx_run_scenario(invalid  "-DVX_RENDERER=invalid"  ""                     TRUE)

message(STATUS "VX_RENDERER smoke: 4/4 scenarios pass ✓ "
               "(default + software + gles + invalid-rejected)")

file(REMOVE_RECURSE "${_scratch}")
