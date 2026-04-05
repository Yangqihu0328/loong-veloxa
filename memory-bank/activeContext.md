# 活跃上下文

## 当前阶段
构建中

## 当前任务
- **ID**：TASK-20260405-02
- **描述**：构建 Graphics HAL 图形抽象层与 Platform HAL 平台抽象层
- **复杂度**：Level 4
- **代码规范**：Google C++ Style Guide

## 当前焦点
- Graphics HAL：Canvas/Path 纯虚接口 + SoftwareCanvas 软件光栅化
- Platform HAL：Surface/EventLoop 纯虚接口 + MemorySurface/HeadlessEventLoop
- 实现计划：`docs/plans/2026-04-05-graphics-platform-hal.md`
- 设计规格：`docs/specs/2026-04-05-graphics-platform-hal-design.md`
- 创意设计：`memory-bank/creative/creative-scanline-rasterizer.md`
- 下一步：`/build` 开始构建

## 设计决策摘要
- 首版后端：软件渲染器（SoftwareCanvas）+ Headless 平台（MemorySurface + HeadlessEventLoop）
- 像素格式：RGBA8888
- 文字/图像/输入事件：延后到后续任务
- Canvas 接口参考 Sciter gool::graphics，简化为核心绘制 + 裁剪 + 变换 + 图层
- Path 支持 MoveTo/LineTo/QuadTo/CubicTo/ArcTo/Close
- Brush 为 tagged union (Solid/LinearGradient)
- **光栅化器：覆盖率扫描线 + 非零缠绕规则 + 内置 AA**（方案 C）
  - 分桶边表 + 逐行覆盖率累加
  - 解析面积计算实现亚像素 AA
  - 贝塞尔 de Casteljau 细分，弦偏差阈值 0.25px
  - 描边转化为轮廓填充
- Surface 提供 Lock/Unlock 像素访问模式
- EventLoop 提供 PostTask + SetTimer + PollOnce
- 7 个 Phase，约 25 个文件，预估 11 小时

## 创意阶段完成
- 扫描线光栅化器 → 方案 C（覆盖率扫描线 + 非零规则 + 内置 AA）✅

## 未合并分支
（无）

## 待处理事项
- **P1**：补充 Benchmark（网络恢复后，来源 TASK-20260405-01）
- **P1**：「环境约束」和「编译器约束」已在计划中体现 ✅

## 遗留改进建议
（无）
