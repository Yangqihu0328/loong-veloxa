# TASK-20260502-02 B.1.1: PerfOverlay FrameStats ring buffer + 60-frame
# sliding aggregate + FPS computation. Pure algorithm unit tests, zero
# external dependencies. Compiled only when VX_BUILD_DEVTOOL=ON.
include(GoogleTest)
add_executable(perf_overlay_test perf_overlay_test.cc)
target_link_libraries(perf_overlay_test
  PRIVATE vx_devtool vx_core vx_foundation GTest::gtest_main)
gtest_discover_tests(perf_overlay_test)
