#include "veloxa/foundation/base/types.h"

#include <type_traits>

#include <gtest/gtest.h>

namespace vx {
namespace {

TEST(TypesTest, SizeGuarantees) {
  EXPECT_EQ(sizeof(u8), 1);
  EXPECT_EQ(sizeof(u16), 2);
  EXPECT_EQ(sizeof(u32), 4);
  EXPECT_EQ(sizeof(u64), 8);
  EXPECT_EQ(sizeof(i8), 1);
  EXPECT_EQ(sizeof(i16), 2);
  EXPECT_EQ(sizeof(i32), 4);
  EXPECT_EQ(sizeof(i64), 8);
  EXPECT_EQ(sizeof(f32), 4);
  EXPECT_EQ(sizeof(f64), 8);
}

TEST(TypesTest, Signedness) {
  EXPECT_FALSE(std::is_signed_v<u8>);
  EXPECT_FALSE(std::is_signed_v<u16>);
  EXPECT_FALSE(std::is_signed_v<u32>);
  EXPECT_FALSE(std::is_signed_v<u64>);
  EXPECT_TRUE(std::is_signed_v<i8>);
  EXPECT_TRUE(std::is_signed_v<i16>);
  EXPECT_TRUE(std::is_signed_v<i32>);
  EXPECT_TRUE(std::is_signed_v<i64>);
  EXPECT_TRUE(std::is_signed_v<f32>);
  EXPECT_TRUE(std::is_signed_v<f64>);
}

TEST(TypesTest, UsizeIsSizeT) {
  EXPECT_TRUE((std::is_same_v<usize, size_t>));
}

TEST(TypesTest, IsizeIsPtrdiffT) {
  EXPECT_TRUE((std::is_same_v<isize, ptrdiff_t>));
}

}  // namespace
}  // namespace vx
