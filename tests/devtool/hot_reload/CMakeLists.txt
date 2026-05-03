# TASK-20260503-01 C.0.1 + C.1.1 + C.1.2 + C.1.3 + C.2.1 + C.5.1: Hot
# Reload unit tests. Compiled only when VX_BUILD_DEVTOOL=ON (parent gate
# via tests/devtool/).
#
# - file_watcher_test: FileWatcher interface + InotifyFileWatcher Linux
#   backend (lifecycle + 8 T2 path-traversal guards + 4 edge cases)
# - hot_reload_manager_test: HotReloadManager bridge between FileWatcher
#   worker thread and Application::LoadCSS main thread (R9 F-025 strict
#   contract reverse probe + DOM state preservation + LoadCSS dispatch)
# - security_test (C.5.1): T2 path-traversal complete dual-probe matrix
#   (8 guards × {forward, reverse} = 16 tests). Independent file from
#   file_watcher_test for archaeological clarity — security expectations
#   live in their own dedicated suite for audit trail purposes.
include(GoogleTest)

add_executable(file_watcher_test file_watcher_test.cc)
target_link_libraries(file_watcher_test
  PRIVATE vx_devtool vx_core GTest::gtest_main)
gtest_discover_tests(file_watcher_test)

add_executable(hot_reload_manager_test hot_reload_manager_test.cc)
target_link_libraries(hot_reload_manager_test
  PRIVATE vx_devtool vx_core GTest::gtest_main)
gtest_discover_tests(hot_reload_manager_test)

add_executable(security_test security_test.cc)
target_link_libraries(security_test
  PRIVATE vx_devtool vx_core GTest::gtest_main)
gtest_discover_tests(security_test)
