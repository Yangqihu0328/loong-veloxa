#include "veloxa/core/update_manager.h"

#include <atomic>
#include <cstring>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "veloxa/core/css/parser.h"
#include "veloxa/core/html/parser.h"
#include "veloxa/core/layout/text_shaper.h"
#include "veloxa/core/render/render_utils.h"
#include "veloxa/graphics/software/software_canvas.h"
#include "veloxa/platform/headless/memory_surface.h"

namespace vx {
namespace {

constexpr u32 kW = 200, kH = 200, kStride = kW * 4;

class UpdateManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    doc_ = html::Parser::Parse("<div id='box'></div>");
    sheets_.push_back(css::CssParser::Parse(
        "#box { width: 100px; height: 100px; background-color: #0000ff; }"));
    std::memset(pixels_, 0, sizeof(pixels_));
    canvas_ = std::make_unique<gfx::sw::SoftwareCanvas>(pixels_, kW, kH,
                                                        kStride);
    canvas_->Begin();
    canvas_->Clear(gfx::Color::White());
  }

  dom::Document* doc_ = nullptr;
  Vector<css::Stylesheet> sheets_;
  layout::SimpleTextShaper shaper_;
  u32 pixels_[kW * kH];
  std::unique_ptr<gfx::sw::SoftwareCanvas> canvas_;

  UpdateManager::Config MakeConfig(event::EventManager* em = nullptr) {
    UpdateManager::Config cfg;
    cfg.document = doc_;
    cfg.stylesheets = &sheets_;
    cfg.layout_context.text_shaper = &shaper_;
    cfg.layout_context.viewport_width = 200;
    cfg.layout_context.viewport_height = 200;
    cfg.canvas = canvas_.get();
    cfg.event_manager = em;
    return cfg;
  }
};

TEST_F(UpdateManagerTest, InitiallyDirty) {
  UpdateManager um(MakeConfig());
  EXPECT_TRUE(um.is_dirty());
}

TEST_F(UpdateManagerTest, UpdateClearsDirty) {
  UpdateManager um(MakeConfig());
  um.Update();
  EXPECT_FALSE(um.is_dirty());
}

TEST_F(UpdateManagerTest, InvalidateSetsDirty) {
  UpdateManager um(MakeConfig());
  um.Update();
  EXPECT_FALSE(um.is_dirty());
  um.Invalidate();
  EXPECT_TRUE(um.is_dirty());
}

TEST_F(UpdateManagerTest, UpdateGeneratesLayoutRoot) {
  UpdateManager um(MakeConfig());
  um.Update();
  ASSERT_NE(um.layout_root(), nullptr);
}

TEST_F(UpdateManagerTest, UpdateGeneratesDisplayList) {
  UpdateManager um(MakeConfig());
  um.Update();
  EXPECT_FALSE(um.display_list().empty());
}

TEST_F(UpdateManagerTest, NoUpdateWhenClean) {
  UpdateManager um(MakeConfig());
  um.Update();
  auto* root1 = um.layout_root();
  auto list_size1 = um.display_list().size();
  um.Update();
  EXPECT_EQ(um.layout_root(), root1);
  EXPECT_EQ(um.display_list().size(), list_size1);
}

TEST_F(UpdateManagerTest, ReInvalidateRebuilds) {
  UpdateManager um(MakeConfig());
  um.Update();
  auto list_size1 = um.display_list().size();
  um.Invalidate();
  um.Update();
  EXPECT_EQ(um.display_list().size(), list_size1);
  EXPECT_FALSE(um.is_dirty());
}

TEST_F(UpdateManagerTest, EventManagerCallbackIntegration) {
  event::EventManager em;
  UpdateManager um(MakeConfig(&em));
  um.Update();
  EXPECT_FALSE(um.is_dirty());

  auto* root = um.layout_root();
  ASSERT_NE(root, nullptr);

  event::InputEvent move{};
  move.type = event::EventType::kPointerMove;
  move.x = 50;
  move.y = 50;
  em.HandleInput(move, root);

  EXPECT_TRUE(um.is_dirty());
}

TEST_F(UpdateManagerTest, DirtyRectComputedOnUpdate) {
  UpdateManager um(MakeConfig());
  um.Update();

  gfx::Rect first_dirty = um.last_dirty_rect();
  EXPECT_FALSE(first_dirty.IsEmpty());

  um.Invalidate();
  um.Update();

  gfx::Rect second_dirty = um.last_dirty_rect();
  EXPECT_TRUE(second_dirty.IsEmpty());
}

