#ifndef VELOXA_DEVTOOL_INSPECTOR_INSPECTOR_DATA_H_
#define VELOXA_DEVTOOL_INSPECTOR_INSPECTOR_DATA_H_

#include "veloxa/core/css/computed_style.h"
#include "veloxa/core/dom/document.h"
#include "veloxa/core/dom/serializer.h"
#include "veloxa/core/layout/layout_box.h"
#include "veloxa/foundation/strings/string.h"

namespace vx::devtool {

// =============================================================================
// Inspector internal C++ API (TASK-20260502-01 A.0.5).
//
// D7=C 第一层 — 零拷贝 hot path. DevTool subsystem 直接调用，避免公开 C API
// 的 double-call 协议开销。返回 BasicString 直传到 Inspector UI 的 JS 代码。
//
// 公开 C API thin wrapper 见 `veloxa/api/veloxa_api.h` (A.0.6) — 同 schema，
// 加上 T7 max_size 守卫和 double-call 协议。
//
// JSON schemas:
//
//   SerializeDocument →
//     {"document":{"type":"document","children":[...DOM nodes...]}}
//     (内部沿用 dom::ToJson 的 element/text/comment/document schema)
//
//   SerializeLayoutBox →
//     LayoutBox::ToJson() schema (geometry + margin collapsing state)
//     "null" 当 box == nullptr
//
//   SerializeComputedStyle →
//     {"display":"block","position":"static","width":"100px","height":"auto",
//      "color":"#rrggbbaa","background_color":"#rrggbbaa","font_size":"16px",
//      "opacity":1, "margin":"<t> <r> <b> <l>","padding":"<t> <r> <b> <l>"}
//     "null" 当 style == nullptr
// =============================================================================

// SerializeDocument — DOM tree JSON for Inspector DOM panel (A1 acceptance).
// `policy` controls T3 sensitive-data redaction (default: kRedactSensitive
// in the public C API; tests may pass kNone to verify raw output).
String SerializeDocument(const dom::Document* doc, dom::RedactionPolicy policy);

// SerializeLayoutBox — single LayoutBox JSON for Inspector Layout panel (A4).
// Returns "null" if box is nullptr.
String SerializeLayoutBox(const layout::LayoutBox* box);

// SerializeComputedStyle — ComputedStyle JSON for Inspector Style panel (A3).
// Outputs ~10 most-used properties in CSS-canonical form. Full property
// enumeration is deferred (A1.3 will extend if needed).
String SerializeComputedStyle(const css::ComputedStyle* style);

}  // namespace vx::devtool

#endif  // VELOXA_DEVTOOL_INSPECTOR_INSPECTOR_DATA_H_
