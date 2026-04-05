#include "veloxa/core/dom/node.h"

#include <utility>

#include "veloxa/core/dom/document.h"
#include "veloxa/core/dom/element.h"

namespace vx::dom {

// --- Element ---

void Element::AppendChild(Node* child) {
  child->set_parent(this);
  child->set_next_sibling(nullptr);
  child->set_prev_sibling(last_child_);
  if (last_child_) {
    last_child_->set_next_sibling(child);
  } else {
    first_child_ = child;
  }
  last_child_ = child;
  ++child_count_;
}

void Element::RemoveChild(Node* child) {
  if (child->prev_sibling()) {
    child->prev_sibling()->set_next_sibling(child->next_sibling());
  } else {
    first_child_ = child->next_sibling();
  }
  if (child->next_sibling()) {
    child->next_sibling()->set_prev_sibling(child->prev_sibling());
  } else {
    last_child_ = child->prev_sibling();
  }
  child->set_parent(nullptr);
  child->set_next_sibling(nullptr);
  child->set_prev_sibling(nullptr);
  --child_count_;
}

void Element::SetAttribute(InternedString name, String value) {
  for (auto& attr : attributes_) {
    if (attr.name == name) {
      attr.value = std::move(value);
      return;
    }
  }
  attributes_.push_back(Attribute{name, std::move(value)});
}

const String* Element::GetAttribute(InternedString name) const {
  for (const auto& attr : attributes_) {
    if (attr.name == name) {
      return &attr.value;
    }
  }
  return nullptr;
}

bool Element::HasAttribute(InternedString name) const {
  return GetAttribute(name) != nullptr;
}

void Element::RemoveAttribute(InternedString name) {
  for (auto it = attributes_.begin(); it != attributes_.end(); ++it) {
    if (it->name == name) {
      attributes_.erase(it);
      return;
    }
  }
}

// --- Document ---

Element* Document::CreateElement(TagId tag_id) {
  void* mem = arena_.Allocate(sizeof(Element), alignof(Element));
  auto* el = new (mem) Element(tag_id);
  owned_nodes_.push_back(el);
  return el;
}

Text* Document::CreateText(String data) {
  void* mem = arena_.Allocate(sizeof(Text), alignof(Text));
  auto* text = new (mem) Text(std::move(data));
  owned_nodes_.push_back(text);
  return text;
}

Comment* Document::CreateComment(String data) {
  void* mem = arena_.Allocate(sizeof(Comment), alignof(Comment));
  auto* comment = new (mem) Comment(std::move(data));
  owned_nodes_.push_back(comment);
  return comment;
}

}  // namespace vx::dom
