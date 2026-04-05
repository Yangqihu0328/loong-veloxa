#include "veloxa/core/css/property.h"

#include <cstring>

#include <gtest/gtest.h>

namespace vx::css {
namespace {

TEST(PropertyTest, PropertyIdFromName_Display) {
  EXPECT_EQ(PropertyIdFromName("display"), PropertyId::kDisplay);
}

TEST(PropertyTest, PropertyIdFromName_MarginTop) {
  EXPECT_EQ(PropertyIdFromName("margin-top"), PropertyId::kMarginTop);
}

TEST(PropertyTest, PropertyIdFromName_BackgroundColor) {
  EXPECT_EQ(PropertyIdFromName("background-color"),
            PropertyId::kBackgroundColor);
}

TEST(PropertyTest, PropertyIdFromName_FontSize) {
  EXPECT_EQ(PropertyIdFromName("font-size"), PropertyId::kFontSize);
}

TEST(PropertyTest, PropertyIdFromName_FlexDirection) {
  EXPECT_EQ(PropertyIdFromName("flex-direction"), PropertyId::kFlexDirection);
}

TEST(PropertyTest, PropertyIdFromName_Unknown) {
  EXPECT_EQ(PropertyIdFromName("unknown-prop"), PropertyId::kUnknown);
}

TEST(PropertyTest, PropertyIdFromName_Empty) {
  EXPECT_EQ(PropertyIdFromName(""), PropertyId::kUnknown);
}

TEST(PropertyTest, GetPropertyInfo_Display) {
  const auto& info = GetPropertyInfo(PropertyId::kDisplay);
  EXPECT_STREQ(info.name, "display");
  EXPECT_FALSE(info.inherited);
}

TEST(PropertyTest, GetPropertyInfo_Color) {
  const auto& info = GetPropertyInfo(PropertyId::kColor);
  EXPECT_STREQ(info.name, "color");
  EXPECT_TRUE(info.inherited);
}

TEST(PropertyTest, IsInherited_Color) {
  EXPECT_TRUE(IsInheritedProperty(PropertyId::kColor));
}

TEST(PropertyTest, IsInherited_FontFamily) {
  EXPECT_TRUE(IsInheritedProperty(PropertyId::kFontFamily));
}

TEST(PropertyTest, IsNotInherited_Display) {
  EXPECT_FALSE(IsInheritedProperty(PropertyId::kDisplay));
}

TEST(PropertyTest, IsNotInherited_Margin) {
  EXPECT_FALSE(IsInheritedProperty(PropertyId::kMarginTop));
}

TEST(PropertyTest, AllProperties_HaveNames) {
  for (u8 i = static_cast<u8>(PropertyId::kUnknown) + 1;
       i < static_cast<u8>(PropertyId::kMaxProperty); ++i) {
    auto id = static_cast<PropertyId>(i);
    const auto& info = GetPropertyInfo(id);
    EXPECT_NE(info.name, nullptr) << "PropertyId " << static_cast<int>(i);
    EXPECT_GT(std::strlen(info.name), 0u)
        << "PropertyId " << static_cast<int>(i);
  }
}

}  // namespace
}  // namespace vx::css
