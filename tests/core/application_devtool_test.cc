#include "veloxa/core/application.h"

#include <gtest/gtest.h>

#include "veloxa/core/dom/serializer.h"
#include "veloxa/core/render/paint_command.h"
#include "veloxa/graphics/types.h"
#include "veloxa/platform/headless/headless_event_loop.h"
#include "veloxa/platform/headless/memory_surface.h"

namespace vx {
namespace {

// =============================================================================
// application_devtool_test — TASK-20260502-01 A.1.6
//
// Verifies the dual-UpdateManager + LoadDevtoolDocument plumbing:
//   - LoadDevtoolDocument() creates the DevTool Document by parsing the
//     embedded inspector_panel.html resource (A.1.2) with __INLINE_CSS__ /
//     __INLINE_JS__ placeholders runtime-replaced by kInspectorPanelCss /
//     kInspectorPanelJs (A.1.3 R2-verified content).
//   - Application now owns TWO UpdateManagers: target_update_manager_
//     (legacy update_manager_) drives the user-facing Document, and
//     devtool_update_manager_ drives the dogfood UI Document. They share
//     the same canvas + image_cache; the DevTool one renders with a
//     canvas Translate offset to land on the right-hand splitter dock
//     (creative #1 decision 1, B-A1.2=a full-viewport overlay model).
//   - UpdateManager exposes a mutable display_list getter so
//     InspectorOverlay::InjectHoverHighlight (A.1.1) can append the
//     overlay PaintCommand from outside the UpdateManager.
// =============================================================================

constexpr u32 kW = 800, kH = 600;

class ApplicationDevtoolTest : public ::testing::Test {
 protected:
  void SetUp() override {
    surface_ = std::make_unique<platform::MemorySurface>(kW, kH);
    event_loop_ = std::make_unique<platform::HeadlessEventLoop>();
  }

  Application::Config MakeConfig() {
    Application::Config cfg;
    cfg.event_loop = event_loop_.get();
    cfg.surface = surface_.get();
    cfg.target_fps = 60;
    return cfg;
  }

