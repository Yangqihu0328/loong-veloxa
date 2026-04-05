#ifndef VELOXA_FOUNDATION_CONTAINERS_HASH_MAP_H_
#define VELOXA_FOUNDATION_CONTAINERS_HASH_MAP_H_

#include <cstring>
#include <functional>
#include <new>
#include <type_traits>
#include <utility>

#include "veloxa/foundation/base/assert.h"
#include "veloxa/foundation/base/types.h"
#include "veloxa/foundation/memory/malloc_allocator.h"

namespace vx {

template <typename K, typename V, typename Hash = std::hash<K>,
          typename Eq = std::equal_to<K>, typename Alloc = MallocAllocator>
class HashMap {
 public:
  struct Slot {
    K key;
    V value;
  };

  static constexpr u8 kEmpty = 0x80;
  static constexpr u8 kDeleted = 0xFE;
  static constexpr usize kDefaultCapacity = 16;

  class Iterator {
   public:
    Iterator(u8* ctrl, Slot* slots, usize index, usize capacity)
        : ctrl_(ctrl), slots_(slots), index_(index), capacity_(capacity) {
      AdvancePastEmptyOrDeleted();
    }

    Slot& operator*() const { return slots_[index_]; }
    Slot* operator->() const { return &slots_[index_]; }

    Iterator& operator++() {
      ++index_;
      AdvancePastEmptyOrDeleted();
      return *this;
    }
    Iterator operator++(int) {
      Iterator tmp = *this;
      ++(*this);
      return tmp;
    }

    bool operator==(const Iterator& other) const { return index_ == other.index_; }
    bool operator!=(const Iterator& other) const { return index_ != other.index_; }

   private:
    void AdvancePastEmptyOrDeleted() {
      while (index_ < capacity_ && (ctrl_[index_] == kEmpty || ctrl_[index_] == kDeleted)) {
        ++index_;
      }
    }

    u8* ctrl_;
    Slot* slots_;
    usize index_;
    usize capacity_;
  };

  using iterator = Iterator;

  HashMap() : HashMap(&DefaultAllocator()) {}

  explicit HashMap(Alloc* alloc)
      : ctrl_(nullptr),
        slots_(nullptr),
        size_(0),
        capacity_(0),
        alloc_(alloc) {
    InitTable(kDefaultCapacity);
  }

  HashMap(const HashMap& other)
      : ctrl_(nullptr),
        slots_(nullptr),
        size_(0),
        capacity_(0),
        alloc_(other.alloc_) {
    InitTable(other.capacity_);
    for (usize i = 0; i < other.capacity_; ++i) {
      if (other.ctrl_[i] != kEmpty && other.ctrl_[i] != kDeleted) {
        InsertInternal(other.slots_[i].key, other.slots_[i].value);
      }
    }
  }

  HashMap(HashMap&& other) noexcept
      : ctrl_(other.ctrl_),
        slots_(other.slots_),
        size_(other.size_),
        capacity_(other.capacity_),
        alloc_(other.alloc_) {
    other.ctrl_ = nullptr;
    other.slots_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;
  }

  HashMap& operator=(const HashMap& other) {
    if (this != &other) {
      HashMap tmp(other);
      Swap(tmp);
    }
    return *this;
  }

  HashMap& operator=(HashMap&& other) noexcept {
    if (this != &other) {
      DestroyTable();
      ctrl_ = other.ctrl_;
      slots_ = other.slots_;
      size_ = other.size_;
      capacity_ = other.capacity_;
      alloc_ = other.alloc_;
      other.ctrl_ = nullptr;
      other.slots_ = nullptr;
      other.size_ = 0;
      other.capacity_ = 0;
    }
    return *this;
  }

  ~HashMap() { DestroyTable(); }

  bool Insert(const K& key, const V& value) {
    if (NeedsRehash()) {
      Rehash(capacity_ * 2);
    }
    return InsertInternal(key, value);
  }

