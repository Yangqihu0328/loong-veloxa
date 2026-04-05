#include "veloxa/platform/headless/memory_surface.h"

#include <cstdio>
#include <cstring>

#include "gtest/gtest.h"

namespace vx::platform {
namespace {

TEST(MemorySurfaceTest, ConstructionSetsCorrectDimensions) {
  MemorySurface surface(100, 200);
  EXPECT_EQ(surface.width(), 100u);
  EXPECT_EQ(surface.height(), 200u);
  EXPECT_EQ(surface.stride(), 100u * 4);
}

TEST(MemorySurfaceTest, LockReturnsValidPointer) {
  MemorySurface surface(10, 10);
  vx::u32* pixels = surface.Lock();
  ASSERT_NE(pixels, nullptr);
  pixels[0] = 0xFF0000FF;
  pixels[99] = 0x00FF00FF;
  EXPECT_EQ(pixels[0], 0xFF0000FFu);
  EXPECT_EQ(pixels[99], 0x00FF00FFu);
  surface.Unlock();
}

TEST(MemorySurfaceTest, PixelsAreZeroInitialized) {
  MemorySurface surface(4, 4);
  vx::u32* pixels = surface.Lock();
  for (vx::u32 i = 0; i < 16; ++i) {
    EXPECT_EQ(pixels[i], 0u);
  }
  surface.Unlock();
}

TEST(MemorySurfaceTest, ResizeChangeDimensions) {
  MemorySurface surface(10, 10);
  surface.Resize(20, 30);
  EXPECT_EQ(surface.width(), 20u);
  EXPECT_EQ(surface.height(), 30u);
  EXPECT_EQ(surface.stride(), 20u * 4);
}

TEST(MemorySurfaceTest, ResizeZerosNewBuffer) {
  MemorySurface surface(4, 4);
  {
    vx::u32* pixels = surface.Lock();
    for (vx::u32 i = 0; i < 16; ++i) {
      pixels[i] = 0xDEADBEEF;
    }
    surface.Unlock();
  }

  surface.Resize(4, 4);
  vx::u32* pixels = surface.Lock();
  for (vx::u32 i = 0; i < 16; ++i) {
    EXPECT_EQ(pixels[i], 0u);
  }
  surface.Unlock();
}

TEST(MemorySurfaceTest, SavePPMWritesValidFile) {
  const char* path = "/tmp/veloxa_test_surface.ppm";
  MemorySurface surface(2, 2);
  {
    vx::u32* pixels = surface.Lock();
    pixels[0] = 0xFF0000FF;  // R=255, G=0,   B=0,   A=255
    pixels[1] = 0xFF00FF00;  // R=0,   G=255, B=0,   A=255
    pixels[2] = 0xFFFF0000;  // R=0,   G=0,   B=255, A=255
    pixels[3] = 0xFFFFFFFF;  // R=255, G=255, B=255, A=255
    surface.Unlock();
  }

  vx::Status status = surface.SavePPM(path);
  ASSERT_TRUE(status.ok()) << status.message();

  std::FILE* f = std::fopen(path, "rb");
  ASSERT_NE(f, nullptr);

  char header[64];
  std::fgets(header, sizeof(header), f);
  EXPECT_STREQ(header, "P6\n");

  std::fgets(header, sizeof(header), f);
  EXPECT_STREQ(header, "2 2\n");

  std::fgets(header, sizeof(header), f);
  EXPECT_STREQ(header, "255\n");

  vx::u8 rgb[12];
  size_t read = std::fread(rgb, 1, 12, f);
  EXPECT_EQ(read, 12u);

  EXPECT_EQ(rgb[0], 0xFF);  // pixel 0 R
  EXPECT_EQ(rgb[1], 0x00);  // pixel 0 G
  EXPECT_EQ(rgb[2], 0x00);  // pixel 0 B

  EXPECT_EQ(rgb[3], 0x00);  // pixel 1 R
  EXPECT_EQ(rgb[4], 0xFF);  // pixel 1 G
  EXPECT_EQ(rgb[5], 0x00);  // pixel 1 B

  EXPECT_EQ(rgb[6], 0x00);  // pixel 2 R
  EXPECT_EQ(rgb[7], 0x00);  // pixel 2 G
  EXPECT_EQ(rgb[8], 0xFF);  // pixel 2 B

  std::fclose(f);
  std::remove(path);
}

TEST(MemorySurfaceTest, ZeroSizeSurfaceDoesNotCrash) {
  MemorySurface surface(0, 0);
  EXPECT_EQ(surface.width(), 0u);
  EXPECT_EQ(surface.height(), 0u);
  EXPECT_EQ(surface.stride(), 0u);

  vx::u32* pixels = surface.Lock();
  EXPECT_EQ(pixels, nullptr);
  surface.Unlock();
}

TEST(MemorySurfaceTest, ZeroSizeSavePPMSucceeds) {
  const char* path = "/tmp/veloxa_test_surface_zero.ppm";
  MemorySurface surface(0, 0);
  vx::Status status = surface.SavePPM(path);
  EXPECT_TRUE(status.ok());
  std::remove(path);
}

TEST(MemorySurfaceTest, SavePPMFailsOnInvalidPath) {
  MemorySurface surface(2, 2);
  vx::Status status = surface.SavePPM("/nonexistent_dir/test.ppm");
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), vx::StatusCode::kInternal);
}

}  // namespace
}  // namespace vx::platform
