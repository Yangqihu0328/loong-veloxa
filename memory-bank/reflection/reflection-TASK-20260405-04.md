# 回顾：CSS 引擎

**日期：** 2026-04-05
**任务 ID：** TASK-20260405-04
**复杂度级别：** Level 4

## 计划 vs 实际

| 维度 | 计划 | 实际 | 偏差原因 |
|------|------|------|---------|
| Phase 数 | 7 | 7 | 完全匹配 |
| 预估测试 | ~109 | 125 | 子代理在 matcher/tokenizer 部分比预估更细致 |
| 代码文件 | 14 生产 + 6 测试 | 14 生产 + 6 测试 | 完全匹配 |
| 生产代码行数 | 未精确预估 | ~2,467 行 | — |
| 测试代码行数 | 未精确预估 | ~1,295 行 | — |
| 设计变更 | 无 | 2 处小偏离 | 见下方 |
| 提交数 | 每 Phase 1 次 | 4+1（合并提交） | Phase 1+2+5 合并提交、Phase 4+6 合并提交 |

### 偏离计划说明

1. **ScanWhitespace 合并注释跳过**：Phase 2 子代理发现 `/* comment */` 嵌在空白中会产生两个 kWhitespace token，修改为在 ScanWhitespace 内循环跳过注释。属于实现优化，不影响接口。

2. **Border 简写测试用 longhand**：Phase 6 的 StyleResolver 测试中，`border: 1px solid red` 简写展开依赖 Parser 的正确实现。子代理选择用 longhand 属性直接测试 ApplyDeclaration，避免跨模块耦合。集成测试中已覆盖简写全流程。

3. **提交粒度**：计划是每 Phase 一次提交，实际采用了分组合并提交（Group 1 含 Phase 1+2+5，Group 3 含 Phase 4+6）。原因是并行子代理产出在同一时间完成，合并提交减少了碎片。

## 回顾检查清单

- [x] 计划精确度 — 7 Phase 结构完全匹配，文件清单 100% 一致
- [x] TDD 执行情况 — 子代理内部先写测试再实现，所有 Phase 均含独立测试
- [x] 子代理质量 — 5 个子代理（Phase 1+2、Phase 5、Phase 3、Phase 4+6、Phase 7）全部一次通过
- [x] 测试隔离 — 所有 528 测试独立，无环境依赖，无 flaky
- [x] 提交粒度 — 4 次功能提交 + 1 次 MB 提交，按 Group 粒度清晰
- [x] 非默认路径 — 错误恢复（未知属性跳过）、简写展开（1/2/3/4 值 margin）、CDO/CDC token、!important 覆盖均已测试

## 做得好的

1. **子代理持续零返工**：连续第二个 Level 4 任务实现子代理零返工。5 个子代理全部一次通过编译和测试。精确 API 签名 prompt 模式已完全固化。

2. **并行策略精准**：
   - Group 1（Phase 1+2 || Phase 5）：属性系统 + Tokenizer 与 DOM 扩展完全独立，并行无冲突
   - Group 3（Phase 4+6 合并）：SelectorMatcher 和 StyleResolver 紧耦合，合并为一个子代理减少了接口协商
   - 子代理合并 Phase 策略首次使用，效果显著

3. **CMake 存根预创建**：Phase 1+2 子代理在创建生产文件的同时，为后续 Phase 的文件创建了空 .cc 存根，避免 CMake 配置失败。这是一个值得标准化的做法。

4. **Creative 设计到实现的一致性**：CSS Tokenizer 的状态机设计（主分支 + 子扫描器 + 注释分离）和颜色解析设计（排序数组 + 二分查找）直接映射为代码，build 阶段无需设计返工。

5. **集成测试覆盖全面**：12 个端到端测试覆盖了 HMI 仪表盘典型场景，包括三层继承链、specificity 层叠、!important 覆盖、内联样式、Flex 布局、多样式表合并。

## 遇到的挑战

