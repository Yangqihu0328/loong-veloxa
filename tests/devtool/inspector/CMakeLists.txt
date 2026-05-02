include(GoogleTest)

add_executable(inspector_data_test inspector_data_test.cc)
target_link_libraries(inspector_data_test
  PRIVATE vx_devtool vx_core GTest::gtest_main)
gtest_discover_tests(inspector_data_test)

add_executable(inspector_overlay_test inspector_overlay_test.cc)
target_link_libraries(inspector_overlay_test
  PRIVATE vx_devtool vx_core GTest::gtest_main)
gtest_discover_tests(inspector_overlay_test)
