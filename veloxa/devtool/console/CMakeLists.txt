# TASK-20260503-04 D.1 + D.2 — DevTool Console subsystem.
#
# Cross-subsystem append pattern (same as overlay/perf_overlay.cc and
# hot_reload/*.cc above in veloxa/devtool/CMakeLists.txt). Sources are
# appended to the parent vx_devtool target rather than producing a
# standalone library — keeps link topology aligned with Phase A/B/C and
# A14 enforcement (DEVTOOL=OFF leaves vx_devtool unlinked, so these
# symbols never reach libvx_api.a).
#
# D.1: console_engine.cc — isolated QuickjsEngine wrap with default-safe
#      T1 budget. RegisterConsoleBindings (D.2) is a separate file added
#      here when D.2 lands.

target_sources(vx_devtool PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/console_engine.cc
)
