# TASK-20260503-01 C.0.1: FileWatcher abstract base unit tests. Validates
# CreatePlatform() factory + IsRunning() initial state contract. Compiled
# only when VX_BUILD_DEVTOOL=ON (parent gate via tests/devtool/).
include(GoogleTest)

add_executable(file_watcher_test file_watcher_test.cc)
target_link_libraries(file_watcher_test
  PRIVATE vx_devtool vx_core GTest::gtest_main)
gtest_discover_tests(file_watcher_test)
