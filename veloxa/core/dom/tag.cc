#include "veloxa/core/dom/tag.h"

#include <cstring>

#include "veloxa/foundation/containers/hash_map.h"

namespace vx::dom {

namespace {

// clang-format off
constexpr TagInfo kTagTable[] = {
  {TagId::kUnknown,    "",           TagType::kInline,     ParseModel::kNormal,  ContentModel::kPhrasing},
  // Structure
  {TagId::kHtml,       "html",       TagType::kBlock,      ParseModel::kNormal,  ContentModel::kFlow},
  {TagId::kHead,       "head",       TagType::kInfo,       ParseModel::kNormal,  ContentModel::kFlow},
  {TagId::kBody,       "body",       TagType::kBlock,      ParseModel::kNormal,  ContentModel::kFlow},
  {TagId::kDiv,        "div",        TagType::kBlock,      ParseModel::kNormal,  ContentModel::kFlow},
  {TagId::kSpan,       "span",       TagType::kInline,     ParseModel::kNormal,  ContentModel::kPhrasing},
  {TagId::kSection,    "section",    TagType::kBlock,      ParseModel::kNormal,  ContentModel::kFlow},
  {TagId::kHeader,     "header",     TagType::kBlock,      ParseModel::kNormal,  ContentModel::kFlow},
  {TagId::kFooter,     "footer",     TagType::kBlock,      ParseModel::kNormal,  ContentModel::kFlow},
  {TagId::kNav,        "nav",        TagType::kBlock,      ParseModel::kNormal,  ContentModel::kFlow},
  {TagId::kMain,       "main",       TagType::kBlock,      ParseModel::kNormal,  ContentModel::kFlow},
  {TagId::kArticle,    "article",    TagType::kBlock,      ParseModel::kNormal,  ContentModel::kFlow},
  {TagId::kAside,      "aside",      TagType::kBlock,      ParseModel::kNormal,  ContentModel::kFlow},
  // Text
  {TagId::kP,          "p",          TagType::kBlock,      ParseModel::kNormal,  ContentModel::kPhrasing},
  {TagId::kH1,         "h1",         TagType::kBlock,      ParseModel::kNormal,  ContentModel::kPhrasing},
  {TagId::kH2,         "h2",         TagType::kBlock,      ParseModel::kNormal,  ContentModel::kPhrasing},
  {TagId::kH3,         "h3",         TagType::kBlock,      ParseModel::kNormal,  ContentModel::kPhrasing},
  {TagId::kH4,         "h4",         TagType::kBlock,      ParseModel::kNormal,  ContentModel::kPhrasing},
  {TagId::kH5,         "h5",         TagType::kBlock,      ParseModel::kNormal,  ContentModel::kPhrasing},
  {TagId::kH6,         "h6",         TagType::kBlock,      ParseModel::kNormal,  ContentModel::kPhrasing},
  {TagId::kBr,         "br",         TagType::kInline,     ParseModel::kVoid,    ContentModel::kEmpty},
  {TagId::kHr,         "hr",         TagType::kBlock,      ParseModel::kVoid,    ContentModel::kEmpty},
  {TagId::kA,          "a",          TagType::kInline,     ParseModel::kNormal,  ContentModel::kTransparent},
  {TagId::kEm,         "em",         TagType::kInline,     ParseModel::kNormal,  ContentModel::kPhrasing},
  {TagId::kStrong,     "strong",     TagType::kInline,     ParseModel::kNormal,  ContentModel::kPhrasing},
  {TagId::kB,          "b",          TagType::kInline,     ParseModel::kNormal,  ContentModel::kPhrasing},
  {TagId::kI,          "i",          TagType::kInline,     ParseModel::kNormal,  ContentModel::kPhrasing},
  {TagId::kU,          "u",          TagType::kInline,     ParseModel::kNormal,  ContentModel::kPhrasing},
  {TagId::kS,          "s",          TagType::kInline,     ParseModel::kNormal,  ContentModel::kPhrasing},
  {TagId::kCode,       "code",       TagType::kInline,     ParseModel::kNormal,  ContentModel::kPhrasing},
  {TagId::kPre,        "pre",        TagType::kBlock,      ParseModel::kNormal,  ContentModel::kPhrasing},
  {TagId::kBlockquote, "blockquote", TagType::kBlock,      ParseModel::kNormal,  ContentModel::kFlow},
  // Lists
  {TagId::kUl,         "ul",         TagType::kBlock,      ParseModel::kNormal,  ContentModel::kFlow},
  {TagId::kOl,         "ol",         TagType::kBlock,      ParseModel::kNormal,  ContentModel::kFlow},
  {TagId::kLi,         "li",         TagType::kBlock,      ParseModel::kNormal,  ContentModel::kFlow},
  {TagId::kDl,         "dl",         TagType::kBlock,      ParseModel::kNormal,  ContentModel::kFlow},
  {TagId::kDt,         "dt",         TagType::kBlock,      ParseModel::kNormal,  ContentModel::kPhrasing},
  {TagId::kDd,         "dd",         TagType::kBlock,      ParseModel::kNormal,  ContentModel::kFlow},
  // Table
  {TagId::kTable,      "table",      TagType::kTableTag,   ParseModel::kNormal,  ContentModel::kTable},
  {TagId::kThead,      "thead",      TagType::kTableBody,  ParseModel::kNormal,  ContentModel::kTable},
  {TagId::kTbody,      "tbody",      TagType::kTableBody,  ParseModel::kNormal,  ContentModel::kTable},
  {TagId::kTfoot,      "tfoot",      TagType::kTableBody,  ParseModel::kNormal,  ContentModel::kTable},
  {TagId::kTr,         "tr",         TagType::kTableRow,   ParseModel::kNormal,  ContentModel::kTable},
  {TagId::kTd,         "td",         TagType::kTableCell,  ParseModel::kNormal,  ContentModel::kFlow},
  {TagId::kTh,         "th",         TagType::kTableCell,  ParseModel::kNormal,  ContentModel::kFlow},
  {TagId::kCaption,    "caption",    TagType::kBlock,      ParseModel::kNormal,  ContentModel::kFlow},
  {TagId::kCol,        "col",        TagType::kInfo,       ParseModel::kVoid,    ContentModel::kEmpty},
  {TagId::kColgroup,   "colgroup",   TagType::kInfo,       ParseModel::kNormal,  ContentModel::kTable},
  // Form
  {TagId::kForm,       "form",       TagType::kBlock,      ParseModel::kNormal,  ContentModel::kFlow},
  {TagId::kInput,      "input",      TagType::kInlineBlock,ParseModel::kVoid,    ContentModel::kEmpty},
  {TagId::kButton,     "button",     TagType::kInlineBlock,ParseModel::kNormal,  ContentModel::kPhrasing},
  {TagId::kSelect,     "select",     TagType::kInlineBlock,ParseModel::kNormal,  ContentModel::kFlow},
  {TagId::kOption,     "option",     TagType::kInline,     ParseModel::kNormal,  ContentModel::kText},
  {TagId::kTextarea,   "textarea",   TagType::kInlineBlock,ParseModel::kNormal,  ContentModel::kText},
  {TagId::kLabel,      "label",      TagType::kInline,     ParseModel::kNormal,  ContentModel::kPhrasing},
  // Media
  {TagId::kImg,        "img",        TagType::kInlineBlock,ParseModel::kVoid,    ContentModel::kEmpty},
  {TagId::kVideo,      "video",      TagType::kInlineBlock,ParseModel::kNormal,  ContentModel::kTransparent},
  {TagId::kCanvas,     "canvas",     TagType::kInlineBlock,ParseModel::kNormal,  ContentModel::kTransparent},
  {TagId::kSource,     "source",     TagType::kInfo,       ParseModel::kVoid,    ContentModel::kEmpty},
  // Metadata
  {TagId::kTitle,      "title",      TagType::kInfo,       ParseModel::kNormal,  ContentModel::kText},
  {TagId::kMeta,       "meta",       TagType::kInfo,       ParseModel::kVoid,    ContentModel::kEmpty},
  {TagId::kLink,       "link",       TagType::kInfo,       ParseModel::kVoid,    ContentModel::kEmpty},
  {TagId::kStyle,      "style",      TagType::kInfo,       ParseModel::kRawText, ContentModel::kText},
  {TagId::kScript,     "script",     TagType::kInfo,       ParseModel::kRawText, ContentModel::kText},
  {TagId::kBase,       "base",       TagType::kInfo,       ParseModel::kVoid,    ContentModel::kEmpty},
  // Other
  {TagId::kTemplate,   "template",   TagType::kBlock,      ParseModel::kNormal,  ContentModel::kFlow},
  {TagId::kSlot,       "slot",       TagType::kInline,     ParseModel::kNormal,  ContentModel::kTransparent},
  {TagId::kArea,       "area",       TagType::kInline,     ParseModel::kVoid,    ContentModel::kEmpty},
  {TagId::kEmbed,      "embed",      TagType::kInlineBlock,ParseModel::kVoid,    ContentModel::kEmpty},
  {TagId::kTrack,      "track",      TagType::kInfo,       ParseModel::kVoid,    ContentModel::kEmpty},
  {TagId::kWbr,        "wbr",        TagType::kInline,     ParseModel::kVoid,    ContentModel::kEmpty},
};
// clang-format on

constexpr usize kTagTableSize = sizeof(kTagTable) / sizeof(kTagTable[0]);

static_assert(kTagTableSize == static_cast<usize>(TagId::kMaxBuiltin),
              "Tag table size must match TagId::kMaxBuiltin");

struct StringViewHash {
  usize operator()(StringView sv) const {
    usize hash = 5381;
    for (usize i = 0; i < sv.size(); ++i) {
      hash = ((hash << 5) + hash) + static_cast<u8>(sv[i]);
    }
    return hash;
  }
};

struct StringViewEq {
  bool operator()(StringView a, StringView b) const { return a == b; }
};

using TagMap = HashMap<StringView, TagId, StringViewHash, StringViewEq>;

TagMap* BuildTagMap() {
  auto* m = new TagMap();
  m->reserve(kTagTableSize);
  for (usize i = 1; i < kTagTableSize; ++i) {
    m->Insert(StringView(kTagTable[i].name), kTagTable[i].id);
  }
  return m;
}

}  // namespace

const TagInfo& GetTagInfo(TagId id) {
  auto idx = static_cast<u16>(id);
  if (idx >= kTagTableSize) return kTagTable[0];
  return kTagTable[idx];
}

TagId TagIdFromName(StringView name) {
  static TagMap* map = BuildTagMap();

  char buf[64];
  usize len = name.size();
  if (len == 0 || len >= sizeof(buf)) return TagId::kUnknown;
  for (usize i = 0; i < len; ++i) {
    char c = name[i];
    buf[i] = (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c;
  }

  TagId* result = map->Find(StringView(buf, len));
  return result ? *result : TagId::kUnknown;
}

bool IsVoidElement(TagId id) {
  return GetTagInfo(id).pmodel == ParseModel::kVoid;
}

bool IsRawTextElement(TagId id) {
  return GetTagInfo(id).pmodel == ParseModel::kRawText;
}

bool IsBlockTag(TagId id) {
  auto type = GetTagInfo(id).type;
  return type == TagType::kBlock || type == TagType::kTableTag ||
         type == TagType::kTableBody || type == TagType::kTableRow ||
         type == TagType::kTableCell;
}

}  // namespace vx::dom
