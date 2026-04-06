#ifndef VELOXA_CORE_CSS_SELECTOR_MATCHER_H_
#define VELOXA_CORE_CSS_SELECTOR_MATCHER_H_

#include "veloxa/core/css/selector.h"
#include "veloxa/core/dom/element.h"

namespace vx::event {
class EventManager;
}

namespace vx::css {

class SelectorMatcher {
 public:
  static bool Matches(const Selector& selector, const dom::Element* element,
                      const event::EventManager* em = nullptr);

 private:
  static bool MatchCompound(const CompoundSelector& compound,
                             const dom::Element* element,
                             const event::EventManager* em);
  static bool MatchSimple(const SimpleSelector& simple,
                           const dom::Element* element,
                           const event::EventManager* em);
};

}  // namespace vx::css
#endif  // VELOXA_CORE_CSS_SELECTOR_MATCHER_H_
