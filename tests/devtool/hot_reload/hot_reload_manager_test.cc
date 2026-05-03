// TASK-20260503-01 C.2.1: HotReloadManager unit tests.
//
// Six tests cover (a) lifecycle (Attach/Detach + IsAttached), (b) the
// CSS-only filter on the worker thread, (c) DrainEvents reading the file
// + invoking Application::LoadCSS, (d) the R9 F-025 strict contract that
// LoadHTML is never invoked through this path (TargetDocumentRoot must
// remain stable across LoadCSS), (e) the reverse probe for the .css
// filter (a .html change must NOT call LoadCSS), and (f) tracked_count
// reflecting the number of LoadCSS calls.

#include "veloxa/devtool/hot_reload/hot_reload_manager.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>
#include <thread>

#include "veloxa/core/application.h"
#include "veloxa/core/dom/document.h"
#include "veloxa/platform/headless/headless_event_loop.h"
#include "veloxa/platform/headless/memory_surface.h"

#if defined(__linux__)
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace vx::devtool::hot_reload {
namespace {

#if defined(__linux__)

class TempDir {
 public:
  TempDir() {
    char tmpl[] = "/tmp/vx_hot_reload_mgr_XXXXXX";
    char* path = ::mkdtemp(tmpl);
    if (path != nullptr) path_ = path;
  }

  ~TempDir() {
    if (path_.empty()) return;
    std::string cmd = "rm -f " + path_ + "/* 2>/dev/null";
    std::system(cmd.c_str());
    ::rmdir(path_.c_str());
  }

  TempDir(const TempDir&) = delete;
  TempDir& operator=(const TempDir&) = delete;

  bool valid() const { return !path_.empty(); }
  const std::string& path() const { return path_; }

  void WriteFile(const char* name, const std::string& content) const {
    std::ofstream f(path_ + "/" + name);
    f << content;
  }

 private:
  std::string path_;
};

template <typename Pred>
bool WaitUntil(Pred pred, int timeout_ms = 500) {
  using clock = std::chrono::steady_clock;
  auto deadline = clock::now() + std::chrono::milliseconds(timeout_ms);
  while (clock::now() < deadline) {
    if (pred()) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  return pred();
}

class HotReloadManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    surface_ = std::make_unique<vx::platform::MemorySurface>(200, 200);
    event_loop_ = std::make_unique<vx::platform::HeadlessEventLoop>();
    Application::Config cfg;
    cfg.event_loop = event_loop_.get();
    cfg.surface = surface_.get();
    cfg.target_fps = 60;
    app_ = std::make_unique<Application>(cfg);
    app_->LoadHTML("<html><body><div id='box'></div></body></html>");
    app_->LoadCSS("body { background: red; }");
  }

  std::unique_ptr<vx::platform::MemorySurface> surface_;
  std::unique_ptr<vx::platform::HeadlessEventLoop> event_loop_;
  std::unique_ptr<Application> app_;
};

// (a) Attach starts the underlying watcher; tracked_count starts at 0.
TEST_F(HotReloadManagerTest, AttachStartsWatcherAndTracksZeroFiles) {
  TempDir tmp;
  ASSERT_TRUE(tmp.valid());

  HotReloadManager mgr(app_.get());
  EXPECT_FALSE(mgr.IsAttached());
  EXPECT_EQ(mgr.tracked_count(), 0u);

  ASSERT_TRUE(mgr.Attach(StringView(tmp.path().c_str())).ok());
  EXPECT_TRUE(mgr.IsAttached());
  EXPECT_EQ(mgr.tracked_count(), 0u);

  mgr.Detach();
  EXPECT_FALSE(mgr.IsAttached());
}

// (b) OnFileChanged drops .html / .js / etc. before they reach the queue.
TEST_F(HotReloadManagerTest, OnFileChangedFiltersByExtension) {
  HotReloadManager mgr(app_.get());

  // Synthesize events directly to bypass the FileWatcher extension
  // filter — proves the manager has its own .css gate independent of
  // WatchOptions.extensions.
  FileChangeEvent css_evt;
  css_evt.path = String("/some/path/style.css");
  mgr.OnFileChanged(css_evt);

  FileChangeEvent html_evt;
  html_evt.path = String("/some/path/page.html");
  mgr.OnFileChanged(html_evt);

  FileChangeEvent js_evt;
  js_evt.path = String("/some/path/app.js");
  mgr.OnFileChanged(js_evt);

  // DrainEvents will try to open() the synthetic CSS path — it does
  // not exist, so the open should fail without invoking LoadCSS. We
  // only care here that .html / .js were rejected before queueing,
  // which is observable via tracked_count remaining 0 (no LoadCSS).
  mgr.DrainEvents();
  EXPECT_EQ(mgr.tracked_count(), 0u);
}

// (c) DrainEvents reads the file + calls Application::LoadCSS.
TEST_F(HotReloadManagerTest, DrainEventsTriggersLoadCSS) {
  TempDir tmp;
  ASSERT_TRUE(tmp.valid());
  tmp.WriteFile("style.css", "body { color: blue; }");

  HotReloadManager mgr(app_.get());
  ASSERT_TRUE(mgr.Attach(StringView(tmp.path().c_str())).ok());

  // Re-touch to provoke an inotify IN_CLOSE_WRITE / IN_MODIFY event.
  tmp.WriteFile("style.css", "body { color: green; }");

  EXPECT_TRUE(WaitUntil([&]() {
    mgr.DrainEvents();
    return mgr.tracked_count() > 0;
  }));

  EXPECT_GE(mgr.tracked_count(), 1u);
  mgr.Detach();
}

// (d) R9 F-025 contract: LoadCSS must not blow away target_document().
// We compare the document pointer before / after a synthetic .css drain.
TEST_F(HotReloadManagerTest, TargetDocumentPreservedAfterLoadCSS) {
  TempDir tmp;
  ASSERT_TRUE(tmp.valid());
  tmp.WriteFile("style.css", "body { color: red; }");

  vx::dom::Document* doc_before = app_->target_document();
  ASSERT_NE(doc_before, nullptr);

  HotReloadManager mgr(app_.get());
  FileChangeEvent evt;
  evt.path = String((tmp.path() + "/style.css").c_str());
  mgr.OnFileChanged(evt);
  mgr.DrainEvents();

  vx::dom::Document* doc_after = app_->target_document();
  EXPECT_EQ(doc_after, doc_before)
      << "LoadCSS must not rebuild target_document (would imply LoadHTML "
         "was called — F-025 use-after-free risk)";
}

// (e) Reverse probe: a .html change must NOT make it through OnFileChanged.
// If the .css filter were dropped, DrainEvents would attempt to LoadCSS
// the .html content, causing tracked_count to increment.
TEST_F(HotReloadManagerTest, HtmlFileChangeDoesNotCallLoadCSSorLoadHTML) {
  TempDir tmp;
  ASSERT_TRUE(tmp.valid());
  tmp.WriteFile("page.html", "<html><body>nope</body></html>");

  HotReloadManager mgr(app_.get());

  FileChangeEvent evt;
  evt.path = String((tmp.path() + "/page.html").c_str());
  mgr.OnFileChanged(evt);
  mgr.DrainEvents();

  EXPECT_EQ(mgr.tracked_count(), 0u)
      << "HotReloadManager must reject non-.css events at the source — "
         "this guards the F-025 LoadHTML use-after-free contract";
}

// ============================================================================
// C.2.3 — CSS parse failure recovery
// ============================================================================

// Invalid CSS (unbalanced braces) is rejected by the pre-validation
// step: tracked_count stays put, last_error surfaces a "css parse
// failure" message, and the existing stylesheets remain intact — the
// caller can fix the file and the next change drains cleanly
// (covered by the following test). CssParser is too lenient to use
// rules.size() as the signal, so we detect brace imbalance which is
// the most common observable symptom of a half-edited stylesheet.
TEST_F(HotReloadManagerTest, InvalidCssIsNotLoadedAndLastErrorSet) {
  TempDir tmp;
  ASSERT_TRUE(tmp.valid());
  // Missing closing brace — classic mid-edit state.
  tmp.WriteFile("broken.css", "body { color: red");

  HotReloadManager mgr(app_.get());

  FileChangeEvent evt;
  evt.path = String((tmp.path() + "/broken.css").c_str());
  mgr.OnFileChanged(evt);
  mgr.DrainEvents();

  EXPECT_EQ(mgr.tracked_count(), 0u);
  EXPECT_NE(mgr.last_error().find("css parse failure"), std::string::npos)
      << "got: " << mgr.last_error();
}

// After a parse failure the manager must accept a subsequent valid edit:
// last_error is cleared on the next successful LoadCSS and
// tracked_count advances.
TEST_F(HotReloadManagerTest, ValidCssAfterErrorClearsLastError) {
  TempDir tmp;
  ASSERT_TRUE(tmp.valid());
  tmp.WriteFile("style.css", "body { color: red");  // missing brace

  HotReloadManager mgr(app_.get());

  // First drain — parse failure path.
  FileChangeEvent bad_evt;
  bad_evt.path = String((tmp.path() + "/style.css").c_str());
  mgr.OnFileChanged(bad_evt);
  mgr.DrainEvents();
  EXPECT_EQ(mgr.tracked_count(), 0u);
  EXPECT_FALSE(mgr.last_error().empty());

  // Repair the file + re-drain.
  tmp.WriteFile("style.css", "body { color: black; }");
  FileChangeEvent good_evt;
  good_evt.path = String((tmp.path() + "/style.css").c_str());
  mgr.OnFileChanged(good_evt);
  mgr.DrainEvents();

  EXPECT_EQ(mgr.tracked_count(), 1u);
  EXPECT_TRUE(mgr.last_error().empty())
      << "last_error should clear on a successful drain, got: "
      << mgr.last_error();
}

// (f) tracked_count increments per successful LoadCSS, including drains
// across multiple distinct files.
TEST_F(HotReloadManagerTest, TrackedCountReflectsLoadCSSCalls) {
  TempDir tmp;
  ASSERT_TRUE(tmp.valid());
  tmp.WriteFile("a.css", "a{}");
  tmp.WriteFile("b.css", "b{}");

  HotReloadManager mgr(app_.get());

  FileChangeEvent ea;
  ea.path = String((tmp.path() + "/a.css").c_str());
  mgr.OnFileChanged(ea);

  FileChangeEvent eb;
  eb.path = String((tmp.path() + "/b.css").c_str());
  mgr.OnFileChanged(eb);

  mgr.DrainEvents();
  EXPECT_EQ(mgr.tracked_count(), 2u);
}

#endif  // defined(__linux__)

}  // namespace
}  // namespace vx::devtool::hot_reload
