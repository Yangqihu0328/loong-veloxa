#ifndef VELOXA_CORE_APPLICATION_H_
#define VELOXA_CORE_APPLICATION_H_

#include <memory>

#include "veloxa/core/css/parser.h"
#include "veloxa/core/dom/document.h"
#include "veloxa/core/event/event_manager.h"
#include "veloxa/core/image/image_cache.h"
#include "veloxa/core/layout/text_shaper.h"
#include "veloxa/core/update_manager.h"
#include "veloxa/foundation/strings/string_view.h"
#include "veloxa/graphics/canvas.h"
#include "veloxa/graphics/types.h"
#include "veloxa/platform/event_loop.h"
#include "veloxa/platform/surface.h"
#include "veloxa/text/font_manager.h"
#include "veloxa/text/glyph_cache.h"

namespace vx::script {
class QuickjsEngine;
class DomBindings;
}  // namespace vx::script

namespace vx {

class Application {
 public:
  struct Config {
    platform::EventLoop* event_loop = nullptr;
    platform::Surface* surface = nullptr;
    u32 target_fps = 60;
    gfx::Color background_color = gfx::Color::White();
  };

  explicit Application(const Config& config);
  ~Application();

  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;

  void LoadHTML(StringView html);
  void LoadCSS(StringView css);
  Status LoadFont(StringView path, StringView family);
  Status LoadScript(StringView source);
  void InjectInput(const event::InputEvent& input);

  void Run();
  void Quit();
  void Update();

  dom::Document* document() const { return document_; }
  event::EventManager& event_manager() { return event_manager_; }
  const UpdateManager* update_manager() const { return update_manager_.get(); }
  platform::EventLoop* event_loop() const { return config_.event_loop; }

 private:
  void OnFrame();
  void EnsureUpdateManager();

  Config config_;
  dom::Document* document_ = nullptr;
  Vector<css::Stylesheet> stylesheets_;
  event::EventManager event_manager_;
  std::unique_ptr<layout::TextShaper> text_shaper_;
  text::FontManager font_manager_;
  text::GlyphCache glyph_cache_;
  image::ImageCache image_cache_;

  u32* surface_pixels_ = nullptr;
  std::unique_ptr<gfx::Canvas> canvas_;
  std::unique_ptr<UpdateManager> update_manager_;
  std::unique_ptr<script::QuickjsEngine> script_engine_;
  std::unique_ptr<script::DomBindings> dom_bindings_;
  platform::EventLoop::TimerId frame_timer_id_ = 0;
};

}  // namespace vx

#endif  // VELOXA_CORE_APPLICATION_H_
