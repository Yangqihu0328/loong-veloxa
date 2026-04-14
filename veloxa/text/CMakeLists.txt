find_package(Freetype REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(HARFBUZZ REQUIRED harfbuzz)

add_library(vx_text STATIC
  font_manager.cc
  glyph_cache.cc
  freetype_shaper.cc
)
target_include_directories(vx_text PUBLIC ${CMAKE_SOURCE_DIR})
target_include_directories(vx_text PRIVATE ${HARFBUZZ_INCLUDE_DIRS})
target_link_libraries(vx_text
  PUBLIC vx_foundation
  PRIVATE Freetype::Freetype ${HARFBUZZ_LIBRARIES}
)
set_target_properties(vx_text PROPERTIES CXX_STANDARD 17)
