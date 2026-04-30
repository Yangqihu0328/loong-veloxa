# 创意设计：DevTool Hot Reload — Linux inotify 抽象层 + CSS-only 增量重载 + DOM 状态保留协议

**日期：** 2026-04-30
**状态：** /plan 头脑风暴产出，待用户审查
**关联任务：** TASK-20260430-04
**关联 spec：** `docs/specs/2026-04-30-devtool-design.md` §3 验收 A10-A12 / §4 D5 / §6.C / §7 T2

## 设计挑战

Hot Reload 是 DevTool 三件套优先级最低（D1=B 第三）但量级最高（Level 3-4）的子系统。核心挑战：
1. **跨平台 file watcher 抽象**：D5=A 锁定 Linux inotify 一期，但抽象层接口需为未来扩展 macOS kqueue / Windows ReadDirectoryChangesW 留路径
2. **CSS-only 增量重载策略**：D5.2=b — CSS 文件变更只触发 restyle，不重建 DOM；保留滚动位置 / hover / focus / JS globals
3. **路径穿越守卫（T2 安全）**：watcher root 必须严格边界检查 — symlink / 相对路径 / `..` 段都可能逃逸
4. **inotify 事件去重 + 文本编辑器 atomic write 模式兼容**：vim / VS Code 保存文件时实际是 `write tmp + rename`，inotify 收到的是 `MOVED_TO` 而非 `MODIFY` — 怎么处理？
5. **DOM 状态保留协议**：restyle 后 hover / focus 节点指针怎么不失效？
6. **dogfood 与 R3+ 强依赖**：Hot Reload C.2 增量重载触发 LoadCSS 时，如果 `LoadHTML` 改造（R3+ #3 F-025 use-after-free）未先做，可能踩 DOM bindings UAF — 怎么办？

### 约束

- 嵌入式 Linux 为主目标（其他平台留 P2）
- 自实现优先（零 FetchContent 依赖，与 V3=A dogfood 一致）
- 一期 ~150-200 LOC inotify 实现量级
- T2 路径穿越守卫不可省（V5=✅ 安全任务必加）

---

## 决策 1：file watcher 抽象层接口设计

### 候选方案

**方案 A：完全跟着 inotify 接口形态**
- 抽象层暴露 mask 概念（IN_MODIFY / IN_CLOSE_WRITE / IN_MOVED_TO 等 inotify 特有）
- 优：实现最直接
- 劣：不跨平台（macOS / Windows 没有 mask 概念）

**方案 B：抽象 callback-based 接口（不暴露平台 mask）** ⭐
- 抽象层暴露：`OnFileChanged(path, FileChangeKind)` 单一 callback
- `FileChangeKind` 枚举：`kModified`（内容变更）/ `kCreated` / `kDeleted` / `kRenamed`
- 各平台实现各自决定如何映射底层事件到抽象事件
- 优：跨平台兼容；用户代码不耦合 platform-specific
- 劣：实现层需做事件归一化（特别是 atomic write 的 MOVED_TO → kModified 转换）

**方案 C：抽象暴露多个 callback（细粒度）**
- `OnFileModified` / `OnFileCreated` / `OnFileDeleted` / `OnFileRenamed` 分别注册
- 优：用户代码可选择性订阅
- 劣：4 callback 模型复杂度高，一期不需要

### 选定方案：B（callback-based + 事件归一化）

```cpp
// veloxa/devtool/hot_reload/file_watcher.h
namespace vx::devtool {

enum class FileChangeKind : u8 {
  kModified,   // 内容变更（包括 atomic write）
  kCreated,    // 新增文件
  kDeleted,    // 删除（unlink）
  kRenamed,    // 重命名（旧路径 → 新路径，仅 source 端 callback）
};

struct FileChangeEvent {
  StringView path;             // 相对 watcher root 的路径（已 canonicalize 验证）
  FileChangeKind kind;
  u64 timestamp_ms;
};

using FileChangeCallback = vx::Function<void(const FileChangeEvent&)>;

struct WatchOptions {
  StringView root_dir;             // 必须是绝对路径（守卫强制）
  Vector<StringView> extensions;   // 仅监听这些后缀（如 ".css"）；空 = 全部
  usize max_file_size = 4 * 1024 * 1024;  // 4 MiB CSS 默认上限（T2 守卫）
  FileChangeCallback callback;
};

// 抽象基类
class FileWatcher {
 public:
  virtual ~FileWatcher() = default;
  virtual Status Start(WatchOptions opts) = 0;
  virtual void Stop() = 0;
  virtual bool IsRunning() const = 0;

  // 平台 factory
  static UniquePtr<FileWatcher> CreatePlatformDefault();
};

}  // namespace vx::devtool
```

