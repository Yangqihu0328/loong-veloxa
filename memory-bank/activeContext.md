# 活跃上下文

## 当前阶段
回顾中

## 当前任务
- **ID**：TASK-20260405-13
- **描述**：CSS 动画系统（CSS Transitions）
- **复杂度**：Level 3

## 现有基础设施（探索结果）
- **动画相关代码**：无（零基础）
- **可动画属性**：ComputedStyle 中的 LengthValue（width/height/margin/padding/inset）、u32 颜色（background_color/color/border_color）、f32（opacity/flex_grow/flex_shrink）
- **帧循环**：Application 按 target_fps（默认 60）定时 SetTimer → OnFrame → Update()，dirty_ == false 时早退
- **EventLoop timer**：`SetTimer(interval_ms, callback, repeat)` / `CancelTimer(id)`

## 待处理事项
- **P1**：创建 `tests/test_pixel_utils.h` 标准化像素通道提取函数（来源 TASK-11）
- **P1**：计划模板增加「测试基础设施审计」段——列出测试需要访问的内部状态及其访问路径（来源 TASK-11，反复出现）
- **P1**：补充 Benchmark（网络恢复后，来源 TASK-01）
- **P1**：子代理 prompt 模板增加「跨模块数据格式」段（来源 TASK-02）— 已验证有效
- **P1**：集成测试优先验证数据格式一致性（来源 TASK-02）
- **P1**：存根文件预创建策略固化到子代理开发规则（来源 TASK-04）
- **P1**：合并 Phase 给子代理的策略固化到计划模板（来源 TASK-04）
- **P1**：计划模板增加「边界输入清单」段——每个 Phase 列出非默认路径（来源 TASK-06，反复出现）
- **P1**：集成测试必须使用真实 HTML/CSS 解析器，禁止仅用手动 DOM 构建（来源 TASK-06，反复出现）
- **P1**：子代理 prompt 涉及 LayoutBox 坐标计算时须包含 x/y 语义定义（content origin vs border box origin）（来源 TASK-07）
- **P1**：集成测试像素验证优先用 DisplayList 检查和区域扫描，避免硬编码坐标（来源 TASK-07）
- **P1**：CSS 颜色测试禁止与 gfx::Color 编程常量直接比较，必须通过 CssColorToGfx 转换（来源 TASK-07）
- **P1**：集成测试禁止使用 HTML inline style（BuildTree 不解析），必须用外部 CSS 选择器（来源 TASK-08，API 能力假设错误第三次出现）
- **P1**：并行子代理可行条件：无共享 .cc + 共享 .h 已创建 + CMakeLists.txt 已更新（来源 TASK-08）
- **P1**：跨模块参数透传修改时，计划模板增加「调用链端到端验证」段（来源 TASK-09）
- **P1**：设计文档管线注入点须附代码级可行性验证（来源 TASK-13）
- **P1**：HashMap 补充 const_iterator / cbegin / cend（来源 TASK-13，技术债 #41）
- **P1**：集成测试模板增加 API 备忘清单：html::Parser / FindElement / HandleInput（来源 TASK-13，反复出现第 4 次）
