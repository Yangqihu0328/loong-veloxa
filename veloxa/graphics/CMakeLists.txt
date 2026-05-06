find_package(Freetype REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(HARFBUZZ REQUIRED harfbuzz)

add_library(vx_graphics STATIC)

target_sources(vx_graphics PRIVATE
  image.cc
  software/rasterizer.cc
  software/software_path.cc
  software/software_canvas.cc
)

# G1.4: GLES canvas backend (compiled only when VX_RENDERER=gles).
# The gles/ subtree calls GLES functions directly; keep the GLESv2 dependency
# private to vx_graphics so software builds do not pick up GPU link flags.
if(VX_RENDERER STREQUAL "gles")
  target_sources(vx_graphics PRIVATE
    gles/gles_canvas.cc
  )
  pkg_check_modules(VX_GFX_GLESV2 REQUIRED IMPORTED_TARGET glesv2)
  target_link_libraries(vx_graphics PRIVATE PkgConfig::VX_GFX_GLESV2)
endif()

target_include_directories(vx_graphics PUBLIC
  ${CMAKE_SOURCE_DIR}
)
target_include_directories(vx_graphics PRIVATE ${HARFBUZZ_INCLUDE_DIRS})

target_link_libraries(vx_graphics
  PUBLIC vx_foundation vx_platform
  PRIVATE vx_text Freetype::Freetype ${HARFBUZZ_LIBRARIES}
)
target_compile_features(vx_graphics PUBLIC cxx_std_17)
