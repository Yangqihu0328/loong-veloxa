# 回顾：功能补全（border-radius / 字体渲染 / 图片支持 / JS-DOM 绑定）

**日期：** 2026-04-14
**任务 ID：** TASK-20260414-01
**复杂度级别：** Level 4

## 计划 vs 实际

| 维度 | 计划 | 实际 | 偏差原因 |
|------|------|------|---------|
| 任务数（Phase） | 21 | 21 | 无偏差 |
| 预估时间 | 8-10.5 hr | ~1 hr（子代理并行） | 子代理效率远超手动逐步 TDD |
| 文件变更数 | ~45（计划列出） | 52 | 额外 CMakeLists 修改、tests/CMakeLists 多处追加 |
| 新增测试数 | ~30（估） | 37 | 子代理补充了额外的边界用例 |
| 设计变更 | 无 | 3 处微调 | 见下方 |
| 提交数 | ~8（每迭代 1-2） | 8 | 基本一致 |

### 设计微调

1. **Canvas::DrawText 签名未变更**（原计划增加 FontHandle 参数）→ 实际采用 SoftwareCanvas 内部持有 FontManager* 的方案，避免 Canvas 接口对 text 层的依赖
2. **vx_script ↔ vx_core 循环依赖**：计划未预见，实际通过"vx_script 不链接 vx_core，由上层统一链接"解决
3. **Renderer Record/Replay 增加 image_cache 参数**：计划中构想了 RenderContext 结构体，实际直接加可选参数（更简单）

## 回顾检查清单

- [x] 计划精确度 — 文件清单基本一致（52 vs ~45），增量为 CMake 和测试辅助；预估偏差大（子代理效率因素）
- [x] TDD 执行情况 — 子代理内部遵循 RED→GREEN→REFACTOR；主线程验证全量测试无回归
- [x] 子代理质量 — 5 个子代理全部一次通过，无返工
- [x] 测试隔离 — 测试独立，无环境依赖（字体测试使用系统 DejaVuSans，PNG 测试用临时文件+PID 隔离）
- [x] 提交粒度 — 8 个提交覆盖 4 迭代 + 文档 + 收尾，合理
- [ ] 非默认路径 — 部分遗漏（见挑战 #4）

## 做得好的

1. **子代理驱动效率极高**：5 个子代理全部一次通过审查，零返工。prompt 中包含了完整的文件内容、API 签名、像素格式说明，子代理上下文充足。
2. **向后兼容设计**：所有新增功能都通过可选参数/条件分支保持向后兼容。无 FontManager 时退化为旧存根，无 ImageCache 时跳过图片处理。
3. **计划与实际高度一致**：21 个 Phase 全部按计划完成，无需回退 `/plan` 或 `/creative`。
4. **循环依赖即时解决**：vx_script ↔ vx_core 的循环在发现后立即通过 CMake 链接策略解决（Phase 4.6），未阻塞后续开发。
5. **技术债 #28 顺带修复**：`BuildTree` 现在传递 `element->inline_declarations()` 给 `StyleResolver::Resolve`，inline style 通路已打通。

## 遇到的挑战

1. **CMake 循环依赖**：vx_script 的 dom_bindings.cc include 了 vx_core 头文件，但 vx_core 的 application.cc 也需要 vx_script。计划中未预见此问题。解决方案是让 vx_script 仅通过 include path（而非链接）访问 vx_core 的头文件。
2. **dom_bindings.cc 全局状态**：`g_bound_doc`、`g_element_class_id`、`g_tracked_callbacks` 使用全局变量，不支持多 Document 实例或多线程场景。这是 MVP 简化，但产生了新的技术债。
3. **Style proxy getter 为空**：`StyleGetProp` 始终返回空字符串——写路径完整（通过 CssParser::ParseDeclarationList），但读路径未实现。需要从 inline_declarations 中反查已设置的值。
4. **非默认路径覆盖不完整**：
   - removeEventListener 简化为移除元素所有监听器（而非匹配特定 handler）
   - textContent setter 不处理无 Text 子节点的情况（不创建新 Text）
   - Image 解码失败路径仅测试了基本情况（无效路径、无效数据），缺少损坏文件测试

## 经验教训

1. **子代理 prompt 中包含完整文件内容（而非路径引用）时效果最好**：本次 prompt 中直接嵌入了当前文件的关键代码片段，子代理可以精确修改。这是 Iteration 1 的 prompt 策略，效果验证后延续到所有后续迭代。
2. **CMake 循环依赖需在计划阶段预判**：当新模块（如 dom_bindings）同时依赖 core 层和 script 层时，应在计划中标注"链接方向约束"，预防构建阶段阻塞。
3. **全局状态是 QuickJS 绑定的常见模式但需要文档化**：QuickJS C API 的 JS_GetContextOpaque 机制是正确的 per-context 存储方案，但 g_bound_doc 等全局变量绕过了它。应统一使用 context opaque。
4. **子代理合并 Phase 的策略继续有效**：Iteration 2 的 Phase 2.1-2.4 合为一个子代理（新模块创建），Phase 2.5-2.6 合为一个子代理（集成）。这种"创建+集成"两步走模式对新模块开发很高效。

