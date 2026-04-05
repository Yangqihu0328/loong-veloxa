#include "veloxa/core/dom/tag.h"

#include <gtest/gtest.h>

namespace vx::dom {
namespace {

TEST(TagTest, KnownTagLookup) {
  EXPECT_EQ(TagIdFromName("div"), TagId::kDiv);
  EXPECT_EQ(TagIdFromName("span"), TagId::kSpan);
  EXPECT_EQ(TagIdFromName("p"), TagId::kP);
  EXPECT_EQ(TagIdFromName("html"), TagId::kHtml);
  EXPECT_EQ(TagIdFromName("body"), TagId::kBody);
  EXPECT_EQ(TagIdFromName("head"), TagId::kHead);
}

TEST(TagTest, CaseInsensitiveLookup) {
  EXPECT_EQ(TagIdFromName("DIV"), TagId::kDiv);
  EXPECT_EQ(TagIdFromName("Div"), TagId::kDiv);
  EXPECT_EQ(TagIdFromName("SPAN"), TagId::kSpan);
  EXPECT_EQ(TagIdFromName("TABLE"), TagId::kTable);
}

TEST(TagTest, UnknownTagReturnsUnknown) {
  EXPECT_EQ(TagIdFromName("my-component"), TagId::kUnknown);
  EXPECT_EQ(TagIdFromName("foobar"), TagId::kUnknown);
  EXPECT_EQ(TagIdFromName(""), TagId::kUnknown);
}

TEST(TagTest, GetTagInfoReturnsCorrectData) {
  const auto& info = GetTagInfo(TagId::kDiv);
  EXPECT_EQ(info.id, TagId::kDiv);
  EXPECT_STREQ(info.name, "div");
  EXPECT_EQ(info.type, TagType::kBlock);
  EXPECT_EQ(info.pmodel, ParseModel::kNormal);
  EXPECT_EQ(info.cmodel, ContentModel::kFlow);
}

TEST(TagTest, GetTagInfoUnknownId) {
  const auto& info = GetTagInfo(TagId::kUnknown);
  EXPECT_EQ(info.id, TagId::kUnknown);
  EXPECT_STREQ(info.name, "");
}

TEST(TagTest, GetTagInfoOutOfRange) {
  const auto& info = GetTagInfo(static_cast<TagId>(9999));
  EXPECT_EQ(info.id, TagId::kUnknown);
}

TEST(TagTest, VoidElements) {
  EXPECT_TRUE(IsVoidElement(TagId::kBr));
  EXPECT_TRUE(IsVoidElement(TagId::kHr));
  EXPECT_TRUE(IsVoidElement(TagId::kImg));
  EXPECT_TRUE(IsVoidElement(TagId::kInput));
  EXPECT_TRUE(IsVoidElement(TagId::kMeta));
  EXPECT_TRUE(IsVoidElement(TagId::kLink));
  EXPECT_TRUE(IsVoidElement(TagId::kArea));
  EXPECT_TRUE(IsVoidElement(TagId::kBase));
  EXPECT_TRUE(IsVoidElement(TagId::kCol));
  EXPECT_TRUE(IsVoidElement(TagId::kEmbed));
  EXPECT_TRUE(IsVoidElement(TagId::kSource));
  EXPECT_TRUE(IsVoidElement(TagId::kTrack));
  EXPECT_TRUE(IsVoidElement(TagId::kWbr));

  EXPECT_FALSE(IsVoidElement(TagId::kDiv));
  EXPECT_FALSE(IsVoidElement(TagId::kP));
  EXPECT_FALSE(IsVoidElement(TagId::kSpan));
}

TEST(TagTest, RawTextElements) {
  EXPECT_TRUE(IsRawTextElement(TagId::kScript));
  EXPECT_TRUE(IsRawTextElement(TagId::kStyle));
  EXPECT_FALSE(IsRawTextElement(TagId::kDiv));
  EXPECT_FALSE(IsRawTextElement(TagId::kTextarea));
}

TEST(TagTest, BlockTagDetection) {
  EXPECT_TRUE(IsBlockTag(TagId::kDiv));
  EXPECT_TRUE(IsBlockTag(TagId::kP));
  EXPECT_TRUE(IsBlockTag(TagId::kH1));
  EXPECT_TRUE(IsBlockTag(TagId::kUl));
  EXPECT_TRUE(IsBlockTag(TagId::kTable));
  EXPECT_TRUE(IsBlockTag(TagId::kTr));
  EXPECT_TRUE(IsBlockTag(TagId::kTd));

  EXPECT_FALSE(IsBlockTag(TagId::kSpan));
  EXPECT_FALSE(IsBlockTag(TagId::kA));
  EXPECT_FALSE(IsBlockTag(TagId::kEm));
  EXPECT_FALSE(IsBlockTag(TagId::kStrong));
}

TEST(TagTest, TableElements) {
  EXPECT_EQ(GetTagInfo(TagId::kTable).type, TagType::kTableTag);
  EXPECT_EQ(GetTagInfo(TagId::kThead).type, TagType::kTableBody);
  EXPECT_EQ(GetTagInfo(TagId::kTbody).type, TagType::kTableBody);
  EXPECT_EQ(GetTagInfo(TagId::kTfoot).type, TagType::kTableBody);
  EXPECT_EQ(GetTagInfo(TagId::kTr).type, TagType::kTableRow);
  EXPECT_EQ(GetTagInfo(TagId::kTd).type, TagType::kTableCell);
  EXPECT_EQ(GetTagInfo(TagId::kTh).type, TagType::kTableCell);
}

TEST(TagTest, AllBuiltinTagsHaveNames) {
  for (u16 i = 1; i < static_cast<u16>(TagId::kMaxBuiltin); ++i) {
    const auto& info = GetTagInfo(static_cast<TagId>(i));
    EXPECT_NE(info.name[0], '\0') << "TagId " << i << " has empty name";
    EXPECT_EQ(info.id, static_cast<TagId>(i))
        << "TagId " << i << " has mismatched id";
  }
}

TEST(TagTest, RoundTripAllBuiltinTags) {
  for (u16 i = 1; i < static_cast<u16>(TagId::kMaxBuiltin); ++i) {
    const auto& info = GetTagInfo(static_cast<TagId>(i));
    TagId resolved = TagIdFromName(info.name);
    EXPECT_EQ(resolved, static_cast<TagId>(i))
        << "Round-trip failed for " << info.name;
  }
}

}  // namespace
}  // namespace vx::dom
