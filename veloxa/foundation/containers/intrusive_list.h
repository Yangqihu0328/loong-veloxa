#ifndef VELOXA_FOUNDATION_CONTAINERS_INTRUSIVE_LIST_H_
#define VELOXA_FOUNDATION_CONTAINERS_INTRUSIVE_LIST_H_

#include <cstddef>
#include <iterator>

#include "veloxa/foundation/base/assert.h"
#include "veloxa/foundation/base/types.h"

namespace vx {

struct IntrusiveListNode {
  IntrusiveListNode* prev = nullptr;
  IntrusiveListNode* next = nullptr;
};

template <typename T, IntrusiveListNode T::*NodeMember>
class IntrusiveList {
 public:
  class Iterator {
   public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = T*;
    using reference = T&;

    explicit Iterator(IntrusiveListNode* node) : node_(node) {}

    reference operator*() const { return *NodeToItem(node_); }
    pointer operator->() const { return NodeToItem(node_); }

    Iterator& operator++() {
      node_ = node_->next;
      return *this;
    }
    Iterator operator++(int) {
      Iterator tmp = *this;
      node_ = node_->next;
      return tmp;
    }
    Iterator& operator--() {
      node_ = node_->prev;
      return *this;
    }
    Iterator operator--(int) {
      Iterator tmp = *this;
      node_ = node_->prev;
      return tmp;
    }

    bool operator==(const Iterator& other) const { return node_ == other.node_; }
    bool operator!=(const Iterator& other) const { return node_ != other.node_; }

   private:
    IntrusiveListNode* node_;
  };

  using iterator = Iterator;

  IntrusiveList() : size_(0) {
    sentinel_.prev = &sentinel_;
    sentinel_.next = &sentinel_;
  }

  IntrusiveList(const IntrusiveList&) = delete;
  IntrusiveList& operator=(const IntrusiveList&) = delete;

  void push_front(T* item) {
    IntrusiveListNode* node = &(item->*NodeMember);
    node->next = sentinel_.next;
    node->prev = &sentinel_;
    sentinel_.next->prev = node;
    sentinel_.next = node;
    ++size_;
  }

  void push_back(T* item) {
    IntrusiveListNode* node = &(item->*NodeMember);
    node->prev = sentinel_.prev;
    node->next = &sentinel_;
    sentinel_.prev->next = node;
    sentinel_.prev = node;
    ++size_;
  }

  void remove(T* item) {
    VX_DCHECK(size_ > 0) << "remove from empty list";
    IntrusiveListNode* node = &(item->*NodeMember);
    VX_DCHECK(node->prev != nullptr && node->next != nullptr);
    node->prev->next = node->next;
    node->next->prev = node->prev;
    node->prev = nullptr;
    node->next = nullptr;
    --size_;
  }

  T& front() {
    VX_DCHECK(!empty());
    return *NodeToItem(sentinel_.next);
  }
  const T& front() const {
    VX_DCHECK(!empty());
    return *NodeToItem(sentinel_.next);
  }

  T& back() {
    VX_DCHECK(!empty());
    return *NodeToItem(sentinel_.prev);
  }
  const T& back() const {
    VX_DCHECK(!empty());
    return *NodeToItem(sentinel_.prev);
  }

  bool empty() const { return size_ == 0; }
  usize size() const { return size_; }

  iterator begin() { return iterator(sentinel_.next); }
  iterator end() { return iterator(&sentinel_); }

 private:
  static T* NodeToItem(IntrusiveListNode* node) {
    // Reverse pointer-to-member offset calculation
    // Given: &(item->*NodeMember) == node
    // Then:  item == (char*)node - offsetof(T, NodeMember)
    // We compute offsetof via a null-based pointer-to-member dereference.
    usize offset =
        reinterpret_cast<usize>(&(static_cast<T*>(nullptr)->*NodeMember));
    return reinterpret_cast<T*>(reinterpret_cast<char*>(node) - offset);
  }

  IntrusiveListNode sentinel_;
  usize size_;
};

}  // namespace vx

#endif  // VELOXA_FOUNDATION_CONTAINERS_INTRUSIVE_LIST_H_
