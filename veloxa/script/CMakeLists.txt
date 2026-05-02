include(FetchContent)

set(QJS_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(QJS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  quickjsng
  GIT_REPOSITORY https://github.com/quickjs-ng/quickjs.git
  GIT_TAG v0.14.0
)
FetchContent_MakeAvailable(quickjsng)

add_library(vx_script STATIC
  quickjs_engine.cc
  dom_bindings.cc
)
target_include_directories(vx_script PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(vx_script PUBLIC vx_foundation)
target_link_libraries(vx_script PRIVATE qjs qjs-libc)
# vx_script needs css::EnumValueToCssString (provided by vx_core).
# vx_core ↔ vx_script form a circular static-library dependency; CMake handles
# this by repeating both archives on the link command line.
target_link_libraries(vx_script PRIVATE vx_core)

# TASK-20260502-01 A.1.4: when DevTool is enabled, vx_script's
# RegisterDevtoolBindings calls vx::devtool::SerializeDocument from
# vx_devtool. Conditional link + compile-define keeps the OFF binary
# untouched (A14 zero-byte guard).
if(VX_BUILD_DEVTOOL)
  target_compile_definitions(vx_script PUBLIC VX_BUILD_DEVTOOL)
  target_link_libraries(vx_script PRIVATE vx_devtool)
endif()