  std::unique_ptr<platform::MemorySurface> surface_;
  std::unique_ptr<platform::HeadlessEventLoop> event_loop_;
};

// -----------------------------------------------------------------------------
// LoadDevtoolDocument / UnloadDevtoolDocument
// -----------------------------------------------------------------------------

TEST_F(ApplicationDevtoolTest, LoadDevtoolDocumentSetsLoadedFlag) {
  Application app(MakeConfig());
  EXPECT_FALSE(app.devtool_loaded());
  EXPECT_EQ(app.devtool_document(), nullptr);

  ASSERT_TRUE(app.LoadDevtoolDocument(/*devtool_width=*/270.0f));
  EXPECT_TRUE(app.devtool_loaded());
  EXPECT_NE(app.devtool_document(), nullptr);
}

TEST_F(ApplicationDevtoolTest, LoadDevtoolDocumentReplacesCssPlaceholder) {
  // The embedded inspector_panel.html contains __INLINE_CSS__ which
  // LoadDevtoolDocument must runtime-replace with kInspectorPanelCss
  // contents BEFORE handing the HTML to the parser. We verify by
  // serializing the parsed devtool_document and grepping for a CSS
  // declaration that appears in inspector_panel.css but NOT in
  // inspector_panel.html.
  Application app(MakeConfig());
  ASSERT_TRUE(app.LoadDevtoolDocument(270.0f));

  const String html_dump = dom::Serialize(app.devtool_document());
  // "display: flex" appears in the CSS file (#devtool-root rule), not in
  // the HTML skeleton. If placeholder replacement worked, the inline
  // <style> block now contains it.
  StringView dump_view = html_dump.view();
  bool has_flex = false;
  StringView needle("display: flex");
  for (usize i = 0; i + needle.size() <= dump_view.size(); ++i) {
    if (StringView(dump_view.data() + i, needle.size()) == needle) {
      has_flex = true;
      break;
    }
  }
  EXPECT_TRUE(has_flex)
      << "DevTool document HTML dump did not contain inlined CSS";
}

TEST_F(ApplicationDevtoolTest, LoadDevtoolDocumentReplacesJsPlaceholder) {
  // Symmetric to the CSS case: __INLINE_JS__ must be replaced with
  // kInspectorPanelJs contents. We grep for a JS function name that
  // only appears in inspector_panel.js.
  Application app(MakeConfig());
  ASSERT_TRUE(app.LoadDevtoolDocument(270.0f));

  const String html_dump = dom::Serialize(app.devtool_document());
  StringView dump_view = html_dump.view();
  bool has_render = false;
  StringView needle("renderDomTree");
  for (usize i = 0; i + needle.size() <= dump_view.size(); ++i) {
    if (StringView(dump_view.data() + i, needle.size()) == needle) {
      has_render = true;
      break;
    }
  }
  EXPECT_TRUE(has_render)
      << "DevTool document HTML dump did not contain inlined JS";
}

TEST_F(ApplicationDevtoolTest, UnloadDevtoolDocumentClearsState) {
  Application app(MakeConfig());
  ASSERT_TRUE(app.LoadDevtoolDocument(270.0f));
  ASSERT_TRUE(app.devtool_loaded());

  app.UnloadDevtoolDocument();
  EXPECT_FALSE(app.devtool_loaded());
  EXPECT_EQ(app.devtool_document(), nullptr);
}

TEST_F(ApplicationDevtoolTest, ReloadDevtoolDocumentReplacesPriorInstance) {
  // Idempotency: calling LoadDevtoolDocument twice without unload in
  // between must not leak the prior Document — second call replaces.
  Application app(MakeConfig());
  ASSERT_TRUE(app.LoadDevtoolDocument(270.0f));
  dom::Document* first = app.devtool_document();
  ASSERT_NE(first, nullptr);

  ASSERT_TRUE(app.LoadDevtoolDocument(270.0f));
  EXPECT_NE(app.devtool_document(), nullptr);
  // Cannot assert pointer inequality safely (allocator may reuse the
  // address); we only assert the load succeeded twice.
}

// -----------------------------------------------------------------------------
// Dual UpdateManager: target + devtool layout trees both built on Update()
// -----------------------------------------------------------------------------

TEST_F(ApplicationDevtoolTest, UpdateBuildsBothLayoutTrees) {
  Application app(MakeConfig());
  app.LoadHTML("<div>Hello Target</div>");
  ASSERT_TRUE(app.LoadDevtoolDocument(270.0f));

  app.Update();
  // Target update manager has built its layout tree.
  ASSERT_NE(app.update_manager(), nullptr);
  EXPECT_NE(app.update_manager()->layout_root(), nullptr);
  // DevTool update manager has built its layout tree (for inspector_panel).
  ASSERT_NE(app.devtool_update_manager(), nullptr);
  EXPECT_NE(app.devtool_update_manager()->layout_root(), nullptr);
}

// -----------------------------------------------------------------------------
// UpdateManager mutable_display_list getter — exposed for
// InspectorOverlay::InjectHoverHighlight (A.1.1) to append commands.
// -----------------------------------------------------------------------------

TEST_F(ApplicationDevtoolTest, UpdateManagerExposesMutableDisplayList) {
  Application app(MakeConfig());
  app.LoadHTML("<div>Hello</div>");
  app.Update();

  ASSERT_NE(app.update_manager(), nullptr);
  // Cast away const because update_manager() returns const* — that is
  // unrelated to the mutability of the display list. We test the
  // mutable getter's existence + write-through.
  auto* um = const_cast<UpdateManager*>(app.update_manager());
  const usize before = um->display_list().size();
  um->mutable_display_list().push_back(render::PaintCommand::OverlayHighlight(
      gfx::Rect{0.0f, 0.0f, 10.0f, 10.0f},
      gfx::Color::FromRGBA(255, 0, 0, 230), /*stroke_width=*/2.0f));
  EXPECT_EQ(um->display_list().size(), before + 1);
}

}  // namespace
}  // namespace vx
