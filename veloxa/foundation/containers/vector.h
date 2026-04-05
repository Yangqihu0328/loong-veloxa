#ifndef VELOXA_FOUNDATION_CONTAINERS_VECTOR_H_
#define VELOXA_FOUNDATION_CONTAINERS_VECTOR_H_

#include <cstring>
#include <initializer_list>
#include <new>
#include <type_traits>
#include <utility>

#include "veloxa/foundation/base/assert.h"
#include "veloxa/foundation/base/types.h"
#include "veloxa/foundation/memory/malloc_allocator.h"

namespace vx {

template <typename T, typename Alloc = MallocAllocator>
class Vector {
 public:
  using value_type = T;
  using iterator = T*;
  using const_iterator = const T*;

  Vector() : data_(nullptr), size_(0), capacity_(0), alloc_(&DefaultAllocator()) {}

  explicit Vector(Alloc* alloc) : data_(nullptr), size_(0), capacity_(0), alloc_(alloc) {}

  Vector(usize count, const T& val, Alloc* alloc = &DefaultAllocator())
      : data_(nullptr), size_(0), capacity_(0), alloc_(alloc) {
    reserve(count);
    for (usize i = 0; i < count; ++i) {
      new (data_ + i) T(val);
    }
    size_ = count;
  }

  Vector(std::initializer_list<T> init, Alloc* alloc = &DefaultAllocator())
      : data_(nullptr), size_(0), capacity_(0), alloc_(alloc) {
    reserve(init.size());
    for (const auto& v : init) {
      new (data_ + size_) T(v);
      ++size_;
    }
  }

  Vector(const Vector& other)
      : data_(nullptr), size_(0), capacity_(0), alloc_(other.alloc_) {
    reserve(other.size_);
    for (usize i = 0; i < other.size_; ++i) {
      new (data_ + i) T(other.data_[i]);
    }
    size_ = other.size_;
  }

  Vector(Vector&& other) noexcept
      : data_(other.data_),
        size_(other.size_),
        capacity_(other.capacity_),
        alloc_(other.alloc_) {
    other.data_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;
  }

  Vector& operator=(const Vector& other) {
    if (this != &other) {
      Vector tmp(other);
      Swap(tmp);
    }
    return *this;
  }

  Vector& operator=(Vector&& other) noexcept {
    if (this != &other) {
      DestroyAll();
      DeallocateStorage();
      data_ = other.data_;
      size_ = other.size_;
      capacity_ = other.capacity_;
      alloc_ = other.alloc_;
      other.data_ = nullptr;
      other.size_ = 0;
      other.capacity_ = 0;
    }
    return *this;
  }

  ~Vector() {
    DestroyAll();
    DeallocateStorage();
  }

  void push_back(const T& val) {
    EnsureCapacity(size_ + 1);
    new (data_ + size_) T(val);
    ++size_;
  }

  void push_back(T&& val) {
    EnsureCapacity(size_ + 1);
    new (data_ + size_) T(std::move(val));
    ++size_;
  }

  template <typename... Args>
  T& emplace_back(Args&&... args) {
    EnsureCapacity(size_ + 1);
    new (data_ + size_) T(std::forward<Args>(args)...);
    return data_[size_++];
  }

  void pop_back() {
    VX_DCHECK(size_ > 0) << "pop_back on empty vector";
    --size_;
    data_[size_].~T();
  }

  void clear() {
    DestroyAll();
    size_ = 0;
  }

  void reserve(usize new_cap) {
    if (new_cap <= capacity_) return;
    Grow(new_cap);
  }

  void resize(usize new_size) {
    if (new_size < size_) {
      for (usize i = new_size; i < size_; ++i) {
        data_[i].~T();
      }
    } else if (new_size > size_) {
      EnsureCapacity(new_size);
      for (usize i = size_; i < new_size; ++i) {
        new (data_ + i) T();
      }
    }
    size_ = new_size;
  }

  void resize(usize new_size, const T& val) {
    if (new_size < size_) {
      for (usize i = new_size; i < size_; ++i) {
        data_[i].~T();
      }
    } else if (new_size > size_) {
      EnsureCapacity(new_size);
      for (usize i = size_; i < new_size; ++i) {
        new (data_ + i) T(val);
      }
    }
    size_ = new_size;
  }