### Platform factory 与未来扩展点

```cpp
// veloxa/devtool/hot_reload/file_watcher.cc
UniquePtr<FileWatcher> FileWatcher::CreatePlatformDefault() {
#if defined(__linux__)
  return MakeUnique<InotifyFileWatcher>();
#elif defined(__APPLE__)
  // P2 候选 — 抛 kUnimplemented Status
  VX_LOG_WARN("Hot Reload: macOS kqueue not implemented");
  return nullptr;
#elif defined(_WIN32)
  // P2 候选
  VX_LOG_WARN("Hot Reload: Windows ReadDirectoryChangesW not implemented");
  return nullptr;
#else
  return nullptr;
#endif
}
```

### Linux inotify 实现关键代码（伪码）

```cpp
// veloxa/devtool/hot_reload/file_watcher_inotify.cc
class InotifyFileWatcher : public FileWatcher {
  int inotify_fd_ = -1;
  int wd_ = -1;
  std::thread watch_thread_;
  std::atomic<bool> running_{false};
  WatchOptions opts_;
  StringHashMap<i64> last_event_ms_;  // path → timestamp，用于去重

 public:
  Status Start(WatchOptions opts) override {
    // 1. T2 安全守卫：root_dir 必须绝对路径
    if (!opts.root_dir.StartsWith("/")) {
      return Status(StatusCode::kInvalidArgument,
                    "watcher root must be absolute path");
    }

    // 2. T2 守卫：canonicalize root_dir 防 symlink
    char canonical_root[PATH_MAX];
    if (!realpath(opts.root_dir.Cstr(), canonical_root)) {
      return Status(StatusCode::kNotFound, "root_dir not found");
    }
    opts_ = opts;
    opts_.root_dir = String(canonical_root);

    // 3. inotify_init1 + add_watch
    inotify_fd_ = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (inotify_fd_ < 0) {
      return Status(StatusCode::kInternal,
                    "inotify_init1 failed: " + StrError(errno));
    }

    // 4. T2 守卫：限定 mask 仅 IN_MODIFY | IN_CLOSE_WRITE | IN_MOVED_TO
    //    不监听 IN_CREATE / IN_DELETE_SELF / IN_MOVE_SELF（防穿越）
    u32 mask = IN_MODIFY | IN_CLOSE_WRITE | IN_MOVED_TO;
    wd_ = inotify_add_watch(inotify_fd_, canonical_root, mask);
    if (wd_ < 0) {
      close(inotify_fd_);
      return Status(StatusCode::kInternal,
                    "inotify_add_watch failed");
    }

    // 5. spawn watcher thread
    running_ = true;
    watch_thread_ = std::thread([this]() { WatchLoop(); });
    return Status::OK();
  }

  void Stop() override {
    running_ = false;
    if (watch_thread_.joinable()) watch_thread_.join();
    if (wd_ >= 0) inotify_rm_watch(inotify_fd_, wd_);
    if (inotify_fd_ >= 0) close(inotify_fd_);
    inotify_fd_ = -1;
    wd_ = -1;
  }

 private:
  void WatchLoop() {
    char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
    while (running_) {
      ssize_t len = read(inotify_fd_, buf, sizeof(buf));
      if (len <= 0) {
        if (errno == EAGAIN) {
          std::this_thread::sleep_for(std::chrono::milliseconds(50));
          continue;
        }
        VX_LOG_ERROR("inotify read failed: " + StrError(errno));
        break;
      }
      ProcessBuffer(buf, len);
    }
  }

  void ProcessBuffer(const char* buf, ssize_t len) {
    const struct inotify_event* event = nullptr;
    for (const char* ptr = buf; ptr < buf + len;
         ptr += sizeof(struct inotify_event) + event->len) {
      event = reinterpret_cast<const struct inotify_event*>(ptr);
      if (event->len == 0) continue;
      String filename(event->name);

      // === T2 安全守卫核心 ===
      // 1. 拒绝相对路径 / .. 段
      if (filename.Contains("..") || filename.StartsWith("/")) {
        VX_LOG_WARN("Hot Reload: rejected suspicious path: " + filename);
        continue;
      }

      // 2. 构建 full path 并 canonicalize
      String full_path = opts_.root_dir + "/" + filename;
      char canonical[PATH_MAX];
      if (!realpath(full_path.Cstr(), canonical)) {
        // 文件可能已被删除，忽略
        continue;
      }

      // 3. boundary check — canonicalized path 必须仍在 root 下
      if (!String(canonical).StartsWith(opts_.root_dir + "/")) {
        VX_LOG_WARN("Hot Reload: path escape rejected: " + full_path);
        continue;
      }

      // 4. extension filter
      if (!opts_.extensions.IsEmpty() &&
          !MatchAnyExtension(filename, opts_.extensions)) {
        continue;
      }

      // 5. T2 max_size 守卫
      struct stat st;
      if (stat(canonical, &st) < 0) continue;
      if (static_cast<usize>(st.st_size) > opts_.max_file_size) {
        VX_LOG_WARN("Hot Reload: file too large, rejected: " + full_path);
        continue;
      }

      // 6. 去重（atomic write 模式可能瞬间多 events）
      i64 now_ms = NowMillis();
      auto* last = last_event_ms_.Get(canonical);
      if (last && (now_ms - *last) < 50) continue;  // 50ms debounce
      last_event_ms_.Insert(canonical, now_ms);

      // 7. 归一化事件类型
      FileChangeKind kind = FileChangeKind::kModified;
      // IN_MOVED_TO 是 atomic write（vim/VS Code）→ 当作 kModified
      // IN_MODIFY / IN_CLOSE_WRITE 也是 kModified

      // 8. 触发用户 callback
      FileChangeEvent fce{
          .path = String(canonical) - opts_.root_dir,  // 相对路径
          .kind = kind,
          .timestamp_ms = static_cast<u64>(now_ms),
      };
      opts_.callback(fce);
    }
  }
};
```