  V* Find(const K& key) {
    if (capacity_ == 0) return nullptr;
    usize hash = Hash{}(key);
    u8 h2 = H2(hash);
    usize pos = H1(hash) & (capacity_ - 1);

    for (usize i = 0; i < capacity_; ++i) {
      usize idx = (pos + i) & (capacity_ - 1);
      if (ctrl_[idx] == kEmpty) return nullptr;
      if (ctrl_[idx] == h2 && Eq{}(slots_[idx].key, key)) {
        return &slots_[idx].value;
      }
    }
    return nullptr;
  }

  const V* Find(const K& key) const {
    return const_cast<HashMap*>(this)->Find(key);
  }

  bool Contains(const K& key) const { return Find(key) != nullptr; }

  bool Erase(const K& key) {
    if (capacity_ == 0) return false;
    usize hash = Hash{}(key);
    u8 h2 = H2(hash);
    usize pos = H1(hash) & (capacity_ - 1);

    for (usize i = 0; i < capacity_; ++i) {
      usize idx = (pos + i) & (capacity_ - 1);
      if (ctrl_[idx] == kEmpty) return false;
      if (ctrl_[idx] == h2 && Eq{}(slots_[idx].key, key)) {
        slots_[idx].key.~K();
        slots_[idx].value.~V();
        ctrl_[idx] = kDeleted;
        --size_;
        return true;
      }
    }
    return false;
  }

  V& operator[](const K& key) {
    if (NeedsRehash()) {
      Rehash(capacity_ * 2);
    }
    usize hash = Hash{}(key);
    u8 h2 = H2(hash);
    usize pos = H1(hash) & (capacity_ - 1);
    usize insert_idx = capacity_;  // sentinel

    for (usize i = 0; i < capacity_; ++i) {
      usize idx = (pos + i) & (capacity_ - 1);
      if (ctrl_[idx] == h2 && Eq{}(slots_[idx].key, key)) {
        return slots_[idx].value;
      }
      if (ctrl_[idx] == kEmpty) {
        if (insert_idx == capacity_) insert_idx = idx;
        break;
      }
      if (ctrl_[idx] == kDeleted && insert_idx == capacity_) {
        insert_idx = idx;
      }
    }

    VX_DCHECK(insert_idx < capacity_);
    new (&slots_[insert_idx].key) K(key);
    new (&slots_[insert_idx].value) V();
    ctrl_[insert_idx] = h2;
    ++size_;
    return slots_[insert_idx].value;
  }

  usize size() const { return size_; }
  bool empty() const { return size_ == 0; }

  void clear() {
    for (usize i = 0; i < capacity_; ++i) {
      if (ctrl_[i] != kEmpty && ctrl_[i] != kDeleted) {
        slots_[i].key.~K();
        slots_[i].value.~V();
        ctrl_[i] = kEmpty;
      }
    }
    size_ = 0;
  }

  void reserve(usize count) {
    // Ensure capacity is enough to hold count elements at 7/8 load
    usize needed = (count * 8 + 6) / 7;  // ceiling of count * 8/7
    usize new_cap = capacity_;
    while (new_cap < needed) {
      new_cap *= 2;
    }
    if (new_cap > capacity_) {
      Rehash(new_cap);
    }
  }

  iterator begin() { return iterator(ctrl_, slots_, 0, capacity_); }
  iterator end() { return iterator(ctrl_, slots_, capacity_, capacity_); }

 private:
  static u8 H2(usize hash) { return static_cast<u8>(hash & 0x7F); }
  static usize H1(usize hash) { return hash >> 7; }

  bool NeedsRehash() const {
    // Load factor 7/8
    return size_ * 8 >= capacity_ * 7;
  }

  void InitTable(usize cap) {
    VX_DCHECK((cap & (cap - 1)) == 0) << "capacity must be power of 2";
    usize alloc_size = cap * sizeof(u8) + cap * sizeof(Slot);
    void* mem = alloc_->Allocate(alloc_size, alignof(Slot));
    // Layout: ctrl bytes first, then slots (Slot-aligned)
    // Place slots first for alignment, then ctrl at the end
    slots_ = static_cast<Slot*>(mem);
    ctrl_ = reinterpret_cast<u8*>(slots_ + cap);
    capacity_ = cap;
    std::memset(ctrl_, kEmpty, cap);
  }

