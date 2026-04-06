# 活跃上下文

## 当前阶段
构建中

## 当前任务
- **ID**：TASK-20260405-07
- **描述**：构建渲染管线（Render Pipeline）
- **复杂度**：Level 4
- **代码规范**：Google C++ Style Guide

## 范围（渲染管线子系统）
1. LayoutBox 树遍历与绘制调度
2. 背景色绘制（background-color → Canvas::FillRect）
3. 边框绘制（border-width/style/color → Canvas 线绘制）
4. 文本绘制（Canvas::DrawText 或等价方案）
5. Opacity 层（PushLayer/PopLayer）
6. Overflow 裁剪（PushClipRect/PopClip）
7. Visibility / display:none 跳过
8. HTML→DOM→CSS→Layout→Render→PPM 全管线集成测试

## 已有基础设施
- **Graphics HAL**：Canvas（FillRect/StrokeRect/FillPath/StrokePath/PushClipRect/PushLayer）、Surface（Lock/Unlock/SavePPM）、Path、Brush（Solid/Linear）、Color/Rect/Point
- **Layout**：LayoutBox（x/y/content_width/content_height/padding/border/margin）、LayoutEngine::Layout
- **CSS**：ComputedStyle（background_color, color, opacity, visibility, border_width[4], border_style[4], border_color[4], border_radius）
- **注意**：Canvas 目前无 DrawText 方法，需决定文本渲染策略

## 待处理事项（非本任务范围）
- **P1**：补充 Benchmark（网络恢复后，来源 TASK-01）
- **P1**：子代理 prompt 模板增加「跨模块数据格式」段（来源 TASK-02）— 已验证有效
- **P1**：集成测试优先验证数据格式一致性（来源 TASK-02）
- **P1**：存根文件预创建策略固化到子代理开发规则（来源 TASK-04）
- **P1**：合并 Phase 给子代理的策略固化到计划模板（来源 TASK-04）
- **P1**：计划模板增加「边界输入清单」段——每个 Phase 列出非默认路径（来源 TASK-06，反复出现）
- **P1**：集成测试必须使用真实 HTML/CSS 解析器，禁止仅用手动 DOM 构建（来源 TASK-06，反复出现）