### 测试覆盖（spec §8 测试策略对应）

```cpp
// tests/devtool/hot_reload/file_watcher_test.cc
TEST(FileWatcherTest, ModifyFileTriggersCallback) { /* 基本路径 */ }
TEST(FileWatcherTest, AtomicWriteVimStyle) { /* tmp + rename → kModified */ }
TEST(FileWatcherTest, RejectRelativePath) { /* opts.root_dir = "./foo" → kInvalidArgument */ }
TEST(FileWatcherTest, RejectSymlinkEscape) { /* root/link → /etc/passwd → 拒绝 */ }
TEST(FileWatcherTest, RejectFileTooLarge) { /* 5 MiB CSS → 不触发 callback */ }
TEST(FileWatcherTest, ExtensionFilter) { /* 仅 .css，.html 不触发 */ }
TEST(FileWatcherTest, Debounce50ms) { /* 连写 5 次 < 50ms → 仅 1 callback */ }
TEST(FileWatcherTest, StopThenStartAgain) { /* 重启后正常工作 */ }
```

8 个测试覆盖所有 T2 守卫 + atomic write + debounce 关键路径。

---

## 决策 2：CSS-only 增量重载策略

### 候选方案

**方案 A：仅触发完整 LoadCSS（清空所有 stylesheet 重载）**
- 优：实现最简单
- 劣：多个 .css 文件中改一个会导致全部 stylesheet 重新解析，性能损失

**方案 B：识别哪个 .css 文件变了，仅 reload 对应 stylesheet**
- 优：增量重载性能好
- 劣：需追踪 stylesheet 与文件路径的关联关系

**方案 C：B + 增量 restyle（仅受影响节点 restyle，不全 restyle）** ⭐
- 在 B 基础上：CSS 变更后，根据 affected selectors 标记 dirty 节点 → 仅 dirty 节点 restyle
- 优：最佳性能
- 劣：实施复杂度最高（依赖 selector 索引）