## 改进建议（附优先级与落实方式）

| # | 建议 | 优先级 | 落实方式 | 目标 |
|---|------|--------|---------|------|
| 1 | 计划模板增加「CMake 链接方向约束分析」段——当新模块同时涉及两个已有库时，提前画依赖图检测循环 | P1 | 改计划模板 | `writing-plans.mdc` |
| 2 | dom_bindings.cc 中 g_bound_doc / g_element_class_id / g_tracked_callbacks 迁移到 DomBindings 实例或 context opaque | P1 | 技术债 #45 | `techContext.md` |
| 3 | StyleGetProp 实现读路径（从 inline_declarations 反查值） | P1 | 技术债 #46 | `techContext.md` |
| 4 | removeEventListener 实现按 type+handler 精确移除 | P2 | 技术债 #47 | `techContext.md` |
| 5 | SoftwareCanvas::DrawText 每次调用创建 hb_font 性能低——应缓存 HarfBuzz font 对象 | P1 | 技术债 #48 | `techContext.md` |
| 6 | textContent setter 应处理无 Text 子节点的情况（通过 Document 创建新 Text 节点） | P2 | 技术债 #49 | `techContext.md` |
| 7 | addEventListener 中 lambda 捕获的 JSContext* 生命周期需保证：Unbind 应先 RemoveEventListeners 再 FreeAll | P1 | 技术债 #50 | `techContext.md` |

## 技术改进建议

1. **dom_bindings.cc（560 行）考虑拆分**：Element 注册、Style proxy、Document 注册可拆为独立的 cc 文件
2. **Image 模块缺少 Resize API**：当前 DrawImage 在 SoftwareCanvas 中做最近邻缩放，若需高质量缩放应提供 Image::Resize 方法
3. **FreeTypeTextShaper 缺少 CJK 脚本支持**：当前硬编码 `HB_SCRIPT_LATIN`，CJK 场景需根据输入文本的 Unicode 范围自动选择脚本（`hb_buffer_guess_segment_properties` 部分覆盖）
4. **测试中使用系统字体路径**（`/usr/share/fonts/truetype/dejavu/`）存在 CI 环境依赖风险——考虑内嵌测试字体文件

## 反复模式识别

| 已知模式 | 出现频率 | 本次是否重复？ | 备注 |
|---------|---------|-------------|------|
| 计划文件清单与实际变更不一致 | 9+ 次 | ⚠️ 轻微（52 vs ~45） | 增量为 CMake 辅助变更，可接受 |
| 子代理产出需大量返工 | 7+ 次 | ✅ 未重复 | 零返工，prompt 质量提升 |
| 前置依赖/环境/API 能力未验证 | 8+ 次 | ⚠️ 轻微（CMake 循环依赖未预见） | 新增建议 #1 |
| 非默认路径遗漏验证 | 4+ 次 | ⚠️ 重复 | style getter 空实现、removeEventListener 简化 |
| 测试隔离问题 | 7+ 次 | ✅ 未重复 | PID 隔离临时文件 |
| 提交粒度偏离计划 | 7+ 次 | ✅ 未重复 | 8 提交覆盖合理 |
| TDD 严格度与场景不匹配 | 11+ 次 | ✅ 未重复 | 子代理内部 TDD |

**关键发现**：子代理驱动的 Level 4 任务首次实现零返工——证明 prompt 中"直接嵌入文件内容 + 精确 API 签名 + 像素格式约束 + 禁止修改清单"的组合策略已成熟。

## 安全评估

| 维度 | 状态 | 备注 |
|------|------|------|
| 输入验证 | ⚠️ | JS→C++ 桥接中的字符串参数有 NULL 检查，但 CSS 值解析依赖 CssParser 的鲁棒性 |
| 认证/授权 | N/A | 本任务不涉及 |
| 数据保护 | N/A | 本任务不涉及 |
| 依赖审计 | ⚠️ | FreeType/HarfBuzz/libpng/libjpeg 为系统包，未执行 CVE 扫描 |
| 错误信息脱敏 | ✅ | Status 错误消息不泄露内部实现细节 |
| 敏感数据处理 | N/A | 本任务不涉及 |

**依赖审计注意**：新增 4 个系统库依赖（FreeType、HarfBuzz、libpng、libjpeg-turbo），均为广泛使用的成熟库。建议在 CI 中加入 `apt list --installed | grep -E 'freetype|harfbuzz|libpng|libjpeg'` 版本记录，便于后续 CVE 追踪。
