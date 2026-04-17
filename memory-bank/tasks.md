# 任务跟踪

## 当前任务

### TASK-20260418-01 — 消化关键技术债务

- **状态**：构建完成（待反思/归档）
- **复杂度**：Level 3（中等功能）
- **创建日期**：2026-04-18
- **描述**：针对 TASK-20260414-01 归档遗留的 4 条 P1 技术债一次性修复：
  - **#45**：`dom_bindings.cc` 全局状态迁移（方案 A1：pimpl + JSClassID 静态幂等注册）
  - **#46**：`StyleGetProp` 实现读路径（方案 B1：length/color/auto/number/inherit/initial 序列化；Enum 暂空）
  - **#48**：`SoftwareCanvas::DrawText` 缓存 HarfBuzz `hb_font_t`（方案 C1：缓存嵌入 `FontManager::FontEntry`）
  - **#50**：`addEventListener` lambda 生命周期（方案 D1：`listener_elements` + Unbind 顺序控制）
- **涉及子系统**：`veloxa/script`（dom_bindings）、`veloxa/text`（font_manager）、`veloxa/graphics/software`（software_canvas）
- **工作流路径**：`/van` ✅ → `/plan` ✅ → (不需要 /creative) → `/build` ✅ → `/reflect` → `/archive`
- **基线分支**：`main`
- **特性分支**：`feature/TASK-20260418-01-tech-debt`（已创建）
- **安全相关**：否（纯内部重构 + UAF 修复，无外部输入/认证/存储变更）
- **前置验证**：✅ 通过
- **设计规格**：`docs/specs/2026-04-18-tech-debt-dom-bindings-design.md`
- **实现计划**：`docs/plans/2026-04-18-tech-debt-dom-bindings.md`
- **需要创意阶段的组件**：无（设计决策已在 `/plan` 头脑风暴阶段全部确定）
- **Phase 划分**：Phase 0（基线）→ Phase 1 (#45) → Phase 2 (#50) → Phase 3 (#46) → Phase 4 (#48) → Phase 5（文档）
- **预估时间**：~2 小时

## 任务历史

| ID | 描述 | 最终状态 | 归档日期 |
|----|------|---------|---------|
| TASK-20260414-01 | 功能补全（border-radius / 字体渲染 / 图片支持 / JS-DOM 绑定） | ✅ 已完成 | 2026-04-14 |
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
