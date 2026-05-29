# LibTess2 — FetchContent wrapper (G1.6 / TASK-20260529-01).
#
# Upstream memononen/libtess2 ships Bazel/premake only (no CMakeLists.txt).
# We populate sources and build a static tess2 target aligned with BUILD.bazel.

include(FetchContent)

if(NOT CMAKE_C_COMPILER_LOADED)
  enable_language(C)
endif()

FetchContent_Declare(
  libtess2
  GIT_REPOSITORY https://github.com/memononen/libtess2.git
  GIT_TAG        v1.0.2
  GIT_SHALLOW    TRUE
)

FetchContent_GetProperties(libtess2)
if(NOT libtess2_POPULATED)
  FetchContent_Populate(libtess2)
endif()

if(NOT TARGET tess2)
  add_library(tess2 STATIC
    ${libtess2_SOURCE_DIR}/Source/bucketalloc.c
    ${libtess2_SOURCE_DIR}/Source/dict.c
    ${libtess2_SOURCE_DIR}/Source/geom.c
    ${libtess2_SOURCE_DIR}/Source/mesh.c
    ${libtess2_SOURCE_DIR}/Source/priorityq.c
    ${libtess2_SOURCE_DIR}/Source/sweep.c
    ${libtess2_SOURCE_DIR}/Source/tess.c
  )
  target_include_directories(tess2 PUBLIC
    ${libtess2_SOURCE_DIR}/Include
  )
  # Third-party C sources — suppress Veloxa -Werror noise.
  if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(tess2 PRIVATE -w)
  endif()
endif()
