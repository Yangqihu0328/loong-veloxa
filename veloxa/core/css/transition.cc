#include "veloxa/core/css/transition.h"

#include <algorithm>
#include <cmath>

#include "veloxa/core/css/computed_style.h"

namespace vx::css {

// --- CubicBezier ---

static f32 EvalCubic(f32 a, f32 b, f32 t) {
  return 3.0f * a * (1 - t) * (1 - t) * t + 3.0f * b * (1 - t) * t * t +
         t * t * t;
}

f32 CubicBezier::Solve(f32 x) const {
  if (x <= 0.0f) return 0.0f;
  if (x >= 1.0f) return 1.0f;

  f32 lo = 0.0f, hi = 1.0f;
  for (int i = 0; i < 20; ++i) {
    f32 mid = (lo + hi) * 0.5f;
    f32 vx = EvalCubic(x1, x2, mid);
    if (vx < x)
      lo = mid;
    else
      hi = mid;
  }
  f32 t = (lo + hi) * 0.5f;
  return EvalCubic(y1, y2, t);
}

CubicBezier CubicBezier::FromTiming(TimingFunction tf) {
  switch (tf) {
    case TimingFunction::kLinear: return Linear();
    case TimingFunction::kEase: return Ease();
    case TimingFunction::kEaseIn: return EaseIn();
    case TimingFunction::kEaseOut: return EaseOut();
    case TimingFunction::kEaseInOut: return EaseInOut();
    case TimingFunction::kCubicBezier: return Ease();
  }
  return Ease();
}

// --- IsAnimatable ---

bool IsAnimatable(PropertyId id) {
  switch (id) {
    case PropertyId::kBackgroundColor:
    case PropertyId::kColor:
    case PropertyId::kBorderTopColor:
    case PropertyId::kBorderRightColor:
    case PropertyId::kBorderBottomColor:
    case PropertyId::kBorderLeftColor:
    case PropertyId::kOpacity:
    case PropertyId::kFlexGrow:
    case PropertyId::kFlexShrink:
    case PropertyId::kWidth:
    case PropertyId::kHeight:
    case PropertyId::kMinWidth:
    case PropertyId::kMinHeight:
    case PropertyId::kMaxWidth:
    case PropertyId::kMaxHeight:
    case PropertyId::kMarginTop:
    case PropertyId::kMarginRight:
    case PropertyId::kMarginBottom:
    case PropertyId::kMarginLeft:
    case PropertyId::kPaddingTop:
    case PropertyId::kPaddingRight:
    case PropertyId::kPaddingBottom:
    case PropertyId::kPaddingLeft:
    case PropertyId::kTop:
    case PropertyId::kRight:
    case PropertyId::kBottom:
    case PropertyId::kLeft:
    case PropertyId::kBorderTopWidth:
    case PropertyId::kBorderRightWidth:
    case PropertyId::kBorderBottomWidth:
    case PropertyId::kBorderLeftWidth:
    case PropertyId::kBorderRadius:
    case PropertyId::kFontSize:
    case PropertyId::kLineHeight:
    case PropertyId::kLetterSpacing:
    case PropertyId::kGap:
    case PropertyId::kFlexBasis:
      return true;
    default:
      return false;
  }
}

// --- Interpolation ---

f32 InterpolateF32(f32 from, f32 to, f32 t) { return from + (to - from) * t; }

u32 InterpolateColor(u32 from, u32 to, f32 t) {
  auto lerp8 = [t](u32 a, u32 b) -> u32 {
    f32 fa = static_cast<f32>(a);
    f32 fb = static_cast<f32>(b);
    f32 r = fa + (fb - fa) * t;
    return static_cast<u32>(std::max(0.0f, std::min(255.0f, r)));
  };

  u32 r = lerp8((from >> 24) & 0xFF, (to >> 24) & 0xFF);
  u32 g = lerp8((from >> 16) & 0xFF, (to >> 16) & 0xFF);
  u32 b = lerp8((from >> 8) & 0xFF, (to >> 8) & 0xFF);
  u32 a = lerp8(from & 0xFF, to & 0xFF);
  return (r << 24) | (g << 16) | (b << 8) | a;
}

LengthValue InterpolateLength(const LengthValue& from, const LengthValue& to,
                               f32 t) {
  if (from.unit != to.unit) return to;
  return {InterpolateF32(from.value, to.value, t), from.unit};
}

// --- InterpolateStyle (per-property) ---

namespace {

enum class PropValueKind { kColor, kFloat, kLength, kNone };

PropValueKind GetPropertyValueKind(PropertyId id) {
  switch (id) {
    case PropertyId::kBackgroundColor:
    case PropertyId::kColor:
    case PropertyId::kBorderTopColor:
    case PropertyId::kBorderRightColor:
    case PropertyId::kBorderBottomColor:
    case PropertyId::kBorderLeftColor:
      return PropValueKind::kColor;
    case PropertyId::kOpacity:
    case PropertyId::kFlexGrow:
    case PropertyId::kFlexShrink:
      return PropValueKind::kFloat;
    case PropertyId::kWidth:
    case PropertyId::kHeight:
    case PropertyId::kMinWidth:
    case PropertyId::kMinHeight:
    case PropertyId::kMaxWidth:
    case PropertyId::kMaxHeight:
    case PropertyId::kMarginTop:
    case PropertyId::kMarginRight:
    case PropertyId::kMarginBottom:
    case PropertyId::kMarginLeft:
    case PropertyId::kPaddingTop:
    case PropertyId::kPaddingRight:
    case PropertyId::kPaddingBottom:
    case PropertyId::kPaddingLeft:
    case PropertyId::kTop:
    case PropertyId::kRight:
    case PropertyId::kBottom:
    case PropertyId::kLeft:
    case PropertyId::kBorderTopWidth:
    case PropertyId::kBorderRightWidth:
    case PropertyId::kBorderBottomWidth:
    case PropertyId::kBorderLeftWidth:
    case PropertyId::kBorderRadius:
    case PropertyId::kFontSize:
    case PropertyId::kLineHeight:
    case PropertyId::kLetterSpacing:
    case PropertyId::kGap:
    case PropertyId::kFlexBasis:
      return PropValueKind::kLength;
    default:
      return PropValueKind::kNone;
  }
}

u32 GetColorField(const ComputedStyle& s, PropertyId id) {
  switch (id) {
    case PropertyId::kBackgroundColor: return s.background_color;
    case PropertyId::kColor: return s.color;
    case PropertyId::kBorderTopColor: return s.border_color[0];
    case PropertyId::kBorderRightColor: return s.border_color[1];
    case PropertyId::kBorderBottomColor: return s.border_color[2];
    case PropertyId::kBorderLeftColor: return s.border_color[3];
    default: return 0;
  }
}

void SetColorField(ComputedStyle& s, PropertyId id, u32 v) {
  switch (id) {
    case PropertyId::kBackgroundColor: s.background_color = v; break;
    case PropertyId::kColor: s.color = v; break;
    case PropertyId::kBorderTopColor: s.border_color[0] = v; break;
    case PropertyId::kBorderRightColor: s.border_color[1] = v; break;
    case PropertyId::kBorderBottomColor: s.border_color[2] = v; break;
    case PropertyId::kBorderLeftColor: s.border_color[3] = v; break;
    default: break;
  }
}

f32 GetFloatField(const ComputedStyle& s, PropertyId id) {
  switch (id) {
    case PropertyId::kOpacity: return s.opacity;
    case PropertyId::kFlexGrow: return s.flex_grow;
    case PropertyId::kFlexShrink: return s.flex_shrink;
    default: return 0;
  }
}

void SetFloatField(ComputedStyle& s, PropertyId id, f32 v) {
  switch (id) {
    case PropertyId::kOpacity: s.opacity = v; break;
    case PropertyId::kFlexGrow: s.flex_grow = v; break;
    case PropertyId::kFlexShrink: s.flex_shrink = v; break;
    default: break;
  }
}

const LengthValue& GetLengthField(const ComputedStyle& s, PropertyId id) {
  switch (id) {
    case PropertyId::kWidth: return s.width;
    case PropertyId::kHeight: return s.height;
    case PropertyId::kMinWidth: return s.min_width;
    case PropertyId::kMinHeight: return s.min_height;
    case PropertyId::kMaxWidth: return s.max_width;
    case PropertyId::kMaxHeight: return s.max_height;
    case PropertyId::kMarginTop: return s.margin_top;
    case PropertyId::kMarginRight: return s.margin_right;
    case PropertyId::kMarginBottom: return s.margin_bottom;
    case PropertyId::kMarginLeft: return s.margin_left;
    case PropertyId::kPaddingTop: return s.padding_top;
    case PropertyId::kPaddingRight: return s.padding_right;
    case PropertyId::kPaddingBottom: return s.padding_bottom;
    case PropertyId::kPaddingLeft: return s.padding_left;
    case PropertyId::kTop: return s.top;
    case PropertyId::kRight: return s.right;
    case PropertyId::kBottom: return s.bottom;
    case PropertyId::kLeft: return s.left;
    case PropertyId::kBorderTopWidth: return s.border_width[0];
    case PropertyId::kBorderRightWidth: return s.border_width[1];
    case PropertyId::kBorderBottomWidth: return s.border_width[2];
    case PropertyId::kBorderLeftWidth: return s.border_width[3];
    case PropertyId::kBorderRadius: return s.border_radius;
    case PropertyId::kFontSize: return s.font_size;
    case PropertyId::kLineHeight: return s.line_height;
    case PropertyId::kLetterSpacing: return s.letter_spacing;
    case PropertyId::kGap: return s.gap;
    case PropertyId::kFlexBasis: return s.flex_basis;
    default: {
      static const LengthValue kZero{};
      return kZero;
    }
  }
}

void SetLengthField(ComputedStyle& s, PropertyId id, const LengthValue& v) {
  switch (id) {
    case PropertyId::kWidth: s.width = v; break;
    case PropertyId::kHeight: s.height = v; break;
    case PropertyId::kMinWidth: s.min_width = v; break;
    case PropertyId::kMinHeight: s.min_height = v; break;
    case PropertyId::kMaxWidth: s.max_width = v; break;
    case PropertyId::kMaxHeight: s.max_height = v; break;
    case PropertyId::kMarginTop: s.margin_top = v; break;
    case PropertyId::kMarginRight: s.margin_right = v; break;
    case PropertyId::kMarginBottom: s.margin_bottom = v; break;
    case PropertyId::kMarginLeft: s.margin_left = v; break;
    case PropertyId::kPaddingTop: s.padding_top = v; break;
    case PropertyId::kPaddingRight: s.padding_right = v; break;
    case PropertyId::kPaddingBottom: s.padding_bottom = v; break;
    case PropertyId::kPaddingLeft: s.padding_left = v; break;
    case PropertyId::kTop: s.top = v; break;
    case PropertyId::kRight: s.right = v; break;
    case PropertyId::kBottom: s.bottom = v; break;
    case PropertyId::kLeft: s.left = v; break;
    case PropertyId::kBorderTopWidth: s.border_width[0] = v; break;
    case PropertyId::kBorderRightWidth: s.border_width[1] = v; break;
    case PropertyId::kBorderBottomWidth: s.border_width[2] = v; break;
    case PropertyId::kBorderLeftWidth: s.border_width[3] = v; break;
    case PropertyId::kBorderRadius: s.border_radius = v; break;
    case PropertyId::kFontSize: s.font_size = v; break;
    case PropertyId::kLineHeight: s.line_height = v; break;
    case PropertyId::kLetterSpacing: s.letter_spacing = v; break;
    case PropertyId::kGap: s.gap = v; break;
    case PropertyId::kFlexBasis: s.flex_basis = v; break;
    default: break;
  }
}

}  // namespace

void InterpolateStyle(const ComputedStyle& from, const ComputedStyle& to,
                      PropertyId prop, f32 t, ComputedStyle& out) {
  auto kind = GetPropertyValueKind(prop);
  switch (kind) {
    case PropValueKind::kColor:
      SetColorField(out, prop, InterpolateColor(GetColorField(from, prop),
                                                 GetColorField(to, prop), t));
      break;
    case PropValueKind::kFloat:
      SetFloatField(out, prop, InterpolateF32(GetFloatField(from, prop),
                                               GetFloatField(to, prop), t));
      break;
    case PropValueKind::kLength:
      SetLengthField(out, prop,
                     InterpolateLength(GetLengthField(from, prop),
                                       GetLengthField(to, prop), t));
      break;
    case PropValueKind::kNone:
      break;
  }
}

// --- TransitionManager ---

void TransitionManager::OnStyleChange(const dom::Element* el,
                                       const ComputedStyle& old_style,
                                       const ComputedStyle& new_style,
                                       SteadyTimePoint now) {
  if (new_style.transitions.empty()) return;

  for (const auto& spec : new_style.transitions) {
    if (spec.duration_ms <= 0) continue;

    auto check_and_start = [&](PropertyId pid) {
      if (!IsAnimatable(pid)) return;

      auto kind = GetPropertyValueKind(pid);
      bool changed = false;
      ActiveTransition at{};
      at.property = pid;
      at.bezier = spec.bezier;
      at.duration_ms = spec.duration_ms;
      at.delay_ms = spec.delay_ms;
      at.start_time = now;

      switch (kind) {
        case PropValueKind::kColor: {
          u32 ov = GetColorField(old_style, pid);
          u32 nv = GetColorField(new_style, pid);
          if (ov != nv) {
            changed = true;
            at.kind = ValueKind::kColor;
            at.from_color = ov;
            at.to_color = nv;
          }
          break;
        }
        case PropValueKind::kFloat: {
          f32 ov = GetFloatField(old_style, pid);
          f32 nv = GetFloatField(new_style, pid);
          if (ov != nv) {
            changed = true;
            at.kind = ValueKind::kFloat;
            at.from_float = ov;
            at.to_float = nv;
          }
          break;
        }
        case PropValueKind::kLength: {
          const auto& ov = GetLengthField(old_style, pid);
          const auto& nv = GetLengthField(new_style, pid);
          if (ov.value != nv.value || ov.unit != nv.unit) {
            changed = true;
            at.kind = ValueKind::kLength;
            at.from_length = ov;
            at.to_length = nv;
          }
          break;
        }
        default:
          break;
      }

      if (!changed) return;

      const void* key = static_cast<const void*>(el);
      Vector<ActiveTransition>* existing = transitions_.Find(key);
      if (existing) {
        bool replaced = false;
        for (auto& tr : *existing) {
          if (tr.property == pid) {
            tr = at;
            replaced = true;
            break;
          }
        }
        if (!replaced) {
          existing->push_back(at);
        }
      } else {
        Vector<ActiveTransition> vec;
        vec.push_back(at);
        transitions_.Insert(key, std::move(vec));
      }
    };

    if (spec.property == PropertyId::kUnknown) {
      for (u8 i = 1; i < static_cast<u8>(PropertyId::kMaxProperty); ++i) {
        check_and_start(static_cast<PropertyId>(i));
      }
    } else {
      check_and_start(spec.property);
    }
  }
}

void TransitionManager::Tick(SteadyTimePoint now) {
  for (auto it = transitions_.begin(); it != transitions_.end(); ++it) {
    auto& vec = it->value;
    for (auto& tr : vec) {
      if (tr.completed) continue;

      auto elapsed =
          std::chrono::duration<f32, std::milli>(now - tr.start_time).count();
      if (elapsed < tr.delay_ms) {
        tr.progress = 0.0f;
        continue;
      }

      f32 active_time = elapsed - tr.delay_ms;
      f32 raw_progress =
          (tr.duration_ms > 0) ? (active_time / tr.duration_ms) : 1.0f;
      if (raw_progress >= 1.0f) {
        raw_progress = 1.0f;
        if (!tr.completed) {
          tr.completed = true;
        }
      }
      tr.progress = tr.bezier.Solve(raw_progress);
    }
  }
}

void TransitionManager::ApplyTo(const dom::Element* el,
                                 ComputedStyle& style) const {
  const void* key = static_cast<const void*>(el);
  const Vector<ActiveTransition>* vec = transitions_.Find(key);
  if (!vec) return;

  for (const auto& tr : *vec) {
    if (tr.completed) continue;

    f32 t = tr.progress;
    switch (tr.kind) {
      case ValueKind::kColor:
        SetColorField(style, tr.property,
                      InterpolateColor(tr.from_color, tr.to_color, t));
        break;
      case ValueKind::kFloat:
        SetFloatField(style, tr.property,
                      InterpolateF32(tr.from_float, tr.to_float, t));
        break;
      case ValueKind::kLength:
        SetLengthField(style, tr.property,
                       InterpolateLength(tr.from_length, tr.to_length, t));
        break;
    }
  }
}

bool TransitionManager::HasActive() const {
  for (const auto& slot : transitions_) {
    for (const auto& tr : slot.value) {
      if (!tr.completed) {
        return true;
      }
    }
  }
  return false;
}

void TransitionManager::Clear() { transitions_ = {}; }

}  // namespace vx::css
