# 回顾：Graphics HAL + Platform HAL

**日期：** 2026-04-05
**任务 ID：** TASK-20260405-02
**复杂度级别：** Level 4

## 计划 vs 实际

| 维度 | 计划 | 实际 | 偏差原因 |
|------|------|------|---------|
| Phase 数 | 7 | 7 | 完全匹配 |
| 预估时间 | ~11h | ~1.5h（会话时间） | 子代理并行大幅缩短 |
| 代码文件 | ~25 | 24（不含 docs/MB） | 基本匹配 |
| 测试数量 | 未精确预估 | 57 个新测试 | 充分 |
| 设计变更 | 无 | SavePPM 像素格式修复 | 跨模块格式假设不一致 |
| 新增行数 | 未预估 | 3601（含文档） | — |

## 回顾检查清单

- [x] 计划精确度 — 文件清单与实际高度一致，7 Phase 结构完全匹配
- [x] TDD 执行情况 — 子代理均先写测试再实现，但 TDD 由子代理内部执行，主代理未逐步验证 RED 状态
- [x] 子代理质量 — 4 个子代理中 3 个一次通过，1 个（集成测试）工作绕过了已有 bug
- [ ] 测试隔离 — PPM 测试使用 /tmp 文件，有环境依赖风险
- [x] 提交粒度 — 按计划 Phase 粒度提交，8 个提交结构清晰
- [ ] 非默认路径 — PushClipPath 仅使用 bounds 近似、ArcTo 近似精度未验证

## 做得好的

1. **子代理并行策略高效**：Phase 2（值类型）+ Phase 3（Platform HAL）并行执行，Phase 5 中 SoftwarePath 和 Rasterizer+Canvas 并行执行。将预估 11h 压缩到 ~1.5h 会话时间。

2. **计划精确度高**：7 Phase 结构、文件清单、CMake 配置、接口设计在规划阶段就基本定型，构建阶段几乎无需调整。这验证了 `/plan` + `/creative` 两阶段设计的价值。

3. **集成测试发现真实 bug**：SavePPM R/B 通道互换问题仅通过集成测试（跨 Graphics HAL 和 Platform HAL）才暴露。单模块测试无法检测到，因为 MemorySurface 测试自己写入自己验证，内部一致即通过。

4. **分层架构验证**：Graphics HAL 和 Platform HAL 完全独立实现后通过集成测试无缝连接，证明纯虚接口 + 具体后端分离的架构模式有效。

5. **Foundation 复用顺畅**：`vx::Vector`、`MallocAllocator`、`Status`、`VX_DCHECK` 等 Foundation 组件在上层模块中零摩擦复用。

## 遇到的挑战

1. **像素格式跨模块不一致**（严重）：`Color::ToRGBA32()` 定义 R 在 bits 0-7（`R|G<<8|B<<16|A<<24`），但 `MemorySurface::SavePPM()` 按 `0xAARRGGBB` 假设提取通道（R 从 bits 16-23）。两个子代理各自内部一致但跨模块假设矛盾。根因是像素格式定义没有单一权威来源。

2. **子代理对跨模块约定的感知不足**：Platform HAL 子代理不知道 Graphics HAL 的 `Color::ToRGBA32()` 实际格式，自行假设了一个不同的像素布局。当 prompt 涉及多模块交互时，需要更明确的格式规范。

3. **覆盖率光栅化简化**：Rasterizer 实现使用了中点近似（`coverage[ix] += height * direction`），而非计划中的解析面积计算。这降低了 AA 质量（特别是近垂直边），但保证了正确的填充结果。

4. **部分功能简化实现**：
   - `PushClipPath` 仅使用路径 bounds 而非真正的路径裁剪
   - `StrokePath` 逐段独立渲染，无 join 处理
   - `ArcTo` 近似精度未经严格验证

## 经验教训

1. **跨模块像素格式必须有唯一权威定义**：应在 `types.h` 或专门的 `pixel_format.h` 中明确记录像素存储格式，所有涉及像素操作的代码引用此定义。这比在注释中假设更可靠。

2. **子代理 prompt 需包含跨模块约定**：当任务涉及两个模块的交互点（如 Canvas 向 Surface 写像素、Surface 导出 PPM），子代理的 prompt 必须包含交互点的精确数据格式规范，不能让子代理自行推断。

3. **集成测试是跨模块 bug 的唯一防线**：单模块 TDD 无法检测格式假设不一致。集成测试阶段应优先验证数据流跨模块的格式一致性。

4. **"先简化再优化"策略有效**：光栅化器的简化覆盖率计算让 Phase 5 顺利完成，核心的非零缠绕规则和 AA 框架已就位，后续可针对性优化覆盖率算法而无需重构。

## 改进建议（附优先级与落实方式）

