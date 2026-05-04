#include "veloxa/devtool/resources/inspector_resources.h"

#include <gtest/gtest.h>

#include <string>

namespace vx::devtool::resources {
namespace {

// =============================================================================
// inspector_panel.{html,css,js} smoke 测试
// （TASK-20260502-01 A.1.3 — DevTool UI dogfood UI 编写）
//
// 这些测验证 resource 内容的「形态合规性」：HTML 含必需结构 + CSS 仅用
// R2-verified shorthand 子集 + JS 含必需函数。运行时 dogfood 验证留待
// A.1.8 dogfood headless smoke 集成测。
//
// R2 mitigation 锁定（plan §A.1.3 步骤 0 grep 验证后）：
//   ✅ shorthand: flex / padding / margin / border / border-{top,right,
//                  bottom,left} / border-color/style/width
//   ✅ longhand: display: flex / position / width / height / overflow /
//                 background-color / color / font-size / border-radius
//   ❌ avoid:  background: shorthand / font: shorthand / pointer-events
// =============================================================================

inline std::string AsString(const char* s) {
  return s == nullptr ? std::string{} : std::string{s};
}

// Strip CSS /* ... */ comments so R2 grep-checks (AvoidsBackgroundShorthand
// etc.) don't false-positive on doc strings inside the .css source. Returns
// a copy with comments replaced by single spaces (keeps offsets readable).
inline std::string StripCssComments(const std::string& css) {
  std::string out;
  out.reserve(css.size());
  std::size_t i = 0;
  while (i < css.size()) {
    if (i + 1 < css.size() && css[i] == '/' && css[i + 1] == '*') {
      i += 2;
      while (i + 1 < css.size() &&
             !(css[i] == '*' && css[i + 1] == '/')) {
        ++i;
      }
      if (i + 1 < css.size()) i += 2;  // skip closing */
      out.push_back(' ');
    } else {
      out.push_back(css[i++]);
    }
  }
  return out;
}

// -----------------------------------------------------------------------------
// HTML 结构（splitter dock + 3 tabs + 3 panels + inline 占位符）
// -----------------------------------------------------------------------------

TEST(InspectorPanelHtmlSmoke, ContainsDevtoolRoot) {
  const std::string html = AsString(kInspectorPanelHtml);
  EXPECT_NE(html.find("id=\"devtool-root\""), std::string::npos);
}

TEST(InspectorPanelHtmlSmoke, ContainsThreeTabButtons) {
  const std::string html = AsString(kInspectorPanelHtml);
  EXPECT_NE(html.find("data-tab=\"dom\""), std::string::npos);
  EXPECT_NE(html.find("data-tab=\"style\""), std::string::npos);
  EXPECT_NE(html.find("data-tab=\"layout\""), std::string::npos);
}

TEST(InspectorPanelHtmlSmoke, ContainsThreePanelDivs) {
  const std::string html = AsString(kInspectorPanelHtml);
  EXPECT_NE(html.find("id=\"dom-tree-panel\""), std::string::npos);
  EXPECT_NE(html.find("id=\"style-panel\""), std::string::npos);
  EXPECT_NE(html.find("id=\"layout-panel\""), std::string::npos);
}

// TASK-20260503-04 D.3 — Console pane is the 4th DevTool sub-pane.
// These smoke tests stand in for the manual "F12 attach + click Console
// tab" visual verification noted in plan §3.D.3, ensuring the 4 tab
// structure + 3 placeholders are present in the embedded HTML so that
// LoadDevtoolDocument's ReplaceFirst chain has something to operate on.
TEST(InspectorPanelHtmlSmoke, ContainsFourthConsoleTab) {
  const std::string html = AsString(kInspectorPanelHtml);
  EXPECT_NE(html.find("data-tab=\"console\""), std::string::npos);
  EXPECT_NE(html.find("id=\"console-panel\""), std::string::npos);
}

TEST(InspectorPanelHtmlSmoke, ContainsThreeConsoleInlinePlaceholders) {
  const std::string html = AsString(kInspectorPanelHtml);
  EXPECT_NE(html.find("__INLINE_CONSOLE_HTML__"), std::string::npos);
  EXPECT_NE(html.find("__INLINE_CONSOLE_CSS__"), std::string::npos);
  EXPECT_NE(html.find("__INLINE_CONSOLE_JS__"), std::string::npos);
}

TEST(ConsolePanelResourceSmoke, ConsoleHtmlContainsReplPrimitives) {
  // creative §C1 decision A — REPL uses <span> + JS-managed value state
  // instead of <input> (no native input behaviour in Veloxa subset).
  const std::string html = AsString(kConsolePanelHtml);
  EXPECT_NE(html.find("id=\"console-output\""), std::string::npos);
  EXPECT_NE(html.find("id=\"console-input-display\""), std::string::npos);
  EXPECT_NE(html.find("id=\"console-caret\""), std::string::npos);
  // Defensive: ensure we did NOT regress by introducing <input> which
  // would compile+render but never capture keyboard input.
  EXPECT_EQ(html.find("<input"), std::string::npos);
}

TEST(ConsolePanelResourceSmoke, ConsoleCssDefinesLevelColors) {
  const std::string css = AsString(kConsolePanelCss);
  EXPECT_NE(css.find(".console-log"),    std::string::npos);
  EXPECT_NE(css.find(".console-warn"),   std::string::npos);
  EXPECT_NE(css.find(".console-error"),  std::string::npos);
  EXPECT_NE(css.find(".console-input"),  std::string::npos);
  EXPECT_NE(css.find(".console-result"), std::string::npos);
}

TEST(ConsolePanelResourceSmoke, ConsoleJsCallsRequiredBindings) {
  const std::string js = AsString(kConsolePanelJs);
  // creative §C4 — drain query polled every 200 ms.
  EXPECT_NE(js.find("vx_devtool_get_console_log_drain"), std::string::npos);
  // D.4 will register vx_console_eval; D.3 already wires the call site.
  EXPECT_NE(js.find("vx_console_eval"), std::string::npos);
  // creative §C1 — keydown listener on window guarded by isConsoleActive.
  EXPECT_NE(js.find("isConsoleActive"), std::string::npos);
  EXPECT_NE(js.find("addEventListener"), std::string::npos);
}

TEST(InspectorPanelHtmlSmoke, ContainsInlineCssPlaceholder) {
  // A.1.6 Application::LoadDevtoolDocument runtime 用 std::string::replace
  // 把占位符替换为实际 CSS 内容（避免 DevTool Document 加载 sub-resources
  // 链未验证）
  const std::string html = AsString(kInspectorPanelHtml);
  EXPECT_NE(html.find("__INLINE_CSS__"), std::string::npos);
}

TEST(InspectorPanelHtmlSmoke, ContainsInlineJsPlaceholder) {
  const std::string html = AsString(kInspectorPanelHtml);
  EXPECT_NE(html.find("__INLINE_JS__"), std::string::npos);
}

// -----------------------------------------------------------------------------
// CSS R2 mitigation — 仅用 grep-verified shorthand 子集
// -----------------------------------------------------------------------------

TEST(InspectorPanelCssSmoke, ContainsDevtoolRootSelector) {
  const std::string css = AsString(kInspectorPanelCss);
  EXPECT_NE(css.find("#devtool-root"), std::string::npos);
}

TEST(InspectorPanelCssSmoke, UsesFlexLayout) {
  // creative #1 决策 1 splitter dock 用 display: flex
  const std::string css = AsString(kInspectorPanelCss);
  EXPECT_NE(css.find("display: flex"), std::string::npos);
}

TEST(InspectorPanelCssSmoke, AvoidsBackgroundShorthand) {
  // R2 mitigation: parser.cc 不支持 `background:` shorthand，仅
  // background-color longhand
  const std::string css = StripCssComments(AsString(kInspectorPanelCss));
  // 允许 "background-color"，但不允许独立 "background:"（不带 -color/-image）
  // 简化检查：确保 "background-color" 出现 N 次时，"background:" 出现 0 次
  std::size_t pos = 0;
  while ((pos = css.find("background", pos)) != std::string::npos) {
    // 跳过 background-color / background-image 等带 dash 的
    if (pos + 11 < css.size() && css[pos + 10] == '-') {
      pos += 11;
      continue;
    }
    // 检查后续字符是否是 ':' (即 background:)
    if (pos + 10 < css.size() && css[pos + 10] == ':') {
      ADD_FAILURE() << "Found 'background:' shorthand at offset " << pos
                    << " — Veloxa CSS parser does not support background "
                    << "shorthand (R2 mitigation). Use background-color "
                    << "longhand instead.";
    }
    pos += 10;
  }
}

TEST(InspectorPanelCssSmoke, AvoidsFontShorthand) {
  // R2 mitigation: parser.cc 不支持 `font:` shorthand，仅 font-size /
  // font-family longhand
  const std::string css = StripCssComments(AsString(kInspectorPanelCss));
  std::size_t pos = 0;
  while ((pos = css.find("font", pos)) != std::string::npos) {
    if (pos + 5 < css.size() && css[pos + 4] == '-') {
      pos += 5;
      continue;
    }
    if (pos + 4 < css.size() && css[pos + 4] == ':') {
      ADD_FAILURE() << "Found 'font:' shorthand at offset " << pos
                    << " — Veloxa CSS parser does not support font "
                    << "shorthand (R2 mitigation). Use font-size / "
                    << "font-family longhand instead.";
    }
    pos += 4;
  }
}

TEST(InspectorPanelCssSmoke, AvoidsPointerEventsNone) {
  // R2 mitigation: pointer-events: none 未实现 (creative #1 决策 3 警告)
  const std::string css = StripCssComments(AsString(kInspectorPanelCss));
  EXPECT_EQ(css.find("pointer-events"), std::string::npos);
}

// -----------------------------------------------------------------------------
// JS — DOM tree 渲染 + tab 切换 + native binding 调用
// -----------------------------------------------------------------------------

TEST(InspectorPanelJsSmoke, ContainsRenderDomTreeFunction) {
  const std::string js = AsString(kInspectorPanelJs);
  EXPECT_NE(js.find("renderDomTree"), std::string::npos);
}

TEST(InspectorPanelJsSmoke, ContainsRenderTreeNodeFunction) {
  const std::string js = AsString(kInspectorPanelJs);
  EXPECT_NE(js.find("renderTreeNode"), std::string::npos);
}

TEST(InspectorPanelJsSmoke, CallsVxDevtoolGetDomJsonNativeBinding) {
  // A.1.4 将 binding 此函数；A.1.3 仅写调用桩
  const std::string js = AsString(kInspectorPanelJs);
  EXPECT_NE(js.find("vx_devtool_get_dom_json"), std::string::npos);
}

TEST(InspectorPanelJsSmoke, ContainsTabSwitchHandler) {
  const std::string js = AsString(kInspectorPanelJs);
  // tab 切换通过 click event 监听 data-tab 属性切换 active panel
  EXPECT_NE(js.find("data-tab"), std::string::npos);
}

// -----------------------------------------------------------------------------
// TASK-20260502-02 B.2.1 — HUD overlay HTML/CSS smoke
// -----------------------------------------------------------------------------

TEST(InspectorPanelHudSmoke, HtmlContainsDevtoolHudId) {
  const std::string html = AsString(kInspectorPanelHtml);
  EXPECT_NE(html.find("id=\"devtool-hud\""), std::string::npos);
}

TEST(InspectorPanelHudSmoke, HtmlContainsHudFpsValueSlot) {
  const std::string html = AsString(kInspectorPanelHtml);
  EXPECT_NE(html.find("id=\"hud-fps\""), std::string::npos);
}

TEST(InspectorPanelHudSmoke, HtmlContainsAllFourStageBars) {
  const std::string html = AsString(kInspectorPanelHtml);
  for (const char* bar :
       {"id=\"bar-style\"", "id=\"bar-layout\"", "id=\"bar-render\"",
        "id=\"bar-paint\""}) {
    EXPECT_NE(html.find(bar), std::string::npos)
        << "missing HUD stage bar slot: " << bar;
  }
}

TEST(InspectorPanelHudSmoke, HtmlContainsDataPassthroughAttribute) {
  // creative #1 决策 3 — `pointer-events: none` 不支持 → data-passthrough
  // attribute fallback (HitTest 待 R3+ EventManager 改造识别)。
  const std::string html = AsString(kInspectorPanelHtml);
  EXPECT_NE(html.find("data-passthrough=\"1\""), std::string::npos);
}

TEST(InspectorPanelHudSmoke, CssContainsHudStyleBlock) {
  const std::string css = StripCssComments(AsString(kInspectorPanelCss));
  EXPECT_NE(css.find("#devtool-hud"), std::string::npos);
}

TEST(InspectorPanelHudSmoke, CssHudUsesAbsoluteNotFixed) {
  // R2 + plan §0.9 grep 实证：position: fixed 在当前 layout 等价 absolute
  // → HUD 用 absolute 视觉等价，避免引擎不一致。
  const std::string css = StripCssComments(AsString(kInspectorPanelCss));
  // 必须含 absolute 用法（HUD 块）
  EXPECT_NE(css.find("position: absolute"), std::string::npos);
  // 不应在 HUD 块用 fixed（这一断言保留宽松：整个 css 不应有 position: fixed）
  EXPECT_EQ(css.find("position: fixed"), std::string::npos);
}

TEST(InspectorPanelHudSmoke, CssHudUsesOpacity) {
  const std::string css = StripCssComments(AsString(kInspectorPanelCss));
  EXPECT_NE(css.find("opacity"), std::string::npos);
}

// -----------------------------------------------------------------------------
// TASK-20260502-02 B.2.2 — HUD JS updateHud + native binding call
// -----------------------------------------------------------------------------

TEST(InspectorPanelHudSmoke, JsContainsUpdateHudFunction) {
  const std::string js = AsString(kInspectorPanelJs);
  EXPECT_NE(js.find("updateHud"), std::string::npos);
}

TEST(InspectorPanelHudSmoke, JsCallsVxViewGetPerfStatsBinding) {
  const std::string js = AsString(kInspectorPanelJs);
  EXPECT_NE(js.find("vx_view_get_perf_stats"), std::string::npos);
}

// -----------------------------------------------------------------------------
// TASK-20260503-01 C.3.1 — Hot Reload status indicator HTML/CSS/JS smoke
//
// 三件套契约（与 HUD 子模式对齐）：
//   HTML: 含 <div id="hot-reload-status"> 槽位（位于 #devtool-hud 兄弟节点）
//   CSS:  含 #hot-reload-status 块 + .status-watching / .status-error 样式类
//   JS:   含 updateHotReloadStatus() 函数 + 调 vx_devtool_get_hot_reload_status
// -----------------------------------------------------------------------------

TEST(InspectorPanelHotReloadSmoke, HtmlContainsHotReloadStatusSlot) {
  const std::string html = AsString(kInspectorPanelHtml);
  EXPECT_NE(html.find("id=\"hot-reload-status\""), std::string::npos);
}

TEST(InspectorPanelHotReloadSmoke, CssContainsStatusWatchingAndErrorClasses) {
  const std::string css = StripCssComments(AsString(kInspectorPanelCss));
  EXPECT_NE(css.find(".status-watching"), std::string::npos);
  EXPECT_NE(css.find(".status-error"), std::string::npos);
}

TEST(InspectorPanelHotReloadSmoke, JsContainsUpdateHotReloadStatusBinding) {
  const std::string js = AsString(kInspectorPanelJs);
  EXPECT_NE(js.find("updateHotReloadStatus"), std::string::npos);
  EXPECT_NE(js.find("vx_devtool_get_hot_reload_status"), std::string::npos);
}

}  // namespace
}  // namespace vx::devtool::resources