### 选定方案：B（识别变更文件 → 重载对应 stylesheet → 全 restyle）

一期 B（量级合适）；C 列扩展段（量级 Level 4 单独立项）。

```cpp
// veloxa/devtool/hot_reload/hot_reload_manager.cc
class HotReloadManager {
  Application* app_;
  StringHashMap<u32> path_to_stylesheet_id_;  // CSS 文件路径 → stylesheet ID
  UniquePtr<FileWatcher> watcher_;

 public:
  Status Start(StringView watch_dir) {
    auto opts = WatchOptions{
        .root_dir = watch_dir,
        .extensions = Vector<StringView>{".css"},
        .max_file_size = 4 * 1024 * 1024,
        .callback = [this](const FileChangeEvent& e) { OnFileChanged(e); },
    };
    watcher_ = FileWatcher::CreatePlatformDefault();
    if (!watcher_) {
      return Status(StatusCode::kUnimplemented,
                    "Hot Reload: platform not supported");
    }
    return watcher_->Start(opts);
  }

  void RegisterStylesheet(StringView file_path, u32 stylesheet_id) {
    path_to_stylesheet_id_.Insert(file_path, stylesheet_id);
  }

 private:
  void OnFileChanged(const FileChangeEvent& e) {
    if (e.kind != FileChangeKind::kModified) return;

    auto* sheet_id = path_to_stylesheet_id_.Get(e.path);
    if (!sheet_id) return;

    // === 状态保留快照 ===
    DomStateSnapshot snapshot = SnapshotDomState(app_->target_document());

    // 重新读取 + 解析 CSS 文件
    String css_content = ReadFile(e.path);  // 已被 max_size 守卫过
    auto parsed = css::Parser::Parse(css_content);
    if (!parsed.ok()) {
      VX_LOG_ERROR("Hot Reload: CSS parse error in " + e.path);
      return;
    }

    // 替换对应 stylesheet
    app_->target_document().ReplaceStylesheet(*sheet_id, std::move(parsed.value()));

    // 全文档 restyle（一期；增量 restyle 列 P2）
    app_->target_document().Invalidate(InvalidationKind::kRestyle);

    // === 状态恢复 ===
    RestoreDomState(app_->target_document(), snapshot);

    VX_LOG_INFO("Hot Reload: reloaded CSS " + e.path);
  }
};
```

### Stylesheet 与文件路径的关联

由 Application 在 LoadCSS 时主动注册（或 `vx_view_load_css(path, &id)` 返回 ID 由用户传给 HotReloadManager）：

```cpp
// veloxa/api/veloxa_api.cc 扩展
extern "C" vx_status vx_view_attach_devtool(vx_view* view,
                                            const vx_devtool_options* opts) {
  // ... 创建 DevTool Document ...
  if (opts && opts->hot_reload_dir) {
    auto& hot_reload = view->app->hot_reload();
    hot_reload.Start(opts->hot_reload_dir);
    // 既有 stylesheets 自动注册（由 Application 维护映射）
  }
  return kVxOk;
}
```

---

## 决策 3：DOM 状态保留协议（关键）

### 挑战

CSS-only 增量重载触发全文档 restyle 后，**dom::Node tree 不变**（HTML 没变），但需要保留：
- 滚动位置（scroll_x / scroll_y）
- hover_target 节点指针
- focus_target 节点指针
- JS globals（QuickJS context）
- :hover / :active / :focus 状态标志

### dom::Node 指针稳定性分析（grep 实证 ✅）

CSS 重载不修改 DOM tree（仅替换 stylesheet）→ 所有 dom::Node* 指针保持稳定 → 状态保留只需重新关联一次即可。

但是 ComputedStyle / LayoutBox 会在 restyle / relayout 时重新计算 — **这两个对象会失效**。所以状态保留必须以 dom::Node* 为 key，不能直接持 ComputedStyle / LayoutBox 指针。

### 候选方案

**方案 A：完全不做状态保留**（restyle 后 hover/focus 全部丢失）
- 优：实现零成本
- 劣：用户体验差（每次改 CSS 都要重新 hover）

