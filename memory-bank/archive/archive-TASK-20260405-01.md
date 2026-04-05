# 归档：Foundation 基础库

**日期：** 2026-04-05
**任务 ID：** TASK-20260405-01
**复杂度级别：** Level 4
**状态：** ✅ 已完成
**分支：** `feature/TASK-20260405-01-foundation`

---

## 任务概述

为 Loong Veloxa（面向车载 HMI 与嵌入式面板的开源 UI 渲染引擎）构建生产级的 Foundation 基础库。该库是整个引擎的最底层模块，所有上层子系统（Graphics HAL、Core Engine、Script 等）都将依赖它。

基于对 Sciter 5.0.2.16 源码架构的深度分析，参考其 `tool` 层设计，结合嵌入式场景需求做了大量裁剪和优化。

## 技术方案

| 决策项 | 选择 | 理由 |
|--------|------|------|
| 语言标准 | C++17 | 嵌入式工具链兼容 + 足够的现代特性 |
| 代码规范 | Google C++ Style | 业界广泛使用，工具链支持好 |
| 内存管理 | Arena/Pool/Malloc 三级分配器 | 满足不同生命周期需求，嵌入式友好 |
| 字符串 | fbstring 风格 SSO（24 字节，22 字节内联） | 避免短字符串堆分配，sizeof 紧凑 |
| 容器 | 自定义 Vector/SmallVector/IntrusiveList/HashMap | Allocator 模板可插拔，嵌入式可控 |
| HashMap | Swiss Table 风格（ctrl 字节 H2 + 线性探测） | 高缓存命中率，SIMD 可后续优化 |
| 日志 | 编译时可裁剪宏 + 可插拔 Sink | 嵌入式零开销，生产可关闭 Debug 日志 |
| 错误处理 | Status/StatusOr + CHECK 断言，无异常 | 嵌入式禁用异常，Status 显式错误传播 |
| 测试框架 | Google Test（系统安装 v1.11.0） | 成熟稳定，系统包可用 |

## 实现摘要

### 文件变更

| 模块 | 头文件 | 实现文件 | 测试文件 |
|------|--------|---------|---------|
| Base | types.h, assert.h, span.h, status.h | — | 4 |
| Memory | allocator.h, memory_stats.h, malloc_allocator.h, arena_allocator.h, pool_allocator.h | 3 (.cc placeholder) | 3 |
| Log | log_sink.h, log.h | log.cc | 1 |
| Containers | vector.h, small_vector.h, intrusive_list.h, hash_map.h | — | 4 |
| Strings | string_view.h, utf.h, string.h, interned_string.h | utf.cc, string.cc, interned_string.cc | 4 |
| Integration | — | — | 1 |
| **总计** | **18** | **7** | **17** |

### 提交历史

| # | Hash | 消息 |
|---|------|------|
| 1 | `0220e09` | chore: project scaffolding with CMake, clang-format, clang-tidy |
| 2 | `44278a2` | feat(foundation): add base types, assert, span, status |
| 3 | `0743f3b` | feat(foundation): memory subsystem - allocator concept, malloc/arena/pool |
| 4 | `69f84b4` | feat(foundation): log subsystem with compile-time level stripping |
| 5 | `33e1133` | feat(foundation): containers - vector, small_vector, intrusive_list, hash_map |
| 6 | `9d46060` | feat(foundation): strings subsystem - string_view, utf, string (SSO), interned_string |
| 7 | `d5d3076` | feat(foundation): integration tests across all subsystems |
| 8 | `668f1c5` | chore(build): finalize TASK-20260405-01 memory bank state |
| 9 | `0c414e0` | docs(reflect): add reflection for TASK-20260405-01 |

### 关键决策

1. **系统 GTest 替代 FetchContent** — 网络限制下无法从 GitHub 拉取，切换为系统包 v1.11.0，功能足够
2. **ArenaAllocator 避免柔性数组** — `-Wpedantic` 禁止 C99 `char data[]`，改用 `reinterpret_cast` 偏移
3. **HashMap 简化为标量探测** — 首版不实现 SIMD Group 探测，保留接口兼容性，SIMD 作为后续优化
4. **日志宏添加 -Wno-type-limits** — `VX_MIN_LOG_LEVEL=0` 时恒真比较触发警告，编译器豁免