  void DestroyTable() {
    if (!slots_) return;
    for (usize i = 0; i < capacity_; ++i) {
      if (ctrl_[i] != kEmpty && ctrl_[i] != kDeleted) {
        slots_[i].key.~K();
        slots_[i].value.~V();
      }
    }
    usize alloc_size = capacity_ * sizeof(u8) + capacity_ * sizeof(Slot);
    alloc_->Deallocate(slots_, alloc_size);
    ctrl_ = nullptr;
    slots_ = nullptr;
    capacity_ = 0;
  }

  bool InsertInternal(const K& key, const V& value) {
    usize hash = Hash{}(key);
    u8 h2 = H2(hash);
    usize pos = H1(hash) & (capacity_ - 1);
    usize insert_idx = capacity_;  // sentinel

    for (usize i = 0; i < capacity_; ++i) {
      usize idx = (pos + i) & (capacity_ - 1);
      if (ctrl_[idx] == h2 && Eq{}(slots_[idx].key, key)) {
        slots_[idx].value = value;
        return false;  // updated existing
      }
      if (ctrl_[idx] == kEmpty) {
        if (insert_idx == capacity_) insert_idx = idx;
        break;
      }
      if (ctrl_[idx] == kDeleted && insert_idx == capacity_) {
        insert_idx = idx;
      }
    }

    VX_DCHECK(insert_idx < capacity_);
    new (&slots_[insert_idx].key) K(key);
    new (&slots_[insert_idx].value) V(value);
    ctrl_[insert_idx] = h2;
    ++size_;
    return true;  // new insert
  }

  void Rehash(usize new_cap) {
    VX_DCHECK((new_cap & (new_cap - 1)) == 0);
    u8* old_ctrl = ctrl_;
    Slot* old_slots = slots_;
    usize old_cap = capacity_;

    ctrl_ = nullptr;
    slots_ = nullptr;
    capacity_ = 0;
    size_ = 0;
    InitTable(new_cap);

    for (usize i = 0; i < old_cap; ++i) {
      if (old_ctrl[i] != kEmpty && old_ctrl[i] != kDeleted) {
        InsertInternal(std::move(old_slots[i].key), std::move(old_slots[i].value));
        old_slots[i].key.~K();
        old_slots[i].value.~V();
      }
    }

    if (old_slots) {
      usize old_alloc_size = old_cap * sizeof(u8) + old_cap * sizeof(Slot);
      alloc_->Deallocate(old_slots, old_alloc_size);
    }
  }

  // Move-aware overload for Rehash
  bool InsertInternal(K&& key, V&& value) {
    usize hash = Hash{}(key);
    u8 h2 = H2(hash);
    usize pos = H1(hash) & (capacity_ - 1);
    usize insert_idx = capacity_;

    for (usize i = 0; i < capacity_; ++i) {
      usize idx = (pos + i) & (capacity_ - 1);
      if (ctrl_[idx] == h2 && Eq{}(slots_[idx].key, key)) {
        slots_[idx].value = std::move(value);
        return false;
      }
      if (ctrl_[idx] == kEmpty) {
        if (insert_idx == capacity_) insert_idx = idx;
        break;
      }
      if (ctrl_[idx] == kDeleted && insert_idx == capacity_) {
        insert_idx = idx;
      }
    }

    VX_DCHECK(insert_idx < capacity_);
    new (&slots_[insert_idx].key) K(std::move(key));
    new (&slots_[insert_idx].value) V(std::move(value));
    ctrl_[insert_idx] = h2;
    ++size_;
    return true;
  }

  void Swap(HashMap& other) noexcept {
    std::swap(ctrl_, other.ctrl_);
    std::swap(slots_, other.slots_);
    std::swap(size_, other.size_);
    std::swap(capacity_, other.capacity_);
    std::swap(alloc_, other.alloc_);
  }

  u8* ctrl_;
  Slot* slots_;
  usize size_;
  usize capacity_;
  Alloc* alloc_;
};

}  // namespace vx

#endif  // VELOXA_FOUNDATION_CONTAINERS_HASH_MAP_H_
