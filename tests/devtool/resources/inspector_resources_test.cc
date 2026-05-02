#include "veloxa/devtool/resources/inspector_resources.h"

#include <gtest/gtest.h>

#include <cstring>

namespace vx::devtool::resources {
namespace {

// =============================================================================
// inspector_resources — DevTool UI HTML/CSS/JS 编译期嵌入
// （TASK-20260502-01 A.1.2 / B-A1.1=b 推翻 B4=B → B4=A）。
//
// 关键约束（plan A.1.2 锁定）:
//   - 3 个 resource 在编译期由 CMake `file(READ)` + Python codegen 嵌入，
//     生成 inspector_resources.cc 含 extern const char* exports
//   - 运行时不读文件 → T2 路径穿越威胁面**完全消除**
//   - libvx_api.a (DEVTOOL=OFF) size diff = 0 (A14 zero-byte guard 维持)
// =============================================================================

TEST(InspectorResourcesTest, HtmlExportedAndNonEmpty) {
  ASSERT_NE(kInspectorPanelHtml, nullptr);
  EXPECT_GT(std::strlen(kInspectorPanelHtml), 0u);
}

TEST(InspectorResourcesTest, CssExportedAndNonEmpty) {
  ASSERT_NE(kInspectorPanelCss, nullptr);
  EXPECT_GT(std::strlen(kInspectorPanelCss), 0u);
}

TEST(InspectorResourcesTest, JsExportedAndNonEmpty) {
  ASSERT_NE(kInspectorPanelJs, nullptr);
  EXPECT_GT(std::strlen(kInspectorPanelJs), 0u);
}

}  // namespace
}  // namespace vx::devtool::resources
