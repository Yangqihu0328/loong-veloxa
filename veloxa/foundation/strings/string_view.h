#ifndef VELOXA_FOUNDATION_STRINGS_STRING_VIEW_H_
#define VELOXA_FOUNDATION_STRINGS_STRING_VIEW_H_

#include <cstring>

#include "veloxa/foundation/base/types.h"

namespace vx {

class StringView {
 public:
  static constexpr usize npos = static_cast<usize>(-1);

  constexpr StringView() : data_(nullptr), size_(0) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  StringView(const char* str) : data_(str), size_(str ? std::strlen(str) : 0) {}

  constexpr StringView(const char* data, usize size)
      : data_(data), size_(size) {}

  constexpr const char* data() const { return data_; }
  constexpr usize size() const { return size_; }
  constexpr bool empty() const { return size_ == 0; }

  constexpr char operator[](usize idx) const { return data_[idx]; }

  constexpr char front() const { return data_[0]; }
  constexpr char back() const { return data_[size_ - 1]; }

  constexpr const char* begin() const { return data_; }
  constexpr const char* end() const { return data_ + size_; }

  constexpr StringView substr(usize pos, usize count = npos) const {
    if (pos > size_) pos = size_;
    if (count > size_ - pos) count = size_ - pos;
    return StringView(data_ + pos, count);
  }

  constexpr usize find(char ch, usize pos = 0) const {
    for (usize i = pos; i < size_; ++i) {
      if (data_[i] == ch) return i;
    }
    return npos;
  }

  constexpr usize find(StringView sv, usize pos = 0) const {
    if (sv.size_ == 0) return pos <= size_ ? pos : npos;
    if (sv.size_ > size_) return npos;
    usize limit = size_ - sv.size_;
    for (usize i = pos; i <= limit; ++i) {
      bool match = true;
      for (usize j = 0; j < sv.size_; ++j) {
        if (data_[i + j] != sv.data_[j]) {
          match = false;
          break;
        }
      }
      if (match) return i;
    }
    return npos;
  }

  constexpr bool starts_with(StringView prefix) const {
    if (prefix.size_ > size_) return false;
    for (usize i = 0; i < prefix.size_; ++i) {
      if (data_[i] != prefix.data_[i]) return false;
    }
    return true;
  }

  constexpr bool ends_with(StringView suffix) const {
    if (suffix.size_ > size_) return false;
    usize offset = size_ - suffix.size_;
    for (usize i = 0; i < suffix.size_; ++i) {
      if (data_[offset + i] != suffix.data_[i]) return false;
    }
    return true;
  }

  constexpr int compare(StringView other) const {
    usize min_len = size_ < other.size_ ? size_ : other.size_;
    for (usize i = 0; i < min_len; ++i) {
      if (data_[i] < other.data_[i]) return -1;
      if (data_[i] > other.data_[i]) return 1;
    }
    if (size_ < other.size_) return -1;
    if (size_ > other.size_) return 1;
    return 0;
  }

  friend constexpr bool operator==(StringView a, StringView b) {
    if (a.size_ != b.size_) return false;
    for (usize i = 0; i < a.size_; ++i) {
      if (a.data_[i] != b.data_[i]) return false;
    }
    return true;
  }

  friend constexpr bool operator!=(StringView a, StringView b) {
    return !(a == b);
  }

  friend constexpr bool operator<(StringView a, StringView b) {
    return a.compare(b) < 0;
  }

 private:
  const char* data_;
  usize size_;
};

}  // namespace vx

#endif  // VELOXA_FOUNDATION_STRINGS_STRING_VIEW_H_
