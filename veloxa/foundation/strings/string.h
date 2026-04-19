#ifndef VELOXA_FOUNDATION_STRINGS_STRING_H_
#define VELOXA_FOUNDATION_STRINGS_STRING_H_

#include <cstring>

#include "veloxa/foundation/base/assert.h"
#include "veloxa/foundation/base/types.h"
#include "veloxa/foundation/memory/malloc_allocator.h"
#include "veloxa/foundation/strings/string_view.h"

namespace vx {

template <typename Alloc = MallocAllocator>
class BasicString {
  static constexpr usize kSSOCapacity = 22;
  static constexpr u8 kSSOFlag = 0x80;

  struct Heap {
    char* ptr;
    usize size;
    usize cap_and_flag;  // lowest bit = 0 means heap
  };

  struct SSO {
    char data[23];
    u8 size_and_flag;  // high bit = 1 means SSO, low 7 bits = size
  };

  union Storage {
    Heap heap;
    SSO sso;
  };

 public:
  BasicString() noexcept : alloc_(&DefaultAllocator()) { InitSSO(); }

  explicit BasicString(Alloc* alloc) noexcept : alloc_(alloc) { InitSSO(); }

  // NOLINTNEXTLINE(google-explicit-constructor)
  BasicString(StringView sv, Alloc* alloc = &DefaultAllocator())
      : alloc_(alloc) {
    if (sv.size() <= kSSOCapacity) {
      storage_.sso.size_and_flag =
          static_cast<u8>(sv.size()) | kSSOFlag;
      std::memcpy(storage_.sso.data, sv.data(), sv.size());
      storage_.sso.data[sv.size()] = '\0';
    } else {
      usize cap = AlignCap(sv.size() + 1);
      storage_.heap.ptr =
          static_cast<char*>(alloc_->Allocate(cap, alignof(char)));
      std::memcpy(storage_.heap.ptr, sv.data(), sv.size());
      storage_.heap.ptr[sv.size()] = '\0';
      storage_.heap.size = sv.size();
      storage_.heap.cap_and_flag = cap;  // lowest bit 0 = heap
    }
  }

  BasicString(const char* str, Alloc* alloc = &DefaultAllocator())
      : BasicString(StringView(str), alloc) {}

  // Marked [[gnu::noinline]] to break GCC IPA cross-function correlation that
  // mis-derives offset bounds for `other.data()` and emits a false-positive
  // -Warray-bounds in Release (-O2). See TASK-20260419-07.
#if defined(__GNUC__) && !defined(__clang__)
  [[gnu::noinline]]
#endif
  BasicString(const BasicString& other) : alloc_(other.alloc_) {
    if (other.IsSSO()) {
      std::memcpy(&storage_, &other.storage_, sizeof(Storage));
    } else {
      usize sz = other.size();
      usize cap = AlignCap(sz + 1);
      storage_.heap.ptr =
          static_cast<char*>(alloc_->Allocate(cap, alignof(char)));
      std::memcpy(storage_.heap.ptr, other.data(), sz);
      storage_.heap.ptr[sz] = '\0';
      storage_.heap.size = sz;
      storage_.heap.cap_and_flag = cap;
    }
  }

  BasicString(BasicString&& other) noexcept : alloc_(other.alloc_) {
    std::memcpy(&storage_, &other.storage_, sizeof(Storage));
    other.InitSSO();
  }

  BasicString& operator=(const BasicString& other) {
    if (this != &other) {
      BasicString tmp(other);
      Swap(tmp);
    }
    return *this;
  }

  BasicString& operator=(BasicString&& other) noexcept {
    if (this != &other) {
      FreeHeap();
      alloc_ = other.alloc_;
      std::memcpy(&storage_, &other.storage_, sizeof(Storage));
      other.InitSSO();
    }
    return *this;
  }

  ~BasicString() { FreeHeap(); }

  const char* data() const {
    return IsSSO() ? storage_.sso.data : storage_.heap.ptr;
  }

  char* data() {
    return IsSSO() ? storage_.sso.data : storage_.heap.ptr;
  }

