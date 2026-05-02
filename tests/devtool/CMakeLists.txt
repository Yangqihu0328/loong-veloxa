add_subdirectory(inspector)
add_subdirectory(overlay)
add_subdirectory(resources)

# TASK-20260502-01 A.1.5: InputDispatchSplitter pure state-machine tests.
include(GoogleTest)
add_executable(input_dispatch_splitter_test input_dispatch_splitter_test.cc)
target_link_libraries(input_dispatch_splitter_test
  PRIVATE vx_devtool vx_core GTest::gtest_main)
gtest_discover_tests(input_dispatch_splitter_test)
