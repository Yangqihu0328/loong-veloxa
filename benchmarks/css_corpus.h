// benchmarks/css_corpus.h
//
// Deterministic CSS corpus generators for CSS benchmarks. All synthesized
// strings are lazily built and cached behind a static map keyed by their
// generation parameters; subsequent calls hand back the same const reference.
// This keeps generation cost outside the benchmark inner loop entirely.
//
// Usage:
//   const std::string& css = vx::bench::StylesheetCorpus(20, 5);
//   for (auto _ : state) {
//     CssParser::Parse(StringView(css.data(), css.size()));
//   }
//
// All generators are deterministic functions of their integer parameters; no
// randomness, no I/O, no global state outside the cache.

#ifndef VELOXA_BENCHMARKS_CSS_CORPUS_H_
#define VELOXA_BENCHMARKS_CSS_CORPUS_H_

#include <cstddef>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace vx::bench {

namespace detail {

inline const char* kDeclPool[] = {
    "padding: 10px",
    "margin: 5px",
    "color: #336699",
    "background-color: #ffffff",
    "border: 1px solid #cccccc",
    "display: flex",
    "flex-direction: row",
    "justify-content: center",
    "width: 100%",
    "height: 32px",
    "font-size: 14px",
    "line-height: 1.5",
    "text-align: left",
    "opacity: 0.95",
    "z-index: 10",
};
inline constexpr int kDeclPoolSize =
    static_cast<int>(sizeof(kDeclPool) / sizeof(kDeclPool[0]));

inline std::string BuildStylesheet(int rules, int decls_per_rule) {
  std::string out;
  out.reserve(static_cast<std::size_t>(rules) * 96 +
              static_cast<std::size_t>(rules * decls_per_rule) * 32);
  for (int i = 0; i < rules; ++i) {
    out += ".cls";
    out += std::to_string(i);
    out += " { ";
    for (int j = 0; j < decls_per_rule; ++j) {
      out += kDeclPool[(i + j) % kDeclPoolSize];
      out += "; ";
    }
    out += "}\n";
  }
  return out;
}

inline std::string BuildInlineStyle(int decls) {
  std::string out;
  out.reserve(static_cast<std::size_t>(decls) * 32);
  for (int i = 0; i < decls; ++i) {
    out += kDeclPool[i % kDeclPoolSize];
    if (i + 1 < decls) out += "; ";
  }
  return out;
}

}  // namespace detail

inline const std::string& StylesheetCorpus(int rules, int decls_per_rule) {
  using Key = std::pair<int, int>;
  static std::map<Key, std::string> cache;
  static std::mutex mu;
  std::lock_guard<std::mutex> g(mu);
  Key k{rules, decls_per_rule};
  auto it = cache.find(k);
  if (it == cache.end()) {
    it = cache.emplace(k, detail::BuildStylesheet(rules, decls_per_rule)).first;
  }
  return it->second;
}

inline const std::string& InlineStyleCorpus(int decls) {
  static std::map<int, std::string> cache;
  static std::mutex mu;
  std::lock_guard<std::mutex> g(mu);
  auto it = cache.find(decls);
  if (it == cache.end()) {
    it = cache.emplace(decls, detail::BuildInlineStyle(decls)).first;
  }
  return it->second;
}

inline const std::vector<std::string>& AllPropertyNames() {
  static const std::vector<std::string> names = {
      "display", "position", "top", "right", "bottom", "left",
      "width", "height", "min-width", "min-height", "max-width", "max-height",
      "margin-top", "margin-right", "margin-bottom", "margin-left",
      "padding-top", "padding-right", "padding-bottom", "padding-left",
      "box-sizing", "overflow", "z-index",
      "flex-direction", "flex-wrap", "justify-content", "align-items",
      "align-self", "flex-grow", "flex-shrink", "flex-basis", "gap",
      "background-color", "color", "opacity",
      "border-top-width", "border-right-width", "border-bottom-width", "border-left-width",
      "border-top-style", "border-right-style", "border-bottom-style", "border-left-style",
      "border-top-color", "border-right-color", "border-bottom-color", "border-left-color",
      "border-radius", "visibility",
      "font-family", "font-size", "font-weight", "font-style",
      "line-height", "text-align", "white-space", "letter-spacing",
      "transition-property", "transition-duration", "transition-timing-function", "transition-delay",
  };
  return names;
}

inline const std::vector<std::string>& Hot5PropertyNames() {
  static const std::vector<std::string> names = {
      "display", "color", "width", "padding-top", "margin-top",
  };
  return names;
}

inline const std::vector<std::string>& MissPropertyNames() {
  static const std::vector<std::string> names = {
      "-webkit-transform", "moz-foo", "color2", "wid", "marg-top",
  };
  return names;
}

}  // namespace vx::bench

#endif  // VELOXA_BENCHMARKS_CSS_CORPUS_H_
