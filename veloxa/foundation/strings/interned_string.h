#ifndef VELOXA_FOUNDATION_STRINGS_INTERNED_STRING_H_
#define VELOXA_FOUNDATION_STRINGS_INTERNED_STRING_H_

#include "veloxa/foundation/base/types.h"
#include "veloxa/foundation/strings/string_view.h"

namespace vx {

class InternedString {
 public:
  constexpr InternedString() : data_(nullptr), size_(0) {}

  static InternedString Intern(StringView sv);
  static void ClearInternedStrings();

  const char* data() const { return data_; }
  usize size() const { return size_; }
  bool empty() const { return size_ == 0; }

  StringView view() const { return StringView(data_, size_); }

  friend bool operator==(const InternedString& a, const InternedString& b) {
    return a.data_ == b.data_;
  }

  friend bool operator!=(const InternedString& a, const InternedString& b) {
    return a.data_ != b.data_;
  }

 private:
  InternedString(const char* data, usize size) : data_(data), size_(size) {}

  const char* data_;
  usize size_;
};

}  // namespace vx

#endif  // VELOXA_FOUNDATION_STRINGS_INTERNED_STRING_H_
