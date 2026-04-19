// benchmarks/bench_layout_flex.cc
//
// Flex layout benchmarks (TASK-20260419-05 phase-1 smoke).
// Run: ./build-bench/benchmarks/bench_layout_flex

#include <benchmark/benchmark.h>

#include "veloxa/core/css/css_value.h"
#include "veloxa/core/css/enums.h"
#include "veloxa/core/css/property.h"
#include "veloxa/core/css/selector.h"
#include "veloxa/core/dom/document.h"
#include "veloxa/core/dom/element.h"
#include "veloxa/core/dom/tag.h"
#include "veloxa/core/layout/layout_engine.h"
#include "veloxa/core/layout/layout_utils.h"
#include "veloxa/foundation/containers/vector.h"
#include "veloxa/foundation/memory/arena_allocator.h"

namespace {

const vx::Vector<vx::css::Stylesheet>& EmptySheets() {
  static const vx::Vector<vx::css::Stylesheet> sheets;
  return sheets;
}

vx::layout::LayoutContext MakeFlexCtx() {
  vx::layout::LayoutContext ctx;
  ctx.viewport_width = 800;
  ctx.viewport_height = 600;
  ctx.root_font_size = 16.0f;
  ctx.stylesheets = &EmptySheets();
  return ctx;
}

vx::dom::Document& SmokeFlexDocument() {
  static vx::dom::Document doc;
  static bool built = false;
  if (!built) {
    auto* body = doc.CreateElement(vx::dom::TagId::kBody);
    body->SetInlineDeclaration(
        vx::css::PropertyId::kDisplay,
        vx::css::CssValue::Enum(static_cast<vx::u16>(vx::css::Display::kFlex)));
    doc.AppendChild(body);
    auto* child = doc.CreateElement(vx::dom::TagId::kDiv);
    body->AppendChild(child);
    built = true;
  }
  return doc;
}

static void BM_LayoutFlexSmoke(benchmark::State& state) {
  auto& doc = SmokeFlexDocument();
  auto ctx = MakeFlexCtx();
  for (auto _ : state) {
    vx::ArenaAllocator arena;
    auto* root = vx::layout::LayoutEngine::Layout(&doc, ctx, arena);
    benchmark::DoNotOptimize(root);
    benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_LayoutFlexSmoke);

}  // namespace

BENCHMARK_MAIN();
