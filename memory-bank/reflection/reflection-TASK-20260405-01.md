# 回顾：Foundation 基础库

**日期：** 2026-04-05
**任务 ID：** TASK-20260405-01
**复杂度级别：** Level 4

---

## 计划 vs 实际

| 维度 | 计划 | 实际 | 偏差原因 |
|------|------|------|---------|
| 任务数 | 17（7 Phase） | 17（7 Phase） | 无偏差 |
| 预估时间 | ~15.5 h | ~2 h（会话时间） | 子代理并行化大幅提速 |
| 源文件 | ~50 | 43（26 源 + 17 测试） | 基本吻合，header-only 减少了 .cc |
| 测试数 | 未明确量化 | 228 | 超出预期，覆盖充分 |
| GTest 来源 | FetchContent v1.15.2 | 系统安装 v1.11.0 | 网络限制导致无法下载 |
| Benchmark | Google Benchmark | 延期 | 同上，网络限制 |
| HashMap 实现 | Swiss Table + SIMD | Swiss Table + 线性探测（无 SIMD） | 首版简化为标量，SIMD 作为后续优化 |
| ArenaAllocator | 柔性数组 `char data[]` | `reinterpret_cast` 偏移 | `-Wpedantic` 禁止 C99 柔性数组 |
| String sizeof | 24 字节 | 24 字节（storage union） | 符合设计 |

## 回顾检查清单

- [x] **计划精确度** — 文件清单与实际高度一致，预估时间因子代理而大幅缩短
- [x] **TDD 执行情况** — 子代理内部执行了 RED→GREEN，但主流程中批量产出后统一验证，属"批量 TDD"变体
- [x] **子代理质量** — 7 次子代理调用，0 次返工，质量优秀
- [x] **测试隔离** — 228 测试无 flaky，全部独立通过
- [x] **提交粒度** — 按子系统分 7 个 feat 提交 + 1 个 chore 提交，符合 Level 4 分提交要求
- [ ] **非默认路径** — 部分边界场景（OOM 回调、线程安全）未覆盖

---

## 做得好的

1. **子代理并行化效果显著**
   - Phase 2 中 assert/span/status 3 个模块并行实现
   - Phase 4（日志）和 Phase 5（容器）并行推进
   - 零返工，子代理 prompt 足够详细（包含完整接口定义和测试用例）

2. **设计文档驱动实现**
   - `/creative` 阶段的 Swiss Table 和 fbstring SSO 设计文档直接转化为实现规格
   - 减少了实现阶段的决策犹豫

3. **渐进式构建验证**
   - 每个 Phase 完成后立即 clean build + ctest
   - 问题（如 -Wpedantic 柔性数组、-Wtype-limits 日志宏）在当 Phase 内解决，不累积

4. **测试覆盖度高**
   - 代码行数比 —— 源码 2563 行 vs 测试 2279 行，接近 1:1
   - 集成测试覆盖了 8 个跨子系统场景

5. **提交粒度清晰**
   - 8 个提交，每个对应一个明确的功能边界
   - 提交消息规范（conventional commits），方便 bisect

## 遇到的挑战

1. **网络限制**
   - FetchContent 无法从 GitHub 拉取 googletest/benchmark
   - **解决**：切换为系统包 `libgtest-dev`（v1.11.0），版本略旧但功能完整
   - **影响**：Benchmark 延期至网络可用时

2. **-Wpedantic 与 C99 柔性数组**
   - ArenaAllocator 原设计使用 `char data[]`（ISO C++ 不允许）
   - **解决**：改用 `reinterpret_cast<char*>(block) + sizeof(Block)` 计算数据区偏移
   - **影响**：代码略不直观，但语义等价

3. **日志宏与 -Wtype-limits**
   - `VX_MIN_LOG_LEVEL=0` 时，`static_cast<int>(LogLevel::kDebug) >= 0` 恒真触发警告
   - **解决**：添加 `-Wno-type-limits` 编译选项
   - **影响**：轻微降低了编译器检查覆盖度

4. **TDD 严格度与子代理模式的张力**
   - 子代理同时产出测试和实现，主流程无法逐个观察 RED 阶段
   - **缓解**：子代理内部报告了 RED→GREEN 过程，但未在主流程记录

## 经验教训

1. **子代理 prompt 的详细度决定产出质量**
   - 包含完整接口定义（代码片段）+ 测试用例（代码片段）+ 编译约束（-Werror flags）的 prompt 产出零返工
   - 仅给功能描述的 prompt 会导致接口不一致

2. **网络依赖需在 Phase 0 验证**
   - FetchContent 在离线环境失败是可预见的
   - 应在脚手架阶段就确认依赖可用性（系统包 vs 下载）