**方案 B：仅保留滚动位置 + hover/focus 节点指针**（最小集）
- 优：实现简单（4 个字段）
- 劣：JS globals / 动画进度会丢失

**方案 C：B + JS globals 不重建（JSContext 不重启）** ⭐
- CSS-only 重载不触碰 JS — JSContext 完全保留
- DOM bindings 不重建（避免 R3+ #3 F-025 use-after-free）
- 实现：HotReloadManager 仅调 `Document::ReplaceStylesheet` + `Invalidate(kRestyle)`，**不触发 LoadCSS / LoadHTML**

**方案 D：C + 动画进度保留**
- 进行中的 CSS transition / animation 在 restyle 后从相同进度继续
- 优：用户视觉体验最佳
- 劣：实施复杂（需 animation engine 区分「样式变化是否中断动画」）

### 选定方案：C（B + JS globals 不重建）

```cpp
// veloxa/devtool/hot_reload/hot_reload_manager.cc
struct DomStateSnapshot {
  i32 scroll_x;
  i32 scroll_y;
  dom::Node* hover_target;     // 仅指针，不持 ComputedStyle
  dom::Node* focus_target;
  dom::Node* active_target;
  Vector<dom::Node*> hover_chain;  // hover 状态向上传递的链
};

DomStateSnapshot SnapshotDomState(Document& doc) {
  return DomStateSnapshot{
      .scroll_x = doc.scroll_x(),
      .scroll_y = doc.scroll_y(),
      .hover_target = doc.event_manager().hover_target(),
      .focus_target = doc.event_manager().focus_target(),
      .active_target = doc.event_manager().active_target(),
      .hover_chain = doc.event_manager().hover_chain(),
  };
}

void RestoreDomState(Document& doc, const DomStateSnapshot& s) {
  doc.SetScroll(s.scroll_x, s.scroll_y);
  // hover/focus/active 直接复位指针 → restyle 时 :hover/:focus/:active 伪类自动应用
  doc.event_manager().SetHoverTarget(s.hover_target);
  doc.event_manager().SetFocusTarget(s.focus_target);
  doc.event_manager().SetActiveTarget(s.active_target);
  doc.event_manager().SetHoverChain(s.hover_chain);
}
```

### 与 R3+ #3 F-025 LoadHTML use-after-free 的依赖关系

R3+ #3 警告：`LoadHTML` 不重置 `dom_bindings_` 导致 use-after-free。

**关键判定：** Hot Reload C.2 增量重载**不触发 LoadHTML**（CSS-only 路径）→ **不踩 F-025 use-after-free**。

但是如果未来扩展段加入 HTML 增量重载（D5.2=c 三档增量）→ 必须先做 R3+ #3 F-025 修复（强依赖，spec §12.3 已记录）。

一期 D5.2=b CSS-only 锁定，意味着 Hot Reload Phase C 实施时无需先修 F-025。这是 D5=A「嵌入式专注 + CSS-only」决策的隐含 ROI。

---

## 决策 4：watcher root 边界（用户 API）

### 候选方案

**方案 A：单 root（用户传一个目录）**
- `vx_view_attach_devtool(view, opts={hot_reload_dir: "./styles"})`
- 优：API 最简单
- 劣：CSS 文件可能分散在多目录（如 `assets/css` + `vendor/lib.css`）

**方案 B：多 root 列表**
- `vx_view_attach_devtool(view, opts={hot_reload_dirs: ["./styles", "./vendor"], n: 2})`
- 优：灵活
- 劣：API 复杂度上升 + 多 inotify_add_watch 调用

**方案 C：单 root + glob pattern 子目录递归**
- `vx_view_attach_devtool(view, opts={hot_reload_dir: "./", recursive: true, extensions: [".css"]})`
- 优：最灵活
- 劣：递归 watch 性能开销 + 守卫复杂

### 选定方案：A 单 root（一期）+ B 多 root 列入 P2

一期 V1=B 三件套主线足够；多 root 与 recursive 留 P2 候选。

---

## 决策 5：错误恢复 — CSS 解析失败时怎么办？

### 候选方案

