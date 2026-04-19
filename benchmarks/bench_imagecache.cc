// benchmarks/bench_imagecache.cc
//
// ImageCache + true-path Replay benchmarks (TASK-20260419-09 phase-2).
// Run: ./build-bench/benchmarks/bench_imagecache
//
// Coverage (7 BMs):
//   BM_ImageCacheLoad_Miss       Load() against growing cache, decoded each time
//   BM_ImageCacheLoad_Hit/1      hit on size-1 cache (best case)
//   BM_ImageCacheLoad_Hit/16     hit on size-16 cache, worst-case position
//   BM_ImageCacheLoad_Hit/256    hit on size-256 cache, worst-case position
//                                 (proves O(N) linear scan in path string compare)
//   BM_ReplayImageReal/16        end-to-end true-path layout+Record+Replay,
//                                 16 <img src=...> elements with cache
//   BM_ReplayImageReal/64        same shape, 64 elements
//   BM_ImageCacheGet             handle->Image* lookup (proves O(1) array index)
//
// Fixtures: written once at first call to EnsureFixturePool() into
// /tmp/vx_bench_<pid>_<i>.png; cleaned up on process exit. Each fixture is
// a 1x1 RGBA red PNG (ensures DecodeFromFile is exercised but stays cheap).

#include <benchmark/benchmark.h>

#include <png.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "benchmarks/layout_corpus.h"
#include "veloxa/core/css/selector.h"
#include "veloxa/core/image/image_cache.h"
#include "veloxa/core/layout/layout_engine.h"
#include "veloxa/core/layout/layout_utils.h"
#include "veloxa/core/render/renderer.h"
#include "veloxa/foundation/containers/vector.h"
#include "veloxa/foundation/memory/arena_allocator.h"
#include "veloxa/foundation/strings/string_view.h"
#include "veloxa/graphics/software/software_canvas.h"