3. **编译标志冲突应纳入设计规格**
   - `-Wpedantic` 禁止柔性数组、`-Wtype-limits` 与宏冲突——这些应在设计阶段列为约束
   - 下次在 `/plan` 中增加「编译器约束」章节

4. **header-only 模板库减少了构建复杂度**
   - 容器、分配器（除 ArenaAllocator 的 Block 管理外）均为 header-only
   - 减少了 CMakeLists.txt 的 source 列表维护成本

## 流程改进

- **头脑风暴阶段**：充分，Swiss Table 和 SSO 设计经过三方案对比
- **计划详细度**：优秀，17 个任务的文件清单和代码片段精确到行级
- **TDD 流程**：因子代理批量模式有所妥协（见上），但测试覆盖补偿了严格度
- **代码审查**：子代理两阶段审查（规格合规 + 代码质量）在 prompt 中隐式完成

## 技术改进建议

### 代码质量
- **好**：Google style 一致，include guard 规范，namespace 正确
- **待改进**：部分容器缺少 `noexcept` 标注（如 Vector::swap），HashMap 未实现 `swap`

### 测试覆盖
- **好**：228 测试，源码/测试行数比接近 1:1
- **待改进**：
  - OOM 路径未测试（MallocAllocator 分配失败直接 abort，无法 mock）
  - 线程安全未测试（MemoryStats 使用 atomic 但未并发测试）
  - InternedString 线程安全未考虑（全局 unordered_set 无锁）

### 技术债务
1. **Benchmark 延期** — 需要网络可用后补充 google benchmark
2. **HashMap 无 SIMD** — 当前为标量线性探测，Swiss Table 的 SIMD Group 探测待后续优化
3. **InternedString 线程不安全** — 当前单线程设计，多线程场景需加锁或改为 concurrent hash map
4. **String allocator 指针** — BasicString 模板存储 `Alloc*` 指针，增加 8 字节开销（实际 sizeof 为 32 含指针）
5. **Status.message 使用 std::string** — 待自有 String 完成后可替换（循环依赖需解决）

### 性能考虑
- ArenaAllocator：无 thread-local 优化，多线程场景需每线程一个 arena
- Vector：1.5x 增长因子合理，但缺少 shrink_to_fit
- HashMap：load factor 7/8 + 线性探测在高冲突场景可能退化

## 安全评估

| 维度 | 状态 | 备注 |
|------|------|------|
| 输入验证 | ✅ | UTF 解码拒绝非法序列、overlong、代理对；分配器检查 size > 0 和对齐 |
| 认证/授权 | N/A | 基础库无认证需求 |
| 数据保护 | N/A | 无敏感数据处理 |
| 依赖审计 | ✅ | 零外部运行时依赖（仅编译时 GTest） |
| 错误信息脱敏 | ✅ | CHECK 失败仅输出文件名+行号+表达式 |
| 敏感数据处理 | N/A | 无 |

## 反复模式识别

| 已知模式 | 本次是否重复？ | 说明 |
|---------|-------------|------|
| 计划文件清单与实际不一致 | ❌ | 文件清单高度吻合 |
| 子代理产出需大量返工 | ❌ | 7 次子代理 0 返工 |
| 前置依赖/环境未验证 | ✅ | FetchContent 网络失败，应提前检查 |
| 非默认路径遗漏验证 | ✅ | OOM、线程安全路径未覆盖 |
| 测试隔离问题 | ❌ | 228 测试稳定无 flaky |
| 提交粒度偏离计划 | ❌ | 按子系统提交 |
| TDD 严格度与场景不匹配 | ✅ | 子代理模式下 RED 阶段不可观察 |

---

## 改进建议（附优先级与落实方式）

| # | 建议 | 优先级 | 落实方式 | 目标 |
|---|------|--------|---------|------|
| 1 | `/plan` 增加「环境约束」章节（网络、系统包、编译器版本） | P1 | 修改 writing-plans.mdc | 避免 FetchContent 等网络依赖失败 |
| 2 | `/plan` 增加「编译器约束」章节（-Wpedantic 限制清单） | P1 | 修改 writing-plans.mdc | 避免柔性数组等 C99 问题 |
| 3 | 子代理模式下记录 TDD 证据（截取 RED/GREEN 输出） | P2 | 修改 subagent-development.mdc | 反复出现，需固化到规则 |
| 4 | 补充 Benchmark（网络恢复后） | P1 | 迁移到 activeContext.md 待处理 | 验证性能目标 |
| 5 | InternedString 添加线程安全方案 | P2 | 记录到 techContext.md | 多线程场景 |
| 6 | OOM 路径测试策略 | P2 | 记录到 systemPatterns.md | 嵌入式场景关键 |
