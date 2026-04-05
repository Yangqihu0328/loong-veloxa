# 创意设计：隐式标签关闭规则

**日期：** 2026-04-05
**状态：** 已批准
**关联任务：** TASK-20260405-03

## 设计挑战

HTML 允许省略某些闭合标签。解析器需要在遇到特定上下文时自动关闭栈顶开放元素。规则需高效查找、可维护、可扩展。

## 选定方案：B — 数据驱动规则表

### 规则结构

```cpp
struct ImplicitCloseRule {
  TagId open_tag;       // 栈顶标签
  TagId trigger_tag;    // 新标签（kUnknown = 通配，配合 use_type）
  TagType trigger_type; // 新标签类型（当 use_type == true 时使用）
  bool use_type;        // true = 按类型匹配，false = 按具体标签匹配
};
```

### 完整规则表

```cpp
static constexpr ImplicitCloseRule kImplicitCloseRules[] = {
  // <p> 遇到任何块级标签自动关闭
  {TagId::kP,      TagId::kUnknown, TagType::kBlock, true},

  // <li> 遇到 <li> 自动关闭
  {TagId::kLi,     TagId::kLi,      TagType::kInline, false},

  // <dt> 遇到 <dt> 或 <dd> 自动关闭
  {TagId::kDt,     TagId::kDt,      TagType::kInline, false},
  {TagId::kDt,     TagId::kDd,      TagType::kInline, false},

  // <dd> 遇到 <dd> 或 <dt> 自动关闭
  {TagId::kDd,     TagId::kDd,      TagType::kInline, false},
  {TagId::kDd,     TagId::kDt,      TagType::kInline, false},

  // <td> 遇到 <td>/<th>/<tr> 自动关闭
  {TagId::kTd,     TagId::kTd,      TagType::kInline, false},
  {TagId::kTd,     TagId::kTh,      TagType::kInline, false},
  {TagId::kTd,     TagId::kTr,      TagType::kInline, false},

  // <th> 遇到 <td>/<th>/<tr> 自动关闭
  {TagId::kTh,     TagId::kTd,      TagType::kInline, false},
  {TagId::kTh,     TagId::kTh,      TagType::kInline, false},
  {TagId::kTh,     TagId::kTr,      TagType::kInline, false},

  // <tr> 遇到 <tr> 自动关闭
  {TagId::kTr,     TagId::kTr,      TagType::kInline, false},

  // <thead>/<tbody>/<tfoot> 互相关闭
  {TagId::kThead,  TagId::kTbody,   TagType::kInline, false},
  {TagId::kThead,  TagId::kTfoot,   TagType::kInline, false},
  {TagId::kTbody,  TagId::kTbody,   TagType::kInline, false},
  {TagId::kTbody,  TagId::kTfoot,   TagType::kInline, false},
  {TagId::kTfoot,  TagId::kTbody,   TagType::kInline, false},

  // <head> 遇到 <body> 自动关闭
  {TagId::kHead,   TagId::kBody,    TagType::kInline, false},

  // <option> 遇到 <option> 自动关闭
  {TagId::kOption, TagId::kOption,  TagType::kInline, false},
};
```

### 查找算法

```cpp
void Parser::HandleImplicitClose(TagId new_tag) {
  TagType new_type = GetTagInfo(new_tag).type;
  while (!open_elements_.empty()) {
    TagId top = open_elements_.back()->tag_id();
    bool should_close = false;
    for (const auto& rule : kImplicitCloseRules) {
      if (rule.open_tag != top) continue;
      if (rule.use_type) {
        should_close = (new_type == rule.trigger_type);
      } else {
        should_close = (new_tag == rule.trigger_tag);
      }
      if (should_close) break;
    }
    if (!should_close) break;
    CloseTopElement();
  }
}
```

### Void 元素

通过 `TagInfo::pmodel == ParseModel::kVoid` 判断，不推入开放元素栈：
- br, hr, img, input, meta, link, area, base, col, embed, source, track, wbr

### 性能分析

- 规则表 ~21 条，每条 8 字节，总计 168 字节，常驻 L1 cache
- 每次 ProcessStartTag 最多扫描 21 条规则 × 栈深度（通常 < 10），微秒级
- 车载 HMI 页面通常 < 500 个元素，解析总耗时 < 1ms
