# Foundation 基础库 — 设计规格

**任务 ID**：TASK-20260405-01
**日期**：2026-04-05
**状态**：已批准

## 设计决策记录

| 决策项 | 选择 | 理由 |
|--------|------|------|
| 开发策略 | 从完整子系统切入（Foundation 先行） | 零依赖底座，做好后上层开发效率翻倍 |
| 内存管理 | 自定义分配器体系（Arena/Pool/MallocWrapper） | 嵌入式最大灵活性，可替换为静态内存池 |
| 字符串 | UTF-8 内部 + 自定义 String/StringView + InternedString | 节省内存，与 Linux/JS 生态兼容，零拷贝解析 |
| 容器 | 4 个核心容器 + std:: 补充 | YAGNI，覆盖 90%+ 场景 |
| 日志 | 编译时可裁剪分级日志（自研） | 零运行时开销，嵌入式关键需求 |
| 测试框架 | Google Test + Google Benchmark | 与 Google C++ 规范配套 |
| 代码规范 | Google C++ Style Guide | 用户指定 |
| 错误处理 | 不使用异常，Status/StatusOr + CHECK 断言 | Google C++ 规范 + 嵌入式环境 |

## 架构

```
veloxa/foundation/
├── memory/              # 内存管理子系统
│   ├── allocator.h              # Allocator 概念与接口
│   ├── malloc_allocator.h       # 全局 malloc 封装（计数/对齐/OOM）
│   ├── arena_allocator.h        # Arena 线性分配器
│   ├── pool_allocator.h         # Pool 固定大小块分配器
│   └── memory_stats.h           # 全局内存统计
├── containers/          # 容器库
│   ├── vector.h                 # vx::Vector<T, Alloc>
│   ├── small_vector.h           # vx::SmallVector<T, N, Alloc>
│   ├── hash_map.h               # vx::HashMap<K, V, Hash, Eq, Alloc>
│   └── intrusive_list.h         # vx::IntrusiveList<T>
├── strings/             # 字符串子系统
│   ├── string_view.h            # vx::StringView
│   ├── string.h                 # vx::String (SSO + Allocator)
│   ├── interned_string.h        # vx::InternedString
│   └── utf.h                    # UTF-8/16/32 编解码
├── log/                 # 日志子系统
│   ├── log.h                    # 日志宏与级别
│   └── log_sink.h               # 可插拔输出后端
├── base/                # 基础类型
│   ├── types.h                  # 类型别名 u8/u16/u32/i32/f32...
│   ├── status.h                 # vx::Status / vx::StatusOr<T>
│   ├── span.h                   # vx::Span<T>
│   └── assert.h                 # VX_CHECK / VX_DCHECK
└── CMakeLists.txt
```

## 组件详细设计

### 内存管理

- **Allocator 概念**：模板约束（非虚函数），要求 `Allocate(size_t size, size_t align)` 返回 `void*`，`Deallocate(void* ptr, size_t size)` 释放
- **MallocAllocator**：`aligned_alloc/free` 封装，原子计数器统计当前/峰值分配量，OOM 回调钩子
- **ArenaAllocator**：从大块内存（Block）线性递增分配，Block 用链表管理，`Reset()` 整块释放，不支持单个 `Deallocate`
- **PoolAllocator**：固定 `block_size` 的空闲链表，批量预分配 chunk，用完时扩展新 chunk

### 容器

- **Vector**：连续内存，增长因子 1.5x，`push_back/emplace_back/pop_back/insert/erase/resize/reserve`，迭代器为裸指针
- **SmallVector<T, N>**：继承 Vector 接口，栈上 `alignas(T) char[N * sizeof(T)]` 缓冲，`size <= N` 时零堆分配
- **HashMap**：开放寻址 Robin Hood 哈希，`uint8_t[]` 控制字节数组（Empty/Deleted/H2），负载因子 0.75 触发 rehash
- **IntrusiveList**：`IntrusiveListNode` 混入基类（prev/next），O(1) 插入/删除，不管理元素生命周期

### 字符串

- **StringView**：`const char* data_ + size_t size_`，提供 `substr/find/starts_with/ends_with/operator==/compare`
- **String**：SSO 阈值 22 字节，内部 union { struct { char* ptr; size_t size; size_t cap; }; char sso[24]; }，最后一字节存剩余容量/标志
- **InternedString**：全局 `HashMap<StringView, const char*>`，`Intern(StringView)` 返回 `InternedString`，比较为指针比较
- **UTF**：`EncodeUtf8(char32_t) -> int`，`DecodeUtf8(const char*, size_t) -> char32_t`，`Utf8ToUtf16/Utf16ToUtf8` 批量转换

### 日志

- `VX_LOG(level, fmt, ...)` 展开为条件检查 + 格式化 + Sink 写入
- `VX_MIN_LOG_LEVEL` 编译宏裁剪，低于此级别的 `VX_LOG` 展开为空
- `LogSink`：`virtual void Write(LogLevel, const char* file, int line, StringView msg) = 0`
- `SetLogSink(LogSink*)` 全局注册

### 错误处理

- **Status**：`enum StatusCode { kOk, kInvalidArgument, kOutOfMemory, kNotFound, ... }` + `std::string message_`
- **StatusOr<T>**：`union { Status; T }` 的标签联合，`ok()` / `value()` / `status()`
- **VX_CHECK(cond)**：Release 断言，失败时 `VX_LOG(FATAL, ...)` + `abort()`
- **VX_DCHECK(cond)**：Debug-only 断言，Release 下编译为空

## 测试策略

- 每个子系统独立 gtest 目标
- 分配器：对齐验证、统计正确性、OOM 回调、Arena 批量释放、Pool 空闲链
- 容器：CRUD、迭代器正确性、边界（空/单元素/大量）、与 Arena/Pool 配合
- 字符串：SSO 阈值边界（21/22/23 字节）、UTF 正确性（BMP/supplementary/边界码点/非法序列）、Interned 唯一性与并发安全
- Benchmark：Vector vs std::vector、HashMap vs std::unordered_map、String SSO vs std::string
