#ifndef VELOXA_CORE_CSS_STYLE_RESOLVER_H_
#define VELOXA_CORE_CSS_STYLE_RESOLVER_H_

#include "veloxa/core/css/computed_style.h"
#include "veloxa/core/css/selector.h"
#include "veloxa/core/css/selector_matcher.h"
#include "veloxa/core/dom/element.h"

namespace vx::css {

class StyleResolver {
 public:
  static ComputedStyle Resolve(
      const dom::Element* element,
      const Vector<Stylesheet>& stylesheets,
      const ComputedStyle* parent_style,
      const SmallVector<Declaration, 8>* inline_decls = nullptr);

 private:
  struct MatchedRule {
    const StyleRule* rule;
    u32 specificity;
    u32 order;
  };

  static void CollectMatchingRules(
      const dom::Element* element,
      const Vector<Stylesheet>& stylesheets,
      Vector<MatchedRule>& matched);

  static void SortBySpecificity(Vector<MatchedRule>& rules);

  static void ApplyDeclaration(ComputedStyle& style,
                                const Declaration& decl,
                                const ComputedStyle* parent_style);

  static void InheritProperties(ComputedStyle& style,
                                 const ComputedStyle* parent_style);
};

}  // namespace vx::css
#endif  // VELOXA_CORE_CSS_STYLE_RESOLVER_H_
