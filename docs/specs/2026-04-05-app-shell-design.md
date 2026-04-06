# 设计规格：事件循环与应用壳（Application Shell）

**日期：** 2026-04-05
**任务 ID：** TASK-20260405-10

## 概述

构建 `vx::Application` 类，将所有已完成子系统串联为可运行的交互式应用运行时。

## 设计决策

### 1. 所有权模型 — 核心拥有 + 外部注入

Application 拥有核心引擎模块（Document, Stylesheets, EventManager, UpdateManager, TextShaper, Canvas），
EventLoop 和 Surface 由外部注入（嵌入式场景中宿主应用提供窗口和事件循环）。

### 2. 帧调度 — 按需帧 + 定时器心跳

EventLoop::SetTimer(1000/fps, repeat) 提供帧 tick，UpdateManager::Update() 内部检查 dirty_ 跳过无变化帧。
无变化时仅一次 bool 检查，脏区时执行完整 restyle/relayout/repaint 管线。

### 3. Canvas/Surface 生命周期 — 持久 Lock

初始化时 Lock Surface，退出时 Unlock。SoftwareCanvas 持有 pixel 指针。
HMI 场景屏幕固定，无运行时 Resize。

## API 表面

```cpp
namespace vx {
class Application {
 public:
  struct Config {
    platform::EventLoop* event_loop;
    platform::Surface* surface;
    u32 target_fps = 60;
    gfx::Color background_color = gfx::Color::White();
  };

  explicit Application(const Config& config);
  ~Application();

  void LoadHTML(StringView html);
  void LoadCSS(StringView css);
  void InjectInput(const event::InputEvent& input);

  void Run();
  void Quit();
  void Update();  // 单帧更新（测试用）

  dom::Document* document() const;
  event::EventManager& event_manager();
  const UpdateManager* update_manager() const;
};
}  // namespace vx
```

## 数据流

```
宿主应用
  ↓ Config{EventLoop*, Surface*}
Application::Application()
  → Lock Surface → 创建 SoftwareCanvas
  → 创建 EventManager + SimpleTextShaper

LoadHTML(html)
  → html::Parser::Parse → document_

LoadCSS(css)
  → css::CssParser::Parse → stylesheets_

Run()
  → 创建 UpdateManager (wiring document + stylesheets + canvas + event_manager)
  → SetTimer(1000/fps, OnFrame, repeat)
  → Update() (初始帧)
  → event_loop->Run()

OnFrame()
  → update_manager_->Update()

InjectInput(input)
  → event_manager_->HandleInput(input, layout_root)
  → InvalidationCallback → dirty_ = true → 下一帧 Update 执行重绘
```
