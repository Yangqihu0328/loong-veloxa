# 任务跟踪

## 当前任务

### TASK-20260414-01：功能补全（border-radius / 字体渲染 / 图片支持 / JS-DOM 绑定）

- **状态：** ✅ 回顾完成（待归档）
- **复杂度：** Level 4（多子系统、架构决策、分阶段迭代）
- **分支：** `feature/TASK-20260414-01-feature-completion`
- **创建日期：** 2026-04-14

**设计文档：** `docs/specs/2026-04-14-feature-completion-design.md`
**实现计划：** `docs/plans/2026-04-14-feature-completion.md`

**迭代分解：**

#### Iteration 1：border-radius 渲染
- [x] Phase 1.1：PaintCommand 扩展（kFillRoundedRect, kStrokeRoundedRect）
- [x] Phase 1.2：Renderer Record/Replay 圆角支持

#### Iteration 2：FreeType + HarfBuzz 字体渲染
- [x] Phase 2.1：CMake 基础设施 + FontHandle 类型
- [x] Phase 2.2：FontManager 实现
- [x] Phase 2.3：GlyphCache 实现
- [x] Phase 2.4：FreeTypeTextShaper 实现
- [x] Phase 2.5：Canvas::DrawText 签名变更 + SoftwareCanvas 实现
- [x] Phase 2.6：C API + Application 集成

#### Iteration 3：图片支持
- [x] Phase 3.1：Image 类
- [x] Phase 3.2：ImageDecoder (PNG + JPEG)
- [x] Phase 3.3：ImageCache
- [x] Phase 3.4：Layout kReplaced + BuildTree 集成
- [x] Phase 3.5：Canvas::DrawImage + PaintCommand + Renderer
- [x] Phase 3.6：Application 集成 + 端到端测试

#### Iteration 4：QuickJS DOM 绑定
- [x] Phase 4.1：CMake + Element inline_declarations
- [x] Phase 4.2：BuildTree inline style 集成（修复技术债 #28）
- [x] Phase 4.3：DomBindings — document.getElementById
- [x] Phase 4.4：Element 属性访问
- [x] Phase 4.5：style proxy
- [x] Phase 4.6：addEventListener / removeEventListener
- [x] Phase 4.7：QuickjsEngine + Application + C API 集成

**需要创意阶段的组件：** 无

## 任务历史

| ID | 描述 | 最终状态 | 归档日期 |
|----|------|---------|---------|
| TASK-20260413-02 | 消化技术债务子集（#41 `HashMap` const 迭代、#39 测试像素头、`active_count_` 移除） | ✅ 已完成 | 2026-04-13 |
| TASK-20260413-01 | QuickJS 脚本引擎集成（quickjs-ng、`vx_script`、`QuickjsEngine`） | ✅ 已完成 | 2026-04-13 |
| TASK-20260405-01 | 构建 Foundation 基础库（内存管理/容器/字符串/日志） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-02 | 构建 Graphics HAL 图形抽象层与 Platform HAL 平台抽象层 | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-03 | 构建 DOM 树 + HTML 解析器 | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-04 | 构建 CSS 引擎（Tokenizer/Parser/选择器/属性/层叠） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-05 | 消化技术债务（Arena/HashMap/PPM/Parser Error） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-06 | 构建 Layout Engine 布局引擎 | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-07 | 构建渲染管线（Render Pipeline） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-08 | 构建事件系统（Event System） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-09 | 脏区更新与重绘机制 | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-10 | 事件循环与应用壳（EventLoop / Application Shell） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-11 | C API 层（对外嵌入接口） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-12 | 示例应用（examples/hello） | ✅ 已完成 | 2026-04-05 |
| TASK-20260405-13 | CSS 动画系统（CSS Transitions） | ✅ 已完成 | 2026-04-05 |
