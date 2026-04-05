#ifndef VELOXA_CORE_DOM_TAG_H_
#define VELOXA_CORE_DOM_TAG_H_

#include "veloxa/foundation/base/types.h"
#include "veloxa/foundation/strings/string_view.h"

namespace vx::dom {

enum class TagId : u16 {
  kUnknown = 0,
  // Structure
  kHtml, kHead, kBody, kDiv, kSpan, kSection, kHeader, kFooter, kNav, kMain,
  kArticle, kAside,
  // Text
  kP, kH1, kH2, kH3, kH4, kH5, kH6, kBr, kHr, kA, kEm, kStrong, kB, kI,
  kU, kS, kCode, kPre, kBlockquote,
  // Lists
  kUl, kOl, kLi, kDl, kDt, kDd,
  // Table
  kTable, kThead, kTbody, kTfoot, kTr, kTd, kTh, kCaption, kCol, kColgroup,
  // Form
  kForm, kInput, kButton, kSelect, kOption, kTextarea, kLabel,
  // Media
  kImg, kVideo, kCanvas, kSource,
  // Metadata
  kTitle, kMeta, kLink, kStyle, kScript, kBase,
  // Other
  kTemplate, kSlot, kArea, kEmbed, kTrack, kWbr,
  kMaxBuiltin,
};

enum class TagType : u8 {
  kBlock,
  kInline,
  kInlineBlock,
  kTableTag,
  kTableBody,
  kTableRow,
  kTableCell,
  kInfo,
};

enum class ParseModel : u8 {
  kNormal,
  kVoid,
  kRawText,
};

enum class ContentModel : u8 {
  kFlow,
  kPhrasing,
  kTransparent,
  kText,
  kTable,
  kEmpty,
};

struct TagInfo {
  TagId id;
  const char* name;
  TagType type;
  ParseModel pmodel;
  ContentModel cmodel;
};

const TagInfo& GetTagInfo(TagId id);

TagId TagIdFromName(StringView name);

bool IsVoidElement(TagId id);

bool IsRawTextElement(TagId id);

bool IsBlockTag(TagId id);

}  // namespace vx::dom

#endif  // VELOXA_CORE_DOM_TAG_H_
