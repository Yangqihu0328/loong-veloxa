# 创意设计：颜色解析策略

**日期：** 2026-04-05
**状态：** 已批准
**关联任务：** TASK-20260405-04

## 设计挑战

CSS 颜色值有多种格式：`#hex`、`rgb()`/`rgba()`、命名颜色。核心问题是命名颜色表的存储和查找方式。

约束：车载 HMI 常用命名颜色约 18 个，需 constexpr 零运行时初始化。

## 探索的方案

### 方案 A：排序数组 + 二分查找（选定）
constexpr 排序数组，二分查找 log2(18) ≈ 5 次比较。零初始化，~288 字节。

### 方案 B：HashMap 查找
O(1) 均摊，但需运行时初始化，内存占用 512+ 字节。

### 方案 C：线性扫描
最简单，O(N) ~18 次比较。实际极快但无扩展余地。

## 选定方案

方案 A：排序数组 + 二分查找。

## 实现指导

### 命名颜色表

```cpp
struct NamedColor {
  const char* name;
  u32 rgba;  // RGBA32: R[0:7] | G[8:15] | B[16:23] | A[24:31]
};

static constexpr NamedColor kNamedColors[] = {
  {"aqua",        0x00FFFFFF},
  {"black",       0x000000FF},
  {"blue",        0x0000FFFF},
  {"cyan",        0x00FFFFFF},
  {"gray",        0x808080FF},
  {"green",       0x008000FF},
  {"lime",        0x00FF00FF},
  {"maroon",      0x800000FF},
  {"navy",        0x000080FF},
  {"olive",       0x808000FF},
  {"orange",      0xFFA500FF},
  {"purple",      0x800080FF},
  {"red",         0xFF0000FF},
  {"silver",      0xC0C0C0FF},
  {"teal",        0x008080FF},
  {"transparent", 0x00000000},
  {"white",       0xFFFFFFFF},
  {"yellow",      0xFFFF00FF},
};
static constexpr u32 kNamedColorCount = 18;
```

### 查找函数

```cpp
bool LookupNamedColor(StringView name, u32& out_rgba) {
  int lo = 0, hi = kNamedColorCount - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    int cmp = CompareInsensitive(name, kNamedColors[mid].name);
    if (cmp == 0) { out_rgba = kNamedColors[mid].rgba; return true; }
    if (cmp < 0) hi = mid - 1;
    else lo = mid + 1;
  }
  return false;
}
```

大小写不敏感比较，与 CSS 规范一致。

### 颜色解析入口

```cpp
CssValue CssParser::ParseColor() {
  CssToken token = tokenizer_.Peek();
  
  // #hex
  if (token.type == CssTokenType::kHash) {
    tokenizer_.Next();
    return ParseHexColor(token.value);
  }
  
  // rgb() / rgba()
  if (token.type == CssTokenType::kFunction) {
    if (token.value == "rgb" || token.value == "rgba") {
      tokenizer_.Next();
      return ParseRgbFunction(token.value);
    }
  }
  
  // 命名颜色
  if (token.type == CssTokenType::kIdent) {
    u32 rgba;
    if (LookupNamedColor(token.value, rgba)) {
      tokenizer_.Next();
      return CssValue::Color(rgba);
    }
  }
  
  return CssValue::None();
}
```

### Hex 颜色解析

- `#RGB` → 每位复制：`#F00` → `0xFF0000FF`
- `#RRGGBB` → 直接解析 + 0xFF alpha
- `#RRGGBBAA` → 直接解析

### RGBA32 像素格式

与 Graphics HAL 统一：R[0:7] | G[8:15] | B[16:23] | A[24:31]
