#include <gtest/gtest.h>

#include <cstdio>

#include <png.h>

#include "veloxa/core/image/image_cache.h"

namespace vx::image {

static std::string CreateTestPng() {
  char path[256];
  std::snprintf(path, sizeof(path), "/tmp/vx_cache_test_%d.png", getpid());

  FILE* fp = std::fopen(path, "wb");
  png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr,
                                            nullptr, nullptr);
  png_infop info = png_create_info_struct(png);
  png_init_io(png, fp);
  png_set_IHDR(png, info, 4, 4, 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
               PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
  png_write_info(png, info);

  u8 row[16];
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      row[j * 4 + 0] = 255;
      row[j * 4 + 1] = 0;
      row[j * 4 + 2] = 0;
      row[j * 4 + 3] = 255;
    }
    png_bytep rp = row;
    png_write_rows(png, &rp, 1);
  }

  png_write_end(png, info);
  png_destroy_write_struct(&png, &info);
  std::fclose(fp);
  return path;
}

TEST(ImageCacheTest, LoadAndGet) {
  auto path = CreateTestPng();
  ImageCache cache;
  auto result = cache.Load(StringView(path.c_str()));
  ASSERT_TRUE(result.ok());
  EXPECT_NE(result.value(), gfx::kInvalidImage);
  auto* img = cache.Get(result.value());
  ASSERT_NE(img, nullptr);
  EXPECT_EQ(img->width(), 4u);
  EXPECT_EQ(cache.size(), 1u);
  std::remove(path.c_str());
}

TEST(ImageCacheTest, DeduplicateSamePath) {
  auto path = CreateTestPng();
  ImageCache cache;
  auto r1 = cache.Load(StringView(path.c_str()));
  auto r2 = cache.Load(StringView(path.c_str()));
  ASSERT_TRUE(r1.ok());
  ASSERT_TRUE(r2.ok());
  EXPECT_EQ(r1.value(), r2.value());
  EXPECT_EQ(cache.size(), 1u);
  std::remove(path.c_str());
}

TEST(ImageCacheTest, LoadInvalidPath) {
  ImageCache cache;
  auto result = cache.Load("/nonexistent.png");
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(cache.size(), 0u);
}

}  // namespace vx::image
