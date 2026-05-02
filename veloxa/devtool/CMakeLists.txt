add_subdirectory(resources)
add_subdirectory(inspector)

# TASK-20260502-01 A.1.5: InputDispatchSplitter — input dispatch layer
# component shared across all DevTool subsystems (Inspector / Performance
# Overlay / Hot Reload). Lives at the devtool/ root rather than inside
# inspector/ to reflect its cross-subsystem scope.
target_sources(vx_devtool PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/input_dispatch_splitter.cc
)
