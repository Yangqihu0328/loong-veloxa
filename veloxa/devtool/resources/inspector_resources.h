#ifndef VELOXA_DEVTOOL_RESOURCES_INSPECTOR_RESOURCES_H_
#define VELOXA_DEVTOOL_RESOURCES_INSPECTOR_RESOURCES_H_

// TASK-20260502-01 A.1.2 / B-A1.1=b: DevTool UI resources are embedded at
// compile time (CMake `file(READ)` + Python codegen → inspector_resources.cc
// generated, exporting these 3 const char* pointers). Runtime never reads
// any file → T2 path-traversal threat surface eliminated entirely.
//
// A.1.3 replaces the placeholder content of the 3 source files in
// veloxa/devtool/resources/. A.1.4 wires the JS to native bindings.
//
// A14 zero-byte guard: when VX_BUILD_DEVTOOL=OFF, vx_devtool_resources is
// never linked → libvx_api.a size diff = 0.

namespace vx::devtool::resources {

extern const char* const kInspectorPanelHtml;
extern const char* const kInspectorPanelCss;
extern const char* const kInspectorPanelJs;

// TASK-20260503-04 D.3 — Console panel resources (4th DevTool sub-pane).
// Injected via __INLINE_CONSOLE_HTML__ / __INLINE_CONSOLE_CSS__ /
// __INLINE_CONSOLE_JS__ placeholders inside inspector_panel.html, same
// embed pipeline + same A14 zero-byte-OFF guard.
extern const char* const kConsolePanelHtml;
extern const char* const kConsolePanelCss;
extern const char* const kConsolePanelJs;

}  // namespace vx::devtool::resources

#endif  // VELOXA_DEVTOOL_RESOURCES_INSPECTOR_RESOURCES_H_
