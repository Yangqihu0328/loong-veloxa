#include "veloxa/core/dom/element.h"

namespace vx::dom {

void Element::AddClass(InternedString cls) {
  if (!HasClass(cls)) {
    classes_.push_back(cls);
  }
}

void Element::RemoveClass(InternedString cls) {
  for (usize i = 0; i < classes_.size(); ++i) {
    if (classes_[i] == cls) {
      classes_[i] = classes_[classes_.size() - 1];
      classes_.pop_back();
      return;
    }
  }
}

bool Element::HasClass(InternedString cls) const {
  for (usize i = 0; i < classes_.size(); ++i) {
    if (classes_[i] == cls) return true;
  }
  return false;
}

}  // namespace vx::dom
