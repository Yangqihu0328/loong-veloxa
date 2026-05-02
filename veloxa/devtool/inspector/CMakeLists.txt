add_library(vx_devtool STATIC
  inspector_data.cc
  inspector_overlay.cc
)
target_include_directories(vx_devtool PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(vx_devtool PUBLIC vx_core)