**方案 A：解析失败 → 保留旧 stylesheet 不变**（错误回退） ⭐
- 优：用户视觉体验稳定（不会因为一次手抖错误改坏页面）
- 劣：用户可能不知道自己写错了 CSS

**方案 B：解析失败 → 清空对应 stylesheet**（严格更新）
- 优：所见即所得（写错了立刻看到样式丢失）
- 劣：体验糟（一个分号忘加 = 样式全丢）

**方案 C：A + 错误日志 / DevTool 状态指示器**
- 优：双重保护 + 用户可见反馈
- 劣：依赖 DevTool 状态指示器 UI（C.3 任务）

### 选定方案：C（保留旧 stylesheet + 错误日志 + 状态指示器）

```cpp
// hot_reload_manager.cc 在 OnFileChanged 内
auto parsed = css::Parser::Parse(css_content);
if (!parsed.ok()) {
  VX_LOG_ERROR("Hot Reload: CSS parse error in " + e.path
               + ": " + parsed.status().message());
  // 通知 DevTool 状态指示器
  if (devtool_panel_) {
    devtool_panel_->SetHotReloadStatus(
        kHotReloadParseError, e.path, parsed.status().message());
  }
  // 旧 stylesheet 保持不变 → 视觉无变化
  return;
}
```

DevTool 状态指示器（C.3 任务）显示：
- ✅ 绿色：watching 3 files
- ⚠️ 黄色：parse error in /path/to/file.css — `unexpected token at line 12`

---

## 总体决策汇总

| 决策 | 锁定值 |
|---|---|
| 1. file watcher 抽象层 | B callback-based + 事件归一化（kModified / kCreated / kDeleted / kRenamed）|
| 2. CSS-only 增量策略 | B 识别变更文件 → 重载对应 stylesheet → 全文档 restyle（一期；增量 restyle 留 P2）|
| 3. DOM 状态保留协议 | C 滚动 + hover/focus/active + JS globals 不重建（CSS-only 不触发 LoadHTML 避开 F-025 UAF）|
| 4. watcher root 边界 | A 单 root（一期）；多 root + recursive + glob 列 P2 |
| 5. 错误恢复 | C 保留旧 stylesheet + 错误日志 + DevTool 状态指示器（C.3）|

## 与 spec 的交叉引用

- **§4 D5=A**：本 creative 详化了 D5 决策的具体实施模型
- **§5.1 hot_reload/ 模块边界**：file_watcher.h / file_watcher_inotify.cc / hot_reload_manager 三件本 creative 落地
- **§7 T2 路径穿越**：本 creative 决策 1 中 InotifyFileWatcher::WatchLoop 的 8 步守卫（绝对路径 / canonicalize / boundary check / extension filter / max_size / debounce / 归一化）即 T2 mitigation 落地
- **§12.3 R3+ #3 F-025 LoadHTML use-after-free 强依赖**：本 creative 决策 3 通过「CSS-only 不触发 LoadHTML」规避了强依赖，使 Phase C 不需前置 #3 修复

## 不在本 creative 范围

- macOS kqueue / Windows ReadDirectoryChangesW 实现 → spec §11 扩展段
- HTML / JS 增量 hot reload（D5.2=c 三档增量）→ spec §扩展段（与 R3+ #3 F-025 强依赖）
- DOM diff 算法（D5.2=d）→ Level 4 单独立项
- 增量 restyle（决策 2 中 C 方案）→ P2 候选
- 多 root + recursive + glob → 决策 4 中 B/C → P2 候选

---

**待用户审查重点：**
1. 决策 1 的 file watcher 抽象基类接口（FileChangeKind / WatchOptions / FileChangeCallback）是否足够通用为未来 macOS / Windows 扩展？
2. 决策 3 的 DOM 状态保留协议（C 方案）是否覆盖用户最关心场景？JS 动画进度不保留是否可接受？
3. 决策 1 中 inotify mask 仅 `IN_MODIFY | IN_CLOSE_WRITE | IN_MOVED_TO` 是否覆盖主流编辑器（vim / VS Code / sublime）的 atomic write 模式？
4. 决策 5 的错误恢复策略（保留旧 stylesheet）是否符合用户对开发体验的预期？
