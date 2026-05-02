# TASK-20260502-01 A.1.2 / B-A1.1=b: compile-time embed DevTool UI resources
# (inspector_panel.html/css/js → inspector_resources.cc with extern const
# char* exports). Eliminates T2 path-traversal threat surface; UI changes
# require recompile (acceptable for dogfood phase).

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(_embed_inputs
  "${CMAKE_CURRENT_SOURCE_DIR}/inspector_panel.html"
  "${CMAKE_CURRENT_SOURCE_DIR}/inspector_panel.css"
  "${CMAKE_CURRENT_SOURCE_DIR}/inspector_panel.js"
)
set(_embed_names
  "kInspectorPanelHtml"
  "kInspectorPanelCss"
  "kInspectorPanelJs"
)
set(_gen_cc "${CMAKE_CURRENT_BINARY_DIR}/inspector_resources.cc")
set(_embed_script "${CMAKE_CURRENT_SOURCE_DIR}/embed_resources.py")

add_custom_command(
  OUTPUT ${_gen_cc}
  COMMAND ${Python3_EXECUTABLE} ${_embed_script}
          --inputs ${_embed_inputs}
          --names ${_embed_names}
          --output ${_gen_cc}
  DEPENDS ${_embed_inputs} ${_embed_script}
  COMMENT "Embedding DevTool UI resources into ${_gen_cc}"
  VERBATIM
)

add_library(vx_devtool_resources STATIC
  ${_gen_cc}
  inspector_resources.h
)
target_include_directories(vx_devtool_resources PUBLIC ${PROJECT_SOURCE_DIR})
