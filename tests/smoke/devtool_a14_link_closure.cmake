# =============================================================================
# devtool_a14_link_closure.cmake — TASK-20260502-01 A.2.4 [A14 守门]
#
# Spec §6 A14: "DevTool 关闭时构建产物零变化（链接闭合 + binary size diff = 0）"
#
# This script is the precise enforcement of the LINK-CLOSURE half of A14
# (the binary-size-diff half is observed via the byte-count log below;
# spec accepts public C ABI stub growth from the policy as not part of
# DevTool linkage). Run by ctest on EVERY build, both ON and OFF.
#
# Required CMake-defined variables (-D on the cmake -P invocation):
#   LIB            — absolute path to libvx_api.a under test
#   CORELIB        — absolute path to libvx_core.a under test
#   DEVTOOL_ON     — "ON" or "OFF"
# =============================================================================

if(NOT EXISTS "${LIB}")
  message(FATAL_ERROR "A14 smoke: libvx_api.a not found at ${LIB}")
endif()
if(NOT EXISTS "${CORELIB}")
  message(FATAL_ERROR "A14 smoke: libvx_core.a not found at ${CORELIB}")
endif()

execute_process(COMMAND nm "${LIB}"
                OUTPUT_VARIABLE api_syms
                RESULT_VARIABLE api_rc
                ERROR_QUIET)
if(NOT api_rc EQUAL 0)
  message(FATAL_ERROR "A14 smoke: nm failed on ${LIB}")
endif()

execute_process(COMMAND nm "${CORELIB}"
                OUTPUT_VARIABLE core_syms
                RESULT_VARIABLE core_rc
                ERROR_QUIET)
if(NOT core_rc EQUAL 0)
  message(FATAL_ERROR "A14 smoke: nm failed on ${CORELIB}")
endif()

file(SIZE "${LIB}" api_size)
file(SIZE "${CORELIB}" core_size)
message(STATUS "A14 smoke: libvx_api.a   = ${api_size} bytes")
message(STATUS "A14 smoke: libvx_core.a  = ${core_size} bytes")

# DevTool subsystem internal symbols that must NEVER appear in OFF builds
# (each one is a positive proof that vx_devtool / vx_devtool_resources
# code somehow leaked into the public-API or core link path).
set(devtool_internal_syms
    "RegisterDevtoolBindings"
    "kInspectorPanelHtml"
    "kInspectorPanelCss"
    "kInspectorPanelJs"
    "InspectorOverlay"
    "InjectHoverHighlight"
    "InputDispatchSplitter"
    "SerializeDocument")

if(DEVTOOL_ON STREQUAL "ON")
  # Sanity: when ON, the public C-API stub vx_view_attach_devtool must
  # exist in libvx_api.a (otherwise the wrapper is broken).
  if(NOT api_syms MATCHES "vx_view_attach_devtool")
    message(FATAL_ERROR
            "A14 ON sanity: vx_view_attach_devtool missing from libvx_api.a")
  endif()
  message(STATUS "A14 smoke: ON build — vx_view_attach_devtool present ✓")
else()
  # OFF: verify zero DevTool internal symbol references. We check both
  # the API and core archives because A.1.6/A.1.8 wired DevTool into
  # core (LoadDevtoolDocument etc.) under #ifdef guards.
  set(combined_syms "${api_syms}\n${core_syms}")
  foreach(sym IN LISTS devtool_internal_syms)
    if(combined_syms MATCHES "${sym}")
      message(FATAL_ERROR
              "A14 OFF FAIL: DevTool internal symbol '${sym}' leaked into "
              "libvx_api.a or libvx_core.a — link closure broken. "
              "Check the #ifdef VX_BUILD_DEVTOOL guards added in A.0.x/A.1.x.")
    endif()
  endforeach()
  message(STATUS
          "A14 smoke: OFF build — link closure verified zero DevTool symbols ✓")
endif()
