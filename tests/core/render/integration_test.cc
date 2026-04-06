#include "veloxa/core/render/renderer.h"

#include <cstring>

#include <gtest/gtest.h>

#include "veloxa/core/css/parser.h"
#include "veloxa/core/html/parser.h"
#include "veloxa/core/layout/layout_engine.h"
#include "veloxa/core/layout/text_shaper.h"
#include "veloxa/core/render/render_utils.h"
#include "veloxa/graphics/software/software_canvas.h"

namespace vx::render {
namespace {

class RenderIntegrationTest : public ::testing::Test {
 protected:
  static constexpr u32 kW = 100, kH = 100, kStride = kW * 4;

  layout::SimpleTextShaper shaper_;
  layout::LayoutContext layout_ctx_;
  u32 pixels_[kW * kH];

  void SetUp() override {
    layout_ctx_.text_shaper = &shaper_;
    layout_ctx_.viewport_width = static_cast<f32>(kW);
    layout_ctx_.viewport_height = static_cast<f32>(kH);
    layout_ctx_.root_font_size = 16.0f;
    std::memset(pixels_, 0, sizeof(pixels_));
  }

  void DoRender(const char* html, const char* css) {
    doc_ = html::Parser::Parse(html);
    sheet_ = css::CssParser::Parse(css);
    sheets_.clear();
    sheets_.push_back(std::move(sheet_));
    layout_ctx_.stylesheets = &sheets_;
    auto* root = layout::LayoutEngine::Layout(doc_, layout_ctx_);

    gfx::sw::SoftwareCanvas canvas(pixels_, kW, kH, kStride);
    canvas.Begin();
    canvas.Clear(gfx::Color::White());
    Paint(root, &canvas);
    canvas.End();
  }

  DisplayList DoRecord(const char* html, const char* css) {
    doc_ = html::Parser::Parse(html);
    sheet_ = css::CssParser::Parse(css);
    sheets_.clear();
    sheets_.push_back(std::move(sheet_));
    layout_ctx_.stylesheets = &sheets_;
    auto* root = layout::LayoutEngine::Layout(doc_, layout_ctx_);
    return Record(root);
  }

  u32 PixelAt(u32 x, u32 y) const { return pixels_[y * kW + x]; }

  bool HasColorInRegion(u32 x0, u32 y0, u32 x1, u32 y1, u32 packed) const {
    for (u32 y = y0; y < y1 && y < kH; ++y) {
      for (u32 x = x0; x < x1 && x < kW; ++x) {
        if (PixelAt(x, y) == packed) return true;
      }
    }
    return false;
  }

