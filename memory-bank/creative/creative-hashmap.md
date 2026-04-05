# 创意设计：HashMap Swiss Table 实现

**日期：** 2026-04-05
**状态：** 已批准
**关联任务：** TASK-20260405-01

## 设计挑战

为 `vx::HashMap<K, V>` 选择开放寻址哈希表的内部实现。该容器用于 CSS 属性查找、DOM 属性存储、InternedString 全局表等热路径。

**约束：** 嵌入式友好、支持自定义 Allocator、缓存友好、C++17 / Google C++ 规范。

## 探索的方案

1. **方案 A：Swiss Table（Abseil 风格）** — SIMD 分组探测，ctrl 字节 + 三角数探测 ✅ 选定
2. **方案 B：经典 Robin Hood 线性探测** — PSL 字段，backward-shift 删除
3. **方案 C：混合方案** — ctrl 字节 + 线性探测，无 SIMD

## 选定方案：Swiss Table（Abseil 风格）

用户选择了工业级验证的 Swiss Table 设计，接受 SIMD 平台适配的额外复杂度以换取极致查找性能。

## 实现指导

### 内部布局

```
┌──────────────────────────────────────────┐
│ ctrl_[] : uint8_t 控制字节数组            │
│  Empty = 0x80 (10000000)                 │
│  Deleted = 0xFE (11111110)               │
│  H2 = hash 的低 7 位 (0x00-0x7F)         │
│  按 16 字节一组 (Group)                   │
│  末尾 clone 首组的哨兵 (处理边界探测)     │
├──────────────────────────────────────────┤
│ slots_[] : Slot{ K key; V value; } 数组   │
│  与 ctrl_ 索引一一对应                    │
└──────────────────────────────────────────┘
```

### Hash 拆分

```cpp
// H1: 用于定位起始 Group
size_t H1(size_t hash) { return hash >> 7; }

// H2: 存入 ctrl 字节，用于快速过滤
uint8_t H2(size_t hash) { return hash & 0x7F; }
```

### Group 抽象

```cpp
class Group {
 public:
  explicit Group(const uint8_t* ctrl);

  // 返回 Group 内所有匹配 h2 的位掩码
  BitMask Match(uint8_t h2) const;

  // 返回 Group 内所有 Empty 槽的位掩码
  BitMask MatchEmpty() const;

  // 返回 Group 内所有 Empty 或 Deleted 的位掩码
  BitMask MatchEmptyOrDeleted() const;

 private:
#if defined(__SSE2__)
  __m128i ctrl_;  // SSE2: 16 字节一次比较
#elif defined(__ARM_NEON)
  uint8x16_t ctrl_;  // NEON: ARM 等效
#else
  uint8_t ctrl_[kGroupSize];  // 标量回退
#endif
};
```

### 探测策略

三角数探测：`probe(i) = (i * (i + 1)) / 2`，对 2^n 容量探测不重复。

```cpp
class ProbeSeq {
 public:
  ProbeSeq(size_t hash, size_t mask)
      : offset_(H1(hash) & mask), mask_(mask) {}

  size_t offset() const { return offset_; }

  void Next() {
    index_ += kGroupSize;
    offset_ = (offset_ + index_) & mask_;
  }

 private:
  size_t offset_;
  size_t mask_;
  size_t index_ = 0;
};
```

### 关键参数

| 参数 | 值 | 理由 |
|------|-----|------|
| Group 大小 | 16 字节 | SSE2/NEON 寄存器宽度 |
| 负载因子 | 7/8 (87.5%) | Swiss Table 经验值，H2 过滤保证高负载仍高效 |
| 初始容量 | 16 (1 Group) | 最小合理值 |
| 增长因子 | 2x | 容量始终为 2^n |

### 平台适配策略

1. **x86-64 (开发环境)**：SSE2（所有 x86-64 CPU 必备）
2. **ARM (车载嵌入式)**：NEON（ARMv7+ 标配）
3. **无 SIMD 回退**：逐字节标量比较，接口一致

### 删除策略

- 删除时标记 `ctrl_[i] = kDeleted (0xFE)`
- 查找时跳过 Deleted 继续探测
- 插入时可复用 Deleted 槽
- `size + deleted > capacity * 7/8` 时触发 rehash 清理

### 与 Allocator 集成

- ctrl_ 和 slots_ 在同一次分配中连续分配（`ctrl | padding | slots`）
- Allocator 通过 `[[no_unique_address]]` 存储，无状态 Allocator 零开销