TEST_F(UpdateManagerTest, HoverChangeProducesDirtyRect) {
  sheets_.clear();
  sheets_.push_back(css::CssParser::Parse(
      "#box { width: 100px; height: 100px; background-color: #0000ff; } "
      "#box:hover { background-color: #ff0000; }"));

  event::EventManager em;
  UpdateManager um(MakeConfig(&em));
  um.Update();
  EXPECT_FALSE(um.is_dirty());

  auto* root = um.layout_root();
  ASSERT_NE(root, nullptr);

  event::InputEvent move{};
  move.type = event::EventType::kPointerMove;
  move.x = 50;
  move.y = 50;
  em.HandleInput(move, root);

  EXPECT_TRUE(um.is_dirty());
  um.Update();

  gfx::Rect dirty = um.last_dirty_rect();
  EXPECT_FALSE(dirty.IsEmpty());

  bool found_red = false;
  for (const auto& cmd : um.display_list()) {
    if (cmd.type == render::PaintCommand::Type::kFillRect &&
        cmd.color == render::CssColorToGfx(0xFF0000FFu)) {
      found_red = true;
      break;
    }
  }
  EXPECT_TRUE(found_red);
}

// =============================================================================
// TASK-20260502-02 B.0.1 — UpdateManager PipelineHooks 五钩子（#35 阶段 1 闭环）
// =============================================================================

// 静态 recorder（PipelineHooks Callback 是 C 函数指针，不能捕获 lambda — 用
// 文件局部静态状态记录调用顺序 + 计数）。
namespace {
std::vector<std::string>* g_pipeline_hook_order = nullptr;
std::atomic<int> g_pipeline_hook_calls[5]{};

void PipeRec_OnFrameStart(void*) {
  if (g_pipeline_hook_order) g_pipeline_hook_order->push_back("start");
  g_pipeline_hook_calls[0]++;
}
void PipeRec_OnAfterStyle(void*) {
  if (g_pipeline_hook_order) g_pipeline_hook_order->push_back("style");
  g_pipeline_hook_calls[1]++;
}
void PipeRec_OnAfterLayout(void*) {
  if (g_pipeline_hook_order) g_pipeline_hook_order->push_back("layout");
  g_pipeline_hook_calls[2]++;
}
void PipeRec_OnAfterRender(void*) {
  if (g_pipeline_hook_order) g_pipeline_hook_order->push_back("render");
  g_pipeline_hook_calls[3]++;
}
void PipeRec_OnFrameEnd(void*) {
  if (g_pipeline_hook_order) g_pipeline_hook_order->push_back("end");
  g_pipeline_hook_calls[4]++;
}

void ResetPipelineHookRecorder() {
  g_pipeline_hook_order = nullptr;
  for (auto& c : g_pipeline_hook_calls) c.store(0);
}
}  // namespace

TEST_F(UpdateManagerTest, PipelineHooksAllNullByDefault) {
  UpdateManager um(MakeConfig());
  EXPECT_EQ(um.pipeline_hooks(), nullptr);
}

TEST_F(UpdateManagerTest, SetPipelineHooksStoresPointer) {
  UpdateManager um(MakeConfig());
  PipelineHooks hooks{};
  um.SetPipelineHooks(&hooks);
  EXPECT_EQ(um.pipeline_hooks(), &hooks);
  um.SetPipelineHooks(nullptr);
  EXPECT_EQ(um.pipeline_hooks(), nullptr);
}

TEST_F(UpdateManagerTest, AllFiveHooksFireInOrderOnUpdate) {
  ResetPipelineHookRecorder();
  std::vector<std::string> order;
  g_pipeline_hook_order = &order;

  UpdateManager um(MakeConfig());
  PipelineHooks hooks;
  hooks.on_frame_start  = &PipeRec_OnFrameStart;
  hooks.on_after_style  = &PipeRec_OnAfterStyle;
  hooks.on_after_layout = &PipeRec_OnAfterLayout;
  hooks.on_after_render = &PipeRec_OnAfterRender;
  hooks.on_frame_end    = &PipeRec_OnFrameEnd;
  um.SetPipelineHooks(&hooks);
  um.Update();

  EXPECT_EQ(order, (std::vector<std::string>{"start", "style", "layout",
                                              "render", "end"}));
  ResetPipelineHookRecorder();
}

TEST_F(UpdateManagerTest, NullHooksUpdateRemainsLossless) {
  // 反向 verify A14 R3 — nullptr 路径不影响既有契约（layout_root + display_list
  // 都正确生成）。
  UpdateManager um(MakeConfig());
  um.SetPipelineHooks(nullptr);
  um.Update();
  EXPECT_NE(um.layout_root(), nullptr);
  EXPECT_FALSE(um.display_list().empty());
}

TEST_F(UpdateManagerTest, HooksNotFiredWhenNoUpdateNeeded) {
  ResetPipelineHookRecorder();
  UpdateManager um(MakeConfig());
  PipelineHooks hooks;
  hooks.on_frame_start = &PipeRec_OnFrameStart;
  um.SetPipelineHooks(&hooks);

  // 第 1 次 Update(): dirty=true → fire
  um.Update();
  EXPECT_EQ(g_pipeline_hook_calls[0].load(), 1);

  // 第 2 次 Update(): dirty=false (无 Invalidate) → 早期 return → 不 fire
  um.Update();
  EXPECT_EQ(g_pipeline_hook_calls[0].load(), 1);
  ResetPipelineHookRecorder();
}

