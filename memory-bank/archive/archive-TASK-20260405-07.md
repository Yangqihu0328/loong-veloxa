# 归档：渲染管线（Render Pipeline）

**日期：** 2026-04-05
**任务 ID：** TASK-20260405-07
**复杂度级别：** Level 4
**状态：** ✅ 已完成

## 任务概述

构建渲染管线模块 `vx::render`，将 LayoutBox 布局树转换为 Canvas 绘制指令，实现 HTML→DOM→CSS→Layout→Render→PPM 的端到端可视化。这是 Veloxa 项目的里程碑——第一次能够将 HTML/CSS 内容渲染为像素输出。

## 技术方案

采用 **Display List（命令缓冲）** 架构：

1. **Record**：递归遍历 LayoutBox 树，按 z-index stable_sort 排序子元素，生成有序 `DisplayList`（`Vector<PaintCommand>`）
2. **Replay**：遍历 DisplayList，逐条调用 Canvas 方法
3. **Paint**：Record + Replay 的便捷入口

### 三项关键设计决策

| 决策点 | 选定方案 | 理由 |
|--------|---------|------|
| Renderer API | Display List 命令缓冲 | 支持缓存/调试/独立测试，未来可扩展为 GPU 命令缓冲 |
| 文本渲染 | 扩展 Canvas::DrawText 纯虚接口 | 架构正确，SoftwareCanvas 用逐字符 FillRect 存根，后续 FreeType 零修改上层 |
| 绘制顺序 | Stacking Context z-index 排序 | stable_sort 保证同 z-index DOM 源序，opacity/overflow 通过 PushLayer/PushClipRect 处理 |

## 实现摘要

### 文件变更

| 操作 | 文件路径 | 说明 |
|------|---------|------|
| 创建 | `veloxa/core/render/paint_command.h` | PaintCommand 6 种命令类型 + DisplayList 类型别名 + 工厂函数 |
| 创建 | `veloxa/core/render/render_utils.h` | CssColorToGfx 颜色格式桥接（header-only） |
| 创建 | `veloxa/core/render/renderer.h` | Record / Replay / Paint 函数声明 |
| 创建 | `veloxa/core/render/renderer.cc` | RecordBox 递归 + z-index 排序 + PaintBorders + Replay + Paint |
| 创建 | `tests/core/render/paint_command_test.cc` | 13 个测试：工厂函数 + CssColorToGfx 6 色覆盖 |
| 创建 | `tests/core/render/renderer_test.cc` | 18 个测试：Record 14 + Replay 4 |
| 创建 | `tests/core/render/integration_test.cc` | 9 个测试：HTML→PPM 全管线 |
| 修改 | `veloxa/graphics/canvas.h` | +DrawText 纯虚函数 + StringView include |
| 修改 | `veloxa/graphics/software/software_canvas.h` | +DrawText override 声明 |
| 修改 | `veloxa/graphics/software/software_canvas.cc` | +DrawText 逐字符 FillRect 实现 |
| 修改 | `veloxa/core/CMakeLists.txt` | +render/renderer.cc, +vx_graphics 依赖 |
| 修改 | `tests/CMakeLists.txt` | +3 个测试目标 |

### 关键决策

1. **vx_core 新增 vx_graphics 依赖**：Core Engine 依赖 Graphics HAL 符合分层架构（上层依赖下层），无循环依赖
2. **CssColorToGfx 颜色桥接**：CSS RRGGBBAA 与 gfx::Color 字节序不同，通过 inline 函数转换
3. **边框用 4 个 FillRect 而非 StrokeLine**：Left/Right 边排除 Top/Bottom 区域避免角落重叠
4. **border box 原点 = x - padding[left] - border[left]**：修复了初始实现中仅减 border 未减 padding 的 bug

### 安全决策

本任务不涉及安全变更。

## 测试覆盖

| 测试文件 | 测试数 | 覆盖内容 |
|---------|--------|---------|
| paint_command_test.cc | 13 | 6 种 PaintCommand 工厂 + DisplayList 操作 + 6 种颜色转换 |
| renderer_test.cc | 18 | Record 14（空根/背景/边框/文本/visibility/opacity/overflow/z-index/嵌套）+ Replay 4（空列表/FillRect/DrawText/ClipRect 像素验证） |
| integration_test.cc | 9 | 全管线：红色 div/边框/文本/visibility:hidden/opacity 混合/嵌套着色/透明跳过/DisplayList 结构 |
| **总计** | **40** | 650/650 全量通过 |

## 经验教训

1. **LayoutBox 坐标语义是坐标计算的基础**：x/y 是 content origin，border box origin 需减去 padding + border。子代理 prompt 中的错误公式会被忠实实现。
2. **CSS 命名颜色 ≠ gfx 编程常量**：`green` = #008000 ≠ `Color::Green()` = #00FF00，测试中必须通过 CssColorToGfx 桥接。
3. **集成测试多策略验证**：DisplayList 检查（结构）> 区域扫描（存在性）> 精确像素（简单场景），避免因 HTML 隐式元素导致的坐标偏移。
4. **子代理精确签名 prompt 持续有效**：Phase 2 子代理 18 个测试零返工。

## 参考文档

- 实现计划：`docs/plans/2026-04-05-render-pipeline.md`
- 创意设计：`memory-bank/creative/creative-render-pipeline.md`
- 回顾文档：`memory-bank/reflection/reflection-TASK-20260405-07.md`
