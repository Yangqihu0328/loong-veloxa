# Graphics HAL + Platform HAL 设计规格

**日期：** 2026-04-05
**任务 ID：** TASK-20260405-02
**状态：** 已批准

---

## 目标

为 Veloxa 引擎构建图形抽象层（Graphics HAL）和平台抽象层（Platform HAL），提供后端可替换的 2D 绘制能力和平台无关的 Surface/事件循环。首版实现软件渲染器 + Headless 平台后端，WSL2 完全可测试。

## 设计决策

| 决策项 | 选择 | 理由 |
|--------|------|------|
| 首版后端 | 软件渲染器 + Headless | WSL2 可测试，接口先稳定 |
| Graphics 范围 | 核心绘制 + Path + Brush | 足够渲染 CSS box model + 简单图形 |
| Platform 范围 | Surface + EventLoop + Timer | 足够驱动渲染管线 |
| 像素格式 | RGBA8888 | 最通用，PPM 导出零依赖 |
| 文字/图像 | 延后 | 独立为后续任务 |
| 输入事件 | 延后 | Surface 跑通后补充 |

## 架构

```
上层消费者 (Core Engine / 测试)
    │
    ├── Graphics HAL (vx::gfx)
    │     Canvas (纯虚) ← SoftwareCanvas
    │     Path (纯虚)   ← SoftwarePath
    │     Brush / Color / Point / Rect / Matrix3x2 (值类型)
    │
    ├── Platform HAL (vx::platform)
    │     Surface (纯虚)   ← MemorySurface
    │     EventLoop (纯虚) ← HeadlessEventLoop
    │
    └── Foundation (已完成)
```

- Graphics HAL 和 Platform HAL 互相独立
- Canvas 通过 Surface 获取像素 buffer 进行绘制
- 上层代码仅依赖纯虚接口

## Graphics HAL

### Canvas 纯虚接口

- Begin/End/Clear — 帧生命周期
- FillRect/FillRoundedRect/FillPath — 填充
- StrokeRect/StrokeRoundedRect/StrokePath/StrokeLine — 描边
- PushClipRect/PushClipPath/PopClip — 裁剪栈
- PushLayer/PopLayer — 图层（透明度合成）
- SetTransform/GetTransform/PushState/PopState — 变换栈
- CreatePath() — 路径工厂

### Path 纯虚接口

MoveTo/LineTo/QuadTo/CubicTo/ArcTo/Close/Reset/IsEmpty/Bounds/Contains

### 值类型

- Color { u8 r, g, b, a }
- Point { f32 x, y }
- Rect { f32 x, y, w, h }
- Matrix3x2 { f32 m[6] } — 2D 仿射变换
- Brush — tagged union (Solid / LinearGradient)

### SoftwareCanvas 实现

- RGBA8888 像素 buffer (u32*)
- 矩形：逐像素填充 + SrcOver alpha 合成
- 路径：扫描线填充算法 (edge table + active edge list)
- 描边：路径偏移转化为填充
- 裁剪栈：Vector<Rect> 交集
- 变换栈：Vector<Matrix3x2> 点变换后光栅化
- 图层栈：临时 buffer + alpha 合成

## Platform HAL

### Surface 纯虚接口

- width/height/stride — 尺寸
- Lock/Unlock — 像素访问
- Resize — 重置大小
- SavePPM — 测试用 PPM 导出

### EventLoop 纯虚接口

- Run/Quit/is_running — 生命周期
- PostTask — 异步任务投递
- SetTimer/CancelTimer — 定时器
- PollOnce — 单次 poll（测试友好）

### Headless 实现

- MemorySurface：MallocAllocator 分配 RGBA8888 buffer
- HeadlessEventLoop：Vector 任务队列 + 定时器表 + steady_clock 计时

## 错误处理

- 不抛异常，与 Foundation 一致
- Surface 分配失败 → Status::OutOfMemory
- PPM 写入失败 → Status::Internal
- 无效操作 → VX_DCHECK 断言

## 安全考量

- 本任务不涉及外部输入/认证/网络
- Lock/Unlock 期间不可 Resize（文档约束）
- PPM 路径仅测试用途

## 测试策略

- Canvas：绘制 + 像素级验证
- Path：几何 + Bounds/Contains + 填充像素
- EventLoop：任务执行顺序、定时器、Quit
- 集成：完整渲染管线 → PPM → 像素验证