TEST_F(UpdateManagerTest, PartialNullHooksOnlyNonNullCallbackFires) {
  // R3 mitigation 验证 — 部分 nullptr hook 时其它 hook 仍正常 fire（branch
  // predictor 友好的 nullptr-check 各自独立）。
  ResetPipelineHookRecorder();
  UpdateManager um(MakeConfig());
  PipelineHooks hooks;
  hooks.on_after_layout = &PipeRec_OnAfterLayout;  // 只设这一个
  um.SetPipelineHooks(&hooks);
  um.Update();

  EXPECT_EQ(g_pipeline_hook_calls[0].load(), 0);  // start: nullptr
  EXPECT_EQ(g_pipeline_hook_calls[1].load(), 0);  // style: nullptr
  EXPECT_EQ(g_pipeline_hook_calls[2].load(), 1);  // layout: ✅ fired
  EXPECT_EQ(g_pipeline_hook_calls[3].load(), 0);  // render: nullptr
  EXPECT_EQ(g_pipeline_hook_calls[4].load(), 0);  // end: nullptr
  ResetPipelineHookRecorder();
}

// =============================================================================
// TASK-20260502-02 B.0.2 — dirty_rects_ Vector 累积扩展
//
// 注意：依据既有 DirtyRectComputedOnUpdate 测（line 120-132），第 2 次
// Invalidate+Update 当 ComputeDirtyRect(old, new) 无变化时返 empty rect。
// 因此 Vector 累积逻辑是 "只 push 非空 dirty rect"，与既有 last_dirty_rect_
// 的 empty 语义一致 — plan §B.0.2 的 size=3 假设需现实校准为：模拟 hover
// 等真实 visual change 才能累积多个非空 rect。
// =============================================================================

TEST_F(UpdateManagerTest, DirtyRectsEmptyBeforeUpdate) {
  UpdateManager um(MakeConfig());
  EXPECT_TRUE(um.dirty_rects().empty());
}

TEST_F(UpdateManagerTest, FirstUpdateAccumulatesOneDirtyRect) {
  UpdateManager um(MakeConfig());
  um.Update();
  EXPECT_EQ(um.dirty_rects().size(), 1u);
  EXPECT_EQ(um.dirty_rects().back(), um.last_dirty_rect());
}

TEST_F(UpdateManagerTest, ReInvalidateWithoutChangeDoesNotAccumulate) {
  // 既有 DirtyRectComputedOnUpdate 行为延续：第 2 次 Update 无变化 →
  // last_dirty_rect_ empty → Vector 不 push。
  UpdateManager um(MakeConfig());
  um.Update();
  EXPECT_EQ(um.dirty_rects().size(), 1u);
  um.Invalidate();
  um.Update();
  EXPECT_EQ(um.dirty_rects().size(), 1u);  // unchanged
}

TEST_F(UpdateManagerTest, ClearDirtyRectsResetsVector) {
  UpdateManager um(MakeConfig());
  um.Update();
  EXPECT_EQ(um.dirty_rects().size(), 1u);
  um.ClearDirtyRects();
  EXPECT_TRUE(um.dirty_rects().empty());
  EXPECT_FALSE(um.last_dirty_rect().IsEmpty());  // last_ 不变（独立字段）
}

TEST_F(UpdateManagerTest, HoverChangeAccumulatesAdditionalDirtyRect) {
  // 复用 HoverChangeProducesDirtyRect 范式 — hover 变化产生 visual change
  // → ComputeDirtyRect 返非空 → push。
  sheets_.clear();
  sheets_.push_back(css::CssParser::Parse(
      "#box { width: 100px; height: 100px; background-color: #0000ff; } "
      "#box:hover { background-color: #ff0000; }"));
  event::EventManager em;
  UpdateManager um(MakeConfig(&em));
  um.Update();
  usize initial = um.dirty_rects().size();
  EXPECT_GE(initial, 1u);

  auto* root = um.layout_root();
  ASSERT_NE(root, nullptr);
  event::InputEvent move{};
  move.type = event::EventType::kPointerMove;
  move.x = 50;
  move.y = 50;
  em.HandleInput(move, root);
  um.Update();

  EXPECT_EQ(um.dirty_rects().size(), initial + 1);
  EXPECT_EQ(um.dirty_rects().back(), um.last_dirty_rect());
}

TEST_F(UpdateManagerTest, LastDirtyRectStaysContractCompatible) {
  UpdateManager um(MakeConfig());
  um.Update();
  EXPECT_EQ(um.last_dirty_rect(), um.dirty_rects().back());
}

TEST_F(UpdateManagerTest, UserdataPassedToCallback) {
  // userdata 透传验证（PerfOverlay::Attach 时传 this 给 trampoline 用）。
  static int captured = 0;
  static int sentinel = 0xC0FFEE;
  struct Local {
    static void OnStart(void* ud) { captured = *static_cast<int*>(ud); }
  };
  UpdateManager um(MakeConfig());
  PipelineHooks hooks;
  hooks.on_frame_start = &Local::OnStart;
  hooks.userdata = &sentinel;
  um.SetPipelineHooks(&hooks);
  um.Update();
  EXPECT_EQ(captured, 0xC0FFEE);
}

}  // namespace
}  // namespace vx
