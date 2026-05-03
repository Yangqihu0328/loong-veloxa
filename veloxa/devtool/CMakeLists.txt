add_subdirectory(resources)
add_subdirectory(inspector)

# TASK-20260502-01 A.1.5: InputDispatchSplitter — input dispatch layer
# component shared across all DevTool subsystems (Inspector / Performance
# Overlay / Hot Reload). Lives at the devtool/ root rather than inside
# inspector/ to reflect its cross-subsystem scope.
target_sources(vx_devtool PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/input_dispatch_splitter.cc
)

# TASK-20260502-02 B.1.1: PerfOverlay FrameStats ring buffer + 60-frame
# sliding aggregate + FPS computation (pure algorithm). Append to vx_devtool
# library (same target_sources pattern as input_dispatch_splitter.cc above
# — avoids creating a new sub-library for a single small module).
target_sources(vx_devtool PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/overlay/perf_overlay.cc
)

# TASK-20260503-01 C.0.1 + C.1.1: Hot Reload file watcher. Cross-subsystem
# append pattern (same as overlay/perf_overlay.cc above). Linux backend
# `file_watcher_inotify.cc` is unconditionally compiled — its top-level
# `#if defined(__linux__)` guard means it produces an empty TU on other
# platforms while keeping the CMake target list portable.
target_sources(vx_devtool PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/hot_reload/file_watcher.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/hot_reload/file_watcher_inotify.cc
)
