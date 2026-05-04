# TASK-20260503-04 D.1 + D.2 + D.5 — DevTool Console subsystem unit tests.
# Compiled only when VX_BUILD_DEVTOOL=ON (parent gate via tests/devtool/).
#
# - console_engine_test (D.1): isolated runtime + default-safe T1 budget +
#   opt-in tighter override + reset-per-eval semantics + Shutdown idempotence.
#   D.2 will add console_bindings_test.cc; D.5 will add T1 mitigation 5-dim
#   completeness coverage.
include(GoogleTest)

add_executable(console_engine_test console_engine_test.cc)
target_link_libraries(console_engine_test
  PRIVATE vx_devtool vx_core GTest::gtest_main)
gtest_discover_tests(console_engine_test)
