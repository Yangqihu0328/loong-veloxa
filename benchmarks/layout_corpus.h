// benchmarks/layout_corpus.h
//
// Deterministic DOM corpus builders for Layout + Render benchmarks.
// All documents are lazily constructed once and cached behind static maps
// keyed by their shape parameters; subsequent calls return the same
// `Document&`. Construction cost stays outside the benchmark hot loop.
//
// Memory: cached `Document`s live for the entire process and own their
// elements via the document arena. The OS reclaims them at exit.
//
// Layout shapes available:
//   FlatDocument(n)            — <body><div/>×n
//   NestedDocument(depth)      — <body><div><div>...×depth chain
//   MixedDocument()            — 3-level × 4-fanout block tree + text leaves
//   TextHeavyDocument(n)       — <body><div>text×n (each div holds 1 Text)
//   FlexDocument(rows, cols)   — <body display:flex column> ×rows
//                                 each row is <div display:flex row> ×cols
//   NestedFlexDocument()       — flex containing flex (3-level nesting)
//   ImageDocument(n)           — <body><img/>×n with default sizing
//
// Conventions:
//   - All flex documents use SetInlineDeclaration; layout context must
//     supply a non-null `stylesheets` pointer (even an empty Vector) so
//     LayoutEngine routes through StyleResolver and reads inline decls.
//   - No HTML parsing; pure DOM API.

#ifndef VELOXA_BENCHMARKS_LAYOUT_CORPUS_H_
#define VELOXA_BENCHMARKS_LAYOUT_CORPUS_H_

#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "veloxa/core/css/css_value.h"
#include "veloxa/core/css/enums.h"
#include "veloxa/core/css/property.h"
#include "veloxa/core/dom/document.h"
#include "veloxa/core/dom/element.h"
#include "veloxa/core/dom/tag.h"
#include "veloxa/foundation/base/assert.h"
#include "veloxa/foundation/strings/interned_string.h"
#include "veloxa/foundation/strings/string.h"

