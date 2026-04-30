#include "veloxa/core/css/selector_matcher.h"

#include <cctype>
#include <cstring>

#include "veloxa/core/dom/tag.h"
#include "veloxa/core/event/event_manager.h"
#include "veloxa/foundation/base/assert.h"

namespace vx::css {

bool SelectorMatcher::Matches(const Selector& selector,
                               const dom::Element* element,
                               const event::EventManager* em) {
  if (selector.compounds.empty() || element == nullptr) return false;

  if (!MatchCompound(selector.compounds[0], element, em)) return false;

  const dom::Element* current = element;
  for (usize i = 1; i < selector.compounds.size(); ++i) {
    Combinator comb = selector.compounds[i].combinator;
    if (comb == Combinator::kChild) {
      current = current->parent();
      if (current == nullptr) return false;
      if (!MatchCompound(selector.compounds[i], current, em)) return false;
    } else if (comb == Combinator::kDescendant) {
      bool found = false;
      const dom::Element* ancestor = current->parent();
      while (ancestor != nullptr) {
        if (MatchCompound(selector.compounds[i], ancestor, em)) {
          current = ancestor;
          found = true;
          break;
        }
        ancestor = ancestor->parent();
      }
      if (!found) return false;
    } else {
      return false;
    }
  }
  return true;
}

bool SelectorMatcher::MatchCompound(const CompoundSelector& compound,
                                     const dom::Element* element,
                                     const event::EventManager* em) {
  for (const auto& simple : compound.simple_selectors) {
    if (!MatchSimple(simple, element, em)) return false;
  }
  return true;
}

bool SelectorMatcher::MatchSimple(const SimpleSelector& simple,
                                   const dom::Element* element,
                                   const event::EventManager* em) {
  switch (simple.type) {
    case SimpleSelectorType::kUniversal:
      return true;

    case SimpleSelectorType::kTag: {
      StringView tag_sv = simple.value.view();
      dom::TagId tag_id = dom::TagIdFromName(tag_sv);
      if (tag_id != dom::TagId::kUnknown) {
        return element->tag_id() == tag_id;
      }
      const char* el_name = element->tag_name();
      if (tag_sv.size() != std::strlen(el_name)) return false;
      for (usize j = 0; j < tag_sv.size(); ++j) {
        if (std::tolower(static_cast<unsigned char>(tag_sv[j])) !=
            std::tolower(static_cast<unsigned char>(el_name[j]))) {
          return false;
        }
      }
      return true;
    }

    case SimpleSelectorType::kClass:
      return element->HasClass(simple.value);

    case SimpleSelectorType::kId:
      return element->id() == simple.value;

    case SimpleSelectorType::kAttribute: {
      if (simple.attr_value.empty()) {
        return element->HasAttribute(simple.value);
      }
      const String* attr = element->GetAttribute(simple.value);
      if (attr == nullptr) return false;
      return attr->view() == simple.attr_value.view();
    }

    case SimpleSelectorType::kPseudoClass: {
      StringView name = simple.value.view();
      if (name == StringView("first-child")) {
        return element->prev_sibling() == nullptr;
      }
      if (name == StringView("last-child")) {
        return element->next_sibling() == nullptr;
      }
      if (name == StringView("hover")) {
        return em != nullptr && em->IsHovered(element);
      }
      if (name == StringView("active")) {
        return em != nullptr && em->IsActive(element);
      }
      if (name == StringView("focus")) {
        return em != nullptr && em->IsFocused(element);
      }
      return false;
    }
  }
  // Exhaustive over SimpleSelectorType (5 enum values); fall-through is
  // unreachable today. If a new SimpleSelectorType is added without a case
  // branch above, control reaches here. VX_DCHECK fires in debug to flag the
  // gap during development; release returns "no-match" by safe default.
  VX_DCHECK(false);
  return false;
}

}  // namespace vx::css