  usize size() const {
    return IsSSO() ? (storage_.sso.size_and_flag & 0x7F)
                   : storage_.heap.size;
  }

  usize capacity() const {
    if (IsSSO()) return kSSOCapacity;
    return (storage_.heap.cap_and_flag & ~usize{1}) - 1;
  }

  bool empty() const { return size() == 0; }

  const char* c_str() const { return data(); }

  StringView view() const { return StringView(data(), size()); }

  // NOLINTNEXTLINE(google-explicit-constructor)
  operator StringView() const { return view(); }

  char operator[](usize idx) const {
    VX_DCHECK(idx < size());
    return data()[idx];
  }

  char& operator[](usize idx) {
    VX_DCHECK(idx < size());
    return data()[idx];
  }

  void reserve(usize new_cap) {
    if (new_cap <= capacity()) return;
    GrowTo(new_cap);
  }

  void append(StringView sv) {
    usize old_sz = size();
    usize new_sz = old_sz + sv.size();
    if (new_sz > capacity()) {
      usize grow = capacity() + capacity() / 2;
      GrowTo(grow > new_sz ? grow : new_sz);
    }
    std::memcpy(data() + old_sz, sv.data(), sv.size());
    SetSize(new_sz);
    data()[new_sz] = '\0';
  }

  void append(char ch) { append(StringView(&ch, 1)); }

  void clear() {
    SetSize(0);
    data()[0] = '\0';
  }

  BasicString& operator+=(StringView sv) {
    append(sv);
    return *this;
  }

  BasicString& operator+=(char ch) {
    append(ch);
    return *this;
  }

  friend bool operator==(const BasicString& a, StringView b) {
    return a.view() == b;
  }

  friend bool operator!=(const BasicString& a, StringView b) {
    return !(a.view() == b);
  }

  friend bool operator<(const BasicString& a, StringView b) {
    return a.view() < b;
  }

  friend bool operator==(StringView a, const BasicString& b) {
    return a == b.view();
  }

  friend bool operator!=(StringView a, const BasicString& b) {
    return !(a == b.view());
  }

  const char* begin() const { return data(); }
  const char* end() const { return data() + size(); }

 private:
  bool IsSSO() const {
    return (storage_.sso.size_and_flag & kSSOFlag) != 0;
  }

  void InitSSO() {
    std::memset(&storage_, 0, sizeof(Storage));
    storage_.sso.size_and_flag = kSSOFlag;  // SSO, size=0
  }

  void SetSize(usize sz) {
    if (IsSSO()) {
      storage_.sso.size_and_flag =
          static_cast<u8>(sz) | kSSOFlag;
    } else {
      storage_.heap.size = sz;
    }
  }

  void FreeHeap() {
    if (!IsSSO()) {
      alloc_->Deallocate(storage_.heap.ptr,
                         storage_.heap.cap_and_flag & ~usize{1});
    }
  }

  static usize AlignCap(usize cap) {
    return (cap + 1) & ~usize{1};  // round up to even
  }

  void GrowTo(usize new_cap) {
    usize aligned = AlignCap(new_cap + 1);  // +1 for null terminator
    char* new_ptr =
        static_cast<char*>(alloc_->Allocate(aligned, alignof(char)));
    usize old_sz = size();
    std::memcpy(new_ptr, data(), old_sz);
    new_ptr[old_sz] = '\0';
    FreeHeap();
    storage_.heap.ptr = new_ptr;
    storage_.heap.size = old_sz;
    storage_.heap.cap_and_flag = aligned;  // lowest bit 0 = heap
  }

  void Swap(BasicString& other) noexcept {
    Storage tmp;
    std::memcpy(&tmp, &storage_, sizeof(Storage));
    std::memcpy(&storage_, &other.storage_, sizeof(Storage));
    std::memcpy(&other.storage_, &tmp, sizeof(Storage));
    std::swap(alloc_, other.alloc_);
  }

  Storage storage_;
  Alloc* alloc_;
};

using String = BasicString<MallocAllocator>;

}  // namespace vx

#endif  // VELOXA_FOUNDATION_STRINGS_STRING_H_