namespace vx::bench {

namespace detail {

inline void SetDisplay(vx::dom::Element* el, vx::css::Display d) {
  el->SetInlineDeclaration(
      vx::css::PropertyId::kDisplay,
      vx::css::CssValue::Enum(static_cast<vx::u16>(d)));
}

inline void SetFlexDirection(vx::dom::Element* el,
                             vx::css::FlexDirection d) {
  el->SetInlineDeclaration(
      vx::css::PropertyId::kFlexDirection,
      vx::css::CssValue::Enum(static_cast<vx::u16>(d)));
}

inline void BuildFlat(vx::dom::Document& doc, int n) {
  auto* body = doc.CreateElement(vx::dom::TagId::kBody);
  doc.AppendChild(body);
  for (int i = 0; i < n; ++i) {
    auto* d = doc.CreateElement(vx::dom::TagId::kDiv);
    body->AppendChild(d);
  }
}

// Styled variants used by render benches: every leaf div gets a non-zero
// background-color so RecordBox emits a FillRect command (default style has
// alpha=0 which suppresses background paints). Render benches must use these
// or their inner Replay loops will see an empty DisplayList.
inline void BuildFlatStyled(vx::dom::Document& doc, int n) {
  auto* body = doc.CreateElement(vx::dom::TagId::kBody);
  doc.AppendChild(body);
  for (int i = 0; i < n; ++i) {
    auto* d = doc.CreateElement(vx::dom::TagId::kDiv);
    d->SetInlineDeclaration(vx::css::PropertyId::kBackgroundColor,
                            vx::css::CssValue::Color(0xff336699u));
    body->AppendChild(d);
  }
}

inline void BuildTextHeavyStyled(vx::dom::Document& doc, int n) {
  auto* body = doc.CreateElement(vx::dom::TagId::kBody);
  doc.AppendChild(body);
  for (int i = 0; i < n; ++i) {
    auto* d = doc.CreateElement(vx::dom::TagId::kDiv);
    d->SetInlineDeclaration(vx::css::PropertyId::kBackgroundColor,
                            vx::css::CssValue::Color(0xff336699u));
    body->AppendChild(d);
    d->AppendChild(doc.CreateText(vx::String("The quick brown fox")));
  }
}

inline void BuildImageStyled(vx::dom::Document& doc, int n) {
  auto* body = doc.CreateElement(vx::dom::TagId::kBody);
  doc.AppendChild(body);
  for (int i = 0; i < n; ++i) {
    auto* img = doc.CreateElement(vx::dom::TagId::kImg);
    img->SetInlineDeclaration(vx::css::PropertyId::kBackgroundColor,
                              vx::css::CssValue::Color(0xff993366u));
    body->AppendChild(img);
  }
}

// TASK-20260419-09: <img> with `src` attribute pointing at fixture PNGs so
// LayoutEngine triggers the real image_cache->Load() path (sets
// LayoutType::kReplaced + image_handle), enabling end-to-end true ImageCache
// Replay benchmarks. Each src must be unique to keep cache hit/miss
// behaviour controllable from the bench side.
inline void BuildImageWithSrcStyled(vx::dom::Document& doc, int n,
                                    const std::vector<std::string>& paths) {
  VX_DCHECK(static_cast<int>(paths.size()) >= n);
  auto* body = doc.CreateElement(vx::dom::TagId::kBody);
  doc.AppendChild(body);
  for (int i = 0; i < n; ++i) {
    auto* img = doc.CreateElement(vx::dom::TagId::kImg);
    img->SetAttribute(vx::InternedString::Intern("src"),
                      vx::String(paths[i].c_str()));
    img->SetInlineDeclaration(vx::css::PropertyId::kBackgroundColor,
                              vx::css::CssValue::Color(0xff993366u));
    body->AppendChild(img);
  }
}

inline void BuildNested(vx::dom::Document& doc, int depth) {
  auto* body = doc.CreateElement(vx::dom::TagId::kBody);
  doc.AppendChild(body);
  vx::dom::Element* parent = body;
  for (int i = 0; i < depth; ++i) {
    auto* d = doc.CreateElement(vx::dom::TagId::kDiv);
    parent->AppendChild(d);
    parent = d;
  }
}

inline void BuildMixed(vx::dom::Document& doc) {
  auto* body = doc.CreateElement(vx::dom::TagId::kBody);
  doc.AppendChild(body);
  // 3-level x 4-fanout block tree, each leaf div carries 1 text node.
  for (int a = 0; a < 4; ++a) {
    auto* l1 = doc.CreateElement(vx::dom::TagId::kDiv);
    body->AppendChild(l1);
    for (int b = 0; b < 4; ++b) {
      auto* l2 = doc.CreateElement(vx::dom::TagId::kDiv);
      l1->AppendChild(l2);
      for (int c = 0; c < 4; ++c) {
        auto* l3 = doc.CreateElement(vx::dom::TagId::kDiv);
        l2->AppendChild(l3);
        l3->AppendChild(doc.CreateText(vx::String("Hello world")));
      }
    }
  }
}

inline void BuildTextHeavy(vx::dom::Document& doc, int n) {
  auto* body = doc.CreateElement(vx::dom::TagId::kBody);
  doc.AppendChild(body);
  for (int i = 0; i < n; ++i) {
    auto* d = doc.CreateElement(vx::dom::TagId::kDiv);
    body->AppendChild(d);
    d->AppendChild(doc.CreateText(vx::String("The quick brown fox")));
  }
}

inline void BuildFlex(vx::dom::Document& doc, int rows, int cols) {
  auto* body = doc.CreateElement(vx::dom::TagId::kBody);
  SetDisplay(body, vx::css::Display::kFlex);
  SetFlexDirection(body, vx::css::FlexDirection::kColumn);
  doc.AppendChild(body);
  for (int r = 0; r < rows; ++r) {
    auto* row = doc.CreateElement(vx::dom::TagId::kDiv);
    SetDisplay(row, vx::css::Display::kFlex);
    SetFlexDirection(row, vx::css::FlexDirection::kRow);
    body->AppendChild(row);
    for (int c = 0; c < cols; ++c) {
      auto* cell = doc.CreateElement(vx::dom::TagId::kDiv);
      row->AppendChild(cell);
    }
  }
}

inline void BuildNestedFlex(vx::dom::Document& doc) {
  // 3-level nested flex, 4 children per level.
  auto* body = doc.CreateElement(vx::dom::TagId::kBody);
  SetDisplay(body, vx::css::Display::kFlex);
  SetFlexDirection(body, vx::css::FlexDirection::kRow);
  doc.AppendChild(body);
  for (int a = 0; a < 4; ++a) {
    auto* l1 = doc.CreateElement(vx::dom::TagId::kDiv);
    SetDisplay(l1, vx::css::Display::kFlex);
    SetFlexDirection(l1, vx::css::FlexDirection::kColumn);
    body->AppendChild(l1);
    for (int b = 0; b < 4; ++b) {
      auto* l2 = doc.CreateElement(vx::dom::TagId::kDiv);
      SetDisplay(l2, vx::css::Display::kFlex);
      SetFlexDirection(l2, vx::css::FlexDirection::kRow);
      l1->AppendChild(l2);
      for (int c = 0; c < 4; ++c) {
        auto* leaf = doc.CreateElement(vx::dom::TagId::kDiv);
        l2->AppendChild(leaf);
      }
    }
  }
}

inline void BuildImage(vx::dom::Document& doc, int n) {
  auto* body = doc.CreateElement(vx::dom::TagId::kBody);
  doc.AppendChild(body);
  for (int i = 0; i < n; ++i) {
    auto* img = doc.CreateElement(vx::dom::TagId::kImg);
    body->AppendChild(img);
  }
}

}  // namespace detail

// ----- Cached getters (preferred for benches) -----

inline vx::dom::Document& CachedFlatDocument(int n) {
  static std::mutex mu;
  static std::map<int, vx::dom::Document*> cache;
  std::lock_guard<std::mutex> lock(mu);
  auto it = cache.find(n);
  if (it != cache.end()) return *it->second;
  auto* doc = new vx::dom::Document();
  detail::BuildFlat(*doc, n);
  cache.emplace(n, doc);
  return *doc;
}

inline vx::dom::Document& CachedNestedDocument(int depth) {
  static std::mutex mu;
  static std::map<int, vx::dom::Document*> cache;
  std::lock_guard<std::mutex> lock(mu);
  auto it = cache.find(depth);
  if (it != cache.end()) return *it->second;
  auto* doc = new vx::dom::Document();
  detail::BuildNested(*doc, depth);
  cache.emplace(depth, doc);
  return *doc;
}

inline vx::dom::Document& CachedMixedDocument() {
  static vx::dom::Document* doc = []() {
    auto* d = new vx::dom::Document();
    detail::BuildMixed(*d);
    return d;
  }();
  return *doc;
}

inline vx::dom::Document& CachedTextHeavyDocument(int n) {
  static std::mutex mu;
  static std::map<int, vx::dom::Document*> cache;
  std::lock_guard<std::mutex> lock(mu);
  auto it = cache.find(n);
  if (it != cache.end()) return *it->second;
  auto* doc = new vx::dom::Document();
  detail::BuildTextHeavy(*doc, n);
  cache.emplace(n, doc);
  return *doc;
}

inline vx::dom::Document& CachedFlexDocument(int rows, int cols) {
  static std::mutex mu;
  static std::map<std::pair<int, int>, vx::dom::Document*> cache;
  std::lock_guard<std::mutex> lock(mu);
  auto key = std::make_pair(rows, cols);
  auto it = cache.find(key);
  if (it != cache.end()) return *it->second;
  auto* doc = new vx::dom::Document();
  detail::BuildFlex(*doc, rows, cols);
  cache.emplace(key, doc);
  return *doc;
}

inline vx::dom::Document& CachedNestedFlexDocument() {
  static vx::dom::Document* doc = []() {
    auto* d = new vx::dom::Document();
    detail::BuildNestedFlex(*d);
    return d;
  }();
  return *doc;
}

inline vx::dom::Document& CachedFlatStyledDocument(int n) {
  static std::mutex mu;
  static std::map<int, vx::dom::Document*> cache;
  std::lock_guard<std::mutex> lock(mu);
  auto it = cache.find(n);
  if (it != cache.end()) return *it->second;
  auto* doc = new vx::dom::Document();
  detail::BuildFlatStyled(*doc, n);
  cache.emplace(n, doc);
  return *doc;
}

inline vx::dom::Document& CachedTextHeavyStyledDocument(int n) {
  static std::mutex mu;
  static std::map<int, vx::dom::Document*> cache;
  std::lock_guard<std::mutex> lock(mu);
  auto it = cache.find(n);
  if (it != cache.end()) return *it->second;
  auto* doc = new vx::dom::Document();
  detail::BuildTextHeavyStyled(*doc, n);
  cache.emplace(n, doc);
  return *doc;
}

inline vx::dom::Document& CachedImageStyledDocument(int n) {
  static std::mutex mu;
  static std::map<int, vx::dom::Document*> cache;
  std::lock_guard<std::mutex> lock(mu);
  auto it = cache.find(n);
  if (it != cache.end()) return *it->second;
  auto* doc = new vx::dom::Document();
  detail::BuildImageStyled(*doc, n);
  cache.emplace(n, doc);
  return *doc;
}

// TASK-20260419-09: caller must supply a `paths` vector with at least `n`
// entries; the document captures references to these paths via
// dom::Element::SetAttribute(InternedString, String) which copies the
// string content, so the caller's vector lifetime does not matter after
// the cached document is built.
inline vx::dom::Document& CachedImageWithSrcStyledDocument(
    int n, const std::vector<std::string>& paths) {
  static std::mutex mu;
  static std::map<int, vx::dom::Document*> cache;
  std::lock_guard<std::mutex> lock(mu);
  auto it = cache.find(n);
  if (it != cache.end()) return *it->second;
  auto* doc = new vx::dom::Document();
  detail::BuildImageWithSrcStyled(*doc, n, paths);
  cache.emplace(n, doc);
  return *doc;
}

inline vx::dom::Document& CachedImageDocument(int n) {
  static std::mutex mu;
  static std::map<int, vx::dom::Document*> cache;
  std::lock_guard<std::mutex> lock(mu);
  auto it = cache.find(n);
  if (it != cache.end()) return *it->second;
  auto* doc = new vx::dom::Document();
  detail::BuildImage(*doc, n);
  cache.emplace(n, doc);
  return *doc;
}

}  // namespace vx::bench

#endif  // VELOXA_BENCHMARKS_LAYOUT_CORPUS_H_
