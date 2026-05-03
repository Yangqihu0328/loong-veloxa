// TASK-20260503-01 C.2.1: HotReloadManager implementation.
//
// Strict CSS-only contract: OnFileChanged short-circuits any non-.css
// event before queueing, and DrainEvents only ever calls
// Application::LoadCSS — never LoadHTML — so the F-025 LoadHTML
// use-after-free bug (R3+ #3, still open per VAN F3) cannot be triggered
// through this path. The reverse-probe unit test
// HtmlFileChangeDoesNotCallLoadCSSorLoadHTML guards that contract.

#include "veloxa/devtool/hot_reload/hot_reload_manager.h"

#include <fstream>
#include <sstream>
#include <utility>

#include "veloxa/core/application.h"
#include "veloxa/foundation/log/log.h"

namespace vx::devtool::hot_reload {
namespace {

bool EndsWithCss(const String& path) {
  if (path.size() < 4) return false;
  const char* tail = path.data() + path.size() - 4;
  return ::memcmp(tail, ".css", 4) == 0;
}

}  // namespace

HotReloadManager::HotReloadManager(vx::Application* app) : app_(app) {}

HotReloadManager::~HotReloadManager() { Detach(); }

Status HotReloadManager::Attach(StringView root_dir) {
  Detach();

  watcher_ = FileWatcher::CreatePlatform();
  if (!watcher_) {
    return Status(StatusCode::kInternal,
                  "FileWatcher not supported on this platform");
  }

  WatchOptions opts;
  opts.root_dir = String(root_dir);
  opts.extensions.push_back(String(".css"));
  opts.callback = [this](const FileChangeEvent& evt) { OnFileChanged(evt); };

  Status s = watcher_->Start(std::move(opts));
  if (!s.ok()) {
    watcher_.reset();
    return s;
  }
  return Status::Ok();
}

void HotReloadManager::Detach() {
  if (watcher_) {
    watcher_->Stop();
    watcher_.reset();
  }
  std::lock_guard<std::mutex> lk(queue_mu_);
  while (!queue_.empty()) queue_.pop();
}

std::string HotReloadManager::last_error() const {
  std::lock_guard<std::mutex> lk(error_mu_);
  return last_error_;
}

void HotReloadManager::OnFileChanged(const FileChangeEvent& evt) {
  // R9 F-025 strict contract: drop anything that is not a .css file
  // before it can reach LoadCSS / LoadHTML.
  if (!EndsWithCss(evt.path)) return;
  std::lock_guard<std::mutex> lk(queue_mu_);
  queue_.push(evt);
}

void HotReloadManager::DrainEvents() {
  std::queue<FileChangeEvent> local;
  {
    std::lock_guard<std::mutex> lk(queue_mu_);
    std::swap(local, queue_);
  }
  while (!local.empty()) {
    const FileChangeEvent& evt = local.front();
    std::string path_str(evt.path.data(), evt.path.size());

    std::ifstream f(path_str);
    if (!f) {
      VX_LOG_WARN("Hot Reload: failed to open %s", path_str.c_str());
      {
        std::lock_guard<std::mutex> lk(error_mu_);
        last_error_ = "open failed: " + path_str;
      }
      local.pop();
      continue;
    }

    std::stringstream buf;
    buf << f.rdbuf();
    std::string content = buf.str();

    if (app_) {
      // R9 F-025 strict contract: only LoadCSS, never LoadHTML.
      app_->LoadCSS(StringView(content.data(), content.size()));
      ++tracked_count_;
      {
        std::lock_guard<std::mutex> lk(error_mu_);
        last_error_.clear();
      }
    }

    local.pop();
  }
}

}  // namespace vx::devtool::hot_reload
