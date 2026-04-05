# 创意设计：String SSO 内存布局

**日期：** 2026-04-05
**状态：** 已批准
**关联任务：** TASK-20260405-01

## 设计挑战

为 `vx::String` 设计 Small String Optimization (SSO) 的内部内存布局。UI 引擎中短字符串极其常见（CSS 值、HTML 属性名等），SSO 消除热路径上的堆分配。

**约束：** sizeof <= 32，SSO >= 22 字节，支持 Allocator 模板，UTF-8 + null 终止，移动 O(1)。

## 探索的方案

1. **方案 A：libc++ 风格** — remaining 计数，cap 最高位标志
2. **方案 B：libstdc++ 风格** — self-pointer，独立 local_buf
3. **方案 C：fbstring 风格** — size_and_flag，cap 最低位标志 ✅ 选定

## 选定方案：fbstring 风格

用户选择 fbstring 风格，sizeof=24，SSO 22 字节，标志位编码直观清晰。

## 实现指导

### 内存布局

```cpp
template <typename Alloc = MallocAllocator>
class BasicString {
 private:
  static constexpr usize kSSOCapacity = 22;

  struct Heap {
    char* ptr;              // 8 bytes: 堆上数据指针
    usize size;             // 8 bytes: 字符串长度
    usize cap_and_flag;     // 8 bytes: 最低位=0 表示 Heap
                            //          容量 = cap_and_flag & ~1
  };

  struct SSO {
    char data[23];          // 23 bytes: 数据（含 null 终止符）
    u8 size_and_flag;       // 1 byte: 最高位=1 表示 SSO
                            //         低 7 位 = size (0-22)
  };

  union {
    Heap heap_;
    SSO sso_;
  };

  [[no_unique_address]] Alloc alloc_;

  // sizeof(BasicString) = 24（无状态 Alloc）或 24 + sizeof(Alloc)
};
```

### 模式判断

```cpp
bool IsSSO() const {
  return (sso_.size_and_flag & 0x80) != 0;
}
```

### 关键操作实现

```cpp
const char* data() const {
  return IsSSO() ? sso_.data : heap_.ptr;
}

usize size() const {
  return IsSSO() ? (sso_.size_and_flag & 0x7F) : heap_.size;
}

usize capacity() const {
  return IsSSO() ? kSSOCapacity : (heap_.cap_and_flag & ~usize{1});
}
```

### 构造/赋值规则

**默认构造（空 SSO）：**
```cpp
BasicString() {
  sso_.data[0] = '\0';
  sso_.size_and_flag = 0x80;  // SSO 模式, size=0
}
```

**从 StringView 构造：**
```cpp
BasicString(StringView sv) {
  if (sv.size() <= kSSOCapacity) {
    std::memcpy(sso_.data, sv.data(), sv.size());
    sso_.data[sv.size()] = '\0';
    sso_.size_and_flag = 0x80 | static_cast<u8>(sv.size());
  } else {
    heap_.size = sv.size();
    heap_.cap_and_flag = AlignCap(sv.size() + 1) & ~usize{1};
    heap_.ptr = static_cast<char*>(
        alloc_.Allocate(heap_.cap_and_flag, 1));
    std::memcpy(heap_.ptr, sv.data(), sv.size());
    heap_.ptr[sv.size()] = '\0';
  }
}
```

**移动构造：**
```cpp
BasicString(BasicString&& other) noexcept {
  std::memcpy(this, &other, sizeof(*this));
  // 把 other 重置为空 SSO，防止析构 double-free
  other.sso_.data[0] = '\0';
  other.sso_.size_and_flag = 0x80;
}
```

**析构：**
```cpp
~BasicString() {
  if (!IsSSO()) {
    alloc_.Deallocate(heap_.ptr, heap_.cap_and_flag & ~usize{1});
  }
}
```

### 增长策略

- 容量增长因子 1.5x
- 新容量向上对齐到偶数（保证 `cap_and_flag` 最低位可用作标志）
- SSO → Heap 过渡：分配堆内存，拷贝 SSO 数据，设置 Heap 字段

```cpp
usize AlignCap(usize requested) {
  usize cap = requested < 24 ? 24 : requested;
  cap = (cap + 1) & ~usize{1};  // 对齐到偶数
  return cap;
}
```

### StringView 互转

```cpp
// String → StringView（零开销）
operator StringView() const {
  return StringView(data(), size());
}

StringView view() const {
  return StringView(data(), size());
}
```

### 关键边界

| 场景 | size | 模式 | 备注 |
|------|------|------|------|
| 空字符串 | 0 | SSO | `sso_.size_and_flag = 0x80` |
| `"flex"` | 4 | SSO | 常见 CSS 值 |
| `"background-color"` | 16 | SSO | 仍在 SSO 内 |
| 22 字节字符串 | 22 | SSO | SSO 上限 |
| 23 字节字符串 | 23 | Heap | 首次溢出到堆 |

### 与 Allocator 集成

- 无状态 Allocator（如 MallocAllocator）：`[[no_unique_address]]` 使其零大小，sizeof 保持 24
- 有状态 Allocator（如 ArenaAllocator*）：额外 8 字节指针存储
- `using String = BasicString<MallocAllocator>;` 作为默认别名
