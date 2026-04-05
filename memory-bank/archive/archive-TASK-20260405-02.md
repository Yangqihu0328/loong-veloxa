# 归档：Graphics HAL + Platform HAL

**日期：** 2026-04-05
**任务 ID：** TASK-20260405-02
**复杂度级别：** Level 4
**状态：** ✅ 已完成

## 任务概述

为 Veloxa 引擎构建图形抽象层（Graphics HAL）和平台抽象层（Platform HAL）。参考 Sciter 的 GOOL 层设计，提供后端可替换的 2D 绘制能力和平台无关的 Surface/事件循环。首版实现软件渲染器 + Headless 平台后端，WSL2 环境完全可测试。

## 技术方案

### 架构

```
Graphics HAL (vx::gfx)         Platform HAL (vx::platform)
  Canvas (纯虚)                   Surface (纯虚)
  Path (纯虚)                     EventLoop (纯虚)
  ↑                               ↑
  SoftwareCanvas                  MemorySurface
  SoftwarePath                    HeadlessEventLoop
  Rasterizer
```

- 纯虚接口 + 具体后端实现，后端可编译时替换
- 值类型（Color/Point/Rect/Matrix3x2/Brush）header-only
- RGBA32 像素格式：R[0:7] | G[8:15] | B[16:23] | A[24:31]

### 光栅化器

选定方案 C：覆盖率扫描线 + 非零缠绕规则 + 内置 AA。
- 分桶边表 + 逐行覆盖率累加 + SrcOver alpha 混合
- de Casteljau 贝塞尔曲线细分（阈值 0.25px，最大深度 16）
- 描边转轮廓填充（逐段法线偏移，butt 端帽）

## 实现摘要

### 文件变更

| 操作 | 文件路径 | 说明 |
|------|---------|------|
| 修改 | `CMakeLists.txt` | 添加 platform/graphics 子目录 |
| 创建 | `veloxa/graphics/CMakeLists.txt` | vx_graphics 库 |
| 创建 | `veloxa/graphics/types.h` | Color/Point/Rect/Matrix3x2 值类型 |
| 创建 | `veloxa/graphics/brush.h` | Brush tagged union (Solid/LinearGradient) |
| 创建 | `veloxa/graphics/path.h` | Path 纯虚接口 |
| 创建 | `veloxa/graphics/canvas.h` | Canvas 纯虚接口 |
| 创建 | `veloxa/graphics/software/software_path.h/.cc` | SoftwarePath 实现 |
| 创建 | `veloxa/graphics/software/rasterizer.h/.cc` | 覆盖率扫描线光栅化器 |
| 创建 | `veloxa/graphics/software/software_canvas.h/.cc` | SoftwareCanvas 实现 |
| 创建 | `veloxa/platform/CMakeLists.txt` | vx_platform 库 |
| 创建 | `veloxa/platform/surface.h` | Surface 纯虚接口 |
| 创建 | `veloxa/platform/event_loop.h` | EventLoop 纯虚接口 |
| 创建 | `veloxa/platform/headless/memory_surface.h/.cc` | MemorySurface 实现 |
| 创建 | `veloxa/platform/headless/headless_event_loop.h/.cc` | HeadlessEventLoop 实现 |
| 修改 | `tests/CMakeLists.txt` | 添加 6 个测试目标 |
| 创建 | `tests/graphics/types_test.cc` | 值类型测试（30 个） |
| 创建 | `tests/graphics/software_path_test.cc` | SoftwarePath 测试（12 个） |
| 创建 | `tests/graphics/software_canvas_test.cc` | SoftwareCanvas 测试（9 个） |
| 创建 | `tests/graphics/integration_test.cc` | 渲染管线集成测试（6 个） |
| 创建 | `tests/platform/memory_surface_test.cc` | MemorySurface 测试（9 个） |
| 创建 | `tests/platform/event_loop_test.cc` | HeadlessEventLoop 测试（9 个） |

**总计：** 32 个文件变更，3601 行新增

### 关键决策

1. **软件渲染器 + Headless 首版**：WSL2 无 GPU/窗口系统，选择离屏渲染作为首版后端，接口先稳定。
2. **覆盖率扫描线 AA（方案 C）**：一次到位实现 AA + 非零缠绕规则，与 CSS/SVG 标准兼容。
3. **值类型 header-only**：Color/Point/Rect/Matrix3x2/Brush 全部在头文件中实现，零链接开销。
4. **Canvas 接收裸指针**：SoftwareCanvas 构造函数接收 `u32* pixels`，不直接依赖 Surface 接口，保持 Graphics/Platform 解耦。
5. **Brush::Sample() 供光栅化器使用**：Brush 值类型内含采样方法，光栅化器逐像素调用。

### 安全决策

本任务不涉及安全变更。SavePPM 文件写入仅用于测试，路径由调用者完全控制。

## 测试覆盖

| 测试套件 | 测试数 | 覆盖内容 |
|---------|--------|---------|
| ColorTest/PointTest/RectTest/Matrix3x2Test/BrushTest | 30 | 值类型全覆盖 |
| MemorySurfaceTest | 9 | Lock/Unlock/Resize/SavePPM/零尺寸 |
| HeadlessEventLoopTest | 9 | FIFO/PollOnce/Run+Quit/Timer/Cancel/Nested |
| SoftwarePathTest | 12 | Bounds/Contains/曲线/多子路径 |
| SoftwareCanvasTest | 9 | Clear/FillRect/Alpha/Clip/Transform/Path/Stroke |
| GfxIntegration | 6 | 完整管线/混合/变换/Path/EventLoop/PPM |
| **总计** | **57 新测试** | **303/303 全通过** |

## 经验教训

1. **像素格式必须有唯一权威定义**：跨模块操作必须引用同一格式。SavePPM bug 根因是缺少统一定义。
2. **子代理 prompt 需包含跨模块数据格式**：不同模块的子代理可能做出矛盾的格式假设。
3. **集成测试是跨模块 bug 的唯一防线**：单模块 TDD 无法检测格式不一致。
4. **"先简化再优化"有效**：覆盖率光栅化使用中点近似，核心框架就位后可针对性优化。

## 已知技术债务

1. Rasterizer 覆盖率算法使用中点近似（非解析面积），AA 质量有限
2. PushClipPath 仅用 bounds 近似，非真正路径裁剪
3. StrokePath 无 join/cap 处理
4. PPM 测试使用硬编码 /tmp 路径
5. CMake: vx_graphics 链接 vx_platform 可能引入不必要耦合

## 参考文档

- 设计规格：`docs/specs/2026-04-05-graphics-platform-hal-design.md`
- 实现计划：`docs/plans/2026-04-05-graphics-platform-hal.md`
- 创意设计：`memory-bank/creative/creative-scanline-rasterizer.md`
- 回顾文档：`memory-bank/reflection/reflection-TASK-20260405-02.md`
