#ifndef VELOXA_CORE_CSS_SELECTOR_H_
#define VELOXA_CORE_CSS_SELECTOR_H_

#include "veloxa/core/css/css_value.h"
#include "veloxa/core/css/property.h"
#include "veloxa/foundation/containers/small_vector.h"
#include "veloxa/foundation/containers/vector.h"
#include "veloxa/foundation/strings/interned_string.h"

namespace vx::css {

enum class SimpleSelectorType : u8 {
  kTag,
  kClass,
  kId,
  kUniversal,
  kAttribute,
  kPseudoClass,
};

struct SimpleSelector {
  SimpleSelectorType type;
  InternedString value;
  InternedString attr_value;
};

enum class Combinator : u8 {
  kNone,
  kDescendant,
  kChild,
};

struct CompoundSelector {
  SmallVector<SimpleSelector, 3> simple_selectors;
  Combinator combinator = Combinator::kNone;
};

struct Selector {
  SmallVector<CompoundSelector, 2> compounds;
  u32 specificity = 0;
};

struct Declaration {
  PropertyId property;
  CssValue value;
  bool important = false;
};

struct StyleRule {
  SmallVector<Selector, 1> selectors;
  SmallVector<Declaration, 8> declarations;
};

struct Stylesheet {
  Vector<StyleRule> rules;
};

u32 ComputeSpecificity(const Selector& sel);

}  // namespace vx::css
#endif  // VELOXA_CORE_CSS_SELECTOR_H_
