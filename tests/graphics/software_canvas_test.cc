#include "veloxa/graphics/software/software_canvas.h"

#include <cstring>
#include <memory>

#include <gtest/gtest.h>

namespace vx::gfx::sw {
namespace {

constexpr vx::u32 kWidth = 20;
constexpr vx::u32 kHeight = 20;
constexpr vx::u32 kStride = kWidth * 4;

class SoftwareCanvasTest : public ::testing::Test {
 protected:
  void SetUp() override {
    std::memset(pixels_, 0, sizeof(pixels_));
    canvas_ =
        std::make_unique<SoftwareCanvas>(pixels_, kWidth, kHeight, kStride);
    canvas_->Begin();
  }

  void TearDown() override { canvas_->End(); }

  vx::u32 PixelAt(vx::u32 x, vx::u32 y) const {
    return pixels_[y * (kStride / 4) + x];
  }

  bool ChannelsNear(vx::u32 pixel, vx::u8 er, vx::u8 eg, vx::u8 eb,
                    vx::u8 ea, int tolerance = 1) const {
    int r = static_cast<int>(pixel & 0xFF);
    int g = static_cast<int>((pixel >> 8) & 0xFF);
    int b = static_cast<int>((pixel >> 16) & 0xFF);
    int a = static_cast<int>((pixel >> 24) & 0xFF);
    auto near = [tolerance](int actual, int expected) {
      int diff = actual - expected;
      return diff >= -tolerance && diff <= tolerance;
    };
    return near(r, er) && near(g, eg) && near(b, eb) && near(a, ea);
  }

  vx::u32 pixels_[kWidth * kHeight];
  std::unique_ptr<SoftwareCanvas> canvas_;
};

TEST_F(SoftwareCanvasTest, ClearFillsAllPixels) {
  canvas_->Clear(Color::Red());
  vx::u32 red = Color::Red().ToRGBA32();
  for (vx::u32 i = 0; i < kWidth * kHeight; ++i) {
    EXPECT_EQ(pixels_[i], red);
  }
}

TEST_F(SoftwareCanvasTest, FillRectSolidColor) {
  canvas_->Clear(Color::White());
  canvas_->FillRect({2, 2, 4, 4}, Brush::Solid(Color::Red()));

  vx::u32 red = Color::Red().ToRGBA32();
  vx::u32 white = Color::White().ToRGBA32();

  EXPECT_EQ(PixelAt(0, 0), white);
  EXPECT_EQ(PixelAt(1, 1), white);
  EXPECT_EQ(PixelAt(2, 2), red);
  EXPECT_EQ(PixelAt(3, 3), red);
  EXPECT_EQ(PixelAt(5, 5), red);
  EXPECT_EQ(PixelAt(6, 6), white);
  EXPECT_EQ(PixelAt(10, 10), white);
}

TEST_F(SoftwareCanvasTest, FillRectWithAlpha) {
  canvas_->Clear(Color::White());
  Color half_alpha_red = Color::FromRGBA(255, 0, 0, 128);
  canvas_->FillRect({2, 2, 4, 4}, Brush::Solid(half_alpha_red));

  EXPECT_TRUE(ChannelsNear(PixelAt(3, 3), 255, 127, 127, 255, 2));
}

TEST_F(SoftwareCanvasTest, PushClipRectLimitsDrawing) {
  canvas_->Clear(Color::White());
  canvas_->PushClipRect({5, 5, 5, 5});
  canvas_->FillRect({0, 0, 20, 20}, Brush::Solid(Color::Red()));
  canvas_->PopClip();

  vx::u32 white = Color::White().ToRGBA32();
  vx::u32 red = Color::Red().ToRGBA32();

  EXPECT_EQ(PixelAt(0, 0), white);
  EXPECT_EQ(PixelAt(4, 4), white);
  EXPECT_EQ(PixelAt(5, 5), red);
  EXPECT_EQ(PixelAt(7, 7), red);
  EXPECT_EQ(PixelAt(9, 9), red);
  EXPECT_EQ(PixelAt(10, 10), white);
}

TEST_F(SoftwareCanvasTest, TranslationShiftsFillRect) {
  canvas_->Clear(Color::White());
  canvas_->SetTransform(Matrix3x2::Translation(5, 5));
  canvas_->FillRect({0, 0, 3, 3}, Brush::Solid(Color::Red()));

  vx::u32 white = Color::White().ToRGBA32();
  vx::u32 red = Color::Red().ToRGBA32();

  EXPECT_EQ(PixelAt(4, 4), white);
  EXPECT_EQ(PixelAt(5, 5), red);
  EXPECT_EQ(PixelAt(6, 6), red);
  EXPECT_EQ(PixelAt(7, 7), red);
  EXPECT_EQ(PixelAt(8, 8), white);
}

TEST_F(SoftwareCanvasTest, PushPopStateRestoresTransform) {
  canvas_->PushState();
  canvas_->SetTransform(Matrix3x2::Translation(10, 10));
  EXPECT_FLOAT_EQ(canvas_->GetTransform().m[4], 10.0f);
  canvas_->PopState();
  EXPECT_FLOAT_EQ(canvas_->GetTransform().m[4], 0.0f);
}

TEST_F(SoftwareCanvasTest, FillPathTriangle) {
  canvas_->Clear(Color::White());
  auto path = canvas_->CreatePath();
  path->MoveTo({5, 5});
  path->LineTo({15, 5});
  path->LineTo({10, 15});
  path->Close();
  canvas_->FillPath(*path, Brush::Solid(Color::Red()));

  vx::u32 white = Color::White().ToRGBA32();
  vx::u32 red = Color::Red().ToRGBA32();

  EXPECT_EQ(PixelAt(10, 8), red);
  EXPECT_EQ(PixelAt(0, 0), white);
}

TEST_F(SoftwareCanvasTest, StrokeLineDrawsPixels) {
  canvas_->Clear(Color::White());
  canvas_->StrokeLine({0, 10}, {19, 10}, Brush::Solid(Color::Red()), 2);

  vx::u32 white = Color::White().ToRGBA32();
  EXPECT_NE(PixelAt(10, 10), white);
}

TEST_F(SoftwareCanvasTest, CreatePathReturnsValid) {
  auto path = canvas_->CreatePath();
  EXPECT_NE(path, nullptr);
}

}  // namespace
}  // namespace vx::gfx::sw
