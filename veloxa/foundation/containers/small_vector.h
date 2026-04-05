#ifndef VELOXA_FOUNDATION_CONTAINERS_SMALL_VECTOR_H_
#define VELOXA_FOUNDATION_CONTAINERS_SMALL_VECTOR_H_

#include <cstring>
#include <initializer_list>
#include <new>
#include <type_traits>
#include <utility>

#include "veloxa/foundation/base/assert.h"
#include "veloxa/foundation/base/types.h"
#include "veloxa/foundation/memory/malloc_allocator.h"

namespace vx {

template <typename T, usize N, typename Alloc = MallocAllocator>
class SmallVector {
 public:
  using value_type = T;
  using iterator = T*;
  using const_iterator = const T*;

  SmallVector() : size_(0), capacity_(N), alloc_(&DefaultAllocator()), heap_(nullptr) {}

  explicit SmallVector(Alloc* alloc)
      : size_(0), capacity_(N), alloc_(alloc), heap_(nullptr) {}

  SmallVector(std::initializer_list<T> init, Alloc* alloc = &DefaultAllocator())
      : size_(0), capacity_(N), alloc_(alloc), heap_(nullptr) {
    reserve(init.size());
    for (const auto& v : init) {
      new (data() + size_) T(v);
      ++size_;
    }
  }

  SmallVector(const SmallVector& other)
      : size_(0), capacity_(N), alloc_(other.alloc_), heap_(nullptr) {
    reserve(other.size_);
    for (usize i = 0; i < other.size_; ++i) {
      new (data() + i) T(other.data()[i]);
    }
    size_ = other.size_;
  }

  SmallVector(SmallVector&& other) noexcept
      : size_(0), capacity_(N), alloc_(other.alloc_), heap_(nullptr) {
    if (other.IsHeap()) {
      heap_ = other.heap_;
      capacity_ = other.capacity_;
      size_ = other.size_;
      other.heap_ = nullptr;
      other.size_ = 0;
      other.capacity_ = N;
    } else {
      for (usize i = 0; i < other.size_; ++i) {
        new (data() + i) T(std::move(other.data()[i]));
        other.data()[i].~T();
      }
      size_ = other.size_;
      other.size_ = 0;
    }
  }

  SmallVector& operator=(const SmallVector& other) {
    if (this != &other) {
      SmallVector tmp(other);
      Swap(tmp);
    }
    return *this;
  }

  SmallVector& operator=(SmallVector&& other) noexcept {
    if (this != &other) {
      DestroyAll();
      DeallocateHeap();
      capacity_ = N;
      heap_ = nullptr;

      if (other.IsHeap()) {
        heap_ = other.heap_;
        capacity_ = other.capacity_;
        size_ = other.size_;
        alloc_ = other.alloc_;
        other.heap_ = nullptr;
        other.size_ = 0;
        other.capacity_ = N;
      } else {
        size_ = 0;
        alloc_ = other.alloc_;
        for (usize i = 0; i < other.size_; ++i) {
          new (data() + i) T(std::move(other.data()[i]));
          other.data()[i].~T();
        }
        size_ = other.size_;
        other.size_ = 0;
      }
    }
    return *this;
  }

  ~SmallVector() {
    DestroyAll();
    DeallocateHeap();
  }

  void push_back(const T& val) {
    EnsureCapacity(size_ + 1);
    new (data() + size_) T(val);
    ++size_;
  }

  void push_back(T&& val) {
    EnsureCapacity(size_ + 1);
    new (data() + size_) T(std::move(val));
    ++size_;
  }

  template <typename... Args>
  T& emplace_back(Args&&... args) {
    EnsureCapacity(size_ + 1);
    new (data() + size_) T(std::forward<Args>(args)...);
    return data()[size_++];
  }

  void pop_back() {
    VX_DCHECK(size_ > 0) << "pop_back on empty SmallVector";
    --size_;
    data()[size_].~T();
  }

  void clear() {
    DestroyAll();
    size_ = 0;
  }

  void reserve(usize new_cap) {
    if (new_cap <= capacity_) return;
    GrowTo(new_cap);
  }

  void resize(usize new_size) {
    if (new_size < size_) {
      for (usize i = new_size; i < size_; ++i) {
        data()[i].~T();
      }
    } else if (new_size > size_) {
      EnsureCapacity(new_size);
      for (usize i = size_; i < new_size; ++i) {
        new (data() + i) T();
      }
    }
    size_ = new_size;
  }

