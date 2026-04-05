#include "veloxa/foundation/strings/interned_string.h"

#include <string>
#include <unordered_set>

namespace vx {

namespace {

std::unordered_set<std::string>& InternTable() {
  static std::unordered_set<std::string> table;
  return table;
}

}  // namespace

InternedString InternedString::Intern(StringView sv) {
  auto& table = InternTable();
  auto it = table.emplace(sv.data(), sv.size()).first;
  return InternedString(it->c_str(), it->size());
}

void InternedString::ClearInternedStrings() { InternTable().clear(); }

}  // namespace vx
