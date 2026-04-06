# 回顾：渲染管线（Render Pipeline）

**日期：** 2026-04-05
**任务 ID：** TASK-20260405-07
**复杂度级别：** Level 4

## 计划 vs 实际

| 维度 | 计划 | 实际 | 偏差原因 |
|------|------|------|---------|
| Phase 数 | 3 | 3 | 无偏差 |
| 测试数 | ~36 | 40（13+18+9） | 集成测试增加了 DivWithBorderPixels（区域扫描） |
| 新增文件 | 8 | 8 | 一致：4 header/src + 3 test + 1 plan |
| 修改文件 | 5 | 5 | 一致：canvas.h, software_canvas.h/cc, 2 CMakeLists.txt |
| 子代理 | 1 个（Phase 2） | 1 个（Phase 2） | 零返工通过 |
| 提交数 | 每 Phase 一次 | 1 次整体 | Phase 间强依赖，单次提交更合理 |
| 集成测试失败 | 0 | 3（已修复） | border box 坐标 bug + CSS 命名颜色 + 像素位置 |

## 回顾检查清单

- [x] 计划精确度 — 文件清单完全一致，Phase 分解合理
- [x] TDD 执行 — Phase 1 先测试后实现；Phase 2 子代理同时交付代码和测试；Phase 3 集成测试先失败后修复
- [x] 子代理质量 — Phase 2 子代理零返工，prompt 精确签名模式持续有效
- [x] 测试隔离 — 无串扰，全部可独立运行
- [x] 提交粒度 — 单次提交（合理偏差，Phase 间强耦合）
- [x] 非默认路径 — 覆盖了 visibility:hidden、opacity 混合、overflow 裁剪、z-index 排序、空文本、空根节点

## 做得好的

1. **子代理精确签名 prompt 模式再次验证**：Phase 2 子代理交付了 18 个测试 + 完整 renderer.cc，零返工。详细的 API 签名（含 LayoutBox 坐标语义、ComputedStyle 字段格式、Canvas 方法签名）和 18 个测试用例规格描述是关键。

2. **Creative + Plan 合并策略**：跳过独立的 `/plan` 阶段，将创意设计和实现计划合并完成，减少了上下文切换。对于架构清晰、上下游 API 已知的任务，这种合并是高效的。

3. **跨模块数据格式提前识别**：在 Creative 阶段就发现了 CSS RRGGBBAA 与 gfx::Color 格式的差异，提前设计了 CssColorToGfx 转换函数并编写了 6 个颜色转换测试。这避免了运行时 debug。

4. **边界输入覆盖**：计划中明确列出了边界输入清单（nullptr root、空文本、visibility:hidden、opacity=0、border_style=none 等），实际实现中每一项都有对应测试。

## 遇到的挑战

1. **Border box 坐标计算 bug**（P0 级别）：RecordBox 和 PaintBorders 中计算 border box 原点时只减去了 border 宽度（`x - border[left]`），未减去 padding（`x - padding[left] - border[left]`）。LayoutBox 的 `x`/`y` 是 content area 原点，border box 原点需要同时偏移 padding 和 border。

   - **根因**：子代理 prompt 中描述了"计算 border box 坐标"的公式 `bx = box->x - box->border[kLeft]`，这个公式本身就是错的。prompt 中的算法描述不准确，子代理忠实执行了错误的指令。
   - **修复**：在代码审查阶段手动修正为 `bx = box->x - box->padding[kLeft] - box->border[kLeft]`。

2. **CSS 命名颜色 ≠ gfx 颜色常量**：CSS `green` = #008000（暗绿），`gfx::Color::Green()` = #00FF00（亮绿）。集成测试中用 `green` CSS 值与 `gfx::Color::Green()` 比较导致断言失败。

3. **集成测试像素位置脆弱性**：HTML 解析器创建隐式 html/body 元素，导致 CSS 目标元素在布局树中的绝对位置与预期不同。硬编码像素坐标的测试不够健壮。

## 经验教训

