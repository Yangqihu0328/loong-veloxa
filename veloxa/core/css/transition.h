#ifndef VELOXA_CORE_CSS_TRANSITION_H_
#define VELOXA_CORE_CSS_TRANSITION_H_

#include <chrono>

#include "veloxa/core/css/css_value.h"
#include "veloxa/core/css/property.h"
#include "veloxa/foundation/containers/hash_map.h"
#include "veloxa/foundation/containers/small_vector.h"
#include "veloxa/foundation/containers/vector.h"

namespace vx::dom {
class Element;
}

namespace vx::css {

struct ComputedStyle;

using SteadyTimePoint = std::chrono::steady_clock::time_point;
using SteadyClock = std::chrono::steady_clock;

enum class TimingFunction : u8 {
  kLinear,
  kEase,
  kEaseIn,
  kEaseOut,
  kEaseInOut,
  kCubicBezier,
};

struct CubicBezier {
  f32 x1 = 0, y1 = 0, x2 = 1, y2 = 1;

  f32 Solve(f32 x) const;

  static CubicBezier Linear() { return {0, 0, 1, 1}; }
  static CubicBezier Ease() { return {0.25f, 0.1f, 0.25f, 1.0f}; }
  static CubicBezier EaseIn() { return {0.42f, 0, 1, 1}; }
  static CubicBezier EaseOut() { return {0, 0, 0.58f, 1}; }
  static CubicBezier EaseInOut() { return {0.42f, 0, 0.58f, 1}; }

  static CubicBezier FromTiming(TimingFunction tf);
};

struct TransitionSpec {
  PropertyId property = PropertyId::kUnknown;
  f32 duration_ms = 0.0f;
  f32 delay_ms = 0.0f;
  TimingFunction timing = TimingFunction::kEase;
  CubicBezier bezier = CubicBezier::Ease();
};

bool IsAnimatable(PropertyId id);

f32 InterpolateF32(f32 from, f32 to, f32 t);
u32 InterpolateColor(u32 from, u32 to, f32 t);
LengthValue InterpolateLength(const LengthValue& from, const LengthValue& to,
                               f32 t);

void InterpolateStyle(const ComputedStyle& from, const ComputedStyle& to,
                      PropertyId prop, f32 t, ComputedStyle& out);

class TransitionManager {
 public:
  void OnStyleChange(const dom::Element* el, const ComputedStyle& old_style,
                     const ComputedStyle& new_style, SteadyTimePoint now);

  void Tick(SteadyTimePoint now);

  void ApplyTo(const dom::Element* el, ComputedStyle& style) const;

  bool HasActive() const;

  void Clear();

 private:
  enum class ValueKind : u8 { kColor, kFloat, kLength };

  struct ActiveTransition {
    PropertyId property;
    CubicBezier bezier;
    f32 duration_ms;
    f32 delay_ms;
    SteadyTimePoint start_time;
    f32 progress = 0.0f;
    bool completed = false;

    ValueKind kind;
    u32 from_color = 0, to_color = 0;
    f32 from_float = 0, to_float = 0;
    LengthValue from_length, to_length;
  };

  struct PtrHash {
    usize operator()(const void* p) const {
      return reinterpret_cast<usize>(p);
    }
  };
  struct PtrEq {
    bool operator()(const void* a, const void* b) const { return a == b; }
  };

  HashMap<const void*, Vector<ActiveTransition>, PtrHash, PtrEq> transitions_;
  usize active_count_ = 0;
};

}  // namespace vx::css
#endif  // VELOXA_CORE_CSS_TRANSITION_H_
