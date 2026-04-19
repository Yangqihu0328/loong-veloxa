#include "veloxa/core/css/enum_serialization.h"

#include <gtest/gtest.h>

#include "veloxa/foundation/strings/string_view.h"

namespace vx::css {

TEST(EnumSerialization, DisplayCanonical) {
  EXPECT_EQ(EnumValueToCssString(PropertyId::kDisplay, 0), StringView("none"));
  EXPECT_EQ(EnumValueToCssString(PropertyId::kDisplay, 1), StringView("block"));
  EXPECT_EQ(EnumValueToCssString(PropertyId::kDisplay, 2), StringView("inline"));
  EXPECT_EQ(EnumValueToCssString(PropertyId::kDisplay, 3),
            StringView("inline-block"));
  EXPECT_EQ(EnumValueToCssString(PropertyId::kDisplay, 4), StringView("flex"));
}

TEST(EnumSerialization, PositionCanonical) {
  EXPECT_EQ(EnumValueToCssString(PropertyId::kPosition, 0),
            StringView("static"));
  EXPECT_EQ(EnumValueToCssString(PropertyId::kPosition, 1),
            StringView("relative"));
  EXPECT_EQ(EnumValueToCssString(PropertyId::kPosition, 2),
            StringView("absolute"));
  EXPECT_EQ(EnumValueToCssString(PropertyId::kPosition, 3),
            StringView("fixed"));
}

TEST(EnumSerialization, FlexDirectionCanonical) {
  EXPECT_EQ(EnumValueToCssString(PropertyId::kFlexDirection, 0),
            StringView("row"));
  EXPECT_EQ(EnumValueToCssString(PropertyId::kFlexDirection, 1),
            StringView("row-reverse"));
  EXPECT_EQ(EnumValueToCssString(PropertyId::kFlexDirection, 2),
            StringView("column"));
  EXPECT_EQ(EnumValueToCssString(PropertyId::kFlexDirection, 3),
            StringView("column-reverse"));
}

TEST(EnumSerialization, FlexWrapCanonical) {
  EXPECT_EQ(EnumValueToCssString(PropertyId::kFlexWrap, 0),
            StringView("nowrap"));
  EXPECT_EQ(EnumValueToCssString(PropertyId::kFlexWrap, 1), StringView("wrap"));
}

TEST(EnumSerialization, JustifyContentCanonical) {
  EXPECT_EQ(EnumValueToCssString(PropertyId::kJustifyContent, 0),
            StringView("flex-start"));
  EXPECT_EQ(EnumValueToCssString(PropertyId::kJustifyContent, 1),
            StringView("flex-end"));
  EXPECT_EQ(EnumValueToCssString(PropertyId::kJustifyContent, 2),
            StringView("center"));
  EXPECT_EQ(EnumValueToCssString(PropertyId::kJustifyContent, 3),
            StringView("space-between"));
  EXPECT_EQ(EnumValueToCssString(PropertyId::kJustifyContent, 4),
            StringView("space-around"));
}

TEST(EnumSerialization, AlignItemsAndAlignSelfShareTable) {
  for (auto prop : {PropertyId::kAlignItems, PropertyId::kAlignSelf}) {
    EXPECT_EQ(EnumValueToCssString(prop, 0), StringView("auto"));
    EXPECT_EQ(EnumValueToCssString(prop, 1), StringView("flex-start"));
    EXPECT_EQ(EnumValueToCssString(prop, 2), StringView("flex-end"));
    EXPECT_EQ(EnumValueToCssString(prop, 3), StringView("center"));
    EXPECT_EQ(EnumValueToCssString(prop, 4), StringView("stretch"));
    EXPECT_EQ(EnumValueToCssString(prop, 5), StringView("baseline"));
  }
}

TEST(EnumSerialization, BoxSizingCanonical) {
  EXPECT_EQ(EnumValueToCssString(PropertyId::kBoxSizing, 0),
            StringView("content-box"));
  EXPECT_EQ(EnumValueToCssString(PropertyId::kBoxSizing, 1),
            StringView("border-box"));
}

TEST(EnumSerialization, OverflowCanonical) {
  EXPECT_EQ(EnumValueToCssString(PropertyId::kOverflow, 0),
            StringView("visible"));
  EXPECT_EQ(EnumValueToCssString(PropertyId::kOverflow, 1),
            StringView("hidden"));
  EXPECT_EQ(EnumValueToCssString(PropertyId::kOverflow, 2),
            StringView("scroll"));
  EXPECT_EQ(EnumValueToCssString(PropertyId::kOverflow, 3),
            StringView("auto"));
}

TEST(EnumSerialization, VisibilityCanonical) {
  EXPECT_EQ(EnumValueToCssString(PropertyId::kVisibility, 0),
            StringView("visible"));
  EXPECT_EQ(EnumValueToCssString(PropertyId::kVisibility, 1),
            StringView("hidden"));
}

TEST(EnumSerialization, TextAlignCanonical) {
  EXPECT_EQ(EnumValueToCssString(PropertyId::kTextAlign, 0),
            StringView("left"));
  EXPECT_EQ(EnumValueToCssString(PropertyId::kTextAlign, 1),
            StringView("right"));
  EXPECT_EQ(EnumValueToCssString(PropertyId::kTextAlign, 2),
            StringView("center"));
  EXPECT_EQ(EnumValueToCssString(PropertyId::kTextAlign, 3),
            StringView("justify"));
}

TEST(EnumSerialization, WhiteSpaceCanonical) {
  EXPECT_EQ(EnumValueToCssString(PropertyId::kWhiteSpace, 0),
            StringView("normal"));
  EXPECT_EQ(EnumValueToCssString(PropertyId::kWhiteSpace, 1),
            StringView("nowrap"));
  EXPECT_EQ(EnumValueToCssString(PropertyId::kWhiteSpace, 2),
            StringView("pre"));
  EXPECT_EQ(EnumValueToCssString(PropertyId::kWhiteSpace, 3),
            StringView("pre-wrap"));
}

TEST(EnumSerialization, BorderStyleCoversAllFourSides) {
  for (auto prop : {PropertyId::kBorderTopStyle, PropertyId::kBorderRightStyle,
                    PropertyId::kBorderBottomStyle,
                    PropertyId::kBorderLeftStyle}) {
    EXPECT_EQ(EnumValueToCssString(prop, 0), StringView("none"));
    EXPECT_EQ(EnumValueToCssString(prop, 1), StringView("solid"));
    EXPECT_EQ(EnumValueToCssString(prop, 2), StringView("dashed"));
    EXPECT_EQ(EnumValueToCssString(prop, 3), StringView("dotted"));
  }
}

TEST(EnumSerialization, FontStyleCanonical) {
  EXPECT_EQ(EnumValueToCssString(PropertyId::kFontStyle, 0),
            StringView("normal"));
  EXPECT_EQ(EnumValueToCssString(PropertyId::kFontStyle, 1),
            StringView("italic"));
}

// ----- Boundary inputs (per writing-plans.mdc 边界输入清单) -----

TEST(EnumSerialization, OutOfRangeReturnsEmpty) {
  EXPECT_EQ(EnumValueToCssString(PropertyId::kDisplay, 99), StringView());
  EXPECT_EQ(EnumValueToCssString(PropertyId::kDisplay, 5), StringView());
  EXPECT_EQ(EnumValueToCssString(PropertyId::kPosition, 4), StringView());
  EXPECT_EQ(EnumValueToCssString(PropertyId::kBorderTopStyle, 4),
            StringView());
}

TEST(EnumSerialization, NonEnumPropertyReturnsEmpty) {
  EXPECT_EQ(EnumValueToCssString(PropertyId::kBackgroundColor, 0),
            StringView());
  EXPECT_EQ(EnumValueToCssString(PropertyId::kColor, 0), StringView());
  EXPECT_EQ(EnumValueToCssString(PropertyId::kWidth, 0), StringView());
  EXPECT_EQ(EnumValueToCssString(PropertyId::kFontSize, 0), StringView());
  EXPECT_EQ(EnumValueToCssString(PropertyId::kOpacity, 0), StringView());
}

TEST(EnumSerialization, UnknownPropertyReturnsEmpty) {
  EXPECT_EQ(EnumValueToCssString(PropertyId::kUnknown, 0), StringView());
}

TEST(EnumSerialization, ReturnedStringHasStaticStorage) {
  // Calling twice with same args must return identical pointers (string
  // literals), proving callers may safely cache without copying.
  StringView a = EnumValueToCssString(PropertyId::kDisplay, 1);
  StringView b = EnumValueToCssString(PropertyId::kDisplay, 1);
  EXPECT_EQ(a.data(), b.data());
}

}  // namespace vx::css