namespace {

constexpr int kMaxFixtures = 256;
constexpr vx::u32 kCanvasW = 256;
constexpr vx::u32 kCanvasH = 256;

void WriteOnePng(const char* path, vx::u8 r, vx::u8 g, vx::u8 b) {
  FILE* fp = std::fopen(path, "wb");
  if (!fp) {
    std::fprintf(stderr, "FATAL: cannot open %s for write\n", path);
    std::abort();
  }
  png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr,
                                            nullptr, nullptr);
  png_infop info = png_create_info_struct(png);
  png_init_io(png, fp);
  png_set_IHDR(png, info, 1, 1, 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
               PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
  png_write_info(png, info);
  vx::u8 row[4] = {r, g, b, 255};
  png_bytep rp = row;
  png_write_rows(png, &rp, 1);
  png_write_end(png, info);
  png_destroy_write_struct(&png, &info);
  std::fclose(fp);
}

struct PngFixturePool {
  std::vector<std::string> paths;
  ~PngFixturePool() {
    for (auto& p : paths) std::remove(p.c_str());
  }
};

PngFixturePool& EnsureFixturePool(int n) {
  static PngFixturePool pool;
  static std::mutex mu;
  std::lock_guard<std::mutex> lk(mu);
  for (int i = static_cast<int>(pool.paths.size()); i < n; ++i) {
    char buf[256];
    std::snprintf(buf, sizeof(buf), "/tmp/vx_bench_%d_%d.png", getpid(), i);
    WriteOnePng(buf, 255, 0, 0);
    pool.paths.emplace_back(buf);
  }
  return pool;
}

// ---- B1: Load microbench ---------------------------------------------------

static void BM_ImageCacheLoad_Miss(benchmark::State& state) {
  auto& pool = EnsureFixturePool(kMaxFixtures);
  vx::image::ImageCache cache;
  int i = 0;
  for (auto _ : state) {
    if (i >= kMaxFixtures) {
      state.PauseTiming();
      cache.Clear();
      i = 0;
      state.ResumeTiming();
    }
    auto r = cache.Load(
        vx::StringView(pool.paths[i].data(), pool.paths[i].size()));
    benchmark::DoNotOptimize(r);
    ++i;
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_ImageCacheLoad_Miss);

template <int kSize>
static void BM_ImageCacheLoad_Hit(benchmark::State& state) {
  auto& pool = EnsureFixturePool(kSize);
  vx::image::ImageCache cache;
  for (int i = 0; i < kSize; ++i) {
    auto r = cache.Load(
        vx::StringView(pool.paths[i].data(), pool.paths[i].size()));
    if (!r.ok()) {
      std::fprintf(stderr, "FATAL: pre-load Load() failed at i=%d\n", i);
      std::abort();
    }
  }
  // Worst-case hit: scan from images_[0] to images_[kSize - 1].
  auto worst = vx::StringView(pool.paths[kSize - 1].data(),
                              pool.paths[kSize - 1].size());
  for (auto _ : state) {
    auto r = cache.Load(worst);
    benchmark::DoNotOptimize(r);
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK_TEMPLATE(BM_ImageCacheLoad_Hit, 1);
BENCHMARK_TEMPLATE(BM_ImageCacheLoad_Hit, 16);
BENCHMARK_TEMPLATE(BM_ImageCacheLoad_Hit, 256);

// ---- B2: end-to-end Replay with true ImageCache ----------------------------

struct ImageRecorded {
  std::unique_ptr<vx::ArenaAllocator> arena;
  std::unique_ptr<vx::image::ImageCache> cache;
  vx::layout::LayoutBox* root = nullptr;
  vx::render::DisplayList list;
};

const vx::Vector<vx::css::Stylesheet>& EmptySheets() {
  static const vx::Vector<vx::css::Stylesheet> sheets;
  return sheets;
}

ImageRecorded& CachedImageRec(int n) {
  static std::mutex mu;
  static std::map<int, std::unique_ptr<ImageRecorded>> mp;
  std::lock_guard<std::mutex> lk(mu);
  auto it = mp.find(n);
  if (it != mp.end()) return *it->second;

  auto rec = std::make_unique<ImageRecorded>();
  rec->arena = std::make_unique<vx::ArenaAllocator>();
  rec->cache = std::make_unique<vx::image::ImageCache>();
  auto& pool = EnsureFixturePool(n);

  // Pre-warm the cache so the inner loop's hit-path Get is exercised, not
  // a first-time decode (we want to time Replay, not background Decode).
  for (int i = 0; i < n; ++i) {
    auto r = rec->cache->Load(
        vx::StringView(pool.paths[i].data(), pool.paths[i].size()));
    if (!r.ok()) {
      std::fprintf(stderr, "FATAL: pre-warm Load() failed at i=%d\n", i);
      std::abort();
    }
  }

  auto& doc = vx::bench::CachedImageWithSrcStyledDocument(n, pool.paths);
  vx::layout::LayoutContext ctx;
  ctx.viewport_width = static_cast<vx::f32>(kCanvasW);
  ctx.viewport_height = static_cast<vx::f32>(kCanvasH);
  ctx.root_font_size = 16.0f;
  ctx.stylesheets = &EmptySheets();
  ctx.image_cache = rec->cache.get();
  rec->root = vx::layout::LayoutEngine::Layout(&doc, ctx, *rec->arena);
  rec->list = vx::render::Record(rec->root, rec->cache.get());

  auto* out = rec.get();
  mp.emplace(n, std::move(rec));
  return *out;
}

vx::gfx::sw::SoftwareCanvas& SharedCanvas() {
  static vx::u32 pixels[kCanvasW * kCanvasH] = {};
  static vx::gfx::sw::SoftwareCanvas c(pixels, kCanvasW, kCanvasH, kCanvasW);
  return c;
}

template <int kN>
static void BM_ReplayImageReal(benchmark::State& state) {
  auto& rec = CachedImageRec(kN);
  auto& c = SharedCanvas();
  for (auto _ : state) {
    vx::render::Replay(rec.list, &c, rec.cache.get());
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<int64_t>(rec.list.size()));
}
BENCHMARK_TEMPLATE(BM_ReplayImageReal, 16);
BENCHMARK_TEMPLATE(BM_ReplayImageReal, 64);

// ---- B3: Get microbench ---------------------------------------------------

static void BM_ImageCacheGet(benchmark::State& state) {
  auto& pool = EnsureFixturePool(64);
  vx::image::ImageCache cache;
  for (int i = 0; i < 64; ++i) {
    auto r = cache.Load(
        vx::StringView(pool.paths[i].data(), pool.paths[i].size()));
    if (!r.ok()) std::abort();
  }
  vx::gfx::ImageHandle h = 32;  // mid-cache, well-defined handle
  for (auto _ : state) {
    auto* img = cache.Get(h);
    benchmark::DoNotOptimize(img);
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_ImageCacheGet);

}  // namespace

BENCHMARK_MAIN();