### 安全决策

本任务不涉及外部输入处理或认证场景。安全相关措施：
- UTF-8 解码严格拒绝非法序列、overlong 编码、代理对
- 分配器断言检查 size > 0 和对齐为 2 的幂次
- CHECK 失败信息仅含文件名+行号+表达式，不泄露内部状态
- 零外部运行时依赖，攻击面最小

## 测试覆盖

| 模块 | 测试数 | 覆盖要点 |
|------|--------|---------|
| types | 4 | 大小保证、有符号性、类型别名 |
| assert | 6 | CHECK pass/fail、DCHECK debug-only、消息流、表达式输出 |
| span | 11 | 构造、索引、subspan、const、mutation |
| status | 11 | Ok/Error、StatusOr 值/错误、abort on error access |
| malloc_allocator | 6 | 分配/释放、对齐、统计、null |
| arena_allocator | 7 | 同 block、跨 block、超大、Reset、对齐 |
| pool_allocator | 6 | 分配/释放/重用、chunk 扩展、null、最小 block |
| vector | 23 | CRUD、copy/move、非平凡类型生命周期、insert/erase |
| small_vector | 17 | 内联/堆、溢出、copy/move、string 类型 |
| intrusive_list | 11 | push/remove head/tail/mid、双向遍历 |
| hash_map | 16 | CRUD、rehash、自定义 hash/eq、copy/move |
| log | 9 | 各级别、format、fatal abort、自定义 sink |
| string_view | 21 | 构造、find、substr、starts_with/ends_with |
| utf | 35 | 编解码全范围、非法序列、UTF-8/16 往返 |
| string | 26 | SSO/heap 边界、copy/move、append、c_str、sizeof |
| interned_string | 11 | 指针相等、clear、re-intern |
| integration | 8 | 跨子系统组合场景 |
| **总计** | **228** | **100% 通过** |

## 经验教训

1. **子代理 prompt 含完整接口定义 + 测试用例 + 编译约束 = 零返工**
2. **网络依赖须在 Phase 0 验证**（FetchContent 失败可预见，应提前检查系统包）
3. **编译器约束应纳入设计规格**（`-Wpedantic` 柔性数组、`-Wtype-limits` 宏冲突）
4. **Header-only 模板减少构建复杂度**，但增加编译时间——后续大型模块需评估

## 技术债务

| # | 项目 | 优先级 | 说明 |
|---|------|--------|------|
| 1 | Benchmark 延期 | P1 | 需 google benchmark，网络恢复后补充 |
| 2 | HashMap 无 SIMD | P2 | 标量线性探测，Swiss Table SIMD Group 待优化 |
| 3 | InternedString 线程不安全 | P2 | 全局 unordered_set 无锁，多线程需改造 |
| 4 | String allocator 指针 | P2 | sizeof 实际为 32（24 union + 8 Alloc*），可用 compressed pair 优化 |
| 5 | Status::message 用 std::string | P2 | 与自有 String 循环依赖待解决 |

## 架构影响与长期维护

### 对上层模块的影响
- 所有上层模块应通过 `#include "veloxa/foundation/..."` 引用
- 容器使用 `MallocAllocator` 为默认，场景级可替换为 `ArenaAllocator`
- 日志通过 `VX_LOG_*` 宏使用，编译时通过 `VX_LOG_LEVEL` 裁剪

### 维护建议
- 新增容器/字符串功能时保持 header-only 模式
- 添加新 StatusCode 枚举值需同步更新所有 switch 语句
- InternedString 在引入多线程前必须解决线程安全问题

## 参考文档

- 设计规格：`docs/specs/2026-04-05-foundation-design.md`
- 实现计划：`docs/plans/2026-04-05-foundation.md`
- 创意设计（HashMap）：`memory-bank/creative/creative-hashmap.md`
- 创意设计（String SSO）：`memory-bank/creative/creative-string-sso.md`
- 回顾文档：`memory-bank/reflection/reflection-TASK-20260405-01.md`