  iterator insert(iterator pos, const T& val) {
    VX_DCHECK(pos >= begin() && pos <= end());
    usize idx = static_cast<usize>(pos - data_);
    EnsureCapacity(size_ + 1);
    // data_ may have changed after EnsureCapacity
    pos = data_ + idx;
    // Shift elements right
    if (idx < size_) {
      new (data_ + size_) T(std::move(data_[size_ - 1]));
      for (usize i = size_ - 1; i > idx; --i) {
        data_[i] = std::move(data_[i - 1]);
      }
      data_[idx] = val;
    } else {
      new (data_ + size_) T(val);
    }
    ++size_;
    return data_ + idx;
  }

  iterator erase(iterator pos) {
    VX_DCHECK(pos >= begin() && pos < end());
    usize idx = static_cast<usize>(pos - data_);
    data_[idx].~T();
    for (usize i = idx; i + 1 < size_; ++i) {
      new (data_ + i) T(std::move(data_[i + 1]));
      data_[i + 1].~T();
    }
    --size_;
    return data_ + idx;
  }

  iterator erase(iterator first, iterator last) {
    VX_DCHECK(first >= begin() && last <= end() && first <= last);
    if (first == last) return first;
    usize start = static_cast<usize>(first - data_);
    usize count = static_cast<usize>(last - first);
    // Move tail elements forward
    usize tail = size_ - start - count;
    for (usize i = 0; i < count; ++i) {
      data_[start + i].~T();
    }
    for (usize i = 0; i < tail; ++i) {
      new (data_ + start + i) T(std::move(data_[start + count + i]));
      data_[start + count + i].~T();
    }
    size_ -= count;
    return data_ + start;
  }

  usize size() const { return size_; }
  usize capacity() const { return capacity_; }
  bool empty() const { return size_ == 0; }
  T* data() { return data_; }
  const T* data() const { return data_; }

  T& front() {
    VX_DCHECK(size_ > 0) << "front on empty vector";
    return data_[0];
  }
  const T& front() const {
    VX_DCHECK(size_ > 0) << "front on empty vector";
    return data_[0];
  }

  T& back() {
    VX_DCHECK(size_ > 0) << "back on empty vector";
    return data_[size_ - 1];
  }
  const T& back() const {
    VX_DCHECK(size_ > 0) << "back on empty vector";
    return data_[size_ - 1];
  }

  T& operator[](usize idx) {
    VX_DCHECK(idx < size_) << "index " << idx << " out of bounds (size " << size_ << ")";
    return data_[idx];
  }
  const T& operator[](usize idx) const {
    VX_DCHECK(idx < size_) << "index " << idx << " out of bounds (size " << size_ << ")";
    return data_[idx];
  }

  iterator begin() { return data_; }
  iterator end() { return data_ + size_; }
  const_iterator begin() const { return data_; }
  const_iterator end() const { return data_ + size_; }

 private:
  void DestroyAll() {
    for (usize i = 0; i < size_; ++i) {
      data_[i].~T();
    }
  }

  void DeallocateStorage() {
    if (data_) {
      alloc_->Deallocate(data_, capacity_ * sizeof(T));
      data_ = nullptr;
      capacity_ = 0;
    }
  }

  void Grow(usize min_cap) {
    usize new_cap = capacity_ + capacity_ / 2;  // 1.5x
    if (new_cap < min_cap) new_cap = min_cap;
    if (new_cap < 4) new_cap = 4;

    T* new_data = static_cast<T*>(alloc_->Allocate(new_cap * sizeof(T), alignof(T)));
    for (usize i = 0; i < size_; ++i) {
      new (new_data + i) T(std::move(data_[i]));
      data_[i].~T();
    }
    DeallocateStorage();
    data_ = new_data;
    capacity_ = new_cap;
  }

  void EnsureCapacity(usize needed) {
    if (needed > capacity_) {
      Grow(needed);
    }
  }

  void Swap(Vector& other) noexcept {
    std::swap(data_, other.data_);
    std::swap(size_, other.size_);
    std::swap(capacity_, other.capacity_);
    std::swap(alloc_, other.alloc_);
  }

  T* data_;
  usize size_;
  usize capacity_;
  Alloc* alloc_;
};

}  // namespace vx

#endif  // VELOXA_FOUNDATION_CONTAINERS_VECTOR_H_