 private:
  dom::Document* doc_ = nullptr;
  css::Stylesheet sheet_;
  Vector<css::Stylesheet> sheets_;
};

TEST_F(RenderIntegrationTest, SimpleRedDiv) {
  DoRender("<div id='r'></div>",
           "#r { width: 20px; height: 20px; background-color: red; }");

  u32 red = gfx::Color::Red().ToRGBA32();
  EXPECT_EQ(PixelAt(5, 5), red);
  EXPECT_EQ(PixelAt(10, 10), red);
}

TEST_F(RenderIntegrationTest, DivWithBorder) {
  auto list = DoRecord(
      "<div id='b'></div>",
      "#b { width: 20px; height: 20px; "
      "border: 3px solid blue; }");

  gfx::Color blue = CssColorToGfx(0x0000FFFFu);
  u32 blue_fill_count = 0;
  for (const auto& cmd : list) {
    if (cmd.type == PaintCommand::Type::kFillRect && cmd.color == blue) {
      ++blue_fill_count;
    }
  }
  EXPECT_EQ(blue_fill_count, 4u);
}

TEST_F(RenderIntegrationTest, DivWithBorderPixels) {
  DoRender("<div id='b'></div>",
           "#b { width: 20px; height: 20px; "
           "border: 3px solid blue; }");

  u32 blue = CssColorToGfx(0x0000FFFFu).ToRGBA32();
  EXPECT_TRUE(HasColorInRegion(0, 0, 30, 30, blue));
}

TEST_F(RenderIntegrationTest, TextInParagraph) {
  DoRender("<p id='t'>Hi</p>",
           "#t { color: red; font-size: 10px; }");

  u32 white = gfx::Color::White().ToRGBA32();
  bool has_non_white = false;
  for (u32 y = 0; y < 30 && !has_non_white; ++y) {
    for (u32 x = 0; x < 30 && !has_non_white; ++x) {
      if (PixelAt(x, y) != white) has_non_white = true;
    }
  }
  EXPECT_TRUE(has_non_white);
}

TEST_F(RenderIntegrationTest, VisibilityHidden) {
  DoRender("<div id='h'></div>",
           "#h { width: 20px; height: 20px; "
           "background-color: red; visibility: hidden; }");

  u32 red = gfx::Color::Red().ToRGBA32();
  EXPECT_FALSE(HasColorInRegion(0, 0, kW, kH, red));
}

TEST_F(RenderIntegrationTest, OpacityBlending) {
  DoRender("<div id='o'></div>",
           "#o { width: 20px; height: 20px; "
           "background-color: red; opacity: 0.5; }");

  u32 white = gfx::Color::White().ToRGBA32();
  u32 red = gfx::Color::Red().ToRGBA32();
  u32 pixel = PixelAt(5, 5);
  EXPECT_NE(pixel, white);
  EXPECT_NE(pixel, red);
}

TEST_F(RenderIntegrationTest, NestedColoredBlocks) {
  auto list = DoRecord(
      "<div id='outer'><div id='inner'></div></div>",
      "#outer { width: 50px; height: 50px; background-color: red; "
      "padding: 10px; }"
      "#inner { width: 30px; height: 30px; background-color: #0000ff; }");

  gfx::Color red = CssColorToGfx(0xFF0000FFu);
  gfx::Color blue = CssColorToGfx(0x0000FFFFu);

  bool found_red = false;
  bool found_blue = false;
  bool red_before_blue = false;
  for (const auto& cmd : list) {
    if (cmd.type == PaintCommand::Type::kFillRect) {
      if (cmd.color == red && !found_red) {
        found_red = true;
      }
      if (cmd.color == blue && !found_blue) {
        found_blue = true;
        if (found_red) red_before_blue = true;
      }
    }
  }
  EXPECT_TRUE(found_red);
  EXPECT_TRUE(found_blue);
  EXPECT_TRUE(red_before_blue);
}

TEST_F(RenderIntegrationTest, TransparentBackgroundNoFillRect) {
  auto list = DoRecord("<div id='t'></div>",
                       "#t { width: 20px; height: 20px; }");

  for (const auto& cmd : list) {
    if (cmd.type == PaintCommand::Type::kFillRect) {
      EXPECT_GT(cmd.color.a, 0u);
    }
  }
}

TEST_F(RenderIntegrationTest, DisplayListStructure) {
  auto list = DoRecord(
      "<div id='box'></div>",
      "#box { width: 50px; height: 50px; background-color: #00ff00; "
      "border: 2px solid #000000; }");

  u32 fill_count = 0;
  gfx::Color lime = CssColorToGfx(0x00FF00FFu);
  gfx::Color black = CssColorToGfx(0x000000FFu);
  bool has_lime_bg = false;
  bool has_black_border = false;
  for (const auto& cmd : list) {
    if (cmd.type == PaintCommand::Type::kFillRect) {
      ++fill_count;
      if (cmd.color == lime) has_lime_bg = true;
      if (cmd.color == black) has_black_border = true;
    }
  }

  EXPECT_GE(fill_count, 5u);
  EXPECT_TRUE(has_lime_bg);
  EXPECT_TRUE(has_black_border);
}

}  // namespace
}  // namespace vx::render