  iterator insert(iterator pos, const T& val) {
    VX_DCHECK(pos >= begin() && pos <= end());
    usize idx = static_cast<usize>(pos - data());
    EnsureCapacity(size_ + 1);
    T* d = data();
    if (idx < size_) {
      new (d + size_) T(std::move(d[size_ - 1]));
      for (usize i = size_ - 1; i > idx; --i) {
        d[i] = std::move(d[i - 1]);
      }
      d[idx] = val;
    } else {
      new (d + size_) T(val);
    }
    ++size_;
    return d + idx;
  }

  iterator erase(iterator pos) {
    VX_DCHECK(pos >= begin() && pos < end());
    usize idx = static_cast<usize>(pos - data());
    T* d = data();
    d[idx].~T();
    for (usize i = idx; i + 1 < size_; ++i) {
      new (d + i) T(std::move(d[i + 1]));
      d[i + 1].~T();
    }
    --size_;
    return d + idx;
  }

  iterator erase(iterator first, iterator last) {
    VX_DCHECK(first >= begin() && last <= end() && first <= last);
    if (first == last) return first;
    usize start = static_cast<usize>(first - data());
    usize count = static_cast<usize>(last - first);
    T* d = data();
    usize tail = size_ - start - count;
    for (usize i = 0; i < count; ++i) {
      d[start + i].~T();
    }
    for (usize i = 0; i < tail; ++i) {
      new (d + start + i) T(std::move(d[start + count + i]));
      d[start + count + i].~T();
    }
    size_ -= count;
    return d + start;
  }

  usize size() const { return size_; }
  usize capacity() const { return capacity_; }
  bool empty() const { return size_ == 0; }

  T* data() { return IsHeap() ? heap_ : InlineData(); }
  const T* data() const { return IsHeap() ? heap_ : InlineData(); }

  T& front() {
    VX_DCHECK(size_ > 0);
    return data()[0];
  }
  const T& front() const {
    VX_DCHECK(size_ > 0);
    return data()[0];
  }

  T& back() {
    VX_DCHECK(size_ > 0);
    return data()[size_ - 1];
  }
  const T& back() const {
    VX_DCHECK(size_ > 0);
    return data()[size_ - 1];
  }

  T& operator[](usize idx) {
    VX_DCHECK(idx < size_);
    return data()[idx];
  }
  const T& operator[](usize idx) const {
    VX_DCHECK(idx < size_);
    return data()[idx];
  }

  iterator begin() { return data(); }
  iterator end() { return data() + size_; }
  const_iterator begin() const { return data(); }
  const_iterator end() const { return data() + size_; }

  bool IsInline() const { return heap_ == nullptr; }

 private:
  bool IsHeap() const { return heap_ != nullptr; }

  T* InlineData() { return reinterpret_cast<T*>(inline_); }
  const T* InlineData() const { return reinterpret_cast<const T*>(inline_); }

  void DestroyAll() {
    T* d = data();
    for (usize i = 0; i < size_; ++i) {
      d[i].~T();
    }
  }

  void DeallocateHeap() {
    if (heap_) {
      alloc_->Deallocate(heap_, capacity_ * sizeof(T));
      heap_ = nullptr;
    }
  }

  void GrowTo(usize min_cap) {
    usize new_cap = capacity_ + capacity_ / 2;  // 1.5x
    if (new_cap < min_cap) new_cap = min_cap;

    T* new_data = static_cast<T*>(alloc_->Allocate(new_cap * sizeof(T), alignof(T)));
    T* old_data = data();
    for (usize i = 0; i < size_; ++i) {
      new (new_data + i) T(std::move(old_data[i]));
      old_data[i].~T();
    }
    DeallocateHeap();
    heap_ = new_data;
    capacity_ = new_cap;
  }

  void EnsureCapacity(usize needed) {
    if (needed > capacity_) {
      GrowTo(needed);
    }
  }

  void Swap(SmallVector& other) noexcept {
    SmallVector tmp(std::move(other));
    other = std::move(*this);
    *this = std::move(tmp);
  }

  alignas(T) char inline_[N * sizeof(T)];
  usize size_;
  usize capacity_;
  Alloc* alloc_;
  T* heap_;
};

}  // namespace vx

#endif  // VELOXA_FOUNDATION_CONTAINERS_SMALL_VECTOR_H_
