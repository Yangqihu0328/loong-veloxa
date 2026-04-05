#ifndef VELOXA_FOUNDATION_BASE_SPAN_H_
#define VELOXA_FOUNDATION_BASE_SPAN_H_

#include "veloxa/foundation/base/assert.h"
#include "veloxa/foundation/base/types.h"

namespace vx {

template <typename T>
class Span {
 public:
  static constexpr usize npos = static_cast<usize>(-1);

  constexpr Span() noexcept : data_(nullptr), size_(0) {}
  constexpr Span(T* data, usize size) noexcept : data_(data), size_(size) {}

  template <usize N>
  constexpr Span(T (&arr)[N]) noexcept : data_(arr), size_(N) {}

  constexpr T* data() const noexcept { return data_; }
  constexpr usize size() const noexcept { return size_; }
  constexpr bool empty() const noexcept { return size_ == 0; }

  constexpr T& operator[](usize i) const {
    VX_DCHECK(i < size_);
    return data_[i];
  }

  constexpr T& front() const {
    VX_DCHECK(!empty());
    return data_[0];
  }

  constexpr T& back() const {
    VX_DCHECK(!empty());
    return data_[size_ - 1];
  }

  constexpr Span subspan(usize offset, usize count = npos) const {
    VX_DCHECK(offset <= size_);
    usize actual_count = (count == npos) ? (size_ - offset) : count;
    VX_DCHECK(offset + actual_count <= size_);
    return Span(data_ + offset, actual_count);
  }

  constexpr T* begin() const noexcept { return data_; }
  constexpr T* end() const noexcept { return data_ + size_; }

 private:
  T* data_;
  usize size_;
};

}  // namespace vx

#endif  // VELOXA_FOUNDATION_BASE_SPAN_H_
