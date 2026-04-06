# 归档：事件循环与应用壳

**日期：** 2026-04-05
**任务 ID：** TASK-20260405-10
**复杂度级别：** Level 3
**状态：** ✅ 已完成

## 任务概述

构建 `vx::Application` 类，将 9 个已完成子系统（Foundation/Graphics/Platform/DOM/CSS/Layout/Render/Event/Update）串联为可运行的交互式应用运行时。这是项目的第一个端到端里程碑——Veloxa 可以加载 HTML/CSS、响应鼠标交互、增量重绘脏区。

## 技术方案

### 设计决策

| 决策点 | 选定方案 | 理由 |
|--------|---------|------|
| 所有权模型 | 核心拥有 + 外部注入 | Application 拥有核心模块，EventLoop/Surface 由宿主注入（匹配嵌入式场景） |
| 帧调度 | 按需帧 + 定时器心跳 | SetTimer(16ms) 提供帧 tick，UpdateManager 检查 dirty_ 跳过无变化帧 |
| Canvas/Surface 生命周期 | 持久 Lock | 初始化时 Lock，退出时 Unlock，HMI 屏幕固定无运行时 Resize |

### 架构

Application 是纯编排层（87 行实现），连接已验证的子系统：
- `html::Parser::Parse` → Document
- `css::CssParser::Parse` → Stylesheets
- EventManager + UpdateManager + SoftwareCanvas + EventLoop

## 实现摘要

### 文件变更

| 操作 | 文件路径 | 说明 |
|------|---------|------|
| 创建 | `veloxa/core/application.h` | Application 类定义（Config, LoadHTML, LoadCSS, InjectInput, Run, Quit, Update） |
| 创建 | `veloxa/core/application.cc` | 实现：构造/析构、内容加载、输入注入、帧调度、延迟初始化 |
| 创建 | `tests/core/application_test.cc` | 10 个单元测试 |
| 创建 | `tests/core/application_integration_test.cc` | 6 个集成测试 |
| 修改 | `veloxa/core/CMakeLists.txt` | 添加 application.cc |
| 修改 | `tests/CMakeLists.txt` | 添加 2 个测试目标 |

### 关键决策

1. **EnsureUpdateManager 延迟初始化**：UpdateManager 依赖 Document + Canvas，但 LoadHTML/LoadCSS 可任意顺序调用。延迟到首次 Update/InjectInput 时创建，LoadHTML/LoadCSS 调用后 reset 触发重建。
2. **Surface 持久 Lock**：构造时 Lock 获取 pixels 指针，析构时 Unlock。避免每帧 Lock/Unlock 开销。
3. **帧定时器由 Run() 管理**：Run() 创建 repeat timer，Quit() cancel timer + 退出 EventLoop。

### 安全决策

本任务不涉及安全变更。

## 测试覆盖

- **新增测试**：16 个（10 单元 + 6 集成）
- **总测试**：747/747 通过
- **单元测试覆盖**：构造、LoadHTML/LoadCSS、Update、InjectInput、Run/Quit、帧定时器、多 CSS
- **集成测试覆盖**：简单页面渲染、:hover 变色、:active 变色、Run/Quit 生命周期、输入合并、完整交互序列（idle→hover→active→release→leave）

## 经验教训

1. **纯编排层计划精确度最高**：文件清单、测试数、代码行数全部与预估精确匹配。
2. **C++ allocator 地址复用**：`delete` 后 `new` 可能返回相同地址，验证对象替换应用状态变化而非指针不等。
3. **项目里程碑**：10 个任务完成后 Veloxa 具备端到端可运行能力。

## 新增技术债务

- **#36**：Application 缺少 Resize 支持
- **#37**：Application 缺少 HTML/CSS 解析错误回调
- **#38**：Application::LoadHTML 使用裸 delete，可改用 unique_ptr

## 参考文档

- 设计规格：`docs/specs/2026-04-05-app-shell-design.md`
- 实现计划：`docs/plans/2026-04-05-app-shell.md`
- 回顾文档：`memory-bank/reflection/reflection-TASK-20260405-10.md`
