#include <cstdio>
#include <cstring>

#include <gtest/gtest.h>

#include "veloxa/graphics/software/software_canvas.h"
#include "veloxa/platform/headless/headless_event_loop.h"
#include "veloxa/platform/headless/memory_surface.h"

namespace {

using vx::gfx::Brush;
using vx::gfx::Color;
using vx::gfx::Matrix3x2;
using vx::gfx::Rect;
using vx::gfx::sw::SoftwareCanvas;
using vx::platform::HeadlessEventLoop;
using vx::platform::MemorySurface;

vx::u32 PixelAt(const vx::u32* pixels, vx::u32 stride, vx::u32 x,
                vx::u32 y) {
  return pixels[y * (stride / 4) + x];
}

void ExpectChannelsNear(vx::u32 pixel, vx::u8 er, vx::u8 eg, vx::u8 eb,
                        vx::u8 ea, int tolerance = 3) {
  int r = static_cast<int>(pixel & 0xFF);
  int g = static_cast<int>((pixel >> 8) & 0xFF);
  int b = static_cast<int>((pixel >> 16) & 0xFF);
  int a = static_cast<int>((pixel >> 24) & 0xFF);
  EXPECT_NEAR(r, er, tolerance);
  EXPECT_NEAR(g, eg, tolerance);
  EXPECT_NEAR(b, eb, tolerance);
  EXPECT_NEAR(a, ea, tolerance);
}

TEST(GfxIntegration, SurfaceAndCanvasClearAndFillRect) {
  MemorySurface surface(200, 200);

  vx::u32* pixels = surface.Lock();
  SoftwareCanvas canvas(pixels, surface.width(), surface.height(),
                        surface.stride());
  canvas.Begin();
  canvas.Clear(Color::White());
  canvas.FillRect({10, 10, 50, 50}, Brush::Solid(Color::Red()));
  canvas.End();
  surface.Unlock();

  pixels = surface.Lock();
  vx::u32 stride = surface.stride();

  EXPECT_EQ(PixelAt(pixels, stride, 0, 0), Color::White().ToRGBA32());
  EXPECT_EQ(PixelAt(pixels, stride, 25, 25), Color::Red().ToRGBA32());

  surface.Unlock();
}

TEST(GfxIntegration, MultiLayerBlending) {
  MemorySurface surface(100, 100);

  vx::u32* pixels = surface.Lock();
  SoftwareCanvas canvas(pixels, surface.width(), surface.height(),
                        surface.stride());
  canvas.Begin();
  canvas.Clear(Color::White());
  canvas.FillRect({10, 10, 30, 30}, Brush::Solid(Color::Blue()));
  canvas.FillRect({20, 20, 30, 30},
                  Brush::Solid(Color::FromRGBA(255, 0, 0, 128)));
  canvas.End();
  surface.Unlock();

  pixels = surface.Lock();
  vx::u32 stride = surface.stride();

  EXPECT_EQ(PixelAt(pixels, stride, 15, 15), Color::Blue().ToRGBA32());

  // (45,45): half-transparent red over white → ~(255, 127, 127, 255)
  ExpectChannelsNear(PixelAt(pixels, stride, 45, 45), 255, 127, 127, 255);

  // (25,25): half-transparent red over blue → ~(128, 0, 127, 255)
  ExpectChannelsNear(PixelAt(pixels, stride, 25, 25), 128, 0, 127, 255);

  surface.Unlock();
}

TEST(GfxIntegration, TransformAndClip) {
  MemorySurface surface(100, 100);

  vx::u32* pixels = surface.Lock();
  SoftwareCanvas canvas(pixels, surface.width(), surface.height(),
                        surface.stride());
  canvas.Begin();
  canvas.Clear(Color::Black());
  canvas.SetTransform(Matrix3x2::Translation(50, 50));
  canvas.FillRect({0, 0, 20, 20}, Brush::Solid(Color::Green()));
  canvas.End();
  surface.Unlock();

  pixels = surface.Lock();
  vx::u32 stride = surface.stride();

  EXPECT_EQ(PixelAt(pixels, stride, 60, 60), Color::Green().ToRGBA32());
  EXPECT_EQ(PixelAt(pixels, stride, 10, 10), Color::Black().ToRGBA32());

  surface.Unlock();
}

TEST(GfxIntegration, PathTriangleFill) {
  MemorySurface surface(100, 100);

  vx::u32* pixels = surface.Lock();
  SoftwareCanvas canvas(pixels, surface.width(), surface.height(),
                        surface.stride());
  canvas.Begin();
  canvas.Clear(Color::White());
  auto path = canvas.CreatePath();
  path->MoveTo({50, 10});
  path->LineTo({10, 90});
  path->LineTo({90, 90});
  path->Close();
  canvas.FillPath(*path, Brush::Solid(Color::Red()));
  canvas.End();
  surface.Unlock();

  pixels = surface.Lock();
  vx::u32 stride = surface.stride();

  vx::u32 center = PixelAt(pixels, stride, 50, 60);
  EXPECT_EQ(center & 0xFF, 255);  // red channel
  EXPECT_EQ(PixelAt(pixels, stride, 5, 5), Color::White().ToRGBA32());

  surface.Unlock();
}

TEST(GfxIntegration, EventLoopDrivenRender) {
  const char* ppm_path = "/tmp/veloxa_integration_test.ppm";

  HeadlessEventLoop loop;
  bool task_executed = false;

  loop.PostTask([&]() {
    MemorySurface surface(50, 50);
    vx::u32* pixels = surface.Lock();
    SoftwareCanvas canvas(pixels, surface.width(), surface.height(),
                          surface.stride());
    canvas.Begin();
    canvas.Clear(Color::White());
    canvas.FillRect({5, 5, 40, 40}, Brush::Solid(Color::Blue()));
    canvas.End();
    surface.Unlock();
    EXPECT_TRUE(surface.SavePPM(ppm_path).ok());
    task_executed = true;
  });

  loop.PollOnce();
  EXPECT_TRUE(task_executed);

  FILE* f = std::fopen(ppm_path, "rb");
  ASSERT_NE(f, nullptr);

  char header[32];
  ASSERT_NE(std::fgets(header, sizeof(header), f), nullptr);
  EXPECT_STREQ(header, "P6\n");
  std::fclose(f);

  std::remove(ppm_path);
}

TEST(GfxIntegration, SavePPMVerifyFormat) {
  const char* ppm_path = "/tmp/veloxa_ppm_test.ppm";

  MemorySurface surface(10, 10);
  vx::u32* pixels = surface.Lock();
  SoftwareCanvas canvas(pixels, surface.width(), surface.height(),
                        surface.stride());
  canvas.Begin();
  canvas.Clear(Color::Red());
  canvas.End();
  surface.Unlock();

  ASSERT_TRUE(surface.SavePPM(ppm_path).ok());

  FILE* f = std::fopen(ppm_path, "rb");
  ASSERT_NE(f, nullptr);

  char magic[4] = {};
  ASSERT_EQ(std::fread(magic, 1, 3, f), 3u);
  EXPECT_STREQ(magic, "P6\n");

  char dims[32] = {};
  ASSERT_NE(std::fgets(dims, sizeof(dims), f), nullptr);
  EXPECT_STREQ(dims, "10 10\n");

  char maxval[8] = {};
  ASSERT_NE(std::fgets(maxval, sizeof(maxval), f), nullptr);
  EXPECT_STREQ(maxval, "255\n");

  vx::u8 rgb[3] = {};
  ASSERT_EQ(std::fread(rgb, 1, 3, f), 3u);
  EXPECT_EQ(rgb[0], 255);
  EXPECT_EQ(rgb[1], 0);
  EXPECT_EQ(rgb[2], 0);

  std::fclose(f);
  std::remove(ppm_path);
}

}  // namespace
