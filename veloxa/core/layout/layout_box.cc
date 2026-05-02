#include "veloxa/core/layout/layout_box.h"

#include <cstdio>

#include "veloxa/foundation/strings/string_view.h"

namespace vx::layout {

namespace {

const char* LayoutTypeName(LayoutType type) {
  switch (type) {
    case LayoutType::kBlock:
      return "block";
    case LayoutType::kInline:
      return "inline";
    case LayoutType::kFlex:
      return "flex";
    case LayoutType::kText:
      return "text";
    case LayoutType::kReplaced:
      return "replaced";
  }
  return "unknown";
}

void AppendF32(String& out, f32 value) {
  char buf[32];
  // %g 紧凑浮点输出（剪掉尾零；e.g. 10.5 → "10.5"，20.0 → "20"）。
  std::snprintf(buf, sizeof(buf), "%g", static_cast<double>(value));
  out.append(StringView(buf));
}

void AppendBool(String& out, bool value) {
  out.append(value ? StringView("true") : StringView("false"));
}

void AppendU32(String& out, u32 value) {
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%u", value);
  out.append(StringView(buf));
}

void AppendQuad(String& out, const f32 (&quad)[4]) {
  out.append('[');
  for (int i = 0; i < 4; ++i) {
    if (i > 0) out.append(',');
    AppendF32(out, quad[i]);
  }
  out.append(']');
}

}  // namespace

String LayoutBox::ToJson() const {
  String out;
  // SSO 容量 22 byte；典型 box JSON ~250-300 byte，预 reserve 避免多次扩容。
  out.reserve(320);

  out.append(StringView("{\"type\":\""));
  out.append(StringView(LayoutTypeName(type)));
  out.append(StringView("\",\"x\":"));
  AppendF32(out, x);
  out.append(StringView(",\"y\":"));
  AppendF32(out, y);
  out.append(StringView(",\"content_width\":"));
  AppendF32(out, content_width);
  out.append(StringView(",\"content_height\":"));
  AppendF32(out, content_height);
  out.append(StringView(",\"padding\":"));
  AppendQuad(out, padding);
  out.append(StringView(",\"border\":"));
  AppendQuad(out, border);
  out.append(StringView(",\"margin\":"));
  AppendQuad(out, margin);
  out.append(StringView(",\"collapsed_through\":"));
  AppendBool(out, collapsed_through);
  out.append(StringView(",\"margin_top_collapsed_into_ancestor\":"));
  AppendBool(out, margin_top_collapsed_into_ancestor);
  out.append(StringView(",\"margin_bottom_collapsed_into_ancestor\":"));
  AppendBool(out, margin_bottom_collapsed_into_ancestor);
  out.append(StringView(",\"child_count\":"));
  AppendU32(out, child_count());
  out.append('}');

  return out;
}

}  // namespace vx::layout
