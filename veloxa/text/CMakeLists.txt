find_package(Freetype REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(HARFBUZZ REQUIRED harfbuzz)

add_library(vx_text STATIC
  font_manager.cc
  glyph_cache.cc
  freetype_shaper.cc
  shape_cache.cc
  hb_buffer_holder.cc
)
target_include_directories(vx_text PUBLIC ${CMAKE_SOURCE_DIR})
# HarfBuzz + Freetype are propagated PUBLIC so downstream test targets
# that include ft2build.h / hb.h (e.g. tests/text/font_manager_test.cc
# exercising GetHbFont) do not need to re-invoke pkg_check_modules.
# vx_graphics still links these directly because it uses the APIs too.
target_include_directories(vx_text PUBLIC ${HARFBUZZ_INCLUDE_DIRS})
target_link_libraries(vx_text
  PUBLIC vx_foundation Freetype::Freetype ${HARFBUZZ_LIBRARIES}
)
set_target_properties(vx_text PROPERTIES CXX_STANDARD 17)
