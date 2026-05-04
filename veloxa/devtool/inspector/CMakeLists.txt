add_library(vx_devtool STATIC
  inspector_data.cc
  inspector_overlay.cc
)
target_include_directories(vx_devtool PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(vx_devtool PUBLIC vx_core vx_devtool_resources)

# TASK-20260503-01 C.1.1: InotifyFileWatcher uses std::thread for its
# worker loop. find_package(Threads) sets up the right pthread linker
# flag for the host toolchain. PRIVATE — only the library implementation
# needs the link, downstream consumers do not propagate the dependency.
find_package(Threads REQUIRED)
target_link_libraries(vx_devtool PRIVATE Threads::Threads)

# TASK-20260503-04 D.2: console/console_bindings.cc uses QuickJS C ABI
# directly to install Console capability allowlist on a fresh JSContext.
# PRIVATE — vx_script already brings qjs into the link closure for
# downstream targets, this is only for vx_devtool's own translation units.
target_link_libraries(vx_devtool PRIVATE qjs)