| # | 建议 | 优先级 | 落实方式 | 目标 |
|---|------|--------|---------|------|
| 1 | 像素格式定义标准化：在 types.h 添加 RGBA32 格式文档注释 | P0 立即 | 加注释到 Color::ToRGBA32() | `veloxa/graphics/types.h` |
| 2 | 子代理 prompt 模板增加「跨模块数据格式」段 | P1 下次 | 更新工作流规则 | `/build` 命令规则 |
| 3 | 集成测试优先验证数据格式一致性 | P1 下次 | 加 checklist 到 `/plan` | 集成测试规划 |
| 4 | 覆盖率算法升级为解析面积计算 | P2 长期 | 记技术债 | `rasterizer.cc` |
| 5 | PushClipPath 实现真正的路径裁剪 | P2 长期 | 记技术债 | `software_canvas.cc` |
| 6 | StrokePath 添加 join/cap 处理 | P2 长期 | 记技术债 | `rasterizer.cc` |
| 7 | PPM 测试使用 tmpfile() 替代硬编码 /tmp 路径 | P2 长期 | 记技术债 | 测试文件 |

## 反复模式识别

| 已知模式 | 出现频率 | 本次是否重复？ | 说明 |
|---------|---------|-------------|------|
| 计划文件清单与实际变更不一致 | 9+ 次 | ❌ 否 | 24/25 文件匹配，高精确度 |
| 子代理产出需大量返工 | 7+ 次 | ⚠️ 部分 | SavePPM 需手动修复，但仅 1 处，非"大量" |
| 前置依赖/环境/API 能力未验证 | 8+ 次 | ✅ 是 | 像素格式约定未作为前置条件验证。**反复出现，需固化到规则** |
| 非默认路径遗漏验证 | 4+ 次 | ✅ 是 | PushClipPath 简化实现、ArcTo 精度未验证 |
| 测试隔离问题 | 7+ 次 | ⚠️ 轻微 | PPM 测试硬编码 /tmp 路径 |
| 提交粒度偏离计划 | 7+ 次 | ❌ 否 | 按 Phase 粒度提交，结构清晰 |
| TDD 严格度与场景不匹配 | 11+ 次 | ⚠️ 部分 | 子代理内部执行 TDD，主代理未逐步验证 RED |

**反复模式处理：** "前置依赖/API 能力未验证"再次出现。建议 #2 升至 P1：子代理 prompt 必须包含跨模块数据格式规范。

## 技术改进建议

1. **Rasterizer 覆盖率精度**：当前中点近似对水平/45° 边缘 AA 效果不错，但近垂直边缘锯齿明显。后续应实现逐像素列解析面积计算。

2. **RAII 辅助类**：Sciter 的 `gool::layer`/`gool::state`/`gool::clipper` 模式值得引入。当前 PushState/PopState 需手动配对，易泄漏。

3. **后端工厂**：当前 SoftwareCanvas 由调用者直接构造。后续应参考 Sciter 的 `app_factory()` 引入后端工厂模式，通过编译选项或运行时选择 Backend。

4. **测试基础设施**：应抽取 `TestSurface` 辅助类（创建 surface + canvas + lock/unlock 的 RAII 封装），减少测试样板代码。

## 安全评估

| 维度 | 状态 | 备注 |
|------|------|------|
| 输入验证 | N/A | 本任务不处理外部输入 |
| 认证/授权 | N/A | 不涉及 |
| 数据保护 | N/A | 不涉及敏感数据 |
| 依赖审计 | N/A | 无新依赖引入 |
| 错误信息脱敏 | N/A | 不涉及 |
| 敏感数据处理 | N/A | 不涉及 |

本任务不涉及安全变更。SavePPM 的文件写入仅用于测试目的，路径由调用者完全控制。

## 架构评估（Level 4）

### 分层隔离验证

```
Core Engine (未来)
    ↓ 依赖
Graphics HAL (vx::gfx)  ←→  Platform HAL (vx::platform)
    ↓ 依赖                        ↓ 依赖
Foundation (vx)                Foundation (vx)
```

- Graphics HAL 依赖 Platform HAL（通过 Canvas 构造函数接收像素指针）：✅ 方向正确
- Platform HAL 不依赖 Graphics HAL：✅ 隔离良好
- 两者都依赖 Foundation 但不直接交叉：✅
- **注意**：`vx_graphics` CMake target links `vx_platform`，这意味着 graphics 对 platform 有编译依赖。当前 SoftwareCanvas 通过裸指针接收像素，不直接 include surface.h，但 CMake 链接关系可能在未来引入不必要耦合。考虑让 Canvas 仅依赖指针参数，CMake 层面 graphics 不链接 platform。

### 长期影响

1. **接口稳定性**：Canvas 和 Path 的纯虚接口现已定义，后续添加新后端（OpenGL ES、Vulkan）只需实现新的子类。接口变更需谨慎。
2. **二进制大小**：当前 software renderer 约 1285 行实现代码，编译后体积可控。
3. **性能基线**：覆盖率光栅化器为 CPU 密集型，嵌入式场景需要硬件加速后端。软件渲染器定位为 fallback。