1. **LayoutBox 坐标语义必须在计划文档中精确定义**：`x`/`y` 是 content area 原点；padding box 原点 = `x - padding[left]`，`y - padding[top]`；border box 原点 = `x - padding[left] - border[left]`。任何涉及 LayoutBox 坐标计算的子代理 prompt 都必须包含这个语义定义。

2. **集成测试策略：DisplayList 检查 > 像素检查 > 精确坐标检查**：
   - 结构验证（命令类型、颜色、数量）用 DisplayList 检查
   - 像素存在性验证用区域扫描（HasColorInRegion）
   - 精确像素验证仅用于最简单的无嵌套场景
   
3. **CSS 命名颜色与代码颜色常量分离**：测试中涉及 CSS 颜色时，始终用 hex 值（如 `#00ff00`）并通过 `CssColorToGfx(0x00FF00FFu)` 构造预期值，不要与 `gfx::Color::Green()` 等编程常量混用。

4. **子代理 prompt 中的算法公式必须正确**：子代理忠实执行 prompt 中的算法，如果 prompt 中的公式有误，子代理不会自行纠正。在编写 prompt 前应人工验证关键公式。

## 改进建议

| # | 建议 | 优先级 | 落实方式 | 目标 |
|---|------|--------|---------|------|
| 1 | 子代理 prompt 涉及 LayoutBox 坐标计算时必须包含 x/y 语义定义（content origin vs border box origin） | P1 | 加到子代理 prompt 模板 | 子代理开发规则 |
| 2 | 集成测试像素验证优先用 DisplayList 检查和区域扫描，避免硬编码坐标 | P1 | 更新集成测试最佳实践 | activeContext.md |
| 3 | CSS 颜色测试禁止与 gfx::Color 编程常量直接比较，必须通过 CssColorToGfx 转换 | P1 | 记录到 techContext.md | 跨模块数据格式 |
| 4 | LayoutBox 增加 border_box_origin() 和 padding_box_origin() 辅助方法 | P2 | 技术债 | layout_box.h |
| 5 | SoftwareCanvas::DrawText 集成 FreeType + HarfBuzz 实现真实字形渲染 | P2 | 技术债 | 后续任务 |
| 6 | border-radius 渲染（FillRoundedRect 替代 FillRect） | P2 | 技术债 | renderer.cc |

## 反复模式识别

| 已知模式 | 本次是否重复？ | 说明 |
|---------|-------------|------|
| 计划文件清单与实际变更不一致 | ❌ | 完全一致 |
| 子代理产出需大量返工 | ❌ | 零返工 |
| 前置依赖/环境/API 能力未验证 | ❌ | /van 阶段已验证 |
| 非默认路径遗漏验证 | ❌ | 边界清单覆盖全面 |
| 测试隔离问题 | ❌ | 无串扰 |
| 提交粒度偏离计划 | ✅ | 单次提交而非 per-Phase（合理偏差） |
| TDD 严格度与场景不匹配 | ❌ | Phase 1 严格 TDD，Phase 2+3 test-with-code |

**新增反复模式**：
- **子代理 prompt 中的坐标/公式错误传播**：prompt 中的算法如果有误，子代理会忠实实现错误逻辑。这与 TASK-02 的"跨模块数据格式"问题同源——prompt 中的精确技术描述必须人工验证。首次出现，标记为观察。

## 技术改进建议

1. LayoutBox 应增加 `border_box_origin()` 返回 `{x - padding[left] - border[left], y - padding[top] - border[top]}` 方法，避免多处重复计算
2. Display List 可增加 `Dump()` 方法输出可读文本（调试用途）
3. Paint 函数可考虑接受 `gfx::Color clear_color` 参数，自动 Begin + Clear + Paint + End

## 安全评估

本任务不涉及安全变更。

| 维度 | 状态 | 备注 |
|------|------|------|
| 输入验证 | N/A | 渲染管线不处理外部输入 |
| 认证/授权 | N/A | |
| 数据保护 | N/A | |
| 依赖审计 | N/A | 无新增外部依赖 |
| 错误信息脱敏 | N/A | |
| 敏感数据处理 | N/A | |