1. **无显著阻碍**：本任务是四个任务中最顺利的一次。编译零错误、测试零失败、子代理零返工。前三个任务积累的所有工程模式（编译器约束、像素格式约定、子代理 prompt 模板、枚举+元数据表模式）在此全部复用。

2. **Parser 是最大文件**：`parser.cc` 达 1,035 行，包含选择器解析、声明解析、值类型分发、简写展开、颜色解析等多个责任。虽然功能内聚（都是 CSS 文本 → 数据结构的转换），但后续增加属性或选择器类型时可能需要拆分。

3. **ApplyDeclaration switch 规模大**：`style_resolver.cc` 中的 ApplyDeclaration 对 ~55 个 PropertyId 做 switch，逻辑重复度高。这是 CSS 引擎的固有复杂度，Sciter 也使用类似模式。

## 经验教训

1. **合并 Phase 给子代理是有效的**：当两个 Phase 紧密耦合（如 Phase 1+2 共享 Unit 枚举，Phase 4+6 共享 selector 类型），合并为一个子代理比分别启动更高效——减少了 prompt 中重复提供的上下文，也避免了中间产物的同步问题。

2. **存根文件预创建应标准化**：当 CMakeLists.txt 预列了所有文件但并行构建时部分文件尚未创建，第一个子代理应负责创建空存根。这避免了后续子代理因 CMake 配置失败而报错。

3. **颜色像素格式一致性**：RGBA32 格式（R[0:7]|G[8:15]|B[16:23]|A[24:31]）在 CSS 解析中的使用与 Graphics HAL 完全一致。TASK-02 建立的像素格式约定在此得到了跨模块验证。

4. **枚举 + 元数据表模式的可复用性**：PropertyId 的设计（枚举 + kPropertyTable[] 静态表 + O(1) 查找 + 线性扫描名称查找）与 TASK-03 的 TagId 完全一致。该模式已验证为 Veloxa 的标准 ID 系统模式。

## 改进建议

| # | 建议 | 优先级 | 落实方式 | 目标 |
|---|------|--------|---------|------|
| 1 | parser.cc 过大（1035 行），考虑拆分为 parser_selector.cc + parser_value.cc | P2 | 记录到 techContext.md | parser.cc |
| 2 | ApplyDeclaration 的 switch 可用宏或代码生成简化 | P2 | 记录到 techContext.md | style_resolver.cc |
| 3 | PropertyIdFromName 使用 O(N) 线性扫描（与 TagIdFromName 相同问题） | P2 | 记录到 techContext.md | property.cc |
| 4 | CSS 解析器应支持 @media 规则（至少跳过而非截断） | P2 | 记录到 techContext.md | parser.cc |
| 5 | 存根文件预创建策略应固化到子代理开发规则中 | P1 | 更新 systemPatterns.md | subagent-development |
| 6 | 合并 Phase 给子代理的策略应固化到计划模板中 | P1 | 更新 systemPatterns.md | writing-plans |

## 技术改进建议

1. **Parser 拆分**：1035 行的 parser.cc 包含选择器解析、值解析、简写展开、颜色解析四个职责。随着 CSS 属性子集扩展（如未来增加 transform、animation），该文件将快速膨胀。建议拆分为 `parser.cc`（规则/选择器）+ `parser_value.cc`（值/颜色/简写）。

2. **PropertyIdFromName 优化**：当前 ~55 个属性线性扫描。与 TagIdFromName 的 ~70 标签相同问题。二者可统一升级为 HashMap 或 constexpr perfect hash。

3. **CSS 变量支持**：当前不支持 `var(--custom-property)`。车载 HMI 主题切换通常依赖 CSS 变量。建议作为后续独立 TASK。

4. **Selector 哈希加速**：当前对每个元素遍历所有规则的所有选择器（O(rules × selectors × elements)）。可通过按 tag/class/id 建立哈希索引加速匹配。

5. **内存优化**：ComputedStyle 结构体约 ~200 字节。对于大量元素的 HMI 页面，可考虑共享默认 ComputedStyle 实例（CSS 继承场景下多个子元素可能共享同一计算样式）。

