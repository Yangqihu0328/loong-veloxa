#include "veloxa/devtool/inspector/inspector_data.h"

#include <cstdio>

#include "veloxa/core/css/enum_serialization.h"
#include "veloxa/core/css/property.h"
#include "veloxa/foundation/strings/string_view.h"

namespace vx::devtool {

namespace {

// =============================================================================
// JSON value formatting helpers (kept private — Inspector internal only).
// =============================================================================

void AppendF32(String& out, f32 value) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%g", static_cast<double>(value));
  out.append(StringView(buf));
}

const char* UnitSuffix(css::Unit unit) {
  switch (unit) {
    case css::Unit::kPx:
      return "px";
    case css::Unit::kEm:
      return "em";
    case css::Unit::kRem:
      return "rem";
    case css::Unit::kPercent:
      return "%";
    case css::Unit::kVw:
      return "vw";
    case css::Unit::kVh:
      return "vh";
    case css::Unit::kAuto:
    case css::Unit::kNone:
    case css::Unit::kNumber:
      return "";
  }
  return "";
}

// "auto" / "" / "<value><unit>" — covers Inspector Style panel display needs.
void AppendLengthValue(String& out, const css::LengthValue& lv) {
  if (lv.is_auto()) {
    out.append(StringView("auto"));
    return;
  }
  if (lv.is_none()) {
    return;  // emit nothing — caller chooses to omit or wrap empty string
  }
  AppendF32(out, lv.value);
  out.append(StringView(UnitSuffix(lv.unit)));
}

// Hex RGBA color: "#rrggbbaa" lowercase 8 chars.
// ComputedStyle stores colors as packed u32 with the convention bits:
// [31:24] R, [23:16] G, [15:8] B, [7:0] A — see CssColorToGfx() in render_utils.
void AppendColorHex(String& out, u32 rgba) {
  char buf[12];
  std::snprintf(buf, sizeof(buf), "#%08x", static_cast<unsigned int>(rgba));
  out.append(StringView(buf));
}

void AppendEnumProperty(String& out, css::PropertyId prop, u16 enum_value,
                        const char* fallback) {
  StringView name = css::EnumValueToCssString(prop, enum_value);
  if (name.empty()) {
    out.append(StringView(fallback));
  } else {
    out.append(name);
  }
}

}  // namespace

// =============================================================================
// SerializeDocument — wraps dom::ToJson with a "{\"document\":...}" envelope
// so consumers can distinguish DOM panel payloads from layout / style payloads.
// =============================================================================

String SerializeDocument(const dom::Document* doc,
                         dom::RedactionPolicy policy) {
  if (!doc) return String("null");
  String out;
  out.reserve(2048);  // typical small DOM tree fits in one heap block
  out.append(StringView("{\"document\":"));
  out.append(dom::ToJson(doc, policy));
  out.append(StringView("}"));
  return out;
}

// =============================================================================
// SerializeLayoutBox — pure delegate to LayoutBox::ToJson() (A.0.2).
// Wrapper exists so DevTool subsystem has a single import surface.
// =============================================================================

String SerializeLayoutBox(const layout::LayoutBox* box) {
  if (!box) return String("null");
  return box->ToJson();
}

// =============================================================================
// SerializeComputedStyle — emits the ~10 most-used properties for the
// Inspector Style panel (A3 acceptance). Full enumeration deferred until
// A.1.3 wiring exposes which properties are actually consumed.
// =============================================================================

String SerializeComputedStyle(const css::ComputedStyle* style) {
  if (!style) return String("null");
  String out;
  out.reserve(384);

  out.append(StringView("{\"display\":\""));
  AppendEnumProperty(out, css::PropertyId::kDisplay,
                     static_cast<u16>(style->display), "block");
  out.append(StringView("\",\"position\":\""));
  AppendEnumProperty(out, css::PropertyId::kPosition,
                     static_cast<u16>(style->position), "static");

  out.append(StringView("\",\"width\":\""));
  AppendLengthValue(out, style->width);
  out.append(StringView("\",\"height\":\""));
  AppendLengthValue(out, style->height);

  out.append(StringView("\",\"margin\":\""));
  AppendLengthValue(out, style->margin_top);
  out.append(' ');
  AppendLengthValue(out, style->margin_right);
  out.append(' ');
  AppendLengthValue(out, style->margin_bottom);
  out.append(' ');
  AppendLengthValue(out, style->margin_left);

  out.append(StringView("\",\"padding\":\""));
  AppendLengthValue(out, style->padding_top);
  out.append(' ');
  AppendLengthValue(out, style->padding_right);
  out.append(' ');
  AppendLengthValue(out, style->padding_bottom);
  out.append(' ');
  AppendLengthValue(out, style->padding_left);

  out.append(StringView("\",\"color\":\""));
  AppendColorHex(out, style->color);
  out.append(StringView("\",\"background_color\":\""));
  AppendColorHex(out, style->background_color);

  out.append(StringView("\",\"font_size\":\""));
  AppendLengthValue(out, style->font_size);

  out.append(StringView("\",\"opacity\":"));
  AppendF32(out, style->opacity);

  out.append('}');
  return out;
}

}  // namespace vx::devtool
