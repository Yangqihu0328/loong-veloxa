add_library(vx_api STATIC veloxa_api.cc)
target_include_directories(vx_api PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(vx_api PUBLIC vx_core vx_graphics vx_platform vx_script)