## 反复模式识别

| 已知模式 | 本次是否重复？ | 备注 |
|---------|-------------|------|
| 计划文件清单与实际不一致 | ❌ | 14+6 完全匹配 |
| 子代理产出需大量返工 | ❌ | 5 个子代理零返工 |
| 前置依赖/环境未验证 | ❌ | /van 阶段已验证 GCC/GTest/Foundation/DOM |
| 非默认路径遗漏验证 | ❌ | 错误恢复、简写展开、!important 均已测试 |
| 测试隔离问题 | ❌ | 528 测试全部独立 |
| 提交粒度偏离 | ⚠️ 轻微 | 按 Group 而非 Phase 提交（合理偏离） |
| TDD 严格度不匹配 | ❌ | 子代理内部均为 TDD |

**本次无负面反复模式。** 提交粒度偏离属合理优化（并行子代理合并提交）。

## 安全评估

本任务不涉及安全变更。CSS 引擎处理的是开发者编写的 HMI 样式（非用户输入），不需要防御 CSS 注入。未来如果接入动态样式更新（如 OTA 主题包），需要添加输入验证。

| 维度 | 状态 | 备注 |
|------|------|------|
| 输入验证 | N/A | CSS 来源为开发者，非用户输入 |
| 认证/授权 | N/A | 不涉及 |
| 数据保护 | N/A | 不涉及 |
| 依赖审计 | N/A | 无新增外部依赖 |
| 错误信息脱敏 | N/A | 不涉及 |
| 敏感数据处理 | N/A | 不涉及 |

## 架构评估

### CSS 引擎在系统中的位置

```
           ┌──────────────────────┐
           │  Layout Engine (待建) │ ← 读取 ComputedStyle 驱动布局
           └──────────┬───────────┘
                      │
           ┌──────────┴───────────┐
           │  CSS Engine (本任务)  │ ← Stylesheet + DOM → ComputedStyle
           │  Tokenizer → Parser  │
           │  → Matcher → Resolver│
           └──────────┬───────────┘
                      │ 读取 DOM 属性、匹配元素
           ┌──────────┴───────────┐
           │   DOM Tree (TASK-03)  │ ← HTML Parser 产出
           └──────────┬───────────┘
                      │
           ┌──────────┴───────────┐
           │  Graphics HAL (TASK-02)│
           └──────────────────────┘
```

### CSS 引擎数据流

```
CSS text ─→ CssTokenizer ─→ CssParser ─→ Stylesheet
                                              │
HTML text ─→ HtmlParser ─→ Document           │
                              │                │
                              ├── Element ←────┤
                              │   (id/classes) │
                              │                │
                              └── SelectorMatcher ──→ StyleResolver ──→ ComputedStyle
                                                         ↑
                                                    parent ComputedStyle (继承)
```

### 对后续模块的接口保证

- **Layout Engine**：读取 `ComputedStyle` 的 display、position、width/height/margin/padding、flex-* 属性驱动盒模型计算。接口已就绪。
- **事件系统**：`:hover`/`:active`/`:focus` 伪类当前返回 false（stub）。事件系统实现后需回填状态查询。
- **JS DOM API**：可通过 `CssParser::ParseDeclarationList` 解析 `element.style.cssText`，通过 `StyleResolver::Resolve` 获取 `getComputedStyle()`。

### 长期影响

1. **CSS 属性扩展路径清晰**：新增属性只需在 PropertyId 添加枚举值、kPropertyTable 添加表项、ApplyDeclaration 添加 case、ComputedStyle 添加字段。四点修改，完全数据驱动。

2. **性能瓶颈在 Resolve**：当前 O(rules × elements) 的全量匹配在大页面上可能成为瓶颈。建议在 Layout Engine 任务之前添加选择器索引优化。

3. **像素格式统一已跨三个模块验证**：RGBA32 格式从 Graphics HAL（TASK-02）→ CSS Color（TASK-04）一致，为 Layout → Render 管线奠定基础。
