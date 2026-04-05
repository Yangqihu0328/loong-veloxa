# 回顾：DOM 树 + HTML 解析器

**日期：** 2026-04-05
**任务 ID：** TASK-20260405-03
**复杂度级别：** Level 4

## 计划 vs 实际

| 维度 | 计划 | 实际 | 偏差原因 |
|------|------|------|---------|
| Phase 数 | 7 | 7 | 完全匹配 |
| 预估时间 | ~4h | ~1h（会话时间） | 子代理并行大幅缩短 |
| 预估测试 | ~85 | 100 | 子代理比预估更充分 |
| 代码文件 | ~20 | 32（含 docs/MB） | 基本匹配，含文档 |
| 生产代码 | 未精确预估 | 1340 行 | — |
| 设计变更 | 无 | 无 | 设计与实现完全匹配 |

## 回顾检查清单

- [x] 计划精确度 — 7 Phase 结构完全匹配，文件清单与实际高度一致
- [x] TDD 执行情况 — 子代理内部先写测试再实现，Phase 2 由主代理直接 TDD
- [x] 子代理质量 — 3 个子代理全部一次通过，零返工（P1 精确 API 签名改进生效）
- [x] 测试隔离 — 所有测试独立，无环境依赖
- [x] 提交粒度 — 按 Phase 粒度提交，7 个提交结构清晰
- [x] 非默认路径 — 实体解码、隐式关闭、void/self-closing/rawtext 均有测试

## 做得好的

1. **子代理零返工**：本次所有子代理（Phase 3/4/5/6）全部一次通过编译和测试。根因：prompt 中提供了精确的 API 签名（包括构造函数参数、返回类型、namespace）。这是 TASK-20260405-02 P1 改进「子代理 prompt 增加跨模块数据格式」的直接收益。

2. **Creative 设计精准**：Tokenizer 混合方案和数据驱动隐式关闭规则表均直接映射为实现代码，无需在 build 阶段做设计调整。20 条隐式关闭规则覆盖了所有车载 HMI 常见场景。

3. **并行策略有效**：Phase 3（DOM）+ Phase 4（Tokenizer）并行执行，因为二者仅共享 Phase 2 的 tag.h 接口，互不依赖。

4. **测试超额完成**：计划 85 个测试，实际 100 个。子代理在 Element 操作（14 个）、Tokenizer（25 个）方面比计划更细致。

5. **Sciter 参考价值高**：`html-dom.h` 的 `node::NODE_TYPE` 枚举、`html-symbols.h` 的 tag 定义、`html-parser.cpp` 的 `dom_builder` 模式直接指导了 Veloxa 设计，同时成功裁剪了 Sciter 桌面特有的功能（bookmark、split、selection 等）。

## 遇到的挑战

1. **无显著阻碍**：本任务是三个任务中最顺利的一次。没有编译错误、格式错误或跨模块不一致。前两个任务积累的经验（编译器约束、像素格式 bug）在此得到了回报。

2. **Document 内存管理简陋**：当前使用 `Vector<Node*> owned_nodes_` + 析构时逐一 `delete`。计划中提到 Arena 分配但推迟到后续任务。这是已知技术债务。

## 经验教训

1. **精确 API 签名是子代理零返工的关键**：提供完整的函数签名（参数类型、返回类型、命名空间、构造函数形式）比描述性文字更有效。本次验证了 TASK-02 的 P1 改进。

2. **Creative 阶段投入产出比高**：Tokenizer 和隐式关闭规则的 creative 设计文档直接成为了 build 阶段的实现规范，避免了边写边设计的混乱。

3. **先建 Tag 系统是正确顺序**：TagId 枚举是 DOM 和 Parser 的共同基础，先单独完成后，后续 Phase 的子代理可以精确引用。

4. **数据驱动表优于硬编码**：隐式关闭规则的数据表设计使 parser.cc 的 HandleImplicitClose 非常简洁（仅 ~30 行），且规则可独立验证。

## 改进建议

| # | 建议 | 优先级 | 落实方式 | 目标 |
|---|------|--------|---------|------|
| 1 | TagIdFromName 应使用 HashMap 或完美哈希替代 O(N) 线性扫描 | P2 | 记录到 techContext.md | tag.cc |
| 2 | Document 应支持 ArenaAllocator（节点批量分配/释放） | P1 | 下个任务或独立 TASK | document.h |
| 3 | Parser 应收集解析错误而非静默忽略 | P2 | 记录到 techContext.md | parser.cc |
| 4 | 子代理 prompt 模板中提供精确 API 签名 — 已验证有效，应固化为标准做法 | P1 | 更新到 systemPatterns.md | subagent prompt |

## 技术改进建议

1. **TagIdFromName O(N) 查找**：当前约 70 个标签线性扫描。实际解析中每个标签调用一次，对车载 HMI 页面（<500 元素）影响微乎其微，但可优化为 HashMap 或 constexpr 哈希。
2. **Document 内存管理**：应集成 ArenaAllocator，Document 拥有 Arena，所有 CreateElement/CreateText/CreateComment 从 Arena 分配。
3. **Parser 错误收集**：当前 kError token 被静默忽略。应支持 `Vector<ParseError>` 收集错误，含行号/列号/描述。
4. **Serializer 空白处理**：当前不做空白规范化。HTML → DOM → Serialize 往返时空白可能变化。对车载 HMI 场景影响不大。
5. **DocumentFragment**：当前不支持。未来 JS 操作 DOM 时需要。

## 反复模式识别

| 已知模式 | 本次是否重复？ | 备注 |
|---------|-------------|------|
| 计划文件清单与实际不一致 | ❌ | 高度一致 |
| 子代理产出需大量返工 | ❌ | 零返工（P1 改进生效） |
| 前置依赖/环境未验证 | ❌ | /van 阶段已验证 |
| 非默认路径遗漏验证 | ❌ | 实体/void/rawtext 均已测试 |
| 测试隔离问题 | ❌ | 全部独立 |
| 提交粒度偏离 | ❌ | 严格按 Phase |
| TDD 严格度不匹配 | ❌ | Phase 2 主代理 TDD，其余子代理内部 TDD |

**本次无反复模式。** 前两次任务积累的改进全部生效。

## 安全评估

本任务不涉及安全变更。HTML 解析器处理的是开发者编写的 HMI HTML（非用户输入），不需要防御 XSS 等攻击。未来如果接入用户内容，需要添加 sanitizer。

## 架构评估

### DOM 层与引擎架构的关系

```
           ┌──────────────────────┐
           │   CSS Engine (待建)   │
           └──────────┬───────────┘
                      │ 读取 DOM 属性、匹配元素
           ┌──────────┴───────────┐
           │   DOM Tree (本任务)   │ ← HTML Parser 产出
           └──────────┬───────────┘
                      │ 提供节点数据给 Layout
           ┌──────────┴───────────┐
           │  Layout Engine (待建) │
           └──────────┬───────────┘
                      │ 生成绘制指令
           ┌──────────┴───────────┐
           │  Graphics HAL (已完成)│
           └──────────────────────┘
```

DOM 层的接口设计（Node/Element/Text/Document）与后续 CSS 和 Layout 模块的预期交互点：
- CSS 引擎：遍历 Element 树，匹配选择器，读取 tag_id/attributes
- Layout 引擎：遍历 DOM 树，根据计算样式生成布局盒
- 事件系统：DOM 树作为事件冒泡/捕获的传播路径

当前接口已能支撑这些需求。
